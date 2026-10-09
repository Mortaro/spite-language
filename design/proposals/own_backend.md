# Spite without C: the roadmap (D397)

Mortaro's roadmap for making Spite independent of C, decided 2026-10-02. The stages run in order; each one is
built only after the one before it. The backlog lists them under "Later, in order".

## The stages

1. **No C runtime.** The standard library calls the operating system directly: `kernel32` and `ntdll` on Windows,
   raw system calls on Linux with no libc, and `libSystem` on macOS, which stays as the platform the way the
   platform's TLS does (D394, D395). Everything else the C library gives today (number formatting, memory, text,
   maths) is written in Spite (D350, D361).
2. **Released builds.** A moron downloads one Spite release for their operating system, the compiler and its
   library, and uses it at once, with no step that builds the compiler. The C seed stays only for building Spite
   from source.
3. **A development backend owned by Spite**, for default builds, hot reload and the REPL: a compact IR held in
   arrays, direct instruction selection, a simple register allocator, and executables (PE, ELF, Mach-O) written by
   Spite itself. x86-64 first, then ARM64. Hot reload becomes code generated into the running program through the
   function slots (D322): compiled on save, close to a JIT but with no interpreter and no tiers, and the compiler
   stays outside the program.
4. **Spite's own optimising backends, aiming to beat LLVM.** Mortaro's reasoning: code agents do the work, and a
   specialised goal is easier to win than a general one. Spite's IR keeps what C and LLVM lose: ownership (D354,
   D380), checks proven safe (D360), whole-program specialisation (D321) and how lists are stored (D331). Until
   Spite's own backend wins on the benchmarks, `--optimized` keeps going through C.

## Measured against LLVM on every benchmark (D535)

Every folder in `benchmarks/` gains one more form per target the own backend supports: the naive Spite program
built by Spite's own backend for that target. It is timed against the same program's C (the compiler's
`generated.c`, and the hand-written `naive.c` and `expert.c`) built by clang with every optimisation it has:
`-O3`, link-time optimisation, tuned for the machine it runs on (`-march=native`), and profile-guided where a
profile exists. Each README then shows, per target, own backend over LLVM, so it is always known where the own
backend stands. Once the own backend wins, it is the default for every build, and C stays only as one more form in
the benchmarks, for comparison.

## When to start (the signal Claude watches for, D535)

Mortaro asked to be warned when it is the right time. The signal is when these hold:

1. **The proofs no longer read generated C.** Several passes work on the C text today (`count_sharing.spite`,
   `class_pools.spite`, `overlap_facts.spite`, `item_values.spite`, the case extractor). An own backend has no C
   text, so those proofs must move onto the compiler's own representation first: the shared Spite IR of "The
   shape" below. This step is worth doing early anyway, because it makes every proof cheaper and per use (D520).
2. **The naive engine is at least as fast as the hand engine** through C, so the optimisations exist as compiler
   knowledge and not as luck in the C compiler.
3. **The benchmarks show which LLVM optimisations Spite relies on**: for each case, the C built at `-O0`, `-O1`,
   `-O2` and `-O3`, so the backend knows what it must do itself (vectorising, inlining, register allocation) and
   what Spite's own passes already did.
4. **No C runtime** (stage 1 above) is far enough along that a program does not need the C library.

Point 1 can start before the others; it is the first piece of backend work.

## The build plan

The step-by-step plan for the first version, written for an agent to implement, is
[own_backend_plan.md](own_backend_plan.md) (D558).

## The shape

One shared Spite IR, where Spite's own optimisations run once, and a thin lowering per architecture and per
operating system below it. C is one more lowering of that IR until stage 4 retires it for `--optimized`.
