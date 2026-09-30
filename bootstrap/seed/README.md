# Bootstrap seed

`spite_compiler.c` is the C that the Spite compiler (the Spite sources in `bootstrap/`) emits for ITSELF. It is the
fixpoint: compiling it gives a compiler that emits this exact file again. It exists so Spite can be built with nothing
but a C compiler:

    cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite_seed -lm
    ./spite_seed.exe bootstrap --c-source --run=false      # writes .spite/build/bootstrap/bootstrap.c, which must equal the seed
    ./spite_seed.exe path/to/program

The compiler's C goes to its default place, `.spite/build/bootstrap/bootstrap.c` (D283), and not to a `--c-path`:
every `Build` field is a constant in the compiler it describes, so a path given there would be written into the C
and no two generations would be equal. Regenerate the seed whenever the compiler sources change (the compiler finds
`library/` in a folder above its own executable, so build it inside the repository):

    ./spite_seed.exe bootstrap --c-source --run=false
    mv bootstrap/bootstrap.c bootstrap/seed/spite_compiler.c

`bash check.sh` proves the fixpoint: it rebuilds this file from the compiler sources and requires the result to be byte identical. It does not read this copy.
