/*
 * isprobe: read-only memory probe for the running original (INSPACE.EXE v1.29.08 under Wine).
 *
 * Build:  oracle/probe/build.sh [OUTDIR]       -> OUTDIR/isprobe.exe (default work/probe/)
 *         i686-w64-mingw32-gcc -static -O2; a 32-bit Win32 console program.
 * Run:    in the game's own Wine prefix while the game runs, from the repo root, e.g.
 *           ORACLE_ID=3 source oracle/env.sh; wine work/probe/isprobe.exe find
 *         Give the exe as a relative path: when no drive maps the repo (newgame.sh removes Z:),
 *         Wine 11.0 segfaults at start-up on an absolute Unix path to the exe.
 *         oracle/probe/probe_run.sh runs the probe beside oracle/scenarios/newgame.sh.
 *
 * Usage:  isprobe [OPTIONS] COMMAND ARGS
 *   find                                   locate the DGROUPs of INSPACE.EXE and ISPACE.DLL
 *   read  MODULE DGROUP:0xOFF LEN          hex + ASCII dump of LEN bytes
 *   dump  MODULE DGROUP OUT [LEN|all]      write LEN bytes from DGROUP:0x0 to OUT ("-" = stdout);
 *                                          default LEN is the NE minalloc, "all" is selector limit + 1
 *   watch MODULE DGROUP:0xOFF LEN MS COUNT poll every MS ms, COUNT times (0 = until killed);
 *                                          prints a timestamped line at the start and on every change
 *   ldt   SEL                              linear base and limit of LDT selector SEL (hex)
 * MODULE is INSPACE or ISPACE (INSPACE.EXE, ISPACE.DLL; any case). Numbers: LEN, MS and COUNT take
 * decimal or 0x hex; offsets and selectors are hex.
 * Options:
 *   --dry-run   parse the arguments, print the parsed command as key=value pairs, touch no process
 *   --sel SEL   take the DGROUP base from LDT selector SEL instead of scanning (watch then follows
 *               the selector if Wine moves the block)
 *   --base ADDR take the DGROUP base as linear address ADDR (hex) instead of scanning
 *   --pid PID   target process id (decimal or 0x hex) instead of the window lookup
 * Exit status: 0 ok, 2 usage, 3 no target process, 4 DGROUP not found or ambiguous, 5 read/write error.
 *
 * How it finds DGROUP:
 *   - Target: the owner of the window of class "Inner Space" (FindWindow + GetWindowThreadProcessId,
 *     which also gives the 16-bit task's thread); without that window, the first winevdm.exe process.
 *     Under Wine every Win16 task runs as a thread of a 32-bit winevdm.exe process, and its 16-bit
 *     segments are ordinary memory of that process, so a selector's base is a linear address there.
 *   - Opened with PROCESS_VM_READ | PROCESS_QUERY_INFORMATION and the thread with
 *     THREAD_QUERY_INFORMATION. The probe never writes to the target.
 *   - Scan route (default): VirtualQueryEx over the address space, ReadProcessMemory of every
 *     committed region, search for each module's first anchor. A hit gives base = hit - anchor
 *     offset, and every other anchor must match at its offset from that base. The anchors are runs of
 *     the initialised DGROUP bytes (work/unpacked/inspace/seg19.bin, work/unpacked/ispace/seg11.bin)
 *     that occur once in their image; oracle/tests/test_probe.py checks them against the images.
 *   - Selector route: GetThreadSelectorEntry on the game thread. After a scan, `find` walks the LDT
 *     (selectors 0x0007..0xffff) for the selectors whose base equals the scanned base and reports
 *     them with their limits. Wine 11.0 answers GetThreadSelectorEntry for another process from the
 *     LDT copy that the target publishes through its PEB.
 *   - Cost to the target: ReadProcessMemory and GetThreadSelectorEntry are served by wineserver
 *     (process_vm_readv) without stopping the game; each cross-process VirtualQueryEx is a system
 *     APC that the game process itself answers [REF Wine 11.0 server/ptrace.c, dlls/ntdll/unix/virtual.c].
 *     So `find` (about 500 regions) briefly interrupts the game; use --sel afterwards.
 * Under Wine with no Z: drive (oracle/scenarios/newgame.sh removes it), give dump a C:\ path or "-".
 */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <fcntl.h>
#include <io.h>
#else
#include <strings.h>
#endif

#define ANCHOR_MAX 64
#define SEG_SIZE 0x10000UL
#define WATCH_MAX 0x1000U

