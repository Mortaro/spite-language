#!/bin/bash
# Runs the high level report over every program of the conformance corpus and the
# examples, and writes the totals, every function the walk does not cover and why,
# and every difference, which must be none.
#
#   scripts/high_level_coverage.sh <compiler> [output folder]
#
# It is a check of the compiler, never an error in a user's program: a program's
# two answers differing is a bug in the compiler, and each comparison is deleted
# the day its decision has moved onto the high level. It fails on any difference.
#
# It compiles each program with --check, so nothing is built and no program runs.
# The programs are checked side by side, as many at once as there are processors
# (COVERAGE_JOBS sets how many). Each one holds the whole program's high level in
# memory (D569), so it is one compiler process per program.
cd "$(dirname "$0")/.." || exit 1
compiler=${1:-.spite/ir_compiler.exe}
out=${2:-.spite/high_level_coverage}
if [ ! -x "$compiler" ]; then
  echo "no compiler at '$compiler': pass the path to one built by this branch" >&2
  exit 1
fi
rm -rf "$out" || exit 1
mkdir -p "$out/reports" || exit 1
reports="$out/reports"
jobs=${COVERAGE_JOBS:-$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}

run_one() {
  program=$1
  name=$(echo "$program" | tr '/' '_')
  # A program that must not compile has no high level to compare: it never gets one.
  [ -f "$program/must_fail.txt" ] && return 0
  # The flags a program is checked with are its own, as check.sh reads them: a
  # program under --repl or with an environment is not the program without them.
  flags=""
  [ -f "$program/flags.txt" ] && flags=$(tr -d '\r\n' < "$program/flags.txt")
  "$compiler" "$program" --check --high-level-report="$reports/$name.txt" $flags \
    > "$reports/$name.log" 2>&1
  echo $? > "$reports/$name.status"
}

programs=$(ls -d conformance/*/*/ examples/*/ | tr -d '\r')
total=0
running=0
for program in $programs; do
  total=$((total + 1))
  run_one "$program" &
  running=$((running + 1))
  if [ "$running" -ge "$jobs" ]; then
    wait -n 2>/dev/null || wait
    running=$((running - 1))
  fi
done
wait

# The programs whose own report did not come back, each named, since a program the
# walk stops on is a compiler bug and never an error in the user's program.
stopped=0
for status in "$reports"/*.status; do
  [ -f "$status" ] || continue
  if [ "$(cat "$status")" != "0" ]; then
    name=$(basename "$status" .status)
    stopped=$((stopped + 1))
    echo "STOPPED: '$name'. A case the walk does not build is an operation of unknown effect or a crash naming its cause:"
    head -3 "$reports/$name.log"
  fi
  rm -f "$status"
done

awk -f scripts/high_level_summary.awk "$reports"/*.txt > "$out/summary.txt" 2>&1 || {
  echo "FAILED: could not summarise '$reports'"; exit 1; }
{ echo "programs $total"; echo "stopped $stopped"; cat "$out/summary.txt"; } > "$out/summary_all.txt" \
  && mv "$out/summary_all.txt" "$out/summary.txt"

echo "high level over $total programs, $stopped stopped: see $out/summary.txt"
head -8 "$out/summary.txt"

# Any difference is a bug in the compiler, never an error in a user's program, and it fails here.
differing=$(grep -c '^difference ' "$out/summary.txt")
if [ "$differing" != "0" ]; then
  echo "FAILED: the high level and the C reader differ on $differing facts, and each one is a bug in one of the two:"
  grep '^difference ' "$out/summary.txt" | head -20
  exit 1
fi
[ "$stopped" -eq 0 ] || exit 1