# Computer pilots: the tick's ship calls, the flight AI and the laws

What the wave does for computer ships each 1500 ms tick, how a computer ship decides and flies, and how the Enforcers charge and arrest. The roster and the pictures are in [ships.md](ships.md); contact, damage and death are in [ship-contact.md](ship-contact.md). [DESIGN]

Addresses are INSPACE.EXE unless a module is named; DGROUP is seg19. "Ship word +N" is DGROUP:0x7090 + 0x15a·i + N; "object +N" is record i of the ship pool (DGROUP:0x9324, 14 in a wave) through the pool table DGROUP:0x1df8 (or the pool-14 pointer DGROUP:0x1e30, the same block). A(a, b) is ship a's word +0x62 + 2b (DGROUP:0x70f2 + 2·(0xad·a + b)); T(x, y) is the team table word DGROUP:0x3824 + 2·(8x + y). Teams are the 0-based design word +0x38 (DGROUP:0x970a + 0xad·n, n = ship word +0x4e): 0 AVENGERS, 1 PREDATORS, 2 FUZZY ONES, 3 PIRATES, 4 KNIGHTS, 5 SPEED DEMONS, 6 RENEGADES, 7 ENFORCERS (INSPACE.INI [Teams] Team1 … Team8) [BYTES seg13:0x21aa seg13:0x21fa]. Calls are written in declaration order ([pitfalls.md](pitfalls.md)); seg8:0x285a is Pascal (`retf 0x10`): (far &out1, far &out2, first pushed point, second pushed point), and the distance does not depend on the point order.

## The tick's ship calls

The player's 1500 ms tick seg7:0xdaf–0x117d ([world.md](world.md#the-1500-ms-tick-body-seg70xdef-0x1174)) calls, in this order [BYTES seg7:0xf07 seg7:0xfaf seg7:0x101e seg7:0x1074 seg7:0x107a seg7:0x107f seg7:0x116a seg7:0x116f seg7:0x1174]:

1. seg4:0x59bc, in Action mode only (DGROUP:0x9134 = 0), then the copy of the estimate words +0x06/+0x08/+0x36/+0x3a into the goal words +0x32/+0x34/+0x38/+0x3c;
2. seg13:0x7b40(+0x56) and seg13:0x2714(...) on the hunt/chase tests (Action mode);
3. seg4:0x6700(player) for a capture goal of pool 0, 4 or 11 (Action mode);
4. seg8:0x9ab8(), seg13:0x3302() (every tick);
5. the in-use clamp and the ship loop seg7:0x10b9–0x1166;
6. seg4:0x7e2a(), seg4:0x83ae(), seg8:0x9b4e() (every tick);
7. seg13:0x2f30(player).

seg13:0x675e is not in the tick: the ship mover seg13:0x0 calls it first on every wave frame [BYTES seg13:0x15]. Everything a computer ship does to fly is **not** in the ten: it is in seg13:0x0's per-ship body ("The pilot" below).

## Map of the ten calls

| # | Call | Name | Caller, cadence | Subject |
|---|---|---|---|---|
| 1 | seg4:0x59bc() | the player's activity estimate | seg7:0xf07, 1500 ms, Action mode | commands (the player as a commanded ship), attitudes |
| 2 | seg13:0x7b40(c) | the player did c's bidding: report and reward | seg7:0xfaf, seg4:0x5aed, seg13:0xa684 | commands |
| 3 | seg13:0x2714(a, b, dt, ds, prop, kind) | the attitude change: a's view of b | 24 sites | teams and attitudes |
| 4 | seg4:0x6700(s) | rivals for s's capture | seg7:0x1074 (player, Action), seg4:0x3116 | teams and attitudes (rivals) |
| 5 | seg8:0x9ab8() | defenders sent to the gate leave | seg7:0x107a, 1500 ms | defenders |
| 6 | seg13:0x3302() | the capture-claim reset and "hands off" | seg7:0x107f, 1500 ms | teams and attitudes (rivals) |
| 7 | seg4:0x7e2a() | the call-ins: Pirate, Knight, Patrolman | seg7:0x116a, 1500 ms | entry, laws |
| 8 | seg4:0x83ae() | the hunters' guess at a cloaked ship | seg7:0x116f, 1500 ms | flying (target position) |
| 9 | seg8:0x9b4e() | defenders follow their owner's goal | seg7:0x1174, 1500 ms | defenders |
| 10 | seg13:0x675e() | the watch windows' 5000 ms refresh | seg13:0x15, every frame | HUD (IS_SMART, IS_INFO) |

