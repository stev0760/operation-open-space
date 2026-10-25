# intro-0001: the original's state-0 attract cycle, timed and measured

- **Date:** 2026-10-25
- **Host:** Arch Linux (kernel 7.2.11-zen1), Wine 11.0 old WoW64 with the ExtractIcon patch (`~/opt/wine-11.0-oldwow-patched`, the `oracle/env.sh` default), Xvfb 1024×768×24 on `:113`
- **Prefix:** `~/.local/share/wineprefixes/innerspace3` (`ORACLE_ID=3`), made fresh by `scenario_prefix`: `install_layout.py --with-ini`, fixture `basic` as `D:`, no `Z:`, untraced (`TRACE=0`, so the relay does not slow the frames)
- **Reproduce:** `ORACLE_ID=3 oracle/scenarios/intro.sh work/oracle/intro-0001` (both runs, about 4 minutes)
- **Bulk output (gitignored):** `work/oracle/intro-0001/` (8.7 MB)
  - `watch/`: run 1, no input. `tSSS.SS.png` every 0.5 s for 30 s, then every 2 s. It stops 5 shots after the demo start.
  - `watch-run0/`: run 0. It used an earlier revision of the script with the same schedule up to the 300 s cap.
  - `step/`: run 2, Space on each phase.
  - `montage/`: one montage per phase, built from the watch run (`00-logo` … `07-demo`), and `step.png` for the step run.
  - `steps.txt`, `run.out`, and `INSPACE.INI` and `files.txt` in each run directory.
