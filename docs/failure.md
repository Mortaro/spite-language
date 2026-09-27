# Nullable values and failure

Spite has no exceptions, no error values and no `Result` types. Something that may not be there is a `T?`, and
something that went wrong has exactly three outcomes: a compile error, an `assert`, or a `crash`.

## Nothing fails silently

**Anything that can go wrong silently is a bug** ([D244](decisions.md)). Every failure is loud: a compile error,
or a crash that names its cause. It is never a wrong value that looks like a right one, a write that is lost, a
step that is skipped, memory that leaks or a program that hangs. When the compiler can know about a mistake it
refuses the program; when only the run can find it, the run stops and says where. Every rule on this page, and
every open question about the language, is judged against this one.

What that already means in practice:

- A value that may be missing is a `T?`, and nothing reads it until it is [narrowed](#narrowing); a `[]` read
  and `first()` answer a `T?` instead of a made-up element ([below](#reading-with--answers-t)).
- A check that stops proving something -- after an assignment, or a call that may change what it read -- has to
  be made again ([a call may undo a proof](#a-call-may-undo-a-proof)).
- A guard `assert` stops a function only where its result can say "nothing"; a function answering a number, a
  `Boolean`, a text or an object writes its answer down ([below](#a-default-that-looks-like-an-answer-is-an-error)).
- A developer's mistake halts naming the line: a whole number divided by zero, signed arithmetic that does not
  fit while you develop ([values_and_types.md](values_and_types.md#signed-arithmetic-that-does-not-fit-halts-while-you-develop)),
  a `crash` that fails, a native fault ([below](#what-a-native-fault-reports)).
- A failed `crash` names what is missing instead of printing a default that reads like a real zero
  ([what a crash reports](#what-a-crash-reports)).
- An operand that would be cut to fit is a compile error, in arithmetic and in comparisons
  ([values_and_types.md](values_and_types.md#wider-arithmetic-goes-wider-operand-first)).

The whole list, with what is still open, is in [the rules](#nothing-fails-silently--the-rule).

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
`value == null` -- comparing against `null` is an error naming the forms below:

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

Narrowing proves a `T?` holds a value and turns it into a plain `T` in place -- the same name, no new variable.
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
([below](#reading-with--answers-t)).

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
prefixes, nothing else -- a sibling path stays a `T?`. `while current` narrows its body the same way, and is how a
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
error, because the next pass would read it unproven: narrow it inside the loop instead.

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
or an `assert` on a plain `Tracker`.

### A call may undo a proof

A call between a proof and a read undoes the proof only when the call may change what the proof depends on
([D169](decisions.md)). The compiler follows the called function, and everything it calls, to see
which attributes it can assign and which lists it can shrink (`clear`, `remove_at`, `remove_first`,
`remove_last`, a dictionary's `remove`). If it may change an attribute the proven path reads through -- or the
list a proven `[]` reads, or anything its index reads -- the proof is gone and the read must be proven again; if
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

The new value is computed first, then the old one is released, then the name is bound -- so `var text =
text.trim()` reads the binding it is about to replace. Shadowing a name that was never read is still the unused
error.

### Reading with `[]` answers `T?`

An index or a key may not be there, so `names[index]` and `table["key"]` are `T?` and are narrowed like any other
path. The compiler also understands the usual proofs, so most reads need nothing extra:

- **A proven count proves the indices below it**: after `crash names.count() == 3`, or `>= 3`, or `> 2`,
  `names[0]` to `names[2]` are plain values, and `crash not names.is_empty()` proves `names[0]`. A
  `Dictionary`'s count proves no key, since its keys need not be `0`, `1`, `2`.
- **A bound proves its index**: `while index < names.count()` proves `names[index]` in the loop body, and
  `while index < lines.count() and lines[index] != "end"` needs nothing more.
- **The index may be any expression without a call**: `crash glyphs[code - 32]` proves `glyphs[code - 32]`
  on the next line. An index with a call in it, `glyphs[offset()]`, is not a path: name it first.
- **Changing the list or the index undoes it**: after `index = index + 1` or `names.clear()` -- or a call that
  may do either ([above](#a-call-may-undo-a-proof)) -- the read must be proven again.
- **Checking a proven read again is an error** whose message names what already proved it, so the line can
  simply go.
- A `Boolean?` cannot be a condition: `if flags[index]` would test that the element is there, not that it is true.
  Prove it is there first, or compare it: `flags[index] == true`.

Every case, with its exact messages, is in [the rules](#null-safety-and-assert-narrowing--implemented). A proven
read still checks its bounds at run time, so a proof removes a line of source, not a safety check.

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
    ages.set("ada", 36)
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

### Comparing needs no narrowing

`==` and `!=` accept a `T?` on either side: `null` is simply not equal to anything. So `maybe_name == "ada"` is a
whole test, and is `false` when there is no name. Only the comparison is exempt -- reading a member through a `T?`
still needs it narrowed, and so does assigning through one.

```gdscript title=nullable_comparison/nullable_comparison.spite entry
var console = Console()

func NullableComparison() {
    var nobody: String? = null
    var someone: String? = "ada"
    console.print(nobody == "ada", someone == "ada", nobody != "ada")
}
```
```output
false true true
```

## Three outcomes, and no others

1. **A compile error**, for anything the compiler can know: a type that does not fit, a member a template
   cannot use, a symbol outside its enum, an unfilled codegen value, a missing foreign symbol.
2. **`assert`**, when something is absent and the program should keep running: there is nothing more to do
   here, and everything else carries on.
3. **`crash`**, when something is absent and that is a bug: everything stops so the code gets rewritten.

`T?` is the only run-time failure value, and it carries no reason. When a caller must tell cases apart -- a
timeout from a refusal -- that is data, not an error: return an enum or a union that models exactly those cases,
and handle it like any other value.

Absence is fine: `assert`. Absence is a bug: `crash`. Absence is a case: `if value { } else { }`.

## `assert` is a guard

`assert condition` is a production feature, not a debug one: when the condition is false, the function stops
there and answers "nothing", and no code after it runs. A failed one is remembered for the crash report
([below](#what-a-crash-reports)). "Nothing" has to be something the caller can see, so a guard `assert` is
allowed only where the function's result can say it ([D244](decisions.md), [D245](decisions.md)):

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

A number, a `Boolean`, a `String`, an enum value or an object cannot say "nothing": its default -- `0`, `false`,
`""`, the first value, an object with every attribute at its default -- looks exactly like a real answer. A guard
`assert` in a function returning one is a compile error, since the caller would carry on with a value nobody
meant (SlopEngine's `row_of(entity)` answered row 0, a real row, and removed the wrong one):

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

- **the result says "nothing"**: `Integer?`, and every caller narrows it -- when a missing row is a case the
  program meets in normal use;
- **an answer you chose**: an `if` that returns it -- when a value outside the real ones already means "none",
  as `-1` does for an index;
- **`crash`**: when asking for a row that is not there is the caller's bug ([D199](decisions.md)).

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

An `if` with no `else` whose block ends by leaving the function -- a `return` or a bare `crash` -- proves the
opposite of its condition for the rest of the block, the way an `assert` does. So `if not found { return -1 }`
narrows `found`, `if not left or not right { return 0 }` narrows both, and `if index >= names.count() { return
"" }` proves `names[index]`:

```gdscript title=leaving_if/leaving_if.spite entry
var console = Console()
var prices = Dictionary<Integer>()

func LeavingIf() {
    prices.set("apple", 3)
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

### An `if` that only returns the default is an `assert`

Where a guard `assert` is allowed, it is the only way to write a guard. An `if` with no `else` whose whole body
returns the function's default -- `null` from a function returning a `T?`, or a bare `return` from one returning
nothing -- is a guard spelled the long way. It is a compile error wherever it stands, inside a loop or a nested
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

The message turns the condition around for you -- `if count < 0 or count > limit` becomes
`assert count >= 0 and count <= limit` ([how](#null-safety-and-assert-narrowing--implemented)).

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

A function whose *last* statement is `if value { ... }` with no `else` -- wrapping the rest of the function only
to check that something exists -- is a compile error too, naming the `assert` that lets the rest run unindented:

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

`crash condition` mirrors `assert` exactly -- same polarity, same narrowing -- with the other severity: when the
condition is false, the program halts. The compiler captures the condition's source text and the values of its
operands, so nothing has to be written and nothing can drift out of date. A bare `crash` marks a branch that
cannot happen (`crash false` is formatted to it).

```gdscript title=crash_guard/crash_guard.spite entry
var console = Console()
var names = Dictionary<String>()

func CrashGuard() {
    names.set("ada", "Ada Lovelace")
    var name_length = full_name_length("ada")
    console.print(name_length)
    var description = describe(3)
    console.print(description)
}

func full_name_length(key: String): Integer {
    var name = names.get(key)
    crash name
    return name.length()
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

A `crash` the compiler can decide is not left for the run: one whose condition asks only what is known while
compiling (a codegen value, `has_function`, `fits_vector`, ...) and is false, in a function the program reaches,
is a compile error at the `crash`, since the program would halt there every time
([metaprogramming.md](metaprogramming.md#codegen-values---implemented)).

### What a crash reports

A crash flushes what the program printed, writes one tab-separated line to the error stream and exits with
status 1. The line starts `spite.crash` and carries the site's id, its place, its class and function, the
condition, and the named operands with their values -- then a `spite.assert` line for each `assert` that failed
before it, because those are usually the trail that explains how the program got there:

```
spite.crash	64b935f1	crash_report/crash_report.spite:14	CrashReport	check	value > limit	value=-9	limit=0
spite.assert	0aae5005	crash_report/crash_report.spite:18	CrashReport	announce	amount > 0
```

When the condition is a call on a value, `crash settings.exists()` (or `crash not names.contains(name)`), the
value the call was made on is the operand: text, a number or an enum is written as it is, and an object whose class
has a `to_string()` as that text, so a missing `File` names its path:

```
spite.crash	00b7d810	crash_receiver/crash_receiver.spite:6	CrashReceiver	CrashReceiver	settings.exists()	settings=.spite-cache/crash_receiver/missing_settings.txt
```

A crash that narrows a `T?` fails because something is not there, so it never prints a value for it -- a default
would read like a real `0`. It names the first link of the path that is missing, and for a `[]` read what was
asked for: the index and the count of a list, the key of a dictionary.

```
spite.crash	26b57b59	crash_missing_item/crash_missing_item.spite:13	CrashMissingItem	read	clip.keys [start + 1]	clip.keys[start + 1] is missing: index 3, count 2
spite.crash	474c3f17	crash_missing_key/crash_missing_key.spite:11	CrashMissingKey	show	scores [player]	scores[player] is missing: key "bea"
spite.crash	0f912d8f	crash_missing_link/crash_missing_link.spite:14	CrashMissingLink	show	rig.skeleton.heights [0]	rig.skeleton is null
spite.crash	75b30821	crash_missing_value/crash_missing_value.spite:10	CrashMissingValue	show	width	width is null
```

The id is derived from the site's content, so it stays the same when unrelated lines move. Each build also writes
a `.crashes` file beside the executable it builds: one line per `assert` and `crash` site, sorted by id, with its file,
line, class, function, kind and condition, so `grep 64b935f1 program.crashes` finds a site from a report.
The trace keeps only the latest failed asserts, in a fixed-size ring, so it never allocates and never grows.
It holds the asserts of the program and of every package it `load`s, never those of the standard library: an
`assert` in `library/` is how the library answers routine questions (a key that is not there, text that does not
match, a read past the end), and those would push the program's own entries out of the ring. The compiler leaves
the record out of a library `assert` altogether, so it costs what an `if` costs. A crash inside the library still
reports its own site ([D189](decisions.md)).

### What a native fault reports

Some failures happen below Spite: a foreign library reads through a null pointer, a recursion runs out of stack, a
driver runs an instruction the processor refuses. The program cannot survive them, but it never ends silently
([D244](decisions.md)): every program installs a fault handler before its first line runs, and a fault writes a
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
  `bus-error`, `breakpoint`, `floating-point-exception`, or `exception` for any other code the system raises.
- **Where**, as `path:line`, class and function: the Spite function the faulting instruction is in, with the line
  that function starts on -- a fault has no Spite line of its own. `-	-	-` means it is in no Spite function:
  inside a foreign library or the C library, and the first `spite.frame` is the Spite function that called in.
- `address=`: the address that could not be read or written. `code=` (Windows) is the exception code, `signal=`
  (Linux, macOS) the signal number.
- `at=`: the file the faulting instruction is in -- the program itself or a foreign library -- and its offset from
  the start of that file, which `llvm-objdump -d` or a debugger turns into the instruction.
- `foreign=`: the last foreign function this thread called, the `library=` it is in, and `from=` the line of Spite
  that called it. It stays set after the call returns, so when `at=` names a library it is the call that went in,
  and when it does not it is the last C that touched the program's data -- the fact most worth having when a Vulkan
  or Win32 call goes wrong.
- Each `spite.frame` is a Spite function on the stack, innermost first; `repeated=` counts the same function called
  from itself, and `spite.frame	more` says the stack goes deeper than the 256 frames walked or the 16 lines written.

The program then ends with the status the system gives that fault, exactly as it would have without the report:
on Windows the exception code (`0xC0000005` for an access violation), on Linux and macOS death by the signal, so
a core dump is still written where the system keeps them. What the program printed is flushed after the report.

## What it costs at run time

- **Narrowing costs nothing.** Every proof -- `assert`, `crash`, `if`, `while`, a proven `[]` read, following a
  call to see what it may change -- is worked out while compiling, and nothing of it is emitted. A proven `[]`
  read still checks its bounds, as every `[]` read does.
- **A `T?` of an object is just the pointer**; a `T?` of a number or a `Boolean` is the value with a flag beside
  it, passed by value, never allocated.
- **A passing `assert` or `crash` is one branch.** A failed `assert` in the program or a loaded package also
  stores one pointer into the 32-entry ring and counts it; one in `library/` does not.
- **A crash's report is written only when it fires**: each site's place and condition are fixed text in the
  program, and the operand values are turned into text on the way out. The `.crashes` map is a file beside the
  executable, which nothing reads at run time.
- As built, the ring and the function that prints it are in every program, since the standard library has
  `crash` sites of its own; they are 32 pointers, a counter and one function.
- **The fault handler is in every program and costs nothing until a fault.** It is about 3.7 KB of machine code, and
  a table of every function the program kept -- its start address and name, 32 bytes and the name each (73
  functions, about 4 KB, in `examples/hello`). Installing it is two system calls when the program starts, and one
  more when a thread starts. Each foreign call stores one pointer before it goes in, which measured as nothing
  ([optimizations.md](optimizations.md#the-fault-handler-is-in-every-program)).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Nothing fails silently  **[the rule]**

D244 (decided by Mortaro, 2026-09-27): **anything that can go wrong silently is a bug.** "anything that can go
wrong silently should be considered a bug. this should be in the spite documentation and guidelines". Every
failure is loud -- a compile error for what the compiler can know, or a crash naming its cause for what only the
run can find (D24, D199) -- and never:

- **a wrong value** that the program cannot tell from a right one (a default, a zero, a truncated number);
- **a lost write** (a value computed or stored that nothing reads);
- **a skipped step** (work the reader expects to happen that does not);
- **a leak** (memory or a resource that is never given back);
- **a hang** (a wait or a loop that can never end).

It is a rule for the language, the compiler, the standard library and the programs written in it, and every open
item -- in [open_questions.md](open_questions.md), [KNOWN_ISSUES.md](KNOWN_ISSUES.md) and
`mortaros_missing_decisions.md` -- is judged against it: a proposal that lets one of the five happen without a
word needs a reason, and a place where the compiler lets one through is a bug to fix, not a style to document.

**What the language already refuses.** Each is a compile error or a crash naming its cause:

| Would go wrong silently | What happens instead | Where |
|---|---|---|
| reading a value that may be absent as if it were there | `T?` must be narrowed first; `value == null` is an error | [narrowing](#null-safety-and-assert-narrowing--implemented) |
| an index past the end, a key never set | `[]`, `first()`, `last()`, `remove_first()` answer a `T?` | [reading with `[]`](#reading-with--answers-t) |
| a guard `assert` answering a `0`, `false`, `""` or default object the caller takes for a real answer | compile error unless the result can say "nothing" (D244, D245) | [a default that looks like an answer](#a-default-that-looks-like-an-answer-is-an-error) |
| a `false`, `0` or `""` taken for "missing" | narrowing tests presence, never the value; a `Boolean?` is never a condition | [null safety](#null-safety-and-assert-narrowing--implemented) |
| a proof that went stale after an assignment or a call | the read must be proven again; in a loop, an error naming the call | [a call may undo a proof](#a-call-may-undo-a-proof) |
| a failed `crash` printing a default that reads as a real zero | the report names the missing link, index and count, or key (D248) | [what a crash reports](#what-a-crash-reports-1) |
| a native fault ending the program with nothing printed | `spite.fault` with the place, the last foreign call and the stack (D255) | [what a native fault reports](#what-a-native-fault-reports-1) |
| a whole number divided by zero | halts naming the line; a zero written as the divisor is a compile error (D201) | [values_and_types.md](values_and_types.md#numeric-types--implemented-provisional) |
| signed arithmetic that does not fit | halts naming the operation in a development build (D249) | [values_and_types.md](values_and_types.md#signed-arithmetic-that-does-not-fit-halts-while-you-develop) |
| a wider operand cut to fit, in arithmetic or a comparison | compile error naming the operation turned around (D162, D251) | [values_and_types.md](values_and_types.md#wider-arithmetic-goes-wider-operand-first) |
| a constant that overflows its type | compile error | [values_and_types.md](values_and_types.md#numeric-types--implemented-provisional) |
| a `Float` gone to infinity written as JSON | crash naming the attribute (D198) | [json.md](json.md) |
| a condition that is always false because it asks the compiler | a folded-false `crash` in reached code is a compile error (D250) | [metaprogramming.md](metaprogramming.md#codegen-values---implemented) |
| a value computed and never read, an attribute nothing reads | compile error | [style.md](style.md#unused-is-an-error--implemented) |
| an object made and dropped on the same line, to "do" something | compile error | [classes_and_files.md](classes_and_files.md#a-constructed-object-must-be-kept-and-used) |
| `for`, `break`, `continue`, `++`, `&&` and other habits that would parse as something else | compile error naming the Spite form | [control_flow.md](control_flow.md#control-flow--implemented), [for_ai_writers.md](for_ai_writers.md#habits-from-other-languages-that-spite-rejects) |
| a borrowed `Vector` item kept, or read after the vector moved | compile error | [memory.md](memory.md#borrowed-items-of-a-vectort--implemented) |
| two threads writing a singleton at once | the compiler makes the singleton safe; anything else a `Parallel` reaches is an error | [concurrency.md](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows) |
| a `while true` loop holding a singleton's lock forever | compile error | [concurrency.md](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows) |
| a peer that hung up read as a count of `-1` | `socket.closed` turns `true`; reads answer `0` or `null` | [standard_library.md](standard_library.md) |
| an impossible date such as `Date(2023, 2, 29)` rolled over | halts; text from outside is read with `TimeText`, which answers `null` | [time.md](time.md) |
| a `crash` or `assert` whose site cannot be found again | every build writes `<program>.crashes`, one line per site | [what a crash reports](#what-a-crash-reports-1) |

**Still open** (each a bug under D244, recorded so it is not mistaken for a design): reference cycles leak without
a word unless the program runs with `--debug-memory`, which prints the allocation balance
([memory.md](memory.md#cycles-leak)); signed arithmetic wraps in production builds and unsigned arithmetic
wraps in every build (D249); a `Concurrent` polled for `finished` under `resume_only_when_asked()` without
`run_ready()` never ends ([concurrency.md](concurrency.md#choosing-where-concurrents-resume)); a function that
declares a result can reach its end without a `return`, which answers whatever the C compiler leaves there; and
`String.to_integer()` and `to_long()` answer `0` for text that is not a number.


`assert` doubles as the way to prove a `T?` is not null without nesting: after `assert value` on a
`T?` local, parameter, or field, `value` is a plain `T` for the rest of that block and any block
nested inside it -- reads, writes, and method calls all go straight to the value the `T?` holds, and ownership
and dropping still belong to that `T?` (assigning a new `T` through the narrowed name drops the old one first,
exactly like overwriting any other owning slot). `assert value and
other_condition` narrows `value` too, and `other_condition` itself already sees the narrowed type. Assigning
the narrowed name something that may be null (`value = null`) makes it a `T?` again from that line, so reading
through it afterwards is the usual may-be-null error (`diagnostics/narrowed_name_reassigned`; the full rule is
under D43 below). A value type's narrowed `T?` is assigned the same way: `got = next`, with `got` and `next` both
narrowed `Long?`s, stores the whole `Long?` -- its presence and its value -- into `got`, inside a loop too, and
`got = missing` with a `Long?` that may be null makes it a `Long?` again (`conformance/stage6/narrowed_value_assignment`;
before, the C written for it did not compile).

```gdscript
var content = program_file.read()
assert content
var length = content.length()          # content is a String here, not String?
console.print(length)
```

Narrowing tests presence and never the value it holds: a `T?` of a number, an enum or a `String` holding `0`,
`0.0` or `""` is present, and so is an in-range `[]` read of an element holding it. A value type's `T?` carries a
presence flag beside the value (`has_value`), a reference's `T?` is its pointer, and a `[]` read goes through
`find_at`/`get`, which answer that flag -- no representation uses a sentinel, so no value can be mistaken for
absence (`conformance/stage6/present_zero`, which narrows zeros, `false` and `""` from lists, a dictionary, an
`Integer?` result, `first()`/`last()` and `remove_first()` by `crash`, `assert`, `if` and `and`). A `Boolean?` is
never a condition (below), so `false` meets only `== true`/`== false` or a `switch`.

`if` on a `T?` narrows the same way, in place, for the whole block: `if value { } else { }` runs the
block with `value` already a plain `T`, and the `else` exactly when it is null/absent. One rule for narrowing
everywhere -- `assert`, `if`, and `switch` all read/write/call straight through to the value the `T?`
still owns, and there is no other form.

**Lint:** a function whose body's *last* statement is `if value { ... }` **with no `else`** -- wrapping
the rest of the function only to check existence -- is a compile error, because it should be written with
`assert` instead (a narrowing `if` with an `else`, D3, already handles the missing case explicitly, so the no-
else lint never applies to it):

```
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

**An `if` that only returns the default is an `assert`** (D106, decided by Mortaro, 2026-09-24; narrowed by D244
to the functions where a guard `assert` is allowed). In a function returning nothing, a `T?`, or a `List`,
`Dictionary`, `Vector` or `Items`, an `if` with no `else` whose whole body is one `return` of the function's
default -- `null`, or a bare `return` in a function returning nothing -- is a compile error wherever it stands in
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
for the rest of the block. In a function returning anything else -- a number, a `Boolean`, a `String`, an enum or
an object -- a guard `assert` is an error (below), so an `if` that returns a value there is an answer written
down, `return 0` and `return false` included, and is left alone. How the
condition is turned around (proposed by Claude, unconfirmed):
`==` and `!=` swap, `<` becomes `>=` and `>` becomes `<=` (and back), `not x` becomes `x`, anything else becomes
`not x`, and `and`/`or` are turned around by De Morgan, side by side -- `if count < 0 or count > limit` becomes
`assert count >= 0 and count <= limit`. An `else if` is covered too. In a constructor, where `assert` is not
allowed (D28), the message names `crash` instead. `diagnostics/default_guard`, `diagnostics/returning_guard`.
**[implemented]**

**An `if` that leaves proves the opposite of its condition** (proposed by Claude, unconfirmed, 2026-09-27, so that
D244's explicit answers read as guards do). An `if` with no `else` whose block's last statement is a `return` or a
bare `crash` proves, for the rest of the enclosing block, what an `assert` of the opposite condition would: each
side of an `or` in its condition written `not path` narrows that path, as `assert path` does (the later sides
already see the earlier ones narrowed, so `if not found or found.count() == 0 { return 0 }` is one test), and every
other side proves the `[]` reads its opposite bounds (`if index >= names.count() { return "" }` proves
`names[index]`). The block itself sees none of it. A check that the `if` already proves is the usual "proves
nothing" error (`conformance/stage6/leaving_if`). Compile time only: the emitted test is the condition as written.
**[implemented]**

**`assert` narrows every link of a chain, not only the last** (D43, decided by Mortaro, 2026-09-20). Walking a
nullable structure would otherwise need one `assert` per hop, and a thousand asserts is not a language, it is a
tax:

```
assert class.namespace.namespace
console.print(class.namespace.namespace.name)
```

One `assert` proves the whole path, so `class.namespace` and `class.namespace.namespace` are both plain values
for the rest of that block and any block nested inside it. It is the same rule [Null safety and `assert` narrowing](#null-safety-and-assert-narrowing--implemented) already has for
`assert value and other_condition`, applied along a member chain instead of across an `and`.  **[implemented]**
Across an `and`, every side narrows, for `assert`, `crash` and `if` alike, since each side is known true when the
whole is: `crash names[position] and ages[position]` proves both elements, as two `crash` lines would (proposed by
Claude, unconfirmed, 2026-09-24; `conformance/stage6/and_narrowing`).

- It narrows the path asserted **and its prefixes**, nothing else. A sibling path stays `T?`: asserting
  `class.namespace.namespace` says nothing about `other_class.namespace`.
- A local holding a copy is its own path, so narrowing it says nothing about the path it copied
  (`diagnostics/namespace_nullable`) -- and a local that only copies a name or a path so it can be narrowed is
  itself an error (D63, decided by Mortaro): "'watcher' only copies 'tracker' so it can be narrowed: narrow
  'tracker' itself ('assert tracker') and use it directly" (`diagnostics/copy_to_narrow`). Its scope (proposed by
  Claude, unconfirmed): a `var` whose value is a bare name or member path of a `T?` type, which is then narrowed,
  is never assigned again, and whose source is not assigned later in the function either -- a snapshot taken
  before the source changes is not a copy for narrowing.  **[implemented]**
- A check on something that cannot be null proves nothing and is an error naming the fix: `assert tracker`
  written twice, or `crash` on a path an earlier `crash` already narrowed (`diagnostics/check_proves_nothing`).
  **[implemented; proposed by Claude, unconfirmed]** For a `[]` read the message names what proved it, so the
  line can simply go: `crash codes[index]` inside `while index < codes.count()` is "'codes[index]' is already
  proven by the loop condition 'index < codes.count()', so this 'crash' proves nothing: remove it"; a count or
  bound check is named the same way, and anything else as "an earlier check" (proposed by Claude, unconfirmed;
  `diagnostics/proven_element`). A plain `T?` that is already narrowed is "this value cannot be null here (it is
  a Tracker), so 'assert' on it proves nothing: remove the check".
- `crash` (below) narrows a chain the same way, since it narrows exactly as `assert` does but never returns.
- Reading through a link that may be null and has not been narrowed is still an error. This removes the verbosity of
  proving a path, not the requirement to prove it.
- Assigning to a narrowed path, or to anything it reads through, undoes the narrowing from that point on: after
  `outer = Box()` or `outer.inner = null`, `outer.inner` is a `Box?` again. Assigning a value that cannot be null
  (a constructor, a literal, a name that is not `T?`) to the narrowed path itself keeps it narrowed, and still
  undoes everything narrowed beneath it. A narrowed *name* follows the same rule: `current = current.next` or
  `maybe = null` after `assert current` stores into the `T?` it really is and un-narrows it
  (`diagnostics/narrowed_name_reassigned`).  **[implemented; proposed by Claude, unconfirmed]**
- **`while value` narrows its body like `if value`** (proposed by Claude, unconfirmed; implemented): the
  condition is tested again before every pass, and a store that may be null undoes it for the rest of the pass, so
  `while current { ... current = current.next }` walks a chain with no `assert` inside (`examples/linked_walk`).
- Inside a `while`, undoing a narrowing that was made before the loop *and read inside it* is an error, because
  the next pass would read it unproven: narrow it inside the loop instead (`diagnostics/path_narrow_loop`).
  **[implemented; proposed by Claude, unconfirmed]**


**Reading with `[]` answers `T?`** (D64, decided by Mortaro, 2026-09-23). `names[index]` and `table["key"]` may
not be there, so they are values that may be null, and an index or key that is a name, a path, a number or
quoted text makes the read a path that narrows like any other: `crash names[index]`, then `names[index].upper_case()`.
How a read is proven (the rules are Claude's proposal, unconfirmed -- D64 asked for them):

- **A proven count proves the indices below it.** After `crash names.count() == 3` (or `>= 3`, or `> 2`),
  `names[0]` to `names[2]` are plain values; so are they inside `if names.count() > 2 { }`. `names.count() != 0`
  and `not names.is_empty()` prove `names[0]` the same way (proposed by Claude, unconfirmed;
  `conformance/stage6/count_bound_proofs`). A count proves indices of a `List` only: a `Dictionary`'s keys need
  not be `0` to `count() - 1`, so after `crash names.count() == 1` a number-keyed `names[0]` is still a `T?`
  (`diagnostics/count_proves_no_key`).
- **A bound proves its index.** `index < names.count()` (or `names.count() > index`) in an `assert`, a
  `crash`, an `if`, or a `while` proves `names[index]` in what follows -- the loop body, for a `while` -- and
  `index < names.count() - 1` does too. On the left of an `and`, it proves the right side, so
  `while index < lines.count() and lines[index] != "end"` needs nothing more. Every side of an `and` holds in
  what the condition guards, so `while not found and index < names.count()` proves `names[index]` in the body.
  A list in a local, a parameter and an attribute are proven alike, and `assert`/`crash` on a read already
  proven is an error that says so (proposed by Claude, unconfirmed; `diagnostics/proven_index_check`).
- **Assigning the list or the index undoes it**, as for any path ([Null safety and `assert` narrowing](#null-safety-and-assert-narrowing--implemented), D43): `index = index + 1`
  un-proves `names[index]`, and so do `names.clear()`, `remove_at`, `remove_first` and `remove_last`. Inside a
  loop, undoing a proof made before the loop that the loop has read is an error.
- **Any index without a call is a path** (proposed by Claude, unconfirmed, 2026-09-24): `crash glyphs[code - 32]`
  proves `glyphs[code - 32]` for what follows, not only `glyphs[index]`. The index may be names, member reads,
  numbers, `true`/`false`, enum values, operators and other `[]` reads; two indices are the same when D78's
  printer prints them the same, so spacing and redundant parentheses do not matter. Assigning any name the index
  reads undoes it, as assigning the index itself does. A call anywhere in the index keeps the read a plain `T?`,
  since the call could answer something else the second time. A call *between* the check and the read undoes it
  only when the call may change the list or what the index reads (D169, below). `conformance/stage6/structural_index`,
  `diagnostics/structural_index_undone`.
- **A call undoes a proof only if it may change what the proof depends on** (D169, decided by Mortaro,
  2026-09-25): "we need to be smart enough at compiler to figure out if going down into that function has any
  chances of altering that value (without needing to have a const keyword)." This covers every proof -- a
  narrowed path or attribute and a proven `[]` read alike. The compiler follows the called function and what it
  calls, and collects which attributes it may assign and which lists it may shrink (`clear`, `remove_at`,
  `remove_first`, `remove_last`, `remove`); a proof that reads through one of them is undone, and reading it again
  unproven is the usual error. How it is followed (proposed by Claude, unconfirmed): an attribute is known by its
  class and name, so assigning `Scope.owned` does not undo a proof about `Monster.owned`; a receiver's class is
  read from declared types (a parameter's type, a `var` built by a constructor or annotated), a `List`,
  `Dictionary` or `String` receiver included, so `items.count()` reaches only `List.count` and never a program's
  own `count`; a receiver that is itself an expression has the class of that expression's type -- the class a
  constructor makes (`File(path).read()` reaches `File.read`), the class the called function returns
  (`names.copy().count()` reaches `List.count`), the element class of a `[]` read on a `List`, `Vector` or
  `Dictionary` (`lists[0].count()`), an attribute read's declared class (`receiver_call_effects`); a call whose
  receiver's class cannot be told (a shape, a type parameter, a function value) is followed into every function
  of that name; `clear`, `remove_at`, `remove_first` or `remove_last` on a collection reached through `[]` or a
  call rather than a name may be the very list a proof reads, so it undoes every proof about a list
  (`append`, `prepend` and `insert` there count as growing a borrowed `Vector`'s storage, D204/D206); the calls a `return` makes undo nothing, since nothing after it runs on that path (a call in a branch
  before its `return` still undoes proofs after the branch); a list passed to a function that
  shrinks its parameter undoes proofs about the list passed; calling a function value may do anything, so it
  undoes every proof that reads an attribute or a list; operator functions and getters are not followed; a
  constructor's own assignments are to the new object and change nothing proven. A local that aliases an
  attribute's list is not tracked (a proven read still checks its bounds). Inside a `while`, a call that undoes a
  proof made before the loop and read inside it is an error naming the call (`diagnostics/call_undoes_proof`).
  All of it happens while compiling; nothing is emitted. **[implemented]**
- **A proven read still checks its bounds at runtime** and answers the default out of range, so a counter that
  went negative gives a wrong value rather than reading memory it does not own.
- **A `Boolean?` cannot be a condition** -- not in `if`, `while`, `assert` or `crash`, and not under `not`, `and`
  or `or`: `if flags[index]` would test that the element is there, not that it is true, and the two mean
  opposite things for `false`. Prove the element is there first, or `switch` over it; `== true` also works,
  since comparing needs no narrowing.
- Writing through `[]` is unchanged, and **assigning through a `T?` is an error** naming the fix: "this value
  may be null (it is a Box?), so 'label' cannot be assigned through it yet: narrow it first with 'if value { }',
  'assert value' or 'crash value'" (`diagnostics/store_through_nullable`).

`first()` and `last()` answer a `T?` too, `null` on an empty list, by the same argument (proposed by Claude,
unconfirmed; the compiler's own reads of them are narrowed with `crash`), and so do `remove_first()` and
`remove_last()` (D211): nothing to take from an empty list is a normal outcome. `tests/list_tests`,
`diagnostics/index_reads`, `conformance/stage3/lists`.
**[implemented]**

**Comparing needs no narrowing** (D69, decided by Mortaro, 2026-09-23). `==` and `!=` accept a `T?` on the left:
null is not equal to anything, so `crash Spite.Class.namespace == "Spite"` is a whole test. Either side may be the `T?`. Only the comparison
is exempt -- reading a member through a `T?` still needs it narrowed first. A `Spite.Namespace` compares with
text through `equals(String)` in `library/spite/namespace.spite`, against its `name_with_namespaces`; two
namespaces still compare by identity, because a class's `equals` is used for a right side of that same class
only when `equals` takes that class.  **[implemented]**

### Failure: three outcomes and no others  **[partial]**

D24 (decided by Mortaro, 2026-09-19): Spite has **a compile error, an `assert`, or a crash**. There are no
exceptions, no error unions, no bubbling, and no error value carrying a message, because "errors and exceptions
tend to be useless: they tell us a message we have no action to take about them".

1. **A compile error**, for anything the compiler can know. This is already where the language keeps landing:
   an unserialisable type ([Foreign libraries](foreign_libraries.md#foreign-libraries--partial)), the wrong target (D20), an unfilled codegen hole ([Codegen values (`$`)](metaprogramming.md#codegen-values---implemented)), a
   missing foreign symbol ([Foreign libraries](foreign_libraries.md#foreign-libraries--partial)), a member that does not fit a template ([Standard library metaprogramming](collections.md#standard-library-metaprogramming--partial)), a symbol outside its
   enum ([Types](values_and_types.md#types)).
2. **`assert`**, for when the program should keep running.
3. **`crash`**, for when everything should stop so the code gets rewritten.

**What `crash` is for** (D199, decided by Mortaro): what the compiler can prove away, so a program written with
its help never meets it; what leaves the program unable to work at all; and a developer's mistake -- a whole
number divided by zero (D201, [values_and_types.md](values_and_types.md)), signed arithmetic that does not fit its
type in a development build ([values_and_types.md](values_and_types.md#numeric-types--implemented-provisional)), a `Float` gone to infinity that `JsonWriter`
is asked to write (D198, [json.md](json.md)). A condition the program can meet in normal use -- a missing file,
a user's bad input, an absent record -- answers `T?` or an empty value (D24, D26), never a crash, and a library
`crash` must be one the compiler can show the program how to avoid, or a bug in the program that made the value.

`T?` is the only runtime failure value, and it carries no reason. A caller narrows it with `assert`,
halts on it with `crash`, or handles the absent case with `if value { } else { }`.

**If a distinction is actionable, it is data, not an error** (proposed by Claude, unconfirmed). A caller that
must tell a timeout from a rejection takes back a `type`, `union` or enum modelling exactly that -- ordinary
data with ordinary handling. Something you can act on was never an error; it was a value that was not modelled.

#### `assert` is control flow

D26 (decided by Mortaro, 2026-09-19): `assert` is a guard clause, not validation -- "this value is absent, so
there is no more logic to do here, but the program is fine and keeps serving everything else". It is why a web
server or a game written in Spite should rarely crash.

**A guard `assert` answers "nothing", so it is allowed only where the result can say it** (D244, decided by
Mortaro; D245, decided by Claude under D244; superseding D106's "legal in a function returning any type", which
had relaxed D27's limit to functions returning nothing or `T?`). A failed `assert` returns at once, answering:

- nothing, in a function returning nothing;
- `null`, in a function returning a `T?`;
- an empty one, in a function returning a `List`, `Dictionary`, `Vector` or `Items`.

In a function returning anything else -- a whole number or a `Float`/`Double` (`0`), a `Boolean` (`false`), a
`String` (`""`, since an empty text is a real text), an enum (its first value), a `Memory.Address`, a union or a
class that cannot be null (an object with every attribute at its default) -- an `assert` is a compile error, since
the caller would carry on with a default it cannot tell from a real answer:

```
this 'assert' would answer a default Integer (0) that a caller cannot tell from a real one: return a value ('if entity >= rows.count() { return -1 }'), make the result 'Integer?', or 'crash entity < rows.count()' if this is a developer mistake
```

The message names the opposite condition, an answer of the type (`-1`, `-1.0`, `false`, `""`, the enum's first
value, or `...` for an object), the nullable result (left out for a `Boolean`, since a `Boolean?` is never a
condition) and the `crash`. It is checked against the result type of each function as compiled, so in a generic
class it is checked per instance, and an `assert` whose condition is decided while compiling
(`assert $slot_type == Entity`) is held to it too: its instance would answer the default every call. `crash` is
never limited this way. Which of the three a site takes is the writer's decision (D199): a `T?` when the case is
met in normal use, a value chosen on purpose when one outside the real answers already means "none", a `crash`
when asking is the caller's bug (proposed by Claude, unconfirmed: the message, the choice of `-1`, and that
`String` counts as a value). SlopEngine met it three times in a day: `World.create_entity_from_bundle(): Entity`
answered a default `Entity` instead of the one just made, `deform_layer(): Integer` answered offset 0 for a mesh
without skin, `object_world(): Math.Matrix4` answered a default matrix for an object without a parent
(`diagnostics/default_answer`, `conformance/stage6/leaving_if`). **[implemented]**

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

D28: **`assert` is banned in a constructor**, including the entry class's own (D29). A constructor is setup, not
logic: if something can be invalid inside one, function logic has been put where only setup belongs. The caller
receives an object with every attribute at its default and cannot tell it from a properly built one, which is
exactly the surprise the rule exists to prevent. A program constructor that grows complex splits into smaller
functions it calls -- and reads as a table of contents for the program, with the logic one level down. The
error is "'assert' is not allowed in a constructor: a constructor is setup, not logic. Take already resolved
values, or use 'crash' when absence is a bug", and an `if` that only leaves a constructor is "this 'if' only
leaves the constructor, and a constructor is setup, not logic: take already resolved values, or write 'crash
...' when this is a bug" (`diagnostics/default_guard`). `crash` is allowed in a constructor.

#### `crash`

D30 (decided by Mortaro): `crash` is a keyword, so the compiler takes its context from the program rather than
from a message string. **It mirrors `assert` exactly** -- same polarity, same shape, different severity:

```
assert database.connect()        # falsey: return the default, keep going
crash  database.connect()        # falsey: halt
```

The compiler captures the condition's source text and every operand value, so nothing has to be written and
nothing can drift out of sync. **A bare `crash` is the one form for a branch that cannot happen** (D119,
decided by Mortaro): it reports its enclosing context, and the formatter rewrites `crash false` to it. `crash
user` narrows a `T?` exactly as `assert user` does, but never returns.

Absence is fine -> `assert`. Absence is a bug -> `crash`. Absence is meaningful -> `if value { } else { }`.

#### What a crash reports

D25: a crash always reports backend information **and the trace of every `assert` that failed before it**. Those
asserts are the causal trail explaining how the program reached the state that crashed, which is usually several
frames earlier than the crash itself. The cost is only on the failure path -- a passing `assert` already
branches -- and the trace is a fixed-size ring buffer with a total count, so it never allocates and cannot grow
without bound. **Not built** from D25's list: the call chain, and the default each failed `assert` returned;
a report is the crash's own line and the asserts' lines. Compile-time evaluation ([the functions of `Spite.Class`](reflection.md#functions-of-spiteclass-and-no-static-functions), [JSON is reflection, not a library](json.md#json-is-reflection-not-a-library--implemented)'s generated JSON) reports
the same way.

D26 refinement (proposed by Claude, unconfirmed; **not built**): the trace would record **predicate asserts
only**. A narrowing assert firing is routine control flow -- thousands an hour on a server -- and on concurrent
work the last few would come from unrelated requests, reading as a causal chain that does not exist; a per-site
count would cover them instead. As built, a failed narrowing `assert` (`assert found`) enters the ring like a
predicate one.

D189: **an `assert` in `library/` never enters the trace.** The trace explains how *the program* reached a crash,
and the standard library's asserts are routine answers (a missing key, text that does not match, a read past the
end) that fire constantly and would push the program's own entries out of the ring. The compiler leaves the ring
write out of every `assert` site whose file is in `library/`, so a library guard costs what an `if` costs; the
program's own files and every `load`ed package keep recording, and a crash inside the library still reports its
own site (`conformance/stage5/library_guards_untraced`).

D32: **a crash site is identified by a compile-time id, and the compiler emits an id-to-source map as a build
artifact** rather than embedding the information in the program. The binary carries none of it, which matters
most in a wasm module where size is startup time, and **the same logical site keeps the same id across every
target**, so a crash from the server bundle and one from the browser bundle compare directly. The id is
content-derived -- a hash of namespace, class, function, the site's ordinal and the condition source -- never a
counter, because a counter renumbers everything below an inserted line and destroys both old reports and
cross-target identity. **Not built:** the binary without the text. Every build today writes each site's report
line -- id, place, class, function and condition -- into the program as fixed text beside writing the map, so no
build needs a lookup; other targets are not built either ([targets.md](targets.md)).

D33: the map is a greppable tab-separated table named after the program, `<program>.crashes`, beside the
executable, one line per site, sorted by id so archived maps diff cleanly: id, file, line, column (written as `0`
today), class, function, kind (`crash`, `assert-predicate`, `assert-narrowing`), the condition source, and the
operand names with their types (`value:Integer`). It opens with a `#` comment line carrying the format version
and the build hash (`# spite crashes 1 2660c8aa`). At runtime a crash emits one tab-separated line prefixed
`spite.crash`, and an assert `spite.assert`. **Values are rendered as text at crash time but stored raw in the
ring buffer**: a crash happens once and then the program is dead, so it should be informative rather than fast,
while an assert may fire thousands of times an hour and must stay cheap until something dumps it. **Not built:**
an assert's values in the ring. A ring entry is a pointer to its site's fixed `spite.assert` line, so a failed
`assert` stores one pointer and counts it, and its trace line names the condition without operand values.

**Implemented.** `crash` is built -- the keyword, narrowing, bare `crash` for an unreachable branch, the compiler
enforcing D28/D29 and D106 -- and so is the reporting above: crash and assert sites carry content-derived ids, every
build writes `<program>.crashes`, and a crash prints the asserts that failed before it, oldest first, from a ring
of 32, with a `spite.assert	earlier=<count>` line when more failed than it kept
(`conformance/stage5/crash_report`). A firing `crash` flushes stdout, writes its line to stderr, and exits with
status 1:

```
spite.crash<TAB>id<TAB>path:line<TAB>Class<TAB>function<TAB>condition
```

The condition is rebuilt from its own tokens and the line ends with the named operands of the failed
comparison and their values -- `value > limit<TAB>value=-9<TAB>limit=0` -- with calls never evaluated a second
time. **A condition that is a call on a value** (D228), `crash file.exists()` or `crash not list.contains(x)`,
has that value as its operand when it is a name or a path: text, a number or an enum is written as it is, and an
object whose class declares `to_string()` with no arguments is written as what that answers, called once on the
way out -- `settings.exists()<TAB>settings=saves/settings.txt`, since `File`'s `to_string()` is its path. A value
with no `to_string()` (a `List`) adds nothing, and the call itself is not evaluated again. The `.crashes` map
lists it with its type (`settings:File`; `conformance/stage5/crash_receiver`).

D248 (under D244): **a failed narrowing reports what is missing, never a value.** A `crash` that narrows a `T?` --
a name, a path, a `[]` read, each side of an `and` -- fails only when something is absent, and printing a value
there would print a default (`clip.keys[start + 9]=0`), which reads as a present zero and sends the reader after
a bug that does not exist. The report tests each nullable link of the path again, in order, and names the first
one that is absent: `rig.skeleton is null` for a name or member, `list[expression] is missing: index 11990, count
11500` for a list read (the index as evaluated, and the list's count), `table[key] is missing: key "bea"` for a
dictionary or any other `get_at` (a `String` key quoted, so an empty one shows). The index is evaluated again, which
is safe because an index with a call in it is not a path. The value report of D25/D32 stays for every condition
that is not a narrowing (`conformance/stage6/crash_missing_item`, `crash_missing_key`, `crash_missing_link`,
`crash_missing_value`). Cost: only on the failure path, which already halts; a passing `crash` is the same
presence test it always was.
time.

#### What a native fault reports

D244 (decided by Mortaro): anything that can go wrong silently is a bug, and a native fault -- an access violation
or `SIGSEGV`, a stack overflow, an illegal instruction, `SIGBUS`, `SIGFPE` -- used to end a program with nothing
printed. The design below is Claude's (proposed by Claude, unconfirmed; the field names provisional under D214).

- **Every program installs a fault handler**, in every build, as the first line of `main`: on Windows
  `SetUnhandledExceptionFilter` and `SetThreadStackGuarantee` (16 KB kept back, so a stack overflow can still be
  reported on the thread that overflowed); on Linux and macOS `sigaction` for `SIGSEGV`, `SIGBUS`, `SIGILL`,
  `SIGFPE` and `SIGTRAP`, run on an alternate signal stack of 64 KB per thread. Every thread the program starts --
  a pool runner, a waiting call's thread, the REPL's and the file watcher's -- sets up its own room the same way.
  An unhandled exception only: a fault a foreign library catches itself never reaches it.
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
  [optimizations.md](optimizations.md#the-fault-handler-is-in-every-program)).
- In an `--optimized` build a function the C compiler inlined into its caller is reported as the caller. A
  function `--hot-reload` swapped in lives in the reload's library, so a fault in it names that library in `at=`
  and no Spite function.
- **The last foreign call**: each foreign call first stores a pointer to a fixed text naming the C function, its
  library and the calling line in a thread-local, in every build (D214: one store per call measured as nothing
  against the call itself, so it is not kept to inspectable builds). A library reopened by `--hot-reload` records
  into its own copy, which the host's report does not see.
- **Not reported**: what the system ends without asking the program -- Windows' fast fail on a corrupted heap or a
  `__fastfail` -- and anything while a debugger is attached, which sees the fault first. A fault on a thread a
  foreign library started is reported, but without room kept for a stack overflow and without a last foreign call.

**Implemented** on Windows, and compiled (not run) for Linux and macOS: `conformance/stage6/native_fault_foreign`
(a null read inside a fixture library), `native_fault_stack` (endless recursion) and `native_fault_illegal` (an
illegal instruction inside a fixture library). `check.sh` compares their reports with the offset after `+0x`
left out, since it is the C compiler's, and also builds `native_fault_foreign` `--optimized` from four translation
units, where the handler and the table land in the first unit and the functions it names are spread over all four.
