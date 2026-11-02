# ADR 0015: The collision pair's orientation word

- **Status:** Accepted. [DESIGN]
- **Scope:** the one uninitialised read in the collision path, the orientation flag of the pair test seg8:0x6afa, and the rule the port applies where the original's bytes leave the value to the stack. The collision rules themselves are in [core.md](../re/core.md#collisions). [DESIGN]

## Context
- **The flag decides which party is A.** seg8:0x6afa writes its local [bp−0xc] by four tests in order, the last write winning: the candidate K is the player → 1, the caller C is the player → 0, K `fixed` → 1, C `fixed` → 0. Flag 1 makes K party A of the response seg8:0x546c and the bounce (ISPACE.DLL ordinal 29); flag 0 makes K party B. Nothing initialises the slot ([core.md](../re/core.md#pair-test-seg80x6afa), Roles A and B). [BYTES seg8:0x6b04 seg8:0x6d0e seg8:0x6d23 seg8:0x6d3d seg8:0x6d57 seg8:0x6d5c]
- **Where the slot lies.** The pair test is called from the pair finder seg8:0x50aa with a fixed frame, so [bp−0xc] is the word at seg8:0x50aa's bp − 0x30 on every call of one search, and only a hitting call writes it, after which the search returns. So every call of a search reads the word that lay at that address when the search began: the write of an earlier hit when nothing else used the slot since, otherwise the residue of whatever frame last reached that depth. [BYTES seg8:0x50b4 seg8:0x50b8 seg8:0x52f3 seg8:0x52f8 seg8:0x52fb seg8:0x6afd seg8:0x6b04 seg8:0x6cda seg8:0x6d0e]
- **The pairs it touches.** A pair with the player or a `fixed` object (a gate post) is decided by the four writes: against a rock or a post the player is party A except against a post, which is A itself. Rock against rock, rock against a computer ship and ship against ship are decided by the residue. A non-zero residue makes K party A. [BYTES seg8:0x6d0e seg8:0x6d57 seg8:0x6d5c]
- **What A and B change.** Only B's position is moved by the bounce, A's damage gains 5 × the rock's heat against B's 4 ×, and the response's two branches differ in their mass rules ([core.md](../re/core.md#response-seg80x546c), steps 3 and 8). So the residue changes where a rock ends up and how much it is hurt, not whether the pair collides. [BYTES seg8:0x5611 seg8:0x5870 seg8:0x5824 seg8:0x5a59] [BYTES ISPACE.DLL seg2:0x3c]

## Decision
1. **A persistent word stands for the slot.** The port keeps one word, the pair orientation, that the pair test writes with the original's four tests on every hit and that nothing else writes. Every pair test reads it as the original reads [bp−0xc]: the four tests overwrite it on a hit, a pair without the player or a `fixed` object keeps the value of the last hit. The word starts at 0 at the wave start (seg9:0x60bc), so the first undecided pair makes K party B. [DESIGN]
2. **Decided pairs are exact.** The player and the gate posts take the original's orientation on every hit, so wave 1's collisions (the player against rocks and posts) match the oracle whatever the word holds. [DESIGN] [ORACLE core-0001]
3. **The rule is tagged at its site** with this ADR, and a Wine debugger break at seg8:0x6d5c that logs [bp−0xc] for undecided pairs can replace decision 1's "last hit" with the observed residue. [DESIGN] [OPEN]

## Alternatives
| Alternative | Verdict |
|---|---|
| Always K is A (a non-zero residue) | Rejected. It is as arbitrary as decision 1 and loses the one deterministic part of the original's behaviour, the carry-over from the last hit when the slot is undisturbed. [DESIGN] |
| Reproduce the stack | Rejected. The port has no 16-bit stack and the frames that reach that depth between searches are not known. [DESIGN] |

## Consequences
- M4 stage 1 implements decision 1 in the pair test with the word in the wave's globals, reset at the wave start. [DESIGN]
- The oracle comparison of rock-against-rock bounces is approximate until decision 3's break is run; the player's collisions are compared exactly. [DESIGN]
