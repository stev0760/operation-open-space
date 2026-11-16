# The law: crimes, arrests, the warrant, the Hall of Justice and the cheat keys

Facts of INSPACE.EXE (unless a module is named; DGROUP is seg19) about what the Enforcers do and what follows an arrest: the cheat keys of seg11:0x72e0 that drive it in tests, the crime sites, the crime routine seg13:0xb6f2, the Enforcers' executor seg13:0xa6bc with its arrest and escort, the warrant seg13:0xb93a, the gate hand-off into state 8 and the Hall of Justice's text, fine and debriefing. "Ship word +N" is DGROUP:0x7090 + 0x15a·i + N; "object +N" is pool-14 record i (DGROUP:0x1e30, 0x70 bytes); P is the player DGROUP:0x9646; "now" is the dword DGROUP:0x1e50 and frame# the dword DGROUP:0x1e68. Calls are written in declaration order ([pitfalls.md](pitfalls.md)). The Enforcer classes, the order codes and the call for an Interceptor are in [pilots.md](pilots.md); the manual's account is [MANUAL Laws of Inner Space].

## The Enforcer classes
Team 7 designs (MASTER.SHP) carry their class in SHP byte +0x05, which seg14:0x1430 copies into ship byte +0x5e (DGROUP:0x70ee, structs.md +0x5d/+0x5e): INTERCEPTOR 1, CONSTABLE 3, LETHAL WEAPON 1, PATROLMAN 2, TERMINATOR 0, ELIMINATOR 0 [BYTES MASTER.SHP records 22, 35, 42, 47, 77, 85]. The code reads 0 as Terminator class, 1 as Intercept class and > 1 as Patrol class: the call-in kinds 0x14, 0x15, 0x16 ask for class 0, 1 and exactly 2 [BYTES seg13:0xaee3 seg13:0xaecd seg13:0xb048]; seg4:0x5e8e's Patrol branch tests > 1 [BYTES seg4:0x635b]; the tick's ship loop returns a destroyed class-1 Enforcer off-world [BYTES seg7:0x111f-0x1158]. [MANUAL Laws of Inner Space]: Patrol (Patrolman, Constable), Intercept (Interceptor, Lethal Weapon), Terminator (Terminator, Eliminator). "Team 7" is the design word +0x38 at DGROUP:0x970a + 0xad·n (n = ship word +0x4e) = 7, the ENFORCERS.

## The cheat keys
### The gate
- The game-input handler seg11:0x6b64 (`retf 8`; [bp+0xc] = message, [bp+0xa] = wParam = VK) dispatches at seg11:0x7288: message 0x100 (WM_KEYDOWN) → seg11:0x6e19, 0x101 (WM_KEYUP) → seg11:0x6d19, the private 0x99b and 0x99e → seg11:0x6e19 [BYTES seg11:0x7288-0x729e]. [shell.md](shell.md) "Cheat keys" has the DELETE toggle; what follows adds the per-key table.
- **DELETE** (VK 0x2e, WM_KEYDOWN or 0x99b/0x99e): with DGROUP:0x95bc ≠ 0 (a player game; with 0x95bc = 0 only VK 0x6b, 0x6d, 0x57, 0xbd, 0xbb pass seg11:0x6e1e-0x6e3e) the switch sends 0x2e to seg11:0x6ee7: nothing unless the edition DGROUP:0x920c is 2 or −1; then DGROUP:0x951c = (DGROUP:0x951c == 0) (`cmp 1; sbb; neg`: 0 → 1, anything else → 0); turned off → DGROUP:0x104c = 0 (god mode off too); turned on → sound 0x49 (seg10:0x744c(0x49)); both → dword DGROUP:0x5018 = 0xffffffff (score redraw) [BYTES seg11:0x6ee7-0x6f1d].
- **The key-up gate** seg11:0x6d19: nothing unless DGROUP:0x95bc ≠ 0; VK 0x31…0x39 ('1'…'9') and VK 0x72…0x78 (F3…F9) go to seg11:0x6d58, which calls seg11:0x72e0(VK) (near, `push cs`) only while DGROUP:0x951c ≠ 0 [BYTES seg11:0x6d19-0x6d69]. The other key-ups of the same switch: F12 0x7b → seg11:0x6d6c (PlayMode toggle), Enter 0x0d and 'Z' 0x5a → seg11:0x6e11, CAPS LOCK 0x14 → seg11:0x6dd6 (shield / cloak toggle), 'P' 0x50 → seg11:0x6dc4 (DGROUP:0x1032 = 1, the view shift).
- **The wave gate first:** seg11:0x6b64 drops every key while P's dword +0x84 (the control lock-out) > now or P's object `state` ≠ 0, except WM_KEYUP 'P' and WM_KEYDOWN 'H' ([bp−0x18]) [BYTES seg11:0x6c9b-0x6cd1]; so DELETE and the cheat keys need P alive in `state` 0 and not locked out (a cuffed player is still in `state` 0 and can use them).
- The cheat word is cleared by a restore (seg6:0x98bb, with DGROUP:0x104c and DGROUP:0x104e) and by seg10:0x82a2 at seg10:0x850b; it is not saved, and the INI is untouched [BYTES seg6:0x98bb seg10:0x850b] [ORACLE ships-0003].
- The keys act on WM_KEYUP: a runner's `xdotool key F6` (press and release) works; a held key fires once, on release.

