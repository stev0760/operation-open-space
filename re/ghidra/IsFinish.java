// Inner Space NE post-analysis: runs as a -postScript after auto-analysis.
// Rebuilds every 'jmp cs:[reg+table]' jump table from its 'cmp reg,N' bound, seeds
// 'inc bp; push bp; mov bp,sp' and 'push bp; mov bp,sp' frames and isre's gap entries
// in bytes that analysis left undefined, re-runs analysis, folds jump-only epilogue
// fragments back into the one function that owns their frame, removes anything Ghidra
// put in data segments, puts the Borland RTL segment in its own namespace, and joins
// GetProcAddress-by-ordinal uses of a DLL to that DLL's entry table.
//
// Arguments (key=value):
//   module=<name> out=<dir>                         always
//   gaps=<functions.json>                           isre asm output; its 'gap' entries
//   rtl=segN rtl_start=segN:0xOFF                   Borland RTL segment and startup
//   join_dll=<NE file> join_name=<MODULE>           GetProcAddress join target
//   join_loader=segN:0xOFF                          the LoadLibrary call of that DLL
//   join_tsv=<file>                                 a join table written by another run;
//                                                   comments this module's exports with it
// Writes <out>/<module>-finish.json (and <out>/<module>-join.tsv for a join).
//@category InnerSpace

import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.AccessMode;
import java.nio.file.Files;
import java.util.*;

import com.google.gson.GsonBuilder;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.app.util.bin.FileByteProvider;
import ghidra.app.util.bin.format.ne.*;
import ghidra.program.model.address.*;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.data.WordDataType;
import ghidra.program.model.lang.OperandType;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.pcode.JumpTable;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;

public class IsFinish extends GhidraScript {

	static final int SEL_BASE = 0x1000;

	Map<String, String> args = new HashMap<>();
	Map<String, Object> report = new LinkedHashMap<>();
	List<String> problems = new ArrayList<>();

	static int selector(int seg) {
		return SEL_BASE + (seg - 1) * 8;
	}

	static int segOf(Address a) {
		return ((((SegmentedAddress) a).getSegment()) - SEL_BASE) / 8 + 1;
	}

	static String canon(Address a) {
		return "seg" + segOf(a) + ":0x" + Integer.toHexString(((SegmentedAddress) a).getSegmentOffset());
	}

	Address addr(int seg, int off) {
		return ((SegmentedAddressSpace) currentProgram.getAddressFactory()
				.getDefaultAddressSpace()).getAddress(seg, off);
	}

	Address parseCanon(String s) {
		String[] p = s.toLowerCase().replace("seg", "").split(":");
		return addr(selector(Integer.parseInt(p[0])), Integer.decode(p[1]));
	}

	@Override
	public void run() throws Exception {
		for (String a : getScriptArgs()) {
			int eq = a.indexOf('=');
			if (eq > 0) {
				args.put(a.substring(0, eq), a.substring(eq + 1));
			}
		}
		String module = args.get("module");
		File outDir = new File(args.get("out"));
		report.put("module", module);

		report.put("switch_repair_before_seeds", repairSwitches());
		seedFrames();
		seedGaps();
		analyzeChanges(currentProgram);
		report.put("switch_repair_after_analysis", repairSwitches());
		removeFrameTails();
		cleanDataSegments();
		farConventions();
		checkReturning();
		if (args.containsKey("rtl")) {
			labelRtl();
		}
		if (args.containsKey("join_dll")) {
			joinGetProcAddress(outDir, module);
		}
		if (args.containsKey("join_tsv")) {
			commentExports(new File(args.get("join_tsv")));
		}
		summarize();
		report.put("problems", problems);
		try (PrintWriter pw = new PrintWriter(new File(outDir, module + "-finish.json"), "UTF-8")) {
			pw.println(new GsonBuilder().setPrettyPrinting().disableHtmlEscaping().create()
					.toJson(report));
		}
		println("IsFinish " + module + ": " + currentProgram.getFunctionManager().getFunctionCount() +
			" functions, " + problems.size() + " problems");
		for (String p : problems) {
			println("  PROBLEM " + p);
		}
	}

	/** True when control cannot fall into address a from the code unit before it. */
	boolean boundaryBefore(Address a) {
		Address prev = a.previous();
		if (prev == null || ((SegmentedAddress) a).getSegmentOffset() == 0) {
			return true;
		}
		CodeUnit cu = currentProgram.getListing().getCodeUnitContaining(prev);
		if (cu instanceof Instruction) {
			return !((Instruction) cu).getFlowType().hasFallthrough();
		}
		return true; // undefined bytes or data: padding or a table
	}

