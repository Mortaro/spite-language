#!/bin/bash
# Measures rule P of design/proposals/compiler_archetypes.md by hand: writes the C of each archetype case's naive/
# (--optimized), gives the classes of its parts (or its union's members) a pool of their own with pool_parts.py, builds
# both with clang -O2 and times them, the best of five interleaved runs, at the case's defaults and at two million
# items. Prints one line per case and size; the answers of both builds must match. Not part of check.sh.
#   bash design/proposals/compiler_archetypes/pools.sh
cd "$(dirname "$0")/../../.." || exit 1
compiler=.spite/spite_development.exe
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
maths_library="-lm"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) maths_library="" ;; esac
work=.spite/archetypes_pools
mkdir -p "$work"
# the case, the pooled class whose pool is copied, the classes given a pool, and the larger size's settings
cases=(
    "shapes_with_optional_parts Shape Texture,Velocity --items=2000000"
    "inventory_with_fields_set_and_cleared Record Supplier,Reservation --items=2000000 --churn=20000"
    "events_with_different_payloads Spite_Function Click,Key,Resize --events=4000000 --rounds=1"
    "entities_with_components_added_and_removed Entity Velocity,Health,Burning --items=2000000 --churn=20000"
)
for line in "${cases[@]}"; do
    set -- $line
    name=$1; model=$2; classes=${3//,/ }; shift 3; larger="$*"
    "$compiler" "benchmarks/$name/naive" --check --c-source --optimized --c-path="$work/$name.c" > /dev/null || {
        echo "FAILED: $name does not compile"; continue; }
    python "design/proposals/compiler_archetypes/pool_parts.py" "$work/$name.c" "$work/${name}_pooled.c" $model $classes || continue
    "$CC_BIN" -O2 -w "$work/$name.c" -o "$work/$name.exe" $maths_library || continue
    "$CC_BIN" -O2 -w "$work/${name}_pooled.c" -o "$work/${name}_pooled.exe" $maths_library || continue
    for settings in "" "$larger"; do
        today=""; pooled=""
        for attempt in 1 2 3 4 5; do
            took=$("$work/$name.exe" $settings 2>&1 > "$work/today.txt" | tr -d '\r' | sed -n 's/^microseconds //p')
            [ -z "$today" ] || [ "$took" -lt "$today" ] && today=$took
            took=$("$work/${name}_pooled.exe" $settings 2>&1 > "$work/pooled.txt" | tr -d '\r' | sed -n 's/^microseconds //p')
            [ -z "$pooled" ] || [ "$took" -lt "$pooled" ] && pooled=$took
            cmp -s "$work/today.txt" "$work/pooled.txt" || { echo "FAILED: $name answers differently"; exit 1; }
        done
        echo "$name ${settings:-defaults}: today $today, parts pooled $pooled, $(awk -v a="$today" -v b="$pooled" 'BEGIN { printf "%.2f", a / b }') times"
    done
done
