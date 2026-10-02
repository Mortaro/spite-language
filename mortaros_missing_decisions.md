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
D374 (2026-10-01); 264 by D378 and 250 by D379, 252 by D380, 254 by D381, 256 by D383, 257 by D382, 258 by D384, 261 by D385, 253 by D386, 255 by D387, 259 by D388, 260 by D389, 262 by D390, 251 by D391 (2026-10-02). Earlier answers are listed in each row of `design/decisions.md`.

## Open

263. **How TLS is built** (D294, D350, D361). Options: (a) TLS 1.3 written in Spite (X25519, an AEAD, certificate
     verification against the system's root store), the long road D350 and D361 point to; (b) the system's own TLS
     through `DynamicLibrary` on Windows and macOS, which leaves Linux without one that is not a third-party
     library. Recommendation: (a), client first, checked against public test vectors, with an outside security
     review before it is called done.
