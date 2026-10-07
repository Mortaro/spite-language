#!/bin/bash
# Writes each case's generated.c and highlights.c (benchmarks/README.md): the compiler writes the C of the case's
# naive/ program as an --optimized build for Windows does; generated.c is all of it, with the compiler's own numbered
# names numbered again from 1 (scripts/cases/renumber.awk), and highlights.c only the definitions its functions.txt
# names, copied out of it (scripts/cases/functions.awk). Run from anywhere:
#   bash scripts/cases/extract.sh [--compiler=path] [--check] [case ...]
# With no case, every case. A file is written only when what the compiler writes now differs from it, so a case
# whose C did not change is not touched. --check compares instead of writing, and fails naming each case whose
# generated.c or highlights.c the compiler no longer writes; check.sh runs it that way. --from-c=file takes the C
# from a file instead of compiling (one case only). The compiler defaults to .spite/spite_development.exe, the one
# check.sh last built.
cd "$(dirname "$0")/../.." || exit 1
compiler=.spite/spite_development.exe
check=false
from_c=""
names=()
for argument in "$@"; do
    case "$argument" in
        --check) check=true ;;
        --compiler=*) compiler=${argument#--compiler=} ;;
        --from-c=*) from_c=${argument#--from-c=} ;;
        *) names+=("$(basename "${argument%/}")") ;;
    esac
done
if [ ${#names[@]} -eq 0 ]; then
    for folder in benchmarks/*/; do names+=("$(basename "$folder")"); done
fi
work=${CASES_WORK:-.spite/cases}   # check.sh gives each run its own
mkdir -p "$work"
failures=0
# compares the file the compiler writes now ($1) with the case's ($2): --check reports a difference, otherwise the
# case's file is replaced, and only when it differs
settle() {
    if tr -d '\r' 2> /dev/null < "$2" | cmp -s "$1" -; then
        return 0
    fi
    if $check; then
        echo "FAILED: $2 is not what the compiler writes now; run bash scripts/cases/extract.sh $name"
        diff "$2" "$1" | head -10
        failures=$((failures + 1))
    else
        cp "$1" "$2"
        echo "wrote $2"
    fi
}
for name in "${names[@]}"; do
    folder="benchmarks/$name"
    source_c="$from_c"
    if [ -z "$source_c" ]; then
        source_c="$work/$name.c"
        "$compiler" "$folder/naive" --check --c-source --optimized --target-operating-system=windows \
            --c-path="$source_c" > "$work/$name.log" 2>&1 || {
            echo "FAILED: $folder/naive does not compile"; head -5 "$work/$name.log"; failures=$((failures + 1)); continue; }
    fi
    {
        echo "/* All of the C the compiler writes from naive/ for this case, as an --optimized build for Windows does, with"
        echo " * the compiler's own numbered names numbered again from 1. Written by scripts/cases/extract.sh; check.sh"
        echo " * compares it with what the compiler writes now. */"
        echo ""
        awk -f scripts/cases/renumber.awk "$source_c"
    } > "$work/$name.generated.c"
    {
        echo "/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions"
        echo " * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered"
        echo " * names numbered again from 1. check.sh compares it with what the compiler writes now. */"
        echo ""
        awk -f scripts/cases/functions.awk "$folder/functions.txt" "$source_c"
    } > "$work/$name.highlights.c" 2> "$work/$name.extract.log" || {
        echo "FAILED: $name: $(head -3 "$work/$name.extract.log")"; failures=$((failures + 1)); continue; }
    settle "$work/$name.generated.c" "$folder/generated.c"
    settle "$work/$name.highlights.c" "$folder/highlights.c"
done
[ "$failures" == "0" ]
