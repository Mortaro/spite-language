#!/bin/bash
# Proves the Spite compiler still compiles itself, still passes the conformance corpus, and still does
# what docs/ says it does.
# Needs only a C compiler. Set CC to choose one, otherwise the first available of cc, clang or gcc
# is used.  Run from anywhere:   bash check.sh
# Update the committed seed after an intended compiler change:   bash check.sh --update-seed
# The programs are checked side by side, as many at once as there are processors. CHECK_JOBS sets how many
# (CHECK_JOBS=1 checks one at a time); the report is the same either way, since each result is printed in a fixed
# order once every program has run, so one program failing never hides another.
cd "$(dirname "$0")" || exit 1
work=.spite/check_$$   # one folder per run: two sessions may run this at the same time
trap 'rm -rf "$work"' EXIT
mkdir -p "$work"
job_errors="$work/c_errors.txt"   # what a failed C compile said; each job of a pool below keeps its own
jobs=${CHECK_JOBS:-$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}

# SPITE.md: no em dashes anywhere, neither the character nor two hyphens between spaces standing in for one, and
# no third-party package named inside the language. Every tracked text file is read but the seed, which is C the
# compiler writes from these sources. A '--' between spaces is allowed only where it is a command line's own
# separator: before a flag, after 'spite <program>' or 'set', and between two holes of a Spite text.
# It runs while the seed is compiled, and its answer is read once the seed is built.
writing_check() {
  em_dash=$(printf '\342\200\224')
  stand_in=' -''- '
  dashes=$(git grep -n -I -F -e "$em_dash" -e "$stand_in" ':!bootstrap/seed' \
    | grep -v -E "$stand_in-|(spite|bin/spite|spite\.exe) [^ ]+$stand_in|set$stand_in|\}$stand_in\{")
  if [ -n "$dashes" ]; then
    echo "FAILED: em dashes (end the sentence, or use a colon, a comma or parentheses):"; echo "$dashes"; exit 1
  fi
  # the names are written in pieces so this file does not name them
  names="(^|[^[:alnum:]])sl""op(eng""ine)?([^[:alnum:]]|$)|thes""eus"
  package_names=$(git grep -n -I -i -E "$names" ':!bootstrap/seed')
  if [ -n "$package_names" ]; then
    echo "FAILED: a third-party package is named (use a neutral invented one):"; echo "$package_names"; exit 1
  fi
  echo "writing: no em dashes, and no third-party package named"
}
( writing_check ) > "$work/writing.txt" 2>&1 &
writing=$!
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

# Each generation writes its C to the default place, .spite/build/bootstrap/bootstrap.c (D283; a seed from before
# D283 writes bootstrap/bootstrap.c beside the program), and it is moved into the work folder at once: every Build
# field is a constant in the built compiler, so a --c-path naming this run's folder would be written into the C and
# no two generations (or seeds) would ever be equal. Given a third argument, the same compile also builds that
# executable the way any program is built, from translation units compiled in parallel and cached in .spite/objects
# (docs/compiler.md): how the executable is made changes nothing in the C, which is what the generations compare.
compile_compiler() {
  rm -f .spite/build/bootstrap/bootstrap.c bootstrap/bootstrap.c
  if [ -n "$3" ]; then
    "$1" bootstrap --run=false --c-source --executable --translation-units="$jobs" --executable-path="$3" || return 1
  else
    "$1" bootstrap --run=false --c-source || return 1
  fi
  produced=.spite/build/bootstrap/bootstrap.c
  [ -f "$produced" ] || produced=bootstrap/bootstrap.c
  cp "$produced" "$2" || return 1   # a copy: Windows may still hold the file, refusing a rename
  for attempt in 1 2 3 4 5 6 7 8 9 10; do   # and a virus scanner may hold it a moment longer, refusing the delete
    rm -f "$produced" 2>/dev/null && return 0
    sleep 1
  done
  rm -f "$produced"
}

# The seed only has to write the compiler's C once, so it is compiled without optimisation: that takes a tenth of
# the time, and the slower seed still finishes far sooner.
echo "1/4 building the seed compiler"
"$CC_BIN" -O0 -Wno-parentheses-equality -Wno-deprecated-declarations bootstrap/seed/spite_compiler.c -o "$work/seed.exe" $maths_library 2> "$job_errors" || { head -20 "$job_errors"; exit 1; }
wait $writing; written=$?
cat "$work/writing.txt"
[ "$written" == "0" ] || exit 1

echo "2/4 the seed compiles the compiler sources (generation 2)"
compile_compiler "$work/seed.exe" "$work/generation_two.c" "$work/generation_two.exe" || { echo "FAILED: the seed could not compile the compiler sources"; exit 1; }
cp "$work/generation_two.exe" .spite/spite_development.exe   # the freshly built compiler, handy for trying things by hand

# Everything below that only needs generation 2 is one job of a pool, and the pool runs `jobs` of them at once:
# generation 3, the corpus and examples, the documentation's programs, the diagnostics, the benchmarks, the tests,
# the compiler's own memory, each operating system's library, the fast reloads, --final-classes and the launcher.
# Each job's output and exit code go into its own files, and the report reads them in the order of the job list,
# so it never depends on which job ended first.
# A job runs in a subshell: its 'exit 1' ends that job alone.
repository=$(pwd)
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) position_independent="" ;; *) position_independent="-fPIC" ;; esac
export work jobs repository CC_BIN maths_library

job_generation_three() {
  compile_compiler "$work/generation_two.exe" "$work/generation_three.c" || { echo "FAILED: generation 2 could not compile the compiler sources"; exit 1; }
}

