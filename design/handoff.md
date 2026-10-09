# Handoff: where the next agents pick up (2026-10-09)

Read this after `AGENTS.md`, `SPITE.md` and `design/naive_programs.md`. It is agent-owned state: rewrite it when you
hand off again.

## The goal right now

**The naive engine replaces the engine package's main branch as soon as it is at least as fast on every
benchmark** (D550). Everything else waits unless it moves the engine or is cheap. The rule for every optimisation:
any method counts if the printed answer is the same, the fastest wins (D549); nothing may go wrong silently (D244);
nothing ships beside the program (zero runtime).

## Where the engine stands

Quiet measurement, compiler `579d35c5`, medians of 5 alternating rounds (design/naive_programs.md, "Quiet
measurement"):

| Benchmark | Original (main) | Naive |
|---|---|---|
| stress tick | 5,953 µs | 24,180 µs |
| stress despawn, sixty ticks | 23,915 µs | 20,909 µs (naive wins) |
| physics step | 5,691 µs | 6,941 µs, then 6,955 against 5,869 after the physics pass (D552, D553) |

Functionally the naive engine can now do everything main does: compiler-arranged waits (D554) make
`examples/io_systems` pass on the unchanged naive branch (18 frames while waiting; main 30). Both engine branches
compile with master (Dictionary<Key, Value>, plain bytes, MongoDB removed from the engine). What is left is speed.

## What is merged on master (4c298eb2)

Since the start of the naive-programs work (D507): plain counts (C5), inlined releases (C6), class pools (M6),
uncounted tested and held items (B1, B3, B1t), held attributes, write-back elision (B2), the in-place store (D544),
the range proof (D543), loop bands and rows of different classes at once (T1, T4b), S1 steps 1 and 2 (D523, D551),
lists held in their slot and lists of lists keeping their room (D552, D553), compiler-arranged waits with the
lost-write proof (D554), plain bytes (D540), Dictionary<Key, Value> (D541), Benchmark(work) (D545), the
wait-cycle race fix (D546). Every benchmark case has quiet timings in the table at the top of `benchmarks/README.md`.

## Work in flight, stopped on 2026-10-09 when Mortaro needed the PC

None of it is lost. Each item says where it is and what is left. **Re-run `bash check.sh` before merging any of
them**, because master moved under several, and **renumber decision rows at merge time** (numbers collide often:
re-read the end of `design/decisions.md` right before appending).

| Work | Where | State | Next |
|---|---|---|---|
| Per-object facts and counts per group (stress on two cores) | branch `wip-per-object-facts` (clone `D:\sf`, 7 commits, last check reported green after merging master; rows numbered D555, D556) | built: objects made for their owner told apart in the overlap facts; counts per group of overlapped calls | verify on stress that Move and Regenerate now overlap (by hand two cores were worth 1.75x on the stage); re-check, renumber, merge |
| In-place writes on attribute paths (stress, hand edit W, 4.1 ms) | branch `wip-attribute-paths` (clone `D:\s7`, 1 commit plus 7 uncommitted files) | "leave out a call that only writes back what its slots already hold" committed; engine checks agreed; the R measurement (row in locals, 5.5 ms) was next | finish docs and case, measure, merge; then R |
| Memory: deep copies shared when nothing writes (M7), pools for classes no list holds, arenas, presized lists | branch `memory-wins` (worktree `.claude/worktrees/agent-a19777fa565b84228`, 2 commits pushed, 155 uncommitted files) | M7 built ("share a deep copy that nothing changes while it lives"); its decision row (planned D555, will collide) not yet written | commit the rest, renumber, check, merge; then shrink List objects (40 bytes to 24 or 16: the physics broad phase's remaining cost) |
| Singletons: plain attributes under a held or skipped lock, per-thread reductions, files kept open | branch `singleton-wins` (worktree `.claude/worktrees/agent-a667c5b716ea97b51`, 1 commit pushed, 12 uncommitted files) | B built for attributes nothing reads past the lock | finish, check, merge; then A (privatised reductions) |
| Archetypes chosen by the compiler (research with experiments) | worktree `.claude/worktrees/agent-ad0a790df5f4cc7b0`, branch `research-archetypes`, nothing pushed, 2 uncommitted files | the sweep was running | restart the study; write `design/proposals/compiler_archetypes.md` |

## The order of work after that (by what closes the engine gap)

1. **Stress, one thread** (design/naive_programs.md, "Ninth pass"): of the hand edits on today's C (20.6 ms one
   file), R (the row in locals with the system inlined) is 5.5 ms and needs a whole-program proof that the row's
   attributes are dead between passes; F and M (the candidate list's producer and consumer fused, the repeated
   match dropped) are 2.7 ms. All five edits together reach 7.4 ms on one thread.
2. **Stress, two cores**: merge `wip-per-object-facts`; the original's 5.95 ms is two systems at 5.6 ms each at once.
3. **Physics** (design/naive_programs.md, "Physics pass"): smaller List objects; stamps instead of a visited list
   (about 0.3 ms); the runner's fill and store (about 0.5 ms, shared with stress); a flat counting sort needs a
   proof that the grid's filling loop is safe to run twice.
4. **Per-field layout** (L1, L2, L3, S2 with M8, an object merged into its only owner): every research batch names
   it the largest single win across the benchmark cases; it also serves physics. Not started.
5. **Ranges of attributes and list items** (A1 extended, then speculate-and-replay): the gate to vectorising.
6. Then the cases still behind expert C, worst first, from `benchmarks/README.md`.

When the engine is at least as fast everywhere, the naive branch replaces main (D550), after a quiet re-timing.

## How to work here (lessons from this session)

- **Agent isolation "remote" did not offload**: those agents ran in local worktrees on Mortaro's PC. Count every
  agent against the machine. When Mortaro needs the PC, stop all agents and kill their `check.sh` pools, including
  orphaned `timeout.exe` and `generation_two.exe` processes.
- **Only stop processes you started**, by your own clone's path. Never by name.
- **Run `check.sh` in a short-path clone** (`D:\s9`, `D:\w\x`): paths over 260 characters break the formatting step
  on Windows. A fresh worktree has no compiler yet, so build it before running `scripts/cases/extract.sh`.
- **Commit and push in small steps**: the PC restarted twice and the network dropped once in one day.
- **Benchmarks taken while the PC is in other use are provisional**; final numbers come from a quiet pass with
  nothing else running (the script used is described in design/naive_programs.md's quiet measurement).
- **The free research models** (opencode: big-pickle, space-bunny-free, muse-spark-1.3-contributor-free,
  mimo-v2.6-flash-free, ling-3.1-flash-free) run on their own servers. Start them from `D:\Projects` so they can read
  both the language and the engine; give hard questions a 2700 s timeout; verify everything they say before using
  it. Standing order from Mortaro: keep them researching the cases behind expert C until Spite beats it everywhere.
  Batch 9 (R24, R30, R32 retries and R33 on working out closed computations while compiling) was started; its logs
  are in the session scratchpad under `research/`.
- **Editing shared design files**: insert, never replace a heading by accident; diff the headings afterwards.

## Waiting on Mortaro

Everything decided by Claude is under "To confirm" in `mortaros_missing_decisions.md`; he asked to confirm the D536
items only after the speed goal is met. Open for him: whether a frame call refused by the lost-write proof should
be a compile error instead of waiting in place (D554), and generic functions (D545's alternative to
`Benchmark(work)`).
