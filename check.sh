#!/bin/bash
# Proves the Spite compiler still compiles itself and still passes the conformance corpus.
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
echo "4/4 conformance corpus with generation 2"
passed=0; failed=0
for folder in conformance/*/*/; do
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")   # compiler flags such as --environment=server
  actual=$("$work/generation_two.exe" --file="$folder$name.spite" --mode=run --debug_memory=true $flags 2>&1 | tr -d '\r')
  # some toolchains intermittently fail to open their own cache files on Windows; that is not a
  if echo "$actual" | grep -q "failed to check cache"; then
    actual=$("$work/generation_two.exe" --file="$folder$name.spite" --mode=run --debug_memory=true $flags 2>&1 | tr -d '\r')
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
  if [ "$body" == "$expected" ] && [ -n "$balance" ] && [ "$allocations" == "$frees" ]; then passed=$((passed+1)); else failed=$((failed+1)); echo "FAILED: $name"; echo "$actual" | head -8; fi
done
echo "conformance: $passed passed, $failed failed"
[ "$failed" == "0" ] || exit 1

# Programs that must NOT compile: the errors are the language's main channel to whoever (or whatever) writes the code.
wrong=0; checked=0
for folder in diagnostics/*/; do
  name=$(basename "$folder")
  flags=""; [ -f "$folder/flags.txt" ] && flags=$(tr -d '\r\n' < "$folder/flags.txt")
  actual=$("$work/generation_two.exe" --file="$folder$name.spite" --mode=c $flags 2>&1 >/dev/null | tr -d '\r')
  expected=$(tr -d '\r' < "$folder/expected_errors.txt")
  checked=$((checked+1))
  if [ "$actual" != "$expected" ]; then wrong=$((wrong+1)); echo "FAILED diagnostics: $name"; echo "$actual" | head -8; fi
done
echo "diagnostics: $checked checked, $wrong wrong"
[ "$wrong" == "0" ] || exit 1

if cmp -s "$work/generation_two.c" bootstrap/seed/spite_compiler.c; then
  echo "OK: fixpoint holds and the committed seed is current"
elif [ "$1" == "--update-seed" ]; then
  cp "$work/generation_two.c" bootstrap/seed/spite_compiler.c; echo "OK: fixpoint holds; bootstrap/seed/spite_compiler.c updated"
else
  echo "OK: fixpoint holds, but the compiler sources changed since the seed was written: run  bash check.sh --update-seed"
fi
