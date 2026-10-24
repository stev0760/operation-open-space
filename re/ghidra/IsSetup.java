// Inner Space NE pre-analysis setup: runs as a -preScript after Ghidra's NE loader.
// Checks the import (every segment loaded, every relocation applied, imports are
// external ordinals), sets DS/SS to DGROUP, names imports from Wine .spec files,
// gives KERNEL.1 FatalExit the signature Wine's spec lacks, patches equate imports,
// and seeds functions from exports, the program entry, internal far-call targets and
// the Win16 far prologue.
//
// Arguments (key=value): module=<name> ne=<NE file> specs=<dir of *.spec> out=<dir>
// Writes <out>/<module>-setup.json and <out>/<module>-imports.tsv.
//@category InnerSpace

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.nio.file.AccessMode;
import java.nio.file.Files;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.google.gson.GsonBuilder;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.bin.FileByteProvider;
import ghidra.app.util.bin.format.ne.*;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.reloc.Relocation;
import ghidra.program.model.symbol.*;

public class IsSetup extends GhidraScript {

	static final int SEL_BASE = 0x1000;
	static final int SEL_STEP = 8;

	/** One entry of a Wine 16-bit .spec file. */
	static class SpecEntry {
		int ordinal;
		String kind;      // pascal, cdecl, varargs, stub, equate, variable
		String name;
		List<String> args = new ArrayList<>();
		boolean ret16;
		boolean retVoid;
		boolean register;
		long value;       // equate value
		String note;      // plate comment of an override
	}

	static final Pattern SPEC_LINE = Pattern.compile(
		"^(\\d+)\\s+(\\w+)\\s+((?:-\\S+\\s+)*)([^\\s(]+)(?:\\(([^)]*)\\))?(?:\\s+(\\S+))?");

	/** Parses the 16-bit .spec files in dir into module name -> ordinal -> entry. */
	static Map<String, Map<Integer, SpecEntry>> loadSpecs(File dir) throws IOException {
		Map<String, Map<Integer, SpecEntry>> out = new TreeMap<>();
		File[] files = dir.listFiles((d, n) -> n.endsWith("16.spec"));
		if (files == null) {
			throw new IOException("no *16.spec files in " + dir);
		}
		Arrays.sort(files);
		for (File f : files) {
			String base = f.getName().substring(0, f.getName().indexOf('.')).toUpperCase();
			String module = base.equals("KRNL386") ? "KERNEL" : base;
			Map<Integer, SpecEntry> m = new TreeMap<>();
			for (String raw : Files.readAllLines(f.toPath(), StandardCharsets.UTF_8)) {
				int hash = raw.indexOf('#');
				String line = (hash >= 0 ? raw.substring(0, hash) : raw).trim();
				Matcher mt = SPEC_LINE.matcher(line);
				if (!mt.find()) {
					continue;
				}
				SpecEntry e = new SpecEntry();
				e.ordinal = Integer.parseInt(mt.group(1));
				e.kind = mt.group(2);
				String flags = mt.group(3);
				e.ret16 = flags.contains("-ret16");
				e.register = flags.contains("-register");
				e.name = mt.group(4);
				if (mt.group(5) != null && !e.kind.equals("variable")) {
					for (String a : mt.group(5).trim().split("\\s+")) {
						if (!a.isEmpty()) {
							e.args.add(a);
						}
					}
				}
				if (e.kind.equals("equate") && mt.group(6) != null) {
					e.value = Long.decode(mt.group(6));
				}
				m.put(e.ordinal, e);
			}
			out.put(module, m);
		}
		return out;
	}

	/**
	 * Imports that do not return. FatalExit ends the task; in INSPACE seg5 the call
	 * FatalExit(0xff) at 0xb9e is followed by one pad byte and a far prologue at 0xba4.
	 */
	static final Set<String> NO_RETURN = Set.of("KERNEL.1");

	/**
	 * Imports that must stay returning. The Borland RTL follows FatalAppExit (INSPACE
	 * seg5:0xb95, ISPACE.DLL seg3:0xa8c) with a FatalExit(0xff) fallback that only the
	 * fall-through reaches. IsFinish checks that analysis left these returning.
	 */
	static final Set<String> RETURNS = Set.of("KERNEL.137");

