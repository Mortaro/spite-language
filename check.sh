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
compile_c() { "$CC_BIN" -O1 -Wno-parentheses-equality -Wno-deprecated-declarations "$1" -o "$2" 2> "$work/c_errors.txt" || { head -20 "$work/c_errors.txt"; exit 1; }; }

echo "1/4 building the seed compiler"
compile_c bootstrap/seed/spite_compiler.c "$work/seed.exe"

echo "2/4 the seed compiles the compiler sources (generation 2)"
"$work/seed.exe" --file=bootstrap/spite_compiler.spite --mode=c > "$work/generation_two.c" || { echo "FAILED: the seed could not compile the compiler sources"; exit 1; }
compile_c "$work/generation_two.c" "$work/generation_two.exe"

echo "3/4 generation 2 compiles the compiler sources again (generation 3): must be byte identical"
"$work/generation_two.exe" --file=bootstrap/spite_compiler.spite --mode=c > "$work/generation_three.c" || exit 1
if ! cmp -s "$work/generation_two.c" "$work/generation_three.c"; then
  # A change to how the compiler compiles ITS OWN source needs one more generation to settle.
  echo "    generation 2 and 3 differ: trying one more generation"
  compile_c "$work/generation_three.c" "$work/generation_three.exe"
  "$work/generation_three.exe" --file=bootstrap/spite_compiler.spite --mode=c > "$work/generation_four.c" || exit 1
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
  actual=$("$work/generation_two.exe" --file="$folder$name.spite" --mode=run --debug_memory=true $flags < "$input" 2>&1 | tr -d '\r')
  # some toolchains intermittently fail to open their own cache files on Windows; that is not a
  if echo "$actual" | grep -q "failed to check cache"; then
    actual=$("$work/generation_two.exe" --file="$folder$name.spite" --mode=run --debug_memory=true $flags < "$input" 2>&1 | tr -d '\r')
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

# The tests: a package that crashes (D46). No framework: a test is a function, and `crash` is the assertion.
test_output=$("$work/generation_two.exe" --file=tests/tests.spite --mode=run --debug_memory=true < /dev/null 2>&1 | tr -d '\r')
if echo "$test_output" | grep -q "failed to check cache"; then
  test_output=$("$work/generation_two.exe" --file=tests/tests.spite --mode=run --debug_memory=true < /dev/null 2>&1 | tr -d '\r')
fi
test_balance=$(echo "$test_output" | grep '^allocations: ' | sed -E 's/allocations: ([0-9]+) frees: ([0-9]+)/\1 \2/')
if [ "$(echo "$test_output" | grep -vc '^allocations: ')" != "0" ] || [ -z "$test_balance" ] || [ "${test_balance% *}" != "${test_balance#* }" ]; then
  echo "FAILED: tests"; echo "$test_output" | head -8; exit 1
fi
echo "tests: passed"

# The compiler is held to the corpus standard too: compiling itself, it frees everything it takes.
"$CC_BIN" -O1 -w -DSPITE_DEBUG_MEMORY "$work/generation_two.c" -o "$work/generation_two_debug.exe" 2> "$work/c_errors.txt" || { head -20 "$work/c_errors.txt"; exit 1; }
self_leaks=$("$work/generation_two_debug.exe" --file=bootstrap/spite_compiler.spite --mode=c 2>&1 > /dev/null | head -5)
if [ -n "$self_leaks" ]; then echo "FAILED: the compiler leaks while compiling itself"; echo "$self_leaks"; exit 1; fi
echo "compiler memory: compiling itself frees everything it takes"

