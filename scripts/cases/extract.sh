#!/bin/bash
# Writes each case's generated.c (benchmarks/cases/README.md): the compiler writes the C of the case's naive/ program
# as an --optimized build does, and the definitions its functions.txt names are copied out of it
# (scripts/cases/functions.awk). Run from anywhere:
#   bash scripts/cases/extract.sh [--compiler=path] [--check] [case ...]
# With no case, every case. --check compares instead of writing, and fails naming each case whose generated.c the
# compiler no longer writes; check.sh runs it that way. --from-c=file takes the C from a file instead of compiling
# (one case only). The compiler defaults to .spite/spite_development.exe, the one check.sh last built.
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
    for folder in benchmarks/cases/*/; do names+=("$(basename "$folder")"); done
fi
work=${CASES_WORK:-.spite/cases}   # check.sh gives each run its own
mkdir -p "$work"
failures=0
for name in "${names[@]}"; do
    folder="benchmarks/cases/$name"
    source_c="$from_c"
    if [ -z "$source_c" ]; then
        source_c="$work/$name.c"
        "$compiler" "$folder/naive" --check --c-source --optimized --c-path="$source_c" > "$work/$name.log" 2>&1 || {
            echo "FAILED: $folder/naive does not compile"; head -5 "$work/$name.log"; failures=$((failures + 1)); continue; }
    fi
    {
        echo "/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions"
        echo " * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered"
        echo " * names numbered again from 1. check.sh compares it with what the compiler writes now. */"
        echo ""
        awk -f scripts/cases/functions.awk "$folder/functions.txt" "$source_c"
    } > "$work/$name.generated.c" 2> "$work/$name.extract.log" || {
        echo "FAILED: $name: $(head -3 "$work/$name.extract.log")"; failures=$((failures + 1)); continue; }
    if $check; then
        if ! tr -d '\r' < "$folder/generated.c" 2> /dev/null | cmp -s "$work/$name.generated.c" -; then
            echo "FAILED: $folder/generated.c is not what the compiler writes now; run bash scripts/cases/extract.sh $name"
            diff "$folder/generated.c" "$work/$name.generated.c" | head -10
            failures=$((failures + 1))
        fi
    else
        cp "$work/$name.generated.c" "$folder/generated.c"
    fi
done
[ "$failures" == "0" ]
