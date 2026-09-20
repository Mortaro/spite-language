# Bootstrap seed

`spite_compiler.c` is the C that the Spite compiler (the Spite sources in `bootstrap/`) emits for ITSELF. It is the
fixpoint: compiling it gives a compiler that emits this exact file again. It exists so Spite can be built with nothing
but a C compiler:

    cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite_seed
    ./spite_seed.exe --file=bootstrap/spite_compiler.spite --mode=c > next.c      # must equal the seed
    ./spite_seed.exe --file=path/to/program.spite --mode=run

Regenerate it whenever the compiler sources change (run from the repository root, because the compiler reads
`runtime/*.h` relative to the working directory):

    ./spite_seed.exe --file=bootstrap/spite_compiler.spite --mode=c > bootstrap/seed/spite_compiler.c

`bash check.sh` proves the fixpoint: it rebuilds this file from the compiler sources and requires the result to be byte identical. It does not read this copy.e result to be byte identical. It does not read this copy.e result to