struct anchor {
    unsigned off;     /* DGROUP offset */
    const char *hex;  /* expected bytes */
};

struct module {
    const char *name;       /* canonical module file name */
    const char *shortname;  /* name as typed on the command line */
    const char *ext;
    unsigned autodata;      /* NE segment number of DGROUP */
    unsigned minalloc;      /* NE minalloc of DGROUP */
    const struct anchor *anchors;
    int nanchors;
};

/* Anchors: offset, bytes of the initialised DGROUP image. The first one is the scan signature. */
static const struct anchor inspace_anchors[] = {
    /* "Another version of game already running.\0Insufficient memory to " */
    { 0x04a0, "416e6f746865722076657273696f6e206f662067616d6520616c72656164792072756e6e696e672e00496e73756666696369656e74206d656d6f727920746f20" },
    /* "Some game settings are missing. Will attempt to " */
    { 0x02d6, "536f6d652067616d652073657474696e677320617265206d697373696e672e2057696c6c20617474656d707420746f20" },
    /* " 1994-2000 Software Dynamics, inc. All rights re" */
    { 0x12d3, "20313939342d3230303020536f6674776172652044796e616d6963732c20696e632e20416c6c20726967687473207265" },
};

static const struct anchor ispace_anchors[] = {
    /* "WaveMixGetInfo\0WaveMixConfigureInit\0WaveMixOpenChannel\0WaveMixAc" */
    { 0x06b6, "576176654d6978476574496e666f00576176654d6978436f6e666967757265496e697400576176654d69784f70656e4368616e6e656c00576176654d69784163" },
    /* "Wave\0drivers\0\\control.ini\0Sound Card\0drivers.des" */
    { 0x007a, "576176650064726976657273005c636f6e74726f6c2e696e6900536f756e64204361726400647269766572732e646573" },
    /* "INSPACE.EXE\0INSTALL.EXE\0LevClass\0\0NOTEPAD \0ORDER" */
    { 0x01b2, "494e53504143452e45584500494e5354414c4c2e455845004c6576436c61737300004e4f544550414420004f52444552" },
};

static const struct module modules[] = {
    { "INSPACE.EXE", "INSPACE", "EXE", 19, 41456, inspace_anchors, 3 },
    { "ISPACE.DLL", "ISPACE", "DLL", 11, 8876, ispace_anchors, 3 },
};
#define NMODULES ((int)(sizeof modules / sizeof modules[0]))

enum cmd { CMD_NONE, CMD_FIND, CMD_READ, CMD_DUMP, CMD_WATCH, CMD_LDT };

struct args {
    enum cmd cmd;
    int dry_run;
    const struct module *mod;
    unsigned long off, len;
    int len_all;
    const char *out;
    unsigned long ms, count;
    unsigned long sel;
    int have_sel;
    unsigned long base;
    int have_base;
    unsigned long pid;
    int have_pid;
};

static void usage(void)
{
    fputs("usage: isprobe [--dry-run] [--sel SEL | --base ADDR] [--pid PID] COMMAND ARGS\n"
          "  find\n"
          "  read  MODULE DGROUP:0xOFF LEN\n"
          "  dump  MODULE DGROUP OUT [LEN|all]\n"
          "  watch MODULE DGROUP:0xOFF LEN MS COUNT\n"
          "  ldt   SEL\n"
          "MODULE: INSPACE | ISPACE\n", stderr);
}

static int fail_usage(const char *fmt, ...)
{
    va_list ap;
    fputs("isprobe: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    usage();
    return 2;
}

/* ------------------------------------------------------------------ parsing */

static int parse_num(const char *s, int base, unsigned long max, unsigned long *out)
{
    char *end;
    unsigned long v;
    if (!s || !*s || *s == '-' || *s == '+' || isspace((unsigned char)*s)) return 0;
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) return 0;
    errno = 0;
    v = strtoul(s, &end, base);
    if (errno || *end || v > max) return 0;
    *out = v;
    return 1;
}

static const struct module *parse_module(const char *s)
{
    int i;
    for (i = 0; i < NMODULES; i++) {
        const struct module *m = &modules[i];
        size_t n = strlen(m->shortname);
        if (!strcasecmp(s, m->shortname) || !strcasecmp(s, m->name)) return m;
        if (!strncasecmp(s, m->shortname, n) && s[n] == '.' && !strcasecmp(s + n + 1, m->ext)) return m;
    }
    return NULL;
}