	void seedFrames() throws Exception {
		Map<String, Integer> counts = new TreeMap<>();
		List<String> seeded = new ArrayList<>();
		Listing listing = currentProgram.getListing();
		FunctionManager fm = currentProgram.getFunctionManager();
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (!b.isExecute() || !b.isInitialized()) {
				continue;
			}
			byte[] d = new byte[(int) b.getSize()];
			b.getBytes(b.getStart(), d);
			for (int i = 0; i + 3 <= d.length; i++) {
				String kind;
				if (i + 4 <= d.length && d[i] == 0x45 && d[i + 1] == 0x55 &&
					d[i + 2] == (byte) 0x8b && d[i + 3] == (byte) 0xec) {
					kind = "frame_incbp";      // inc bp; push bp; mov bp,sp
				}
				else if (d[i] == 0x55 && d[i + 1] == (byte) 0x8b && d[i + 2] == (byte) 0xec &&
					(i == 0 || d[i - 1] != 0x45)) {
					kind = "frame_bp";         // push bp; mov bp,sp
				}
				else {
					continue;
				}
				// A far prologue's mov ax,ss/ds; nop (offset 3 of a strong prologue) is
				// excluded by the d[i-1] test above for frame_bp, and frame_incbp inside a
				// strong prologue is already a function.
				seedIn(b.getStart().add(i), kind, counts, seeded);
			}
		}
		report.put("frame_seeds", counts);
		report.put("frame_seeded", seeded);
	}

	/**
	 * Creates a function at a only when a lies in bytes that analysis left undefined and
	 * no fall-through reaches it. Counts the outcome under kind.
	 */
	void seedIn(Address a, String kind, Map<String, Integer> counts, List<String> seeded)
			throws Exception {
		Listing listing = currentProgram.getListing();
		FunctionManager fm = currentProgram.getFunctionManager();
		if (fm.getFunctionAt(a) != null) {
			counts.merge(kind + "_already_function", 1, Integer::sum);
			return;
		}
		Instruction at = listing.getInstructionAt(a);
		if (at != null && fm.getFunctionContaining(a) != null) {
			counts.merge(kind + "_inside_function", 1, Integer::sum);
			return;
		}
		if (at == null && listing.getInstructionContaining(a) != null) {
			counts.merge(kind + "_inside_instruction", 1, Integer::sum);
			return;
		}
		if (listing.getDefinedDataContaining(a) != null) {
			counts.merge(kind + "_inside_data", 1, Integer::sum);
			return;
		}
		if (!boundaryBefore(a)) {
			counts.merge(kind + "_after_fallthrough", 1, Integer::sum);
			return;
		}
		if (at == null) {
			disassemble(a);
		}
		Function f = createFunction(a, null);
		counts.merge(f != null ? kind + "_seeded" : kind + "_create_failed", 1, Integer::sum);
		seeded.add(kind + " " + canon(a) + (f != null ? "" : " (create failed)"));
	}

	/**
	 * Seeds isre asm's 'gap' entries (functions.json, seeds containing "gap"): code that
	 * nothing references, such as Borland RTL routines reached only through tables the
	 * linker filled in. The same placement rules as the frame seeds apply, so an isre gap
	 * entry that Ghidra decoded as part of another function is left alone.
	 */
	void seedGaps() throws Exception {
		String path = args.get("gaps");
		if (path == null) {
			report.put("gap_seeds", "no gaps= argument");
			return;
		}
		File f = new File(path);
		if (!f.isFile()) {
			problems.add("gap seeds: " + path + " not found");
			return;
		}
		JsonObject root = JsonParser.parseString(
			new String(Files.readAllBytes(f.toPath()), StandardCharsets.UTF_8)).getAsJsonObject();
		List<Address> gaps = new ArrayList<>();
		for (JsonElement e : root.getAsJsonArray("functions")) {
			JsonObject fn = e.getAsJsonObject();
			for (JsonElement sd : fn.getAsJsonArray("seeds")) {
				if (sd.getAsString().equals("gap")) {
					gaps.add(parseCanon(fn.get("addr").getAsString()));
					break;
				}
			}
		}
		Collections.sort(gaps);
		Map<String, Integer> counts = new TreeMap<>();
		List<String> seeded = new ArrayList<>();
		for (Address a : gaps) {
			MemoryBlock b = currentProgram.getMemory().getBlock(a);
			if (b == null || !b.isExecute()) {
				counts.merge("gap_not_code", 1, Integer::sum);
				continue;
			}
			seedIn(a, "gap", counts, seeded);
		}
		report.put("gap_entries", gaps.size());
		report.put("gap_seeds", counts);
		report.put("gap_seeded", seeded);
	}

	// ------------------------------------------------------------------ jump tables

	/** A Borland jump table: jmp cs:[reg+table], index bounded by cmp reg,N and a jcc. */
	static class Switch {
		Address jmp, table, bound;
		int count;
		List<Address> targets = new ArrayList<>();
	}

	static final Map<String, String> UNSIGNED_JCC = Map.ofEntries(
		Map.entry("JA", "ja"), Map.entry("JNBE", "ja"), Map.entry("JBE", "jbe"),
		Map.entry("JNA", "jbe"), Map.entry("JC", "jb"), Map.entry("JB", "jb"),
		Map.entry("JNAE", "jb"), Map.entry("JNC", "jae"), Map.entry("JAE", "jae"),
		Map.entry("JNB", "jae"));

	static boolean isSegReg(Register r) {
		String n = r.getName();
		return n.equals("CS") || n.equals("DS") || n.equals("ES") || n.equals("SS");
	}

	/** The index register of 'jmp word ptr cs:[reg+disp]', or null for any other jump. */
	Register tableIndex(Instruction i) {
		if (!i.getMnemonicString().equals("JMP") || !i.getFlowType().isComputed() ||
			i.getNumOperands() != 1 ||
			!i.getDefaultOperandRepresentation(0).contains("CS:[")) {
			return null;
		}
		Register base = null;
		boolean disp = false;
		for (Object o : i.getOpObjects(0)) {
			if (o instanceof Register && !isSegReg((Register) o)) {
				if (base != null) {
					return null; // base + index
				}
				base = (Register) o;
			}
			else if (o instanceof Scalar) {
				disp = true;
			}
		}
		return disp ? base : null;
	}

	long tableDisp(Instruction i) {
		for (Object o : i.getOpObjects(0)) {
			if (o instanceof Scalar) {
				return ((Scalar) o).getUnsignedValue() & 0xffff;
			}
		}
		return -1;
	}

	/** (predecessor, "fall" | "jump") pairs of the instruction at a. */
	List<Object[]> preds(Address a) {
		Listing listing = currentProgram.getListing();
		List<Object[]> out = new ArrayList<>();
		Instruction prev = listing.getInstructionBefore(a);
		if (prev != null && a.equals(prev.getFallThrough())) {
			out.add(new Object[] { prev, "fall" });
		}
		for (Reference r : getReferencesTo(a)) {
			Instruction src = listing.getInstructionAt(r.getFromAddress());
			if (src != null && r.getReferenceType().isFlow() && src.getFlowType().isJump() &&
				!src.getFlowType().isComputed()) {
				out.add(new Object[] { src, "jump" });
			}
		}
		return out;
	}

	/**
	 * Walks back from a table jump to the 'cmp reg,N' that bounds its index, as isre asm
	 * does: the index may pass through xchg/mov, be doubled by add r,r or shl r,1 and be
	 * shifted by inc/dec/add/sub imm; the first unsigned jcc on the path is the guard.
	 */
	Switch boundSwitch(Instruction jmp) throws Exception {
		Register idx = tableIndex(jmp);
		if (idx == null) {
			return null;
		}
		Deque<Object[]> stack = new ArrayDeque<>();
		Set<String> seen = new HashSet<>();
		// state: instruction, tracked register family, scaled, guard jcc, guard edge, depth
		stack.push(new Object[] { jmp, idx.getBaseRegister(), false, null, null, 0 });
		while (!stack.isEmpty()) {
			Object[] st = stack.pop();
			Instruction cur = (Instruction) st[0];
			Register fam = (Register) st[1];
			boolean scaled = (Boolean) st[2];
			String jcc = (String) st[3], edge = (String) st[4];
			int depth = (Integer) st[5];
			String key = cur.getMinAddress() + "|" + fam + "|" + scaled + "|" + jcc + "|" + edge;
			if (depth > 16 || !seen.add(key)) {
				continue;
			}
			for (Object[] pe : preds(cur.getMinAddress())) {
				Instruction p = (Instruction) pe[0];
				String kind = (String) pe[1];
				FlowType ft = p.getFlowType();
				String mn = p.getMnemonicString();
				if (ft.isJump() && ft.isConditional()) {
					String g = jcc, e = edge;
					if (g == null && UNSIGNED_JCC.containsKey(mn)) {
						g = UNSIGNED_JCC.get(mn);
						e = kind;
					}
					stack.push(new Object[] { p, fam, scaled, g, e, depth + 1 });
					continue;
				}
				if (ft.isJump()) {
					stack.push(new Object[] { p, fam, scaled, jcc, edge, depth + 1 });
					continue;
				}
				if (ft.isCall() || ft.isTerminal() || ft.isComputed()) {
					continue;
				}
				Register r0 = p.getNumOperands() > 0 && OperandType.isRegister(p.getOperandType(0))
						? p.getRegister(0) : null;
				Register r1 = p.getNumOperands() > 1 && OperandType.isRegister(p.getOperandType(1))
						? p.getRegister(1) : null;
				Scalar s1 = p.getNumOperands() > 1 && OperandType.isScalar(p.getOperandType(1))
						? p.getScalar(1) : null;
				Register f0 = r0 == null ? null : r0.getBaseRegister();
				Register f1 = r1 == null ? null : r1.getBaseRegister();
				if (mn.equals("XCHG") && f0 != null && f1 != null &&
					(fam.equals(f0) || fam.equals(f1))) {
					stack.push(new Object[] { p, fam.equals(f0) ? f1 : f0, scaled, jcc, edge,
						depth + 1 });
					continue;
				}
				if (mn.equals("CMP") && fam.equals(f0) && s1 != null) {
					if (jcc == null) {
						continue;
					}
					int n = (int) (s1.getUnsignedValue() & 0xffff);
					int count;
					if ((jcc.equals("ja") && edge.equals("fall")) ||
						(jcc.equals("jbe") && edge.equals("jump"))) {
						count = n + 1;
					}
					else if ((jcc.equals("jae") && edge.equals("fall")) ||
						(jcc.equals("jb") && edge.equals("jump"))) {
						count = n;
					}
					else {
						continue;
					}
					if (!scaled) {
						count = (count + 1) / 2;
					}
					if (count <= 0 || count > 1024) {
						continue;
					}
					return makeSwitch(jmp, p.getMinAddress(), count);
				}
				if (!fam.equals(f0)) {
					boolean writes = false;
					for (Object o : p.getResultObjects()) {
						writes |= o instanceof Register &&
							fam.equals(((Register) o).getBaseRegister());
					}
					if (!writes) {
						stack.push(new Object[] { p, fam, scaled, jcc, edge, depth + 1 });
					}
					continue;
				}
				if (mn.equals("MOV") && f1 != null) {
					stack.push(new Object[] { p, f1, scaled, jcc, edge, depth + 1 });
				}
				else if (mn.equals("ADD") && f1 != null) {
					stack.push(new Object[] { p, fam, true, jcc, edge, depth + 1 });
				}
				else if ((mn.equals("SHL") || mn.equals("SAL")) && s1 != null &&
					s1.getUnsignedValue() == 1) {
					stack.push(new Object[] { p, fam, true, jcc, edge, depth + 1 });
				}
				else if (mn.equals("DEC") || mn.equals("INC") ||
					((mn.equals("SUB") || mn.equals("ADD")) && s1 != null)) {
					stack.push(new Object[] { p, fam, scaled, jcc, edge, depth + 1 });
				}
			}
		}
		return null;
	}

	Switch makeSwitch(Instruction jmp, Address bound, int count) throws Exception {
		SegmentedAddress j = (SegmentedAddress) jmp.getMinAddress();
		MemoryBlock b = currentProgram.getMemory().getBlock(j);
		long t = tableDisp(jmp);
		Address table = addr(j.getSegment(), (int) t);
		if (!b.contains(table) || !b.contains(table.add(2L * count - 1))) {
			return null;
		}
		Switch sw = new Switch();
		sw.jmp = j;
		sw.table = table;
		sw.bound = bound;
		sw.count = count;
		for (int i = 0; i < count; i++) {
			int off = currentProgram.getMemory().getShort(table.add(2L * i)) & 0xffff;
			Address tg = addr(j.getSegment(), off);
			if (!b.contains(tg)) {
				return null;
			}
			sw.targets.add(tg);
		}
		return sw;
	}

	List<Instruction> tableJumps() {
		List<Instruction> out = new ArrayList<>();
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (!b.isExecute() || !b.isInitialized()) {
				continue;
			}
			InstructionIterator it = currentProgram.getListing()
					.getInstructions(new AddressSet(b.getStart(), b.getEnd()), true);
			while (it.hasNext()) {
				Instruction i = it.next();
				if (tableIndex(i) != null) {
					out.add(i);
				}
			}
		}
		return out;
	}

	Set<Address> computedTargets(Instruction i) {
		Set<Address> out = new HashSet<>();
		for (Reference r : i.getReferencesFrom()) {
			if (r.getReferenceType() == RefType.COMPUTED_JUMP) {
				out.add(r.getToAddress());
			}
		}
		return out;
	}

	/** True when Ghidra's flows for sw match the table and its owner holds every case. */
	boolean switchGood(Switch sw) {
		Listing listing = currentProgram.getListing();
		Instruction j = listing.getInstructionAt(sw.jmp);
		if (j == null || !computedTargets(j).equals(new HashSet<>(sw.targets))) {
			return false;
		}
		Function f = currentProgram.getFunctionManager().getFunctionContaining(sw.jmp);
		for (Address t : sw.targets) {
			if (listing.getInstructionAt(t) == null || (f != null && !f.getBody().contains(t))) {
				return false;
			}
		}
		return true;
	}

	/** Word data on each table entry, flow references to each case, cases decoded. */
	void applySwitch(Switch sw) throws Exception {
		Listing listing = currentProgram.getListing();
		for (int i = 0; i < sw.count; i++) {
			Address e = sw.table.add(2L * i);
			Data d = listing.getDefinedDataAt(e);
			if (d != null && d.getLength() == 2) {
				continue;
			}
			if (listing.getInstructionContaining(e) != null ||
				listing.getInstructionContaining(e.add(1)) != null) {
				problems.add("jump table of " + canon(sw.jmp) + ": entry " + canon(e) +
					" overlaps code");
				continue;
			}
			listing.clearCodeUnits(e, e.add(1), false);
			createData(e, WordDataType.dataType);
		}
		listing.setComment(sw.table, CommentType.PRE, "jump table of " + canon(sw.jmp) + ": " +
			sw.count + " entries, bound from " + canon(sw.bound));
		Instruction j = listing.getInstructionAt(sw.jmp);
		for (Address t : new TreeSet<>(sw.targets)) {
			j.addOperandReference(0, t, RefType.COMPUTED_JUMP, SourceType.ANALYSIS);
		}
		for (Address t : new TreeSet<>(sw.targets)) {
			if (listing.getInstructionAt(t) == null) {
				disassemble(t);
			}
		}
	}

	/**
	 * Rebuilds every table jump whose flows do not match its cmp bound. Ghidra resolves a
	 * few Borland tables with a wrong index range (INSPACE seg12:0x1673 got 160 targets for
	 * a 10-entry table) or not at all (ISPACE.DLL seg10:0x57fe), puts table entries as data
	 * over case code and decodes code at the bogus targets. The owning function is cleared
	 * from its entry up to the next function and decoded again with the right tables; data
	 * left over code elsewhere and code reached only from a bogus target are cleared.
	 */
	Map<String, Object> repairSwitches() throws Exception {
		Listing listing = currentProgram.getListing();
		FunctionManager fm = currentProgram.getFunctionManager();
		ReferenceManager rm = currentProgram.getReferenceManager();
		List<String> repaired = new ArrayList<>(), unbounded = new ArrayList<>();
		Set<Address> bogusAll = new TreeSet<>();
		int checked = 0;
		for (Instruction j : tableJumps()) {
			j = listing.getInstructionAt(j.getMinAddress());
			if (j == null) {
				continue; // cleared by an earlier repair in this pass
			}
			Switch sw = boundSwitch(j);
			if (sw == null) {
				unbounded.add(canon(j.getMinAddress()));
				continue;
			}
			checked++;
			if (switchGood(sw)) {
				continue;
			}
			Set<Address> bogus = computedTargets(j);
			bogus.removeAll(sw.targets);
			bogusAll.addAll(bogus);
			for (Address b : bogus) {
				Function bf = fm.getFunctionAt(b);
				boolean onlyJ = true;
				for (Reference r : getReferencesTo(b)) {
					onlyJ &= r.getFromAddress().equals(sw.jmp);
				}
				if (bf != null && onlyJ) {
					fm.removeFunction(b);
				}
			}
			for (Reference r : j.getReferencesFrom()) {
				if (r.getReferenceType() == RefType.COMPUTED_JUMP) {
					rm.delete(r);
				}
			}
			Function f = fm.getFunctionContaining(sw.jmp);
			String how = "flows";
			if (f != null) {
				Address lo = f.getEntryPoint();
				MemoryBlock blk = currentProgram.getMemory().getBlock(lo);
				Address hi = blk.getEnd().add(1);
				FunctionIterator after = fm.getFunctions(lo.add(1), true);
				if (after.hasNext()) {
					Address next = after.next().getEntryPoint();
					if (blk.contains(next)) {
						hi = next;
					}
				}
				boolean own = true;
				for (Address t : sw.targets) {
					own &= t.compareTo(lo) >= 0 && t.compareTo(hi) < 0;
				}
				own &= sw.table.compareTo(lo) >= 0 && sw.table.add(2L * sw.count).compareTo(hi) <= 0;
				InstructionIterator it = listing.getInstructions(new AddressSet(lo, hi.previous()),
					true);
				while (own && it.hasNext()) {
					Function g = fm.getFunctionContaining(it.next().getMinAddress());
					own = g == null || g.equals(f);
				}
				if (own) {
					listing.clearCodeUnits(lo, hi.previous(), false);
					disassemble(lo);
					rebuildTables(lo, hi);
					how = "rebuilt " + canon(lo) + "-" + canon(hi);
				}
			}
			if (how.equals("flows")) {
				applySwitch(sw);
			}
			dropSwitchLabels(sw.jmp);
			if (f != null) {
				CreateFunctionCmd.fixupFunctionBody(currentProgram, f, monitor);
				// The decompiler recovers tables on its own and repeats the wrong range;
				// an override hands it the cases.
				new JumpTable(sw.jmp, new ArrayList<>(new LinkedHashSet<>(sw.targets)), true, 0)
						.writeOverride(f);
			}
			repaired.add(canon(sw.jmp) + " (" + sw.count + " entries, " + bogus.size() +
				" bogus targets, " + how + ")");
		}
		int dataCleared = clearStrayTableData();
		int orphanCleared = 0;
		for (Address b : bogusAll) {
			orphanCleared += clearOrphanFlow(b);
		}
		Map<String, Object> out = new LinkedHashMap<>();
		out.put("table_jumps_bounded", checked);
		out.put("table_jumps_unbounded", unbounded);
		out.put("repaired", repaired);
		out.put("stray_table_data_cleared", dataCleared);
		out.put("orphan_instructions_cleared", orphanCleared);
		return out;
	}

	/** Deletes the switchD_ namespace and case labels Ghidra made for a table jump. */
	void dropSwitchLabels(Address jmp) throws Exception {
		SymbolTable st = currentProgram.getSymbolTable();
		String name = "switchD_" + jmp.toString().replace(':', '_');
		List<Symbol> spaces = new ArrayList<>();
		SymbolIterator it = st.getSymbols(name);
		while (it.hasNext()) {
			Symbol s = it.next();
			if (s.getSymbolType() == SymbolType.NAMESPACE) {
				spaces.add(s);
			}
		}
		for (Symbol ns : spaces) {
			List<Symbol> kids = new ArrayList<>();
			SymbolIterator k = st.getSymbols((Namespace) ns.getObject());
			while (k.hasNext()) {
				kids.add(k.next());
			}
			for (Symbol c : kids) {
				c.delete();
			}
			ns.delete();
		}
	}

	/** Bounds and applies every table jump in [lo, hi) that has no flows yet. */
	void rebuildTables(Address lo, Address hi) throws Exception {
		boolean progress = true;
		while (progress) {
			progress = false;
			InstructionIterator it = currentProgram.getListing()
					.getInstructions(new AddressSet(lo, hi.previous()), true);
			List<Instruction> todo = new ArrayList<>();
			while (it.hasNext()) {
				Instruction i = it.next();
				if (tableIndex(i) != null && computedTargets(i).isEmpty()) {
					todo.add(i);
				}
			}
			for (Instruction i : todo) {
				Switch sw = boundSwitch(i);
				if (sw != null) {
					applySwitch(sw);
					progress = true;
				}
			}
		}
	}

	/** Ranges [table, table + 2*count) of every bounded table jump. */
	AddressSet tableRanges() throws Exception {
		AddressSet set = new AddressSet();
		for (Instruction j : tableJumps()) {
			Switch sw = boundSwitch(j);
			if (sw != null) {
				set.add(sw.table, sw.table.add(2L * sw.count - 1));
			}
		}
		return set;
	}

	/**
	 * Clears 2-byte data that no reference reaches and that lies outside every bounded
	 * jump table in a code block (entries of a table read with a wrong index range), then
	 * decodes the flows that the data had blocked.
	 */
	int clearStrayTableData() throws Exception {
		Listing listing = currentProgram.getListing();
		AddressSet tables = tableRanges();
		List<Data> stray = new ArrayList<>();
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (!b.isExecute() || !b.isInitialized()) {
				continue;
			}
			DataIterator it = listing.getDefinedData(new AddressSet(b.getStart(), b.getEnd()), true);
			while (it.hasNext()) {
				Data d = it.next();
				if (d.getLength() == 2 && !tables.intersects(d.getMinAddress(), d.getMaxAddress()) &&
					currentProgram.getReferenceManager().getReferenceCountTo(d.getMinAddress()) == 0) {
					stray.add(d);
				}
			}
		}
		List<Address> redo = new ArrayList<>();
		for (Data d : stray) {
			Address a = d.getMinAddress();
			listing.clearCodeUnits(a, d.getMaxAddress(), false);
			Instruction prev = listing.getInstructionBefore(a);
			if (prev != null && prev.getFallThrough() != null &&
				prev.getFallThrough().compareTo(a) >= 0 && prev.getFallThrough().compareTo(a.add(1)) <= 0) {
				redo.add(prev.getFallThrough());
			}
			for (Address x : new Address[] { a, a.add(1) }) {
				for (Reference r : getReferencesTo(x)) {
					if (r.getReferenceType().isFlow()) {
						redo.add(x);
					}
				}
			}
		}
		for (Address a : redo) {
			if (listing.getInstructionAt(a) == null) {
				disassemble(a);
			}
		}
		return stray.size();
	}

	/**
	 * Clears code decoded from a bogus table target that no function owns: the run of
	 * instructions reached from b by jumps and fall-through while no function holds them.
	 */
	int clearOrphanFlow(Address b) throws Exception {
		Listing listing = currentProgram.getListing();
		FunctionManager fm = currentProgram.getFunctionManager();
		Deque<Address> work = new ArrayDeque<>(List.of(b));
		Set<Address> clear = new TreeSet<>();
		while (!work.isEmpty()) {
			Address a = work.pop();
			Instruction i = listing.getInstructionAt(a);
			if (i == null || clear.contains(a) || fm.getFunctionContaining(a) != null) {
				continue;
			}
			clear.add(a);
			for (Address t : i.getFlows()) {
				if (!i.getFlowType().isCall()) {
					work.push(t);
				}
			}
			if (i.getFallThrough() != null) {
				work.push(i.getFallThrough());
			}
		}
		for (Address a : clear) {
			Instruction i = listing.getInstructionAt(a);
			if (i != null) {
				listing.clearCodeUnits(a, i.getMaxAddress(), false);
			}
		}
		return clear.size();
	}

	// ------------------------------------------------------------------ frame tails

	static boolean popsFrame(Listing listing, AddressSetView body, boolean[] pushes) {
		boolean pop = false;
		InstructionIterator it = listing.getInstructions(body, true);
		while (it.hasNext()) {
			Instruction i = it.next();
			String t = i.toString();
			pushes[0] |= t.equals("PUSH BP");
			pop |= t.equals("POP BP") || i.getMnemonicString().equals("LEAVE");
		}
		return pop;
	}

	/**
	 * Removes a function that is reached only by jumps from one other function and that
	 * returns through that function's frame (pop bp without a push bp of its own), then
	 * recomputes the owner's body. Ghidra's shared-return analysis makes such epilogue
	 * fragments functions and turns the jump into a call (INSPACE seg5:0xdc3 after the
	 * jmp at seg5:0x3db), which drops the owner's AX result. A fragment that several
	 * functions jump to stays a function.
	 */
	void removeFrameTails() throws Exception {
		Listing listing = currentProgram.getListing();
		FunctionManager fm = currentProgram.getFunctionManager();
		SymbolTable st = currentProgram.getSymbolTable();
		List<String> removed = new ArrayList<>();
		List<Function> cands = new ArrayList<>();
		for (Function f : fm.getFunctions(true)) {
			MemoryBlock b = currentProgram.getMemory().getBlock(f.getEntryPoint());
			if (b != null && b.isExecute() && !f.isThunk()) {
				cands.add(f);
			}
		}
		for (Function f : cands) {
			Address e = f.getEntryPoint();
			if (st.isExternalEntryPoint(e)) {
				continue;
			}
			Set<Function> from = new HashSet<>();
			List<Instruction> jumps = new ArrayList<>();
			boolean ok = true;
			for (Reference r : getReferencesTo(e)) {
				Instruction src = listing.getInstructionAt(r.getFromAddress());
				Function g = src == null ? null : fm.getFunctionContaining(src.getMinAddress());
				if (src == null || !r.getReferenceType().isFlow() ||
					!src.getMnemonicString().startsWith("J") || g == null || g.equals(f)) {
					ok = false;
					break;
				}
				from.add(g);
				jumps.add(src);
			}
			Instruction prev = listing.getInstructionBefore(e);
			if (ok && prev != null && e.equals(prev.getFallThrough())) {
				Function g = fm.getFunctionContaining(prev.getMinAddress());
				ok = g != null && !g.equals(f);
				from.add(g);
			}
			if (!ok || jumps.isEmpty() || from.size() != 1) {
				continue;
			}
			boolean[] pushes = { false };
			if (!popsFrame(listing, f.getBody(), pushes) || pushes[0]) {
				continue;
			}
			Function owner = from.iterator().next();
			boolean[] ownerPushes = { false };
			popsFrame(listing, owner.getBody(), ownerPushes);
			if (!ownerPushes[0]) {
				continue;
			}
			for (Instruction j : jumps) {
				if (j.getFlowOverride() != FlowOverride.NONE) {
					j.setFlowOverride(FlowOverride.NONE);
				}
			}
			fm.removeFunction(e);
			CreateFunctionCmd.fixupFunctionBody(currentProgram, owner, monitor);
			removed.add(canon(e) + " into " + canon(owner.getEntryPoint()));
		}
		report.put("frame_tails_removed", removed);
	}

	/** Imports that IsSetup leaves returning must still return after analysis. */
	void checkReturning() {
		Function fae = externalNamed("KERNEL", "FatalAppExit");
		if (fae != null && fae.hasNoReturn()) {
			problems.add("KERNEL.137 FatalAppExit is marked non-returning after analysis");
		}
		Function fe = externalNamed("KERNEL", "FatalExit");
		if (fe != null && !fe.hasNoReturn()) {
			problems.add("KERNEL.1 FatalExit is not marked non-returning");
		}
	}

	/**
	 * A function whose body has no ret of its own (it jumps into a shared tail) keeps the
	 * near default convention, which shifts every stack parameter by 2. Functions that
	 * start with a far frame (inc bp; push bp) or are reached by a far call are far.
	 */
	void farConventions() throws Exception {
		Memory mem = currentProgram.getMemory();
		Listing listing = currentProgram.getListing();
		int set = 0;
		Map<String, Integer> conv = new TreeMap<>();
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			MemoryBlock b = mem.getBlock(f.getEntryPoint());
			if (b == null || !b.isExecute()) {
				continue;
			}
			String cc = f.getCallingConventionName();
			if (Function.UNKNOWN_CALLING_CONVENTION_STRING.equals(cc) ||
				Function.DEFAULT_CALLING_CONVENTION_STRING.equals(cc)) {
				boolean far = false;
				byte[] head = new byte[10];
				int n = mem.getBytes(f.getEntryPoint(), head);
				for (int k = 0; k + 4 <= n && k <= 3 && !far; k++) {
					far = head[k] == 0x45 && head[k + 1] == 0x55 && head[k + 2] == (byte) 0x8b &&
						head[k + 3] == (byte) 0xec;
				}
				for (Reference r : getReferencesTo(f.getEntryPoint())) {
					Instruction ci = listing.getInstructionAt(r.getFromAddress());
					if (!far && ci != null && r.getReferenceType().isCall() &&
						ci.getMnemonicString().equals("CALLF")) {
						far = true;
					}
				}
				if (far) {
					f.setCallingConvention("__cdecl16far");
					set++;
				}
			}
			conv.merge(f.getCallingConventionName(), 1, Integer::sum);
		}
		report.put("far_convention_set", set);
		report.put("conventions", conv);
	}

	/** No function or instruction may live in a data segment (DGROUP) or resource. */
	void cleanDataSegments() throws Exception {
		FunctionManager fm = currentProgram.getFunctionManager();
		Listing listing = currentProgram.getListing();
		int removedFunctions = 0, clearedInstructions = 0;
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (b.isExecute() || b.getName().equals(MemoryBlock.EXTERNAL_BLOCK_NAME)) {
				continue;
			}
			AddressSet set = new AddressSet(b.getStart(), b.getEnd());
			List<Function> fs = new ArrayList<>();
			for (Function f : fm.getFunctions(set, true)) {
				fs.add(f);
			}
			for (Function f : fs) {
				problems.add("function in data block " + b.getName() + " at " + f.getEntryPoint() +
					" removed");
				fm.removeFunction(f.getEntryPoint());
				removedFunctions++;
			}
			InstructionIterator it = listing.getInstructions(set, true);
			List<Address> ins = new ArrayList<>();
			while (it.hasNext()) {
				ins.add(it.next().getMinAddress());
			}
			for (Address a : ins) {
				Instruction i = listing.getInstructionAt(a);
				if (i != null) {
					listing.clearCodeUnits(a, i.getMaxAddress(), false);
					clearedInstructions++;
				}
			}
		}
		report.put("data_block_functions_removed", removedFunctions);
		report.put("data_block_instructions_cleared", clearedInstructions);
	}

	void labelRtl() throws Exception {
		int seg = Integer.parseInt(args.get("rtl").replace("seg", ""));
		Address start = addr(selector(seg), 0);
		MemoryBlock b = currentProgram.getMemory().getBlock(start);
		SymbolTable st = currentProgram.getSymbolTable();
		Namespace ns = st.getNamespace("BorlandRTL", null);
		if (ns == null) {
			ns = st.createNameSpace(null, "BorlandRTL", SourceType.ANALYSIS);
		}
		int n = 0;
		for (Function f : currentProgram.getFunctionManager()
				.getFunctions(new AddressSet(b.getStart(), b.getEnd()), true)) {
			f.setParentNamespace(ns);
			n++;
		}
		if (args.containsKey("rtl_start")) {
			Address s = parseCanon(args.get("rtl_start"));
			Function f = currentProgram.getFunctionManager().getFunctionAt(s);
			if (f == null) {
				problems.add("no function at RTL startup " + args.get("rtl_start"));
			}
			else {
				f.setName("startup", SourceType.ANALYSIS);
				f.setParentNamespace(ns);
				currentProgram.getListing().setComment(s, CommentType.PLATE,
					"Borland RTL startup (NE CS:IP " + canon(s) + "): xor bp,bp; push bp; " +
						"call KERNEL.InitTask");
			}
		}
		currentProgram.getListing().setComment(start, CommentType.PLATE,
			"seg" + seg + ": Borland C++ runtime library (namespace BorlandRTL)");
		report.put("rtl_segment", seg);
		report.put("rtl_functions", n);
	}

	/** ordinal -> (seg, off) of a DLL's entry table. */
	Map<Integer, int[]> entryTable(File dll) throws Exception {
		Map<Integer, int[]> out = new TreeMap<>();
		try (FileByteProvider bp = new FileByteProvider(dll, null, AccessMode.READ)) {
			NewExecutable ne = new NewExecutable(bp, null);
			int ordinal = 0;
			for (EntryTableBundle bundle : ne.getWindowsHeader().getEntryTable().getBundles()) {
				if (bundle.getType() == EntryTableBundle.UNUSED) {
					ordinal += Byte.toUnsignedInt(bundle.getCount());
					continue;
				}
				for (EntryPoint pt : bundle.getEntryPoints()) {
					ordinal++;
					int seg = bundle.isMoveable() ? Byte.toUnsignedInt(pt.getSegment())
							: bundle.getType();
					if (!bundle.isConstant() && seg > 0) {
						out.put(ordinal, new int[] { seg, Short.toUnsignedInt(pt.getOffset()) });
					}
				}
			}
		}
		return out;
	}

	Address dataRef(Instruction i, boolean write) {
		for (Reference r : i.getReferencesFrom()) {
			if (r.getReferenceType().isData() && r.getToAddress().isMemoryAddress() &&
				(!write || r.getReferenceType().isWrite())) {
				return r.getToAddress();
			}
		}
		return null;
	}

	Function externalNamed(String lib, String name) {
		ExternalManager em = currentProgram.getExternalManager();
		ExternalLocationIterator it = em.getExternalLocations(lib);
		while (it.hasNext()) {
			ExternalLocation loc = it.next();
			if (name.equals(loc.getLabel())) {
				return loc.getFunction();
			}
		}
		return null;
	}

	void joinGetProcAddress(File outDir, String module) throws Exception {
		String dllName = args.get("join_name");
		Map<Integer, int[]> entries = entryTable(new File(args.get("join_dll")));
		Listing listing = currentProgram.getListing();
		SymbolTable st = currentProgram.getSymbolTable();

		// The handle global: 'call LoadLibrary' at the loader site, then 'mov [h],ax'.
		Address loaderSite = parseCanon(args.get("join_loader"));
		Instruction loadCall = listing.getInstructionAt(loaderSite);
		Address handle = null;
		Function loaderFn = currentProgram.getFunctionManager().getFunctionContaining(loaderSite);
		if (loadCall != null && loadCall.getFlowType().isCall()) {
			Instruction next = loadCall.getNext();
			if (next != null && next.toString().endsWith(",AX")) {
				handle = dataRef(next, true);
			}
		}
		if (handle == null) {
			problems.add("join: no 'mov [h],ax' after the LoadLibrary call at " +
				args.get("join_loader"));
			return;
		}
		st.createLabel(handle, "h" + dllName, SourceType.ANALYSIS);
		listing.setComment(loaderSite, CommentType.PRE,
			"LoadLibrary of " + dllName + ".DLL; handle stored at DGROUP:0x" +
				Integer.toHexString(((SegmentedAddress) handle).getSegmentOffset()));

		Function gpa = externalNamed("KERNEL", "GetProcAddress");
		if (gpa == null) {
			problems.add("join: KERNEL GetProcAddress is not an external");
			return;
		}
		List<String> rows = new ArrayList<>();
		rows.add("ordinal\tdll_address\tdll_name\tglobal\tglobal_name\tgetprocaddress_site\t" +
			"indirect_calls");
		Map<Integer, String> joined = new TreeMap<>();
		List<Address> sites = new ArrayList<>();
		for (Address thunk : gpa.getFunctionThunkAddresses(false)) {
			for (Reference r : getReferencesTo(thunk)) {
				if (r.getReferenceType().isCall()) {
					sites.add(r.getFromAddress());
				}
			}
		}
		Collections.sort(sites);
		int otherHandle = 0, byName = 0, otherShape = 0;
		for (Address site : sites) {
			Instruction call = listing.getInstructionAt(site);
			Instruction pOrd = call.getPrevious();
			Instruction pSel = pOrd == null ? null : pOrd.getPrevious();
			Instruction pMod = pSel == null ? null : pSel.getPrevious();
			if (pMod == null || !pOrd.getMnemonicString().equals("PUSH") ||
				!pSel.getMnemonicString().equals("PUSH") ||
				!pMod.getMnemonicString().equals("PUSH")) {
				otherShape++;
				continue;
			}
			Scalar ord = pOrd.getScalar(0);
			Scalar sel = pSel.getScalar(0);
			if (ord == null || sel == null || sel.getUnsignedValue() != 0) {
				byName++; // by name, not by ordinal
				continue;
			}
			Address modRef = dataRef(pMod, false);
			boolean ours = handle.equals(modRef) || (modRef == null && loaderFn != null &&
				loaderFn.getBody().contains(site));
			if (!ours) {
				otherHandle++;
				continue;
			}
			int ordinal = (int) ord.getUnsignedValue();
			Instruction stLo = call.getNext();
			Instruction stHi = stLo == null ? null : stLo.getNext();
			Address g = stLo == null ? null : dataRef(stLo, true);
			Address g2 = stHi == null ? null : dataRef(stHi, true);
			if (g == null || g2 == null || !g2.equals(g.add(2))) {
				problems.add("join: result of GetProcAddress at " + canon(site) +
					" is not stored to a far pointer");
				continue;
			}
			int[] ent = entries.get(ordinal);
			String dllAddr = ent == null ? "" : "seg" + ent[0] + ":0x" + Integer.toHexString(ent[1]);
			String dllFn = dllName + "_Ordinal_" + ordinal;
			String gName = "p" + dllFn;
			listing.clearCodeUnits(g, g.add(3), false);
			createData(g, new PointerDataType(VoidDataType.dataType, 4));
			st.createLabel(g, gName, SourceType.ANALYSIS).setPrimary();
			String what = dllName + ".DLL ordinal " + ordinal +
				(ent == null ? " (not in its entry table)" : " = " + dllName + ".DLL " + dllAddr);
			listing.setComment(g, CommentType.EOL, what);
			listing.setComment(site, CommentType.EOL, "GetProcAddress -> " + what);
			int calls = 0;
			for (Reference r : getReferencesTo(g)) {
				Instruction ci = listing.getInstructionAt(r.getFromAddress());
				if (ci != null && ci.getFlowType().isCall()) {
					listing.setComment(ci.getMinAddress(), CommentType.EOL, "calls " + what);
					calls++;
				}
			}
			joined.put(ordinal, dllAddr);
			rows.add(ordinal + "\t" + dllAddr + "\t" + dllFn + "\tDGROUP:0x" +
				Integer.toHexString(((SegmentedAddress) g).getSegmentOffset()) + "\t" + gName +
				"\t" + canon(site) + "\t" + calls);
		}
		Files.write(new File(outDir, module + "-join.tsv").toPath(), rows,
			StandardCharsets.UTF_8);
		Map<String, Object> j = new LinkedHashMap<>();
		j.put("dll", dllName);
		j.put("handle", "DGROUP:0x" +
			Integer.toHexString(((SegmentedAddress) handle).getSegmentOffset()));
		j.put("getprocaddress_sites", sites.size());
		j.put("sites_other_handle", otherHandle);
		j.put("sites_by_name", byName);
		j.put("sites_other_shape", otherShape);
		j.put("ordinals", new ArrayList<>(joined.keySet()));
		List<Integer> missing = new ArrayList<>();
		for (Map.Entry<Integer, String> e : joined.entrySet()) {
			if (e.getValue().isEmpty()) {
				missing.add(e.getKey());
			}
		}
		j.put("ordinals_not_in_entry_table", missing);
		report.put("join", j);
	}

	/** Comments this module's exports with the importer's join table. */
	void commentExports(File tsv) throws Exception {
		if (!tsv.isFile()) {
			problems.add("join table " + tsv + " not found; exports left uncommented");
			return;
		}
		List<String> lines = Files.readAllLines(tsv.toPath(), StandardCharsets.UTF_8);
		int n = 0;
		for (String line : lines.subList(1, lines.size())) {
			String[] c = line.split("\t", -1);
			if (c.length < 7 || c[1].isEmpty()) {
				continue;
			}
			Address a = parseCanon(c[1]);
			String note = "Imported by ordinal " + c[0] + " through GetProcAddress at " +
				args.getOrDefault("join_importer", "the importer") + " " + c[5] +
				"; pointer " + c[3] + " (" + c[4] + "), " + c[6] + " indirect call sites";
			String old = currentProgram.getListing().getComment(CommentType.PLATE, a);
			currentProgram.getListing().setComment(a, CommentType.PLATE,
				old == null || old.isEmpty() ? note : old + "\n" + note);
			n++;
		}
		report.put("exports_commented_from_join", n);
	}

	void summarize() throws Exception {
		FunctionManager fm = currentProgram.getFunctionManager();
		Map<Integer, Integer> perSeg = new TreeMap<>();
		int thunks = 0, internal = 0;
		for (Function f : fm.getFunctions(true)) {
			MemoryBlock b = currentProgram.getMemory().getBlock(f.getEntryPoint());
			if (b == null || !b.isExecute()) {
				thunks++;
				continue;
			}
			internal++;
			perSeg.merge(segOf(f.getEntryPoint()), 1, Integer::sum);
		}
		// Instructions that analysis decoded but no function owns.
		int orphan = 0;
		InstructionIterator it = currentProgram.getListing().getInstructions(true);
		while (it.hasNext()) {
			Instruction i = it.next();
			if (fm.getFunctionContaining(i.getMinAddress()) == null) {
				orphan++;
			}
		}
		report.put("functions", internal);
		report.put("functions_per_segment", perSeg);
		report.put("thunks_and_externals", thunks);
		report.put("instructions_outside_functions", orphan);

		// Every bounded jump table, and every defined datum in code outside them.
		List<Map<String, Object>> sws = new ArrayList<>();
		AddressSet tables = new AddressSet();
		for (Instruction j : tableJumps()) {
			Switch sw = boundSwitch(j);
			Map<String, Object> m = new LinkedHashMap<>();
			m.put("at", canon(j.getMinAddress()));
			if (sw == null) {
				m.put("bounded", false);
				sws.add(m);
				continue;
			}
			tables.add(sw.table, sw.table.add(2L * sw.count - 1));
			Function owner = fm.getFunctionContaining(sw.jmp);
			m.put("table", canon(sw.table));
			m.put("count", sw.count);
			m.put("bound_at", canon(sw.bound));
			m.put("owner", owner == null ? null : canon(owner.getEntryPoint()));
			m.put("good", switchGood(sw));
			sws.add(m);
		}
		report.put("switches", sws);
		List<String> codeData = new ArrayList<>();
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (!b.isExecute() || !b.isInitialized()) {
				continue;
			}
			DataIterator di = currentProgram.getListing()
					.getDefinedData(new AddressSet(b.getStart(), b.getEnd()), true);
			while (di.hasNext()) {
				Data d = di.next();
				if (!tables.intersects(d.getMinAddress(), d.getMaxAddress())) {
					codeData.add(canon(d.getMinAddress()) + " " + d.getLength());
				}
			}
		}
		report.put("code_data_outside_tables", codeData);
	}
}
