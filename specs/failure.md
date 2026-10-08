# Nullable values and failure

The specification of [Nullable values and failure](../docs/failure.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Nothing fails silently: the rule

**Anything that can go wrong silently is a bug.** Every failure is loud: a compile error for what the compiler can
know, or a crash naming its cause for what only the run can find, and never:

- **a wrong value** that the program cannot tell from a right one (a default, a zero, a truncated number);
- **a lost write** (a value computed or stored that nothing reads);
- **a skipped step** (work the reader expects to happen that does not);
- **a leak** (memory or a resource that is never given back);
- **a hang** (a wait or a loop that can never end).

It is a rule for the language, the compiler, the standard library and the programs written in it: a proposal that
lets one of the five happen without a word needs a reason, and a place where the compiler lets one through is a bug
to fix, not a style to document. **What the compiler can see is refused while compiling**, even when the run would
also stop it: behaviour that is wrong is a compile error wherever it can be one, so code that compiles is code worth
keeping, and the run's halt is the backstop for what only the run can find.

**What the language already refuses.** Each is a compile error or a crash naming its cause:

| Would go wrong silently | What happens instead | Where |
|---|---|---|
| reading a value that may be absent as if it were there | `T?` must be narrowed first; `value == null` is an error | [narrowing](../docs/failure.md#narrowing) |
| an index past the end, a key never set | `[]`, `first()`, `last()`, `remove_first()` answer a `T?` | [reading with `[]`](../docs/failure.md#reading-with--answers-t) |
| a read a loop bound proved, with a counter gone below zero | halts naming the read and the line | [reading with `[]`](../docs/failure.md#reading-with--answers-t) |
| a guard `assert` answering a `0`, `false`, `""` or default object the caller takes for a real answer | compile error unless the result can say "nothing" | [a default that looks like an answer](../docs/failure.md#a-default-that-looks-like-an-answer-is-an-error) |
| a write to an object only this function holds (made here, a copy, or a function's result that is always made new), with the object never read after it | compile error at the write naming the object (`diagnostics/lost_writes`); a class with `drop()` or a setter for the attribute is left alone, since either may read it | [this section](#nothing-fails-silently-the-rule) |
| a local read from a list or dictionary (or answered by a function returning one of its items) assigned a value nothing reads, as if that wrote the list | compile error at the assignment naming the write to use (`diagnostics/replaced_items`) | [memory.md](memory.md#assigning-a-name-read-from-a-list-changes-only-the-name) |
| a `false`, `0` or `""` taken for "missing" | narrowing tests presence, never the value; a `Boolean?` is a condition only in `assert` and `crash`, which ask for `true` | [null safety](../docs/failure.md#narrowing) |
| text that is not a number, read as `0` | `to_integer()` and the other readings answer a `T?`; text assigned to a plain number is an error | [standard_library.md](../docs/standard_library.md#string) |
| a function that declares a result reaching its end without a `return` | compile error at its last line naming the path | [every path ends in a `return`](../docs/failure.md#every-path-ends-in-a-return) |
| a proof that went stale after an assignment or a call | the read must be proven again; in a loop, an error naming the call | [a call may undo a proof](../docs/failure.md#a-call-may-undo-a-proof) |
| a failed `crash` printing a default that reads as a real zero | the report names the missing link, index and count, or key | [what a crash reports](#what-a-crash-reports) |
| a native fault ending the program with nothing printed | `spite.fault` with the place, the last foreign call and the stack | [what a native fault reports](#what-a-native-fault-reports) |
| a whole number divided by zero | halts naming the line; a zero written as the divisor is a compile error | [values_and_types.md](values_and_types.md#numeric-types) |
| arithmetic that does not fit, signed or unsigned, and the wrap it would make | halts naming the operation and the operands, in every build; wrapping only by `wrapping_sum`, `wrapping_subtract` and `wrapping_multiply` | [values_and_types.md](../docs/values_and_types.md#arithmetic-that-does-not-fit-halts) |
| a value too big for the narrower name it is assigned, passed or returned to | halts naming the value and both types; a number written there that does not fit is a compile error | [values_and_types.md](../docs/values_and_types.md#arithmetic-that-does-not-fit-halts) |
| a wider operand cut to fit, in arithmetic or a comparison | compile error naming the operation turned around | [values_and_types.md](../docs/values_and_types.md#wider-arithmetic-goes-wider-operand-first) |
| a constant that overflows its type | compile error | [values_and_types.md](values_and_types.md#numeric-types) |
| a `Float` gone to infinity written as JSON | crash naming the attribute | [json.md](../docs/json.md) |
| a condition that is always false because it asks the compiler | a folded-false `crash` in reached code is a compile error | [metaprogramming.md](metaprogramming.md#codegen-values-) |
| a value computed and never read, an attribute nothing reads | compile error | [style.md](style.md#unused-is-an-error) |
| an object made and dropped on the same line, to "do" something | compile error | [classes_and_files.md](classes_and_files.md#a-constructed-object-must-be-kept-and-used) |
| `for`, `break`, `continue`, `++`, `&&` and other habits that would parse as something else | compile error naming the Spite form | [control_flow.md](control_flow.md#control-flow-in-full) |
| a borrowed `Vector` item kept, or read after the vector moved | compile error | [memory.md](memory.md#borrowed-items-of-a-vectort) |
| an item lent to the caller retained and released as if owned: narrowed by `if`, called as a function value or through a `type` | the narrowed name stays borrowed; the other two are compile errors | [memory.md](memory.md#borrowed-items-of-a-vectort) |
| a C compile that reports success but leaves no executable, or two builds sharing one generated C file | the build is an error naming the missing file; each output gets its own generated C file | [compiler.md](../docs/compiler.md) |
| two threads writing a singleton at once | the compiler makes the singleton safe; anything else a `Parallel` reaches is an error | [concurrency.md](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting) |
| a `while true` loop holding a singleton's lock forever | compile error | [concurrency.md](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting) |
| a loop polling a `Concurrent`'s `finished` while nothing steps it, spinning forever | halts after a million polls in a row with no step | [concurrency.md](../docs/concurrency.md#choosing-where-concurrents-resume) |
| `call_function()` skipping a function that takes arguments | halts naming the function | [testing.md](../docs/testing.md) |
| C calling a function through a `ForeignCallback` that was dropped | halts naming the function and the line that made it | [foreign_libraries.md](../docs/foreign_libraries.md) |
| a `while true` that can never leave and calls nothing | compile error | [control_flow.md](../docs/control_flow.md#while-is-the-only-loop) |
| work given to a `Parallel` that loops forever and never waits | compile error | [concurrency.md](../docs/concurrency.md#the-thread-pool) |
| a peer that hung up read as a count of `-1` | `socket.closed` turns `true`; reads answer `0` or `null` | [standard_library.md](../docs/standard_library.md) |
| an impossible date such as `Date(2023, 2, 29)` rolled over | halts; text from outside is read with `TimeText`, which answers `null` | [time.md](../docs/time.md) |
| a `crash` or `assert` whose site cannot be found again | every build writes `<program>.crashes`, one line per site | [what a crash reports](#what-a-crash-reports) |

`assert` doubles as the way to prove a `T?` is not null without nesting: after `assert value` on a
`T?` local, parameter, or field, `value` is a plain `T` for the rest of that block and any block
nested inside it. Reads, writes, and method calls all go straight to the value the `T?` holds, and ownership
and dropping still belong to that `T?` (assigning a new `T` through the narrowed name drops the old one first,
exactly like overwriting any other owning slot). `assert value and
other_condition` narrows `value` too, and `other_condition` itself already sees the narrowed type. Assigning
the narrowed name something that may be null (`value = null`) makes it a `T?` again from that line, so reading
through it afterwards is the usual may-be-null error (`diagnostics/narrowed_name_reassigned`; the rule for paths
is below). A value type's narrowed `T?` is assigned the same way: `got = next`, with `got` and `next` both
narrowed `Long?`s, stores the whole `Long?` (its presence and its value) into `got`, inside a loop too, and
`got = missing` with a `Long?` that may be null makes it a `Long?` again (`conformance/stage6/narrowed_value_assignment`).

```gdscript
var content = program_file.read()
assert content
var length = content.length()          # content is a String here, not String?
console.print(length)
```

Narrowing tests presence and never the value it holds: a `T?` of a number, an enum or a `String` holding `0`,
`0.0` or `""` is present, and so is an in-range `[]` read of an element holding it. A value type's `T?` carries a
presence flag beside the value (`has_value`), a reference's `T?` is its pointer, and a `[]` read goes through
`get_at`/`get`, which answer that flag, so no representation uses a sentinel and no value can be mistaken for
absence (`conformance/stage6/present_zero`, which narrows zeros, `false` and `""` from lists, a dictionary, an
`Integer?` result, `first()`/`last()` and `remove_first()` by `crash`, `assert`, `if` and `and`). A `Boolean?` is
never a condition but in `assert` and `crash`, which ask for `true` (below), so a `false` that is there is never taken for a
missing one.

`if` on a `T?` narrows the same way, in place, for the whole block: `if value { } else { }` runs the
block with `value` already a plain `T`, and the `else` exactly when it is null/absent. One rule for narrowing
everywhere: `assert`, `if`, and `switch` all read/write/call straight through to the value the `T?`
still owns, and there is no other form.

**Lint:** a function whose body's *last* statement is `if value { ... }` **with no `else`** (wrapping
the rest of the function only to check existence) is a compile error, because it should be written with
`assert` instead. A narrowing `if` with an `else` already handles the missing case explicitly, so the no-
else lint never applies to it:

```gdscript
# error: rewrite this with assert
func read_first_line(file: File): String {
    var content = file.read()
    if content {
        return content.lines().first()
    }
}

# the fix:
func read_first_line(file: File): String {
    var content = file.read()
    assert content
    return content.lines().first()
}
```

A narrowing `if` that is not the function's last statement (more statements follow it), or one nested inside
another statement (an outer `if`/`while`/switch case) rather than being the function body's own tail, is not
a lint error.

**An `if` that only returns the default is an `assert`.** In a function returning nothing, a `T?`, or a `List`,
`Dictionary`, `Vector` or `Items`, an `if` with no `else` whose whole body is one `return` of the function's
default (`null`; an empty collection, `List<T>()`, `Dictionary<T>()`, `Vector<T>()`, `Items<T>()` or `[]`, in a
function returning one; or a bare `return` in a function returning nothing) is a compile error wherever it stands in
the function, inside a `while` or a nested `if` included, and the message names the `assert` of the opposite
condition:

```gdscript
func first_name(): String? {
    if names.is_empty() {    # error: this 'if' only returns the default 'null', which is how a guard is
        return null          #        written: write 'assert not names.is_empty()' and let the rest run unindented
    }
    return names.first()
}
```

It is the same rule as narrowing, so `if not value { return null }` becomes `assert value` and narrows `value`
for the rest of the block. In a function returning anything else (a number, a `Boolean`, a `String`, an enum or
an object) a guard `assert` is an error (below), so an `if` that returns a value there is an answer written
down, `return 0` and `return false` included, and is left alone. How the
condition is turned around: `==` and `!=` swap, `<` becomes `>=` and `>` becomes `<=` (and back), `not x` becomes
`x`, anything else becomes `not x`, and `and`/`or` are turned around by De Morgan, side by side, so
`if count < 0 or count > limit` becomes `assert count >= 0 and count <= limit`. An `else if` is covered too. In a
constructor, where `assert` is not allowed, the message names `crash` instead. `diagnostics/default_guard`,
`diagnostics/returning_guard`, `diagnostics/empty_collection_guard`.

**An `if` that leaves proves the opposite of its condition.** An `if` with no `else` whose block's last statement
is a `return`, a bare `crash`, or a `switch` whose every case's last statement is one of these proves, for the rest
of the enclosing block, what an `assert` of the opposite condition would: each
side of an `or` in its condition written `not path` narrows that path, as `assert path` does (the later sides
already see the earlier ones narrowed, so `if not found or found.count() == 0 { return 0 }` is one test), and every
other side proves the `[]` reads its opposite bounds (`if index >= names.count() { return "" }` proves
`names[index]`). The block itself sees none of it. A check that the `if` already proves is the usual "proves
nothing" error (`conformance/stage6/leaving_if`). Compile time only: the emitted test is the condition as written.

**`assert` narrows every link of a chain, not only the last.** Walking a
nullable structure would otherwise need one `assert` per hop, and a thousand asserts is not a language, it is a
tax:

```gdscript
assert class.namespace.namespace
console.print(class.namespace.namespace.name)
```

One `assert` proves the whole path, so `class.namespace` and `class.namespace.namespace` are both plain values
for the rest of that block and any block nested inside it. It is the same rule [narrowing](../docs/failure.md#narrowing) already has for
`assert value and other_condition`, applied along a member chain instead of across an `and`.
Across an `and`, every side narrows, for `assert`, `crash` and `if` alike, since each side is known true when the
whole is: `crash names[position] and ages[position]` proves both elements, as two `crash` lines would
(`conformance/stage6/and_narrowing`).

- It narrows the path asserted **and its prefixes**, nothing else. A sibling path stays `T?`: asserting
  `class.namespace.namespace` says nothing about `other_class.namespace`.
- A local holding a copy is its own path, so narrowing it says nothing about the path it copied
  (`diagnostics/namespace_nullable`). A local that only copies a name or a path so it can be narrowed is
  itself an error: "'watcher' only copies 'tracker' so it can be narrowed: narrow
  'tracker' itself ('assert tracker') and use it directly" (`diagnostics/copy_to_narrow`). Its scope: a `var` whose
  value is a bare name or member path of a `T?` type, which is then narrowed by any condition that tests it (`if
  watcher`, `if not watcher`, `while watcher`, `assert watcher and ready`; `diagnostics/copy_to_narrow_conditions`),
  is never assigned again, is not a `var` with a written type in a generic class (there it converts for some
  instance, as `var key: $value_type.key_type? = key_text` does), and whose source is not assigned later in the function either (a snapshot taken
  before the source changes is not a copy for narrowing). A local holding what a `[]` read answered (`var age =
  ages[name]`, `var cell = grid[row].cells[column]`) is not a copy: it is narrowed instead of reading the key twice.
- A check on something that cannot be null proves nothing and is an error naming the fix: `assert tracker`
  written twice, or `crash` on a path an earlier `crash` already narrowed (`diagnostics/check_proves_nothing`).
  For a `[]` read the message names what proved it, so the
  line can simply go: `crash codes[index]` inside `while index < codes.count()` is "'codes[index]' is already
  proven by the loop condition 'index < codes.count()', so this 'crash' proves nothing: remove it"; a count or
  bound check is named the same way, and anything else as "an earlier check"
  (`diagnostics/proven_element`). A `T?` of any type that is already narrowed names the check that narrowed it:
  "'tracker' is already narrowed by 'assert tracker' on line 8: remove this check"
  (`diagnostics/check_proves_nothing`, `diagnostics/known_fact_checked`); a value that was never a `T?` is "this
  value cannot be null here (it is a Tracker), so 'assert' on it proves nothing: remove the check". The other facts
  refused this way are listed in [proofs.md](../docs/proofs.md#proving-what-is-proven-is-an-error).
- `crash` (below) narrows a chain the same way, since it narrows exactly as `assert` does but never returns.
- Reading through a link that may be null and has not been narrowed is still an error. This removes the verbosity of
  proving a path, not the requirement to prove it.
- Assigning to a narrowed path, or to anything it reads through, undoes the narrowing from that point on: after
  `outer = Box()` or `outer.inner = null`, `outer.inner` is a `Box?` again. Assigning a value that cannot be null
  (a constructor, a literal, a name that is not `T?`) to the narrowed path itself keeps it narrowed, and still
  undoes everything narrowed beneath it. A narrowed *name* follows the same rule: `current = current.next` or
  `maybe = null` after `assert current` stores into the `T?` it really is and un-narrows it
  (`diagnostics/narrowed_name_reassigned`).
- **`while value` narrows its body like `if value`**: the
  condition is tested again before every pass, and a store that may be null undoes it for the rest of the pass, so
  `while current { ... current = current.next }` walks a chain with no `assert` inside (`examples/linked_walk`).
- Inside a `while`, undoing a narrowing that was made before the loop *and read inside it* is an error, because
  the next pass would read it unproven: narrow it inside the loop instead (`diagnostics/path_narrow_loop`). A read
  counts when the loop's condition names it, or when the body reads it before what undid it; a body that proves it
  again after what undid it and before reading it (`record_hit()`, then `crash target`, then `target.hurt(3)`)
  reads it proven on every pass, and is accepted (`conformance/stage6/proof_again_in_loop`,
  `diagnostics/call_undoes_proof`).


**Reading with `[]` answers `T?`.** `names[index]` and `table["key"]` may
not be there, so they are values that may be null, and an index or key that is a name, a path, a number or
quoted text makes the read a path that narrows like any other: `crash names[index]`, then `names[index].upper_case()`.
An index never holds another `[]` read:
`values[rows[index]]` is "'rows[index]' is read inside the index of 'values': compute it first into a named 'var'
and pass the name", found anywhere in the index (under arithmetic, `not` or a member read) and reported for the
first read found. A walk's template writes the hoisted form (`var stored_row = rows[attribute.index]`, then
`values[stored_row]`), which it walks exactly as it did the nested one ([memory.md](../docs/memory.md)).
How a read is proven:

- **A proven count proves the indices below it.** After `crash names.count() == 3` (or `>= 3`, or `> 2`),
  `names[0]` to `names[2]` are plain values; so are they inside `if names.count() > 2 { }`. `names.count() != 0`
  and `not names.is_empty()` prove `names[0]` the same way
  (`conformance/stage6/count_bound_proofs`). A count proves indices of a `List` only: a `Dictionary`'s keys need
  not be `0` to `count() - 1`, so after `crash names.count() == 1` a number-keyed `names[0]` is still a `T?`
  (`diagnostics/count_proves_no_key`).
- **A bound proves its index.** `index < names.count()` (or `names.count() > index`) in an `assert`, a
  `crash`, an `if`, or a `while` proves `names[index]` in what follows (the loop body, for a `while`) and
  `index < names.count() - 1` does too. On the left of an `and`, it proves the right side, so
  `while index < lines.count() and lines[index] != "end"` needs nothing more. Every side of an `and` holds in
  what the condition guards, so `while not found and index < names.count()` proves `names[index]` in the body.
  A list in a local, a parameter and an attribute are proven alike, and `assert`/`crash` on a read already
  proven is an error that says so (`diagnostics/proven_index_check`).
- **A bound past the index proves the reads below it.**
  `base + k < names.count()`, `k` a whole-number literal from 1 to 63, proves `names[base]`, `names[base + 1]`
  ... `names[base + k]`, each compared as printed; `base` is any call-free index. It is proven wherever
  `index < names.count()` would be (the forms above), and undone the same way. A bound on `base` alone proves
  only `names[base]`.
- **A count kept in a name proves as the count does.** `var count =
  names.count()` (also `names.count() - k`) records that `count` is at most the count; `index < count` then proves
  `names[index]` as `index < names.count()` would. Shrinking `names` or assigning `count` undoes it, and a loop
  that proves a read from it and then shrinks `names` is the error for a proof undone inside a loop. Only a
  `var` whose value is exactly the count (or the count less a literal) counts; nothing else is traced.
- **A read proven before a loop may be proven again inside it.** An
  `assert`/`crash` of a `[]` read inside a `while` is refused as proving nothing only when a proof made inside
  that same loop already covers it; a proof from before the loop does not, since an assignment in the loop would
  make the next pass read it unproven.
- **Assigning the list or the index undoes it**, as for any path ([narrowing](../docs/failure.md#narrowing)): `index = index + 1`
  un-proves `names[index]`, and so do `names.clear()`, `remove_at`, `remove_first` and `remove_last`. Inside a
  loop, undoing a proof made before the loop that the loop has read is an error.
- **Any index without a call is a path**: `crash glyphs[code - 32]`
  proves `glyphs[code - 32]` for what follows, not only `glyphs[index]`. The index may be names, member reads,
  numbers, `true`/`false`, enum values, operators and other `[]` reads; two indices are the same when the formatter
  prints them the same, so spacing and redundant parentheses do not matter. Assigning any name the index
  reads undoes it, as assigning the index itself does. A call anywhere in the index keeps the read a plain `T?`,
  since the call could answer something else the second time. A call *between* the check and the read undoes it
  only when the call may change the list or what the index reads (next). `conformance/stage6/structural_index`,
  `diagnostics/structural_index_undone`.
- **A call undoes a proof only if it may change what the proof depends on**, with no `const` keyword to declare:
  the compiler is smart enough to tell whether going down into that function has any chance of altering the value.
  This covers every proof, a
  narrowed path or attribute and a proven `[]` read alike. The compiler follows the called function and what it
  calls, and collects which attributes it may assign and which lists it may shrink (`clear`, `remove_at`,
  `remove_first`, `remove_last`, `remove_swapping`, `remove_where` and `remove_where_<member>`, `truncate`, `swap`
  (it changes which item an index names), and `remove`); a proof that reads through one of them is undone, and reading it again
  unproven is the usual error. How it is followed: an attribute is known by its
  class and name, so assigning `Scope.owned` does not undo a proof about `Monster.owned`; a receiver's class is
  read from declared types (a parameter's type, a `var` built by a constructor or annotated), a `List`,
  `Dictionary` or `String` receiver included, so `items.count()` reaches only `List.count` and never a program's
  own `count`; a receiver that is itself an expression has the class of that expression's type: the class a
  constructor makes (`File(path).read()` reaches `File.read`), the class the called function returns
  (`names.copy().count()` reaches `List.count`), the element class of a `[]` read on a `List`, `Vector` or
  `Dictionary` (`lists[0].count()`), an attribute read's declared class (`receiver_call_effects`); a call whose
  receiver's class cannot be told (a shape, a type parameter, a function value) is followed into every function
  of that name; any of those shrinking calls but `remove` on a collection reached through `[]` or a
  call rather than a name may be the very list a proof reads, so it undoes every proof about a list
  (`append`, `prepend` and `insert` there count as growing a borrowed `Vector`'s storage); the calls a `return` makes undo nothing, since nothing after it runs on that path (a call in a branch
  before its `return` still undoes proofs after the branch); a list passed to a function that
  shrinks its parameter undoes proofs about the list passed; calling a function value may do anything, so it
  undoes every proof that reads an attribute or a list; operator functions and getters are not followed; a
  constructor's own assignments are to the new object and change nothing proven. A local that aliases an
  attribute's list is not tracked (a proven read still checks its bounds). Inside a `while`, a call that undoes a
  proof made before the loop and read inside it is an error naming the call (`diagnostics/call_undoes_proof`).
  All of it happens while compiling; nothing is emitted.
- **A proven read still checks its bounds at runtime** and halts out of range, naming the read and where it is
  (`spite: 'names[index]' is outside its list: a bound proves only the top of an index, and this one is below 0 or
  the list changed, at ...`), so a counter that went negative stops the program rather than reading memory it
  does not own or answering a default. The read is the collection's `get_at`, which compares the index
  once; a counted loop proves both ends and reads with no test at all.
- **Every `[]` answers `T?`.**
  `List`, `Dictionary`, `Vector` and `Items` alike, and every class that declares `get_at`: the
  function must answer a `T?`, and one declared with a plain `T` is `'get_at' answers 'Integer', but it is what
  '[ ]' calls, and every '[ ]' answers a 'T?' that is narrowed before use: declare it to answer 'Integer?', and
  answer null when nothing is at the index` (`diagnostics/plain_get_at`). The proofs above (counts, bounds, a
  literal's items, the call effects and counted loops) hold for the library's collections, whose
  `count()` and `get_at` the compiler knows; a read through a program's own `get_at` is narrowed only by what the
  program writes (`diagnostics/indexable_unproven`). Calling `get_at(index)` by name answers the same `T?`, but a
  call is not a path, so narrow `list[index]`
  instead. A read used before it is narrowed is an error that names the read and the lines that would
  narrow it: `crash`, `assert` and `if` on the read itself, then the count or loop bound that proves it for a
  library collection ([on the page](../docs/failure.md#reading-with--answers-t), `diagnostics/index_reads`).
- **A `Boolean?` cannot be a condition**, not in `if` or `while`, and not under `not` or `or`: `if
  flags[index]` would test that the element is there, not that it is true, and the two mean opposite things for
  `false`. Prove the element is there first, or `switch` over it; `== true` also works, since comparing needs no
  narrowing.
- **`assert flag` on a `Boolean?` means the flag is true**, as it does on a `Boolean`: it passes when the flag is
  there and not `false`, and fails (the function answers "nothing") when it is `null` or `false`. **`crash flag`
  reads it exactly the same way**: it halts unless the flag is there and `true`
  (`conformance/stage6/crash_maybe_boolean`). A side of an
  `and` under `assert` reads the same way. After it the flag is a `Boolean`. Behaviour that depends on whether the
  flag is there is written by testing the value (`if flag == true`, `if flag == false`) or by a `switch`; there is
  no `flag != null` form (`conformance/stage6/assert_maybe_boolean`, `diagnostics/maybe_boolean_conditions`).
- Writing through `[]` is unchanged, and **assigning through a `T?` is an error** naming the fix: "this value
  may be null (it is a Box?), so 'label' cannot be assigned through it yet: narrow it first with 'if value { }',
  'assert value' or 'crash value'" (`diagnostics/store_through_nullable`).

`first()` and `last()` answer a `T?` too, `null` on an empty list, by the same argument, and so do `remove_first()`
and `remove_last()`: nothing to take from an empty list is a normal outcome. `tests/list_tests`,
`diagnostics/index_reads`, `conformance/stage3/lists`.

**Comparing needs no narrowing.** `==` and `!=` accept a `T?` on the left:
null is equal only to null, so `crash Spite.Class.namespace == "Spite"` is a whole test, and two missing values are
equal. Either side may be the `T?`. Only the comparison
is exempt: reading a member through a `T?` still needs it narrowed first. A `Spite.Namespace` compares with
text through `equals(String)` in `library/spite/namespace.spite`, against its `name_with_namespaces`; two
namespaces still compare by identity, because a class's `equals` is used for a right side of that same class
only when `equals` takes that class.

## Failure: three outcomes and no others

Spite has **a compile error, an `assert`, or a crash**. There are no
exceptions, no error unions, no bubbling, and no error value carrying a message, because errors and exceptions
tend to be useless: they carry a message the program has no action to take on.

1. **A compile error**, for anything the compiler can know. This is already where the language keeps landing:
   an unserialisable type ([Foreign libraries](foreign_libraries.md#foreign-libraries)), the wrong target ([targets.md](../docs/targets.md)), an unfilled codegen hole ([Codegen values (`$`)](metaprogramming.md#codegen-values-)), a
   missing foreign symbol ([Foreign libraries](foreign_libraries.md#foreign-libraries)), a member that does not fit a template ([Standard library metaprogramming](collections.md#standard-library-metaprogramming)), a symbol outside its
   enum ([Types](values_and_types.md#types)).
2. **`assert`**, for when the program should keep running.
3. **`crash`**, for when everything should stop so the code gets rewritten.

**What `crash` is for**: what the compiler can prove away, so a program written with
its help never meets it; what leaves the program unable to work at all; and a developer's mistake: a whole
number divided by zero ([values_and_types.md](../docs/values_and_types.md)), arithmetic that does not fit its
type, or a value its narrower name cannot hold ([values_and_types.md](values_and_types.md#numeric-types)), a `Float` gone to infinity that `JsonWriter`
is asked to write ([json.md](../docs/json.md)). A condition the program can meet in normal use (a missing file,
a user's bad input, an absent record) answers `T?` or an empty value, never a crash, and a library
`crash` must be one the compiler can show the program how to avoid, or a bug in the program that made the value.

`T?` is the only runtime failure value, and it carries no reason. A caller narrows it with `assert`,
halts on it with `crash`, or handles the absent case with `if value { } else { }`.

**If a distinction is actionable, it is data, not an error.** A caller that
must tell a timeout from a rejection takes back a `type`, `union` or enum modelling exactly that: ordinary
data with ordinary handling. Something you can act on was never an error; it was a value that was not modelled.

### `assert` is control flow

`assert` is a guard clause, not validation: when the value is absent there is no more logic to do here, but the
program is fine and keeps serving everything else. It is why a web
server or a game written in Spite should rarely crash.

**A guard `assert` answers "nothing", so it is allowed only where the result can say it.** A failed `assert`
returns at once, answering:

- nothing, in a function returning nothing;
- `null`, in a function returning a `T?`;
- an empty one, in a function returning a `List`, `Dictionary`, `Vector` or `Items`.

In a function returning a whole number or a `Float`/`Double` (`0`), a `Boolean` (`false`), a
`String` (`""`, since an empty text is a real text), an enum (its first value), a `Memory.Address`, a union or a
class that cannot be null (an object with every attribute at its default), an `assert` is a compile error, since
the caller would carry on with a default it cannot tell from a real answer:

```
this 'assert' would answer a default Integer (0) that a caller cannot tell from a real one: return a value ('if entity >= rows.count() { return -1 }'), make the result 'Integer?', or 'crash entity < rows.count()' if this is a developer mistake
```

The message names the opposite condition, an answer of the type (`-1`, `-1.0`, `false`, `""`, the enum's first
value, or `Mark()` for a class that is made with no arguments and is not a singleton), the nullable result (left out for a `Boolean`, since a `Boolean?` is never a
condition) and the `crash`. It is checked against the result type of each function as compiled, so in a generic
class it is checked per instance, and an `assert` whose condition is decided while compiling
(`assert $slot_type == Entity`) is held to it too: its instance would answer the default every call. `crash` is
never limited this way. Which of the three a site takes is the writer's decision: a `T?` when the case is
met in normal use, a value chosen on purpose when one outside the real answers already means "none", a `crash`
when asking is the caller's bug. Every answer the message offers is code that compiles where it stands: for a class
made with arguments, a singleton, or a function of a generic class, it offers no answer at all, only the `T?` and
the `crash`: `this 'assert' would answer a default Badge (an object with every attribute at its default) that a
caller cannot tell from a real one: make the result 'Badge?', or 'crash ready' if this is a developer mistake`.
A game engine met it three times in a day: `World.create_entity_from_bundle(): Entity`
answered a default `Entity` instead of the one just made, `deform_layer(): Integer` answered offset 0 for a mesh
without skin, `object_world(): Math.Matrix4` answered a default matrix for an object without a parent
(`diagnostics/default_answer`, `conformance/stage6/leaving_if`).

```gdscript
func first_name(): String? {
    assert not names.is_empty()      # a failed assert answers null
    return names.first()
}

func is_open(): Boolean {
    if handle == -1 {                # a Boolean cannot say "nothing": the answer is written down
        return false
    }
    return true
}
```

**Every path of a function that declares a result ends in a `return`.** A path that reaches the end of the function's body is a
compile error at the line of the function's closing `}`, and the message names the path, one step per branch it
took: `'sign_of' answers a String, but when 'value < 0' is false (line 12, an 'if' with no 'else') it reaches its
end without a 'return', so a caller would get a String nobody wrote: end that path with 'return <value>', or with
'crash' if it cannot happen`. Without the check, the C the compiler writes would answer the type's default there, so the function
would quietly answer `0`, `""`, `false`, a default object or `null`. A path ends:

- at a `return`, or at a bare `crash` (`crash false` is formatted to it);
- at an `if` with an `else` whose two branches both end, and at a `switch` whose cases all end (a `switch`
  covers every member or value, or has `_:`, so there is no other way through it);
- at a `while true`, which ends only by leaving the function, since Spite has no `break`;
- at an `if` whose condition is decided while compiling (a codegen value, `functions['run_each']`, ...) and whose branch
  taken ends, and at an `assert` so decided to be false, read per instance of a generic class, as they are
  compiled.

Everything else (an `if` with no `else`, the path where its condition is false; a `while` with any other
condition, the path after it ends; an `assert` or a `crash` with a condition; any other statement) lets the
path through. A result that is a `T?` is held to it too: `return null` is written where that is the answer. The
check reads each function as it is compiled, so a function the program never reaches is not checked, and a
generic class's function is reported once, for the first instance that falls off, naming it (`'get_at' answers a
String in List<String>`). It costs nothing at run time. `diagnostics/falling_end`.

**`assert` is banned in a constructor**, including the entry class's own. A constructor is setup, not
logic: if something can be invalid inside one, function logic has been put where only setup belongs. The caller
receives an object with every attribute at its default and cannot tell it from a properly built one, which is
exactly the surprise the rule exists to prevent. A program constructor that grows complex splits into smaller
functions it calls, and reads as a table of contents for the program, with the logic one level down. The
error is "'assert' is not allowed in a constructor: a constructor is setup, not logic. Take already resolved
values, or use 'crash' when absence is a bug", and an `if` that only leaves a constructor is "this 'if' only
leaves the constructor, and a constructor is setup, not logic: take already resolved values, or write 'crash
...' when this is a bug" (`diagnostics/default_guard`). `crash` is allowed in a constructor.

### `crash`

`crash` is a keyword, so the compiler takes its context from the program rather than
from a message string. **It mirrors `assert` exactly** (same polarity, same shape, different severity):

```gdscript
assert database.connect()        # falsey: return the default, keep going
crash  database.connect()        # falsey: halt
```

The compiler captures the condition's source text and every operand value, so nothing has to be written and
nothing can drift out of sync. **A bare `crash` is the one form for a branch that cannot happen**: it reports its
enclosing context, and the formatter rewrites `crash false` to it. `crash
user` narrows a `T?` exactly as `assert user` does, but never returns.

Absence is fine -> `assert`. Absence is a bug -> `crash`. Absence is meaningful -> `if value { } else { }`.

**`crash` never takes a message, and neither does `assert`.** There is no syntax for one: a report points at the
code and shows the memory, so the reader reads the source line and the data instead of prose about them. **The
report:** the crash line names the site (id, `path:line`, class, function) and carries the values that matter
there: the condition's operands, then the function's parameters
and locals in scope and the attributes of the object it runs on, each once as one `name=value` field. Location and
memory are enough, so neither the `spite.crash` line nor a `spite.assert` line carries the condition; the
`.crashes` map, which costs the executable nothing, keeps it for `grep`. **What is shown:** a value in scope is shown
when it is text, a number, an enum,
or a `T?` of one (`name is null` when empty); an object, a list, a function or a row is left out, since showing
one would call code (`to_string()`) or walk memory while the program is failing. A name the condition mentions is
left out, since its part already reported it (and a narrowing crash has just proven it absent). At most 12 values
are shown, locals and parameters first in the order they were declared, then the attributes in declaration order,
and a text shows its first 80 bytes (cut back to a whole UTF-8 character) and then `...(<length> bytes)`. The
values are read where they are, as the C locals and the object's fields, with no reflection tables, so they cost
the code of the failure branch only. A condition written only to put text into the report (`crash
bone_matrix_is_finite or bone_name == ""`) has no reason to exist. In `library/`, `JsonReader.read_or_crash` sets
the cursor's `crashes`, and the cursor crashes where it finds the fault, so the
report shows what it wanted, the position and the text there; `JsonWriter.written_decimal` crashes on
`shown - shown == 0`, which is false for infinities and NaN, and the report shows the value and the attribute's
`path`. `conformance/stage6/crash_scope_values`, `json_crash`, `json_infinity`.

### What a crash reports

A crash always reports backend information **and the trace of every `assert` that failed before it**. Those
asserts are the causal trail explaining how the program reached the state that crashed, which is usually several
frames earlier than the crash itself. The cost is only on the failure path (a passing `assert` already
branches), and the trace is a fixed-size ring buffer with a total count, so it never allocates and cannot grow
without bound. A report is the crash's own line, the asserts' lines (each with what its function answered:
`answered=nothing`, `answered=null` or `answered=empty`, and `repeated=<count>` when the same site failed several
times in a row, which takes one line in the ring) and the call chain, one `spite.frame` line per Spite
function on the crashing thread's stack, innermost first, written as a native fault writes its frames and walked
the same way (frame pointers on Linux and macOS, so an `--optimized` build there has none; the unwind tables on
64-bit Windows), leaving out the runtime's own functions above the first Spite one. Compile-time evaluation ([the functions of `Spite.Class`](../docs/reflection.md#functions-of-spiteclass-and-no-static-functions), [JSON is reflection, not a library](json.md#json-is-reflection-not-a-library)'s generated JSON) reports
the same way.

**A failed `assert` is not printed when it fails; `--trace-asserts` streams them.** A failed `assert` writes nothing
as it fails, in every build, `--debug-memory` and
inspectable builds included: it stores its site's line in the ring and counts it, and the line reaches the error
stream only in a crash's report, or a native fault's. `trace_asserts` (`--trace-asserts`, default `false`) opts
in to streaming: `SPITE_TRACE_ASSERT` then also flushes the program's output and writes the site's `spite.assert`
line to the error stream when it fails, in a program that cannot crash too. It is the same fixed line the ring
holds, with no values, and it covers the asserts the ring covers (the program's and its loaded packages', never
`library/`'s). Cost: none without the flag, since the ring's update is the same either way; with it, a
flush and a write per failed `assert`. `conformance/stage6/quiet_asserts` (four calls, two failing: nothing but the
program's lines), `conformance/stage6/trace_asserts` (the same with `--trace-asserts`),
`conformance/stage6/quiet_asserts_optimized` (an `--optimized` build failing a guard 500 times: nothing but the
program's line) and `conformance/stage6/trace_asserts_optimized` (`--optimized --trace-asserts`).

**A crash prints the ring as it stood when it began, and only the first crash reports.** The report reads the
ring's count once, when it starts,
and prints at most 32 lines and the `earlier=` count from it. Reading the count again on every line would let a
crash on one thread, while pool threads kept failing guard asserts, chase the count forever: every failed `assert`
of the program would reach the error stream, and the crash would never reach its `exit`. A native fault's report reads the
count once the same way. A crash first claims the report with one atomic exchange; a thread that crashes while
another's report is being written waits for the program to end instead of interleaving a second report. Cost:
only on the crash path. A failed `assert` costs one call to the ring's update, kept out of line so the function
around it stays small: a comparison with the newest line and a count, or a new line; in a program with threads, a
compare-and-swap on a word that holds the site, the line's place and its count together, so a count can never land
on another site's line. Measured with `--optimized`: a loop failing a guard two times in three, 200 million times,
runs in the time it takes with no ring at all, within the machine's noise, so every build keeps the ring.

An `assert` in `library/` never enters the trace. The trace explains how *the program* reached a crash,
and the standard library's asserts are routine answers (a missing key, text that does not match, a read past the
end) that fire constantly and would push the program's own entries out of the ring. The compiler leaves the ring
write out of every `assert` site whose file is in `library/`, so a library guard costs what an `if` costs; the
program's own files and every `load`ed package keep recording, and a crash inside the library still reports its
own site (`conformance/stage5/library_guards_untraced`).

**A crash site is identified by a compile-time id, and the compiler emits an id-to-source map as a build
artifact** rather than embedding the information in the program. The binary carries none of it, which matters
most in a wasm module where size is startup time, and **the same logical site keeps the same id across every
target**, so a crash from the server bundle and one from the browser bundle compare directly. The id is
content-derived (a hash of namespace, class, function, the site's ordinal and the condition source), never a
counter, because a counter renumbers everything below an inserted line and destroys both old reports and
cross-target identity. No build carries a condition's text, and an `--optimized` build carries
only each site's id: its crash line is `spite.crash<TAB>id` followed by the values, and its trace line
`spite.assert<TAB>id<TAB>answered=...`, while other builds also write the place, class and function so a local run needs no lookup
(`conformance/stage6/trace_asserts_optimized`).

The map is a greppable tab-separated table named after the program, `<program>.crashes`, beside the
executable, one line per site, sorted by id so archived maps diff cleanly: id, file, line, column (of the `assert` or `crash` keyword, counting from 1),
class, function, kind (`crash`, `assert-predicate`, `assert-narrowing`), the condition source, and the
operand names with their types (`value:Integer`). It opens with a `#` comment line carrying the format version
and the build hash (`# spite crashes 1 2660c8aa`). At runtime a crash emits one tab-separated line prefixed
`spite.crash`, and an assert `spite.assert`. **A crash renders its values as text; an assert keeps none**: a crash happens once and then the program is dead,
so it should be informative rather than fast, while an assert may fire thousands of times an hour and must stay
cheap until something dumps it. A ring line is a pointer to its site's fixed `spite.assert` line and how many times
in a row it failed, so its report and its trace line name the site without values.

A crash and its report work as follows. Crash and assert sites carry content-derived ids, every
build writes `<program>.crashes`, and a crash prints the asserts that failed before it, oldest first, from a ring
of 32 lines, a site failing several times in a row as one line with `repeated=<count>`, and a
`spite.assert	earlier=<count>` line when the ring held more lines than it kept
(`conformance/stage5/crash_report`). A firing `crash` flushes stdout, writes its line to stderr, and exits with
status 1:

```
spite.crash<TAB>id<TAB>path:line<TAB>Class<TAB>function<TAB>name=value...
```

The line carries every part of the condition and its value (`value=-9<TAB>limit=0`), with calls never
evaluated a second time, and then the values in scope. **Every name and call in a
condition of any shape is reported.** The compiler walks
the condition as written and reports, left to right and each once by its text: every name or path (a `[]` read
whose index has no call included) with its value, every call with the answer it gave, and inside a call its
receiver and its arguments. `and` and `or` are followed into, and so are `not`, comparisons and arithmetic; a
literal adds nothing. A value is written as for one operand: text, a number or an enum as it is, an object through
its `to_string()`, anything else left out; a `T?` that is null is `name is null`, and a `[]` read that is missing
is `list[index] is missing: index 5, count 2` or `table[key] is missing: key "bea"`, as a failed
narrowing words it. **Only what ran is reported:** when the crash fires, each side of a failed `or` ran, and so did each side
of an `and` under a `not`; a side an `and` or `or` may have skipped is reported only if it ran, which a flag set as
it runs records, and a path there is reported only when it is a bare name, so nothing is read that the condition
did not prove readable. **A call is never evaluated twice:** a call whose answer is reported is kept, as the
condition runs, in a local beside a flag saying it ran, and the report reads the local; a call answering an object
it owns hands it to that local, which releases it after the check. A call that is the whole condition, or its
`not`, is not reported, since its answer is what failed. A name that is a class or a namespace (`Math.pi`'s
`Math`) is not a value and is left out. **Cost:** the report is written only when the crash fires; on the passing
path, a condition with a reported call inside it stores the call's answer and a flag, and one with a skippable side
stores a flag (stores into locals that only the failure branch reads, which the C compiler keeps in registers or
drops); a condition of names and comparisons stores nothing. The `.crashes` map lists every part with its type
(`record:String? cooked.count():Integer cooked:List<String> never_cooked_id:String name:String`, a receiver whose value the report leaves out included). `conformance/stage6/crash_compound`.
**A condition that is a call on a value**, `crash file.exists()` or `crash not list.contains(x)`,
has that value as its operand when it is a name or a path: text, a number or an enum is written as it is, and an
object whose class declares `to_string()` with no arguments is written as what that answers, called once on the
way out: `settings=saves/settings.txt`, since `File`'s `to_string()` is its path. A value
with no `to_string()` (a `List`) adds nothing, and the call itself is not evaluated again. The `.crashes` map
lists it with its type (`settings:File`; `conformance/stage5/crash_receiver`).

**A failed narrowing reports what is missing, never a value.** A `crash` that narrows a `T?` (a name, a path, a
`[]` read, each side of an `and`) fails only when something is absent, and printing a value
there would print a default (`clip.keys[start + 9]=0`), which reads as a present zero and sends the reader after
a bug that does not exist. The report tests each nullable link of the path again, in order, and names the first
one that is absent: `rig.skeleton is null` for a name or member, `list[expression] is missing: index 11990, count
11500` for a list read (the index as evaluated, and the list's count), `table[key] is missing: key "bea"` for a
dictionary or any other `get_at` (a `String` key quoted, so an empty one shows). The index is evaluated again, which
is safe because an index with a call in it is not a path. The value report stays for every condition
that is not a narrowing (`conformance/stage6/crash_missing_item`, `crash_missing_key`, `crash_missing_link`,
`crash_missing_value`). Cost: only on the failure path, which already halts; a passing `crash` is the same
presence test it always was.

### What a native fault reports

Anything that can go wrong silently is a bug, so a native fault (an access violation
or `SIGSEGV`, a stack overflow, an illegal instruction, `SIGBUS`, `SIGFPE`) never ends a program with nothing
printed.

- **Every program installs a fault handler**, in every build, as the first line of `main`: on Windows
  `SetUnhandledExceptionFilter` and `SetThreadStackGuarantee` (16 KB kept back, so a stack overflow can still be
  reported on the thread that overflowed); on Linux and macOS `sigaction` for `SIGSEGV`, `SIGBUS`, `SIGILL`,
  `SIGFPE`, `SIGTRAP` and `SIGABRT`, run on an alternate signal stack of 64 KB per thread. A corrupted heap is
  reported the same way on every system: on Linux and macOS the C library finds it, prints its own message and
  aborts, and an abort whose stack passes through the C library's allocator (`malloc`, `free`, `realloc`,
  `calloc`, by the names the dynamic linker gives the return addresses) is `heap-corruption`, any other `abort`.
  Reading the stack for that is the one thing the handler does that the C library does not promise is safe in a
  signal handler, so the program reads it once at start, which loads what reading it needs before anything can
  break. Every thread the program starts
  (a pool runner, a waiting call's thread, the REPL's and the file watcher's) sets up its own room the same way.
  An unhandled exception only: a fault a foreign library catches itself never reaches it. On Windows a corrupted
  heap (`0xC0000374`) never reaches the unhandled-exception filter, since the system ends the program straight after
  raising it, so the same report is also installed as a vectored exception handler, `AddVectoredExceptionHandler`,
  which looks at that one code and passes every other exception on untouched (its cost is one comparison per
  exception the process raises, and Spite raises none).
- **The handler allocates nothing and calls nothing that could**: it builds each line in a buffer on its stack and
  writes it with `WriteFile` or `write`. It writes the `spite.fault` line first, from what is safe to read (the
  fault record, the faulting address, the function table, the thread's last foreign call), then the assert trace,
  then walks the stack, so a stack too damaged to walk still leaves the first line. A second fault on the same
  thread while reporting ends the program at once; a fault on another thread waits for the first report.
- `spite.fault<TAB>kind<TAB>path:line<TAB>Class<TAB>function<TAB>address=...<TAB>code=...<TAB>at=file+offset<TAB>foreign=...<TAB>library=...<TAB>from=path:line`,
  with `address=` only for a memory fault and `foreign=` only once the thread has called a foreign function; then
  one `spite.assert` line per failed assert still in the ring, exactly as a crash prints them, when the program keeps
  the ring; then `spite.frame<TAB>path:line<TAB>Class<TAB>function` per Spite function on the stack, innermost
  first, consecutive calls of one function as one line with `repeated=<count>`, at most 16 lines from at most 256
  frames, and `spite.frame<TAB>more` when there were more. Then stdout is flushed and the program ends with the
  system's own status for the fault: `TerminateProcess` with the exception code on Windows, and on Linux and macOS
  the default action of the signal, raised again (a core dump where enabled).
- **Which function** comes from a table the compiler writes after every other function: each function the C
  keeps after tree shaking, with its start address, the file and class, the Spite name and the line it starts on
  (`-` and the C name for the C the compiler writes itself, such as a class's `___release`). On 64-bit Windows the
  system's unwind data gives the exact start of the function an address is in, which the table then names, and the
  same data walks the stack in every build, `--optimized` included, with no frame pointers. On Linux and macOS the
  function is the one whose start is the nearest below the address, inside the program's own file, and the stack is
  walked by frame pointers: a build without `--optimized` always keeps them, and an inspectable build
  (`--development`, `--hot-reload`, `--repl`, `--repl-port`) is compiled with `-fno-omit-frame-pointer` there, so
  only an `--optimized` production build on Linux or macOS names the faulting function alone. Keeping them in
  production too was measured and not taken (the numbers are in
  [optimizations.md](../docs/optimizations.md#the-fault-handler-is-in-every-program)).
- In an `--optimized` build a function the C compiler inlined into its caller is reported as the caller. A
  function `--hot-reload` swapped in lives in the reload's library, so a fault in it names that library in `at=`
  and no Spite function.
- **The last foreign call**: each foreign call first stores a pointer to a fixed text naming the C function, its
  library and the calling line in a thread-local, in every build (one store per call measured as nothing
  against the call itself, so it is not kept to inspectable builds). A library reopened by `--hot-reload` records
  into its own copy, which the host's report does not see.
- **Not reported**: what the system ends without asking the program, such as a `__fastfail` (`0xC0000409`, which a
  stack-buffer check or the C runtime's own invalid-parameter handler raises), and anything while a debugger is
  attached, which sees the fault first. A fault on a thread a
  foreign library started is reported, but without room kept for a stack overflow and without a last foreign call.

The conformance programs are `conformance/stage6/native_fault_foreign`
(a null read inside a fixture library), `native_fault_stack` (endless recursion), `native_fault_illegal` (an
illegal instruction inside a fixture library) and `native_fault_heap` (a fixture library freeing a pointer inside
a block, which every system reports as a corrupted heap, with one expected output: `check.sh` leaves out the C
library's own message, the shell's `Aborted` notice, and the exception code or signal and the system file of that
one line).

---

Next: [Functions and operators](functions_and_operators.md).