# A corpus program or an example: its output must be exactly expected_output.txt, and it must free everything it takes.
job_program() {
  local folder=$1 name flags input executable actual expected body balance allocations frees
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")   # compiler flags such as --environment=server
  input=/dev/null; [ -f "$folder/input.txt" ] && input="$folder/input.txt"   # what the program reads from the console
  # the executable goes into the work folder rather than beside the program, so the repository stays clean, under the
  # program's own path, so two programs of one name never share one, and named like the program, as its crashes name it
  executable="$work/${folder%/}.exe"
  mkdir -p "$(dirname "$executable")"
  actual=$("$work/generation_two.exe" "$folder" --debug-memory --executable-path="$executable" $flags < "$input" 2>&1 | tr -d '\r')
  # some toolchains intermittently fail to open their own cache files on Windows; that is not a
  if echo "$actual" | grep -q "failed to check cache"; then
    actual=$("$work/generation_two.exe" "$folder" --debug-memory --executable-path="$executable" $flags < "$input" 2>&1 | tr -d '\r')
  fi
  expected=$(tr -d '\r' < "$folder/expected_output.txt")
  body=$(echo "$actual" | grep -v '^allocations: ')
  balance=$(echo "$actual" | grep '^allocations: ' | sed -E 's/allocations: ([0-9]+) frees: ([0-9]+)/\1 \2/')
  allocations=${balance% *}; frees=${balance#* }
  if [ -f "$folder/crashes.txt" ]; then
    # a program that is meant to crash: its whole output (stdout and the crash line) must match, and there is no
    # balance line because a crash halts before the program would have released anything. A native fault names where
    # it stopped as module+offset (D244), and the offset is the C compiler's, so it is compared without it.
    actual=$(echo "$actual" | sed -E 's/\+0x[0-9a-f]+/+0x.../g')
    [ "$actual" == "$expected" ] && exit 0
    echo "FAILED: $name"; echo "$actual" | head -8; exit 1
  fi
  # allocations.txt pins how many allocations a program makes, so an optimisation that removes them stays removed
  if [ -f "$folder/allocations.txt" ] && [ "$allocations" != "$(tr -d '\r\n' < "$folder/allocations.txt")" ]; then
    echo "FAILED: $name allocated $allocations times, allocations.txt says $(tr -d '\r\n' < "$folder/allocations.txt")"; exit 1
  fi
  [ "$body" == "$expected" ] && [ -n "$balance" ] && [ "$allocations" == "$frees" ] && exit 0
  echo "FAILED: $name"; echo "$actual" | head -8; exit 1
}

# Every program written in docs/ and README.md is a program: scripts/docs_corpus (itself Spite) writes each titled
# code block out, and each one has to compile, run, print its ```output block and free everything it took.
# A block marked `error` must fail to compile with its ```diagnostic text somewhere in the message.
job_documentation() {
  local folder=$1 name flags actual expected body balance
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")
  if [ -f "$folder/must_fail.txt" ]; then
    actual=$("$work/generation_two.exe" "$folder" --run=false $flags 2>&1 >/dev/null | tr -d '\r')
    expected=$(tr -d '\r' < "$folder/expected_diagnostic.txt")
    [ -n "$actual" ] && [ "${actual#*$expected}" != "$actual" ] && exit 0
    echo "FAILED docs: $name wanted an error saying '$expected'"; echo "$actual" | head -4; exit 1
  fi
  # a program that loads a repository pinned to a commit (docs/packages.md) says so the first time it fetches it
  actual=$("$work/generation_two.exe" "$folder" --debug-memory --executable-path="$work/docs_$name.exe" $flags < /dev/null 2>&1 | tr -d '\r' | grep -v '^fetched .* into ')
  expected=$(tr -d '\r' < "$folder/expected_output.txt")
  body=$(echo "$actual" | grep -v '^allocations: ')
  balance=$(echo "$actual" | grep '^allocations: ' | sed -E 's/allocations: ([0-9]+) frees: ([0-9]+)/\1 \2/')
  [ "$body" == "$expected" ] && [ -n "$balance" ] && [ "${balance% *}" == "${balance#* }" ] && exit 0
  echo "FAILED docs: $name"; echo "$actual" | head -6; exit 1
}

# Programs that must NOT compile: the errors are the language's main channel to whoever (or whatever) writes the code.
# Every compile formats the program first (D190), and some of these are unformatted on purpose, so each is compiled
# from a copy in the work folder, from where its paths read as they do here.
job_diagnostic() {
  local folder=$1 name flags actual expected
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")
  actual=$(cd "$work/unformatted" && "$repository/$work/generation_two.exe" "$folder" --run=false $flags 2>&1 >/dev/null | tr -d '\r')
  expected=$(tr -d '\r' < "$folder/expected_errors.txt")
  [ "$actual" == "$expected" ] && exit 0
  echo "FAILED diagnostics: $name"; echo "$actual" | head -8; exit 1
}

# The benchmarks (benchmarks/README.md) are timed by hand with benchmarks/run.sh; here they only have to compile.
job_benchmark() {
  local folder=$1 name errors
  name=$(basename "$folder")
  # a program measured against C (benchmarks/versus_c/run.sh) brings its C twin, which has to compile too
  if [ -f "$folder/twin.c" ]; then
    errors=$("$CC_BIN" -fsyntax-only -w "$folder/twin.c" 2>&1) || {
      echo "FAILED: the C twin of benchmark $name does not compile"; echo "$errors" | head -5; exit 1; }
  fi
  errors=$("$work/generation_two.exe" "$folder" --run=false --c-source --c-path="$work/benchmark_$name.c" 2>&1) || {
    echo "FAILED: benchmark $name does not compile"; echo "$errors" | head -5; exit 1; }
  errors=$("$CC_BIN" -fsyntax-only -w "$work/benchmark_$name.c" 2>&1) || {
    echo "FAILED: the C of benchmark $name does not compile"; echo "$errors" | head -5; exit 1; }
}

# The tests: a package that crashes (D46). No framework: a test is a function, and `crash` is the assertion.
job_tests() {
  local test_output test_balance
  test_output=$("$work/generation_two.exe" tests --debug-memory --executable-path="$work/tests.exe" < /dev/null 2>&1 | tr -d '\r')
  if echo "$test_output" | grep -q "failed to check cache"; then
    test_output=$("$work/generation_two.exe" tests --debug-memory --executable-path="$work/tests.exe" < /dev/null 2>&1 | tr -d '\r')
  fi
  test_balance=$(echo "$test_output" | grep '^allocations: ' | sed -E 's/allocations: ([0-9]+) frees: ([0-9]+)/\1 \2/')
  if [ "$(echo "$test_output" | grep -vc '^allocations: ')" != "0" ] || [ -z "$test_balance" ] || [ "${test_balance% *}" != "${test_balance#* }" ]; then
    echo "FAILED: tests"; echo "$test_output" | head -8; exit 1
  fi
}

# The compiler is held to the corpus standard too: compiling itself, it frees everything it takes. The table that
# counts is only in a --debug-memory build, so the compiler builds itself once more with it.
job_compiler_memory() {
  local self_leaks
  "$work/generation_two.exe" bootstrap --run=false --debug-memory --executable --translation-units="$jobs" --executable-path="$work/generation_two_debug.exe" || {
    echo "FAILED: the compiler does not build itself with --debug-memory"; exit 1; }
  self_leaks=$("$work/generation_two_debug.exe" bootstrap --run=false 2>&1 > /dev/null | head -5)   # no output: compile only
  if [ -n "$self_leaks" ]; then echo "FAILED: the compiler leaks while compiling itself"; echo "$self_leaks"; exit 1; fi
}

# Each operating system's folder in library/ reopens the classes it changes (D80). Only this machine's can run
# here, so the others are held to compiling: the compiler writes itself out once for each, and so does each program
# that asks the system for what the compiler never does (time zones, file watching, clocks, mapped files, sockets,
# UDP and secure random numbers), since each system answers its own way.
job_target() {
  local system=$1 program=$2 name errors
  name=$(basename "$program")
  "$work/generation_two.exe" "$program" --run=false --c-source --c-path="$work/target_${system}_$name.c" --target-operating-system="$system" || {
    echo "FAILED: $program does not compile with library/$system"; exit 1; }
  errors=$("$CC_BIN" -fsyntax-only -w "$work/target_${system}_$name.c" 2>&1) || {
    echo "FAILED: the C of $program written for library/$system does not compile"; echo "$errors" | head -5; exit 1; }
}

# A reload compiles only the classes of the changed files (docs/repl.md#how-it-works), from what the running
# program's manifest says about the rest. SPITE_RELOAD_CHECK makes a reload of the file it names compile both
# ways and compare what they write; a fast reload that differs from a whole compile is a bug. Every file of these
# programs is checked, and two edits of hot_counter must fall back to a whole compile: a changed parameter list,
# and a change that makes a function write another class's attribute (the call effects its callers were compiled
# with change). Each copy sits as deep below the repository as a conformance program, so its comments' links resolve.
fast_root="${work}_fast_reload"
trap 'rm -rf "$work" "$fast_root"' EXIT
mkdir -p "$work/fast_reload" "$fast_root"
checked_port=$((20001 + $$ % 20000))   # written into each build; nothing listens on it, since nothing runs
export fast_root checked_port
job_fast_reload() {
  local program=$1 name copy flags errors file answer checked=0
  name=$(basename "$program"); copy="$fast_root/$name"
  cp -r "$program" "$copy"
  flags=""; [ -f "$copy/flags.txt" ] && flags=$(tr -d '\r\n' < "$copy/flags.txt")
  errors=$("$work/generation_two.exe" "$copy" --executable --run=false --hot-reload --repl-port=$checked_port --executable-path="$work/fast_reload/$name.exe" $flags 2>&1) || {
    echo "FAILED fast reload: $name does not build with --hot-reload"; echo "$errors" | head -5; exit 1; }
  for file in "$copy"/*.spite; do
    case "$(basename "$file")" in environment.spite|build.spite) continue ;; esac
    answer=$(SPITE_RELOAD_CHECK="$file" "$work/generation_two.exe" reload "$copy" --hot-reload --repl-port=$checked_port --executable-path="$work/fast_reload/$name.exe" $flags 2>&1 | tr -d '\r')
    case "$answer" in
      "reload check: the fast reload matches a whole compile"*) checked=$((checked + 1)) ;;
      *) echo "FAILED fast reload: $name/$(basename "$file"): $answer" | head -c 600; echo; exit 1 ;;
    esac
  done
  echo "$checked" > "$work/fast_reload/$name.checked"
  [ "$name" == "hot_counter" ] || exit 0
  cp "$copy/monster.spite" "$work/fast_reload/monster.spite"
  sed -i 's/^func roar(): String {$/func roar(_loudness: Integer): String {/' "$copy/monster.spite"
  sed -i 's/monster\.roar()/monster.roar(2)/' "$copy/hot_counter.spite"
  fall_back "a function of a changed class has another parameter list or return type" "a new parameter"
  cp "$work/fast_reload/monster.spite" "$copy/monster.spite"
  sed -i 's/monster\.roar(2)/monster.roar()/' "$copy/hot_counter.spite"
  sed -i 's/^func greeting(): String {$/func greeting(): String {\n    monster.name = "Orc"/' "$copy/hot_counter.spite"
  fall_back "" "a function that writes another class's attribute"
}
fall_back() {
  local answer
  answer=$(SPITE_RELOAD_CHECK=1 "$work/generation_two.exe" reload "$fast_root/hot_counter" --hot-reload --repl-port=$checked_port --executable-path="$work/fast_reload/hot_counter.exe" 2>&1 | tr -d '\r')
  case "$answer" in
    "reload check: whole $1"*) ;;
    *) echo "FAILED fast reload: after $2 a reload answered: $answer" | head -c 600; echo; exit 1 ;;
  esac
}

# --final-classes writes the program back out as Spite source. What it writes has to be a program:
# printing a corpus program and running what came out must print the same thing. symbol_codegen proves the
# functions Spite made from a template are printed as real functions (D61).
job_final_classes() {
  local printed_program=$1 printed_name printed printed_output
  printed_name=$(basename "$printed_program")
  printed="$work/final/$printed_name"   # a program is a folder named like its entry file (D89)
  "$work/generation_two.exe" "$printed_program" --run=false --final-classes="$printed" > /dev/null 2>&1 || {
    echo "FAILED: --final-classes could not write $printed_program out"; exit 1; }
  printed_output=$("$work/generation_two.exe" "$printed" --debug-memory --executable-path="$work/final_$printed_name.exe" < /dev/null 2>&1 | tr -d '\r' | grep -v '^allocations: ')
  if [ "$printed_output" != "$(tr -d '\r' < "$printed_program/expected_output.txt")" ]; then
    echo "FAILED: the printed $printed_name does not run like the one it was printed from"; echo "$printed_output" | head -6; exit 1
  fi
}

# bin/spite passes a program's own arguments through untouched: Git for Windows' bash would rewrite ones that look
# like POSIX paths (`/Game/Legacy/` into `C:/Program Files/Git/Game/Legacy/`) on their way to a Windows program.
# The launcher builds .spite/spite.exe from the seed when that is missing or older, so this may be the longest job.
job_launcher() {
  local launched
  launched=$(bin/spite conformance/stage6/launcher_arguments --executable-path="$work/launched.exe" -- --prefixes=/Game/Legacy/ /usr/share "a b" < /dev/null 2>&1 | tr -d '\r' | grep -v '^spite: building the compiler')
  if [ "$launched" != "$(printf '[--prefixes=/Game/Legacy/]\n[/usr/share]\n[a b]')" ]; then
    echo "FAILED: bin/spite changed the program's arguments after --"; echo "$launched" | head -5; exit 1
  fi
}

run_job() {
  local index=$1 kind=$2
  shift 2
  ( job_errors="$work/results/$index.errors"; "job_$kind" "$@" ) > "$work/results/$index.txt" 2>&1 < /dev/null
  echo $? > "$work/results/$index.status"
}

# A program that calls a foreign library brings the library's C as fixture.c; it is built next to it here, as
# fixture.dll on every platform, so the program can name one real file (D71); dlopen does not mind the name.
for fixture in conformance/*/*/fixture.c; do
  [ -f "$fixture" ] || continue
  "$CC_BIN" -shared $position_independent -w "$fixture" -o "$(dirname "$fixture")/fixture.dll" > "$job_errors" 2>&1 || { echo "FAILED: could not build $fixture"; head -5 "$job_errors"; exit 1; }
done
rm -rf .spite/docs   # so a program deleted from docs/ stops being checked
"$work/generation_two.exe" scripts/docs_corpus --executable-path="$work/docs_corpus.exe" > /dev/null || {
  echo "FAILED: could not extract the documentation's programs"; exit 1; }
mkdir -p "$work/unformatted" && cp -r diagnostics "$work/unformatted/"

# The longest jobs come first, so the pool does not end waiting on one of them.
{
  echo generation_three
  echo launcher
  echo compiler_memory
  for program in .spite/docs/hot_counter conformance/stage6/kept_templates conformance/stage6/items_columns                  conformance/stage6/class_argument conformance/stage6/json_symbols conformance/stage6/attribute_object                  conformance/stage6/foreign_callbacks conformance/stage6/waiting_systems conformance/stage6/allocator_choice                  examples/dungeon; do
    echo "fast_reload $program"
  done
  for system in windows linux mac; do
    for program in bootstrap conformance/stage6/daylight_saving conformance/stage6/file_watching conformance/stage6/clock_reads \
                   conformance/stage6/mapped_files conformance/stage6/socket_bytes conformance/stage6/socket_waits \
                   conformance/stage6/datagrams conformance/stage6/hashes; do
      echo "target $system $program"
    done
  done
  echo tests
  echo "final_classes conformance/stage3/interpolation"
  echo "final_classes conformance/stage6/symbol_codegen"
  for folder in conformance/*/*/ examples/*/; do echo "program $folder"; done   # the examples are held to the same standard as the corpus
  for folder in .spite/docs/*/; do echo "documentation $folder"; done
  for folder in benchmarks/*/ benchmarks/versus_c/*/; do
    [ "$(basename "$folder")" == "versus_c" ] || echo "benchmark $folder"   # a suite of programs, each checked on its own
  done
  for folder in diagnostics/*/; do echo "diagnostic $folder"; done
} | awk '{ print NR, $0 }' > "$work/jobs.txt"

# Each job is started by a new bash, which reads the functions from a file: Windows refuses to start a process whose
# environment is as large as these functions exported would make it.
run_pool() {
  rm -rf "$work/results"; mkdir -p "$work/results"
  declare -f > "$work/functions.sh"
  xargs -P "$jobs" -L 1 bash -c '. "$work/functions.sh"; run_job "$@"' run_job < "$work/jobs.txt"
}

# The report prints each job's output in the order of the job list, never in the order the jobs ended, and counts
# the results of each kind; a job that left no exit code failed.
report_pool() {
  failures=0; passed=0; failed=0; documented=0; undocumented=0; checked=0; wrong=0; benchmarked=0
  while read -r index kind rest; do
    status=""; [ -f "$work/results/$index.status" ] && read -r status < "$work/results/$index.status"
    [ -s "$work/results/$index.txt" ] && tr -d '\r' < "$work/results/$index.txt"
    if [ "$status" != "0" ]; then
      failures=$((failures+1))
      [ -n "$status" ] || echo "FAILED: the job '$kind $rest' left no result"
    fi
    case "$kind" in
      program) if [ "$status" == "0" ]; then passed=$((passed+1)); else failed=$((failed+1)); fi ;;
      documentation) if [ "$status" == "0" ]; then documented=$((documented+1)); else undocumented=$((undocumented+1)); fi ;;
      diagnostic) checked=$((checked+1)); [ "$status" == "0" ] || wrong=$((wrong+1)) ;;
      benchmark) [ "$status" == "0" ] && benchmarked=$((benchmarked+1)) ;;
    esac
  done < "$work/jobs.txt"
}

echo "3/4 generation 2 compiles the compiler sources again (generation 3), while the programs are checked: must be byte identical"
echo "4/4 conformance corpus, examples, documentation, diagnostics, benchmarks and tests with generation 2, $jobs at a time"
run_pool
generated=""; [ -f "$work/results/1.status" ] && read -r generated < "$work/results/1.status"
if [ "$generated" == "0" ] && ! cmp -s "$work/generation_two.c" "$work/generation_three.c"; then
  # A change to how the compiler compiles ITS OWN source needs one more generation to settle, and every program is
  # checked again with the generation that settled.
  echo "    generation 2 and 3 differ: trying one more generation"
  compile_compiler "$work/generation_two.exe" "$work/generation_three.c" "$work/generation_three.exe" || exit 1
  compile_compiler "$work/generation_three.exe" "$work/generation_four.c" || exit 1
  if ! cmp -s "$work/generation_three.c" "$work/generation_four.c"; then echo "FAILED: no fixpoint, generation 3 and 4 still emit different C"; exit 1; fi
  cp "$work/generation_three.c" "$work/generation_two.c"; cp "$work/generation_three.exe" "$work/generation_two.exe"
  cp "$work/generation_two.exe" .spite/spite_development.exe
  rm -rf "$work/unformatted" && mkdir -p "$work/unformatted" && cp -r diagnostics "$work/unformatted/"
  rm -rf "$fast_root" && mkdir -p "$fast_root"   # the fast reloads copy their programs afresh
  run_pool
fi

report_pool
echo "conformance and examples: $passed passed, $failed failed"
echo "documentation: $documented passed, $undocumented failed"
echo "diagnostics: $checked checked, $wrong wrong"
echo "benchmarks: $benchmarked compile"
[ "$failures" == "0" ] || { echo "FAILED: $failures jobs failed"; exit 1; }
echo "generation 3: byte identical to generation 2"
echo "tests: passed"
echo "compiler memory: compiling itself frees everything it takes"
echo "fast reload: $(cat "$work/fast_reload/"*.checked | awk '{ sum += $1 } END { print sum }') files reloaded by compiling only their classes match a whole compile, and a changed signature or call effect compiles the whole program"
echo "final classes: the printed program runs the same"
echo "launcher: bin/spite passes the program's arguments after '--' as they were typed"
echo "operating systems: the compiler, a time zone program, a file watching program, a clock program, a mapped file program, socket and UDP programs and a secure random program compile with the windows, linux and mac library folders"
# Every compile formats first (D190) and the formatter deletes an empty line inside a function (D196), so compiling
# diagnostics/blank_line above fixed its copy: the copy changed, and is now in the one style.
if cmp -s "$work/unformatted/diagnostics/blank_line/blank_line.spite" diagnostics/blank_line/blank_line.spite \
   || [ -n "$("$work/generation_two.exe" format --check "$work/unformatted/diagnostics/blank_line" 2>&1)" ]; then
  echo "FAILED: compiling diagnostics/blank_line should have deleted the empty line inside its function"; exit 1
fi
echo "format: compiling deletes an empty line inside a function"

# A program runs in the folder spite was run from, not the language's: the compiler finds library/ and launcher/
# from its own executable, so a relative path the program opens is the caller's. With no --executable-path its
# executable goes into that folder's .spite/ (D283): a program outside the folder into .spite/elsewhere/, one inside
# it into .spite/build/<its path>/, and nothing is ever written beside the program's source.
job_places() {
repository=$(pwd)
mkdir -p "$work/elsewhere/inside"
echo "hello" > "$work/elsewhere/greeting.txt"
beside="$repository/conformance/stage6/working_directory/working_directory"
elsewhere=$(cd "$work/elsewhere" && "$repository/$work/generation_two.exe" "$repository/conformance/stage6/working_directory" 2>&1 | tr -d '\r')
if [ "$elsewhere" != "read hello from the folder spite was run in" ] || [ "$(ls -A "$work/elsewhere" | tr '\n' ' ')" != ".spite greeting.txt inside " ]; then
  echo "FAILED: a program run from another folder does not open its relative paths there"; echo "$elsewhere" | head -5; ls -A "$work/elsewhere"; exit 1
fi
if [ -f "$beside.crashes" ] || [ -f "$beside.exe" ] || [ -f "$beside" ]; then echo "FAILED: a build wrote beside the program's source"; exit 1; fi
ls "$work/elsewhere/.spite/elsewhere/"working_directory_*/working_directory.crashes > /dev/null 2>&1 || {
  echo "FAILED: the executable of a program outside the working folder was not built into .spite/elsewhere/"; exit 1; }
mkdir -p "$work/elsewhere/inside/working_directory"
cp conformance/stage6/working_directory/working_directory.spite "$work/elsewhere/inside/working_directory/"
inside=$(cd "$work/elsewhere" && "$repository/$work/generation_two.exe" inside/working_directory --c-source 2>&1 | tr -d '\r')
if [ "$inside" != "read hello from the folder spite was run in" ] || [ "$(ls -A "$work/elsewhere/inside/working_directory")" != "working_directory.spite" ] \
   || [ ! -f "$work/elsewhere/.spite/build/inside/working_directory/working_directory.c" ] \
   || [ ! -f "$work/elsewhere/.spite/build/inside/working_directory/working_directory.crashes" ]; then
  echo "FAILED: a program inside the working folder was not built into .spite/build/<its path>/"; echo "$inside" | head -5; exit 1
fi
echo "working directory: a program opens relative paths in the folder spite was run from, and its outputs go into that folder's .spite/"

# D227: 'load' also takes an absolute path, for a package that lives in another repository. The path is written
# into a program in the work folder, since no committed program can know where this run's folder is.
absolute_root=$(pwd -W 2>/dev/null || pwd)
mkdir -p "$work/absolute/far_engine" "$work/absolute/absolute_load"
printf 'func name(): String {\n    return "far engine"\n}\n' > "$work/absolute/far_engine/engine.spite"
printf 'var console = Console()\n\nfunc AbsoluteLoad() {\n    load "%s"\n    var engine = Engine()\n    var named = engine.name()\n    console.print(named)\n}\n' \
  "$absolute_root/$work/absolute/far_engine" > "$work/absolute/absolute_load/absolute_load.spite"
absolute=$("$work/generation_two.exe" "$work/absolute/absolute_load" --executable-path="$work/absolute_load.exe" < /dev/null 2>&1 | tr -d '\r')
if [ "$absolute" != "far engine" ]; then echo "FAILED: a program could not load a folder by its absolute path"; echo "$absolute" | head -5; exit 1; fi
echo "absolute load: a program loads a folder named by its absolute path"

# A94: the program's own errors are listed before a loaded package's, which are capped at 25 with a count of the rest.
mkdir -p "$work/error_list/broken_package" "$work/error_list/many_errors"
{ printf 'func run() {\n'; for i in $(seq 1 30); do printf '    missing_%d()\n' "$i"; done; printf '}\n'; } > "$work/error_list/broken_package/thing.spite"
printf 'func ManyErrors() {\n    load "../broken_package"\n    var thing = Thing()\n    thing.run()\n    own_missing()\n}\n' > "$work/error_list/many_errors/many_errors.spite"
listed=$(cd "$work/error_list" && "$repository/$work/generation_two.exe" many_errors --run=false 2>&1 | tr -d '\r')
if [ "$(echo "$listed" | wc -l)" != "27" ] || [ "$(echo "$listed" | head -1)" != "many_errors/many_errors.spite:5: error: this class has no function 'own_missing' (in ManyErrors.ManyErrors)" ] \
   || [ "$(echo "$listed" | tail -1)" != "and 5 more in broken_package" ]; then
  echo "FAILED: the program's own errors are not listed first, with a loaded package's capped at 25"; echo "$listed" | head -3; echo "$listed" | tail -2; exit 1
fi
echo "error list: the program's own errors come first, and a broken package's are capped at 25"
}

# D38, D283: a load names a repository and a commit, 'load "../engine_repo@<commit>/engine"', and the ordinary compile
# checks that commit's files out into the working folder's .spite/git/. The repository is a local one made here, so
# no network is needed; its HEAD moves on after the pin, and the program must keep the pinned commit's code.
job_git_load() {
if command -v git > /dev/null 2>&1; then
  pinned_work="$work/git_load"
  engine_repository="$pinned_work/engine_repo"
  mkdir -p "$engine_repository/engine" "$pinned_work/git_load" "$pinned_work/unknown_commit"
  commit_in() { git -C "$engine_repository" -c user.email=check@spite.invalid -c user.name=check -c commit.gpgsign=false commit -q "$@"; }
  git -C "$engine_repository" init -q
  printf 'func greeting(): String {\n    return "hello from the pinned commit"\n}\n' > "$engine_repository/engine/greeter.spite"
  printf 'var names = List<String>()

func longest(): String {
    var best = ""
    var index = 0
    while index < names.count() {
        var name = names[index]
        if name.length() > best.length() {
            best = name
        }
        index = index + 1
    }
    return best
}
' > "$engine_repository/engine/shelf.spite"
  git -C "$engine_repository" add engine/greeter.spite engine/shelf.spite && commit_in -m pinned
  pinned=$(git -C "$engine_repository" rev-parse --short=7 HEAD)
  printf 'func greeting(): String {\n    return "hello from a later commit"\n}\n' > "$engine_repository/engine/greeter.spite"
  commit_in -am later
  printf 'var console = Console()\n\nfunc GitLoad() {\n    load "../engine_repo@%s/engine"\n    var greeter = Greeter()\n    var said = greeter.greeting()\n    console.print(said)\n}\n' "$pinned" > "$pinned_work/git_load/git_load.spite"
  first_build=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" git_load < /dev/null 2>&1 | tr -d '\r')
  second_build=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" git_load < /dev/null 2>&1 | tr -d '\r')
  checkout=$(ls -d "$pinned_work/.spite/git/engine_repo_"*/"$pinned" 2>/dev/null)
  if [ "$(echo "$first_build" | tail -1)" != "hello from the pinned commit" ] || ! echo "$first_build" | grep -q "^fetched .*engine_repo@$pinned into " \
     || [ "$second_build" != "hello from the pinned commit" ] || [ ! -f "$checkout/engine/greeter.spite" ] \
     || [ -n "$(git -C "$engine_repository" status --porcelain)" ]; then
    echo "FAILED: a git load did not build the pinned commit, once fetched and then from .spite/git/ with no git work"
    echo "$first_build" | head -5; echo "$second_build" | head -5; exit 1
  fi
  hot=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" git_load --hot-reload --executable --run=false --executable-path="$repository/$work/git_load_hot.exe" 2>&1 | tr -d '\r')
  [ -z "$hot" ] || { echo "FAILED: a --hot-reload build of a program with a git load"; echo "$hot" | head -5; exit 1; }
  printf 'var console = Console()\n\nfunc UnknownCommit() {\n    load "../engine_repo@0000000/engine"\n}\n' > "$pinned_work/unknown_commit/unknown_commit.spite"
  unknown=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" unknown_commit --run=false 2>&1 | tr -d '\r')
  echo "$unknown" | grep -q "^unknown_commit/unknown_commit.spite:4: error: 'load \"../engine_repo@0000000/engine\"': .*engine_repo has no commit 0000000" || {
    echo "FAILED: a git load of a commit the repository does not hold is not an error at its line"; echo "$unknown" | head -5; exit 1; }
  missing_git=$(cd "$pinned_work" && PATH=/nothing "$repository/$work/generation_two.exe" unknown_commit --run=false 2>&1 | tr -d '\r')
  echo "$missing_git" | grep -q "with git, and there is no 'git' on the PATH" || {
    echo "FAILED: a git load without git on the PATH is not an error naming it"; echo "$missing_git" | head -5; exit 1; }
  echo "// changed" >> "$checkout/engine/greeter.spite"
  edited=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" git_load --run=false 2>&1 | tr -d '\r')
  echo "$edited" | grep -q "is not what the commit $pinned of .*engine_repo holds: a checkout is read-only" || {
    echo "FAILED: a changed checkout is not an error"; echo "$edited" | head -5; exit 1; }
  echo "git load: a load pinned to a commit of a local repository is checked out into .spite/git/ once, and stays that commit"
  # D296: two commits of one repository are two libraries. The program pins the engine at the first commit and a
  # plugin repository pins it at the later one; each reads its own version, and a mod folder the program loads reopens
  # the program's version only. One package pinning both commits reads them as two loads, the later reopening the
  # earlier (D334).
  rm -rf "$checkout" "$checkout.files"   # the edited copy above is fetched again
  later=$(git -C "$engine_repository" rev-parse --short=7 HEAD)
  plugin_repository="$pinned_work/plugin_repo"
  mkdir -p "$plugin_repository/plugin" "$pinned_work/two_versions/mods" "$pinned_work/one_package_two_pins"
  printf 'func Plugin() {
    load "../../engine_repo@%s/engine"
}

func told(): String {
    var greeter = Greeter()
    return greeter.greeting()
}

func stored(): String {
    var shelf = Shelf()
    shelf.names.append("the plugin shelf")
    return shelf.longest()
}
' "$later" > "$plugin_repository/plugin/plugin.spite"
  git -C "$plugin_repository" init -q && git -C "$plugin_repository" add plugin/plugin.spite     && git -C "$plugin_repository" -c user.email=check@spite.invalid -c user.name=check -c commit.gpgsign=false commit -q -m plugin
  plugin_commit=$(git -C "$plugin_repository" rev-parse --short=7 HEAD)
  printf 'var console = Console()

func TwoVersions() {
    load "../engine_repo@%s/engine"
    load "../plugin_repo@%s/plugin"
    load "mods"
    var greeter = Greeter()
    var said = greeter.greeting()
    console.print(said)
    var plugin = Plugin()
    var told = plugin.told()
    console.print(told)
}
' "$pinned" "$plugin_commit" > "$pinned_work/two_versions/two_versions.spite"
  printf 'func greeting(): String {
    return "the program modded its own version"
}
' > "$pinned_work/two_versions/mods/greeter.spite"
  two_versions=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" two_versions < /dev/null 2>&1 | grep -v '^fetched ' | tr -d '\r')
  if [ "$two_versions" != "$(printf 'the program modded its own version
hello from a later commit')" ]; then
    echo "FAILED: two commits of one repository are not two libraries, each read by the package that pinned it"; echo "$two_versions" | head -5; exit 1
  fi
  # A function both versions hold unchanged is folded into one, crash sites included (D296).
  mkdir -p "$pinned_work/folded_versions"
  printf 'var console = Console()

func FoldedVersions() {
    load "../engine_repo@%s/engine"
    load "../plugin_repo@%s/plugin"
    var shelf = Shelf()
    shelf.names.append("the program shelf")
    var own = shelf.longest()
    console.print(own)
    var plugin = Plugin()
    var stored = plugin.stored()
    console.print(stored)
}
' "$pinned" "$plugin_commit" > "$pinned_work/folded_versions/folded_versions.spite"
  folded_versions=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" folded_versions --c-source --c-path=folded_versions.c < /dev/null 2>&1 | grep -v '^fetched ' | tr -d '\r')
  if [ "$folded_versions" != "$(printf 'the program shelf\nthe plugin shelf')" ] \
     || [ "$(grep -c "^SpiteString engine_repo_[0-9a-f]*_Shelf_longest(.*) {$" "$pinned_work/folded_versions.c")" != 1 ] \
     || ! grep -q "((__typeof__(&engine_repo_[0-9a-f]*_Shelf_longest))&engine_repo_[0-9a-f]*_Shelf_longest)(" "$pinned_work/folded_versions.c"; then
    echo "FAILED: a function two versions of one repository hold unchanged is not folded into one"; echo "$folded_versions" | head -5; exit 1
  fi
  printf 'var console = Console()

func OnePackageTwoPins() {
    load "../engine_repo@%s/engine"
    load "../engine_repo@%s/engine"
    var greeter = Greeter()
    var said = greeter.greeting()
    console.print(said)
}
' "$pinned" "$later" > "$pinned_work/one_package_two_pins/one_package_two_pins.spite"
  two_pins=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" one_package_two_pins < /dev/null 2>&1 | grep -v '^fetched ' | tr -d '\r')
  [ "$two_pins" = "hello from a later commit" ] || {
    echo "FAILED: one package pinning two commits of one repository does not read them as two loads, the later reopening the earlier"; echo "$two_pins" | head -5; exit 1; }
  mkdir -p "$pinned_work/mixed_pins"
  printf 'func MixedPins() {
    load "../engine_repo@%s/engine"
    load "../engine_repo@%s/engine"
    load "../plugin_repo@%s/plugin"
}
' "$pinned" "$later" "$plugin_commit" > "$pinned_work/mixed_pins/mixed_pins.spite"
  mixed=$(cd "$pinned_work" && "$repository/$work/generation_two.exe" mixed_pins --run=false 2>&1 | tr -d '\r')
  echo "$mixed" | grep -q "error: .* reads that commit as part of the version from $pinned: a commit's files are read once, into one version" || {
    echo "FAILED: a commit one package reads alone and another reads into its version is not an error naming both"; echo "$mixed" | head -5; exit 1; }
  echo "git load: two commits of one repository are two libraries, and one package's two commits are two loads in order"
else
  echo "git load: SKIPPED, there is no git on the PATH to make a repository with"
fi
}

# Every program above is built with --debug-memory, whose allocations go through a locked table. The thread pool is
# also run the way a user runs it (`spite <program>`, compile and run, no flags) with runners started from a
# stage, a singleton reached from the pool, a ThreadLocal and a Lock.
job_units() {
stages=conformance/stage6/parallel_stages
plain=$("$work/generation_two.exe" "$stages" --executable-path="$work/parallel_stages_plain.exe" < /dev/null 2>&1 | tr -d '\r')
if [ "$plain" != "$(tr -d '\r' < "$stages/expected_output.txt")" ]; then
  echo "FAILED: run mode without --debug-memory: parallel_stages"; echo "$plain" | head -5; exit 1
fi
echo "run mode: the thread pool runs without --debug-memory"
# The corpus is small enough to be built from one C file each; an executable is built from several translation units
# compiled in parallel, each object cached by the hash of what it was compiled from (docs/compiler.md). The same
# program built from four units must run the same, and built again it compiles no unit: only the link runs.
split="$work/parallel_stages_split.exe"
units_output=$("$work/generation_two.exe" "$stages" --translation-units=4 --debug-memory --executable-path="$split" < /dev/null 2>&1 | tr -d '\r')
if [ "$(echo "$units_output" | grep -v '^allocations: ')" != "$(tr -d '\r' < "$stages/expected_output.txt")" ] \
   || ! echo "$units_output" | grep -qE '^allocations: ([0-9]+) frees: \1$'; then
  echo "FAILED: parallel_stages built from four translation units"; echo "$units_output" | head -5; exit 1
fi
objects_before=$(ls .spite/objects | grep -c '\.o$')
"$work/generation_two.exe" "$stages" --translation-units=4 --debug-memory --executable --run=false --executable-path="$split" > /dev/null 2>&1 || {
  echo "FAILED: parallel_stages could not be built again from its cached units"; exit 1; }
if [ "$(ls .spite/objects | grep -c '\.o$')" != "$objects_before" ]; then
  echo "FAILED: building parallel_stages again compiled a unit whose object was cached"; exit 1
fi
echo "translation units: a program built from four units runs the same, and building it again only links"
# The fault handler and its function table are written after every function (D255), and must still report when the
# C is split into units and linked with link-time optimisation: an --optimized native_fault_foreign from four units.
faulted=$("$work/generation_two.exe" conformance/stage6/native_fault_foreign --optimized --translation-units=4 --executable-path="$work/native_fault_units.exe" < /dev/null 2>&1 | tr -d '\r')
if ! echo "$faulted" | grep -qE "^spite.fault	[a-z]+-violation	-	-	-	address=0x0	.*	at=fixture.dll\+0x[0-9a-f]+	foreign=read_integer_at	library=conformance/stage6/native_fault_foreign/fixture.dll	from=conformance/stage6/native_fault_foreign/native_fault_foreign.spite:13$" \
   || ! echo "$faulted" | grep -q "^spite.frame	" || ! echo "$faulted" | grep -q "^before the fault 5$"; then
  echo "FAILED: native_fault_foreign built --optimized from four translation units"; echo "$faulted" | head -8; exit 1
fi
echo "native faults: an --optimized program built from four units reports its fault, its last foreign call and its frames"
}

# A class or namespace object is made once, on whichever thread asks first: threaded_class_objects has eight pool
# threads ask for the same class objects at once, and a race shows up as a leak or a double free, so the build the
# corpus made is run a few more times.
job_threads() {
threaded="$work/conformance/stage6/threaded_class_objects.exe"
for attempt in 1 2 3 4 5; do
  raced=$("$threaded" < /dev/null 2>&1 | tr -d '\r')
  if [ "$(echo "$raced" | grep -v '^allocations: ')" != "$(tr -d '\r' < conformance/stage6/threaded_class_objects/expected_output.txt)" ] \
     || ! echo "$raced" | grep -qE '^allocations: ([0-9]+) frees: \1$'; then
    echo "FAILED: threaded_class_objects, run $attempt"; echo "$raced" | head -5; exit 1
  fi
done
echo "threads: eight threads asking for the same class objects make each once, five runs balanced"
# A crash reports the asserts that failed before it, as the ring stood when it crashed (D244). A pool thread that keeps
# failing a guard assert while the program's thread crashes must not keep the report printing (that streamed every
# failed assert and never exited), so the crash ends with its crash line, at most 32 assert lines and an earlier count.
mkdir -p "$work/crash_while_asserting"
cat > "$work/crash_while_asserting/crash_while_asserting.spite" <<'SPITE'
var console = Console()
var program = Program()

func CrashWhileAsserting() {
    var guarding = Parallel(guard_forever)
    program.sleep(100)
    console.print("crashing while a thread fails asserts")
    check(0 - 1)
    console.print("never printed", guarding)
}

func check(value: Integer) {
    crash value > 0
}

func guard_forever(): Integer {
    var index = 0
    while true {
        refuse(index)
        var text: String = index
        index = index + text.length()
    }
    return index
}

func refuse(value: Integer) {
    assert value < 0
}
SPITE
"$work/generation_two.exe" "$work/crash_while_asserting" --optimized --executable --run=false --executable-path="$work/crash_while_asserting.exe" > "$job_errors" 2>&1 || {
  echo "FAILED: could not build crash_while_asserting"; head -5 "$job_errors"; exit 1; }
timeout 60 "$work/crash_while_asserting.exe" < /dev/null > "$work/crash_while_asserting.txt" 2>&1
crashed=$?
reported=$(tr -d '\r' < "$work/crash_while_asserting.txt")
if [ "$crashed" != "1" ] || [ "$(echo "$reported" | grep -c '^spite.crash	')" != "1" ] \
   || [ "$(echo "$reported" | grep -c '^spite.assert	[0-9a-f]\{8\}	')" -gt 32 ] \
   || ! echo "$reported" | tail -1 | grep -qE '^spite.assert	earlier=[0-9]+$'; then
  echo "FAILED: a crash while another thread fails asserts (exit $crashed, $(echo "$reported" | wc -l) lines)"; echo "$reported" | head -3; exit 1
fi
echo "crash reports: a crash while a pool thread keeps failing asserts reports the ring as it stood and exits"
}

# What a production build leaves out is only visible in its C (docs/optimizations.md): hello carries no struct,
# allocate or singleton slot of a library class it never makes, and each singleton singleton_forms reaches from a
# Parallel takes its cheapest safe form (atomics for a counter, nothing for one that never changes or that no
# Parallel reaches), so no lock at all.
job_production_c() {
"$work/generation_two.exe" examples/hello --run=false --c-source --c-path="$work/hello_shaken.c" > /dev/null 2>&1 || {
  echo "FAILED: examples/hello does not write its C"; exit 1; }
if grep -qE "struct (Watcher|Socket|Process|HotReload|ThreadPool|Scheduler|ForeignCallback) \{|(Watcher|Socket|ThreadPool|Scheduler)___allocate|spite_singleton_(ThreadPool|Scheduler)_cache|spite_callback_" "$work/hello_shaken.c"; then
  echo "FAILED: examples/hello's C still carries library classes it never uses"; exit 1
fi
# --debug-memory's table and the allocation counter are only in builds that read them (D177): hello allocates with
# the C library's malloc, realloc and free and nothing beside them.
if grep -qE "AllocationTable|spite_debug_|spite_live_allocation|SPITE_DEBUG_MEMORY" "$work/hello_shaken.c"; then
  echo "FAILED: examples/hello's C carries --debug-memory's table or an allocation counter"; exit 1
fi
# Signed arithmetic is checked for overflow only in a --debug-memory or inspectable build: production is the plain operator.
if grep -qE "__builtin_(add|sub|mul)_overflow|spite_overflowed" "$work/hello_shaken.c"; then
  echo "FAILED: examples/hello's C checks arithmetic for overflow in a production build"; exit 1
fi
# Identical functions are folded into one (D296): two instances of a generic over classes of the same layout keep
# one function, called through a cast, and an instance over a class of another layout keeps its own.
folded="$work/folded_functions.c"
"$work/generation_two.exe" conformance/stage6/folded_functions --run=false --c-source --c-path="$folded" > /dev/null 2>&1 || {
  echo "FAILED: folded_functions does not write its C"; exit 1; }
if grep -q "^float Column__Velocity_total_across(.*) {" "$folded" || ! grep -q "^float Column__Position_total_across(Column__Position\* self) {" "$folded" \
   || ! grep -q "((__typeof__(&Column__Velocity_total_across))&Column__Position_total_across)(self->velocities_)" "$folded" \
   || ! grep -q "^float Column__Label_total_across(Column__Label\* self) {" "$folded"; then
  echo "FAILED: folded_functions should fold Column<Velocity>.total_across into Column<Position>'s and keep Column<Label>'s"; exit 1
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
# D265: a counted loop calling one locked singleton takes its lock once around the loop and calls the unlocked body;
# a loop polling another singleton for what a Parallel posts keeps a lock per call, or it would never see the post.
coarse="$work/coarse_locks.c"
"$work/generation_two.exe" conformance/stage6/coarse_locks --run=false --c-source --c-path="$coarse" > /dev/null 2>&1 || {
  echo "FAILED: coarse_locks does not write its C"; exit 1; }
if ! grep -q "^#define spite_coarse_0_enter() spite_guard_enter(&Tally___guard)$" "$coarse" \
   || ! grep -q "^Tally_add___unguarded(self->tally_, 1);$" "$coarse" || grep -q "Mailbox_take___unguarded(self->mailbox_" "$coarse" \
   || grep -q "Mailbox_put___unguarded(self->mailbox_" "$coarse"; then
  echo "FAILED: coarse_locks should lock Tally once around count_up's loop and Mailbox on every call"; exit 1
fi
# D266: a locked singleton's function that only reads takes the readers' side of its lock, a count on the reading
# thread's own cache line, and a function that writes waits for the readers to leave.
reads="$work/singleton_reads.c"
"$work/generation_two.exe" conformance/stage6/singleton_reads --run=false --c-source --c-path="$reads" > /dev/null 2>&1 || {
  echo "FAILED: singleton_reads does not write its C"; exit 1; }
if ! grep -q "^int64_t\* spite_reading = spite_read_enter(&Column__Transform___guard, Column__Transform___readers);$" "$reads" \
   || ! grep -q "^spite_guard_enter_writing(&Column__Transform___guard, Column__Transform___readers);$" "$reads"; then
  echo "FAILED: singleton_reads should read Column<Transform>.at on the readers' side and write insert on the writer's"; exit 1
fi
# D267: while no task is in flight a locked function runs unlocked, and a task it starts takes the lock it skipped.
unshared="$work/unshared_locks.c"
"$work/generation_two.exe" conformance/stage6/unshared_locks --run=false --c-source --c-path="$unshared" > /dev/null 2>&1 || {
  echo "FAILED: unshared_locks does not write its C"; exit 1; }
if ! grep -A1 "^Parallel__Integer\* Ledger_start_posting(Ledger\* self, int32_t count_) {$" "$unshared" \
     | grep -q "^if (spite_skipped_depth < 16 && __atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0) {$" \
   || ! grep -q "spite_enter_skipped(spite_skipped\[spite_index\])" "$unshared"; then
  echo "FAILED: unshared_locks should skip Ledger's lock while no task runs and take it when one starts"; exit 1
fi
# D269: a row borrows a reference column's element when nothing the rest of its block runs can let go of it, holding
# the column's readers' side for the block in a program with threads; a system that reads the column counts it.
lent_elements="$work/lent_list_elements_parallel.c"
"$work/generation_two.exe" conformance/stage6/lent_list_elements_parallel --run=false --c-source --c-path="$lent_elements" > /dev/null 2>&1 || {
  echo "FAILED: lent_list_elements_parallel does not write its C"; exit 1; }
if ! grep -q "^#define spite_lend_0_enter() int64_t\* spite_lend_0 = spite_read_enter(&Column__Trail___guard, Column__Trail___readers)$" "$lent_elements" \
   || ! grep -q "^#define spite_lend_0_read(lent, counted, receiver, index) lent(receiver, index)$" "$lent_elements" \
   || ! grep -q "^#define spite_lend_1_read(lent, counted, receiver, index) counted(receiver, index)$" "$lent_elements"; then
  echo "FAILED: lent_list_elements_parallel should lend Mover its trail under the readers' side and count Peeker's"; exit 1
fi
# D270: a parameter passed on is not counted again; D271: a singleton's attribute nothing reassigns is read in place.
held="$work/held_arguments.c"
"$work/generation_two.exe" conformance/stage6/held_arguments --run=false --c-source --c-path="$held" > /dev/null 2>&1 || {
  echo "FAILED: held_arguments does not write its C"; exit 1; }
if ! grep -q "^int32_t Packer_pack___held_0(Packer\* self, Bag\* bag_, int32_t value_) {$" "$held" \
   || ! grep -q "^int32_t Packer_swap_in___held_1(Packer\* self, Bag\* bag_, Bag\* other_) {$" "$held"; then
  echo "FAILED: held_arguments should pass held bags uncounted, except to the parameter swap_in assigns"; exit 1
fi
attribute_reads="$work/singleton_attribute_reads.c"
"$work/generation_two.exe" conformance/stage6/singleton_attribute_reads --run=false --c-source --c-path="$attribute_reads" > /dev/null 2>&1 || {
  echo "FAILED: singleton_attribute_reads does not write its C"; exit 1; }
if ! grep -q "while (((index_ < List_String_count((self->registry_)->names_)))) {" "$attribute_reads" \
   || ! grep -q "Registry___outside_read_enter(); Board\* " "$attribute_reads"; then
  echo "FAILED: singleton_attribute_reads should read names in place and current under the lock"; exit 1
fi
echo "production C: hello carries no unused class, table or counter, singleton_forms takes no lock, singleton_lock_calls locks only Registry, a stateless function of a locked singleton takes no lock, a counted loop of calls locks once, a reading function takes the readers' side, no task in flight skips the lock, a row borrows a reference column's element, a held argument is not counted again, a singleton's fixed attribute is read in place"
# D201: a whole-number division checks its divisor for zero, except where a proof already shows it is not zero:
# division_by_zero's 'whole / pieces' follows 'assert pieces != 0', so its C carries no check for it.
"$work/generation_two.exe" conformance/stage6/division_by_zero --run=false --c-source --c-path="$work/division.c" > /dev/null 2>&1 || {
  echo "FAILED: division_by_zero does not write its C"; exit 1; }
if grep -q "whole / pieces" "$work/division.c" || ! grep -q "total / parts" "$work/division.c"; then
  echo "FAILED: division_by_zero should check 'total / parts' and not the proven 'whole / pieces'"; exit 1
fi
echo "division: a proven divisor carries no zero check"
# Every '.class' a program compares is known once generics are resolved: walked_class_fold tests 'attribute.class'
# in an attribute walk and 'given == known.class' on an Anything, and its C makes no Spite.Class object at all.
"$work/generation_two.exe" conformance/stage6/walked_class_fold --run=false --c-source --c-path="$work/walked_class.c" > /dev/null 2>&1 || {
  echo "FAILED: walked_class_fold does not write its C"; exit 1; }
if grep -q "spite_class_object_" "$work/walked_class.c"; then
  echo "FAILED: walked_class_fold should compare classes without making a Spite.Class object"; exit 1
fi
echo "class comparisons: a class known while compiling is compared by its id, with no class object made"
# A loop over a list of plain values that cannot change its size reads the count once and its items without a range
# check (docs/optimizations.md): counted_loops' scale_in_place is a plain C loop the C compiler can vectorise, and
# scale_into checks the list it writes once, before the loop; add_from, whose counter starts at a parameter, is not.
"$work/generation_two.exe" conformance/stage6/counted_loops --run=false --c-source --c-path="$work/counted.c" > /dev/null 2>&1 || {
  echo "FAILED: counted_loops does not write its C"; exit 1; }
if ! grep -qE "^spite_temp_[0-9]+\[index_\] = \(spite_temp_[0-9]+\[index_\] \* 2\.0\);$" "$work/counted.c" \
   || ! grep -qE "^if \(spite_temp_[0-9]+ <= spite_temp_[0-9]+\) \{$" "$work/counted.c" \
   || [ "$(grep -cE "^while \(\(\(index_ < (List_Integer_count|\(\(__typeof__\(&List_Integer_count\)\)&[A-Za-z_]+_count\))\(values_\)\)\)\) \{$" "$work/counted.c")" != "2" ]; then
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
# A --hot-reload build keeps every function and, with a REPL, every member template that fits a reachable list:
# kept_templates (run above with --development) must build that way too, its lists of Parallel and ThreadLocal
# and its two unrelated dictionaries included (docs/collections.md#how-the-member-templates-are-written).
"$work/generation_two.exe" conformance/stage6/kept_templates --executable --run=false --hot-reload --repl-port=4000 --executable-path="$work/kept_templates_hot.exe" > "$job_errors" 2>&1 || {
  echo "FAILED: kept_templates does not build with --hot-reload --repl-port"; head -5 "$job_errors"; exit 1; }
echo "live reload: a build that keeps every function compiles the library's templates it keeps"
# Maths on constants is worked out while compiling (docs/optimizations.md): every folded_ value in maths_folding is
# a literal in its C, and the program itself holds each one to the bits the C library computes at run time.
"$work/generation_two.exe" conformance/stage6/maths_folding --run=false --c-source --c-path="$work/folding.c" > /dev/null 2>&1 || {
  echo "FAILED: maths_folding does not write its C"; exit 1; }
if grep -E "(float|double) folded_[a-z_]*_ = " "$work/folding.c" | grep -v "_wide_ = " | grep -qE "Spite(Float|Double)_[a-z_0-9]+\(" \
   || ! grep -qE "float folded_sine_ = \(0x1\.[0-9a-f]+p-2f\);" "$work/folding.c"; then
  echo "FAILED: maths_folding's constant maths should be literals in its C"; exit 1
fi
echo "maths: a maths function of constants is a literal in the C"
}

# A ```wire block in docs/ is a remote REPL session: its program runs with --repl-port, every
# `$ spite connect <port> --command="..."` line is sent with the compiler's own client, and each answer must be
# the JSON line written under it. The port comes from this run's process id, so two runs do not share one.
job_wire() {
port=$((port + 6))   # the live reload checks below take the five after this run's port
sessions=0
for wire in .spite/docs/*/wire.txt; do
  [ -f "$wire" ] || continue
  folder=$(dirname "$wire"); name=$(basename "$folder")
  "$work/generation_two.exe" "$folder" --executable --run=false --repl-port=$port --executable-path="$work/wire_$name.exe" > "$job_errors" 2>&1 || {
    echo "FAILED wire: $name does not build with --repl-port"; head -5 "$job_errors"; exit 1; }
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
}

# Live reload (docs/repl.md): docs/'s hot_counter program runs with --hot-reload and --repl-port from a copy in the
# work folder, which this step edits. An edit followed by `reload` must run the new code with the state set before
# it, and an edit nobody reports must be picked up by the file watcher, rebuilding only its class. Every wait has a
# timeout, and the program is killed if it outlives the session.
job_hot_reload() {
hot_folder="$work/hot_reload/hot_counter"
[ -d .spite/docs/hot_counter ] || { echo "FAILED live reload: docs/repl.md has no hot_counter program"; exit 1; }
mkdir -p "$work/hot_reload"; cp -r .spite/docs/hot_counter "$hot_folder"
"$work/generation_two.exe" "$hot_folder" --executable --run=false --hot-reload --repl-port=$port --executable-path="$work/hot_reload/hot_counter.exe" > "$job_errors" 2>&1 || {
  echo "FAILED live reload: hot_counter does not build with --hot-reload"; head -5 "$job_errors"; exit 1; }
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
expect 'wait_reload' '{"ok":true,"value":"rebuilt HotCounter","type":""}'
expect 'greeting()' '{"ok":true,"value":"welcome back, visit 42","type":"String"}'
sed -i 's/{name} roars/{name} roars louder/' "$hot_folder/monster.spite"
watched=$(ask wait_reload)   # answers once the watcher has swapped in every save it sees
[ "$watched" == '{"ok":true,"value":"rebuilt Monster","type":""}' ] || hot_fail "the watcher did not rebuild Monster alone: $watched"
expect 'monster.roar()' '{"ok":true,"value":"Goblin roars louder","type":"String"}'
expect 'greeting()' '{"ok":true,"value":"welcome back, visit 42","type":"String"}'
# A function a reload adds is listed and callable at the prompt: reflection follows the reload (D211).
printf '\nfunc growl(): String {\n    return "{name} growls"\n}\n' >> "$hot_folder/monster.spite"
ask wait_reload > /dev/null
grown=$(ask 'monster.growl()')
[ "$grown" == '{"ok":true,"value":"Goblin growls","type":"String"}' ] || hot_fail "a function added by a reload was not callable: $grown"
# A file that holds none of the program's own classes, here a first environment.spite reopening Environment, is
# swapped in as well (the whole program is compiled), and so is its removal; then nothing has changed again.
printf 'var greeting_word = "hello"\n' > "$hot_folder/environment.spite"
added=$(ask reload)   # a new file is seen by the watcher's event only, so reload compiles it unless the watcher did
case "$added" in *"nothing changed since the code the program runs"*) added=$(ask wait_reload) ;; esac
case "$added" in
  *'"ok":true'*"greeting_word is new"*) ;;
  *) hot_fail "a reload of a new environment.spite answered $added" ;;
esac
rm "$hot_folder/environment.spite"
removed=$(ask reload)
case "$removed" in *"nothing changed since the code the program runs"*) removed=$(ask wait_reload) ;; esac
case "$removed" in
  *'"ok":true'*"greeting_word is gone"*) ;;
  *) hot_fail "a reload of a removed environment.spite answered $removed" ;;
esac
expect 'reload' '{"ok":true,"value":"nothing changed since the code the program runs","type":""}'
expect 'exit' '{"ok":true,"value":"","type":""}'
for attempt in $(seq 1 50); do kill -0 $served 2>/dev/null || break; sleep 0.2; done
kill -0 $served 2>/dev/null && hot_fail "the program kept running after exit"
wait $served || hot_fail "the program ended with exit code $?"
echo "live reload: an edited class was swapped in by reload and another by the watcher, keeping the program's state, and a new function answers at the prompt"
}

# A reload moves live objects to their class's new attributes (docs/repl.md#changing-a-classs-attributes): docs/'s
# live_party program runs from a copy that this step edits. A kept attribute keeps its value, a new one holds its
# default, a renamed one keeps its value, a removed one is released (in objects and in an Items' own memory), and
# a reload after a move still swaps in a change to a body. Every wait has a timeout.
job_moving_objects() {
party_folder="$work/live_party/live_party"
[ -d .spite/docs/live_party ] || { echo "FAILED moving objects: docs/repl.md has no live_party program"; exit 1; }
mkdir -p "$work/live_party"; cp -r .spite/docs/live_party "$party_folder"
party_port=$((port + 2))
"$work/generation_two.exe" "$party_folder" --executable --run=false --hot-reload --repl-port=$party_port --executable-path="$work/live_party/live_party.exe" > "$job_errors" 2>&1 || {
  echo "FAILED moving objects: live_party does not build with --hot-reload"; head -5 "$job_errors"; exit 1; }
"$work/live_party/live_party.exe" > "$work/live_party/output.txt" 2>&1 < /dev/null &
party=$!
party_fail() { kill $party 2>/dev/null; echo "FAILED moving objects: $1"; head -5 "$work/live_party/output.txt"; exit 1; }
party_ask() { timeout 120 "$work/generation_two.exe" connect $party_port --command="$1" 2>&1 | tr -d '\r'; }
party_expect() { local answer; answer=$(party_ask "$1"); [ "$answer" == "$2" ] || party_fail "'$1' answered $answer, not $2"; }
party_reload() {   # the watcher may swap the save in first: then wait_reload answers what it swapped in
  local answer; answer=$(party_ask reload)
  case "$answer" in *"nothing changed since the code the program runs"*) answer=$(party_ask wait_reload) ;; esac
  case "$answer" in *"$1"*) ;; *) party_fail "a reload answered $answer, without $1" ;; esac
}
listening=false
for attempt in $(seq 1 100); do
  timeout 10 "$work/generation_two.exe" connect $party_port --command=help > /dev/null 2>&1 && { listening=true; break; }
  kill -0 $party 2>/dev/null || break
  sleep 0.2
done
$listening || party_fail "the program never listened on $party_port"
party_expect 'program.hero.level = 7' '{"ok":true,"value":"7","type":"Integer"}'
sed -i 's/^var level = 1$/var level = 1\nvar health = 100/; s/at level {level}"/at level {level} with {health}"/' "$party_folder/hero.spite"
sed -i 's/^var distance = 0.0$/var pace = 2.0\nvar distance = 0.0/' "$party_folder/step.spite"
party_reload "moved every Step to its new attributes: pace is new and holds its default"
party_expect 'describe()' '{"ok":true,"value":"Ann at level 7 with 100, walked 4","type":"String"}'
sed -i 's/^var level = 1$/var rank = 1/; s/at level {level} with {health}"/at rank {rank} with {health}"/' "$party_folder/hero.spite"
held=$(party_ask wait_reload)   # a rename would lose level's values, so the watcher holds the reload and says so
case "$held" in *'"ok":false'*"the reload of Hero is held"*"reload {Hero.attributes['level']: "*) ;; *) party_fail "a rename was not held: $held" ;; esac
party_expect 'describe()' '{"ok":true,"value":"Ann at level 7 with 100, walked 4","type":"String"}'
answer=$(party_ask "reload {Hero.attributes['level']: \"rank\"}")
case "$answer" in *'"ok":true'*"moved every Hero to its new attributes: level is now rank"*) ;; *) party_fail "the reload with a map answered $answer" ;; esac
party_expect 'describe()' '{"ok":true,"value":"Ann at rank 7 with 100, walked 4","type":"String"}'
sed -i '/^var health = 100$/d; s/at rank {rank} with {health}"/at rank {rank}"/' "$party_folder/hero.spite"
party_reload "health is gone, and what it held was released"
party_expect 'describe()' '{"ok":true,"value":"Ann at rank 7, walked 4","type":"String"}'
sed -i 's/at rank {rank}"/ranked {rank}"/' "$party_folder/hero.spite"
party_reload "rebuilt Hero"
party_expect 'describe()' '{"ok":true,"value":"Ann ranked 7, walked 4","type":"String"}'
party_expect 'exit' '{"ok":true,"value":"","type":""}'
for attempt in $(seq 1 50); do kill -0 $party 2>/dev/null || break; sleep 0.2; done
kill -0 $party 2>/dev/null && party_fail "the program kept running after exit"
wait $party || party_fail "the program ended with exit code $?"
# A whole reload's manifest is the running program's baseline, so the body edit after the moves compiled only its
# class, and a fast reload against that baseline still matches a whole compile.
wholes=$(grep -c "compiling the whole program" "$work/live_party/output.txt")
[ "$wholes" == "3" ] || party_fail "the three attribute changes and the body edit compiled the whole program $wholes times, not 3"
sed -i 's/ranked {rank}"/ranked {rank} again"/' "$party_folder/hero.spite"
answer=$(SPITE_RELOAD_CHECK="$party_folder/hero.spite" "$work/generation_two.exe" reload "$party_folder" --hot-reload --repl-port=$party_port --executable-path="$work/live_party/live_party.exe" 2>&1 | tr -d '\r')
case "$answer" in
  "reload check: the fast reload matches a whole compile"*) ;;
  *) echo "FAILED moving objects: a fast reload against the baseline answered: $answer" | head -c 600; echo; exit 1 ;;
esac
echo "moving objects: a reload moved live objects and an Items' own memory to new attributes, keeping, renaming and releasing them, and a body edit after compiled only its class, matching a whole compile"
}

# An enum's values change live (docs/repl.md#what-a-reload-can-change): a copy of conformance/stage6/live_enum runs
# with --hot-reload, gains a value before the others and loses one an attribute still holds. Every value keeps its
# meaning, and a switch that meets the removed value halts naming it. Every wait has a timeout.
job_live_enums() {
enum_folder="$work/live_enum/live_enum"
mkdir -p "$work/live_enum"; cp -r conformance/stage6/live_enum "$enum_folder"; rm -f "$enum_folder/expected_output.txt"
enum_port=$((port + 3))
"$work/generation_two.exe" "$enum_folder" --executable --run=false --hot-reload --repl-port=$enum_port --executable-path="$work/live_enum/live_enum.exe" > "$job_errors" 2>&1 || {
  echo "FAILED live enums: live_enum does not build with --hot-reload"; head -5 "$job_errors"; exit 1; }
"$work/live_enum/live_enum.exe" > "$work/live_enum/output.txt" 2>&1 < /dev/null &
enum_program=$!
enum_fail() { kill $enum_program 2>/dev/null; echo "FAILED live enums: $1"; head -5 "$work/live_enum/output.txt"; exit 1; }
enum_ask() { timeout 120 "$work/generation_two.exe" connect $enum_port --command="$1" 2>&1 | tr -d '\r'; }
enum_expect() { local answer; answer=$(enum_ask "$1"); [ "$answer" == "$2" ] || enum_fail "'$1' answered $answer, not $2"; }
enum_reload() {   # the watcher may swap the save in first: then wait_reload answers what it swapped in
  local answer; answer=$(enum_ask reload)
  case "$answer" in *"nothing changed since the code the program runs"*) answer=$(enum_ask wait_reload) ;; esac
  [ "$answer" == '{"ok":true,"value":"rebuilt LiveEnum","type":""}' ] || enum_fail "a reload answered $answer"
}
listening=false
for attempt in $(seq 1 100); do
  timeout 10 "$work/generation_two.exe" connect $enum_port --command=help > /dev/null 2>&1 && { listening=true; break; }
  kill -0 $enum_program 2>/dev/null || break
  sleep 0.2
done
$listening || enum_fail "the program never listened on $enum_port"
sed -i "s/^    'calm'$/    'sleepy'\n    'calm'/; s/'calm': return \"fine\"/'calm': return \"fine\"\n        'sleepy': return \"resting\"/" "$enum_folder/live_enum.spite"
enum_reload
enum_expect "other = 'sleepy'" '{"ok":true,"value":"sleepy","type":"Mood"}'
enum_expect 'describe()' '{"ok":true,"value":"angry and sleepy","type":"String"}'
enum_expect 'verdict()' '{"ok":true,"value":"careful","type":"String"}'
sed -i "/^    'angry'$/d; /'angry': return/d; s/^var mood: Mood = 'angry'$/var mood: Mood = 'calm'/" "$enum_folder/live_enum.spite"
enum_reload
enum_expect 'describe()' '{"ok":true,"value":"angry and sleepy","type":"String"}'
printf 'var name = "goat"\n\nfunc bleat(): String {\n    return "{name} bleats"\n}\n' > "$enum_folder/goat.spite"
case "$(enum_ask reload)" in *'"ok":true'*) enum_ask wait_reload > /dev/null ;; *) enum_fail "a reload of a new class failed" ;; esac
enum_expect 'classes' '{"ok":true,"value":"Build()\nLiveEnum\nGoat","type":""}'
enum_expect 'describe Goat' '{"ok":true,"value":"Goat\nattributes:\nname: String\nfunctions:\nbleat(): String","type":""}'
enum_ask 'verdict()' > /dev/null
for attempt in $(seq 1 50); do kill -0 $enum_program 2>/dev/null || break; sleep 0.2; done
kill -0 $enum_program 2>/dev/null && enum_fail "a switch that met a removed value kept running"
wait $enum_program && enum_fail "a switch that met a removed value ended with exit code 0"
grep -q "a switch over LiveEnum.Mood met the value 'angry', which a reload removed from it" "$work/live_enum/output.txt" || enum_fail "the halt did not name the removed value"
echo "live enums: an enum gained and lost values while the program ran, every held value kept its meaning, a new class answered at the prompt, and a switch that met the removed value halted naming it"
}

# Settings and the standard library's classes change live (docs/repl.md#what-a-reload-can-change): a copy of
# conformance/stage6/live_settings runs with --hot-reload and a setting on its command line that it does not declare
# yet. A function the program adds to Integer changes, environment.spite gains that setting and a new default, and
# build.spite a new default; each is swapped in, and the new setting is read from the command line. Every wait is a
# wait_reload with a timeout.
job_live_settings() {
settings_folder="$work/live_settings/live_settings"
mkdir -p "$work/live_settings"; cp -r conformance/stage6/live_settings "$settings_folder"; rm -f "$settings_folder/expected_output.txt"
settings_port=$((port + 5))
"$work/generation_two.exe" "$settings_folder" --executable --run=false --hot-reload --repl-port=$settings_port --executable-path="$work/live_settings/live_settings.exe" > "$job_errors" 2>&1 || {
  echo "FAILED live settings: live_settings does not build with --hot-reload"; head -5 "$job_errors"; exit 1; }
"$work/live_settings/live_settings.exe" --volume=7 > "$work/live_settings/output.txt" 2>&1 < /dev/null &
settings_program=$!
settings_fail() { kill $settings_program 2>/dev/null; echo "FAILED live settings: $1"; head -5 "$work/live_settings/output.txt"; exit 1; }
settings_ask() { timeout 120 "$work/generation_two.exe" connect $settings_port --command="$1" 2>&1 | tr -d '\r'; }
settings_expect() { local answer; answer=$(settings_ask "$1"); [ "$answer" == "$2" ] || settings_fail "'$1' answered $answer, not $2"; }
settings_reload() {   # the watcher swaps each save in; wait_reload answers once it has
  local answer; answer=$(settings_ask wait_reload)
  case "$answer" in *'"ok":true'*"$1"*) ;; *) settings_fail "a reload answered $answer, without $1" ;; esac
}
listening=false
for attempt in $(seq 1 100); do
  timeout 10 "$work/generation_two.exe" connect $settings_port --command=help > /dev/null 2>&1 && { listening=true; break; }
  kill -0 $settings_program 2>/dev/null || break
  sleep 0.2
done
$listening || settings_fail "the program never listened on $settings_port"
settings_expect 'greeting()' '{"ok":true,"value":"hello","type":"String"}'
settings_expect 'twice()' '{"ok":true,"value":"14","type":"Integer"}'
sed -i 's/this \* 2/this * 3/' "$settings_folder/integer.spite"
settings_reload "rebuilt Integer"
settings_expect 'twice()' '{"ok":true,"value":"21","type":"Integer"}'
printf 'var greeting_word = "welcome"\nvar volume = 1\n' > "$settings_folder/environment.spite"
settings_reload "volume is new and is read like every setting"
settings_expect 'greeting()' '{"ok":true,"value":"welcome","type":"String"}'
settings_expect 'environment.volume' '{"ok":true,"value":"7","type":"Integer"}'
sed -i 's/false/true/' "$settings_folder/build.spite"
settings_reload "rebuilt Build"
settings_expect 'greeting()' '{"ok":true,"value":"welcome!","type":"String"}'
settings_expect 'build.loud' '{"ok":true,"value":"true","type":"Boolean"}'
settings_expect 'exit' '{"ok":true,"value":"","type":""}'
for attempt in $(seq 1 50); do kill -0 $settings_program 2>/dev/null || break; sleep 0.2; done
kill -0 $settings_program 2>/dev/null && settings_fail "the program kept running after exit"
wait $settings_program || settings_fail "the program ended with exit code $?"
echo "live settings: a function added to Integer, a new setting read from the command line, a setting's default and a Build field's default were swapped in while the program ran"
}

# Breakpoints (docs/repl.md#breakpoints): a program that ticks in a loop runs with --hot-reload and --repl-port; a
# breakpoint is compiled into it, the loop stops there with its locals readable, goes on, and the breakpoint is
# cleared again. A prompt call that meets a breakpoint answers that it stopped. Every wait has a timeout.
job_breakpoints() {
break_folder="$work/live_break/ticker"
mkdir -p "$break_folder"
cat > "$break_folder/ticker.spite" <<'SPITE'
var console = Console()
var program = Program()
var total = 0
var stopped = false

func Ticker() {
    console.print("ticking")
    while not stopped {
        tick(1)
        program.sleep(20)
    }
}

func tick(amount: Integer) {
    var doubled = amount * 2
    total = total + doubled
}
SPITE
break_port=$((port + 4))
"$work/generation_two.exe" "$break_folder" --executable --run=false --hot-reload --repl-port=$break_port --executable-path="$work/live_break/ticker.exe" > "$job_errors" 2>&1 || {
  echo "FAILED breakpoints: the ticker does not build with --hot-reload"; head -5 "$job_errors"; exit 1; }
"$work/live_break/ticker.exe" > "$work/live_break/output.txt" 2>&1 < /dev/null &
ticker=$!
break_fail() { kill $ticker 2>/dev/null; echo "FAILED breakpoints: $1"; head -5 "$work/live_break/output.txt"; exit 1; }
break_ask() { timeout 120 "$work/generation_two.exe" connect $break_port --command="$1" 2>&1 | tr -d '\r'; }
break_expect() { local answer; answer=$(break_ask "$1"); [ "$answer" == "$2" ] || break_fail "'$1' answered $answer, not $2"; }
listening=false
for attempt in $(seq 1 100); do
  timeout 10 "$work/generation_two.exe" connect $break_port --command=help > /dev/null 2>&1 && { listening=true; break; }
  kill -0 $ticker 2>/dev/null || break
  sleep 0.2
done
$listening || break_fail "the program never listened on $break_port"
break_expect 'break ticker.spite:3' '{"ok":false,"error":"the program keeps the code it runs: error: no statement starts on '"'"'ticker.spite:3'"'"': a breakpoint stops before a statement, so name the line a statement starts on"}'
break_expect 'break ticker.spite:16' '{"ok":true,"value":"the program stops before ticker.spite:16: rebuilt Ticker","type":""}'
where=""
for attempt in $(seq 1 100); do   # the loop meets the breakpoint at its next tick
  where=$(break_ask where)
  case "$where" in *'ticker.spite:16"'*) break ;; esac
  sleep 0.1
done
case "$where" in *'ticker.spite:16"'*) ;; *) break_fail "the loop never stopped at the breakpoint: $where" ;; esac
break_expect 'doubled' '{"ok":true,"value":"2","type":"Integer"}'
break_expect 'amount' '{"ok":true,"value":"1","type":"Integer"}'
held=$(break_ask 'locals')
case "$held" in '{"ok":true,"value":"self: Ticker = Ticker {...}\namount: Integer = 1\ndoubled: Integer = 2","type":""}') ;; *) break_fail "locals answered $held" ;; esac
before=$(break_ask total)
break_expect 'clear' '{"ok":true,"value":"cleared: rebuilt Ticker","type":""}'
break_expect 'breaks' '{"ok":true,"value":"no breakpoint is set: '"'"'break hero.spite:12'"'"' sets one","type":""}'
case "$(break_ask continue)" in *'"ok":true,"value":"continued from '*) ;; *) break_fail "continue did not go on" ;; esac
sleep 0.2
after=$(break_ask total)
[ "$before" != "$after" ] || break_fail "the loop did not go on after continue: total stayed $after"
break_expect 'continue' '{"ok":false,"error":"the program is not stopped at a breakpoint: '"'"'break hero.spite:12'"'"' sets one"}'
# Code typed at the prompt is compiled into the running program and run once (docs/repl.md#code-typed-at-the-prompt).
break_expect 'eval 20 + 22' '{"ok":true,"value":"42","type":"String"}'
case "$(break_ask 'eval nope + 1')" in *"unknown identifier 'nope'"*) ;; *) break_fail "eval of an unknown name did not fail naming it" ;; esac
break_expect 'run stopped = true' '{"ok":true,"value":"","type":"Nothing"}'
break_expect 'stopped' '{"ok":true,"value":"true","type":"Boolean"}'
break_expect 'exit' '{"ok":true,"value":"","type":""}'
for attempt in $(seq 1 50); do kill -0 $ticker 2>/dev/null || break; sleep 0.2; done
kill -0 $ticker 2>/dev/null && break_fail "the program kept running after exit"
wait $ticker || break_fail "the program ended with exit code $?"
echo "breakpoints: a breakpoint compiled into a running loop stopped it with its locals readable, it went on once cleared, and code typed at the prompt ran in the program"
}

# The compiler is the formatter: every file outside diagnostics/ (whose expected errors carry line numbers) is
# already in the one style, so formatting it changes nothing. `spite format --check` lists every file that would
# change and fails; a docs/ program that must fail may be wrong on purpose, formatting included.
job_formatting() {
formatted_folders=(bootstrap launcher library tests conformance examples scripts benchmarks)
for folder in .spite/docs/*/; do
  [ -f "$folder/must_fail.txt" ] || formatted_folders+=("$folder")
done
unformatted=$("$work/generation_two.exe" format --check "${formatted_folders[@]}" 2>&1 | tr -d '\r')
if [ -n "$unformatted" ]; then echo "FAILED: not formatted (run: bin/spite format <path>):"; echo "$unformatted"; exit 1; fi
echo "formatting: every file is in the one style"
}

# These checks need only generation 2 and the corpus's executables, and they are a second pool, run once the first
# has ended: several of them start a program and talk to it within a timeout, which a machine busy with the first
# pool could miss. Each takes its own ports, from this run's process id.
port=$((20000 + $$ % 20000))
export port
{
  echo places
  echo git_load
  echo units
  echo threads
  echo production_c
  echo wire
  echo hot_reload
  echo moving_objects
  echo live_enums
  echo live_settings
  echo breakpoints
  echo formatting
} | awk '{ print NR, $0 }' > "$work/jobs.txt"
run_pool
report_pool
[ "$failures" == "0" ] || { echo "FAILED: $failures checks failed"; exit 1; }

if cmp -s "$work/generation_two.c" bootstrap/seed/spite_compiler.c; then
  echo "OK: fixpoint holds and the committed seed is current"
elif [ "$1" == "--update-seed" ]; then
  cp "$work/generation_two.c" bootstrap/seed/spite_compiler.c; echo "OK: fixpoint holds; bootstrap/seed/spite_compiler.c updated"
else
  echo "OK: fixpoint holds, but the compiler sources changed since the seed was written: run  bash check.sh --update-seed"
fi
