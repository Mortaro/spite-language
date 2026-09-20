"""Extracts every Spite program embedded in docs/ into a folder check.sh can run.

A documentation program is one or more fenced blocks that name their file:

    ```spite title=<folder>/<file>.spite [entry] [error] [vars=name:value,name:value]
    ```output      the exact stdout the program must print
    ```diagnostic  text that must appear in the compile errors (with `error`)

Each <folder> becomes one folder under the output root, holding its sources plus `entry.txt`,
`expected_output.txt` or `expected_diagnostic.txt`, and `flags.txt` when the block declares vars.
"""
import io
import os
import re
import shutil
import sys

header = re.compile(r"^```spite\s+title=([^/\s]+)/(\S+\.spite)(.*)$")

root = sys.argv[1] if len(sys.argv) > 1 else ".spite-cache/docs"
shutil.rmtree(root, ignore_errors=True)

programs = {}
order = []
for name in sorted(os.listdir("docs")):
    if not name.endswith(".md"):
        continue
    lines = io.open(os.path.join("docs", name), encoding="utf-8").read().split("\n")
    index = 0
    while index < len(lines):
        match = header.match(lines[index])
        if not match:
            index = index + 1
            continue
        folder, file_name, rest = match.group(1), match.group(2), match.group(3)
        body = []
        index = index + 1
        while index < len(lines) and lines[index].rstrip() != "```":
            body.append(lines[index])
            index = index + 1
        index = index + 1
        program = programs.get(folder)
        if program is None:
            program = {"files": [], "entry": "", "flags": "", "expect": None, "kind": "run", "source": name}
            programs[folder] = program
            order.append(folder)
        program["files"].append((file_name, "\n".join(body) + "\n"))
        if "entry" in rest.split():
            program["entry"] = file_name
        if "error" in rest.split():
            program["kind"] = "error"
        for word in rest.split():
            if word.startswith("vars="):
                program["flags"] = " ".join(
                    "--" + pair.replace(":", "=", 1) for pair in word[len("vars="):].split(","))
        # the expectation block, when there is one, follows immediately
        while index < len(lines) and lines[index].strip() == "":
            index = index + 1
        if index < len(lines) and lines[index].rstrip() in ("```output", "```diagnostic"):
            kind = lines[index].rstrip()[3:]
            index = index + 1
            expected = []
            while index < len(lines) and lines[index].rstrip() != "```":
                expected.append(lines[index])
                index = index + 1
            index = index + 1
            program["expect"] = (kind, "\n".join(expected).rstrip("\n") + "\n")

written = 0
for folder in order:
    program = programs[folder]
    if not program["entry"]:
        sys.stderr.write("docs: %s has no block marked 'entry' (in %s)\n" % (folder, program["source"]))
        sys.exit(1)
    target = os.path.join(root, folder)
    os.makedirs(target, exist_ok=True)
    for file_name, text in program["files"]:
        destination = os.path.join(target, file_name)
        os.makedirs(os.path.dirname(destination), exist_ok=True)
        io.open(destination, "w", encoding="utf-8", newline="\n").write(text)
    io.open(os.path.join(target, "entry.txt"), "w", encoding="utf-8", newline="\n").write(program["entry"] + "\n")
    if program["flags"]:
        io.open(os.path.join(target, "flags.txt"), "w", encoding="utf-8", newline="\n").write(program["flags"] + "\n")
    if program["kind"] == "error":
        io.open(os.path.join(target, "must_fail.txt"), "w", encoding="utf-8", newline="\n").write("\n")
    if program["expect"]:
        kind, text = program["expect"]
        name = "expected_output.txt" if kind == "output" else "expected_diagnostic.txt"
        io.open(os.path.join(target, name), "w", encoding="utf-8", newline="\n").write(text)
    written = written + 1
print("%d documentation programs" % written)
