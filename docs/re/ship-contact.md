# Computer ships in contact: damage, death, capture and defenders

What a computer ship does when it meets things, is damaged and dies, and what the player sees of it. The collision code of [core.md](core.md) is generic: the pair finder, the pair test, the response, the dispatcher, seg8:0x45e0, the wings, the death, the pod, the touch routine and the gun's hit block run for any ship index. This file lists what differs for a ship that is not the player. The roster is in [ships.md](ships.md), the pilot in [pilots.md](pilots.md). [DESIGN]

Addresses without a module name are INSPACE.EXE (DGROUP is seg19). Ship fields are `+0xNN` of the 0x15a-byte record at DGROUP:0x7090 + 0x15a·i, design words `DGROUP:0x96xx + 0xad·n` of `design_index` n (ship word +0x4e, DGROUP:0x70de), object fields as [structs.md](structs.md) names them. "ships" is the pool word DGROUP:0x9324 (14 in a wave). Calls are written in declaration order ([pitfalls.md](pitfalls.md)).

## Who finds a computer ship
The pair finder hands a pair only to the lower pool's mover ([core.md](core.md)), so every pair below has one site.

| Pair | Mover / site |
|---|---|
| ship–ship | seg13:0xb5a–0xcf9 |
| ship–rock | seg7:0x3efb–0x4068 |
| ship–post | seg12:0x1d79–0x1dc3 |
| ship–icon | seg7:0x309c → seg7:0x4a38 |
| ship–tank (pool 4) | seg12:0xae4 site seg12:0xb6d → seg13:0x16ac |
| ship–pod (pool 11) | seg12:0x6b1e site seg12:0x6c91 → seg13:0x16ac or seg8:0x546c/0x5df2 |
| ship–missile (pool 6) | seg12:0x0 site seg12:0x332 |
| ship–pool 5 (balls) | seg7:0x40c4 site seg7:0x43c9 |
| ship–pool 10 (tether) | seg12:0x3148 site seg12:0x3307 |
| ship–ambulance (pool 7) | seg16:0xa2 site seg16:0x7e1 |
| ship–defender (pool 2) | seg8:0x8256 sites seg8:0x864b, seg8:0x8e2d |
| ship–gun shot | seg8:0x3688 → seg8:0x6f3c |
| ship–blast | seg8:0x5ab0 |
| ship–ring, lightning, wildcard | seg11:0x8a42, seg11:0x9fdc/0xa234, seg11:0x9078 |
| ship–gate (passage) | seg12:0x1f7a, seg12:0x2288 |
The ship–gate passage needs now > +0x88 + 3000 of a computer ship [BYTES seg12:0x1f7a seg12:0x2288].