### seg11:0x72e0(vk), the per-key table (cdecl, near; switch at seg11:0x749e) [BYTES seg11:0x72e0-0x7502]
| Key (VK) | Target | Effect | Evidence |
|---|---|---|---|
| '1' (0x31) | seg11:0x73b2 | race only (DGROUP:0x9320 = 6): seg4:0x2da2() (the race bonus to score and rating, globals.md "Race bonus"); then AX = P's word +0xda (`laps`) − DGROUP:0x9ea0 − 1, discarded. Outside a race nothing | [BYTES seg11:0x73b2-0x73cd] |
| '2' (0x32) | seg11:0x73d0 | duel only (DGROUP:0x9320 = 7): word DGROUP:0x668a = 0xffff (gate 0's speed word, world.md) | [BYTES seg11:0x73d0-0x73dd] |
| '3' (0x33) | seg11:0x73e0 | with DGROUP:0x9316 < 8 (signed): seg13:0xadbe(P, 4, 0x6a): call a Knight (helper) with message 0x6a (703 "The Force Is with You, A Knight Is Coming!", sound 0x37) | [BYTES seg11:0x73e0-0x73eb seg11:0x732c-0x7335 seg3:0x1137] |
| '4' (0x34) | seg11:0x73ee | with DGROUP:0x9316 < 8: seg13:0xadbe(P, 3, 0x69): call a Pirate, message 0x69 (699 "Hold On To Your Wallet, A Pirate Is Entering!", sound 0x2c) | [BYTES seg11:0x73ee-0x73f9 seg3:0x1147] |
| '5' (0x35) | seg11:0x73fc | Demon's Gate only (DGROUP:0x92e8 ≠ 0): DGROUP:0x1318 = 1, P's object byte +0x54 = 0 (through DGROUP:0x1e30, stride 0x70), DGROUP:0x913e = 1, seg3:0x6ec(P, 7, 0, 0xb5) (1944 "Now Take The Enlightened Path And Free Your Demon!"): the kind-2 pod's effect (core.md "A kind-2 pod") without its DGROUP:0x94b2 test | [BYTES seg11:0x73fc-0x7430 seg3:0xe7b] |
| '6' (0x36) | seg11:0x7433 | refuel: P's `fuel` +0x1e = min(design word +0x1e (DGROUP:0x96f0 + 0xad·(P's +0x4e)), fuel + 250) (signed 16-bit `jle`); sound seg10:0x754a(0x71, DGROUP:0x9324, P, 1) | [BYTES seg11:0x7433-0x7461 seg11:0x730b] |
| '7' (0x37) | seg11:0x7464 | toggles DGROUP:0x104e (0 → 1, else → 0): while set, the placement seg14:0x1764 gives weapon 13 (WILDCARD DEFENSE, core.md) | [BYTES seg11:0x7464-0x7470 seg14:0x1758] |
| '8' (0x38) | seg11:0x7473 | P's `resources` +0xce += 100 (one icon); sound seg10:0x754a(0x23, DGROUP:0x9324, P, 1) | [BYTES seg11:0x7473-0x748a] |
| '9' (0x39) | seg11:0x748d | P's roster record (DGROUP:0x6e24 + 0x7c·DGROUP:0x931c) byte +0x33 (THRUSTERS powerup) = 6; sound seg10:0x754a(0x20, DGROUP:0x9324, P, 1) | [BYTES seg11:0x748d-0x749b seg11:0x72ff-0x7313] |
| F3 (0x72) | seg11:0x72f1 | the same record's byte +0x32 (GUNS powerup) = 6; sound 0x20 as '9' | [BYTES seg11:0x72f1-0x7313] |
| F4 (0x73) | seg11:0x7316 | DGROUP:0x913e = 2: the next 1500 ms tick calls the event routine seg18:0x5cec (world.md "The event call") | [BYTES seg11:0x7316 seg7:0xdef] |
| F5 (0x74) | seg11:0x731f | with DGROUP:0x9316 < 8: seg13:0xadbe(P, 0x16, 0x6d): call a Patrolman (class 2), message 0x6d (730 "Abide By The Law, The Patrolman Is Entering.", sound 0x2c) | [BYTES seg11:0x731f-0x7335] |
| F6 (0x75) | seg11:0x733b | DGROUP:0x104a = 1; c = `rand()` % 10 (seg5:0xdc, `cdq; idiv` 16-bit signed, so 0…9); seg13:0xb6f2(P, c): the forced path (section 3) | [BYTES seg11:0x733b-0x7359] |
| F7 (0x76) | seg11:0x735c | seg12:0x6d64(P, 22), seg12:0x6d64(P, 23), seg12:0x6d64(P, 26): loads ENLIGHTENMENT, UNCERTAINTY and INNER FORCE (strings 208, 209, 212) into new slots; sound seg10:0x744c(0x25) | [BYTES seg11:0x735c-0x738e] |
| F8 (0x77) | seg11:0x7391 | toggles DGROUP:0x104c (god mode: the player's ship takes no damage, globals.md; with the cheat word on, the score digits are drawn in 0x02FFC800 while it is set and 0x02FFFFFF otherwise, shell.md); dword DGROUP:0x5018 = −1 | [BYTES seg11:0x7391-0x73a6 seg8:0x4607] |
| F9 (0x78) | seg11:0x73a9 | DGROUP:0x1048 = 0 (file value 1; no other writer): stops the 10 s defender launches of the wave pass (seg7:0x886 gates the DGROUP:0x1e6c block, seg8:0x9464 and seg8:0x9564 test it) for the rest of the session | [BYTES seg11:0x73a9 seg7:0x886 seg8:0x9479 seg8:0x9579] [BYTES DGROUP:0x1048] |
- The keys' calls draw `rand()` only through their callees: F6's own draw at seg11:0x7341, then seg13:0xb6f2 → seg13:0xadbe → seg13:0xae68 (the gate draw). '3', '4', F5 draw inside seg13:0xae68.
- **Use in oracle runs:** F6 is the arrest path (it needs seg13:0xae68 to accept: DGROUP:0x9316 ≤ `Detail` + 3, not kinds 6/7, not Demon's Gate, no Interceptor already on the player with +0x52 > 0x14; [pilots.md](pilots.md)); F5 brings a Patrolman (the witness for a real crime: then a crime of the player within 2400 cells is seen); F8 keeps the player alive in a fight-back test; '8' gives resources for the Hall's fine; F9 removes the defenders' noise. None of the keys needs a script to fly.

- **What a forced crime shows** [ORACLE ships-0003]: `k1` (F6 drew c = 2) showed "MESSAGE from / INTERCEPTOR / NEUTRAL -4 / THAT WAS EVIDENCE! / IT'S MINE TO TAKE" (message 0x46). `k5` (F6 drew c = 7, DGROUP:0xa163 = 07 at 13:26:49.315 in `watch-crime.txt`) shows "MESSAGE from / INTERCEPTOR / NEUTRAL -4" with no text line in the video frames at 12.0 s and 13.5 s of `k5/vid-s.mkv` (13:26:49.66 and 13:26:51.16): the unwritten message word of seg13:0xb6f2 for c = 7 gave a message with no string (read by this analyst from the bulk; frames kept in the scratchpad only).

## The crime sites, per crime code

Crime c's Hall of Justice name is ENG_LANG string 1098 + c (STRING 69/70); the seg13:0xb6f2 warning m and its line (seg3:0x6ec case m − 1, string by the case's push) are from the switch at seg13:0xb761 (The crime routine, which follows this table). The checklist rows are the LAW rows of docs/spec/checklist.md.

| c | Hall name (1098 + c) | warning m → line | Raised by (caller) | Condition | Row |
|---|---|---|---|---|---|
| 0 | " Murdering An Enforcer." | m unwritten (stale) | **no seg13:0xb6f2 caller with 0** except F6's `rand()` % 10. The murder is recorded directly by the death handler seg4:0x7636: a dead team-7 ship of class byte +0x5e > 0 (not a Terminator) sets DGROUP:0x9346 = the killer's roster index (+0xd6) and, for a killer = P, byte DGROUP:0xa14d += 1, dword DGROUP:0xa147 = frame# (DGROUP:0x1e68), DGROUP:0xa14b = DGROUP:0x91f8 [BYTES seg4:0x774e-0x7786] | killer = the revenge-redirected credited ship cr of seg4:0x7636 (ship-contact.md); no witness, no Interceptor, no counter words | LAW-08 |
| 1 | " Destroying An Icon" | 0x45 → 570 "Stop That! You Just Broke a Good Icon!" | seg8:0x653b (hit dispatcher kill), seg8:0x7761 (gun kill), seg7:0x4b95 (FIREBALL burn) | the credited ship's goal (+0x32, +0x34) = (0, icon) with activity +0x38 = 1 (attack) and the icon's `capturable` byte +0x53 = 1; seg8:0x653b also needs X pool ≠ 10 [BYTES seg8:0x6500-0x6540 seg8:0x772b-0x7766]; seg7:0x4b95 needs a pool-DGROUP:0x9328 (pool 5) record of mass word +0x44 = 0x78 (a FIREBALL) on a good icon (byte +0x2f ≥ 0) not yet burning (word DGROUP:0x57aa + 0x23·icon = 0; the touch then sets it to 2 and the dword DGROUP:0x57a4 + 0x23·icon = now + 4000) whose `+0x1e` = 0 and `capturable` = 1, the fireball's shooter entry (DGROUP:0x5a50/0x5a52 + 0x14·rec) = (literal 14, s) with s's goal (0, icon), +0x38 = 1 [BYTES seg7:0x4aa1 seg7:0x4ab7 seg7:0x4b4e-0x4b9a] | LAW-05 |
| 2 | " Removing Evidence" | 0x46 → 572 "That Was Evidence! It's Mine To Take." | seg13:0x1f90 in the touch handler seg13:0x16ac | any ship s not of team 7 taking a pool-11 record with kind word DGROUP:0x8560 + 0x34·rec = 1 (the dead ship's resource pack) outside Demon's Gate, after seg12:0x129e [BYTES seg13:0x1f61-0x1f95] | LAW-04 |
| 3 | " Attacking The Defenseless" | 0x5d → 960 "You're Charged With Using Excessive Force." | seg4:0x5fd8, seg4:0x61ac in the attack reaction seg4:0x5e8e(v, a, amount, kind) | (a) v flees (+0x38 = 4), T(7, team v) > −100, T(7, team a) < 100, amount > 10, `rand()` % 4 ≤ 1 (seg4:0x5fbc) and v's Engine lamp +0x26 < 40 [BYTES seg4:0x5f7b-0x5fdd]; with the team bounds held but the amount, the draw or the lamp failing, only message 0x5c (950 "You're Not Supposed To Bully. Now Stop It!") from the first team-7 ship j < DGROUP:0x9316 (any state) to a; (b) v nearly out of fuel (fuel < design word +0x1e / 10), a = P, grudge byte v+0xc2+P = 4 and the same team bounds [BYTES seg4:0x613a-0x61cb] (pilots.md) | LAW-03, LAW-10 |
| 4 | " Taking An Enforcer's Doughnut" | 0x65 → 961 "Rule #1 ...Donut Mess With My Doughnuts!!" | seg13:0x320e in seg13:0x3008(code, taker) | the taken object is pool 6 (a doughnut) and a ship k ≠ taker, k = **P** (DGROUP:0x9646), of team 7, in `state` 0, alive, with goal (6, rec) and +0x38 = 2 wanted it: seg13:0xb6f2(taker, 4). A team-7 k that is not P takes the rivals' path (seg4:0x5e8e(k, taker, …, 2)) [BYTES seg13:0x31e2-0x3216]. So the crime arises only when the player flies an Enforcer (after a view shift, ships.md) | LAW-01 |
| 5 | " Firing Upon An Officer" | 0x72 → no line of its own (case 113 is the shared tail seg3:0x14d6); nothing at all when byte DGROUP:0xa163 = 5 already | **no seg13:0xb6f2 caller with 5**. Recorded directly: (a) seg4:0x6356-0x6481 (a Patrol-class victim, pilots.md); (b) seg13:0x4ac4(ship), the resource-pack drop of the key case Ctrl+Alt+S (seg11:0x7033-0x7043): after seg13:0x498e(ship, 0), each ship j ≠ ship with pool-14 object `state` 0 (no `alive` test) attacking ship (goal (14, ship), +0x38 = 1) with a roster record turns to a pod (DGROUP:0x8680 + 2·ship = j, +0x38 = 2, dword +0x32 = 0x0000000b: goal (11, 0)); the first such j **without** a roster record (+0xd6 = −1; any team, no team-7 test) gets DGROUP:0x8680 + 2·ship = j and gets +0x52 = 0x16, +0x54 = 14, +0x56 = ship, +0x98 = now + 50000, +0xed = 1, +0x94 = now + 500, and for ship = P DGROUP:0xa163 = 5 and byte DGROUP:0xa152 += 1 (no cap) [BYTES seg13:0x4ac4-0x4b97 seg11:0x6cdc-0x6d0e] | LAW-08 |
| 6 | " Destroying Doughnut Dispensors" | 0x86 → 962 "Donut Deploying Devices Are Not To Be Disturbed!" | seg8:0x6559 (hit dispatcher kill), seg8:0x779b (gun kill) | the destroyed object is pool 8 with kind word DGROUP:0x3d5a + 0x34·rec = 0x10 (a doughnut launcher) and the credited ship's goal is (8, rec) with +0x38 = 1 [BYTES seg8:0x6543-0x655e seg8:0x7769-0x77a0] | LAW-02 |
| 7 | " Resisting Arrest." | m unwritten (stale; the oracle shows an empty message, section 1) | **no seg13:0xb6f2 caller with 7**. Recorded directly by the arrest, seg13:0xaa22: byte DGROUP:0xa154 += 1 when the resister is P (section 3, step A) | LAW-07 |
| 8 | " Unsportsmanlike Race Conduct" | 0xb9 → 963 "The Prize Is For The Winner You Loser!" | seg13:0x1879 in the icon capture seg13:0x16ac | a kind-6 wave (DGROUP:0x9320 = 6) and the capturer ≠ seg4:0x8d3a() (the race winner: the alive, in-play ship with object byte +0x51 = 2 and the least dword DGROUP:0x9e60 + 4i, unsigned, from 1000000); then twice seg13:0x2714(winner, ship, −50, −80, 1, 2) [BYTES seg13:0x1863-0x18a5 seg4:0x8d3a-0x8db9]. c = 8 always takes the forced path (both words += 2, an Interceptor without a witness), and seg13:0xae68 lets kind 0x15 into a race only while byte DGROUP:0xa163 = 8 | LAW-06, LAW-23 |
| 9 | " Being An Idiot." (1108 "(It's Been A Slow Day)") | 0xc0 → 964 "You're Guilty Of Reckless Thrusting!!" | seg8:0x5eeb at the head of the hit dispatcher seg8:0x5df2(pool, rec, Xpool, Xrec, …) | X = (DGROUP:0x9324, P), P's thrust multiplier word +0x1c > 2 (signed; OverThruster sets 5, seg11:0x5ab6 lowers it), the hit object's pool = DGROUP:0x934a (rocks, 3), 8 or 9, `PlayMode` DGROUP:0x9134 < 2: first seg3:0x6ec(P, 7, 0, 0xd) (case 12: sound 0x67 at most every 4000 ms via DGROUP:0x1cb4, string 350 "Be More Careful, %s. Avoid Dangerous Hazards."), then seg13:0xb6f2(P, 9) [BYTES seg8:0x5e9e-0x5ef0 seg3:0xcfd-0xd2a] | LAW-09, LAW-11 |

- **Seg4:0x8d3a's result is a stack word when no ship has finished:** it returns [bp−0xa], written only when a candidate beats the best [BYTES seg4:0x8d96-0x8db0]; ADR 0016 decision 1 reads it as 0 (ship 0).
- **Computer ships commit crimes too.** Every site but crime 9's and the attack reaction's (b) passes the credited ship s, not P. seg13:0xb6f2 raises the counter words for any s; the per-crime bytes DGROUP:0xa14d + c count P's only. A witness calls an Interceptor for s (kind 0x15, the order words name s), so an Enforcer arrests a computer ship the same way: its IS_INFO activity line reads 508 "Arresting %D For " + string 1098 + byte DGROUP:0xa163 (the global last crime, not the ship's) [BYTES seg3:0x2d09-0x2d47] (LAW-24). The arrested computer ship answers by team (section 3, step B3).
- **Team lawfulness** (LAW-21) is the team table's row 7, T(7, y) = word DGROUP:0x3894 + 2y (DGROUP:0x3824 + 2·(8·7 + y)), initial values from ISPACE.DLL #5600 135 row 7: AVENGERS 0, PREDATORS 0, FUZZY ONES 0, PIRATES −200, KNIGHTS 200, SPEED DEMONS −200, RENEGADES −250, ENFORCERS 250 [BYTES ISPACE.DLL 5600 135]. The crime-3 tests read it: a KNIGHT attacker (T(7, 4) = 200 ≥ 100) is never charged with crime 3, and a PIRATE, SPEED DEMON or RENEGADE victim (≤ −100) may be attacked without that charge. seg4:0x7e2a's Patrolman score adds T(7, tp)/−140. The row moves in play through seg13:0x2714 and is saved in the header +0x36 (formats.md). The manual's "Knights law-abiding, Pirates flout the law" has no other code path: no site tests a ship's own team before raising a crime, and the arrested ship's answer (step B3) makes PIRATES, RENEGADES and PREDATORS fight [MANUAL Laws of Inner Space].
- **Duels and races** (LAW-22, LAW-23): seg13:0xb6f2 returns at once in a duel (DGROUP:0x9320 = 7) [BYTES seg13:0xb705]; seg13:0xae68 refuses every call-in in kinds 6 and 7 except kind 0x15 while DGROUP:0xa163 = 8 [BYTES seg13:0xae7c-0xae95]; the call-ins seg4:0x7e2a are off in kinds 6 and 7; the wave set-up places no Enforcer in kinds 6 and 7 (DGROUP:0x932c = 0, DGROUP:0x9346 = −1, ships.md). So a duel has no law at all, and a race has only crime 8's Interceptor.

## The crime routine seg13:0xb6f2(s, c) (cdecl, `retf`) [BYTES seg13:0xb6f2-0xb939]

The routine as pseudo-code with the widths.

```
// locals: m = [bp-0xc] (word, written only by the switch), k = [bp-4], d = [bp-0xa], seen0 = [bp-0xe] (dead)
seen0 = 1                                                        // b700, no reader
if DGROUP:0x9320 == 7: return                                    // b705 (duel)
switch (u16)(c - 1):  (table seg13:0xb761, 9 words; u16 > 8 -> no case)
  0 (c=1): m = 0x45   1 (c=2): m = 0x46   2 (c=3): m = 0x5d   3 (c=4): m = 0x65
  4 (c=5): if byte DGROUP:0xa163 == 5: return; m = 0x72        // b72d-b737
  5 (c=6): m = 0x86   6 (c=7): m unwritten   7 (c=8): m = 0xb9   8 (c=9): m = 0xc0
  c = 0 or c > 9: m unwritten                                    // b753-b757
if s == P && c != 9: DGROUP:0x964a += 1; DGROUP:0x9648 += 1      // b773-b785 (16-bit add)
if c == 6: both += 1 else both += 3                              // b789-b79f
if c == 2: both += 4                                             // b7a3-b7ae
if c == 8 || DGROUP:0x104a != 0:                                 // b7b3-b807, the forced path
    both += 2; byte DGROUP:0xa163 = lo8(c)
    seg13:0xadbe(s, 0x15, -1)                                    // result ignored
    seg3:0x6ec(s, 14, DGROUP:0x9316, m)                          // after the call-in: the newest ship speaks
    if s == P: byte DGROUP:0xa14d[c] += 1                        // 8-bit wrap
    DGROUP:0x104a = 0; return
for k = 0 .. DGROUP:0x9316 (signed `jg` exit):                    // b80a-b8c9, the witness test
    if team(k) == 7 && pool14[k].state == 0 && ship[k].+0x52 not in {0x16, 0x17}:
        d = seg8:0x285a(&[bp-6], &[bp-8], pool14[k] (x,y), pool14[s] (x,y))   // cells, signed
        if d < 0x960 && !(c == 6 && sbyte a14d[6] > sbyte a157[6]):
            if seg13:0xadbe(s, 0x15, -1) == 0:                   // b8cd-b8dd
                seg3:0x6ec(s, 14, k, 0x44); return               // 559, absent: voice only
            seg3:0x6ec(s, 14, k, m)                              // the witness speaks
            byte DGROUP:0xa163 = lo8(c); if s == P: byte DGROUP:0xa14d[c] += 1
            return
    (team(k) == 7 && state 0 also clears seen0: a dead store)  // b826-b847
if 4 < DGROUP:0x9648 < 1000 (signed):                             // b90b-b92c, nobody saw it
    seg3:0x6ec(P, 7, 0, 0x6e); DGROUP:0x9648 = 1001              // to the player, whoever s is
```
Further facts:
- **The witness's "state 0" is pool-14 object `state` +0x18 through DGROUP:0x1e30, and the witness test has no `alive` test** (+0xec): a dead Enforcer still in `state` 0 during its 6 s tumble witnesses [BYTES seg13:0xb86b-0xb87a]. The witness may be s itself when s is an Enforcer.
- **Line 707 goes to the player for any offender** (recipient DGROUP:0x9646, not s) [BYTES seg13:0xb920].
- **DGROUP:0x9648 = 1001 is sticky for the wave:** it stays above 4, so seg4:0x7e2a's Patrolman block (DGROUP:0x9648 > 4) stays open for the rest of the wave once 707 has been said; seg9:0x64c8 clears it at the next wave start [BYTES seg13:0xb92c seg4:0x82a1 seg9:0x64c8].
- **The forced message's sender is ship DGROUP:0x9316 after the call-in**, which is the new Interceptor when seg13:0xae68 built one (the k1 and k5 panels name INTERCEPTOR), and whatever ship is last in the roster when it brought in an off-world one or reused one in play [BYTES seg13:0xb7e1] [ORACLE ships-0003].
- **What each counter is** (writers and lifetime):

| Word | Size | Meaning | Writers | Lifetime |
|---|---|---|---|---|
| DGROUP:0x964a | word | the session crime level | seg13:0xb781/0xb78f/0xb79b/0xb7a9/0xb7c0 (crime routine), seg4:0x63fc (+1), seg4:0x644d (+3); seg9:0x2762 (0, in seg9:0x22b4); seg6 restore (header +0x30) | saved (header +0x30), per game; read by seg4:0x7e2a block 3 (> 6, and its score /2), seg9:0x2a52-0x2aba (the wave end's Patrol chance class: 0; 1…4; 5…8; 9…15; > 15, world.md), seg14:0x254b (> 7 picks PATROLMAN over CONSTABLE) |
| DGROUP:0x9648 | word | the wave's crime level | the same five sites, seg4:0x6400 (+3), seg4:0x6452 (+3), seg13:0xb92c (1001) | per wave: seg9:0x64c8 = 0 at the wave start; not saved; read by seg4:0x82a1 (> 4) and seg13:0xb90b |
| DGROUP:0x964c | word | Enforcers and helpers called in this wave | seg13:0xb644 (+1 per new ship), seg9:0x6527 (0) | per wave ([pilots.md](pilots.md)) |
| DGROUP:0xa14d + c, c = 0…9 | 10 bytes | the player's crimes, counted when witnessed or forced (c = 0, 5, 7 directly) | seg13:0xb7fd, seg13:0xb905, seg4:0x7774 (c = 0), seg4:0x6449 (c = 5, capped), seg13:0x4b93 (c = 5), seg13:0xaa22 (c = 7); seg14:0x401 (0, fresh player record); seg6 restore | saved (header +0x174) |
| DGROUP:0xa157 + c | 10 bytes | the served crimes: a copy of DGROUP:0xa14d taken when the slate is cleared | seg16:0x7175 (end of the Hall of Justice text seg16:0x6afe), seg4:0x7794 (the death of a Terminator-class team-7 ship that a pool-14 ship other than a team-7 ship killed, [Terminator deaths]); seg14:0x405 (0); seg6 restore | saved (header +0x17e) |
| DGROUP:0xa163 | byte | the crime now being acted on | seg13:0xb7cd, seg13:0xb8f7 (c), seg4:0x6419 (5), seg13:0x4b8e (5), seg13:0xb9a4 (= DGROUP:0xa161) | not saved; read by seg13:0xae68 (8 lets kind 0x15 into a race), seg3:0x2d40 (the arrest activity line), seg13:0xb72d (5 suppresses crime 5), seg13:0xab01 |
| dword DGROUP:0xa147, word DGROUP:0xa14b | 6 bytes | the arrest record's frame# (DGROUP:0x1e68) and wave (DGROUP:0x91f8) | seg13:0xaaf4/0xaafb (the arrest's line 552), seg4:0x777c/0x7783 (a murder by P) | not saved; no reader in INSPACE.EXE outside these writers (xrefs) |
| DGROUP:0xa161, DGROUP:0xa162 | bytes | the arrest's crime (copy of DGROUP:0xa163) and the arresting ship's index | seg13:0xab01, seg13:0xab07; seg9:0xd33 (0xa161 = 0xff at every wave load, seg9:0xc06); seg6 restore (header +0x188/+0x189, which the writer leaves 0) | read by the Hall (seg16:0x6bfa-0x6ccb) and the warrant (seg13:0xb9a1) |
| DGROUP:0x104a | word | the force flag of F6 | seg11:0x733b (1), seg13:0xb801 (0) | session |

  [BYTES seg13:0xb781-0xb7c5 seg4:0x63fc seg4:0x644d seg9:0x2762 seg9:0x64c8 seg9:0x2a52-0x2aba seg14:0x254b seg4:0x82a1 seg13:0xb644 seg9:0x6527 seg13:0xb7fd seg13:0xb905 seg4:0x7774 seg4:0x6449 seg13:0x4b93 seg13:0xaa22 seg14:0x401 seg14:0x405 seg16:0x7175 seg4:0x7794 seg13:0xaaf4 seg13:0xaafb seg4:0x777c seg4:0x7783 seg13:0xab01 seg13:0xab07 seg9:0xd33 seg6:0x96f7 seg6:0x96ff] [ORACLE ships-0003 k5 watch-crime.txt: 0xa161 = 0xff from 13:26:24.902]
- **Who says which warning line:** the witness k says m (seg3:0x6ec(s, 14, k, m)); the forced path's speaker is ship DGROUP:0x9316; the failed Interceptor call gives 0x44 from the witness; nobody seeing it gives 707 from the "system" sender pool 7. Lines 550–558 are the Enforcers' order executor's and the attack reaction's (section 4); 860–863 are the Hall of Justice's last line for a repeat offender: when the served sum (Σ DGROUP:0xa157 + c, before this Hall's copy) ≠ 0, string 860 + `rand()` % 4 (seg5:0xdc, signed) is appended [BYTES seg16:0x712e-0x7152] (the Hall's track). The shared voice case seg3:0xf61 (codes 0x3b–0x3f, 0x41–0x44, 0x57, 0x58) draws one `rand()` % 4: 0 → sound 0x3d, 1 → 0x3e, 2 → 0x3f, 3 → no sound; its string is code + 0x1eb (491) [BYTES seg3:0xf61-0xf93].
- **Unwritten m (c = 0, 7, > 9):** the word [bp−0xc] holds what the stack held; ADR 0016 decision 1 reads 0, which seg3:0x6ec sends to its default case seg3:0x14d6 (code 0 is `dec` → 0xffff > 0xbf) [BYTES seg3:0x1346-0x134a]. The oracle's c = 7 panel shows the sender with no line (section 1), which the default case with no string matches. This is a read of an unwritten word ([pitfalls.md](pitfalls.md)).

## The Enforcer's executor seg13:0xa6bc(e) (Pascal, `retf 2`), the arrest and the escort [BYTES seg13:0xa6bc-0xadba]

Its one caller is seg13:0x6900 at seg13:0x693a (the think's re-plan for an order ≥ 1, after +0x94 = now + 4000). Words are ship words of e unless named; t = (+0x54, +0x56) the order's object; "now" is dword DGROUP:0x1e50; every deadline compare is 32-bit unsigned.

```
0.  +0x94 = now + 750                                                   // a6cb-a6e1
1.  DOUGHNUTS: if word DGROUP:0x9412 (pool-6 in-use count) != 0 && obj14[e].+0x36 == -1 (tether idle):
        for k = 0 .. DGROUP:0x9412-1: if word DGROUP:0x66e4 + 0x1c*k == 0x10 && pool6[k].state == 0:
            +0x06 = 6; +0x08 = k; +0x3a = +0x36 = 2
            if obj14[e].+0x36 == 14: fire the first slot j <= sbyte +0xcb with byte +0xb8+j == 15:
                 seg4:0x584c(j, e, 0)                                   // dead here: +0x36 was -1
            return                                                      // a6e5-a78b
2.  ORDER 0x14, PATROL (a78e-a921):
        w = +0x56; lost = obj(+0x54, w).state != 0 || ship[w].+0xec == 0 || dword +0x98 < now + 5000
        if lost:
            for n = 0 .. DGROUP:0x9316: w = (w + 1 > DGROUP:0x9316) ? 0 : w + 1      // cyclic, after +0x56
                if pool14[w].state == 0 && ship[w].+0xec != 0 && team(w) != 7: break
            +0x54 = 14; +0x56 = w; dword +0x98 = now + 13000
            if pool14[w].state != 0 || ship[w].+0xec == 0 || team(w) == 7: goto LEAVE_B7   // nobody to watch
        +0x06 = +0x54; +0x08 = +0x56; +0x3a = 2; +0x36 = 3                 // follow the watched ship
        if ship[+0x56] goal == (14, e) && its +0x38 == 1:                  // it is attacking e
            +0x36 = 1; seg4:0x8346(e, 14, +0x56, 0x12)
        return
3.  ORDER 0x15, WARN (a924-a987):
        if byte +0xed == 1: seg3:0x6ec(+0x56, 14, e, 0x3b); +0xed = 0      // 550 "Do Not Provoke Me! Or There Will Be Trouble."
        else: +0x52 = 0x14; dword +0x98 = now + 5000                       // back to patrol
        +0x94 = now + 2000; return
4.  ORDER 0x16, ARREST (a98a-ac34): t = +0x56
    if obj(+0x54, t).state != 0 || ship[t].+0xec == 0: goto 4C            // a998-a9c0 (t gone; [bp-8] stays unwritten)
    A. RESISTING (a9c6-aa76): if ship[t].+0x38 == 1 && ship[t] goal == (14, e) && dword +0x98 < now:
        h = #{j in 0..4 : ship word e+0x44+2j != 0}                        // the player's hit stamps on e
        if h > 1:
            if t == P: byte DGROUP:0xa154 += 1                            // crime 7, count
            +0x52 = 0x17; byte +0xed = (+0xed == 2 || +0xed == -1) ? 2 : 1
            dword +0x98 = now + 20000; +0x94 = now; return
    B. if sbyte +0xed <= 0: goto 4C                                       // aa79-aa83
       +0x06 = 14; +0x08 = t; +0x3a = 2; +0x36 = 3                        // follow t
       d = seg8:0x285a(&[bp-0xc], &[bp-0xe], obj14[e] (x,y), obj14[t] (x,y))   // cells, signed 16
       if d >= 250: goto 4D                                               // aac8-aace
       B1. if +0xed == 1:                                                 // aad2-ab0a
              seg3:0x6ec(t, 14, e, 0x3d)                                  // 552 "Follow Me Out. And Don't Try Anything!"
              +0xed = 0
              dword DGROUP:0xa147 = dword DGROUP:0x1e68 (frame#); word DGROUP:0xa14b = DGROUP:0x91f8 (wave)
              byte DGROUP:0xa161 = byte DGROUP:0xa163; byte DGROUP:0xa162 = lo8(e)
           else:                                                          // +0xed in 2..127
              seg3:0x6ec(t, 14, e, 0x57); +0xed = -1                      // 578 "You Will Pay For That Next Wave, Tough Guy!"
       B3. if sbyte ship[t].+0x5e > 0 && !(t == P && DGROUP:0x95bc != 0):    // ab28-aba7, the prisoner answers
              if team(t) in {3, 6, 1}:                                    // PIRATES, RENEGADES, PREDATORS fight
                  ship[t]: dword +0x52 = 0x000e0002 (+0x52 = 2, +0x54 = 14); +0x56 = e; +0x58 = t
                  seg4:0x8346(t, 14, e, 10)
              else:                                                       // the others comply: leave
                  ship[t]: +0x52 = 10; dword +0x98 = now + 500; +0x58 = e
       B4. for j = 0 .. sbyte +0xcb (inclusive): if byte +0xb8+j == 15:      // abaa-abda, the HANDCUFFS
              seg4:0x584c(j, e, 0); break
       goto 4D
    C. LEAVE BY GATE 0 (abdc-abf1): dword +0x06 = 12 (+0x06 = 12, +0x08 = 0); +0x3a = +0x36 = 2
    D. (abf5-ac34): if sbyte +0xed >= 1: return
       if obj14[e].+0x36 == 10: return                                    // the hook is flying
       if obj14[e].+0x36 == 14 && obj14[e].+0x38 == t: return             // the cuffs hold t
       +0xed = 1; return                                                  // lost hold: approach and throw again
5.  ANY OTHER ORDER (0x17, 0x3e, ...) (ac37-adb0): +0x52 = 0x17; t = +0x56
    if +0xed != 0:
        if +0xed == 2 || (sbyte e+0xf7+9t > 2 && sbyte e+0x13e+t < -2 && +0x26 < 55):   // ac52-ac71
            LEAVE_B7: seg3:0x6ec(P, 14, e, 0xb7)                          // 426 "I'm Heading For The Exit. See You Later...", sound 0x36
            dword +0x06 = 12; +0x3a = +0x36 = 2; byte +0xed = 0
            (only on this path) seg13:0xadbe(t, 0x14, 0x58); return       // a Terminator for t; 0x58 = 579, absent
        seg3:0x6ec(t, 14, e, 0x3e); +0xed = 0                             // 553 "Alright Tough Guy, I'll Resort To Force."
        seg3:0xf4(e, +0x54, +0x56); seg4:0x8346(e, 14, t, 0x13)
    if dword +0x98 < now && sbyte +0x5e == 1:                               // acf4-ad36, an Interceptor re-arrests
        +0x52 = 0x16; +0xed = 2; +0x94 = now; dword +0x98 = now + 5000; return
    if pool14[t].state != 0 || ship[t].+0xec == 0: seg3:0x6ec(P, 14, e, 0xb7); dword +0x06 = 12; +0x3a = +0x36 = 2; return
    seg3:0xf4(e, +0x54, +0x56); +0x06 = 14; +0x08 = t; +0x3a = 2; +0x36 = 1         // attack t
```
(The order-0x14 "nobody to watch" exit and the step-5 "t gone" exit are seg13:0xad53: message 0xb7 to the player and the gate estimate, without the Terminator call; the Terminator call is only at seg13:0xac73-0xacac.)

Further facts:
- **The hit stamps are the player's**: seg4:0x7d36(a, v, amount) shifts v's words +0x44 … +0x4a up one and writes +0x44 = lo16(frame# / 10) only when a = P and amount > 0 [BYTES seg4:0x7de1-0x7e1e]. So "resisting" (step 4A) means: t's goal is (14, e) with activity 1, e's +0x98 has passed, and the player has hit e at least twice at any time (the stamps are never cleared by this code). For a computer prisoner the stamps are still the player's. For the player as t, its goal words are the activity estimate of the 1500 ms tick (seg4:0x59bc): (14, e) with +0x38 = 1 needs the player to have fired within 2000 ms (DGROUP:0x8e28 + 2000 > now) with e scored best (section 6) [BYTES seg4:0x8642-0x8696].
- **The arrest record is rewritten on every throw.** After 552 and the throw (B1, B4), D returns while the hook flies (+0x36 = 10) or holds t; when a throw misses or the cuffs break (core.md: the HANDCUFFS rope breaks above 350 cells), D sets +0xed = 1 and the next re-plan (750 ms) inside 250 cells says 552 again and rewrites DGROUP:0xa147/0xa14b/0xa161/0xa162. The oracle's DGROUP:0xa147 = 1482, 1676, 1866, 1963, 2153 in `k1` and 731 → 822 in `k5` are these rewrites, and the goal flips (14, P) ↔ (12, 0) with +0x36 3 ↔ 2 are B (follow) and C (leave) alternating [ORACLE ships-0003].
- **Outside 250 cells nothing is said or thrown**: d ≥ 250 jumps to D, which with +0xed = 1 returns; the Interceptor keeps following [BYTES seg13:0xaace].
- **The prisoner's answer (B3) skips the player in a player game and tests the prisoner's byte +0x5e > 0**: for a non-Enforcer that byte is the attitude byte (structs.md +0x5d/+0x5e), not a class; a ship with +0x5e ≤ 0 neither fights nor leaves. The fight is order 2 (a command to attack e, with +0x58 = t itself) [BYTES seg13:0xab28-0xaba7].
- **The slot loop of B4 runs to +0xcb inclusive** (`cmp sext(+0xcb), j; jl` exits only when +0xcb < j), one byte past the slots; slot +0xcb holds 0 in a well-formed record (core.md) [BYTES seg13:0xabb9-0xabc1].
- **[bp−8] is unwritten when t is gone**: 4C then D compares object +0x38 with it [BYTES seg13:0xa9c0 seg13:0xac1f-0xac26]; ADR 0016 decision 1 reads 0.
- **Order 0x3e and the give-up:** the attack reaction's class-1 branch sets +0x52 = 0x3e, +0xed = 2 for a class ≤ 1 Enforcer hit by a third ship ([pilots.md](pilots.md)); step 5 turns it into 0x17 and, with +0xed = 2, the Enforcer at once says 426 to the player, heads for gate 0 and calls a Terminator (kind 0x14) for the attacker. seg13:0xae68 kind 0x14 asks for team 7 class 0 with order 0x17; kind 0x15 class 1 with order 0x16; kind 0x16 class 2 with order 0x14 [BYTES seg13:0xaec7-0xaef0 seg13:0xb042-0xb055].
- **426 is heard once per departure:** seg3:0x6ec returns at once for code 0xb7 when the sender's goal +0x32 or estimate +0x06 is already 12, and every 0xb7 call of seg13:0xa6bc and of the self-order is made before the gate estimate is written, so only the first re-plan of a departure says it [BYTES seg3:0x711-0x72b].
- **A called-in Terminator** (order 0x17, +0xed = 1, +0x98 = now + 5000 off-world or new) passes step 5 with grudge 0, so it says 553 to t, turns its defenders on t (seg4:0x8346(e, 14, t, 0x13)) and attacks t; from then on (+0xed = 0) it attacks t while t is alive and in play, then says 426 and leaves (a class-0 ship never re-arrests) [BYTES seg13:0xac59-0xadb0].

### The HANDCUFFS, the escort and the gate
1. B4's seg4:0x584c(j, e, 0) ([pilots.md](pilots.md)): nothing while e's object byte +0x2e ≠ 0; the waits; seg13:0x4eec(e, 5); W(15)+0x0a = 0 → seg11:0x947a(14, e, 0xf), which aims an Enforcer's cuffs at (+0x54, +0x56) (core.md "The tether weapons") [BYTES seg4:0x58c4-0x59ac].
2. The hook flies (object +0x36 = 10); a hit makes the holder's object (+0x36, +0x38) = (14, t) and the prisoner's object (`owner_pool` +0x32, +0x34) = (14, e) (k1: the player's pair (14, 3) at t0 + 22.02 s); the HANDCUFFS rope is 300 cells and breaks above 350 (core.md) [ORACLE ships-0003].
3. Every 750 ms re-plan with the hold: B follows t (+0x36 = 3) only while +0xed > 0; with +0xed ≤ 0 it is C: the estimate (12, 0) with +0x36 = 2, which the think copies into the goal; D returns because the cuffs hold. So the Interceptor flies to pool-12 record 0 (gate 0's first post, seg13:0x371c(12, 0) = 1) and drags t on the rope.
4. **Off screen** the think's hazard step seg4:0x4fc8 takes the ship out when its goal passes seg13:0x371c (pool 12: 1 for an even record, 2 for an odd one), its object is off screen (+0x1a = 0), the rect (x − 800, y − 800, x + extent(+0x22) + 800, y + extent + 800) meets gate (goal rec / 2)'s beam rect DGROUP:0x6678 + 0x1c·(rec/2), and dword +0x88 + 3000 < now: seg12:0x2288(14, i) [BYTES seg4:0x5468-0x5505 seg13:0x371c-0x3741]. **On screen** the gate mover calls seg12:0x2288 for a ship in the beam (core.md).
5. seg12:0x2288(pool, rec) for the Interceptor : ship +0x2e = 1, the vanish or `state` 1 with sound 0x27, then: a team-7 ship whose +0x54 = DGROUP:0x9324 and whose object (+0x36, +0x38) = (DGROUP:0x9324, +0x56) takes the prisoner through: seg12:0x2288(DGROUP:0x9324, object +0x38); when that prisoner is P: DGROUP:0x1e80 = now + 1000 and DGROUP:0x93a4 = 1. And P passing the gate itself while its object +0x32 = DGROUP:0x9324 and the holder (object +0x34) is of team 7 sets DGROUP:0x93a4 = 1 (the gate mover sets DGROUP:0x1e80 for P) [BYTES seg12:0x23fd-0x2472 seg12:0x2478-0x24b1].
6. **The hand-off.** DGROUP:0x1e80 due in a player game (DGROUP:0x95bc ≠ 0) with P's `alive` byte ≠ 0: `PlayMode` capped at 1, seg9:0x6bc2: seg10:0x7480(−1, 1), DGROUP:0x1e74 = 0, the race credit for kind 6, then with DGROUP:0x93a4 ≠ 0: **DGROUP:0x90f4 = 8**, DGROUP:0x929c = 0, seg16:0x6928(0), **DGROUP:0x9346 = −1** (no Terminator next wave), and every team-7 ship of class byte +0x5e = 0 whose +0x56 = P gets `alive` +0xec = 0; no debriefing entry here (seg9:0x6cc4 is not called) [BYTES seg7:0x62b-0x700 seg9:0x6bc2-0x6cb3]. In the demo (DGROUP:0x95bc = 0) seg7:0x659 sets state 8 and DGROUP:0x929c = 0 and runs the state-8 frame (seg7:0x1b7) without the DGROUP:0x9346 and Terminator writes [BYTES seg7:0x652-0x667].
7. **What the wave end saves.** Nothing is written at state 8's entry. The Hall (state 8, the Hall's track) adds the fine, copies DGROUP:0xa14d into DGROUP:0xa157 at the end of its text (seg16:0x7167-0x7182) and clears DGROUP:0x93a4 (seg16:0x6acb); its phase 10 calls seg9:0x6cc4(1) (state 2, the debriefing) in a player game, whose path writes `lastgame.sav` (ships-0003: the file is rewritten when the debriefing opens, 34.9 s after the Hall began) [BYTES seg16:0x6a87-0x6acb] [ORACLE ships-0003].
8. **Oracle times** (k1, t0 = F6): the Interceptor at t0 + 0.005 s; goal (14, P) at t0 + 0.021; follow at t0 + 0.081; on screen at t0 + 5.15; 552 and the record at t0 + 5.16 (the record's frame 1482); the hook record live from t0 + 5.15; the hold at t0 + 22.02; DGROUP:0x93a4 = 1 at t0 + 28.21; P's object `state` 1 at t0 + 28.89 (the vanish of an on-screen ship ends 0.68 s after seg12:0x2288); DGROUP:0x90f4 = 8 at t0 + 29.21 = DGROUP:0x93a4's write + 1000 ms. k5: hold at t0 + 10.07, DGROUP:0x93a4 at t0 + 10.43, state 8 at t0 + 11.47 [ORACLE ships-0003].

## The warrant and how an escape is remembered

### seg13:0xb93a(), every 300 frames (cdecl, no arguments) [BYTES seg13:0xb93a-0xba03]
Called from the wave frame's step 22 when dword DGROUP:0x1e68 (frame#) % 300 = 0 (32-bit unsigned `div`), with no other gate [BYTES seg7:0xd60-0xd78] .
```
for k = 0 .. DGROUP:0x9316 (signed `jg` exit):
    if ship[k].+0xec == 0 || pool14[k].state != 0 || team(k) != 7 || sbyte ship[k].+0x5e <= 1 || pool14[k].+0x1a == 0: continue
    W = sum over c = 0..9, c != 9 of sext(byte DGROUP:0xa14d + c)     // 16-bit
    S = sum over the same c of sext(byte DGROUP:0xa157 + c)
    if S >= W (signed): continue                                      // the next k computes the same sums
    if seg13:0xadbe(P, 0x15, -1) != 0:
        seg3:0x6ec(P, 14, k, 0xae)                                    // 1940 "Hey! I Have An Outstanding Warrant For Your Arrest!"
        byte DGROUP:0xa163 = byte DGROUP:0xa161
    return
```
So the warrant needs a Patrol-class Enforcer (PATROLMAN, CONSTABLE) alive, in play and **on screen** (object +0x1a ≠ 0); it is the player's alone; crime 9 never makes one; and its charge for the Interceptor's activity line and the next arrest record is DGROUP:0xa161, the last arrest's crime: 0xff after a game start (seg9:0xc06 writes 0xff at seg9:0xd33, also after a restore), so a warrant in a restored or newly started session charges "crime −1", whose activity string is 1098 − 1 = 1097 "AFTER WITNESSING YOUR DISGRACEFUL," [BYTES seg9:0xd33 seg3:0x2d40-0x2d44]. In kinds 6 and 7, in Demon's Gate and with more than `Detail` + 3 ships seg13:0xae68 refuses, and the warrant waits for the next multiple of 300 [BYTES seg13:0xae7c-0xaec5]. The routine acts at the first ship that passes the per-ship tests and returns after it whatever seg13:0xadbe returned: the sums do not depend on k, so every later k would give the same answer [BYTES seg13:0xb98b seg13:0xb9a7].

### What survives a wave and a save
- **Unserved crimes** (Σ DGROUP:0xa14d > Σ DGROUP:0xa157, c ≠ 9) are the warrant: saved in the header (+0x174, +0x17e), cleared only by the Hall of Justice's copy (seg16:0x7175), by the death of a Terminator-class Enforcer that a pool-14 ship other than a team-7 ship killed (seg4:0x7788-0x779f, [the death of an Enforcer]) and by a fresh player record (seg14:0x3fc-0x410, a new game's wave 1). An arrest the player escapes (leaving through the gate free, or the wave ending) leaves them unserved [BYTES seg16:0x7167-0x7182 seg4:0x7788-0x779f seg14:0x3fc-0x410 seg6:0x9086-0x909c seg6:0x96c9-0x96e6].
- **The Terminator warrant DGROUP:0x9346** (a roster index, −1 none): set by the murder of a non-Terminator Enforcer to the killer's roster index (seg4:0x7769, any killer) and by seg9:0x2828 (the debriefing entry seg9:0x6cc4 calls it at seg9:0x6d85) to P's roster index when a live team-7 ship of class 0 holds (+0x54, +0x56) = (14, P) [BYTES seg9:0x29d1-0x2a09]; cleared to −1 by the state-8 entry (seg9:0x6c74), the new-game reset (seg9:0xc56), every wave set-up's end (seg14:0xdbd, ships.md) and kinds 6/7; saved as header byte +0x18f. The next wave set-up places a Terminator (and a second one when DGROUP:0x9334 > 6, the warrant's ship is P, byte DGROUP:0xa14d (murders) > 2 and fewer than 7 ships) hunting the ship that carries that roster index (ships.md "The waves' ships") [BYTES seg14:0x797-0x833].
- **The crime levels:** DGROUP:0x964a is saved (+0x30) and drives the wave end's Patrol chance (world.md seg9:0x2828) and seg4:0x7e2a's Patrolman call; DGROUP:0x9648 is per wave.
- **Not saved:** the arrest record DGROUP:0xa147/0xa14b, DGROUP:0xa163; DGROUP:0xa161/0xa162 come back as 0 (the writer leaves header +0x188/+0x189 zero) and seg9:0xc06 then makes DGROUP:0xa161 = 0xff (formats.md).

## Patrol class, the call for an Interceptor, and the player's attack goal

- **How a Patrolman gets its order:** a call-in of kind 0x16 gives +0x52 = 0x14 (seg13:0xb048-0xb055); one placed at the wave start (+0x52 below 0x14) self-orders in the think's re-plan (seg4:0x6ef-0x814): +0x52 = 0x16 first; then for class < 2 with byte +0xcd ≥ 0 the target is +0xcd (or, when that ship is dead, message 0xb7 to P and the gate estimate, keeping 0x16); else for goal pool 14 or class 0 the target is (14, goal rec), with +0x52 = 0x17 for class 0 and **0x16 kept for a Patrol or Intercept ship whose goal pool is 14**; else +0x52 = 0x14 (or 0x16 for class 1 with +0x54 = 14). Then +0xed = 2, +0x58 = i, +0x94 = now, +0x38 = 3, seg13:0x6900(i) [BYTES seg4:0x6ef-0x814]. ([pilots.md](pilots.md) "The think, in detail"; the 0x16 kept for a Patrol ship heading for a ship is the point to keep.)
- **Patrol (0x14):** step 2 of section 4: it follows a non-Enforcer ship in play for about 8000 ms (the 13000 ms deadline is replaced 5000 ms before it ends), cycling through the roster; when the watched ship attacks it, it attacks back and sends its defenders (seg4:0x8346(e, 14, w, 0x12)); with no ship to watch it says 426 to the player and leaves by gate 0.
- **Warn (0x15):** set only by the attack reaction for a Patrol-class victim with a small grudge (crime 5 (a)); step 3 says 550 once to the attacker, then back to patrol with a 5000 ms deadline.
- **Call an Interceptor:** a Patrolman never calls one itself in its executor. The calls are: the crime routine's witness (any class, section 3), the attack reaction's crime-5 path (a Patrol-class victim with grudge ≥ 2: seg13:0xadbe(a, 0x15, 0x3c)), and the warrant (section 5.1, a Patrol-class ship on screen). So "they warn the offender and call in an Intercept Class Enforcer" [MANUAL Laws of Inner Space] is the witness and crime-5 paths.
- **The player's attack goal and crime 1.** In Action mode the player's goal words are the tick's copy of seg4:0x59bc's estimate ([pilots.md](pilots.md)). A candidate scored by seg4:0x8626 carries DGROUP:0x9686 = 1 when now < dword DGROUP:0x8e28 + 2000 (the player fired or used a weapon in the last 2000 ms), or when it is already the player's goal with +0x38 = 1 and now < DGROUP:0x8e28 + 3000; the winner's DGROUP:0x9686 makes +0x36 = 1 [BYTES seg4:0x8635-0x8696]. So destroying a good icon is crime 1 for the player only when, at the last 1500 ms tick, the player had fired within 2 s (3 s if it was already the attack goal) and the icon was the best-scored candidate (score base 30 for a capturable object, +25 while attacking, then the distance and angle terms of seg4:0x8626); an icon killed between ticks, or before the icon became the goal, is no crime. In Strategy mode (`PlayMode` ≠ 0) the computer pilot sets the goal as for any ship.

## The law's other sites

Sites that raise a crime, a warrant or an order outside seg13:0xb6f2 and seg13:0xa6bc. Ship fields are ship words (DGROUP:0x7090 + 0x15a·i + N); P is DGROUP:0x9646.

### seg3:0x6ec's filters on the law's messages
These decide whether any message of the routines above reaches the screen [BYTES seg3:0x6fa-0x828].
- It shows nothing unless its first argument is P (seg3:0x6fa-0x700), and nothing for sender pool 14 with the sender = P (seg3:0x704-0x70d).
- For code 0xb7 ("I'm Heading For The Exit", string 426) it shows nothing when the sender's +0x32 = 12 or +0x06 = 12 (seg3:0x711-0x72b). Every 0xb7 send in seg13:0xa6bc and seg4:0x78 sets the estimate +0x06 = 12 right after the call, so the line shows once per departure.
- Apart from pool −1 (its own path, seg3:0x77d-0x7c3), only sender pools 14, 7 and 2 get through (seg3:0x7c6-0x7d6).
- The display time is DGROUP:0x870c = now + 2500 for pool 7, now + 5000 for other pools, and now + 7000 while DGROUP:0x94e = 0 (seg3:0x7f3-0x828).

### seg13:0xa6bc: the preamble and the order-0x16 tail
- Every call first stores +0x94 = now + 750; seg13:0x6900, its only caller (seg13:0x693a), stores +0x94 = now + 4000 before the call [BYTES seg13:0x690f-0x6925 seg13:0xa6cb-0xa6e1].
- The doughnut step needs DGROUP:0x9412 ≠ 0 and the object word +0x36 = −1. The weapon-15 search right after it (seg13:0xa77d-0xa78b, seg4:0x584c) needs the same object word to be 14, so it never runs [BYTES seg13:0xa6e5-0xa78b].
- When no ship qualifies as the next watched ship in order 0x14, the step loop runs DGROUP:0x9316 + 1 steps and ends on the old +0x56; that ship then fails the test that follows unless it has become eligible since the flag test. The re-pick flag has four sources: the watched object's `state` ≠ 0; the watched ship's +0xec = 0; the watched ship's +0x38 ≠ 1 with e's +0x98 < now; e's +0x98 < now + 5000 (32-bit unsigned). The third is contained in the fourth [BYTES seg13:0xa78e-0xa897].
- The five words that step 4A counts are e's own +0x44 … +0x4c (`imul bx, [bp+6], 0xad; add bx, j; add bx, bx`, seg13:0xa9f7); the test counts the non-zero words and makes no time comparison [BYTES seg13:0xa9c6-0xaa18].
- The call seg4:0x584c(j, e, 0) is Pascal (`retf 6`), so j, e, 0 are in declared order; it reads j at [bp+0xa] and e at [bp+8] and stores e's +0x18 = j [BYTES seg4:0x584c-0x59ac]. For e ≠ P, seg3:0xf4(e, pool, rec) only stores e's +0x3e/+0x40 = (pool, rec) and does not write +0x56, so the seg4:0x8346 call that follows receives t [BYTES seg3:0xf4-0x11b].
- The dword store 0x000e0002 at seg13:0xab85 sets +0x52 = 2 and +0x54 = 14. The tail at seg13:0xabf5 returns at once when +0xed ≥ 1; the resisting path returns through seg13:0xa97f and never reaches it [BYTES seg13:0xab85 seg13:0xabf5 seg13:0xa97f].
- The pool test on the handcuff link is the literal 14: t's object is read through the pool-14 pointer DGROUP:0x1e30 [BYTES seg13:0xac46].

### seg4:0x5e8e: the crime sites of the attack reaction
The reaction is seg4:0x5e8e(v, a, amount, kind) (cdecl; v the victim, a the attacker); [pilots.md](pilots.md) has the rest of its branches.
- **Crime 3, a fleeing victim (+0x38 = 4)**, when T(7, team v) > −100 and T(7, team a) < 100: crime 3 needs amount [bp+0xa] > 10, `rand()` % 4 ≤ 1 and the victim's word +0x26 < 40, and calls seg13:0xb6f2(a, 3). Otherwise the first team-7 ship k < DGROUP:0x9316 (strict `jge`, so the last ship is excluded; no `state` or alive test) sends seg3:0x6ec(a, 14, k, 0x5c) [BYTES seg4:0x5f7b-0x6015]. The tables are T(7, ·) = word DGROUP:0x3894 + 2·team; a fleeing victim skips both crime 3 and message 0x5c when T(7, team v) ≤ −100 or T(7, team a) ≥ 100 [BYTES seg4:0x5f89-0x5fb2 seg4:0x6018].
- **Crime 3, a victim nearly out of fuel**, for a = P (seg4:0x60ff-0x61cb): the per-attacker byte v+0xc2+a must be < 20 (at 20 it is reset to 0 and the routine returns). The routine returns when v's goal is (14, a) with +0x38 = 1 and v is not team 7. It needs v's +0x1e < design word DGROUP:0x96f0 / 10 (signed) and sends seg3:0x6ec(P, 14, v, 0xbe). If v+0xc2+a = 4, the amount doubles and, when T(7, team v) > −100 and T(7, team a) < 100, it calls seg13:0xb6f2(a, 3); if not 4, it plays sound 0x3d. Either way it then sets v+0xc2+a = 4 and returns.
- **Crime 5, an Enforcer is hit** (seg4:0x633a-0x6480): the branch applies when the victim's team [bp−8] (seg4:0x6048) is 7, the attacker is not team 7 and the victim's class +0x5e > 1.
  - Per-attacker byte v+0xc2+a < 2: when no class-1 team-7 ship in `state` 0 and alive exists, seg3:0x6ec(a, 14, v, 0x3b) (550 "Do Not Provoke Me!"); then v's +0x52 = 0x15, +0x54 = 14, +0x56 = a, +0xed = 1, +0x94 = now; DGROUP:0x964a += 1 and DGROUP:0x9648 += 3; then the tail seg4:0x66e9.
  - Byte ≥ 2: seg13:0xadbe(a, 0x15, 0x3c); a non-zero result stores DGROUP:0xa163 = 5, a zero result sends seg3:0x6ec(a, 14, v, 0x43). For a = P, byte DGROUP:0xa152 rises by 1 only while sext(DGROUP:0xa152) − 1 < sext(DGROUP:0xa15c), that is, last wave's count for code 5. Both crime words += 3 and v's +0x52 = 0x14; v's bytes +0xc2 + j for j = 0 … DGROUP:0x9316 are cleared (seg4:0x6462-0x647f).
  - Victim class ≤ 1 (seg4:0x6484-0x64ea): the routine returns if v already targets (14, a), or if v targets another ship and its class is not 1. Otherwise a byte > 2 sets v's +0x52 = 0x3e, +0xed = 2, +0x98 = now + 20000 and (+0x54, +0x56) = (14, a); a byte ≤ 2 sends seg3:0x6ec(a, 14, v, 0x41) (556).
  - All three paths end at seg4:0x66e9, which adds the hit score [bp−0xc] (built at seg4:0x62b1-0x6337 from the attitude read, the amount and the class) to v's byte +0xc2 + a, so after the clear that byte restarts at the score.

### The death of an Enforcer, seg4:0x7636
For a dead team-7 ship with class byte +0x5e > 0 and a pool-14 killer (after the substitution below) [BYTES seg4:0x7725-0x779f]:
- DGROUP:0x9346 = the killer's +0xd6, for any killer ship, not only P (seg4:0x7760-0x7769).
- When the killer is P, byte DGROUP:0xa14d += 1 (code 0, " Murdering An Enforcer."; no seg13:0xb6f2 call) and dword DGROUP:0xa147 = frame#, word DGROUP:0xa14b = DGROUP:0x91f8 (seg4:0x776c-0x7783).
- For a killer = P the routine also raises word DGROUP:0x6b40 + 2·team(dead), whatever the team (seg4:0x7738-0x774a).
- Other writers of DGROUP:0x9346: seg6:0x96b8, seg9:0xc56, seg14:0xf1 and seg14:0xdbd (−1 for the last three).

**The Terminator copy.** seg4:0x7788-0x779f copies DGROUP:0xa14d … +9 into DGROUP:0xa157 … +9 when the dead ship is of design team 7 with signed byte +0x5e ≤ 0, its original killer is not a team-7 ship, and the killer after substitution has pool 14 [BYTES seg4:0x7669-0x768f seg4:0x7692-0x7721 seg4:0x774e-0x775e seg4:0x7788-0x779f]. A team-7 killer (a ship of pool 14 and design team 7) makes seg4:0x7636 store the dead ship's estimate = killer and return (seg4:0x7669-0x768f). The substitution: the killer becomes (14, goal rec) when the dead ship's goal pool is 14 and its byte +0xf7 + 9·goal rec > 3 (seg4:0x7692-0x76b3); otherwise it becomes (14, j) for the ship j ≠ dead with the highest score above 7, where the score is sext(byte +0xf7 + 9j), raised by 5 and then by 100 more when j is the killer and the raised value exceeds 12 (seg4:0x76b6-0x771a). If the killer pool is still not 14, the routine goes to seg4:0x7977 and does not copy.

### Crime 5 by a dropped resource pack, seg13:0x4ac4(ship)
Its only call is seg11:0x7043: WM_KEYDOWN 'S' with Ctrl and Alt held, or the equivalent 0x99b/0x99e flags (seg11:0x7033-0x7043); Ctrl alone calls seg13:0x9810(P), neither modifier seg13:0xa568() ([pilots.md](pilots.md) "The player's commands to ships"). It first calls seg13:0x498e(ship, 0). A matching ship j with +0xd6 ≠ −1 gets DGROUP:0x8680 + 2·ship = j, +0x38 = 2 and dword +0x32 = 11 (goal (11, 0)), and the scan goes on (seg13:0x4ae5-0x4b05). The first ship j ≠ ship with pool-14 `state` 0, goal (14, ship), +0x38 = 1 and +0xd6 = −1 gets DGROUP:0x8680 + 2·ship = j (seg13:0x4b50), +0x52 = 0x16, +0x54 = 14, +0x56 = ship, +0x98 = now + 50000, +0xed = 1, +0x94 = now + 500, and for ship = P byte DGROUP:0xa163 = 5 and byte DGROUP:0xa152 += 1 (no cap) [BYTES seg13:0x4ac4-0x4b97].

### Other writers of the arrest record
seg6:0x96fc and seg6:0x9704 (in seg6:0x930a, called from seg11:0x7a8d and seg15:0x5b91) write DGROUP:0xa161 and DGROUP:0xa162. seg4:0x6419 writes DGROUP:0xa163 = 5. The readers of DGROUP:0xa161 and 0xa162 in seg16 are seg16:0x6bfa-0x6ccb. The history words +0x44 … +0x4c that the resisting test counts are cleared for ships 0 … 7, with the bytes +0x13e + j and +0xc2 + j for j < 8, at seg9:0x2430-0x2479 (in the routine around seg9:0x22b4) [BYTES seg9:0x2430-0x2479]. The routine's place in the wave set-up is [OPEN].

### seg13:0xae68's gates on the call-in
These decide whether the crime routine's seg13:0xadbe(s, 0x15, −1) returns non-zero [BYTES seg13:0xae7c-0xaec5]: in a kind-6 or kind-7 wave only kind 0x15 with byte DGROUP:0xa163 = 8 passes; with 2·Detail + 1 < DGROUP:0x964c only kind 0x15 passes; with Detail + 3 < DGROUP:0x9316 (signed) nothing passes; while DGROUP:0x92e8 ≠ 0 only kind 0x13 passes.

### The prisoner's gate passage, seg12:0x2288
For a team-7 ship of pool DGROUP:0x9324 with +0x54 = DGROUP:0x9324 whose object (+0x36, +0x38) = (DGROUP:0x9324, +0x56), the HANDCUFFS link, seg12:0x2288 calls itself with (DGROUP:0x9324, object +0x38), so the prisoner leaves with the Enforcer; if that prisoner is P it sets DGROUP:0x1e80 = now + 1000 and DGROUP:0x93a4 = 1 (seg12:0x23fd-0x2472). When P passes a gate with its object (+0x32, +0x34) = (14, a team-7 ship), it also sets DGROUP:0x93a4 = 1 (seg12:0x2478-0x24b1). DGROUP:0x93a4 is also read at seg7:0x652, which sets DGROUP:0x90f4 = 8 and DGROUP:0x929c = 0 and jumps to seg7:0x1b7 with 1, skipping seg9:0x2828 (seg7:0x652-0x667); it is cleared at seg9:0x6111 and seg16:0x6acb. The conditions that lead to seg7:0x630 are [OPEN].

### seg4:0x78's other self-order branches (seg4:0x734-0x7e7)
With byte +0xcd ≥ 0 and class < 2: if ship +0xcd is dead, the ship sends seg3:0x6ec(P, 14, i, 0xb7) and sets the estimate (12, 0) with +0x36 = +0x3a = 2; if it is alive, the ship keeps +0x52 = 0x16 with (+0x54, +0x56) = (14, +0xcd). Otherwise class 0 gets (14, +0x34) and +0x52 = 0x17, whatever the goal pool; when +0x32 ≠ 14 and class ≠ 0, the order is 0x16 for class 1 with +0x54 = 14 and 0x14 in every other case. +0xcd is written by the call-in (seg13:0xb1e3, 0xb2da, 0xb640) [BYTES seg4:0x734-0x7e7]. A class-0 or class-1 Enforcer whose goal pool is 14 keeps 0x16 (a Patrol or Intercept ship), and the self-order of a class-0 ship is 0x17 [BYTES seg4:0x720-0x7b1].

### The player's attack goal (seg4:0x8626 and seg4:0x59bc)
seg4:0x8626 stores DGROUP:0x9686 = 1 when dword DGROUP:0x8e28 + 2000 > now, or when the player's goal is the scored (pool, rec) with +0x38 = 1 and DGROUP:0x8e28 + 3000 > now, and 0 otherwise; it also clears DGROUP:0x968a and DGROUP:0x9688 [BYTES seg4:0x8642-0x8696]. seg4:0x59bc later overwrites DGROUP:0x9686 at seg4:0x5c78, so the value seg4:0x8626 leaves is not the final one ([pitfalls.md](pitfalls.md)).

### The race winner, seg4:0x8d3a()
It returns [bp−0xa]: the ship with byte +0xec ≠ 0, pool-14 object `state` 0 and object byte +0x51 = 2 whose dword DGROUP:0x9e60 + 4i is least (unsigned, from 1000000). The word is written only when a candidate beats the best, so with no finished ship it is a stale stack word (ADR 0016 decision 1 reads it as 0) [BYTES seg4:0x8d47-0x8db0].

## The Hall of Justice (state 8)

### Into state 8, the set-up, the phases and their pace

#### The way in
- The flag DGROUP:0x93a4 = 1 is set by the gate routine seg12:0x2288 in two cases (core.md
  "seg12:0x2288", items 6 and 7): a team-7 ship that takes its HANDCUFFS prisoner through, when
  the prisoner is the player, also sets DGROUP:0x1e80 = DGROUP:0x1e50 + 1000 [BYTES
  seg12:0x23fd-0x2472]; the player going through while its object `owner_pool`/`owner_rec`
  (+0x32/+0x34) name a team-7 ship sets only the flag [BYTES seg12:0x2478-0x24b1]. In the second
  case DGROUP:0x1e80 comes from the player's own gate exit (seg12:0x1e3b, + 1000, shell.md).
  "Team 7" is the design word +0x38 at DGROUP:0x970a + 0xad·n (n = ship word +0x4e) = 7, the
  ENFORCERS (the ENFORCERS).
- The oracle agrees: k1 flag at 13:13:46.982, state 8 at 47.982 (1.000 s); k5 flag 59.748,
  state 8 at 13:27:00.785 [ORACLE ships-0003].
- **Player game.** DGROUP:0x1e80 due → seg7:0x62b: DGROUP:0x95bc ≠ 0 → seg7:0x6dd (PlayMode ≤ 1;
  the player's `alive` +0xec = 0 sends DGROUP:0x1e84 = now, i.e. GAME OVER, else seg9:0x6bc2),
  then DGROUP:0x1e80 = 0 at seg7:0x6a0.
- **seg9:0x6bc2** [BYTES seg9:0x6bc2-0x6cc3]: seg10:0x7480(−1, 1) (hard pause); DGROUP:0x1e74 = 0;
  the race credit loop in kind 6; then with DGROUP:0x93a4 ≠ 0:
  ```
  DGROUP:0x90f4 = 8                       // seg9:0x6c62
  DGROUP:0x929c = 0                       // seg9:0x6c6a, the phase word
  seg16:0x6928(0)                         // seg9:0x6c6e: runs phase 0 at once (below)
  DGROUP:0x9346 = -1                      // seg9:0x6c74
  for k = 0 .. DGROUP:0x9316 (signed, inclusive):          // seg9:0x6c7a-0x6cb3
      if design word DGROUP:0x970a + 0xad·(ship k word +0x4e) == 7
         and sbyte ship k +0x5e (DGROUP:0x70ee, the class) == 0        // Terminator class only
         and ship k word +0x56 (DGROUP:0x70e6) == P:
          byte ship k +0xec (DGROUP:0x717c, `alive`) = 0
  return                                  // no seg9:0x6cc4 on this path
  ```
  core.md:479 says "team-7 ships escorting the player"; the bytes add the class-0 test (only a
  Terminator or Eliminator ordered against the player), and the escort itself (class 1) keeps
  `alive`.
- **Demo** (DGROUP:0x95bc = 0): seg7:0x652-0x667 sets state 8 and DGROUP:0x929c = 0 and jumps to
  seg7:0x1b7 with the argument 1 (seg16:0x6928(1)), leaving DGROUP:0x1e80 set and running neither
  the DGROUP:0x9346 store nor the alive loop [BYTES seg7:0x652-0x667].

#### The state-8 frame and the repaint
- Tick case 8 (seg7:0x197): DGROUP:0x22 = 0; the ungated counter DGROUP:0x9144 against the
  threshold DGROUP:0x9004; on a frame seg7:0x2206 (the adaptive threshold, target 72 frames/s,
  timing.md) and seg16:0x6928(0). No input is handed to any handler [BYTES seg7:0x197-0x1bc].
- Repaint (seg11:0xafe case 8): seg16:0x6928(1) [BYTES seg11:0x1686].
- **seg16:0x6928(arg)** (cdecl, one word; 11-entry jump table seg16:0x6ae0) [BYTES
  seg16:0x6928-0x6afc]:
  ```
  if (GetKeyState(VK_CONTROL=0x11) & 0x8000) and DGROUP:0x92a2 != 1: DGROUP:0x928 = 1   // 0x6935
  if DGROUP:0x8f70 (QuickIntro) > 1 (signed): DGROUP:0x928 = 1                        // 0x694e
  if arg != 0 and (DGROUP:0x929c > 1 or DGROUP:0x92a2 >= 1): seg15:0x760()             // 0x695b
  switch DGROUP:0x929c (unsigned > 10 does nothing):
   0: seg11:0x2f0(); seg15:0x302e(); phase++                       // landing scene, scene 110
   1: seg15:0x3590()        // jet, descent set-up, descent; raises the phase itself (landing)
   2: seg16:0x6afe(); phase++                                       // the text, the punishment (2, 3)
   3, 7: seg15:0x1584(0)    // typing; raises the phase itself (typewriter)
   4, 8: t0 = GetCurrentTime(); do seg10:0x740c() while GetCurrentTime() < t0 + 1500
         (32-bit, high word first); phase++                         // 1500 ms inside one frame
   5: seg15:0x1584(0); dword DGROUP:0x1c64 = 0                      // typing
   6: if dword DGROUP:0x1c64 == 0: DGROUP:0x1c64 = GetCurrentTime() + 4000
      if GetCurrentTime() <= DGROUP:0x1c64 (unsigned 32-bit): return
      seg15:0xcb0(); phase++                                        // launch prep (launch)
   9: seg15:0x10be()        // the launch; raises the phase at its step 9 (launch)
  10: PatBlt(DGROUP:0x13dc, 0, 0, W = DGROUP:0x90ea, H = DGROUP:0x10, BLACKNESS 0x00000042)
      t0 = GetCurrentTime(); DGROUP:0x928 = 0; seg15:0x1328(0)
      do seg10:0x740c() while (GetCurrentTime() - t0) has high word 0 and low word < 2000
      seg10:0x6e76(8, 0); seg10:0x6dd4(0)
      if DGROUP:0x95bc != 0: seg9:0x6cc4(1)             // state 2, the debriefing ("The exit and the debriefing")
      else: DGROUP:0x90f4 = 1; DGROUP:0x93a4 = 0        // demo
  ```
  Phases 4, 8 and 10 block the frame loop for their whole wait (busy waits that pump only
  seg10:0x740c, ISPACE ordinal 7 command 9, the sound pump); phase 6 is a per-frame deadline.
- **The repaint during the Hall** runs the current phase's step a second time after
  seg15:0x760 (the switch follows the seg15:0x760 call whatever `arg`), and seg15:0x760 blits the
  scene DC over the screen: a WM_PAINT during the typing wipes the typed text, which the scene
  DC holds only after phase 9's step 5 copy (launch) [BYTES seg16:0x695b-0x6977 seg15:0x760].

#### The landing: scene 110, the jet, scene 115 and the descent
State 8 shares the state-6 landing (shell.md "The landing scene"); the differences are:
- **seg15:0x302e in state 8** [BYTES seg15:0x302e-0x358d]: seg13:0x65f2(3); music
  seg10:0x7480(−2, 1) (not in state 9); sound seg10:0x744c(0x6e); PatBlt black and
  seg10:0x6da6(0, 1) (DGROUP:0x93a6 = 0); the slow-machine text; DGROUP:0x92d6 = trunc(22H/100),
  DGROUP:0x92d8 = H − 10; unless the fast flag DGROUP:0x928: **scene 110** (`push 0x6e`,
  seg15:0x3284) rendered by seg10:0x9526 (ISPACE ordinal 32) at W × H into DGROUP:0x9294 in the DC
  DGROUP:0x929a (palette DGROUP:0x920e selected), then SetRect(&DGROUP:0x3ca8, out[0], out[1],
  out[2], out[3]) (slot 0 of the out block DGROUP:0x3cc0), DGROUP:0x92de = H and DGROUP:0x92d6 =
  DGROUP:0x3caa + 20 (stored at seg15:0x34f9) [BYTES seg15:0x325d-0x32f7]; the BitBlt to the
  screen; seg15:0x1a42 (the jet's frames and seg15:0x20ca, the player's ship), DGROUP:0x92a2 = 0,
  seg15:0x3caa(1) (sound 0x1c).
  Scene 110 is the long hall: six pillars, the far monitor, the blue star disc, the jet entering
  from the bottom (`k5/r-5`) [ORACLE ships-0003].
- **seg15:0x3590 in state 8** [BYTES seg15:0x3590-0x374a]: sub-step DGROUP:0x92a2 = 0 runs
  seg15:0x3caa(0) per frame; when it returns non-zero: dword DGROUP:0x17f2 = 0, DGROUP:0x92a2 = 1,
  seg15:0x1328(1) (which keeps the scene DC in state 8, shell.md), seg15:0x1eac (the two ship
  frames: frame 0 with flame, frame 1 without), PatBlt black (DGROUP:0x93a6 = 0), unless
  DGROUP:0x928 the ordinal-14 fades seg10:0x993a(3, 0xcc) and (2, 0x89), then for state ≠ 6
  **seg10:0x6e76(−2, 2)** (no greys, no pad) [BYTES seg15:0x36ad-0x36b6] and seg15:0x374c(1).
  Sub-step 1: while dword DGROUP:0x17f2 = 0, seg15:0x374c(0) per frame; its non-zero return plays
  **sound 9** and sets DGROUP:0x17f2 = DGROUP:0x1e50 + 1000; once DGROUP:0x1e50 passes it
  (unsigned 32-bit, strictly) DGROUP:0x92a2 = 2 [BYTES seg15:0x36c3-0x370e]. Sub-step 2 in state 8
  raises **DGROUP:0x929c** (1 → 2) [BYTES seg15:0x371d-0x3728].
- **seg15:0x374c(1) in states 8 and 9** (not 6) [BYTES seg15:0x387d-0x38fe]: PatBlt black;
  DGROUP:0x92d6 = −1; **seg15:0x760()** (below), which now renders the Hall; the clip
  DGROUP:0x3cb0..0x3cb6 = (0, 0, W, H); with s4 = 4·sz ([bp−6]): the end y DGROUP:0x92d6 =
  DGROUP:0x92ba − s4 and the start y DGROUP:0x92d8 = DGROUP:0x3cae − s4 (the monitor rectangle's
  bottom), or end + 1 with the fast flag; y10 DGROUP:0x17fa = 10·start, DGROUP:0x17f8 = DGROUP:0x17fc
  = 0, the work DC seg15:0x3bfa(s4, s4 + 40), speed DGROUP:0x17f6 = 150. The call that ends the
  descent (10·DGROUP:0x92d6 < y10) sets **DGROUP:0x92d8 = −1** in state 8 and returns 1 [BYTES
  seg15:0x3bd4-0x3bef]. So the ship comes down from under the monitor onto the disc.
- **seg15:0x760, the Hall's scene** (cdecl, no arguments; callers seg15:0x389d and seg16:0x696f)
  [BYTES seg15:0x760-0x9e4]:
  ```
  if DGROUP:0x929a != 0:
      if (DGROUP:0x929c > 1 or DGROUP:0x92a2 > 0) and DGROUP:0x92d6 != -1:    // redraw
          BitBlt(DGROUP:0x13dc, 0, 0, W, H, DGROUP:0x929a, 0, 0, SRCCOPY)
          if (DGROUP:0x92a2 == 1 and DGROUP:0x92d8 != -1) or DGROUP:0x929c > 6: return
          s4 = 4·(sext(R+0x7a) + (sext(R+0x7b) - 6)/2), R = roster record of P's +0xd6
          BitBlt(13dc, W/2 - s4/2, DGROUP:0x92d6 + 5, s4, s4, DGROUP:0x3cde, 0, DGROUP:0x3cea, 0x008800C6)
          BitBlt(13dc, same, DGROUP:0x3cdc, 0, DGROUP:0x3cea, 0x00EE0086)   // frame 1, no flame
          return
      free the scene DC (old bitmap back, palette back, DeleteDC, DeleteObject; both words 0)
  elif DGROUP:0x929c > 5: return
  the 8-byte out block DGROUP:0x92a6 / far DGROUP:0x3cc0 (allocated when 0)
  DGROUP:0x9294 = seg10:0x9526(out, DGROUP:0x92a6, scene 0x73 = 115, 0 x6, W, H, 0, 0, 0)  // seg15:0x903
  DGROUP:0x929a = CreateCompatibleDC; select the bitmap and DGROUP:0x920e
  SetRect(&DGROUP:0x3ca8, out[0], out[1], out[2], out[3])     // the monitor (slot 0)
  DGROUP:0x3ca4 = out[0]; DGROUP:0x3ca6 = out[1]; DGROUP:0x92ba = out[5]
  DGROUP:0x92de = H; DGROUP:0x3d48 = DGROUP:0x3ca4; DGROUP:0x3d4a = DGROUP:0x3ca6
  seg15:0x8604()                       // the monitor frame and black screen, shell.md "Monitor"
  BitBlt(13dc, 0, 0, W, H, DGROUP:0x929a, 0, 0, SRCCOPY)
  free the out block
  ```
  The Ghidra C of seg15:0x760 matches its listing at these points; the scene id 0x73 and the
  word indices 0, 1, 2, 3, 5 are from the listing's pushes and `es:[bx + 0xa]`. Scene 115 is the
  near view: two pillars, the steps, the star disc, the monitor with the Software Dynamics badge
  (`k1/f6-92`) [ORACLE ships-0003]. ispace.md lists 115 as "seg15:0x903" without naming the
  routine: it is seg15:0x760, the state-8 scene.
- **The ship image in the Hall goes through seg15:0x28b2 when DGROUP:0x9e5b = 0**: seg15:0x1a42
  calls seg15:0x20ca outside state 9, and seg15:0x20ca calls seg15:0x28b2 ("dead ship", [OPEN],
  draws `rand()` at seg15:0x28de … 0x2990) while DGROUP:0x9e5b = 0 [BYTES seg15:0x239f-0x23b6
  seg15:0x27af]. The wave placement sets DGROUP:0x9e5b = 0 [BYTES seg9:0x6865 seg9:0x6867] and
  only the ambulance's pick-up (seg13:0x3772, seg16:0x441, seg16:0x3b07) and the seg18 entry
  write it again, so after an ordinary wave the Hall builds the ship through seg15:0x28b2 [OPEN:
  what it does to the squares; k1's Hall sprite shows no obvious burn; see Later stages].

#### The text and the typewriter (seg15:0x1584 in state 8)
- seg16:0x6afe sets DGROUP:0x92d4 = 0 first [BYTES seg16:0x6b0c] and ends with seg15:0x1584(1)
  [BYTES seg16:0x7193-0x719a]; with DGROUP:0x92d4 = 0 that call is the init: the CRT flash
  seg15:0x84b0(W/2, DGROUP:0x3ca6 + DGROUP:0x92d2/2), DGROUP:0x92a = 0, index DGROUP:0x17ea = 0, line
  DGROUP:0x17ec = 0, x DGROUP:0x17e2 = DGROUP:0x3d48 + 29, y DGROUP:0x17e4 = DGROUP:0x3d4a + 29,
  deadline DGROUP:0x17ee = DGROUP:0x1e50 + 500 [BYTES seg15:0x15b1-0x1612]. The font, cursor,
  wrap and scroll are shell.md's "Typing" items (same routine); the font height h =
  (DGROUP:0x3cac − DGROUP:0x3ca8 − 63)/15 is from the Hall's monitor rectangle (seg15:0x760's
  SetRect) [BYTES seg15:0x1592-0x15ae].
- A call with arg 0 [BYTES seg15:0x1665-0x19fb]:
  ```
  if DGROUP:0x1e50 < deadline (32-bit, DGROUP:0x17f0:0x17ee): return
  deadline = DGROUP:0x1e50 + 40; n = 2
  if DGROUP:0x92a != 0: return                       // the text is finished
  repeat while n characters not yet done:
      type text[idx] (printable: erase cursor, TextOut, cursor block 0x0000FF80; space: wrap test;
      CR or wrap: new line or ScrollDC; a CR also steps over its LF); idx++ ; seg10:0x754a(0x2b,0,0,0)
      // checks on the NEXT character, before it is typed:
      if text[idx] == CR and text[idx-1] == LF: deadline = DGROUP:0x1e50 + 1900; stop   // blank line
      if text[idx] == CR and text[idx+2] == ' ':
          state 8: DGROUP:0x929c++ ; stop        // seg15:0x1899-0x18a4
      if text[idx] == NUL:
          state 8: DGROUP:0x929c++ ; DGROUP:0x92a = 1 ; stop   // seg15:0x18bc-0x18e4
  ```
  The state-8 branches raise the Hall's phase where states 0/6 raise DGROUP:0x92d4 and state 9
  raises DGROUP:0x92be. The checks run after every typed character, so the phase rises with the
  CR still untyped: the next phase's first call types it.
- **Where the phases fall.** The text's two "\r\n \r\n" (DGROUP:0x1188 and DGROUP:0x1198) are
  the only CR-space pairs: phase 3 types paragraphs 1–4 and stops before the CR after the
  charges; phase 4 holds 1500 ms; phase 5 types the verdict, the sentence and the credit line
  and stops before the CR after the credit; phase 6 holds 4000 ms and runs seg15:0xcb0 (launch);
  phase 7 types the farewell to the NUL (phase 8, DGROUP:0x92a = 1). Every "\r\n\r\n" gives a
  1900 ms pause between its two line breaks.
- **Pace (ADR 0009).** Clock-paced: the 500 ms start, the 40 ms deadline per call, the 1900 ms
  pauses (all against DGROUP:0x1e50, sampled once per pass) and the 1500/4000/1550/1500/2000 ms
  holds (GetCurrentTime). Frame-paced: how often the 40 ms deadline is tested (once per state-8
  frame, the ungated threshold DGROUP:0x9004 aiming at 72 frames/s), the jet, the descent and the
  launch rise (frame counts, as in state 6). At 72 frames/s a call comes every 3 frames, 41.7 ms:
  2 characters per 41.7 ms, about 48 per second; a typed character every 0.1 s is not the
  mechanism. A frame passes seg16:0x6928 only when the ungated counter DGROUP:0x9144 reaches the
  threshold DGROUP:0x9004, so the 40 ms rule is a ceiling and the real rate also follows the frame
  cadence and the shifts of DGROUP:0x9004 in seg15:0x302e (timing.md).
- **Keys.** State 8 hands no message to a screen handler [BYTES seg7:0x197]. Ctrl held at a
  state-8 frame (DGROUP:0x92a2 ≠ 1), or QuickIntro > 1, sets the fast flag DGROUP:0x928 that only
  the landing reads (no scene-110 render, the jet returns at once, no fades, the descent starts
  at its end + 1) (shell.md "Ctrl skip"); phase 10 clears it. Nothing skips or speeds the typing,
  the holds or the launch.
- Rough timeline check for k1 at 48 characters per second (pilot name of n characters): typing
  starts 0.5 s after phase 2 (k1: 50.275 + 0.5, 2.8 s into the Hall); phase 3 ≈ 273 + n
  characters and 3 pauses (≈ 11.4 s), phase 4 1.5 s, phase 5 ≈ 226 characters and 2 pauses
  (≈ 8.5 s), phase 6 4.0 + 1.55 s, phase 7 ≈ 28 + n characters: the last line ends about 30 s
  into the Hall against about 13:14:17 (29 s) in [ORACLE ships-0003]. Phase 8 (1.5 s), the launch
  (800 ms + 950 ms + the rise's frames) and phase 10 (2 s) give about 36–37 s at 72 frames/s
  against k1's 34.9 s; the difference sits in the frame-paced parts and the typing rate on the
  oracle's host (ADR 0009) [ORACLE ships-0003].

#### The launch (phases 6 to 10)
- **seg15:0xcb0 outside state 0** [BYTES seg15:0xcb0-0xf2c]: sound 0x6e and the 90 s desk
  deadline (shell.md item 6), DGROUP:0x92d4 = 5 (seg15:0xede), then sound **0x4b** (the launch
  voice, sound.md), a 1550 ms (0x60e) busy wait pumping seg10:0x740c, DGROUP:0x92d8 =
  DGROUP:0x92d6 (the ship's resting y on the disc), DGROUP:0x92d6 = 10 (the rise's end) [BYTES
  seg15:0xee4-0xf20]. State 6 takes the same path.
- **seg15:0x10be at step 5** (phase 9): copies the monitor text area from the screen into the
  scene DC, busy-waits 800 ms, seg15:0x0(2) (sound 0x20) … step 9: seg10:0x740c, seg15:0x1328(1),
  seg15:0x14be, then DGROUP:0x929c + 1 in state 8, with no black screen (shell.md "Launch scene",
  item 9). In state 8 the rising ship's background comes from the scene DC DGROUP:0x929a, which now holds scene 115, the monitor and the typed text. The flame seen in the launch is frame 0 of seg15:0x1eac, the ship with its flame.
- **Phase 10** blacks the screen, waits 2000 ms, sets palette scheme 8 mode 0 and
  seg10:0x6dd4(0), and calls **seg9:0x6cc4(1)** ("The exit and the debriefing").

---

### The text, paragraph by paragraph (seg16:0x6afe) [BYTES seg16:0x6afe-0x71a1]

Locals: S = dword [bp−0xda] (severity, then the fine), total = word [bp−0xd0], served = word
[bp−0xd6], conf = word [bp−0xd2], buf = [bp−0xca] (150-byte LoadString target). All bytes of the
arrays are signed (`cbw`).

```
DGROUP:0x92d4 = 0                                                        // 0x6b0c
DGROUP:0x93a8 = GlobalAlloc(0x42, 2000); far DGROUP:0x4fcc = GlobalLock() // 0x6b12-0x6b2a, the text
total  = Σ_{c=0..9} sbyte DGROUP:0xa14d[c]      (16-bit sum)             // 0x6b2e-0x6b50
served = Σ_{c=0..9} sbyte DGROUP:0xa157[c]      (16-bit sum)             // 0x6b52-0x6b74
P.credit (dword +0xdc) += sext16(P.resources word +0xce); P.resources = 0   // 0x6b76-0x6b87
```

**P1** (overwrite = 1, so it starts the text):
- served = 0: 908 "THIS IS THE HALL OF JUSTICE, %s.%N%N" (%s = DGROUP:0x380c) [BYTES seg16:0x6b94-0x6ba2].
- served ≠ 0: buf = seg17:0x7ba6(total) (decimal + 2095 "ST" when total % 10 = 1 and total ≠
  11, 2096 "ND" for 2 and ≠ 12, 2097 "RD" for 3 and ≠ 13, else 2098 "TH"; signed 16-bit `%`)
  [BYTES seg17:0x7ba6-0x7c28]; 909 "THIS IS YOUR %s ARREST IN %#, %s.%N%N" with (buf,
  DGROUP:0x91f8, 2518, DGROUP:0x380c); %# writes n, a space and 2518 "WAVE" for n = 1, else 12518
  "WAVES" [BYTES seg16:0x6bac-0x6bde]. "ARREST" counts total, the witnessed crimes, not arrests.

**P2** (served = 0 only): 911 "THOSE WHO BREAK THE LAWS OF INNER SPACE ARE BROUGHT HERE TO MEET
THEIR FATE. %N%N" [BYTES seg16:0x6be1-0x6bf7].

**P3**, the arrest [BYTES seg16:0x6bfa-0x6d4b]:
```
c = sbyte DGROUP:0xa161 (the arrest record's crime); e = sbyte DGROUP:0xa162 (the officer)
if c < 0:  LoadString(291 "SINCE THERE IS AN OUTSTANDING WARRANT FOR YOUR ARREST") into buf,
           then jump to the ", " below: buf is overwritten before any lstrcat, the 291 text is lost
else:      lstrcat(text, LoadString(c == 8 ? 1097 "AFTER WITNESSING YOUR DISGRACEFUL," : 912 "AFTER OBSERVING YOU"))
           lstrcat(text, LoadString(c == 9 ? 1093 " Thrusting Recklessly" : 1098 + c))
lstrcat(text, ", ")                                   // DGROUP:0x9f2
lstrcat(text, seg17:0x8e60(e))                        // the officer (below)
lstrcat(text, " (")                                   // DGROUP:0x117e
lstrcat(text, design name DGROUP:0x9742 + 0xad·(ship e word +0x4e))   // e.g. "INTERCEPTOR"
lstrcat(text, ")")                                    // DGROUP:0x1181
lstrcat(text, LoadString(913 " ARRESTED YOU AT "))
lstrcat(text, seg10:0x97ce())                         // the time NOW, not the arrest's
lstrcat(text, ". "); lstrcat(text, "\r\n\r\n")        // DGROUP:0x9ee, DGROUP:0x1183
```
- **The officer** seg17:0x8e60(i, buf) [BYTES seg17:0x8e60-0x8f1c] ship i's roster index +0xd6 ≥ 0 → the roster record's name (+0x65);
  else 788 "Officer" + " " + decimal(lo16(2·lo16(lo16(lo16((2i + 3)(c + 2))(i + 6))(i + 7))) /
  (c + 3) + 956), c = sbyte +0x5e (CWD, 16-bit idiv). Ship 3 of class 1: 9·3·9·10·2/4 + 956 =
  2171, the oracle's "OFFICER 2171" [ORACLE ships-0003].
- **The time** seg10:0x97ce(buf) [BYTES seg10:0x97ce-0x9939]: t = time() (seg5:0x5be), tm =
  localtime(&t) (seg5:0x4c0); h = tm_hour; PM = h > 11 (then h −= 12); h = 0 → 12; buf = "" +
  dec(h) + ":" + ("0" when tm_min < 10) + dec(tm_min) + ("PM" or "AM") + " " + dec(tm_mon + 1) +
  "/" + dec(tm_mday) + "/" + dec(tm_year). **tm_year is printed raw** (years since 1900): the k1
  screen reads "1:13PM 11/15/126" (`k1/f6-70`) [ORACLE ships-0003].
- The k1 paragraph: "AFTER OBSERVING YOU REMOVING EVIDENCE, OFFICER 2171 (INTERCEPTOR) ARRESTED
  YOU AT 1:13PM 11/15/126." matches, letter for letter, once uppercased.

**P4**, the charges [BYTES seg16:0x6d50-0x6e79]:
```
lstrcat(text, LoadString(914 "YOU ARE CHARGED WITH "))
S = 0 (dword); k = 0
for c = 0 .. 9:
    d = sbyte a14d[c] - sbyte a157[c]        (16-bit)
    if d == 0: continue
    if k == 0: A = 0
    else:      A = 1; for j = c+1 .. 9: if a157[j] != a14d[j]: A = 0      // A: this is the last charge
    C = (k != 0 and A == 0)                                              // C: a middle charge
    seg10:0xa332(text, 234 "%< AND %>%<, %>%# OF %S", 0, A, C, d, 2519, 1098 + c)
    k++
    S += sext32(word DGROUP:0x116a[c])
    if d > 1 (signed): S += sext32(lo16((word DGROUP:0x116a[c] / 5) · (d - 1)))   // CWD drops the high word
lstrcat(text, ". "); lstrcat(text, "\r\n \r\n")    // DGROUP:0x1188: the phase-3 stop
```
- The formatter's %< takes A (or C) as the output flag; with the flag set it scans to the next
  %< or %> marker, writes ", " or " AND " only when an earlier group of the run was counted
  (none here: the count restarts at each %>) and copies the literal; a zero flag drops the
  literal up to %> [BYTES seg10:0xa61a-0xa6db]. So 234 yields (A ? " AND " : "") + (C ? ", " :
  "") + "d COUNT" (d = 1) or "d COUNTS" (12519) + " OF " + LoadString(1098 + c) cut at a
  trailing caret pair (shell.md %S). The crime strings start with a space, so the text holds two
  spaces after "OF" (the oracle's wrap hides one); 1098 and 1105 and 1107 end in "." and the
  ". " that follows gives "..".
- Charges come in ascending crime order: "A, B AND C"; one charge has neither. The weights
  DGROUP:0x116a: 700, 400, 550, 550, 500, 300, 300, 450, 500, 0 for crimes 0 … 9 [BYTES DGROUP:0x116a].
- For d > 1 a crime weighs w + (w/5)(d − 1); a negative d (a wrapped byte) prints a negative count
  and adds w only.

**P5**: 915 "AFTER DUE CONSIDERATION, YOU HAVE BEEN FOUND GUILTY AS CHARGED" + ". " + "\r\n\r\n"
[BYTES seg16:0x6e7e-0x6eba].

**P6**, the sentence ("The punishment" computes S, conf): buf = seg17:0x7ba6(total); 723 "CONSIDERING THE
SERIOUSNESS OF YOUR CRIME AND THAT THIS IS YOUR %s OFFENSE, " (buf) [BYTES seg16:0x6f98-0x6fbf];
then [BYTES seg16:0x6fc2-0x704e]:
- S ≠ 0: buf = seg6:0x1010(lo16(S), 100, 2) and 725 "WE ARE ASSESSING YOU A FINE OF %s ICONS".
- S = 0 and 1 ≤ conf ≤ 27: 726 "WE ARE CONFISCATING YOUR %S" with %S = 186 + conf (186 "NO
  WEAPON" … 201 "HANDCUFFS" … 212 "INNER FORCE"; 213 is absent, so conf = 27 prints nothing).
- S = 0 otherwise: 630 "WE ARE LOOKING AT %S IDIOT HERE! (HE HE HE)" with %S = 632 +
  min(sbyte DGROUP:0xa160, 3): 632 "AN ", 633 "A REAL ", 634 "A SUPREME ", 635 "A SUPER SUPREME ".
  DGROUP:0xa160 is DGROUP:0xa157 + 9, the served count of crime 9, read before this Hall's copy.
- then ". " + "\r\n\r\n" [BYTES seg16:0x7051-0x7068].
- **seg6:0x1010(v, div, digits, buf)** [BYTES seg6:0x1010-0x10a9]: "" (or "-" from DGROUP:0x491 and
  v = −v when v < 0) + dec(v / div) + "." (DGROUP:0x493) + dec(v % div) cut to `digits` characters.
  The fraction is **not zero-padded**: 150 → "1.50", 105 → "1.5", 100 → "1.0". seg6:0xf54 is the
  same for a dword (DGROUP:0x48d "-", DGROUP:0x48f ".") [BYTES seg6:0xf54-0x100c]; credit 0 → "0.0",
  the oracle's "0.0 ICONS" [ORACLE ships-0003]. Note the fine reaches seg6:0x1010 as its low word.

**P7**: buf = seg6:0xf54(P.credit, 100, 2); 1158 "YOUR CREDIT IS NOW %s ICONS. " + "\r\n \r\n"
(DGROUP:0x1198: the phase-5 stop) [BYTES seg16:0x706d-0x70ab].

**P8** [BYTES seg16:0x70b0-0x7162]:
```
if S != 0 or 1 <= conf <= 27:  line = 916 "STAY OUT OF TROUBLE"
elif sbyte DGROUP:0xa160 < 4:  line = 636 "FURTHER IDIOCY WILL BE DEALT WITH MORE SEVERELY.."
else:                          line = 637 "DON'T DO IT AGAIN! WE CAN'T THINK OF ANY MORE INSULTS.."
text += line + ", " + DGROUP:0x380c + ". "
if served != 0: text += LoadString(860 + rand() % 4)     // seg5:0xdc at seg16:0x7139, the Hall's only draw
     // 860 "WE'LL BE WATCHING YOU..", 861 "OBEY THE LAW OR YOU'RE GOING TO GET IT!",
     // 862 "FLY CAREFULLY AND BE A GOOD CITIZEN..", 863 "IF YOU IGNORE THE LAW, WE'LL TAKE YOU DOWN!"
```
Then a157[c] = a14d[c] for c = 0 … 9 [BYTES seg16:0x7167-0x7182]; seg11:0x3fd4(text, 1)
uppercases the whole text, the names included [BYTES seg16:0x7184-0x7190]; seg15:0x1584(1)
[BYTES seg16:0x7193].

k1's eight paragraphs follow from: served = 0, total = 1, a161 = 2, a162 = 3, d[2] = 1, S = 550,
credit 150 [ORACLE ships-0003]; k5's from c = 7 (450).

---

### The punishment [BYTES seg16:0x6ebf-0x6f94]

```
// S from P4 (dword); served, conf as above; Cr = P.credit (dword +0xdc, after the resources)
if served == 0 and S > 0 (signed 32):                    // first offence      0x6ebf-0x6ecc
    S = S / 3                (32-bit signed idiv)        //                    0x6ece-0x6ede
    if S < 100: S = 100                                  //                    0x6ee3-0x6ef8
    if Cr > 0: S = min(Cr, S)                            //                    0x6efc-0x6f26
if Cr + 100 < S (signed 32) and S != 0:                  //                    0x6f2a-0x6f48
    if sbyte P +0xcb (DGROUP:0x715b, the slot count) > 1:
        S = 0
        conf = seg16:0x71a2(P, 1)                        // confiscation       0x6f51-0x6f67
    else:
        S = S / 2            (32-bit signed idiv)        //                    0x6f6d-0x6f7d
P.credit -= S                (32-bit)                    //                    0x6f82-0x6f94
```
- **Inputs**: prior arrests are `served` (Σ DGROUP:0xa157, signed bytes); the severity is S from
  the uncharged counts and the weights DGROUP:0x116a; wealth is the credit plus the resources the
  Hall first moves into it; weapons are the slot count +0xcb. No `rand()` here.
- **A first offence** pays max(S/3, 100), capped at the credit when the credit is positive; it
  never triggers the confiscation when the credit is positive. A broke first offender (credit ≤ 0)
  with max(S/3, 100) > credit + 100 loses a weapon (more than one slot) or pays half.
- **A repeat offender** pays S in full when credit + 100 ≥ S, so the credit can go to −100; above
  that, a weapon or S/2, and the credit can go further negative.
- k1: S = 550, /3 = 183, credit 150 → fine 150, credit 0, "1.50" / "0.0" (credit 150 → 0 at
  13:13:50.275, phase 2) [ORACLE ships-0003]. k5: S = 450 → 150 → 150.
- **seg16:0x71a2(s, k)** (cdecl, k = 1) [BYTES seg16:0x71a2-0x720e]: w = sext(byte +0xb8 + k);
  bytes +0xb8 + k and +0xae + k = 0; for j = k … 8: +0xb8 + j = +0xb8 + j + 1, +0xae + j =
  +0xae + j + 1; bytes +0xc1 (+0xb8 + 9) and +0xb7 (+0xae + 9) = 0; byte +0xcb −= 1; return w.
  So the confiscation takes slot 1, the first weapon system after the gun's slot 0, with its
  count, and closes the gap. The selected-slot word +0x18 is not touched [OPEN: a selection
  past the new count in the next wave].
- **conf is unwritten unless the confiscation ran**: [bp−0xd2] has one writer, seg16:0x6f67, and
  is read at seg16:0x6fcc, 0x6fd3, 0x7014, 0x701b, 0x7022, 0x70b8, 0x70bf whenever S = 0. With
  S = 0 and no confiscation (only crime-9 charges, weight 0, or no charge) the original reads a
  stale stack word: a read of an unwritten stack word (ADR 0016 decision 1). The path that the stale word cannot change for S = 0 without a confiscation is the insult (conf ≤ 0).
- **Writes**: credit (and resources = 0), the weapon slots and +0xcb, a157 = a14d (all ten), the
  text. Nothing writes the team table DGROUP:0x3824, the crime words DGROUP:0x964a/0x9648, a14d,
  a161/a162/a163, the score or INSPACE.INI (the oracle: INI byte-identical, score unchanged)
  [ORACLE ships-0003].
- **Where the counts live**: a14d (witnessed) and a157 (served) are the save header's +0x174 and
  +0x17e (formats.md); k1's lastgame.sav has 0x176 = 1 (a14d[2]) and 0x180 = 1 (a157[2]) and
  0x162 = 0 (credit/10) [ORACLE ships-0003]. The arrest record a161/a162 is read back from header
  +0x188/+0x189, which the writer leaves 0, and seg9:0xc06 sets a161 = 0xff on every path anyway
  [BYTES seg9:0xd33]. The INI holds none of them.
- **The rest of the arrest record has no reader**: dword DGROUP:0xa147 (= the wave frame counter
  DGROUP:0x1e68) and word DGROUP:0xa14b (= DGROUP:0x91f8) are written at seg13:0xaaf4/0xaafb and
  seg4:0x777c/0x7783 and read nowhere [BYTES seg13:0xaaf0-0xaafb seg4:0x777c-0x7783]; the Hall
  prints the current time instead.
- **a161/a162 are written for any prisoner**: seg13:0xaad2-0xab0a has no player test, so a
  computer ship's arrest also overwrites a161 = a163 and a162 = e [BYTES seg13:0xaad2-0xab0a].

---

### The exit and the debriefing

- Phase 10 → seg9:0x6cc4(1): state 2, the wave's teardown, seg9:0x2828 (the roster writes and
  the LASTGAME save at its end, formats.md) and the box drawn at once (core.md "seg9:0x6cc4"). k1: lastgame.sav rewritten at 13:14:22.91, the state 8 → 2 at
  22.919 [ORACLE ships-0003]. The Hall's credit, slots and a157 reach the roster and the save
  through seg9:0x2828 (formats.md "Saved games", the per-ship body: credit / 10, so the last
  digit of the credit is lost at each wave end).
- DGROUP:0x93a4 stays 1 through state 2 on the player path (only the demo's phase 10 clears it,
  seg16:0x6acb); the next wave start clears it (seg9:0x6111), and DGROUP:0x9648 (seg9:0x64c8) and
  DGROUP:0x964c (seg9:0x6527). DGROUP:0x964a keeps its value (session, saved at +0x30). With a157 =
  a14d the warrant check seg13:0xb93a finds nothing pending. DGROUP:0x9346 = −1 from seg9:0x6c74
  (the "second Terminator" roster test of ships.md reads it).
- **The relations paragraph** (seg9:0x8455-0x87d9), after the ship lines and two seg17:0x42ea(1):
  ```
  for k in 0,1: best_d[k] = 0; best_t[k] = -1                               // 0x8455-0x8470
  present[t] = 1 when some ship 0..DGROUP:0x9316 has design team t          // 0x8472-0x84bb
  tp = P's design team; count = 0                                           // 0x84bd-0x84d5
  for k in 0, 1:                                                            // 0x8619-0x8627
    for t in 0..7:
      if t == tp or !present[t] or (k != 0 and t == best_t[0]): continue    // 0x85ca-0x85e6
      cx = T(t, tp) = word DGROUP:0x3824 + 2(8t + tp); d = cx - word DGROUP:0x9350[t]   // 0x85e8-0x8601
      (a level 0..4 from cx against ±80, ±250 is written to [bp-0x32+2k]; nothing reads it)
      v = #{ j < 3 : word DGROUP:0x4eec[3t+j] == DGROUP:0x931c and word DGROUP:0x4fd0[3t+j] == DGROUP:0x91f8 }
      if |d - 50v| > |best_d[k]| or v != 0:                                 // 0x857d-0x85be
          vsel[k] = v; best_d[k] = d; best_t[k] = t; count++
  if count == 0: page += 789 "RELATIONS WITH OTHER TEAMS HAVE NOT CHANGED."  // 0x87d2
  else:
    page += 2514 " THE " + name(best_t[0])                                  // seg13:0x218a, INI [Teams]
    both = count > 1 and ((d0 < 0 and d1 < 0) or (d0 > 0 and d1 > 0) or (vsel0 != 0 and vsel0 == vsel1))
    if both: page += " " + 795 "AND" + 2514 " THE " + name(best_t[1])
    page += vsel0 != 0 ? 917 " WILL SEEK" + (vsel0 == 2 ? 918 " MAJOR" : vsel0 > 2 ? 919 " SUPREME" : "")
                         + 884 " VENGEANCE LATER"
                       : 798 " HAVE BECOME " + (d0 < 0 ? 878 "LESS " : 799 "MORE ") + 248 "FRIENDLY"
    if !both and count > 1: page += " " + "AND" + " THE " + name(best_t[1]) + the same tail for (d1, vsel1)
    page += ". "
  ```
  [BYTES seg9:0x8455-0x87d0] (DGROUP:0xa56 and DGROUP:0xa58 are " "). count > 1 always means
  slot 1 holds a team: any team taken in pass 0 other than the final slot 0 passes pass 1's test
  against a best of 0. `d0`, `d1` are best_d.
- **The snapshot it compares with** is taken by the wave start [BYTES seg9:0x654c-0x6583]:
  DGROUP:0x9350[t] = T(t, tp) for t = 0 … 7, tp = P's design team. The same block stores
  DGROUP:0x9374[k] = sbyte P +0x5d + k (k < 2) and DGROUP:0x9360[k] = seg13:0x2688(k, P) (k < 8)
  [BYTES seg9:0x652a-0x654a seg9:0x6585-0x65a9], which have no reader (only seg9:0x85fd reads the
  0x9350 block; no instruction names 0x9360 or 0x9374 again), then DGROUP:0x93b6 = −1
  [BYTES seg9:0x65ab]. Only DGROUP:0x9350 is read.
- "THE ENFORCERS HAVE BECOME LESS FRIENDLY." (k1) is this paragraph with best_t[0] = 7, d0 < 0,
  vsel0 = 0, count = 1 [ORACLE ships-0003]; ships-0001 t1's "THE RENEGADES HAVE BECOME LESS
  FRIENDLY." is the same rule.
- **The player at the gate in ships-0001 `t1`**: the player was
  not killed. Its ship byte +0xec (`alive`, DGROUP:0x717c) reads 01 in every watch line through
  08:44:01.281 (`watch-sr0_3.txt`, offset 0x2c), its lamps read 51, 46, 51, 29, the object went to
  state 1 at 08:44:00.961 and state 2 followed at 08:44:01.276; the gate's DGROUP:0x1e80 = entry +
  1000 fits an entry about 0.7 s before state 1, as in ships-0003 k1 (flag 46.982, state 1
  47.663, state 8 47.982) [ORACLE ships-0001 bulk t1] [ORACLE ships-0003]. The hits pushed the
  player onto the gate line and seg12:0x2288 took it through; the "particles" are the gate's
  vanish. A killed player (`alive` 0) goes to DGROUP:0x1e84 and GAME OVER (seg7:0x6f0).

---

### A computer ship's arrest as the player sees it
- The arrest executor's 0x16 branch ("The Enforcer's executor" above) treats any target t; the record
  writes (a147, a14b, a161, a162) are not player-only ("The punishment"). A computer prisoner of class
  > 0 complies (+0x52 = 10) or, for teams 3, 6, 1, fights (executor step 4B3).
- The prisoner leaves with its captor: seg12:0x2288's team-7 branch calls seg12:0x2288(14,
  prisoner) [BYTES seg12:0x243c-0x2444]; only a player prisoner sets the flag and DGROUP:0x1e80
  [BYTES seg12:0x244a-0x2472]. The computer ship's `alive` stays 1, so the debriefing's dead list
  does not name it; a14d/a157 are untouched (the counters are the player's).
- **The activity text** seg3:0x1d0a(a, ship, mode), goal mode 3 with a pool-14 goal: +0x52 = 0x14
  gives the second line 498 ".It's Part Of My Patrol." [BYTES seg3:0x2ba8-0x2bb9]; +0x52 = 0x16
  gives 507 "Arresting You For " when ship +0x56 = a, else 508 "Arresting %D For " formatted with
  (14, +0x56, 5), and the second line LoadString(1098 + sbyte DGROUP:0xa163), the global last
  crime [BYTES seg3:0x2bc1-0x2be7 seg3:0x2d20-0x2d47]. The same pair sits in the capture-mode
  case after seg13:0x371c [BYTES seg3:0x2d09-0x2d1c]. The right display passes a = the player
  (seg13:0x51b3-0x51c5); seg3:0xf4 passes its own argument (seg3:0x1cd3-0x1cdc).
- Not read: whether the player sees seg3:0x6ec messages addressed to another ship (552 to the
  prisoner), the sirens' sound ids, and the rest of seg3:0x1d0a (Later stages).

### Further facts of the Hall
Facts of the routines above that the sections do not state.
- **seg9:0x6bc2's head** runs on both paths: seg10:0x7480(−1, 1), dword DGROUP:0x1e74 = 0, and in waves with DGROUP:0x9320 = 6 a loop over ships k ≤ DGROUP:0x9316 with +0x32 = 0 and k ≠ player: for every pool-0 object j < DGROUP:0x9406 with `state` (+0x18) = 0, ship k +0xce += 100 + trunc(−90·(+0x1e)/210), +0xd2 += 1 and seg7:0x5ae8(j, 1) [BYTES seg9:0x6bd0-0x6c59]. The state-8 path stores DGROUP:0x9346 = 0xffff and the loop clears `alive` only for a team-7 ship of class 0 with +0x56 = P, so an escorting class-1 Enforcer keeps `alive`.
- **seg15:0x302e in state 8** has, before the scene: seg13:0x65f2(3); seg10:0x7480(−2, 1) (any state but 9); seg10:0x744c(0x6e); PatBlt black when DGROUP:0x93a6 = 0; seg10:0x6da6(0, 1); when DGROUP:0x93a6 = 0 and (Calibrate DGROUP:0x8ffc < 850, or < 1600 with depth DGROUP:0x90ee < 8) the loading text LoadString(2137) "Incoming Jet " when W > 700 (signed) and LoadString(2138) "Approaching Landing Pad..", drawn through seg10:0x94c4(0, 6, 0x0000ff00, half(DGROUP:0x90ea), half(DGROUP:0x90ec), far text); then DGROUP:0x92d6 = (22·DGROUP:0x10)/100 (seg5:0x110, a 32-bit divide) and DGROUP:0x92d8 = DGROUP:0x10 − 10. With the fast flag DGROUP:0x928 ≠ 0 the scene branch seg15:0x3192-0x3199 jumps to seg15:0x3547: no scene 110, no SetRect, no DGROUP:0x92de store and no scene blit, and DGROUP:0x92d6/0x92d8 keep the values set just before. The render path also needs the 8-byte out block (GlobalAlloc(0x42, 8) into DGROUP:0x92a6, locked into far DGROUP:0x3cc0). The tail on every path: seg15:0x1a42 (the jet frames; it calls the visit's ship builder seg15:0x20ca except in state 9 with DGROUP:0x92be other than 0 and 2, so always in state 8), DGROUP:0x92a2 = 0, seg15:0x3caa(1), and with depth ≥ 8 and DGROUP:0x93c ≠ 0: DGROUP:0x9004 <<= 2 when Calibrate > 2000, <<= 1 when > 1500; then DGROUP:0x93c = 0 [BYTES seg15:0x3045-0x3166 seg15:0x3192-0x31d6 seg15:0x3547-0x3581].
- **seg15:0x760's re-render branch** is taken when the scene DC exists and either DGROUP:0x92d6 = −1 or (DGROUP:0x929c < 2 and DGROUP:0x92a2 < 1); it also runs with no scene DC when DGROUP:0x929c < 6. It restores DGROUP:0x9296 and DGROUP:0x92a0 into the DC, deletes the DC and DGROUP:0x9294 and zeroes both; it allocates the out block when DGROUP:0x92a6 = 0. No ship is drawn on this branch [BYTES seg15:0x775-0x793 seg15:0x863-0x8a0 seg15:0x8a2-0x9d8].
- **The descent's s4** is 4·(sext roster byte +0x7a + half(sext roster byte +0x7b − 6)) of the player's roster record (far DGROUP:0x6e24 + 0x7c·(+0xd6)); the block that seg15:0x374c(1) runs sets the clip DGROUP:0x3cb0 … 0x3cb6 = (0, 0, W, DGROUP:0x10) between the seg15:0x760 call and the y stores [BYTES seg15:0x3759-0x3785 seg15:0x387d-0x38d3]. The descent restores the background under the ship from the scene DC DGROUP:0x929a (state 6 uses DGROUP:0x92b6), and its shadow block (depth ≤ 8, brush DGROUP:0x5ea8 below 8 bpp or DGROUP:0x5eaa) runs at every height in state 8, against only within 120 pixels of the end in other states [BYTES seg15:0x3a4d-0x3a76 seg15:0x3aa4-0x3b12].
- **The typer's three tests and the init.** After each typed character, the pause test (next character CR, previous LF: deadline = DGROUP:0x1e50 + 1900), the CR-space test and the NUL test run in that order (seg15:0x184e, 0x1882, 0x18bc). A CR preceded by LF is consumed by typing on the next call without the CR-space test firing; in the Hall text the CR-space pairs are the first CR of DGROUP:0x1188 and DGROUP:0x1198 (". " + CR LF space CR LF). Every later call returns at seg15:0x168c while DGROUP:0x92a ≠ 0 (after re-arming the deadline). The arg-0 path is also the one taken for arg ≠ 0 when DGROUP:0x92d4 ≥ 2; with DGROUP:0x92d4 = 0 the call is the init (deadline DGROUP:0x1e50 + 500), with 1 the Space skip; seg16:0x6afe sets DGROUP:0x92d4 = 0 before its seg15:0x1584(1) [BYTES seg15:0x15b1-0x1612 seg15:0x1615-0x1662 seg15:0x1665-0x1691]. The geometry comes from scene 115: h = (DGROUP:0x3cac − DGROUP:0x3ca8 − 63)/15, the start DGROUP:0x3d48/0x3d4a + 29, the CRT flash at (W/2, DGROUP:0x3ca6 + DGROUP:0x92d2/2) through seg15:0x84b0, and the wrap and scroll limits DGROUP:0x6088 and DGROUP:0x92d2, which seg15:0x8604 (called by seg15:0x760) presumably sets [OPEN].
- **The repaint drives the phases too.** seg11:0xafe's state dispatch calls seg16:0x6928(1) in state 8 (seg11:0x1684-0x1686, case 8 of seg11:0x16be), and seg2:0x970 forces DGROUP:0x8f64 = 1 in state 8; the call also runs a phase step, so a WM_PAINT can type characters or run the in-frame waits of phases 4, 8 and 10 [BYTES seg2:0x970 seg11:0x1684-0x1686].
- **seg15:0xcb0's head runs in state 8:** seg10:0x744c(0x6e) and dword DGROUP:0x6e28 = DGROUP:0x1e50 + 90000 come before the state test, so phase 6 plays 0x6e and then 0x4b [BYTES seg15:0xcc5-0xcdd].
- **Phase 9 is the launch step machine.** seg15:0x10be switches on DGROUP:0x92d4 (10 cases, table seg15:0x130c), entered at step 5, which seg15:0xcb0 set in phase 6: step 5 outside state 0 calls seg15:0x9e6 and seg15:0x0(2) and sets the dword DGROUP:0x3cc4 = now + 950; step 6 waits for it (unsigned, strictly later) and calls seg15:0x0(1); step 7 calls seg15:0x0(0) until it returns non-zero, then falls into step 8 (one seg10:0x740c pump) and step 9; step 9 pumps, calls seg15:0x1328(1) and seg15:0x14be and in state 8 increments DGROUP:0x929c (seg15:0x12f2-0x12f9), the only way to phase 10. Phase 10 does not touch DGROUP:0x929c; it leaves state 8 by DGROUP:0x90f4 = 1 (demo) or seg9:0x6cc4(1) (player game), and on the second path DGROUP:0x93a4 stays 1 (its other clear is seg9:0x6111) [BYTES seg15:0x1101 seg15:0x1193-0x1258 seg15:0x125b-0x1268 seg15:0x12f2-0x1307].
- **Text lifetime.** The 2000-byte handle DGROUP:0x93a8 is freed by seg9:0x699e (seg9:0x69da-0x6a04), which seg15:0x1328 calls, so the first release in state 8 is phase 9's step 9 (seg15:0x1328(1) at seg15:0x1263), before phase 10's seg15:0x1328(0); seg16:0x6afe never unlocks or frees it.
- **Space in state 8.** The state-8 tick passes no message to seg11:0x7902, and that handler's Space skip (seg11:0x7b03-0x7b13) needs DGROUP:0x92d4 = 1, which state 8 never has. Whether any other key path acts in state 8 (ESC through the window procedure) is [OPEN].
- **The text's details.** Every phase-2 text is upper-cased by seg11:0x3fd4(text, 1), so the officer name, the design name, the crime strings (mixed case in ENG_LANG.DLL) and the pilot name type in capitals. The charges' tail is ". " + DGROUP:0x1188, then string 915 ("AFTER DUE CONSIDERATION, YOU HAVE BEEN FOUND GUILTY AS CHARGED") + ". " + DGROUP:0x118e (CR LF CR LF), before the sentence. The charge flags: A = 1 for a charge after the first with no later crime j > c having a157[j] ≠ a14d[j]; C = 1 for a charge after the first with A = 0; the formatter's %< scan writes a separator only when its run counter is at least 1, and in string 234 each %< is followed directly by its own %>, which resets the counter, so the scan adds nothing and flag 0 suppresses only the group's literal; %# (d, 2519 "COUNT" / 12519 "COUNTS") and %S (1098 + c, caret cut) follow the second %> and always print [BYTES seg10:0xa630-0xa6db seg16:0x6d88-0x6dbd seg16:0x6e43-0x6e5f]. d is the signed 16-bit difference, so a negative d prints and is charged as well. The arrest paragraph uses 1093 " Thrusting Recklessly" for crime 9 and the charge loop 1107 " Being An Idiot." for the same crime; several crime strings start with a space and some end in ".", so the bytes produce "OF  MURDERING AN ENFORCER.. " (double space, double period).
- **seg6:0x1010 and seg6:0xf54** write the remainder through seg6:0x10ce, an unsigned decimal with no padding; the remainder string is then cut at the `digits` argument (2), which never shortens a remainder below 100. So 1205 prints "12.5" and 100 prints "1.0". A fine of 32768 or more prints from its low word as a negative value [BYTES seg6:0x10ce-0x11e2].
- **The ordinals** seg17:0x7ba6 take signed remainders (`cwd; idiv`), so a negative n gets "TH" and 111 gets "ST".
- **The fine and the first-offence rules** hold for S ≤ 0 too: a first offence with S ≤ 0 skips the three steps (fine = S) [BYTES seg16:0x6ebf-0x6ec4]. The confiscation test runs for first and repeat offenders alike, on the credit after the +0xce merge [BYTES seg16:0x6f2a-0x6f7d]. The credit can go negative without a floor: a credit of 0 with a first-offence fine of 100 gives −100 [BYTES seg16:0x6f82-0x6f94].
- **The relations paragraph** takes a team when |d − 50v| > |best d of the slot| or v ≠ 0 (seg9:0x88ba = abs, 16-bit); a take stores v, d itself (not d − 50v) and t, and increments the take counter [bp−0x1e]; later candidates in the same pass can replace an earlier take, including a v ≠ 0 take by a v = 0 team with a larger |d|. Presence reads only the design team word of ship records 0 … DGROUP:0x9316, with no `alive` or state test. The two slots share one verb when the take counter exceeds 1 (it holds exactly when slot 1 holds a team: if pass 0 takes twice, its first-taken team is still a pass-1 candidate and takes again from best 0) and (both d < 0, both d > 0, or v0 ≠ 0 with v1 = v0). Slot 0's verb uses [bp−0xe4]/[bp−0x1a], slot 1's [bp−0xe2]/[bp−0x18]; d = 0 reads MORE (799). With no take the page gets 789 with no ". " after it. The class [bp−0x32 + 2s] (4 below −250, 3 below −80, 2 below 80, 1 below 250, else 0, on T(t, tp)) is written for every candidate and never read [BYTES seg9:0x8455-0x8627 seg9:0x865a-0x8689 seg9:0x86d6-0x8727 seg9:0x8778-0x87cb].
- **The wave start's snapshot block** also stores DGROUP:0x9374 + 2j = sext byte player +0x5d + j (j = 0, 1) and DGROUP:0x9360 + 2t = seg13:0x2688(t, player) just around the DGROUP:0x9350 stores; no instruction reads them [BYTES seg9:0x652f-0x65a9].
- **The ship in the Hall.** seg15:0x20ca has two calls to seg15:0x28b2, both gated only by DGROUP:0x9e5b = 0: seg15:0x23b3 on the thrust-over path and seg15:0x27c3 on the thrust-behind path. The wave placement stores DGROUP:0x9e5b = 0 (seg9:0x6865-0x6867 in seg9:0x6624); its other writers are seg9:0x5d38, seg13:0x3772, seg16:0x441, seg16:0x3b07 (dword 1) and seg18:0x10e. What seg15:0x28b2 draws is [OPEN].
- **The arrest record's writers.** seg9:0xc06 stores the byte DGROUP:0xa161 = 0xff (a byte, `c6 06 61 a1 ff`, so DGROUP:0xa162 is untouched) on its new-game path and on its restore path [BYTES seg9:0xc44-0xc49 seg9:0xcbe seg9:0xd33]. No instruction in any INSPACE.EXE segment reads DGROUP:0xa147 … 0xa14c: the stores at seg4:0x777c/0x7783 and seg13:0xaaf4/0xaafb are the only references [BYTES seg4:0x777c seg4:0x7783 seg13:0xaaf4 seg13:0xaafb].
- **Which routine writes which arrest record.** The writes at seg13:0xaad2-0xab0a sit on the branch where the Enforcer e's byte +0xed = 1 (after +0x06 = 14, +0x08 = target, +0x3a = 2, +0x36 = 3 and seg8:0x285a < 250); e = [bp+6]. The same branch sends message 0x3d to the target first and sets e's +0xed = 0. The other positive +0xed sends 0x57 and sets +0xed = 0xff with none of these writes [BYTES seg13:0xaa79-0xab23].
- **The activity text** (seg3:0x1d0a) reaches the caption code from seg3:0x2bbc and from seg3:0x2cf5. The second needs +0x38 = 2 and seg13:0x371c(pool [bp−4], rec [bp−6]) ≠ 0. When +0x56 is the first argument the routine loads string 507 "Arresting You For " (seg3:0x2bd3) instead of 508 and also sets the charge id 1098 + sext(DGROUP:0xa163) (seg3:0x2be7 → seg3:0x2d40); the id is kept in [bp−0xc] (seg3:0x2cc7), and the tail loads it into the second buffer [bp−0x110] (seg3:0x2fde-0x2ff3). The tail skips that load for the player in a player game with DGROUP:0x9460 = 0 (seg3:0x2a71-0x2a96). The viewer is the first argument: seg17:0x367f passes (DGROUP:0x9646, DGROUP:0x9464, 2), so the watch window reads "Arresting You For" for the player's arrest. The bytes test ship i for order +0x52 = 0x16 only, with no team test, so "an Enforcer" rests on who gets order 0x16 [BYTES seg3:0x2ba8-0x2be7 seg3:0x2d09-0x2d47 seg17:0x3675-0x367f].
- **The gate passage of a prisoner** calls seg12:0x2288 for the prisoner and reads the escort's object +0x38 again after that call (seg12:0x2458) to test the player; on a match dword DGROUP:0x1e80 = DGROUP:0x1e50 + 1000 and DGROUP:0x93a4 = 1 [BYTES seg12:0x23fd-0x2472]. In the escorted arrest the recursive call for the player reaches the second DGROUP:0x93a4 writer (seg12:0x2478-0x24b1) first, then the escort's own block stores both.

## The oracle and the law

[ORACLE ships-0003] and [ORACLE ships-0004] record arrests with the Hall's text and the numbers it printed; every number the runs show follows from the Hall's text routine above.
- **The fine.** Severity S is the sum of the crime weights (DGROUP:0x116a) of the counts not yet served, with (w/5)(d − 1) added for d counts of one crime. A first offence (served sum 0) first lowers S to max(S/3, 100), capped at a positive credit. Then, for first and repeat offences alike: S is paid in full when credit + 100 ≥ S; otherwise, with more than one weapon slot, slot 1 is confiscated and nothing is fined, and with one slot S/2 is fined. The runs of ships-0004 agree: crime 1 (weight 400) cost 2.00 at its 4th, 7th and 11th offence (400/2 after the confiscation of the missiles); crime 2 (550) cost 1.50 as a first offence (credit 150, S/3 = 183) and 2.75 later; crime 3 (550) 2.75; crime 4 (500) 2.50; crime 5 (300) 1.00 as a first offence and 1.50 later; crime 8 (500) 2.50; the arrest with crimes 4, 4, 9 (500 + 100 + 0) 3.00 and with 3, 8, 8, 4, 8 (550 + 500 + 700) 8.75; the warrant arrest with crimes 0 and 5 (700 + 300) as a first offence (served sum 0) 1.50 (S/3 = 333 capped at the credit 150). Crime 9 has weight 0, so it fines nothing and takes the insult text [ORACLE ships-0004].
- **The credit's last digit.** The runs show the credit moving by +0.05 between the debriefing and the next wave start in two arrests (−525 to −520, −995 to −990) and not in the others (−720, −1140, −250). The roster record keeps the credit as a word of credit / 10 and the placement sets `credit` = 10 × that word ([formats.md](formats.md)), so the last digit is lost at each wave end, toward zero [ORACLE ships-0004].
- **The confiscation** takes slot 1, the first weapon system after the gun's slot 0: HEATSEEKER MISSILES, weapon byte 7 with count 18, in the runs; the gun stays [ORACLE ships-0004].
- **The offence number** is the sum of the witnessed counts (DGROUP:0xa14d), not the number of arrests: 1 … 10 over ten one-count arrests, 1, 2, 5, 10, 11 over five arrests with several counts [ORACLE ships-0004].
- **The officer number** 2·(2i + 3)(c + 2)(i + 6)(i + 7)/(c + 3) + 956 (16-bit products) gives 2171, 2771, 3530, 4466 for the class-1 ships i = 3, 4, 5, 6, the numbers the Hall printed [ORACLE ships-0004].
- **The cuffs** held for 42 s under thrust in one run and then released with no arrest, the Interceptor's goal going to −1 [ORACLE ships-0004]; what ends a hold after that long is [OPEN].
- **A flight through the gate before the cuffs** gives no Hall and no fine, leaves the first crime array with the count and the second with none, and the next wave starts with an Interceptor already in order 0x16; its arrest charges the unserved crime together with the next one [ORACLE ships-0004].
- **Sounds.** SIRENS (slot 0x4a, id 0x70) at the call-in; SLING then CLUNK (0x06) 2 ms after the goal pair becomes (14, i); HYPER (0x27) three times 0.7 s before the player's gate state; MISSILE, IGNITE, SMASH and STATIC at the Hall's opening; POP (0x2b) per typed character; ENGAGE (0x4b) when the text is done [ORACLE ships-0004].
- **Not drawn or not recorded.** Crime 6 was not drawn. Crime 7 was drawn only in a Grand Prix wave (kind 6), where F6 made no ship and no arrest followed in 150 s, as seg13:0xae68 admits kind 0x15 there only while byte DGROUP:0xa163 = 8. A Terminator after a second flight through the gate and a second confiscation are [OPEN].
