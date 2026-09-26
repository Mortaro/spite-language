# Nullable values and failure

Spite has no exceptions, no error values and no `Result` types. Something that may not be there is a `T?`, and
something that went wrong has exactly three outcomes: a compile error, an `assert`, or a `crash`.

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
| `assert value` | the function returns its default; after it, `value` is a `T` for the rest of the block |
| `crash value` | the program halts; after it, `value` is a `T` |
| `while value { }` | the loop ends; inside the body, `value` is a `T` |
| `switch value { Monster: ... Null: ... }` | runs the `Null` case |

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
    console.print("[{first_line}]")
}

func read_first_line(text: String?): String {
    assert text
    var text = text.trim()
    var lines = text.lines()
    var first = lines.first()
    crash first
    return first
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

- **A proven count proves the indices below it**: after `crash names.count() == 3`, `names[0]` to `names[2]`
  are plain values.
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

`assert condition` is a production feature, not a debug one: when the condition is false, the function returns
its return type's default immediately -- `false`, `0`, `""`, `null`, an empty value, or nothing -- and no code
after it runs. It works in a function returning any type, and a failed one is remembered for the crash report
([below](#what-a-crash-reports)).

```gdscript title=assert_guard/assert_guard.spite entry
var console = Console()
var handle = -1

func AssertGuard() {
    var closed = is_open()
    handle = 3
    var opened = is_open()
    console.print(closed, opened)
    var total = sum_positives(2.5, 1.5)
    var refused = sum_positives(-1.0, 4.0)
    console.print(total, refused)
}

func is_open(): Boolean {
    assert handle != -1
    return true
}

func sum_positives(first: Float, second: Float): Float {
    assert first > 0 and second > 0
    return first + second
}
```
```output
false true
4 0
```

When a caller must tell absence from a real `0` or `false`, return a `T?` instead -- or, if absence is a bug,
`crash`.

### An `if` that only returns the default is an `assert`

An `if` with no `else` whose whole body returns the function's default -- `false`, `0`, `0.0`, `""`, `null`, or a
bare `return` in a function returning nothing -- is a guard spelled the long way. It is a compile error wherever
it stands, inside a loop or a nested `if` included, and the message names the `assert` of the opposite
condition. Returning anything else (`return -1`, `return true`) is a real answer and is left alone:

```gdscript title=default_guard_error/default_guard_error.spite entry error
var console = Console()
var handle = -1

func DefaultGuardError() {
    var open = is_open()
    console.print(open)
}

func is_open(): Boolean {
    if handle == -1 {
        return false
    }
    return true
}
```
```diagnostic
this 'if' only returns the default 'false', which is how a guard is written: write 'assert handle != -1' and let the rest run unindented
```

The fix reads top to bottom:

```gdscript title=default_guard_fixed/default_guard_fixed.spite entry
var console = Console()
var handle = -1

func DefaultGuardFixed() {
    var open = is_open()
    console.print(open)
}

func is_open(): Boolean {
    assert handle != -1
    return true
}
```
```output
false
```

The message turns the condition around for you -- `if count < 0 or count > limit` becomes
`assert count >= 0 and count <= limit` ([how](#null-safety-and-assert-narrowing--implemented)).

Sometimes the default is the real answer and not a guard: an empty text is zero bytes long. The rule still holds,
so give that answer through a local set in an `if`/`else` and return it once:

```gdscript title=default_answer/default_answer.spite entry
var console = Console()
var text = ""

func DefaultAnswer() {
    var bytes = size()
    console.print(bytes)
}

func size(): Integer {
    var bytes = 0
    if text.is_empty() {
        bytes = 0
    } else {
        bytes = text.length() + 1
    }
    return bytes
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

### What a crash reports

A crash flushes what the program printed, writes one tab-separated line to the error stream and exits with
status 1. The line starts `spite.crash` and carries the site's id, its place, its class and function, the
condition, and the named operands with their values -- then a `spite.assert` line for each `assert` that failed
before it, because those are usually the trail that explains how the program got there:

```
spite.crash	64b935f1	crash_report/crash_report.spite:14	CrashReport	check	value > limit	value=-9	limit=0
spite.assert	0aae5005	crash_report/crash_report.spite:18	CrashReport	announce	amount > 0
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

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Null safety and `assert` narrowing  **[implemented]**

`assert` doubles as the way to prove a `T?` is not null without nesting: after `assert value` on a
`T?` local, parameter, or field, `value` is a plain `T` for the rest of that block and any block
nested inside it -- reads, writes, and method calls all go straight to the value the `T?` holds, and ownership
and dropping still belong to that `T?` (assigning a new `T` through the narrowed name drops the old one first,
exactly like overwriting any other owning slot). `assert value and
other_condition` narrows `value` too, and `other_condition` itself already sees the narrowed type. Assigning
the narrowed name something that may be null (`value = null`) makes it a `T?` again from that line, so reading
through it afterwards is the usual may-be-null error (`diagnostics/narrowed_name_reassigned`; the full rule is
under D43 below).

```gdscript
var content = program_file.read()
assert content
var length = content.length()          # content is a String here, not String?
console.print(length)
```

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

**An `if` that only returns the default is an `assert`** (D106, decided by Mortaro, 2026-09-24). An `if` with no
`else` whose whole body is one `return` of the function's default -- `false`, `0`, `0.0`, `""`, `null`, or a bare
`return` in a function returning nothing -- is a compile error wherever it stands in the function, inside a
`while` or a nested `if` included, and the message names the `assert` of the opposite condition:

```gdscript
func is_open(): Boolean {
    if handle == -1 {        # error: this 'if' only returns the default 'false', which is how a guard is
        return false         #        written: write 'assert handle != -1' and let the rest run unindented
    }
    return true
}
```

It is the same rule as narrowing, so `if not value { return null }` becomes `assert value` and narrows `value`
for the rest of the block. Returning anything else -- `return true` from a `Boolean` function, `return -1` -- is an
answer, not a guard, and is left alone. A default that is a real answer (an empty text is zero bytes) is still
caught, and the message names the way to give it: a local set in an `if`/`else` and returned once. How the
condition is turned around (proposed by Claude, unconfirmed):
`==` and `!=` swap, `<` becomes `>=` and `>` becomes `<=` (and back), `not x` becomes `x`, anything else becomes
`not x`, and `and`/`or` are turned around by De Morgan, side by side -- `if count < 0 or count > limit` becomes
`assert count >= 0 and count <= limit`. An `else if` is covered too. In a constructor, where `assert` is not
allowed (D28), the message names `crash` instead. `diagnostics/default_guard`, `diagnostics/returning_guard`.
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
  `names[0]` to `names[2]` are plain values; so are they inside `if names.count() > 2 { }`.
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
number divided by zero (D201, [values_and_types.md](values_and_types.md)), a `Float` gone to infinity that `JsonWriter`
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

**`assert` is legal in a function returning any type, and a failed one returns that type's default** (D106,
decided by Mortaro, superseding D27's limit to functions returning nothing or `T?`) -- `false`, `0`, `""`,
`null`, an empty value, or nothing. `if handle == -1 { return false }` returns the very same default, only spelled
as an indented `if`, so that `if` is an error naming its `assert`
([Null safety and `assert` narrowing](#null-safety-and-assert-narrowing--implemented), "An `if` that only returns
the default is an `assert`").

```gdscript
func is_open(): Boolean {
    assert handle != -1              # a failed assert returns false
    return true
}
```

When the caller must tell absence from a real `0` or `false`, the choice is still deliberate: return `Integer?`, or,
if absence is a bug rather than a case, `crash`.

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
time.