# Programs that must NOT compile: the errors are the language's main channel to whoever (or whatever) writes the code.
wrong=0; checked=0
for folder in diagnostics/*/; do
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")
  actual=$("$work/generation_two.exe" --file="$folder$name.spite" --mode=c --no-format $flags 2>&1 >/dev/null | tr -d '\r')
  expected=$(tr -d '\r' < "$folder/expected_errors.txt")
  checked=$((checked+1))
  if [ "$actual" != "$expected" ]; then wrong=$((wrong+1)); echo "FAILED diagnostics: $name"; echo "$actual" | head -8; fi
done
echo "diagnostics: $checked checked, $wrong wrong"
[ "$wrong" == "0" ] || exit 1

# Every program written in docs/ is a program: scripts/docs_corpus.spite (itself Spite) writes each titled
# code block out, and each one has to compile, run, print its ```output block and free everything it took.
# A block marked `error` must fail to compile with its ```diagnostic text somewhere in the message.
rm -rf .spite-cache/docs   # so a program deleted from docs/ stops being checked
"$work/generation_two.exe" --file=scripts/docs_corpus.spite --mode=run > /dev/null || {
  echo "FAILED: could not extract the documentation's programs"; exit 1; }
documented=0; undocumented=0
for folder in .spite-cache/docs/*/; do
  name=$(basename "$folder")
  entry=$(tr -d '\r\n' < "$folder/entry.txt")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")
  if [ -f "$folder/must_fail.txt" ]; then
    actual=$("$work/generation_two.exe" --file="$folder$entry" --mode=c --no-format $flags 2>&1 >/dev/null | tr -d '\r')
    expected=$(tr -d '\r' < "$folder/expected_diagnostic.txt")
    if [ -n "$actual" ] && [ "${actual#*$expected}" != "$actual" ]; then documented=$((documented+1))
    else undocumented=$((undocumented+1)); echo "FAILED docs: $name wanted an error saying '$expected'"; echo "$actual" | head -4; fi
    continue
  fi
  actual=$("$work/generation_two.exe" --file="$folder$entry" --mode=run --debug_memory=true --no-format $flags < /dev/null 2>&1 | tr -d '\r')
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
  entry=$(tr -d '\r\n' < "$folder/entry.txt")
  port=$((20000 + $$ % 20000))
  "$work/generation_two.exe" --file="$folder/$entry" --mode=build --no-format --repl-port=$port --output="$work/wire_$name.exe" > "$work/c_errors.txt" 2>&1 || {
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

# --final-classes writes the program back out as Spite source. What it writes has to be a program:
# printing a corpus program and running what came out must print the same thing. symbol_codegen proves the
# functions Spite made from a template are printed as real functions (D61).
for printed_program in conformance/stage3/interpolation conformance/stage6/symbol_codegen; do
  printed_name=$(basename "$printed_program")
  printed="$work/printed_$printed_name"
  "$work/generation_two.exe" --file="$printed_program/$printed_name.spite" --final-classes="$printed" > /dev/null 2>&1 || {
    echo "FAILED: --final-classes could not write $printed_program out"; exit 1; }
  printed_output=$("$work/generation_two.exe" --file="$printed/$printed_name.spite" --mode=run --debug_memory=true < /dev/null 2>&1 | tr -d '' | grep -v '^allocations: ')
  if [ "$printed_output" != "$(tr -d '' < "$printed_program/expected_output.txt")" ]; then
    echo "FAILED: the printed $printed_name does not run like the one it was printed from"; echo "$printed_output" | head -6; exit 1
  fi
done
echo "final classes: the printed program runs the same"

# Each operating system's folder in library/ reopens the classes it changes (D80). Only this machine's can run
# here, so the others are held to compiling: the compiler writes itself out once for each.
for operational_system in windows linux mac; do
  "$work/generation_two.exe" --file=bootstrap/spite_compiler.spite --mode=c --operational_system=$operational_system > "$work/compiler_$operational_system.c" || {
    echo "FAILED: the compiler does not compile with library/$operational_system"; exit 1; }
  "$CC_BIN" -fsyntax-only -w "$work/compiler_$operational_system.c" 2> "$work/c_errors.txt" || {
    echo "FAILED: the C written for library/$operational_system does not compile"; head -5 "$work/c_errors.txt"; exit 1; }
done
echo "operational systems: the compiler compiles with the windows, linux and mac library folders"

# The compiler is the formatter: every file outside diagnostics/ (whose expected errors carry line numbers) is
# already in the one style, so formatting it changes nothing.
unformatted=""
for folder in .spite-cache/docs/*/; do
  [ -f "$folder/must_fail.txt" ] && continue   # a program that must fail may be wrong on purpose, formatting included
  for file in "$folder"*.spite; do
    "$work/generation_two.exe" --file="$file" --mode=check_format > /dev/null 2>&1 || unformatted="$unformatted docs:$(basename "$folder")/$(basename "$file")"
  done
done
for file in $(find bootstrap library tests conformance examples scripts -name "*.spite"); do
  "$work/generation_two.exe" --file="$file" --mode=check_format > /dev/null 2>&1 || unformatted="$unformatted $file"
done
if [ -n "$unformatted" ]; then echo "FAILED: not formatted (run: bin/spite format <path>):$unformatted"; exit 1; fi
echo "formatting: every file is in the one style"

if cmp -s "$work/generation_two.c" bootstrap/seed/spite_compiler.c; then
  echo "OK: fixpoint holds and the committed seed is current"
elif [ "$1" == "--update-seed" ]; then
  cp "$work/generation_two.c" bootstrap/seed/spite_compiler.c; echo "OK: fixpoint holds; bootstrap/seed/spite_compiler.c updated"
else
  echo "OK: fixpoint holds, but the compiler sources changed since the seed was written: run  bash check.sh --update-seed"
fi