### seg4:0x59bc(), the player's activity estimate (Action mode)
cdecl, no arguments, returns a score in AX that seg7:0xf0c stores in [bp−8], which nothing reads. Reads the player P = DGROUP:0x9646 [BYTES seg4:0x59bc-0x5d9b].
1. **Command bookkeeping (P's +0x52 command state, +0x54/+0x56 command target, +0x58 commander).** With +0x52 = 1, a flag b = 1 unless some ship's goal (+0x32, +0x34) equals P's (+0x54, +0x56) [BYTES seg4:0x5a00-0x5a35]. seg4:0x84de() runs (the greed level, below). With 2 ≤ +0x52 ≤ 4 and P's estimate (+0x06, +0x08) = (+0x54, +0x56) (or +0x06 = 9 with `rand()` % 5 = 1) and +0x36 = 1, +0x52 += 1; a ship target whose word +0x48 (DGROUP:0x70d8) is within 40 of lo16(frame#/10) (seg5:0x2b4) sets +0x52 = 4. At +0x52 > 3: when P is its own commander and the target ship's +0x52 = 2 with +0x56 = P and +0x58 = itself, seg3:0x6ec(P, 14, target, 0x1f); otherwise seg13:0x7b40(+0x58); then +0x52 = 0 [BYTES seg4:0x5a41-0x5b4d].
2. A standing command (+0x52 ≠ 0) is dropped (+0x52 = −1, then 0) when dword +0x98 + 20000 < now unless the target ship's word +0x44 (DGROUP:0x70d4) is within 26 of frame#/10, and when b is still 1 [BYTES seg4:0x5b4f-0x5bae].
3. **The estimate.** Best score s0 = −40; for each visibility-list entry other than P itself, seg4:0x8626 (below) scores it and the best wins with the side outputs DGROUP:0x9686, DGROUP:0x9688, DGROUP:0x968a; then each pool-12 record 2k + 1, k < DGROUP:0x9318 (the gates) [BYTES seg4:0x5bb2-0x5c6d]. A best ≤ 0 with DGROUP:0x9686 ≠ 0 or s0 < −39 writes (+0x06, +0x08) = (8, 0), +0x36 = 3, +0x3a = 2 and returns 0; otherwise (+0x06, +0x08) = the winner, +0x3a = 2, and +0x36 = 1 (attack: DGROUP:0x9686 ≠ 0, or a ship whose word +0x46 (DGROUP:0x70d6) is within 9 of frame#/36 or whose grudge byte +0xc2 + P ≥ 3), 2 (capture: a capturable winner or a gate with DGROUP:0x9688 ≠ 0), 4 with +0x3a = 5 (DGROUP:0x968a ≠ 0), else 3 [BYTES seg4:0x5c74-0x5d8f].

So in Action mode the player's goal words are what the game infers from the player's flying; other ships read them (seg4:0x7e2a, seg13:0x2714's propagation, seg4:0x6700, the chase test of the tick). seg4:0x8626 is read below.

#### The bookkeeping and the candidates of seg4:0x59bc
**CONFIRMED.** The three dwords now − DGROUP:0x52d8 / 0x8e44 / 0x8e28 stored at [bp−0x138 …
−0x140] [BYTES seg4:0x59cb-0x59f0] have no reader in the routine.
none = 1, cleared by order 1 when some ship's goal is the order object; seg4:0x84de; orders 2-4:
none = 0, the advance when the estimate is the order object (or pool 9 with rand() % 5 = 1, drawn
only when the pool matches and the record does not) and +0x36 = 1; order 4 when the order ship's
+0x48 stamp is within 40 (frame#/10); at 4 or more seg13:0x7b40(+0x58) unless the player gave it,
else message 0x1f to the player (from the order ship) when that ship's order 2 on the player is
its own; order 0 [BYTES seg4:0x5a41-0x5b3c]. The give-up: +0x98 + 20000 < now (unsigned) and the
order ship's +0x44 stamp more than 25 frame#/10 old, or none → −1 → 0 [BYTES seg4:0x5b42-0x5bac].
Candidates k < DGROUP:0x9146 (the player's own entry skipped by `cmp byte, 0xe` and cbw rec), then
the gates (12, 2k + 1); strict `>` over −40; [bp−0x2e], [bp−0x30], [bp−0x32] latched per winner
[BYTES seg4:0x5bb2-0x5c73]; the exit to (8, 0) / act 3 / app 2 with +0x3a = 0 first; the winner's
+0x06 / +0x08 / +0x3a = 2; the attacked test from the ship's +0x46 stamp (frame#/36, under 10) or its
grudge byte +0xc2 + P > 2; acts 1 / 2 (+0x53 ≠ 0 or pool 12, with [bp−0x32]) / 4 with app 5 / 3
[BYTES seg4:0x5c75-0x5d6c]. Only [bp−0x2e] can be read unwritten (best = −40 stores it to DGROUP:
0x9686 and takes the exit); [bp−0x30] / [bp−0x32] / [bp−0x142] / [bp−0x144] are read only after a
winner wrote them. That store is the one stale read.

### seg4:0x8626(pool, rec, 1), the player's activity score [BYTES seg4:0x8626-0x8b80]
in every term: DGROUP:0x968a = 0; DGROUP:0x9686 = att = (dword DGROUP:0x8e28 + 2000 >
now, unsigned, 32-bit add); the player's attack goal on (pool, rec) within 3000 ms sets both;
DGROUP:0x9688 = 0; sc = 30 (pool 14 or +0x53 ≠ 0; pool 14 reads no object), 20 (pool 9; pool 8 with
DGROUP:0x3d6c + 0x34·rec = 2, indexed by the record here; pool 2 with DGROUP:0x52fe + 0x24·rec =
15), −200 (pool 5); +25 when sc ≠ 0 and att. seg8:0x285a(&[bp−0xa], &[bp−0xc], object, player)
gives d, dx = player − object, dy likewise; the bearing seg8:0x19b0 gets −dx as its first push and
−dy as its second (`retf 4`: [bp+6] = −dy is the numerator), that is the bearing of the vector (−dx, −dy) [BYTES seg4:0x874d-0x8759 seg8:0x19bf-0x1a09]; ang = ((frame − dir)·360 + (15 if negative))
sar 4; facing = sext(seg6:0x720c(ang)); speed = sext(seg8:0x35d0(vx, vy)) (cdecl, two dwords);
closing = −seg5:0x110(vy·dy + vx·dx, d); approach = (closing << 7) / speed (386 `idiv`). Hit
stamps: pool 14 the five words +0x44 + 2k of ship rec; other pools the eight entries DGROUP:0x938a
/ 0x9392 (cbw, rec ignored for pool 3) / 0x937a; fresh < 36, recent < 300 as "(frame#/10 − sext(stamp))
high word 0 and low word below n". The att and non-att sums, the pool-12 bonus, the capture-gate
tests (d < 360, DGROUP:0x9688 = 1, the +0x53 = 1 bonus), the head-on test (approach < −60, facing
< −70, closing < −30, pool 14, the ship's goal (14, P) with act 1 or order 2-4 on (14, P): DGROUP:
0x968a = 1, sc += 25 − lo16(closing)), the distance term d / −7 + 55 (pool 12) or d / −10 + 40, l8 =
12, the order bonus 15, DGROUP:0x6e54 / 0x6e52 bonus 30, and the result lo16(l8·sc) (CWD) / 10
[BYTES seg4:0x8b69-0x8b75].

### seg4:0x84de(), the greed level DGROUP:0x967e
cdecl, no arguments [BYTES seg4:0x84de-0x8625]: v = min(7·DGROUP:0x9334 + 45, 100); before frame# 500, v − ((700 − frame#) >> 3) (a logical shift); from frame# 500, min(frame#/100 + v, 100) (unsigned); at least 3. The player's money (sext resources +0xce + credit dword +0xdc) below 100 takes 20 off, above 250 adds 20, above 1000 adds 30. n = the pool-0 records 0 … 7 in `state` 0 (the in-use count is not read) and r = 10n/(DGROUP:0x9316 + 1): r < 15, r < 10 and r < 5 each take 10 off; DGROUP:0x9000 = 0 takes 10 off. The result goes to DGROUP:0x967e, which the goal choice reads as "greed".

### seg13:0x7b40(c), the player did c's bidding
cdecl (c at [bp+6]) [BYTES seg13:0x7b40-0x7ba1]: seg3:0x6ec(P, 14, c, 3) (message 3: sound 0x3b
[BYTES seg3:0xb80]); P's +0x52 = 0 in Action mode; then seg13:0x2714(c, P, 15, 0x41, 1, 8), or seg13:0x2714(c, P, 10, 0x16, 0, 6) when P's +0x54 = 9 (the command was against a virus).

### seg13:0xa65a(), the deferred reply to a command
[BYTES seg13:0xa65a-0xa6ba]: acts only when dword DGROUP:0x885c ≠ 0 and below now
(`jae`, unsigned); dword DGROUP:0x8858 ≠ 0xb → seg13:0x7b40(DGROUP:0x9654), = 0xb →
seg3:0x6ec(DGROUP:0x9646, DGROUP:0x9650, DGROUP:0x9652, 0x1f) (its 8 bytes left to the epilogue);
then words DGROUP:0x964e, 0x9650, 0x9652, 0x9654 and dwords DGROUP:0x885c, 0x8858 = 0. The only
code references to DGROUP:0x885c are seg13:0xa647 (in seg13:0xa5c2) and this routine's three
(`isre pack DGROUP:0x885c`); seg13:0xa5c2's callers are seg13:0xa229, 0xa3bb, 0xa471, 0xa4ea,
0xa55e. While no command routine has run, DGROUP:0x885c stays 0 and seg13:0xa65a does nothing. (DGROUP:0x8858 has
other writers, seg13:0x81c2, 0x8570, 0x8e98, 0x915c, 0xa5c2; they do not matter while 0x885c
stays 0.)

### seg13:0x2714(a, b, dt, ds, prop, kind), the attitude change
cdecl, six words, `retf` with the caller's `lea sp` [BYTES seg13:0x2714-0x2da2]. a's attitudes toward b move: the team word T(ta, tb) by dt and the ship word A(a, b) by ds; prop ≠ 0 spreads the change to third ships; kind is passed on to seg13:0x2e60 (a reaction, unread) and seg13:0x2db8 (unread). The reading of the order of operations:
1. Nothing in a duel (DGROUP:0x9320 = 7); kind 2 forces prop = 0 [BYTES seg13:0x2724 seg13:0x2731].
2. dt is halved when |dt| > 1 and either T(ta, tb) or T(tb, ta) is outside ±300, divided by 3 when also outside ±450, and set to 0 outside ±800 (seg9:0x88ba = abs) [BYTES seg13:0x2750-0x2808].
3. Nothing when a = b.
4. ds < 0: doubled with dt when a's +0x38 = 4 and a's +0x34 = b; the ally/enemy byte c is a's +0x5e; ds ≥ 0: c is b's +0x5d. ds += (−(c − 3)·ds·10)/100 (16-bit signed).
5. Different teams: the team pair moves by (dt/2, dt/6) when dt < 0 and b holds a command (+0x52 2…4) against a ((+0x54, +0x56) = (14, a)), or when T(ta, tb)·dt > 0; else by (dt, dt/3); both changes go to seg13:0x2da4.
6. The ship pair: with ds < 0 and b commanded against a: A(a, b) += ds/2, A(b, a) += ds sar 3 (rounded toward 0); otherwise with seg13:0x2688(a, b)·ds < 1: A(a, b) += ds, A(b, a) += ds/3; else A(a, b) += 2ds/3, A(b, a) += ds/4. seg13:0x2e60(kind, b, a) and seg13:0x2db8 run with each.
7. prop ≠ 0: every other ship e in play and alive with |T(te, tb)| < 350 joins in: e disliking a (seg13:0x2688(e, a) < −100) moves against b by 3/n of the amounts (n = 3, with message 0x51 from e to b, when a hunts e and b hunts a; else n = 11), e liking a (> 100) moves by a quarter; A(e, b) is clamped to ±200.
8. A(a, b) and A(b, a) are clamped to ±200; when b is the player whose target is a and the left display shows a ship (DGROUP:0xa76 = 2), PostMessage(DGROUP:0xa7c, WM_PAINT). The ten's other sites pass (q's +0x34, P, 2(10−b), 5(10−b), 0, 7) (the tick's chase) and the seg13:0x7b40 values above.

**seg13:0x2688(a, b), the attitude read** (`retf`, 52 instructions) [BYTES seg13:0x2688-0x2713]: v = A(a, b) + T(ta, tb) (16-bit); v > 0 adds 20·(3 − sbyte a+0x5d), else adds 20·(sbyte a+0x5e − 3); the result is −300 while DGROUP:0x92e8 ≠ 0 (Demon's Gate), else v. 54 callers, among them the goal choice.

#### The pair and spreading parts of seg13:0x2714
**CONFIRMED.** The word [bp−0xc] = 1 written at seg13:0x2842 when a or b is the player has no
reader before seg13:0x2ac3 makes it the loop counter: dead. Team pair (ta ≠ tb) [BYTES
seg13:0x289a-0x295c]: (dt < 0 and b commanded against a: +0x54 = DGROUP:0x9324, +0x56 = a, +0x52 in
2-4) or lo16(T(ta, tb)·dt) > 0 (`imul word; or ax, ax; jg`) → T(ta, tb) += dt/2, T(tb, ta) += dt/6;
else += dt and dt/3 (`cwd`, `idiv`, toward zero); seg13:0x2da4 twice (empty). Ship pair [BYTES
seg13:0x295f-0x2ab8], all with seg13:0x2e60(a, b, kind) (Pascal `retf 6`, declaration order)
between the two writes: ds < 0 and commanded → A(a, b) += ds/2, A(b, a) += ds/8 (`and dx, 7; sar
3`); lo16(seg13:0x2688(a, b)·ds) > 0 (the relation after the team change) → += lo16(2ds)/3 and ds/4;
else += ds and ds/3. Spreading [BYTES seg13:0x2abb-0x2d0b]: prop ≠ 0; k ≤ DGROUP:0x9316, k ∉ {a, b},
in play in pool DGROUP:0x9324, +0xec ≠ 0; |T(tk, tb)| ≥ 350 skips k without its clamp
(seg13:0x2ca5 jumps to the increment); seg13:0x2688(k, a) < −100: n = 11, or 3 with message (b,
DGROUP:0x9324, k, 0x51) when a's goal is (·, k) act 1 and b's goal is (·, a) act 1; tk ≠ tb → T(tk,
tb) −= lo16(3dt)/n; A(k, b) −= lo16(3ds)/n; ds < 0 with a's goal on k or k's goal on a (act 1) →
seg13:0x2e60(k, b, 6). seg13:0x2688(k, a) > 100 (called again): T(tk, tb) += dt/4 (tk ≠ tb),
A(k, b) += ds/4, and (tk = ta or seg13:0x2688(k, a) > 180) with ds < 0 → seg13:0x2e60(k, b, 3).
Every k that passed the team test gets A(k, b) clamped to ±200 [BYTES seg13:0x2c0e-0x2c38]. Then
A(a, b) and A(b, a) clamped, and PostMessage(DGROUP:0xa7c, WM_PAINT, 0, 0) when b is the player,
the player's +0x40 = a and DGROUP:0xa76 = 2 [BYTES seg13:0x2d0e-0x2d96].

### seg4:0x6700(s), rivals for s's capture
cdecl [BYTES seg4:0x6700-0x6a77]. With s's estimate (+0x06, +0x08) = G in `state` 0: every ship j in play and alive, +0x52 ≠ 0x0e, j ≠ s, whose goal is G, a capturable G, whose claim bytes (+0x148, +0x149) are not G, gets a message from s and its claim bytes = G. Then the first ship j of team 2 or 5 (FUZZY ONES, SPEED DEMONS), not already racing (+0x52 ≠ 0x0e), in play, alive, j ≠ s, not the player in a player game (DGROUP:0x95bc ≠ 0), with (+0x52 ≠ 2 or team 2) and (seg13:0x2688(j, s) < 160 or grudge byte +0xc2 + s > 0), of another team than s and not already after G (team 2 also skips an s of team 0), takes G: +0x32/+0x06 and +0x34/+0x08 = G; within 1900 cells (seg8:0x285a < 0x76c) +0x36/+0x38 = 2 and a message from j to s, else 3; +0x3a = +0x3c = 2, +0x52 = 0x0e, +0x54 = 14, +0x56 = s, dword +0x94 = now + 500; for the player j, the activity text DGROUP:0x8630 = string 1413 "I'm Stealing " + name(G) + 1414 " To Annoy " + name(s) + DGROUP:0x13da, DGROUP:0x58c = 1, DGROUP:0x5c0 = now + 1200. A FUZZY ONE or SPEED DEMON can thus steal the player's capture goal.

### seg8:0x9ab8(), defenders sent to the gate leave
cdecl [BYTES seg8:0x9ab8-0x9b4c]: for k < DGROUP:0x940a, a pool-2 record in `state` 0, off screen (+0x1a = 0), whose defender entry word +0x0a (DGROUP:0x5308 + 0x24k) = 12, and within 170 cells (seg8:0x285a < 0xaa) of pool-12 record (entry word +0x0c (DGROUP:0x530a) / 2, signed): seg12:0x2288(2, k) takes it through the gate. 

### seg13:0x3302(), the capture-claim reset
cdecl [BYTES seg13:0x3302-0x33d5]; nothing in a race (DGROUP:0x9320 = 6). For every ship i whose +0x38 ≠ 2 or whose claim (sbyte +0x148, sbyte +0x149) ≠ goal (+0x32, +0x34): when the player's +0x38 ≠ 1, i ≠ player and i's claim is the player's goal, and seg13:0x2688(i, player) > 0 or i is the player's teammate, seg3:0x6ec(player, 14, i, 0xb8) (a friend gives up the player's capture: case 183 at seg3:0x132b pushes 5 and formats with i's claim bytes; its string is unread [OPEN]); then both claim bytes = 0xff [BYTES seg13:0x33b2-0x33bd]. A claim kept by a ship still after its capture goal (+0x38 = 2) survives. 

### seg4:0x7e2a(), the call-ins (the law part is under "The crime path")
cdecl [BYTES seg4:0x7e2a-0x8344]. Off in a race-path wave (DGROUP:0x913a or 0x913c ≠ 0), before frame# − DGROUP:0x8e2c ≥ 400 (32-bit unsigned; DGROUP:0x8e2c is the frame of the last call-in, seg13:0xb6e0, and the wave start's seg9:0x696b), in Demon's Gate, in kind 6 and 7 waves and while DGROUP:0x992 + 20 > DGROUP:0x932a (the wave budget). Before frame 500 it clears the three deadlines DGROUP:0x1d0a, 0x1d0e, 0x1d12, which no other routine writes [BYTES seg4:0x7e68-0x8282]. Three blocks, each the "warn, then 7000 ms later call":
1. **A Pirate** (team 3) when T(3, tp) < 150 (tp the player's team), DGROUP:0x1d0a passed or 0, fewer than Detail/2 + 3 ships (DGROUP:0x9316 < (`Detail` DGROUP:0x9208)/2 + 3, `cdq; sub; sar` = signed halving) and the player's `resources` +0xce > 398. n = 2 − (Pirates in the wave), 0 when one of them already has byte +0xcd = player; q counts the on-screen ships whose goal is a ship (+0x32 = 14, +0x38 = 1), twice when that ship is the player [BYTES seg4:0x7f05-0x7f69]. With n > 0: c = (+0xce − 220)/32 (signed, `and dx,0x1f` rounding) + T(3, tp)/−100 + DGROUP:0x9334/3 − q − DGROUP:0x9316, + 1 when the player's Engine +0x26 < 40. With `rand()` % 720 < (n·c)/2 (signed) and DGROUP:0x9334 > 0: a passed deadline DGROUP:0x1d0a ≠ 0 calls seg13:0xadbe(player, 3, 0x69) and clears DGROUP:0x1d0a; a zero deadline sends seg3:0x6ec(player, 7, 0, 0x6b) (string 701 "You're Loaded With Stuff. Watch Out for Thieves!", sound 0x30) and sets DGROUP:0x1d0a = now + 7000, and returns [BYTES seg4:0x7f6b-0x803b].
2. **A Knight** (help) when T(4, tp) > −50, DGROUP:0x1d0e passed or 0, the same ship limit and the player's Engine +0x26 < 60. n = 2 − (helpers of the player's kind), where the kind is team 0 for a team-2 player, team 3 for a team-3 player and team 4 otherwise (one with +0xcd = player makes n = 0); f = ships of the player's team, alive; h = hunters of the player alive, +2 when seg4:0x5d9c(player, j) < 40, plus pool-2 defenders in `state` 0 with entry (+0x0a, +0x0c) = (14, player) and entry +0 = 10 [BYTES seg4:0x80a0-0x81bb]. With n > 0 and h ≠ 0: c = (T(4, tp)/120 + 2(2h − f) − Engine/15 + DGROUP:0x9334/2 − DGROUP:0x9316 + 5)·n/2; `rand()` % 300 < c: as above with seg13:0xadbe(player, 4, 0x6a) and seg3:0x6ec(player, 7, 0, 0x6c) (705 "Hang In There, You Could Get Help Anytime Now.") on DGROUP:0x1d0e [BYTES seg4:0x81bd-0x8286].
3. **A Patrolman** when DGROUP:0x9316 < Detail/2 + 4, DGROUP:0x964a > 6, DGROUP:0x9648 > 4, `Detail` > 0, and no Enforcer (team 7) among ships 0 … DGROUP:0x9316 (any state): `rand()` % 300 < DGROUP:0x964a/2 + T(7, tp)/−140 + DGROUP:0x9334/2 calls seg13:0xadbe(player, 0x16, 0x6d) (730 "Abide By The Law, The Patrolman Is Entering.", sound 0x2c), no warning step [BYTES seg4:0x8289-0x833a]. `rand()` order per tick: at most one draw in block 1 (seg4:0x7fc7), then, if block 1 did not return, at most one in block 2 (seg4:0x821f), then at most one in block 3 (seg4:0x831d); a block that returns ends the tick's draws here.

### seg4:0x83ae(), the hunters' guess at a cloaked ship
cdecl [BYTES seg4:0x83ae-0x84dd]. For each ship c with `alive` +0xec ≠ 0 and `cloaked` +0xe8 ≠ 0, for each other alive ship j (ascending) whose goal is (14, c): d = seg8:0x285a(c's dword +0x14a, c's object (x, y)) (cells); d > 350 (signed) moves the guess: +0x14a.x = `rand()` % 4000 + x − 2000, +0x14c.y = `rand()` % 4000 + y − 2000 (two draws, x first), each wrapped once: x < 0 adds 8·DGROUP:0x5a0, x > 8·DGROUP:0x5a0 subtracts it (signed), the same for y with DGROUP:0x5a2. The hunters aim at +0x14a while the target is cloaked (seg4:0x78 step 6 and seg4:0xbde step 3, sections the think and the steer), so a cloaked ship draws pursuit to a point up to 2000 units off, re-guessed every 1500 ms once it is 350 cells from the guess. 

### seg8:0x9b4e(), defenders follow their owner's goal
cdecl [BYTES seg8:0x9b4e-0x9bc0]: for k < DGROUP:0x940a in `state` 0 owned by a ship (entry +6 = 14) with entry word +0 ≠ 15: owner's +0x38 = 1 calls seg8:0x9028(k, 10, owner's +0x32, owner's +0x34) (attack the owner's goal), otherwise entry +0 = 5. 

### seg13:0x675e(), the watch windows' 5000 ms refresh
cdecl, no arguments, the first call of seg13:0x0 on every wave frame [BYTES seg13:0x675e-0x688d].
1. **The ship loop.** For i = 0 … DGROUP:0x9316: while INI `ShipAnim` DGROUP:0x131c = 0, nothing unless dword +0x156 + 10000 < now (32-bit, `jae` skips). Then the ship is refreshed when (+0x156 < now, unsigned 32-bit, and i ≠ player) or its object `state` ≠ 0: +0x156 = now + 5000 (`add 0x1388; adc 0`) and seg13:0x6890(i) [BYTES seg13:0x6773-0x67e6].
   - seg13:0x6890(i) [BYTES seg13:0x6890-0x68ff]: seg13:0x6450(0, 2, i) (cdecl, pushes i, 2, 0) when the IS_SMART window DGROUP:0x95fe exists and i ≠ player; seg13:0x50b0(0, 2) when the right display is shown (DGROUP:0x95fc ≠ 0) and its mark (DGROUP:0x6e90, DGROUP:0x6e8e) is (ship pool, i); seg13:0x58dc(0, 2) when the left display's mark (DGROUP:0x6e18, DGROUP:0x6e16 + 0x1e·DGROUP:0xa76) is (ship pool, i). So ship word +0x156 is, for a computer ship, the due time of its watch refresh; seg13:0x6716 (ship_touched) sets it to now − 1 to force one [BYTES seg13:0x674f]. For the player it is the 1500 ms tick's due time, which this loop rewrites only while the player's object `state` ≠ 0.
2. Then seg13:0x50b0(0, 0); seg13:0x58dc(0, 0) unless the left mark's pool is the ship pool; seg7:0x64ac(0, 0, −1).
3. **The All Ships notice.** With no IS_SMART window, more than one ship and SmWnd DGROUP:0x8f74 ≠ 0: DGROUP:0x95fa = number of ships i ≠ player with seg3:0x0(ship pool, i) ≠ 0; non-zero posts PostMessage(DGROUP:0x16, 0x9b4, 0, 0) (the All Ships window opens, shell.md 0x9b4) [BYTES seg13:0x6824-0x6881]. 

## The pilot

None of this is in the ten calls. seg13:0x0 ([world.md](world.md#ships-seg130x0)) runs every wave frame for i = 0 … DGROUP:0x9316 in `state` 0; for a ship that is not "the player in Action mode" (`PlayMode` DGROUP:0x9134 = 0 and i = DGROUP:0x9646), it reaches these calls. Cadences, all from seg13:0x0 [BYTES seg13:0x224-0x68d]:

| Call | When |
|---|---|
| seg11:0x5ab6(i, 3) | not tumbling (+0xe2 = 0), even DGROUP:0x95f6 (5 frames of 9) [BYTES seg13:0x23b seg13:0x245 seg13:0x255] |
| seg4:0x78(i), the think | DGROUP:0x134e % (10 − max(2·`Detail`, 5)) = 0 while on screen, or DGROUP:0x134e % (18 − 4·`Detail`) = 0, or a race wave [BYTES seg13:0x3e3-0x4d8 seg13:0x4d3] |
| seg4:0x6e70(i), weapon choice | the 9000 ms flag of DGROUP:0x1350 and ship word +0x38 = 1 [BYTES seg13:0x4e1-0x501] |
| seg4:0xbde(i), the steer | turn counter +0x16 < 10: +0x16 += turn word +0x14 (×3 off screen outside a race) [BYTES seg13:0x616-0x676] |
| the throw seg13:0x6e5–0xa57 | `Detail` > 1, on screen, +0x38 = 1, object +0x36 ≥ 0 ([core.md](core.md)) |

With the defaults (`Detail` 3): the think runs on frames with DGROUP:0x134e ≡ 0 (mod 4) on screen and ≡ 0 (mod 6) anywhere; DGROUP:0x134e cycles 0 … 100 (101 values) so the pattern restarts every 101 frames. `Detail` = 5 makes the first divisor 0, a divide fault in the original
[BYTES seg13:0x3e3-0x41a] [OPEN].

### The words a computer ship lives by
The AI keeps two quadruples [BYTES seg4:0xb2f-0xb4b seg4:0x5443-0x545f]:

| Estimate (this think's plan) | Goal (what the ship does) | Meaning |
|---|---|---|
| +0x06 (DGROUP:0x7096) | +0x32 (DGROUP:0x70c2) `goal_pool` | pool of the object |
| +0x08 (DGROUP:0x7098) | +0x34 (DGROUP:0x70c4) `goal_rec` | record |
| +0x36 (DGROUP:0x70c6) | +0x38 (DGROUP:0x70c8) | activity: 1 attack, 2 capture or pick up (also "leave": goal pool 12 with 2), 3 go to or follow, 4 defend, 5 hold (no thrust in seg4:0xbde) |
| +0x3a (DGROUP:0x70ca) | +0x3c (DGROUP:0x70cc) | approach: 2 head for it (seg13:0xd6c mode 2), 3 and 4 veer to one side (modes 3, 4, the dodge), 5 away (mode 5), 7 one think of 3 then back to 2 |

The meanings are names for how seg4:0xbde, seg4:0x78 and seg13:0xd6c use the values; the numbers are bytes. Order words, set by commands, call-ins and the Enforcer code: +0x52 (DGROUP:0x70e2) the order (0 none, −1 re-plan, 1 help, 2–4 the commands, 5, 6, 7 wait for the ambulance, 9, 10 leave, 0x0e race for a capture, 0x14 patrol, 0x15 warn, 0x16 arrest, 0x17 terminate, 0x3e fight back), +0x54/+0x56 (DGROUP:0x70e4/0x70e6) the order's object, +0x58 (DGROUP:0x70e8) who gave it, byte +0xed (DGROUP:0x717d) a one-shot flag (1 = say the line, 2 = second round, −1), byte +0xcd (DGROUP:0x715d) the ship this one was called in for (−1 none). Timers (dwords on the DGROUP:0x1e50 clock): +0x94 (DGROUP:0x7124) the re-plan deadline, +0x98 (DGROUP:0x7128) the order's deadline, +0x9c (DGROUP:0x712c), +0xa4 (DGROUP:0x7134) and +0xa8 (DGROUP:0x7138) the time of the last goal change. Counters: byte +0xca (DGROUP:0x715a) the weapon wait, byte +0xcc (DGROUP:0x715c) the gun wait ([core.md](core.md) "The gun", Bookkeeping), word +0xd4 (DGROUP:0x7164) a second weapon wait, word +0x30 (DGROUP:0x70c0) the think count, byte +0x5f (DGROUP:0x70ef) the weapon class 0–3, byte +0x61 (DGROUP:0x70f1) "no speed cap this think" (structs.md +0x61, +0xda). Grudge bytes +0xc2 + j (DGROUP:0x7152 + j): how often j has hurt this ship; −1 marks j as the target of revenge (structs.md lists them as [OPEN]).

### seg4:0x78(i), the think (Pascal, `retf 2`) [BYTES seg4:0x78-0xbdc]
1. Nothing in a race while byte DGROUP:0x9eb5 = 0. Byte +0x61 = 0 [BYTES seg4:0x93-0xa6].
2. **Clean-ups** [BYTES seg4:0xa6-0x1b5]: an Enforcer whose goal is an Enforcer drops goal, +0x94, +0x52, +0x54, +0x56 to 0; a goal ship that is dead (`alive` +0xec = 0) zeroes +0x94; the Strategy player (DGROUP:0x9134 > 1) with DGROUP:0x8690 < now and Engine +0x26 < 40 zeroes +0x94; a non-Enforcer that hunts (+0x38 = 1) a ship whose word DGROUP:0x8680 + 2·rec (the ambulance's served-ship slot table) is i zeroes +0x52 and +0x94.
3. Demon's Gate (DGROUP:0x92e8 ≠ 0) and the duel wave (DGROUP:0x9320 = 7) blocks: unread [BYTES seg4:0x1b5-0x3c1]. The race: +0x94 = now unless the goal is pool 12 or the ship is an Enforcer.
4. **Re-plan** when +0x94 < now (32-bit unsigned, high word then low) or the goal object's `state` ≠ 0 [BYTES seg4:0x3c1-0x3f9]:
   a. a gone goal with an order +0x52 ∉ {0, 7} whose object (+0x54, +0x56) is also out of play drops the order (+0x52 = 0); in a duel it retargets [BYTES seg4:0x3f9-0x49f];
   b. the Strategy player's DGROUP:0x9134 2/3/4 steps back (DGROUP:0x95bc = 1) and message 0xa4 [BYTES seg4:0x4a5-0x4e3];
   c. seg4:0x702e(i) (revenge, Later stages) returns non-zero → step f; otherwise, with seg13:0x371c(goal) ≠ 0, +0x3c = 2, +0x38 = 2, not the player in a player game (DGROUP:0x95bc ≠ 0) and not an Enforcer → step f; otherwise seg13:0x7ba2(i) (the reactions to other ships, Later stages) and dword +0x9c = 1 becomes now + 5000 [BYTES seg4:0x4e3-0x548];
   d. outside a race (or for an Enforcer), with +0x38 ≠ 4: an order +0x52 < 1 (except the Strategy player in a player game) → byte +0xed = 0, seg4:0x2f98(i, 1) unless +0x36 = 4 (the goal choice, the goal choice), then seg4:0x485c(i, −1) (the offer of help: its chosen-ship local [bp−0x12] is set to −1 once, before the loop over the candidates j, and never per j, so once one ship is chosen every later candidate that reaches the choice test keeps the choice [BYTES seg4:0x486a]; the rest is [OPEN]); an order ≥ 1 → seg13:0x6900(i) (the order executor; team 7 goes to seg13:0xa6bc, section 3.6). A negative +0x06 becomes 0 (both words, `mov dword 0`). The player in a player game with seg3:0x0(estimate) ≠ 0 targets it (DGROUP:0xa62 = 1, seg3:0xf4). Outside a race: orders 3 and 4 outside a duel, or the Strategy player in a player game, call seg13:0xa65a(i) (reduces to seg13:0x7b40), else seg4:0x4d00(i) (the needs, 2.7). When any estimate word differs from its goal word the goal changes: +0xa8 = +0xa4 = now and the four estimate words are copied into the goal words [BYTES seg4:0x548-0x6ab];
   e. **Enforcer patrol upkeep:** an Enforcer with +0x52 < 0x14 whose goal pool is not 11, 12 or 4 gets +0x52 = 0x16, and by byte +0xcd and the class byte +0x5e (DGROUP:0x70ee: 0 Terminator, 1 Intercept, 2–3 Patrol, section 3.1) its order object (14, goal rec or +0xcd) and +0x52 = 0x17 or 0x14; +0xed = 2, +0x58 = i, +0x94 = now, +0x38 = 3, then seg13:0x6900(i) [BYTES seg4:0x6ab-0x810];
   f. (from c) with seg13:0x371c(goal) = 2 seg13:0x3678(i) (unread); seg3:0xf4(i, goal) (the target words +0x3e/+0x40); +0x94 = now + 3000 [BYTES seg4:0x816-0x83f];
   g. seg4:0x79ae(i) (unread, Later stages) [BYTES seg4:0x83f].
5. Word +0x30 += 1; seg4:0x4fc8(i) (the hazard step, 2.8) unless DGROUP:0x9134 = 2 [BYTES seg4:0x848-0x85c].
6. **The aim point** P: for an estimate (14, j) with j `cloaked` (+0xe8 ≠ 0) outside Demon's Gate, j's dword +0x14a (the guess of seg4:0x83ae); otherwise the estimate object's (x, y) [BYTES seg4:0x85c-0x8a4]. d = seg8:0x285a(P, own (x, y)) (16-bit, cells, [bp−0x12]) [BYTES seg4:0x8b6].
7. **Defend** (+0x38 = 4): within 400 cells of a ship estimate that is also the goal, outside the ambulance's hold of i: byte +0xca < 1 calls seg4:0x5654(i) (the weapon system decision), then +0xca += 4 − `Detail` (8-bit), and within 100 cells seg11:0x5ab6(i, 2) (a burst); then byte +0x61 = 1 and +0x3a = 5 [BYTES seg4:0x8a4-0x941].
8. The ambulance-served ship while DGROUP:0x9e49 is 1 … 6 stops here [BYTES seg4:0x947-0x960].
9. **The class-2 weapon** (byte +0x5f = 2, a mine-like weapon, 2.10) with +0x38 ≠ 4: with +0x38 = 1, d < 160 and no live pool-6 record of its own (missile entry words +4/+6 = (14, i)) → +0x3a = +0x3c = 5 (back off), byte +0xca = 0; 160 ≤ d < 211 keeps +0x3c = 5 as +0x3a; farther → +0x3a = +0x3c = 2; with +0x38 ≠ 1, byte +0x5f = 0 [BYTES seg4:0x960-0xa49].
10. **The waits:** bytes +0xca, +0xcc and word +0xd4 each fall by 1 with a floor of 0 [BYTES seg4:0xa4c-0xa76].
11. **Attack** (+0x36 = 1; nothing else follows for other activities) [BYTES seg4:0xa7a-0xbd2]:
    - **Near path** (+0x36 = 1 and the aim distance d < 400 cells, with `Detail` > 1 or the ship on screen; a ship beyond 400 cells never gives up here): with i ≠ DGROUP:0x9646, dword +0xa4 + 19000 < now (unsigned) and (+0x52 = 0, or +0x98 < now with +0x52 ∉ {4, 1, 0x17}), the ship gives up: grudge byte +0xc2 + (+0x34) = 0xff when +0x32 = 14 (seg4:0xaed-0xafd), then +0x94 = 0, +0x52 = −1, seg4:0x2f98(i, 1), +0xa8 = +0xa4 = now, and the estimate +0x06/+0x08/+0x36/+0x3a is copied into +0x32/+0x34/+0x38/+0x3c [BYTES seg4:0xa7a-0xa91 seg4:0xaa4-0xb4b]. Then, with byte +0xca ≤ 0 and on screen, w = seg4:0x5654(i) (the weapon system decision); with +0x3a = 2, w = 0, byte +0xcc ≤ 0 and not (estimate pool 0 with weapon 5 selected): **seg11:0x4e0a(i), the gun** ([core.md](core.md#the-gun-seg110x4e0a); its own aim test fires only when the screen heading to the goal equals `frame`) [BYTES seg4:0xb4f-0xbab].
    - Beyond 400 cells (or off screen at `Detail` ≤ 1) with fuel +0x1e > 150 and d > 700: seg11:0x5ab6(i, 2) (a burst toward it) and byte +0x61 = 1 [BYTES seg4:0xbad-0xbcd].

### seg4:0xbde(i), the steer (cdecl) [BYTES seg4:0xbde-0x1441]
1. Nothing for the ambulance-served ship while DGROUP:0x9e49 is 1 … 6. Word DGROUP:0x1cb8 + 2i += 1 (its only other reference is the race path's low-bit test at seg4:0xc39) [BYTES seg4:0xc0a seg4:0xc39]. A race: seg4:0x144c (race, unread) and seg4:0x4fc8 [BYTES seg4:0xbed-0xcf0].
2. a0 = 1 when +0x36 ≠ 1 and the estimate is the goal, else 0 (passed to seg13:0xd6c). An estimate pool of −1 calls seg4:0x78(i) at once [BYTES seg4:0xcf3-0xd2a].
3. **Aim point** P: a cloaked ship estimate → its +0x14a; an even pool-12 estimate (the record 150 above a gate's middle) while on screen and pool-8 record r of the same number (DGROUP:0x1e18) is on screen above the ship (its `scr_y` < own `scr_y`) [BYTES seg4:0xd1b-0xd4b] → (record x ± 1400 by the side seg8:0x29ea puts it on, wrapped into 0 … 8·DGROUP:0x5a0, record y − 400): the ship lines up beside and above the gate before it turns in; else the estimate's (x, y) [BYTES seg4:0xd2a-0xdef]. d, dx, dy = seg8:0x285a(P, own position) (cells) [BYTES seg4:0xdf2-0xe0d].
4. **Near** (d < 220, +0x36 ≠ 2): order 5 on a ship estimate, or order 0x14 → +0x3a = 7; a virus estimate within 180 that is also the attack goal → seg11:0x5ab6(i, 5) (brake) and no thrust this step; a ship estimate whose own estimate is (14, i) with |dx| > 34 or |dy| > 34 → the same [BYTES seg4:0xe10-0xeb9].
5. **Closing fast** (+0x36 = 1, d > 100, `Detail` > 1): (vy·dy + vx·dx)/d < −50 (32-bit, `idiv` by seg5:0x110) brakes the same way: seg8:0x285a writes dx (the second pushed point − the first, in cells) through its first-pushed far pointer [bp−6] and dy through the second, [bp−8] [BYTES seg4:0xebb-0xf19 seg8:0x2981-0x2990].
6. mode = +0x3a; 7 becomes 3 for this step and +0x3a = 2. (The race adjustments are unread.)
7. **Turn:** heading = seg13:0xd6c(own position, aim point, heading, mode, i, estimate dword, a0), Pascal (`retf 0x14`, ten words): the own position is the pool-14 dword +4, the aim point [bp−0x1a], the heading object word +0x28, then the mode [bp−0x12], i, the dword [bp−0xc] (estimate pool in the high word, record in the low) and a0 = [bp−0x14] last. The mode is +0x3a, except that +0x3a = 7 passes 3 and rewrites +0x3a = 2 (seg4:0xf23-0xf39), and the kind-6 block of seg4:0xf3f-0x1016 passes 2; a0 = 1 when +0x36 ≠ 1 and (+0x06, +0x08) = (+0x32, +0x34) (seg4:0xc8c-0xcb3). A changed heading calls seg13:0x6716(i) (seg4:0x105b-0x106b). [core.md](core.md#the-turn-seg130xd6c) has the shared rules. For a ship, seg13:0xd6c keeps the heading while byte +0xeb ≠ 0 or `shield_step` +0x10 is 1 … 6 [BYTES seg13:0xd7a-0xda3]; in mode 2 outside a race a0 = 0 aims at P (seg8:0x19b0), a0 ≠ 0 takes seg8:0x2658(14, i, est pool, est rec), a led heading [BYTES seg13:0xef4-0xf11 seg4:0x102d-0x104f].
8. **Thrust.** Outside kind-6 waves: with no change or a ±1 step of the heading (seg4:0x1071-0x108e), +0x38 ≠ 5 and [bp−0x16] ≠ 0 (no brake ordered), seg11:0x5ab6(i, 1); otherwise +0x3a 3 or 4 gives seg11:0x5ab6(i, 1) and byte +0x61 = 1, else seg11:0x5ab6(i, 6) (drift; the flame goes out). +0x3a 3 or 4 sets the turn counter +0x16 = turn/3. In a kind-6 wave a larger turn still takes the first branch when +0x3c = 2, sbyte +0x149 < 5 and byte DGROUP:0x9eb5 = 0 (seg4:0x1090-0x10c1); the first branch calls seg11:0x5ab6(i, 6) instead of mode 1 when byte +0x61 = 0 and DGROUP:0x9eb5 = 0 (seg4:0x10d5-0x10f6); and after the mode-6 call of the else branch the turn counter +0x16 is set to +0x14/3 when word DGROUP:0x8e3c + 2i < 2, else +0x14/2 when it is < 4 (seg4:0x1130-0x116e) [BYTES seg4:0x1071-0x116e seg4:0x10b0-0x1191].
9. +0x3a = 5 within 70 cells → seg11:0x5ab6(i, 2) [BYTES seg4:0x1191-0x11b5].
10. **Capture approach** (+0x3a = 2 and +0x36 = 2). (a) A close burst without `rand()`: with (+0x30 & 1) = 0, DGROUP:0x932e > 1, distance < 220, fuel > 200, the heading unchanged and an estimate of pool 0, seg11:0x5ab6(i, 2) at once [BYTES seg4:0x11d1-0x1203 seg4:0x1393]. (b) The far burst needs distance > 800, word +0x26 > 50, fuel > 200, the heading unchanged and [bp−0x16] ≠ 0 (no brake ordered earlier in this call): `rand()` % 18 = 0 (one draw) fires seg11:0x5ab6(i, 2); it stores byte +0x61 = 1 whether or not the draw hits, then skips the near path [BYTES seg4:0x1207-0x12bc]. In a kind-6 wave the same block is also entered at distance > 500 with pool-14 object byte +0x51 = 0 and fuel > 200, skips the % 18 test and, with byte +0x51 = 0, draws a second `rand()`: (`rand()` & 7) = 0 or sbyte +0x149 > 2 fires. (c) At 80 < d < 400 (`Detail` > 1) a capturable estimate that is not `fixed` (or a tank) is hooked: the first slot k (1 … +0xcb − 1) holding weapon 2 calls seg4:0x584c(k, i, 0) when object +0x36 ≠ 10 and ≠ the goal pool, an icon only with resources + credit ≥ 400, a tank only with fuel ≤ tank − 50 [BYTES seg4:0x12b5-0x138c].
11. **Off screen** (only with +0x3a = 2 and +0x36 = 2: seg4:0x11ba-0x11cd return otherwise). The box is built around the estimate object's own position (pool [bp−0xa], record [bp−0xc], object words +4/+6), not around the aim point: SetRect(x − 0x90, y − 0x90, x + 0x180, y + 0x180), so a cloak guess or the gate offset of the aim point does not move it. When it meets the ship's square (x, y, x + extent +0x20, y + extent of its pool-14 object) and the ship is off screen (+0x1a = 0), the touch seg13:0x16ac(i, pool << 8 | cbw(rec)) is called (cdecl), so an unseen ship captures without a collision [BYTES seg4:0x11ba-0x11cd seg4:0x139d-0x143e seg4:0x13b3-0x13e1].

### seg11:0x5ab6(i, mode) for a computer ship [BYTES seg11:0x5cd7-0x5f11 seg11:0x62ed-0x63a8]
Pascal (`retf 4`, ship at [bp+8], mode at [bp+6]); world.md "Thrust" has the shared path. The modes the AI passes: 1 start thrust (seg11:0x63b4 turns the flame on but skips the velocity change in mode 1, world.md "Flame length"), 2 burst then 1, 3 the frame tick (thrust while +0x02 ≠ 0), 5 brake, 6 drift. The computer part (flag [bp−0x1e] set: byte +0x61 = 0 or Thrust +0x2a ≤ 75, or goal pool 12; not the player in Action mode):
1. cap = +0xda = 120 for team 5 (SPEED DEMONS), else 90 [BYTES seg11:0x5cd7-0x5cf6].
2. `Detail` > 1: the first entry k < DGROUP:0x9344 of the 16-byte table DGROUP:0x4f30 (word +0 r, dword +6 a world position) within (r sar 3) + 100 cells changes the cap once: × 7/3 inside the ring (r sar 3) ± 40, × 2/3 otherwise (16-bit `idiv`) [BYTES seg11:0x5cfc-0x5de9]. What the table holds is [OPEN] (hazard zones?).
3. When |vx| + |vy| > cap (seg9:0x88ba): cap ×5/4 (DGROUP:0x94e2 < 30), ×4/5 (70 < … < 100), ×3/4 (≥ 100), then ×3/4, all `(x·n + (sign & 3)) sar 2` or `idiv 5`; `Size` 2 ×4/5, `Size` 0 ×5/4; turn word +0x14 > 30: cap = cap·35/(turn + 10). With sp = seg8:0x35d0(vx, vy): cap ≥ sp ends; else mode = 5 (brake). The branch that would block thrust instead (heading = seg8:0x19b0(vx, vy) and cap > 3sp/2) cannot hold once cap < sp [BYTES seg11:0x5d6c-0x5f11].
4. **The tail** after brake, thrust or drift (not the player in Action mode): with +0x3a = 2, +0x38 = 3 and +0x52 0 or 2, within 400 cells of the goal object: vx = 3vx/4, vy = 3vy/4 (toward zero) [BYTES seg11:0x62ed-0x63a4]. The tail runs after the brake, thrust and drift paths alike. Fuel, mass and auto-repair are per ship already (seg11:0x1bcc, seg11:0x1d7c over i = 0 … DGROUP:0x9316, world.md "Fuel and mass tick"). The thrust force reads the roster byte +0x33 (world.md "Force" step 2), the computer ship's skill; 

### seg13:0xd6c for ships
core.md "The turn" holds the shared rules. For a ship (S > −1) two additions [BYTES seg13:0xd7a-0xda3 seg13:0xef4-0xf2b seg13:0xe54-0xf25]: the early return of the heading while +0xeb ≠ 0 or +0x10 is 1 … 6; in a race, mode 2 with a checkpoint bias from bytes +0x148/+0x149 (race, unread).

### seg4:0x2f98(i, 1), the goal choice (Pascal) [BYTES seg4:0x2f98-0x474a]
1845 instructions; the regions are read in full in "The five regions of the goal choice" below.
1. +0x52 = −1 becomes 0 and sets the "keep" flag b (the old goal is excluded from the search); +0x94 = now + 3000 [BYTES seg4:0x2fa5-0x2fc6].
2. The ambulance-served ship (DGROUP:0x952e) while the ambulance (pool 7 rec 0) is in play: +0x52 = 7, +0x98 = now + 10000, +0x58 = i, +0x9c = now + 5000, +0xed = 0; return [BYTES seg4:0x2fcf-0x3010].
3. **Enforcers** (team 7) [BYTES seg4:0x3013-0x3112]: Patrol class (+0x5e > 1): +0x52 = 0x14, except while the goal is a pod (pool 11) in play: estimate (11, ·), +0x36 = +0x3a = 2 and seg4:0x6700. Intercept/Terminator: when +0x54 ≠ 14 or a race path: +0x52 = 0x16 (class 1) or 0x17, +0x54 = 14, +0x56 = byte +0xcd (or, without one, an out-of-range +0x56 becomes the player with +0x52 = 0x14), +0xed = 1; else message 0xb7 and the estimate (12, 0) with +0x36 = 2: leave.
4. **Ships** j = 0 … DGROUP:0x9316: one `rand()` per j first, ascending, all j (the desire a[j] = `rand()` % 2 + 1 − enemies byte +0x5e): the loop's test sits at its foot and the draw at its head (seg4:0x3683, `call seg5:0xdc`), so every j draws, the skipped ones too [BYTES seg4:0x367b-0x36a1]; for each other live ship in play (not the kept goal) a distance cost (seg8:0x285a + 300) shaped by seg13:0x2688(i, j), seg4:0x5d9c(i, j), the hunt relations, the grudge bytes, the team and the ally/enemy bytes; a liked ship (attitude > 50·(allies − 1), a Knight's > −100, or the same team) competes for "help" (cost local_28, best j), the others for "attack" (local_2c).
5. **Objects** of pools 0, 3, 4, 8, 9, 11 in that order, records below the in-use count (DGROUP:0x9406 + 2·pool), skipping the kept goal and records out of play: costs for "capture" (local_30, a `capturable` 1 object) and "attack an object" (local_34), with fuel, resources, DGROUP:0x9000, DGROUP:0x967e and DGROUP:0x9334 terms [BYTES seg4:0x36c7-0x3cab]. The base is sext(seg8:0x285a(object, i) + 300) (16-bit add), the kept goal's 2·max(cost − 100, 1)/3. Pool 3: (cost + 450) << 2. Pool 8: a gate post (record < 2·DGROUP:0x9318) (cost + 1500) << 3; any other record reads the hazard table DGROUP:0x3d50 + 0x34·r (words +0x0a at DGROUP:0x3d5a and +0x1c at DGROUP:0x3d6c) with r the **pool number 8**, not the record (`imul bx, [bp−4], 0x34` with [bp−4] = 8), so every hazard scores by entry 8: +0x1c = 2 with +0x0a = −3 gives (cost + 2500) << 3, +0x1c = 2 otherwise nothing, else 2·(cost + 350) [BYTES seg4:0x3823 seg4:0x3845]. Pool 0: +0x54 ≠ 0 with i's Engine +0x26 < 50 gives 2·(cost + 200), and DGROUP:0x9000 > 1 takes a quarter off. Pool 11: a pod whose word +0 = 3 (or any pod in a kind-7 wave) that DGROUP:0x8680 + 2·(pod word +0x0a) names as i costs cost/5 (then /3 for a Pirate); otherwise a Pirate pays cost/3 and the other teams cost·3. Pool 4: fuel > 200 gives cost << 2 and +80000 unless tank − 100 ≥ fuel; fuel < 150 gives cost sar 3.
6. **Defend a friend under attack:** a ship hunting the help candidate raises local_28 [BYTES seg4:0x3c3b-0x3d9c].
7. Picks the cheapest of help, attack ship, capture, attack object [the pick and the help block are read below]; the choices draw more `rand()`: attack a ship needs `rand()` % 3 + 1 < a[j] (a Pirate after a rich ship takes order 4 and message 0x67); capture tests `rand()` % 30 and `rand()` % 600 (Pirates and Knights); attack an object `rand()` % (enemies + 2) < 2. With nothing chosen: after frame 800 (or an idle estimate (8, 0) with +0x36 = 3) the nearest gate (12, 2k) with message 0xb7 ("I'm Heading For The Exit", +0x36 = 2); otherwise estimate (8, 0), +0x36 = 3, +0x98 = now + 3000, +0x58 = i [BYTES seg4:0x3d9c-0x474a]. The outputs are only the estimate words, the order words above and seg3:0xf4, seg4:0x8346 (a voice or reaction, unread), seg4:0x474c, seg13:0x9ae8, seg13:0x9ce0 and seg4:0#### The five regions of the goal choice (seg4:0x3149-0x4394)
- **Ship cost terms** [BYTES seg4:0x3149-0x36c4]: help = att = 80000, `mov dword [bp−0x4e],
  −1` sets best_att [bp−0x4e] and best_help [bp−0x4c]; per j (draw first, at seg4:0x3683, then the
  keep test [bp−0x5e] / [bp−0x60]); cost = sext(lo16(d + 300)) (`add ax; cwd`); a[j]++ below 250;
  str = seg4:0x5d9c(i, j), rel = seg13:0x2688(i, j); the player and |rel| > 160 each take a
  quarter (32-bit `sar`); a[j] −= rel / 100 (16-bit); the revenge word > 3; i the player; i's goal
  on j (act 1: sar 1, +2, and with byte +0x146 = 8 another +2, sar 3, at most 70; else +1 and −¼);
  j hunting i (act 1: the fight byte +0x13e + j < −2 gives cost·(−(cbw(f) idiv 2)) by seg5:0x314,
  else sar 1, a[j] += 2, rel −= 300; other act: +1, sar 1); the grudge byte (0xff: (cost + 1000)·5;
  1-8: (9 − g)·cost / 10 in 386 `imul` / `idiv`, a[j] += g idiv 2). The liked test (allies − 1)·50 <
  rel, team 4 with rel > −100, or the same team, reads the rel after the −300 [BYTES seg4:0x33c9-0x33fa].
  Liked [BYTES seg4:0x33fe-0x34a5]: j attacking halves, the same goal as i's then ·2 / 3; then
  seg5:0x110(cost·sext(3·allies + 4), 6) (the signed RTL divide, dividend pushed last); team 4
  takes seg5:0x346's arithmetic quarter. Attack [BYTES seg4:0x34a8-0x3675]: ·2(e + 2) / 6 (386),
  a[j] −= e, str > 300 or < 50, DGROUP:0x8680 + 2j = i (+100000), str < 25 or i's +0x26 < 40,
  teams 2 / 4 / 5, teams 3 / 6 (team 3: j's +0xce > 250, sbyte +0xcd = j: + cost / −3), team 1
  against team 2 (/ 2, a[j] += 2), memory > 5 (+80000), j cloaked (<< 2), i's goal on j (·2 / 3).
  Strict `<` for both bests.
- **The objects' head and attack-object terms** [BYTES seg4:0x36c7-0x37d4 seg4:0x37d8-0x380b
  seg4:0x3b4a-0x3c3e]: the goal-record adjustment max(cost − 100, 1)·2 / 3; the switch sends pools
  3, 8 and 9 to the +0x53 test at seg4:0x37c1 and pools 0, 4 and 11 to their terms and then to it
  as well (seg4:0x3977 → seg4:0x398a), so a +0x53 = 1 object of pools 0 / 4 / 11 is scored for
  capture only and every other in-play object for attack only. Attack: +100; the player (cost +
  400)·2, else seg5:0x110(cost·max(DGROUP:0x967e, 10), 100); then seg5:0x110(cost·2(e + 2), 6);
  team 6 −¼ (seg5:0x346), team 2 + (cost sar 1); with (e + 6)·5 > +0x26 or money ≤ 200: pool 0 ·3,
  else +80000 when (9 − DGROUP:0x9334)·20 < 2·DGROUP:0x967e, else << 2 (seg5:0x8c0).
- **The capture branch and the claimant loop** [BYTES seg4:0x398e-0x3b47]: teams 2 / 5 sar 2, team
  6 −¼; DGROUP:0x9000 < 2 and i not the player: pool 0 (cost − sext(lo16(5·DGROUP:0x967e)) + 300)·100
  / max(DGROUP:0x967e, 10), other pools cost·max(…, 10) / 100 (386). Claimants k ≤ DGROUP:0x9316,
  k ≠ i, cbw(+0x148) = pool and cbw(+0x149) = rec, +0xec ≠ 0, in play, i's team ≠ 6; another team
  needs seg13:0x2688(i, k) > −200; then cost = max(rel + cost + 300, 800)·sext(2(6 − allies)) and
  +80000 when rel > 100. The bytes call seg13:0x2688(i, k) up to four times per claimant (seg4:0x3ae9,
  0x3afc, 0x3a2f, 0x3a71); it is a pure function of the ship records, so one call gives the same value. Both exits go to seg4:0x37c1, whose +0x53 =
  1 test sends the record to the next one: no capture object is also scored for attack.
- **The help block** [BYTES seg4:0x3cad-0x3e00]: help += help sar 2; per k, defender =
  the last matching k; seg4:0x5d9c(best_help, k) ≥ 60 takes a quarter, else sar 2; the same-team
  test is made twice (seg4:0x3ce0 and seg4:0x3cf9: +160000); otherwise seg13:0x2688(i, k) + 100 >
  seg13:0x2688(i, best_help) adds 80000; (allies + 3)·10 > +0x26; money < −50 with sbyte +0x5e > 0
  and +0x26 < 65; team 4 −¼.
- **The pick** [BYTES seg4:0x3e03-0x438d]: the four copies [bp−0x46 + 4k]; all of help, att, cap >
  20000 (signed) → 90000 each and the first in-play pool-0 record sets cap (+0x53 = 1) or attobj to
  100 [BYTES seg4:0x3e91-0x3ebb]; strict minimum below 80000. Pick 0 with best_help ≠ −1: the
  defender test (allies << 5 < seg4:0x5d9c(i, defender), grudge ≠ 0xff, not the kept goal), else
  best_help's goal with the rand() % 30 = 2 gate for the player on pools 3 / 8 and seg4:0x8346's
  AX = 1 exit; the fall-through at seg4:0x40d4 [BYTES seg4:0x40d4-0x4124 seg4:0x4239-0x433c] and the
  attack pick [BYTES seg4:0x4129-0x4236 seg4:0x4347-0x438d], including the Pirate's robbery (DGROUP:
  0x9680, 0x9682 and the two dwords DGROUP:0x8e00 / 0x8e04 cleared) and the gate loop with
  seg13:0x9ae8 at each new best (start 30000). Pick 0 with best_help = −1 and pick −1 both fall to
  the pick-3 test and then to the nothing tail.

x6700/0x6e70.

### seg4:0x4d00(i), the needs (Pascal) [BYTES seg4:0x4d00-0x4fc6]
After a computer goal choice: (a) nothing while leaving ((12, ·) with +0x36 = 2) or in Demon's Gate [BYTES seg4:0x4d0f-0x4d29]; (b) for a ship that is not an Enforcer, with defenders, `Detail` > 1 and order 0 or 7: the pool-2 records in `state` 0 with entry +0x0a/+0x0c = (14, i) and entry +0 = 15 are counted; the nearest-record compare never lowers its bound ([bp−0xe] stays 20000), so the record kept is the last match and a "< 180" alternative never holds; with two or more matches the ship takes the estimate (2, k), +0x36 = 1, +0x3a = 2 and calls seg4:0x8346(i, 2, k, 6) [BYTES seg4:0x4d2d-0x4e17]; (c) fuel: the test is the design tank (DGROUP:0x96f0 + 0xad·(+0x4e)) / 10 > fuel (+0x1e), signed. With fuel ≠ 0 and `rand()` % fuel > 150 the routine returns at once. Otherwise the nearest pool-4 record (pool pointer DGROUP:0x1e08, k < DGROUP:0x940e, `state` 0) becomes the estimate (4, k) with +0x36 = 2, +0x3a = 2 and dword +0x94 = now + 4000, then +0x52 = 0 and the routine returns. With no tank and resources (+0xce, sign-extended) + credit (dword +0xdc) < 200: nothing in a kind-7 wave; otherwise seg3:0x6ec(player, 14, i, 0xb7) and the estimate (12, 0) with +0x36 = +0x3a = 2 (leave). With 200 or more and seg16:0xb4e(i) ≠ 0 (the ambulance accepts): estimate (7, 0), +0x36 = 3, +0x3a = 2 [BYTES seg4:0x4e1a-0x4f57]; (d) a ship held by another ship's tether (object `owner_pool` +0x32 = 14) attacks its holder (estimate (14, +0x34), +0x36 = 1, +0x52 = 0) unless +0x36 = 4 [BYTES seg4:0x4d0d-0x4fb6].

### seg4:0x4fc8(i), the hazard step (Pascal) [BYTES seg4:0x4fc8-0x55e0]
On screen: h = seg13:0x1076(14, i) (an obstacle ahead, pool·256 + rec or 0xffff; its reading is in [core.md](core.md) under the dodge seg4:0x5514, and the ship pool's goal is the ship words +0x32/+0x34 with mode +0x38):
- **Nothing ahead:** the nearest virus (pool 9, DGROUP:0x9418 records, on screen, not the goal) within 250 cells becomes the estimate with +0x36 = 3 and +0x3a = 3 or 4 by the sign of the 32-bit cross product of relative position and relative velocity; then the nearest live pool-6 missile not its own and not of entry kind 0x1b: within 160 cells the first slot k ≥ 1 whose weapon word +0x46 (DGROUP:0x2f9a) ≠ 0 fires through seg4:0x584c (for weapon 2 only while object +0x36 = −1 and the missile's launch kind is 1 or 5); the slot loop ends at that first slot whether or not it fired; within 400 and Engine < 15·class + 40 the missile becomes the estimate (6, k) with +0x36 = 3 and +0x3a = 5 (the dodge); nothing near copies the goal into the estimate [BYTES seg4:0x4fec-0x5170 seg4:0x539e-0x5440 seg4:0x5443].
- **An obstacle h:** the estimate is h with +0x36 = 3 and +0x3a = 4 or 3 by the same cross product (4 also for a gate post of the gate the ship is leaving by), 5 for a pool-8 record of kind −3 [BYTES seg4:0x5173-0x5440]. Then, off screen, a goal that seg13:0x371c accepts whose gate box (DGROUP:0x6678 + 0x1c·(goal rec / 2)) meets the ship's square ± 800, with entry time +0x88 + 3000 < now: seg12:0x2288(14, i) takes the ship through the gate unseen [BYTES seg4:0x5468-0x5505].

#### The threat search of seg4:0x4fc8 (seg4:0x4fef-0x5512)
Off screen (+0x1a = 0) goes straight to the goal copy [BYTES seg4:0x4fef-0x4ff4].
seg13:0x1076(14, i) (Pascal, `retf 4`); 0xffff is "none" (the `cdq` makes the second compare always
true) [BYTES seg4:0x4ffd-0x5010]. A hit: estimate (pool = high byte, rec = low byte zero-extended),
act 3; pool 8 with DGROUP:0x3d5a + 0x34·rec = −3 → app 5; a gate goal (12, act 2) with a pool-8
record < 2·DGROUP:0x9318 → app 4 for an even record, 3 for odd; otherwise the cross product
(O.x − own.x)(O.vy − own.vy) − (O.y − own.y)(O.vx − own.vx) (16-bit differences, `cwd`, seg5:0x314)
> 0 → 4, else 3 [BYTES seg4:0x5014-0x5186]. No hit: the nearest in-play, on-screen pool-9 record
(count DGROUP:0x9418, pointer DGROUP:0x1e1c) that is not the goal, start 350; under 250 → estimate
(9, k), act 3, the same cross product with that virus, best = 20000; else best = 18888 (0x49c8)
[BYTES seg4:0x5189-0x531f]. Then the nearest pool-6 record (count DGROUP:0x9412, pointer
DGROUP:0x1e10) in play, not launched by (14, i) (DGROUP:0x66e8 / 0x66ea + 0x1c·k) and not weapon
0x1b (DGROUP:0x66e4 + 0x1c·k), sharing the index word [bp−0xe] with the virus search
[BYTES seg4:0x5324-0x539c]; the reply and the dodge as in the hazard step above; the goal copy only
when best is still 18888 [BYTES seg4:0x543c-0x5464]; the gate tail with seg13:0x371c, SetRect(x −
800, y − 800, x + size + 800, y + size + 800), IntersectRect with DGROUP:0x6678 + 0x1c·(goal rec / 2,
`cwd; sub; sar`), +0x88 + 3000 < now and seg12:0x2288(14, i) [BYTES seg4:0x5468-0x5505]. The flag
[bp−0x2a] (1, cleared when the obstacle is the current estimate) and [bp−4] (own size) have no
reader. An unwritten [bp−0xe] (no virus and no missile winner) is never read:
best is then 18888 or 20000, both ≥ 400.

### seg4:0x5654(i), the weapon system decision (Pascal) [BYTES seg4:0x5654-0x5848]
Only with a weapon system selected (+0x18 ≠ 0): a ship goal that is tumbling with a W+0x42 ≠ 0 weapon calls seg4:0x6e70(i) first; unless byte +0x5f = 3, the weapon fires only when the direction (seg8:0x19b0 of the screen delta, through seg8:0x29ea for an off-screen end) equals the heading (+0x5f = 1) or its opposite (+0x5f = 2); then, while not cloaked and `shield_step` < 1, or 3000 ms after DGROUP:0x8e28: seg4:0x584c(+0x18, i, 25) and return 1. Otherwise a cloak +0x2e = 0xfe toggles off (seg11:0x4c72) and `shield_step` 7 drops the shield (seg11:0x4b00), each with seg3:0x332e for the player; return 0 [BYTES seg4:0x5662-0x583e].

### seg4:0x584c(k, s, life), the weapon use (Pascal, `retf 6`) [BYTES seg4:0x584c-0x59b7]
k at [bp+0xa], s at [bp+8], life at [bp+6]. Nothing while s's object byte +0x2e ≠ 0; the player in Action mode sets DGROUP:0x8e28 = now. w = byte +0xb8 + k. The wait: W+0x42 (DGROUP:0x2f96 + 0x50w) = 0 sets byte +0xca = 4·(class byte +0x5e) + low byte of W+0x28 (DGROUP:0x2f7c), else +0xca = 2 and word +0xd4 = 3·W+0x28. seg13:0x4eec(s, 5). Then: W+0x0a (DGROUP:0x2f5e) ≠ 0 → seg11:0x7c12(w, 14, s) (Pascal, `retf 6`: the launcher, [core.md](core.md)); w 5 and 4 → seg13:0x1318(14, s, life, w) (Pascal, `retf 8`); w 1 → seg11:0x8a42(14, s) (the ring); w 2 → seg11:0x947a(14, s, 2), w 15 → seg11:0x947a(14, s, 0xf) (the tether); w 3 → seg11:0x9fdc(14, s) (lightning); w 0x17 → seg11:0x9daa(14, s) (the shock); w 0xd → seg11:0x9078(14, s) (the wildcard defence); a weapon with W+0x0a = 0 outside that list launches nothing. The callees other than the two Pascal ones are cdecl. Finally +0x18 = k (the Strategy player with a different slot also DGROUP:0x9524 = k and seg11:0x2f8c) [BYTES seg4:0x58c4-0x59ac].
A computer ship's launches read the same words as the player's: weapon bytes +0xb8 and counts +0xae, `weapon_slot` +0x18, fuel +0x1e, the `cloaked`/`shielded` gates, the goal words +0x32/+0x34/+0x38 (the launcher's aim and the tether search score the goal), +0x54/+0x56 for an Enforcer's cuffs, `frame` and the muzzle tables (design words +0x00/+0x02 and +0x68 … +0x6e) and the ship's charge +0x50 for LIGHTNING. All of these are set by the ship set-up ([ships.md](ships.md)).

### seg4:0x6e70(i), the weapon choice (cdecl) [BYTES seg4:0x6e70-0x702c]
With slot count byte +0xcb ≤ 1 the routine returns after clearing byte +0x5f to 0, leaving +0x18 unchanged [BYTES seg4:0x6e8a-0x6e9e]. Otherwise each slot k = 1 … +0xcb − 1 with a count +0xae + k > 0 is scored: 10000 for weapon 5 against a pool-0 goal whose object byte +0x2f < 0, else seg4:0x6a78(i, k) (cdecl; weapon words W+0x2a, +0x2c, +0x2e, +0x42, +0x04, +0x0a; seg13:0x2688 and seg4:0x5d9c against a ship goal; one `rand()` % 3 per slot that passes its gates; for weapon 2 the count of heavy rocks and gate posts within 320 cells) [BYTES seg4:0x6a78-0x6e6e]. The best starts at −1000. The best score > 0 selects the slot (+0x18 = k) with +0x5f = 2 for class W+0x04 = 2, 3 for class 3, 1 otherwise. The fallback runs only when the best score is exactly −100: then the slot with byte +0xae + k ≥ 0, W+0x2e < fuel and the lowest W+0x2c below 100 wins. Otherwise +0x18 = 0 (the gun) and +0x5f = 1. The player gets seg11:0x2f8c [BYTES seg4:0x6e7e-0x7026].

### seg4:0x79ae(i), cloak and shield for computer ships (cdecl) [BYTES seg4:0x79ae-0x7d34]
Run at the end of each re-plan (the think, step 4g) for a ship with `has_cloak` +0xe7 or `has_shield` +0xe9.
Exits: +0xe7 = 0 and +0xe9 = 0 [BYTES seg4:0x79bd-0x79ce]; +0xe9 ≠ 0 with wing
byte +0xac = 0 [BYTES seg4:0x79d7-0x79e8]; +0xe7 ≠ 0 with fuel +0x1e < 100 [BYTES seg4:0x79f0-0x7a01].
u [bp−0x10] = 1 when +0x10 = 0 and +0xe8 = 0 [BYTES seg4:0x7a05-0x7a1f]. `mov dword [bp−6],
0xffff` gives h = −1 and the loop counter j [bp−4] = 0 [BYTES seg4:0x7a24]; h = the last j ≤
DGROUP:0x9316 in play, j ≠ i, with goal (+0x32, +0x34, +0x38) = (14, i, 1) and seg8:0x285a < 500
[BYTES seg4:0x7a2e-0x7a92]. Then, in this order: the capture block (u = 0, +0x38 = 2, goal object
within 150 cells: toggle 0x408 / 0x409, b = 0x410, falling through) [BYTES seg4:0x7a94-0x7b13];
the exit u ∧ h = −1 ∧ team ≠ 3 [BYTES seg4:0x7b18-0x7b34]; ¬u ∧ h = −1: rand() % 3 = 0 and team
≠ 3 → 0x408 / 0x409, b = 0x411 [BYTES seg4:0x7b38-0x7b95]; u ∧ h ≠ −1: r = rand() % 5, (r = 0 or
fight +0x13e + h < −1) and fight < 2 → 0x406 / 0x407, b = 0x412 [BYTES seg4:0x7b98-0x7c03]; u ∧
team 3 ∧ h = −1 ∧ +0x38 = 1 ∧ +0x32 = 14: rand() % 4 ≤ 2 and the byte +0x13d (h = −1) < 2 →
0x406 / 0x407, b = 0x413 [BYTES seg4:0x7c06-0x7c7f]; ¬u ∧ h = −1 ∧ +0x38 ≠ 1 → 0x408 / 0x409, b =
0x414 [BYTES seg4:0x7c82-0x7ccc]; ¬u ∧ fight > 2 → 0x408 / 0x409, b = 0x415
[BYTES seg4:0x7cce-0x7d10]. Each toggle is seg11:0x4b00(i) when +0xe9 ≠ 0, else seg11:0x4c72(i),
and a = the id only when the call returns non-zero. The notice seg3:0x332e(dword [bp−0xe]) = (a
<< 16) | b only for i = DGROUP:0x9646 with a ≠ 0 [BYTES seg4:0x7d15-0x7d27]. rand() is drawn only
in the three branches named, each only when its own state test passes. The byte +0x13d is read for h = −1, and the capture block falls through to the tests after it.

### seg13:0x6900(i), the order executor (Pascal, `retf 2`) [BYTES seg13:0x6900-0x78b4]
- Head [BYTES seg13:0x690f-0x693d]: +0x94 = now + 4000 (`add ax, 0xfa0; adc dx, 0`); team word
  DGROUP:0x970a + 0xad·(+0x4e) = 7 → seg13:0xa6bc(i) and out.
- Team 6 [BYTES seg13:0x6940-0x69e0]: when +0x58 ≠ i and not (i = DGROUP:0x9646 with DGROUP:0x95bc
  ≠ 0): +0x52 = 0, seg3:0x6ec(+0x58, 14, i, 0x83), +0x94 = now, +0xed = 0; then only for i = the
  player with DGROUP:0x95bc = 0 the text seg10:0xa332(DGROUP:0x8630, 1421, 1, +0x58, [bp−0x46], 5)
  (string 1421 "I Can't Believe %D Thinks I'll Help Him!"), DGROUP:0x58c = 1, dword DGROUP:0x5c0 =
  now + 700.
- Order 1 [BYTES seg13:0x69e3-0x6bc3]: k = seg13:0x78b8(i, +0x56). k ≥ 0: +0x06 = 14, +0x08 = k,
  +0x36 = 1, +0x3a = 2; out unless +0xed = 1; +0xed = 0; seg13:0x2714(+0x56, i, 20, 60, 1, 8);
  seg3:0x6ec(+0x56, 14, i, 1); team 4: the inner block needs team ≠ 4 again
  [BYTES seg13:0x6a8f-0x6a96] and never runs, so message 0x80 to k; then seg13:0x2714(i, k, 0,
  −140, 0, 3); other teams message 0x81 to k; then unless +0x52 = 4, seg4:0x8346(i, 14, k, 14).
  k < −1: seg3:0x6ec(+0x56, 14, i, 0xad − k), +0x52 = 0. k = −1: +0x52 = 5, +0x94 = now + 200,
  message 1 to +0x58 and to +0x56, +0xed = 0. The word [bp−0xe] = 20000 written at seg13:0x69f1
  is never read in this branch.
- Orders 2-4 [BYTES seg13:0x6bc6-0x6da6]: including the stale
  [bp−0x1a] for a non-ship target (read at seg13:0x6c70, the refusal there also needs +0x54 = 14 at
  seg13:0x6c8d: no effect), the sound seg10:0x754a(0x3f, 0, 0, 0) (cdecl, 4 words) for a ship that
  ordered itself against the player, the refusal's 20·sbyte(+0x5d) + 15 (`imul byte`, 8 x 8 → 16)
  against seg4:0x5d9c(i, +0x56), messages 0x8a / 0x73 / 0x13, the fight byte +0x13e + t = 0 and the
  grudge byte +0xc2 + t = 0xff, and seg13:0x78a1's +0x52 = 0 at the end of both refusals.
- Order 5 [BYTES seg13:0x6da9-0x6f82]: est approach 3 under 0x15e cells else 2; +0x94 = now + 700;
  the give-up when ship(+0x56) +0x26 > 40 and +0x98 < now (unsigned) and not (player, PlayMode ≠
  0, demo ≠ 0); +0xcd (cbw) = +0x56 and i ≠ player → order 10, +0x94 = now + 500; a dead target
  (+0xec = 0) → order 0, now + 500; seg13:0x78b8 k > −1 → +0x08 = k (+0x06 kept), act 1, app 2,
  order 1, +0xed = 1, now + 700; k < −1 → message 0xad − k to +0x56, order 0 (or 10), now + 500;
  k = −1 → nothing more.
- Order 7 [BYTES seg13:0x6f85-0x7128]: the pool word [bp−6] = 14 is never changed, so the loop is
  over ship records k < DGROUP:0x9406 + 2·14 (in use), skipping i, state ≠ 0 and mass +0x44 = 0;
  seg13:0x2688(k, i) < 100 takes 0x96 off; strict `best > d`; seg11:0x5ab6(i, 5) (Pascal) when
  |vx| or |vy| of ship br > 30 at 300 cells or more; act 5 when nothing was found; the order ends
  when +0x98 < now unless i is the player with DGROUP:0x95bc ≠ 0.
- Order 9 [BYTES seg13:0x712b-0x727b], order 10 [BYTES seg13:0x727e-0x732c] (the message 0xb7
  to the player at every new best gate record, k = 0, 2, … < 2·DGROUP:0x9318, pool-12 pointer
  DGROUP:0x1e28), order 0x0c [BYTES seg13:0x732e-0x73fb] (pool-4 count DGROUP:0x940e, message
  0x38 then +0x52 = 0 through seg13:0x6b69), order 0x0d [BYTES seg13:0x73fe-0x776f] (the defender
  words DGROUP:0x52fe / 0x5308 / 0x530a + 0x24k = 15 / 14 / i over DGROUP:0x940a records; dword
  stores 0x50005 at +0x3a and 0x40004 at +0x36; seg4:0x8346(i, bp, br, 15) through seg13:0x6b41),
  order 0x0e [BYTES seg13:0x7772-0x784b] (drop: +0x52 = 0, dword +0x98 = now and, through
  seg13:0x6edb, +0x94 = dx:ax = now), the default [BYTES seg13:0x784e-0x78a6].

### seg4:0x8346(i, pool, rec, kind) and seg13:0x9a18(i, pool, rec) [BYTES seg4:0x8346-0x83ac seg13:0x9a18-0x9ae6]
[BYTES seg4:0x8346-0x83ac]: cdecl; kind 3 with +0x38 = 1, +0x32 = pool, +0x34 = rec
becomes sbyte +0x146 (cbw); seg13:0x9a18 unless i = DGROUP:0x9646 with DGROUP:0x95bc ≠ 0; byte
+0x146 = lo8(kind); AX = 1.
seg13:0x9a18 ends in a plain `retf` (cdecl; seg4:0x8346 leaves the 6 bytes to its own `lea sp,
[bp−2]`), arguments i [bp+6], pool [bp+8], rec [bp+0xa] **CONFIRMED** [BYTES seg13:0x9a18-0x9ae6]:
out when rec < 0, rec ≥ DGROUP:0x9406 + 2·pool, or the object's state ≠ 0; for each defender k <
DGROUP:0x940a with +0x06 = 14, +0x08 = i, not already (+0x0a, +0x0c, +0x00) = (pool, rec, 15), and
pool-2 record k in play: +0x00 = 15, +0x0a = pool, +0x0c = rec, **word DGROUP:0x52f6 + 0x24k = 0**
[BYTES seg13:0x9ab1], last = k; then seg10:0x754a(0x15, 2, last, 1) once.

### seg4:0x702e(i), the revenge recall (cdecl) [BYTES seg4:0x702e-0x7633]
- Entry [BYTES seg4:0x703d-0x7107]: dword [bp−0x4a] = 0 clears both the free flag [bp−0x4a] and
  found [bp−0x48]; out (AX = 0) when byte +0xee + P = 0 or word +0x72 + 2P = 0 (P = DGROUP:0x9646),
  DGROUP:0x92e8 ≠ 0, DGROUP:0x9320 = 7 or 6; word +0x00 ≠ 0 and < 4 is decremented; out for i = P
  with DGROUP:0x95bc = 0; out when not on screen (+0x1a); word +0x00 ≠ 0: message 0x88 to P when it
  is 4, decrement, out; out when +0x26 < 45, team 4, or DGROUP:0x934e < 7.
- DGROUP:0x934e is the wave clock in whole seconds, lo16(frame# / 36) (globals.md), so "DGROUP:0x934e < 7" is the first 7 s of the wave [BYTES seg11:0x1f0e-0x1f1e].
- The loop j < DGROUP:0x9316 (`jge`, the last ship excluded) [BYTES seg4:0x75ac-0x75b5], on screen,
  j ≠ i, word +0x72 + 2j > 5, i not team 7; another team, or memory byte > 8 or = 4 → the taunt;
  otherwise byte +0xee + j = 0, word +0x72 + 2j = 0, AX = 0 [BYTES seg4:0x75b9-0x7627].
- The taunt [BYTES seg4:0x7114-0x75aa]: k = (m ≤ 5); one rand() % 3 (seg4:0x7128 or seg4:0x718c);
  0 → lstrcpy(name, DGROUP:0x380c); else by +0x5e (k = 1: 0 → 668, < 3 → 669, = 3 → 670) or +0x5d
  (k = 0: 0 → 671, < 2 → 672, = 3 → 673), LoadString(DGROUP:0x9212, id, name, 40), any other value
  leaves the name unwritten; for m = 4 all three killer slots n are tested (no break), each match
  sets found = 1, loads MASTER.SHP once through seg14:0x3eda when dword DGROUP:0x6e20 = 0 (flag
  [bp−0x4a] = 1), copies far DGROUP:0x6e20 + lo16(0x41·DGROUP:0x6b10[3t + n] + 0x0a) and ago =
  DGROUP:0x91f8 − DGROUP:0x4fd0[3t + n]; remember = (rand() & 1) = 0 only when found = 0; hey =
  (rand() & 1) = 0; seg10:0xa332(DGROUP:0x8d38, 3033, 1, hey, name, remember, found = 0, 849 + m,
  found, killed, ago, 2518) (cdecl, 15 words); seg14:0x4074 when the flag and found are set;
  message 0x4f + k to j. k = 1: +0x72 + 2j < 9 or team 2 → order 0, goal and estimate (14, j),
  +0x94 = now + 8000, dword +0x36 = 0x10001, +0x3a = 0x20002; team 2 loops n ≤ DGROUP:0x9316
  (inclusive): team-2 n ≠ j (i itself included) gets +0x36 = 0x30003, +0x3a = 0x20002, order 0,
  goal and estimate (14, j), now + 15000; a team-0 n (never i) with i's memory of j = 3 or 1 gets
  order 2, +0x98 = now + 20000, (14, j) by i, +0xed = 1, then seg3:0xf4(i, i +0x54, i +0x56),
  seg4:0x8346(n, 14, j, 8), n's +0xee + j = 3, +0x72 + 2j = 0; other teams seg4:0x8346(i, 14, j, 8).
  Otherwise order 2 / 3 / 4 for the word < 14 / < 18 / else, seg4:0x8346(i, 14, j, 8), +0x98 = now
  + 20000, (14, j) by i, +0xed = 1, seg3:0xf4(i, 14, j). A memory of 4 (re-read) clears the team's
  killer slots naming j's +0xd6. Always +0x72 + 2j = 0 and AX = 1.

## The crime path and the arrest

The crimes, the crime routine seg13:0xb6f2, the Enforcers' executor seg13:0xa6bc (patrol, warning, arrest, resisting, the terminate order), the warrant seg13:0xb93a and the Hall of Justice are in [law.md](law.md). What stays here is the call for an Interceptor and the attack reaction, which every ship uses.

### The call for an Interceptor: seg13:0xadbe(s, 0x15, msg) → seg13:0xae68
seg13:0xadbe(s, kind, msg) (cdecl) [BYTES seg13:0xadbe-0xae67]: r = seg13:0xae68(s, kind, msg); r ≠ 0 plays the tension music seg10:0x7480(m, 0) with m = 2 for kinds 0x15/0x16, 7 for kinds 3, 0x13, 0x14 and kind 4 while the player hunts s, 8 for kind 4 with s = player, and sets DGROUP:0x2384 = now + 12000 (the music-change deadline of the frame's step 22); the return value is AX of seg13:0xae68, which the callers test [BYTES seg13:0xadd9 seg13:0xae28-0xae5d]. seg13:0xae68(s, kind, msg) (cdecl) [BYTES seg13:0xae68-0xb6ef], for kind 0x15 (team 7, class 1, order 0x16):
1. Refused (0) in kind-6/7 waves unless the kind is 0x15 and byte DGROUP:0xa163 = 8; refused for other kinds once DGROUP:0x964c (ships called this wave) > 2·`Detail` + 1; refused for all when DGROUP:0x9316 > `Detail` + 3; Demon's Gate allows only kind 0x13 [BYTES seg13:0xae7c-0xaec5].
2. **A team-7 ship off-world** (object `state` ≠ 0) of the class: brought in at once (first found, ascending): sound 0x70 for class < 2 (seg10:0x744c), seg16:0x3784(k, 1), one `rand()` % DGROUP:0x9318 picks gate a, the ship's (x, y) = pool-8 record 2a's, x += DGROUP:0x6682 + 0x1c·a / 2 − (size/2 − post size), `state` 0, seg10:0x754a(0x27, 14, k, 1), seg12:0x2556(14, k), seg7:0x13b2(2, 14, k), then the order words (step 4); return 1 [BYTES seg13:0xb075-0xb1ea].
3. **A team-7 ship in play** of the class, alive, with byte +0xcd −1 or s: unless it is already on s with +0x52 > 0x14 (return 0), message 0x3f (554 "I Must Pursue Someone Else. Try To Be A Better Citizen.") to its previous charge +0x56, then the order words; return 1. One in play but busy or dead makes the call fail (0) [BYTES seg13:0xb1ed-0xb2f6].
4. **The order words.** On an off-world ship (seg13:0xb198-0xb1e3): +0x52 = 0x16, +0x54 = 14, +0x56 = s, +0x32 = 14, +0x34 = s, +0x94 = now, byte +0xed = 1, +0x58 = k (the ship itself), +0x98 = now + 5000, +0x38 = 3 and byte +0xcd = lo8(s). On a ship in play (seg13:0xb2a5-0xb2da): only +0x52, +0x54 = 14, +0x56 = s, +0xed = 1, +0x58 = k, +0x94 = now, +0x38 = 3 and +0xcd = lo8(s); its goal words +0x32/+0x34 and +0x98 are not written. A new ship (step 5) gets the off-world set plus design word +0x42 (captures icons) = 0 and +0x98 = now + 5000 [BYTES seg13:0xb198-0xb1e3 seg13:0xb2a5-0xb2f6].
5. **A new ship** when none was found: seg14:0x3eda(), the siren 0x70 (seg10:0x744c for the player, positional otherwise), seg3:0x6ec(s, 7, 0, msg) when msg ≠ −1, seg11:0x238e(DGROUP:0xa7c, 0), seg14:0x0(0x15) (creates ship DGROUP:0x9316 + 1, [ships.md](ships.md#the-waves-ships-seg140x0)), DGROUP:0x992 += 10, seg14:0x4074(), the object's `src_y` = `gap`, byte +0x2e = 0, +0x32 = +0x36 = −1, one `rand()` % DGROUP:0x9318 for the gate (outside Demon's Gate), `state` 0, the entry sound and effect calls of step 2, the order words, design word +0x42 (captures icons) = 0, DGROUP:0x964c += 1, +0x98 = now + 5000, DGROUP:0x8e2c = frame#; return 2 [BYTES seg13:0xb3f2-0xb6e3].

### seg4:0x5e8e's early returns (the attack reaction) [BYTES seg4:0x5e8e-0x6356]
The reaction runs from the ship damage handler seg8:0x45e0 (kind 1, gun and weapon hits), the hit dispatcher (kind 2?) and the rivals seg13:0x3008 [BYTES seg8:0x47e4 seg8:0x61f9 seg13:0x3127 seg13:0x318f]. Before the Enforcer branch: amount 0x5a becomes 0x32; seg13:0x2714(v, a, −(amount/4 + 4), −8 − amount, 0, −1); a player attack (kind 1) on a ship it is not hunting with grudge v+0xc2+player = 0 returns unless amount ≥ 33 and v's word +0x46 is within 80 of frame#/10; nothing in Demon's Gate or a duel; kind 1 with v's byte +0xf7 + 9a < 9: seg13:0x2714(v, a, −8, −10 − amount, 0, 1); a defending v (+0x38 = 4) handles the law and returns: unless T(7, team v) ≤ −100 or T(7, team a) ≥ 100, an amount above 10 draws one `rand()` (seg4:0x5fbc) and `rand()` % 4 ≤ 1 with v's Engine +0x26 < 40 calls seg13:0xb6f2(a, 3); otherwise only the **first** team-7 ship j < DGROUP:0x9316 sends seg3:0x6ec(a, 14, j, 0x5c), because the code after that call falls into the loop's exit seg4:0x6018; every path then calls seg13:0x2714(v, a, −13, −amount, 0, kind) [BYTES seg4:0x5f7b-0x6036]; v's Engine < 41 returns unless kind 1; kind 2 returns for teammates, order 9 or attitude > 100 (> 130 later); a = v returns; a v commanded against a returns; grudge > 19 resets to 0 and returns; a v hunting a returns unless v is an Enforcer; v nearly out of fuel (fuel < tank/10) with a = player: message 0xbe, and at grudge 4 the law (seg13:0xb6f2(a, 3) when T(7, team v) > −100 and T(7, team a) < 100), else sound seg10:0x744c; grudge = 4; return. Then seg13:0x2714(v, a, −(amount/2 + 7), −10 − amount, 1, kind) unless kind 1 with byte +0xf7 + 9a ≤ 8; the player v in Action mode returns; v of class > 1 returns when seg4:0x5d9c(v, a) < 15 or > 700 (not for Enforcers); the hit weight w = 0 … 8 from the damage to v's Engine, the amount, the attitude (seg13:0x2688) and the class [BYTES seg4:0x62b6-0x633a]. The crime sites of the Enforcer branch are in [law.md](law.md); the other ships' answers follow below.

### The answers of the other ships in seg4:0x5e8e (seg4:0x6034-0x66e9)
The branches past the fleeing one, in order: the head (d = 90 → 50, seg13:0x2714(v, a, −(d/4 + 4), −8 − d, 0,
−1), the player's kind-1 gate, the Demon's Gate / duel exit, the kind-1 hurt term) [BYTES
seg4:0x5e9c-0x5f78]; the exits and counts of seg4:0x6034-0x6136 (engine < 41 unless kind 1; kind 2:
same team, +0x52 = 9 or seg13:0x2688(v, a) > 100; a = v; v under a's command; kind 2 and rel > 130,
never true after the first test; team 5 or the same team, team 4; grudge ≥ 20 → 0 and out; v
hunting a unless team 7); the low-fuel plea (design +0x1e / 10 > fuel, a the player: message 0xbe;
grudge 4: d <<= 1 and crime 3 for a within the team words, else seg10:0x744c(0x3d); grudge = 4)
[BYTES seg4:0x613a-0x61cb]; seg13:0x2714(v, a, −(d/2 + 7), −10 − d, 1, kind) unless kind 1 with hurt ≤ 8;
the player's exit (v = P with PlayMode = 0 or demo ≠ 0); seg4:0x5d9c(v, a) and its exit for +0x5e >
1 with s < 15 or > 700 and v not team 7; cnt for s < 30 or > 300; the first-hurt sound and the grudge
`inc byte`; DGROUP:0x9432 += d/2; the weight from +0x26 − d / design +0x10 (< 65, < 45), d > 24,
d > 36, d = 90 (never: step 1), rel < −100, < −210 [BYTES seg4:0x61ce-0x6337]; the Enforcer branch
(no Interceptor in play → message 0x3b; dword +0x52 = 0x000e0015, +0x56 = a, +0xed = 1, +0x94 = now,
DGROUP:0x964a += 1, DGROUP:0x9648 += 3; grudge ≥ 2: seg13:0xadbe(a, 0x15, 0x3c) ≠ 0 → byte
DGROUP:0xa163 = 5, else message 0x43; for the player byte DGROUP:0xa152 + 1 while DGROUP:0xa152 − 1 <
DGROUP:0xa15c; +3 / +3; order 0x14; all grudge bytes 0; Interceptor / Terminator: order 0x3e, +0xed
= 2, +0x98 = now + 20000, (14, a) when grudge > 2 else message 0x41, skipped when +0x54 = 14 and
(+0x56 = a or +0x5e ≠ 1)) [BYTES seg4:0x633a-0x64ea]; the rel ≤ 0 branch (lim = +0x5e + cnt + 1,
messages 0x2d / 0x2e, 0x2f with sound 0x3e when +0x5e − g + cnt = −2 for the player) [BYTES
seg4:0x65f5-0x6660]; the FUZZY ONES' hand-over to the last team-0 ship j ≤ DGROUP:0x9316, j ≠ P, alive,
in play, engine > 40, j ≠ a, which rewrites [bp+6] [BYTES seg4:0x6583-0x65f2 seg4:0x6663-0x66ce];
seg3:0xf4(victim, 14, a), seg13:0x915c(victim); the grudge step on the rewritten [bp+6]
[BYTES seg4:0x66d0-0x66f4].

**The rel > 0 branch reads the allies byte +0x5d, not +0xed.** [BYTES seg4:0x64fa seg4:0x654e]: `mov al, byte ptr [bx + 0x70ed]` with bx = 0x15a·v, which is ship field +0x5d (the allies byte, DGROUP:0x70ed + 0x15a·v), not +0xed (`one_shot`, DGROUP:0x717d). lim = 4 + cnt − sbyte v +0x5d; lim ≥ sbyte g → (d ≠ 90) message 0x2a (g > 1) or 0x29, then cnt − g − sbyte v +0x5d = −5 with a the player → message 0x2b and seg10:0x754a(0x3e, 0, 0, 0); lim < g → the hand-over and the turn [BYTES seg4:0x64ed-0x6583]. Message 0x2b and its sound never come: on that branch lim ≥ g gives cnt − g − sbyte v +0x5d ≥ −4, so the test against −5 never holds [BYTES seg4:0x64ed-0x6560].

## The player's commands to ships

The Ctrl, Alt and Strategy keys of seg11:0x6b64 and the routines they call, the ships' reactions that call the same routines, and the reply strings. [MANUAL Dealing With Other Ships]

### The key decode (what reaches which routine)

seg11:0x6b64 reads Ctrl as GetKeyState(VK_CONTROL) bit 15 into [bp−0x1c] and Alt as
GetKeyState(VK_MENU) bit 15 into [bp−0x1a]; private message 0x99e forces Ctrl = 1 and 0x99b
forces Alt = 1 (the menu items), both then decoded as WM_KEYDOWN [BYTES seg11:0x6cd5-0x6d16
seg11:0x7288-0x729e]. The gate of world.md "Key messages" applies first (no keys while the
player's dword +0x84 > now or object `state` ≠ 0, except KEYUP 'P' and KEYDOWN 'H'; the demo
takes only +, −, W). The WM_KEYDOWN letter handlers [BYTES seg11:0x6f20-0x7130]:

| Key | Ctrl (to other ships) | Alt (to own defenders) | neither (Strategy, F12) |
|---|---|---|---|
| A seg11:0x7065 | seg13:0x8570(P) "attack my target" | seg13:0x9a18(P, P+0x3e, P+0x40) [push dword +0x3e] | seg13:0xa186() (Ctrl and Alt clear) |
| C seg11:0x7019 | seg13:0x9ce0(P) "capture my target" | — | seg13:0xa234() |
| D seg11:0x6f20 | seg13:0x8e98(P) "defend my target" | — (Alt+D falls to seg13:0xa4f4) | seg13:0xa4f4() |
| E seg11:0x70a5 | seg13:0x9f3e(P) "exit the wave" | the inline loop seg11:0x70bb-0x7113 | seg13:0xa3c8() |
| F seg11:0x6feb | seg13:0x915c(P) "challenge to a fight" | — | nothing |
| G seg11:0x6fff | seg13:0xa01c(P) "get gas" | — | seg13:0xa47c() |
| H seg11:0x6f7a | seg13:0x81c2(P) "help me" | seg13:0x9ae8(P) "defenders return" | seg16:0xb4e(P), ambulance (KEY-07), in a player game only |
| S seg11:0x7033 | seg13:0x9810(P) "self-preserve"; Ctrl+Alt+S seg13:0x4ac4(P) | — (Alt+S falls to seg13:0xa568) | seg13:0xa568() |
| W seg11:0x7127 | seg13:0x563c(ctrl, alt) for every modifier (push dword [bp−0x1c]) | | |
| X seg11:0x6f3a | Ctrl+Alt+X: seg16:0x8e4(DGROUP:0x9324, P), self destruct | | |

- A: seg13:0x8570 runs with Ctrl, seg13:0x9a18 with Alt; the Strategy call seg13:0xa186 needs both Ctrl and Alt clear. E likewise: seg13:0xa3c8 only with both clear (Alt+E takes the defender loop). C, D, G and S call seg13:0xa234, 0xa4f4, 0xa47c and 0xa568 whenever Ctrl is up, with or without Alt [BYTES seg11:0x7065-0x70a2 seg11:0x70b5-0x711f]. Ctrl+Alt+A runs seg13:0x8570(P) and then seg13:0x9a18; Ctrl+Alt+H runs seg13:0x81c2(P) and then seg13:0x9ae8(P) [BYTES seg11:0x7065-0x707b seg11:0x6f7a-0x6f94]. H with Ctrl+Alt runs both
  seg13:0x81c2 and seg13:0x9ae8 [BYTES seg11:0x6f7a-0x6f99]. E with Ctrl runs seg13:0x9f3e and
  then, with Alt also held, the defenders' exit [BYTES seg11:0x70a5-0x70b9].
- Every Ctrl handler passes the player and ignores its return value. The Strategy routines
  (seg13:0xa186 … 0xa568) test PlayMode ≠ 0 themselves and return at once in Action mode.
- No key asks a ship for an activity report: the decode has no other command letter, and the
  manual's "report on his current activity" (AI-03) is the watch's text (the activity text in [shell.md](shell.md)) [BYTES
  seg11:0x6e42-0x6ee4] [MANUAL Dealing With Other Ships].
- **Alt+E, the defenders leave** [BYTES seg11:0x70bb-0x7113]: last = −1; for k = 0 … DGROUP:0x940a
  − 1 whose defender entry owner (DGROUP:0x5304, 0x5306) + 0x24k = (DGROUP:0x9324, P):
  seg8:0x9028(k, 15, 12, 1) (cdecl; attack pool-12 record 1, the way out) and last = k; last ≥ 0
  plays seg10:0x754a(0x15, 2, last, 1). (The owner test uses the word DGROUP:0x9324, not 14.)

### The command routines

Common words. +0x52 order, +0x54/+0x56 the order's object, +0x58 who gave it, byte +0xed the
one-shot (1 = acknowledge on the next executor run),
dword +0x94 the re-plan time, dword +0x98 the order's deadline, dword +0x9c **the talk cooldown**
(the next time this ship may ask or command; these routines, seg13:0x7ba2 and the think's
re-plan, which turns a value of 1 into now + 5000 at seg4:0x548, write it),
dword +0xa4 the goal-change time. "Busy" below always means: +0x52 ∉ {0, 7} and dword +0x98 not
yet passed (the exact comparison is given per site, they differ: `jb`, `jae`, `jbe`, `ja`).
"Free" = not busy. "In play" = object `state` +0x18 = 0 (pool-14 pointer DGROUP:0x1e30); "alive"
= byte +0xec (DGROUP:0x717c) ≠ 0.

The **cooldown test** that opens seg13:0x81c2, 0x8570, 0x8e98, 0x9ce0 reads: when c ≠ P or
PlayMode ≠ 0, dword c+0x9c > now (high word compared first, `ja`/`jbe` on the low) refuses. The
player in Action mode is never refused by it. seg13:0x9810 tests only c ≠ P; seg13:0x915c writes
the cooldown before its test (below).

#### seg13:0x81c2(c), "help me" (Ctrl+H; also the reactions and the challenge) [BYTES seg13:0x81c2-0x856e]
cdecl, returns AX: −1 (cooldown), 1 (a helper took it), 0 (none).
1. The cooldown test; −1 on refusal [BYTES seg13:0x81d5-0x8201].
2. c+0x9c = now + 5000 (32-bit, `add 0x1388; adc 0`); seg13:0x4eec(c, 15) [BYTES seg13:0x8204-0x8228].
3. For j = 0 … DGROUP:0x9316 (signed `jg` exit) [BYTES seg13:0x8346-0x84b9]:
   - skip: not in play, not alive, j = c;
   - skip unless free (+0x52 = 0 or 7, or dword +0x98 < now `jb`) or +0x58 = c, or (a player game
     and c = P) — so the player's call reaches busy ships too;
   - v = seg13:0x2688(c, j) [c's view of j] + AX(−100 ⊗ sbyte j+0x5d) (`mov al,0x9c; imul byte`);
     j's Engine word +0x26 < 40: v −= 100; team(j) = team(c): v += 100; (j's goal (+0x32,
     +0x34) = (14, c) with +0x38 = 1) or (c's goal = (14, j) with +0x38 = 1): v −= 800; team(j)
     = 0 and team(c) = 2: v += 350; c's fuel +0x1e < design word DGROUP:0x96f0 (tank) of c / 10
     (16-bit signed): v −= 1000 [BYTES seg13:0x83a3-0x8473];
   - v ≤ −150 (signed, `cmp 0xff6a; jle`) → next j;
   - seg13:0x4eec(c, 20); c = P: seg13:0x4eec(j, 40) [BYTES seg13:0x8481-0x84a0];
   - **j = P in Strategy in a player game** (PlayMode ≠ 0, DGROUP:0x95bc ≠ 0): when now − 15000 >
     dword DGROUP:0x8858 (unsigned, `jbe` skips): the request record DGROUP:0x964e = 1, 0x9654 =
     c, 0x9652 = c, dword 0x8858 = now, 0x9650 = 14, and msg(P, 14, c, 0x34); then **next j**
     (the loop goes on) [BYTES seg13:0x84a3-0x8500];
   - otherwise (seg13:0x8233): team(j) = 7 → next j; j's +0x52 ≠ 0: msg(j+0x58, 14, c, 0x13);
     c = P with PlayMode = 1: seg3:0x33b4(1); j+0x94 = now + 200; j+0x52 = 1; j+0x54 = 14; j+0x56
     = c; dword j+0x98 = now + 15000; c+0x9c = the same dword; j+0x58 = c; seg3:0xf4(j, 14, c);
     byte j+0xed = 1; dword j+0xa4 = now − sext16(lo16(2000·sbyte j+0x5d)) + 8000 (`cwde; imul
     ax,ax,0x7d0; cdq`, then `sub; sbb; add 0x1f40; adc 0`); msg(j, 14, c, 2);
     seg13:0x2714(c, j, 15, c = P ? 15 : 0x23, 0, −1); **return 1** [BYTES seg13:0x8233-0x8343].
4. No helper [BYTES seg13:0x8503-0x8565] ([bp−0xa] is always 0 here): for c ≠ P, r = `rand()` %
   14 (16-bit signed); r ≥ 12 − `Detail` (DGROUP:0x9208): when DGROUP:0x992 + 20 ≥ DGROUP:0x932a
   return 0, else when seg13:0xadbe(c, 4, 0x68) ≠ 0 (a Knight called in) return 0; then (and for
   r < 12 − Detail) msg(c, 7, 0, 6) unless c = P in the demo; return 0. For c = P no `rand()`:
   msg(P, 7, 0, 6) in a player game.

#### seg13:0x8570(c), "attack my target" (Ctrl+A; also seg13:0x9970) [BYTES seg13:0x8570-0x8e95]
cdecl, returns −1, 0 or 1. (tp, tr) = c's target words +0x3e/+0x40 ([bp−8], [bp−0xa]).
1. The cooldown test (−1); c+0x9c = now + 10000 [BYTES seg13:0x857e-0x85c5].
2. Unless c = P in a player game: return 0 while the wave clock DGROUP:0x934e (seconds,
   seg11:0x1f1e) < 5; return 0 when P's goal pool +0x32 = tp and (tp = 0x16 or P's +0x34 = tr)
   (a computer ship does not ask for the player's own goal) [BYTES seg13:0x85c9-0x8621].
3. c ≠ P and tp = 9 (a virus): return 0 while dword DGROUP:0x8710 ≠ 0 and DGROUP:0x8710 + 1500 >
   frame#, or dword DGROUP:0x8714 ≠ 0 and DGROUP:0x8714 + 600 > frame# (DGROUP:0x8714 is the
   frame of the player's last virus attack goal, seg7:0xf4b); when any ship j < DGROUP:0x9316 (j ≠
   P, alive) hunts the player (goal (14, P), +0x38 = 1); when P's Engine +0x26 < 40
   [BYTES seg13:0x8623-0x86b1].
4. seg13:0x4eec(c, 4); best = 1000 ([bp−0x14]); for j = 0 … DGROUP:0x9316 [BYTES seg13:0x86b5-0x8b3d]:
   - r = 1000; s = seg4:0x5d9c(j, tr) + 20 ([bp−0xc]; tr is used as a ship index whatever tp is);
     a = seg13:0x2688(j, c) ([bp−0xe]); b = seg13:0x2688(j, tr) when tp = 14, else −400 ([bp−0x18]).
   - The first test that holds sets the reply code r (the reply strings below have every string) [BYTES seg13:0x86d4-0x8ab7]:
     1. j = c or j not in play → 0xa3;
     2. tp = 14 and tr = j → 0x9e;
     3. team(j) = 2 and tp = 14 → 0x93; team(j) = 7 → 0x96; team(j) = 6 → 0x95;
        team(j) = 4, team(c) ≠ 4 and tp = 14 → 0x94;
     4. a < −100 and team(j) ≠ team(c) → 0x9d (tp = 14) or 0xa2;
     5. b + 70 > a or b > 150 → 0x91;
     6. team(j) ≠ team(ship tr) and (sbyte j+0x5d ≥ 3, or a < 100 and sbyte j+0x5e ≥ 3) → 0x9a
        (no pool test: for a non-ship target ship tr's team is read);
     7. **tr = 14** (the record, not the pool: `cmp [bp−0xa], 0xe` at seg13:0x8821 and
        seg13:0x884f) and team(ship 14) = team(j) → 0x98; tr = 14, team(ship 14) = team(c), not
        (ship 14's +0x38 = 1 and goal (14, c)), team(j) ≠ 3 → 0x9b (team(j) = team(c)) or 0x92.
        Ship 14 lies past the eight-record table (DGROUP:0x837c + off): ADR 0016 site;
     8. tp = 0, object (0, tr) `capturable` byte +0x53 = 1 and word +0x1e = 0, team(j) ≠ 3 → 0x97;
     9. byte j+0xee + tr = 8 (DGROUP:0x717e) → 0x99;
     10. tr = 14 (again the record) and sbyte j+0xc2 + 14 = −1 → 0x90 (reads byte +0xd0 of j);
     11. AX(62 ⊗ sbyte j+0x5d) − (3a)/4 + 60 > s (3a/4 by `imul 3; cdq; and dx,3; add; sar 2`,
         toward zero) → 0x8f;
     12. j's fuel +0x1e < tank(j)/10 → 0x8b;
     13. (sext(j's resources +0xce) + dword j+0xdc) < sext(lo16((sbyte j+0x5e − 2)·50)) (32-bit
         signed compare), a < 200, Engine +0x26 < 50 and s < 80 → 0x9c;
     14. j's +0x38 = 1, goal pool 14, fight byte sbyte j+0x13e + (goal rec) > 0, not (goal pool
         = c — sic, `cmp [+0x70c2], c` at seg13:0x89e4 — with 2 ≤ +0x52 ≤ 4), and not (tp = 14 and
         j's goal rec = tr) → 0x8e;
     15. j busy (+0x52 ∉ {0, 7, 0x0d} and dword +0x98 > now) and +0x58 ≠ c: (+0x54, +0x56) ≠
         (14, c) → 0x8d; +0x52 = 1 or 5 → 0x9f; +0x52 ≥ 2 → (+0x58 = j → 0xa1, else 0xa0);
         (+0x52 < 2 and > 4 never holds, so 0xa0/0xa1 cover the rest);
     16. the cloak test seg13:0x8a9e-0x8ab7 (tp = 14 and ship tr cloaked, byte +0xe8 → 0x8c) runs
         only when r ≠ 1000, but its five sources (seg13:0x8a1e, 0x8a25, 0x8a2c, 0x8a37, 0x8a40)
         all arrive with r = 1000 (every code above jumps to seg13:0x8abc), so it is dead: 0x8c
         never comes from this routine.
   - The choice [BYTES seg13:0x8abc-0x8ae2 seg13:0x8b38]: r < best (signed) takes r (best = r,
     who = j). Else, with best > 0x90 and r < 0x9e, one `rand()` (seg13:0x8ad0): odd takes r.
     The draw happens only on that path. Then r = 1000 → **j obeys** (step 5); any other r →
     next j. So the first free, willing ship in index order obeys; refusals keep the lowest
     code, with coin flips among 0x91 … 0x9d.
5. **j obeys** [BYTES seg13:0x8b3f-0x8d8e]: seg13:0x4eec(j, 15); c = P: seg13:0x4eec(j, 25);
   seg13:0x4eec(c, 20); tp = 9: seg13:0x4eec(j, −10), seg13:0x4eec(c, −13) and, c ≠ P, dword
   DGROUP:0x8710 = frame#.
   - j = P in Strategy in a player game: when now − 15000 > DGROUP:0x8858: DGROUP:0x964e = 4,
     0x9650 = tp, 0x9652 = tr, 0x8858 = now, 0x9654 = c, msg(P, 14, c, 0x34); then **next j**.
   - Otherwise: j's +0x52 ≠ 0: msg(j+0x58, 14, c, 0x13); c = P: violence DGROUP:0x9432 += 120
     (tp = 14) or 30; j's +0x52 = 2 when sbyte j+0x5d ≥ 2, else 3 (the branch to 4 at
     seg13:0x8c60 is unreachable); seg4:0x8346(j, tp, tr, 9); j = P: +0x52 = 2; c = P with
     PlayMode = 1: seg3:0x33b4(2); dword j+0x94 = now + 400; j+0x54 = tp, +0x56 = tr, +0x58 = c;
     j+0x98 = now + 10000; c+0x9c = now + 15000; byte j+0xed = 1; j+0xa4 = now −
     sext16(lo16(2000·sbyte j+0x5e)) + 8000; msg(j, 14, c, 2); j ≠ P: seg3:0xf4(j, tp, tr);
     tp = 14: seg13:0x2714(tr, c, −10·o, −40·o, 0, 5) with o = j's +0x52; return 1.
6. Nobody obeyed [BYTES seg13:0x8d91-0x8e8c]:
   - c ≠ P (a computer ship asking), the player's +0x52 ∈ {0, 7}, PlayMode = 0, not (tp = 14 and
     tr = P), seg13:0x2688(c, P) > −100 and P's goal ≠ (14, c): **the player gets the order**:
     P's +0x52 = 2, +0x58 = c, dword +0x98 = now + 15000, c+0x9c = the same, +0x54 = tp, +0x56 = tr,
     seg4:0x8346(P, tp, tr, 9), msg(P, 14, c, 2) ("Please Attack %D"), tp = 9: DGROUP:0x8710 =
     frame#; return 1. seg4:0x59bc then tracks it and seg13:0x7b40 rewards it (pilots.md).
   - Otherwise the reply: best = 0xa3 → msg(c, 7, 0, 0xa3); else msg(c, 14, who, best); return 0.

#### seg13:0x8e98(c), "defend my target" (Ctrl+D) [BYTES seg13:0x8e98-0x9109]
cdecl, no meaningful return. (tp, tr) = c's +0x3e/+0x40.
1. tp ≠ 14 → return at once (only a ship can be defended) [BYTES seg13:0x8eac-0x8ec2].
2. The cooldown test, silent on refusal; c+0x9c = now + 5000 [BYTES seg13:0x8ec6-0x8f09].
3. seg13:0x910a(c) ≠ 0 (no ship j ≠ c in play and alive, j = 0 … DGROUP:0x9316
   [BYTES seg13:0x910a-0x915b]) → msg(c, 7, 0, 0x64); return [BYTES seg13:0x8f0d-0x8f1b].
4. For j = 0 … DGROUP:0x9316 [BYTES seg13:0x8fe2-0x9099]: skip team(j) = 6 (RENEGADES), not in
   play, not alive; skip busy ships (+0x52 ∉ {0, 7} and dword +0x98 ≥ now, `jb` admits) unless c
   = P; v = seg13:0x2688(j, tr) + AX(−50 ⊗ sbyte j+0x5d) (`mov al,0xce; imul byte`) +
   seg13:0x2688(j, c); skip v ≤ 0, j = c, j = tr.
   - j = P with PlayMode ≠ 0 in a player game: when now − 15000 > DGROUP:0x8858: DGROUP:0x964e =
     5, 0x9654 = c, 0x9650 = tp, 0x9652 = tr, 0x8858 = now and msg(P, 14, c, 0x34); **return**
     either way [BYTES seg13:0x9083-0x90d9].
   - Otherwise (seg13:0x8f26): team(j) = 7 → next j. j's +0x52 = 5; msg(j+0x58, 14, c, 0x13)
     (always, after the store); dword j+0x94 = now + 200; j+0x98 = now + 20000; c+0x9c = now +
     15000; j+0x54 = tp, +0x56 = tr; byte j+0xed = 1; msg(j, 14, c, 2); j+0x58 = c;
     seg13:0x2714(tr, c, 15, 50, 1, −1); return [BYTES seg13:0x8f26-0x8fdf].
5. No ship: return for c = P in the demo, else msg(c, 7, 0, 5) [BYTES seg13:0x90db-0x90f9].
Order 5 is the executor's "follow": the ship follows and
defends its +0x56.

#### seg13:0x915c(c), "challenge my target" (Ctrl+F; also seg4:0x66e3) [BYTES seg13:0x915c-0x980f]
cdecl. t = c's +0x40. q = 0 ([bp−0x16], the acceptance), r = 0 ([bp−0x18], the mismatch).
1. c+0x9c = now + 5000 **first** [BYTES seg13:0x9183-0x9195]; tp ≠ 14 → msg(c, 7, 0, 0x14);
   team(t) = 7 → msg(c, 14, t, 0x40); t's fuel +0x1e < tank(t)/10 → msg(c, 14, t, 0xbc); t's
   +0x52 = 1 → msg(c, 14, t, 0xbd); each returns [BYTES seg13:0x9199-0x91ee].
2. Unless c = P with (PlayMode = 0 or a player game, DGROUP:0x95bc ≠ 0): the cooldown test
   (always true, the store of step 1 just made +0x9c = now + 5000) leads to: grudge sbyte c+0xc2
   + t < 4 → return. So a computer ship (or the player in the demo with DGROUP:0x9134 ≠ 0)
   challenges only a ship that has hurt it 4 times [BYTES seg13:0x91f1-0x922b].
3. s = seg4:0x5d9c(c, t) ([bp−0x14]). s > 500 or s < 20: when sbyte t+0x5e > 0 and t ≠ P: s > 500
   → msg(c, 14, t, 0x4c) and seg13:0x81c2(t) (t calls for help), return; s < 20 → msg(c, 14, t,
   0x4e), return. Otherwise r = 1 (s < 20) or 2 (s > 500) [BYTES seg13:0x922f-0x929e].
4. Both already commanded against each other by themselves (t's +0x52 2…4, (+0x54, +0x56) = (14,
   c), +0x58 = t, and the same for c toward t) → msg(c, 14, t, 0x74); return [BYTES seg13:0x92a3-0x92fa].
5. t not in play or not alive → return. d = seg4:0x5d9c(c, t); e = AX(30 ⊗ sbyte t+0x5e); a =
   seg13:0x2688(t, c); w = AX(30 ⊗ g) when g = sbyte c+0xc2 + t > 2, else 0 [BYTES seg13:0x92fd-0x9363].
   - a > 50: w += AX(20 ⊗ sbyte t+0x5d) + a/(−5) − 2·|100 − d| (seg9:0x88ba = abs) + AX(15 ⊗ g);
     g = −1: w −= 100. r ≠ 0 → q = 2. Else w > −70 and (t free (+0x52 0 or 7, or dword +0x98 <
     now) or t = P) → q = 2. Else (decline) unless c = P in the demo: msg(c, 14, t, 0x10) when t
     is busy (+0x52 ≠ 0 and +0x98 > now), else msg(c, 14, t, 0x0e) [BYTES seg13:0x9366-0x9420].
   - a ≤ 50: w = −110 when t is busy (+0x52 ∉ {0,7} and +0x98 > now), else −30; w += AX(25 ⊗ g)
     − a/5 − e + d; g = −1: w −= 140. r ≠ 0 or w > −90 → q = 1. Else unless c = P in the demo:
     msg(c, 14, t, 0x10) (busy) or 0x0f [BYTES seg13:0x9423-0x94cc].
6. q = 0: byte c+0xc2 + t = 0xff; return [BYTES seg13:0x94cf-0x94e2].
7. Accepted [BYTES seg13:0x94e5-0x975b]: byte c+0xc2+t = 0x15, byte t+0xc2+c = 0x14; t's +0x52 ≠ 0:
   msg(t+0x58, 14, c, 0x13); c's +0x52 ≠ 0: msg(c+0x58, 14, c, 0x13).
   - t = P in Strategy in a player game: when now − 15000 > DGROUP:0x8858: DGROUP:0x964e = 0x0b,
     0x9650 = 14, 0x9654 = c, 0x9652 = c, 0x8858 = now; then straight to step 8.
   - Otherwise t: +0x52 = 2, dword +0x94 = now + 200, +0x98 = now + 40000, +0x9c = now + 8000,
     +0x54 = 14, +0x56 = c, +0x58 = t, byte +0xed = 1, goal +0x32 = 14, +0x34 = c, +0x38 = 1, +0xa4
     = now − sext16(lo16(2000·sbyte t+0x5e)) + 8000; seg4:0x8346(t, 14, c, 0x15); t = P:
     DGROUP:0xa62 = 1 (manual targeting on). c: +0x52 = 2, +0x94 = now + 200, +0x98 = now + 40000,
     +0x9c = now + 8000, +0x54 = 14, +0x56 = t, +0x58 = c, byte +0xed = 0, +0x38 = 1; c ≠ P:
     seg3:0xf4(c, 14, t); +0x32 = 14, +0x34 = t, +0xa4 as for t with c's byte; q = 1 and team(c) ∈
     {4, 0}: c's +0x52 = 3; seg4:0x8346(c, 14, t, 0x14); outside duels (DGROUP:0x9320 ≠ 7)
     DGROUP:0x9682 = 0x9680 = 0 and the low words of the dwords DGROUP:0x8e04 and 0x8e00 = 0
     (the duel score words; `mov word ptr` [BYTES seg13:0x9700-0x970b]); fight bytes c+0x13e+t
     = t+0x13e+c = 0; c = P with PlayMode = 1: seg3:0x33b4(3); seg13:0x2714(t, c, −12, −55, 1, −1).
8. The replies [BYTES seg13:0x975e-0x97ff]: r = 1: msg(t, 14, c, 0x4b), msg(c, 14, t, 0x4d);
   r = 2: msg(t, 14, c, 0x4d), msg(c, 14, t, 0x4b); else c's +0x52 = 3: msg(t, 14, c, 0x82),
   msg(c, 14, t, 0x12); else q = 1: msg(c, 14, t, 0x12), msg(t, 14, c, 0x16); else q = 2: msg(c,
   14, t, 0x11), msg(t, 14, c, 0x15). (With q = 0 step 6 has returned.)
No `rand()` in the routine.

#### seg13:0x9810(c), "self-preserve" (Ctrl+S) [BYTES seg13:0x9810-0x996d]
cdecl. Tells the **targeted** ship t = c's +0x40 to look after itself.
1. c ≠ P and dword c+0x9c > now → return (the player is never refused) [BYTES seg13:0x9823-0x9845].
2. seg13:0x910a(c) ≠ 0 → msg(c, 7, 0, 0x64); tp ≠ 14 → return; team(t) = 7 → msg(c, 14, t, 0x40).
3. t not in play, not alive, busy (+0x52 ∉ {0,7} and dword +0x98 ≥ now, `jae`), t = c, or
   seg13:0x2688(t, c) ≤ 0 → msg(c, 14, t, 0x5a) [BYTES seg13:0x9890-0x98df seg13:0x994e-0x995e].
4. Else: t's +0x52 ≠ 0: msg(t+0x58, 14, t, 0x13); t's +0x52 = 0x0d, dword +0x94 = now + 200,
   +0x58 = c, +0x98 = now + 15000, byte +0xed = 0; msg(t, 14, c, 2) [BYTES seg13:0x98e1-0x9944].
   c+0x9c is not written. Order 0x0d is the executor's "needs" (order_needs).

#### seg13:0x9ce0(c), "capture my target" (Ctrl+C; also seg4:0x446e) [BYTES seg13:0x9ce0-0x9f3c]
cdecl, returns 0 or 1.
1. The cooldown test (returns 0). (tp, tr) = c's target [BYTES seg13:0x9cee-0x9d31].
2. The target object's byte +0x53 = 0 (not capturable): the first teammate j ≠ c (team(j) =
   team(c); no state test) gets msg(c, 14, j, 0x23 when tp = 14, else 0x24); no teammate →
   msg(c, 7, 0, 0x25); return 0 [BYTES seg13:0x9d34-0x9dae].
3. For j = 0 … DGROUP:0x9316: in play, alive, free (+0x52 0 or 7, or dword +0x98 < now, `jae`
   skips), j ≠ c, team(j) = team(c) (teammates only): j's +0x52 ≠ 0 → msg(j+0x58, 14, j, 0x13);
   +0x52 = 9, dword +0x94 = now + 200, +0x54 = tp, +0x56 = tr, +0x58 = c, +0x98 = now + 25000,
   c+0x9c = now + 15000, byte +0xed = 1; msg(j, 14, c, 2); c = P with PlayMode = 1:
   seg3:0x33b4(9); j ≠ P: seg3:0xf4(j, tp, tr); return 1 [BYTES seg13:0x9db0-0x9edf].
4. None: the first teammate j ≠ c (any state) → msg(c, 14, j, 0x26); none → msg(c, 7, 0, 0x27);
   return 0 [BYTES seg13:0x9ee1-0x9f32].
Order 9 is the executor's capture order (the acknowledgement is message 1 case 9, 389).

#### seg13:0x9f3e(c), "exit the wave" (Ctrl+E) [BYTES seg13:0x9f3e-0xa01b]
cdecl. t = c's +0x40.
1. tp ≠ 14 or team(t) ≠ team(c) → msg(c, 7, 0, 0x32); return.
2. t busy (+0x52 ∉ {0, 7} and dword +0x98 > now, `jbe` admits) and sbyte t+0x5d > 1 → msg(c, 14,
   t, 0x31); return.
3. team(t) = 7 → msg(c, 14, t, 0x40); return (reachable only for a team-7 c).
4. t's +0x52 ≠ 0 → msg(t+0x58, 14, t, 0x13); t's +0x52 = 10, dword +0x94 = now; msg(t, 14, c, 2).
   No cooldown, no +0x58, no +0xed, no +0x98 [BYTES seg13:0x9fd0-0xa00c].
Order 10 is the executor's "leave by the nearest gate".

#### seg13:0xa01c(c), "get gas" (Ctrl+G) [BYTES seg13:0xa01c-0xa185]
cdecl. t = c's +0x40.
1. tp = 14 and team(t) = 7 → msg(c, 14, t, 0x40); tp ≠ 14 or team(t) ≠ team(c) → msg(c, 7, 0,
   0x36); return.
2. t's +0x52 ∉ {0, 7}, **dword t+0x9c** (sic, not +0x98) > now and sbyte t+0x5d > 0 → msg(c, 14,
   t, 0x35); return [BYTES seg13:0xa08a-0xa0b5].
3. t's +0x52 ≠ 0 → msg(t+0x58, 14, t, 0x13); t: +0x52 = 0x0c, goal +0x32 = 14, +0x34 = c, +0x38
   = 2, +0x3c = 2, +0x54 = 14, +0x56 = c, +0x58 = c, dword +0x98 = now + 25000, +0x94 = now + 500;
   c+0x9c = now + 15000; byte t+0xed = 1; t+0xa4 = now − sext16(lo16(2000·sbyte t+0x5e)) + 8000;
   msg(t, 14, c, 2) [BYTES seg13:0xa0b8-0xa175].
Order 0x0c is the executor's "fetch the nearest fuel tank".

#### The defenders (Alt keys) and seg13:0x9970
- **seg13:0x9a18(i, pool, rec)** (Alt+A with i = P and the player's target): the reading in
  see "seg4:0x8346 and seg13:0x9a18" above [BYTES seg13:0x9a18-0x9ae6]; it writes the entry's word −8, DGROUP:0x52f6 + 0x24k = 0 [BYTES seg13:0x9ab1] (the defenders' mover below says what that word does).
- **seg13:0x9ae8(i)**, defenders return (Alt+H; also seg4:0x42fd, 0x442f, 0x4630 in the goal
  choice) [BYTES seg13:0x9ae8-0x9bb7]: last = −1; for k < DGROUP:0x940a with owner (+6, +8) =
  (14, i) (literal 14) and state word +0 = 15: +0 = 0; word DGROUP:0x52ec + 0x24k =
  lo16(defender k's x − owner's x) (object word +4 of pool 2 k, through DGROUP:0x1e00, minus the
  owner object's +4 through DGROUP:0x1df8 + 4·(+6)); word DGROUP:0x52ee + 0x24k = the same for y
  (+6); defender object word +0x1e = 0; last = k. last ≥ 0 → seg10:0x754a(0x15, 2, last, 1).
  The two words are the defender's offset from its owner, the orbit slot the mover keeps it at
  (they are read at seg8:0x8728-0x8735, 0x8825, 0x884f, 0x8957, 0x8962) [OPEN].
- **Alt+E**: (the key decode above).
- **seg13:0x9970(c)** (the reactions' "ask for an attack", called with c's target already set)
  [BYTES seg13:0x9970-0x9a15]: dword c+0x9c > now → −1. When any ship j (in play, alive, 2 ≤
  +0x52 ≤ 4, +0x58 = c, j ≠ P, j ≠ c) already carries c's command: c+0x9c = now + 5000, return
  0. Else return seg13:0x8570(c).

#### The reply strings (seg3:0x6ec cases) used by the commands [BYTES seg3:0x1351 table seg3:0x1356]
String ids are ENG_LANG.DLL; "+snd" is the seg10:0x744c sound the case plays first; mode 2
everywhere unless stated. Codes 0x8c … 0xa3 share seg3:0x128f: string = code + 0x6af, no sound,
no argument [BYTES seg3:0x128f-0x1297 seg3:0x12bb seg3:0x8d9].

| Code | Case | String | Arguments |
|---|---|---|---|
| 1 | seg3:0x841 | by the **sender's** +0x52: 1 → 293 +snd 0x37; 2, 3 → +snd 0x6c, 296 "I'm Attacking %D%< (you)%>." (or, +0x54 = 9: 1402 when seg13:0x2688(sender, P) > −50, else 1403); 4 → +snd 0x6c, 297; 5 → +snd 0x6d, 295; 7 → 367; 9 → 389 (mode 1); 10 → +snd 0x36, 426; 12 → 444; 13 → 450 +snd 0x44 when +0x56 = sender, else 681; 6, 8, 11, other → lines unchanged. End time now + 9000 | (+0x54, +0x56, 5, (+0x54, +0x56) = (14, P)) |
| 2, 0x34 | seg3:0x9ca | the request, by o = recipient's +0x52 (code 2) or DGROUP:0x964e (0x34): 1 → +snd 0x34, 3020 "Please help me %s. I'm in trouble!" (%s = pilot DGROUP:0x380c); 2–4 → +snd 0x34 when ship r hunts the sender (p = 14 and ship r's goal (+0x32, +0x34) = (14, sender) with +0x38 = 1), else 0x40; the recipient's +0x54 = 9 (for both codes) → 3019, else 302 "Please Attack %D"; 5 → +snd 0x34, 303; 9 → 388; 10 → 428; 12 → 442; 6, 7, 8, 11 and above 12 → unchanged (the tail) [BYTES seg3:0x9ca-0xade]. End time 0 and DGROUP:0x9608 = 1 | p, r = recipient's +0x54/+0x56 (2) or DGROUP:0x9650/0x9652 (0x34); %D (p, r, 5) |
| 3 | seg3:0xb80 | the bidding report (seg13:0x7b40 above) | — |
| 5 | seg3:0xc19 | 331 "No One Can Help %D." | (DGROUP:0x6e54, 0x6e52, 5) |
| 6 | seg3:0xc29 | 329 "You'll Have To Fend For Yourself" | — |
| 0x0e | seg3:0xc31 | 332 "Sorry, %s. I Will Not Fight You." | pilot |
| 0x0f | seg3:0xc3d | 334 "You Are Not Worth The Trouble of Destroying!" | (an unused 334) |
| 0x10 | seg3:0xc64 | +snd 0x3c, 340 "Busy Now, I'll Get You Later..." | — |
| 0x11 | seg3:0xc74 | 336 "Very Well, %s, I Accept Your Challenge!" | pilot |
| 0x12 | seg3:0xc7e | 338 "I Love A Good Fight! Prepare To Die, Fool!" | — |
| 0x13 | seg3:0xae0 | by the **sender's** +0x52: 1 → +snd 0x44, 342; 2–4 → +snd 0x44, 344 "Breaking Off Attack On %D."; 5 → +snd 0x44, 345 (code 0); 6 → +snd 0x6b, 345 (code 5); 9 → 387; 10 → 424; 12 → 446; other → unchanged | (+0x54, +0x56, …) of the sender |
| 0x14 | seg3:0xc54 | 334 (sic: not 354) | (DGROUP:0x6e54, 0x6e52, 2), unused |
| 0x15 | seg3:0xc96 | +snd 0x3f, 358 "Look, Friend... Do You Want To Rumble?" | — |
| 0x16 | seg3:0xc86 | +snd 0x3f, 356 "I've Had Enough of You. Let's Fight It Out!" | — |
| 0x1f | seg3:0xda1 | 385 "I Love A Good Duel!, Who Will Come Out Alive?" | — |
| 0x23 | seg3:0xe83 | +snd 0x3c, 398 "I Can't Capture %D Should I Attack It?" | (0x6e54, 0x6e52, 5) |
| 0x24 | seg3:0xe9b | +snd 0x3c, 400 "What Use Have I Got For %D?" | (DGROUP:0x6e54, 0x6e52, 2) |
| 0x25 | seg3:0xeb3 | +snd 0x3c, 401 "Can't Capture That. %D Isn't Capturable." | (DGROUP:0x6e54, 0x6e52, 2) |
| 0x26 | seg3:0xed3 | +snd 0x3c, 404 "I'm Too Busy Right Now. You Go Ahead And Get It." | — |
| 0x27 | seg3:0xee3 | 406 "It's Nice To Share, But No Teammates Are Around." | — |
| 0x31 | seg3:0xf21 | 432 "I'm Staying. I've Got Some Business To Take Care Of." | — |
| 0x32 | seg3:0xf19 | 430 "You Do Not Have An Ally Willing To Leave." | — |
| 0x35 | seg3:0xf41 | 438 "I'm Busy. I'll Get Gas When I Need Some." | — |
| 0x36 | seg3:0xf39 | 436 "You'll Have To Get Your Own Fuel Buddy.." | — |
| 0x38 | seg3:0xf49 | 448 "I Can't Find A Tank Of Gas Anywhere!" | — |
| 0x40 | seg3:0xf59 | 555 "We Don't Take Orders, We Enforce Order!" | — |
| 0x47 | seg3:0xf29 | 650 "Leave Me Be! Can't You See You've Done Enough?" | — |
| 0x4b | seg3:0x1009 | 657 "If I Die, I Die. But You'll Still Suffer!" | — |
| 0x4c | seg3:0x1011 | 659 "I Can't Take You Now, But Your Day Will Come." | — |
| 0x4d | seg3:0x1019 | 661 "You're Crazy To Take Me On! But That's Your Problem!" | — |
| 0x4e | seg3:0x1021 | 663 "If You Were Stronger, I Would Fight You!" | — |
| 0x5a | seg3:0x1068 | +snd 0x3c, 685 "Don't Worry! I'll Be Alright." | — |
| 0x64 | seg3:0xbfe | 328 "Sorry, Can't Attack %D" with the far pointer DGROUP:0x380c pushed as its arguments (%D reads type 0x380c, arg = DS, code = the next stack word): ADR 0016 site | — |
| 0x74 | seg3:0x11c8 | 1408 "We Are Already In a Duel!" | — |
| 0x82 | seg3:0x1235 | 1419 "I Don't Want Any More Duels. So I'll Kill You Now!!" | — |
| 0x8b | seg3:0x1281 | 1850 "Sorry, But I Can't Help. I'm Too Low On %D." | (4, 0, 0) |
| 0x8c … 0xa3 | seg3:0x128f | 1851 … 1874 (0x8d is seg3:0x1255: +snd 0x3c, 1852 with %d = lo16 of the unsigned seg5:0x2b4((sender's dword +0x98) − now, 1000) [BYTES seg3:0x1255-0x127e]) | — |
| 0xbc | seg3:0x12ed | 1948 "You're Gonna Get It When I Get %D!" | (4, 0, 2) |
| 0xbd | seg3:0x12fb | 2000 "I Don't Accept. I'm Here To Help %D." | (sender's +0x54, +0x56, 5): the head's [bp−0x70] / [bp−0x72], written for a pool-14 sender only [BYTES seg3:0x73b-0x74f] |

So the refusal table of (the attack command) reads: 0x8e 1853 "Already In A Battle", 0x8f 1854 "He Looks Too
Difficult", 0x90 1855 "He Just Kicked My Butt", 0x91 1856 "I Happen To Like Him", 0x92 1857
"Attack Your Own Teammate?", 0x93 1858 pacifists (FUZZY ONES), 0x94 1859 Knights, 0x95 1860
Renegades, 0x96 1861 Enforcers, 0x97 1862 "An Icon?", 0x98 1863, 0x99 1864, 0x9a 1865 "Just
Leave Me Alone", 0x9b 1866, 0x9c 1867 "Can't Afford The Body Work", 0x9d 1868 "You Do It!", 0x9e
1869 "I'm The Only One Here" (the target is the asked ship), 0x9f 1870 "Already Helping You",
0xa0 1871 "Already Got Orders To Kill You", 0xa1 1872 "If We Weren't In A Duel!", 0xa2 1873 "No
Friend Of Mine", 0xa3 1874 "Try That Command When There's Someone Around".

#### The player as a commanded ship, the deferred reply, Strategy mode
- **DGROUP:0x964e/0x9650/0x9652/0x9654, dword DGROUP:0x8858, dword DGROUP:0x885c** hold one
  request a computer ship c made of the **Strategy** player: the order (1 help, 4 attack, 5
  defend, 0x0b duel), its object (pool, rec), the requester, the time it was recorded, and the
  time of the deferred reply. Writers: seg13:0x84d1 (help), 0x8bc9 (attack), 0x90af (defend),
  0x955e (duel), each only when now − 15000 > DGROUP:0x8858, so at most one request per 15 s; the
  clear is seg13:0xa69d-0xa6b0 [BYTES seg13:0x84d1-0x84f2 seg13:0x8bc9-0x8bec seg13:0x90af-0x90cb
  seg13:0x955e-0x9577 seg13:0xa69d-0xa6b0]. Message 0x34 shows the request (case table above); the duel record of seg13:0x915c (11, 14, c, c) is stored without message 0x34 and gives neither ship an order [BYTES seg13:0x954b-0x957b seg13:0x975e].
- **seg13:0xa5c2()** [BYTES seg13:0xa5c2-0xa659], called at the end of the Strategy keys A, C,
  E, G, D (seg13:0xa229, 0xa3bb, 0xa471, 0xa4ea, 0xa55e), checks whether the player's new order
  answers the request: (P's +0x52 = DGROUP:0x964e, P's +0x54 = DGROUP:0x9650, dword DGROUP:0x8858
  + 20000 > now, and P's +0x56 = DGROUP:0x9652 or DGROUP:0x9650 = 9) or (P's +0x52 = 4,
  DGROUP:0x964e = 0x0b, DGROUP:0x8858 + 20000 > now, P's (+0x54, +0x56) = (DGROUP:0x9650,
  0x9652)) → dword DGROUP:0x885c = now + 700 and DGROUP:0x8858 = now. Nothing else.
- **seg13:0xa65a()** [BYTES seg13:0xa65a-0xa6ba] (from the player's re-plan seg4:0x681): with
  DGROUP:0x885c ≠ 0 and < now (unsigned): seg13:0x7b40(DGROUP:0x9654) unless **dword
  DGROUP:0x8858 = 0x0b**, else seg3:0x6ec(P, DGROUP:0x9650, DGROUP:0x9652, 0x1f); then the six
  words are cleared. DGROUP:0x8858 holds a time (seg13:0xa64f stores now), so the 0x1f branch
  runs only when the answer came at now = 11 ms: the duel reply is practically dead, and the
  the comparison stays as written.
- **Strategy keys** (PlayMode ≠ 0; F12 Command mode): A, C and D act on the mark DGROUP:0x6e54 /
  0x6e52, not on the player's target words +0x3e/+0x40 [BYTES seg13:0xa1a5-0xa1af
  seg13:0xa24c-0xa255 seg13:0xa508-0xa50e]. A seg13:0xa186 (P's order 4 at the
  mark, pool 12 replaced by 8, +0x98 = now + 30000, seg4:0x8346(P, …, 12),
  seg13:0x2714(rec, P, −15, −60, 1, 1) for a ship); C seg13:0xa234 (order 9 at the target if pool
  0 or 11 and in play, else the nearest pool-0 or pool-11 record within 20000 cells, else msg(P,
  7, 0, 0x25)); E seg13:0xa3c8 (order 10 at the nearest even pool-12 record within 20000); G
  seg13:0xa47c (order 0x0c at P itself when pool 4 has records, else msg(P, 7, 0, 0x38)); D
  seg13:0xa4f4 (order 1 at a ship target); S seg13:0xa568 (order 0x0d at P). Each sets +0x94 =
  now + 200, +0xed = 1, +0x58 = P. They belong to Command mode (F12).
- **Ctrl+Alt+S, seg13:0x4ac4(P)** [BYTES seg13:0x4ac4-0x4b9e]: seg13:0x498e(P, 0), then the first
  in-play ship j hunting P (goal (14, P), +0x38 = 1) with word +0xd6 = −1 (no roster record, a
  called-in ship) gets the arrest order (+0x52 = 0x16, +0x54 = 14, +0x56 = P, +0x98 = now +
  50000, +0xed = 1, +0x94 = now + 500), DGROUP:0x8680 + 2·P = j, and for P DGROUP:0xa163 = 5,
  DGROUP:0xa152 += 1; hunters with a roster record set P's goal to (11, 0) with +0x38 = 2. Law
  track (Later stages).

### seg13:0x7ba2(i), the reactions [BYTES seg13:0x7ba2-0x81c0]

cdecl, one caller: the think's re-plan step 4c, seg4:0x545 (the think's re-plan, step 4c). It
decides, for a computer ship under pressure, to flee, to stand and fight, to call for help, to ask
others to attack, to call the ambulance, or to calm down. Locals: n ([bp−6], the threat count),
w ([bp−0x10]), h ([bp−0x14]), best ([bp−0xa], 20000 = 0x4e20), k ([bp−0xc], −1).

1. **Exits** [BYTES seg13:0x7bb0-0x7be6]: i = P in a player game; Demon's Gate (DGROUP:0x92e8 ≠
   0); a duel (DGROUP:0x9320 = 7); team(i) = 7.
2. w = object (14, i) word +0x34 when object word +0x32 = 14 (pool-14 pointer DGROUP:0x1e30), else
   −1 [BYTES seg13:0x7be9-0x7c05]. h = 1 when i's +0x52 is 1 or 5 and grudge byte i+0xc2 + (i's
   +0x56) = 0 [BYTES seg13:0x7c08-0x7c30].
3. Busy (i's +0x52 ∉ {0, 7} and dword +0x98 > now, `ja` exits) → return [BYTES seg13:0x7c3f-0x7c5b].
4. **Count the threats** [BYTES seg13:0x7c5f-0x7d0b]: for j = 0 … DGROUP:0x9316 in play whose goal
   is (14, i) with +0x38 = 1 and alive: s = seg4:0x5d9c(i, j); s < best → best = s, k = j; n += 1.
   For each pool-2 record k2 < DGROUP:0x940a in play with entry target (DGROUP:0x5308, 0x530a) +
   0x24·k2 = (14, i) and state DGROUP:0x52fe + 0x24·k2 = 10: n += 1. w ≠ −1: k = w (object word +0x34, replacing the nearest hunter k while the distance `best` stays the nearest hunter's, 20000 when there is none), n += 1.
5. n = 0 → step 9 [BYTES seg13:0x7d0e-0x7d12]. Else: k ≠ −1 and fight byte sbyte i+0x13e+k < −1
   → n += 1; h ≠ 0, Engine +0x26 > 320 and w = −1 → return [BYTES seg13:0x7d16-0x7d45].
6. **Flee or repair** (skipped to step 7 when +0x38 = 4, when +0x52 = 3 with Engine ≥ 20, when
   e6 + 15 ≤ Engine and e6 + 17 ≤ word +0x28 with e6 = lo16(lo16(sbyte i+0x5e · n)·6), or when
   +0x52 ≠ 0 and dword +0x98 − 7000 ≥ now) [BYTES seg13:0x7d49-0x7da9]:
   - when 20n + 35 > best or AX(15 ⊗ sbyte i+0x5e) + 20 > Engine, with k ≠ −1 and sbyte
     i+0x13e+k < −1: **flee from k**: goal and estimate (14, k), +0x3a = +0x3c = 5, +0x36 = +0x38
     = 4, +0x52 = 7, dword +0x94 = now + 4000, msg(k, 14, i, 0x47) [BYTES seg13:0x7dad-0x7e25];
   - otherwise, with word +0x2a < 50 and 8·sbyte i+0x5e + 35 > Engine: **repair**: +0x52 = 0x0d,
     +0x58 = i, dword +0x98 = +0x94 = now + 1000, byte +0xed = 0 [BYTES seg13:0x7e28-0x7e6c]. The
     block after it (seg13:0x7e71-0x7efb: messages 0x8a, 0x73 and the 0x13 pair, and the stores byte +0x13e + (+0x56) = 0 and the grudge byte = 0xff) tests +0x52 for 2 … 4 and
     for 1 or 2 right after the store of 0x0d, so it never runs; the call at seg13:0x7eff is live, the flee branch reaches it from seg13:0x7e25 to send 0x47.
7. **Stand** [BYTES seg13:0x7f06-0x7f31]: +0x38 = 4 and 20n + 35 < best → +0x36 = +0x38 = 1
   and seg4:0x8346(i, +0x32, +0x34, 0x10).
8. **Calls** [BYTES seg13:0x7f39-0x80a2]:
   - seg13:0x9bb8(i, n) ≠ 0 → seg16:0xb4e(i) (the ambulance, below).
   - Engine < 20n + 25 and k ≠ −1: when some other ship (in play, alive, ≠ i, ≠ P) already has
     order 1 on i (+0x56 = i) → return; else r = seg13:0x81c2(i) (help); r = 0 → dword i+0x9c = 1.
   - Else n ≠ 0 and k ≠ −1, and (n > 1, best < 78, or i's goal is (14, DGROUP:0x952e) while
     DGROUP:0x9e49 ≠ 10): seg3:0xf4(i, 14, k) and r = seg13:0x9970(i); r = 0 → i+0x9c = 1.
   - team(i) = 2 (FUZZY ONES), n ≠ 0, k ≠ −1: the same "already helped → return" test, then
     seg13:0x81c2(i); 0 → i+0x9c = 1.
   (`dword +0x9c = 1` writes the low word 1 and the high word AX = 0: the cooldown is cleared.)
9. **Calm down** [BYTES seg13:0x80a6-0x80d3]: (n = 0 or best > 60) and +0x38 = 4: +0x36 = +0x38 =
   3, +0x52 = 0, dword +0x94 = now.
10. seg13:0x9bb8(i, n) ≠ 0 → seg16:0xb4e(i) again [BYTES seg13:0x80d8-0x80f1].
11. **Ask for an attack on the enemy** [BYTES seg13:0x80f2-0x814d]: +0x38 = 1, goal pool 14, sbyte
    i+0x13e + (goal rec) < −2, seg13:0x2688(i, k) < −150 with k = i's word +0x34 (the call pushes two
    arguments, `push si` at seg13:0x8110 with si loaded from +0x34 at seg13:0x8105, and `push [bp+6]`; `add sp, 4`)
    and i alive: seg3:0xf4(i, 14, goal rec); seg13:0x9970(i); 0 → i+0x9c = 1.
12. **Viruses** [BYTES seg13:0x8151-0x81b5]: pool 9 in use DGROUP:0x9418 > 1, goal pool ≠ 14, one
    `rand()` (seg13:0x8166) % 3 = 0 and dword i+0x9c < now: the first pool-9 record in play
    (index < DGROUP:0x9418) becomes the target (seg3:0xf4(i, 9, rec)) and seg13:0x9970(i).

`rand()`: one direct draw (step 12, only on its path), plus the callees' (seg13:0x81c2's % 14 for a
computer c with no helper; seg13:0x8570's coin flips; seg16:0xb4e's). The reactions never draw
for the player (step 1).

#### seg13:0x9bb8(i, n), "time for the ambulance" [BYTES seg13:0x9bb8-0x9cde]
cdecl, returns 0 or 1. 0 unless pool-7 record 0 (DGROUP:0x1e14) has `state` ≠ 0 (the ambulance
is out of the wave). m = lo16(word +0xdc + word +0xce) ([bp−8]); m < 250 and i ≠ P → 0. When the
player hunts i or i hunts the player (goal (14, ·) both ways): 0 unless word +0x82 ≥ 1700 and
Engine ≤ 1. Word DGROUP:0x1c8e + 2i = DGROUP:0x91f8 (already this wave) → 0. Then 1 (and
DGROUP:0x1c8e + 2i = DGROUP:0x91f8) when word +0x28 ≤ 0 or +0x2a ≤ 0; when +0x26 + +0x28 + +0x2a <
clamp(20(n + 2) + m/10, 70, 180); when +0xce > 200 and (DGROUP:0x9334 + 20)·35 < m; or when a wing
is gone (byte +0xac = 0 or +0xad = 0) and m > 500. Else 0.


### The defenders' mover as far as commands reach it

- The entry (DGROUP:0x52fe + 0x24k): +0x00 state (0 orbit the owner, 5, 10 attack-orbit, 15
  attack), +0x06/+0x08 the owner, +0x0a/+0x0c the target, dword +0x0e a timer; below the base:
  words −0x12/−0x10 (DGROUP:0x52ec/0x52ee) the orbit offset, −8 (DGROUP:0x52f6) the homing flag,
  −4 (DGROUP:0x52fa) ([OPEN]).
- **State 15** in seg8:0x8256 [BYTES seg8:0x8f5b-0x9000 seg8:0x8300-0x844c]: a target record
  above its pool's count (word DGROUP:0x92ec + 2·pool < rec, `jge` passes equality) counts
  DGROUP:0x9498 and becomes (12, 1) (globals.md's DGROUP:0x9498 line calls these the owner words;
  they are the target words +0x0a/+0x0c); a target not in play becomes (12, 1) (the way out). The
  steering below runs only while DGROUP:0x9452 ≠ 0 and the entry's dword +0x0e < now; otherwise
  the defender spins while +0x0e > now (object +0x28 += 1, back to 0 above 15) [BYTES
  seg8:0x8fb4-0x8fd6 seg8:0x844e-0x847e]. It aims at the target's (x, y), or a cloaked ship
  target's guess dword +0x14a (byte +0xe8 ≠ 0), through
  seg8:0x285a (d at least 4, [bp−0x36]/[bp−0x38] the outputs): **word −8 = 0**: no thrust; it
  becomes 1 once d > 220 cells; **word −8 = 1**: vx += lo16(8·o1)/(d sar 2), vy += lo16(8·o2)/(d
  sar 2) (16-bit `idiv`), heading +0x28 = seg8:0x19b0(vx', vy'), both velocities clamped to ±70.
  So seg13:0x9a18 (and seg8:0x9028 with state 15) reset the flag: a defender already near its new
  target first drifts out past 220 cells before it homes in. Which sign o1/o2 carry is
  seg8:0x285a's (core.md).
- **State 0** after Alt+H: the offset words seg13:0x9ae8 wrote are the orbit slot; the orbit
  steering (seg8:0x8728-0x8b17) is [OPEN] (Later stages).

#### Further facts of the commands
Facts of the routines above that the sections leave out.

##### Key decode (seg11:0x6b64)
- In the demo (DGROUP:0x95bc = 0) only VK 0x6b, 0x6d, 'W', 0xbd and 0xbb reach the switch, so no command key works in the demo [BYTES seg11:0x6e19-0x6e3e].
- H with neither Ctrl nor Alt, in a player game, calls the ambulance. When seg16:0xb4e(P) ≠ 0 it sends seg3:0x6ec(P, 7, 0, 7). Otherwise it sends 0x84 when P's +0xdc + +0xce < −300, else 8 [BYTES seg11:0x6f9c-0x6fe5].
- Ctrl+Alt+E runs seg13:0x9f3e(P) and then the Alt+E defender loop. Ctrl+F ignores Alt. Ctrl+Alt+X calls seg16:0x8e4(P, DGROUP:0x9324); Ctrl+X alone does nothing [BYTES seg11:0x70a5-0x70b9 seg11:0x6f3a-0x6f55].

##### seg13:0x81c2 (help)
- It calls seg13:0x4eec(c, 15) after the cooldown store. For each scored candidate it calls seg13:0x4eec(c, 20), and seg13:0x4eec(j, 40) when c = P; these run before the team-7 and Strategy tests [BYTES seg13:0x821e seg13:0x8481-0x84a0].
- Candidate filter: object state 0, byte +0xec ≠ 0, j ≠ c, and free (+0x52 ∈ {0, 7}, +0x98 < now, or +0x58 = c). A busy j is admitted only when c = P in a player game [BYTES seg13:0x8353-0x83a1].
- Taking a candidate: when j's +0x52 ≠ 0, message 0x13 goes to j's old +0x58 from (14, c). When c = P with DGROUP:0x9134 = 1 it calls seg3:0x33b4(1). It then calls seg3:0xf4(j, 14, c) [BYTES seg13:0x8247-0x82db].
- For c = P there is no draw: message 6 goes from (7, 0) in a player game, nothing in the demo [BYTES seg13:0x8509-0x8552].

##### seg13:0x8570 (attack)
- After the cooldown store it returns 0, for a computer asker or the demo player, when P's +0x32/+0x34 equal the asker's target, or when both pools are 0x16 [BYTES seg13:0x85f7-0x8621]. For a computer asker with a pool-9 target it also returns 0 in four cases [BYTES seg13:0x8623-0x86b1]:
  - DGROUP:0x8710 ≠ 0 and DGROUP:0x8710 + 1500 > DGROUP:0x1e68;
  - DGROUP:0x8714 ≠ 0 and DGROUP:0x8714 + 600 > DGROUP:0x1e68;
  - some ship k ≠ P with +0xec ≠ 0 hunts P (+0x32 = 14, +0x34 = P, +0x38 = 1);
  - P's +0x26 < 40.
  It then calls seg13:0x4eec(c, 4).
- The loop tests object state but never byte +0xec [BYTES seg13:0x86d4-0x86e9].
- Code 0x9a compares j's team with the team of ship record [bp−0xa] whatever the pool, and code 0x99 reads byte j+0xee+rec with no pool test [BYTES seg13:0x87e5-0x8803 seg13:0x88fd-0x890a]. Code 0x8e compares j's +0x32 (already 14) with c, so its first exemption holds only when c = 14 [BYTES seg13:0x89e1-0x89f6].
- On obeying, the routine does the following, then returns 1 [BYTES seg13:0x8b3f-0x8d8b]:
  - it calls seg13:0x4eec(j, 15), seg13:0x4eec(j, 0x19) when c = P, and seg13:0x4eec(c, 0x14);
  - for pool 9 it calls seg13:0x4eec(j, −10) and seg13:0x4eec(c, −13), and for c ≠ P sets DGROUP:0x8710 = DGROUP:0x1e68;
  - message 0x13 goes to j's old commander when j had an order;
  - it calls seg4:0x8346(j, pool, rec, 9);
  - for c = P with DGROUP:0x9134 = 1 it calls seg3:0x33b4(2);
  - it sends msg(j, 14, c, 2), and for j ≠ P calls seg3:0xf4(j, pool, rec).

##### Request record and acknowledgement (seg13:0xa5c2, seg13:0xa65a, the Strategy routines)
- The Strategy routines set P's +0x52 as follows:
  - seg13:0xa4f4: 1;
  - seg13:0xa186: 4, with pool 12 mapped to 8;
  - seg13:0xa234: 9;
  - seg13:0xa3c8: 10, on the nearest pool-12 record;
  - seg13:0xa47c: 12, on (14, P);
  - seg13:0xa568: 13, on (14, P).

  The recorded requests are 1, 4, 5 and 11, so a recorded defend request (5) can never be answered. The order-9 alternative at seg13:0xa601 never matches a recorded order. seg13:0xa568 does not call seg13:0xa5c2 [BYTES seg13:0xa19f seg13:0xa37c seg13:0xa426 seg13:0xa4ae seg13:0xa51f seg13:0xa57f].
- seg13:0xa186 also calls seg4:0x8346(P, pool, rec, 12) with the mark's own pool, before the 12 → 8 mapping [BYTES seg13:0xa1f6-0xa1fc], and, for a ship target, seg13:0x2714(rec, P, −15, −60, 1, 1). Each Strategy routine sets +0x94 = now + 200, +0x98 = now + 30000 (25000 for capture), +0xed = 1 and +0x58 = P [BYTES seg13:0xa1c0-0xa225].
- seg13:0xa234 uses the mark when it is a pool-0 or pool-11 object in play, and searches only otherwise. Otherwise it takes the nearest in-play pool-0 record (count DGROUP:0x91fa) or pool-11 record (count DGROUP:0x941c) by seg8:0x285a and targets it with seg3:0xf4; with none it sends 0x25 from (7, 0). seg13:0xa47c refuses with 0x38 from (7, 0) when DGROUP:0x940e < 1 [BYTES seg13:0xa24c-0xa374 seg13:0xa48d-0xa4a6].

##### seg13:0x8e98 (defend)
- Message 0x13 goes to j's old +0x58 unconditionally, even when j had no order [BYTES seg13:0x8f44-0x8f4f].
- On the Strategy-player candidate the routine returns whether or not it records, without asking the other ships [BYTES seg13:0x90ad seg13:0x90d9].

##### seg13:0x915c (challenge)
- Already fighting each other on their own orders (each ship's +0x54/+0x56 = the other, +0x52 in 2..4 and +0x58 = itself): message 0x74 from (14, t). Target not in play: silent return [BYTES seg13:0x92a3-0x9318].
- Refusal messages: 0xe (target free) or 0x10 (busy) on the liked branch, 0xf or 0x10 on the other, all from (14, t) to c. The demo player gets none [BYTES seg13:0x93f1-0x94c7].
- Acceptance [BYTES seg13:0x9503-0x9756]:
  - message 0x13 goes to each ship's old commander;
  - t gets +0x52 = 2, +0x94 = now + 200, +0x54/+0x56 = (14, c), +0x58 = t, +0xed = 1, +0x32/+0x34 = (14, c), +0x38 = 1, +0xa4 from sbyte t+0x5e, and seg4:0x8346(t, 14, c, 0x15); when t = P, DGROUP:0xa62 = 1;
  - c gets the mirror stores with +0xed = 0, seg3:0xf4(c, 14, t) when c ≠ P, and seg4:0x8346(c, 14, t, 0x14);
  - bytes c+0x13e+t and t+0x13e+c are cleared;
  - for c = P with DGROUP:0x9134 = 1 it calls seg3:0x33b4(3);
  - it calls seg13:0x2714(t, c, −12, −55, 1, −1).
- Final messages [BYTES seg13:0x975e-0x97ff]:
  - r = 1: (t, 14, c, 0x4b) and (c, 14, t, 0x4d);
  - r = 2: (t, 14, c, 0x4d) and (c, 14, t, 0x4b);
  - otherwise, with c's +0x52 = 3: (t, 14, c, 0x82) and (c, 14, t, 0x12);
  - q = 1: (c, 14, t, 0x12) and (t, 14, c, 0x16);
  - q = 2: (c, 14, t, 0x11) and (t, 14, c, 0x15).

##### seg13:0x9810, 0x9ce0, 0x9f3e, 0xa01c
- seg13:0x9810 [BYTES seg13:0x9823-0x9944]:
  - the cooldown applies only to computer askers, and nothing writes +0x9c;
  - with no other ship in play it sends 0x64 from (7, 0); a non-ship target returns silently; an Enforcer gets 0x40;
  - it sends 0x5a when the target is not in play, is busy (+0x98 ≥ now), is c itself, or has A(t, c) ≤ 0;
  - message 0x13 goes to the target's old commander; it sends msg(t, 14, c, 2).
- seg13:0x9ce0 [BYTES seg13:0x9e39-0x9f32]:
  - the teammate also gets +0x94 = now + 200, +0x54/+0x56 = the target, +0x58 = c, +0xed = 1; c's +0x9c = now + 15000;
  - it sends msg(k, 14, c, 2), calls seg3:0x33b4(9) for c = P with DGROUP:0x9134 = 1, and seg3:0xf4(k, pool, rec) for k ≠ P;
  - with no free teammate it sends 0x26 from the first teammate in any state, or 0x27 from (7, 0).
- seg13:0x9f3e [BYTES seg13:0x9f7d-0x9fe6]:
  - not a teammate ship: 0x32 from (7, 0); an Enforcer teammate: 0x40;
  - message 0x13 goes to the old commander **from (14, t)**; it sends msg(t, 14, c, 2); no cooldown.
- seg13:0xa01c [BYTES seg13:0xa059-0xa175]:
  - an Enforcer target gets 0x40; a non-teammate or non-ship target gets 0x36 from (7, 0);
  - busy (+0x9c > now) with sbyte +0x5d > 0: 0x35;
  - the teammate also gets +0x94 = now + 500, +0x58 = c, +0xed = 1 and +0xa4 from sbyte +0x5e; c's +0x9c = now + 15000;
  - message 0x13 goes to the old commander, from (14, t); it sends msg(t, 14, c, 2).

##### Defenders
- seg13:0x9a18 returns at once when the target record is negative, at least the pool count (DGROUP:0x9406 + 2p), or its object is not in state 0. It skips entries already in state 15 on the same target and entries whose pool-2 object is not in state 0, then plays seg10:0x754a(0x15, 2, last k, 1) [BYTES seg13:0x9a26-0x9ad7].
- seg13:0x9ae8 also clears defender object word +0x1e and plays the same sound for the last k [BYTES seg13:0x9b7f seg13:0x9b98-0x9ba7].
- Mover state 15 [BYTES seg8:0x8f5b-0x9000 seg8:0x83e9-0x83f8]:
  - a target record above the pool's allocated count DGROUP:0x92ec + 2·pool (`jge` passes a record equal to the count), or not in state 0, becomes (12, 1); the first case also raises DGROUP:0x9498 [BYTES seg8:0x8f68-0x8fb0];
  - the steering runs only on frames with DGROUP:0x9452 ≠ 0 and entry dword +0x0e (DGROUP:0x530c) < now, strictly and unsigned; on the other frames object word +0x28 steps 0..15 while that dword > now [BYTES seg8:0x8fb4-0x8fd6 seg8:0x844e-0x847e];
  - a ship target with byte +0xe8 ≠ 0 is steered at ship words +0x14a/+0x14c instead of its object position;
  - after the thrust, heading +0x28 = seg8:0x19b0(vx add, vy add): seg8:0x285a(&[bp−0x36], &[bp−0x38], defender, target) leaves the x and y offsets from the defender to the target in cells, and the Pascal seg8:0x19b0 takes the x add as its first push [BYTES seg8:0x8325-0x836f seg8:0x83e9-0x83f8].
- The mover's head, once per call before its loop: DGROUP:0x9452 = 0; once dword DGROUP:0x186c < now (`jae` skips) it becomes now + sext(max((55 − DGROUP:0x91f8) << 2, 110)) (16-bit shift) and DGROUP:0x9452 = 1; DGROUP:0x1874 counts 0 … 4 (≥ 4 → 0, else + 1) [BYTES seg8:0x8277-0x82c0]. Its loop head runs for every entry k < DGROUP:0x940a, and an entry whose pool-2 object is not in `state` 0 goes to the next k before the state switch [BYTES seg8:0x8ecb-0x8f57].
- seg8:0x9028(k, s, pool, rec), cdecl, orders defender k: it returns at once when the entry's owner pool +0x06 is 14 and either its state +0x00 is 15 or pool = 14 with rec = the owner's rec; when (pool, rec) is the owner; and when pool = 2 and entry rec's owner words equal entry k's. Then s = 5 sets word +0x02 (DGROUP:0x5300) = 0 and raises the state to 5 (`cmp 5; jg`); any other s raises the state to max(state, s) (`cmp ax, s; jge`); the target words +0x0a/+0x0c take (pool, rec), and s = 15 clears the homing word DGROUP:0x52f6. So Alt+E leaves a defender already in state 15 on its target [BYTES seg8:0x9028-0x90f8].

##### seg13:0x7ba2 (reactions), beyond claims 50 to 55
- It returns when the ship holds order 1 or 5 with a zero grudge toward its +0x56, has +0x26 > 320 and no object attacker [BYTES seg13:0x7c0d-0x7c30 seg13:0x7d2e-0x7d45].
- Flee or repair is skipped when +0x38 = 4, or when +0x52 = 3 with +0x26 ≥ 20 [BYTES seg13:0x7d49-0x7d63].
- The trigger needs either 6·count·sbyte(+0x5e) + 15 > +0x26, or that value + 2 > +0x28. It also needs +0x52 = 0 or +0x98 − 7000 < now [BYTES seg13:0x7d67-0x7da9].
- The flee is chosen when 20·count + 35 > distance or 15·sbyte(+0x5e) + 20 > +0x26; otherwise the repair test runs: +0x2a < 50 and 8·sbyte(+0x5e) + 35 > +0x26 [BYTES seg13:0x7dad-0x7e45].
- With +0x38 = 4 and 20·count + 35 < distance: +0x36 = +0x38 = 1 and seg4:0x8346(i, +0x32, +0x34, 0x10) [BYTES seg13:0x7f06-0x7f31].
- After that, seg13:0x9bb8(i, count) ≠ 0 calls seg16:0xb4e(i) [BYTES seg13:0x7f39-0x7f4d].
- With +0x26 < 20·count + 25 and k known: it returns if another in-play ship (not P) already holds order 1 for i. Otherwise it calls seg13:0x81c2(i), and on 0 sets +0x9c = 1 [BYTES seg13:0x7f53-0x7fbf seg13:0x800d-0x801c].
- Otherwise, with count ≠ 0 and k known, it attacks k (seg3:0xf4(i, 14, k), seg13:0x9970(i), and +0x9c = 1 on 0) when any of these holds [BYTES seg13:0x7fc1-0x801c]:
  - count > 1;
  - distance < 78;
  - +0x32/+0x34 = (14, DGROUP:0x952e) and DGROUP:0x9e49 ≠ 10.
- FUZZY ONES (team 2) with threats ask for help the same way [BYTES seg13:0x8020-0x80a2].
- With no threats, or distance > 60, and +0x38 = 4: +0x36 = +0x38 = 3, +0x52 = 0 and +0x94 = now. seg13:0x9bb8 is then checked again [BYTES seg13:0x80a6-0x80ec].
- Revenge: when +0x38 = 1, +0x32 = 14, byte i+0x13e+k < −2, A(i, k) < −150 and i's own +0xec ≠ 0, it targets k and calls seg13:0x9970(i) [BYTES seg13:0x80f2-0x814d].

##### seg13:0x9bb8 (ambulance time), other conditions
- It returns 0 for a computer ship with +0xdc + +0xce < 250 [BYTES seg13:0x9bd5-0x9bf0].
- When the ship and the player hunt each other (either direction), it returns 0 unless +0x82 ≥ 1700 and +0x26 ≤ 1 [BYTES seg13:0x9bf2-0x9c34].
- It returns 1 and marks the wave when any of these holds [BYTES seg13:0x9c44-0x9cd5]:
  - +0x28 ≤ 0 or +0x2a ≤ 0;
  - +0x26 + +0x28 + +0x2a < clamp(20·(n + 2) + (+0xdc + +0xce)/10, 70, 180);
  - +0xce > 200 and 35·(DGROUP:0x9334 + 20) < +0xdc + +0xce;
  - byte +0xac = 0 or byte +0xad = 0, with +0xdc + +0xce > 500.

## Further readings of the pilot
- **Cadence of the tick block.** Every tick call sits inside seg7:0xdaf-0x117d, entered when the player's dword +0x156 is not above now (`ja` exits only when strictly greater) and DGROUP:0x951a = 0, which then sets +0x156 = now + 1500 ([world.md](world.md#the-1500-ms-tick-body-seg70xdef-0x1174)). seg13:0x675e after its loop calls seg13:0x50b0(0, 0) always, seg13:0x58dc(0, 0) when the word DGROUP:0x6e18 + 0x1e·DGROUP:0xa76 ≠ DGROUP:0x9324, and seg7:0x64ac(0, 0, −1) (cdecl), before the All Ships post [BYTES seg13:0x67f2-0x6821].
- **More callers of the think.** seg4:0x78 is also called by the steer at seg4:0xcd2 when the estimate pool +0x06 = −1, and by seg3:0x1d0a at seg3:0x2226 after it zeroes +0x94 and sets +0x52 = −1 (not for the player while DGROUP:0x95bc ≠ 0), so a re-plan can happen outside the damage-drop frames [BYTES seg4:0xcc4-0xcd2 seg3:0x21fe-0x2226].
- **The Enforcer's re-plan clock.** seg13:0x6900 (Pascal, one argument) sets +0x94 = now + 4000 and, for a team-7 ship, calls seg13:0xa6bc(i), whose first act is +0x94 = now + 750; seg13:0xa6bc's only caller is seg13:0x693a. So an Enforcer whose order is ≥ 1 re-plans about every 750 ms of the DGROUP:0x1e50 clock, at the think's own frame cadence [BYTES seg13:0x690f-0x693a seg13:0xa6cb-0xa6e1].
- **seg13:0x2714.** After the clamps it posts PostMessage(DGROUP:0xa7c, WM_PAINT, 0, 0) when b is the player, the player's word +0x40 = a and DGROUP:0xa76 = 2 (seg13:0x2d6e-0x2d96). With prop ≠ 0 a loop over the other ships k ≠ a, b in play and alive with |T(team k, team b)| < 350 spreads the change to k+0x62+2b through seg13:0x2688, seg13:0x2db8 and seg13:0x2da4, with notice 0x51 to b in one branch (seg13:0x2abb-0x2d0b); only its per-k clamp is checked (the rest of the loop) [OPEN].
- **The think, in detail.** In a kind-6 wave it returns at once while byte DGROUP:0x9eb5 = 0; otherwise it clears byte +0x61 (seg4:0x87-0x9c). A team-7 ship whose goal is a team-7 ship gets dword +0x32 = 0, +0x94 = 0 and +0x52 = +0x54 = +0x56 = 0 (seg4:0xa1-0xe1). The Demon's Gate block (seg4:0x174-0x249) and the kind-7 block (seg4:0x24f-0x382) rewrite ship 1's and this ship's goal words. In the re-plan, before the choice: an order whose target object is out of play resets +0x52 = 0, with a kind-7 retarget loop (seg4:0x3ec-0x49e); after it, an estimate pool below 0 makes dword +0x06 = 0 (seg4:0x5ef-0x5fb); the player's seg3:0xf4 notice with DGROUP:0xa62 = 1 when DGROUP:0x95bc = 0 and seg3:0x0(estimate) ≠ 0 (seg4:0x604-0x63e); then, outside kind 6, seg4:0x4d00(i) for a ship whose +0x52 is not 3 or 4 (or in kind 7), else seg13:0xa65a() (seg4:0x641-0x681). **The Enforcer self-order** (seg4:0x6ef-0x814): a team-7 ship with +0x52 < 0x14 and goal pool not 11, 12 or 4 gets +0x52 = 0x16, then by class (byte +0x5e) and byte +0xcd: class < 2 with +0xcd ≥ 0 → target +0xcd (+0x54 = 14, +0x56 = +0xcd) or, when that ship is dead, the notice seg3:0x6ec(player, 14, i, 0xb7) and the gate estimate (12, 0); otherwise goal pool 14 or class 0 → +0x54 = 14, +0x56 = +0x34, with +0x52 = 0x17 for class 0; otherwise +0x52 = 0x14, or 0x16 for class 1 when +0x54 = 14. Then byte +0xed = 2, +0x58 = i, +0x94 = now, +0x38 = 3 and seg13:0x6900(i). Weapon class 2 (byte +0x5f = 2) with +0x38 = 1: under 160 cells the ship brakes (+0x3a = +0x3c = 5, +0xca = 0) unless one of its pool-6 records (DGROUP:0x66e8 + 0x1c·k = 14, DGROUP:0x66ea = i, `state` 0) exists; over 210 cells it sets +0x3a = +0x3c = 2; between it keeps 5; with +0x38 ≠ 1 it clears +0x5f (seg4:0x994-0xa47). The ship DGROUP:0x952e returns before seg4:0xa4c while 1 ≤ DGROUP:0x9e49 ≤ 6 (seg4:0x97c-0x990).
- **The steer, in detail.** It returns for the ship DGROUP:0x952e while 1 ≤ DGROUP:0x9e49 ≤ 6; adds 1 to word DGROUP:0x1cb8 + 2i; in a kind-6 wave with DGROUP:0x9eb5 = 0 or goal pool 0 it calls seg4:0x144c(i, Detail > 0 or that counter odd) (cdecl), copies the goal into the estimate and calls seg4:0x4fc8(i) unless `PlayMode` = 2 (seg4:0xbed-0xc89). With the estimate pool −1 the estimate locals [bp−0xa]/[bp−0xc] are read before the seg4:0x78 call of seg4:0xcd2 and not re-read, so the aim would be read through the far pointer at DGROUP:0x1df4 (a null selector) and seg13:0xd6c would get pool −1 (whether +0x06 = −1 is reachable at steer entry; no direct store of −1 to +0x06 appears in the listings) [OPEN]. The gate detour sign is + when the ship's object word +0 exceeds the x of seg8:0x29ea(pool-12 record r's position), else −: the record offset di = r·0x70 indexes the pool-12 block, the record whose x the detour moves (seg4:0xcf8-0xdd9) [BYTES seg4:0xd1b seg4:0xd21 seg4:0xd6b-0xd71 seg4:0xd96-0xdb6]. The brake orders ([bp−0x16] = 0) before the turn: with +0x36 ≠ 2 under 220 cells, +0x3a = 7 for (estimate 14 and +0x52 = 5) or +0x52 = 0x14, else seg11:0x5ab6(i, 5) for an estimate-9 goal-9 attacker under 180 cells, or when the target ship's estimate is (14, i) and the ship's own |vx| or |vy| (pool-14 object +0x0c/+0x0e of i) is above 34; with +0x36 = 1 beyond 100 cells at `Detail` > 1, seg11:0x5ab6(i, 5) when (vy·[bp−8] + vx·[bp−6]) / distance < −50, a 32-bit divide (seg4:0xe15-0xf1e). The kind-6 race block (seg4:0xf3f-0x1016): with +0x3a 3 or 4 and DGROUP:0x9eb5 = 0 the estimate becomes the goal, byte +0x149 moves by 2, +0x3a = +0x36 = 2, +0x61 = 1 and the aim is the goal object's position.
- **seg13:0xd6c.** Only modes 2, 3, 4, 5 and 0x1e (and the race forms) write the wanted heading [bp−8]; for any other mode the routine compares the stale stack word with 0 … 15, an undefined read (seg13:0xe2f-0xf87). With seg8:0x19b0(x, y) the heading of a vector and `sar` for the quarters: mode 2 with a0 = 0 uses (dx, dy), mode 3 (−(dx/4 + dy), dx − dy/4), mode 4 (dy − dx/4, −(dx + dy/4)), modes 5 and 0x1e (−dx, −dy). Mode 0x1e or S = −100 returns h directly; otherwise the heading steps one unit toward h (h below the heading: −1 when the gap is under 9, else +1 wrapping 15 → 0; h above: +1 under 9, else −1 wrapping −1 → 15), and seg13:0xd6c itself calls seg13:0x6716(S) whenever h differs from the heading, before the steer's own seg13:0x6716 call. dx and dy are taken in 1/8 cells and wrapped by half of DGROUP:0x5a0/0x5a2 outside kind 6 [BYTES seg13:0xda5-0xe2c seg13:0xf96-0xffd].
- **seg11:0x5ab6.** The ring scaling (seg11:0x5cfc-0x5ddf), at `Detail` > 1 before the exceed test: for the first k < DGROUP:0x9344 with d = seg8:0x285a(16-byte entry DGROUP:0x4f30 + 16k dword +6, own position) below r + 100, r = entry word +0 >> 3: cap ×7/3 when r − 40 < d < r + 40, else ×2/3. What the DGROUP:0x4f30 table holds is [OPEN]. Entry exits: kind 6 with byte DGROUP:0x9eb4 = 0, and the ship DGROUP:0x952e while 3 ≤ DGROUP:0x9e49 ≤ 6 and DGROUP:0x9130 = 0, return at once (no thrust at all).
- **The hazard step.** An off-screen ship's estimate is reset to its goal on every call (seg4:0x5443-0x5464); on screen it asks seg13:0x1076(14, i) (Pascal) for a threat, sets the estimate to it with +0x36 = 3 (+0x3a = 5 for a pool-8 threat whose word DGROUP:0x3d5a + 0x34·rec = −3), and with a pool-6 distance [bp−0xc] under 160 fires the first slot k = 1 … +0xcb whose weapon has W+0x46 ≠ 0 through seg4:0x584c(k, i, 0); for weapon 2 only with the tether idle (object +0x36 = −1) and a launch-kind test W+0x0a ∈ {1, 5} on the pool-6 table entry DGROUP:0x66e4 + 0x1c·k indexed by the slot number k (seg4:0x539e-0x5408). The threat search is read in "The threat search of seg4:0x4fc8".
- **The player's activity estimate.** The +0x52 command state machine: +0x52 = 1 sets a flag b = 1 unless some ship's goal (+0x32, +0x34) equals P's (+0x54, +0x56); seg4:0x84de() is called always; +0x52 in 2 … 4 with P's estimate (+0x06, +0x08) = (+0x54, +0x56) (or +0x06 = 9 and `rand()` % 5 = 1, the draw made only then) and +0x36 = 1 → +0x52 + 1; with +0x54 = 14 and frame#/10 − sext(ship (+0x56) word +0x48) < 40 → +0x52 = 4; +0x52 ≥ 4 → seg13:0x7b40(+0x58) when +0x58 ≠ P, else seg3:0x6ec(P, 14, c, 0x1f) when +0x54 = 14 and ship c = +0x56 has +0x52 = 2, +0x56 = P and +0x58 = c; then +0x52 = 0. +0x52 ≠ 0 → −1 when b is set or when dword +0x98 + 20000 < now and frame#/10 − sext(ship (+0x56) word +0x44) > 25; a −1 becomes 0 [BYTES seg4:0x59fa-0x5bac]. This is one reader's reading (a second reader) [OPEN]. The estimate: the candidates are the visibility list DGROUP:0x507c (pool byte) / DGROUP:0x50e0 (rec byte), i < DGROUP:0x9146, skipping (14, player), scored seg4:0x8626(pool, rec, 1), then the pool-12 odd records 2i + 1, i < DGROUP:0x9318; the best must exceed the previous best (start −40). A winner is taken when its score > 0, or when the latched DGROUP:0x9686 is 0 and the score > −40. The writes: with a winner +0x06 = pool, +0x08 = rec, +0x3a = 2; then +0x36 = 1 when DGROUP:0x9686 ≠ 0; else, when the goal object's byte +0x53 ≠ 0 or the pool is 12, +0x36 = 2 if the latched DGROUP:0x9688 ≠ 0; otherwise +0x36 = 4 and +0x3a = 5 if the latched DGROUP:0x968a ≠ 0, else +0x36 = 3. With no winner: dword +0x06 = 8 (+0x06 = 8, +0x08 = 0), +0x36 = 3, +0x3a = 2, return 0. With a winner in pool 14 and DGROUP:0x9686 = 0, DGROUP:0x9686 becomes 1 when the target's word +0x46 ≠ 0 and frame#/36 − sext(+0x46) < 10, or when the target's relation byte +0xc2 + player > 2 (signed). seg4:0x5c75 stores DGROUP:0x9686 = [bp−0x2e] unconditionally and [bp−0x2e] is written only when a candidate beats the best, so with no candidate above −40 DGROUP:0x9686 takes a stale stack word [BYTES seg4:0x5bbb seg4:0x5c11 seg4:0x5c5b seg4:0x5c75 seg4:0x5c78 seg4:0x5c99-0x5d8c].
- **The attack reaction's Enforcer branches** (classes, the warn and terminate orders) and **the fight-back of the arrest** are in [law.md](law.md).

- **The attack reaction, flag 1 with the player as attacker** (seg4:0x5ed0-0x5f39): when the player's goal is not (14, target) with +0x38 = 1 and the per-attacker byte DGROUP:0x7152 + 0x15a·target + attacker is 0, the routine returns when d < 33, when the target's word +0x46 is 0, or when frame#/10 − +0x46 has a high word ≠ 0 or a low word > 80 (+0x46 is the second word of the hit-time list +0x44 … +0x4c that seg4:0x7d36 shifts; on the gun path seg4:0x7d36 runs before the damage call, in the dispatcher after seg4:0x5e8e). Then DGROUP:0x92e8 ≠ 0 or DGROUP:0x9320 = 7 returns, and flag 1 with signed byte +0xf7 + 9·attacker < 9 calls seg13:0x2714(target, attacker, −8, −10 − d, 0, 1). For a fleeing target (+0x38 = 4) the path also needs DGROUP:0x92e8 = 0 and DGROUP:0x9320 ≠ 7 (seg4:0x5f3b-0x5f47); its only direct `rand()` is at seg4:0x5fbc, but the callees draw more (seg13:0x2714, seg3:0x6ec through the formatter's `R`, seg13:0xb6f2 → seg13:0xadbe → seg13:0xae68 → seg16:0x3784 → seg16:0x3b18, seg13:0x915c and seg3:0xf4); on the other branch (`rand()` % 4 > 1, +0x26 ≥ 40 or d ≤ 10) each team-7 ship j below DGROUP:0x9316 (`jge`, so not the last record) gets seg3:0x6ec(attacker, 14, j, 0x5c); every exit of this branch ends with seg13:0x2714(target, attacker, −13, −d, 0, flag) (seg4:0x6018-0x6029). For the other targets (seg4:0x6034-0x66f8): +0x26 < 41 returns unless flag = 1; flag 2 returns for an attacker of the same design team, for a target with +0x52 = 9 and when seg13:0x2688(target, attacker) > 100 (signed; the later test > 130 at seg4:0x60cc-0x60e1 can then never fire); attacker = target returns; a target with +0x52 in 2 … 4, +0x58 = itself and (+0x54, +0x56) = (14, attacker) returns; a per-attacker byte +0xc2 + attacker ≥ 20 is reset to 0 and returns; a target whose goal is already (14, attacker) with +0x38 = 1 returns unless its design team is 7 (seg4:0x6116-0x6136). When the design word DGROUP:0x96f0/10 exceeds the target's +0x1e and the attacker is the player, seg3:0x6ec(attacker, 14, target, 0xbe) goes out; with the byte +0xc2 + attacker at 4, d is doubled (the value is not read again) and, under the same DGROUP:0x3894 team bounds, seg13:0xb6f2(attacker, 3) runs, with any other value sound seg10:0x744c(0x3d) plays, and either way the byte becomes 4 and the routine returns (seg4:0x613a-0x61cb). Then flag ≠ 1, or signed byte +0xf7 + 9·attacker > 8, calls seg13:0x2714(target, attacker, −(d/2 + 7), −10 − d, 1, flag) (seg4:0x61ce-0x6202); a player target returns when DGROUP:0x9134 = 0 or DGROUP:0x95bc ≠ 0 (seg4:0x620a-0x6220). [BYTES seg4:0x5ed0-0x6031 seg4:0x6034-0x6283 seg4:0x63c7-0x66e3 seg4:0x5f3b-0x5fdd seg4:0x5fe2-0x6015]
- **seg13:0x78b8(i, j), the answer to a call for help** (cdecl), for each attacker k of j it weighs: la = seg4:0x5d9c(i, k) + (allies·la)/−5 (i's allies byte +0x5d), doubled when **j's** team is i's or i is a Knight (team 4), +50 when i was called in for j (byte +0xcd = j); la < 50 answers −2 [BYTES seg13:0x79a2-0x7a00]. The rest of the routine is [OPEN].
- **The rivals' amount.** The 0x49 branch of seg13:0x3008's goal loop runs only for pool 0 or 11, so the call is always seg4:0x5e8e(k, ship, 90, 2), whose first step turns 90 into 50 ([ship-contact.md](ship-contact.md)).

## Oracle observations
- **A revisited directory** places its ships at wave start, off screen, in `state` 0, with a random velocity; a new directory starts them off-world (`state` 100, x = −1000) and the first enters 4.93 s after the start at the player's gate, the second 5.0 s later [ORACLE ships-0001].
- **Goals of two friendly computer ships over 100 s** were rocks first, then fuel tanks, then icons; the target display printed "I'M APPROACHING ROCK / THERE'S NOTHING BETTER TO DO.", "I'M ATTACKING ROCK. / TARGET PRACTICE." and "I'M APPROACHING FUEL TANK / ICON …" [ORACLE ships-0001].
- **A teammate's icon capture is credited to the player**: the score rose by 4000 and the resources word by 77 with "GOOD ICON CAPTURE! 0.77 RESOURCE GAINED" while the player rested 6400 units away [ORACLE ships-0001].
- **A hostile ship shot the player** 13 s after it entered a new-directory wave, and the debriefing named the RENEGADES [ORACLE ships-0001].
- **No Enforcer came in seven scripted sessions of wave 1:** the crime counters DGROUP:0x9648 and DGROUP:0x964a stayed 0 while rocks and fuel tanks were destroyed [ORACLE ships-0002]. The arrests that were recorded are in [law.md](law.md) [ORACLE ships-0003] [ORACLE ships-0004].
- **The commands** were given to a Teammate, a Platypus and a Pirate: Ctrl-H made the Teammate defend the player within 0.3 s, Ctrl-A at an icon sent it to attack the icon "AS REQUESTED BY" the player for 12 s, and a Pirate answered Ctrl-F with "I LOVE A GOOD FIGHT! PREPARE TO DIE, FOOL!" and went to order 2 within 2 ms; the Platypus answered Ctrl-A at a ship with 1858, and Ctrl-E at the Platypus drew 430 from the Ambulance [ORACLE ships-0005]. Which ship answers is not the watched or targeted one: the reading of the command routines above gives the order of the first free willing ship [OPEN].

## Not yet read
- **Teams and attitudes:** seg13:0x2714's callees seg13:0x2da4, seg13:0x2db8 and seg13:0x2e60; seg4:0x474c; seg13:0x78b8 past the help answer's first terms; seg4:0x485c past the carried [bp−0x12]; seg4:0x7e2a's Knight body; seg4:0x6a78; seg13:0x218a's INI section DGROUP:0x1354 (an empty string in the image) [BYTES seg13:0x21ec].
- **Commands and F12:** the Strategy player's DGROUP:0x9134 2–4 steps (the Strategy routines seg13:0xa186 … 0xa568 are read above), seg3:0x33b4 (the order notice), seg13:0x2f30 and seg7:0x1f38 (the command line).
- **Hazards, viruses and defenders:** the table DGROUP:0x4f30 (DGROUP:0x94e2); the defenders' mover states 0, 5 and 10 (seg8:0x8728-0x8b17); seg4:0x7e2a's three score formulas (the gates and branches are checked, the scores are not).
- **Strategy mode:** every "the player with DGROUP:0x9134 ≠ 0" branch of the pilot; seg4:0x584c's DGROUP:0x9524 and seg11:0x2f8c.
- **Races and duels:** seg4:0x144c, seg4:0x277a, the race branches of seg4:0x78, seg4:0xbde, seg13:0xd6c (seg13:0xe54-0xef1) and seg11:0x5ab6; the duel retargets of seg4:0x78.
