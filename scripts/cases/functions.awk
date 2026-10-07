# Prints the definitions a case names in its functions.txt, taken from the C the compiler wrote for it:
#   awk -f scripts/cases/functions.awk benchmarks/cases/<case>/functions.txt <generated C>
# A line of functions.txt is a function's name, "struct <Name>" for a struct, or "lines <start>" for every line of the
# C that starts with <start> (a declaration, a macro), in the order the C has them. Each definition is printed in the
# order the file lists them, indented by its braces, and the compiler's own numbered names (spite_temp_9028,
# spite_site_1128, ...) are numbered again from 1 in the order they appear, so a change elsewhere in the program or
# the library that only shifts those numbers does not change the case. A name the C does not define is an error.
FNR == NR {
    sub(/\r$/, "")
    if ($0 == "" || substr($0, 1, 1) == "#") next
    wanted[++wanted_count] = $0
    next
}
{
    sub(/\r$/, "")
    if (capturing != "") {
        body[capturing] = body[capturing] "\n" $0
        depth += braces($0)
        if (depth <= 0) capturing = ""
        next
    }
    for (index_ = 1; index_ <= wanted_count; index_++) {
        name = wanted[index_]
        if (substr(name, 1, 6) == "lines " && index($0, substr(name, 7)) == 1) {
            body[name] = (name in body) ? body[name] "\n" $0 : $0
        }
    }
    if (index($0, "{") == 0) next
    for (index_ = 1; index_ <= wanted_count; index_++) {
        name = wanted[index_]
        if (name in body || substr(name, 1, 6) == "lines ") continue
        if (substr(name, 1, 7) == "struct ") {
            if ($0 != name " {") continue
        } else if (substr($0, 1, 1) !~ /[A-Za-z_]/ || !starts_definition($0, name)) {
            continue
        }
        body[name] = $0
        depth = braces($0)
        if (depth > 0) capturing = name   # a definition written on one line ends where it starts
        next
    }
}
# what a line adds to the depth of braces, leaving out those inside text and character literals
function braces(line,    total, position, character, quote) {
    total = 0
    quote = ""
    for (position = 1; position <= length(line); position++) {
        character = substr(line, position, 1)
        if (quote != "") {
            if (character == "\\") position++
            else if (character == quote) quote = ""
        } else if (character == "\"" || character == "'") {
            quote = character
        } else if (character == "{") {
            total++
        } else if (character == "}") {
            total--
        }
    }
    return total
}
function starts_definition(line, name,    at, before) {
    line = substr(line, 1, index(line, "{") - 1)
    if (index(line, ";") > 0 || index(line, "=") > 0) return 0
    at = index(line, name "(")
    while (at > 0) {
        before = at == 1 ? "" : substr(line, at - 1, 1)
        if (before !~ /[A-Za-z0-9_]/) return 1
        line = substr(line, at + length(name))
        at = index(line, name "(")
    }
    return 0
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
END {
    for (index_ = 1; index_ <= wanted_count; index_++) {
        name = wanted[index_]
        if (!(name in body)) { print "the generated C defines no " name > "/dev/stderr"; failed = 1; continue }
        if (index_ > 1) print ""
        line_count = split(body[name], lines, "\n")
        depth = 0
        for (line_index = 1; line_index <= line_count; line_index++) {
            line = lines[line_index]
            level = depth
            if (substr(line, 1, 1) == "}" && level > 0) level--
            indent = ""
            for (step = 0; step < level; step++) indent = indent "    "
            print indent renumbered(line)
            depth += braces(line)
        }
    }
    exit failed
}