static int parse_dgroup_off(const char *s, unsigned long *off)
{
    if (strncasecmp(s, "DGROUP:", 7)) return 0;
    return parse_num(s + 7, 16, SEG_SIZE - 1, off);
}

static int parse_args(int argc, char **argv, struct args *a)
{
    const char *pos[8];
    int npos = 0, i;
    memset(a, 0, sizeof *a);
    for (i = 1; i < argc; i++) {
        const char *s = argv[i];
        if (!strcmp(s, "--dry-run")) a->dry_run = 1;
        else if (!strcmp(s, "--sel") || !strcmp(s, "--base") || !strcmp(s, "--pid")) {
            if (i + 1 >= argc) return fail_usage("%s needs a value", s);
            if (!strcmp(s, "--sel")) {
                if (!parse_num(argv[++i], 16, 0xffff, &a->sel) || !(a->sel & 4))
                    return fail_usage("bad LDT selector '%s'", argv[i]);
                a->have_sel = 1;
            } else if (!strcmp(s, "--base")) {
                if (!parse_num(argv[++i], 16, 0xffffffffUL, &a->base) || !a->base)
                    return fail_usage("bad linear address '%s'", argv[i]);
                a->have_base = 1;
            } else {
                if (!parse_num(argv[++i], 0, 0xffffffffUL, &a->pid) || !a->pid)
                    return fail_usage("bad process id '%s'", argv[i]);
                a->have_pid = 1;
            }
        } else if (s[0] == '-' && s[1] == '-') return fail_usage("unknown option '%s'", s);
        else {
            if (npos >= (int)(sizeof pos / sizeof pos[0])) return fail_usage("too many arguments");
            pos[npos++] = s;
        }
    }
    if (a->have_sel && a->have_base) return fail_usage("--sel and --base exclude each other");
    if (npos == 0) return fail_usage("no command");

#define NEED(n) do { if (npos != (n)) return fail_usage("%s takes %d argument(s)", pos[0], (n) - 1); } while (0)
    if (!strcmp(pos[0], "find")) {
        NEED(1);
        a->cmd = CMD_FIND;
        if (a->have_sel || a->have_base) return fail_usage("find always scans; --sel/--base apply to read, dump and watch");
        return 0;
    }
    if (!strcmp(pos[0], "ldt")) {
        NEED(2);
        a->cmd = CMD_LDT;
        if (!parse_num(pos[1], 16, 0xffff, &a->sel) || !(a->sel & 4)) return fail_usage("bad LDT selector '%s'", pos[1]);
        return 0;
    }
    if (!strcmp(pos[0], "read")) { NEED(4); a->cmd = CMD_READ; }
    else if (!strcmp(pos[0], "watch")) { NEED(6); a->cmd = CMD_WATCH; }
    else if (!strcmp(pos[0], "dump")) {
        if (npos != 4 && npos != 5) return fail_usage("dump takes 3 or 4 arguments");
        a->cmd = CMD_DUMP;
    } else return fail_usage("unknown command '%s'", pos[0]);
#undef NEED

    if (!(a->mod = parse_module(pos[1]))) return fail_usage("unknown module '%s' (INSPACE or ISPACE)", pos[1]);
    if (a->cmd == CMD_DUMP) {
        if (strcasecmp(pos[2], "DGROUP")) return fail_usage("dump reads the whole DGROUP: write 'DGROUP', not '%s'", pos[2]);
        a->out = pos[3];
        a->len = a->mod->minalloc;
        if (npos == 5) {
            if (!strcasecmp(pos[4], "all")) a->len_all = 1;
            else if (!parse_num(pos[4], 0, SEG_SIZE, &a->len) || !a->len) return fail_usage("bad length '%s'", pos[4]);
        }
        return 0;
    }
    if (!parse_dgroup_off(pos[2], &a->off)) return fail_usage("bad offset '%s' (want DGROUP:0xOFF)", pos[2]);
    if (!parse_num(pos[3], 0, SEG_SIZE, &a->len) || !a->len) return fail_usage("bad length '%s'", pos[3]);
    if (a->off + a->len > SEG_SIZE) return fail_usage("DGROUP:0x%lx + %lu runs past the 64 KB segment", a->off, a->len);
    if (a->cmd == CMD_WATCH) {
        if (a->len > WATCH_MAX) return fail_usage("watch length is at most %u", WATCH_MAX);
        if (!parse_num(pos[4], 0, 3600000UL, &a->ms) || !a->ms) return fail_usage("bad interval '%s' (ms)", pos[4]);
        if (!parse_num(pos[5], 0, 0xffffffffUL, &a->count)) return fail_usage("bad count '%s'", pos[5]);
    }
    return 0;
}

