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
D370, 76 by D371, 235 by D372, 161 by D373 and 105 by
D374 (2026-10-01). Earlier answers are listed in each row of `design/decisions.md`.

## Open

250. **A heap the C library finds corrupted on Linux and macOS** ends in `SIGABRT` with glibc's own line (`free():
     invalid pointer`), and no `spite.fault` line or Spite frames; Windows reports `heap-corruption` with frames
     ([failure.md](docs/failure.md#what-a-native-fault-reports)). Options: (a) the fault handler also takes
     `SIGABRT`, reported as `abort` (any `abort()`, not only the heap's); (b) as `heap-corruption` when the C
     library's message says so; (c) leave it to the C library. Recommend (a): the frames are the useful part.
     Blocks nothing; `native_fault_heap` pins today's Linux output.
