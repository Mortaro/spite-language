#!/bin/bash
# Times every benchmark: the compiler writes its C, clang -O2 builds it, and the best of seven runs is reported.
# Not part of check.sh, which only compiles them. Run from anywhere:
#   bash benchmarks/run.sh [compiler] [benchmark ...]
# The compiler defaults to .spite-cache/spite_development.exe, the one check.sh last built.
cd "$(dirname "$0")/.." || exit 1
compiler=${1:-.spite-cache/spite_development.exe}
shift
work=.spite-cache/benchmarks
mkdir -p "$work"
if [ -z "$CC" ]; then
    for candidate in clang cc gcc; do
        command -v "$candidate" >/dev/null 2>&1 && { CC="$candidate"; break; }
    done
    if [ -z "$CC" ]; then
        for found in "/c/Program Files/Microsoft Visual Studio/"*/*/VC/Tools/Llvm/x64/bin/clang.exe; do
            [ -x "$found" ] && { CC="$found"; break; }
        done
    fi
fi
names=("$@")
[ ${#names[@]} -eq 0 ] && names=(fused_chain dictionary_keys text_building reflection_walks function_values small_allocations parallel_calls stress console_lines vector_items vector_rows)
printf "| %-18s | %8s | %12s | %s\n" "benchmark" "best ms" "allocations" "output"
for name in "${names[@]}"; do
    "$compiler" "benchmarks/$name" --run=false --c-source --c-path="$work/$name.c" > "$work/$name.log" 2>&1 || {
        echo "FAILED: $name does not compile"; head -5 "$work/$name.log"; continue; }
    "$CC" -O2 -w "$work/$name.c" -o "$work/$name.exe" 2> "$work/$name.log" || {
        echo "FAILED: $name's C does not compile"; head -5 "$work/$name.log"; continue; }
    best=""
    for attempt in 1 2 3 4 5 6 7; do
        start=$(date +%s%N)
        output=$("$work/$name.exe" 2>&1 | tr -d '\r' | tail -n 2 | tr '\n' ' ')   # console_lines prints 200 000
        finish=$(date +%s%N)
        took=$(( (finish - start) / 1000000 ))
        if [ -z "$best" ] || [ "$took" -lt "$best" ]; then best=$took; fi
    done
    # the program built again with --debug-memory's table, which only that build's C carries, for the number of
    # allocations it makes
    "$compiler" "benchmarks/$name" --run=false --c-source --debug-memory --c-path="$work/${name}_counted.c" > "$work/$name.log" 2>&1 || continue
    "$CC" -O2 -w "$work/${name}_counted.c" -o "$work/${name}_counted.exe" 2> "$work/$name.log" || continue
    allocations=$("$work/${name}_counted.exe" 2>&1 | tr -d '\r' | grep '^allocations: ' | sed -E 's/allocations: ([0-9]+) frees: ([0-9]+)/\1/')
    printf "| %-18s | %8s | %12s | %s\n" "$name" "$best" "$allocations" "$output"
done