static void print_route(const struct args *a)
{
    if (a->have_sel) printf(" route=sel:0x%04lx", a->sel);
    else if (a->have_base) printf(" route=base:0x%08lx", a->base);
    else printf(" route=scan");
    if (a->have_pid) printf(" pid=%lu", a->pid);
}

static int dry_run(const struct args *a)
{
    static const char *names[] = { "none", "find", "read", "dump", "watch", "ldt" };
    printf("dry-run cmd=%s", names[a->cmd]);
    switch (a->cmd) {
    case CMD_FIND: break;
    case CMD_LDT: printf(" sel=0x%04lx", a->sel); break;
    case CMD_READ: printf(" module=%s off=0x%04lx len=%lu", a->mod->name, a->off, a->len); break;
    case CMD_DUMP:
        if (a->len_all) printf(" module=%s len=all out=%s", a->mod->name, a->out);
        else printf(" module=%s len=%lu out=%s", a->mod->name, a->len, a->out);
        break;
    case CMD_WATCH:
        printf(" module=%s off=0x%04lx len=%lu ms=%lu count=%lu", a->mod->name, a->off, a->len, a->ms, a->count);
        break;
    default: break;
    }
    if (a->cmd != CMD_LDT) print_route(a);
    else if (a->have_pid) printf(" pid=%lu", a->pid);
    printf("\n");
    return 0;
}

#ifdef _WIN32
/* ------------------------------------------------------------------ target access (read only) */

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int decode_hex(const char *hex, unsigned char *out, int max)
{
    int n = 0;
    while (hex[0] && hex[1] && n < max) {
        int hi = hexval(hex[0]), lo = hexval(hex[1]);
        if (hi < 0 || lo < 0) return -1;
        out[n++] = (unsigned char)(hi << 4 | lo);
        hex += 2;
    }
    return n;
}

static void hexdump(unsigned long off, const unsigned char *p, unsigned long len)
{
    unsigned long row, i;
    for (row = off & ~15UL; row < off + len; row += 16) {
        printf("DGROUP:0x%04lx ", row);
        for (i = row; i < row + 16; i++) {
            if (i < off || i >= off + len) printf("   ");
            else printf(" %02x", p[i - off]);
            if (i == row + 7) printf(" ");
        }
        printf("  |");
        for (i = row; i < row + 16; i++) {
            int c = (i < off || i >= off + len) ? ' ' : p[i - off];
            putchar(c >= 0x20 && c < 0x7f ? c : '.');
        }
        printf("|\n");
    }
}

static HANDLE g_proc, g_thread;
static DWORD g_pid, g_tid;

static DWORD first_thread(DWORD pid)
{
    THREADENTRY32 te;
    DWORD tid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    te.dwSize = sizeof te;
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) { tid = te.th32ThreadID; break; }
            te.dwSize = sizeof te;
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
    return tid;
}

