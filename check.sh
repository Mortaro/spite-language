#!/bin/bash
# Proves the Spite compiler still compiles itself, still passes the conformance corpus, and still does
# what docs/ says it does.
# Needs only a C compiler. Set CC to choose one, otherwise the first available of cc, clang or gcc
# is used.  Run from anywhere:   bash check.sh
# Update the committed seed after an intended compiler change:   bash check.sh --update-seed
cd "$(dirname "$0")" || exit 1
work=.spite-cache/check_$$   # one folder per run: two sessions may run this at the same time
trap 'rm -rf "$work"' EXIT
mkdir -p "$work"
if [ -z "$CC" ]; then
    for candidate in cc clang gcc; do
        command -v "$candidate" >/dev/null 2>&1 && { CC="$candidate"; break; }
    done
    # Windows usually has one inside Visual Studio rather than on PATH.
    if [ -z "$CC" ]; then
        for found in "/c/Program Files/Microsoft Visual Studio/"*/*/VC/Tools/Llvm/x64/bin/clang.exe \
                     "/c/Program Files (x86)/Microsoft Visual Studio/"*/*/VC/Tools/Llvm/x64/bin/clang.exe; do
            [ -x "$found" ] && { CC="$found"; break; }
        done
    fi
    [ -z "$CC" ] && { echo "no C compiler found: set CC, or install cc, clang or gcc" >&2; exit 1; }
fi
CC_BIN="$CC"
# The compiler passes CC to a shell unquoted, so a path with spaces has to be shortened.
case "$CC" in *\ *) command -v cygpath >/dev/null 2>&1 && CC=$(cygpath -d "$CC" 2>/dev/null || echo "$CC") ;; esac
CC="$CC -Wno-deprecated-declarations"   # CC may carry arguments; MSVC headers warn on fopen
export CC   # the Spite compiler reads it to compile the C it emits
# The compiler folds maths on constants with the C library's own functions, which Linux and macOS keep in libm.
maths_library="-lm"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) maths_library="" ;; esac
compile_c() { "$CC_BIN" -O1 -Wno-parentheses-equality -Wno-deprecated-declarations "$1" -o "$2" $maths_library 2> "$work/c_errors.txt" || { head -20 "$work/c_errors.txt"; exit 1; }; }

# Each generation writes its C to the default place beside the program, bootstrap/bootstrap.c, and it is moved into
# the work folder at once: every Build field is a constant in the built compiler, so a --c-path naming this run's
# folder would be written into the C and no two generations (or seeds) would ever be equal.
compile_compiler() {
  "$1" bootstrap --run=false --c-source || return 1
  cp bootstrap/bootstrap.c "$2" || return 1   # a copy: Windows may still hold the file, refusing a rename
  for attempt in 1 2 3 4 5 6 7 8 9 10; do   # and a virus scanner may hold it a moment longer, refusing the delete
    rm -f bootstrap/bootstrap.c 2>/dev/null && return 0
    sleep 1
  done
  rm -f bootstrap/bootstrap.c
}

echo "1/4 building the seed compiler"
compile_c bootstrap/seed/spite_compiler.c "$work/seed.exe"

echo "2/4 the seed compiles the compiler sources (generation 2)"
compile_compiler "$work/seed.exe" "$work/generation_two.c" || { echo "FAILED: the seed could not compile the compiler sources"; exit 1; }
compile_c "$work/generation_two.c" "$work/generation_two.exe"

echo "3/4 generation 2 compiles the compiler sources again (generation 3): must be byte identical"
compile_compiler "$work/generation_two.exe" "$work/generation_three.c" || exit 1
if ! cmp -s "$work/generation_two.c" "$work/generation_three.c"; then
  # A change to how the compiler compiles ITS OWN source needs one more generation to settle.
  echo "    generation 2 and 3 differ: trying one more generation"
  compile_c "$work/generation_three.c" "$work/generation_three.exe"
  compile_compiler "$work/generation_three.exe" "$work/generation_four.c" || exit 1
  if ! cmp -s "$work/generation_three.c" "$work/generation_four.c"; then echo "FAILED: no fixpoint, generation 3 and 4 still emit different C"; exit 1; fi
  cp "$work/generation_three.c" "$work/generation_two.c"; cp "$work/generation_three.exe" "$work/generation_two.exe"
fi

cp "$work/generation_two.exe" .spite-cache/spite_development.exe   # the freshly built compiler, handy for trying things by hand
echo "4/4 conformance corpus and examples with generation 2"
# A program that calls a foreign library brings the library's C as fixture.c; it is built next to it here, as
# fixture.dll on every platform, so the program can name one real file (D71) -- dlopen does not mind the name.
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) position_independent="" ;; *) position_independent="-fPIC" ;; esac
for fixture in conformance/*/*/fixture.c; do
  [ -f "$fixture" ] || continue
  "$CC_BIN" -shared $position_independent -w "$fixture" -o "$(dirname "$fixture")/fixture.dll" > "$work/c_errors.txt" 2>&1 || { echo "FAILED: could not build $fixture"; head -5 "$work/c_errors.txt"; exit 1; }
