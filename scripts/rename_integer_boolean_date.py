"""Renames the abbreviated and "Local" type names everywhere (D122, D160).

    Int -> Integer, UnsignedInt -> UnsignedInteger, Bool -> Boolean, and every name built from them
    (to_int -> to_integer, read_unsigned_int -> read_unsigned_integer, List_Int -> List_Integer, int.spite ->
    integer.spite); LocalDate/LocalTime/LocalDateTime -> Date/Time/DateTime, read_local_date -> read_date.

Run from the repository root:   python scripts/rename_integer_boolean_date.py [path ...]
With no path it rewrites the whole repository and renames the files. It is idempotent, so a branch that still
says Int can be migrated by running it again after merging. Read the diff afterwards: it rewrites words, not
meaning. C's own `int` and `bool`, and `Interface`, `print`, `IntegerLiteral`, are left alone.

Never touched: design/decisions.md (the log is history, and so is a leftover manual.md), Mortaro's own files, the
seed, the progress log of bootstrap/COMPILER_PLAN.md, the compiler's table of old spellings with the diagnostic
that proves it, and a line that names the old spelling on purpose (KEPT_LINE).
"""
import os
import re
import subprocess
import sys

SKIPPED_FOLDERS = {".git", ".spite", ".claude", "seed", "node_modules"}
SKIPPED_PATHS = {
    "manual.md",
    "design/decisions.md",
    "scripts/rename_integer_boolean_date.py",
    "bootstrap/source/generation/old_spellings.spite",
    "diagnostics/old_type_spellings",
}
TEXT_ENDINGS = (".spite", ".md", ".txt", ".sh", ".py", ".c", ".h", ".json", ".cmd", "")
# Files whose history below a heading is kept as it was written.
HISTORY_HEADINGS = {"bootstrap/COMPILER_PLAN.md": "## Progress log"}

WHOLE_NAMES = {
    "LocalDateTime": "DateTime",
    "LocalDate": "Date",
    "LocalTime": "Time",
    "read_local_date_time": "read_date_time",
    "read_local_date": "read_date",
    "read_local_time": "read_time",
    "_Bool": "_Bool",  # C's own
}
FILE_NAMES = {
    "local_date_time.spite": "date_time.spite",
    "local_date.spite": "date.spite",
    "local_time.spite": "time.spite",
    "unsigned_int.spite": "unsigned_integer.spite",
    "int.spite": "integer.spite",
    "bool.spite": "boolean.spite",
    "bool_argument.spite": "boolean_argument.spite",
}
SNAKE_WORDS = {"int": "integer", "bool": "boolean"}
CAMEL = re.compile(r"(?:^|(?<=[a-z0-9]))(Int|Bool)(?![a-z])")
CAMEL_LOCAL = re.compile(r"(?:^|(?<=[a-z0-9]))Local(DateTime|Date|Time)(?![a-z])")
IDENTIFIER = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*")
FILE_NAME = re.compile(r"(?<![A-Za-z0-9_])(" + "|".join(re.escape(name) for name in FILE_NAMES) + r")(?![A-Za-z0-9_])")
# The compiler's stems for the number classes' library files (prelude.spite): "int" is a file, not C's int.
STEM = re.compile(r'"(int|bool|unsigned_int)"')
STEMS = {"int": "integer", "bool": "boolean", "unsigned_int": "unsigned_integer"}
# A line that names the old spelling on purpose: the decisions themselves, the error, another language's names.
KEPT_LINE = re.compile(r"D122|D160|is spelled|NodaTime")


def renamed_identifier(match):
    name = match.group(0)
    if name in WHOLE_NAMES:
        return WHOLE_NAMES[name]
    segments = name.split("_")
    result = []
    for segment in segments:
        if len(segments) > 1 and segment in SNAKE_WORDS:
            result.append(SNAKE_WORDS[segment])
            continue
        segment = CAMEL_LOCAL.sub(lambda found: found.group(1), segment)
        segment = CAMEL.sub(lambda found: {"Int": "Integer", "Bool": "Boolean"}[found.group(1)], segment)
        result.append(segment)
    return "_".join(result)


def renamed_text(text, path):
    history = ""
    heading = HISTORY_HEADINGS.get(path)
    if heading and heading in text:
        cut = text.index(heading)
        text, history = text[:cut], text[cut:]
    lines = text.split("\n")
    for index, line in enumerate(lines):
        if KEPT_LINE.search(line):
            continue
        line = FILE_NAME.sub(lambda found: FILE_NAMES[found.group(1)], line)
        line = IDENTIFIER.sub(renamed_identifier, line)
        if path.endswith("generation/prelude.spite"):
            line = STEM.sub(lambda found: '"%s"' % STEMS[found.group(1)], line)
        lines[index] = line
    return "\n".join(lines) + history


def skipped(path):
    if os.path.basename(path).startswith("mortaros_"):
        return True
    return any(path == skip or path.startswith(skip + "/") for skip in SKIPPED_PATHS)


def files_under(paths):
    for start in paths:
        if os.path.isfile(start):
            yield start
            continue
        for folder, folders, names in os.walk(start):
            folders[:] = sorted(name for name in folders if name not in SKIPPED_FOLDERS)
            for name in sorted(names):
                yield os.path.join(folder, name)


def main():
    paths = sys.argv[1:] or ["."]
    changed = 0
    moves = []
    for full in files_under(paths):
        path = os.path.relpath(full).replace(os.sep, "/")
        if skipped(path) or os.path.splitext(path)[1] not in TEXT_ENDINGS:
            continue
        try:
            with open(full, encoding="utf-8", newline="") as file:
                text = file.read()
        except (UnicodeDecodeError, OSError):
            continue
        rewritten = renamed_text(text, path)
        if rewritten != text:
            with open(full, "w", encoding="utf-8", newline="") as file:
                file.write(rewritten)
            changed += 1
        base = os.path.basename(path)
        if base in FILE_NAMES:
            moves.append((full, os.path.join(os.path.dirname(full), FILE_NAMES[base])))
    for old, new in moves:
        if subprocess.call(["git", "mv", old, new]) != 0:
            os.replace(old, new)
    print("rewrote %d files, renamed %d" % (changed, len(moves)))


if __name__ == "__main__":
    main()
