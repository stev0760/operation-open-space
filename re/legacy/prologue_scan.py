#!/usr/bin/env python3
"""Enumerate far procedures directly from unpacked segment bytes.

The Borland/TLINK far-procedure prologue is a fixed 9-byte signature:

    8c d0        mov %ss,%ax        ; save caller's DS
    90           nop
    45           inc %bp
    55           push %bp
    8b ec        mov %sp,%bp
    1e           push %ds
    8e d8        mov %ax,%ds        ; restore DS

Verified against two independently known entries (1018:2da2 = FUN_1000_2ea2 and
1018:144c), so this locates function ENTRY points from raw bytes -- no decompiler,
no disassembler, no heuristics. Compare the result against the 122-entry
cross-segment call graph (callgraph.json), which is only a lower bound because
same-segment near calls are not recoverable from relocation records.

Note: NEAR procedures use a shorter prologue and are NOT found by this scan.
"""
import glob, json, os, sys

SIG = bytes.fromhex("8cd090455 5 8bec1e8ed8".replace(" ", ""))

def scan(data):
    out, i = [], 0
    while True:
        j = data.find(SIG, i)
        if j < 0:
            break
        out.append(j)
        i = j + 1
    return out

if __name__ == "__main__":
    print("signature:", SIG.hex())
    total = 0
    per_seg = {}
    allf = {}
    for path in sorted(glob.glob("unpacked/seg*.bin"),
                       key=lambda p: int(''.join(c for c in os.path.basename(p) if c.isdigit()))):
        seg = int(''.join(c for c in os.path.basename(path) if c.isdigit()))
        data = open(path, "rb").read()
        hits = scan(data)
        per_seg[seg] = hits
        allf[seg] = hits
        total += len(hits)
        flag = "  <-- DATA segment?" if seg == 1 else ""
        print(f"  seg{seg:<3} size={len(data):>7}  prologues={len(hits):>4}{flag}")

    print(f"\nTOTAL far-procedure prologues: {total}")

    # cross-check against the call-graph lower bound
    try:
        cg = json.load(open("callgraph.json"))["entries"]
        want = {(e["tseg"], e["toff"]) for e in cg}
    except Exception as e:
        print("callgraph load failed:", e)
        want = set()

    found = {(s, o) for s, offs in allf.items() for o in offs}
    missing = sorted(want - found)
    print(f"\ncall-graph entries: {len(want)}")
    print(f"  located by prologue scan : {len(want) - len(missing)}")
    print(f"  NOT located (near procs?) : {len(missing)}")
    for s, o in missing[:30]:
        print(f"    seg{s}:{o:#06x}")

    extra = len(found) - (len(want) - len(missing))
    print(f"\nfunctions found only by the scan (recovered beyond the call graph): {extra}")
    print(f"  -> total inventory is at least {total}, vs 122 known cross-segment entries")

    json.dump({str(k): v for k, v in sorted(allf.items())},
              open("function-entries.json", "w"), indent=1)
    print("\nwrote function-entries.json")