static DWORD find_process(const char *exe)
{
    PROCESSENTRY32 pe;
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof pe;
    if (Process32First(snap, &pe)) {
        do {
            const char *base = strrchr(pe.szExeFile, '\\');
            base = base ? base + 1 : pe.szExeFile;
            if (!strcasecmp(base, exe)) { pid = pe.th32ProcessID; break; }
            pe.dwSize = sizeof pe;
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static int open_target(const struct args *a, int verbose)
{
    const char *how;
    HWND w = NULL;
    if (a->have_pid) {
        g_pid = (DWORD)a->pid;
        g_tid = first_thread(g_pid);
        how = "--pid";
    } else if ((w = FindWindowA("Inner Space", NULL)) != NULL) {
        g_tid = GetWindowThreadProcessId(w, &g_pid);
        how = "window class \"Inner Space\"";
    } else {
        g_pid = find_process("winevdm.exe");
        g_tid = g_pid ? first_thread(g_pid) : 0;
        how = "process winevdm.exe";
    }
    if (!g_pid) {
        fprintf(stderr, "isprobe: no window of class \"Inner Space\" and no winevdm.exe process\n");
        return 3;
    }
    g_proc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, g_pid);
    if (!g_proc) {
        fprintf(stderr, "isprobe: OpenProcess(%lu) failed, error %lu\n", (unsigned long)g_pid, GetLastError());
        return 3;
    }
    if (g_tid) g_thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, g_tid);
    if (verbose)
        printf("target pid 0x%04lx thread 0x%04lx (%s)%s\n", (unsigned long)g_pid, (unsigned long)g_tid, how,
               g_thread ? "" : " [no thread handle: no selector route]");
    return 0;
}

static int read_mem(unsigned long addr, void *buf, unsigned long len)
{
    SIZE_T got = 0;
    if (!ReadProcessMemory(g_proc, (LPCVOID)(ULONG_PTR)addr, buf, len, &got)) return 0;
    return got == len;
}

struct selinfo {
    unsigned long base, limit;
    int code, present, big;
};

static int sel_entry(unsigned long sel, struct selinfo *si)
{
    LDT_ENTRY e;
    if (!g_thread || !GetThreadSelectorEntry(g_thread, (DWORD)sel, &e)) return 0;
    si->base = e.BaseLow | ((unsigned long)e.HighWord.Bytes.BaseMid << 16) | ((unsigned long)e.HighWord.Bytes.BaseHi << 24);
    si->limit = e.LimitLow | ((unsigned long)e.HighWord.Bits.LimitHi << 16);
    if (e.HighWord.Bits.Granularity) si->limit = (si->limit << 12) | 0xfff;
    si->code = (e.HighWord.Bits.Type & 0x8) != 0;
    si->present = e.HighWord.Bits.Pres;
    si->big = e.HighWord.Bits.Default_Big;
    return 1;
}

static int anchors_ok(const struct module *m, unsigned long base, int report)
{
    int i, ok = 0;
    for (i = 0; i < m->nanchors; i++) {
        unsigned char want[ANCHOR_MAX], got[ANCHOR_MAX];
        int n = decode_hex(m->anchors[i].hex, want, ANCHOR_MAX);
        int good = n > 0 && read_mem(base + m->anchors[i].off, got, (unsigned long)n) && !memcmp(want, got, (size_t)n);
        ok += good;
        if (report && !good)
            fprintf(stderr, "isprobe: %s anchor at DGROUP:0x%04x (%d bytes) differs at base 0x%08lx\n",
                    m->name, m->anchors[i].off, n, base);
    }
    return ok;
}

#define MAX_CAND 16
struct scanres {
    unsigned long base[MAX_CAND];
    int ok[MAX_CAND];
    int n;
    unsigned long regions, hits;
    unsigned long long bytes;
    unsigned long reached;  /* address where the region walk ended */
    unsigned long qerr;     /* GetLastError of a failed VirtualQueryEx, 0 if the walk reached the end */
};

static void scan_module(const struct module *m, struct scanres *r)
{
    enum { CHUNK = 1 << 20 };
    unsigned char sig[ANCHOR_MAX], *buf;
    int siglen = decode_hex(m->anchors[0].hex, sig, ANCHOR_MAX);
    SYSTEM_INFO si;
    ULONG_PTR addr, end;
    memset(r, 0, sizeof *r);
    buf = malloc(CHUNK + ANCHOR_MAX);
    if (!buf || siglen <= 0) { free(buf); return; }
    GetSystemInfo(&si);
    addr = (ULONG_PTR)si.lpMinimumApplicationAddress;
    end = (ULONG_PTR)si.lpMaximumApplicationAddress;
    while (addr < end) {
        MEMORY_BASIC_INFORMATION mbi;
        ULONG_PTR rbase, rend, p;
        if (!VirtualQueryEx(g_proc, (LPCVOID)addr, &mbi, sizeof mbi)) { r->qerr = GetLastError(); break; }
        rbase = (ULONG_PTR)mbi.BaseAddress;
        rend = rbase + mbi.RegionSize;
        r->regions++;
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            for (p = rbase; p < rend; p += CHUNK) {
                SIZE_T want = rend - p, got = 0, i;
                if (want > (SIZE_T)CHUNK + siglen - 1) want = (SIZE_T)CHUNK + siglen - 1;
                ReadProcessMemory(g_proc, (LPCVOID)p, buf, want, &got);
                r->bytes += got < CHUNK ? got : CHUNK;
                for (i = 0; got >= (SIZE_T)siglen && i <= got - siglen && i < CHUNK; i++) {
                    const unsigned char *q = memchr(buf + i, sig[0], got - siglen + 1 - i);
                    if (!q) break;
                    i = (SIZE_T)(q - buf);
                    if (i >= CHUNK) break;
                    if (!memcmp(q, sig, (size_t)siglen)) {
                        unsigned long base = (unsigned long)(p + i) - m->anchors[0].off;
                        r->hits++;
                        if (r->n < MAX_CAND) {
                            r->base[r->n] = base;
                            r->ok[r->n] = anchors_ok(m, base, 0);
                            r->n++;
                        }
                    }
                }
            }
        }
        if (rend <= addr) break;
        addr = rend;
    }
    r->reached = (unsigned long)addr;
    free(buf);
}