## Ship against ship: whose dispatch sees whom
- The ship mover stores DGROUP:0x943a = DGROUP:0x9324 and DGROUP:0x943c = i before its search (seg13:0xb75, seg13:0xb7d), and nothing between that store and the caller's own dispatch at seg13:0xc8d rewrites them: their writers in seg8 are the shot paths (seg8:0x3971, seg8:0x39d5, seg8:0x725e, seg8:0x7269), the blast (seg8:0x5b7f, seg8:0x5b85) and the defender mover (seg8:0x864b, seg8:0x8e2d); none lies in seg8:0x50aa, seg8:0x6afa, seg8:0x546c, seg8:0x5df2 or seg8:0x45e0. [BYTES seg13:0xb75 seg13:0xb7d seg8:0x3971 seg8:0x39d5 seg8:0x5b7f seg8:0x5b85 seg8:0x725e seg8:0x7269 seg8:0x864b seg8:0x8e2d]
- So when ship i's search finds ship k, k is dispatched first, inside seg8:0x546c (result 1), and its seg8:0x45e0 sees attacker (14, i): k can die. Then i's own dispatch seg8:0x5df2(14, i, 14, k, amount) reaches seg8:0x45e0 with the attacker word still (14, i), which is i itself: step 13's death test skips a self-attacker (seg8:0x4c9a-0x4cbf), so a ram never kills the ship whose search found the pair, which keeps flying at engine 0; step 2 never explodes it early either (seg8:0x461b). Step 4 takes the attacker's velocity and position from i's own object (seg8:0x4830-0x488c) and scales d by 3/4. [BYTES seg8:0x5867 seg8:0x5a9d seg8:0x4c9a seg8:0x4cb5 seg8:0x461b seg8:0x4830 seg8:0x4865]
- The searching ship is the lower index among the on-screen ships that have not collided this frame (the loop runs i = 0 … DGROUP:0x9316, seg13:0x9f), so with the player at index 0 a ram between the player and a computer ship cannot kill the player and can kill the computer ship. [BYTES seg13:0x9f seg13:0xd37] [DESIGN]
- Nothing new to port.
- The pair holds (DGROUP:0x9324, i) from seg13:0xb75 to seg13:0xc8d: the listed functions' own instructions never write DGROUP:0x943a/0x943c, but seg8:0x50aa's callee seg18:0x2c1e (called at seg8:0x53f2 with the searcher's own pool and record bytes [bp−8], [bp−0xa]) stores them at seg18:0x2d19/0x2d22 with the same pair and dispatches seg8:0x5df2 through seg8:0x6a44 (seg18:0x2ef6). The pair is (pool, sign-extended low byte of i) packed for seg8:0x50aa (seg13:0xb78 `cbw`; it matters only for i ≥ 128). The other writers (other movers, seg8:0x6f3c, seg8:0x8256, seg11:0xa234, seg16:0xa2, seg18:0x37d8, seg18:0x403e) are reachable from the dispatch only through seg16:0x8e4 → seg11:0xaece, which runs for the player's death alone (seg16:0xab4-0xb2b) [BYTES seg8:0x50b9-0x50c6 seg8:0x53ec-0x53f2 seg18:0x2d16-0x2d26 seg16:0xab4 seg16:0xb2b].

## The gun on a computer ship (seg8:0x6f3c after the sound)
The shooter-ship steps of seg8:0x6f3c, in order (o = owner record [bp−0x2a], op = owner pool [bp−0x28], (pool, rec) the target, [bp−0x20] the flag, [bp−0x1c] the result):
1. op = DGROUP:0x9324: o's dword +0xa4 = now when o's +0x38 = 1 and goal (+0x32, +0x34) = (pool, rec); then each ship i ≤ DGROUP:0x9316, i ≠ o, with `alive` (+0xec) ≠ 0, goal = (pool, rec) and +0x38 = 1 gets seg13:0x2714(i, o, 5, 10, 0, −1). [BYTES seg8:0x754e-0x75d7]
2. op and pool both DGROUP:0x9324: seg4:0x7d36(o, rec, DGROUP:0x9438). [BYTES seg8:0x75d9-0x75f0]
3. The player's shot: DGROUP:0x943e/0x9440/0x52d8 and the DGROUP:0x9432 additions (core.md). [BYTES seg8:0x75f8-0x768b]
4. result = on_damage(pool, rec, DGROUP:0x9438). [BYTES seg8:0x768f-0x76ae]
5. The spark. [BYTES seg8:0x76b1-0x76d3]
6. seg8:0x99ce(pool, rec, op, o, result + 2) (`inc ax; inc ax` on the result). [BYTES seg8:0x76d6-0x76e8]
7. Pool 15: seg18:0x3f02. [BYTES seg8:0x76f0-0x770d]
8. Only when result ≠ 0 and op = DGROUP:0x9324 (seg8:0x771a-0x7728): crime 1 seg13:0xb6f2(o, 1) for a pool-0 target with `capturable` = 1 while o's +0x32 = 0, +0x34 = rec, +0x38 = 1; crime 6 seg13:0xb6f2(o, 6) for a pool-8 target with word DGROUP:0x3d5a + 0x34·rec = 0x10 while o's goal is (8, rec) with +0x38 = 1. [BYTES seg8:0x772b-0x77a0]
   - Then, when o's word +0x52 is 2 … 4, +0x58 ≠ o and +0x54 = pool: a pool-9 target sends seg3:0x6ec(o, DGROUP:0x9324, +0x58, 0x75) and seg13:0x2714(+0x58, o, 15, 45, 0, 6); else when +0x56 = rec, seg3:0x6ec(o, DGROUP:0x9324, +0x58, 0x76) and seg13:0x2714(+0x58, o, 40, 90, 0, 6); else nothing more of step 8. After either call +0x52 = 0 when `PlayMode` = 0, or when o ≠ the player and DGROUP:0x95bc = 0; the flag = 1. [BYTES seg8:0x77a3-0x7858]
   - When those first tests fail: with +0x52 = 1 or 5 and a ship target whose goal is (DGROUP:0x9324, o's +0x58) with +0x38 = 1, seg3:0x6ec(o, DGROUP:0x9324, +0x58, 0x77), seg13:0x2714(+0x58, o, 60, 120, 0, 8) and the flag 1; with +0x52 = 1 or 5 and a ship target failing those tests, nothing; otherwise each live i ≠ o whose goal is the target with +0x38 = 1 gets seg13:0x2714(i, o, 25, 40, 0, 6) and seg3:0x6ec(o, DGROUP:0x9324, i, 0x77). [BYTES seg8:0x785a-0x792d]
   - This differs from the dispatcher's copy (core.md): the dispatcher tests +0x52 = 1 only (seg8:0x661f) and passes (15, 40), (50, 90), (30, 50) to seg13:0x2714. [BYTES seg8:0x661f seg8:0x65bc seg8:0x6604 seg8:0x66c3]
9. The virused icon's 0x5e and 3000 (core.md). [BYTES seg8:0x792f-0x7977]
10. The race marker. [BYTES seg8:0x797c-0x79e2]
11. A destroyed ship target: seg4:0x7636(rec, op, o, flag), then DGROUP:0x9432 += 200 when o is the player. [BYTES seg8:0x79e7-0x7a19]
12. The player's +0x156 = now − 1 on any destruction. [BYTES seg8:0x7a1f-0x7a3c]
13. The kill scorer for the player's shot. [BYTES seg8:0x7a40-0x7a5e]
14. Message 0x5b for a destroyed pool-9 target when op = DGROUP:0x9324 and the flag is 0, sent to o (shown only when o is the player). [BYTES seg8:0x7a64-0x7a87]
15. The tumble and lock-out of a live ship target (core.md). [BYTES seg8:0x7a8f-0x7b1c]
- **Precisions.** After 0x75/0x76 the escort block needs o's +0x52 in 2 … 4, +0x58 ≠ o and +0x54 = the target pool, and has **no DGROUP:0xb0c test** (the dispatcher's twin tests DGROUP:0xb0c ≠ 0 at seg8:0x6589); a pool-≠9 target with +0x56 ≠ the target record leaves the whole block (seg8:0x7836 → seg8:0x792f). The 0x77 branch (o's +0x52 = 1 or 5, target pool = DGROUP:0x9324) needs the target's goal = (DGROUP:0x9324, o's +0x58) and target +0x38 = 1, else it does nothing; the "else" loop runs only for +0x52 ∉ {1, 5} or a non-ship target and for each i ≠ o, alive, goal = the target, +0x38 = 1 calls seg13:0x2714(i, o, 25, 40, 0, 6) **and sends seg3:0x6ec(o, DGROUP:0x9324, i, 0x77)** (seg8:0x7910-0x791c). After 0x75/0x76, o's +0x52 is cleared when DGROUP:0x9134 = 0, or when o is not the player and DGROUP:0x95bc = 0 (seg8:0x77ff-0x781a); [bp−0x20] = 1 after 0x75, 0x76 and the 0x77 branch, not after the loop [BYTES seg8:0x77a3-0x792d].
- The return value is 2 for a destroying hit, 1 otherwise (seg8:0x7b20-0x7b2e); the pool-8 store at seg8:0x7b06-0x7b1c is DGROUP:0x3d54 + 0x34·rec = now + 120·DGROUP:0x9438 (16-bit product) [BYTES seg8:0x7b00-0x7b2e].

## Pool 7 (the ambulance)
- **on_damage seg16:0x85e** is `xor ax, ax` between the far prologue and epilogue: it returns 0 and writes nothing, so every dispatch, blast, gun hit or bolt on the ambulance does nothing but the dispatcher's sound and tail. [BYTES seg16:0x85e-0x870]
- **on_destroy** is seg12:0x274a (world.md). INSPACE.EXE calls through an object's word +0x66 at exactly four sites: seg12:0xe36 (the tank's on_damage seg12:0xdc4, pool 4), seg12:0x6ba2 (the pod expiry, pool 11), seg7:0x26b1 (the generic on_damage seg7:0x2652, which pool 7 does not use) and seg13:0x4a1d (the pod drop, pool 11), so the ambulance's on_destroy word is never called. [BYTES seg12:0xe36 seg12:0x6ba2 seg7:0x26b1 seg13:0x4a1d]
- The direct callers of seg12:0x274a are seg7:0x51a2 (rocks), seg8:0x4662 and seg13:0x181 (ships), seg12:0x45f and seg12:0x9a8 (pool 6), seg12:0xda2 (pool 4), seg18:0x450e (seg18) and seg12:0x22ff in the gate exit seg12:0x2288, whose callers pass 14 (seg4:0x54fe), 2 (seg8:0x9b2f), the gate test's result (seg12:0x1e28, seg12:0x1e81; the test excludes pool 7, world.md) and, at seg12:0x2444, a word +0x38 of the passing object's record (a held object; a tether never holds pool 7, core.md). No path passes pool 7. seg12:0x2444 is a recursive call inside seg12:0x2288 itself and passes (DGROUP:0x9324, the ship object's word +0x38), reached only for a ship of design team 7 whose roster +0x54 = DGROUP:0x9324, object +0x36 = DGROUP:0x9324 and object +0x38 = roster +0x56; seg12:0x1e28 passes (pool = DGROUP:0x9324, rec = DGROUP:0x9646) and seg12:0x1e81 the gate result's (pool, rec) bytes [bp−9], [bp−0xa]. [BYTES seg7:0x51a2 seg8:0x4662 seg13:0x181 seg12:0x45f seg12:0x9a8 seg12:0xda2 seg12:0x22ff seg18:0x450e seg4:0x54fe seg8:0x9b2f seg12:0x1e28 seg12:0x1e81 seg12:0x243c-0x2444]

## A computer ship's damage model

### Systems and divisors
- seg8:0x45e0 is the ship pool's handler for every ship (core.md); a computer ship differs from the player only where the bytes test DGROUP:0x9646: step 3's scale (d·(DGROUP:0x9000 + 6)/8) and the rating loss apply to the player only; step 3's Demon's Gate stamp to ship 1. [BYTES seg8:0x469a-0x46a2 seg8:0x4776]
- **The divisors** (seg14:0x1cae-0x1d0e, inside seg14:0x1b84, di = the MASTER.SHP record): s = sext8(SHP byte +2); each store is `cbw; idiv bl|cl; cbw`, an 8-bit signed divide whose quotient byte is sign-extended:

  | Store | Design word | Value | Use |
  |---|---|---|---|
  | seg14:0x1cb8 | +0x0a DGROUP:0x96dc | s/4 | shot, lamp 1 |
  | seg14:0x1cc6 | +0x0c DGROUP:0x96de | s/5 | shot, lamp 2 |
  | seg14:0x1cd2 | +0x0e DGROUP:0x96e0 | s/5 | shot, lamp 3 |
  | seg14:0x1cde | +0x08 DGROUP:0x96da | s/4 | shot, lamp 0 |
  | seg14:0x1cea | +0x12 DGROUP:0x96e4 | s/5 | hit, lamp 1 |
  | seg14:0x1cf6 | +0x14 DGROUP:0x96e6 | s/4 | hit, lamp 2 |
  | seg14:0x1d02 | +0x16 DGROUP:0x96e8 | s/5 | hit, lamp 3 |
  | seg14:0x1d0e | +0x10 DGROUP:0x96e2 | s/4 | hit, lamp 0 |

  then byte +0xa6 (DGROUP:0x9778) = SHP byte +2. [BYTES seg14:0x1cae-0x1d16]
- The divisor read at seg8:0x4a1e is DGROUP:0x96da + 0xad·n + 2k with n = [bp−0x1c], the design index of the **damaged** ship [bp+8] (seg8:0x4683-0x4687), not the attacker's. The selector is [bp−0xc] = 2 (seg8:0x49e4): the shot path stores 2 (seg8:0x47c3), the object path stores the attacker object's word +0x22 (seg8:0x485e-0x4862), so a non-shot attacker whose word +0x22 is 2 also takes the +0x08 divisors. No other store has displacement 0x96da … 0x96e0 [BYTES seg8:0x4683 seg8:0x47c3 seg8:0x4862 seg8:0x49e4 seg8:0x4a1e seg8:0x4a99].

### Wings and the taunts (seg8:0x2cd8, seg8:0x2d96-0x2e5b)
- The taunts run only when DGROUP:0x92e4 = 0 (the first wave after a restore). [BYTES seg8:0x2d96]
- Loop j = 0 … DGROUP:0x9316 (`jg` exit): j qualifies when its +0x32 = DGROUP:0x9324, +0x34 = the torn ship and +0x38 = 1 (j attacks the torn ship). There is no j ≠ ship test and no `alive` test. For each such j, one message seg3:0x6ec(j, DGROUP:0x9324, ship, code), recipient j, sender the torn ship, so only the player as attacker sees it. [BYTES seg8:0x2e15-0x2e39 seg8:0x2e00-0x2e0f seg8:0x2e59-0x2e5b]
- The code, with the torn ship's wing bytes +0xac/+0xad read after the tear (seg8:0x2d0e/0x2d1e cleared one):
  ```
  if (j.b_ac == ship.b_ac && j.b_ad == ship.b_ad)            code = 0x1b   // seg8:0x2e3b-0x2e57
  else if (ship.b_ac != 0 || ship.b_ad != 0) {                // the torn ship keeps a wing
      if (j.b_ac == 0 && j.b_ad == 0)                       code = 0x1e
      else code = seg4_5d9c(ship, j) > 95 ? 0x1c : 0x19      // signed `jle` on 0x5f
  } else code = seg4_5d9c(ship, j) > 95 ? 0x1d : 0x1a
  ```
  [BYTES seg8:0x2da6-0x2dfe seg8:0x2e3b-0x2e57]
- **seg4:0x5d9c(a, b)** (cdecl, `retf`): returns 100 when a or b is outside 0 … DGROUP:0x9316 (signed). Otherwise, all 32-bit signed (`cdq`, `idiv ecx`, `imul ecx`): Ka = sext(a +0xd8), Kb = sext(b +0xd8) (DGROUP:0x7168, the rating word seg13:0x33d6 writes), Ea = sext(a +0x26), Eb = sext(b +0x26) (engine);
  `Sa = (Ea·Ka) / (Ea/2 + 50)`, `Sb = (Eb·Kb) / (Eb/2 + 50)`, Sb = 10 when 0; result = lo16(seg5:0x110(100·Sa, Sb)). So it is a's strength as a percentage of b's. [BYTES seg4:0x5dab-0x5e84]
- **The taunts and warnings of seg3:0xf61.** Codes 0x3b–0x3f, 0x41–0x44, 0x57 and 0x58 (cases 58–62, 64–67, 86 and 87 of the switch seg3:0x1351) share seg3:0xf61: one `rand()` % 4 picks seg10:0x744c(0x3d), (0x3e), (0x3f) or no sound, then the string code + 0x1eb in mode 2. The dispatcher returns before its switch for a recipient that is not the player, so these codes draw only for player-bound messages [BYTES seg3:0x6ec-0x760 seg3:0xf61-0xf95].
- **Texts** (seg3:0x6ec cases, table seg3:0x1356 + 2·(code − 1)): 0x19 → seg3:0xda9: seg10:0x744c(0x3f) then string 373 "Oohh, That Smarts. You Owe Me A Wing!!"; 0x1a → seg3:0xdb9: 375 "There Goes The Other One. Must Be Wing Night Tonight."; 0x1b → seg3:0xde1: 383 "Would You Look At That.. Now Our Wings Match!"; 0x1c → seg3:0xde9: 377 "You Ripped Off My Wing And I'm Still Tougher!!"; 0x1d → seg3:0xdd1: seg10:0x744c(0x3f) then 379 "I May Have No Wings, But You're Still Weaker!"; 0x1e → seg3:0xdc1: 381 "I Still Have More Wings, You Lousy %D" with %D = (14, DGROUP:0x9646, 0), the player's name upper-cased. All mode 2 into DGROUP:0x8db0. Sound 0x3f is GETYOU.WAV "I'm gonna get you." [BYTES seg3:0xda9-0xdee] [BYTES ENG_LANG.DLL STRING 24] ids 373–383 [REF re/artifacts/sounds.tsv]
- DGROUP:0x92e4 ≠ 0 jumps from seg8:0x2d9b to seg8:0x343d, skipping the taunt loop **and** the debris block seg8:0x2e5d-0x343a (a free-entry search over DGROUP:0x9176 + 0x22k, k < 3); DGROUP:0x92e4 ≠ 0 is the first wave after a restore, when seg14:0x0 re-tears wings [BYTES seg8:0x2d96-0x2d9b seg8:0x2e5d-0x343d].

### Shield, cloak, repair, fuel
- **Shield and cloak** on a computer ship: the mode factor of seg8:0x45e0 step 8, the touch exit 4, the capture pass-through, the weapon-system gate and the damage drop ([core.md](core.md)) are per ship already. For a ship other than the player (or the player with `PlayMode` ≠ 0) both toggles refuse within 150 frames of the last toggle (+0x152) and the cloak needs +0x2c > 40 ([core.md](core.md)). Who toggles a computer ship's shield or cloak is the pilot's (Later stages).
- **Repair**: seg11:0x1d7c loops every ship ; a computer ship repairs 1 per lamp per tick (2 only for the player's lamps 0 … 2). Needs the design's +0x4c and lamp rows (D4).
- **Fuel**: seg11:0x1bcc loops every ship ([core.md](core.md)). A cloaked computer ship below 120 drops its cloak and writes "<pilot>: Uncloaking Because I'm Low On Gas." into the command line DGROUP:0x8630 with the player's name, as for the player. No contact path reads `fuel` except the doughnut's +20 and the tank's +250.
- **Fleeing** is goal mode +0x38 = 4: the activity text seg3:0x1d0a switches on +0x38 (seg3:0x20bd-0x20d9: 1 → seg3:0x20f9, 2 → seg3:0x2cf5 "Capturing", 3 → seg3:0x2a9e, 4 → seg3:0x2f50, 5 → seg3:0x2f9d), and case 4 writes string 482 "Running Alright?" with 483 "Please Leave Me Alone!" when the ship's goal (+0x32, +0x34, read into [bp−4]/[bp−6] at seg3:0x1d19-0x1d29) is (14, a), a being seg3:0x1d0a's first argument (0x64 from seg13:0x2f30, so never there), and otherwise 484 "Fleeing From %D" with the goal words. [BYTES seg3:0x1d19-0x1d29 seg3:0x20bd-0x20d9 seg3:0x2f50-0x2f92] [BYTES ENG_LANG.DLL STRING 31] ids 482–484
  - Contact reads mode 4 at: the dispatcher's DGROUP:0x9432 += a for the player hitting a ship that flees from the player ([core.md](core.md)); the gun's += DGROUP:0x9438/2 ([core.md](core.md)); seg4:0x7636's revenge test (§3.2); seg4:0x5e8e's crime 3 (§Later stages). Its writers are the pilot's (seg4:0x6e3, seg4:0xb43, seg4:0x14f3, seg4:0x26a2, seg4:0x272e, seg4:0x2f5c, seg4:0x68ba, seg12:0x21b9, seg13:0x80c5, seg13:0xa0ef and the stores of 1, 2, 3, 5 listed in Later stages); none sets 4 by an immediate. [BYTES seg4:0x6e3 seg4:0xb43 seg4:0x14f3 seg4:0x26a2 seg4:0x272e seg4:0x2f5c seg4:0x68ba seg12:0x21b9 seg13:0x80c5 seg13:0xa0ef]
- **On fire**: for the mover, the only "on fire" state is the dead tumble : `alive` 0, `tumbling` 1, the burnt sheet, the explosion at +0x84. The live ship's smoke below engine 50 is the other visible damage cue.

## Death

### What happens for any ship
- seg8:0x45e0 step 13 → seg16:0x8e4 (8 effect entries, the tumble, the burnt sheet seg13:0x4710 on slot 0x28 + i, `alive` 0, KABOOM 0x45 at the ship, the pod seg13:0x498e(i, 1)) → 6 s later seg12:0x274a (fragments, `state` = 1 at seg12:0x2a97, KABOOM again). The player-only branches are the 8000/1500 ms deadlines and the rating. [BYTES seg16:0x8e4-0xb4c seg12:0x2a97 seg12:0x2a9d-0x2ae2]
- After `state` = 1 the ship mover skips the record (seg13:0xd37) and the visibility pass no longer lists it ([world.md](world.md#visibility-pass-seg80x4e14)). The record stays in pool 14 for the debriefing (+0x06/+0x08 the killer, §3.2).
- The burnt sheet needs the computer ship's sheet in slot 0x28 + i (the routine returns when the slot is empty), which [ships.md](ships.md) builds.

### seg4:0x7636(ship, cp, cr, kill): the killer, the team tallies and the death line
cdecl (`retf`), called from the dispatcher (seg8:0x684b, after seg7:0x2080 and the ship-kill seg13:0x2714 block, for every destroyed ship target) and from the gun (seg8:0x7a01). Locals: [bp−4] = team t of the dead ship (design word +0x38 DGROUP:0x970a of +0x4e), [bp−0xe] the slot flag, [bp−0xc]/[bp−0xa] the best grudge and its ship. [BYTES seg4:0x7636-0x79ac seg8:0x6837-0x684b seg8:0x79e7-0x7a01]
```
DGROUP:0x2384 (dword) = now + 5000                                  // seg4:0x7649-0x7653
if cp == 14 && team(cr) == 7: ship.w06 = 14; ship.w08 = cr; return  // seg4:0x7669-0x768f
if ship.w32 == 14 && sbyte(ship + 0xf7 + 9*ship.w34) > 3:           // seg4:0x7692-0x76b3
    cp = 14; cr = ship.w34
best = 7; bestj = -1
for j = 0 .. DGROUP:0x9316, j != ship:                              // seg4:0x76c5-0x770a
    v = sbyte(ship + 0xf7 + 9*j)
    if cp == 14 && j == cr: v += 5; if v > 12: v += 100
    if v > best: best = v; bestj = j                                // `cmp [bp-0xc], ax; jge`
if bestj != -1: cp = 14; cr = bestj                                 // seg4:0x770c-0x771a
if cp != 14:                                                        // seg4:0x7977-0x79a2
    if ship != player: ship.w06 = cp; ship.w08 = cr; msg(player, 14, ship, 0x62)
    return
ship.w06 = 14; ship.w08 = cr                                        // seg4:0x7725-0x7734
if cr == player: word DGROUP:0x6b40[t] += 1                         // seg4:0x7738-0x774a
if t == 7:                                                          // seg4:0x774e-0x779f
    if sbyte(ship + 0x5e) > 0:
        DGROUP:0x9346 = cr.wd6                                      // cr's roster index
        if cr == player: byte DGROUP:0xa14d += 1; dword DGROUP:0xa147 = frame#; DGROUP:0xa14b = DGROUP:0x91f8
    else: bytes DGROUP:0xa157[0..9] = DGROUP:0xa14d[0..9]
if (sbyte(ship + 0xee + cr) > 5 || ship.w38 == 4)                    // seg4:0x77a1-0x77b5
   && !(t == 3 && sbyte(ship + 0xcd) == cr) && DGROUP:0x9320 != 7:  // seg4:0x77bf-0x77e0
    revenge slot (below)
if ship == player: return                                           // seg4:0x78c9-0x78cf
if cr == ship: msg(player, 14, ship, 0x89); return                  // seg4:0x78d3-0x78df
if cr == player:                                                    // seg4:0x78e2-0x7923
    if team(player) == t && kill == 0: msg(player, 14, ship, 0x63)
    else if kill == 0: msg(player, 14, ship, 0x5f)
else:                                                               // seg4:0x7925-0x7975
    ship.w06 = 14; ship.w08 = cr
    if kill == 0: msg(player, 14, ship, team(player) == t ? 0x60 : 0x61)
```
- **The revenge slot.** Three words per team at DGROUP:0x4eec + 6t + 2j, j = 0 … 2 (−1 = free; the writer of the −1s is [OPEN]). The first free slot takes cr's word +0xd6 (DGROUP:0x7166, the roster index); the word DGROUP:0x4fd0 + 6t + 2j takes DGROUP:0x91f8; the word DGROUP:0x6b10 + 6t + 2j takes the dead ship's roster word +0x00 (far DGROUP:0x6e24 + 0x7c·idx) when its +0xd6 ≠ −1, else word DGROUP:0x4ed8 + 2·sbyte(ship + 0x5e). With no free slot, slot 0's DGROUP:0x4eec word is overwritten but the loop index is 3 for the other two stores, which land at index 3t + 3, the next team's slot 0 (for t = 7, DGROUP:0x5000 and DGROUP:0x6b40, the start of the kill tally). [BYTES seg4:0x77e2-0x78c5]
- **The death lines** (no sound on any of them; mode 2, the sender name from seg3:0x1542 for pool 14):

  | Code | Case | String | Arguments |
  |---|---|---|---|
  | 0x5f | seg3:0x10b3 | 1050 "This Ship Is A Write Off. I Hope My Allies Get You!" | none |
  | 0x60 | seg3:0x10c3 | 1052 "Flame Out..Arrrgghh! Get %D!" | %D = (14, sender +0x08, 5) |
  | 0x61 | seg3:0x10d8 | 1054 "I'm Outta Here, For Good. I Was Killed By %D!" | %D = (14, sender +0x08, 5) |
  | 0x62 | seg3:0x10e7 | 1054 | %D = (sender +0x06, sender +0x08, 2) (dword push) |
  | 0x63 | seg3:0x10ab | 1058 "I Can't Believe You Did That. I'm Your Ally!" | none |
  | 0x89 | seg3:0x10bb | 1425 "Silly Me. I Killed Myself." | none |

  %D's code 5 is not upper-cased, code 2 is ([shell.md](shell.md)). So a computer ship the player kills says 1050 (another team, kill flag 0) or 1058 (the player's team); one killed by a rock says "I'm Outta Here, For Good. I Was Killed By ROCK!". [BYTES seg3:0x10ab-0x10f8] [BYTES ENG_LANG.DLL STRING 66] ids 1050–1054 [BYTES ENG_LANG.DLL STRING 67] id 1058 [BYTES ENG_LANG.DLL STRING 90] id 1425
- **The kill flag** passed in is the dispatcher's [bp−0x1c] ([core.md](core.md#hit-dispatcher-seg80x5df2)) or the gun's [bp−0x20]: 1 after an escort message 0x75/0x76/0x77, so an escorted kill has no death line.
- DGROUP:0x2384 gates the frame's seg13:0xba04 call ([world.md](world.md#update-loop)), so every ship death postpones seg13:0xba04 by 5 s.
- seg4:0x7636 draws no `rand()`.

- **Precisions.** With j = 3 after the scan, seg4:0x7815-0x7856 stores cr's +0xd6 at DGROUP:0x4eec + 6t (slot 0) but DGROUP:0x91f8 at DGROUP:0x4fd0 + 2(3t + 3) and the name word at DGROUP:0x6b10 + 2(3t + 3) (seg4:0x78aa-0x78c5 for +0xd6 = −1): the slot 0 of team t + 1, and for t = 7 the word after an 8 × 3 table [BYTES seg4:0x7815-0x7856 seg4:0x78aa-0x78c5]. The comparisons with the literal 14 in seg4:0x7636, seg4:0x7d36, seg8:0x9028, seg8:0x8256, seg3:0x1d0a case 4 and the pool-14 pointer DGROUP:0x1e30 of seg13:0xb6f2 test the literal 14 while their callers pass DGROUP:0x9324; in the layout of seg18:0x8bc (ship pool 20, [pitfalls.md](pitfalls.md)) cp = 20 never equals 14 and seg4:0x7636 takes the cp ≠ 14 path (message 0x62, no revenge slot) [BYTES seg4:0x7669 seg4:0x771d seg4:0x7d49 seg8:0x9037 seg8:0x85f3].
- **seg3:0x6ec filters every message:** it returns at once unless the recipient [bp+6] is the player, and it drops a message whose sender is (14, the player) (seg3:0x6fa-0x70d). So the taunts, the 0x5b message, the crime messages and the escort messages reach the screen only when the recipient argument is the player. Before its switch it also calls seg7:0x119c(3, 0) when DGROUP:0xa76 ≠ 3, and seg11:0x3e6e when DGROUP:0xa7c ≠ 0, even for the empty codes such as 0x72 (seg3:0x75e-0x778) [BYTES seg3:0x6fa-0x70d seg3:0x75e-0x778].

### The assist score (seg8:0x7ce0)
seg8:0x7ce0(T pool, T rec, a, res) runs in the dispatcher for a destroyed target when neither the credit nor T is the player ([core.md](core.md)). [BYTES seg8:0x64bd-0x64ea]
```
n = 0
for k = 0 .. 7:                                                      // seg8:0x7cf7-0x7d51
    if sbyte(DGROUP:0x938a[k]) == T pool && sbyte(DGROUP:0x9392[k]) == T rec:
        if word DGROUP:0x937a[k] == 0: n += 1
        q = seg5:0x2b4(frame#, 10) - sext32(word DGROUP:0x937a[k])    // 32-bit
        if hi16(q) == 0 && lo16(q) < 20 (unsigned): n += 3
if n > 6: seg8:0x7be8(T pool, T rec, a, res)                         // the kill scorer, as if the player killed it
```
The history is filled by seg8:0x81f8, called only at seg8:0x6259 (the dispatcher, pool-0 targets) and seg8:0x7290 (the gun). So the player gets an icon's kill score when another object destroys it within 20 tenth-frames (about 5.6 s at 36 frames/s) of three of the player's hits. [BYTES seg8:0x7ced-0x7d66 seg8:0x6259 seg8:0x7290]

### The pod
- Ported For a computer ship the fill has no team test: with flag 1 and DGROUP:0x92e8 = 0 it copies slot bytes j = 1 … 9 below +0xcb, so **a dead Enforcer holding HANDCUFFS (weapon 15) in slot j ≥ 1 drops a kind-1 pod with weapon 15 and its count**, which the taker's seg13:0x1dea-0x1e68 adds through seg12:0x6d64 (or to an existing HANDCUFFS slot, capped at 100). [BYTES seg13:0x4bbc-0x4c3e seg13:0x1dea-0x1e68]
- A non-team-7 taker of a kind-1 pod outside Demon's Gate commits crime 2 (seg13:0x1f59-0x1f90, core.md): with a witnessing Enforcer the message 0x46 "That Was Evidence! It's Mine To Take." ([pilots.md](pilots.md)).
- After a flag-1 drop the first live hunter (team 7 with signed +0x5e > 1, or team 3) gets the pod as its capture goal.
- **The kind-3 pod** (flag 0) has one source: seg13:0x4ac4(ship), whose only caller is seg11:0x7043 in the key handler seg11:0x6b64 (a key case reached at seg11:0x6ea6 with both locals [bp−0x1c] and [bp−0x1a] non-zero); seg16:0xb3c, the death, always passes 1. A kind-3 pod gives its taker nothing (core.md). After the drop seg13:0x4ac4 re-aims the ships that attack the dropper (Later stages, commands). [BYTES seg13:0x4ad2-0x4ad8 seg11:0x7033-0x7043 seg16:0xb37-0xb3c]
- **Kind-2 pod** (seg13:0x4ba0, DGROUP:0x92e8 ≠ 0 and a ship other than the player): the path seg13:0x4c98 → seg13:0x4cb0 writes kind 2 and +0x4a = 0x1d only. It skips the fuel move at seg13:0x4c77-0x4c84, so P +0x02 and the weapon words DGROUP:0x856c/0x8580 keep the record's previous pod values and the ship keeps its fuel. [structs.md](structs.md#pool-roles) documents kinds 1 and 3 only. The flag-1 fill also clears the ship's slot bytes +0xb8 + j and +0xae + j for j = 1 … 9 and sets +0xcb = 1 (seg13:0x4bf0-0x4bfe, seg13:0x4c51) [BYTES seg13:0x4bf0-0x4c51 seg13:0x4c98-0x4cb0].

### Score and roster
- **Score**: only kills credited to the player score (seg8:0x7be8 at seg8:0x6486 and the gun's seg8:0x7a5e, plus the assist of §3.3). A computer ship killing a computer ship scores nothing; the player's ship kill adds P[14] = 0 points and 8·d / the ship's +0xd8 terms to the rating ([globals.md](globals.md)). DGROUP:0x9432 += 200 for the player's ship kill ([core.md](core.md)).
- **Roster at death**: no roster write in seg16:0x8e4, seg12:0x274a or seg4:0x7636 except the reads of the roster index +0xd6 and roster word +0x00 above; the dead ship's roster record is updated at the wave end (the wave-end write-back, [ships.md](ships.md)).
- **Attitudes at death** (Later stages): seg13:0x2714(T, cr, −50, −150, 1, −1) for every ship kill and seg13:0x2e60(j, cr, 4) for each j < DGROUP:0x9316 with seg13:0x2688(j, T) > 0 ([core.md](core.md)).

## Capture by a computer ship
- **The capture code is generic:** the touch's exits, the PatBlt, seg13:0x6716, the push-out, the capture with the dissolve and the counters, the virused branch for any ship (resources −100; the steering words +0x06/+0x08/+0x32/+0x34/+0x36…+0x3c, +0x94; DGROUP:0x8690), the good branch (resources, +0xd2, the teammate's message 0x6f with 1000 to the score and rating, the defenders moving to the capturer, the sound, message 0x55 sent to the capturer and shown only to the player), the tank's +250 fuel, the pods and the doughnut. A computer ship captures only with its design's +0x42 ≠ 0.
- **What the player sees and hears** when a computer ship captures an icon on screen: the icon's square blacked and its dissolve (seg7:0x3a42, 4/6/11 frames by `Detail`), the icon column's row dissolving and then black, the capture sound at the icon's position through seg10:0x754a(id, 0, rec, 1) (0x23, 0x26 or 0x66 by the defender count; silent beyond 800 cells, sound.md), no message unless the capturer is on the player's team (0x6f "I've Captured An Icon. Score 1 For The %s.", +1000 score), "Hey, I Wanted That %D!" (0x49) when the player had the icon as its own capture goal with the bytes +0x148/+0x149 set ([core.md](core.md)), and the player's sprite redrawn on the next frame (the touch sets the player's +0x156 = now − 1 whichever ship touched). [BYTES seg13:0x17fb-0x1812 seg13:0x1bba-0x1c06]
- **Off screen**: an off-screen computer ship never meets an icon in the pair finder (only `on_screen` records are in the visibility list); the pilot seg4:0xbde captures its goal off screen by IntersectRect of the goal's (x − 0x90, y − 0x90, x + 0x180, y + 0x180) with the ship's world square and calls seg13:0x16ac(ship, goal) ([core.md](core.md)). That call is the pilot's ([pilots.md](pilots.md)).
- **DOUGHNUTS eaten by a ship** (the pool-6 branch with W(w)+0x42 ≠ 0): on screen through the pool-6 mover's seg13:0x16ac, off screen through the pilot's goal touch. The eater gets object +0x26 = 1, `tumbling` = 1, +0x84 = now + 1200 and `fuel` = min(tank, fuel + 20); sound 0x71 (INSPACE's sequence: 0x24 at once, 0x25 660 ms later, re/artifacts/sounds.tsv) at the eater for W+0x24 ≠ 6; the doughnut dissolves (seg12:0x129e); seg13:0x4eec(eater, 50). In the rivals loop a ship k whose capture goal (+0x32, +0x34, +0x38 = 2) was the doughnut matters only when k is a team-7 ship and the player: then crime 4 is charged to the eater (seg13:0x31f1-0x320e); a computer k sends nothing, since the message branch needs pool 0 or 11 (seg13:0x3152-0x315c). [BYTES seg13:0x1e8d-0x1f59 seg13:0x3152-0x315c seg13:0x31f1-0x320e]
- **The kind-6 catch** (BOOMERANGS) by a computer ship: the pool-6 mover's touch with +0x53 = 1 and the dispatcher's flag clear for a ship target holding a kind-6 weapon ([core.md](core.md)), generic.

- **Rivals' amount.** In seg13:0x3008's goal loop the 0x49 branch runs only for pool 0 or 11, so its `cmp [bp−6], 4` (seg13:0x3172) never holds: the call is always seg4:0x5e8e(k, ship, 90, 2), and seg4:0x5e8e's first step turns 90 into 50 ([core.md](core.md) writes 90 for the rivals) [BYTES seg13:0x3152-0x318f seg4:0x5e9c-0x5ea2].

## The defender mover (seg8:0x8256, pool 2)
- seg8:0x8256 (3538 bytes, called at seg7:0xce7) moves the pool-2 records k < DGROUP:0x940a with their 0x24-byte entries DGROUP:0x52fe + 0x24k (word +0x00 the state, +0x02 DGROUP:0x5300, +0x06/+0x08 the owner DGROUP:0x5304/0x5306, +0x0a/+0x0c the target DGROUP:0x5308/0x530a, +0x0e dword DGROUP:0x530c, and words below the base at DGROUP:0x52ec/0x52ee/0x52f6/0x52fa). It shares with the ship code only the integrator seg8:0x1b54, the speed limit, the sprite seg8:0xb76 and the contact calls; its steering (seg8:0x19b0, seg8:0x285a, seg8:0x1290), its firing seg8:0x90fa and its states are its own (Later stages). [BYTES seg8:0x8256-0x9027 seg7:0xce7]
- **Contact site 1** (seg8:0x864b-0x86cf), for an on-screen defender: DGROUP:0x943a = 2, DGROUP:0x943c = k, hit = seg8:0x50aa(0x200 | k); on a hit a = seg8:0x546c(hit, &vx, &vy, `extent` + 2, `mass`) and seg8:0x5df2(2, k, hit pool, hit rec, a), and when the hit is a pool-0 record whose flame word DGROUP:0x57aa + 0x23·rec ≠ 0 (a burning icon) a second seg8:0x5df2(2, k, 0, rec, 180). [BYTES seg8:0x864b-0x86cf]
- **Contact site 2** (seg8:0x8e2d-0x8eb3), a second search in the same iteration: seg8:0x546c with the literal mass 55 (`push 0x37`) instead of the defender's; a non-icon hit is dispatched with the response's amount, a burning icon with 180, and an icon that does not burn is not dispatched. [BYTES seg8:0x8e2d-0x8ea9]
- A defender meets a ship in its own mover (pool 2 < 14). The dispatcher's pool-2 rules ([core.md](core.md)) decide: a defender is never hurt by its owner or another defender, a pool-2 attacker does not hurt a `capturable` target, and a pool-2 target's amount is negated.
- **seg8:0x9028(k, state, pool, rec)** (cdecl), the defender's new order: no change when the entry's owner is a ship (+0x06 = 14) whose entry state is already 15, or the target is that owner ship; when (pool, rec) is the owner; when pool is 2 and defender rec has the same owner. State 5 sets +0x02 = 0, +0x00 = max(+0x00, 5) and the target (pool, rec); any other state sets +0x00 = max(+0x00, state) and the target, and for state 15 also the word DGROUP:0x52f6 + 0x24k = 0. Callers: the dispatcher (seg8:0x61c3, state 10), seg8:0x99ce (seg8:0x9aa9, state 15), the defender mover (seg8:0x8623), seg8:0x9b4e (seg8:0x9bb1, the tick's "pool 2's goals") and the key handler (seg11:0x70e6). [BYTES seg8:0x9033-0x90eb]
- **A ship's defenders after the ship leaves**: the mover calls seg8:0x9028(k, 15, 12, 1) for a defender whose owner is a ship (DGROUP:0x5304 = 14) when that ship's `state` ≠ 0 while its `alive` ≠ 0 (a ship gone through the gate), so the defenders follow it to the pool-12 record 1. A dead owner (`alive` 0) does not trigger it. [BYTES seg8:0x85ef-0x8626]
- **A defender's capture**: the capture's good branch hands an icon's defenders to the capturing ship, the player or a computer ship ([core.md](core.md)); the defenders' death on their owner's death is seg8:0x45e0 step 13 (state 15 and the killer). What the defenders then do is the mover's.
- **Site 1 passes `size` + 2.** The response gets the defender's word +0x22 + 2, which is `size`, not `extent` (+0x20): seg8:0x867a `mov dx, es:[di+0x22]; inc; inc` (the ship mover passes +0x20 + 2 at seg13:0xbc1, pool 11 at seg12:0x6b5a too). The preconditions are defender `on_screen` ≠ 0 (seg8:0x8642) and seg8:0x50aa ≠ −1; site 1 dispatches a burning icon twice (the response, then 180), site 2 once (180). Site 2 uses mass 55 and copies DGROUP:0x5794/0x5796 into the entry afterwards [BYTES seg8:0x8642-0x86cf seg8:0x867a seg8:0x8e60 seg8:0x8e87 seg8:0x8eac].

## Overlapping sprites
- **No overlap rule exists.** seg8:0xb76 (seg8:0xb76-0x1247) has no operand DGROUP:0x58a and writes no ship dword +0x156; it erases (L strips or the whole old cell) and blits only the record it is given, SRCCOPY or MERGECOPY of a size × size cell with a black background ([world.md](world.md#sprites-seg80xb76)) [BYTES seg8:0xb76-0x1247].
- **Computer ships repaint themselves every frame**: seg13:0x0 draws every on-screen ship except the pinned player and the served ship in ambulance states 3 … 6 on each frame, and it is the first mover of step 21 of the frame, so an object drawn later in the same frame (rocks, posts, defenders, icons, pool 5, missiles, pods, tanks) wins the overlap on screen and the ship is whole again on the next frame [BYTES seg13:0xa57-0xb55 seg7:0xcd3-0xd2e].
- **The player is redrawn only by seg7:0x1208**, whose triggers are DGROUP:0x58a (writers seg7:0x6025, a smoke record whose point lies in the player's square grown by a margin [bp−6], and seg12:0x4f3e in seg12:0x4d8e, the kind-6 frame call; seg7:0x13a2 clears it), the player byte +0xeb, the 1500 ms tick once +0x156 is due, and every seg13:0x6716(player) (turns, flame changes, touches, the tumble). DGROUP:0x588 is tested at seg7:0xd84 and has no writer, a build constant 0. Every touch by any ship and every destroying dispatch or shot sets the player's +0x156 = now − 1 (seg13:0x17fb-0x1812, seg8:0x6853-0x686a, seg8:0x7a1f-0x7a3c), and every dispatched collision of the live player tumbles it (seg8:0x6890-0x68d4), which redraws it every second frame [BYTES seg7:0x6025 seg12:0x4f3e seg7:0x13a2 seg7:0xd84 seg7:0xd7d-0xdca seg13:0x6720-0x6753].
- So in the original a rock that overlaps a coasting player without a dispatched collision (a separating response, result 0, or a rock with `collided` set by an earlier pair that frame) leaves the overlap black until the next trigger, at most 1.5 s (the tick). An oracle check would be a run in which a rock bounces off the coasting player and the screenshots show the player's cell partly black until the tick [OPEN].

## Image routines that are not a ship's
- **seg10:0x18b0** (1320 bytes) is the pool-8 kind-2 hazard image builder: its callers are the placement's hazard loop (seg9:0x1c2d in seg9:0x10d4) and the rescale (seg10:0x9eba); it reads the cache words DGROUP:0x381a/0x381c/0x381e (+ 6k) and DGROUP:0x38a4/0x38a6/0x38a8 and writes image slots through DGROUP:0x39e8 (0x17 … 0x1b) [BYTES seg10:0x18b0-0x1dd7 seg9:0x1c2d seg10:0x9eba seg10:0x18ed seg10:0x18f6 seg10:0x18ff].
- **seg10:0x533a** (306 bytes) is the launched weapon's sheet (slots 0x30 … 0x38). Its callers are seg9:0x1c4f (a hazard's weapon), seg10:0x9f28 (the rescale, the hazard's weapon DGROUP:0x3d5a), seg12:0x6ddc (loading a weapon, seg12:0x6d64) and seg14:0x140f in the sheet builder's tail, which runs seg10:0x533a(w) for each slot j < byte +0xcb of the ship being built, w = byte +0xb8 + j, when W(w)+0x0a ≠ 0 [BYTES seg10:0x533a-0x546b seg14:0x13f3-0x1425 seg10:0x9cd7-0x9cf0].
- **seg10:0x464a** (39 bytes) loads BITMAP "DEF" (DGROUP:0x769) through seg10:0x6d38 into slot 0 and calls seg10:0x4672; callers the load (seg9:0x26b5) and seg10:0x9651. It is the defenders' picture [BYTES seg10:0x464a-0x4670 seg9:0x26b5 seg10:0x9651].
- **seg10:0x58ce** builds slot 0x16 (DC DGROUP:0x3a14) from IGRAPHIC #340, nine 37 × 37 cells. In a wave its reader is the virus mover seg12:0x532c, which draws pool-9 records from DGROUP:0x3a14 at seg12:0x55e4-0x55ec; the other readers are seg15:0x7d7a and seg15:0x7f75 in seg15:0x7cda [BYTES seg12:0x55e4-0x55ec seg15:0x7d7a seg15:0x7f75 seg10:0x5925].

## Not yet read
- seg13:0x2714's helpers seg13:0x2da4, seg13:0x2db8 and seg13:0x2e60 (reads the table DGROUP:0x131e, calls seg13:0x2688 and seg13:0x4eec, writes ship byte +0xf6 (DGROUP:0x7186)) [BYTES seg13:0x2e60-0x2f2e]; the ship tables met: bytes +0xf7 + 9j (grudge, read by seg4:0x7636 and seg4:0x5e8e), +0xee + j, +0x13e + j (seg4:0x7d36's fight bytes), words +0x62 + 2j, +0x44 … +0x4c (the player's hit times), bytes +0x5d/+0x5e (allies/enemies), +0xc2 + j; the team tables DGROUP:0x3824 (8 × 8 words), DGROUP:0x3894 (8 words), DGROUP:0x6b40 (kills by the player per team), DGROUP:0x4eec/0x4fd0/0x6b10 (revenge).
- The player's hit tallies DGROUP:0x9680/0x9682/0x8e00/0x8e04 are read at seg3:0x2386-0x2605 (an assessment text).
- seg13:0x4ac4 (drop a resource pack, from the key case at seg11:0x7033): after seg13:0x498e(ship, 0) every live ship j in `state` 0 attacking the dropper (+0x38 = 1, goal (14, ship)) gets DGROUP:0x8680 + 2·ship = j and either (roster index ≠ −1) +0x38 = 2 with dword +0x32 = 0x0000000b, or (no roster) the +0x52 = 0x16, +0x54 = 14, +0x56 = ship block from seg13:0x4b49 on [OPEN] [BYTES seg13:0x4ac4-0x4b61].
- The defender mover's own states and steering, its shot seg8:0x90fa (called at seg8:0x8523 and seg8:0x8dce, while DGROUP:0xb6a = 4 and DGROUP:0x9452 ≠ 0 [OPEN]) and the colour cycle at its head (DGROUP:0x186c, DGROUP:0x1874, DGROUP:0x9452).
- Message 0x8a "That's Enough, You Win. I'm Breaking Off Attack." (sound 0x44, string 1426) is sent from seg13:0x6d4d in seg13:0x6900 and seg13:0x7e8c in seg13:0x7ba2. seg13:0xba04 is gated by DGROUP:0x2384.
- seg3:0x1d0a's activity texts by goal mode (strings 470-491) and the writers of +0x38 (immediates 1 at seg4:0x37c, seg13:0x95d9, seg13:0x9673; 2 at seg4:0x359, seg4:0x1cf2, seg13:0x4af6; 3 at seg14:0x15c6, seg4:0x808, seg13:0xb1dd, seg13:0xb2d4, seg13:0xb62e; 5 at seg4:0x1d4f, and register stores).

