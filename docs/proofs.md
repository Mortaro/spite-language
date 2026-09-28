# Proofs the compiler makes

A **proof** is a fact the compiler establishes while compiling: that a `T?` holds a value here, that an index is in
range, that an object never leaves its function, that no other thread can reach a singleton. With the fact in hand
the compiler does one of four things: it **accepts** code it would otherwise refuse, it **drops** a check it would
otherwise emit, it **chooses cheaper code**, or it **refuses** code that would go wrong. Every proof is worked out
while compiling and emits nothing of its own ([D177](decisions.md)); what a proof costs, if anything, is the check it
leaves in place when it does not hold.

This page is the catalogue of every such proof, built, partial or planned (Mortaro, 2026-09-28: "all the proofs the
compiler does should be well documented in our docs", [D276](decisions.md)). It is a catalogue, not a second home
for the rules: each entry says in a few lines what the proof is, and links the page that teaches it and states it in
full. Where an entry and that page differ, the page decides ([D193](decisions.md)).

**Why the fallback matters most.** A proof that does not apply never fails silently ([D244](decisions.md)): either
the compiler refuses the code and says what to write, or it keeps the run-time check. So each entry ends with
**Falls back**: what happens when the proof does not hold, and what you write yourself. That is the part to read
before relying on a proof.

Each entry has the same parts:

- **Status** -- **built**, **partial** or **planned**, as the compiler is today.
- **Proves** -- the fact.
- **Rule** -- how the compiler decides it, stated so you can predict it.
- **Buys** -- the check removed, the code accepted or refused, the cost saved.
- **Falls back** -- when it does not apply, and what you write instead.
- **See** -- the decisions, the page that teaches it, and a program in `conformance/` or `diagnostics/` that shows
  it.

Where the compiler does something other than what a page says, the entry says so under **Compiler today**. The
code is what it is; the difference is an open item, not a new rule.

## Which proofs apply to my code

A short guide for an AI writing Spite. Find what you are writing; the entries below say the rest.

- **Reading a `T?`** (a `Monster?`, a `[]` read, `first()`, a number read from text): narrow the name or the path
  itself with `if`, `assert`, `crash`, `while` or `switch`, or leave early with `if not value { return ... }`.
  [Narrowing](#narrowing-a-t), [an `if` that leaves](#an-if-that-leaves-proves-the-rest). A condition with a call in
  it narrows nothing: put the result in a `var` and narrow that.
- **A call between your check and your read**: the proof survives unless the call may assign what it reads or
  shrink a list it reads. After a call through a function value, prove it again.
  [A call keeps a proof](#a-call-keeps-a-proof-it-cannot-change).
- **Reading `list[index]`**: a bound (`index < list.count()`, or `at + 2 < list.count()` for `list[at]` to
  `list[at + 2]`), a count (`list.count() >= 3`), a count kept in a `var`, or `not list.is_empty()` proves the read. A `Dictionary` read is proven only by checking that key.
  [Proven reads](#a-proven-count-or-bound-proves-a-read).
- **A tight loop over numbers**: write `while index < values.count()` with `index = index + 1` last, over a local
  `List` of plain values, and call nothing but the list's reading functions and maths.
  [Counted loops](#a-counted-loop-reads-its-items-unchecked).
- **Dividing whole numbers**: prove the divisor with `!= 0` or `> 0` against a written `0`, or divide by a
  constant. [Proven divisor](#a-proven-divisor-is-not-checked).
- **Arithmetic that could overflow**: no proof removes the development check; write the wider type first.
  [Overflow](#signed-arithmetic-is-checked-while-developing).
- **A function that answers something**: end every path with `return` or a bare `crash`; use a guard `assert` only
  in a function whose result can say "nothing". [Every path ends](#every-path-ends-in-a-return),
  [guard `assert`](#a-guard-assert-answers-only-nothing).
- **A `switch`**: cover every member or value, or end with `_:`. [Switches](#a-switch-covers-every-case).
- **Generic code asking about `$T`**: `has_function`, `function_waits`, `fits_vector`, `argument_count`,
  `function_writes_parameter` fold; only the branch taken is compiled, and a `crash` that folds false in reached code
  is a compile error. [Compile-time questions](#compile-time-questions).
- **Small objects of numbers** (`Vector3`, a `Velocity`): make them, pass them to functions that keep nothing, and
  they live in the frame. [Frame objects](#objects-that-never-leave-their-function-live-in-the-frame).
- **`Vector`/`Items` items, rows, lent results**: a borrowed item is never kept and never read past a resize.
  [Borrowing and lending](#borrowing-and-lending).
- **Singletons with `Parallel`**: write nothing; the compiler picks the lock. Do not wait inside a locked function
  for work that calls back into it. [Threads](#threads-and-locks).

## At a glance

| Proof | Status | Buys | When it does not hold |
|---|---|---|---|
| [Narrowing a `T?`](#narrowing-a-t) | built | reads through a `T?` accepted | refused until narrowed |
| [Every side of an `and` narrows](#every-side-of-an-and-narrows) | built | one line proves several reads | in a `while` condition or an expression: refused |
| [An `if` that leaves proves the rest](#an-if-that-leaves-proves-the-rest) | built | guards read as answers | refused until narrowed |
| [Assigning undoes a proof](#assigning-undoes-a-proof) | built | nothing stale is read | prove it again |
| [A call keeps a proof it cannot change](#a-call-keeps-a-proof-it-cannot-change) | built | no re-check after harmless calls | prove it again; in a loop, an error |
| [Proving what is proven is an error](#proving-what-is-proven-is-an-error) | built | refuses dead checks | -- |
| [A copy made to narrow is an error](#a-copy-made-to-narrow-is-an-error) | built | refuses the copy | -- |
| [A class test narrows its block](#a-class-test-narrows-its-block) | built | union member access | use `switch` |
| [Every path ends in a `return`](#every-path-ends-in-a-return) | built | refuses a made-up answer | -- |
| [A guard `assert` answers only "nothing"](#a-guard-assert-answers-only-nothing) | built | refuses a silent default | write the answer, a `T?` or `crash` |
| [A switch covers every case](#a-switch-covers-every-case) | built | refuses a forgotten case | add the case or `_:` |
| [A proven count or bound proves a read](#a-proven-count-or-bound-proves-a-read) | built | `list[i]` read without narrowing | refused until proven, naming how |
| [A proven read halts outside its list](#a-proven-read-halts-outside-its-list) | built | a stray counter stops the program | -- |
| [A counted loop reads its items unchecked](#a-counted-loop-reads-its-items-unchecked) | built | no range checks, vectorisable | the ordinary checked loop |
| [A proven divisor is not checked](#a-proven-divisor-is-not-checked) | built | no zero check | the zero check |
| [A divisor written as zero is an error](#a-divisor-written-as-zero-is-an-error) | built | refuses a certain halt | -- |
| [Signed arithmetic is checked while developing](#signed-arithmetic-is-checked-while-developing) | built; the proof planned | nothing yet | the overflow check, in development builds |
| [A wider operand is written first](#a-wider-operand-is-written-first) | built | refuses a silent cut | write the wider side first |
| [A dictionary's key kind](#a-dictionarys-key-kind-is-decided-while-compiling) | built | numbers hashed as numbers | text keys; mixing is an error |
| [Maths on constants](#maths-on-constants-is-worked-out-while-compiling) | built | no call | the call |
| [Conditions decided while compiling](#conditions-decided-while-compiling) | built | the branch not taken is absent | a run-time test |
| [Whether a function waits](#whether-a-function-waits) | built | engines branch while compiling | `true` where unsure |
| [How many arguments a function takes](#how-many-arguments-a-function-takes) | built | one branch per arity | -- |
| [Whether a class fits a `Vector`](#whether-a-class-fits-a-vector) | built | inline storage | reference storage |
| [Whether a function writes a parameter](#whether-a-function-writes-a-parameter) | built | engines refuse lost writes | `true` where unsure |
| [Whether a class keeps state](#whether-a-class-keeps-state) | built | engines refuse stateful systems | `true` where unsure |
| [A `crash` that folds false is an error](#a-crash-that-folds-false-is-a-compile-error) | built | a certain halt becomes a build error | a run-time check |
| [Tree shaking](#tree-shaking-what-main-can-reach) | built | smaller program | an inspectable build keeps all |
| [A reload compiles only the changed classes](#a-reload-compiles-only-the-changed-classes) | built | a reload in seconds | the whole program is compiled |
| [Buffers in the frame](#a-buffer-freed-in-its-block-lives-in-the-frame) | built | no allocation | the heap |
| [Frame objects](#objects-that-never-leave-their-function-live-in-the-frame) | built | no allocation | the heap |
| [Local and variadic lists in the frame](#local-and-variadic-lists-in-the-frame) | built | no allocation | the heap |
| [Borrowed items of a `Vector` or `Items`](#borrowed-items-of-a-vector-or-items) | built | no count, no copy | refused, naming `copy()` |
| [No read past a resize](#no-borrowed-read-past-a-resize) | built | the item cannot move under you | refused |
| [Rows and walked rows](#rows-of-borrowed-items) | built | a row in the frame | refused, or an ordinary `type` value |
| [Lent results](#an-item-lent-to-the-caller) | built | no count on the result | an owned result, or refused |
| [Lent arguments](#an-item-lent-to-a-call) | built | no count on the argument | refused, saying why |
| [A list element lent to a row](#a-list-element-lent-to-a-row) | built | no count, one lock per block | the counted call |
| [Held arguments](#an-argument-its-caller-holds-is-passed-uncounted) | built | no count on the argument | the counted call |
| [A singleton attribute that never changes](#a-singleton-attribute-that-never-changes-is-read-in-place) | built | no count, no lock | the counted, locked read |
| [List templates read uncounted](#a-lists-templates-read-their-elements-uncounted) | partial | no count per element | the counted read |
| [Which singletons a `Parallel` reaches](#which-singletons-a-parallel-reaches) | built | no lock | a lock |
| [Read-only and atomic singletons](#read-only-and-atomic-singletons) | built | no lock | a lock |
| [A function that touches no changing state](#a-function-that-touches-no-changing-state-takes-no-lock) | built | no lock for it | the lock |
| [Reading functions share the lock](#reading-functions-share-the-lock) | built | readers never contend | the plain lock |
| [A counted loop takes the lock once](#a-counted-loop-takes-a-singletons-lock-once) | built | one lock per loop | one lock per call |
| [A lock that would wait forever](#a-lock-that-would-wait-forever-is-an-error) | partial | refuses a hang | -- |
| [What a `Parallel` may reach](#what-a-parallel-may-reach) | built | refuses a data race | -- |
| [Which calls suspend](#which-calls-suspend-a-concurrent) | built | state machines, no fibers | the wait runs in place |
| [Other refusals](#other-refusals-built-on-an-analysis) | built | refuses dead code and leaks | -- |
| [Planned proofs](#planned-proofs) | planned | | |

## Presence and control flow

### Narrowing a `T?`

- **Status.** Built.
- **Proves.** A `T?` name or path holds a value, so it is a plain `T` in place, the same name.
- **Rule.** The condition must be exactly a name or a path -- names, `.member` and `[index]` links with no call in
  them (`operand_text`, `try_nullable_guard`). `if value { }` narrows the block, and its `else` nothing; `assert value`
  and `crash value` narrow the rest of the block and every block nested in it; `while value { }` narrows the body
  and is tested again every pass; `switch value { T: ... Null: ... }` narrows the `T` case. One check narrows every
  link of a path and its prefixes, never a sibling path (D43). Narrowing tests presence, never the value: a `0`,
  `false` or `""` that is there is present.
- **Buys.** Member reads, calls and stores through the value compile. Nothing is emitted beyond the test itself.
- **Falls back.** Reading, calling or storing through an unproven `T?` is refused: `this value may be null (it is a
  Monster?), so 'name' cannot be read from it yet: narrow it first with 'if value { }', 'assert value' or 'crash
  value'`. A condition with a call (`if find(name) { }`) is not a path: keep the result in a `var` and narrow the
  `var`. A `Boolean?` is never a condition; compare it `== true` or `switch` over it. `value == null` is an error.
  What a branch narrows is gone after the branch: two branches' proofs are not merged.
- **See.** D3, D43, D45, D69; [failure.md: Narrowing](failure.md#narrowing),
  [Narrowing a path](failure.md#narrowing-a-path); `conformance/stage6/path_narrowing`,
  `conformance/stage6/present_zero`, `diagnostics/store_through_nullable`, `diagnostics/path_narrow_else`.

### Every side of an `and` narrows

- **Status.** Built.
- **Proves.** Each side of an `and` in an `if`, `assert` or `crash` holds where the whole condition holds.
- **Rule.** `assert` and `crash` split their `and` into one check per side; an `if` narrows each side into its
  block. A later side already sees the earlier sides narrowed, so `assert target and target.health > 0` is one line.
- **Buys.** `crash names[position] and ages[position]` proves both reads.
- **Falls back.** In a `while` condition and inside an ordinary expression (`return node and node.ready`), the right
  side of an `and` sees only proven *indices* from the left side, not narrowed `T?`s: such code is refused. Write
  `while node { if ... }`, or an `if`. `or` narrows only in [an `if` that leaves](#an-if-that-leaves-proves-the-rest).
- **See.** D43; [failure.md: Narrowing](failure.md#narrowing); `conformance/stage6/and_narrowing`.

### An `if` that leaves proves the rest

- **Status.** Built.
- **Proves.** After an `if` with no `else` whose block leaves the function, the opposite of its condition holds for
  the rest of the enclosing block.
- **Rule.** The block leaves when its **last** statement is a `return`, a bare `crash`, or a `switch` whose every
  case leaves in the same sense. The condition is split into `or` sides: each side written `not path` narrows that
  path, the later sides seeing the earlier ones narrowed; every other side is turned around and proves the `[]`
  reads and divisors its opposite bounds (`if index >= names.count() { return "" }` proves `names[index]`). The
  block itself sees none of it.
- **Buys.** `if not found { return -1 }` followed by `found.length()` compiles, so D244's explicit answers read as
  guards do. The C is the condition as written.
- **Falls back.** Nothing is proven after an `if` with an `else`, a block ending in `crash condition`, in an
  `if`/`else` whose branches both return, or in `while true`; a class test (`if creature != Fish { return }`)
  narrows nothing; a `not` side that is not a path (`not x.ready()`) proves nothing. Use `assert` or `crash`.
- **See.** D256; [failure.md: An `if` that leaves proves the rest](failure.md#an-if-that-leaves-proves-the-rest);
  `conformance/stage6/leaving_if`, `conformance/stage6/returning_branch_proof`.
- **Compiler today.** "Leaving" here is narrower than [every path ends](#every-path-ends-in-a-return), which also counts
  `if`/`else` and `while true`.

### Assigning undoes a proof

- **Status.** Built.
- **Proves.** A proof is still true at the read.
- **Rule.** Assigning a narrowed path, or anything it reads through (a name in a `[]` index included), undoes every
  proof beneath it from that line. Assigning the path itself a value that cannot be null keeps its own narrowing;
  "cannot be null" is a constructor, a literal, an arithmetic expression, or a name or path whose type is not `T?`.
  Inside a `while`, undoing a proof made before the loop and read in it is an error, since the next pass would read
  it unproven.
- **Buys.** A narrowing never outlives what it narrowed.
- **Falls back.** Prove it again after the assignment, or narrow inside the loop. Note that a call answering a plain
  `T` counts as "may be null" here: `box = make_box()` un-narrows `box` even when `make_box(): Box`.
- **See.** D43; [failure.md: Narrowing a path](failure.md#narrowing-a-path); `diagnostics/narrowed_name_reassigned`,
  `diagnostics/path_narrow_assignment`, `diagnostics/path_narrow_loop`,
  `conformance/stage6/narrowed_value_assignment`.

### A call keeps a proof it cannot change

- **Status.** Built.
- **Proves.** A call between a check and a read cannot change what the check proved, so the proof stands without a
  `const` keyword.
- **Rule.** The compiler follows the called function and everything it calls (`call_effects.spite`,
  `effect_study.spite`) and collects the attributes it may assign, by class and name, and the lists it may shrink
  (`clear`, `remove_at`, `remove_first`, `remove_last`, `remove`, `remove_swapping`, `remove_where...`, `truncate`,
  `swap`). A proof that reads through one of them is undone. The receiver's class comes from declared types, the class
  a constructor makes, the class a called function returns, a `[]` read's element class and an attribute's declared
  class; `List`, `Dictionary` and `String` reach only their own functions. Growing a list keeps an index proof. A
  `return`'s own calls undo nothing. Locals and parameters are never changed by a call.
- **Buys.** No re-check after a call that provably cannot change the value.
- **Falls back.** A call on a value whose class cannot be told (a `type`, a type parameter) is followed into every
  function of that name; a call through a function value undoes every proof about attributes and lists; a `clear` or
  `remove_...` on a list reached through `[]` or a call undoes every proof about a list; operators and getters are
  not followed; a local that aliases an attribute's list is not tracked. After such a call, prove it again; inside a
  loop it is an error naming the call: `'forget_target()' inside this loop may change 'target', which undoes what was
  proven about it before the loop ...`.
- **See.** D169, D262; [failure.md: A call may undo a proof](failure.md#a-call-may-undo-a-proof),
  [optimizations.md: Proofs that survive a call](optimizations.md#proofs-that-survive-a-call);
  `diagnostics/call_undoes_proof`, `diagnostics/receiver_call_undoes_proof`, `diagnostics/branch_undoes_proof`,
  `conformance/stage6/receiver_call_effects`.
- **Compiler today.** A proven divisor is kept as a list-like fact, so a call that may shrink any list (through a
  function value) undoes it too.

### Proving what is proven is an error

- **Status.** Partial. [D279](decisions.md) (Mortaro) makes it every fact the compiler holds: a narrowed
  `T?` of any type, a proven read or divisor, a decided class test, a condition checked twice with nothing between
  that could change it, and a folded condition -- each refused with where it was proven. Built today: the two below.
- **Proves.** A check adds nothing, because the value is already narrowed or the read already proven.
- **Rule.** `assert`/`crash` on a `[]` read a bound or count already proves names the proof: `'codes[index]' is
  already proven by the loop condition 'index < codes.count()', so this 'crash' proves nothing: remove it`. On a
  class-typed value that cannot be null: `this value cannot be null here (it is a Tracker), so 'assert' on it proves
  nothing: remove the check`.
  Inside a `while`, only a proof made inside that loop makes a check of a `[]` read redundant: one made before the
  loop does not, since an assignment in the loop would leave the next pass unproven (D277), so `crash
  names[index - 1]` may be written inside a loop that lowers `index`.
- **Buys.** No dead check survives, so a reader never wonders what it guards.
- **Falls back.** Delete the line.
- **See.** D43, D256; [failure.md: Narrowing a path](failure.md#narrowing-a-path);
  `diagnostics/check_proves_nothing`, `diagnostics/proven_element`, `diagnostics/proven_index_check`.
- **Compiler today.** On a narrowed class the message is "proves nothing"; on a narrowed `String?`, number or enum it
  is the error for a non-`Boolean` condition, which names the wrong fix. Still open in [failure.md](failure.md#nothing-fails-silently--the-rule).

### A copy made to narrow is an error

- **Status.** Built.
- **Proves.** A local only copies a `T?` name or path so that it can be narrowed.
- **Rule.** A `var` whose value is a bare name or path of a `T?` type, narrowed later by `assert`, `crash`, `if` or
  `switch` on its bare name, never assigned again, and whose source is not assigned in the function either.
- **Buys.** Refuses the copy: `'watcher' only copies 'tracker' so it can be narrowed: narrow 'tracker' itself`.
- **Falls back.** A snapshot taken before its source changes is not a copy for narrowing, and is allowed.
- **See.** D63; [failure.md: Narrowing a path](failure.md#narrowing-a-path); `diagnostics/copy_to_narrow`.
- **Compiler today.** D63 says "which is then narrowed", in any way; the compiler recognises only a condition that is
  exactly the bare name, so a copy narrowed by `if not copy { return }`, `while copy` or `assert copy and ...` is not
  caught. Still open in [failure.md](failure.md#nothing-fails-silently--the-rule).

### A class test narrows its block

- **Status.** Built.
- **Proves.** A union value is one given member.
- **Rule.** `if value == Monster { }`, with `value` a bare name, narrows it to `Monster` in the block; a `null` tests
  false. In a generic class a test that can never hold folds to `false` (D167).
- **Buys.** Member access of that class inside the block.
- **Falls back.** `!=`, a path, a test inside an `and`, the `else` branch and a leaving `if` narrow nothing: use a
  `switch` over the union.
- **See.** D75, D167; [control_flow.md: `value == Class`](control_flow.md#value--class);
  `conformance/stage6/class_test`, `diagnostics/switch_single_case`.

### Every path ends in a `return`

- **Status.** Built.
- **Proves.** A function that declares a result never reaches its closing `}`.
- **Rule.** A path ends at a `return`, a bare `crash`, an `if`/`else` whose branches both end, a `switch` whose every
  case ends, a `while true`, and a condition the compiler folded whose taken branch ends. It passes through an `if`
  with no `else`, any other `while`, an `assert` or a `crash` with a condition, and anything else.
- **Buys.** Refuses a result nobody wrote, naming the path: `'sign_of' answers a String, but when 'value < 0' is
  false (line 12, an 'if' with no 'else') it reaches its end without a 'return', so a caller would get a String
  nobody wrote ...`.
- **Falls back.** Write the last `return` after the `if` or the loop, `return null` included, or a bare `crash`
  where the path cannot happen.
- **See.** The D244 implementation row of 2026-09-27; [failure.md: Every path ends in a
  `return`](failure.md#every-path-ends-in-a-return); `diagnostics/falling_end`.
- **Compiler today.** `while true` ends a path whether or not its body can leave, and nothing reports such a loop
  outside a locked singleton function; statements after a `return` in the same block compile without an error. Both
  still open in [failure.md](failure.md#nothing-fails-silently--the-rule).

### A guard `assert` answers only "nothing"

- **Status.** Built.
- **Proves.** When an `assert` fails, the function's answer is one the caller can tell from a real one.
- **Rule.** A guard `assert` is allowed only in a function whose result, per instance, is nothing, a `T?`, a `List`,
  a `Dictionary`, or a `Vector`/`Items`. In a function answering a number, `Boolean`, text, enum or object it is an
  error (D244, D245), and never in a constructor (D28). Where it is allowed, an `if` with no `else` whose only
  statement returns the default is an error naming the `assert` to write (D106), and so is a last `if` with no `else`
  that only checks a value is there.
- **Buys.** Refuses a silent default: `this 'assert' would answer a default Integer (0) that a caller cannot tell
  from a real one: return a value (...), make the result 'Integer?', or 'crash ...' if this is a developer mistake`.
- **Falls back.** Write the answer (`if not found { return -1 }`, which [then proves the
  rest](#an-if-that-leaves-proves-the-rest)), make the result a `T?`, or `crash`.
- **See.** D106, D244, D245; [failure.md: A default that looks like an answer is an
  error](failure.md#a-default-that-looks-like-an-answer-is-an-error),
  [An `if` that only returns the default is an `assert`](failure.md#an-if-that-only-returns-the-default-is-an-assert),
  [The last `if` of a function](failure.md#the-last-if-of-a-function); `diagnostics/default_answer`,
  `diagnostics/default_guard`, `diagnostics/returning_guard`, `diagnostics/terminal_if`.
- **Compiler today.** D106's lint recognises only the literal defaults `null`, `false`, `0`, `0.0`, `""` and a bare
  `return`, so `if ... { return List<T>() }` in a function answering a `List` is not refused, and a function that
  lends a list element (D269) is not checked for a guard `assert`. Still open in [failure.md](failure.md#nothing-fails-silently--the-rule).

### A switch covers every case

- **Status.** Built.
- **Proves.** No value reaches a `switch` without a case for it.
- **Rule.** Over a union, every member or a last `_:`; over a `T?`, a `T:` case and a `Null:` case; over an enum,
  every value or a last `_:`; over a whole number or text, always a last `_:`. A `_:` that answers for nothing is an
  error, and so is a repeated case. Three `if`s in a row (or an `else if` chain of three) that each compare one name
  with a constant and only return are an error that writes the `switch` for you (D239).
- **Buys.** A member or value added later cannot be forgotten; a `switch` whose cases all return ends a function.
- **Falls back.** Add the cases, or end with `_:` for the rest.
- **See.** D60, D170, D239; [control_flow.md: `switch` over a union](control_flow.md#switch-over-a-union),
  [`switch` over values](control_flow.md#switch-over-values); `diagnostics/missing_switch_case`,
  `diagnostics/switch_rest_case`, `diagnostics/switch_chain`, `conformance/stage6/rest_case`.
- **Compiler today.** The union message reads "has no case for File: every member is covered, so a member added
  later cannot be forgotten", which says the opposite of what it means. Still open in [failure.md](failure.md#nothing-fails-silently--the-rule).

## Indices and numbers

### A proven count or bound proves a read

- **Status.** Built for `List`, `Vector`, `Items` and `Dictionary` (D225, D226).
- **Proves.** A `list[index]` read is in range from above, so it is a plain `T`, not a `T?`.
- **Rule.** In an `assert`, `crash`, `if` block, `while` body, the right side of an `and`, or after [an `if` that
  leaves](#an-if-that-leaves-proves-the-rest): `index < list.count()` (also `- k` for a literal `k`, and
  `list.count() > index`) proves `list[index]`; `base + k < list.count()`, `k` a literal from 1 to 63, proves
  `list[base]` to `list[base + k]` (D277); `index < count` proves `list[index]` when `var count = list.count()` (or
  `- k`) was declared and neither `list` shrank nor `count` was assigned since (D277); `list.count() == k` or
  `>= k` proves `list[0]` to `list[k-1]`, `> k` to `list[k]`, and `!= 0` or `not list.is_empty()` proves `list[0]`
  (at most 64 indices); a list literal proves its own indices (`var sizes = [3, 5, 8]`). The list and the index are
  paths with no call in them; any call-free index is its own path, compared as printed (`crash glyphs[code - 32]`).
  A count proves no `Dictionary` key, and nothing proves a read through a program's own `get_at` but a written
  narrowing.
- **Buys.** The read needs no written narrowing, and costs no presence test ([below](#a-proven-read-halts-outside-its-list)).
- **Falls back.** Unproven, the read is a `T?` that must be narrowed, and the error names the read, its three
  narrowing lines and the count or bound that would prove it. Assigning the list, the index or any name the index
  reads, or shrinking the list, undoes the proof; so does a call that may (above); inside a loop, undoing a proof
  the loop read is an error. Not traced, so still to be narrowed by hand: an index that is a parameter (a handle
  into lists the caller keeps in step), an index read from another list, a count kept in an attribute or worked out
  (`count() / 8`, `width * height`), lists that only grow together, a counter walking down from `count() - 1`, and a
  read after `append` at `count() - 1`. Two bounds joined by `and` prove two lists but stop at the shorter one
  without a word: when the lists must match, bound one and `crash` the other's read.
- **See.** D64, D169, D225, D226, D277; [failure.md: Reading with `[]` answers `T?`](failure.md#reading-with--answers-t);
  `conformance/stage6/count_bound_proofs`, `conformance/stage6/bound_proofs`, `conformance/stage6/structural_index`,
  `diagnostics/index_reads`, `diagnostics/bound_proofs_undone`, `diagnostics/count_proves_no_key`,
  `diagnostics/structural_index_undone`.

### A proven read halts outside its list

- **Status.** Built (D225, D244; proposed by Claude, unconfirmed).
- **Proves.** Nothing new: it is what a proven read does when its proof covered only the top of the index.
- **Rule.** A read a bound or count proves is taken from `get_at` directly, with one branch the C compiler is told
  is never taken; if the answer is missing the program halts with `spite: 'names[index]' is outside its list ...,
  at file:line`. A read narrowed by `crash`, `assert` or `if` is taken with no test, since that line tested it; a
  read in [a counted loop](#a-counted-loop-reads-its-items-unchecked) has both ends proven and no test at all.
- **Buys.** A counter gone below zero stops the program where it happened instead of reading a default.
- **Falls back.** --
- **See.** D225, D244; [optimizations.md: A proven read tests only its bounds](optimizations.md#a-proven-read-tests-only-its-bounds);
  `conformance/stage6/proven_read_outside`.

### A counted loop reads its items unchecked

- **Status.** Built.
- **Proves.** Inside `while index < values.count()`, the counter is at least 0 and below the count, and the list's
  size cannot change.
- **Rule.** The condition is exactly `index < values.count()` over a local or parameter holding a `List`
  of numbers or `Boolean`s (the only list that holds them since D225); `index` is a local `Integer` whose every assignment in the function is a literal of 0 or
  more, or `index = index + 1` as the last statement of a loop bounded by `index < ...count()` or a literal; the body
  only declares and assigns plain locals, reads and writes items of plain-value lists, and calls those lists' reading
  functions and maths. A second list indexed by the counter is checked once, before the loop.
- **Buys.** The count is read once and items are read and written as a C array, which the C compiler vectorises;
  `benchmarks/plain_loops` from about 680 to 190 µs.
- **Falls back.** The ordinary loop, every read checked. Any other call (an `append`), a counter starting from a
  parameter, a `Long` counter, a list held in an attribute, a `--repl`, `--repl-port` or `--hot-reload` build, or a
  function that waits keeps it ordinary. A body with an `assert` or `crash` keeps the second list's checks.
- **See.** D222; [optimizations.md: A loop over plain values reads its count once and its items
  unchecked](optimizations.md#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked);
  `conformance/stage6/counted_loops`.

### A proven divisor is not checked

- **Status.** Built.
- **Proves.** A whole-number divisor is not zero.
- **Rule.** The divisor is a constant other than zero (a tree of `Integer`-range literals), or a call-free path
  proven by `path != 0` or `path > 0` with the literal `0` on the right, in the same places a
  [read is proven](#a-proven-count-or-bound-proves-a-read), kept or undone by calls as any proof is.
- **Buys.** No compare-and-branch before `/` or `%`.
- **Falls back.** The check stays and a zero halts naming the line: `spite: 'total / parts' divided by zero, at ...`.
  `parts >= 1`, `parts > 5`, `0 != parts`, a divisor with a call in it (`total / names.count()`) and a `Long`
  constant outside the `Integer` range are not proofs. As in Go, the smallest signed value divided by `-1` wraps to
  itself in every build.
- **See.** D199, D201; [optimizations.md: A proven divisor is not checked](optimizations.md#a-proven-divisor-is-not-checked),
  [values_and_types.md: Numeric types](values_and_types.md#numeric-types--implemented-provisional);
  `conformance/stage6/division_by_zero`.

### A divisor written as zero is an error

- **Status.** Built.
- **Proves.** A division always halts.
- **Rule.** A whole-number `/` or `%` whose divisor is a constant expression equal to zero, or a codegen value folded
  to `0`.
- **Buys.** `'total / 0' divides by zero, which always halts the program ...` at compile time.
- **Falls back.** --
- **See.** D201; [values_and_types.md](values_and_types.md#numeric-types--implemented-provisional);
  `diagnostics/division_by_constant_zero`.

### Signed arithmetic is checked while developing

- **Status.** The check is built; the proof that would drop it is planned.
- **Proves.** Nothing yet: the check depends only on the build and the type.
- **Rule.** In a `--debug-memory` or inspectable build (`--development`, `--hot-reload`, `--repl`, `--repl-port`),
  every `+`, `-` and `*` on `Tiny`, `Short`, `Integer` or `Long` halts if the answer does not fit. A constant
  expression that does not fit its type is a compile error in every build (D162).
- **Buys.** A production build carries no check at all.
- **Falls back.** Production builds wrap, and unsigned types wrap in every build (D249). No range fact removes the
  development check, not even a counted loop's `index = index + 1`; dropping it where a proof bounds the operands is
  planned. Write the wider type first where a total may grow.
- **See.** D162, D249; [values_and_types.md: Signed arithmetic that does not fit halts while you
  develop](values_and_types.md#signed-arithmetic-that-does-not-fit-halts-while-you-develop),
  [optimizations.md](optimizations.md#signed-arithmetic-is-checked-only-while-developing);
  `conformance/stage6/integer_overflow`.

### A wider operand is written first

- **Status.** Built.
- **Proves.** A right operand fits the left operand's type, so casting it loses nothing.
- **Rule.** In arithmetic, comparisons and bitwise functions, a right side with more bits than the left, or a
  floating side under a whole-number left, is an error naming the operation turned around -- unless it is an
  integer literal within the left type's range.
- **Buys.** Refuses a silent cut (`progress > -0.5` with an `Integer` `progress`).
- **Falls back.** Write the wider side first, or keep the value in a variable of the wider type. Assigning,
  passing or returning a wider value into a narrower name is not checked by this proof.
- **See.** D162, D251; [values_and_types.md: Wider arithmetic goes wider operand
  first](values_and_types.md#wider-arithmetic-goes-wider-operand-first),
  [Comparisons are checked the same way](values_and_types.md#comparisons-are-checked-the-same-way);
  `diagnostics/wider_right_operand`, `diagnostics/wider_comparison`, `diagnostics/wider_bitwise_operand`.

### A dictionary's key kind is decided while compiling

- **Status.** Built.
- **Proves.** Every key a dictionary is given, and every dictionary it flows into, is of one kind: text or a whole
  number.
- **Rule.** Each place a dictionary type is written is joined with every place it flows to; the keys given to
  `[]`, `set`, `get`, `has` and `remove` decide the kind, the widest whole number winning, text for a `String`, symbol
  or enum, and text for a dictionary given no key. It settles in at most eight passes.
- **Buys.** A number key is hashed as a number, with no `String` made.
- **Falls back.** Text keys. Two kinds meeting is a compile error, not a conversion; to key by text, give text.
- **See.** D223, D224; [collections.md: Keyed by numbers](collections.md#keyed-by-numbers),
  [optimizations.md](optimizations.md#a-dictionary-keyed-by-numbers-hashes-the-numbers);
  `conformance/stage6/number_keys`, `diagnostics/mixed_dictionary_keys`.

### Maths on constants is worked out while compiling

- **Status.** Built.
- **Proves.** A maths function called on constants has one answer.
- **Rule.** A maths member (`sine`, `square_root`, ...) whose receiver and arguments are literals, the number
  classes' constants (`pi()`, `infinity()`, ...) or other folded calls is answered by the compiler's own C library.
- **Buys.** No call, and no `math.h` when every call folds.
- **Falls back.** A variable is never a constant: `angle.sine()` is a call. Plain arithmetic on literals is left
  to the C compiler.
- **See.** [optimizations.md: Maths on constants is worked out while
  compiling](optimizations.md#maths-on-constants-is-worked-out-while-compiling); `conformance/stage6/maths_folding`.

## Compile-time questions

### Conditions decided while compiling

- **Status.** Built.
- **Proves.** A condition's answer is a fact of the build.
- **Rule.** An `if` whose condition is a `Build` field, a codegen value (`$is_magic`), a test on a codegen type
  (`$T == List`, `$T.element_type == Float`), `attribute.class == X` in a walk, a class test the value's type already
  answers, one of the questions below, or `not`, `and`, `or`, `==`, `!=` over them. An `and` whose left folds false,
  or an `or` whose left folds true, folds whatever the right side is. A function of a generic class is compiled for
  an instance only when code that survived folding names it.
- **Buys.** The branch not taken is absent, so it may use what this build or instance does not have.
- **Falls back.** Anything else is tested at run time.
- **See.** D84, D85, D114, D167, D237; [optimizations.md: Deciding conditions at compile
  time](optimizations.md#deciding-conditions-at-compile-time),
  [metaprogramming.md: Asking what a generic was given](metaprogramming.md#asking-what-a-generic-was-given);
  `conformance/stage6/codegen_member_fold`, `conformance/stage6/walked_class_fold`.
- **Compiler today.** A `Build` field in an `assert` or `crash` is not folded (only codegen questions are), so
  [D250](#a-crash-that-folds-false-is-a-compile-error) does not apply to it: still open in [failure.md](failure.md#nothing-fails-silently--the-rule).

### Whether a function waits

- **Status.** Built.
- **Proves.** A function can or cannot reach a wait (a sleep, a read of a console, file or socket, a scheduler
  join).
- **Rule.** Waiting spreads from those library functions through calls by name, function values and dispatch
  through a union or `type`, to a fixed point. `$T.function_waits("update_each")` folds to the answer; an answer that
  would change once the asking function is compiled is an error (D209). Joining a `Parallel` is not a wait, nor is
  `finished` (D216).
- **Buys.** An engine runs a system on a `Concurrent` only where it waits.
- **Falls back.** Where it cannot tell (a function value of the same signature, a constructor, dispatch), the
  answer is `true`.
- **See.** D176, D209, D216; [metaprogramming.md: Asking whether a function
  waits](metaprogramming.md#asking-whether-a-function-waits); `conformance/stage6/waiting_systems`,
  `diagnostics/function_waits_paradox`.

### How many arguments a function takes

- **Status.** Built.
- **Proves.** A function's arity.
- **Rule.** `$T.argument_count("update_each")` and `phase.argument_count()` in a walk fold to a whole number,
  compared with `==`, `!=`, `<`, `<=`, `>` or `>=`; 0 when the class does not declare the function; a pattern must
  agree across every function it matches.
- **Buys.** A runner compiles only the branch that fits a system's arity.
- **Falls back.** A pattern whose functions disagree is an error.
- **See.** D219; [metaprogramming.md: Asking how many arguments a function
  takes](metaprogramming.md#asking-how-many-arguments-a-function-takes); `conformance/stage6/folded_argument_count`,
  `diagnostics/argument_count`.

### Whether a class fits a `Vector`

- **Status.** Built.
- **Proves.** A class's objects can be laid out inline in a block, with no header and no count.
- **Rule.** Every attribute is a number, `Boolean`, text, enum, symbol or singleton, or a `T?` of those; the class
  has no `drop()` and no function uses `this` as a value; it is not a generic template.
  `$T.fits_vector()`, `attribute.class.fits_vector()` and `$T.any_attribute_fits_vector(Entity)` fold to the answer.
- **Buys.** `Items<T>` chooses inline storage and lends its items.
- **Falls back.** Reference storage: counted items, none of the borrow rules.
- **See.** D217, D218, D268; [metaprogramming.md: Asking whether a class fits a
  Vector](metaprogramming.md#asking-whether-a-class-fits-a-vector),
  [optimizations.md](optimizations.md#an-items-storage-is-chosen-while-compiling); `conformance/stage6/items_columns`.

### Whether a function writes a parameter

- **Status.** Built.
- **Proves.** A function writes, or does not write, what one of its parameters is given, or anything reached
  through it.
- **Rule.** `$T.function_writes_parameter("update_each", index)` folds. A write is an attribute or item set on what
  the parameter reaches, a call that writes its own object, passing it to a parameter that is written, or storing
  it where a later write could reach it; reassigning the name and copying plain values out are not writes. It follows
  a walked argument's `.index` and what a callee returns (D268).
- **Buys.** An engine refuses a system whose writes a snapshot would lose, with `crash not ...`.
- **Falls back.** Where it cannot decide -- a function value, dispatch through a union or `type`, a template, an
  unknown class -- the answer is `true`.
- **See.** D261, D268; [metaprogramming.md: Asking whether a function writes a
  parameter](metaprogramming.md#asking-whether-a-function-writes-a-parameter); `conformance/stage6/parameter_writes`,
  `diagnostics/snapshot_argument_writes`.
- **Compiler today.** A function whose body the compiler supplies counts as writing only when its name is on a fixed
  list (`resize`, `free`, `copy_to`, `write_value`, ...) or starts with `write_`; any other supplied function answers
  "does not write", a silent `false`. Still open in [failure.md](failure.md#nothing-fails-silently--the-rule). This analysis decides no memory layout; the memory
  proofs below use their own.

### Whether a class keeps state

- **Status.** Built.
- **Proves.** No function of a class writes its own object after it is made, and no singleton it binds keeps
  state -- or that one does.
- **Rule.** `$T.has_state()` folds, and so does `kind.class.has_state()` in a walk over `Symbol<Spite.Class>`. It
  asks the question of [Whether a function writes a parameter](#whether-a-function-writes-a-parameter) of each
  function's own object, skipping the constructor and `drop()`, and follows the singletons the class binds.
- **Buys.** An engine can refuse a system that keeps state between frames, for every system at once.
- **Falls back.** As that question does: where the study cannot decide, the answer is `true`. The standard
  library counts like any code, so a class binding `Console` has state (reading a line writes its buffer).
- **See.** D287; [metaprogramming.md: Every class in the program](metaprogramming.md#every-class-in-the-program);
  `conformance/stage6/class_walk`.
- **Compiler today.** A supplied function outside the fixed list answers "does not write", as above.

### A `crash` that folds false is a compile error

- **Status.** Built.
- **Proves.** A `crash` in code the program reaches would halt on every run.
- **Rule.** An `assert` or `crash` side (split on `and`) that asks only codegen questions folds. Holding, it writes
  nothing; failing, a `crash` in a function that survives [tree shaking](#tree-shaking-what-main-can-reach) is an
  error naming the instance, and a failed `assert` returns its default with the rest of the block not compiled.
- **Buys.** A certain halt becomes a build error; a check that holds costs nothing.
- **Falls back.** A folded `crash` in an instance nothing calls is left alone; a condition that also reads a run-time
  value through `or` is a run-time check.
- **See.** D250; [metaprogramming.md: Codegen values](metaprogramming.md#codegen-values---implemented);
  `diagnostics/folded_crash`, `conformance/stage6/folded_crash_uncalled`.

### Tree shaking: what `main` can reach

- **Status.** Built.
- **Proves.** A piece of the generated C cannot be reached from `main`.
- **Rule.** After generating, the compiler keeps `main`, every piece that is not a named function, type or static,
  and everything any kept piece names, transitively (`tree_shaker.spite`). A dispatcher over a union or `type` names
  every member's function, so all of them stay when it does; a function made into a value is kept.
- **Buys.** Only what is used is in the program: `examples/hello` from 5 130 to 1 332 lines of C; a native symbol is
  looked up only if a kept function calls it.
- **Falls back.** An inspectable build (`--repl`, `--repl-port`, `--hot-reload`, `--development`) keeps everything.
  A function nobody calls is still compiled and checked (D140). The match is by name, so it keeps too much, never
  too little.
- **See.** D140, D143, D177; [optimizations.md: Tree shaking the generated
  C](optimizations.md#tree-shaking-the-generated-c), [metaprogramming.md: Tree
  shaking](metaprogramming.md#tree-shaking); `conformance/stage6/development_internals`.

### A reload compiles only the changed classes

- **Status.** Built.
- **Proves.** Compiling the changed classes against what the running program was compiled with writes the same code
  a whole compile would.
- **Rule.** A `--hot-reload` build records in its manifest every fact compiling one class took from another: the
  functions that wait, the call effects and writes of every function, which classes fit each shape, the attributes
  and words read, the key kinds, the template instances and the functions made on demand, with a hash of every
  piece of C. A reload replays them, compiles the changed files' classes, and compares: a recorded fact that now
  differs, a function of another class whose C would change, or a changed signature of a changed class sends the
  reload to the whole compile, which says why on the error output.
- **Buys.** A changed system of SlopTheseus's server swaps in about 6 seconds instead of 20 to 30.
- **Falls back.** The whole compile. `SPITE_RELOAD_CHECK` compiles both ways and compares them, and `check.sh` runs
  it on every file of several programs; a fact the generator starts consulting across classes must be recorded too.
- **See.** D111, D211, D244; [repl.md: How it works](repl.md#how-it-works), [optimizations.md: A reload compiles
  only the classes that changed](optimizations.md#a-reload-compiles-only-the-classes-that-changed).

## Memory

### A buffer freed in its block lives in the frame

- **Status.** Built.
- **Proves.** An address from `allocate(n)` never outlives its block.
- **Rule.** `var name = heap.allocate(size)` whose block later calls `heap.free(name)` at its own level, with every
  use in between an address primitive, a `TypedMemory` read or write, or a call to a function of the same class that
  provably keeps the parameter; neither name is assigned again.
- **Buys.** No allocation: a slot of the frame, and `free` does nothing.
- **Falls back.** The heap, for a size over 256 bytes known only at run time, any other use, a recursive call, or a
  `--hot-reload` build.
- **See.** D108, D151, D211; [memory.md: Placement](memory.md#placement-the-compiler-decides-where-memory-lives--implemented-the-rule-proposed-by-claude-unconfirmed);
  `conformance/stage6/lent_buffers`.
- **Compiler today.** The compiler accepts `read_value`, `write_value`, `release_value` and `swap_values` by name on
  any receiver, not only `TypedMemory`'s, so a program's own `write_value` that keeps the address would pass. Still
  open in [failure.md](failure.md#nothing-fails-silently--the-rule).

### Objects that never leave their function live in the frame

- **Status.** Built.
- **Proves.** A fresh object of a class of numbers never outlives the call that made it.
- **Rule.** The class's attributes are all numbers, `Boolean`s, enums or singletons; it has no `drop()`, is not a
  singleton, its constructor keeps nothing and nothing reads its `.instances`. Four places qualify: a **local** only
  read and written through its attributes, compared, passed to functions proven to keep nothing, or returned from a
  function answering its class; a **result** of a function whose every `return` is fresh, written into the caller's
  slot through a hidden `___into` version; a **temporary** (`(a + b).length()`); and a **copy used as a value**
  (`var local = other.copy()`). "Keeps nothing" is proven per parameter from the function's source.
- **Buys.** No allocation; `.memory.section` answers `'stack'`. `benchmarks/game_maths` from 3 200 046 allocations
  to 37.
- **Falls back.** An ordinary heap object when it is stored, returned as another type, given a second name, passed to
  a function that keeps it (a foreign or built-in function, a recursive call, a variadic list, a `Parallel` or
  `Concurrent`), made into a function value, or named in a text hole other than `{name.attribute}`; in inspectable
  builds; in a function that waits; and for a class holding text, lists or objects (planned).
- **See.** D108, D149; [optimizations.md: Objects that never leave their function live in the
  frame](optimizations.md#objects-that-never-leave-their-function-live-in-the-frame); `conformance/stage6/frame_objects`.

### Local and variadic lists in the frame

- **Status.** Built.
- **Proves.** A list is only read after it is filled, or only read by the function it is handed to.
- **Rule.** A local list made by a literal or `List<T>()` and filled by `append` statements at its own level, then only
  read, lives in the frame (16 items) or in constant data (256). A variadic list whose callee only reads it
  (`count`, `[]`, `get_at`, `first`, `last`, `contains`, `join`, ...), in a call that is a statement of its own, lives
  in the caller's frame.
- **Buys.** Two allocations fewer per list.
- **Falls back.** The heap, when the list is stored, returned, changed after filling, handed to a template, asked for
  `.memory`, or in an inspectable build or a function that waits.
- **See.** D211, D222; [optimizations.md: A local list of known size lives in the
  frame](optimizations.md#a-local-list-of-known-size-lives-in-the-frame),
  [A variadic list the callee only reads](optimizations.md#a-variadic-list-the-callee-only-reads-lives-in-the-callers-frame);
  `conformance/stage6/text_building`.

## Borrowing and lending

A borrowed item is the address of an item inside a collection: never retained, never released. Every proof in this
section keeps one fact true: a borrowed item is never kept past its use, and never read after its collection may
have moved.

### Borrowed items of a `Vector` or `Items`

- **Status.** Built.
- **Proves.** An item read from a `Vector`, or an `Items` whose class [fits a `Vector`](#whether-a-class-fits-a-vector),
  is used only while it is borrowed.
- **Rule.** `vector[index]`, once narrowed, and the item a template visits are borrowed. A borrowed name is not kept
  in an attribute or a list, returned, made into a function value, given a second name or assigned again. Plain values are never items of a `Vector` or `Items` (D225 folded them into `List`). Only `library/`'s own `Vector`, `Items` and `InlineMemory` are
  trusted; a program reopening them is checked.
- **Buys.** No count and no copy: `layout.order = 3` writes the stored item.
- **Falls back.** Each way of keeping one is an error naming `copy()`, an independent object.
- **See.** D204, D218, D221; [memory.md: Borrowed items of a
  `Vector<T>`](memory.md#borrowed-items-of-a-vectort--implemented); `diagnostics/vector_borrows`,
  `conformance/stage6/vector_items`.

### No borrowed read past a resize

- **Status.** Built.
- **Proves.** Nothing between taking a borrowed item and reading it can move the collection's items.
- **Rule.** A borrowed name is not read after a statement that may grow or shrink its collection: `append`,
  `prepend`, `insert`, `reserve`, a removal, `truncate`, `swap`, `clear`, assigning the collection or its path, or a
  call whose effects may do one of those. A call is followed only into what it can run (D262): a collection read with
  `[]` from an attribute is that attribute's element, and a call through a `type` reaches only the classes it is
  handed. A resizing statement anywhere in a loop ends the borrow for the whole loop.
- **Buys.** A borrowed item cannot dangle.
- **Falls back.** Refused, on the resizing line: read the item again after it, or keep a `copy()`. A value the
  compiler cannot place (a nullable, a union, a call's result, a reassigned parameter) is followed into every
  function of that name, and a function value may do anything.
- **See.** D204, D206, D262, D263; [memory.md: Borrowed items of a
  `Vector<T>`](memory.md#borrowed-items-of-a-vectort--implemented); `diagnostics/dispatched_resizes`,
  `diagnostics/vector_reserve_borrows`, `conformance/stage6/queued_borrows`.

### Rows of borrowed items

- **Status.** Built.
- **Proves.** A row -- an object literal holding borrowed items, or a `type` local filled by a walk -- lives exactly
  as long as one call.
- **Rule.** A row is passed only to a function called by name whose parameter is a `type`, which gets a
  `___lent_<positions>` copy that never counts it; it is never kept, returned, listed, aliased or assigned; the call
  may not resize what it borrows from. A walked row (D212) may take each attribute from a generic singleton per
  attribute class and mix borrowed items with other values (D217); a plural's arguments may carry borrowed items into
  the one call it fills (D220).
- **Buys.** The row is a struct in the frame and its reads are uncounted: `benchmarks/sparse_rows` 7.4 ms a tick
  against 24.0 ms.
- **Falls back.** Refused as above; a walk of any other shape leaves an ordinary `type` value, and a counted value
  in a walked row is counted once for the row's block.
- **See.** D206, D212, D217, D220; [memory.md: A row of borrowed items, for one
  call](memory.md#a-row-of-borrowed-items-for-one-call), [Borrowed arguments, for one
  call](memory.md#borrowed-arguments-for-one-call); `conformance/stage6/vector_rows`, `conformance/stage6/sparse_rows`,
  `conformance/stage6/lent_arguments`, `diagnostics/walked_rows`.

### An item lent to the caller

- **Status.** Built.
- **Proves.** A function's result is an item of a singleton's `Vector` or `Items` storage.
- **Rule.** A `return` that reads an item straight from an attribute path of the function's class ending at a
  singleton's `Vector` or fitting `Items` lends its result, decided per instance with compile-time conditions folded
  (`fits_vector`, a literal `has_function`, `$T == X`; not `function_waits`). Every return is such an item or `null`.
  At the caller the result is borrowed under every rule above, narrowing it keeps it borrowed (D259), and it is not
  returned further. A lending function is only called by name, never made a function value or used through a `type`.
- **Buys.** The caller gets the address: no retain, no release.
- **Falls back.** An owned result for an instance whose class does not fit; storage through a local, a parameter or
  an object that is not a singleton is refused as returning a borrowed item.
- **See.** D230, D259; [memory.md: An item lent to the caller](memory.md#an-item-lent-to-the-caller);
  `conformance/stage6/lent_results`, `conformance/stage6/narrowed_lends`, `conformance/stage6/folded_lends`,
  `diagnostics/lent_escapes`.

### An item lent to a call

- **Status.** Built.
- **Proves.** A function handed a borrowed item keeps nothing of it.
- **Rule.** A borrowed name, an item read with `[]` or a row's attribute, passed to a program function called by name
  at a place that is not a union or variadic list, calls its `___lent_<positions>` copy, compiled under the borrow
  rules, so any way the function would keep the item is an error in that function naming the lending call. The call
  may not resize what the item is borrowed from.
- **Buys.** The argument is the item's address, never counted or copied.
- **Falls back.** Not lent to a library function, a union parameter or an unnamed expression: each is an error that
  says why and never offers a copy the function would write (a write to a copy vanishes).
- **See.** D257; [memory.md: An item lent to a call](memory.md#an-item-lent-to-a-call);
  `conformance/stage6/lent_to_calls`, `diagnostics/lent_to_calls`.

### A list element lent to a row

- **Status.** Built.
- **Proves.** An element of a singleton's `List` read into a row cannot be let go while the row's block runs.
- **Rule.** The row's line calls a singleton function whose body is only `crash <list>[<index>]` then
  `return <list>[<index>]` (D225), the list an attribute nothing assigns after the singleton is made, the
  index a whole-number parameter or literal; the rest of the block names every call it makes, none of which lets go
  of an object, removes from a list, assigns an attribute holding an object or calls a function value; and no class
  whose objects may be let go meanwhile has a `drop()` reaching that singleton or list. With threads, the block takes
  the singleton's readers' side or lock once, only when what it runs reaches no singleton and no wait.
- **Buys.** One retain and one release per row, and the lock per call.
- **Falls back.** The ordinary counted call; nothing is an error.
- **See.** D265, D266, D269; [memory.md: Borrowed items of a
  `Vector<T>`](memory.md#borrowed-items-of-a-vectort--implemented) (a reference column's element);
  `conformance/stage6/lent_list_elements`, `conformance/stage6/lent_list_elements_parallel`.

### An argument its caller holds is passed uncounted

- **Status.** Built.
- **Proves.** The caller holds an argument for the whole call, so counting it for the callee adds nothing.
- **Rule.** The argument is a bare name, the caller's own parameter or a local it owns; the callee is a program
  function with a body, called by name, not a constructor, that never assigns that parameter; the parameter is a
  class, list or dictionary (not text, a union or variadic); the call passes every parameter. The call goes to
  `<name>___held_<positions>`, which does not let those parameters go.
- **Buys.** One retain and one release per argument, atomic with threads: 18 to 13 ns an entity in
  `benchmarks/held_arguments`.
- **Falls back.** The counted call: in `--hot-reload`, `--repl`, `--repl-port` and `--development` builds, the
  resumable copy of a function a `Concurrent` runs, a library function, and a call whose result is made in the
  caller's frame.
- **See.** D270; [optimizations.md: An argument its caller holds is passed without
  counting](optimizations.md#an-argument-its-caller-holds-is-passed-without-counting);
  `conformance/stage6/held_arguments`.

### A singleton attribute that never changes is read in place

- **Status.** Built.
- **Proves.** An object attribute of a singleton stays the singleton's for the rest of the program.
- **Rule.** No function of the singleton outside its constructor and no other class assigns the attribute (from the
  whole program's call effects); it holds a class, list or dictionary (or a `T?` of one), not text, a number or a
  function value; it has no getter.
- **Buys.** Read from another class, it is the attribute's address: no count and no lock. 6.0 to 3.2 ns a read.
- **Falls back.** The counted read, under the singleton's lock when a `Parallel` reaches it; in `--hot-reload`,
  `--repl` and `--repl-port` builds.
- **See.** D211, D271; [optimizations.md: A singleton's attribute that never changes is read in
  place](optimizations.md#a-singletons-attribute-that-never-changes-is-read-in-place);
  `conformance/stage6/singleton_attribute_reads`.

### A list's templates read their elements uncounted

- **Status.** Partial: a known hole.
- **Proves.** Nothing in the rest of a template's pass over one element can let that element go.
- **Rule.** In `List` templates, the reference kind of `Items` and fused chains, an element is read uncounted when
  the rest of its pass only assigns locals and makes calls the compiler can name, none of which may let go of an
  object or assign an attribute through an unknown class.
- **Buys.** One retain and one release per element (`benchmarks/fused_chain` 332 to 185 ms).
- **Falls back.** The counted read, in `--hot-reload` builds and for a nullable element.
- **See.** D222; [optimizations.md: A list's templates read its elements without counting
  them](optimizations.md#a-lists-templates-read-its-elements-without-counting-them);
  `conformance/stage6/fused_chain_allocations`.
- **Compiler today.** The proof does not consider the `drop()` functions that run when an object is let go, so a
  `drop()` that removes from the list being walked could free a lent element in use. D269's proof checks `drop()`;
  this one should. Listed as still open in [failure.md](failure.md#nothing-fails-silently--the-rule).

## Threads and locks

These apply only in a program that makes a `Parallel`, runs a `parallel_each_` pass or makes a `ForeignCallback`
(D234); any other program has no lock at all.

### Which singletons a `Parallel` reaches

- **Status.** Built.
- **Proves.** No thread but the program's own calls a singleton.
- **Rule.** The compiler walks the calls from every function a `Parallel` or the thread pool can run -- every
  function made into a value, and the members and filters of every `parallel_each_` pass -- following a call on an
  unknown class into every function of that name. A singleton with an operator, getter, setter, `get_at`/`set_at`,
  `missing_function`, `to_string` or `to_debug` is always counted as reached.
- **Buys.** No lock for a singleton only the program's thread uses.
- **Falls back.** A reached singleton takes one of the forms below, the lock last.
- **See.** D183, D184; [optimizations.md: Thread safety for singletons, the cheapest safe
  form](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form),
  [concurrency.md: A value per thread, and a lock](concurrency.md#a-value-per-thread-and-a-lock);
  `conformance/stage6/singleton_forms`.

### Read-only and atomic singletons

- **Status.** Built.
- **Proves.** A singleton never changes after it is made, or each of its functions touches its changing state once.
- **Rule.** Read-only: no function assigns an attribute outside the constructor, no other class assigns one, and it
  holds no list, dictionary, function value or object that can change. Atomic: every changing attribute is a whole
  number or `Boolean`, and each function touches it at most once (one read, one `count = count + step`, or one
  store), not in a loop.
- **Buys.** No lock: plain reads, or single atomic instructions.
- **Falls back.** The lock.
- **See.** D183, D184; [optimizations.md: Thread safety for singletons, the cheapest safe
  form](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form); `conformance/stage6/singleton_forms`,
  `conformance/stage6/singleton_lock_calls`.

### A function that touches no changing state takes no lock

- **Status.** Built.
- **Proves.** A function of a locked singleton reads nothing that can change, or the lock is already held.
- **Rule.** A function that reads or writes no changing attribute, and calls nothing that does, runs unlocked; a
  call from inside a locked function to another of the same singleton goes to its unlocked body.
- **Buys.** Fewer atomic operations, and one deadlock shape gone.
- **Falls back.** The lock; a function value of a singleton's function always goes through the locked version.
- **See.** D183, D211; [optimizations.md: Singletons a `Parallel` reaches take a
  lock](optimizations.md#singletons-a-parallel-reaches-take-a-lock); `conformance/stage6/singleton_stateless_calls`,
  `conformance/stage6/singleton_function_values`.

### Reading functions share the lock

- **Status.** Built.
- **Proves.** A locked singleton's function changes nothing, and no function that writes it runs on the pool.
- **Rule.** Its statements assign only its own locals; it calls only reading functions of its class, reading
  members of lists and dictionaries, text and number functions but writes, and reading functions of stateless
  singletons; its text has no holes; its operators are on numbers. And none of the singleton's writing functions is
  reachable from what a `Parallel` runs.
- **Buys.** Readers take a count on their own cache line and never contend: 202 to 6 ns a row.
- **Falls back.** Anything else is a writing function; a singleton also written from the pool keeps the plain lock.
- **See.** D266; [optimizations.md: A singleton's reading functions do not exclude each
  other](optimizations.md#a-singletons-reading-functions-do-not-exclude-each-other); `conformance/stage6/singleton_reads`.

### A counted loop takes a singleton's lock once

- **Status.** Built.
- **Proves.** Holding a singleton's lock for a whole loop can neither deadlock nor wait.
- **Rule.** A `while` outside the singleton that is counted (`index < bound`, stepped last), cannot `return`, calls
  only that singleton's functions and plain-value list functions, computes only numbers, `Boolean`s, hole-free text
  and enum values, and whose calls reach no wait and no `Parallel`, `Concurrent`, `ThreadPool`, `Scheduler`, `Lock`,
  `Program`, `Console`, `File` or `Socket`; at least one call takes the lock.
- **Buys.** One lock for the loop: 51 to 0.6 ms with one singleton shared by eight workers.
- **Falls back.** A lock per call; in `--hot-reload`, `--repl` and `--repl-port` builds and a `Concurrent`'s
  resumable copy.
- **See.** D265; [optimizations.md: A counted loop of calls to one singleton takes its lock
  once](optimizations.md#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once); `conformance/stage6/coarse_locks`.

While no task is on the thread pool, a locked function also skips its lock (D267). That is a run-time test of a
counter, one load per call, not a proof: [optimizations.md](optimizations.md#while-no-task-runs-a-singletons-lock-is-skipped).

### A lock that would wait forever is an error

- **Status.** Partial: refused where the compiler can prove it.
- **Proves.** A locked function would hold its lock forever.
- **Rule.** A locked function of a singleton that makes a `Parallel` whose work reaches a locked function of the same
  singleton, and waits for it, is an error (D264); so is a `while true` that cannot leave inside a locked function
  (D211).
- **Buys.** A hang becomes a build error.
- **Falls back.** Not seen: a handle that escapes (stored, switched on, put in a literal), work reached through a
  function value, a union or an unknown receiver, a `parallel_each_` pass, and generic singletons. Do not wait inside
  a locked function for work that calls back into it.
- **See.** D211, D264; [concurrency.md: Rules in full](concurrency.md#rules-in-full); `diagnostics/locked_wait`,
  `diagnostics/endless_locked_loop`.

### What a `Parallel` may reach

- **Status.** Built.
- **Proves.** Work run on another thread touches nothing another thread may touch at the same time.
- **Rule.** A `Parallel(function)` and a `parallel_each_` member may reach only their own instance's plain values,
  their locals and singletons (which the compiler makes safe); an object handed over and held only by the task may
  keep its lists and objects (D207); a `Weak` a `Parallel` reaches is an error.
- **Buys.** A data race is a compile error.
- **Falls back.** Refused, naming what the other thread could reach.
- **See.** D35, D179, D207; [concurrency.md: A value per thread, and a
  lock](concurrency.md#a-value-per-thread-and-a-lock), [`parallel_each_`: a member on every
  element](concurrency.md#parallel_each_-a-member-on-every-element); `diagnostics/parallel_reach`,
  `diagnostics/parallel_function_reach`, `diagnostics/parallel_handover`, `diagnostics/weak_across_threads`.

### Which calls suspend a `Concurrent`

- **Status.** Built on Windows.
- **Proves.** Which functions a `Concurrent` can pause inside.
- **Rule.** A function given to a `Concurrent` that can wait, and every function it calls by name that can wait, gets
  a resumable copy, a state machine; the rest of the program is unchanged. A program that makes no `Concurrent` has
  no machines, and its waits are plain blocking calls.
- **Buys.** Hidden async/await with no fibers and no runtime.
- **Falls back.** A wait reached through a function value, a union, a constructor, the right side of `and`/`or` or
  a comparison runs in place, holding its `Concurrent` while the others run.
- **See.** D35, D99, D176; [concurrency.md: What the compiler does at a
  wait](concurrency.md#what-the-compiler-does-at-a-wait),
  [optimizations.md](optimizations.md#hidden-asyncawait-as-compile-time-state-machines);
  `conformance/stage6/concurrent_waits`.

## Other refusals built on an analysis

Built. Each reads the program while compiling and accepts, speeds up or refuses code; each is taught in full on its
own page, and none has a fallback beyond the error it gives.

- **Unused is an error**: a name, parameter or attribute that is only written and never read. It judges the source,
  so reads inside a folded-away branch still count; a function nobody calls is not reported (D140), and a loaded
  package's attributes are shaken rather than reported (D118, D136, D137, D157;
  [style.md: Nothing unused](style.md#nothing-unused); `diagnostics/unused_names`, `diagnostics/written_not_read`).
- **A constructed object must be kept and used** (D139,
  [classes_and_files.md](classes_and_files.md#a-constructed-object-must-be-kept-and-used);
  `diagnostics/dropped_construction`).
- **Singletons that make each other in a circle** are an error; a circle only a constructor's body closes is not
  followed, and halts naming it at run time ([classes_and_files.md: Singletons](classes_and_files.md#singletons);
  `diagnostics/singleton_circle`, `conformance/stage6/singleton_circle`).
- **An allocator is set only on a fresh object**, on the line after it is made, so it is made there and never twice
  (D152, D153; [optimizations.md](optimizations.md#an-allocator-set-after-construction-is-where-the-object-is-made);
  `diagnostics/allocator_after_use`).
- **Build settings are constants**: a flag's value has its field's type, an unknown flag or an `Environment` setting
  given to the compiler is an error, and an `if` around a `load` is decided from `Build` fields and literals (D84,
  D85; [programs.md: Compile-time settings](programs.md#compile-time-settings-build); `diagnostics/flag_value`,
  `diagnostics/load_unknown_condition`).
- **A value fits a `type`**, and a `$T` its constraint: attributes, required functions, return types, not-null, so a
  call through a `type` tests only the classes the program admits (D16;
  [values_and_types.md: Inline types and duck typing](values_and_types.md#inline-types-and-duck-typing);
  `diagnostics/shape_mismatch`, `diagnostics/generic_constraints`).
- **A member template fits**: the member exists and has the right kind (a `Boolean` for `filter_`, a number for
  `sum_`), and a passed function takes the element; only called templates are generated, and a function passed by
  name is called directly (D15, D148; [collections.md: Member templates](collections.md#member-templates-loops-you-do-not-write);
  `diagnostics/member_template_mistakes`, `diagnostics/passed_functions`).
- **A value can be written**: the `T` of a `JsonWriter`, `JsonReader`, `BinaryWriter` or `BinaryReader` holds no union,
  `type` or function value anywhere, so writing never fails at run time, and a binary schema is a constant (D13, D22, D208,
  D215; [json.md](json.md#json-is-reflection-not-a-library--implemented); `diagnostics/json_unwritable`).
- **What crosses into C**: numbers, `Boolean`, enums, text, lists of numbers, number-only `type`s and function values;
  a callback's parameters and context are checked (D231, D233, D234;
  [foreign_libraries.md: What crosses](foreign_libraries.md#what-crosses); `diagnostics/foreign_call_mistakes`,
  `diagnostics/foreign_callback_mistakes`).
- **Reads in a row are independent**: consecutive `var x = file.read()` lines that do not name each other's results
  run at once (D134; [concurrency.md: Reads in a row overlap](concurrency.md#reads-in-a-row-overlap)).
- **The short form is proven the same, and the long form refused**: a `while` a member template already says, a chain
  of `if`s that is a `switch`, the same call opening every branch, a switch that is a class test, a text that is one
  hole (D60, D75, D171, D173, D239; [style.md: The short form is the only form](style.md#the-short-form-is-the-only-form);
  `diagnostics/walk_with_function`, `diagnostics/repeated_branch_call`, `diagnostics/lone_hole`).

Not proven anywhere, so write the check or accept the run-time report: how deep recursion goes (a stack overflow is
a [native fault report](failure.md#what-a-native-fault-reports)); unreachable code after a `return`; a reference
cycle, which leaks ([memory.md: Cycles leak](memory.md#cycles-leak)); a wider value assigned, passed or returned
into a narrower name, which wraps; and two threads writing one number attribute of an instance they share.

## Planned proofs

- **A foreign function's status is handled while compiling** (D272, decided by Mortaro; the design proposed by
  Claude, unconfirmed): a C enum result becomes a Spite enum that must be switched over. Not built: today a call
  answers an `Integer` and `crash result == 0` compiles
  ([foreign_libraries.md](foreign_libraries.md#foreign-libraries--partial)).
- **An overflow check left out where a proof bounds the operands**
  ([optimizations.md](optimizations.md#signed-arithmetic-is-checked-only-while-developing)).
- **A write to a copy that dies unread is an error** (proposed by Claude, unconfirmed): escape analysis already
  proves a result fresh ([failure.md](failure.md#nothing-fails-silently--the-rule), still open).
- **Frame objects holding text, lists or objects**, their attributes let go at the end of the frame
  ([optimizations.md: Copies that cost nothing](optimizations.md#copies-that-cost-nothing)).
- **A singleton hands out only safe values** (D183's check at `return`), and D184's per-thread forms
  ([optimizations.md](optimizations.md#thread-safety-for-singletons-the-rest-of-the-plan)).
- **Whether a function runs in pieces** (D229, `function_runs_in_pieces`): decided, and on no page and not in the
  compiler.
- **A read overlaps the statements after it until its name is used** (D211, item 149): decided, not built.
- **A class compiled for the wrong target** (D20): planned ([targets.md](targets.md)).

## Adding a proof

A change that makes the compiler prove something new, or changes what an existing proof accepts, drops or refuses,
updates this page in the same commit ([D276](decisions.md)): the entry, its fallback, its status, and the program
that shows it. The rule itself goes on the page that teaches that part of the language, and a proof that is also an
optimisation is described on [optimizations.md](optimizations.md) too (D185). A proof that contradicts a rule on
another page is recorded in `mortaros_missing_decisions.md` for Mortaro.