	/**
	 * Signatures that Wine's specs do not give: krnl386.exe16.spec lists KERNEL.1 as
	 * 'stub FatalExit'. The Win16 SDK declares void FAR PASCAL FatalExit(int).
	 */
	static final Map<String, SpecEntry> SIGNATURE_OVERRIDES = new TreeMap<>();
	static {
		SpecEntry fe = new SpecEntry();
		fe.ordinal = 1;
		fe.kind = "pascal";
		fe.name = "FatalExit";
		fe.args.add("word");
		fe.retVoid = true;
		fe.note = "Win16 SDK: void FAR PASCAL FatalExit(int); Wine's spec lists it as a stub";
		SIGNATURE_OVERRIDES.put("KERNEL.1", fe);
	}

	/** Register inputs of -register imports: DOS3Call takes the INT 21h registers. */
	static final Map<String, String[]> REGISTER_ARGS =
		Map.of("DOS3Call", new String[] { "AX", "BX", "CX", "DX" });

	static int selector(int segNumber) {
		return SEL_BASE + (segNumber - 1) * SEL_STEP;
	}

	Map<String, String> args = new HashMap<>();
	Map<String, Object> report = new LinkedHashMap<>();
	List<String> problems = new ArrayList<>();
	List<String> rejected = new ArrayList<>();

	String arg(String key) {
		String v = args.get(key);
		if (v == null) {
			throw new IllegalArgumentException("missing script argument " + key + "=...");
		}
		return v;
	}

	Address addr(int seg, int off) {
		return ((SegmentedAddressSpace) currentProgram.getAddressFactory()
				.getDefaultAddressSpace()).getAddress(seg, off);
	}

	@Override
	public void run() throws Exception {
		for (String a : getScriptArgs()) {
			int eq = a.indexOf('=');
			if (eq > 0) {
				args.put(a.substring(0, eq), a.substring(eq + 1));
			}
		}
		String module = arg("module");
		File neFile = new File(arg("ne"));
		File outDir = new File(arg("out"));
		outDir.mkdirs();
		Map<String, Map<Integer, SpecEntry>> specs = loadSpecs(new File(arg("specs")));

		NewExecutable ne;
		try (FileByteProvider bp = new FileByteProvider(neFile, null, AccessMode.READ)) {
			ne = new NewExecutable(bp, null);
			// NewExecutable reads lazily in places; touch everything while open.
			WindowsHeader wh = ne.getWindowsHeader();
			report.put("module", module);
			checkSegments(wh);
			int dgroup = wh.getInformationBlock().getAutomaticDataSegment();
			report.put("dgroup_seg", dgroup);
			setSegmentRegisters(wh, dgroup);
			checkRelocations(wh);
			nameImports(wh, specs, outDir, module);
			fixAdditive();
			seedFunctions(wh);
		}
		setAnalysisOptions();
		report.put("problems", problems);
		try (PrintWriter pw = new PrintWriter(new File(outDir, module + "-setup.json"), "UTF-8")) {
			pw.println(new GsonBuilder().setPrettyPrinting().disableHtmlEscaping().create()
					.toJson(report));
		}
		println("IsSetup " + module + ": " + problems.size() + " problems");
		for (String p : problems) {
			println("  PROBLEM " + p);
		}
	}

	/** Every NE segment must be a memory block at its selector with minalloc bytes. */
	void checkSegments(WindowsHeader wh) {
		Segment[] segs = wh.getSegmentTable().getSegments();
		Memory mem = currentProgram.getMemory();
		int loaded = 0;
		List<Map<String, Object>> rows = new ArrayList<>();
		for (int i = 0; i < segs.length; i++) {
			int n = i + 1;
			int size = Short.toUnsignedInt(segs[i].getMinAllocSize());
			if (size == 0) {
				size = 0x10000;
			}
			MemoryBlock b = mem.getBlock(addr(selector(n), 0));
			boolean ok = b != null && b.getStart().equals(addr(selector(n), 0)) &&
				b.getSize() == size && b.getName().endsWith(Integer.toString(n)) &&
				b.isExecute() == segs[i].isCode();
			if (ok) {
				loaded++;
			}
			else {
				problems.add("seg" + n + " not loaded as expected: " + b);
			}
			Map<String, Object> r = new LinkedHashMap<>();
			r.put("seg", n);
			r.put("selector", String.format("%04x", selector(n)));
			r.put("size", size);
			r.put("code", segs[i].isCode());
			r.put("relocs", segs[i].getRelocations().length);
			rows.add(r);
		}
		report.put("segments", segs.length);
		report.put("segments_loaded", loaded);
		report.put("segment_table", rows);
	}

