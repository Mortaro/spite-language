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
D374 (2026-10-01); 264 by D378 and 250 by D379, 252 by D380, 254 by D381, 256 by D383, 257 by D382, 258 by D384, 261 by D385, 253 by D386, 255 by D387, 259 by D388, 260 by D389, 262 by D390, 251 by D391, 263 by D394 (2026-10-02); 266 to 296 by D468 to D488, and 268, 269, 271 and 276 to 279 by D493 to D497, 298 by D498, 301 by D499, 291 by D500, 299 by D501 (2026-10-03); 275 by D503, 302 by D504, 304 by D505 (2026-10-04); 280, 281, 284, 297, 300 and 303 by D510, decided by Claude under D509 and listed under To confirm (2026-10-07). Earlier answers are listed in each row of `design/decisions.md`.

## To confirm

Decided by an agent under D509 (anything that can be changed later). Each is built or documented as decided; say
"confirmed" or give the other answer, and the agent changes it.

- **D510, item 280**: a walk's extra values are extra arguments to `each_<member>(...)`. Why: keeps the binary
  format's walker a stateless singleton, no allocation and no state shared between threads.
- **D510, item 281**: a loop that can never leave, and whose calls can neither end the program nor wait, is a
  compile error. No time-based halt, since it would misfire on long computations.
- **D510, item 284**: the callback ticket table stays C until the backend primitives get a home.
- **D510, item 297**: strings, numbers and generic classes stay refused as union members; wrap them in a class.
- **D510, item 300**: `Fraction<$number_type>` names a number's fractional class (`Double` stays `Double`). Why:
  making everything `Float` would silently lose precision for `Double` programs.
- **D510, item 303**: `move_to` never replaces an existing file; delete it first if you mean to.
- **D510, D505's limits**: calls in a row overlap only as bare calls without arguments that reach a loop, with
  objects told apart by class. Widening them is planned work, not a question.
- **D510, D508's run-time branch**: an optimisation may leave one branch between two forms compiled in advance,
  on a fact only the run knows (a list's length). Never a scheduler.

## Open

None.
