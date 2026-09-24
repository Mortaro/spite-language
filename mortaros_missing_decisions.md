# Decisions waiting on Mortaro

Agents add here what only Mortaro can decide; Mortaro answers inline or in `mortaros_notes.md`, and an agent
moves the answer into `manual.md` (prose and decision log) and removes the item. Each item links to where the
manual argues it.

## Naming

1. **The two environments' names** (D85). Proposed: `Build` for compile time (`var build = Build()`,
   `build.target_operating_system`) and `Environment` for run time. You said "make better names, we can just
   change later".

## Open questions still open in manual.md

2. **Open question 15: whether `while` goes.** Proposal: the index loop over a list (199 of 259 loops) becomes an
   error naming the member template; `while` stays for loops over state.
   D94 review: [mortaros_review_while_and_else_if.md](mortaros_review_while_and_else_if.md). Of 472 loops, 31 are
   replaceable today and the exact-shape rule there catches about 10; most index loops pass the element to a
   function of the caller, which no template expresses.
3. **Open question 20: whether a nested `if`/`else` is an error.** Proposal: an `if` with an `else` inside a
   branch of another `if` with an `else` is an error naming "extract a function".
   D94 review: [mortaros_review_while_and_else_if.md](mortaros_review_while_and_else_if.md), section 3 (16 cases,
   with the `else if` chains and a proposed `switch` over an enum).
4. **Open question 13: how a class defines its own casts** (`func from_type(type: Symbol, value: type.class)`).
   You asked to be asked later.
5. **Open question 16: two versions of one dependency.** Needs D38 (git dependencies) first.
6. **Open questions 1, 3, 6, 8, 9, 10, 11**, the older ones: `= null` on a generic field, right-to-left casting
   in comparisons, `_` meaning private and unused, an unrelated `get_x()` intercepting `.x`, `${` in text,
   where `--final-classes` shows provenance, and function-valued `type` members.

## Proposals built and waiting for a yes or no

7. **D77, the five interpretations** (manual section 12): the rule covers constructor calls too; any call
   anywhere inside an argument counts; text holes are not arguments; calls moved out of `while` conditions and
   the right of `and`/`or` only where harmless; a hoisted variable may reuse the function's name
   (`var file_stem = file_stem(path)`).
8. **D78's narrow form**: only a call with arguments, in every branch, at the start of the branches, counts.
9. **The floor** (section 15, "The floor, named"): what stays C, and D82's form for showing it in
   `--final-classes`.
10. **Rows marked "(proposed by Claude, unconfirmed)"** in the decision log from 2026-09-23 and 2026-09-24:
    `Memory.address_of`/`compare_bytes`/`take_text`, the tree shaker, `nan` printing as `nan`, the REPL's
    command names and output, `Environment`'s sources and their order, `--operational_system` (superseded by
    D86), and the containers row.

## From the remote REPL

11. **D37 drain points and the remote REPL.** The remote REPL answers each command the moment it arrives, on its
    own thread (races accepted as a debug tool). D37 says commands should wait for a drain point, where the program
    is already waiting. Being built with D35's `Task` now; confirm the drain rule once it lands.
12. **`Socket` is new public library surface** (`library/socket.spite`), and the REPL's port is fixed at build time.

