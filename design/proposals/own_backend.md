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

## The shape

One shared Spite IR, where Spite's own optimisations run once, and a thin lowering per architecture and per
operating system below it. C is one more lowering of that IR until stage 4 retires it for `--optimized`.
