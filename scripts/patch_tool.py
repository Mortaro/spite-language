"""Tiny patch helper: python patch_tool.py <target file> <patch file>
Patch file format: blocks separated by lines of '=====', each block is
OLD text, a line '-----', NEW text. OLD must occur in the target (first occurrence replaced).
A block whose OLD is the single word APPEND appends NEW to the end of the file."""
import sys

target, patch = sys.argv[1], sys.argv[2]
source = open(target, encoding="utf-8").read()
blocks = open(patch, encoding="utf-8").read().split("\n=====\n")
for number, block in enumerate(blocks, 1):
    if not block.strip():
        continue
    old, new = block.split("\n-----\n", 1)
    new = new.rstrip("\n") if new.endswith("\n\n") else new
    if old.strip() == "APPEND":
        source = source.rstrip("\n") + "\n\n" + new.strip("\n") + "\n"
        continue
    if source.count(old) < 1:
        print("block %d: OLD text not found:\n%s" % (number, old[:200]))
        sys.exit(1)
    source = source.replace(old, new, 1)
open(target, "w", encoding="utf-8", newline="\n").write(source)
print("patched %d blocks" % len([b for b in blocks if b.strip()]))