	/** DS (all code) and SS (code of an application, which runs with SS == DGROUP). */
	void setSegmentRegisters(WindowsHeader wh, int dgroup) throws Exception {
		ProgramContext ctx = currentProgram.getProgramContext();
		Register ds = currentProgram.getRegister("DS");
		Register ss = currentProgram.getRegister("SS");
		boolean library = (wh.getInformationBlock().getApplicationFlags() &
			InformationBlock.FLAGS_APP_LIBRARY_MODULE) != 0;
		BigInteger sel = BigInteger.valueOf(selector(dgroup));
		int n = 0;
		Segment[] segs = wh.getSegmentTable().getSegments();
		for (int i = 0; i < segs.length; i++) {
			if (!segs[i].isCode()) {
				continue;
			}
			MemoryBlock b = currentProgram.getMemory().getBlock(addr(selector(i + 1), 0));
			ctx.setValue(ds, b.getStart(), b.getEnd(), sel);
			if (!library) {
				ctx.setValue(ss, b.getStart(), b.getEnd(), sel);
			}
			n++;
		}
		report.put("dgroup_selector", String.format("%04x", selector(dgroup)));
		report.put("context_blocks", n);
		report.put("context_registers", library ? List.of("DS") : List.of("DS", "SS"));
	}

	/** Every NE relocation record has an APPLIED site; import FAR_ADDR sites hit thunks. */
	void checkRelocations(WindowsHeader wh) throws Exception {
		Segment[] segs = wh.getSegmentTable().getSegments();
		int records = 0, recordsApplied = 0;
		for (int i = 0; i < segs.length; i++) {
			for (SegmentRelocation r : segs[i].getRelocations()) {
				records++;
				Address site = addr(selector(i + 1), Short.toUnsignedInt(r.getOffset()));
				boolean applied = false;
				for (Relocation rel : currentProgram.getRelocationTable().getRelocations(site)) {
					applied |= rel.getStatus() == Relocation.Status.APPLIED;
				}
				if (applied) {
					recordsApplied++;
				}
				else {
					problems.add("relocation record at " + site + " not applied");
				}
			}
		}
		int sites = 0, importSites = 0, importOk = 0;
		Map<String, Integer> byType = new TreeMap<>();
		Iterator<Relocation> it = currentProgram.getRelocationTable().getRelocations();
		Memory mem = currentProgram.getMemory();
		while (it.hasNext()) {
			Relocation rel = it.next();
			sites++;
			byType.merge(SegmentRelocation.TYPE_STRINGS[rel.getType()], 1, Integer::sum);
			long[] v = rel.getValues();
			boolean isImport = (v[1] & SegmentRelocation.FLAG_TARGET_MASK) ==
				SegmentRelocation.FLAG_IMPORT_ORDINAL;
			if (!isImport || rel.getType() != SegmentRelocation.TYPE_FAR_ADDR ||
				(v[1] & SegmentRelocation.FLAG_ADDITIVE) != 0) {
				continue;
			}
			importSites++;
			Address a = rel.getAddress();
			int off = mem.getShort(a) & 0xffff;
			int sel = mem.getShort(a.add(2)) & 0xffff;
			Function thunk = currentProgram.getFunctionManager().getFunctionAt(addr(sel, off));
			String module = wh.getModuleReferenceTable().getNames()[(int) v[3] - 1].getString();
			String want = SymbolUtilities.ORDINAL_PREFIX + v[4];
			if (thunk != null && thunk.isThunk()) {
				Function ext = thunk.getThunkedFunction(true);
				ExternalLocation loc = ext == null ? null
						: currentProgram.getExternalManager()
								.getExternalLocation(ext.getSymbol());
				if (loc != null && module.equals(loc.getLibraryName()) &&
					want.equals(loc.getLabel())) {
					importOk++;
					continue;
				}
			}
			if (problems.size() < 50) {
				problems.add("import site " + a + " (" + module + "." + v[4] +
					") does not reach an external ordinal thunk");
			}
		}
		report.put("ne_relocation_records", records);
		report.put("ne_relocation_records_applied", recordsApplied);
		report.put("relocation_sites", sites);
		report.put("relocation_sites_by_type", byType);
		report.put("import_far_sites", importSites);
		report.put("import_far_sites_to_external_ordinal", importOk);
	}

