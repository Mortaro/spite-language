# Spite's own backend and linker: the build plan

**Status:** approved direction (D397, D535, D558), first version not built. Every rule here is proposed by Claude
unless a decision number says otherwise. Work happens on the branch `backend-experimental`, never on master, until
the backend wins its benchmarks (D535).

This file is written for an AI agent (including a free model run through opencode) to implement step by step. Each
milestone is small, has an exact acceptance test, and leaves the compiler working.

## Goals

1. **Our own code generator and our own linker** for x86-64 on Windows (PE/COFF) and Linux (ELF) first, macOS
   (Mach-O, then ARM64) after. No LLVM, no system linker in the default path.
2. **A fast development tool first.** An unoptimised build must compile and link in a fraction of what C plus clang
   takes today, so the edit, build, run loop is near instant. Hot reload becomes code written straight into the
   running program's function slots by our own linker (D322), with no C compiler in the loop.
3. **Specialised, so faster than LLVM even when optimising.** Spite knows what C cannot say (no aliasing it did not
   create, proven bounds, ownership, whole-program types, D520 representation per use). The optimising mode uses
   those facts directly and skips the generic passes LLVM must run, so `--optimized` builds are faster to make and
   the programs at least as fast.
4. **Measured on every benchmark.** Each folder in `benchmarks/` gains a form per target: the naive Spite program
   built by our backend, timed against the same program's C (the compiler's `generated.c`, `naive.c` and `expert.c`)
   built by clang with every optimisation (D535). A benchmark case is the backend's acceptance test.
5. **C stays as a fallback.** Any function or platform the backend cannot handle is compiled through C as today, and
   our linker links clang's object files with ours. C remains the path for targets we do not support.

## Rules for the agent doing this work

- Work only on the branch `backend-experimental`, in its own worktree (for example `D:\w\be`). Merge `master` into it
  often; never push to master.
- **Every commit names the AI that wrote it**, so each change has an owner: start with a gitmoji, then a lowercase
  imperative summary, and end with a trailer naming your actual model, for example
  `Co-authored-by: big-pickle via opencode <noreply@opencode.ai>`. The commit hook refuses a commit without one,
  and refuses em dashes anywhere in the message.
- Read `AGENTS.md` and `SPITE.md` first. The backend is written in Spite, in `bootstrap/source/backend/`.
- Each milestone ends with its acceptance test passing and `bash check.sh` still green (the C path must never break).
- **Nothing in the compiler exists only for a human to read** (D561). Humans skim the final program, not the
  compiler's steps: the IR has no text format, pretty printer or readable names unless a test or a diagnostic needs
  them; the generated C and machine code need no comments, layout or naming meant for people. What a crash report
  or an error message must name (D244) stays, because that is for whoever reads the failure. Spend the effort on
  speed of the build and of the program.
- When unsure, keep the C path: a function the backend refuses is compiled through C, never miscompiled. Anything
  that can go wrong silently is a bug (D244): a wrong instruction is worse than a slow build.

## The shape

```
Spite source -> parser -> checks and proofs (as today) -> backend IR -> per-target code -> our object -> our linker
                                                      \-> C (fallback, as today) -> clang object ---------/
```

- **Two levels of IR, shared by every output (D560).** A high-level IR keeps Spite's own operations: a walk over a
  list, a template step, a field of an object, a count up and down, a waiting call, a reduction. The naive-to-expert
  optimisations run there (layout per use, fusion, bands, ownership, waits), because those facts are lost once the
  program is flat. It is lowered to a low-level IR held in flat arrays (functions, blocks, typed instructions on
  virtual registers), where instruction selection and register allocation run. **The C generator is fed from the
  same IR**, so every proof is written once, on the IR, for both C and our backend, and the proofs that read the
  generated C today move onto it.
- **Code generation:** direct instruction selection to x86-64, a simple register allocator (linear scan) in the
  development build, a better one in the optimising build.
- **Object and linking:** our linker reads our objects and COFF or ELF objects from clang, resolves symbols, applies
  relocations, writes an executable with an import table (Windows) or dynamic section (Linux).

## Milestones

Each is a separate pull of work. Do them in order; stop and report at the end of each.

### M0: a hand-built executable (Windows)

