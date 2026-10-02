# Proofs the compiler makes

A **proof** is a fact the compiler establishes while compiling: that a `T?` holds a value here, that an index is in
range, that an object never leaves its function, that no other thread can reach a singleton. With the fact in hand
the compiler does one of four things: it **accepts** code it would otherwise refuse, it **drops** a check it would
otherwise emit, it **chooses cheaper code**, or it **refuses** code that would go wrong. Every proof is worked out
while compiling and emits nothing of its own; what a proof costs, if anything, is the check it leaves in place when
it does not hold.

This page is the catalogue of every such proof. It is a catalogue, not a second home for the rules: each entry
says in a few lines what the proof is, and links the page that teaches it and states it in full. Where an entry
and that page differ, the page decides.

**Why the fallback matters most.** A proof that does not apply never fails silently: either
the compiler refuses the code and says what to write, or it keeps the run-time check. So each entry ends with
**Falls back**: what happens when the proof does not hold, and what you write yourself. That is the part to read
before relying on a proof.

Each entry has the same parts:

- **Proves**: the fact.
- **Rule**: how the compiler decides it, stated so you can predict it.
- **Buys**: the check removed, the code accepted or refused, the cost saved.
- **Falls back**: when it does not apply, and what you write instead.
- **See**: the page that teaches it, and a program in `conformance/` or `diagnostics/` that shows it.

## Which proofs apply to my code

A short guide by task. Find what you are writing; the entries below say the rest.

