#!/bin/bash
# "As fast as C" as a number: each program here has a C twin, twin.c, doing the same work the way a C programmer
# writes it. Both are built for release (the Spite program with --optimized, its twin with the same -O3), both
# must print the same answer, and each times its own work with the same clock and prints the microseconds on its
# error output, so starting a process (slow and noisy on Windows) is not counted. The best of seven interleaved runs
# of each is reported with the ratio Spite/C: 1.00 is as fast as C, 2.00 takes twice as long. Not part of check.sh,
# which only compiles them. Run from anywhere:
#   bash benchmarks/versus_c/run.sh [compiler] [program ...]
# The compiler defaults to .spite/spite_development.exe, the one check.sh last built. SPITE_FLAGS is added
# to the Spite build and C_FLAGS to the C build, so --tune-for-this-machine is compared with -march=native.
cd "$(dirname "$0")/../.." || exit 1
compiler=${1:-.spite/spite_development.exe}
shift
work=.spite/versus_c
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
CC_BIN="$CC"
case "$CC" in *\ *) command -v cygpath >/dev/null 2>&1 && CC=$(cygpath -d "$CC" 2>/dev/null || echo "$CC") ;; esac
export CC   # the Spite compiler builds with it too, so both sides use one C compiler
maths_library="-lm"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) maths_library="" ;; esac
names=("$@")
[ ${#names[@]} -eq 0 ] && names=(vector_maths particles number_dictionary text_building sorting)

# runs one executable: its answer (standard output) into $work/answer.txt, the microseconds it reports on its error output
microseconds_of() {
    "$1" > "$work/answer.txt" 2> "$work/time.txt"
    tr -d '\r' < "$work/time.txt" | sed -n 's/^microseconds //p'
}

printf "| %-18s | %10s | %10s | %7s |\n" "program" "Spite µs" "C µs" "Spite/C"
printf "|%s|%s|%s|%s|\n" "--------------------" "------------" "------------" "---------"
for name in "${names[@]}"; do
    folder="benchmarks/versus_c/$name"
    "$compiler" "$folder" --executable --run=false --optimized $SPITE_FLAGS --executable-path="$work/${name}_spite.exe" > "$work/$name.log" 2>&1 || {
        echo "FAILED: $name does not build"; head -5 "$work/$name.log"; continue; }
    "$CC_BIN" -O3 $C_FLAGS -w "$folder/twin.c" -o "$work/${name}_c.exe" $maths_library 2> "$work/$name.log" || {
        echo "FAILED: $name/twin.c does not build"; head -5 "$work/$name.log"; continue; }
    spite_best=""; c_best=""; spite_answer=""; c_answer=""
    for attempt in 1 2 3 4 5 6 7; do
        took=$(microseconds_of "$work/${name}_spite.exe")
        spite_answer=$(tr -d '\r' < "$work/answer.txt")
        if [ -z "$spite_best" ] || [ "$took" -lt "$spite_best" ]; then spite_best=$took; fi
        took=$(microseconds_of "$work/${name}_c.exe")
        c_answer=$(tr -d '\r' < "$work/answer.txt")
        if [ -z "$c_best" ] || [ "$took" -lt "$c_best" ]; then c_best=$took; fi
    done
    if [ "$spite_answer" != "$c_answer" ]; then
        echo "FAILED: $name and its C twin answer differently"; echo "  Spite: $spite_answer"; echo "  C:     $c_answer"; continue
    fi
    ratio=$(awk -v spite="$spite_best" -v c="$c_best" 'BEGIN { if (c == 0) c = 1; printf "%.2f", spite / c }')
    printf "| %-18s | %10s | %10s | %7s |\n" "$name" "$spite_best" "$c_best" "$ratio"
done
