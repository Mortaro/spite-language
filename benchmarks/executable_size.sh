#!/bin/bash
# Prints the size of each program's --optimized executable, built by one compiler, so two compilers can be compared
# (identical function folding, docs/optimizations.md). Not part of check.sh. Run from anywhere:
#   bash benchmarks/executable_size.sh [compiler] [program ...]
# The compiler defaults to .spite/spite_development.exe, the one check.sh last built; the programs default to the
# compiler itself and two benchmarks built on generic classes.
cd "$(dirname "$0")/.." || exit 1
compiler=${1:-.spite/spite_development.exe}
shift
work=.spite/sizes
mkdir -p "$work"
if [ -z "$CC" ]; then
    for candidate in clang cc gcc; do
        command -v "$candidate" >/dev/null 2>&1 && { CC="$candidate"; break; }
    done
    if [ -z "$CC" ]; then
        for found in "/c/Program Files/Microsoft Visual Studio/"*/*/VC/Tools/Llvm/x64/bin/clang.exe; do
            [ -x "$found" ] && { CC="$(cygpath -d "$found" 2>/dev/null || echo "$found")"; break; }
        done
    fi
fi
export CC
names=("$@")
[ ${#names[@]} -eq 0 ] && names=(bootstrap benchmarks/sparse_rows benchmarks/matched_rows)
printf "| %-24s | %12s | %10s |\n" "program" "bytes" "functions"
for name in "${names[@]}"; do
    base=$(basename "$name")
    "$compiler" "$name" --optimized --build --c-source --c-path="$work/$base.c" --executable-path="$work/$base.exe" > "$work/$base.log" 2>&1 || {
        echo "FAILED: $name does not build"; head -5 "$work/$base.log"; continue; }
    bytes=$(wc -c < "$work/$base.exe")
    functions=$(grep -cE '^[A-Za-z_][A-Za-z_0-9]*\*? [A-Za-z_][A-Za-z_0-9]*\(.*\) \{' "$work/$base.c")
    printf "| %-24s | %12s | %10s |\n" "$name" "$bytes" "$functions"
done
