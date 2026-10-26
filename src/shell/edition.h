// The edition check seg10:0x885e with the parts of ISPACE.DLL ordinal 24 it calls (command 2,
// the record check ISPACE.DLL seg10:0x4f9c, and command 10, the name record ISPACE.DLL
// seg10:0x4c68 in mode 0 through seg10:0x946a). docs/re/shell.md "Intro and title screen"
// (**Edition check**, **Editions**), docs/re/ispace.md ordinal 24, docs/re/formats.md
// "Resources rewritten in place", ADR 0008 decisions 2, 4 and 5.
//
// Portable and pure: the caller reads INSPACE.EXE #5500 110 and #5000 143, the three INI values
// and the local date, and keeps the result between calls. Every quantity is a 16-bit word as in
// the original; the compares are signed 16-bit.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace is::shell {

// The five flags and the decoded name. The defaults are the file image: everything 0, the
// "Limited" edition.
struct Edition {
    int edition = 0;  // DGROUP:0x920c: -1 registered, 0 limited, 1 trial, 2 35-day, 3 none
    int plus = 0;     // DGROUP:0x11aa: " Plus Edition" and the gold title colours
    int invalid = 0;  // DGROUP:0x922a: "The game isn't valid on this hard drive."
    int checked = 0;  // DGROUP:0x922c: set by every full pass; no instruction reads it
    int day = 0;      // DGROUP:0x9224: the trial day, or -1 - dd for the 35-day form
    // The "Name - Organization" text of #5000 143 as seg10:0x946a leaves it (at most 48
    // bytes, cp1252); filled on the registered path only.
    std::string name;
    // Diagnostics: the command-2 result of the last full pass (-1 before one ran).
    int record_result = -1;
};

struct EditionInputs {
    // INSPACE.EXE #5500 110 (the install record, 10 bytes used) and #5000 143 (the name
    // record, 50 bytes used). Empty when the resource is missing; short data reads as zeros.
    std::vector<std::uint8_t> install_record;
    std::vector<std::uint8_t> name_record;
    bool name_found = false;  // #5000 143 exists (command 10's "resource found")
    // SYSTEM.INI [386Enh] vidsp and 32BitSP, WIN.INI [windows] PDev (or the port's
    // [Registration] section, ADR 0008 decision 1), as GetPrivateProfileInt returns them.
    int vidsp = 0;
    int bit32sp = 0;
    int pdev = 0;
    // localtime(time()): tm_year (years since 1900) and tm_yday (0-365).
    int tm_year = 0;
    int tm_yday = 0;
};

// seg10:0x885e's day value: (tm_year - 93) * 365 + tm_yday as a 16-bit word [BYTES
// seg10:0x886d seg10:0x8874].
int edition_day_value(int tm_year, int tm_yday);

// Ordinal 24 command 2, ISPACE.DLL seg10:0x4f9c(b, 0), on the Windows NT branch (the drive
// serial ISPACE.DLL seg10:0x4440 is 0, ADR 0008 decision 2): 2 when PDev - vidsp < 0, 3 when
// the record's dword D is 0, 1 when PDev = vidsp, 0 when PDev - vidsp = (D & 0xFFFF) mod 653 +
// 654 (ISPACE.DLL seg10:0x51b6), else 1000.
int record_check(const std::vector<std::uint8_t>& install_record, int vidsp, int pdev);

// ISPACE.DLL seg10:0x4dba(buf, n, 0): bytes >= 0x80 become 0, the others are XORed with
// {0x2A, 0x32, 0x2D}[i mod 3].
std::vector<std::uint8_t> descramble_name(std::vector<std::uint8_t> bytes);
// The scramble direction, mode 1, for tests: a byte <= 0 (signed) becomes -(r(i) % 96 + 2),
// where r(i) stands for the rand() the original draws, the others are XORed with the key.
std::vector<std::uint8_t> scramble_name(std::vector<std::uint8_t> bytes, unsigned (*r)(int));

// Ordinal 24 command 10 through seg10:0x946a: the descrambled 50 bytes, moved down by 3 when
// byte 0, 1 or 2 decodes to 0 (the result is then 0), cut at +0x30. Returns the command's
// result (1: found and the first three bytes non-zero) and stores the text up to its first NUL.
int read_name(const std::vector<std::uint8_t>& name_record, bool found, std::string* name);

// seg10:0x885e(arg). `prev` is the state the previous call left (the file image at first);
// the result is the new state and *ret the routine's return value (1 when it returned at once
// or found no edition, 0 otherwise). `arg` != 0 is the start-up call, whose CD branch the port
// does not take: CDDrive counts as empty (ADR 0008 decision 4), so arg only documents the
// caller. The DGROUP:0x5d0 branches are dead in v1.29.08 and are not ported.
Edition edition_check(const Edition& prev, int arg, const EditionInputs& in, int* ret = nullptr);

}  // namespace is::shell
