# Nullable values and failure

Spite has no exceptions, no error values and no `Result` types. Something that may not be there is a `T?`, and
something that went wrong has exactly three outcomes: a compile error, an `assert`, or a `crash`.

## Nothing fails silently

**Anything that can go wrong silently is a bug.** Every failure is loud: a compile error, or a crash that names its
cause. It is never a wrong value that looks like a right one, a write that is lost, a
step that is skipped, memory that leaks or a program that hangs. When the compiler can know about a mistake it
refuses the program; when only the run can find it, the run stops and says where. Every rule on this page, and
every open question about the language, is judged against this one.

What that already means in practice:

- A value that may be missing is a `T?`, and nothing reads it until it is [narrowed](#narrowing); a `[]` read
  and `first()` answer a `T?` instead of a made-up element ([below](#reading-with--answers-t)).
- A check that stops proving something (after an assignment, or a call that may change what it read) has to be
  made again ([a call may undo a proof](#a-call-may-undo-a-proof)).
- A guard `assert` stops a function only where its result can say "nothing"; a function answering a number, a
  `Boolean`, a text or an object writes its answer down ([below](#a-default-that-looks-like-an-answer-is-an-error)).
- A developer's mistake halts naming the line: a whole number divided by zero, arithmetic that does not fit,
  signed or unsigned, and a value too big for the narrower name it is put into, in every build
  ([values_and_types.md](values_and_types.md#arithmetic-that-does-not-fit-halts)),
  a `crash` that fails, a native fault ([below](#what-a-native-fault-reports)).
- A failed `crash` names what is missing instead of printing a default that reads like a real zero
  ([what a crash reports](#what-a-crash-reports)).
- Text that is not a number reads as `null`, never as a `0`: `"forty two".to_integer()` is an `Integer?`
  ([standard_library.md](standard_library.md#string)).
- An operand that would be cut to fit is a compile error, in arithmetic and in comparisons
  ([values_and_types.md](values_and_types.md#wider-arithmetic-goes-wider-operand-first)).

The whole list is in [the rules](../specs/failure.md#nothing-fails-silently-the-rule).

## `T?`: a value that may be null

`Monster?` is a `Monster` or `null`. It is not a special wrapper: it is the union of `Monster` and `Null`, a class
whose only value is the literal `null`, so everything that works on a union works on it. `null` exists only as the
empty state of a `T?`; every other type has a default value instead (`0`, `""`, `false`, a class's attribute
defaults).

```gdscript
var target: Monster? = null

func find_target(): Monster? {
    return null
}
```

A `T?` must be **narrowed** before a member is read, a function is called on it, or it is printed. There is no
`value == null`: comparing against `null` is an error naming the forms below:

```gdscript title=null_comparison_error/null_comparison_error.spite entry error
var console = Console()

func NullComparisonError() {
    announce(null)
}

func announce(name: String?) {
    if name == null {
        console.print("nobody")
    }
}
```
```diagnostic
'null' is not a value to compare against or pass around: narrow the value instead
```

## Narrowing

Narrowing proves a `T?` holds a value and turns it into a plain `T` in place: the same name, no new variable.
There is one rule, and every form below follows it.

| Written | When it is null |
|---|---|
| `if value { } else { }` | runs the `else`; inside the `{ }`, `value` is a `T` |
| `assert value` | the function answers "nothing" (below); after it, `value` is a `T` for the rest of the block |
| `if not value { return ... }` | the block runs and leaves; after it, `value` is a `T` for the rest of the block |
| `crash value` | the program halts; after it, `value` is a `T` |
| `while value { }` | the loop ends; inside the body, `value` is a `T` |
| `switch value { Monster: ... Null: ... }` | runs the `Null` case |

**Narrowing tests presence, never the value.** A `0`, a `0.0`, a `false` or a `""` that is there is present:
`crash keys[index]` passes on an element holding `0.0`, and `if count` on an `Integer?` holding `0` runs its
block. A plain `Boolean` condition is the only test of a value, which is why a `Boolean?` cannot be one
([below](#reading-with--answers-t)), with one exception: `assert flag` and `crash flag` on a `Boolean?` mean the
flag is true, so they pass only when the flag is there and not `false`, exactly as they do on a `Boolean`.

`assert value and other_condition` narrows too, and `other_condition` already sees the narrowed type. Each side of
an `and` narrows on its own, for `assert`, `crash` and `if` alike, since each is known true when the whole is:
`crash names[position] and ages[position]` proves both elements, exactly as two `crash` lines would.

```gdscript title=nullable_narrowing/monster.spite
var health = 10

func Monster(starting_health: Integer) {
    health = starting_health
}
```
```gdscript title=nullable_narrowing/nullable_narrowing.spite entry
var console = Console()

func NullableNarrowing() {
    report_health()
    var name = find_name(false)
    if name {
        console.print("found", name)
    } else {
        console.print("missing")
    }
}

func find_monster(missing: Boolean): Monster? {
    assert not missing
    return Monster(30)
}

func find_name(missing: Boolean): String? {
    assert not missing
    return "kal"
}

func report_health() {
    var target = find_monster(false)
    assert target
    console.print("health", target.health)
    var missing_target = find_monster(true)
    if missing_target {
        console.print("should not print", missing_target.health)
    }
    console.print("done")
}
```
```output
health 30
done
found kal
```

### Narrowing a path

One `assert` proves a whole chain of nullable links, not only the last one: after `assert start.next.next`,
both `start.next` and `start.next.next` are plain values for the rest of the block. It narrows the path and its
prefixes and nothing else: a sibling path stays a `T?`. `while current` narrows its body the same way, and is how a
linked chain is walked:

```gdscript title=path_narrowing_doc/node.spite
var name = ""
var next: Node? = null

func Node(new_name: String) {
    name = new_name
}
```
```gdscript title=path_narrowing_doc/path_narrowing_doc.spite entry
var console = Console()

func PathNarrowingDoc() {
    var first = Node("a")
    var second = Node("b")
    second.next = Node("c")
    first.next = second
    walk(first)
    third_of(first)
}

func walk(start: Node) {
    var current: Node? = start
    while current {
        console.print("visit", current.name)
        current = current.next
    }
}

func third_of(start: Node) {
    assert start.next.next
    console.print("third", start.next.next.name, "after", start.next.name)
}
```
```output
visit a
visit b
visit c
third c after b
```

Assigning to a narrowed path, or to anything it reads through, undoes the narrowing from that point on:
`current = current.next` above stores a `Node?` into `current`, and the loop's condition proves it again before
the next pass. Inside a `while`, undoing a narrowing that was made *before* the loop and is read inside it is an
error, because the next pass would read it unproven: narrow it inside the loop instead. A body that narrows it
again after what undid it, before reading it, reads it narrowed on every pass, and is accepted.

Narrow the name or the path itself, never a copy of it: a local that only copies a value so it can be narrowed
is an error.

```gdscript title=copy_to_narrow_error/tracker.spite
var drops = 0

func note_a_drop() {
    drops = drops + 1
}
```
```gdscript title=copy_to_narrow_error/copy_to_narrow_error.spite entry error
var tracker: Tracker? = null

func CopyToNarrowError() {
    tracker = Tracker()
    record()
}

func record() {
    var watcher = tracker
    assert watcher
    watcher.note_a_drop()
}
```
```diagnostic
'watcher' only copies 'tracker' so it can be narrowed: narrow 'tracker' itself
```

A check on something that cannot be null proves nothing, and is an error too: `assert tracker` written twice,
or an `assert` on a plain `Tracker`. The error names the check that already proved it ("'tracker' is already
narrowed by 'assert tracker' on line 8: remove this check"), whatever the type, text and numbers included.

### A call may undo a proof

A call between a proof and a read undoes the proof only when the call may change what the proof depends on. The compiler follows the called function, and everything it calls, to see
which attributes it can assign and which lists it can shrink (`clear`, `remove_at`, `remove_first`,
`remove_last`, `remove_swapping`, `remove_where` and `remove_where_<member>`, `truncate`, `swap`, a dictionary's
`remove`). If it may change an attribute the proven path reads through (or the
list a proven `[]` reads, or anything its index reads), the proof is gone and the read must be proven again; if
it provably cannot, the proof stands and nothing more is written. There is no `const` to declare: the compiler
reads the code.

```gdscript title=call_undoes_proof_error/monster.spite
var name = "goblin"
```
```gdscript title=call_undoes_proof_error/call_undoes_proof_error.spite entry error
var console = Console()
var target: Monster? = null

func CallUndoesProofError() {
    target = Monster()
    show()
}

func show() {
    assert target
    forget()
    console.print(target.name)
}

func forget() {
    target = null
}
```
```diagnostic
this value may be null (it is a Monster?), so 'name' cannot be read from it yet
```

A call that cannot change the target keeps the proof:

```gdscript title=call_keeps_proof/monster.spite
var name = "goblin"
```
```gdscript title=call_keeps_proof/call_keeps_proof.spite entry
var console = Console()
var target: Monster? = null
var shown = 0

func CallKeepsProof() {
    target = Monster()
    show()
}

func show() {
    assert target
    count_shown()
    console.print(target.name, shown)
}

func count_shown() {
    shown = shown + 1
}
```
```output
goblin 1
```

What the compiler follows: calls by name, method calls through the receiver's class, constructors, and what a
class's attribute defaults call. Calling a function value (`change(text)`) may do anything, so it undoes every
proof that reads an attribute or a list. Inside a `while`, a call that undoes a proof made before the loop and
read inside it is an error naming the call, the same as an assignment would be.

### A new name for a new type

A `var` may shadow a name in the same scope, and the new binding may hold a different type. That is how a
narrowed `String?` becomes the `String` it computes, without inventing a second name:

```gdscript title=shadowing_doc/shadowing_doc.spite entry
var console = Console()

func ShadowingDoc() {
    var first_line = read_first_line("  spite\nsecond  ")
    crash first_line
    console.print("[{first_line}]")
}

func read_first_line(text: String?): String? {
    assert text
    var text = text.trim()
    var lines = text.lines()
    return lines.first()
}
```
```output
[spite]
```

The new value is computed first, then the old one is released, then the name is bound, so `var text =
text.trim()` reads the binding it is about to replace. Shadowing a name that was never read is still the unused
error.

### Reading with `[]` answers `T?`

An index or a key may not be there, so `names[index]` and `table["key"]` are `T?` and are narrowed like any other
path. Every `[]` answers this way: a `List`, a `Dictionary`, a `Vector`, an `Items`, and any
class of your own, since `a[x]` is only a shortcut for `a.get_at(x)` and every `get_at` answers a `T?`
([functions_and_operators.md](../specs/functions_and_operators.md#operators)). The
compiler also understands the usual proofs for the library's collections, so most reads need nothing extra:

- **A proven count proves the indices below it**: after `crash names.count() == 3`, or `>= 3`, or `> 2`,
  `names[0]` to `names[2]` are plain values, and `crash not names.is_empty()` proves `names[0]`. So are
  `sizes[0]` to `sizes[2]` after `var sizes = [3, 5, 8]`: a list literal proves the indices of its items until
  the list or its name changes. A `Dictionary`'s count proves no key, since its keys need not be `0`, `1`, `2`.
- **A bound proves its index**: `while index < names.count()` proves `names[index]` in the loop body, and
  `while index < lines.count() and lines[index] != "end"` needs nothing more.
- **A bound past the index proves the reads below it**: `while at + 2 < spans.count()` proves `spans[at]`,
  `spans[at + 1]` and `spans[at + 2]`, so a loop over records of three reads them with nothing written. The bound
  is the loop's own condition: `while at < spans.count()` proves only `spans[at]`. Mind what the stronger bound
  changes: a last record that is not whole is now skipped without a word, where `crash spans[at + 2]` would have
  halted on it. When the list must hold whole records, say so first: `crash spans.count() % 3 == 0`.
- **Two lists read by one counter**: `while index < names.count() and index < ages.count()` proves both reads, but
  it stops at the shorter list without a word. When the two must be as long as each other, keep the one bound and
  write `crash ages[index]` in the body, which halts on the first missing age instead.
- **A count kept in a name proves too**: after `var count = values.count()`, `while index < count` proves
  `values[index]`, until `values` shrinks or `count` is assigned (a shrink inside the loop is then an error, as
  for any proof the loop reads). A count worked out some other way (`count() / 8`, a width times a height) proves
  nothing.
- **Inside a loop, a read may be proven again**: `crash names[index - 1]` inside a loop is accepted even when a
  line before the loop already proved that read, since the loop changes what the index means from one pass to the
  next.
- **The index may be any expression without a call**: `crash glyphs[code - 32]` proves `glyphs[code - 32]`
  on the next line. An index with a call in it, `glyphs[offset()]`, is not a path: name it first.
- **Changing the list or the index undoes it**: after `index = index + 1` or `names.clear()`, or a call that
  may do either ([above](#a-call-may-undo-a-proof)), the read must be proven again.
- **Checking a proven read again is an error** whose message names what already proved it, so the line can
  simply go.
- A `Boolean?` cannot be a condition: `if flags[index]` would test that the element is there, not that it is true.
  Prove it is there first, or compare it: `flags[index] == true`. Only `assert flags[index]` and `crash flags[index]`
  take one, and they pass when the element is there and `true`.
- **An unproven read names how to prove it.** `names[0].upper_case()` with nothing proving `names[0]` is
  `'names[0]' may be missing, since every '[ ]' answers a 'T?' (this one is a String?), so 'upper_case' cannot be
  called on it yet: narrow it first with 'crash names[0]', 'assert names[0]' or 'if names[0] { }', or prove the
  count first, 'crash names.count() > 0'`; a read by a counter names the loop bound instead (`... or read it where a
  bound proves it, inside 'while index < names.count()'`), and a `Dictionary` or a class of your own names the
  three narrowing lines only (`diagnostics/index_reads`).
- **A class of your own is proven by what you write.** The compiler knows what `count()` and `get_at` mean for the
  library's collections only, so `while index < shelf.count()` proves nothing about `shelf[index]` when `Shelf`
  is yours: narrow the read with `crash`, `assert` or `if` (`conformance/stage6/indexable_class`).

Every case, with its exact messages, is in [the rules](../specs/failure.md#nothing-fails-silently-the-rule). A proven
read still checks its bounds at run time and halts naming the read when its index was outside the list, so a
proof removes a line of source, not a safety check.

```gdscript title=index_reads_doc/index_reads_doc.spite entry
var console = Console()

func IndexReadsDoc() {
    var names = ["ada", "bo", "cy"]
    crash names.count() == 3
    var first = names[0].upper_case()
    console.print(first, names[2])
    var index = 0
    while index < names.count() {
        var name_length = names[index].length()
        console.print(names[index], name_length)
        index = index + 1
    }
    var ages = Dictionary<Integer>()
    ages["ada"] = 36
    crash ages["ada"]
    console.print("ada is", ages["ada"])
}
```
```output
ADA cy
ada 3
bo 2
cy 2
ada is 36
```

A read kept in a local is narrowed like any other value, so a `Dictionary` is looked up once: `var age =
ages[name]` reads the key, and `if age` or `assert age` narrows the local. This is not a copy made only to narrow
it, since the local holds what the read answered, not a second name for the path:

```gdscript title=kept_read/kept_read.spite entry
var console = Console()

func KeptRead() {
    var ages = Dictionary<Integer>()
    ages["ada"] = 36
    show_next_year(ages, "ada")
    show_next_year(ages, "bo")
}

func show_next_year(ages: Dictionary<Integer>, name: String) {
    var age = ages[name]
    if age {
        console.print(name, "turns", age + 1)
    } else {
        console.print(name, "is not listed")
    }
}
```
```output
ada turns 37
bo is not listed
```

### An index reads nothing

An index is a name, a path, a number or arithmetic on those, never another `[]` read. `draws[run_draws[run]]` is
hard to read, and under the rule above it would need two `crash` lines for one value, so it is an error:
read the inner value into a named `var` first, narrow it, and index with the name.

```gdscript title=read_in_index_error/read_in_index_error.spite entry error
var console = Console()

func ReadInIndexError() {
    var draws = ["sky", "ground"]
    var run_draws = [1, 0]
    console.print(draws[run_draws[0]])
}
```
```diagnostic
is read inside the index of 'draws': compute it first into a named 'var' and pass the name
```

### Comparing needs no narrowing

`==` and `!=` accept a `T?` on either side: `null` is equal only to `null`. So `maybe_name == "ada"` is a
whole test, and is `false` when there is no name, while two values that are both missing are equal
(`texts["a"] == texts["b"]` on an empty dictionary is `true`), so comparing two `T?` values to see whether
something changed never answers "changed" when neither is there. Only the comparison is exempt: reading a member
through a `T?` still needs it narrowed, and so does assigning through one.

```gdscript title=nullable_comparison/nullable_comparison.spite entry
var console = Console()

func NullableComparison() {
    var nobody: String? = null
    var someone: String? = "ada"
    var nobody_either: String? = null
    console.print(nobody == "ada", someone == "ada", nobody != "ada", nobody == nobody_either)
}
```
```output
false true true true
```

## Three outcomes, and no others

1. **A compile error**, for anything the compiler can know: a type that does not fit, a member a template
   cannot use, a symbol outside its enum, an unfilled codegen value, a missing foreign symbol.
2. **`assert`**, when something is absent and the program should keep running: there is nothing more to do
   here, and everything else carries on.
3. **`crash`**, when something is absent and that is a bug: everything stops so the code gets rewritten.

`T?` is the only run-time failure value, and it carries no reason. When a caller must tell cases apart (a
timeout from a refusal), that is data, not an error: return an enum or a union that models exactly those cases,
and handle it like any other value.

Absence is fine: `assert`. Absence is a bug: `crash`. Absence is a case: `if value { } else { }`.

## `assert` is a guard

`assert condition` is a production feature, not a debug one: when the condition is false, the function stops
there and answers "nothing", and no code after it runs. A failed one is remembered for the crash report
([below](#what-a-crash-reports)). "Nothing" has to be something the caller can see, so a guard `assert` is
allowed only where the function's result can say it:

- a function that returns nothing just returns;
- a function returning a `T?` answers `null`;
- a function returning a `List`, `Dictionary`, `Vector` or `Items` answers an empty one.

```gdscript title=assert_guard/assert_guard.spite entry
var console = Console()
var names = ["ada", "bo"]

func AssertGuard() {
    report(1)
    report(7)
    var short = short_names(3)
    var nothing_short = short_names(0)
    var short_count = short.count()
    var nothing_count = nothing_short.count()
    console.print(short_count, nothing_count)
}

func report(index: Integer) {
    var name = name_at(index)
    assert name
    console.print(index, name)
}

func name_at(index: Integer): String? {
    assert index >= 0 and index < names.count()
    return names[index]
}

func short_names(longest: Integer): List<String> {
    assert longest > 0
    return names.filter(is_short)
}

func is_short(name: String): Boolean {
    return name.length() < 3
}
```
```output
1 bo
1 0
```

### A default that looks like an answer is an error

A number, a `Boolean`, a `String`, an enum value or an object cannot say "nothing": its default (`0`, `false`,
`""`, the first value, an object with every attribute at its default) looks exactly like a real answer. A guard
`assert` in a function returning one is a compile error, since the caller would carry on with a value nobody
meant (a `row_of(entity)` answering row 0, a real row, would make the caller remove the wrong one):

```gdscript title=default_answer_error/default_answer_error.spite entry error
var console = Console()
var rows = [10, 20, 30]

func DefaultAnswerError() {
    var row = row_of(7)
    console.print(row)
}

func row_of(entity: Integer): Integer {
    assert entity < rows.count()
    return rows[entity]
}
```
```diagnostic
this 'assert' would answer a default Integer (0) that a caller cannot tell from a real one: return a value ('if entity >= rows.count() { return -1 }'), make the result 'Integer?', or 'crash entity < rows.count()' if this is a developer mistake
```

The message names the three ways out, and which one is right is a decision about the caller:

- **the result says "nothing"**: `Integer?`, and every caller narrows it, when a missing row is a case the
  program meets in normal use;
- **an answer you chose**: an `if` that returns it, when a value outside the real ones already means "none",
  as `-1` does for an index;
- **`crash`**: when asking for a row that is not there is the caller's bug.

```gdscript title=default_answer_fixed/default_answer_fixed.spite entry
var console = Console()
var rows = [10, 20, 30]

func DefaultAnswerFixed() {
    var found = row_of(2)
    crash found
    var missing = row_or_minus_one(7)
    var second = row_or_halt(1)
    console.print(found, missing, second)
}

func row_of(entity: Integer): Integer? {
    assert entity < rows.count()
    return rows[entity]
}

func row_or_minus_one(entity: Integer): Integer {
    if entity >= rows.count() {
        return -1
    }
    return rows[entity]
}

func row_or_halt(entity: Integer): Integer {
    crash entity < rows.count()
    return rows[entity]
}
```
```output
30 -1 20
```

An `if` that returns a value in such a function is an answer written down, whatever the value, so
`if text.is_empty() { return 0 }` is how an empty text is zero bytes long, and `if handle == -1 { return false }`
is how a closed file says it is not open.

### An `if` that leaves proves the rest

An `if` with no `else` whose block ends by leaving the function (a `return`, a bare `crash`, or a `switch` whose
every case ends so) proves the opposite of its condition for the rest of the block, the way an `assert` does. So `if not found { return -1 }`
narrows `found`, `if not left or not right { return 0 }` narrows both, and `if index >= names.count() { return
"" }` proves `names[index]`:

```gdscript title=leaving_if/leaving_if.spite entry
var console = Console()
var prices = Dictionary<Integer>()

func LeavingIf() {
    prices["apple"] = 3
    var apple = price_or_zero("apple")
    var pear = price_or_zero("pear")
    console.print(apple, pear)
}

func price_or_zero(name: String): Integer {
    if not prices[name] {
        return 0
    }
    return prices[name] * 100
}
```
```output
300 0
```

### Every path ends in a `return`

A function that declares a result answers it on every path. A path that reaches the end of the function without
a `return` is a compile error at the function's last line, naming the path, since the caller would otherwise get
a `0` or a `""` that nobody wrote. A bare `crash` ends a path as a `return` does, and so
does a `while true`, which Spite leaves only by returning:

```gdscript title=falling_end_error/falling_end_error.spite entry error
var console = Console()

func FallingEndError() {
    var sign = sign_of(3)
    console.print(sign)
}

func sign_of(value: Integer): String {
    if value > 0 {
        return "positive"
    }
    if value < 0 {
        return "negative"
    }
}
```
```diagnostic
'sign_of' answers a String, but when 'value < 0' is false (line 12, an 'if' with no 'else') it reaches its end without a 'return', so a caller would get a String nobody wrote: end that path with 'return <value>', or with 'crash' if it cannot happen
```

A `switch` covers every case, so one whose cases all return needs nothing after it:

```gdscript title=falling_end_fixed/falling_end_fixed.spite entry
enum Light {
    'red'
    'green'
}

var console = Console()

func FallingEndFixed() {
    var sign = sign_of(0)
    var wait = wait_for('green')
    console.print(sign, wait)
}

func sign_of(value: Integer): String {
    if value > 0 {
        return "positive"
    }
    if value < 0 {
        return "negative"
    }
    return "zero"
}

func wait_for(light: Light): Integer {
    switch light {
        'red': return 30
        'green': return 20
    }
}
```
```output
zero 20
```

### An `if` that only returns the default is an `assert`

Where a guard `assert` is allowed, it is the only way to write a guard. An `if` with no `else` whose whole body
returns the function's default (`null` from a function returning a `T?`, an empty collection made on the spot
from one returning a collection, or a bare `return` from one returning nothing) is a guard spelled the long way. It is a compile error wherever it stands, inside a loop or a nested
`if` included, and the message names the `assert` of the opposite condition:

```gdscript title=default_guard_error/default_guard_error.spite entry error
var console = Console()
var names = List<String>()

func DefaultGuardError() {
    var first = first_name()
    console.print(first == "ada")
}

func first_name(): String? {
    if names.is_empty() {
        return null
    }
    return names.first()
}
```
```diagnostic
this 'if' only returns the default 'null', which is how a guard is written: write 'assert not names.is_empty()' and let the rest run unindented
```

The fix reads top to bottom:

```gdscript title=default_guard_fixed/default_guard_fixed.spite entry
var console = Console()
var names = List<String>()

func DefaultGuardFixed() {
    var first = first_name()
    console.print(first == "ada")
}

func first_name(): String? {
    assert not names.is_empty()
    return names.first()
}
```
```output
false
```

The message turns the condition around for you: `if count < 0 or count > limit` becomes
`assert count >= 0 and count <= limit`.

In a function returning a number, a `Boolean`, a `String`, an enum or an object, nothing of this applies: `assert`
is not allowed there ([above](#a-default-that-looks-like-an-answer-is-an-error)), and an `if` returning `0` or
`false` is the answer written down:

```gdscript title=default_answer/default_answer.spite entry
var console = Console()
var text = ""

func DefaultAnswer() {
    var bytes = size()
    console.print(bytes)
}

func size(): Integer {
    if text.is_empty() {
        return 0
    }
    return text.length() + 1
}
```
```output
0
```

### The last `if` of a function

A function whose *last* statement is `if value { ... }` with no `else` (wrapping the rest of the function only
to check that something exists) is a compile error too, naming the `assert` that lets the rest run unindented:

```gdscript title=terminal_if_error/terminal_if_error.spite entry error
var console = Console()

func TerminalIfError() {
    announce()
}

func try_get_name(): String? {
    return null
}

func announce() {
    var maybe_name = try_get_name()
    if maybe_name {
        console.print(maybe_name)
    }
}
```
```diagnostic
write 'assert maybe_name' and let the rest of the function run unindented
```

```gdscript title=terminal_if_fixed/terminal_if_fixed.spite entry
var console = Console()

func TerminalIfFixed() {
    announce()
}

func try_get_name(): String? {
    return "Aria"
}

func announce() {
    var name = try_get_name()
    assert name
    console.print(name)
}
```
```output
Aria
```

A narrowing `if` with an `else`, or one that is not the function's last statement, is ordinary code.

### `assert` is not allowed in a constructor

A constructor is setup, not logic. If something can be invalid inside one, logic has been put where only setup
belongs, and the caller would receive an object with every field at its default that it cannot tell from a
properly built one. So `assert` in a constructor is an error, and so is an `if` that only leaves it; the message
names `crash` for when absence there is a bug:

```gdscript title=constructor_assert_error/constructor_assert_error.spite entry error
var console = Console()
var ready = false

func ConstructorAssertError() {
    assert ready
    console.print("ready")
}
```
```diagnostic
'assert' is not allowed in a constructor: a constructor is setup, not logic
```

## `crash`

`crash condition` mirrors `assert` exactly (same polarity, same narrowing) with the other severity: when the
condition is false, the program halts. The report names the line and shows the values of the condition's
operands and of the other names in scope, so nothing has to be written and nothing can drift out of date. A bare
`crash` marks a branch that cannot happen (`crash false` is formatted to it).

```gdscript title=crash_guard/crash_guard.spite entry
var console = Console()
var names = Dictionary<String>()

func CrashGuard() {
    names["ada"] = "Ada Lovelace"
    var name_length = full_name_length("ada")
    console.print(name_length)
    var description = describe(3)
    console.print(description)
}

func full_name_length(key: String): Integer {
    crash names[key]
    return names[key].length()
}

func describe(value: Integer): String {
    crash value > -5
    if value > 0 {
        return "positive"
    }
    if value < 1 {
        return "not positive"
    }
    crash
}
```
```output
12
positive
```

`crash` is allowed in a constructor, which is where a program that cannot start says so.

**`crash` never takes a message, and neither does `assert`.** There is no text to write
after the condition: the report points at the line of code and shows the values there, and the reader opens
that line and reads the data, which says more than a sentence about them would and cannot drift from the code. Do
not smuggle text into a condition to have it printed (`crash found or bone_name == ""`): the report shows the
values in scope without it.

A `crash` the compiler can decide is not left for the run: one whose condition asks only what is known while
compiling (a codegen value, `functions['run_each']`, `is_fixed_size`, ...) and is false, in a function the program reaches,
is a compile error at the `crash`, since the program would halt there every time
([metaprogramming.md](../specs/metaprogramming.md#codegen-values-)).

### What a crash reports

A report is the place of the crash and the memory that matters there: its file and line,
and the values there. It does not repeat the condition, which is on the line it names.

A crash flushes what the program printed, writes one tab-separated line to the error stream and exits with
status 1. The line starts `spite.crash` and carries the site's id, its place, its class and function, every name
and call in the condition with its value, and then the other values in scope, then a `spite.assert` line for
each `assert` that failed before it, because those are usually the trail that explains how the program got there.
Each names what its function answered instead (`answered=nothing` for a function that returns nothing, `null` for
a `T?`, `empty` for a collection). Last comes the call chain: a `spite.frame` line for each Spite function on the
stack, innermost first, with the file and line it starts on, as a [native fault](#what-a-native-fault-reports)
writes them:

```
spite.crash	64b935f1	crash_report/crash_report.spite:14	CrashReport	check	value=-9	limit=0
spite.assert	0aae5005	crash_report/crash_report.spite:18	CrashReport	announce	answered=nothing
spite.frame	crash_report/crash_report.spite:12	CrashReport	check
spite.frame	crash_report/crash_report.spite:3	CrashReport	CrashReport
spite.frame	launcher/launcher.spite:3	Launcher	Launcher
spite.frame	-	-	main
```

After the condition's parts come the parameters and locals in scope, then the attributes of the object the
function runs on, each once as `name=value`: text, numbers and enums, and a `T?` of one of them (`name is null`
when it is empty). An object, a list or a function is left out, and so is a name the condition already reported.
A report shows at most 12 of these values, and at most the first 80 bytes of a text, followed by its full length:

```
spite.crash	262f0d5a	crash_scope_values/crash_scope_values.spite:22	CrashScopeValues	load_level	inner=20	level=forest	depth=2	nickname is null	scale=1.5	title=a report longer than the eighty bytes a crash shows of one text, so the rest is ...(92 bytes)	mood=calm	retries=3
```

When the condition is a call on a value, `crash settings.exists()` (or `crash not names.contains(name)`), the
value the call was made on is the operand: text, a number or an enum is written as it is, and an object whose class
has a `to_string()` as that text, so a missing `File` names its path:

```
spite.crash	00b7d810	crash_receiver/crash_receiver.spite:6	CrashReceiver	CrashReceiver	settings=.spite/crash_receiver/missing_settings.txt
```

A condition of any shape (`or`, `not`, comparisons, arithmetic, calls inside it) reports every part it read,
left to right and each once: a name or a path with its value, a call with the answer it gave while the condition
ran (it is never called a second time), and a value that is not there in the words of a failed narrowing
(`is null`, `is missing: ...`). A part that an `and` or an `or` skipped is left out, since it never ran:

```
spite.crash	01478395	crash_compound/crash_compound.spite:21	CrashCompound	finish	record is null	cooked.count()=1	never_cooked_id=cake	name=cake
```

A crash that narrows a `T?` fails because something is not there, so it never prints a value for it: a default
would read like a real `0`. It names the first link of the path that is missing, and for a `[]` read what was
asked for: the index and the count of a list, the key of a dictionary.

```
spite.crash	26b57b59	crash_missing_item/crash_missing_item.spite:13	CrashMissingItem	read	clip.keys[start + 1] is missing: index 3, count 2	frame=1
spite.crash	474c3f17	crash_missing_key/crash_missing_key.spite:11	CrashMissingKey	show	scores[player] is missing: key "bea"
spite.crash	0f912d8f	crash_missing_link/crash_missing_link.spite:14	CrashMissingLink	show	rig.skeleton is null
spite.crash	75b30821	crash_missing_value/crash_missing_value.spite:10	CrashMissingValue	show	width is null
```

The id is derived from the site's content, so it stays the same when unrelated lines move. Each build also writes
a `.crashes` file beside the executable it builds: one line per `assert` and `crash` site, sorted by id, with its file,
line, column, class, function, kind and condition, so `grep 64b935f1 program.crashes` finds a site from a report and the
condition it checked. An `--optimized` build writes only the id where the place would be, and the map is how
the id is read back:

```
spite.crash	64b935f1	value=-9	limit=0
spite.assert	0aae5005	answered=nothing
```

On Linux and macOS an `--optimized` build keeps no frame pointers, so its crash has no `spite.frame` lines; on
Windows the chain is read from the unwind tables every build has.
A failed `assert` prints nothing when it fails, in every build: it is the program answering "nothing" and
carrying on, and a long run fails thousands of them, which would bury the one line that matters. It is kept for the
report instead. To watch them as they fail, build with `--trace-asserts` (a `Build` field, `trace_asserts`): each
failed `assert` of the program then also writes its `spite.assert` line to the error stream at once, after flushing
what the program printed, and still enters the ring.
The trace keeps only the latest failed asserts, in a fixed-size ring, so it never allocates and never grows. An
`assert` that fails again right after itself adds to its last line instead of taking a new one, which then ends in
`repeated=<count>`: a guard that fails every frame is one line, and the 32 lines are 32 steps of the trail, not one
step 32 times.

```
spite.assert	4b7a8666	conformance/stage6/assert_runs/assert_runs.spite:17	AssertRuns	name_at	answered=null	repeated=1000
spite.assert	3a356b09	conformance/stage6/assert_runs/assert_runs.spite:22	AssertRuns	title_of	answered=null
```

A line carries no values. The assert's place, the `.crashes` map and its condition are what it says: a guard that
narrows failed because something was not there, and one that tests a condition names its operands on that line,
while the crash that ends the run prints every value at its own site.
It holds the asserts of the program and of every package it `load`s, never those of the standard library: an
`assert` in `library/` is how the library answers routine questions (a key that is not there, text that does not
match, a read past the end), and those would push the program's own entries out of the ring. The compiler leaves
the record out of a library `assert` altogether, so it costs what an `if` costs. A crash inside the library still
reports its own site.
A crash reports the ring as it stood when the crash began. Other threads may still be failing asserts while it
prints, such as a game's pool threads running their guards, and those never lengthen the report: it is the crash line,
at most 32 `spite.assert` lines and the `earlier=` count of the lines before them, then the call chain of the thread that crashed, and then
the program exits. In a program that starts
threads, each failed `assert` adds to the newest line or takes a new one with a compare-and-swap, so asserts
failing on several threads at once are all counted, and one site failing on every thread is still one line (`conformance/stage6/assert_ring_threads`); a program without threads pays a
plain add. Only the first thread to crash
reports; one that crashes while that report is being written waits for the program to end, so two reports never
interleave.

### What a native fault reports

Some failures happen below Spite: a foreign library reads through a null pointer, a recursion runs out of stack, a
driver runs an instruction the processor refuses. The program cannot survive them, but it never ends silently:
every program installs a fault handler before its first line runs, and a fault writes a
`spite.fault` line to the error stream, then the failed asserts as a crash does, then the Spite functions on the
stack, and ends the program:

```
spite.fault	read-violation	-	-	-	address=0x0	code=0xc0000005	at=fixture.dll+0x1029	foreign=read_integer_at	library=conformance/stage6/native_fault_foreign/fixture.dll	from=conformance/stage6/native_fault_foreign/native_fault_foreign.spite:13
spite.frame	conformance/stage6/native_fault_foreign/native_fault_foreign.spite:11	NativeFaultForeign	read_nothing
spite.frame	conformance/stage6/native_fault_foreign/native_fault_foreign.spite:4	NativeFaultForeign	NativeFaultForeign
spite.frame	launcher/launcher.spite:3	Launcher	Launcher
spite.frame	-	-	main
```

Read it left to right:

- **What happened**: `read-violation`, `write-violation` or `execute-violation` (on Linux and macOS,
  `access-violation`), `stack-overflow`, `illegal-instruction`, `integer-division-by-zero`, `misaligned-access`,
  `bus-error`, `breakpoint`, `heap-corruption` (the C library's heap found its own bookkeeping overwritten, usually
  by a double free or a write past a block; the `spite.frame` lines name the Spite function that freed or
  allocated), `abort` (on Linux and macOS, any other `abort()`), `floating-point-exception`, or `exception` for any
  other code the system raises.
- **Where**, as `path:line`, class and function: the Spite function the faulting instruction is in, with the line
  that function starts on: a fault has no Spite line of its own. `-	-	-` means it is in no Spite function:
  inside a foreign library or the C library, and the first `spite.frame` is the Spite function that called in.
- `address=`: the address that could not be read or written. `code=` (Windows) is the exception code, `signal=`
  (Linux, macOS) the signal number.
- `at=`: the file the faulting instruction is in (the program itself or a foreign library) and its offset from
  the start of that file, which `llvm-objdump -d` or a debugger turns into the instruction.
- `foreign=`: the last foreign function this thread called, the `library=` it is in, and `from=` the line of Spite
  that called it. It stays set after the call returns, so when `at=` names a library it is the call that went in,
  and when it does not it is the last C that touched the program's data, the fact most worth having when a Vulkan
  or Win32 call goes wrong.
- Each `spite.frame` is a Spite function on the stack, innermost first; `repeated=` counts the same function called
  from itself, and `spite.frame	more` says the stack goes deeper than the 256 frames walked or the 16 lines written.

The program then ends with the status the system gives that fault, exactly as it would have without the report:
on Windows the exception code (`0xC0000005` for an access violation), on Linux and macOS death by the signal, so
a core dump is still written where the system keeps them. What the program printed is flushed after the report.

## What it costs at run time

- **Narrowing costs nothing.** Every proof (`assert`, `crash`, `if`, `while`, a proven `[]` read, following a
  call to see what it may change) is worked out while compiling, and nothing of it is emitted. A proven `[]`
  read still checks its bounds, as every `[]` read does.
- **A `T?` of an object is just the pointer**; a `T?` of a number or a `Boolean` is the value with a flag beside
  it, passed by value, never allocated.
- **A passing `assert` or `crash` is one branch.** A failed `assert` in the program or a loaded package also
  stores one pointer into the 32-entry ring and counts it; one in `library/` does not.
- **A crash's report is written only when it fires**: each site's place and condition are fixed text in the
  program, and the operand values are turned into text on the way out. A `crash` whose condition has a call inside
  a comparison, an `and` or an `or` keeps that call's answer and a flag in locals as it runs, so the report can name
  the answer without calling again: two stores only the failure branch reads. The `.crashes` map is a file beside the
  executable, which nothing reads at run time.
- The ring and the function that prints it are in every program, since the standard library has
  `crash` sites of its own; they are 32 pointers, a counter and one function. A crash's call chain is walked by
  the fault handler's own walker, which adds one small function and one pointer the handler sets when it is
  installed.
- **The fault handler is in every program and costs nothing until a fault.** It is about 3.7 KB of machine code, and
  a table of every function the program kept (its start address and name, 32 bytes and the name each; 73
  functions, about 4 KB, in `examples/hello`). Installing it is two system calls when the program starts, and one
  more when a thread starts. Each foreign call stores one pointer before it goes in, which measured as nothing
  ([optimizations.md](optimizations.md#the-fault-handler-is-in-every-program)).

---

Next: [Functions and operators](functions_and_operators.md), functions, calls, operators and their rules.