	/** Renames external Ordinal_N locations from the .spec files and sets signatures. */
	void nameImports(WindowsHeader wh, Map<String, Map<Integer, SpecEntry>> specs, File outDir,
			String moduleName) throws Exception {
		ExternalManager em = currentProgram.getExternalManager();
		// Ordinals referenced per module, split by whether only seg1 references them.
		Map<String, Set<Integer>> all = new TreeMap<>();
		Map<String, Set<Integer>> beyondSeg1 = new TreeMap<>();
		Map<String, Integer> siteCount = new HashMap<>();
		Segment[] segs = wh.getSegmentTable().getSegments();
		LengthStringSet[] mods = wh.getModuleReferenceTable().getNames();
		for (int i = 0; i < segs.length; i++) {
			for (SegmentRelocation r : segs[i].getRelocations()) {
				if (!r.isImportOrdinal()) {
					continue;
				}
				String m = mods[r.getTargetSegment() - 1].getString();
				int ord = Short.toUnsignedInt(r.getTargetOffset());
				all.computeIfAbsent(m, k -> new TreeSet<>()).add(ord);
				if (i > 0) {
					beyondSeg1.computeIfAbsent(m, k -> new TreeSet<>()).add(ord);
				}
			}
		}
		Iterator<Relocation> rit = currentProgram.getRelocationTable().getRelocations();
		while (rit.hasNext()) {
			long[] v = rit.next().getValues();
			if ((v[1] & SegmentRelocation.FLAG_TARGET_MASK) == SegmentRelocation.FLAG_IMPORT_ORDINAL) {
				siteCount.merge(mods[(int) v[3] - 1].getString() + "." + v[4], 1, Integer::sum);
			}
		}

		int total = 0, named = 0, typed = 0;
		List<String> unresolved = new ArrayList<>();
		List<String> noReturn = new ArrayList<>();
		List<String> returning = new ArrayList<>();
		List<String> overridden = new ArrayList<>();
		Map<String, Integer> kinds = new TreeMap<>();
		List<String> tsv = new ArrayList<>();
		tsv.add("module\tordinal\tname\tkind\tsites\tseg1_only");
		Map<String, Map<String, Long>> equates = new HashMap<>();
		for (String lib : em.getExternalLibraryNames()) {
			List<ExternalLocation> locs = new ArrayList<>();
			ExternalLocationIterator it = em.getExternalLocations(lib);
			while (it.hasNext()) {
				locs.add(it.next());
			}
			Map<Integer, SpecEntry> spec = specs.get(lib.toUpperCase());
			for (ExternalLocation loc : locs) {
				int ord = SymbolUtilities.getOrdinalValue(loc.getLabel());
				if (ord < 0) {
					continue;
				}
				total++;
				boolean seg1Only = !beyondSeg1.getOrDefault(lib, Set.of()).contains(ord);
				SpecEntry e = spec == null ? null : spec.get(ord);
				SpecEntry over = SIGNATURE_OVERRIDES.get(lib.toUpperCase() + "." + ord);
				if (e != null && over != null && over.name.equals(e.name)) {
					overridden.add(lib + "." + ord + " " + e.kind + " -> " + over.kind + " " +
						(over.retVoid ? "void " : "") + over.name + "(" +
						String.join(" ", over.args) + ")");
					e = over;
				}
				String kind = e == null ? "missing" : e.kind;
				kinds.merge(kind, 1, Integer::sum);
				int nsites = siteCount.getOrDefault(lib + "." + ord, 0);
				if (e == null) {
					unresolved.add(lib + "." + ord);
					tsv.add(lib + "\t" + ord + "\t\tmissing\t" + nsites + "\t" + seg1Only);
					continue;
				}
				loc.setName(loc.getParentNameSpace(), e.name, SourceType.IMPORTED);
				named++;
				tsv.add(lib + "\t" + ord + "\t" + e.name + "\t" + e.kind + "\t" + nsites + "\t" +
					seg1Only);
				if (e.kind.equals("equate")) {
					equates.computeIfAbsent(lib, k -> new HashMap<>()).put(
						Integer.toString(ord), e.value);
				}
				if (setSignature(loc, e)) {
					typed++;
				}
				if (NO_RETURN.contains(lib.toUpperCase() + "." + ord)) {
					Function f = loc.getFunction() != null ? loc.getFunction() : loc.createFunction();
					f.setNoReturn(true);
					noReturn.add(lib + "." + ord + " " + e.name);
				}
				if (RETURNS.contains(lib.toUpperCase() + "." + ord)) {
					Function f = loc.getFunction() != null ? loc.getFunction() : loc.createFunction();
					f.setNoReturn(false);
					returning.add(lib + "." + ord + " " + e.name);
				}
			}
		}
		patchEquates(wh, specs);

		int distinct = 0, distinctBeyond = 0;
		for (Set<Integer> s : all.values()) {
			distinct += s.size();
		}
		for (Set<Integer> s : beyondSeg1.values()) {
			distinctBeyond += s.size();
		}
		List<String> seg1OnlyList = new ArrayList<>();
		for (Map.Entry<String, Set<Integer>> en : all.entrySet()) {
			for (int o : en.getValue()) {
				if (!beyondSeg1.getOrDefault(en.getKey(), Set.of()).contains(o)) {
					seg1OnlyList.add(en.getKey() + "." + o);
				}
			}
		}
		Map<String, Object> imp = new LinkedHashMap<>();
		imp.put("distinct_ordinals", distinct);
		imp.put("distinct_ordinals_outside_seg1", distinctBeyond);
		imp.put("seg1_only", seg1OnlyList);
		imp.put("external_locations", total);
		imp.put("named_from_spec", named);
		imp.put("signatures_set", typed);
		imp.put("by_spec_kind", kinds);
		imp.put("unresolved", unresolved);
		imp.put("no_return", noReturn);
		imp.put("returning", returning);
		imp.put("signature_overrides", overridden);
		report.put("imports", imp);
		Files.write(new File(outDir, moduleName + "-imports.tsv").toPath(), tsv,
			StandardCharsets.UTF_8);
	}