/* All LDT selectors whose base is `base`; returns the count, fills up to `max`. */
static int ldt_lookup(unsigned long base, unsigned long *sels, struct selinfo *infos, int max)
{
    unsigned long idx;
    int n = 0;
    for (idx = 1; idx < 8192; idx++) {
        struct selinfo si;
        unsigned long sel = idx << 3 | 7;
        if (sel_entry(sel, &si) && si.present && si.base == base) {
            if (n < max) { sels[n] = sel; infos[n] = si; }
            n++;
        }
    }
    return n;
}

struct dgroup {
    unsigned long base;
    int have_sel;
    unsigned long sel;
    struct selinfo si;
};

/* Picks the data selector with the largest limit among those that share the base. */
static void attach_selector(struct dgroup *d)
{
    unsigned long sels[16];
    struct selinfo infos[16];
    int n = ldt_lookup(d->base, sels, infos, 16), i;
    if (n > 16) n = 16;
    for (i = 0; i < n; i++)
        if (!infos[i].code && (!d->have_sel || infos[i].limit > d->si.limit)) {
            d->have_sel = 1;
            d->sel = sels[i];
            d->si = infos[i];
        }
}

static int resolve(const struct args *a, const struct module *m, struct dgroup *d, int want_sel)
{
    memset(d, 0, sizeof *d);
    if (a->have_base) d->base = a->base;
    else if (a->have_sel) {
        if (!sel_entry(a->sel, &d->si) || !d->si.present) {
            fprintf(stderr, "isprobe: GetThreadSelectorEntry(0x%04lx) failed, error %lu\n", a->sel, GetLastError());
            return 4;
        }
        d->base = d->si.base;
        d->sel = a->sel;
        d->have_sel = 1;
    } else {
        struct scanres r;
        int i, full = 0;
        scan_module(m, &r);
        for (i = 0; i < r.n; i++)
            if (r.ok[i] == m->nanchors) { full++; d->base = r.base[i]; }
        if (full == 0) {
            fprintf(stderr, "isprobe: %s DGROUP not found (%lu signature hits, %lu regions)\n", m->name, r.hits, r.regions);
            for (i = 0; i < r.n; i++)
                fprintf(stderr, "  candidate 0x%08lx anchors %d/%d\n", r.base[i], r.ok[i], m->nanchors);
            return 4;
        }
        if (full > 1) {
            int withsel = 0;
            unsigned long pick = 0;
            for (i = 0; i < r.n; i++) {
                unsigned long s[1];
                struct selinfo inf[1];
                if (r.ok[i] == m->nanchors && ldt_lookup(r.base[i], s, inf, 1) > 0) { withsel++; pick = r.base[i]; }
            }
            if (withsel != 1) {
                fprintf(stderr, "isprobe: %s DGROUP ambiguous: %d full matches, %d with an LDT selector\n", m->name, full, withsel);
                return 4;
            }
            d->base = pick;
        }
        if (want_sel) attach_selector(d);
    }
    if (anchors_ok(m, d->base, 1) != m->nanchors)
        fprintf(stderr, "isprobe: warning: %s anchors not all intact at base 0x%08lx\n", m->name, d->base);
    return 0;
}

