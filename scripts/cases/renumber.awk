# Prints the C the compiler wrote for a case with its own numbered names (spite_temp_9028, spite_site_1128, ...)
# numbered again from 1 in the order they appear, so the whole file a case keeps (generated.c) does not change when
# a change elsewhere only shifts those numbers, and the line ends are a newline on every system:
#   awk -f scripts/cases/renumber.awk <generated C>
{
    sub(/\r$/, "")
    print renumbered($0)
}
function renumbered(line,    result, prefix, number, key, matched) {
    result = ""
    while (match(line, /spite_[a-z_]*[a-z]_[0-9]+/)) {
        matched = substr(line, RSTART, RLENGTH)
        prefix = matched
        sub(/[0-9]+$/, "", prefix)
        number = substr(matched, length(prefix) + 1)
        key = prefix SUBSEP number
        if (!(key in renumbering)) renumbering[key] = ++numbers_of[prefix]
        result = result substr(line, 1, RSTART - 1) prefix renumbering[key]
        line = substr(line, RSTART + RLENGTH)
    }
    return result line
}
