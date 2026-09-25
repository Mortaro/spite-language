# Nullable values and failure

Spite has no exceptions, no error values and no `Result` types. Something that may not be there is a `T?`, and
something that went wrong has exactly three outcomes: a compile error, an `assert`, or a `crash`.

## `T?`: a value that may be null

`Monster?` is a `Monster` or `null`. It is not a special wrapper: it is the union of `Monster` and `Null`, a class
whose only value is the literal `null`, so everything that works on a union works on it. `null` exists only as the
empty state of a `T?`; every other type has a default value instead (`0`, `""`, `false`, a class's field
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

func Monster(starting_health: Int) {
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

func find_monster(missing: Bool): Monster? {
    assert not missing
    return Monster(30)
}

func find_name(missing: Bool): String? {
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

- **A proven count proves the indices below it**: after `crash names.count() == 3` (or `>= 3`), `names[0]` to
  `names[2]` are plain values; so they are inside `if names.count() > 2 { }`.
- **A bound proves its index**: `index < names.count()` in an `assert`, `crash`, `if` or `while` proves
  `names[index]` in what follows -- the loop body, for a `while`. On the left of an `and` it proves the right
  side, so `while index < lines.count() and lines[index] != "end"` needs nothing more.
- **The index may be any expression without a call**: `crash glyphs[code - 32]` proves `glyphs[code - 32]`
  on the next line, however it is spaced, as `crash glyphs[index]` would; two indices are the same when they
  print the same (`conformance/stage6/structural_index`). An index with a call in it, `glyphs[offset()]`, is
  not a path: name it first.
- **Changing the list or the index undoes it**: `index = index + 1`, `names.clear()`, `remove_at`,
  `remove_first` and `remove_last` un-prove it, as assigning any path does. Assigning any name the index reads
  counts: after `code = 34`, `glyphs[code - 32]` must be proven again (`diagnostics/structural_index_undone`).
- **Checking a proven read again is an error**: inside `while index < codes.count()`, `crash codes[index]` proves
  nothing, and the message says what already proved it -- "'codes[index]' is already proven by the loop
  condition 'index < codes.count()', so this 'crash' proves nothing: remove it" -- so delete the line
  (`diagnostics/proven_element`).
- A proven read still checks its bounds at run time and answers the default when out of range.
- A `Bool?` cannot be a condition: `if flags[index]` would test that the element is there, not that it is true.
  Prove it is there first, or compare it: `flags[index] == true`.

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
    var ages = Dictionary<Int>()
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

func is_open(): Bool {
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

func is_open(): Bool {
    if handle == -1 {
        return false
    }
    return true
}
```
```diagnostic
this 'if' only returns the default 'false': write 'assert handle != -1' and let the rest run unindented
```

The fix reads top to bottom:

```gdscript title=default_guard_fixed/default_guard_fixed.spite entry
var console = Console()
var handle = -1

func DefaultGuardFixed() {
    var open = is_open()
    console.print(open)
}

func is_open(): Bool {
    assert handle != -1
    return true
}
```
```output
false
```

The condition is turned around for you: `==` and `!=` swap, `<` becomes `>=` and `>` becomes `<=`, `not x`
becomes `x`, and `and`/`or` are turned around side by side -- `if count < 0 or count > limit` becomes
`assert count >= 0 and count <= limit`.

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
operands, so nothing has to be written and nothing can drift out of date. `crash false` marks a branch that
cannot happen (a bare `crash` is formatted to it).

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

func full_name_length(key: String): Int {
    var name = names.get(key)
    crash name
    return name.length()
}

func describe(value: Int): String {
    crash value > -5
    if value > 0 {
        return "positive"
    }
    if value < 1 {
        return "not positive"
    }
    crash false
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