static int cmd_find(const struct args *a)
{
    int i, rc = open_target(a, 1), worst = 0;
    if (rc) return rc;
    for (i = 0; i < NMODULES; i++) {
        const struct module *m = &modules[i];
        struct scanres r;
        unsigned long sels[16];
        struct selinfo infos[16];
        int j, full = 0, n;
        DWORD t0 = GetTickCount(), t1;
        scan_module(m, &r);
        t1 = GetTickCount();
        for (j = 0; j < r.n; j++) full += r.ok[j] == m->nanchors;
        printf("%-11s scan: %lu regions up to 0x%08lx%s, %llu bytes read, %lu signature hit(s), %d with all %d anchors, %lu ms\n",
               m->name, r.regions, r.reached, r.qerr ? " (VirtualQueryEx failed)" : "", r.bytes, r.hits, full,
               m->nanchors, (unsigned long)(t1 - t0));
        if (!full) worst = 4;
        for (j = 0; j < r.n; j++) {
            int k;
            printf("%-11s DGROUP base 0x%08lx anchors %d/%d", m->name, r.base[j], r.ok[j], m->nanchors);
            t0 = GetTickCount();
            n = ldt_lookup(r.base[j], sels, infos, 16);
            t1 = GetTickCount();
            if (!g_thread) printf(" selectors ?\n");
            else if (!n) printf(" no LDT selector (%lu ms)\n", (unsigned long)(t1 - t0));
            else {
                printf(" (LDT walk %lu ms)\n", (unsigned long)(t1 - t0));
                for (k = 0; k < n && k < 16; k++)
                    printf("%-11s   selector 0x%04lx base 0x%08lx limit 0x%04lx %s%s\n", m->name, sels[k],
                           infos[k].base, infos[k].limit, infos[k].code ? "code" : "data", infos[k].big ? " 32-bit" : "");
            }
        }
    }
    return worst;
}

static int cmd_ldt(const struct args *a)
{
    struct selinfo si;
    int rc = open_target(a, 1);
    if (rc) return rc;
    if (!sel_entry(a->sel, &si)) {
        fprintf(stderr, "isprobe: GetThreadSelectorEntry(0x%04lx) failed, error %lu\n", a->sel, GetLastError());
        return 4;
    }
    printf("selector 0x%04lx base 0x%08lx limit 0x%04lx %s%s%s\n", a->sel, si.base, si.limit,
           si.code ? "code" : "data", si.big ? " 32-bit" : "", si.present ? "" : " not-present");
    for (rc = 0; rc < NMODULES; rc++)
        if (si.present && anchors_ok(&modules[rc], si.base, 0) == modules[rc].nanchors)
            printf("selector 0x%04lx is the %s DGROUP (all anchors match)\n", a->sel, modules[rc].name);
    return 0;
}

static void describe(const struct module *m, const struct dgroup *d)
{
    fprintf(stderr, "isprobe: %s DGROUP base 0x%08lx", m->name, d->base);
    if (d->have_sel) fprintf(stderr, " selector 0x%04lx limit 0x%04lx", d->sel, d->si.limit);
    fprintf(stderr, "\n");
}

static int cmd_read(const struct args *a)
{
    struct dgroup d;
    unsigned char *buf;
    int rc = open_target(a, 0);
    if (rc || (rc = resolve(a, a->mod, &d, 0))) return rc;
    describe(a->mod, &d);
    buf = malloc(a->len);
    if (!buf || !read_mem(d.base + a->off, buf, a->len)) {
        fprintf(stderr, "isprobe: ReadProcessMemory(0x%08lx, %lu) failed\n", d.base + a->off, a->len);
        free(buf);
        return 5;
    }
    hexdump(a->off, buf, a->len);
    free(buf);
    return 0;
}

static int cmd_dump(const struct args *a)
{
    struct dgroup d;
    unsigned char *buf;
    unsigned long len = a->len;
    FILE *f;
    int rc = open_target(a, 0);
    if (rc || (rc = resolve(a, a->mod, &d, a->len_all))) return rc;
    describe(a->mod, &d);
    if (a->len_all) {
        if (!d.have_sel) {
            fprintf(stderr, "isprobe: dump all needs the DGROUP selector (no LDT match)\n");
            return 4;
        }
        len = d.si.limit + 1 > SEG_SIZE ? SEG_SIZE : d.si.limit + 1;
    }
    buf = malloc(len);
    if (!buf || !read_mem(d.base, buf, len)) {
        fprintf(stderr, "isprobe: ReadProcessMemory(0x%08lx, %lu) failed\n", d.base, len);
        free(buf);
        return 5;
    }
    f = strcmp(a->out, "-") ? fopen(a->out, "wb") : stdout;
    if (!f || fwrite(buf, 1, len, f) != len || (f != stdout && fclose(f))) {
        fprintf(stderr, "isprobe: cannot write %s\n", a->out);
        free(buf);
        return 5;
    }
    if (f == stdout) fflush(stdout);
    fprintf(stderr, "isprobe: wrote %lu bytes (DGROUP:0x0000-0x%04lx) to %s\n", len, len - 1, a->out);
    free(buf);
    return 0;
}

