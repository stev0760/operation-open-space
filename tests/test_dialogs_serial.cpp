// The Order dialog's serial (ISPACE.DLL seg10:0x5dfe and the generator seg10:0x492, shell.md
// "Game-start dialogs", Order, **Serial**) and the New Game DRIVE list's label and selection
// rules (ISPACE.DLL seg10:0x1e7c), as pure functions on the host. The generator's vectors are
// the documented ones (seed and date number of the start-0001 runs).
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dialogs/rules.h"
#include "doctest.h"
#include "shell/rng.h"

using namespace is::dialogs;

TEST_CASE("dialogs serial: the generator seg10:0x492 reproduces the documented serials") {
    // D = 12342 = (126 - 93) * 365 + 297 [ORACLE start-0001]
    CHECK(date_number(126, 297) == 12342);
    CHECK(generate_serial(std::uint16_t{13176}, 12342) == 10127790);
    CHECK(generate_serial(std::uint16_t{17457}, 12342) == 10543958);
    CHECK(generate_serial(std::uint16_t{36978}, 12342) == 10543958);
    CHECK(serial_text(10127790) == "10127790");
    // srand(seed) then the generator equals the seeded overload, and consumes the state
    is::shell::Rng r;
    r.srand(13176);
    CHECK(generate_serial(r, 12342) == 10127790);
    CHECK(r.state() != 13176u);
}

TEST_CASE("dialogs serial: the generator's ranges over many seeds") {
    // labs comes before the fold and the last term, so the serial is never negative.
    for (unsigned seed = 0; seed < 65536; seed += 97) {
        for (int d : {1, 1200, 12342, 32767}) {
            const std::int32_t v = generate_serial(static_cast<std::uint16_t>(seed), d);
            CHECK(v >= 0);
        }
    }
    // a negative date number (a 16-bit wrap) still gives a non-negative serial: labs
    for (unsigned seed = 1; seed < 2000; seed += 13)
        CHECK(generate_serial(static_cast<std::uint16_t>(seed), -20000) >= 0);
}

TEST_CASE("dialogs serial: seg10:0x5dfe's record step") {
    int calls = 0;
    const auto gen = [&calls] {
        ++calls;
        return std::int32_t{4242};
    };
    // a zero serial: regenerated, counter 0
    SerialStep s = serial_step(SerialRecord{0, 3}, false, gen);
    CHECK(s.regenerated);
    CHECK(s.changed);
    CHECK(s.record == SerialRecord{4242, 0});
    CHECK(calls == 1);
    // a counter above 8 (signed): regenerated, even with bump
    s = serial_step(SerialRecord{777, 9}, true, gen);
    CHECK(s.regenerated);
    CHECK(s.record == SerialRecord{4242, 0});
    CHECK(calls == 2);
    // a counter of 8 is kept; bump 0 changes nothing
    s = serial_step(SerialRecord{777, 8}, false, gen);
    CHECK_FALSE(s.regenerated);
    CHECK_FALSE(s.changed);
    CHECK(s.record == SerialRecord{777, 8});
    // bump 1 counts the attempt
    s = serial_step(SerialRecord{777, 8}, true, gen);
    CHECK_FALSE(s.regenerated);
    CHECK(s.changed);
    CHECK(s.record == SerialRecord{777, 9});
    // a negative counter is not above 8; the word wraps at 0x7fff
    s = serial_step(SerialRecord{777, -5}, true, gen);
    CHECK(s.record == SerialRecord{777, -4});
    s = serial_step(SerialRecord{777, -32768}, false, gen);
    CHECK_FALSE(s.regenerated);
    CHECK(calls == 2);
    // nine wrong codes leave 9: the next opening shows a new serial
    SerialRecord r{777, 0};
    for (int i = 0; i < 9; ++i) r = serial_step(r, true, gen).record;
    CHECK(r.count == 9);
    CHECK(serial_step(r, false, gen).regenerated);
}

TEST_CASE("dialogs drive list: label fallback, the '.' rule and the selection") {
    // the find-first failed: "Unknown_HD", exact case
    CHECK(drive_label_text(std::nullopt) == "Unknown_HD");
    CHECK(drive_entry('C', std::nullopt) == "C: Unknown_HD");
    // a found label is kept; an empty found name stays empty (only a failure falls back)
    CHECK(drive_label_text(std::string("GAMES")) == "GAMES");
    CHECK(drive_entry('E', std::string()) == "E: ");
    // the '.' at index 8 of a name longer than 7 characters
    CHECK(drive_label_text(std::string("BACKUPDI.SK1")) == "BACKUPDISK1");
    CHECK(drive_label_text(std::string("ABCDEFGH.")) == "ABCDEFGH");
    CHECK(drive_label_text(std::string("ABCDEFGH")) == "ABCDEFGH");
    CHECK(drive_label_text(std::string("ABCDEFG.H")) == "ABCDEFG.H");    // index 7: kept
    CHECK(drive_label_text(std::string("ABCDEFGHI.JK")) == "ABCDEFGHI.JK");  // index 9: kept
    CHECK(drive_label_text(std::string("A.CDEFGH.IJK")) == "A.CDEFGHIJK");  // only index 8
    // the Win32 label in its 8.3 find-first form, then the rule: the first 11 characters
    CHECK(drive_entry('D', label_as_find_first("BACKUPDISK1XYZ")) == "D: BACKUPDISK1");
    CHECK(drive_entry('D', label_as_find_first("")) == "D: Unknown_HD");
    CHECK(drive_entry('Z', label_as_find_first("ROOT")) == "Z: ROOT");
    // seg15:0x6558 also replaces an empty result
    CHECK(label_text(std::string()) == "Unknown_HD");
    CHECK(drive_text(1, ":", std::string("BACKUPDI.SK1")) == "D:BACKUPDISK1");
    // the selection: the letter of INI Drive's first byte, case-sensitive, else 0
    const std::vector<std::string> entries{"C: Unknown_HD", "D: Unknown_HD", "F: DATA"};
    CHECK(drive_index(entries, "F") == 2);
    CHECK(drive_index(entries, "Fixture") == 2);
    CHECK(drive_index(entries, "f") == 0);
    CHECK(drive_index(entries, "E") == 0);
    CHECK(drive_index(entries, "") == 0);
    CHECK(drive_index({}, "C") == 0);
}
