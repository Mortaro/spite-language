#!/bin/bash
# Builds sweep.c three ways and runs each (design/proposals/data_oriented_layout.md): scalar (the C compiler's
# vectorisers off), the vectoriser for the default target (SSE2, what benchmarks/run.sh builds), and the vectoriser
# for AVX2. Writes the tables to results_<build>.md beside this file. Not part of check.sh: it takes minutes.
#   bash design/proposals/data_oriented_layout/sweep.sh [sizes [rounds]]
# sizes: how many of the eight sizes, from 512 up (all unless given); rounds: how many times each number is measured,
# the least kept (three unless given)
cd "$(dirname "$0")" || exit 1
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
work=${SWEEP_WORK:-../../../.spite/sweep}
mkdir -p "$work"
build() {
    "$CC" -O2 -w -DBUILD_NAME="\"$1\"" "${@:2}" sweep.c -o "$work/sweep_$1.exe" || exit 1
}
build scalar -fno-vectorize -fno-slp-vectorize
build sse2
build avx2 -mavx2 -mfma
for name in scalar sse2 avx2; do
    "$work/sweep_$name.exe" "${1:-8}" "${2:-3}" > "results_$name.md" || exit 1
done
