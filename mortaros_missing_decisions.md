# Decisions waiting on Mortaro

Only questions Mortaro still has to answer. An agent adds one here with a link to where the docs argue it; when he
answers (inline or in `mortaros_notes.md`), the answer goes into the page that teaches it and a row of
`design/decisions.md`, and the item is deleted. Item numbers never change. Each item: the question, the options,
the recommendation, what it blocks. "D244" marks something that can go wrong silently today; "D205" one Claude
could decide itself. The principles that answer most questions before they are asked: the moron (anyone who uses
Spite) never chooses, the language does; performance choices belong to the compiler; whatever can be written in
Spite is written in Spite (D350); reflection exposes everything and tree-shakes what is not read (D353); one way to do each
thing; prevent mistakes rather than offer options; storage owns its items and other references are `T?` (D354).

Every other item is answered: the 14 principle answers by D369, and every confirmation of what agents decided by
D370, 76 by D371 and 235 by D372 (2026-10-01). Earlier answers are listed in each row of `design/decisions.md`.

## Open

105. **A root class a nearer one shadows**: a root qualifier (`Root.Plugin`), or fall through only for a
     self-reference? Recommend fall through for a self-reference. Blocks nothing yet.
161. **An enum reopening's values** are appended in merge order, so a program's come before a loaded package's.
     Should the program's come last? Recommend last (the program has the final say). Blocks nothing.
