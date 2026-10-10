# Bootstrap seeds

`<system>/spite_compiler.c` is the C that the Spite compiler (the Spite sources in `bootstrap/`) emits for ITSELF,
written for that system: `linux/` and `windows/`. Each is a fixpoint: compiling it on its system gives a compiler
that emits this exact file again. They exist so Spite can be built with nothing but a C compiler:

    cc -O2 -Wno-parentheses-equality bootstrap/seed/linux/spite_compiler.c -o spite_seed -lm
    ./spite_seed bootstrap --c-source --run=false      # writes .spite/build/bootstrap/bootstrap.c, which must equal the seed
    ./spite_seed path/to/program

There is one per system because the compiler knows the system it runs on as it was compiled (its own
`build.target_operating_system`): the Windows seed loads `kernel32.dll` and `ucrtbase.dll` when it starts. Nothing of
the compiling machine reaches the C, so a compiler on either system writes the other system's seed exactly:

    ./spite_seed bootstrap --c-source --run=false --target-operating-system=windows

The compiler's C goes to its default place, `.spite/build/bootstrap/bootstrap.c` (D283), and not to a `--c-path`:
every `Build` field is a constant in the compiler it describes, so a path given there would be written into the C
and no two generations would be equal. The compiler finds `library/` in a folder above its own executable, so build
it inside the repository.

`bash check.sh` proves the fixpoint on the system it runs on: it rebuilds this system's file from the compiler
sources and requires the result to be byte identical, then writes every other system's file too and says which
seeds are stale. `bash check.sh --update-seed` rewrites all of them.