	DataType argType(String a) {
		switch (a) {
			case "word":
				return WordDataType.dataType;
			case "s_word":
				return ShortDataType.dataType;
			case "long":
				return DWordDataType.dataType;
			case "str":
			case "segstr":
				return new PointerDataType(CharDataType.dataType, 4);
			default: // ptr, segptr
				return new PointerDataType(VoidDataType.dataType, 4);
		}
	}

	int argSize(String a) {
		return (a.equals("word") || a.equals("s_word")) ? 2 : 4;
	}

	/**
	 * pascal: callee pops, arguments pushed left to right, so the first argument sits at
	 * the highest stack offset. The x86-16 cspec has no pascal model, so the storage is
	 * custom under __stdcall16far (callee pops, far return address of 4 bytes).
	 */
	boolean setSignature(ExternalLocation loc, SpecEntry e) throws Exception {
		boolean pascal = e.kind.equals("pascal");
		boolean cdecl = e.kind.equals("cdecl") || e.kind.equals("varargs");
		if (!pascal && !cdecl) {
			return false;
		}
		Function f = loc.getFunction();
		if (f == null) {
			f = loc.createFunction();
		}
		Program p = currentProgram;
		Register ax = p.getRegister("AX"), dx = p.getRegister("DX");
		ReturnParameterImpl ret = e.retVoid
				? new ReturnParameterImpl(VoidDataType.dataType, VariableStorage.VOID_STORAGE, p)
				: e.ret16
						? new ReturnParameterImpl(WordDataType.dataType, new VariableStorage(p, ax), p)
						: new ReturnParameterImpl(DWordDataType.dataType,
							new VariableStorage(p, dx, ax), p);
		List<Variable> params = new ArrayList<>();
		int total = 0;
		for (String a : e.args) {
			total += argSize(a);
		}
		if (e.register) {
			// Arguments and results travel in registers (DOS3Call, InitTask, ...).
			String[] regs = REGISTER_ARGS.get(e.name);
			if (regs != null) {
				for (String r : regs) {
					params.add(new ParameterImpl(r.toLowerCase(), WordDataType.dataType,
						new VariableStorage(p, p.getRegister(r)), p));
				}
				ret = new ReturnParameterImpl(WordDataType.dataType, new VariableStorage(p, ax), p);
			}
			f.updateFunction("__stdcall16far", ret, params, FunctionUpdateType.CUSTOM_STORAGE,
				true, SourceType.IMPORTED);
			f.setStackPurgeSize(0);
			f.setComment("Wine spec: -register (arguments in registers)");
			return true;
		}
		int below = 0;
		int[] offs = new int[e.args.size()];
		for (int i = e.args.size() - 1; i >= 0; i--) {
			int sz = argSize(e.args.get(i));
			offs[i] = pascal ? 4 + below : 0;
			below += sz;
		}
		int up = 4;
		for (int i = 0; i < e.args.size(); i++) {
			String a = e.args.get(i);
			int off = pascal ? offs[i] : up;
			up += argSize(a);
			params.add(new ParameterImpl("p" + (i + 1), argType(a),
				new VariableStorage(p, off, argSize(a)), p));
		}
		f.updateFunction(pascal ? "__stdcall16far" : "__cdecl16far", ret, params,
			FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.IMPORTED);
		f.setStackPurgeSize(pascal ? total : 0);
		if (e.note != null) {
			f.setComment(e.note + " (arguments pushed left to right, callee pops " + total + ")");
		}
		else if (pascal) {
			f.setComment("Wine spec: pascal (arguments pushed left to right, callee pops " +
				total + ")");
		}
		return true;
	}