- Elapsed times count from `wine INSPACE.EXE`. A phase "first appears" in the interval (last shot without it, first shot with it]. Positions are screen pixels. The game window covers the screen, so W = 1024, H = 768, W/2 = 512 and H/2 = 384. "Ink" means pixels brighter than 24 in some channel. "Predicted" quotes [shell.md](../../docs/re/shell.md#intro-and-title-screen) with those values.

## Timeline (no input)

| Phase | Run 0 | Run 1 | Step run (first poll) | Measured duration | Predicted |
|---|---|---|---|---|---|
| 0 logo, step 0 (bare ring spinning) | (0.54, 1.05] | (0.54, 1.04] | 1.04 | ~5.5 s, until Calibrate ends | depends on Calibrate |
| 0 logo, step 1 (cells 10–13) | (6.00, 6.52] | (6.05, 6.54] | | 0.5–1.5 s | 88 frames |
| 0 logo, step 2 (column sweep) | (7.04, 7.51] | (7.03, 7.52] | | 1.5–2.5 s to the yellow letters | 156 + 1 frames |
| 0 logo, letters RGB(253, 253, 2) | (9.01, 9.53] | (9.04, 9.53] | | | |
| 0 logo, step 4 ("presents.. ") | (10.05, 10.52] | (10.51, 11.05] | | 2.5–3.5 s | 3000 ms |
| 1 black second | (13.01, 13.53] | (13.55, 14.04] | 13.90 | 0.5–1.55 s | 1000 ms |
| 2 title | (14.05, 14.56] | (14.55, 15.05] | 14.97 | 51.5–54.0 / 51.0–53.5 s | 242 frames + 51 500 ms, cut short by the scene (below) |
| 2 title, picture dissolve complete | (15.53, 16.01] | (15.51, 16.01] | | 1.0–2.0 s after the title appears | 105 frames |
| 2 title, scene objects at the sides | (62.04, 64.04] | (62.04, 64.01] | | | 45 000 ms after the end of title step 4 |
| 2 title, scene: black screen with spark effects | 66.04 | 66.04 | | | |
| 3 credits | (66.04, 68.02] | (66.04, 68.05] | 18.50 after Space | 30–34 s | about 31 s on this path (Credits timeline) |
| 4 text page 1 | (98.02, 100.00] | (98.03, 100.04] | 20.72 | 22–26 s | 25 000 ms (DGROUP:0x920c 0 or 1) |
| 4 text page 2 | (122.02, 124.05] | (122.03, 124.03] | 22.64 | 26–30 s | 27 000 ms |
| 5 Basic Key Commands | (150.03, 152.01] | (150.02, 152.02] | 24.57 | 8–12 / 10–14 s, so 10–12 s | 11 000 ms |
| demo start (Navigation window) | (160.00, 162.05] | (162.01, 164.02] | before 27.13 | | |

- **Frame rate.** The title dissolve (105 frames) and the logo steps (88, 157 and 87 frames in 0.5–2.5 s each) put the state-0 frame at about 13–14 ms, the 14/13 ms gate of [timing.md](../../docs/re/timing.md#calibrate).
- **Title end.** In both runs, the shot before the credits (66.04) is a black screen with orange or cyan spark effects at the top centre. 64.04 and 64.01 show the two scene objects at x ≈ 293 and ≈ 725, which matches W/2 ∓ 210. So the title ended through the scene's now + 2500 ms deadline (the early-end path), about 48–50 s after title step 4 ended. It did not end at the 51 500 ms deadline.
- **Credits.** In run 0 the entry headings first show at 68.02 ("Directed By"), 74.03 ("Programming By"), 78.02 ("Music Composed By"), 82.01 ("Voice Effects"), 88.02 ("Thanks to") and 92.01 ("For"). 72.05 and 86.04 show the logo alone (the 1000 ms gaps), and so do 96.05 and 98.02 (the 4000 ms tail). Entries start about 4.4 s apart: a 3000 ms hold, the erase, a 1000 ms gap and the typing. The 0.5 s/2 s schedule cannot resolve the 84-frame logo growth or the typing.
- **Demo start.** The demo opens the Navigation window at the top left of the screen on `C:\` and selects "dodge city - duel". Its status line reads "This is the arena for inner space duels. If you have a dispute, take him here!". About 4 s later comes a black screen, then the "WAVE 1 MISSION BRIEFING" box for Dodge City with the text typing. By 210 s (run 0) the demo is playing wave 1 behind the game border. The green "Analysing Drive …" and "(Starting Demo)" texts (shell.md, Demo start) do not appear in any shot: no shot falls between the key list and the Navigation window. The step run's first shot, 1.0 s after Space, already shows the Navigation window.

## Step run (Space 1 s after each complete draw)
- Each Space went through: Calibrate had ended before the title appeared, so the state-0 handler took the keys. After each Space the next phase appeared in 0.36–0.39 s, measured from the poll after Space to the first poll that differed.
- At its first poll (14.97) the title already shows every text and a complete picture. That poll catches the first copy, the pixel doubling that title step 0 draws before the row dissolve (see Title). The screen is unchanged from 17.03 on.
- The credits' first poll, 0.39 s after Space, shows the logo alone at 44×44 ink, (491–534, 343–386), still growing. 0.76 s later, "Directed By" and "Bill Stewart" are complete.

## Measurements per phase

### Logo (phase 0)
- **Cell.** Every logo cell frame inks exactly (424–463, 324–363), 40×40. Predicted: a 40×40 cell at (W/2 − 88, H/2 − 60) = (424, 324). **Agrees.**
- **Step 0** shows the bare ring in greys (#737373, #3c3c3c, #5f5f5f …). The rotation frames add #800000, #ff0000 and #c4c400. The last frame of step 3, which stays through step 4, has 999 ink pixels. It matches cell 5 (shell.md: "it also blits cell 5").
- **SD_LOGO letters** after the sweep ink (474–603, 328–359), 130×32, all #fdfd02 = RGB(253, 253, 2), 1043 pixels. They form two row bands, 328–335 ("SOFTWARE") and 344–359 ("DYNAMICS."). Predicted: a 130×32 copy at (W/2 − 38, H/2 − 56) = (474, 328), last brush RGB(253, 253, 2). **Agrees.**
- **Sweep frames** (7.5–9.0 s) add #0000ff, #00ffff and #ffffff lines and #404040 columns. These are the random pens (blue, cyan, white, teal) and the DKGRAY_BRUSH reveal.
- **"presents.. "** inks (433–578, 410–438), solid #ffff00 with a few #fdfd00 and #fafa00 edge pixels. Without the "p" descender the ink spans 410–431, so the baseline is at y = 432 and the ink centre is x = 505.5. Predicted: TA_CENTER\|TA_TOP at (W/2, H/2 + 17) = (512, 401), colour 0x0000FFFF. The top of the cell at 401 and an ascent of about 31 at height 38 give a baseline near 432. **Agrees.** The ink centre sits 6.5 px left of 512 because the string ends in a space (string 2118 "presents.. "). Wine draws the text with its substitute for Times New Roman. That font is not Times New Roman, so the glyph shapes and widths are approximate.

### Black second (phase 1)
- Every shot in it is black: no pixel above 0 anywhere. **Agrees** (PatBlt BLACKNESS, nothing drawn).

### Title (phase 2), first title of the run (C = 0, D = 0 because H ≥ 500)
| Text | Ink bbox (x, y) | Ink centre x | Dominant colours | Predicted top-left reference |
|---|---|---|---|---|
| "OPERATION:" | 364–478, 143–158 | 421 | #a060ff main, #6000c0 shadow, #ffffff and #e9e9e9 highlight | TA_CENTER at (W/2 − 90, D − 245 + H/2) = (422, 139) |
| "limited inner space™" | 357–664, 168–198 | 510.5 | same | (512, 164) |
| "Standard Edition - V1.29.08 Aug 9/2000" | 338–692, 200–220 | 515 | same | (512, 198) |
| hint " For best results, change Windows to 256 color mode " | 341–682, 470–485 (opaque box, 342×16) | 511.5 | #c0c0c0 box, black text | (512, C + 86 + H/2) = (512, 470) |
| copyright "© 1994-2000 Software Dynamics, inc. All rights reserved." | 332–689, 490–502 | 510.5 | #b480e0 only (transparent) | (512, 487) |
| trial "You are on day 12088 of your demo trial period." | 298–723, 511–531 | 510.5 | #a060ff, #6000c0, #ffffff | (512, 509) |
| " Press 'R' to order the full game package " | 328–696, 534–555 (opaque box, 369×22) | 512 | #b480e0 box, black text | (512, 534) |
| "ESC to play    -    Alt-X to Exit" | 320–704, 572–599 | 512 | #a060ff, #6000c0, #ffffff | (512, 569) |

- Every text lies where shell.md puts it, with C = 0 and D = 0. **Agrees.** The opaque boxes start exactly on the predicted rows (470 and 534). In the other lines the ink starts 2–4 rows below the cell top, which is the fonts' internal leading.
- The embossed colours are PALETTERGB(0xA0, 0x60, 0xFF) main, PALETTERGB(0x60, 0x00, 0xC0) shadow and white highlight, the DGROUP:0x11aa = 0, 8 bpp and above row. **Agrees.** Wine antialiases the Arial edges, so intermediate shades such as #ceadff, #5700af and #e1e1e1 appear even though the fonts ask for quality 2. The port's renderer will not match those edge pixels exactly.
- The hint is drawn. The display is 24 bpp, so DGROUP:0x90ee ≠ 8. **Agrees.**
- **Picture.** Ink (375–648, 230–438), and every change between the first and the final picture falls exactly inside (374–649, 229–438). Predicted: (W/2 − 138, H/2 + D − 155, 276, 210) = (374, 229)–(649, 438). **Agrees.** The final picture uses exactly 43 colours, which matches probe-0001 (black plus the 42 ramp colours of entries 188–229). The first shot of the title (run 0, 14.56) already shows the complete first copy (all 105 row pairs present). The rows then change one pair at a time to the final picture: 100, 62, 25 and 0 rows still differ at 14.56, 15.04, 15.53 and 16.01 (run 1: 105, 71, 37, 0). This is the doubled first copy followed by the 105-frame dissolve of the ordinal-12 copy. **Agrees.**
- **Trial day** reads 12088 on 2026-10-25, one more than probe-0001 (12087 on 2026-10-24). This matches dayval − 254.

### Credits (phase 3)
- **Logo.** When fully grown, its ink is (486–540, 338–392), 55×55. Predicted: s = 57 at (W/2 − s/2, H/2 − s/2 − 20) = (484, 336), 57×57, whose top two rows and left two columns are the window's black row and column scaled by 1.5. **Agrees.** The ink colours are #4040ff, #8c8cff, #000080, #b4b4ff, #0000ff and #6969ff, with #e0e0e0 highlights. The first poll after Space (0.39 s) shows it at 44×44 ink (491–534, 343–386), centred at (512.5, 364.5) = (W/2, H/2 − 20). It is still growing.
- **Heading** "Directed By" inks (427–598, 237–257), centre 512.5. **Name** "Bill Stewart" inks (422–604, 487–507), centre 513. Both are small capitals in #a060ff, #6000c0 and #ffffff. Predicted line tops: H/2 − 150 = 234 and H/2 + 100 = 484, centred on Wt, with the emboss colours of seg15:0x7a62 at 8 bpp and above. **Agrees.** The ink starts 3 rows below each line top.
- **Bolts** appear in run 0 (for example at 82.01, a cyan line from the logo to the heading). The capture schedule cannot time them.

### Text pages (phase 4)
- **Heading** "LIMITED INNER SPACE V1.29.08" in bitmap font 0, ink (260–763, 209–228), 504×20, all #8c00ff. Predicted: colour 0x00FF008C = RGB(0x8C, 0, 0xFF), centred at (W/2, H/2 − 175) = (512, 209), 18×20 cells (28 × 18 = 504). **Agrees.**
- **Body**, Arial 18/10, all #8080ff and not antialiased. The first line's ink is at 271–285; line tops are 269 + 24 i (ink bands start at 271, 295, 319, …). Predicted: (W/2, H/2 + 24 i − 115), first line 269, PALETTERGB(0x80, 0x80, 0xFF). **Agrees.** Page 1 has 10 inked lines (one is empty), and its text is ENG_LANG #5000 136 ("We hope you have enjoyed playing Inner Space for the …"). Page 2 has 9 inked lines ("Inner Space is an exciting action game …").
  - The line ink centres are 500.5–511.5, not 512 (page 1: 501.5, 500.5, 505.5, 503.5, 505.5, 501.0, 504.0, 507.5, 507.5, 503.0). The lines come from splitting the resource at CR LF, and several end in a space. TA_CENTER centres those blank advances too. The port keeps the trailing spaces and its lines are pixel-exact against these shots (`work/shell/s2/compare/report.md`).
- **"press ESC to play Inner Space.. "** in white bitmap font 0, ink (224–773, 557–576). Predicted: (W/2, H/2 + 173) = (512, 557). 32 cells × 18 = 576 wide, so the cells start at 224, and the last cell is the trailing blank. **Agrees.**
- Page 2 keeps the heading and the press-ESC line identical. Only the body changes, so the switch from page 1 to page 2 does not black the screen. **Agrees** (PatBlt from H/2 − 115 down).

### Basic Key Commands (phase 5)
- **Heading** "BASIC KEY COMMANDS", ink (350–672, 184–203), all #ff0000. Predicted: 0x000000FF at (W/2, H/2 − 200) = (512, 184), 18 × 18 = 324 wide from 350. **Agrees.**
- **Rows** i = 0–10: tops at 234 + 30 i (234, 264, …, 534). Every left column starts at x = 217. Predicted: (W/2 − 295, H/2 + 30 (i − 5)) = (217, 234 + 30 i). **Agrees.** The right column ("- " plus the description) has its first ink at x = 510. The dash cell starts at W/2 − 5 = 507, and the glyph is slanted. Row 3, "LEFT\RIGHT ARROW", runs into the dash column: its ink starts at 480.
- **Row colours:** #ff000b, #f20017, #e50023, #d8002f, #cb003b, #be0047, #b10053, #a4005f, #97006b, #8a0077, #7d0083. Predicted: RGB(255 − 13 i, 0, 12 i + 11). **All 11 agree.**

## Environment facts that matter for a comparison
- **Calibrate is negative on this host.** `INSPACE.INI` after the runs holds `Calibrate=-2132` (run 0), `-580` (run 1) and `-1281` (step run). timing.md gives Calibrate = 23 × count / 10, with the oracle measuring 13 340–14 940 on the old Rocky host. On this faster host, 23 × count / 10 exceeds 32 767, and the stored 16-bit value wraps. **Disagrees with timing.md's oracle range** (that range was measured on the old host; the formula itself is not contradicted).
  - The game therefore takes every "Calibrate < N" branch on this host. The credits logo grows by 6 per frame instead of the port's 3 (seg15:0x7cef, seg15:0x7d10), and the bolt finish takes one seg11:0xa9ea call (< 1400).
  - The port fixes Calibrate at 14000 (ADR 0001 decision 9). So a frame-by-frame comparison of the credits logo growth with this capture will differ by design.
  - The phase durations in the timeline do not depend on it.
- **Fonts.** Wine substitutes for Arial and Times New Roman. Title and credits edges are antialiased; the Arial 18 body is not.

## Disagreements with shell.md
- None in geometry or colour. Every predicted position, size and colour checked above agrees within the fonts' internal leading.
- Explained offsets, not errors: the "presents.. " ink centre (505.5) and the text-page line centres (500.5–511.5) sit left of W/2 because of trailing blanks inside the centred strings.
- Not observed: the green demo-start texts ("Analysing Drive …", "(Starting Demo)"). Neither the 2 s schedule nor the step run's 1 s delay caught them, so their position and colour remain unchecked.
- timing.md, not shell.md: the measured Calibrate wraps negative on this host (see above).

## Not working / limits
- The schedule (0.5 s, then 2 s) bounds each phase change to a 0.5 s or 2 s interval. It cannot resolve the 84-frame credits logo growth, the typing of each line or the bolts. A capture at frame rate (or a probe watching DGROUP:0x95b8) would be needed to time those.
- The test list in `oracle/tests/test_scripts.py` (`SCRIPTS`) does not name `scenarios/intro.sh`, so pytest does not run its `bash -n`. `bash -n oracle/scenarios/intro.sh` passes by hand.
