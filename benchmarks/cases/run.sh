#!/bin/bash
# Times every case (benchmarks/cases/README.md) in its three forms: naive/ built --optimized, naive.c and expert.c
# built with clang -O2. Each program times its own work and prints "microseconds <n>" on its error output; the best
# of seven interleaved runs of each is kept, the three answers must match, and the timings table of the case's
# README.md and the summary of benchmarks/cases/README.md are written again. Not part of check.sh, which only builds
# and runs each case once. Run from anywhere:
#   bash benchmarks/cases/run.sh [--compiler=path] [case ...]
#   bash benchmarks/cases/run.sh --summary      only the summary, from the tables the cases already have
# The compiler defaults to .spite/spite_development.exe, the one check.sh last built. CASES_NOTE is added to the
# line that says where the numbers were measured (say, that another program was running on the machine).
cd "$(dirname "$0")/../.." || exit 1
compiler=.spite/spite_development.exe
names=()
summary_only=false
for argument in "$@"; do
    case "$argument" in
        --compiler=*) compiler=${argument#--compiler=} ;;
        --summary) summary_only=true ;;   # only write the summary again, from the tables the cases have
        *) names+=("$(basename "${argument%/}")") ;;
    esac
done
if [ ${#names[@]} -eq 0 ] && ! $summary_only; then
    for folder in benchmarks/cases/*/; do names+=("$(basename "$folder")"); done
fi
work=.spite/cases_timed
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
export CC   # the Spite compiler builds with it too, so all three forms use one C compiler
maths_library="-lm"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) maths_library="" ;; esac

processor=$( (wmic cpu get name 2>/dev/null | tr -d '\r' | sed -n 2p) || true)
[ -z "$processor" ] && processor=$(sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo 2>/dev/null | head -1)
processor=$(echo "$processor" | sed 's/[[:space:]]*$//')
system_name=$(uname -s); case "$system_name" in MINGW*|MSYS*|CYGWIN*) system_name="Windows" ;; esac
machine="$system_name, ${processor:+$processor, }$(nproc 2>/dev/null || echo "?") logical processors, $("$CC_BIN" --version | head -1 | sed 's/ (.*//')"
[ -n "$CASES_NOTE" ] && machine="$machine; $CASES_NOTE"
today=$(date +%Y-%m-%d)

# runs one executable: its answer (standard output) into $work/answer.txt, and echoes the microseconds it reports
microseconds_of() {
    "$1" > "$work/answer.txt" 2> "$work/time.txt"
    tr -d '\r' < "$work/time.txt" | sed -n 's/^microseconds //p'
}
# 1234567 as "1 234 567", the way the docs write numbers
spaced() { echo "$1" | sed -E ':again; s/^([0-9]+)([0-9]{3})/\1 \2/; t again'; }
ratio_of() { awk -v top="$1" -v bottom="$2" 'BEGIN { if (bottom == 0) bottom = 1; printf "%.2f", top / bottom }'; }
size_of() { wc -c < "$1" | tr -d ' '; }

# replaces what is between "<!-- $2 -->" and "<!-- /$2 -->" in the file $1 with the file $3
replace_between() {
    awk -v start="<!-- $2 -->" -v finish="<!-- /$2 -->" -v replacement="$3" '
        $0 == start { print; while ((getline line < replacement) > 0) print line; skipping = 1; next }
        $0 == finish { skipping = 0 }
        !skipping { print }' "$1" > "$1.new" && mv "$1.new" "$1"
}

for name in "${names[@]}"; do
    folder="benchmarks/cases/$name"
    spite="$work/${name}_spite.exe"; naive="$work/${name}_naive.exe"; expert="$work/${name}_expert.exe"
    "$compiler" "$folder/naive" --build --optimized --executable-path="$spite" > "$work/$name.log" 2>&1 || {
        echo "FAILED: $name/naive does not build"; head -5 "$work/$name.log"; continue; }
    "$CC_BIN" -O2 -w "$folder/naive.c" -o "$naive" $maths_library 2> "$work/$name.log" || {
        echo "FAILED: $name/naive.c does not build"; head -5 "$work/$name.log"; continue; }
    "$CC_BIN" -O2 -w "$folder/expert.c" -o "$expert" $maths_library 2> "$work/$name.log" || {
        echo "FAILED: $name/expert.c does not build"; head -5 "$work/$name.log"; continue; }
    spite_best=""; naive_best=""; expert_best=""; differs=""
    for attempt in 1 2 3 4 5 6 7; do
        took=$(microseconds_of "$spite"); spite_answer=$(tr -d '\r' < "$work/answer.txt")
        if [ -n "$took" ] && { [ -z "$spite_best" ] || [ "$took" -lt "$spite_best" ]; }; then spite_best=$took; fi
        took=$(microseconds_of "$naive"); [ "$(tr -d '\r' < "$work/answer.txt")" == "$spite_answer" ] || differs="naive.c"
        if [ -n "$took" ] && { [ -z "$naive_best" ] || [ "$took" -lt "$naive_best" ]; }; then naive_best=$took; fi
        took=$(microseconds_of "$expert"); [ "$(tr -d '\r' < "$work/answer.txt")" == "$spite_answer" ] || differs="expert.c"
        if [ -n "$took" ] && { [ -z "$expert_best" ] || [ "$took" -lt "$expert_best" ]; }; then expert_best=$took; fi
    done
    if [ -n "$differs" ]; then echo "FAILED: $name: $differs answers differently from naive/"; continue; fi
    spite_size=$(size_of "$spite"); naive_size=$(size_of "$naive"); expert_size=$(size_of "$expert")
    {
        echo "| form | best µs | executable bytes |"
        echo "|---|---|---|"
        if [ -n "$spite_best" ]; then
            echo "| Spite: \`naive/\`, \`--optimized\` | $(spaced "$spite_best") | $(spaced "$spite_size") |"
            echo "| naive C: \`naive.c\`, \`clang -O2\` | $(spaced "$naive_best") | $(spaced "$naive_size") |"
            echo "| expert C: \`expert.c\`, \`clang -O2\` | $(spaced "$expert_best") | $(spaced "$expert_size") |"
            echo ""
            echo "Spite takes $(ratio_of "$spite_best" "$naive_best") times naive C's time and $(ratio_of "$spite_best" "$expert_best") times expert C's (lower is faster)."
            echo "Best of seven interleaved runs, $today, $machine."
            echo "<!-- measured spite=$spite_best naive=$naive_best expert=$expert_best -->"
        else
            echo "| Spite: \`naive/\`, \`--optimized\` | not timed | $(spaced "$spite_size") |"
            echo "| naive C: \`naive.c\`, \`clang -O2\` | not timed | $(spaced "$naive_size") |"
            echo "| expert C: \`expert.c\`, \`clang -O2\` | not timed | $(spaced "$expert_size") |"
            echo ""
            echo "Measured $today, $machine."
            echo "<!-- measured untimed -->"
        fi
    } > "$work/$name.table"
    replace_between "$folder/README.md" timings "$work/$name.table"
    if [ -n "$spite_best" ]; then
        printf "%-60s Spite %9s µs  naive C %9s µs  expert C %9s µs  %s %s\n" "$name" "$spite_best" "$naive_best" "$expert_best" \
            "$(ratio_of "$spite_best" "$naive_best")" "$(ratio_of "$spite_best" "$expert_best")"
    else
        printf "%-60s not timed: %s, %s and %s bytes\n" "$name" "$spite_size" "$naive_size" "$expert_size"
    fi
done

# the summary: one row per case, from the line each README's table ends with
{
    echo "| case | Spite's time over naive C's | Spite's time over expert C's |"
    echo "|---|---|---|"
    for folder in benchmarks/cases/*/; do
        name=$(basename "$folder")
        [ -f "$folder/README.md" ] || continue
        measured=$(sed -n 's/^<!-- measured \(.*\) -->$/\1/p' "$folder/README.md" 2>/dev/null)
        spite_best=$(echo "$measured" | sed -n 's/.*spite=\([0-9]*\).*/\1/p')
        naive_best=$(echo "$measured" | sed -n 's/.*naive=\([0-9]*\).*/\1/p')
        expert_best=$(echo "$measured" | sed -n 's/.*expert=\([0-9]*\).*/\1/p')
        if [ -n "$spite_best" ]; then
            echo "| [$name]($name/) | $(ratio_of "$spite_best" "$naive_best") | $(ratio_of "$spite_best" "$expert_best") |"
        elif [ "$measured" == "untimed" ]; then
            echo "| [$name]($name/) | not timed | not timed |"
        else
            echo "| [$name]($name/) | not measured yet | not measured yet |"
        fi
    done
} > "$work/summary.table"
replace_between benchmarks/cases/README.md summary "$work/summary.table"