	/**
	 * OFFSET relocations to equate imports (__AHINCR, __AHSHIFT, __WINFLAGS) receive the
	 * equate value at load time; Ghidra writes a thunk offset there instead.
	 */
	void patchEquates(WindowsHeader wh, Map<String, Map<Integer, SpecEntry>> specs)
			throws Exception {
		LengthStringSet[] mods = wh.getModuleReferenceTable().getNames();
		Memory mem = currentProgram.getMemory();
		Map<String, Integer> patched = new TreeMap<>();
		Iterator<Relocation> it = currentProgram.getRelocationTable().getRelocations();
		while (it.hasNext()) {
			Relocation rel = it.next();
			long[] v = rel.getValues();
			if ((v[1] & SegmentRelocation.FLAG_TARGET_MASK) != SegmentRelocation.FLAG_IMPORT_ORDINAL) {
				continue;
			}
			String m = mods[(int) v[3] - 1].getString();
			Map<Integer, SpecEntry> spec = specs.get(m);
			SpecEntry e = spec == null ? null : spec.get((int) v[4]);
			if (e == null || !e.kind.equals("equate")) {
				continue;
			}
			Address a = rel.getAddress();
			if (rel.getType() == SegmentRelocation.TYPE_OFFSET) {
				mem.setShort(a, (short) e.value);
			}
			else if (rel.getType() == SegmentRelocation.TYPE_LO_BYTE) {
				mem.setByte(a, (byte) e.value);
			}
			else {
				problems.add("equate " + e.name + " at " + a + " has relocation type " +
					rel.getType());
				continue;
			}
			setEOLComment(a, m + "." + v[4] + " " + e.name + " = " + String.format("0x%x", e.value) +
				" (equate import)");
			patched.merge(m + "." + v[4] + " " + e.name, 1, Integer::sum);
		}
		report.put("equate_sites_patched", patched);
	}