done
passed=0; failed=0
for folder in conformance/*/*/ examples/*/; do   # the examples are held to the same standard as the corpus
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")   # compiler flags such as --environment=server
  input=/dev/null; [ -f "$folder/input.txt" ] && input="$folder/input.txt"   # what the program reads from the console
  # the executable goes into the work folder rather than beside the program, so the repository stays clean
  actual=$("$work/generation_two.exe" "$folder" --debug-memory --executable-path="$work/$name.exe" $flags < "$input" 2>&1 | tr -d '\r')
  # some toolchains intermittently fail to open their own cache files on Windows; that is not a
  if echo "$actual" | grep -q "failed to check cache"; then
    actual=$("$work/generation_two.exe" "$folder" --debug-memory --executable-path="$work/$name.exe" $flags < "$input" 2>&1 | tr -d '\r')
  fi
  expected=$(tr -d '\r' < "$folder/expected_output.txt")
  body=$(echo "$actual" | grep -v '^allocations: ')
  balance=$(echo "$actual" | grep '^allocations: ' | sed -E 's/allocations: ([0-9]+) frees: ([0-9]+)/\1 \2/')
  allocations=${balance% *}; frees=${balance#* }
  if [ -f "$folder/crashes.txt" ]; then
    # a program that is meant to crash: its whole output (stdout and the crash line) must match, and there is no
    # balance line because a crash halts before the program would have released anything
    if [ "$actual" == "$expected" ]; then passed=$((passed+1)); else failed=$((failed+1)); echo "FAILED: $name"; echo "$actual" | head -8; fi
    continue
  fi
  # allocations.txt pins how many allocations a program makes, so an optimisation that removes them stays removed
  if [ -f "$folder/allocations.txt" ] && [ "$allocations" != "$(tr -d '\r\n' < "$folder/allocations.txt")" ]; then
    failed=$((failed+1)); echo "FAILED: $name allocated $allocations times, allocations.txt says $(tr -d '\r\n' < "$folder/allocations.txt")"; continue
  fi
  if [ "$body" == "$expected" ] && [ -n "$balance" ] && [ "$allocations" == "$frees" ]; then passed=$((passed+1)); else failed=$((failed+1)); echo "FAILED: $name"; echo "$actual" | head -8; fi
done
echo "conformance and examples: $passed passed, $failed failed"
[ "$failed" == "0" ] || exit 1

# A program runs in the folder spite was run from, not the language's: the compiler finds library/ and launcher/
# from its own executable, so a relative path the program opens is the caller's. With no --executable-path its
# executable is built beside the program (D129), so nothing is written into the caller's folder either.
repository=$(pwd)
mkdir -p "$work/elsewhere"
echo "hello" > "$work/elsewhere/greeting.txt"
beside="$repository/conformance/stage6/working_directory/working_directory"
elsewhere=$(cd "$work/elsewhere" && "$repository/$work/generation_two.exe" "$repository/conformance/stage6/working_directory" 2>&1 | tr -d '\r')
if [ "$elsewhere" != "read hello from the folder spite was run in" ] || [ "$(ls -A "$work/elsewhere")" != "greeting.txt" ]; then
  echo "FAILED: a program run from another folder does not open its relative paths there"; echo "$elsewhere" | head -5; exit 1
fi
[ -f "$beside.crashes" ] || { echo "FAILED: the executable was not built beside the program"; exit 1; }
rm -f "$beside.crashes" "$beside.exe"
[ -f "$beside" ] && rm -f "$beside"
echo "working directory: a program opens relative paths in the folder spite was run from, and is built beside itself"

# Every program above is built with --debug-memory, whose allocations go through a locked table. The thread pool is
# also run the way a user runs it -- `spite <program>`, compile and run, no flags -- with runners started from a
# stage, a singleton reached from the pool, a ThreadLocal and a Lock.
stages=conformance/stage6/parallel_stages
plain=$("$work/generation_two.exe" "$stages" --executable-path="$work/parallel_stages_plain.exe" < /dev/null 2>&1 | tr -d '\r')
if [ "$plain" != "$(tr -d '\r' < "$stages/expected_output.txt")" ]; then
  echo "FAILED: run mode without --debug-memory: parallel_stages"; echo "$plain" | head -5; exit 1
fi
echo "run mode: the thread pool runs without --debug-memory"
# A class or namespace object is made once, on whichever thread asks first: threaded_class_objects has eight pool
# threads ask for the same class objects at once, and a race shows up as a leak or a double free, so the build the
# corpus made is run a few more times.
threaded="$work/threaded_class_objects.exe"
for attempt in 1 2 3 4 5; do
  raced=$("$threaded" < /dev/null 2>&1 | tr -d '\r')
  if [ "$(echo "$raced" | grep -v '^allocations: ')" != "$(tr -d '\r' < conformance/stage6/threaded_class_objects/expected_output.txt)" ] \
     || ! echo "$raced" | grep -qE '^allocations: ([0-9]+) frees: \1$'; then
    echo "FAILED: threaded_class_objects, run $attempt"; echo "$raced" | head -5; exit 1
  fi
done
echo "threads: eight threads asking for the same class objects make each once, five runs balanced"

# What a production build leaves out is only visible in its C (docs/optimizations.md): hello carries no struct,
# allocate or singleton slot of a library class it never makes, and each singleton singleton_forms reaches from a
# Parallel takes its cheapest safe form -- atomics for a counter, nothing for one that never changes or that no
# Parallel reaches -- so no lock at all.
"$work/generation_two.exe" examples/hello --run=false --c-source --c-path="$work/hello_shaken.c" > /dev/null 2>&1 || {
  echo "FAILED: examples/hello does not write its C"; exit 1; }
if grep -qE "struct (Watcher|Socket|Process|HotReload|ThreadPool|Scheduler) \{|(Watcher|Socket|ThreadPool|Scheduler)___allocate|spite_singleton_(ThreadPool|Scheduler)_cache" "$work/hello_shaken.c"; then
  echo "FAILED: examples/hello's C still carries library classes it never uses"; exit 1
fi
# --debug-memory's table and the allocation counter are only in builds that read them (D177): hello allocates with
# the C library's malloc, realloc and free and nothing beside them.
if grep -qE "AllocationTable|spite_debug_|spite_live_allocation|SPITE_DEBUG_MEMORY" "$work/hello_shaken.c"; then
  echo "FAILED: examples/hello's C carries --debug-memory's table or an allocation counter"; exit 1
fi
# The maths functions are the C library's, and <math.h> is included only when one survives tree shaking (D177).
if grep -qE "#include <math.h>|Spite(Float|Double|Integer)_(square_root|sine|absolute|pi)" "$work/hello_shaken.c"; then
  echo "FAILED: examples/hello's C includes math.h or a maths function it never calls"; exit 1
fi
"$work/generation_two.exe" conformance/stage6/singleton_forms --run=false --c-source --c-path="$work/singleton_forms.c" > /dev/null 2>&1 || {
  echo "FAILED: singleton_forms does not write its C"; exit 1; }
if ! grep -q "^#define HitCounter___atomic 1$" "$work/singleton_forms.c" || grep -q "SpiteGuard [A-Za-z_]*___guard" "$work/singleton_forms.c"; then
  echo "FAILED: singleton_forms should make HitCounter atomic and lock no singleton"; exit 1
fi
locks="$work/singleton_lock_calls.c"
"$work/generation_two.exe" conformance/stage6/singleton_lock_calls --run=false --c-source --c-path="$locks" > /dev/null 2>&1 || {
  echo "FAILED: singleton_lock_calls does not write its C"; exit 1; }
if ! grep -q "_Alignas(64) int64_t owner" "$locks" || ! grep -q "^Registry_count_one___unguarded(self);" "$locks" \
   || ! grep -q "^#define Registry___outside_enter() spite_guard_enter(&Registry___guard)" "$locks" || grep -q "Rules___guard" "$locks" \
   || [ "$(grep -cE "Registry___outside_(read_)?enter\(\);" "$locks")" -lt 2 ]; then
  echo "FAILED: singleton_lock_calls should pad its lock, call itself unlocked, lock a write and a read from outside and not lock Rules"; exit 1
fi
# A locked singleton's function that touches none of its changing state takes no lock, so pool work calling it
# never waits on a function of the same singleton that polls that work.
stateless="$work/singleton_stateless_calls.c"
"$work/generation_two.exe" conformance/stage6/singleton_stateless_calls --run=false --c-source --c-path="$stateless" > /dev/null 2>&1 || {
  echo "FAILED: singleton_stateless_calls does not write its C"; exit 1; }
if ! grep -q "^int32_t Workshop_build___unguarded(Workshop\* self) {" "$stateless" || grep -q "Workshop_make_piece___unguarded" "$stateless"; then
  echo "FAILED: singleton_stateless_calls should lock Workshop.build and not Workshop.make_piece"; exit 1
fi
# D229: a counted loop calling one locked singleton takes its lock once around the loop and calls the unlocked body;
# a loop polling another singleton for what a Parallel posts keeps a lock per call, or it would never see the post.
coarse="$work/coarse_locks.c"
"$work/generation_two.exe" conformance/stage6/coarse_locks --run=false --c-source --c-path="$coarse" > /dev/null 2>&1 || {
  echo "FAILED: coarse_locks does not write its C"; exit 1; }
if ! grep -q "^#define spite_coarse_0_enter() spite_guard_enter(&Tally___guard)$" "$coarse" \
   || ! grep -q "^Tally_add___unguarded(self->tally_, 1);$" "$coarse" || grep -q "Mailbox_take___unguarded(self->mailbox_" "$coarse" \
   || grep -q "Mailbox_put___unguarded(self->mailbox_" "$coarse"; then
  echo "FAILED: coarse_locks should lock Tally once around count_up's loop and Mailbox on every call"; exit 1
fi
# D230: a locked singleton's function that only reads takes the readers' side of its lock, a count on the reading
# thread's own cache line, and a function that writes waits for the readers to leave.
reads="$work/singleton_reads.c"
"$work/generation_two.exe" conformance/stage6/singleton_reads --run=false --c-source --c-path="$reads" > /dev/null 2>&1 || {
  echo "FAILED: singleton_reads does not write its C"; exit 1; }
if ! grep -q "^int64_t\* spite_reading = spite_read_enter(&Column__Transform___guard, Column__Transform___readers);$" "$reads" \
   || ! grep -q "^spite_guard_enter_writing(&Column__Transform___guard, Column__Transform___readers);$" "$reads"; then
  echo "FAILED: singleton_reads should read Column<Transform>.at on the readers' side and write insert on the writer's"; exit 1
fi
# D231: while no task is in flight a locked function runs unlocked, and a task it starts takes the lock it skipped.
unshared="$work/unshared_locks.c"
"$work/generation_two.exe" conformance/stage6/unshared_locks --run=false --c-source --c-path="$unshared" > /dev/null 2>&1 || {
  echo "FAILED: unshared_locks does not write its C"; exit 1; }
if ! grep -A1 "^Parallel__Integer\* Ledger_start_posting(Ledger\* self, int32_t count_) {$" "$unshared" \
     | grep -q "^if (spite_skipped_depth < 16 && __atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0) {$" \
   || ! grep -q "spite_enter_skipped(spite_skipped\[spite_index\])" "$unshared"; then
  echo "FAILED: unshared_locks should skip Ledger's lock while no task runs and take it when one starts"; exit 1
fi
echo "production C: hello carries no unused class, table or counter, singleton_forms takes no lock, singleton_lock_calls locks only Registry, a stateless function of a locked singleton takes no lock, a counted loop of calls locks once, a reading function takes the readers' side, no task in flight skips the lock"
# D201: a whole-number division checks its divisor for zero, except where a proof already shows it is not zero:
# division_by_zero's 'whole / pieces' follows 'assert pieces != 0', so its C carries no check for it.
"$work/generation_two.exe" conformance/stage6/division_by_zero --run=false --c-source --c-path="$work/division.c" > /dev/null 2>&1 || {
  echo "FAILED: division_by_zero does not write its C"; exit 1; }
if grep -q "whole / pieces" "$work/division.c" || ! grep -q "total / parts" "$work/division.c"; then
  echo "FAILED: division_by_zero should check 'total / parts' and not the proven 'whole / pieces'"; exit 1
fi
echo "division: a proven divisor carries no zero check"
# A loop over a list of plain values that cannot change its size reads the count once and its items without a range
# check (docs/optimizations.md): counted_loops' scale_in_place is a plain C loop the C compiler can vectorise, and
# scale_into checks the list it writes once, before the loop; add_from, whose counter starts at a parameter, is not.
"$work/generation_two.exe" conformance/stage6/counted_loops --run=false --c-source --c-path="$work/counted.c" > /dev/null 2>&1 || {
  echo "FAILED: counted_loops does not write its C"; exit 1; }
if ! grep -qE "^spite_temp_[0-9]+\[index_\] = \(spite_temp_[0-9]+\[index_\] \* 2\.0\);$" "$work/counted.c" \
   || ! grep -qE "^if \(spite_temp_[0-9]+ <= spite_temp_[0-9]+\) \{$" "$work/counted.c" \
   || [ "$(grep -c "^while (((index_ < List_Integer_count(values_)))) {$" "$work/counted.c")" != "2" ]; then
  echo "FAILED: counted_loops should read its plain lists without range checks, except in add_from and double_up"; exit 1
fi
echo "counted loops: a plain list's loop reads the count once and its items unchecked"
# D211: live reload watches every folder a program loads, and a loop's check point is one load of a flag.
"$work/generation_two.exe" conformance/stage6/load_on_build --run=false --hot-reload --c-source --c-path="$work/watched.c" --flavor=salty > /dev/null 2>&1 || {
  echo "FAILED: load_on_build does not write its C with --hot-reload"; exit 1; }
"$work/generation_two.exe" conformance/stage3/lists --run=false --repl-port=4000 --c-source --c-path="$work/checked_loops.c" > /dev/null 2>&1 || {
  echo "FAILED: lists does not write its C with --repl-port"; exit 1; }
if ! grep -qF 'load_on_build/kitchen\nconformance/stage6/load_on_build/salty\nconformance/stage6/load_on_build/kitchen/garnish/pepper' "$work/watched.c" \
   || ! grep -q "if (__atomic_load_n(&spite_check_point_wanted, __ATOMIC_RELAXED) != 0)" "$work/checked_loops.c"; then
  echo "FAILED: a --hot-reload build should watch every loaded folder and check its loops with one load of a flag"; exit 1
fi
echo "live reload: every loaded folder is watched, and a loop's check point is one load of a flag"
# Maths on constants is worked out while compiling (docs/optimizations.md): every folded_ value in maths_folding is
# a literal in its C, and the program itself holds each one to the bits the C library computes at run time.
"$work/generation_two.exe" conformance/stage6/maths_folding --run=false --c-source --c-path="$work/folding.c" > /dev/null 2>&1 || {
  echo "FAILED: maths_folding does not write its C"; exit 1; }
if grep -E "(float|double) folded_[a-z_]*_ = " "$work/folding.c" | grep -v "_wide_ = " | grep -qE "Spite(Float|Double)_[a-z_0-9]+\(" \
   || ! grep -qE "float folded_sine_ = \(0x1\.[0-9a-f]+p-2f\);" "$work/folding.c"; then
  echo "FAILED: maths_folding's constant maths should be literals in its C"; exit 1
fi
echo "maths: a maths function of constants is a literal in the C"

# The benchmarks (benchmarks/README.md) are timed by hand with benchmarks/run.sh; here they only have to compile.
benchmarked=0
for folder in benchmarks/*/; do
  name=$(basename "$folder")
  "$work/generation_two.exe" "$folder" --run=false --c-source --c-path="$work/benchmark_$name.c" > "$work/c_errors.txt" 2>&1 || {
    echo "FAILED: benchmark $name does not compile"; head -5 "$work/c_errors.txt"; exit 1; }
  "$CC_BIN" -fsyntax-only -w "$work/benchmark_$name.c" 2> "$work/c_errors.txt" || {
    echo "FAILED: the C of benchmark $name does not compile"; head -5 "$work/c_errors.txt"; exit 1; }
  benchmarked=$((benchmarked+1))
