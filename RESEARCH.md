# Donating agent time: research Spite needs

Spite is an experiment: how far can plain code, written by anyone (a "moron"), beat C tuned by hand, when the
compiler does all the optimising? Most of the work is done by AI agents guided by one person, and there is far more
worth investigating than one person's agents can cover. If you have agent time to give, this page lists what is
useful and how to hand back what you find.

Read [WHY.md](WHY.md), [SPITE.md](SPITE.md) and [docs/write_it_plainly.md](docs/write_it_plainly.md) first. The
running notebook of ideas and earlier findings is [design/optimization_research.md](design/optimization_research.md),
and the plan of work is [design/naive_programs.md](design/naive_programs.md).

## The rules every finding is judged by

- **The result must stay the same.** Any method counts: work done while compiling, another layout, another
  algorithm, no data at all. Only the printed answer must not change.
- **Nothing goes wrong silently.** A proposal that could give a wrong value, lose a write, leak or hang in some case
  is not an optimisation. Say what the compiler must prove, and what it does when the proof fails (the plain form).
- **No runtime.** Nothing ships beside the program: no scheduler, collector, interpreter or registry. A choice the
  compiler cannot make may become one branch between two forms compiled in advance, never more.
- **The program is never changed to go faster.** Speed comes from the compiler. A finding that says "write it
  this way instead" is a finding about a missing compiler proof.

## Research that is useful now

Each item says what to find out. Read-only research (reading the compiler and the benchmarks) is welcome; research
with experiments (hand-editing generated C, timing it) is better.

### Closing the gap to expert C

Every folder in [benchmarks/](benchmarks/README.md) holds one program four ways: plain Spite, plain C, C tuned by
hand, and the C the compiler writes. The table at the top of [benchmarks/README.md](benchmarks/README.md) ranks them.
For any case where Spite is slower than `expert.c`:

1. Compare `expert.c` with `generated.c` function by function and list what the expert does differently, ordered by
   what it is worth.
2. Edit `generated.c` by hand to apply one difference at a time and time it: that measures what each missing
   optimisation is worth.
3. Name the general optimisation and the proof the compiler needs, in the terms of
   [design/naive_programs_pairs.md](design/naive_programs_pairs.md) (naive code, proof, faster form, fallback).

The worst cases are listed at the top of the table; findings so far are under "Findings" in the research notebook,
so start from those instead of repeating them.

### Open questions in the notebook

- **Per-field layout:** when a list of objects should become columns, or blocks matching the vector width, and the
  proof that no use can tell (identity, `==`, reflection). Every research batch so far ranks this the largest win.
- **Ranges of attributes and list items**, and checking a sum once instead of every addition ("speculate and
  replay"): the gate to vectorising plain loops.
- **Archetypes chosen by the compiler:** grouping a list's items by which optional parts they have, with no
  "sparse" or "table" for the programmer to declare, and not only for games.
- **Loops split across every core in chunks that fit the cache**, including loops that write through an index:
  what the compiler must prove, and what one check at run time can settle.
- **Working out closed computations while compiling:** finding the parts of a program that read no input and
  reducing them to their answer, safely (no hang at build time, the same decimal bits as the target).
- **The GPU:** loops that could run as compute kernels with the data kept on the device, without the program ever
  mentioning it; the cost model and the rules for decimal results.
- **Learning from a test run:** what a profile of real use should record and which choices it should drive.

### The compiler's own representation

The compiler is getting a two-level intermediate representation that every output shares
([design/proposals/own_backend_plan.md](design/proposals/own_backend_plan.md), milestone M3). Useful research: how
the proofs the compiler makes today (by reading generated C) would look on such an IR; which facts the high level
must keep (loops as operations, reads and writes per field and per object, what a function surely wrote when it
answered true).

### Our own backend

A code generator and linker of Spite's own are planned on the branch `backend-experimental`, milestone by milestone,
each with an exact acceptance test. A milestone is a good self-contained piece of work for an agent: follow the plan
and its rules.

## How to contribute a finding

1. **Fork and branch.** Name the branch `research-<topic>` (or `backend-<milestone>` for backend work).
2. **Write the finding into the notebook.** Add it under "Findings" near the end of
   [design/optimization_research.md](design/optimization_research.md), with the date, a short title, and:
   - what you looked at (cases, files, with line numbers);
   - what you measured and how (machine, compiler commit, build flags, how many runs), or "read only, not measured";
   - the optimisation and its proof in the pairs format, and its fallback;
   - what you are unsure of.
   Mark it unverified unless you measured it. A finding that turned out wrong is still worth writing down, with why,
   so nobody retries it blind.
3. **Add a benchmark case if you built one.** A new case is a folder in `benchmarks/` with the plain Spite program,
   `naive.c`, `expert.c`, and its `generated.c` and `highlights.c` written by `scripts/cases/extract.sh`; it must
   pass `bash scripts/cases/check.sh <case>`.
4. **If you change the compiler,** the whole suite must pass (`bash check.sh`), every optimisation gets its section
   in [docs/optimizations.md](docs/optimizations.md) and every proof its entry in [docs/proofs.md](docs/proofs.md),
   in the same commit. Read [AGENTS.md](AGENTS.md) first; it has the rest of the rules.
5. **Credit the model that did the work.** Every commit starts with a gitmoji and ends with a trailer naming the
   model that wrote it, for example `Co-authored-by: Example Model 2 <noreply@example.com>`. The commit hook
   refuses a commit without one.
6. **Follow the writing rules:** no em dashes anywhere (not the character, not two hyphens between spaces), tables
   with one set of columns, no names of third-party packages. `check.sh` and the hook refuse them.
7. **Open a pull request** against `master` (or `backend-experimental`) that says in two lines what you found and
   how sure you are. Decisions about the language are Mortaro's; a finding becomes work once it is accepted.

## What not to send

- Changes that make a program faster by writing it differently: the program stays plain, and the compiler learns.
- Optimisations that need a runtime, or that change a result in any case.
- Unmeasured claims presented as measured ones.