	/**
	 * Ghidra adds the whole 32-bit placeholder to an additive FAR_ADDR target, which carries
	 * the offset into the selector. Windows stores the selector and adds only the offset.
	 * The call is pointed at the import's thunk and the addend kept in a comment.
	 */
	void fixAdditive() throws Exception {
		Memory mem = currentProgram.getMemory();
		List<String> fixed = new ArrayList<>();
		Iterator<Relocation> it = currentProgram.getRelocationTable().getRelocations();
		while (it.hasNext()) {
			Relocation rel = it.next();
			long[] v = rel.getValues();
			if (rel.getType() != SegmentRelocation.TYPE_FAR_ADDR ||
				(v[1] & SegmentRelocation.FLAG_ADDITIVE) == 0 ||
				(v[1] & SegmentRelocation.FLAG_TARGET_MASK) != SegmentRelocation.FLAG_IMPORT_ORDINAL) {
				continue;
			}
			Address a = rel.getAddress();
			// Find the thunk for this module/ordinal: the one whose external has this label.
			Function target = null;
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				if (!f.isThunk()) {
					continue;
				}
				Function ext = f.getThunkedFunction(true);
				ExternalLocation loc = ext == null ? null
						: currentProgram.getExternalManager().getExternalLocation(ext.getSymbol());
				if (loc != null && (SymbolUtilities.ORDINAL_PREFIX + v[4]).equals(
					loc.getOriginalImportedName() != null ? loc.getOriginalImportedName()
							: loc.getLabel())) {
					target = f;
					break;
				}
			}
			if (target == null) {
				problems.add("additive relocation at " + a + ": no thunk for ordinal " + v[4]);
				continue;
			}
			SegmentedAddress t = (SegmentedAddress) target.getEntryPoint();
			long stored = mem.getInt(a) & 0xffffffffL;
			long base = ((long) t.getSegment() << 16) | t.getSegmentOffset();
			int addend = (int) ((stored - base) & 0xffff);
			mem.setShort(a, (short) t.getSegmentOffset());
			mem.setShort(a.add(2), (short) t.getSegment());
			setEOLComment(a.subtract(1), "additive import: target is " + target.getName() +
				String.format(" + 0x%04x", addend) + " (selector stored, offset added)");
			fixed.add(a + " " + target.getName() + String.format("+0x%04x", addend));
		}
		report.put("additive_far_fixed", fixed);
	}

	int seedAt(Address a, String name, Map<String, Integer> counts, String kind) throws Exception {
		MemoryBlock b = currentProgram.getMemory().getBlock(a);
		if (b == null || !b.isExecute()) {
			counts.merge(kind + "_rejected_not_code", 1, Integer::sum);
			return 0;
		}
		Listing l = currentProgram.getListing();
		if (l.getInstructionAt(a) == null) {
			if (l.getCodeUnitContaining(a) instanceof Instruction) {
				counts.merge(kind + "_rejected_mid_instruction", 1, Integer::sum);
				rejected.add(kind + " " + a + " inside instruction at " +
					l.getCodeUnitContaining(a).getMinAddress());
				return 0;
			}
			disassemble(a);
		}
		if (currentProgram.getFunctionManager().getFunctionAt(a) == null) {
			Function f = createFunction(a, name);
			if (f == null) {
				counts.merge(kind + "_create_failed", 1, Integer::sum);
				return 0;
			}
		}
		else if (name != null) {
			currentProgram.getFunctionManager().getFunctionAt(a).setName(name,
				SourceType.IMPORTED);
		}
		counts.merge(kind, 1, Integer::sum);
		return 1;
	}

	static final byte[][] STRONG_PROLOGUES = {
		{ (byte) 0x8c, (byte) 0xd0, (byte) 0x90, 0x45, 0x55, (byte) 0x8b, (byte) 0xec, 0x1e,
			(byte) 0x8e, (byte) 0xd8 },  // mov ax,ss; nop; inc bp; push bp; mov bp,sp; push ds; mov ds,ax
		{ (byte) 0x8c, (byte) 0xd8, (byte) 0x90, 0x45, 0x55, (byte) 0x8b, (byte) 0xec, 0x1e,
			(byte) 0x8e, (byte) 0xd8 },  // mov ax,ds; nop; ... (exported, patched by the loader)
		{ 0x1e, 0x58, (byte) 0x90, 0x45, 0x55, (byte) 0x8b, (byte) 0xec, 0x1e,
			(byte) 0x8e, (byte) 0xd8 },  // push ds; pop ax; nop; ...
	};

	void seedFunctions(WindowsHeader wh) throws Exception {
		Map<String, Integer> counts = new TreeMap<>();
		SymbolTable st = currentProgram.getSymbolTable();
		String module = args.get("module").toUpperCase();
		// The 10-byte Win16 far prologues are unambiguous; seed them first so that no
		// other flow (for example a fall-through after a non-returning call) eats one.
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (!b.isExecute() || !b.isInitialized()) {
				continue;
			}
			byte[] data = new byte[(int) b.getSize()];
			b.getBytes(b.getStart(), data);
			for (int i = 0; i + 10 <= data.length; i++) {
				for (byte[] pat : STRONG_PROLOGUES) {
					if (match(data, i, pat)) {
						seedAt(b.getStart().add(i), null, counts, "prologue_far");
					}
				}
			}
		}
		// Program entry (CS:IP) and the entry table, named by ordinal.
		InformationBlock ib = wh.getInformationBlock();
		if (ib.getEntryPointSegment() > 0) {
			seedAt(addr(selector(ib.getEntryPointSegment()),
				Short.toUnsignedInt(ib.getEntryPointOffset())), null, counts, "program_entry");
		}
		int ordinal = 0;
		for (EntryTableBundle bundle : wh.getEntryTable().getBundles()) {
			if (bundle.getType() == EntryTableBundle.UNUSED) {
				ordinal += Byte.toUnsignedInt(bundle.getCount());
				continue;
			}
			for (EntryPoint pt : bundle.getEntryPoints()) {
				ordinal++;
				int seg = bundle.isMoveable() ? Byte.toUnsignedInt(pt.getSegment())
						: bundle.getType();
				if (bundle.isConstant() || seg < 1) {
					continue;
				}
				Address a = addr(selector(seg), Short.toUnsignedInt(pt.getOffset()));
				st.createLabel(a, SymbolUtilities.ORDINAL_PREFIX + ordinal, SourceType.IMPORTED);
				String name = null;
				Symbol prim = st.getPrimarySymbol(a);
				if (prim == null || prim.getName().startsWith(SymbolUtilities.ORDINAL_PREFIX)) {
					name = module + "_Ordinal_" + ordinal;
				}
				seedAt(a, name, counts, "export");
			}
		}
		// Internal far-call targets: SEGMENT relocations whose site is the selector word
		// of a far call (9A) or far jump (EA).
		Memory mem = currentProgram.getMemory();
		Set<Address> targets = new TreeSet<>();
		Iterator<Relocation> it = currentProgram.getRelocationTable().getRelocations();
		while (it.hasNext()) {
			Relocation rel = it.next();
			long[] v = rel.getValues();
			if (rel.getType() != SegmentRelocation.TYPE_SEGMENT ||
				(v[1] & SegmentRelocation.FLAG_TARGET_MASK) != SegmentRelocation.FLAG_INTERNAL_REF) {
				continue;
			}
			Address site = rel.getAddress();
			if (((SegmentedAddress) site).getSegmentOffset() < 3) {
				continue;
			}
			int op = mem.getByte(site.subtract(3)) & 0xff;
			if (op != 0x9a && op != 0xea) {
				counts.merge("far_target_site_not_call", 1, Integer::sum);
				continue;
			}
			int off = mem.getShort(site.subtract(2)) & 0xffff;
			int sel = mem.getShort(site) & 0xffff;
			targets.add(addr(sel, off));
		}
		for (Address a : targets) {
			seedAt(a, null, counts, "far_call_target");
		}
		report.put("seeds", counts);
		report.put("seeds_rejected", rejected);
	}

	static boolean match(byte[] data, int i, byte[] pat) {
		if (i + pat.length > data.length) {
			return false;
		}
		for (int k = 0; k < pat.length; k++) {
			if (data[i + k] != pat[k]) {
				return false;
			}
		}
		return true;
	}

	void setAnalysisOptions() {
		Map<String, String> want = new LinkedHashMap<>();
		want.put("Aggressive Instruction Finder", "false");
		Map<String, String> have = getCurrentAnalysisOptionsAndValues(currentProgram);
		Map<String, String> applied = new LinkedHashMap<>();
		for (Map.Entry<String, String> e : want.entrySet()) {
			if (have.containsKey(e.getKey())) {
				setAnalysisOption(currentProgram, e.getKey(), e.getValue());
				applied.put(e.getKey(), e.getValue());
			}
			else {
				problems.add("analysis option not found: " + e.getKey());
			}
		}
		report.put("analysis_options", applied);
	}
}
