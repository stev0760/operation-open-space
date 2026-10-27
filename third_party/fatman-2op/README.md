# The Fat Man 2-operator General MIDI bank (WOPL)

- **Upstream:** the libADLMIDI repository, https://github.com/Wohlstand/libADLMIDI, file `fm_banks/wopl_files/fatman-2op.wopl`, at tag `v1.6.2.1`, commit `99c5e03a5cf9b4fdf35b53efddb853c96320d50f`. libADLMIDI's `banks.ini` lists the same instruments as bank 58, "WOPL (The Fat Man 2op set; Win9x)", the set it names as the one the Windows 9x OPL drivers use [REF libADLMIDI v1.6.2.1 banks.ini].
- **Licence:** MIT, copyright The Fat Man (K. Weston Phelan and George Alistair Sanger), `LICENSE-TheFatMan.txt` copied unchanged from `fm_banks/LICENSE-TheFatMan.txt` at the same commit. libADLMIDI's own GPL/LGPL licences do not cover this file; nothing of libADLMIDI's code is taken.
- **Copied:** `fatman-2op.wopl` and `LICENSE-TheFatMan.txt`.
- **Changed:** nothing.

| File | SHA-256 |
|---|---|
| `fatman-2op.wopl` | `74e9a26abb6ec3c1e9c446724775ff999b3077276dcffc27ba04fb3c29e12a45` |
| `LICENSE-TheFatMan.txt` | `113debfac0bb31c5b65dda0c246ce932ca6870f9528cb7c2efc9c613e97e2b23` |

- **Contents:** WOPL version 3, one melodic and one percussion bank, global flags 3 (deep tremolo and deep vibrato), volume model 4 (Win9x), 66-byte instrument records, 16 983 bytes. All 128 melodic programs are present; the percussion bank holds 53 drums, keys 35 to 87, and blank entries elsewhere. No entry is 4-operator or pseudo 4-operator. The drum keys the game's four MIDI files strike on channels 10 and 16 are all present except 97 and 98 (MAIN.MID), which lie outside the General MIDI drum map. Every operator and feedback byte equals the text form of the same bank, `fm_banks_new/fatman-2op.woplx`, at the same commit (checked for all 181 instruments).
- **Embedding:** `src/CMakeLists.txt` reads this file at configure time (`file(READ ... HEX)`) into the generated header `<build>/generated/audio/fatman_2op_wopl.h`; `src/audio/opl3_bank.cpp` parses it at first use.