done
echo "benchmarks: $benchmarked compile"

# The tests: a package that crashes (D46). No framework: a test is a function, and `crash` is the assertion.
test_output=$("$work/generation_two.exe" tests --debug-memory --executable-path="$work/tests.exe" < /dev/null 2>&1 | tr -d '\r')
if echo "$test_output" | grep -q "failed to check cache"; then
  test_output=$("$work/generation_two.exe" tests --debug-memory --executable-path="$work/tests.exe" < /dev/null 2>&1 | tr -d '\r')
fi
test_balance=$(echo "$test_output" | grep '^allocations: ' | sed -E 's/allocations: ([0-9]+) frees: ([0-9]+)/\1 \2/')
if [ "$(echo "$test_output" | grep -vc '^allocations: ')" != "0" ] || [ -z "$test_balance" ] || [ "${test_balance% *}" != "${test_balance#* }" ]; then
  echo "FAILED: tests"; echo "$test_output" | head -8; exit 1
fi
echo "tests: passed"

# The compiler is held to the corpus standard too: compiling itself, it frees everything it takes. The table that
# counts is only in a --debug-memory build's C, so the compiler writes itself once more with it.
"$work/generation_two.exe" bootstrap --run=false --c-source --debug-memory --c-path="$work/generation_two_debug.c" || { echo "FAILED: the compiler does not write itself with --debug-memory"; exit 1; }
"$CC_BIN" -O1 -w "$work/generation_two_debug.c" -o "$work/generation_two_debug.exe" $maths_library 2> "$work/c_errors.txt" || { head -20 "$work/c_errors.txt"; exit 1; }
self_leaks=$("$work/generation_two_debug.exe" bootstrap --run=false 2>&1 > /dev/null | head -5)   # no output: compile only
if [ -n "$self_leaks" ]; then echo "FAILED: the compiler leaks while compiling itself"; echo "$self_leaks"; exit 1; fi
echo "compiler memory: compiling itself frees everything it takes"

