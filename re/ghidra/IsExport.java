// Inner Space decompiled-C export: one file per function plus an index.
// Runs as the last -postScript. Addresses are written in the canonical segN:0xOFF
// form (N = (selector - 0x1000) / 8 + 1).
//
// Arguments (key=value): module=<name> dir=<output dir> report=<dir>
//                        [threads=N] [timeout=seconds]
// Writes <dir>/segNN_0xOFF.c for every function in a code segment, <dir>/index.tsv
// and <report>/<module>-export.json.
//@category InnerSpace

import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;

import com.google.gson.GsonBuilder;

import ghidra.app.decompiler.*;
import ghidra.app.decompiler.parallel.DecompileConfigurer;
import ghidra.app.decompiler.parallel.DecompilerCallback;
import ghidra.app.decompiler.parallel.ParallelDecompiler;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.SegmentedAddress;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.util.task.TaskMonitor;
import generic.concurrent.GThreadPool;

public class IsExport extends GhidraScript {

	static final int SEL_BASE = 0x1000;

	static int segOf(SegmentedAddress a) {
		return (a.getSegment() - SEL_BASE) / 8 + 1;
	}

	static class Row {
		Function f;
		int seg, off;
		String file;
		String c;
		String status = "ok";
	}

	@Override
	public void run() throws Exception {
		Map<String, String> args = new HashMap<>();
		for (String a : getScriptArgs()) {
			int eq = a.indexOf('=');
			if (eq > 0) {
				args.put(a.substring(0, eq), a.substring(eq + 1));
			}
		}
		String module = args.get("module");
		File dir = new File(args.get("dir"));
		File reportDir = new File(args.get("report"));
		int threads = Integer.parseInt(args.getOrDefault("threads", "2"));
		int timeout = Integer.parseInt(args.getOrDefault("timeout", "120"));
		dir.mkdirs();
		File[] old = dir.listFiles((d, n) -> n.endsWith(".c") || n.equals("index.tsv"));
		if (old != null) {
			for (File f : old) {
				f.delete();
			}
		}

		Map<Function, Row> rows = new LinkedHashMap<>();
		List<Function> funcs = new ArrayList<>();
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			MemoryBlock b = currentProgram.getMemory().getBlock(f.getEntryPoint());
			if (b == null || !b.isExecute()) {
				continue;
			}
			Row r = new Row();
			r.f = f;
			SegmentedAddress a = (SegmentedAddress) f.getEntryPoint();
			r.seg = segOf(a);
			r.off = a.getSegmentOffset();
			r.file = String.format("seg%02d_0x%x.c", r.seg, r.off);
			rows.put(f, r);
			funcs.add(f);
		}

		long t0 = System.currentTimeMillis();
		GThreadPool.getSharedThreadPool("Parallel Decompiler").setMaxThreadCount(threads);
		DecompileConfigurer cfg = di -> {
			DecompileOptions o = new DecompileOptions();
			o.grabFromProgram(currentProgram);
			di.setOptions(o);
			di.toggleCCode(true);
			di.toggleSyntaxTree(false);
			di.setSimplificationStyle("decompile");
		};
		DecompilerCallback<Object> cb = new DecompilerCallback<>(currentProgram, cfg) {
			@Override
			public Object process(DecompileResults res, TaskMonitor m) {
				Row r = rows.get(res.getFunction());
				if (res.decompileCompleted() && res.getDecompiledFunction() != null) {
					r.c = res.getDecompiledFunction().getC();
				}
				else {
					r.status = "failed";
					String err = res.getErrorMessage();
					r.c = "/* decompilation failed: " +
						(err == null ? "" : err.trim().replace("*/", "* /")) + " */\n";
				}
				return null;
			}
		};
		cb.setTimeout(timeout);
		try {
			ParallelDecompiler.decompileFunctions(cb, funcs, monitor);
		}
		finally {
			cb.dispose();
		}
		long ms = System.currentTimeMillis() - t0;

		List<Row> sorted = new ArrayList<>(rows.values());
		sorted.sort(Comparator.comparingInt((Row r) -> r.seg).thenComparingInt(r -> r.off));
		List<String> index = new ArrayList<>();
		index.add("address\tghidra\tname\tsize\tcallers\tcall_sites\tstatus\tfile");
		int failed = 0;
		for (Row r : sorted) {
			Function f = r.f;
			Set<Function> callers = f.getCallingFunctions(monitor);
			int sites = 0;
			for (Reference ref : getReferencesTo(f.getEntryPoint())) {
				if (ref.getReferenceType().isCall()) {
					sites++;
				}
			}
			String canon = String.format("seg%d:0x%x", r.seg, r.off);
			String ghidra = String.format("%04x:%04x",
				((SegmentedAddress) f.getEntryPoint()).getSegment(), r.off);
			long size = f.getBody().getNumAddresses();
			String name = f.getName(true);
			if (r.c == null) {
				r.status = "failed";
				r.c = "/* decompilation produced no result */\n";
			}
			if (r.status.equals("failed")) {
				failed++;
			}
			String header = "// " + module + " " + canon + "  (Ghidra " + ghidra + ")  " + name +
				"\n// size " + size + "  callers " + callers.size() + "  call sites " + sites +
				"  convention " + f.getCallingConventionName() + "\n";
			Files.write(new File(dir, r.file).toPath(),
				(header + r.c).getBytes(StandardCharsets.UTF_8));
			index.add(canon + "\t" + ghidra + "\t" + name + "\t" + size + "\t" + callers.size() +
				"\t" + sites + "\t" + r.status + "\t" + r.file);
		}
		Files.write(new File(dir, "index.tsv").toPath(), index, StandardCharsets.UTF_8);

		Map<String, Object> rep = new LinkedHashMap<>();
		rep.put("module", module);
		rep.put("functions", sorted.size());
		rep.put("failed", failed);
		rep.put("threads", threads);
		rep.put("decompile_ms", ms);
		try (PrintWriter pw = new PrintWriter(new File(reportDir, module + "-export.json"),
			"UTF-8")) {
			pw.println(new GsonBuilder().setPrettyPrinting().create().toJson(rep));
		}
		println("IsExport " + module + ": " + sorted.size() + " functions, " + failed +
			" failed, " + ms + " ms");
	}
}
