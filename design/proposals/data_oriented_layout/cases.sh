#!/bin/bash
# Times the five data-oriented cases of benchmarks/ (design/proposals/data_oriented_layout.md) at five sizes, from a
# few kilobytes of records to far past the last level of cache: every form (naive/ built --optimized, naive.c,
# expert_aos.c, expert_soa.c and expert.c built with clang -O2), the best of several interleaved runs at each size,
# every answer compared with naive.c's. Writes cases.md beside this file: one table per case of best microseconds,
# then the phases each C form reported in its best run. Not part of check.sh: it takes several minutes.
#   bash design/proposals/data_oriented_layout/cases.sh [case ...]
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
work=.spite/cases_sized
mkdir -p "$work"
output=design/proposals/data_oriented_layout/cases.md
sizes="512 8192 131072 2097152 8388608"
flag_of() {
    case "$1" in
        report_over_records) echo "--records" ;;
        tokens_as_columns) echo "--pieces" ;;
        image_filter_over_planes) echo "--pixels" ;;
        spreadsheet_recalculation) echo "--cells" ;;
        records_sorted_by_one_field) echo "--orders" ;;
    esac
}
names=("$@")
[ ${#names[@]} -eq 0 ] && names=(report_over_records tokens_as_columns image_filter_over_planes spreadsheet_recalculation records_sorted_by_one_field)
forms="spite naive expert_aos expert_soa expert"
{
    echo "# Data-oriented cases at five sizes"
    echo ""
    echo "Written by \`bash design/proposals/data_oriented_layout/cases.sh\` on $(date +%Y-%m-%d). Best microseconds of the"
    echo "interleaved runs (seven below two million records, three above), each form's whole program as it times itself."
    echo ""
} > "$output"
for name in "${names[@]}"; do
    folder=benchmarks/$name
    flag=$(flag_of "$name")
    "$compiler" "$folder/naive" --build --optimized --executable-path="$work/${name}_spite.exe" > "$work/$name.log" 2>&1 || {
        echo "FAILED: $name/naive does not build"; head -5 "$work/$name.log"; continue; }
    for form in naive expert_aos expert_soa expert; do
        "$CC_BIN" -O2 -w "$folder/$form.c" -o "$work/${name}_$form.exe" $maths_library || { echo "FAILED: $name/$form.c"; continue 2; }
    done
    declare -A best=()
    declare -A phases=()
    for size in $sizes; do
        attempts=7
        [ "$size" -gt 2000000 ] && attempts=3
        for attempt in $(seq 1 $attempts); do
            expected=""
            for form in naive spite expert_aos expert_soa expert; do
                "$work/${name}_$form.exe" "$flag=$size" > "$work/answer.txt" 2> "$work/time.txt"
                answer=$(tr -d '\r' < "$work/answer.txt")
                if [ "$form" == "naive" ]; then expected=$answer
                elif [ "$answer" != "$expected" ]; then echo "FAILED: $name $form at $size answers '$answer', naive.c '$expected'"; exit 1; fi
                took=$(tr -d '\r' < "$work/time.txt" | sed -n 's/^microseconds //p')
                key="$form:$size"
                if [ -z "${best[$key]}" ] || [ "$took" -lt "${best[$key]}" ]; then
                    best[$key]=$took
                    phases[$key]=$(tr -d '\r' < "$work/time.txt" | sed -n 's/^phases //p')
                fi
            done
        done
        echo "$name $size done" >&2
    done
    {
        echo "## $name"
        echo ""
        printf "| form |"; for size in $sizes; do printf " %s |" "$size"; done; echo ""
        printf "|---|"; for size in $sizes; do printf "%s" "---|"; done; echo ""
        for form in $forms; do
            printf "| %s |" "$form"
            for size in $sizes; do printf " %s |" "${best[$form:$size]}"; done
            echo ""
        done
        echo ""
        echo "Phases of each C form's best run, in microseconds:"
        echo ""
        printf "| form |"; for size in $sizes; do printf " %s |" "$size"; done; echo ""
        printf "|---|"; for size in $sizes; do printf "%s" "---|"; done; echo ""
        for form in naive expert_aos expert_soa expert; do
            printf "| %s |" "$form"
            for size in $sizes; do printf " %s |" "${phases[$form:$size]}"; done
            echo ""
        done
        echo ""
    } >> "$output"
    unset best phases
done