# Programs that must NOT compile: the errors are the language's main channel to whoever (or whatever) writes the code.
# Every compile formats the program first (D190), and some of these are unformatted on purpose, so each is compiled
# from a copy in the work folder, from where its paths read as they do here.
mkdir -p "$work/unformatted" && cp -r diagnostics "$work/unformatted/"
wrong=0; checked=0
for folder in diagnostics/*/; do
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")
  actual=$(cd "$work/unformatted" && "$repository/$work/generation_two.exe" "$folder" --run=false $flags 2>&1 >/dev/null | tr -d '\r')
  expected=$(tr -d '\r' < "$folder/expected_errors.txt")
  checked=$((checked+1))
  if [ "$actual" != "$expected" ]; then wrong=$((wrong+1)); echo "FAILED diagnostics: $name"; echo "$actual" | head -8; fi
done
echo "diagnostics: $checked checked, $wrong wrong"
[ "$wrong" == "0" ] || exit 1
# Every compile formats first (D190) and the formatter deletes an empty line inside a function (D196), so compiling
# diagnostics/blank_line above fixed its copy: the copy changed, and is now in the one style.
if cmp -s "$work/unformatted/diagnostics/blank_line/blank_line.spite" diagnostics/blank_line/blank_line.spite \
   || [ -n "$("$work/generation_two.exe" format --check "$work/unformatted/diagnostics/blank_line" 2>&1)" ]; then
  echo "FAILED: compiling diagnostics/blank_line should have deleted the empty line inside its function"; exit 1
fi
echo "format: compiling deletes an empty line inside a function"

# Every program written in docs/ and README.md is a program: scripts/docs_corpus (itself Spite) writes each titled
# code block out, and each one has to compile, run, print its ```output block and free everything it took.
# A block marked `error` must fail to compile with its ```diagnostic text somewhere in the message.
rm -rf .spite-cache/docs   # so a program deleted from docs/ stops being checked
"$work/generation_two.exe" scripts/docs_corpus --executable-path="$work/docs_corpus.exe" > /dev/null || {
  echo "FAILED: could not extract the documentation's programs"; exit 1; }
documented=0; undocumented=0
for folder in .spite-cache/docs/*/; do
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")
  if [ -f "$folder/must_fail.txt" ]; then
    actual=$("$work/generation_two.exe" "$folder" --run=false $flags 2>&1 >/dev/null | tr -d '\r')
    expected=$(tr -d '\r' < "$folder/expected_diagnostic.txt")
    if [ -n "$actual" ] && [ "${actual#*$expected}" != "$actual" ]; then documented=$((documented+1))
    else undocumented=$((undocumented+1)); echo "FAILED docs: $name wanted an error saying '$expected'"; echo "$actual" | head -4; fi
    continue
  fi
  actual=$("$work/generation_two.exe" "$folder" --debug-memory --executable-path="$work/docs_$name.exe" $flags < /dev/null 2>&1 | tr -d '\r')
  expected=$(tr -d '\r' < "$folder/expected_output.txt")
  body=$(echo "$actual" | grep -v '^allocations: ')
  balance=$(echo "$actual" | grep '^allocations: ' | sed -E 's/allocations: ([0-9]+) frees: ([0-9]+)/\1 \2/')
  if [ "$body" == "$expected" ] && [ -n "$balance" ] && [ "${balance% *}" == "${balance#* }" ]; then documented=$((documented+1))
  else undocumented=$((undocumented+1)); echo "FAILED docs: $name"; echo "$actual" | head -6; fi
