#!/bin/bash
# Compile time at scale: how long building an executable takes, from one C file and from translation units
# compiled in parallel and cached (docs/compiler.md#translation-units). Not part of check.sh. Run from anywhere:
#   bash benchmarks/build_times.sh [compiler] [program folder ...]
# With no program named it times the compiler itself (bootstrap) and a generated program of about 200 000 lines
# (written to .spite/build_times/synthetic by the awk below); name another folder -- a copy of a SlopEngine
# example, say -- to time that too. Every build is timed four ways, with and without --optimized:
#   one file   --translation-units=1, the whole C in one file and one C compiler process, as before
#   cold       translation units, with the object cache emptied first
#   warm       the same build again: every unit's object is found in the cache, only the link runs
#   one edit   then one function body changed (its last `+ 1` made `+ 2`), and built again
# Times are wall-clock milliseconds of the whole `spite` command, the Spite compile to C included. The edit is made
# in place (a program's paths are in its C, so a copy elsewhere would be another program) and undone afterwards.
# Warning: "cold" empties .spite/objects, the cache every build from this repository shares.
cd "$(dirname "$0")/.." || exit 1
repository=$(pwd)
compiler=${1:-.spite/spite_development.exe}
shift
work=.spite/build_times
mkdir -p "$work"

# The synthetic program: 400 classes of 30 functions each, every function a few lines of arithmetic, a branch, a
# list and text, all reached from the entry so tree shaking keeps them.
synthetic="$work/synthetic"
if [ ! -f "$synthetic/synthetic.spite" ]; then
    mkdir -p "$synthetic"
    awk -v folder="$synthetic" 'BEGIN {
        parts = 400; steps = 30
        entry = folder "/synthetic.spite"
        print "var console = Console()\n\nfunc Synthetic() {\n    var total = 0" > entry
        for (part = 0; part < parts; part++) {
            file = folder "/part_" part ".spite"
            print "var total = 0\nvar label = \"part " part "\"\n\nfunc Part" part "() {\n    total = " part "\n}" > file
            for (step = 0; step < steps; step++) {
                print "\nfunc step_" step "(input: Integer): Integer {" > file
                print "    var value = input % 1000 + " step > file
                print "    if value % 3 == 0 {\n        value = value * 2\n    } else {\n        value = value + total\n    }" > file
                print "    value = value + 1" > file
                print "    var words = List<String>()\n    words.append(\"{label} step " step "\")" > file
                print "    words.append(\"{label} {value}\")\n    var joined = words.join(\",\")" > file
                print "    var joined_length = joined.length()\n    return value + joined_length\n}" > file
            }
            print "\nfunc run(): Integer {\n    var result = 0" > file
            for (step = 0; step < steps; step++) {
                print "    result = step_" step "(result)" > file
            }
            print "    return result % 100000\n}" > file
            close(file)
            print "    var part_" part " = Part" part "()\n    total = total + part_" part ".run()" > entry
        }
        print "    console.print(\"total\", total)\n}" > entry
        close(entry)
    }'
    echo "synthetic program: $(cat "$synthetic"/*.spite | wc -l) lines in $(ls "$synthetic" | wc -l) files"
fi

programs=("$@")
[ ${#programs[@]} -eq 0 ] && programs=(bootstrap "$synthetic")

milliseconds() {
    local start finish
    start=$(date +%s%N)
    "$@" > "$work/last.log" 2>&1 || { echo "FAILED: $*" >&2; head -5 "$work/last.log" >&2; }
    finish=$(date +%s%N)
    echo $(( (finish - start) / 1000000 ))
}

# One function body is changed by turning the last `+ 1` of a file into `+ 2`: nothing is added, so only the unit
# holding that function (and the link) should be compiled again. EDIT_FILE names the file to edit, for a program
# whose own folder has no such line (a SlopEngine example, whose engine is loaded from ../../slop).
edited=""
edit_one_function() {
    local wanted
    edited=${EDIT_FILE:-$(grep -rl --include='*.spite' ' + 1$' "$1" | tail -1)}
    cp "$edited" "$work/unedited.spite"
    wanted=$(grep -c ' + 1$' "$edited")
    awk -v wanted="$wanted" '{ if ($0 ~ / [+] 1$/ && ++seen == wanted) sub(/ [+] 1$/, " + 2"); print }' "$work/unedited.spite" > "$edited"
}
undo_edit() {
    [ -n "$edited" ] && cp "$work/unedited.spite" "$edited"
    edited=""
}
trap undo_edit EXIT

printf "| %-12s | %-11s | %9s | %9s | %9s | %9s |\n" "program" "build" "one file" "cold" "warm" "one edit"
printf "|%s|%s|%s|%s|%s|%s|\n" "--------------" "-------------" "-----------" "-----------" "-----------" "-----------"
for program in "${programs[@]}"; do
    name=$(basename "$program")
    for optimized in false true; do
        flags="--executable --run=false --optimized=$optimized --executable-path=$repository/$work/$name.exe"
        one_file=$(milliseconds "$compiler" "$program" $flags --translation-units=1)
        rm -rf .spite/objects
        cold=$(milliseconds "$compiler" "$program" $flags)
        warm=$(milliseconds "$compiler" "$program" $flags)
        edit_one_function "$program"
        edit=$(milliseconds "$compiler" "$program" $flags)
        undo_edit
        label="default"; [ "$optimized" == "true" ] && label="--optimized"
        printf "| %-12s | %-11s | %9s | %9s | %9s | %9s |\n" "$name" "$label" "$one_file" "$cold" "$warm" "$edit"
    done
done
