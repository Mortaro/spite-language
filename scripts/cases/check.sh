#!/bin/bash
# Checks each case of benchmarks/ (benchmarks/README.md) the way check.sh does: naive/ compiles, the C it compiles to
# is still what its generated.c and highlights.c hold (scripts/cases/extract.sh), and naive/, naive.c and expert.c,
# each built with the C compiler at -O2, print the same answer. Nothing is timed. Run from anywhere:
#   bash scripts/cases/check.sh [--compiler=path] [case ...]
# With no case, every case. The compiler defaults to .spite/spite_development.exe; CC chooses the C compiler, and
# CASES_WORK the folder the builds go into (.spite/cases unless set).
cd "$(dirname "$0")/../.." || exit 1
compiler=.spite/spite_development.exe
names=()
for argument in "$@"; do
    case "$argument" in
        --compiler=*) compiler=${argument#--compiler=} ;;
        *) names+=("$(basename "${argument%/}")") ;;
    esac
done
if [ ${#names[@]} -eq 0 ]; then
    for folder in benchmarks/*/; do names+=("$(basename "$folder")"); done
fi
export CASES_WORK=${CASES_WORK:-.spite/cases}
work=$CASES_WORK
mkdir -p "$work"
C_COMPILER=${CC_BIN:-$CC}
if [ -z "$C_COMPILER" ]; then
    for candidate in clang cc gcc; do
        command -v "$candidate" >/dev/null 2>&1 && { C_COMPILER="$candidate"; break; }
    done
    if [ -z "$C_COMPILER" ]; then
        for found in "/c/Program Files/Microsoft Visual Studio/"*/*/VC/Tools/Llvm/x64/bin/clang.exe; do
            [ -x "$found" ] && { C_COMPILER="$found"; break; }
        done
    fi
fi
maths_library="-lm"
on_windows=false
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) maths_library=""; on_windows=true ;; esac
# a program that hangs fails by name; CHECK_TIMEOUT sets the limit in seconds, as in check.sh
limit=${CHECK_TIMEOUT:-300}
run_limited() {
    if timeout --version > /dev/null 2>&1; then timeout -k 10 "$limit" "$@"; else "$@"; fi
}
failures=0
for name in "${names[@]}"; do
    folder="benchmarks/$name"
    failed() { echo "FAILED: case $name: $1"; [ -n "$2" ] && echo "$2" | head -6; failures=$((failures + 1)); }
    # the files a case keeps are the C for Windows, so they are the same on every machine
    errors=$("$compiler" "$folder/naive" --check --c-source --optimized --target-operating-system=windows \
        --c-path="$work/$name.c" 2>&1) || {
        failed "naive/ does not compile" "$errors"; continue; }
    errors=$(bash scripts/cases/extract.sh --check --from-c="$work/$name.c" "$name" 2>&1) || {
        failed "generated.c or highlights.c" "$errors"; continue; }
    runnable="$work/$name.c"
    if ! $on_windows; then
        runnable="$work/${name}_here.c"
        errors=$("$compiler" "$folder/naive" --check --c-source --optimized --c-path="$runnable" 2>&1) || {
            failed "naive/ does not compile for this system" "$errors"; continue; }
    fi
    built=true
    for form in spite naive expert; do
        source="$folder/$form.c"
        [ "$form" == "spite" ] && source="$runnable"
        errors=$("$C_COMPILER" -O2 -w "$source" -o "$work/${name}_$form.exe" $maths_library 2>&1) || {
            failed "the $form C does not compile" "$errors"; built=false; break; }
    done
    $built || continue
    answer=$(run_limited "$work/${name}_spite.exe" 2> /dev/null | tr -d '\r')
    naive_answer=$(run_limited "$work/${name}_naive.exe" 2> /dev/null | tr -d '\r')
    expert_answer=$(run_limited "$work/${name}_expert.exe" 2> /dev/null | tr -d '\r')
    if [ -z "$answer" ] || [ "$answer" != "$naive_answer" ] || [ "$answer" != "$expert_answer" ]; then
        failed "naive/, naive.c and expert.c answer differently" \
            "$(printf 'naive/:   %s\nnaive.c:  %s\nexpert.c: %s' "$(echo "$answer" | head -2)" "$(echo "$naive_answer" | head -2)" "$(echo "$expert_answer" | head -2)")"
    fi
done
[ "$failures" == "0" ]