done
echo "documentation: $documented passed, $undocumented failed"
[ "$undocumented" == "0" ] || exit 1

# A ```wire block in docs/ is a remote REPL session: its program runs with --repl-port, every
# `$ spite connect <port> --command="..."` line is sent with the compiler's own client, and each answer must be
# the JSON line written under it. The port comes from this run's process id, so two runs do not share one.
sessions=0
for wire in .spite-cache/docs/*/wire.txt; do
  [ -f "$wire" ] || continue
  folder=$(dirname "$wire"); name=$(basename "$folder")
  port=$((20000 + $$ % 20000))
  "$work/generation_two.exe" "$folder" --executable --run=false --repl-port=$port --executable-path="$work/wire_$name.exe" > "$work/c_errors.txt" 2>&1 || {
    echo "FAILED wire: $name does not build with --repl-port"; head -5 "$work/c_errors.txt"; exit 1; }
  "$work/wire_$name.exe" > "$work/wire_$name.txt" 2>&1 < /dev/null &
  served=$!
  listening=false
  for attempt in $(seq 1 100); do   # the program listens before its constructor runs; wait up to 20 seconds
    timeout 10 "$work/generation_two.exe" connect $port --command=help > /dev/null 2>&1 && { listening=true; break; }
    kill -0 $served 2>/dev/null || break
    sleep 0.2
  done
  if ! $listening; then kill $served 2>/dev/null; echo "FAILED wire: $name never listened on $port"; cat "$work/wire_$name.txt" | head -5; exit 1; fi
  expected=$(tr -d '\r' < "$wire" | grep -v '^#' | grep -v '^$')
  actual=$(tr -d '\r' < "$wire" | grep '^\$ spite connect ' | while IFS= read -r line; do
    command=$(echo "$line" | sed -E 's/^\$ spite connect [0-9]+ --command="(.*)"$/\1/')
    echo "$line"
    timeout 10 "$work/generation_two.exe" connect $port --command="$command" 2>&1 | tr -d '\r'
  done)
  for attempt in $(seq 1 50); do kill -0 $served 2>/dev/null || break; sleep 0.2; done
  if kill -0 $served 2>/dev/null; then kill $served; echo "FAILED wire: $name kept running after exit"; exit 1; fi
  wait $served; exit_code=$?
  if [ "$actual" != "$expected" ] || [ "$exit_code" != "0" ]; then
    echo "FAILED wire: $name (exit code $exit_code)"; diff <(echo "$expected") <(echo "$actual") | head -10; exit 1
  fi
  if [ "$(tr -d '\r' < "$work/wire_$name.txt")" != "$(tr -d '\r' < "$folder/expected_output.txt")" ]; then
    echo "FAILED wire: $name printed something else while it was served"; head -5 "$work/wire_$name.txt"; exit 1
  fi
  sessions=$((sessions+1))
done
[ "$sessions" -gt 0 ] || { echo "FAILED wire: docs/ has no remote REPL session to replay"; exit 1; }
echo "remote REPL: $sessions documented sessions answered exactly"

# Live reload (docs/repl.md): docs/'s hot_counter program runs with --hot-reload and --repl-port from a copy in the
# work folder, which this step edits. An edit followed by `reload` must run the new code with the state set before
# it, and an edit nobody reports must be picked up by the file watcher, rebuilding only its class. Every wait has a
# timeout, and the program is killed if it outlives the session.
hot_folder="$work/hot_reload/hot_counter"
[ -d .spite-cache/docs/hot_counter ] || { echo "FAILED live reload: docs/repl.md has no hot_counter program"; exit 1; }
mkdir -p "$work/hot_reload"; cp -r .spite-cache/docs/hot_counter "$hot_folder"
port=$((20000 + $$ % 20000))
"$work/generation_two.exe" "$hot_folder" --executable --run=false --hot-reload --repl-port=$port --executable-path="$work/hot_reload/hot_counter.exe" > "$work/c_errors.txt" 2>&1 || {
  echo "FAILED live reload: hot_counter does not build with --hot-reload"; head -5 "$work/c_errors.txt"; exit 1; }
"$work/hot_reload/hot_counter.exe" > "$work/hot_reload/output.txt" 2>&1 < /dev/null &
served=$!
hot_fail() { kill $served 2>/dev/null; echo "FAILED live reload: $1"; head -5 "$work/hot_reload/output.txt"; exit 1; }
ask() { timeout 60 "$work/generation_two.exe" connect $port --command="$1" 2>&1 | tr -d '\r'; }
expect() { local answer; answer=$(ask "$1"); [ "$answer" == "$2" ] || hot_fail "'$1' answered $answer, not $2"; }
listening=false
for attempt in $(seq 1 100); do
  timeout 10 "$work/generation_two.exe" connect $port --command=help > /dev/null 2>&1 && { listening=true; break; }
  kill -0 $served 2>/dev/null || break
  sleep 0.2
done
$listening || hot_fail "the program never listened on $port"
expect 'program.visits = 42' '{"ok":true,"value":"42","type":"Integer"}'
sed -i 's/hello, visit {visits}/welcome back, visit {visits}/' "$hot_folder/hot_counter.spite"
reloaded=$(ask reload)   # the watcher may swap the code in first, and then there is nothing left for reload to do
case "$reloaded" in
  '{"ok":true,"value":"rebuilt HotCounter","type":""}'|'{"ok":true,"value":"nothing changed since the code the program runs","type":""}') ;;
  *) hot_fail "reload answered $reloaded" ;;
esac
expect 'last_reload' '{"ok":true,"value":"rebuilt HotCounter","type":""}'
expect 'greeting()' '{"ok":true,"value":"welcome back, visit 42","type":"String"}'
sed -i 's/{name} roars/{name} roars louder/' "$hot_folder/monster.spite"
watched=""
for attempt in $(seq 1 150); do   # the watcher settles, then the program rebuilds at its next wait: up to 30 seconds
  watched=$(ask last_reload)
  [ "$watched" == '{"ok":true,"value":"rebuilt Monster","type":""}' ] && break
  sleep 0.2
done
[ "$watched" == '{"ok":true,"value":"rebuilt Monster","type":""}' ] || hot_fail "the watcher did not rebuild Monster alone: $watched"
expect 'monster.roar()' '{"ok":true,"value":"Goblin roars louder","type":"String"}'
expect 'greeting()' '{"ok":true,"value":"welcome back, visit 42","type":"String"}'
# A function a reload adds is listed and callable at the prompt: reflection follows the reload (D211).
printf '\nfunc growl(): String {\n    return "{name} growls"\n}\n' >> "$hot_folder/monster.spite"
grown=""
for attempt in $(seq 1 150); do
  ask reload > /dev/null
  grown=$(ask 'monster.growl()')
  [ "$grown" == '{"ok":true,"value":"Goblin growls","type":"String"}' ] && break
  sleep 0.2
done
[ "$grown" == '{"ok":true,"value":"Goblin growls","type":"String"}' ] || hot_fail "a function added by a reload was not callable: $grown"
expect 'exit' '{"ok":true,"value":"","type":""}'
for attempt in $(seq 1 50); do kill -0 $served 2>/dev/null || break; sleep 0.2; done
kill -0 $served 2>/dev/null && hot_fail "the program kept running after exit"
wait $served || hot_fail "the program ended with exit code $?"
echo "live reload: an edited class was swapped in by reload and another by the watcher, keeping the program's state, and a new function answers at the prompt"

# --final-classes writes the program back out as Spite source. What it writes has to be a program:
# printing a corpus program and running what came out must print the same thing. symbol_codegen proves the
# functions Spite made from a template are printed as real functions (D61).
for printed_program in conformance/stage3/interpolation conformance/stage6/symbol_codegen; do
  printed_name=$(basename "$printed_program")
  printed="$work/final/$printed_name"   # a program is a folder named like its entry file (D89)
  "$work/generation_two.exe" "$printed_program" --run=false --final-classes="$printed" > /dev/null 2>&1 || {
    echo "FAILED: --final-classes could not write $printed_program out"; exit 1; }
  printed_output=$("$work/generation_two.exe" "$printed" --debug-memory --executable-path="$work/final_$printed_name.exe" < /dev/null 2>&1 | tr -d '' | grep -v '^allocations: ')
  if [ "$printed_output" != "$(tr -d '' < "$printed_program/expected_output.txt")" ]; then
    echo "FAILED: the printed $printed_name does not run like the one it was printed from"; echo "$printed_output" | head -6; exit 1
  fi
done
echo "final classes: the printed program runs the same"

# Each operating system's folder in library/ reopens the classes it changes (D80). Only this machine's can run
# here, so the others are held to compiling: the compiler writes itself out once for each.
for operating_system in windows linux mac; do
  "$work/generation_two.exe" bootstrap --run=false --c-source --c-path="$work/compiler_$operating_system.c" --target-operating-system=$operating_system || {
    echo "FAILED: the compiler does not compile with library/$operating_system"; exit 1; }
  "$CC_BIN" -fsyntax-only -w "$work/compiler_$operating_system.c" 2> "$work/c_errors.txt" || {
    echo "FAILED: the C written for library/$operating_system does not compile"; head -5 "$work/c_errors.txt"; exit 1; }
  # The compiler names no time zone, so a program that does is written out too: each system reads zones its own way.
  "$work/generation_two.exe" conformance/stage6/daylight_saving --run=false --c-source --c-path="$work/zones_$operating_system.c" --target-operating-system=$operating_system || {
    echo "FAILED: time zones do not compile with library/$operating_system"; exit 1; }
  "$CC_BIN" -fsyntax-only -w "$work/zones_$operating_system.c" 2> "$work/c_errors.txt" || {
    echo "FAILED: the time zone C written for library/$operating_system does not compile"; head -5 "$work/c_errors.txt"; exit 1; }
  # The compiler never watches files, so a program that does is written out too: each system asks its own kernel.
  "$work/generation_two.exe" conformance/stage6/file_watching --run=false --c-source --c-path="$work/watching_$operating_system.c" --target-operating-system=$operating_system || {
    echo "FAILED: Watcher does not compile with library/$operating_system"; exit 1; }
  "$CC_BIN" -fsyntax-only -w "$work/watching_$operating_system.c" 2> "$work/c_errors.txt" || {
    echo "FAILED: the Watcher C written for library/$operating_system does not compile"; head -5 "$work/c_errors.txt"; exit 1; }
  # The compiler only talks lines on 127.0.0.1, so programs that resolve names and move bytes, waiting and not, are too.
  for socket_program in socket_bytes socket_waits; do
    "$work/generation_two.exe" conformance/stage6/$socket_program --run=false --c-source --c-path="$work/${socket_program}_$operating_system.c" --target-operating-system=$operating_system || {
      echo "FAILED: $socket_program does not compile with library/$operating_system"; exit 1; }
    "$CC_BIN" -fsyntax-only -w "$work/${socket_program}_$operating_system.c" 2> "$work/c_errors.txt" || {
      echo "FAILED: the C of $socket_program written for library/$operating_system does not compile"; head -5 "$work/c_errors.txt"; exit 1; }
  done
done
echo "operating systems: the compiler, a time zone program, a file watching program and a socket program compile with the windows, linux and mac library folders"

# The compiler is the formatter: every file outside diagnostics/ (whose expected errors carry line numbers) is
# already in the one style, so formatting it changes nothing. `spite format --check` lists every file that would
# change and fails; a docs/ program that must fail may be wrong on purpose, formatting included.
formatted_folders=(bootstrap launcher library tests conformance examples scripts benchmarks)
for folder in .spite-cache/docs/*/; do
  [ -f "$folder/must_fail.txt" ] || formatted_folders+=("$folder")
done
unformatted=$("$work/generation_two.exe" format --check "${formatted_folders[@]}" 2>&1 | tr -d '\r')
if [ -n "$unformatted" ]; then echo "FAILED: not formatted (run: bin/spite format <path>):"; echo "$unformatted"; exit 1; fi
echo "formatting: every file is in the one style"

if cmp -s "$work/generation_two.c" bootstrap/seed/spite_compiler.c; then
  echo "OK: fixpoint holds and the committed seed is current"
elif [ "$1" == "--update-seed" ]; then
  cp "$work/generation_two.c" bootstrap/seed/spite_compiler.c; echo "OK: fixpoint holds; bootstrap/seed/spite_compiler.c updated"
else
  echo "OK: fixpoint holds, but the compiler sources changed since the seed was written: run  bash check.sh --update-seed"
fi
