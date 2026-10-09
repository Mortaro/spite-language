#!/bin/bash
# Times the four archetype cases of benchmarks/ (design/proposals/compiler_archetypes.md) in every form (naive/ built
# --optimized, naive.c and every expert_*.c built with clang -O2) across sizes (from a list that fits the first
# levels of cache to one far past the last), part densities (1%, 50% and 99% of the items having a part) and churn
# (parts added or removed per pass), keeping the best of several interleaved runs of each and comparing every
# answer with naive.c's. Writes results.md beside this file: per case, a table of whole programs and a table of the
# passes alone (the time each C form reports after making its list), a row per setting, a column per form. Not part
# of check.sh: it takes about an hour.
#   bash design/proposals/compiler_archetypes/sweep.sh [case ...]
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
case "$CC" in *\ *) command -v cygpath >/dev/null 2>&1 && CC=$(cygpath -d "$CC" 2>/dev/null || echo "$CC") ;; esac
export CC
maths_library="-lm"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) maths_library="" ;; esac
work=.spite/archetypes_sweep
mkdir -p "$work"
output=design/proposals/compiler_archetypes/results.md

# the settings each case is timed at, one run's arguments per line
settings_of() {
    case "$1" in
        shapes_with_optional_parts)
            for items in 4096 65536 1048576 4194304; do
                for density in 1 50 99; do echo "--items=$items --density=$density --passes=20"; done
            done ;;
        inventory_with_fields_set_and_cleared|entities_with_components_added_and_removed)
            for items in 4096 1048576 4194304; do
                for density in 1 99; do echo "--items=$items --density=$density --passes=20 --churn=$((items / 100))"; done
                churns="0 $((items / 1000)) $((items / 100)) $((items / 10))"
                [ "$items" -le 1048576 ] && churns="$churns $items"
                for churn in $churns; do echo "--items=$items --density=50 --passes=20 --churn=$churn"; done
            done ;;
        events_with_different_payloads)
            for shape in "1000 4000" "100000 40" "4000000 1"; do
                set -- $shape
                for density in 1 50 99; do echo "--events=$1 --rounds=$2 --density=$density"; done
            done ;;
    esac
}

names=("$@")
[ ${#names[@]} -eq 0 ] && names=(shapes_with_optional_parts inventory_with_fields_set_and_cleared events_with_different_payloads entities_with_components_added_and_removed)
{
    echo "# Archetype cases across sizes, densities and churn"
    echo ""
    echo "Written by \`bash design/proposals/compiler_archetypes/sweep.sh\` on $(date +%Y-%m-%d). Best microseconds of the"
    echo "interleaved runs (five when the list has at most 65 536 items, three above), each form's whole program as it"
    echo "times itself, building the list included. Every answer was compared with naive.c's. \`spite\` is \`naive/\` built"
    echo "\`--optimized\`; the C forms are built with \`clang -O2\`. The last column names the fastest hand form."
    echo "Under each table, the passes alone: what each C form reports for its passes (or, for the events, for handling"
    echo "them) in the same best run, the list already made."
    echo ""
} > "$output"
for name in "${names[@]}"; do
    folder=benchmarks/$name
    "$compiler" "$folder/naive" --build --optimized --executable-path="$work/${name}_spite.exe" > "$work/$name.log" 2>&1 || {
        echo "FAILED: $name/naive does not build"; head -5 "$work/$name.log"; continue; }
    forms="spite naive"
    for source in "$folder"/expert_*.c; do
        form=$(basename "$source" .c)
        "$CC_BIN" -O2 -w "$source" -o "$work/${name}_$form.exe" $maths_library || { echo "FAILED: $source"; continue 2; }
        forms="$forms $form"
    done
    "$CC_BIN" -O2 -w "$folder/naive.c" -o "$work/${name}_naive.exe" $maths_library || { echo "FAILED: $name/naive.c"; continue; }
    c_forms=${forms#spite }
    {
        echo "## $name"
        echo ""
        printf "| settings |"; for form in $forms; do printf " %s |" "$form"; done; echo " fastest hand form |"
        printf "|---|"; for form in $forms; do printf "%s" "---|"; done; echo "---|"
    } >> "$output"
    {
        echo "The passes alone, microseconds:"
        echo ""
        printf "| settings |"; for form in $c_forms; do printf " %s |" "$form"; done; echo " fastest hand form |"
        printf "|---|"; for form in $c_forms; do printf "%s" "---|"; done; echo "---|"
    } > "$work/passes.md"
    while read -r settings; do
        [ -z "$settings" ] && continue
        items=$(echo "$settings" | sed -E 's/.*--(items|events)=([0-9]+).*/\2/')
        attempts=3
        [ "$items" -le 65536 ] && attempts=5
        declare -A best=()
        declare -A passes=()
        for attempt in $(seq 1 $attempts); do
            expected=""
            for form in $forms; do
                "$work/${name}_$form.exe" $settings > "$work/answer.txt" 2> "$work/time.txt"
                answer=$(tr -d '\r' < "$work/answer.txt")
                if [ "$form" == "spite" ]; then spite_answer=$answer
                elif [ "$form" == "naive" ]; then expected=$answer
                elif [ "$answer" != "$expected" ]; then echo "FAILED: $name $form ($settings) answers '$answer', naive.c '$expected'"; exit 1; fi
                took=$(tr -d '\r' < "$work/time.txt" | sed -n 's/^microseconds //p')
                if [ -z "${best[$form]}" ] || [ "$took" -lt "${best[$form]}" ]; then
                    best[$form]=$took
                    passes[$form]=$(tr -d '\r' < "$work/time.txt" | sed -n 's/^phases .* \([0-9]*\)$/\1/p')
                fi
            done
            [ "$spite_answer" != "$expected" ] && { echo "FAILED: $name spite ($settings) answers '$spite_answer', naive.c '$expected'"; exit 1; }
        done
        fastest=""
        for form in $forms; do
            case "$form" in expert_*) ;; *) continue ;; esac
            if [ -z "$fastest" ] || [ "${best[$form]}" -lt "${best[$fastest]}" ]; then fastest=$form; fi
        done
        quickest=""
        for form in $c_forms; do
            case "$form" in expert_*) ;; *) continue ;; esac
            if [ -z "$quickest" ] || [ "${passes[$form]}" -lt "${passes[$quickest]}" ]; then quickest=$form; fi
        done
        {
            printf "| %s |" "$(echo "$settings" | sed 's/--//g')"
            for form in $forms; do printf " %s |" "${best[$form]}"; done
            echo " ${fastest#expert_} |"
        } >> "$output"
        {
            printf "| %s |" "$(echo "$settings" | sed 's/--//g')"
            for form in $c_forms; do printf " %s |" "${passes[$form]}"; done
            echo " ${quickest#expert_} |"
        } >> "$work/passes.md"
        echo "$name $settings done" >&2
        unset best passes
    done < <(settings_of "$name")
    { echo ""; cat "$work/passes.md"; echo ""; } >> "$output"
done