Write `bootstrap/source/backend/windows_executable_writer.spite`: given machine code bytes for `main` and a list of
imported functions from `kernel32.dll`, write a valid Windows executable. Acceptance: a test program builds an
executable whose code calls `ExitProcess(42)`; running it exits with status 42. Add it to `check.sh` as a step that
runs only on Windows.

### M1: the same on Linux

`linux_executable_writer.spite`: a static executable that makes the `exit` system call with 42. Acceptance as M0, run
only on Linux.

### M2: x86-64 encoder

`x86_encoder.spite`: encode the instructions the IR needs (mov, add, sub, imul, idiv, cmp, jcc, jmp, call, ret,
push, pop, lea, the SSE moves and arithmetic for `Float` and `Double`). Acceptance: a table-driven test that
encodes each instruction and compares the bytes with a table checked by hand (taken once from an assembler).

### M3: the two-level IR and a first lowering (D560)

`high_ir.spite`, `low_ir.spite` and `lower.spite`. The high-level IR is built from the compiler's typed tree and the
facts the proof passes record; keep Spite's operations as operations (a list walk is one instruction with a body,
not a hand-made loop). The high level serves the compiler's own optimisations, not only code generation (Mortaro,
2026-10-09): control flow stays explicit, a loop is one operation rather than a loop of instructions (a counted
`while` included) so that a loop's body can be cut into band pieces, and each value carries what it reads and
what it writes, per field and per owning object. Those facts live on the IR, so the analyses that read the
generated C today (`overlap_facts.spite`, the plain counts of `count_sharing.spite`, the class pools of
`class_pools.spite`, the lost-write proof of D554) move onto it instead of being written a second time. The first
two users are what a function surely wrote when it answered `true`, and whether the passes of a loop write
different slots (D559, its four steps in [handoff.md](../handoff.md)). M0 to M2 are built first. Lower it
to the low-level IR for the smallest useful subset: whole-number and decimal locals,
arithmetic with Spite's overflow checks (a checked operation jumps to a crash path that reports like the C path),
`if`, `while`, calls to the program's own functions, returns. Then emit **C from the IR** for that subset and keep the
current generator for everything else, function by function. Acceptance: a conformance program with a recursive
function and a loop prints the same three ways (today's C, C from the IR, and, once M2 is done, our machine code);
`bash check.sh` stays green with the C-from-IR path on for the functions it covers.

### M4: mixed builds

A function the backend cannot lower yet is generated as C and compiled to an object by clang, as today. Our linker
(`linker.spite`, COFF first) links our object with clang's. Acceptance: `bash check.sh` runs the whole conformance
suite with `--backend=own` in mixed mode and every program prints the same as the C path; the report lists how many
functions each path compiled.

### M5: objects, lists and text

Lower reference counts, field reads and writes, list items, text joins, calls into the library. Each family moved
from C to the backend is one commit with its conformance programs. Acceptance: the share of functions compiled by
our backend in the conformance suite goes up, and the suite stays identical.

### M6: hot reload through our linker

In a `--hot-reload` build, a changed function is lowered, linked against the running program's symbols and written
into its slot (D322), with no C compiler. Acceptance: the existing live reload checks in `check.sh` pass with
`--backend=own`, and a reload of one function takes under 100 ms.

### M7: Linux linker and ELF objects

The same linking for ELF. Acceptance: M4 and M6 on Linux.

### M8: benchmarks and the optimising mode

Add the backend form to every `benchmarks/<case>/` (D535): `scripts/cases/` builds it, `benchmarks/run.sh` times it
against `generated.c`, `naive.c` and `expert.c` at clang `-O3 -march=native` with link-time optimisation, and the
README tables gain the column. Then start the optimising mode: use the compiler's proofs (no alias, proven bounds,
ranges) for register allocation, unchecked operations and vectorising. Acceptance: the table exists for every case;
the build time of every case is measured for both paths.

### Later

macOS (Mach-O, x86-64 then ARM64), ARM64 on Linux, the backend as the default when it wins (D535), and the proofs
moved off the generated C onto the IR (the start signal in [own_backend.md](own_backend.md)).

## What to measure at every milestone

| Measure | Why |
|---|---|
| build time of the conformance suite, own backend against C plus clang `-O0` | the development tool must be faster |
| share of functions compiled by our backend | progress of M5 |
| run time of each benchmark case, own backend against clang `-O3` | the optimising mode's goal |
| reload time of one changed function | M6's goal |
