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
D374 (2026-10-01); 264 by D378 and 250 by D379, 252 by D380, 254 by D381, 256 by D383, 257 by D382, 258 by D384, 261 by D385, 253 by D386, 255 by D387, 259 by D388, 260 by D389, 262 by D390, 251 by D391, 263 by D394 (2026-10-02). Earlier answers are listed in each row of `design/decisions.md`.

## Open

265. **A change of signedness that does not fit** (D359, D162; [values_and_types.md](docs/values_and_types.md#arithmetic-that-does-not-fit-halts)).
     Arithmetic and narrowing now halt, but `var bits: UnsignedInteger = count` with a negative `Integer` `count`, or any
     change of signedness at the same width or wider, still keeps the bits, since D162 leaves
     signedness out of "wider" and the hashes read their words this way (`var word: UnsignedLong = block.read_long(at)`).
     Options: (a) **keep it**: a same-width signedness change is a reading of the bits, as today. (b) **check it too**,
     so `-1` into an `UnsignedInteger` halts, and add a named function for reading the bits, as `bits()` reads a
     `Float`'s (say `Long.bits_as_unsigned()` and `UnsignedLong.bits_as_signed()`), which the hashes would call.
     Recommendation: (b), since a negative count turning into four billion is the silent wrap D359 forbids, and the
     bit readings are few and all in `library/`. Blocks: nothing; today's reading is (a).
266. **`crash flag` on a `Boolean?`** (D372). D372 makes `assert flag` mean "there and `true`" and names only
     `assert`, so `crash flag` on a `Boolean?` is still an error. Options: (a) `crash flag` halts unless the flag
     is there and `true`, as `assert` reads it; (b) keep it an error and write `crash flag == true`. Recommend (a):
     one reading of a `Boolean?` condition everywhere.
267. **`UdpSocket.port` of a closed socket** (D369 item 250). Options: (a) halts naming the closed socket, as
     built; (b) answers `Integer?`, `null` once closed. Recommend (a): asking a closed socket is a developer
     mistake (D199), and `0` would be a silent wrong value.
268. **`clamp` and not-a-number** (D369 item 249). `minimum` and `maximum` now pass `nan` on; `clamp` was built the
     same way, answering `nan` when the value or either bound is `nan` (it used to answer the low bound). Recommend
     confirming: the same rule as `minimum` and `maximum`.
269. **The name `wrapping_subtract`** (D359). D359 names `wrapping_sum` and `wrapping_multiply`; the subtraction
     was built as `wrapping_subtract`, after the `subtract` operator function. Recommend confirming it.
270. **Which functions of `Spite.Class` a class may override** (status "Functions of `Spite.Class`"). `Spite.Class`
     declares `to_string`, `to_debug`, `get_name` and the other reflection getters, so every class's own
     `to_string()` would become a class-object override that must fold. Options: (a) only members a reopening of
     `Spite.Class` adds are hooks; (b) everything except the per-object members and the reflection getters, which
     become reserved names; (c) a spelling that marks a hook. Recommend (b). Blocks backlog R8 (overriding).
271. **Member templates by class on a union list beyond `filter_`**: D330 names only `filter_<classes>()`;
     `count_`, `any_`, `all_` and `remove_where_` by member class are not built. Recommend allowing them the same
     way (one rule for every member template). Blocks nothing.
