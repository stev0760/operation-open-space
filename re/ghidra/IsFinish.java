// Inner Space NE post-analysis: runs as a -postScript after auto-analysis.
// Seeds 'inc bp; push bp; mov bp,sp' and 'push bp; mov bp,sp' frames in bytes that
// analysis left undefined, re-runs analysis, removes anything Ghidra put in data
// segments, puts the Borland RTL segment in its own namespace, and joins
// GetProcAddress-by-ordinal uses of a DLL to that DLL's entry table.
//
// Arguments (key=value):
//   module=<name> out=<dir>                         always
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

import ghidra.app.script.GhidraScript;
import ghidra.app.util.bin.FileByteProvider;
import ghidra.app.util.bin.format.ne.*;
import ghidra.program.model.address.*;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
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

		seedFrames();
		analyzeChanges(currentProgram);
		cleanDataSegments();
		farConventions();
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
				Address a = b.getStart().add(i);
				if (fm.getFunctionAt(a) != null) {
					counts.merge(kind + "_already_function", 1, Integer::sum);
					continue;
				}
				Instruction at = listing.getInstructionAt(a);
				if (at != null && fm.getFunctionContaining(a) != null) {
					counts.merge(kind + "_inside_function", 1, Integer::sum);
					continue;
				}
				if (at == null && listing.getInstructionContaining(a) != null) {
					counts.merge(kind + "_inside_instruction", 1, Integer::sum);
					continue;
				}
				Data data = listing.getDefinedDataContaining(a);
				if (data != null) {
					counts.merge(kind + "_inside_data", 1, Integer::sum);
					continue;
				}
				if (!boundaryBefore(a)) {
					counts.merge(kind + "_after_fallthrough", 1, Integer::sum);
					continue;
				}
				// Preceding code is a far prologue's mov ax,ss/ds; nop (offset 3 of a strong
				// prologue) is excluded by the d[i-1] test above for frame_bp, and frame_incbp
				// inside a strong prologue is already a function.
				if (at == null) {
					disassemble(a);
				}
				Function f = createFunction(a, null);
				counts.merge(f != null ? kind + "_seeded" : kind + "_create_failed", 1,
					Integer::sum);
				seeded.add(kind + " " + canon(a) + (f != null ? "" : " (create failed)"));
			}
		}
		report.put("frame_seeds", counts);
		report.put("frame_seeded", seeded);
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

	void summarize() {
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
	}
}