- **Reading a `T?`** (a `Monster?`, a `[]` read, `first()`, a number read from text): narrow the name or the path
  itself with `if`, `assert`, `crash`, `while` or `switch`, or leave early with `if not value { return ... }`.
  [Narrowing](#narrowing-a-t), [an `if` that leaves](#an-if-that-leaves-proves-the-rest). A condition with a call in
  it narrows nothing: put the result in a `var` and narrow that.
- **A call between your check and your read**: the proof survives unless the call may assign what it reads or
  shrink a list it reads. After a call through a function value, prove it again.
  [A call keeps a proof](#a-call-keeps-a-proof-it-cannot-change).
- **Reading `list[index]`**: a bound (`index < list.count()`, or `at + 2 < list.count()` for `list[at]` to
  `list[at + 2]`), a count (`list.count() >= 3`), a count kept in a `var`, or `not list.is_empty()` proves the read.
  A `Dictionary` read is proven only by checking that key.
  [Proven reads](#a-proven-count-or-bound-proves-a-read).
- **A tight loop over numbers**: write `while index < values.count()` with `index = index + 1` last, over a local
  `List` of plain values, and call nothing but the list's reading functions and maths.
  [Counted loops](#a-counted-loop-reads-its-items-unchecked).
- **Dividing whole numbers**: prove the divisor with `!= 0` or `> 0` against a written `0`, or divide by a
  constant. [Proven divisor](#a-proven-divisor-is-not-checked).
- **Arithmetic that could overflow**: every build checks it; step a local counter by one under a `<` (or down under
  a `>`) and that step carries no check; write the wider type first, or call `wrapping_sum`, `wrapping_subtract` or
  `wrapping_multiply` where wrapping is the point.
  [Overflow](#arithmetic-that-does-not-fit-halts).
- **A function that answers something**: end every path with `return` or a bare `crash`; use a guard `assert` only
  in a function whose result can say "nothing". [Every path ends](#every-path-ends-in-a-return),
  [guard `assert`](#a-guard-assert-answers-only-nothing).
- **A `switch`**: cover every member or value, or end with `_:`. [Switches](#a-switch-covers-every-case).
- **Generic code asking about `$T`**: `functions['run_each']`, `.is_resumable`, `.is_fixed_size`, `.arguments.count()`,
  `.is_mutated` fold; only the branch taken is compiled, and a `crash` that folds false in reached code
  is a compile error. [Compile-time questions](#compile-time-questions).
- **Small objects of numbers** (`Vector3`, a `Velocity`): make them, pass them to functions that keep nothing, and
  they live in the frame. [Frame objects](#objects-that-never-leave-their-function-live-in-the-frame).
- **`Vector`/`Items` items, rows, lent results**: a borrowed item is never kept and never read past a resize.
  [Borrowing and lending](#borrowing-and-lending).
- **Singletons with `Parallel`**: write nothing; the compiler picks the lock. Do not wait inside a locked function
  for work that calls back into it. [Threads](#threads-and-locks).

## At a glance

| Proof | Buys | When it does not hold |
|---|---|---|
| [Narrowing a `T?`](#narrowing-a-t) | reads through a `T?` accepted | refused until narrowed |
| [Every side of an `and` narrows](#every-side-of-an-and-narrows) | one line proves several reads | in a `while` condition or an expression: refused |
| [An `if` that leaves proves the rest](#an-if-that-leaves-proves-the-rest) | guards read as answers | refused until narrowed |
| [Assigning undoes a proof](#assigning-undoes-a-proof) | nothing stale is read | prove it again |
| [A call keeps a proof it cannot change](#a-call-keeps-a-proof-it-cannot-change) | no re-check after harmless calls | prove it again; in a loop, an error |
| [Proving what is proven is an error](#proving-what-is-proven-is-an-error) | refuses dead checks | none |
| [A copy made to narrow is an error](#a-copy-made-to-narrow-is-an-error) | refuses the copy | none |
| [A class test narrows its block](#a-class-test-narrows-its-block) | union member access | use `switch` |
| [Every path ends in a `return`](#every-path-ends-in-a-return) | refuses a made-up answer | none |
| [A guard `assert` answers only "nothing"](#a-guard-assert-answers-only-nothing) | refuses a silent default | write the answer, a `T?` or `crash` |
| [A switch covers every case](#a-switch-covers-every-case) | refuses a forgotten case | add the case or `_:` |
| [A proven count or bound proves a read](#a-proven-count-or-bound-proves-a-read) | `list[i]` read without narrowing | refused until proven, naming how |
| [A proven read halts outside its list](#a-proven-read-halts-outside-its-list) | a stray counter stops the program | none |
| [A counted loop reads its items unchecked](#a-counted-loop-reads-its-items-unchecked) | no range checks, vectorisable | the ordinary checked loop |
| [A proven divisor is not checked](#a-proven-divisor-is-not-checked) | no zero check | the zero check |
| [A divisor written as zero is an error](#a-divisor-written-as-zero-is-an-error) | refuses a certain halt | none |
| [Arithmetic that does not fit halts](#arithmetic-that-does-not-fit-halts) | a counter's step and constants unchecked | the overflow check, in every build |
| [A wider operand is written first](#a-wider-operand-is-written-first) | refuses a silent cut | write the wider side first |
| [A dictionary's key kind](#a-dictionarys-key-kind-is-decided-while-compiling) | numbers hashed as numbers | text keys; mixing is an error |
| [Maths on constants](#maths-on-constants-is-worked-out-while-compiling) | no call | the call |
| [Conditions decided while compiling](#conditions-decided-while-compiling) | the branch not taken is absent | a run-time test |
| [Whether a function waits](#whether-a-function-waits) | engines branch while compiling | `true` where unsure |
| [How many arguments a function takes](#how-many-arguments-a-function-takes) | one branch per arity | none |
| [Whether a class fits a `Vector`](#whether-a-class-fits-a-vector) | inline storage | reference storage |
| [Whether a function writes a parameter](#whether-a-function-writes-a-parameter) | engines refuse lost writes | `true` where unsure |
| [Whether a class keeps state](#whether-a-class-keeps-state) | engines refuse stateful systems | `true` where unsure |
| [A `crash` that folds false is an error](#a-crash-that-folds-false-is-a-compile-error) | a certain halt becomes a build error | a run-time check |
| [Tree shaking](#tree-shaking-what-main-can-reach) | smaller program | an inspectable build keeps all |
| [A reload compiles only the changed classes](#a-reload-compiles-only-the-changed-classes) | a reload in seconds | the whole program is compiled |
| [A reload reaches every object whose attributes it changes](#a-reload-reaches-every-object-whose-attributes-it-changes) | attributes change live | refused by name |
| [Buffers in the frame](#a-buffer-freed-in-its-block-lives-in-the-frame) | no allocation | the heap |
| [Frame objects](#objects-that-never-leave-their-function-live-in-the-frame) | no allocation | the heap |
| [Local and variadic lists in the frame](#local-and-variadic-lists-in-the-frame) | no allocation | the heap |
| [Borrowed items of a `Vector` or `Items`](#borrowed-items-of-a-vector-or-items) | no count, no copy | refused, naming `copy()` |
| [No read past a resize](#no-borrowed-read-past-a-resize) | the item cannot move under you | refused |
| [Rows and walked rows](#rows-of-borrowed-items) | a row in the frame | refused, or an ordinary `type` value |
| [Lent results](#an-item-lent-to-the-caller) | no count on the result | an owned result, or refused |
| [Lent arguments](#an-item-lent-to-a-call) | no count on the argument | refused, saying why |
| [A list element lent to a row](#a-list-element-lent-to-a-row) | no count, one lock per block | the counted call |
| [Held arguments](#an-argument-its-caller-holds-is-passed-uncounted) | no count on the argument | the counted call |
| [A singleton attribute that never changes](#a-singleton-attribute-that-never-changes-is-read-in-place) | no count, no lock | the counted, locked read |
| [List templates read uncounted](#a-lists-templates-read-their-elements-uncounted) | no count per element | the counted read |
| [Which singletons a `Parallel` reaches](#which-singletons-a-parallel-reaches) | no lock | a lock |
| [Read-only and atomic singletons](#read-only-and-atomic-singletons) | no lock | a lock |
| [A function that touches no changing state](#a-function-that-touches-no-changing-state-takes-no-lock) | no lock for it | the lock |
| [Reading functions share the lock](#reading-functions-share-the-lock) | readers never contend | the plain lock |
| [A counted loop takes the lock once](#a-counted-loop-takes-a-singletons-lock-once) | one lock per loop | one lock per call |
| [A lock that would wait forever](#a-lock-that-would-wait-forever-is-an-error) | refuses a hang | none |
| [What a `Parallel` may reach](#what-a-parallel-may-reach) | refuses a data race | none |
| [Which calls suspend](#which-calls-suspend-a-concurrent) | state machines, no fibers | the wait runs in place |
| [Other refusals](#other-refusals-built-on-an-analysis) | refuses dead code and leaks | none |

## Presence and control flow

### Narrowing a `T?`

- **Proves.** A `T?` name or path holds a value, so it is a plain `T` in place, the same name.
- **Rule.** The condition must be exactly a name or a path: names, `.member` and `[index]` links with no call in
  them (`operand_text`, `try_nullable_guard`). `if value { }` narrows the block, and its `else` nothing; `assert value`
  and `crash value` narrow the rest of the block and every block nested in it; `while value { }` narrows the body
  and is tested again every pass; `switch value { T: ... Null: ... }` narrows the `T` case. One check narrows every
  link of a path and its prefixes, never a sibling path. Narrowing tests presence, never the value: a `0`,
  `false` or `""` that is there is present.
- **Buys.** Member reads, calls and stores through the value compile. Nothing is emitted beyond the test itself.
- **Falls back.** Reading, calling or storing through an unproven `T?` is refused: `this value may be null (it is a
  Monster?), so 'name' cannot be read from it yet: narrow it first with 'if value { }', 'assert value' or 'crash
  value'`. A condition with a call (`if find(name) { }`) is not a path: keep the result in a `var` and narrow the
  `var`. A `Boolean?` is a condition only in `assert`, where it means "there and `true`"; elsewhere compare it `== true` or
  `switch` over it. `value == null` is an error.
  What a branch narrows is gone after the branch: two branches' proofs are not merged.
- **See.** [failure.md: Narrowing](failure.md#narrowing),
  [Narrowing a path](failure.md#narrowing-a-path); `conformance/stage6/path_narrowing`,
  `conformance/stage6/present_zero`, `diagnostics/store_through_nullable`, `diagnostics/path_narrow_else`.

### Every side of an `and` narrows

- **Proves.** Each side of an `and` in an `if`, `assert` or `crash` holds where the whole condition holds.
- **Rule.** `assert` and `crash` split their `and` into one check per side; an `if` narrows each side into its
  block. A later side already sees the earlier sides narrowed, so `assert target and target.health > 0` is one line.
- **Buys.** `crash names[position] and ages[position]` proves both reads.
- **Falls back.** In a `while` condition and inside an ordinary expression (`return node and node.ready`), the right
  side of an `and` sees only proven *indices* from the left side, not narrowed `T?`s: such code is refused. Write
  `while node { if ... }`, or an `if`. `or` narrows only in [an `if` that leaves](#an-if-that-leaves-proves-the-rest).
- **See.** [failure.md: Narrowing](failure.md#narrowing); `conformance/stage6/and_narrowing`.

### An `if` that leaves proves the rest

- **Proves.** After an `if` with no `else` whose block leaves the function, the opposite of its condition holds for
  the rest of the enclosing block.
- **Rule.** The block leaves when its **last** statement is a `return`, a bare `crash`, or a `switch` whose every
  case leaves in the same sense. The condition is split into `or` sides: each side written `not path` narrows that
  path, the later sides seeing the earlier ones narrowed; every other side is turned around and proves the `[]`
  reads and divisors its opposite bounds (`if index >= names.count() { return "" }` proves `names[index]`). The
  block itself sees none of it.
- **Buys.** `if not found { return -1 }` followed by `found.length()` compiles, so explicit answers read as
  guards do. The C is the condition as written.
- **Falls back.** Nothing is proven after an `if` with an `else`, a block ending in `crash condition`, in an
  `if`/`else` whose branches both return, or in `while true`; a class test (`if creature != Fish { return }`)
  narrows nothing; a `not` side that is not a path (`not x.ready()`) proves nothing. Use `assert` or `crash`.
- **See.** [failure.md: An `if` that leaves proves the rest](failure.md#an-if-that-leaves-proves-the-rest);
  `conformance/stage6/leaving_if`, `conformance/stage6/returning_branch_proof`.

### Assigning undoes a proof

- **Proves.** A proof is still true at the read.
- **Rule.** Assigning a narrowed path, or anything it reads through (a name in a `[]` index included), undoes every
  proof beneath it from that line. Assigning the path itself a value that cannot be null keeps its own narrowing;
  "cannot be null" is a constructor, a literal, an arithmetic expression, or a name or path whose type is not `T?`.
  Inside a `while`, undoing a proof made before the loop and read in it is an error, since the next pass would read
  it unproven.
- **Buys.** A narrowing never outlives what it narrowed.
- **Falls back.** Prove it again after the assignment, or narrow inside the loop. Note that a call answering a plain
  `T` counts as "may be null" here: `box = make_box()` un-narrows `box` even when `make_box(): Box`.
- **See.** [failure.md: Narrowing a path](failure.md#narrowing-a-path); `diagnostics/narrowed_name_reassigned`,
  `diagnostics/path_narrow_assignment`, `diagnostics/path_narrow_loop`,
  `conformance/stage6/narrowed_value_assignment`.

### A call keeps a proof it cannot change

- **Proves.** A call between a check and a read cannot change what the check proved, so the proof stands without a
  `const` keyword.
- **Rule.** The compiler follows the called function and everything it calls (`call_effects.spite`,
  `effect_study.spite`) and collects the attributes it may assign, by class and name, and the lists it may shrink
  (`clear`, `remove_at`, `remove_first`, `remove_last`, `remove`, `remove_swapping`, `remove_where...`, `truncate`,
  `swap`). A proof that reads through one of them is undone. A class is known by its full name, resolved where it is
  written, so a program's `Stock.Items` and the library's `Items<T>` never share effects. The receiver's class comes
  from declared types, the class a constructor makes, the class a called function returns, a `[]` read's element
  class and an attribute's declared class; `List`, `Dictionary` and `String` reach only their own functions. Growing
  a list keeps an index proof. A `return`'s own calls undo nothing. Locals and parameters are never changed by a
  call.
- **Buys.** No re-check after a call that provably cannot change the value.
- **Falls back.** A call on a value whose class cannot be told (a `type`, a type parameter) is followed into every
  function of that name; a call through a function value undoes every proof about attributes and lists; a `clear` or
  `remove_...` on a list reached through `[]` or a call undoes every proof about a list; operators and getters are
  not followed; a local that aliases an attribute's list is not tracked. After such a call, prove it again; inside a
  loop it is an error naming the call: `'forget_target()' inside this loop may change 'target', which undoes what was
  proven about it before the loop ...`.
- **See.** [failure.md: A call may undo a proof](failure.md#a-call-may-undo-a-proof),
  [optimizations.md: Proofs that survive a call](optimizations.md#proofs-that-survive-a-call);
  `diagnostics/call_undoes_proof`, `diagnostics/receiver_call_undoes_proof`, `diagnostics/branch_undoes_proof`,
  `conformance/stage6/receiver_call_effects`, `conformance/stage6/same_name_call_effects`.

### Proving what is proven is an error

- **Proves.** A check adds nothing, because the value is already narrowed, the read or divisor already proven, the
  class already known or the condition already checked.
- **Rule.** Each is refused in an `assert`, `crash`, `if` or `while` condition, naming where it was proven:
  - A `[]` read a bound or count already proves: `'codes[index]' is already proven by the loop condition 'index <
    codes.count()', so this 'crash' proves nothing: remove it`.
  - A narrowed `T?` of any type (a class, `String`, number or enum) names the check that narrowed it: `'first' is
    already narrowed by 'crash first' on line 7: remove this check` (by `'if x'`, `'if not x'`, `'while x'`,
    `'assert x'`, `'crash x'`, `the 'if'` for a side of an `and`, or `the 'switch'`). A value that was never a
    `T?`: `this value cannot be null here (it is a Tracker), so 'assert' on it proves nothing: remove the check`.
  - A divisor already proven: `path != 0` after `path != 0` or `path > 0`, and `path > 0` after `path > 0`, is
    `'parts != 0' is already proven by 'parts > 0' on line 19: remove this check`.
  - A class test the value's type already answers, outside a generic class, a walk or a copy of a function made for
    one class that reaches a `type` (where it folds per instance or per copy): `'creature' is already a Monster, narrowed by 'if creature == Monster' on line 30: remove this test`,
    `'monster' is a Monster here, so this test is decided while compiling: remove it`, and for another class
    `'monster' is a Monster here, so it is never Ghost and this test is decided while compiling: remove it`.
  - The same `assert` or `crash` condition on the very next statement, when it calls nothing (a call could change
    what it reads): `'total > 2' was already checked on line 22, and nothing between could change it: remove this
    check`.
  - An `assert` or `crash` that [folds](#conditions-decided-while-compiling) true outside a generic class or a
    walk: `'crash ...' is decided while compiling and always holds here: remove it`. In a generic class it holds
    for some instances and not others, so it stays. A folded `if` stays everywhere: deciding a branch while
    compiling is what `if` on a `Build` field or a codegen question is for.
  Inside a `while`, only a proof made inside that loop makes a check of a `[]` read redundant: one made before the
  loop does not, since an assignment in the loop would leave the next pass unproven, so `crash
  names[index - 1]` may be written inside a loop that lowers `index`.
- **Buys.** No dead check survives, so a reader never wonders what it guards.
- **Falls back.** Delete the line.
- **See.** [failure.md: Narrowing a path](failure.md#narrowing-a-path);
  `diagnostics/check_proves_nothing`, `diagnostics/proven_element`, `diagnostics/proven_index_check`,
  `diagnostics/known_fact_checked`.
- **Does not apply.** A check the compiler cannot prove redundant stays allowed: two equal checks with a statement
  between them, a condition with a call in it, a stronger check after a weaker one (`parts > 0` after `parts != 0`).

### A copy made to narrow is an error

- **Proves.** A local only copies a `T?` name or path so that it can be narrowed.
- **Rule.** A `var` whose value is a bare name or path of a `T?` type, narrowed later by `assert`, `crash`, `if`,
  `while` or `switch` on its bare name, on `not` of it, or on it as a side of an `and` (`if not watcher { ... }`,
  `assert watcher and ready`), never assigned again, and whose source is not assigned in the function either.
- **Buys.** Refuses the copy: `'watcher' only copies 'tracker' so it can be narrowed: narrow 'tracker' itself`.
- **Falls back.** A snapshot taken before its source changes is not a copy for narrowing, and is allowed, and so is
  a `var` with a written type in a generic class, which converts for some instance.
- **See.** [failure.md: Narrowing a path](failure.md#narrowing-a-path); `diagnostics/copy_to_narrow`,
  `diagnostics/copy_to_narrow_conditions`.

### A class test narrows its block

- **Proves.** A union value is one given member.
- **Rule.** `if value == Monster { }`, with `value` a bare name, narrows it to `Monster` in the block; a `null` tests
  false. In a generic class a test that can never hold folds to `false`.
- **Buys.** Member access of that class inside the block.
- **Falls back.** `!=`, a path, a test inside an `and`, the `else` branch and a leaving `if` narrow nothing: use a
  `switch` over the union.
- **See.** [control_flow.md: `value == Class`](control_flow.md#value--class);
  `conformance/stage6/class_test`, `diagnostics/switch_single_case`.

### Every path ends in a `return`

- **Proves.** A function that declares a result never reaches its closing `}`.
- **Rule.** A path ends at a `return`, a bare `crash`, an `if`/`else` whose branches both end, a `switch` whose every
  case ends, a `while true`, and a condition the compiler folded whose taken branch ends. It passes through an `if`
  with no `else`, any other `while`, an `assert` or a `crash` with a condition, and anything else.
- **Buys.** Refuses a result nobody wrote, naming the path: `'sign_of' answers a String, but when 'value < 0' is
  false (line 12, an 'if' with no 'else') it reaches its end without a 'return', so a caller would get a String
  nobody wrote ...`.
- **Falls back.** Write the last `return` after the `if` or the loop, `return null` included, or a bare `crash`
  where the path cannot happen.
- **See.** [failure.md: Every path ends in a
  `return`](failure.md#every-path-ends-in-a-return); `diagnostics/falling_end`.

### A guard `assert` answers only "nothing"

- **Proves.** When an `assert` fails, the function's answer is one the caller can tell from a real one.
- **Rule.** A guard `assert` is allowed only in a function whose result, per instance, is nothing, a `T?`, a `List`,
  a `Dictionary`, or a `Vector`/`Items`. In a function answering a number, `Boolean`, text, enum or object it is an
  error, and never in a constructor. Where it is allowed, an `if` with no `else` whose only
  statement returns the default is an error naming the `assert` to write, and so is a last `if` with no `else`
  that only checks a value is there.
- **Buys.** Refuses a silent default: `this 'assert' would answer a default Integer (0) that a caller cannot tell
  from a real one: return a value (...), make the result 'Integer?', or 'crash ...' if this is a developer mistake`.
- **Falls back.** Write the answer (`if not found { return -1 }`, which [then proves the
  rest](#an-if-that-leaves-proves-the-rest)), make the result a `T?`, or `crash`.
- **See.** [failure.md: A default that looks like an answer is an
  error](failure.md#a-default-that-looks-like-an-answer-is-an-error),
  [An `if` that only returns the default is an `assert`](failure.md#an-if-that-only-returns-the-default-is-an-assert),
  [The last `if` of a function](failure.md#the-last-if-of-a-function); `diagnostics/default_answer`,
  `diagnostics/default_guard`, `diagnostics/returning_guard`, `diagnostics/empty_collection_guard`,
  `diagnostics/terminal_if`.

### A switch covers every case

- **Proves.** No value reaches a `switch` without a case for it.
- **Rule.** Over a union, every member or a last `_:`; over a `T?`, a `T:` case and a `Null:` case; over an enum,
  every value or a last `_:`; over a whole number or text, always a last `_:`. A `_:` that answers for nothing is an
  error, and so is a repeated case. Three `if`s in a row (or an `else if` chain of three) that each compare one name
  with a constant and only return are an error that writes the `switch` for you.
- **Buys.** A member or value added later cannot be forgotten; a `switch` whose cases all return ends a function.
- **Falls back.** Add the cases, or end with `_:` for the rest.
- **See.** [control_flow.md: `switch` over a union](control_flow.md#switch-over-a-union),
  [`switch` over values](control_flow.md#switch-over-values); `diagnostics/missing_switch_case`,
  `diagnostics/switch_rest_case`, `diagnostics/switch_chain`, `conformance/stage6/rest_case`.

## Indices and numbers

### A proven count or bound proves a read

- **Proves.** A `list[index]` read is in range from above, so it is a plain `T`, not a `T?`.
- **Rule.** In an `assert`, `crash`, `if` block, `while` body, the right side of an `and`, or after [an `if` that
  leaves](#an-if-that-leaves-proves-the-rest): `index < list.count()` (also `- k` for a literal `k`, and
  `list.count() > index`) proves `list[index]`; `base + k < list.count()`, `k` a literal from 1 to 63, proves
  `list[base]` to `list[base + k]`; `index < count` proves `list[index]` when `var count = list.count()` (or
  `- k`) was declared and neither `list` shrank nor `count` was assigned since; `list.count() == k` or
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
- **See.** [failure.md: Reading with `[]` answers `T?`](failure.md#reading-with--answers-t);
  `conformance/stage6/count_bound_proofs`, `conformance/stage6/bound_proofs`, `conformance/stage6/structural_index`,
  `diagnostics/index_reads`, `diagnostics/bound_proofs_undone`, `diagnostics/count_proves_no_key`,
  `diagnostics/structural_index_undone`.

### A proven read halts outside its list

- **Proves.** Nothing new: it is what a proven read does when its proof covered only the top of the index.
- **Rule.** A read a bound or count proves is taken from `get_at` directly, with one branch the C compiler is told
  is never taken; if the answer is missing the program halts with `spite: 'names[index]' is outside its list ...,
  at file:line`. A read narrowed by `crash`, `assert` or `if` is taken with no test, since that line tested it; a
  read in [a counted loop](#a-counted-loop-reads-its-items-unchecked) has both ends proven and no test at all.
- **Buys.** A counter gone below zero stops the program where it happened instead of reading a default.
- **Falls back.** None.
- **See.** [optimizations.md: A proven read tests only its bounds](optimizations.md#a-proven-read-tests-only-its-bounds);
  `conformance/stage6/proven_read_outside`.

### A counted loop reads its items unchecked

- **Proves.** Inside `while index < values.count()`, the counter is at least 0 and below the count, and the list's
  size cannot change.
- **Rule.** The condition is exactly `index < values.count()` over a local or parameter holding a `List`
  of numbers or `Boolean`s (the only list that holds them); `index` is a local `Integer` whose every assignment in the function is a literal of 0 or
  more, or `index = index + 1` as the last statement of a loop bounded by `index < ...count()` or a literal; the body
  only declares and assigns plain locals, reads and writes items of plain-value lists, and calls those lists' reading
  functions and maths. A second list indexed by the counter is checked once, before the loop.
- **Buys.** The count is read once and items are read and written as a C array, which the C compiler vectorises;
  `benchmarks/plain_loops` from about 680 to 190 µs.
- **Falls back.** The ordinary loop, every read checked. Any other call (an `append`), a counter starting from a
  parameter, a `Long` counter, a list held in an attribute, a `--repl`, `--repl-port` or `--hot-reload` build, or a
  function that waits keeps it ordinary. A body with an `assert` or `crash` keeps the second list's checks.
- **See.** [optimizations.md: A loop over plain values reads its count once and its items
  unchecked](optimizations.md#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked);
  `conformance/stage6/counted_loops`.

### A proven divisor is not checked

- **Proves.** A whole-number divisor is not zero.
- **Rule.** The divisor is a constant other than zero (a tree of `Integer`-range literals), or a call-free path
  proven by `path != 0` or `path > 0` with the literal `0` on the right, in the same places a
  [read is proven](#a-proven-count-or-bound-proves-a-read), kept or undone by calls as any proof is.
- **Buys.** No compare-and-branch before `/` or `%`.
- **Falls back.** The check stays and a zero halts naming the line: `spite: 'total / parts' divided by zero, at ...`.
  `parts >= 1`, `parts > 5`, `0 != parts`, a divisor with a call in it (`total / names.count()`) and a `Long`
  constant outside the `Integer` range are not proofs. The smallest signed value divided by `-1` halts as
  [arithmetic that does not fit](#arithmetic-that-does-not-fit-halts) does, even where the divisor is proven.
- **See.** [optimizations.md: A proven divisor is not checked](optimizations.md#a-proven-divisor-is-not-checked),
  [values_and_types.md: Numeric types](values_and_types.md#numeric-types);
  `conformance/stage6/division_by_zero`.

### A divisor written as zero is an error

- **Proves.** A division always halts.
- **Rule.** A whole-number `/` or `%` whose divisor is a constant expression equal to zero, or a codegen value folded
  to `0`.
- **Buys.** `'total / 0' divides by zero, which always halts the program ...` at compile time.
- **Falls back.** None.
- **See.** [values_and_types.md](values_and_types.md#numeric-types);
  `diagnostics/division_by_constant_zero`.

### Arithmetic that does not fit halts

- **Proves.** A counter stepped by one stays inside its type, and arithmetic on constants fits.
- **Rule.** In every build, every `+`, `-` and `*` on a whole number, signed or unsigned, a `-` in front of one, the
  smallest signed value divided by `-1`, and a value put into a narrower name halt if the answer does not fit. The
  check is left out of `counter + 1` where a condition in force says `counter < bound`, and out of `counter - 1`
  where one says `counter > bound`: `counter` is a local or a parameter, the condition is a `while`'s, an `if`'s or
  one side of an `and` in either, and the step comes before any assignment to `counter` in that loop pass. The
  comparison is done in `counter`'s type, so `bound` is at most its largest value and `counter + 1` fits (and at
  least its smallest, so `counter - 1` fits). It is also left out where both operands are constants, which the
  compiler has already worked out (and refused if they do not fit). A constant expression that does not fit its
  type, or a number written into a name it does not fit, is a compile error.
- **Buys.** `index = index + 1` in a counted loop is the plain operator, so the loop around it can still be
  unrolled and vectorised; in the compiler's own C, 1 145 of 2 595 checks go.
- **Falls back.** The check stays wherever no such condition is in force: a second step in the same pass (the first
  assignment ends the proof), a step inside a nested loop whose own condition does not bound `counter` (the inner
  loop may repeat it), `counter + 2`, an attribute (a call could change it), a sum like `total = total + value`,
  and a value put into a narrower name. Write the wider type first where a total may grow, and call
  `wrapping_sum`, `wrapping_subtract` or `wrapping_multiply` where wrapping is the point.
- **See.** [values_and_types.md: Arithmetic that does not fit
  halts](values_and_types.md#arithmetic-that-does-not-fit-halts),
  [optimizations.md](optimizations.md#arithmetic-is-checked-in-every-build);
  `conformance/stage6/counter_room` (the first loop's steps unchecked, the second's checked and halting),
  `conformance/stage6/integer_overflow`, `conformance/stage6/unsigned_overflow`,
  `conformance/stage6/narrowing_overflow`, `conformance/stage6/smallest_divided`.

### A wider operand is written first

- **Proves.** A right operand fits the left operand's type, so casting it loses nothing.
- **Rule.** In arithmetic, comparisons and bitwise functions, a right side with more bits than the left, or a
  floating side under a whole-number left, is an error naming the operation turned around, unless it is an
  integer literal within the left type's range.
- **Buys.** Refuses a silent cut (`progress > -0.5` with an `Integer` `progress`).
- **Falls back.** Write the wider side first, or keep the value in a variable of the wider type. Assigning,
  passing or returning a wider value into a narrower name is not checked by this proof.
- **See.** [values_and_types.md: Wider arithmetic goes wider operand
  first](values_and_types.md#wider-arithmetic-goes-wider-operand-first),
  [Comparisons are checked the same way](values_and_types.md#comparisons-are-checked-the-same-way);
  `diagnostics/wider_right_operand`, `diagnostics/wider_comparison`, `diagnostics/wider_bitwise_operand`.

### A dictionary's key kind is decided while compiling

- **Proves.** Every key a dictionary is given, and every dictionary it flows into, is of one kind: text or a whole
  number.
- **Rule.** Each place a dictionary type is written is joined with every place it flows to; the keys given to
  `[]`, `set`, `get`, `has` and `remove` decide the kind, the widest whole number winning, text for a `String`, symbol
  or enum, and text for a dictionary given no key. It settles in at most eight passes.
- **Buys.** A number key is hashed as a number, with no `String` made.
- **Falls back.** Text keys. Two kinds meeting is a compile error, not a conversion; to key by text, give text.
- **See.** [collections.md: Keyed by numbers](collections.md#keyed-by-numbers),
  [optimizations.md](optimizations.md#a-dictionary-keyed-by-numbers-hashes-the-numbers);
  `conformance/stage6/number_keys`, `diagnostics/mixed_dictionary_keys`.

### Maths on constants is worked out while compiling

- **Proves.** A maths function called on constants has one answer.
- **Rule.** A maths member (`sine`, `square_root`, ...) whose receiver and arguments are literals, the number
  classes' constants (`Float.pi`, `Double.infinity`, ...) or other folded calls is answered by the compiler's own C library.
- **Buys.** No call, and no `math.h` when every call folds.
- **Falls back.** A variable is never a constant: `angle.sine()` is a call. Plain arithmetic on literals is left
  to the C compiler.
- **See.** [optimizations.md: Maths on constants is worked out while
  compiling](optimizations.md#maths-on-constants-is-worked-out-while-compiling); `conformance/stage6/maths_folding`.

## Compile-time questions

### Conditions decided while compiling

- **Proves.** A condition's answer is a fact of the build.
- **Rule.** An `if` whose condition is a `Build` field, a codegen value (`$is_magic`), a test on a codegen type
  (`$T == List`, `$T.element_type == Float`), `attribute.class == X` in a walk, `attribute.name.starts_with("_")`
  or `ends_with` with a literal in a walk, a class test the value's type already
  answers (in a copy of a function made for one class that reaches a `type`, a test on its parameter), one of the questions below, or `not`, `and`, `or`, `==`, `!=` over them. An `and` whose left folds false,
  or an `or` whose left folds true, folds whatever the right side is. A function of a generic class is compiled for
  an instance only when code that survived folding names it.
- **Buys.** The branch not taken is absent, so it may use what this build or instance does not have.
- **Falls back.** Anything else is tested at run time.
- **See.** [optimizations.md: Deciding conditions at compile
  time](optimizations.md#deciding-conditions-at-compile-time),
  [metaprogramming.md: Asking what a generic was given](metaprogramming.md#asking-what-a-generic-was-given);
  `conformance/stage6/codegen_member_fold`, `conformance/stage6/walked_class_fold`, `conformance/stage6/private_walk`.

### Whether a function waits

- **Proves.** A function can or cannot reach a wait (a sleep, a read of a console, file or socket, a scheduler
  join).
- **Rule.** Waiting spreads from those library functions through calls by name, function values and dispatch
  through a union or `type`, to a fixed point. `$T.functions['update_each'].is_resumable` folds to the answer; an answer that
  would change once the asking function is compiled is an error. Joining a `Parallel` is not a wait, nor is
  `finished`.
- **Buys.** An engine runs a system on a `Concurrent` only where it waits.
- **Falls back.** Where it cannot tell (a function value of the same signature, a constructor, dispatch), the
  answer is `true`.
- **See.** [metaprogramming.md: Asking whether a function
  waits](metaprogramming.md#asking-a-question-while-compiling); `conformance/stage6/waiting_systems`,
  `diagnostics/function_waits_paradox`.

### How many arguments a function takes

- **Proves.** A function's arity.
- **Rule.** `$T.functions['update_each'].arguments.count()` and `phase.arguments.count()` in a walk fold to a whole number,
  compared with `==`, `!=`, `<`, `<=`, `>` or `>=`; 0 when the class does not declare the function; a pattern must
  agree across every function it matches.
- **Buys.** A runner compiles only the branch that fits a system's arity.
- **Falls back.** A pattern whose functions disagree is an error.
- **See.** [metaprogramming.md: Asking how many arguments a function
  takes](metaprogramming.md#asking-a-question-while-compiling); `conformance/stage6/folded_argument_count`,
  `diagnostics/argument_count`.

### Whether a class fits a `Vector`

- **Proves.** A class's objects can be laid out inline in a block, with no header and no count.
- **Rule.** Every attribute is a number, `Boolean`, text, enum, symbol or singleton, or a `T?` of those; the class
  has no `drop()` and no function uses `this` as a value; it is not a generic template.
  `$T.is_fixed_size`, `attribute.class.is_fixed_size` and `$T.any_attribute_fits_vector(Entity)` fold to the answer.
- **Buys.** `Items<T>` chooses inline storage and lends its items.
- **Falls back.** Reference storage: counted items, none of the borrow rules.
- **See.** [metaprogramming.md: Asking whether a class fits a
  Vector](metaprogramming.md#asking-whether-a-class-fits-a-vector),
  [optimizations.md](optimizations.md#an-items-storage-is-chosen-while-compiling); `conformance/stage6/items_columns`.

### Whether a function writes a parameter

- **Proves.** A function writes, or does not write, what one of its parameters is given, or anything reached
  through it.
- **Rule.** `$T.functions['update_each'].arguments[index].is_mutated` folds. A write is an attribute or item set on what
  the parameter reaches, a call that writes its own object, passing it to a parameter that is written, or storing
  it where a later write could reach it; reassigning the name and copying plain values out are not writes, and
  neither is a write to a singleton reached through it, which is shared state. It follows a walked
  argument's `.index` and what a callee returns.
- **Buys.** An engine refuses a system whose writes a snapshot would lose, with `crash not ...`.
- **Falls back.** Where it cannot decide (a function value, dispatch through a union or `type`, a template, an
  unknown class, a function whose body the compiler supplies and does not know to only read), the answer is
  `true`.
- **See.** [metaprogramming.md: Asking whether a function writes a
  parameter](metaprogramming.md#asking-a-question-while-compiling); `conformance/stage6/parameter_writes`,
  `diagnostics/snapshot_argument_writes`.

### Whether a class keeps state

- **Proves.** No function of a class writes its own object after it is made, and no singleton it binds keeps
  state, or that one does.
- **Rule.** `$T.is_stateful` folds, and so does `kind.class.is_stateful` in a walk over `Spite.Class.instances`. It
  asks the question of [Whether a function writes a parameter](#whether-a-function-writes-a-parameter) of each
  function's own object, skipping the constructor and `drop()`, counts a function that writes a singleton,
  and follows the singletons the class binds.
- **Buys.** An engine can refuse a system that keeps state between frames, for every system at once.
- **Falls back.** As that question does: where the study cannot decide, the answer is `true`. The standard
  library counts like any code, so a class binding `Console` has state (reading a line writes its buffer).
- **See.** [metaprogramming.md: Every class in the program](metaprogramming.md#walking-a-programs-structure);
  `conformance/stage6/class_walk`.

### A `crash` that folds false is a compile error

- **Proves.** A `crash` in code the program reaches would halt on every run.
- **Rule.** An `assert` or `crash` side (split on `and`) that asks only codegen questions folds. Holding, it writes
  nothing; failing, a `crash` in a function that survives [tree shaking](#tree-shaking-what-main-can-reach) is an
  error naming the instance, and a failed `assert` returns its default with the rest of the block not compiled.
- **Buys.** A certain halt becomes a build error; a check that holds costs nothing.
- **Falls back.** A folded `crash` in an instance nothing calls is left alone; a condition that also reads a run-time
  value through `or` is a run-time check.
- **See.** [metaprogramming.md: Codegen values](metaprogramming.md#codegen-values-);
  `diagnostics/folded_crash`, `conformance/stage6/folded_crash_uncalled`.

### Tree shaking: what `main` can reach

- **Proves.** A piece of the generated C cannot be reached from `main`.
- **Rule.** After generating, the compiler keeps `main`, every piece that is not a named function, type or static,
  and everything any kept piece names, transitively (`tree_shaker.spite`). A dispatcher over a union or `type` names
  every member's function, so all of them stay when it does; a function made into a value is kept.
- **Buys.** Only what is used is in the program: `examples/hello` from 5 130 to 1 332 lines of C; a native symbol is
  looked up only if a kept function calls it.
- **Falls back.** An inspectable build (`--repl`, `--repl-port`, `--hot-reload`, `--development`) keeps everything.
  A function nobody calls is still compiled and checked. The match is by name, so it keeps too much, never
  too little.
- **See.** [optimizations.md: Tree shaking the generated
  C](optimizations.md#tree-shaking-the-generated-c), [metaprogramming.md: Tree
  shaking](metaprogramming.md#tree-shaking); `conformance/stage6/development_internals`.

### A reload compiles only the changed classes

- **Proves.** Compiling the changed classes against what the running program was compiled with writes the same code
  a whole compile would.
- **Rule.** A `--hot-reload` build records in its manifest every fact compiling one class took from another: the
  functions that wait, the call effects and writes of every function, which classes fit each shape, the attributes
  and words read, the key kinds, the template instances and the functions made on demand, with a hash of every
  piece of C. A reload replays them, compiles the changed files' classes, and compares: a recorded fact that now
  differs, a function of another class whose C would change, or a changed signature of a changed class sends the
  reload to the whole compile, which says why on the error output.
- **Buys.** A changed system of a large game's server swaps in about 6 seconds instead of 20 to 30.
- **Falls back.** The whole compile. `SPITE_RELOAD_CHECK` compiles both ways and compares them.
- **See.** [repl.md: How it works](repl.md#how-it-works), [optimizations.md: A reload compiles
  only the classes that changed](optimizations.md#a-reload-compiles-only-the-classes-that-changed).

### A reload reaches every object whose attributes it changes

- **Proves.** After a reload changes a class's attributes, no code reads an object of the class with the old
  layout, and every live object of it has moved.
- **Rule.** A class's layout is its attributes' names and types and whether it fits an `Items`' own memory. Every
  read of a program class's attributes, and every use of its size, is written through `<Class>___fields(object)` or
  `sizeof(<Class>)`, so a whole compile finds each function whose C depends on a changed layout by those names, and
  compiles and installs it; one the running program cannot re-point refuses the reload. Every object of a program
  class, and every `Items` or `Vector` keeping them (in its own memory or as references), is in a list its
  allocation and release keep, so the move reaches objects that only a running function's local holds, and moves
  an `Items`' items between its own memory and objects of their own when the class starts or stops fitting it.
- **Buys.** A class's attributes change while the program runs, with no restart and no lost state.
- **Falls back.** Refused by name, the program keeping all its code: a function it cannot re-point, and a class
  that starts fitting an `Items`' own memory while an object an `Items` holds is held elsewhere too.
- **See.** [repl.md: Changing a class's
  attributes](repl.md#changing-a-classs-attributes), [optimizations.md: What a `--hot-reload` build carries so its
  objects can move](optimizations.md#what-a---hot-reload-build-carries-so-its-objects-can-move).

## Memory

### A buffer freed in its block lives in the frame

- **Proves.** An address from `allocate(n)` never outlives its block.
- **Rule.** `var name = heap.allocate(size)` whose block later calls `heap.free(name)` at its own level, with every
  use in between an address primitive, a `TypedMemory` read or write, or a call to a function of the same class that
  provably keeps the parameter; neither name is assigned again.
- **Buys.** No allocation: a slot of the frame, and `free` does nothing.
- **Falls back.** The heap, for a size over 256 bytes known only at run time, any other use, a recursive call, or a
  `--hot-reload` build.
- **See.** [memory.md: Placement](memory.md#placement-the-compiler-decides-where-memory-lives);
  `conformance/stage6/lent_buffers`.

### Objects that never leave their function live in the frame

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
  builds; and in a function that waits.
- **See.** [optimizations.md: Objects that never leave their function live in the
  frame](optimizations.md#objects-that-never-leave-their-function-live-in-the-frame); `conformance/stage6/frame_objects`.

### Local and variadic lists in the frame

- **Proves.** A list is only read after it is filled, or only read by the function it is handed to.
- **Rule.** A local list made by a literal or `List<T>()` and filled by `append` statements at its own level, then only
  read, lives in the frame (16 items) or in constant data (256). A variadic list whose callee only reads it
  (`count`, `[]`, `get_at`, `first`, `last`, `contains`, `join`, ...), in a call that is a statement of its own, lives
  in the caller's frame.
- **Buys.** Two allocations fewer per list.
- **Falls back.** The heap, when the list is stored, returned, changed after filling, handed to a template, asked for
  `.memory`, or in an inspectable build or a function that waits.
- **See.** [optimizations.md: A local list of known size lives in the
  frame](optimizations.md#a-local-list-of-known-size-lives-in-the-frame),
  [A variadic list the callee only reads](optimizations.md#a-variadic-list-the-callee-only-reads-lives-in-the-callers-frame);
  `conformance/stage6/text_building`.

## Borrowing and lending

A borrowed item is the address of an item inside a collection: never retained, never released. Every proof in this
section keeps one fact true: a borrowed item is never kept past its use, and never read after its collection may
have moved.

### Borrowed items of a `Vector` or `Items`

- **Proves.** An item read from a `Vector`, or an `Items` whose class [fits a `Vector`](#whether-a-class-fits-a-vector),
  is used only while it is borrowed.
- **Rule.** `vector[index]`, once narrowed, and the item a template visits are borrowed. A borrowed name is not kept
  in an attribute or a list, returned, made into a function value, given a second name or assigned again. Plain values are never items of a `Vector` or `Items`. Only `library/`'s own `Vector`, `Items` and
  `InlineMemory` are trusted; a program reopening them is checked.
- **Buys.** No count and no copy: `layout.order = 3` writes the stored item.
- **Falls back.** Each way of keeping one is an error naming `copy()`, an independent object.
- **See.** [memory.md: Borrowed items of a
  `Vector<T>`](memory.md#borrowed-items-of-a-vectort); `diagnostics/vector_borrows`,
  `conformance/stage6/vector_items`.

### No borrowed read past a resize

- **Proves.** Nothing between taking a borrowed item and reading it can move the collection's items.
- **Rule.** A borrowed name is not read after a statement that may grow or shrink its collection: `append`,
  `prepend`, `insert`, `reserve`, a removal, `truncate`, `swap`, `clear`, assigning the collection or its path, or a
  call whose effects may do one of those. A call is followed only into what it can run: a collection read with
  `[]` from an attribute is that attribute's element, and a call through a `type` reaches only the classes it is
  handed. A resizing statement anywhere in a loop ends the borrow for the whole loop.
- **Buys.** A borrowed item cannot dangle.
- **Falls back.** Refused, on the resizing line: read the item again after it, or keep a `copy()`. A value the
  compiler cannot place (a nullable, a union, a call's result, a reassigned parameter) is followed into every
  function of that name, and a function value may do anything.
- **See.** [memory.md: Borrowed items of a
  `Vector<T>`](memory.md#borrowed-items-of-a-vectort); `diagnostics/dispatched_resizes`,
  `diagnostics/vector_reserve_borrows`, `conformance/stage6/queued_borrows`.

### Rows of borrowed items

- **Proves.** A row (an object literal holding borrowed items, or a `type` local filled by a walk) lives exactly
  as long as one call.
- **Rule.** A row is passed only to a function called by name whose parameter is a `type`, which gets a
  `___lent_<positions>` copy that never counts it; it is never kept, returned, listed, aliased or assigned; the call
  may not resize what it borrows from. A walked row may take each attribute from a generic singleton per
  attribute class and mix borrowed items with other values; a plural's arguments may carry borrowed items into
  the one call it fills. The template's branch may name a value with `var` (`var stored_row =
  rows[attribute.index]`); the name is walked as the value it names.
- **Buys.** The row is a struct in the frame and its reads are uncounted: `benchmarks/sparse_rows` 7.4 ms a tick
  against 24.0 ms.
- **Falls back.** Refused as above; a walk of any other shape leaves an ordinary `type` value, and a counted value
  in a walked row is counted once for the row's block.
- **See.** [memory.md: A row of borrowed items, for one
  call](memory.md#a-row-of-borrowed-items-for-one-call), [Borrowed arguments, for one
  call](memory.md#borrowed-arguments-for-one-call); `conformance/stage6/vector_rows`, `conformance/stage6/sparse_rows`,
  `conformance/stage6/lent_arguments`, `diagnostics/walked_rows`.

### An item lent to the caller

- **Proves.** A function's result is an item of a singleton's `Vector` or `Items` storage.
- **Rule.** A `return` that reads an item straight from an attribute path of the function's class ending at a
  singleton's `Vector` or fitting `Items` lends its result, decided per instance with compile-time conditions folded
  (`is_fixed_size`, `functions[...]` with a literal name, `$T == X`; not `is_resumable`). Every return is such an item or `null`.
  At the caller the result is borrowed under every rule above, narrowing it keeps it borrowed, and it is not
  returned further. A lending function is only called by name, never made a function value or used through a `type`.
- **Buys.** The caller gets the address: no retain, no release.
- **Falls back.** An owned result for an instance whose class does not fit; storage through a local, a parameter or
  an object that is not a singleton is refused as returning a borrowed item.
- **See.** [memory.md: An item lent to the caller](memory.md#an-item-lent-to-the-caller);
  `conformance/stage6/lent_results`, `conformance/stage6/narrowed_lends`, `conformance/stage6/folded_lends`,
  `diagnostics/lent_escapes`.

### An item lent to a call

- **Proves.** A function handed a borrowed item keeps nothing of it.
- **Rule.** A borrowed name, an item read with `[]` or a row's attribute, passed to a program function called by name
  at a place that is not a union or variadic list, calls its `___lent_<positions>` copy, compiled under the borrow
  rules, so any way the function would keep the item is an error in that function naming the lending call. The call
  may not resize what the item is borrowed from.
- **Buys.** The argument is the item's address, never counted or copied.
- **Falls back.** Not lent to a library function, a union parameter or an unnamed expression: each is an error that
  says why and never offers a copy the function would write (a write to a copy vanishes).
- **See.** [memory.md: An item lent to a call](memory.md#an-item-lent-to-a-call);
  `conformance/stage6/lent_to_calls`, `diagnostics/lent_to_calls`.

### A list element lent to a row

- **Proves.** An element of a singleton's `List` read into a row cannot be let go while the row's block runs.
- **Rule.** The row's line calls a singleton function whose body is only `crash <list>[<index>]` then
  `return <list>[<index>]`, the list an attribute nothing assigns after the singleton is made, the
  index a whole-number parameter or literal; the rest of the block names every call it makes, none of which lets go
  of an object, removes from a list, assigns an attribute holding an object or calls a function value; and no class
  whose objects may be let go meanwhile has a `drop()` reaching that singleton or list. With threads, the block takes
  the singleton's readers' side or lock once, only when what it runs reaches no singleton and no wait.
- **Buys.** One retain and one release per row, and the lock per call.
- **Falls back.** The ordinary counted call; nothing is an error.
- **See.** [memory.md: Borrowed items of a
  `Vector<T>`](memory.md#borrowed-items-of-a-vectort) (a reference column's element);
  `conformance/stage6/lent_list_elements`, `conformance/stage6/lent_list_elements_parallel`.

### An argument its caller holds is passed uncounted

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
- **See.** [optimizations.md: An argument its caller holds is passed without
  counting](optimizations.md#an-argument-its-caller-holds-is-passed-without-counting);
  `conformance/stage6/held_arguments`.

### A singleton attribute that never changes is read in place

- **Proves.** An object attribute of a singleton stays the singleton's for the rest of the program.
- **Rule.** No function of the singleton outside its constructor and no other class assigns the attribute (from the
  whole program's call effects); it holds a class, list or dictionary (or a `T?` of one), not text, a number or a
  function value; it has no getter.
- **Buys.** Read from another class, it is the attribute's address: no count and no lock. 6.0 to 3.2 ns a read.
- **Falls back.** The counted read, under the singleton's lock when a `Parallel` reaches it; in `--hot-reload`,
  `--repl` and `--repl-port` builds.
- **See.** [optimizations.md: A singleton's attribute that never changes is read in
  place](optimizations.md#a-singletons-attribute-that-never-changes-is-read-in-place);
  `conformance/stage6/singleton_attribute_reads`.

### A list's templates read their elements uncounted

- **Proves.** Nothing in the rest of a template's pass over one element can let that element go.
- **Rule.** In `List` templates, the reference kind of `Items` and fused chains, an element is read uncounted when
  the rest of its pass only assigns locals and makes calls the compiler can name, none of which may let go of an
  object or assign an attribute through an unknown class, and when no `drop()` of the program's own classes may let
  go of an object either.
- **Buys.** One retain and one release per element (`benchmarks/fused_chain` 332 to 185 ms).
- **Falls back.** The counted read, in `--hot-reload` builds, for a nullable element, and in a program one of whose
  classes has a `drop()` that may let go of an object.
- **See.** [optimizations.md: A list's templates read its elements without counting
  them](optimizations.md#a-lists-templates-read-its-elements-without-counting-them);
  `conformance/stage6/fused_chain_allocations`, `conformance/stage6/template_lend_drop`.

## Threads and locks

These apply only in a program that makes a `Parallel`, runs a `parallel_each_` pass or makes a `ForeignCallback`; any other program has no lock at all.

### Which singletons a `Parallel` reaches

- **Proves.** No thread but the program's own calls a singleton.
- **Rule.** The compiler walks the calls from every function a `Parallel` or the thread pool can run (every
  function made into a value, and the members and filters of every `parallel_each_` pass), following a call on an
  unknown class into every function of that name. A singleton with an operator, getter, setter, `get_at`/`set_at`,
  `missing_function`, `to_string` or `to_debug` is always counted as reached.
- **Buys.** No lock for a singleton only the program's thread uses.
- **Falls back.** A reached singleton takes one of the forms below, the lock last.
- **See.** [optimizations.md: Thread safety for singletons, the cheapest safe
  form](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form),
  [concurrency.md: A value per thread, and a lock](concurrency.md#a-value-per-thread-and-a-lock);
  `conformance/stage6/singleton_forms`.

### Read-only and atomic singletons

- **Proves.** A singleton never changes after it is made, or each of its functions touches its changing state once.
- **Rule.** Read-only: no function assigns an attribute outside the constructor, no other class assigns one, and it
  holds no list, dictionary, function value or object that can change. Atomic: every changing attribute is a whole
  number or `Boolean`, and each function touches it at most once (one read, one `count = count + step`, or one
  store), not in a loop.
- **Buys.** No lock: plain reads, or single atomic instructions.
- **Falls back.** The lock.
- **See.** [optimizations.md: Thread safety for singletons, the cheapest safe
  form](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form); `conformance/stage6/singleton_forms`,
  `conformance/stage6/singleton_lock_calls`.

### A function that touches no changing state takes no lock

- **Proves.** A function of a locked singleton reads nothing that can change, or the lock is already held.
- **Rule.** A function that reads or writes no changing attribute, and calls nothing that does, runs unlocked; a
  call from inside a locked function to another of the same singleton goes to its unlocked body.
- **Buys.** Fewer atomic operations, and one deadlock shape gone.
- **Falls back.** The lock; a function value of a singleton's function always goes through the locked version.
- **See.** [optimizations.md: Singletons a `Parallel` reaches take a
  lock](optimizations.md#singletons-a-parallel-reaches-take-a-lock); `conformance/stage6/singleton_stateless_calls`,
  `conformance/stage6/singleton_function_values`.

### Reading functions share the lock

- **Proves.** A locked singleton's function changes nothing, and no function that writes it runs on the pool.
- **Rule.** Its statements assign only its own locals; it calls only reading functions of its class, reading
  members of lists and dictionaries, text and number functions but writes, and reading functions of stateless
  singletons; its text has no holes; its operators are on numbers. And none of the singleton's writing functions is
  reachable from what a `Parallel` runs.
- **Buys.** Readers take a count on their own cache line and never contend: 202 to 6 ns a row.
- **Falls back.** Anything else is a writing function; a singleton also written from the pool keeps the plain lock.
- **See.** [optimizations.md: A singleton's reading functions do not exclude each
  other](optimizations.md#a-singletons-reading-functions-do-not-exclude-each-other); `conformance/stage6/singleton_reads`.

### A counted loop takes a singleton's lock once

- **Proves.** Holding a singleton's lock for a whole loop can neither deadlock nor wait.
- **Rule.** A `while` outside the singleton that is counted (`index < bound`, stepped last), cannot `return`, calls
  only that singleton's functions and plain-value list functions, computes only numbers, `Boolean`s, hole-free text
  and enum values, and whose calls reach no wait and no `Parallel`, `Concurrent`, `ThreadPool`, `Scheduler`, `Lock`,
  `Program`, `Console`, `File` or `Socket`; at least one call takes the lock.
- **Buys.** One lock for the loop: 51 to 0.6 ms with one singleton shared by eight workers.
- **Falls back.** A lock per call; in `--hot-reload`, `--repl` and `--repl-port` builds and a `Concurrent`'s
  resumable copy.
- **See.** [optimizations.md: A counted loop of calls to one singleton takes its lock
  once](optimizations.md#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once); `conformance/stage6/coarse_locks`.

While no task is on the thread pool, a locked function also skips its lock. That is a run-time test of a
counter, one load per call, not a proof: [optimizations.md](optimizations.md#while-no-task-runs-a-singletons-lock-is-skipped).

### A lock that would wait forever is an error

- **Proves.** A locked function would hold its lock forever.
- **Rule.** A locked function of a singleton that makes a `Parallel` whose work reaches a locked function of the same
  singleton, and waits for it, is an error; so is a `while true` that cannot leave inside a locked function.
- **Buys.** A hang becomes a build error.
- **Falls back.** Not seen: a handle that escapes (stored, switched on, put in a literal), work reached through a
  function value, a union or an unknown receiver, a `parallel_each_` pass, and generic singletons. Do not wait inside
  a locked function for work that calls back into it.
- **See.** [concurrency.md: Rules in full](concurrency.md#rules-in-full); `diagnostics/locked_wait`,
  `diagnostics/endless_locked_loop`.

### What a `Parallel` may reach

- **Proves.** Work run on another thread touches nothing another thread may touch at the same time.
- **Rule.** A `Parallel(function)` and a `parallel_each_` member may reach only their own instance's plain values,
  their locals and singletons (which the compiler makes safe); an object handed over and held only by the task may
  keep its lists and objects; a `Weak` a `Parallel` reaches is an error.
- **Buys.** A data race is a compile error.
- **Falls back.** Refused, naming what the other thread could reach.
- **See.** [concurrency.md: A value per thread, and a
  lock](concurrency.md#a-value-per-thread-and-a-lock), [`parallel_each_`: a member on every
  element](concurrency.md#parallel_each_-a-member-on-every-element); `diagnostics/parallel_reach`,
  `diagnostics/parallel_function_reach`, `diagnostics/parallel_handover`, `diagnostics/weak_across_threads`.

### Which calls suspend a `Concurrent`

- **Proves.** Which functions a `Concurrent` can pause inside.
- **Rule.** A function given to a `Concurrent` that can wait, and every function it calls by name that can wait, gets
  a resumable copy, a state machine; the rest of the program is unchanged. A program that makes no `Concurrent` has
  no machines, and its waits are plain blocking calls.
- **Buys.** Hidden async/await with no fibers and no runtime.
- **Falls back.** A wait reached through a function value, a union, a constructor, the right side of `and`/`or` or
  a comparison runs in place, holding its `Concurrent` while the others run.
- **See.** [concurrency.md: What the compiler does at a
  wait](concurrency.md#what-the-compiler-does-at-a-wait),
  [optimizations.md](optimizations.md#hidden-asyncawait-as-compile-time-state-machines);
  `conformance/stage6/concurrent_waits`.

## Other refusals built on an analysis

Each reads the program while compiling and accepts, speeds up or refuses code; each is taught in full on its
own page, and none has a fallback beyond the error it gives.

- **Unused is an error**: a name, parameter or attribute that is only written and never read. It judges the source,
  so reads inside a folded-away branch still count; a function nobody calls is not reported, and a loaded
  package's attributes are shaken rather than reported ([style.md: Nothing unused](style.md#nothing-unused); `diagnostics/unused_names`, `diagnostics/written_not_read`).
- **A constructed object must be kept and used** ([classes_and_files.md](classes_and_files.md#a-constructed-object-must-be-kept-and-used);
  `diagnostics/dropped_construction`).
- **Singletons that make each other in a circle** are an error; a circle only a constructor's body closes is not
  followed, and halts naming it at run time ([classes_and_files.md: Singletons](classes_and_files.md#singletons);
  `diagnostics/singleton_circle`, `conformance/stage6/singleton_circle`).
- **An allocator is set only on a fresh object**, on the line after it is made, so it is made there and never twice
  ([optimizations.md](optimizations.md#an-allocator-set-after-construction-is-where-the-object-is-made);
  `diagnostics/allocator_after_use`).
- **Build settings are constants**: a flag's value has its field's type, an unknown flag or an `Environment` setting
  given to the compiler is an error, and an `if` around a `load` is decided from `Build` fields and literals ([programs.md: Compile-time settings](programs.md#compile-time-settings-build); `diagnostics/flag_value`,
  `diagnostics/load_unknown_condition`).
- **A value fits a `type`**, and a `$T` its constraint: attributes, required functions, return types, not-null, so a
  call through a `type` tests only the classes the program admits ([values_and_types.md: Inline types and duck typing](values_and_types.md#inline-types-and-duck-typing);
  `diagnostics/shape_mismatch`, `diagnostics/generic_constraints`).
- **A member template fits**: the member exists and has the right kind (a `Boolean` for `filter_`, a number for
  `sum_`), and a passed function takes the element; only called templates are generated, and a function passed by
  name is called directly ([collections.md: Member templates](collections.md#member-templates-loops-you-do-not-write);
  `diagnostics/member_template_mistakes`, `diagnostics/passed_functions`).
- **A value can be written**: the `T` of a `JsonWriter`, `JsonReader`, `BinaryWriter` or `BinaryReader` holds no union,
  `type` or function value anywhere, so writing never fails at run time, and a binary schema is a constant ([json.md](json.md#json-is-reflection-not-a-library); `diagnostics/json_unwritable`).
- **What crosses into C**: numbers, `Boolean`, enums, text, lists of numbers, number-only `type`s and function values;
  a callback's parameters and context are checked ([foreign_libraries.md: What crosses](foreign_libraries.md#what-crosses); `diagnostics/foreign_call_mistakes`,
  `diagnostics/foreign_callback_mistakes`).
- **Reads in a row are independent**: consecutive `var x = file.read()` lines that do not name each other's results
  run at once ([concurrency.md: Reads in a row overlap](concurrency.md#reads-in-a-row-overlap)).
- **The short form is proven the same, and the long form refused**: a `while` a member template already says, a chain
  of `if`s that is a `switch`, the same call opening every branch, a switch that is a class test, a text that is one
  hole ([style.md: The short form is the only form](style.md#the-short-form-is-the-only-form);
  `diagnostics/walk_with_function`, `diagnostics/repeated_branch_call`, `diagnostics/lone_hole`).

Not proven anywhere, so write the check or accept the run-time report: how deep recursion goes (a stack overflow is
a [native fault report](failure.md#what-a-native-fault-reports)); unreachable code after a `return`; a reference
cycle, which leaks ([memory.md: Cycles leak](memory.md#cycles-leak)); a wider value assigned, passed or returned
into a narrower name, which wraps; and two threads writing one number attribute of an instance they share.

---

Next: [the contents](README.md), every page of the documentation.