static void stamp(LARGE_INTEGER t0, LARGE_INTEGER freq)
{
    SYSTEMTIME st;
    LARGE_INTEGER now;
    GetLocalTime(&st);
    QueryPerformanceCounter(&now);
    printf("%02u:%02u:%02u.%03u +%lums", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
           (unsigned long)((now.QuadPart - t0.QuadPart) * 1000 / freq.QuadPart));
}

static void print_bytes(const struct args *a, const unsigned char *cur, const unsigned char *prev)
{
    unsigned long i, runs = 0;
    if (a->len <= 64 || !prev) {
        unsigned long n = a->len <= 64 ? a->len : 64;
        for (i = 0; i < n; i++) printf(" %02x", cur[i]);
        if (a->len > 64) printf(" ... (%lu bytes)", a->len);
        else if (a->len == 2) printf(" = 0x%04x", cur[0] | cur[1] << 8);
        else if (a->len == 4) printf(" = 0x%08lx", (unsigned long)cur[0] | (unsigned long)cur[1] << 8 |
                                                   (unsigned long)cur[2] << 16 | (unsigned long)cur[3] << 24);
        return;
    }
    for (i = 0; i < a->len; i++) {
        unsigned long j;
        if (cur[i] == prev[i]) continue;
        for (j = i; j < a->len && cur[j] != prev[j]; j++) ;
        if (++runs <= 8) {
            unsigned long k;
            printf(" DGROUP:0x%04lx:", a->off + i);
            for (k = i; k < j && k < i + 8; k++) printf(" %02x>%02x", prev[k], cur[k]);
            if (j - i > 8) printf(" ...(%lu)", j - i);
        }
        i = j;
    }
    if (runs > 8) printf(" ... %lu runs", runs);
}

static int cmd_watch(const struct args *a)
{
    struct dgroup d;
    unsigned char cur[WATCH_MAX], prev[WATCH_MAX];
    unsigned long i;
    int have_prev = 0, failing = 0;
    LARGE_INTEGER t0, freq;
    int rc = open_target(a, 0);
    if (rc || (rc = resolve(a, a->mod, &d, 1))) return rc;
    describe(a->mod, &d);
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    for (i = 0; a->count == 0 || i < a->count; i++) {
        if (i) Sleep(a->ms);
        if (d.have_sel) {
            struct selinfo si;
            if (sel_entry(d.sel, &si) && si.present && si.base != d.base) {
                stamp(t0, freq);
                printf(" %s DGROUP moved 0x%08lx -> 0x%08lx (selector 0x%04lx), anchors %d/%d\n", a->mod->name,
                       d.base, si.base, d.sel, anchors_ok(a->mod, si.base, 0), a->mod->nanchors);
                d.base = si.base;
            }
        } else if (i % 50 == 0 && anchors_ok(a->mod, d.base, 0) != a->mod->nanchors) {
            stamp(t0, freq);
            printf(" %s anchors no longer match at 0x%08lx\n", a->mod->name, d.base);
        }
        if (!read_mem(d.base + a->off, cur, a->len)) {
            if (!failing) { stamp(t0, freq); printf(" read failed at 0x%08lx\n", d.base + a->off); }
            failing = 1;
            fflush(stdout);
            continue;
        }
        failing = 0;
        if (!have_prev || memcmp(cur, prev, a->len)) {
            stamp(t0, freq);
            printf(" %s DGROUP:0x%04lx %s", a->mod->shortname, a->off, have_prev ? "chg" : "init");
            print_bytes(a, cur, have_prev && a->len > 64 ? prev : NULL);
            printf("\n");
            fflush(stdout);
            memcpy(prev, cur, a->len);
            have_prev = 1;
        }
    }
    return 0;
}
#endif /* _WIN32 */

int main(int argc, char **argv)
{
    struct args a;
    int rc;
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if ((rc = parse_args(argc, argv, &a)) != 0) return rc;
    if (a.dry_run) return dry_run(&a);
#ifdef _WIN32
    switch (a.cmd) {
    case CMD_FIND: return cmd_find(&a);
    case CMD_LDT: return cmd_ldt(&a);
    case CMD_READ: return cmd_read(&a);
    case CMD_DUMP: return cmd_dump(&a);
    case CMD_WATCH: return cmd_watch(&a);
    default: return 2;
    }
#else
    fputs("isprobe: this build only parses arguments (--dry-run); build it with oracle/probe/build.sh\n", stderr);
    return 3;
#endif
}
