# The Spite Language Manual

This is the normative description of Spite. `PLAN.md` tracks compiler implementation details and milestones.
Every language decision gets written here (and logged in [Decision log](#decision-log)) when it is made.

Status tags: **[implemented]**, **[partial]**, **[planned]** refer to the compiler.

---

## 1. Philosophy

- Spite is written mostly by AI and skimmed by humans. Code must read easily; it does not need to be pleasant to type.
- The language is extremely opinionated and small. There is one way to do each thing. There are no macros and no clever features.
- Readability comes from metaprogramming and the standard library: prefer `repositories.filter_active().count_stars()` over loops and ifs.
- No abbreviations, anywhere, except the language keywords themselves (`var`, `func`, `enum`).
- Compilation speed and live reload matter more than anything else in the toolchain.
- Spite compiles to C (later possibly LLVM). The compiler is written in Spite and compiles itself,
  and the standard library follows it: the hand-written C runtime is a bootstrapping stage, not the design (D14,
  [Pure Spite](#pure-spite-dissolving-the-runtime-planned)).

### Self hosting  **[partial]**

The compiler is written in Spite and compiles itself. `bootstrap/seed/spite_compiler.c` is the C it emits for
its own sources, so a C compiler is all that is needed to build it, and `bash check.sh` requires generation 2
and generation 3 to be byte identical before anything else is believed. See `docs/self_hosting.md`.

The language is larger than the subset the compiler implements today; `bootstrap/COMPILER_PLAN.md` is the
progress log, and `conformance/` is the part that demonstrably works.

## 2. Lexical structure  **[implemented]**

- Newlines end statements and separate entries in lists, objects, enums, unions and types. Commas separate entries written on one line.
- Comments start with `#` and run to the end of the line.
- `"text"` is a String. Strings only use double quotes.
- `'value'` is an enum value. Single quotes are only for enum values.
- `name` (lowercase, snake_case) is a variable, attribute, function or folder.
- `Name` (uppercase first letter) is a class or type, including the whole standard library.
- `$name` is a codegen value: it is replaced at code generation time (see [Codegen values](#9-codegen-values-)).
- `_name` is private. (This may change.)
- Keywords: `var func return if else while switch type enum union generic assert crash and or not null true false this`
  (`generic` since D87, `this` since D83: the value a function answers on, section 4), plus `singleton`, which is a keyword only as a line of its own at the top of a file
  (D104; it stays usable as a name, since `Spite.Class` has an attribute called `singleton`) — there is
  no `do` (second batch item 2, decided 2026-09-19: with `for` gone, `if value do name` had nothing to match, so
  `if value { } else { }` narrows in place instead, section 5). Writing `do` is a parse error naming the
  `if value { }` form.

## 3. Files are classes  **[implemented]**

Each `.spite` file is exactly one class, named by the file name in PascalCase (`repository_list.spite` is `RepositoryList`).
This cannot be changed.

A file contains only declarations:

- a `singleton` line, when the class has one instance (section 8, D104)
- `generic $name` lines: the codegen values a caller supplies (section 9, D87)
- `var` declarations: the attributes of the class
- `func` declarations: the functions of the class
- `type`, `enum` and `union` declarations, which are namespaced under the class (`Player.Job`)

There are **no file level statements**: no loops, ifs or calls outside a function.

```person.spite
var age = 0

func Person(new_age: Int) {
    age = new_age
}
```

### Constructors and the entrypoint

A function named like its class is the constructor. A class without one is constructed with `Person()` and just gets its defaults.
There is no `main`: a program is a folder, its entry file is the file named after the folder (`kal/kal.spite`), and
the program is run by constructing that file's class. The entry constructor takes **no arguments** (D89,
`diagnostics/entry_constructor_arguments`): the command line is read through `Environment()` (run-time settings),
`Build()` (what the build decided) and `Arguments()` (the raw list), anywhere.

```kal.spite
func Kal() {
    var some_key = Arguments().some_key
    if some_key {
        console.print(some_key)
    }
}
```

**Nothing starts a program behind its back** (D97; the details proposed by Claude, unconfirmed). Running one is
itself Spite: `launcher/launcher.spite`, at the root of the repository, is the class the executable's `main`
constructs, and its constructor is the whole of how a program is loaded:

```launcher.spite
var build = Build()

func Launcher() {
    load("library")
    load("library/{build.target_operating_system}")
    load(build.program)
}
```

The compiler reads the launcher first and follows its `load` calls in order, instead of walking the standard
library by a rule of its own: `library/`, then the folder of the operating system the program is compiled for
(`library/windows/`, `library/linux/` and `library/mac/` are loaded only by name), then the program's folder,
`build.program`, the folder named on the command line. A `load` in the launcher may use text and the `Build`
fields the compiler knows before reading anything, read through a `var` of the launcher bound to `Build()` (D110) (`target_operating_system`, `operating_system`, `program`, and
the ones given as flags); anywhere else `load` still takes a literal. Loading the program's folder is what runs
it: that one `load` constructs the program's entry class, runs a `--repl` loop on it, and releases it. `main`
keeps only the floor -- it hands `argv` to `Arguments()`, puts standard output in binary mode on Windows, and after
`Launcher` returns releases the singletons and class objects and prints the `--debug_memory` balance. `Launcher`
is library code for reflection (`Spite.Class.instances` does not list it) and `--final_classes` prints it.

Configuration files are ordinary classes whose constructor sets the state.

**A constructed object must be kept and used** (D139, decided by Mortaro; the message proposed by Claude,
unconfirmed). **[implemented]** A constructor call written as a statement on its own (`Spawn(bundle)`,
`Report(text)`, `Box<Int>(3)`) is an error: `'Report(text)' makes a 'Report' and drops it: a constructed object
must be kept and used, so a class whose construction is the whole point should be a function instead -- turn
'Report' into a function of the class that needs it, or keep the object in a variable that is read`. One stored in
a variable nothing reads is already the unused-name error (D118, D136). A singleton is exempt: it is bound as an
attribute (D110, D144), and a bare `Console()` statement is D110's inline-singleton error instead.
`diagnostics/dropped_construction`, `docs/classes_and_files.md`.

## 4. Variables and values  **[implemented]**

```gdscript
var variable_name = "lorem ipsum"
var inline_list = [1, 2, 3, 4]
var multiline_list = [
    "no"
    "commas"
    "between"
    "lines"
]
var object_name = {
    key: "value"
    other_key: 3
}
var target: Monster? = null
```

- Every variable has a default value. The type is inferred from it, or annotated with `: Type`.
- Every class has a default value (`Int` 0, `Float` 0.0, `Bool` false, `String` "", a class: its attribute defaults).
  Operations that cannot succeed produce the default instead of crashing -- except reading with `[]`, which
  answers `T?` (D64): an index or key that may not be there is a value that may be null, narrowed like any other.
- `null` exists only as the empty state of `T?`. See [Open questions](#open-questions) for `= null` on other types.
- An inner `var` may shadow an outer local, parameter, or attribute with the same name (second batch item 4,
  decided 2026-09-19), **and a `var` may shadow a name in the same scope too** (D51, decided by Mortaro,
  2026-09-20). The second binding may hold a different type, which is what makes it worth having:

```gdscript
var content = file.read()        # String?
assert content
var content = content.trim()     # String
```

  **The order is: evaluate, then drop, then bind.** The new value is computed first -- so `var content =
  content.trim()` reads the binding it is about to replace -- then the previous value is released, running its
  `drop()` if that takes it to zero, and only then does the new binding take effect. Releasing first would make
  the common case a use-after-free.

  The unused rule (section 5) still applies to the binding being shadowed: shadowing a name that was never read
  is an error, which is what catches an accidental reuse rather than a deliberate one.

### Casting

There is no cast syntax. The right side is always cast toward the left side.

```gdscript
func whole_part(value: Float): Int {
    return value        # value is cast to Int
}
```

This applies to binary operations, assignment, arguments (toward the parameter type) and `return` (toward the return type).

**Wider arithmetic is written wider operand first** (D162, decided by Mortaro; the errors below proposed by
Claude, unconfirmed). **[implemented]** `Int * Long` is an `Int` multiply and `Long * Int` a `Long` one, so an
arithmetic operator (`+`, `-`, `*`, `/`, `%`) whose right operand is wider than its left is a compile error that
names the rule and the fix: `'count * total' is a multiplication in Int, since arithmetic takes the left side's
type, and the right side is a Long, which would be cut to fit: write the Long first ('total * count'), or store the
right side in an Int first if it fits one` (for `-`, `/` and `%`, where order matters, the fix is to store the left
side in the wider type first). Wider means more bits (`Tiny`/`Byte` 8, `Short`/`UnsignedShort` 16,
`Int`/`UnsignedInt`/`Float` 32, `Long`/`UnsignedLong`/`Double` 64), or a `Float`/`Double` right side under a whole
number left side, which would lose its fraction; signedness alone is not wider. An integer literal on the right
that fits the left type is not wider (`small + 1` with a `Byte` `small` is a `Byte` addition). A comparison is not
arithmetic and is not checked: it still casts the right side toward the left, so `age > 0.5` with an `Int` `age`
means `age > 0` (open question 3). A constant expression that overflows the `Int` its arithmetic is done in is an
error too, naming its value: `'(65536 - 120) * 65536' is 4287102976, which does not fit in an Int, the type its
arithmetic is done in, so it would wrap: write the number itself, 4287102976, which is a Long`.
`diagnostics/wider_right_operand`.

**Text casts to an enum by its name** (proposed by Claude, unconfirmed; built for D95's `Json`, 2026-09-24):
`var course: Recipe.Course = name` is the value spelled `name`, or the enum's first value when none is, exactly as
text that does not parse becomes `0` for an `Int`. Compare `"{course}" == name` to tell the two apart. **[implemented]**

### Numeric types  **[implemented, PROVISIONAL]**

All the basic types a language has, with written names (decided 2026-09-19:
"never `u64`"). **(proposed by Claude, unconfirmed: the exact width mapping below.)**

| Type | C type | Notes |
|---|---|---|
| `Tiny` | `int8_t` | |
| `Short` | `int16_t` | |
| `Int` | `int32_t` | the default integer type |
| `Long` | `int64_t` | |
| `Byte` | `uint8_t` | |
| `UnsignedShort` | `uint16_t` | |
| `UnsignedInt` | `uint32_t` | |
| `UnsignedLong` | `uint64_t` | |
| `Float` | `float` (32-bit) | the default decimal type |
| `Double` | `double` (64-bit) | |

An integer literal defaults to `Int`; one too large to fit becomes a `Long` instead. A decimal literal
defaults to `Float`. All of them follow the same right-side-casts-toward-left-side rule as everything else
(`var tiny: Tiny = some_int_variable` narrows with an ordinary cast); converting between two numeric types
wraps on overflow (an out-of-range value assigned into a narrower type keeps its low bits, the same as a plain
C cast) rather than crashing or saturating. `List<T>`, `Dictionary<T>`, `T?`, and generics all work
with every numeric type; so does `String` conversion both ways -- casting a `String` to any numeric type by
assignment parses it (defaulting to `0`/`0.0` on failure, like `Int`/`Float` always did), and `String` gains
one `to_<name>()` method per type (`to_tiny()`, `to_short()`, `to_int()`, `to_long()`, `to_byte()`,
`to_unsigned_short()`, `to_unsigned_int()`, `to_unsigned_long()`, `to_float()`, `to_double()`) alongside the
existing `to_int()`/`to_float()`. `count()`, `length()`, and `index_of()` always return `Int`, never a wider
type.

Printing a `Float`/`Double` uses shortest-round-trip formatting (try the fewest significant digits that parse
back to the exact same value) rather than a fixed number of digits, so switching `Float` from 64-bit to 32-bit
does not make an ordinary value like `0.1` print with ugly trailing noise (`0.100000001`) -- it still prints
`0.1`, exactly as before this table existed. A value whose shortest digits would need an exponent prints with
`%.17g` (`%.9g` for `Float`) instead, as `1e+21` or `1.0000000000000001e-05`; infinity prints `inf` and not a
number prints `nan` on every platform. **PROVISIONAL** because the exact widths were not explicitly
confirmed by Mortaro; revisit if a different mapping is wanted.

### Numbers are classes, and `this`  **[implemented]**

D83 (decided by Mortaro, 2026-09-24): every type in the table above, and `Bool`, is a class in `library/`
(`library/int.spite`, `library/double.spite`, ...), the way `String` is. A number is still a plain C value in
the emitted code -- the class gives it functions, not a header -- and a function of `Int` receives its `int32_t`
as the receiver. Inside it, **`this`** is that value: `func doubled(): Int { return this * 2 }`, and
`count.doubled()` calls it. A number class is reopened like any other (section 11), by a file named after it.

- **Writing a number as text is its `to_string()`** (D107, decided by Mortaro: converting to text is a cast like
  `to_int()`), in Spite: `Long.to_string()` writes the digits, `Double.to_string()` is the
  shortest-round-trip formatting above (over the digit arithmetic in `library/number_text.spite`), and the
  smaller types widen and call `Long.to_string()`. `Bool.to_string()` answers `"true"` or `"false"`.
  Interpolation (`"{count}"`) and `+` onto a `String` call it. Printing an integer with
  `console.print` is written straight to the stream by the compiler, with the same digits (proposed by Claude,
  unconfirmed: a hidden optimisation, D36).
- **Casting is a function of the class cast to** (D100, decided by Mortaro): each number class has
  `func from_type(type: Symbol, value: type.class)`, a Symbol codegen function whose symbol ranges over the
  program's types, so the right-to-left cast of an `Int` into a `Float` is `Float.from_int(value)`. Its body is
  the compiler's (a C cast, emitted inline, so a cast costs what it did), and `--final_classes` prints the
  declaration in every number class. `type` may name a parameter and begin a type path for this, although it is
  a keyword elsewhere (proposed by Claude, unconfirmed).
- **Bitwise operations are functions of the whole-number classes** (D117, decided by Mortaro; the names and the
  rules below proposed by Claude, unconfirmed). `Tiny`, `Short`, `Int`, `Long`, `Byte`, `UnsignedShort`,
  `UnsignedInt` and `UnsignedLong` each answer `shifted_left(count: Int)`, `shifted_right(count: Int)`,
  `bits_and(other)`, `bits_or(other)`, `bits_exclusive_or(other)` and `bits_inverted()`, all returning the
  receiver's type, and `set_bit_count()`, `leading_zero_count()` and `trailing_zero_count()`, returning an `Int`
  (the width for 0). There are no operator symbols for them. They are bodiless declarations the compiler
  supplies (D82), so `--final_classes` prints them in each class, and each body is the single C operation,
  inlined in an optimised build. The rules, so no undefined C behaviour reaches a program:
  - `shifted_right` is **arithmetic on a signed type** (the sign bit is copied in: an `Int` -20 shifted right by 2
    is -5) and **logical on an unsigned one** (zeros come in). `shifted_left` always brings zeros in, and a bit
    moved past the top is lost, so the result wraps like any narrowing (a `Tiny` 1 shifted left by 7 is -128).
  - **A count of the width or more shifts every bit out**: the answer is 0, or -1 for a negative signed value
    shifted right. **A negative count halts the program**, naming the function and the count
    (`spite: UnsignedShort.shifted_right was given the count -2, and a shift count is 0 or more`).
  - **The other operand is cast to the receiver's type**, the ordinary argument-to-parameter cast: a `Byte`'s
    `bits_and` of a `Long` keeps the `Long`'s low 8 bits and answers a `Byte`; a `Long`'s `bits_and` of a `Byte`
    widens the `Byte`. The count is an `Int`. This does not settle `mortaros_missing_decisions.md` item 88
    (which operand's type arithmetic takes).
  - Called on `Float`, `Double` or `Bool`, they are an error naming the whole numbers
    (`diagnostics/bitwise_on_float`). `conformance/stage6/bitwise_functions` and `negative_shift` pin them.
- **`this` works in every class** (proposed by Claude, unconfirmed): it is the instance a function answers on,
  for handing itself to something -- `registry.append(this)`. Reading your own member through it is an error,
  because a class already reads its members by name: `this.name` is reported as "write 'name', not
  'this.name'", and `this.to_string()` as "write 'to_string()'".
- A decimal literal is written to the C as a decimal (`1.0`, not `1`), so `1.0 / 3.0` divides as decimals
  (found on the way: it used to divide integers).

## 5. Functions  **[implemented]**

```gdscript
func function_name(first: Reference, second: Value): Tiny {
    return 1
}
```

- The return type is written `(): Type`. No other form is valid.
- `return` is always explicit. There is no implicit return of the last expression. A function with a
  return type whose body falls off the end without a `return` gets the defensive default return of its
  return type, with no diagnostic.
- A function with no return type returns nothing.
- **Parameters** (D1, decided by Mortaro 2026-09-19): a scalar (every numeric type, `Bool`, an enum value) is
  passed by value, copied. Everything else -- a class instance, `List<T>`, `Dictionary<T>`, `String`, a union, an
  object literal -- is passed by reference: the caller writes nothing special, and the callee shares the exact
  same object (mutating it through the parameter is visible to the caller). `&Type` no longer exists as syntax --
  writing it is a parse error saying references are the default now. A copy is always explicit: `copy()`/
  `deep_copy()` (section 10).
- `assert condition` is a production feature, not a debug one: when the condition is falsey the function returns
  the default value of its return type immediately.

```gdscript
func sum_positives(a: Float, b: Float): Float {
    assert a > 0 and b > 0
    return a + b
}
```

### Null safety and `assert` narrowing  **[implemented]**

`assert` doubles as the way to prove a `T?` is not null without nesting: after `assert value` on a
`T?` local, parameter, or field, `value` is a plain `T` for the rest of that block and any block
nested inside it -- reads, writes, and method calls all go straight to the value the `T?` holds, and ownership
and dropping still belong to that `T?` (assigning a new `T` through the narrowed name drops the old one first,
exactly like overwriting any other owning slot). `assert value and
other_condition` narrows `value` too, and `other_condition` itself already sees the narrowed type. Reassigning
the narrowed name to `null` afterward is a diagnostic (there is no way back to `T?` in the same
scope).

```gdscript
var content = program_file.read()
assert content
var length = content.length()          # content is a String here, not String?
console.print(length)
```

`if` on a `T?` narrows the same way, in place, for the whole block: `if value { } else { }` runs the
block with `value` already a plain `T`, and the `else` exactly when it is null/absent. One rule for narrowing
everywhere -- `assert`, `if`, and `switch` all read/write/call straight through to the value the `T?`
still owns. (Second batch item 2, decided 2026-09-19: `if value do name { ... }` is gone -- `do` is removed, so
this is the only form.)

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
func is_open(): Bool {
    if handle == -1 {        # error: this 'if' only returns the default 'false':
        return false         #        write 'assert handle != -1' and let the rest run unindented
    }
    return true
}
```

It is the same rule as narrowing, so `if not value { return null }` becomes `assert value` and narrows `value`
for the rest of the block. Returning anything else -- `return true` from a `Bool` function, `return -1` -- is an
answer, not a guard, and is left alone. How the condition is turned around (proposed by Claude, unconfirmed):
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
for the rest of that block and any block nested inside it. It is the same rule section 5 already has for
`assert value and other_condition`, applied along a member chain instead of across an `and`.  **[implemented]**
Across an `and`, every side narrows, for `assert`, `crash` and `if` alike, since each side is known true when the
whole is: `crash names[position] and ages[position]` proves both elements, as two `crash` lines would (proposed by
Claude, unconfirmed, 2026-09-24; `conformance/stage6/and_narrowing`).

- It narrows the path asserted **and its prefixes**, nothing else. A sibling path stays `T?`: asserting
  `class.namespace.namespace` says nothing about `other_class.namespace`.
- A local holding a copy is its own path: `var ns = Hello.namespace; assert ns` narrows `ns`, and a later read
  of `Hello.namespace` is a fresh `Spite.Namespace?` that must be narrowed in its own right --
  `diagnostics/namespace_nullable` is exactly that shape. Since D63 the copy itself is an error: narrow the
  path, not a local holding it.  **[implemented]**
- A check on something that cannot be null proves nothing and is an error naming the fix: `assert tracker`
  written twice, or `crash` on a path an earlier `crash` already narrowed (`diagnostics/check_proves_nothing`).
  **[implemented; proposed by Claude, unconfirmed]** For a `[]` read the message names what proved it, so the
  line can simply go: `crash codes[index]` inside `while index < codes.count()` is "'codes[index]' is already
  proven by the loop condition 'index < codes.count()', so this 'crash' proves nothing: remove it"; a count or
  bound check is named the same way, and anything else as "an earlier check" (proposed by Claude, unconfirmed,
  2026-09-24; the D64 rules below already reported it, without naming the proof; `diagnostics/proven_element`).
- `crash` (below) narrows a chain the same way, since it narrows exactly as `assert` does but never returns.
- Reading through a link that may be null and has not been narrowed is still an error. This removes the verbosity of
  proving a path, not the requirement to prove it.
- Assigning to a narrowed path, or to anything it reads through, undoes the narrowing from that point on: after
  `outer = Box()` or `outer.inner = null`, `outer.inner` is a `Box?` again. Assigning a value that cannot be null
  (a constructor, a literal, a name that is not `T?`) to the narrowed path itself keeps it narrowed, and still
  undoes everything narrowed beneath it. A narrowed *name* follows the same rule: `current = current.next` after
  `assert current` stores into the `Node?` it really is and un-narrows it (until 2026-09-23 it was rejected, and
  `maybe = null` silently stored a default object instead).  **[implemented; proposed by Claude, unconfirmed]**
- **`while value` narrows its body like `if value`** (proposed by Claude, unconfirmed; implemented 2026-09-23): the
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
  proven is an error that says so (proposed by Claude, unconfirmed, 2026-09-24; it read "the condition of
  'crash' is a String" before; `diagnostics/proven_index_check`).
- **Assigning the list or the index undoes it**, as for any path (section 5, D43): `index = index + 1`
  un-proves `names[index]`, and so do `names.clear()`, `remove_at`, `remove_first` and `remove_last`. Inside a
  loop, undoing a proof made before the loop that the loop has read is an error.
- **Any index without a call is a path** (proposed by Claude, unconfirmed, 2026-09-24): `crash glyphs[code - 32]`
  proves `glyphs[code - 32]` for what follows, not only `glyphs[index]`. The index may be names, member reads,
  numbers, `true`/`false`, enum values, operators and other `[]` reads; two indices are the same when D78's
  printer prints them the same, so spacing and redundant parentheses do not matter. Assigning any name the index
  reads undoes it, as assigning the index itself does. A call anywhere in the index keeps the read a plain `T?`,
  since the call could answer something else the second time. A call *between* the check and the read does not
  undo it, exactly as for `glyphs[index]` (the read still checks its bounds). `conformance/stage6/structural_index`,
  `diagnostics/structural_index_undone`.
- **A proven read still checks its bounds at runtime** and answers the default out of range, so a counter that
  went negative gives a wrong value rather than reading memory it does not own.
- **A `Bool?` cannot be a condition** -- not in `if`, `while`, `assert` or `crash`, and not under `not`, `and`
  or `or`: `if flags[index]` would test that the element is there, not that it is true, and the two mean
  opposite things for `false`. Prove the element is there first, or `switch` over it; `== true` also works,
  since comparing needs no narrowing.
- Writing through `[]` is unchanged, and **assigning through a `T?` is an error** naming the fix
  (`diagnostics/store_through_nullable`): until D64 `outer.inner.label = "x"` on a `Box?` was silently
  dropped, emitting nothing at all.

`first()` and `last()` still answer the default on an empty list; whether they should answer `T?` too is
open (Claude would say yes, by the same argument). `tests/list_tests`, `diagnostics/index_reads`.
**[implemented]**

**Comparing needs no narrowing** (D69, decided by Mortaro, 2026-09-23). `==` and `!=` accept a `T?` on the left:
null is not equal to anything, so `crash Spite.Class.namespace == "Spite"` is a whole test. Either side may be the `T?`. Only the comparison
is exempt -- reading a member through a `T?` still needs it narrowed first. A `Spite.Namespace` compares with
text through `equals(String)` in `library/spite/namespace.spite`, against its `name_with_namespaces`; two
namespaces still compare by identity, because a class's `equals` is used for a right side of that same class
only when `equals` takes that class.  **[implemented]**


### Unused is an error  **[implemented]**

Second batch item 5 (decided 2026-09-19): a local variable or parameter that is never read is a compile error.
**Only a read counts** (D136, decided by Mortaro, 2026-09-25): a local or parameter that is only ever assigned
is unused, `'total' is never read: remove it`; a read in the new value, `label = "{label} of things"`, is a read
(`diagnostics/written_not_read`). **Only a parameter may be `_name`** (D137, decided by Mortaro, 2026-09-25): a
signature can need a parameter its body ignores, and `_name` says so; a local has no such reason, so an unread
local is an error whatever its name, and the message only says to remove it. A `_name` that *is* read is also
an error, naming the fix (remove the `_` or the read). A declaration whose own statement already failed is not
reported as unread as well **(proposed by Claude, unconfirmed)**. Parameters whose signature is dictated from
outside are **exempt** from the read requirement, because the author never chose them:

- operator functions (`sum`, `subtract`, `equals`, `get_at`, ... -- the Operators table above): `a + b` calls
  `sum(b)`, so the parameter is the operator's, not the author's
- Symbol codegen templates (`set_attribute`, `get_attribute`, ...): the whole signature is the template
  mechanism's -- one instantiation per attribute, never emitted as the author wrote it
- setters that answer attribute access: a one-parameter `set_age` naming a real `age` attribute answers
  `person.age = 1` (above), so its parameter is the write's, not the author's
- functions that replace another through class reopening (section 11): the signature is dictated by the
  earlier root's call sites

A `set_age` with two parameters never answers attribute access (the dispatch passes exactly one value), so its
parameters are the author's free choice and the ordinary rule applies.

**An attribute nothing reads is an error too** (D118, decided by Mortaro, 2026-09-24): "world is never used,
unused variable declarations should cause a compiler error." The message names the attribute and the fix,
`the attribute 'world' is never read: remove it` -- since D137 no spelling keeps an unread attribute
(`diagnostics/unused_attributes`, `conformance/stage6/attribute_uses`, `docs/style.md`). **A compile-time walk
counts only when it reads the attribute's value** (decided by Mortaro, 2026-09-25, relayed by the coordinating
session): `x.attributes[attribute]` read in a Symbol template, and so `Json` and `to_debug()`, and the REPL's
display. `attribute.name` and `attribute.class` are the attribute's description, not its value, so SlopEngine's
scheduling markers -- `var world = Resource.World()` that only a classify walk inspects -- are errors.

What counts as a read, and the exceptions **(proposed by Claude, unconfirmed)**:

- **A read takes the value**: the attribute's name in its own class's functions or attribute defaults,
  `thing.world` from another class (including through a union's members or a `type` a class is admitted to), a
  getter answering that read, `x.attributes[attribute]` or `attributes[attribute]` in a template, `thing.attributes`
  (the values), a `load(...)` argument the compiler reads, and the REPL's `attributes` in a `--repl`/`--repl_port`
  build. The class-level `Class.attributes` (names and types) does not count.
- **Writing is not reading.** An attribute that is only assigned, from inside or outside, is unused -- the same
  rule as locals since D136.
- **Source-level, not reachability.** A function nothing calls still reads what it names; tree shaking removes it
  later. Where the compiler does not compile a body -- a branch a codegen or `Build` test folds away, a function of
  a generic class no instance emits, a Symbol template no range reaches -- its reads still count: bare names as
  the class's own attributes, and `x.name` by name for any class (so a fold can hide an unused attribute of the
  same name elsewhere, never report a used one).
- **`_name` is only private** (D137): a private attribute nothing reads is an error like a public one, judged by
  its own class, which is its only reader. An attribute kept only for its constructor's side effect (a binding
  such as `var server = Server()` whose construction starts something) is an error too, whatever its name.
- **Never checked**: a number class's or `String`'s storage (`_memory`, `_bytes`, ... -- the compiler reads
  them), and every attribute of `Build` and `Environment`, which the compiler and the program's
  flags read.
- **A library is judged on its own code.** Every non-generic library class is compiled whole in every program,
  and a generic one's uncompiled functions count as above, so the library's own reads are the same in every
  program: an attribute the library itself reads is never an error for a program that ignores it, and `check.sh`
  holds the library to that (every library attribute is read by library code). A program's read of a library
  attribute counts too, which can only hide an error, never cause one.
- **A loaded package's public attributes are not checked** (D136 says a public attribute is reported only where
  every reader is visible; which folders count is **proposed by Claude, unconfirmed**). A public attribute is
  data offered to code its package cannot see: SlopEngine's `ui` package declares `BackgroundColor.color` for its
  `render` package to read, and a program that loads `ui` without `render` must still compile. So the check
  covers the program's own folder (its entry folder, minus the folders it `load`s), the standard library (as
  above), and private attributes everywhere, whose only reader is their own class. An attribute a program adds
  by reopening a loaded class is the program's and is checked
  (`conformance/stage6/package_attributes`, `diagnostics/package_attributes`). A loaded package's unused public
  data is tree-shaken rather than reported (D157).
- **An unused singleton binding is an error everywhere** (D157, decided by Mortaro), a loaded package included:
  `var world = World()` that nothing reads is `the attribute 'world' is never read: remove it` even in a folder
  the program `load`s, since a class that needs a singleton binds it itself. (Proposed by Claude, unconfirmed: a
  read of the binding from another class still counts, as it does for any attribute.)
- The check runs only when the program has no other error, so a failed statement does not report the attributes
  it would have read.

### Functions are values, always bound to an instance  **[implemented, except `.owner`]**

D17 (decided by Mortaro, 2026-09-19): a function is a first-class value. Naming one inside a class passes it
together with the instance doing the passing, so it runs exactly as that instance would have run it:

```
console.log(pretty_print)        # passes this instance's pretty_print
```

- **There are no free functions and no closures.** A function value is `{instance, function}` -- one retain in a
  reference-counted language (section 10), capturing the receiver and nothing else, so no local ever escapes its
  scope. That is the whole of the feature; there is no environment to capture, no lifetime to reason about, and
  no allocation beyond the retain.
- **It makes foreign callbacks expressible.** `{instance, function}` is precisely what a C callback plus its
  `void*` user data wants, which is what section 17 lists as not designed yet.
- **An event handler can be the function itself** -- `onclick: increment` -- checked by signature, rather than
  the symbol `'increment'` (section 7) resolved by name. Symbol literals stay useful elsewhere.

**A function value is an instance of `Spite.Function`** (D39, decided by Mortaro, 2026-09-20), generic over its
arguments and its return, so the type of a function is written the same way any other generic type is:

```gdscript
func log(printer: Spite.Function<String, Bool>)
```

The codegen values are positional and the **last one is the return**; everything before it is an argument, in
order. `Spite.Function<Bool>` takes nothing and returns a `Bool`; `Spite.Function<String, Int, Bool>` takes a
`String` and an `Int` and returns a `Bool`. They line up with `Spite.Function`'s own reflection members
(section 8, D12): the arguments are `.arguments`, the last is `.returns`. So there is nothing new to learn --
the type of a function is its reflection, written down.

That is the point of the choice: **the value and its reflection are the same class.** `Weapon.functions[0]` and
a `pretty_print` passed as an argument are both `Spite.Function` instances, so reflection stops being a mirror
of the language held alongside it and becomes the language describing itself -- which is what "everything is a
class, including classes" (section 8) has always claimed. A function value carries `.name`, `.arguments` and
`.returns` because it *is* the reflection object.

This is the language's first **variadic** generic. `List<T>` and the rest take a fixed count declared by their
`generic` lines (section 9); `Spite.Function` is a compiler built-in and takes as many as the signature has.

#### Calling one

D40 (decided by Mortaro, 2026-09-20): **a `Spite.Function` knows its owner, and every call goes through
`call_function()`.**

- `.owner` is the instance the function is bound to. D17's `{instance, function}` pair stops being a hidden
  representation and becomes an ordinary field you can read: `pretty_print.owner` is the `Console` that will
  run it. Bound-ness is reflectable, like everything else about a function.
- **`call_function()` is the one path.** Writing `printer(value)` is `printer.call_function(value)`, which is
  section 5's operator rule applied once more -- the call operator maps to a named function exactly as `+` maps
  to `sum` and `a[x]` maps to `get_at`. So there is one chokepoint every invocation of a function value passes
  through: an event handler firing, a foreign callback arriving from C (section 17), a framework dispatching.
  One place to instrument, and one place where the owner is applied.
- Because `Spite.Function` is an ordinary standard library class, `call_function` can be reopened (section 11,
  D7) to trace or count every callback in a program. That is the foot, and it is yours to shoot.

The indirection is paid only where it was already accepted. An ordinary call -- `person.set_age(2)` -- is a
direct call and never builds a `Spite.Function`; only a function used *as a value* goes through
`call_function()`.

**`Nothing` is the class a function returns when it returns nothing** (D48, decided by Mortaro, 2026-09-20).
It is required rather than optional: with the return in the last position, `Spite.Function<String>` already
reads as "takes nothing, returns a `String`", so a function taking a `String` and returning nothing could not
otherwise be written. `Spite.Function<Nothing>` takes nothing and returns nothing.

**As implemented** (2026-09-23): naming a function without calling it -- `greet` inside the class, `person.greet`
on a value -- builds a `Spite.Function` bound to that instance, whose type carries the signature; `change(text)`
and `change.call_function(text)` call it with their arguments checked; a typed value can go where a plain
`Spite.Function` (a reflected one) is expected, but not the other way round, since a reflected function's
signature is not known to the type checker (`diagnostics/function_value_mistakes`). In C it is still one
`Spite_Function` with a typed call pointer beside the owner, so a function value *is* its reflection object, as
D39 says (`conformance/stage6/typed_functions`). **Not built:** `.owner` as a readable attribute -- nothing in
`Spite.Function<...>` says what class the owner is, so there is no type to give it; calling a *reflected*
function with arguments; and a class attribute initialised with a function value.

`Nothing` is also what `.returns` reports for every function declared without a return type, so it is not a
notation invented for the type syntax -- it fills a hole the reflection already had. A function whose body falls
off the end still returns nothing in the ordinary sense (section 5); `Nothing` is how that is *named* when a
signature or a reflection object has to say it.

### Variadic arguments  **[implemented]**

D90 (decided by Mortaro, 2026-09-24, answering open question 14): "variadic arguments are going to be mapped to a
generic list so something like: `...args: List<Type or Class>`." The last parameter may be written with `...`, and
the caller then writes its values one by one; the function receives them as an ordinary `List`:

```gdscript
func log(...lines: List<String>) {
    var joined = lines.join(" ")
    console.print(joined)
}

log("a", "b", "c")        # lines is ["a", "b", "c"]
log()                     # lines is empty
```

- **The element type may be a class or a `type`** (section 7). With a `type`, each argument is whatever class
  fits the shape, and a call on an element is dispatched to the class it really is, exactly as a `List<Shape>`
  already dispatches: `func announce(...things: List<Describable>)` takes `announce(Widget("gear"), Gadget(3))`.
- **Only the last parameter**, and it must be written `List<Type>`: anything else is a parse error naming the
  form (`diagnostics/variadic_not_last`). Parameters before it are ordinary and positional.
- Each value is an argument in every sense: it casts toward the element type like any argument (section 4), and
  D77 still applies to it, so only a constructor may be one, one level deep (`diagnostics/variadic_mistakes`).
- A function value keeps the spread: naming `log` and calling the value with `("x", "y")` gathers them the same
  way (`conformance/stage6/variadic_arguments`).

As implemented (proposed by Claude, unconfirmed): **passing a whole list to a `...` parameter is an error** that
says to pass the elements, because the caller writes them one by one and there is no spread operator to ask for
the other reading -- unless the element type is itself a list. Zero values is a legal call, giving an empty list.
`Console.print`, `write` and `error` are ordinary variadic functions taking `...values: List<Printable>` (D109,
section 15).

**A number, a `Bool` or an enum value fits a `type` too** (proposed by Claude, unconfirmed; built for D109). A
shape holds class instances, and these are plain values, so passing one where a `type` is wanted puts it in a
small box the compiler allocates and frees like any object, and a call through the shape reaches its class's
function (`Int.to_string()`, for an `Int`). A `String` needs no box; a `Symbol` gets one so that it keeps its
class. An enum value answers two functions, `to_string()` (its name as text -- `weather.to_string()` works on
any enum value) and `to_debug()`, so that is all a shape can ask of one. `.class` read through the shape names the
value's own class (`Int`, `Symbol`, the enum), as it does for a class instance. A value of a union passed where a
`type` is wanted brings the union's members into the shape.

### Failure: three outcomes and no others  **[partial]**

D24 (decided by Mortaro, 2026-09-19): Spite has **a compile error, an `assert`, or a crash**. There are no
exceptions, no error unions, no bubbling, and no error value carrying a message, because "errors and exceptions
tend to be useless: they tell us a message we have no action to take about them".

1. **A compile error**, for anything the compiler can know. This is already where the language keeps landing:
   an unserialisable type (section 17), the wrong `$target` (D20), an unfilled codegen hole (section 9), a
   missing foreign symbol (section 17), a member that does not fit a template (section 8), a symbol outside its
   enum (section 7).
2. **`assert`**, for when the program should keep running.
3. **`crash`**, for when everything should stop so the code gets rewritten.

`T?` is the only runtime failure value, and it carries no reason. A caller narrows it with `assert`,
halts on it with `crash`, or handles the absent case with `if ... do`.

**If a distinction is actionable, it is data, not an error** (proposed by Claude, unconfirmed). A caller that
must tell a timeout from a rejection takes back a `type`, `union` or enum modelling exactly that -- ordinary
data with ordinary handling. Something you can act on was never an error; it was a value that was not modelled.

#### `assert` is control flow

D26 (decided by Mortaro, 2026-09-19): `assert` is a guard clause, not validation -- "this value is absent, so
there is no more logic to do here, but the program is fine and keeps serving everything else". It is why a web
server or a game written in Spite should rarely crash.

D27 limited `assert` to a function that returns nothing or `T?`, the two cases where the substituted default is
honest. **D106 (decided by Mortaro, 2026-09-24) lifts that limit: `assert` is legal in a function returning any
type, and a failed one returns that type's default** -- `false`, `0`, `""`, `null`, an empty value, or nothing.
The reason is the code the limit produced: `if handle == -1 { return false }` returns the very same default, only
spelled as an indented `if`, so the limit never removed the ambiguity, it only hid the guard. D106 makes that
`if` an error naming its `assert` (section 5, "An `if` that only returns the default is an `assert`").

```gdscript
func is_open(): Bool {
    assert handle != -1              # a failed assert returns false
    return true
}
```

When the caller must tell absence from a real `0` or `false`, the choice is still deliberate: return `Int?`, or,
if absence is a bug rather than a case, `crash`.

D28: **`assert` is banned in a constructor**, including the entry class's own (D29). A constructor is setup, not
logic: if something can be invalid inside one, function logic has been put where only setup belongs. The caller
receives an object with every field at its default and cannot tell it from a properly built one, which is
exactly the surprise the rule exists to prevent. A program constructor that grows complex splits into smaller
functions it calls -- and reads as a table of contents for the program, with the logic one level down.

#### `crash`

D30 (decided by Mortaro): `crash` is a keyword, so the compiler takes its context from the program rather than
from a message string. **It mirrors `assert` exactly** -- same polarity, same shape, different severity:

```
assert database.connect()        # falsey: return the default, keep going
crash  database.connect()        # falsey: halt
```

The compiler captures the condition's source text and every operand value, so nothing has to be written and
nothing can drift out of sync. Bare `crash` is legal for an unreachable branch and reports its enclosing
context. `crash user` narrows a `T?` exactly as `assert user` does, but never returns -- which filled the gap
D27 opened, since a function returning `Int` could then guard without widening its signature (D106 has since
let `assert` guard there too, returning the default).

Absence is fine -> `assert`. Absence is a bug -> `crash`. Absence is meaningful -> `if ... do`.

#### What a crash reports

D25: a crash always reports backend information **and the trace of every `assert` that failed before it**. Those
asserts are the causal trail explaining how the program reached the state that crashed, which is usually several
frames earlier than the crash itself. The cost is only on the failure path -- a passing `assert` already
branches -- and the trace is a fixed-size ring buffer with a total count, so it never allocates and cannot grow
without bound. Compile-time evaluation (section 8's function of `Spite.Class`s, section 15's generated JSON) reports
the same way.

D26 refinement (proposed by Claude, unconfirmed): the trace records **predicate asserts only**. A narrowing
assert firing is routine control flow -- thousands an hour on a server -- and on concurrent work the last few
would come from unrelated requests, reading as a causal chain that does not exist. A per-site count covers them
instead, which is one increment and never consumes the buffer.

D189: **an `assert` in `library/` never enters the trace.** The trace explains how *the program* reached a crash,
and the standard library's asserts are routine answers (a missing key, text that does not match, a read past the
end) that fire constantly and would push the program's own entries out of the ring. The compiler leaves the ring
write out of every `assert` site whose file is in `library/`, so a library guard costs what an `if` costs; the
program's own files and every `load`ed package keep recording, and a crash inside the library still reports its
own site (`conformance/stage5/library_guards_untraced`).

D32: **a crash site is identified by a compile-time id, and the compiler emits an id-to-source map as a build
artifact** rather than embedding the information in the program. The binary carries none of it, which matters
most in a wasm module where size is startup time, and **the same logical site keeps the same id across every
`$target`**, so a crash from the server bundle and one from the browser bundle compare directly. The id is
content-derived -- a hash of namespace, class, function, the site's ordinal and the condition source -- never a
counter, because a counter renumbers everything below an inserted line and destroys both old reports and
cross-target identity. `--development` embeds the text directly, so a local run needs no lookup.

D33: the map is a greppable tab-separated table at `<output-name>.crashes`, one line per site, sorted by id so
archived maps diff cleanly: id, file, line, column, class, function, kind (`crash`, `assert-predicate`,
`assert-narrowing`), the condition source, and the operand names with their types. It opens with a `#` comment
line carrying the format version and the build hash. At runtime a crash emits one tab-separated line prefixed
`spite.crash`, and an assert `spite.assert`. **Values are rendered as text at crash time but stored raw in the
ring buffer**: a crash happens once and then the program is dead, so it should be informative rather than fast,
while a narrowing assert may fire thousands of times an hour and must stay cheap until something dumps it.

**Implemented.** `crash` is built -- the keyword, narrowing, bare `crash` for an unreachable branch, the compiler
enforcing D28/D29 and D106 -- and so is the reporting above: crash and assert sites carry content-derived ids, every
build writes `<output-name>.crashes`, and a crash prints the asserts that failed before it from a ring of 32
(`conformance/stage5/crash_report`, [failure.md](docs/failure.md)). A firing `crash` flushes stdout, writes its
line to stderr, and exits with status 1:

```
spite.crash<TAB>path:line<TAB>Class<TAB>function<TAB>condition
```

The condition is rebuilt from its own tokens and the line ends with the named operands of the failed
comparison and their values -- `value > limit<TAB>value=-9<TAB>limit=0` -- with calls never evaluated a second
time.

### Operators  **[implemented]**

Every operator is a shortcut for a function, which a class can define to support that operator (decided
2026-09-19: "every operator is a shortcut for a function the user can replace").

| Operator | Function | Notes |
|---|---|---|
| `a + b` | `sum(b)` | |
| `a - b` | `subtract(b)` | |
| `a * b` | `multiply(b)` | |
| `a / b` | `divide(b)` | |
| `a % b` | `remainder(b)` | |
| `a == b` | `equals(b)` | |
| `a != b` | negated `equals(b)` | |
| `a < b` | `less_than(b)` | |
| `a > b` | `greater_than(b)` | |
| `a <= b` | `less_than(b) or equals(b)` | derived, not its own function |
| `a >= b` | `greater_than(b) or equals(b)` | derived, not its own function |
| unary `-a` | `negate()` | |
| `not a` | -- | stays a built-in keyword, never a function |
| `a and b` / `a or b` | -- | stay built-in, short-circuiting |
| `a[x]` | `get_at(x)` | |
| `a[x] = v` | `set_at(x, v)` | |

For `Int`/`Float`/`Bool`/enum/`String`/`List<T>`/`Dictionary<T>` these are intrinsic (they compile to exactly
the same C as before this table existed); `String`/`List<T>`/`Dictionary<T>` additionally accept the explicit
call form alongside the operator (`list.get_at(0)` next to `list[0]`, `"a".sum("b")` next to `"a" + "b"`).

For a user class or union, `a + b` compiles to `a.sum(b)` when the class defines (an exact function, or Symbol
codegen answers) `sum`; a union duck-types the same way a method call already does (every member must define
the function with the same signature). Missing the function is the existing "unsupported"-style diagnostic,
now naming the function to define instead. The right side of a binary operator still casts toward the
parameter type, exactly like an ordinary function call argument.

Attribute access from *outside* a class also goes through operator functions: `person.age = 1` calls
`person.set_age(1)` when the class defines or Symbol-codegens `set_age` (an exact function always wins over
Symbol codegen), otherwise it assigns the field directly. `person.age` (a read) calls `person.get_age()` the
same way when defined -- **(proposed by Claude, unconfirmed: the manual only decided the setter half; this
read half mirrors it)**. D1 removes the earlier restriction that skipped interception for a getter returning
an owning type: every function/method return is now a properly retained, independent reference (section 10),
so a getter that computes and hands back a fresh value is exactly as safe to intercept through as one that
just returns the field itself (this also fixes the `get_<attribute>()` double-free that used to be listed in
`docs/KNOWN_ISSUES.md`). A getter written and called explicitly (`person.get_full_name()`) was always just an
ordinary method call. Compound assignment through interception works both ways: `person.age = person.age + 1`
reads through `get_age()` and writes through `set_age(...)`. Inside a class's own functions, a bare `age = 1`
(no receiver) and `attributes[attribute]` stay raw -- interception only applies to `receiver.field` written
from outside.

## 6. Control flow  **[implemented]**

```
if condition { } else { }
if nullable_value { } else { }     # runs with nullable_value unwrapped in place when it is not null; else when it is
while condition { }
switch enemy {
    Player: enemy.hurt()
    Monster: { enemy.die() }
}
```

An `if` on a `T?` narrows the value in place (section 5): the block runs with `value` as a plain `T`,
and the `else` runs exactly when it is null/absent -- one rule for narrowing everywhere, `assert`/`if`/`switch`
alike (second batch item 2, decided 2026-09-19: `if value do name { }` is gone, and `do` is no longer a
keyword, section 2). The terminal-`if` lint (section 5) applies only when the `if` has no `else` -- one with
an `else` already handles the missing case explicitly, so there is nothing left to rewrite with `assert`. D3
(decided by Mortaro, 2026-09-19) was the decision that first gave the form its `else`; the 2026-09-20 removal
of `do` simply renamed the unwrapped value's block to the in-place narrowing above.

There is deliberately no `break`/`continue` (D2, decided by Mortaro, 2026-09-19): `while` is the only loop
construct Spite has, full stop. An early exit re-checks a boolean flag in the loop's own condition instead
(`var stopped = false` ... `while not stopped { ... }`).
writeup of what this costs in practice (found while writing the bootstrap compiler in Spite) and why the
language owner chose to keep it this way regardless.

`while` is the only loop. There is no `for`: the language owner's call (2026-09-19) is "only the
while loop, no for; that makes people favor the metaprogramming" -- reach for `List<T>`/`Dictionary<T>`
metaprogramming (section 15) first, and index with `while index < list.count() { }` when a loop is
genuinely needed. Writing `for` is a parse error naming `while` and the metaprogramming helpers instead
of silently doing something else. Loops and ifs are otherwise discouraged in application code; reach for
standard library metaprogramming first.

## 7. Types

### Enums  **[implemented]**

An `enum`, `type` or `union` declaration takes no `=` and always breaks lines, one entry per line, no commas
(D47). `=` means assignment and nothing else. Writing `enum Job = {` is a parse error naming the fix.

```player.spite
enum Job {
    'knight'
    'magician'
    'archer'
}

func set_job(player_job: Job) { }

func other_function() {
    set_job('knight')
}
```

- An enum declared in `player.spite` is `Player.Job`. It is rarely written that way, because `'knight'` alone is enough wherever the expected type is known.
- From inside `Player` itself, plain `Job` also works; from elsewhere, both `Player.Job` and -- when unambiguous across the whole program -- plain `Job` work (searched: the referencing class's own namespace first, then its containing folder, then each parent folder, then the whole program by simple name).
- An enum value is resolved from where it is used (parameter, annotation, assignment target, comparison). Two enums may share a value name; with no expected type in hand, the value resolves when exactly one enum in the program has it, otherwise it is a compile error listing every enum that does.

**An enum is a closed list of symbols** (D10, decided by Mortaro, 2026-09-19). `'knight'` is a symbol -- a name
known at compile time -- and an `enum Job` declaration lists which symbols are accepted where a `Job` is expected.
That is all an enum is; the integer it compiles to is a representation detail.

- **A symbol literal is legal only where something says what it may be.** Where an enum is expected it must be
  one of that enum's symbols: `set_weapon('orange')`, when the parameter is an enum without `'orange'`, is a
  compile error listing the symbols that enum accepts. There is no widening from a symbol to an enum, and there
  is no untyped symbol literal -- `var choice = 'orange'`, with nothing to check it against, is an error naming
  the missing context.
- **A `Symbol` is text from a precompiled, tree-shaken table** (D70, decided by Mortaro, 2026-09-23, superseding
  this clause's earlier "compile-time only"). Every symbol the program uses is an entry in one table the compiler
  writes, so a `Symbol` value costs no allocation -- reading, storing, passing or comparing one never makes new
  text -- and a symbol the program never uses does not exist in it. So all of Spite's own metaprogramming can use
  symbols freely: a used symbol was needed anyway, and an unused one disappears. A `Symbol` reads as text
  wherever text is expected (every `String` function answers on it) while `symbol.class` is `Symbol`, and text
  becomes a `Symbol` only through `Symbol(text)`, which answers `Symbol?` -- `null` unless that symbol is already
  in the table (D68). An enum is still the closed form: the list of symbols a place accepts.
  Most symbols are short, so the representation can later become a small inline string rather than a pointer
  into the table ("TinyString"); that is an optimisation the language does not observe.
- **What a `Symbol` names is still checked by whatever consumes it.** `person.set_attribute('age', 2)` is
  verified against `Person`'s real attributes at compile time, exactly as the Symbol codegen path already is
  ([Symbol codegen](#symbol-codegen-implemented)) -- a symbol literal only makes that call writable in source,
  which it is not today.

### Unions  **[implemented]**

Tagged unions. A `switch` must cover every member and narrows the value inside each case.
When every member has the same function or attribute (same signature), it can be used directly on the union.

```gdscript
union Enemy {
    Player
    Monster
}

func attack(enemy: Enemy): Bool {
    switch enemy {
        Player: enemy.hurt()
        Monster: enemy.die()
    }
    return enemy.is_alive()
}
```

**`_:` answers for every member without a case of its own** (D60, decided by Mortaro, 2026-09-23). It is the
last case, and it means exactly "this body, written once for each remaining member": the value is narrowed to
each of those members in turn, so `_: creature.sound()` needs `sound()` only on the members `_` answers for,
not on the whole union. A switch is still exhaustive -- `_` is how it covers the rest, not a way to skip it.
**A repeated case body is an error** whenever `_` could absorb it: two cases doing the same thing when there is
no `_` yet, or a case doing what `_` already does. `_` after every member already has a case, or anywhere but
last, is an error too (`diagnostics/switch_rest_case`, `conformance/stage6/rest_case`). A switch over a `T?`
keeps its two cases, `<Type>:` and `Null:`.  **[implemented]**

**`value == Circle` asks what class a value is** (D75, decided by Mortaro, 2026-09-23). A class name on the right
of `==` or `!=` is a class test, true when the value is an instance of that class: `expression ==
Syntax.Expressions.TrueLiteral`. A `T?` that is null is no class, so the test is false. Naming a class that is not
a member of the value's union is an error, since the answer could only be false (`conformance/stage6/class_test`).
`if value == Circle { }` narrows `value` to a `Circle` inside the block, the way a switch case does.
A generic class is named with its codegen values and no parentheses, `found == Storage<$component_type>`
(proposed by Claude, unconfirmed, 2026-09-24): only the right side of `==`/`!=` reads that form, and anywhere
else it is an error saying to call it. Tested through a `type`, a class that fits is admitted to the shape
by the test itself, so a value read back from a `Dictionary<AnyStorage>` can be narrowed before this function
has stored one (`conformance/stage6/generic_class_test`).
**A codegen value bound to a class is a class test too** (D123's request; the readings proposed by Claude,
unconfirmed, 2026-09-25): inside `Fetch<$wanted_type>`, `if item == $wanted_type { found = item }` narrows `item`
to the bound class, as `if item == Health` would. A binding that is a number, `Bool` or enum tests for its boxed
class, since that is what such a value is inside a `type` (D109), and the narrowed name is the plain value again;
`String`, a `List` or a `Dictionary` test for their own classes. Where the value's static type already answers,
the test is decided while compiling instead: a `Health` against `$wanted_type` bound to `Health` is `true`, bound
to `Label` is `false`, and a union that does not hold the bound class is `false` rather than the "never true"
error, since another binding of the same class may make it true. A `T?` holding the bound class still tests for
null. A binding that is a `type` or a union is no class, so it is still "'$wanted_type' is a type here"
(`conformance/stage6/codegen_class_test`).
**So a switch that is one early return is an error** (`diagnostics/switch_single_case`): one class case and
`_:`, each a single `return`, is `if value == Class { return ... }` followed by what `_:` returns -- and when both
return `Bool` literals it is `return value == Class` (or `!=`). In Mortaro's words, code that can be written
simpler with no cost to reading is made to be, but a one-liner nobody can read back is not the goal. Anything
that would need an `else` stays a switch, and so does a switch with more cases: `_:` narrows to each remaining
member (D60), which an `else` cannot.

Memory safety is done with unions instead of borrow checking noise: `T?` is the union of `T` and `Null`, and it is narrowed before use with `if ... else`, `assert`, `crash` or `switch` (section 5).
**`Monster?` is a union, and `Nullable<T>` is gone** (D45, decided by Mortaro, 2026-09-20). A `?` suffix is
sugar for the union of a type and nothing:

```gdscript
var target: Monster? = null

func find_target(): Monster? {
    return null
}
```

`Monster?` *is* `union { Monster, Null }` -- `Null` is an ordinary class whose only value is the literal `null`,
the way `true` and `false` are the values of `Bool`. That is the point of the change: nullability stops being a
special case in the compiler and becomes a union like any other. `switch target { Monster: ... Null: ... }`
works because it is a switch over a union; `assert`, `crash` and `if` narrow it because union narrowing
already exists; and the pile of `Nullable<T>` exceptions collapses into rules the language already had.

Section 10's optimisation survives as a codegen detail rather than a language rule: a union of a reference and
`Null` is still just the pointer, and only a scalar needs a wrapper.

**Considered and rejected on the way** (2026-09-20): `Nullable<T>` (ugly, and a generic wrapper is what keeps it
a special case); `Monster or Null` (`or` short-circuits everywhere else, so a return type reads as an
expression yielding the truthy left side); `maybe Monster` (a keyword bought for a shortcut); `Maybe<Monster>`
as a standard library generic union (generics over unions hide the members, and collide with `$` codegen
replacement when a parent already declares the same name); and `Monster.Maybe` as a class-level member.


### Inline types and duck typing  **[implemented]**

```gdscript
type System {
    query: Query
    with: Dictionary<Class>
}
```

Like `enum` and `union`, a `type` declaration takes no `=` and always breaks lines, one entry per line, with no
commas (D47). The single-line form is what would have needed commas, so removing it removes the choice -- and a
`type` is read far more often than it is written, which is the case where the extra lines pay.

A `type` is a class matched by shape: any value with the same attributes and types is accepted, including object literals.
`.class` of the value still points to its original class. This is what makes fast JSON-like code possible.
`.class` read through a `type`-shaped or union-typed value is answered from the object's own tag at runtime,
so it names the class the value really is (`Widget`), not the shape it is being read through (`Labeled`).
An object literal has no class of its own, so it answers `Object`.

**A `type` may require functions, not only attributes** (D16, decided by Mortaro, 2026-09-19; implemented), matched by shape
exactly as an attribute-only `type` is:

```gdscript
type Renderable {
    render(): Element
}
```

Any class with a `render()` of that signature is accepted. This is what lets a collection hold "any class that
responds to `render()`" -- a list of components, section 17 -- without a union naming every class in advance,
and it makes the respond-to check section 16 item 9 promises expressible as an ordinary type rather than as
reflection.

**A `type` can be the element of a variadic parameter** (D90): `...children: List<Renderable>` takes any number
of values of any classes that fit, written one by one, "a generic can be done over both a type or a class, but in
the end monomorphised into each class". Each class that is passed is admitted to the shape, and each call on an
element is compiled once per admitted class (section 5, [Variadic arguments](#variadic-arguments-implemented)).

**A shape's members behave as a class's do** (proposed by Claude, unconfirmed, 2026-09-24): a required function
read without calling it is a function value bound to the value (D17), dispatched on the value's class when it is
called, so `Parallel(stages[index].run_once)` infers its codegen value from it; and a `List` of a `type` answers
the member templates over the attributes and the argument-free functions the type names
(`conformance/stage6/shape_members`).


## 8. Metaprogramming

Everything is a class, including classes. Reflection is resolved at compile time wherever possible.

### Reflection objects  **[partial]**

Metaprogramming classes live in the `Spite` namespace (decided 2026-09-19:
`Spite.Class`, `Spite.Attribute`, not `SpiteClass`/`SpiteAttribute`). `Spite` is a reserved root namespace: a
user folder named `spite` (or `load("spite")`) is a diagnostic.

D12 (decided by Mortaro, 2026-09-19): **every reflection object lives in the `Spite` namespace** -- `Spite.Class`,
`Spite.Function`, `Spite.Argument`, `Spite.Attribute`, and whatever else reflection grows. One namespace, and
nothing in it can be mistaken for a user class. **[implemented]**

| Object | Members |
|---|---|
| `Spite.Class` | `.name: Symbol` (D68), `.namespace: Spite.Namespace?`, `.attributes`, `.functions`, `.instances`, `has_function(name)` (D114; folds on a codegen type), plus the function of `Spite.Class`s a class may override ([above](#class-level-functions-and-why-there-are-no-static-functions-planned)) |
| `Spite.Function` | `.name: String`, `.arguments: List<Spite.Argument>`, `.returns: Spite.Class` (`Nothing` when none is declared), `.owner`, `call_function()`, `name_fits(pattern)` (D116) |
| `Spite.Argument` | `.name: String`, `.class: Spite.Class` |
| `Spite.Attribute` | `.name: String`, `.class: Spite.Class`, `.value: String`, `.object: Spite.Attribute.Object?` (below) |
| `Spite.Namespace` | `.name: String` (the segment), `.name_with_namespaces: String` (dotted), `.parent: Spite.Namespace?`, `.classes`, `.namespaces` |
| `Spite.Memory` | `.address: Long`, `.bytes: Long`, `.section: Spite.Memory.Section` (`'heap'`, `'stack'`, `'constant'`) -- what `value.memory` answers (D101) |

**Every member above is a getter, and none has a setter** (D88, decided by Mortaro; built 2026-09-24).
`library/spite/class.spite` and its neighbours keep the data in private fields (`_name`, `_namespace`,
`_attributes`, ...) and answer `get_name()`, `get_namespace()` and so on, so `klass.name` reads through the
ordinary getter interception and `klass.name = ...` is "'name' is read-only" (`diagnostics/reflection_read_only`).
Everything reflection offers is ordinary code in `library/spite/`, reachable by name at run time and from the
REPL (D92). The members only the compiler can write -- `value_attributes()`, `value_functions()` and
`assign(text)` on `Spite.Attribute`, `call_function()` and `call_with_text(arguments)` on `Spite.Function` -- are
the compiler's reopening of those classes (D82, section 11), printed by `--final_classes` like any member. A class
of the standard library describes its members too: `Memory.functions` lists every function `Memory` has,
supplied ones included.

**A name starting with `_` is private** (proposed by Claude, unconfirmed, 2026-09-24; section 2 said so without
a rule): it is read, written or called only inside its own class (a reopening is inside), and from anywhere
else it is an error naming the getter when there is one -- "'_name' is private to 'Class': ... and 'name' reads
it from outside". Without the rule, `klass._name = ...` would undo D88.

**`value.memory` is where a named value lives** (D101, decided by Mortaro; built 2026-09-24, the shape proposed
by Claude, unconfirmed): a class instance, a list or a dictionary answers with its object, a `String` with its
characters (`'constant'` for a literal, which is part of the program), and a number held in a local with the
local itself (`'stack'`) or in an attribute with that attribute (`'heap'`). Only a named value has an address: a
number computed on the spot is an error. It is built only where a program reads it. A class with an attribute
of its own named `memory` -- most of the standard library holds one -- answers that attribute instead, the way
an attribute named `class` shadows `.class`.

**What `.functions` contains** (proposed by Claude, unconfirmed): the functions a class declares, plus the
Symbol-codegen instances that were actually generated for it -- because those are functions of the class in the
program as built, and `--final_classes` already prints them (section 16 item 9). Reflection describes the
program that exists, not the source as written, which is the same rule D42 applies everywhere else.

**A constructor is not one of them.** It does not answer on an instance, it makes one, so putting it in
`.functions` would mean every caller that walks the list has to know to skip it -- and the first thing anyone
writes is a loop that calls what it finds. If a constructor needs to be reachable it belongs on the class object
as its own member, not in the list of what an instance answers.

**Reflection may be as detailed as it likes, because what is not used is not emitted** (D42, decided by
Mortaro, 2026-09-20). Most of this metadata will never be reached by any given program, and reaching for it is
the only thing that makes it cost anything -- "if it has a cost it is because we needed it anyway".

This works here in a way it does not elsewhere. Spite's reflection is resolved at compile time, so which parts
of it a program touches is **statically decidable**, and tree shaking is exact rather than conservative. A
language with runtime reflection cannot prove a class is never reflected on, so it must keep the tables for
everything; Spite can prove it, so it keeps nothing it cannot see a use for. Section 8 already works this way --
`value.attributes` is built only for a class that actually reads `.attributes` -- and D42 says that rule covers
the whole family: `Spite.Class`, `Spite.Function`, `Spite.Argument`, `Spite.Attribute`, `Spite.Namespace` and
whatever comes next.

The practical direction, for milestone 10 and for anything designing a reflection member: **err toward more
detail.** An unused field costs a program nothing, while a missing one costs a design.

**A namespace is an instance of `Spite.Namespace`** (D41, decided by Mortaro, 2026-09-20), for the same reason
`.class` is a real class: a string can only be matched, an object can be compared, walked and enumerated.
`Spite.Class.namespace` is one of these rather than a dotted `String`, and printing it prints the dotted name,
the way printing a `Spite.Class` prints its `.name`.

It mirrors what already exists. Section 11 builds namespaces out of folders, so `Spite.Namespace` is that tree
made readable: `.parent` walks up it, `.classes` and `.namespaces` walk down. A `load(...)` root therefore
becomes something a program can enumerate -- which is what D13's isomorphic split needs when it partitions a
package's functions, and what the deferred compile-time class generation would extend.

`.namespace` and `.parent` are `Spite.Namespace?`, and the chain ends at `null` (Mortaro, 2026-09-20,
rejecting Claude's proposal of a root object). A root object would have read as though every namespace were
nested inside something -- and inside the metaprogramming package in particular, when a namespace merely *uses*
`Spite.Namespace`, it is not contained by it. A top-level namespace has no parent, so it has `null`, and you
walk up with `class.namespace.namespace` for as long as that can be satisfied.

**`.class` is a real `Spite.Class`, not the type's name as a `String`** (D12). This is the change that makes
reflection composable: `function.arguments[0].class == ServerContext` is an identity comparison the compiler
checks, where `.class.name.starts_with("Server")` is a string test that quietly turns false when a class is
renamed. Printing a `Spite.Class` still prints its `.name`, so anything that reads as text today keeps reading
that way. **[`Spite.Attribute.class` is a real `Spite.Class` as of the 2026-09-20 reflection port; see the bullet
below.]**

- `value.class` is the value's class: a `Spite.Class` with `.name: String` (the class's own declared name) and
  `.namespace: Spite.Namespace?` (its namespace object -- `.name` is the segment, `.name_with_namespaces` the dotted path,
  `.parent` the chain up, `.classes`/`.namespaces` the tree down; `null` for a global class). Printing a
  `Spite.Class` prints just its `.name`, and printing a `Spite.Namespace` prints its `.name_with_namespaces`.
- **`class` is an inherited attribute of every instance, not a keyword** (decided by Mortaro, 2026-09-22):
  inside any function of a class, a bare `class` answers with that instance's class -- the same `Spite.Class`
  `value.class` gives -- so `class.name` is the class you are writing. A local or an attribute actually named
  `class` takes precedence over the inherited one (that is how `Spite.Attribute.class` reads its own field),
  and the parser's diagnostic is untouched: `class ClassKeyword {` at file scope still fails at 1:1 with
  "there is no 'class' keyword" (`diagnostics/class_keyword`), because a file is a class named after the file.
  **[implemented]**
- **A class name reads its class object member by member** (decided by Mortaro, 2026-09-22): `Weapon.name` is
  `"Weapon"` with no instance anywhere, `Weapon.namespace` answers the same `Spite.Namespace?` as
  `weapon.class.namespace`, and a method receiver written as a class name resolves against the class object --
  `Weapon.is_singleton()` works, while `Weapon.debug()` (a function of the class being described) still reports
  "'Weapon' is a class, not a value: write 'Weapon()' to make one", since there are no static functions. This
  is D6 read literally: a class name is an ordinary `Spite.Class` value, so the substitution that already made
  `Sticker.attributes` work is what every member read and method receiver on a static path does;
  `Weapon.instances` still means its live registry.  **[implemented]**
- **A class name passed as an argument is its class object** (D124, decided by Mortaro, 2026-09-25):
  `entity.remove_component(Ui.Component.Pressed)`, received as `component_class: Spite.Class` -- "we are not
  actually passing a class we are passing a instance of the class class that represents the component class".
  The readings below are **(proposed by Claude, unconfirmed)**: the rule is where a `Spite.Class` (or
  `Spite.Class?`) is wanted, so it covers an argument, `var kind: Spite.Class = Health`, an assignment to such a
  name and a `return` from a function returning one; a codegen value bound to a class (`return $component_type`)
  passes the same way. `Storage<Health>` without parentheses is still read only on the right of `==`. Anywhere else -- `var kind = Health`,
  or an argument whose parameter is a `type` -- a bare class name is still "'Health' is a class, not a value",
  since `Health()` is what was nearly always meant; the message now names the `Spite.Class` form
  (`diagnostics/class_as_value`). This does not meet D77, which is about calls: `f(Health())` passes an instance
  and `f(Health)` its class object, and the parameter's type decides which one compiles. **On the right of `==`
  a class name stays a class test (D75), except when the left side is itself a `Spite.Class`**: then the two
  class objects are compared, so `component.class == component_class` and `kind == Health` both mean "the same
  class" (`conformance/stage6/class_argument`). A class test on a `Spite.Class` value could only ask whether it
  is an instance of `Health`, which a class object never is, and before this it was silently `false`.
- `value.attributes` is a real, runtime `List<Spite.Attribute>` (built fresh, one entry per field, only for a
  class that actually uses `.attributes` -- nothing is generated for a class that never does): each entry has
  `.name: String`, `.class: Spite.Class` (D12) and `.value: String` (the field
  rendered as text; `""` for a field with no obvious textual form, such as a class, list, or union). Because
  it is an ordinary `List<T>`, reading it uses `while`, not a special loop form. `attributes[symbol]` inside a
  class is a separate, compile-time-only form: that same field indexed by a `Symbol` (see [Symbol
  codegen](#symbol-codegen-implemented) below).
- **`attribute.object` is the attribute's value as an object** (D123's second request; the spelling and the
  readings proposed by Claude, unconfirmed, 2026-09-25), where `.value` is its text. Its type is
  `Spite.Attribute.Object?`: `library/spite/attribute.spite` declares `type Object { }`, an empty `type`, which
  accepts any class and fits any empty `type` a program declares (`type Anything { }`), so
  `entity.add_component(attribute.object)` compiles once it is narrowed. There is no built-in "any" type: an
  empty `type` already is one, and each program keeps declaring its own (`mortaros_missing_decisions.md` item
  124). **A number, `Bool` or enum attribute is boxed**, as D109 boxes a plain value passed where a shape is
  wanted, so its `.class` is `Int` and `if object == Int` narrows it back; it is `null` only when the attribute
  holds `null` (item 125). **`value.attributes` works on a `type` or union value**, answered from the value's own
  class at run time, so `create_entity_from_bundle(bundle: Anything)` walks whatever bundle it is given. It is
  built only for a program that reads `.object`: every other program's attribute lists hold `null` there and
  admit nothing. Two mechanisms came with it: a class admitted to one `type` is admitted to every `type` a value
  of it has already been passed on to (the `Object` a bundle's fields fill is passed to `Anything`), and
  `.attributes` through a `type` reads every attribute of every class admitted to it, for D118
  (`conformance/stage6/attribute_object`, `docs/reflection.md`).
- `Person.instances` lists every live instance of a class. Conceptually each class is a variable living on the heap and so is
  its registry of instances; the compiler removes whatever is unused.  **[implemented]**
  A class object's `.attributes` and `.functions` are read from a stand-in instance at its defaults, which is not
  one of `.instances`; a singleton's stand-in is destroyed with the singletons at exit rather than leaked
  (proposed by Claude, unconfirmed, 2026-09-25; `conformance/stage6/reflection_stand_in`).
- **`Spite.Class.instances` is every class in the program** (D49, decided by Mortaro, 2026-09-20). A class is an
  instance of `Spite.Class` (D6), so the registry of *its* instances is the whole program's classes -- nothing
  new is needed for whole-program enumeration, it falls out of "everything is a class, including classes". A
  test runner uses it to find every test class instead of being handed them: no registration, no manifest, no
  `run(...)` per class. D42 pays for it, since a program that never enumerates its classes never generates the
  registry.  **[implemented]**
- **Reflection reads at two levels, and the same word is right at both** (D11, decided by Mortaro, 2026-09-19).
  A class object is an instance of `Spite.Class` (see [Class-level
  functions](#class-level-functions-and-why-there-are-no-static-functions-planned)), so it has attributes of its
  own -- the declarations -- while an instance has their values:

```
weapon.attributes['damage']    # the value held in that field
Weapon.attributes['damage']    # the Spite.Attribute that describes the field
```

  A PascalCase receiver is the class and a lowercase one is an instance, which is the same convention section 17
  uses for `user32.Input` versus `user32.input_mouse`, so nothing new is needed to tell them apart. This is also
  what stops `attributes[symbol]` from being a special, compile-time-only form bolted onto Symbol codegen: it is
  ordinary indexing of the attribute mapping, at whichever level the receiver names. **[planned: the exact shape
  of the class-level mapping -- keyed by symbol, and what it answers for a field the instance has not set -- still
  needs design; see PLAN.md milestone 10.]**

```gdscript
func describe(person: Person) {
    var attributes = person.attributes
    var index = 0
    while index < attributes.count() {
        var attribute = attributes[index]
        console.print(attribute.name, attribute.class)
        index = index + 1
    }
}
```

### Class-level functions, and why there are no static functions  **[implemented]**

D6 (decided by Mortaro, 2026-09-19): **Spite has no static class functions and will not get any.** A class is an
instance of `Spite.Class`, and `Spite.Class` is an ordinary standard library class with an ordinary declaration.
So there is nothing for `static` to mean: a function that belongs to the class rather than to its instances is
just a function on that `Spite.Class` object, and `Spite.Class` is where it is declared, with its default.

A class file may **override** one of those functions for its own class object, the same way any class reopens
another (section 11):

```gdscript/class.spite
func is_singleton(): Bool {
    return false
}
```

```console.spite
func is_singleton(): Bool {
    return true
}
```

Since D104 a singleton is no longer said this way: `singleton` is a header line (below), and declaring
`is_singleton()` in a class is an error naming it. `is_singleton()` stays the member `Spite.Class` answers.

- **The set of class-level hooks is exactly the set of functions `Spite.Class` declares.** There is no keyword
  and no marker: if the name is one of those, the definition belongs to the class object; otherwise it is an
  ordinary instance function, as every function in a file has always been. Reading `Spite.Class` in the standard
  library is how you learn the full list.
- A class wanting an ordinary instance function whose name collides with one of them is a diagnostic naming
  `Spite.Class`. The list is deliberately short.
- The override is **evaluated at compile time** and must fold to a constant. A `return` of a literal always
  folds; a `return` of a codegen value (`return $shared`) folds too, so a class-level fact can differ per build
  without any new syntax. Anything the compile-time evaluator cannot fold is a diagnostic. **[planned: until
  milestone 10's evaluator exists, only a literal `return` folds -- which covers every case the standard library
  needs.]**
- `--final_classes` prints each class's function of `Spite.Class`s with the value they folded to.
- **`Spite.Class` is reopenable, like any other standard library class** (D7, decided by Mortaro, 2026-09-19:
  "by all means shoot the foot"). Reopening it changes a class-level default for the **whole program**: a root
  whose `spite/class.spite` returns `true` from `is_singleton()` makes every class in the program a singleton,
  `List<Int>()` included. Adding a *new* function to `Spite.Class` is louder still -- it creates a new
  class-level hook name program-wide, so any class that already had an ordinary instance function by that name
  becomes an override of it.
  Nothing about this is silent, which is the reason it is allowed: a name that collides is a compile error
  naming `Spite.Class` (above), and a changed default shows up per class in `--final_classes`, with the root it
  came from, exactly as any other reopened function does. The compiler has no warnings (section 12), so
  visible generated output is the whole mitigation. **[planned: blocked on section 16 item 1, reopening standard
  library classes at all.]**

**Why a function and not a declaration** (Mortaro, 2026-09-19): it is the same philosophy as a configuration
file being a class that gets reopened -- like Rails patching its internal options through a block that can run
code, rather than through a static configuration file. A static line can only state a value; a function can
compute one, and it costs the language nothing because functions already exist.

### Singletons  **[implemented]**

D104 (Mortaro, 2026-09-24, correcting an omission; superseding D8's function form): **a singleton says so with a
`singleton` line at the top of its file**, first in D67's order -- before `generic` lines, enums, unions, types,
variables, the constructor and functions. `is_singleton()` stays readable on `Spite.Class` (D88), answered from
the line.

```console.spite
singleton

var memory = Memory()
```

Declaring `func is_singleton()` in any class but `Spite.Class` is an error naming the line
(`diagnostics/singleton_function`). D8 (2026-09-19) had declared it with that function of `Spite.Class`, overriding
the default `Spite.Class` declares; what follows still holds for the line.

- **Call sites never change.** `var console = Console()` everywhere, exactly as it reads today. A human skimming
  a call site is never told and never needs to know; the AI writing the code learns it from the class file and
  from `docs/for_ai_writers.md`.
- **One instance per distinct constructor argument list**, and a singleton's constructor arguments must be
  literals (the rule `load(...)` already has). `Console()` is one object;
  `DynamicLibrary("user32.dll", 'windows', "windows.h")` is one object however many classes ask for it, and
  `"gdi32.dll"` is a second one. The compiler emits one static slot per distinct argument list, so there is no
  runtime registry walk -- only a guarded branch the first time.
- **A generic singleton has one instance per set of codegen values** (proposed by Claude, unconfirmed; the
  reading of "one per literal argument list" for `generic` lines, 2026-09-24): `Column<Health>()` is one object
  wherever it is called and `Column<Label>()` is another, since codegen values are literals too. It was accepted
  and silently made a new instance per call before (`conformance/stage6/generic_singletons`).
- **Memory:** the slot holds one reference, so the count never reaches zero; `drop()` runs at program exit, in
  reverse creation order, and `--debug-memory` counts singletons as roots, never as leaks. Since a singleton never
  dies early, it is not counted at all (proposed by Claude, unconfirmed, 2026-09-24): its retain and release do
  nothing, the program records each singleton as it is made, and destroys them in reverse at exit. Two
  `Parallel` threads fetching `Slot<Int>()` 40 million times took 0.8 s with the atomic count and 0.04 s without
  (`conformance/stage6/singleton_counts`).
- **Teardown order** (proposed by Claude, unconfirmed, 2026-09-24): a singleton counts as made when its
  constructor *finishes*, so a singleton its attributes or constructor made is made before it and outlives it.
  At exit standard output is flushed, then singletons are destroyed newest first, and every `DynamicLibrary` is
  unloaded after all of them, since any code may call through a library and a library calls nothing back. A
  `drop()` may therefore use any singleton made before its own, and any library. A `drop()` that fetches a
  singleton already destroyed -- one first made *after* the dropping singleton, in a later call -- halts with
  `spite: a drop() at exit used the singleton Archive after it was destroyed: ...`, naming the fix: keep it in
  an attribute. A singleton first made inside a `drop()` is destroyed right after it
  (`conformance/stage6/singleton_teardown`, `conformance/stage6/singleton_used_after_exit`).
- **Made once, whichever thread asks first** (proposed by Claude, unconfirmed, 2026-09-24): in a program that
  starts threads, the first fetch of a singleton takes a lock of its own, checks again and makes it; every later
  fetch is still one load. Two `Parallel` threads first touching `Shelf<Int>()` made it twice before, and one
  copy leaked (`conformance/stage6/singleton_race`). A program with no threads keeps the plain check.
- **A singleton that holds nothing** -- no attributes but settings the compiler folds, and no `drop()`, such as
  `Memory`, `Build` and `TypedMemory<T>` -- is one static object in a production build and an ordinary singleton
  in an inspectable one (D143, section 13). There it may be made again if something destroyed after it at exit
  asks for it, since it has nothing to lose (proposed by Claude, unconfirmed).
- **Standard library:** `Console`, `Program` and `DynamicLibrary` are singletons. `File`, `Directory` and
  `Process` are not -- they are values (a path, a spawned command), and several may exist at once.
- A later root reopening the class (section 11) may not disagree about it; that is a diagnostic. Reopening
  `Spite.Class` itself to move the default for every class at once is allowed and is D7's foot to shoot.
- A class wanting an ordinary instance function named `is_singleton` gets the error above, which names the
  `singleton` line.
- **A singleton is always bound to a variable before it is used** (D110, decided by Mortaro): "always force
  singletons to be used as variables first so AI does not get tempted to inline it." A singleton's constructor
  call may only be the whole value of a `var`; a member read or call on it, passing it, returning it, assigning
  it or putting it in an operation is an error naming the fix (`diagnostics/inline_singleton`,
  `diagnostics/inline_singleton_attribute`):

  ```
  console.print(Build().target_operating_system)
  # error: 'Build' is a singleton: bind it once beside the attributes, 'var build = Build()', and use
  # 'build.target_operating_system'
  ```

  The readings below are **(proposed by Claude, unconfirmed)**: the binding may be an attribute ("on top", which
  the message names) or a local `var` in a function -- a value class such as `String` has no attribute to
  spare, and an error path (`var program = Program()` before `program.exit(1)`) need not hold the program for
  the object's whole life. D77 lets a constructor be an argument; a singleton's constructor is the exception
  (`greet(Console())` is an error). A generic singleton (`TypedMemory<Int>()`) is covered the same way. As
  with D77, only code the compiler generates is checked: a function nothing calls is not.

Rejected on the way here, each for a reason worth keeping: `$singleton = true` and `$instances = 1` (`$` means
"replaced at code generation", and a directive the compiler reads and deletes is never replaced by anything); a
bare `singleton` first line (the file-top declaration shape D5 removed for `generics` -- adopted after all by D67
and D104, once `generic` lines made the header a pattern); `func Console(): Console`
or `func Console(): Spite.Singleton` (the constructor's return type carrying the meaning -- quiet, and the second
makes the return type describe how rather than what); `func shared_instance(): Console` (loudest and most honest,
but "the constructor is named after the class" stops being one rule); a private constructor `func _Console()`
(`_` would mean private, intentionally unused, *and* singleton); `var console = Console` without parentheses (the
class can no longer enforce it); and a constructor hand-written to return `Class.instances.first()` (boilerplate
in every singleton, plus a second way to allocate).

### Symbol codegen  **[implemented]**

There are no macros. A parameter of class `Symbol` whose name is a segment of its function's name turns the function into codegen
for every name that fits: below, `set_attribute` answers `set_age`, `set_name`, and so on, for each attribute of the class.
Inside, the symbol names that attribute: written as a type (`value: attribute.class`, `): attribute.class`), it
is the attribute's actual type; written as an expression, `attribute.class` is a `Spite.Class`
naming that type, printing just like the type name would -- `"{attribute.class}"` included, since a text hole
holding a value whose class declares `to_string()` calls it (D109's reading of printable, applied to text;
proposed by Claude, unconfirmed, 2026-09-24; `conformance/stage6/class_text`).

```person.spite
func set_attribute(attribute: Symbol, value: attribute.class) {
    attributes[attribute] = value
}

func get_attribute(attribute: Symbol): attribute.class {
    return attributes[attribute]
}
```

```gdscript
var person = Person(1)
person.set_age(2)
```

A function with the exact name always wins over codegen. Only the names actually called are generated.

**Fixed in milestone 9a:** a `get_<attribute>()` generated this way used to double-free when the attribute was
an owning one (a `String`, `List<T>` or `Dictionary<T>` field). Reference counting made every return a properly
retained, independent value, so a generated getter is now exactly as safe as an explicit one -- which is what
lets D15 (next subsection) treat a field and a zero-argument function as the same member. See `docs/KNOWN_ISSUES.md`
item 2, closed.

**Another class's attributes, and every attribute at once** (proposed by Claude, unconfirmed; built for D95's
`Json`, 2026-09-24).  **[implemented]** Two additions, both needed to write JSON with the metaprogramming rather
than beside it:

- **`attribute: Symbol<Label>` ranges over `Label`'s members** instead of the template's own class: its
  attributes, and its functions that take no arguments (D15). Inside, `label.attributes[attribute]` is that member
  of the `Label` passed in -- the field, read or written, or a call to the function -- and `attribute.name` and
  `attribute.class` mean what they always did. `List`'s member templates are written this way,
  `member: Symbol<$element_type>` (D91). In a generic class the class is usually the codegen value,
  `Symbol<$value_type>`; when that value is not a class the template answers nothing.
- **`Symbol<Row>` over a `type`** (proposed by Claude, unconfirmed, 2026-09-24) ranges over the attributes the
  type names and the functions it requires with no arguments; `row.attributes[attribute]` is the shape's own
  read, write or call, answered from the value's class at run time, and the plural walks the type's attributes in
  its order (`conformance/stage6/shape_attributes`).
- **The plural calls it for every attribute.** `show_attributes(label, lines)`, for a template `show_attribute`
  whose symbol is `attribute`, calls `show_<name>(label, lines)` once per attribute, in declaration order. It is
  an ordinary generated function whose body is those calls, so it is typed, visible and shaken like any other,
  and the template it repeats must return nothing (`diagnostics/every_attribute`). The plural is the word
  `attributes` already means "all of them" in, which is why it was chosen over a new keyword or loop form: the
  old compile-time-unrolled `for instance.attributes` went with `for`, and this is its replacement without one.
  Over another class's attributes it calls only the ones that class lets others read: a private `_` attribute
  is skipped rather than being the private error (proposed by Claude, unconfirmed; found by D109's
  `to_debug()`).

```gdscript
func show_attribute(attribute: Symbol<Label>, label: Label, lines: List<String>) {
    lines.append("{attribute.name}: {label.attributes[attribute]}")
}
```

`conformance/stage6/every_attribute`, `docs/metaprogramming.md`.

### A class's functions, a folder's classes and a name's pattern  **[implemented; the spellings proposed by Claude, unconfirmed]**

D114, D115 and D116 (decided by Mortaro in the SlopEngine session) are one mechanism: the Symbol templates above,
ranging over three more things a program already has -- a function's arguments, a folder's classes, and the
functions whose names fit a pattern -- plus one question a generic asks of its type. Everything is decided while
compiling: there is no registry, no list walked at run time, and nothing is generated for a name no program
calls. The spellings below are Claude's (2026-09-24), chosen to be the existing forms read one step further:

- **`$system_type.has_function('run_each')` in a condition folds like `if $is_magic`** (D114). The name is a
  literal -- a symbol, or text holding a pattern such as `"<phase>_each"` (D116), which is true when some
  function's name fits it with a non-empty middle. Only the taken branch is compiled, so it may call what only
  that type has; any other argument is an error (`diagnostics/function_reflection`). It is an ordinary member of
  `Spite.Class` (`library/spite/class.spite`, D92), so `klass.has_function("boost")` also answers at run time,
  from `.functions`. Folded, it counts the functions the class declares: not its constructor, not a `_`
  function, and not what a template generated for it.
- **`argument: Symbol<$system_type.run_each>` ranges over the arguments of `run_each`** (D114). A `Symbol<X>`
  already ranged over X's members; a function's members are its arguments. Inside, `argument.name` is the
  argument's name and `argument.class` its type, written as a type (`Query<argument.class>()`,
  `): argument.class`, and `argument.class.element_type` for a `List<Row>` argument) or read as a
  `Spite.Class`. The plural (`count_arguments()`) calls the template once per argument, in order. When
  `$system_type` has no `run_each`, the template answers nothing and the plural calls nothing.
- **A template over arguments that returns a value fills a call** (D114): its plural, written as the whole
  argument list of a call to that same function, passes one value per argument --
  `system.run_each(row_arguments())` is `system.run_each(row_potion(), row_target())`, evaluated left to
  right. That is how one generic calls a function of any arity without variadic generics. Anywhere else such a
  plural is an error naming the call it belongs in, and so is passing it to a different function
  (`diagnostics/function_reflection`). D77's one-level rule does not apply to it: the call it stands for is
  written by the compiler.
- **`system: Symbol<System>` ranges over the classes of every folder named `system`** (D115): `System.Heal`,
  `Ui.System.Interact` -- a range that names no class or type is read as the end of a dotted namespace, and the
  classes of every namespace ending in it are walked in order of their dotted names. Inside, `system.class` is
  the class (so `Runner<system.class>()` instantiates the generic per class) and `system.name` its dotted name;
  the single instance is named after the whole path (`add_system_ui_system_interact`). A generic class there is
  skipped, since it has no values to be made with. A range that matches no folder at all walks nothing, like a
  class with no attributes (proposed by Claude, unconfirmed, 2026-09-24; it was an error, which failed an engine
  that walks `Symbol<Recipe>` for a program with no recipes even where nothing called the walk). A typo is caught
  where a single class is named instead: `cook_recipe_bread()` for a class no folder holds is an error listing
  the ones it does (`conformance/stage6/empty_folder_range`, `diagnostics/empty_folder_range`).
- **`phase: Symbol<$system_type.phase_each>` ranges over the functions whose names fit `<phase>_each`** (D116):
  when the parameter's own name is a word of the function name in the range, that word is the hole, exactly as
  it is in a template's own name. `update_each` gives `phase` = `'update'`; `run_each_before_phase` would match
  `run_each_before_render`. The plural (`run_phases_each()`) walks the matching functions in declaration order.
  Inside, `phase.name` is the matched text, and the pattern written as a member -- `system.phase_each(...)` --
  calls the matched function; another template ranging over `$system_type.phase_each` from inside walks that
  function's arguments, its instances named for it (`row_potion_in_update_each`) so two matched functions never
  share one. The engine owns the list of phases and decides what they mean; the language only reads the name.
- **A template's symbol is passed to a helper by name** (proposed by Claude, unconfirmed, 2026-09-24; SlopEngine's
  runner had to keep its whole loop inside `run_phase_each`). A function whose `Symbol<...>` parameter is not a
  word of its own name -- `run_combination(phase: Symbol<$system_type.phase_each>, combination: Int)` -- is not
  an ordinary function taking a run-time `Symbol`: it is a template compiled once for each symbol passed to it,
  and the only thing its symbol argument may be is the calling template's own symbol over the same range, written
  by name: `run_combination(phase, combination)`. Its instance is `run_combination_for_update`, keyed
  `_in_<function>` as above when its range is a function's arguments inside a pattern; inside it `phase` means
  everything it meant in the caller, so `system.phase_each(row_arguments())` works there too. Anything else passed
  is an error, and so is a call from outside such a template (`conformance/stage6/passed_symbol`,
  `diagnostics/passed_symbol`). A plain `Symbol` parameter, with no range, stays a run-time value.

```gdscript
func add_system(system: Symbol<System>) {
    var runner = Runner<system.class>()
    runners.append(runner)
}

func run_phase_each(phase: Symbol<$system_type.phase_each>) {
    counts.clear()
    count_arguments()
    ...
    system.phase_each(row_arguments())
}

func row_argument(argument: Symbol<$system_type.phase_each>): argument.class {
    var query = Query<argument.class>()
    ...
}
```

`--final_classes` shows the result as ordinary functions: `add_system_drink_potion()` holding
`Runner<System.DrinkPotion>()`, and `RunnerDrinkPotion` with `run_update_each()`, `row_potion_in_update_each():
Potion` and the rest. `conformance/stage6/system_functions` (a `run_each` of two rows and a `run_all` of a list,
chosen by `has_function`), `conformance/stage6/system_folder` (`system/` and `ui/system/` found with no list),
`conformance/stage6/system_phases` (two phases run in the engine's order, not the folder's),
`docs/metaprogramming.md`, `docs/reflection.md`.

### Standard library metaprogramming  **[partial]**

D15 (decided by Mortaro, 2026-09-19): **every standard library template names a `<member>`, and a member is a
field or a zero-argument function without distinction.** `map_age` and `map_get_age` are the same call, because
reading an attribute already goes through `get_<attribute>()` (section 5) -- the compiler may answer the field
case with a direct read, but that is an optimisation, not something the writer thinks about. What a template
requires of a member is its **arity and its return type**, never whether it is stored or computed.

`List<T>` of a class: `filter_<member>()`, `count_<member>()`, `sum_<member>()`, `find_by_<member>(value)`,
`sort_by_<member>()`, `each_<member>()`, `map_<member>()`, `any_<member>()`, `all_<member>()` (section 15 has
the full table). `Dictionary<T>` of a class has the same. These, together with `while`, are meant to cover the
cases that used to reach for `for`.

| Template | The member must |
|---|---|
| `filter_`, `any_`, `all_`, `count_` | take no arguments and return `Bool` |
| `sum_` | take no arguments and return a numeric type |
| `sort_by_` | take no arguments and return something ordered (`Int`/`Float`/`String`) |
| `find_by_(value)` | take no arguments and return something comparable to `value` |
| `map_` | take no arguments and return anything; the result is a `List<U>` of that |
| `each_` | take no arguments; its result, if any, is discarded |

This is why a list of components renders with nothing new in the language: `todos.map_render()` calls `render()`
on every element and collects the results, exactly as `todos.map_title()` collects a field. A member that does
not fit the template's requirement is a compile error naming the member, what it returns, and what the template
needs -- and `each_<member>()` on a plain field is one of those, since reading a field and discarding it does
nothing.

`count_<member>()` counts how many elements have a Bool member true; `sum_<member>()` adds up a numeric member
across every element (second batch item 8, decided 2026-09-19: `count()` is only ever a collection's own size,
and `count_<member>()` on a numeric member is a compile error naming the `sum_<member>()` fix).

```gdscript
var repositories = List<Repository>()
repositories.filter_active().sum_stars()
repositories.each_bump_stars()
```

**How the templates are written** (D91, decided by Mortaro; the binding rule is proposed by Claude,
unconfirmed). They are Symbol codegen templates (above) in `library/list.spite`, each a `while` over the list's
`Memory` buffer: `func filter_member(member: Symbol<$element_type>): List<$element_type>` answers every
`filter_<member>` call. The symbol names a member of the *element*, not of the list (whose own attributes are
its buffer), because it says so -- `Symbol<$element_type>` is section 8's `Symbol<Label>`, one mechanism for both
-- and `item.attributes[member]` reads it: the field, or a call to the zero-argument function -- the
D11 reading, "the value held in that field", applied to D15's members. The generator binds the template to the
element's member and checks the table above before it compiles the body, so a member that does not fit is still
the error naming the member, its type and what the template needs; it writes none of the templates' C. Only the
names a program calls are compiled. A `--repl`/`--repl_port` build compiles every template that fits every
element class of a list the loop can reach, and lists them as that list's functions, so `monsters.sum_health()`
works at the prompt. `Dictionary<T>` answers the same names through its values (`inventory.sum_price()` is
`inventory.values().sum_price()`), and a program's own `list.spite` reopens `List` to add a template of its own.
A template there with a plain `member: Symbol` would range over `List`'s own attributes, its buffer, and answer
nothing, so it is a compile error naming `Symbol<$element_type>` (proposed by Claude, unconfirmed;
`diagnostics/plain_symbol_on_list`).

**Chains are one loop** (D105, decided by Mortaro; the form below is proposed by Claude, unconfirmed).
Each template takes what the previous one returns, and a chain means exactly its steps written out one by one.
The compiler runs a chain as one loop over the first list with no list in between: when a template is called
directly on a `map_`/`filter_` call (on a `map_`/`filter_` call, and so on) of a `List` or `Dictionary`, the
generator writes one Spite function on the first list's class -- a `while` over its buffer, an `if` per
`filter_`, a `var` per `map_`, and the last template's step -- and calls that instead. A `map_` in the middle
must reach a class; anything the rule cannot write (a nullable member, a union) is compiled step by step, which
means the same. The difference a program can see is only order: a member function in a fused chain runs element
by element. `conformance/stage6/fused_chain_allocations` pins it: four chains run a thousand times allocate
nothing (15 allocations in all, the `Launcher` and printing the total included -- the `TypedMemory` its lists
share is a static singleton -- against more than 16 000 step by step).

**Passing a function for each element** (D148, decided by Mortaro, superseding D113's caller-function templates;
the forms' details below are proposed by Claude, unconfirmed).  **[implemented]** An iterator sees only the
element and the list, never the class the call is written in: `people.each_say_hello()` names a member
`say_hello` of each `Person`, and a function of the caller never answers
it. When the calling class has a function of that name, the error says to pass it
(`'String' has no attribute or zero argument function 'say_hello' for 'each_say_hello': a template reads a member
of each element, never a function of this class, so pass this class's 'say_hello' instead: 'each(say_hello)'`).
The caller's function is passed as a bound function value (D17/D39), owned by whoever it is bound to:

- **The forms.** `each(f)`, `map(f)`, `filter(f)`, `any(f)`, `all(f)`, `count(f)`, `find(f)`, `sort_by(f)` and
  `sum(f)` on a `List` or a `Dictionary` (through its values), for an element of any type. `f` takes the element
  as its only argument, with exactly the element's type; D15's table applies to what it returns (`filter`, `any`,
  `all`, `count` and `find` want `Bool`, `sum` a number, `sort_by` a number or a `String`, `map` a value, `each`
  anything). `find(f)` answers the first element `f` is true for, or `null` -- the `find_by_` template with
  `true` as its value. `count` with no argument stays the collection's size.
- **The owner.** `say_hello` alone is bound to this instance; `greeter.greet` to `greeter`; a variable holding a
  `Spite.Function<T, R>` is called through the value. `people.map(greeter.label).filter(is_short)` mixes owners.
- **How it is written.** No template changes: the same `library/list.spite` template (`each_member(member:
  Symbol<$element_type>)`) is instantiated once per function and owner class, with a last hidden parameter holding
  the owner (the function's instance, passed at the call site), and `item.attributes[member]` reads as
  `owner.say_hello(item)` -- or `owner(item)` when the owner is a held function value. A function written by name
  therefore costs no allocation and no indirect call; only a held value is called through `Spite.Function`. Only
  the forms a program calls are instantiated, and these instances are not listed among the list's `functions`
  nor offered at a `--repl` prompt.
- **Chains.** A passed function chains and fuses with the member templates (D105): `map(f)` and `filter(f)` may
  sit in the middle of a chain and any form may end one, so `people.filter_active().map(greeter.label)` and
  `names.filter(is_short).map(measure).sum(double_of)` are each one loop; the fused function takes each owner as
  a parameter. The element's own members still come only from the element (`people.map(say_hello).filter_active()`
  reads `active` of whatever `say_hello` returns).
- **Mistakes** name the form: `'map(say_hello)': 'say_hello' returns nothing, but 'map' needs it to return a value
  (to only call it for each element, write 'each(say_hello)')`, `'each' calls 'greet_twice' with each 'String' as
  its only argument, but 'greet_twice' takes 2`, `'each' calls 'count_to' with each 'String', but 'count_to' takes
  'Int'`, and an argument that is no function at all.
- **No loop rule.** D113's error for a `while` that only passes each element to a caller function is removed with
  it. A function that needs more than the element (`print_statement(statement, depth)`) keeps its `while`.

`conformance/stage6/passed_functions`, `diagnostics/passed_functions`, `docs/collections.md`.

## 9. Codegen values (`$`)  **[implemented]**

`$name` means "replaced at code generation". That is its only meaning, everywhere it appears.

D87 (decided by Mortaro, 2026-09-24, superseding D9's constructor list): **a class declares each codegen value on
a `generic` line of its own, at the top of the file**, "instead of constructor":

```weapon.spite
generic $damage_type
generic $is_magic

var damage: $damage_type = 0

func Weapon(new_damage: $damage_type) {
    damage = new_damage
}

func hit(): Int {
    if $is_magic {
        return 1
    }
    return 2
}
```

```gdscript
var sword = Weapon<Magic, true>(10)
```

- **Every value is declared, even a single one**, so skimming the top of a file is how a human sees what a class
  accepts. The lines come after a `singleton` line and before everything else (D67); one line declares one value.
- **Call sites are positional, always, in the order of the lines.** There is no named form: `List<Int>()`,
  `Weapon<Magic, true>(10)`.
- **`$` is for generics only** (D76, decided by Mortaro, 2026-09-23, superseding D9's "undeclared means supplied
  by a compiler flag"). Every `$name` a class uses has a `generic` line and is supplied by the caller, so the
  lines are exactly "what a caller must pass". A `$name` with no line is a compile error pointing at
  [`Environment`](#program-settings-environment), which is where a program's settings live now
  (`diagnostics/codegen_values`).
- **A class needs no constructor to take codegen values.** That was D9's cost, and the reason for D87:
  `library/list.spite` is `generic $element_type` and its functions, and `library/dictionary.spite` is `generic
  $value_type`; an empty constructor is still an error.
- **Every hole must be filled.** There are no defaults. A call supplying the wrong number is a compile error
  naming the class's codegen values, in order, so the mistake is corrected from the message rather than by
  opening the class. A `$name` that is not declared is an error too -- which is what a typo like `$is_magik`
  produces.
- Reordering the `generic` lines changes what every existing positional call site means. Where the values
  have different kinds (a class versus a `Bool`) the compiler catches it immediately; where they are the same
  kind (`Pair<Int, String>` swapped) it compiles and means something else, and the tests are what catch it. This
  is a deliberate, accepted trade (Mortaro, 2026-09-19).
- `--final_classes` prints each class with its codegen values already bound, which is where `true` reads as
  `is_magic` again.
- Writing `generics` is a parse error naming this form (as `for`, `&` and `Heap<T>` already are), and so is D9's
  `func Weapon<$damage_type, $is_magic>(...)`: the message lists the `generic` lines to write
  (`diagnostics/constructor_codegen_list`). A `generic` line inside a function is an error too.

Conditions on codegen values are decided at compile time and the untaken branch is removed (tree shaking), in
every build, `--development` and `--hot_reload` included (settled 2026-09-25 by keeping what was built): a
codegen value is part of which class this is -- `Weapon<Magic, true>` and `Weapon<Magic, false>` are two classes --
so there is no run-time value for live reload to change. To change one, change the call site.

**Asking what type a generic was given** (proposed by Claude, unconfirmed; built for D95's `Json`, 2026-09-24).
**[implemented]** When a codegen value is a type, `$value_type == String` is decided at compile time like any
other condition on a codegen value, and only the branch taken is compiled -- so each branch may use what only
that type has, which is what lets one generic class treat text, numbers, lists and classes differently. It is
section 7's class test (D75) asked of a type instead of a value. A type name asks for exactly that type; four
names ask for a kind, since the type has arguments the test does not want to spell:

| Test | True when the type is |
|---|---|
| `$value_type == List` | any `List<T>` |
| `$value_type == Dictionary` | any `Dictionary<T>` |
| `$value_type == Null` | any `T?` -- `Null` is a member of the union a `T?` is (D45) |
| `$value_type == Symbol` | an enum, or `Symbol` -- an enum is a closed list of symbols (D10) |

A union name is true for any of its members. Such a test always folds, `--development` included, because the
branch it rules out would not compile.

**Only what survives folding is compiled** (proposed by Claude, unconfirmed, 2026-09-24). A function of a generic
class is compiled, and so type-checked, for one instantiation only when code already compiled for the program
names it -- a call that survived folding, a function value, a reflection table, or a call through a `type` the
class is a member of -- so a helper reached only from a
branch the instantiation rules out is never checked against that type. When a folded `if` (or `else if` chain)
takes a branch that ends in `return`, the statements after it in the same block are not compiled either, and
the names they read count as used, as for the untaken branch. Before, `Field<Int>.write_list` was checked for
`.count()` on an `Int`, and the only way out was one generic class per kind. A class that is not generic still
compiles every function. `conformance/stage6/folded_helpers`.

**The types a type was built from are read by their codegen names**: `$value_type.element_type` for a
`List<$element_type>`, `$value_type.value_type` for a `Dictionary<$value_type>` or a `$value_type?`, and a generic
class's own names for one of its instances -- the names this section already gives the containers. Reading a name
the type does not have is an error listing them (`diagnostics/every_attribute`).
`conformance/stage6/every_attribute`, `docs/metaprogramming.md`.

**A codegen value that is a type reads as its class** (proposed by Claude, unconfirmed, 2026-09-24): a member
read through it, `$component_type.name` or `$component_type.attributes`, is read from the bound class's
`Spite.Class`, exactly as `Health.name` is -- no value of the type is made. `$component_type` alone in an
expression is still the "is a type here" error (`conformance/stage6/codegen_class_name`).

**How this got here.** The `generics` header line existed to give positional call sites an order to follow;
Mortaro's objection (2026-09-19) was that a bare first-line declaration "feels outside of our patterns". Three
replacements were tried and rejected in order: a `$name = default` declaration form (kept a declaration alive for
its own sake -- a codegen value is a hole, not a variable); no declaration at all, with call sites naming the
values (`Weapon<$damage_type: Magic, $is_magic: true>`, which made every call site restate what the class already
knew); and a class-level `generics()` function returning a list of symbols (the wrong category -- class-level
functions answer questions *about* a class, while codegen values are inputs to constructing one, and it needed a
second list of names kept in sync with the real holes). D9 then put the ordered list on the constructor, which
forced a constructor on every generic class, even an empty one only there to hold the list; D87 moved it to one
`generic` line per value, which is a declaration like `var` rather than a header list, and made the header a
pattern shared with `singleton` (D104).

### Program settings: `Environment`  **[implemented]**

D76 (decided by Mortaro, 2026-09-23): "using $variables for both command line environment setup and generics was
a bad idea from my end, lets use it just for generics. instead lets have a singleton for dealing with Environment
arguments so entrypoint is not forced to receive the args of the console." `Environment` is a standard library
singleton (`library/environment.spite`), and a program **reopens** it (section 11) with a file of its own named
`environment.spite` to declare the settings it has:

```environment.spite
var serve = false
var name = "client"
var workers = 1
```

```gdscript
var environment = Environment()

func Dungeon() {
    if environment.serve {
        ...
    }
}
```

`Environment()` works anywhere, so the entry constructor no longer has to receive `Arguments` only to pass them
down. Only the declared fields are read: nothing the program does not name is loaded, which is what makes it a
deterministic replacement for packages like Node's dotenv. It mirrors Nullstack's `context.environment`, and it is
how a program such as the game engine gets several environments.

What follows is how it is implemented (proposed by Claude, unconfirmed):

- **Where a value comes from.** Each field is read once, when the singleton is first made, from the first of:
  the program's own command line, `--serve=true` (what the program receives -- after `--` when the compiler runs
  it, section 13 -- and stopping at the program's own `--`); the process environment variable named by the field
  in upper case, `SERVE`; the declared default. The command line wins because it is the more deliberate of the
  two. An argument that names no field is left alone for `Arguments` to read.
- **The default's literal is the type.** A setting is declared with nothing but a literal default: `false`/`true`
  is a `Bool`, a whole number (negative too) an `Int`, `""` a `String`. A type annotation, or any other default,
  is a compile error naming the three forms (`diagnostics/environment_setting`). Text that is not a value of the
  setting's type (`--serve=maybe`, `--workers=many`) **crashes** when the singleton is made (D24: a malformed
  setting is a bug in how the program was started, and there is nothing sensible to continue with).
- **How the fields get filled.** The compiler prepends one assignment per declared field to `Environment`'s
  constructor -- `serve = boolean_setting("serve", serve)` -- where `boolean_setting`, `integer_setting` and
  `text_setting` are ordinary Spite functions in `library/environment.spite`. The reading itself is Spite; the
  compiler only writes the calls, the way it writes a Symbol codegen function.
- **`Arguments()` is the command line, anywhere.** `library/environment.spite` reads the command line through
  `Arguments()`, which the compiler answers in any function with the program's command line. It is available
  to programs too, and since D89 it is how a program reaches arguments no setting names: the entry constructor
  receives nothing.
- **An `Environment` field is never given to the compiler.** `spite program --serve=true` is an error that says
  to pass it after `--` (`diagnostics/environment_setting_to_compiler`); a value decided while compiling is a
  `Build` field instead (below).

### Build settings: `Build`  **[implemented]**

D84 (decided by Mortaro), then D85 and D86 (decided by Mortaro): "Variables supplied as flags during compilation
stay hardcoded the others are runtime", then "lets already implement Environment into two things, one for
RuntimeEnvironment and one for CompileEnvironment but make better names for it, we can just change later, so
compiler flags are explicitely its own thing. make all our compiler options pass as a normal program from
CompileEnvironment so people can reopen to force them with defaults." `Environment` (above) is read when the
program **runs**; `Build` (`library/build.spite`, a singleton) is decided when it is **compiled**. The names are
Claude's proposal (`mortaros_missing_decisions.md`). Every compiler option is a `Build` field with a literal
default, and a program adds its own the same way it adds `Environment` settings, by reopening `Build` in a file
named `build.spite`:

```build.spite
var serve = false
```

```gdscript
var build = Build()

func Server() {
    if build.serve {
        listen()
    }
}
```

`spite server --serve=true` builds a server; `spite server` builds the one without `listen()` in it. What follows
is how it is built (proposed by Claude, unconfirmed, except where a decision is named):

- **Every field is a constant.** A `--name=value` before the `--` sets the field of that name; a field nobody
  sets keeps its declared default -- the program's own `build.spite` if it reopens it, else `library/build.spite`.
  Either way the value is written into the program: `build.serve` compiles to `true`, a condition on it is decided
  while compiling, the branch not taken is never generated, and nothing is read when the program runs. The field
  still exists on the `Build` singleton with that value, for reflection.
- **The compiler's options are fields.** The outputs `run`, `executable`, `c_source` and
  `final_classes`, the paths `executable_path` and `c_path`, and `optimized`, `development`, `repl`, `repl_port`,
  `hot_reload` and `debug_memory` (the flag names follow the fields: `--final_classes=folder`,
  `--repl_port=4000`). Formatting is not one of them: every compile formats first and nothing turns it off
  (D190), so `--format` and a `format` field in a program's `build.spite` are compile errors. The compiler reads
  them from the program's resolved `Build`, so a program
  whose `build.spite` says `var optimized = true` is built optimized unless `--optimized=false` is given. Since
  D128 no option is read before the program is, so there is no exception for `mode` any more (`mode`
  is gone: section 13); only `target_operating_system`, which says which library folder is part of the program,
  comes from the flag alone.
- **A `Bool` may be given bare**: `--optimized` is `--optimized=true`. Any other bare flag is an error naming the
  value it needs.
- **Nothing passes silently.** A value that is not of the field's type is a compile error
  (`diagnostics/build_setting_type`); a flag naming an `Environment` field is an error that says to pass it after
  `--`; a flag naming no field is an error listing the fields `Build` has (`diagnostics/unknown_compiler_flag`).
- **Two operating systems** (D86): `operating_system` is the system doing the compiling -- the compiler supplies
  it, and giving it is an error -- and `target_operating_system` is the one the program is compiled for, which
  defaults to it. `--target_operating_system=linux` loads `library/linux/` (section 3, the launcher) and folds,
  so `if build.target_operating_system == "windows" { }` keeps one branch. Each system's folder reopens `Build`
  with `var target_operating_system = "linux"`, which is only the default a compiler that does not fold `Build`
  sees -- the seed while bootstrapping. The compiler learns which system it runs on from its own
  `build.target_operating_system`, folded when it was built.
- **`program`** is the folder named on the command line, which the launcher loads; the compiler supplies it and
  giving it is an error.
- **`--final_classes` prints `Build` with its declared defaults**, not the values one build folded, so running the
  printed program takes its flags again instead of, say, printing its classes forever.

This settles the compile-time home D76 left open (D13's isomorphic split and section 17's `$target` read a
`Build` field), and answers the reminder Mortaro asked for with D84 -- "we may split these both into two types of
environments for runtime and compile time" -- which D85 did.

## 10. Memory  **[implemented]**

D1 (decided by Mortaro, 2026-09-19): **reference counting is the default memory model**, JavaScript-like. Every
non-scalar value (a class instance, `List<T>`, `Dictionary<T>`, `String`, a union of classes, an object literal) is
a reference: passing it, assigning it, storing it in a field/list/dictionary, and returning it all share the exact
same object. Scalars (every numeric type, `Bool`, an enum value) are still plain values, copied as always. `&` is
removed from the language entirely -- references are the default, so a parameter that used to need `&Type` just
writes `Type`. A copy is always explicit: `copy()`/`deep_copy()` (below).

- **Retain and release.** Every reference is counted. Binding one to a `var`/parameter, or storing it into a
  field/list element/dictionary value, retains it (bumps the count); a scope ending, a value being overwritten, or
  an element being removed from a container releases it (drops the count). When a release brings the count to
  zero: the class's own `drop()` function runs first, if it declared one (new -- see below), then every attribute
  that itself needs releasing is released, then the object is freed.
- **`drop()`.** A class may define `func drop() { ... }` to run cleanup the moment its last reference goes (closing
  a file handle, logging, clearing a back-reference to help break a cycle by hand -- see below). It takes no
  parameters and returns nothing; the compiler calls it automatically, never by name.
- **Identity vs equality.** `==` on two class instances calls `equals` if the class defines one (section 5's
  Operators table, unchanged); otherwise it compares **identity** -- are these two references the same object.
  `String` always compares by content, never by identity (sharing a `String`'s buffer is unobservable, since it is
  immutable).
- **`copy()`/`deep_copy()`** (names **proposed by Claude, unconfirmed**). Every non-scalar value has both:
  `copy()` is shallow -- a fresh object, its own attributes/elements the exact same references the source had
  (retained, not duplicated; a `String` field needs no special handling either way, since it is immutable).
  `deep_copy()` recurses: every reference-kind attribute/element gets its own `deep_copy()`/independent buffer
  instead of being shared. **Cycles are not supported** by `deep_copy()` -- a self-referential (or mutually
  referential) structure recurses forever; break the cycle by hand first if you need to deep-copy one.
- **Cycles leak.** Reference counting cannot free a cycle (two objects holding a reference to each other, directly
  or through several hops): neither one's count ever reaches zero. This is a known, accepted tradeoff, not a bug --
  break a cycle by hand when you are done with it (set the back-reference to `null` inside `drop()`-time logic, or
  clear a `T?` field that closes the loop) if it matters for a long-running program. **[planned]** A future
  opt-in type (e.g. a weak reference) is the intended real fix; not implemented yet.
- Parameters follow the same rule as everything else in section 5: a scalar is passed by value (copied); anything
  else is passed by reference (the same object, retained for the callee's own binding and released when the
  callee's scope ends) -- there is no separate reference syntax to write at the call site any more.

`--debug-memory` reports total allocations/frees (balanced for every example, `tests/references`, and the
bootstrap compiler's own runs, exactly -- not just bounded), plus a **leaked-object summary by class name** when
they do not balance -- naming which classes' instances are still live, which is what makes a leaked cycle visible
instead of just an unexplained non-zero count.

Every function/method return retains its result, so a getter's returned value is always a fresh, independent
reference -- including through attribute read interception (section 5): a `person.age`-shaped read answered by a
getter, used directly as a call/print argument rather than stored, is released like any other temporary
(`ExpressionResult.is_owning` in the generator is how a read that compiles to a call, but still
parses as a `.member`/`.index` expression -- an intercepted attribute read, `arguments.some_key`, the
`attributes[attribute]` template form -- tells every caller whether it is independently owned regardless of what
the plain AST shape alone would suggest).

### Placement: the compiler decides where memory lives  **[implemented; the rule proposed by Claude, unconfirmed]**

D108 (decided by Mortaro): "Memory should be a abstraction that lets us allocate heap/stack/register but our
compiler decides best use placement." A program has one way to ask for memory, `Memory.allocate_bytes`, and
one way to give it back, `free`; where the bytes live is the compiler's choice, and the program's text is the
same whichever it makes:

- **Register:** a number's own memory, declared in its file as `var memory = Memory()` and then
  `var _memory = memory.allocate_bytes(4)` (for `Int`; the `memory` binding is never a field, D110). The wrapper is flattened: a number is its C scalar, and `this` is the value.
- **Frame:** `var name = memory.allocate_bytes(bytes)` in a function, when a later statement of the same block
  is `memory.free(name)` and every other use of `name` hands it to `memory`'s reads, writes, `copy_bytes`,
  `compare_bytes` or `text` -- it is never stored, returned, assigned, resized or passed to anything else, and
  neither `name` nor `memory` is declared or assigned again after it. The compiler gives it a slot of 256 bytes
  in the function's frame (exactly the size, for a literal size up to 256), uses the heap when a run-time size
  is larger, and makes the `free` a no-op for the slot. A loop body is a block like any other, so the slot is
  reused on every pass. `Double.bits()` allocates nothing now, and `Long.to_string()` and `upper_case()` of a
  short text allocate only the `String` they return.
- **Constant:** a `String` literal's characters are part of the program (`_section` is `'constant'`).
- **Heap:** everything else.

`allocate_stack_bytes` (D98's stack section) is removed: asking for the stack by name was a second way to
allocate, and one the program could get wrong (an address kept past the return). **For data-structure authors**
(the ECS D98 names) nothing is lost: what makes a layout efficient is the author's -- one allocation holding many
values at offsets they choose, `TypedMemory<$value_type>` for values of any type, `resize` to grow -- and that
stays exactly as it was. A temporary buffer a function uses and frees lands in its frame without asking.

## 11. Packages, namespaces and loading  **[partial]**

There are no imports. Everything lives in one global namespace, populated by loading folders.

```gdscript
func Game() {
    load("package")
    load("cookie_clicker")
}
```

- The loaded folder is a package root and does **not** appear in the namespace. Folders inside it do:

```
package/engine/renderer/renderer.spite   ->  Engine.Renderer()   (a file named like its folder is the folder's entrypoint)
package/engine/renderer/debug.spite      ->  Engine.Renderer.Debug()
```

- `spite game/game.spite` **loads the entry file's parent folder** (second batch item 1, decided 2026-09-19,
  replacing "the entry folder is flat"): the entry folder is a real root exactly like a `load`-ed one, so every
  subfolder inside it is a namespace, recursively, with no explicit `load` needed. The two differences from a
  `load`-ed root: a subfolder the entry file itself explicitly `load(...)`s is left to that call (a `load`-ed
  root's own folder is never itself a namespace segment), and only the entry file itself is scanned from the
  entry root -- an unrelated sibling file next to it is still pulled in whenever it is reachable as a class, but
  its own `load`s are not followed.
- Folder names are always lowercase snake_case, checked for every folder that actually contains a `.spite` file anywhere inside
  it (an unrelated folder with none, such as `.git` or a build output directory, is never checked or descended for classes).
- Resolving an unqualified `Name` from inside a class tries, in order: that class's own namespace (so a nested enum/type/union
  resolves by its plain name from inside its own class), the same folder's namespace, each parent folder's namespace, then the
  whole program globally. Ambiguity *between roots* at the same level is never an error, because of the next rule.
  A dotted name (`Component.Requested`) takes the same walk, in every position a name is written -- constructor
  call, parameter, return type, attribute or local annotation, generic argument, `type`/`union` member, class test
  and a class's enum (proposed by Claude, unconfirmed, 2026-09-24; before, only a call took the walk and every
  type position needed the full path; `conformance/stage6/relative_namespaces`).
  A class's own namespace holds the classes named under it, so for a folder's entry file it is that folder:
  `click_test/click_test.spite` (`ClickTest`) reaches `click_test/system/verify.spite` as `System.Verify()`
  (proposed by Claude, unconfirmed, 2026-09-24; before, the walk started at the folder's parent, so the entry
  class alone needed `ClickTest.System.Verify()`; `conformance/stage6/folder_class_namespace`).
  A `type`, `union` or `enum` sits in the walk at the level of the class that declares it, so the nearest
  declaration wins: one in the using class before any class, one in a folder's entry file before a class further
  out (proposed by Claude, unconfirmed, 2026-09-24; before, a type written in a type position was looked up only
  after every class, so SlopEngine's `type Healing` in `system/regenerate.spite` meant the program's entry class
  `Healing`; `conformance/stage6/nearest_type`).
  A generic class's constructor takes the walk too, class and arguments alike: `Asset.Pack<Asset.Texture>()`, or
  `Pack<Rule>()` from inside `game/` for `game/pack.spite` (proposed by Claude, unconfirmed, 2026-09-24; until
  then a generic class had to live at a package root, `conformance/stage6/namespaced_generics`). A generic
  constructor that cannot be made reports once, and every later line that reads the value it would have made is
  not reported again, so one mistake is one error (`diagnostics/failed_constructor`).
- Every loaded root merges into the same namespaces. A second root with the same folder structure and file name **reopens** the
  class: this is how monkey patching and game mods work -- later `func`/`var` with the same name replaces the earlier one (in
  load order: the entry folder first, then loads in the order they were discovered), a `var`'s replacement type must match, and
  a new `func`/`var`/`enum`/`type` not seen before is simply added. The standard library (`library/`) is discovered
  before the program, so a program's own file reopens `Console`, `String`, `File` and the rest the same way.
  `Environment` is made to be reopened: a program's own `environment.spite` adds its settings to it (D76, section 9).
  Your foot to shoot. That includes `Spite.Class` (D7, section 8): reopening it moves a class-level default for the whole program.
- **Each operating system reopens the classes it changes** (D80). `library/` holds what every system shares, and
  `library/windows/`, `library/linux/` and `library/mac/` hold only what differs: `library/linux/file.spite` reopens
  `File` with the functions that call `libc.so.6`, `library/windows/file.spite` the same functions over
  `ucrtbase.dll`, and so on for `Directory`, `Process`, `Program`, `Console`, `String` (`to_double`) and
  `Build` (`target_operating_system`). There is no wrapper class in between: a platform file holds its own
  `DynamicLibrary("libc.so.6", 'identity', "")`, which is one shared instance per literal argument list (D8), and
  a one-line foreign call two classes both need is written in both. The launcher (section 3) loads `library/`
  first and then the one folder named by `build.target_operating_system`; that folder adds no namespace segment,
  and its files are read after `library/`'s own, so they replace or add members by the reopening rule above. `--final_classes`
  prints each class as it came out, platform functions included. Because exactly one folder is loaded,
  `library/file.spite` calls `open_file` without declaring it: every system's folder defines it, with the same
  signature.  **[implemented; only `library/windows/` runs here -- `check.sh` holds the linux and mac
  folders to compiling, by writing the compiler out once with each]**
- `load` takes a literal string, so the compiler always knows every bundle; anything else (a variable, an expression) is a
  diagnostic. The compiler finds every `load` reachable from the entry file's own constructor at compile time (a `load` inside
  already-loaded code counts too), and records each root as a bundle (its name and whether the `load` that introduced it sits
  inside an `if`/`while`) in the program model, for dynamic libraries/lazy loading to build on later.
- `load` marks a **bundle boundary**, like an async import in webpack: each loaded root can become a separate dynamic library,
  tree shaking is computed per bundle, and a `load` inside an `if` is loaded lazily when that line runs.  **[planned: every
  bundle is linked statically into the one executable for now, and the `load(...)` call itself compiles to nothing]**
- **`load` is a reserved word** (D166, decided by Mortaro, superseding the earlier proposal that a class's own
  `load` shadows it): `load(...)` always loads a package, and a function named `load` is an error suggesting a
  descriptive name -- `'load' is reserved: it always loads a package, so a function cannot be named 'load'. Name it
  for what it loads, such as 'load_texture'` (`diagnostics/load_function`, `docs/packages.md`).  **[implemented]**
- Because patching is dangerous to read, the toolchain writes a **final class** folder: every class after all codegen, with the
  winning function of every replacement, each preceded by a `#` comment naming the root it came from (and which roots it
  replaced) -- see `--final_classes` in [Command line](#13-command-line). The language server reads it too.
  **[planned: an instantiated Symbol codegen function and a used `List<T>`/`Dictionary<T>` helper signature do not appear here
  yet -- only the source classes are declared with]**
- **What the compiler supplies is a reopening too** (D82, decided by Mortaro: "spite writes that function as if it
  was reopening that class, so in --final-classes it should show the actual content of that class"). The members
  whose bodies stay C -- `Memory`'s floor, `DynamicLibrary`'s opening and symbol lookup, `TypedMemory`'s typed
  slots, a number's `from_type`, the REPL's hooks on `Spite.Attribute`/`Spite.Function`, and the entry addresses of
  `Concurrent` and `Parallel` -- are declarations the compiler merges into their classes right after `library/` (and
  its operating system's folder), exactly as a later root merges a file, so a program can still reopen them.
  Nothing is registered by hand any more (D81). **The form** (proposed by Claude, unconfirmed): **a `func` with a
  signature and no block** is a member whose body the compiler supplies, which is how `--final_classes` prints it
  (`func allocate_bytes(bytes: Long): Long`), and reading that back is what lets the printed program compile. It
  borrows the shape a `type` already uses for a member without a body. Anywhere the compiler supplies nothing by
  that name, a bodiless `func` is an error ("give it a body"), so it is not a way to declare anything else. The
  compiler's own reopening is Spite source in `bootstrap/source/generation/prelude.spite`, beside the C each body
  is; a body the compiler writes per instantiation (`TypedMemory<Int>`, `Float.from_int`) is written by the
  generator. Supplied names skip the naming lint (`read_int` names the type `Int`), as conversions already did.

## 12. Style  **[implemented]**

Decided by Mortaro: "the compiler is the linter and the formatter. Style is
arbitrary, it is Mortaro's taste, and it is the only way. The compiler rewrites source files to the one true
style automatically instead of complaining." He left the specifics open, so every concrete rule below
is **(proposed by Claude, unconfirmed)** except where noted -- revisit any of them on request. Formatting on
every compile and `spite format` are documented in [Command line](#13-command-line); the naming/abbreviation lints that
cannot be auto-fixed are their own subsection below.

<a id="one-call-per-line-and-nothing-said-twice--planned"></a>

### One call per line, and nothing said twice  **[implemented]**

Decided by Mortaro (D77, D78, 2026-09-24), from the tokenizer's `flush()`:

```
tokens.append(Token('number', source.slice(token_start, end_index)))    # error: slice() is passed as an argument
```

- **Only a constructor call may be an argument, and only one deep** (D77). "only constructors can be used as
  arguments of a call, any other functions need to be done outside, even in constructor case a 1 depth limit is
  needed." `tokens.append(Token('number', token_slice))` is legal; `source.slice(...)` inside it is not, and neither is
  a constructor inside a constructor inside the call. Anything else is computed first and named.
  **Implemented (2026-09-24).** The error names the call and the call it is passed to: "'source.slice(0, 2)' is
  called inside an argument of 'console.print': compute it first into a named 'var' and pass the name. Only a
  constructor may be an argument, and only one level deep", or, one level down, "'Text(source)' is passed to
  'Token', which is itself passed to 'tokens.append': ...". A constructor is a call whose callee's last name
  starts with an upper-case letter (`Token(...)`, `List<String>()`, `Syntax.Expressions.IdentifierExpression(...)`).
  The readings below are **(proposed by Claude, unconfirmed)**:
  - The rule is the same for every call, a constructor included: `var token = Token(kind, Text(x))` is legal
    (`Text(x)` is one level deep), `var token = Token(kind, source.slice(a, b))` is not.
  - Anything inside an argument counts, not only the argument itself: `counts.append(count_words(text) + 1)`,
    `print(names[index_of(name)])` and `print(first == Vector(1, 2))` put a call inside an argument, and only the
    last is legal (the one constructor level may sit inside an operator, a list or an index).
  - A method called on a call's result is not an argument: `source.slice(0, 2).upper_case()` is fine on its own,
    and an error only when it is itself passed to something.
  - A text with holes is not a call argument, and each hole is read like a line of its own:
    `console.print("{count_words(text)} words")` is legal, `console.print("{shout(count_words(text))}")` is not.
  - A call in an `if` or `while` condition, a `return`, an assignment or an index is not an argument.
  - A singleton's constructor is never an argument, nor anywhere but the whole value of a `var` (D110,
    [Singletons](#singletons--implemented)).
  - Open for Mortaro: D18's markup nests tag calls (`html.div({ class: "card" }, html.h1(title), ...)` in
    [Markup](#markup--planned)), which this rule forbids as written unless a tag counts as a constructor.
  - Where the call sat on the right of `and`/`or` or in a `while` condition, the compiler's own sources compute it
    before the condition only when that is harmless (a getter, `length()`, or `code_at`, which answers 0 past the
    end), and update it at the end of the loop body; nothing in the repository needed an `if` to keep a call from
    running.
- **An `if` and its `else` do not repeat the same work** (D78): when both branches compute the same thing,
  it is computed once before the `if`. In the example, both branches sliced the same range.
  **Implemented (2026-09-24) in a narrow form, (proposed by Claude, unconfirmed) beyond the example:** the error
  is "'source.slice(token_start, end_index)' is computed in both branches: compute it once before the 'if'".
  It counts a call with at least one argument to a function or method (not a constructor, whose name starts
  with an upper-case letter) that prints identically in every branch -- both branches of an `if`/`else`, or
  every branch of an `else if` chain that ends in `else` ("computed in every branch"). A chain where only some
  branches share the call is not reported, since computing it before the `if` would run it on paths that never
  did. Only the calls evaluated once and first thing in a branch count: the ones in the branch's statements
  up to its first nested `if` (whose condition counts), `while` or `switch`, not a call standing alone as a
  statement (it has no value to compute once), not the right side of `and`/`or`, and not a call that mentions
  a name the branch declared, assigned, asserted or called a method on before it, or a name the `if`'s
  conditions may narrow (tested for truth, or compared with `null` or a class). Names, literals, attribute
  reads and wider repetitions are not reported yet.

### Formatting

- 4 spaces per indentation level; never a tab. No trailing whitespace on any line. Exactly one newline at the
  end of the file.
- A `<...>` list holding a single named codegen value is rewritten to the positional form (section 9), so
  there is one way to write each call.
- **Implemented by the Spite compiler (2026-09-23)**, as `bin/spite format [--check] <file-or-folder>` (the
  compiler's own `format` command since D128), and `check.sh` requires every file outside
  `diagnostics/` to be formatted already. What it does today: 4-space indentation, one space around binary
  operators, the minimum parentheses (a receiver that is an operation always keeps them), `else if` on one line,
  a switch case with one short statement on its own line, a call or a signature wider than 120 columns broken
  one argument per line with trailing commas, floats as written, `: Nothing` dropped from a shape function,
  top-level link comments (the only comments D34 allows) kept before the declaration they preceded, and the
  blank-line rule for grouped `var`s. **Safety check:** the formatted text must lex, parse, print to the same
  fully-parenthesised program as the original, and keep every comment, or the file is left alone and the reason
  printed. **Every compile formats** the entry folder's files and every `load()`ed root once the whole program is
  read (not `library/`), printing `formatted <path>` for each file it rewrote and reading the program again when one
  was; nothing turns it off (D190). A file
  whose function body has an empty line is left alone, so D55's error still fires instead of the formatter
  quietly removing the line; a file the formatter refuses is left alone with its reason, as an error that stops
  the compile, so a program is never compiled from text that is not in the one style.
  `--final_classes` writes its classes already formatted. A list or object literal over 120 columns is written
  one entry per line with no commas. Every documentation program not marked `error` is formatted too, and `check.sh`
  keeps it so.
- **A file is ordered** (D67, decided by Mortaro, 2026-09-23): the `singleton` line (D104), `generic` lines
  (D87), enums, unions, types, variables, the constructor, then functions. Anything out of that order is a
  compile error naming what came before it (`diagnostics/declaration_order`). Where `union` goes was not said;
  beside `enum` and before `type` is Claude's placement (unconfirmed). Consecutive `generic` lines stay together
  with no blank line between them; one blank line follows the `singleton` line and the last `generic` line.
  **[implemented]**
- One blank line between declarations at file level, except that consecutive `var` declarations may stay
  grouped with no blank line between them if they were already written that way (a blank line the source did
  put between two `var`s is kept; one between any other pair of declarations is always exactly one, inserted
  if missing and collapsed if there were several).
- Inside a function/`if`/`while`/switch-case body: at most one consecutive blank line, and never one right
  after the opening `{`.
- K&R braces: `func name() {`, `if condition {`, `} else {`, `} else if other {` -- `else` always continues on
  the closing `}`'s own line, and a nested `if` in an `else` branch collapses onto one `else if` line whenever
  it does not itself carry a comment that would need its own line. An empty body prints as `{ }` on one line
  rather than an empty `{\n}`.
- One space around every binary operator (`a + b`, `a and b`) and after `:`/`,`; no space before `:`/`,`, and
  none just inside `(`/`)`/`[`/`]` (`f(a, b)`, `list[0]`, not `f( a, b )` or `list[ 0 ]`).
- The minimum parentheses needed to preserve meaning are kept (`1 + 2 * 3`, not `1 + (2 * 3)`); parentheses
  that only restated the language's own precedence are dropped, and ones that change it are always kept.
- An inline list/object literal uses `, ` between entries on one line; a multi-line one uses one entry per
  line with no commas (newlines already separate entries -- section 2). A list/object literal (or a function
  call's argument list) that would be longer than 120 columns on one line is broken into the multi-line form
  instead, one entry per line; short ones stay inline regardless of how the source originally wrote them (one
  true style, not "preserve what you typed"). `enum`/`union`/`type` bodies are always printed multi-line (one
  member per line), matching every example so far.
- Comments are `# text`, exactly one space after `#` (a comment with no text is just `#`). String contents are
  never reformatted -- whatever is between the quotes is untouched, escapes and all.
- **Lossless.** A leading (own-line) comment before a declaration/statement/list-or-object-entry/enum-value/
  union-member/type-field/switch-case, a trailing same-line comment after one, and whether a blank line
  preceded one are all attached to that AST node by the parser (`ast.Trivia`) and reproduced by the formatter;
  formatting a file never drops or reorders a comment. **Known limitation:** a comment is only ever attached at
  those specific points -- one written in the middle of an expression, an argument list, or with nothing
  following it before a closing bracket/brace (an "orphan" comment) is not tracked anywhere. The safety check
  below catches this rather than silently losing the comment.
- **Safety check.** After formatting, the formatter re-lexes and re-parses its own output and confirms, before
  ever writing a file: (a) the formatted-and-reparsed AST prints (via the older, comment-blind but
  round-trip-stable canonical printer) to exactly the same text as the original AST does, proving no code was
  added, dropped, or reordered; and (b) every comment's
  text appears, in the same order, in the formatted output. Either check failing refuses to write the file and
  reports an internal formatter error instead -- see the formatter.

### Comments  **[implemented]**

**A comment is one line, and it is nothing but a link to a markdown section** (D34, decided by Mortaro, 2026-09-20):

```
# notes.md#why-this-exists
func Game() {
```

- The link is a path to a markdown file plus a GitHub format anchor (lowercase, punctuation dropped, spaces to
  hyphens, `-1`, `-2` for repeated headings) and no other text. The path is resolved from the entry file's folder,
  the same rule `load` uses, so a local run and an automated one agree.
- The compiler checks that the file exists and has a heading with that anchor. A link that does not resolve fails
  the build, which is the point: prose in source rots into a lie, a link either resolves or stops you.
- A comment may only appear outside functions and declaration bodies. A line that seems to need explaining
  becomes a named function instead, and unlike a comment a name is visible to `functions`, to `--final_classes`
  and to every tool.
- `//` and `/* */` are recognised by the lexer only to be rejected with the explanation.
- Every one of these errors teaches: it states the one legal form, then asks whether the note is needed at all.
  If a reader could work it out from the code, delete it; if it is lasting knowledge, write the section first and
  link to it; if it warns against a change, a test or a compile error pushes harder than prose.
- Why not zero comments: finding out that nothing is there costs a search on every edit, which is the common
  case, while a link costs a few tokens only where there is something to say. Absence becomes free and
  trustworthy.

### Naming and abbreviations -- compile errors, not auto-fixed  **[implemented]**

A naming or abbreviation problem cannot be silently rewritten (renaming a symbol can change what a program
means to a reader who searches for its old name), so these are compile **errors** with `file:line:column` and
a suggested fix, from a linter that runs on the parsed AST of every class file, before codegen.
There is no `--no-lint`: the compiler already **is** the linter.

The compiler has no warnings at all (second batch item 6, decided 2026-09-19): it either reformats your code or
gives an error, nothing is left to the user's taste. The only printout that used to be labeled a warning -- the
`--development`-only "skipped class" notice (section 13) -- is plain informational `note:` text, not a warning.

- Variables, attributes, functions, and parameters: lowercase `snake_case`. Classes, `type`s, `enum`s, and
  `union`s: `PascalCase` (a class's own name is computed from its file name and therefore always correct; a
  function that happens to share its own class's name -- the constructor -- is exempt from the function-naming
  check for the same reason). Enum values: lowercase `snake_case`. File and folder names: lowercase
  `snake_case` (folder names were already checked this way since milestone 5; file names are new).
- **One underscore between words** (proposed by Claude, unconfirmed; implemented 2026-09-24): a snake_case name
  joins its words with one `_` each and may start with one `_` to be private, so `hit__count`, `__strike` and
  `strike_` are errors (`diagnostics/doubled_underscore`). The C the compiler writes for a class or function of
  its own -- `Pool___allocate`, `Pool___make`, `Pool___retain`, `Pool___release`, a singleton's `___destroy`, `Pool___init`,
  `Pool___default`, `Pool___class_of`, `Pool___attributes`, `Pool___functions`, `Pool___instances`,
  `Pool___deep_copy`, `Pool___read_<attribute>`, an enum's `___name`, a function's `___hot`, `___slot`,
  `___waiting`, `___perform` -- joins with `___`, which no Spite name can produce, so none of those words is
  reserved: a class may declare `allocate`, `make` or `release` (`conformance/stage6/generated_names`). A class's
  `copy()`, `to_string()` and `to_debug()` are Spite functions every class answers and a class may declare, and
  keep their plain names.
- **Names C reserves** cannot name a variable, attribute, parameter or function: `auto`, `bool`, `break`, `case`,
  `char`, `const`, `continue`, `default`, `do`, `double`, `extern`, `float`, `goto`, `inline`, `int`, `long`,
  `main`, `register`, `restrict`, `short`, `signed`, `sizeof`, `static`, `stderr`, `stdin`, `stdout`, `struct`,
  `typedef`, `unsigned`, `void`, `volatile`. The error lists all of them, so the next name tried is not another
  one (proposed by Claude, unconfirmed, 2026-09-24; `diagnostics/reserved_name`). Whether a function, whose C
  name is always joined to its class's, needs the list at all is open (`mortaros_missing_decisions.md` item 107).
- Single-letter names are always errors -- no exceptions -- because they read either as a stray leftover or as
  a puzzle for whoever reads the code next.
- **Abbreviations.** An identifier is split into `_`-separated words and each word is checked against a
  denylist, suggesting the identifier with every matching word spelled out in full. Kept in one place
  (one table in the linter) so it is easy to extend:

  | Abbreviation | Full word | | Abbreviation | Full word |
  |---|---|---|---|---|
  | `expr` | `expression` | | `pos` | `position` |
  | `stmt` | `statement` | | `prev` | `previous` |
  | `param` | `parameter` | | `cur`/`curr` | `current` |
  | `arg` | `argument` | | `max` | `maximum` |
  | `args` | `arguments` | | `min` | `minimum` |
  | `idx` | `index` | | `init` | `initialize` |
  | `ctx` | `context` | | `calc` | `calculate` |
  | `cfg`/`conf` | `configuration` | | `attr` | `attribute` |
  | `tmp`/`temp` | `temporary` | | `elem` | `element` |
  | `str` | `string` | | `char` | `character` |
  | `num` | `number` | | `int` (as a name) | `integer` |
  | `len` | `length` | | `bool` (as a name) | `boolean` |
  | `btn` | `button` | | `info` | `information` |
  | `msg` | `message` | | `spec` | `specification` |
  | `req` | `request` | | `env` | `environment` |
  | `res`/`resp` | `response` | | `db` | `database` |
  | `err` | `error` | | `auth` | `authentication` |
  | `val` | `value` | | `impl` | `implementation` |
  | `obj` | `object` | | `util`/`utils` | `utilities` |
  | `fn`/`func` (as a name) | `function` | | `lib` | `library` |
  | `var` (as a name) | `variable` | | `pkg` | `package` |
  | `ptr` | `pointer` | | `cmd` | `command` |
  | `src` | `source` | | `desc` | `description` |
  | `dst`/`dest` | `destination` | | `doc`/`docs` | `documentation` |
  | `dir` | `directory` | | | |

  `id` is explicitly **allowed** even though it is short, since it has no ambiguity and no natural longer form.
- The terminal-`if` lint (manual.md section 5: a function whose body's last statement is `if value { ... }` --
  no `else` -- wrapping the rest of the function only to check existence, which must be written with `assert`)
  now lives here too, moved unchanged from the generator -- checking a function's last statement
  needs nothing that only codegen's resolved types would provide, so this milestone's "if that is easy"
  condition held.
- A diagnostic names the file the problem is in (`registry.spite:3`), not the entry file.

## 13. Command line

```
spite program                         build program/program.exe beside the program and run it (section 3)
spite program --c_source              also write program/program.c (--c_path=path puts it elsewhere)
spite program --executable --run=false   build without running (--executable_path=path puts it elsewhere)
spite program --run=false             compile the whole program and write nothing
spite program --optimized             optimized build
spite program --development           an inspectable build: no tree shaking, internals as ordinary objects (D143)
spite program --hot_reload            swap changed classes into the running program (implies --development)
spite program --repl                  run with an in-place REPL
spite program --repl_port=4000        run with a remote REPL an AI can connect to, to explore memory and debug
spite program --serve=true            decide a Build field the program declares while compiling (section 9)
spite program -- --serve=true         run it with a setting its Environment declares (section 9)
spite program --final_classes=folder  write the final class folder
spite connect 4000                    talk to a running --repl_port program (see section 14)
spite connect 4000 --command="..."    send one REPL command, print its raw JSON response line, and exit
spite program -- ada --player=x       run it, passing everything after -- to the program's Arguments
spite program --c_source --run=false --target_operating_system=linux   write the C for another system (sections 9 and 11)
spite format <file-or-folder> ...     format files without compiling (recursive on a folder)
spite format --check <path> ...       rewrite nothing; exit 1 listing (to stdout) every file that would change
spite reload program ... --executable_path=<running>   what a --hot_reload program runs to rebuild itself
```

**Inspectable and production builds** (D143, decided by Mortaro; the readings below proposed by Claude,
unconfirmed; implemented 2026-09-25). A build with `--development`, `--hot_reload`, `--repl` or `--repl_port` is
*inspectable*: nothing is tree-shaken, and the singletons that hold nothing (`Memory`, `Build`, `TypedMemory<T>`,
section 8) are ordinary objects -- allocated at first use, listed by `.instances`, destroyed at exit -- so the REPL
and reflection see the internals as normal classes. Every other build, the ordinary one included and not only
`--optimized`, is a *production* build, where those singletons are static objects and everything unused is
tree-shaken. The optimisations that change only speed (chains as one loop, placement, text appended in place)
apply in both; `docs/optimizations.md` says which build each optimisation applies in
(`conformance/stage6/development_internals`).

Flags mix freely (an `--optimized` build may keep the REPL, and is then inspectable). Building and running are
the same command. Every flag is a field of `Build` (section 9), and a program is named only by its folder (D89,
D130): a path to a `.spite` file is an error naming the folder form, and the compiler compiles itself as `spite bootstrap`, whose entry is
`bootstrap/bootstrap.spite`, class `Bootstrap`.

**The whole program first, then the outputs** (D128, D129; the spelling below proposed by Claude, unconfirmed;
implemented 2026-09-25). The compiler reads the launcher, the library, the program's folder, its `build.spite` and
every `load` before it reads any option, and then decides what to produce from the program's resolved `Build`,
so a program's own `build.spite` can set every option. The one exception is `target_operating_system`, which the
launcher needs to know which folder of `library/` belongs to the program, so it is read from the flag alone. The
outputs are `Bool` fields, and every one that is on comes from the same compile: `run` (default `true`: build the
executable and run it), `executable` (build it without running), `c_source` (write the C), and `final_classes`, which stays the folder to write to
(`""` is off), since any folder inside the program would be read back as part of it. With every output off the
compiler still compiles the whole program and reports its errors. Tree shaking and every other whole-program
step run before any output is written.

- **Where outputs go** (D129): `executable_path` and `c_path`, each `""` by default, which means beside the
  program: `game/game.exe` (`game/game` for Linux and macOS) and `game/game.c`. A path option given on the
  command line for an output that is off is an error naming the missing flag. What describes an executable stays
  beside it wherever it goes: the `.crashes` map (section 6) and a `--hot_reload` build's `.reload_host`,
  `.reload_files` and `_reload_<n>` libraries. The one intermediate is the C the C compiler reads when `c_source`
  is off, written to the language repository's `.spite-cache/<name>.c`.
- **Formatting is an output** (D128): after the whole program is read, a file of the program whose formatted text
  differs is rewritten, and if any was, the program is read again from disk before anything else is produced, so
  what is compiled -- and every line an error names -- is the formatted file.
- **The compiler's own C goes to its default path**, `bootstrap/bootstrap.c`: every `Build` field is a constant in
  what is built, so a `--c_path` naming a different file each run would be written into the C and the fixpoint
  would never hold.
- `--mode`, `--output`, `--mode=tokens`, `--mode=tree` and the `.spite`-file forms of `--mode=format` are gone;
  `spite format` formats files itself, and `spite reload` is a command the running program uses, not an output.

**Where the compiler's flags end** (proposed by Claude, unconfirmed; implemented 2026-09-23): at the first bare
`--`. Everything after it reaches the program's `Arguments` verbatim -- `arguments.get(0)` is the first,
`arguments.player` reads `--player=...`, and `Environment` reads the settings it declares -- and none of it is
read as a compiler flag or a file to compile. A program's own `Arguments` stops at `--` the same way, which is the ordinary meaning of `--`.
Before this, a program run by the compiler received no arguments at all (`conformance/stage6/program_arguments`).

**Where a program runs** (proposed by Claude, unconfirmed; implemented 2026-09-24): in the folder `spite` was run
from. The compiler finds `launcher/` and `library/` from its own executable -- the first folder above it that
holds `launcher/launcher.spite` -- and never from the working directory, so `bin/spite` no longer changes
directory, and a relative path a program opens (`File`, `Directory`, a cache folder) is the caller's. The
launcher's `load` paths are relative to that folder, and the executable is built beside the program (D129), so
running a program leaves nothing in the caller's folder. Before this, every program ran with the repository as its working
directory (`check.sh` runs `conformance/stage6/working_directory` from another folder).

- Before the `--`, a `--name=value` sets the `Build` field of that name, and one that names no field is an error
  that shows both places a setting can belong -- `build.spite` to decide it while compiling, `environment.spite`
  and `spite program -- --name=value` to read it when the program runs -- so typos never pass silently
  (`diagnostics/unknown_compiler_flag`).
- **Automatic formatting (milestone 7a; since D128 an output decided after the whole program is read).** Every
  `spite program ...` compile formats every `.spite`
  file that belongs to the program -- the entry file's own folder (flat, matching section 11's discovery
  rule), plus every `load(...)`-ed root, recursively -- rewriting a file only when its formatted text differs
  from what is on disk, and printing `formatted <path>` to
  stderr for each one, then reads the program again when one changed. A file with a parse error is left untouched (the ordinary diagnostics still report it);
  a file the formatter's own safety check refuses to touch (see [Style](#12-style-implemented)) is left as it is
  and its reason is printed as an error, which stops the compile. Nothing turns this off (D190, which removed
  `--format=false` and the `format` field): a test input kept deliberately unformatted, like `diagnostics/`, is
  compiled from a copy by `check.sh`, so the formatting lands on the copy.
- `spite format <file-or-folder> ...` runs the same formatter standalone, without compiling, and is a command of
  the compiler itself (like `spite connect`), not an option: a file formats just
  itself, a folder recurses into every `.spite` file under it except `.spite-cache/` folders (no bundle/`load()`
  awareness -- every file found is formatted, unconditionally). `--check` rewrites nothing and instead lists (to stdout) every file
  that would change, exiting 1 if that list is non-empty (0 if the whole tree is already clean).

## 14. REPL and live reload  **[partial]**

Milestone 6a: a REPL that inspects and drives the *running* program, local (`--repl`) and remote
(`--repl_port`). **Status (2026-09-23), D72:** the REPL is written in Spite -- `library/read_evaluate_print_loop.spite` -- and
`runtime/spite_repl.h`, the C interpreter the subsections below were first designed around, is deleted. `spite
program.spite --repl` runs the entry constructor, then loops on `spite> `: `attributes` lists the entry instance's
attributes, `functions` its functions with their arguments, `help`, `exit`, and the command language below is built
for paths, assignment and calls (`conformance/stage6/interactive_loop`, `conformance/stage6/interactive_paths`):
`player.weapon.damage`, `program.monsters[1].name`, `player.health = 12` (through `set_health` when the class has
one), `player.job = 'knight'`, `add(2, 3)`, `monsters[0].roar()`, `monsters.count()`. The emitted `main` hands the
loop the entry instance as a `Spite.Attribute` named `program` before every command, and a `run` build runs the
program attached to the terminal instead of capturing its output. In a `--repl` build a `Spite.Attribute` stays
linked to the live value it describes, through four functions the compiler supplies (proposed by Claude,
unconfirmed): `value_attributes()` and `value_functions()` read the value's own attributes and functions (a
list's attributes are its elements, named `0`, `1`, ...), `assign(text)` writes a number, Bool, text or enum
value into it and answers whether it could, and a `Spite.Function`'s `call_with_text(arguments)` calls it with
literal arguments and answers its result as a `Spite.Attribute?` -- `null` when an argument is not one the loop
can write. Outside `--repl` they answer an empty list, `false` and `null`. **Status (2026-09-24):** `--repl_port`
and `spite connect` are built, in Spite, over `Socket` (section 15) through `DynamicLibrary` -- see their
subsections below; `docs/repl.md`'s worked session is replayed by `check.sh` against a real program. **Not built
yet:** walking a `Dictionary<T>` or a union, assigning a `T?`, a list element or a whole instance, the meta
commands `classes`, `describe`, `enums` and `memory`. D37's drain points are built (2026-09-24): the remote loop's
commands are answered on the program's thread where it waits -- see "Answered where the program waits" below and
section 15's "Concurrency". **Status (2026-09-24), milestone 6b:** live reload is built behind `--hot_reload`
(D111, D112) on Windows, with the Linux and macOS folders held to compiling -- see "Live reload and 6b" below.
Compiling and executing new Spite code typed at the prompt, and `Class.instances`, are not started.

### Reflection tables  **[planned]**

Emitted only when `--repl` or `--repl_port` is given (never for a normal build): for every class the
compiler actually emits, its qualified name, attributes (name, type name, C offset, kind), and every
callable function whose parameters are all scalar/String/enum -- an ordinary method, or an already-
*instantiated* Symbol-codegen function (`set_age`, ...; a template nothing calls is still never
instantiated, so tree shaking for templates is unaffected). Each reflected function gets a generated
uniform thunk, `(self pointer, parsed arguments) -> rendered String result`. Enums get their value
names; `List<T>`/`Dictionary<T>`/`T?`/a union get the element/member kind and C offsets needed
to walk them generically, all resolved with the real `offsetof`/`sizeof` operators at compile time, so
the interpreter (`src/runtime/spite_repl.h`) never has to reason about struct layout itself. D1 (milestone
9a): a class/`List<T>`/`Dictionary<T>`/`String` attribute (or a `T?` of one) is always itself a
pointer now, so every one of those is `via_pointer` (walking through it -- and through a `T?` of
one, which is just that same nullable pointer -- is always transparent); only a still-embedded scalar/
enum/union attribute (or a `T?` of one) is not.

A class itself was already emitted whenever reachable (unaffected by REPL mode); a class's own
*functions*, it turns out, were never tree-shaken to begin with (`classes.emitClassBodies` already
emits every function of a resolved class, called or not), so item 1 of the milestone -- "relax tree
shaking of functions of reachable classes" -- needed no separate mechanism.

### Command language

A small subset of Spite expressions, interpreted by `src/runtime/spite_repl.h` directly over the
reflection tables, rooted at the entry instance under the fixed name `program` (regardless of the
entry class's own Spite name):

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`, `program.settings["volume"]`,
  `program.target`. Walking through a non-null `T?` is transparent; a union shows its active
  member and walks straight into it.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, `program.player.set_age(3)` (any
  reflected function, including an instantiated Symbol-codegen one); `List<T>`'s `count()`;
  `Dictionary<T>`'s `count()`/`keys()`/`has(key)`. A call must be the last part of an expression --
  chaining a `.`/`[...]` after `()` is not supported this milestone.
- **assignment** of a scalar/String/enum literal: `program.player.age = 5`, `program.monsters[1].name =
  "rat"`, `program.player.job = 'knight'`. Always a raw field write; additionally, when a `set_<attribute>`
  function is itself reflected (i.e. some Spite code already called it, instantiating it), the REPL calls
  that instead of writing the field directly -- the same "attribute write goes through `set_<attribute>`"
  rule ordinary compiled Spite code follows.
- **meta commands**: `classes` (every reflected class's qualified name), `describe Engine.Renderer`
  (its attributes and functions with signatures), `enums`, `memory` (live allocation count/bytes from the
  debug allocator -- REPL modes always compile with it enabled, regardless of `--debug-memory`), `help`,
  `exit`.
- Printing a class instance shows `ClassName { attribute: value, ... }` one level deep: a nested class/
  union attribute prints as `ClassName {...}` (never expanded further); a list/dictionary attribute
  prints as `List<T>(count)`/`Dictionary<T>(count)` regardless of depth; a `T?` prints `null` or
  transparently passes through to its held value at the *same* depth (it is never itself a level of
  nesting).
- Every error is one line and never a crash: an unknown attribute names the ones that do exist, an index
  or key out of range says so, a scalar/String/enum-only operation on the wrong kind of value is a type
  mismatch, and calling a non-function is "not callable".
- (proposed by Claude, unconfirmed; built in `library/read_evaluate_print_loop.spite`) Where the above is silent:
  a path may leave out the leading `program.`, so `player.health` and `program.player.health` name the same
  value; text prints without quotes (`name: hero`); an assignment prints the value read back afterwards, so a
  `set_<attribute>` that refused it shows the old one; a call that returns `Nothing` prints nothing; `functions`
  prints `name(argument: Class, ...): Returns`; a text literal has no escapes yet.

### `--repl`  **[implemented]**

Runs the entry constructor normally; when it returns, instead of dropping the entry instance and
exiting, reads commands from stdin with a `spite> ` prompt until `exit` or end of input, then drops and
exits normally (memory balanced -- checked by its own end-to-end test the same way `--debug-memory`'s
own tests are).

### `--repl_port=<port>`  **[implemented]**

`repl_port` is a `Build` field (section 9), so the port is part of the build. Before the entry constructor runs, starts a background thread with a
TCP server bound to `127.0.0.1:<port>` **only** -- it never listens on any other interface, and there is
**no authentication**: anyone who can reach that port on that machine can read and mutate the running
program. This is a local debugging tool, not something to expose past `127.0.0.1`.

The line protocol is designed for an AI client: each request is one line of the command language; each
response is one line of JSON, `{"ok":true,"value":"...","type":"Int"}` on success or
`{"ok":false,"error":"..."}` on failure (values and errors are JSON-escaped strings; a multi-line value,
e.g. from `classes`/`enums`/`describe`, uses `\n` inside that one JSON string, never a real newline in the
wire bytes). The program keeps running (data races with it are accepted -- this is a debug tool) while
clients connect and disconnect one at a time; when the entry constructor returns, the process stays alive
serving the REPL until a client sends `exit`, at which point the server thread calls `exit(0)` directly
(there is no guarantee the main thread, possibly still inside the entry constructor's own `while` loop,
will ever unwind back to a clean `main` return, so this does not attempt one).

**As built (2026-09-24; the choices below proposed by Claude, unconfirmed):**

- **Spite, over `Socket`.** The server is `ReadEvaluatePrintLoop.serve` in `library/read_evaluate_print_loop.spite`,
  over the `Socket` class (section 15), whose operating-system members live in `library/windows/socket.spite`
  (`ws2_32.dll`), `library/linux/socket.spite` (`libc.so.6`) and `library/mac/socket.spite` (`libSystem.dylib`),
  reopened per D80. `Socket` has no way to bind anything but `127.0.0.1`, so "loopback only" holds by
  construction. `ws2_32.dll` is opened when the first `Socket` is made, so a program without a REPL never loads it.
- **The thread.** The one piece of C this adds is written by the compiler, only in a `--repl_port` build: a
  two-line thread entry, `spite_remote_loop_thread`, that calls `ReadEvaluatePrintLoop.serve` with the entry
  instance. Spite starts it and waits for it through the operating system's folder (`CreateThread` and
  `WaitForSingleObject` from `kernel32.dll`; `pthread_create` and `pthread_join` elsewhere). No `Thread` class is
  added to the library: D35 already says how a program is concurrent, and this thread belongs to the REPL.
- **Answered where the program waits** (D37, built 2026-09-24; proposed by Claude, unconfirmed). The socket thread
  only reads a command, hands it to the program's scheduler (section 15, "Concurrency") and waits for the answer.
  The scheduler answers it on the program's thread the next time the program waits -- `program.sleep`,
  `Console.read_line()`, a `File` or `Socket` read or write, a `Concurrent` wait, a `Parallel` join -- so a command
  sees the program between two steps, never in the middle of one, and "data races are accepted" above no longer
  applies. After the constructor returns, the program waits for nothing but commands. A program that never waits
  is answered at its loops' check points (D174, below); `docs/concurrency.md` replays a frame loop served between
  frames and a busy loop served between passes.
- **Every loop is a check point in a REPL build** (D174, decided by Mortaro; the mechanism proposed by Claude,
  unconfirmed).  **[implemented]** In a `--repl_port` or `--hot_reload` build, the generator ends every pass of every
  `while` in the program's own code (not `library/` or `launcher/`) with a call to `Scheduler.check_point()`, which
  on the scheduler's thread answers a pending command (the handover flag `answer_pending` already reads) and runs a
  pending reload. On any other thread -- a `Parallel`, a helper -- it does nothing. A loop that never waits is
  answered between two passes, so the old compile error for such a loop, and `diagnostics/remote_loop_never_waits`,
  are gone. A build without those flags has no check point: its C is byte for byte what it was (checked on a busy
  loop and `examples/dungeon`). `--repl` alone answers after the constructor returns, from the console, so it needs
  none. The cost where it exists is a call and two atomic loads per pass.
- **The port is part of the build**, like any flag the compiler folds (D84): `main` listens on it, on the main
  thread, before the constructor runs, so a client that connects any time after the program starts is served. A
  port another program holds stops the program before its constructor with `error: the REPL could not listen on
  127.0.0.1:<port>` and exit code 1.
- **The answer is shared with `--repl`.** `answer(command, program)` returns a `ReadEvaluatePrintLoop.Answer`
  (`succeeded`, `text`, `class_name`); the console loop prints `text`, and the socket loop writes it as JSON:
  `"type"` is the class of the value, empty for `help`, `attributes` and `functions`, and `Nothing` for a call
  that returns nothing. Every message the console loop prints as a complaint is `"ok":false` on the wire.
  JSON escapes are `\"`, `\\`, `\n`, `\r`, `\t` and `\u00XX` for any other control character.
- **`exit`** answers `{"ok":true,"value":"","type":""}`, closes the connection and calls `program.exit(0)`,
  which flushes what the program printed. It is handed over like any command, so the program stops at a wait
  first. When the constructor returns first, `main` serves commands until then. With `--repl` too, the console loop runs first, and after its `exit` the process keeps serving.
- `spite program --repl_port=4000` is the form, as for every `Build` field; a port outside 1 to 65535 is an
  error naming it.

### `spite connect <port>`  **[implemented]**

A tiny client built into the compiler binary itself, over the same `Socket` class: with no `--command`, an
interactive prompt that sends each typed line and pretty-prints the JSON response (`value (type)`, just `value`
when the type is empty or `Nothing`, or `error: message`); with `--command="..."`, sends that one command, prints
the raw JSON response line, and exits -- this is the form tests and AI clients use. Nothing listening on the port
is `error: nothing is listening on 127.0.0.1:<port>` and exit code 1. `check.sh` replays every ` ```wire ` block
in `docs/` this way (`scripts/docs_corpus.spite` writes it out beside its program): the program is built with
`--repl_port`, each `$ spite connect ... --command="..."` line is sent, and each answer must be the line written
under it, the program must end with exit code 0 after `exit`, and what it printed must be its ` ```output `.

### Live reload and 6b  **[implemented on Windows; the mechanism and the rules below proposed by Claude, unconfirmed]**

D111 (a change is seen through the operating system and only what changed is rebuilt) and D112 (`--hot_reload`, a
flag of its own) as built on 2026-09-24. `docs/repl.md` ("Live reload") is the user's page, and `check.sh` runs its
session against a copy of the program it edits. Still not started: compiling new Spite code typed at the prompt,
and `Class.instances`.

- **The flag.** `hot_reload` is a `Build` field (default `false`). It **implies** `--development` rather than
  requiring it, since a new version of a class may call a function nothing called before. It works without a REPL:
  the watcher still swaps, and each swap or refusal is one line on the program's error output (`spite: rebuilt
  Monster`); an explicit rebuild needs `--repl` or `--repl_port`, whose `reload` swaps in what changed and answers
  what it rebuilt, and whose `last_reload` answers what the last swap (the watcher's included) did. A build without
  it has no slots, no watcher and no reload code: `reload` answers `'reload' answers in a program built with
  --hot_reload, which swaps its code while it runs, and this one was not`, from a branch folded on the `Build`
  constant. As with `--repl_port`, a `--hot_reload` build swaps where the program waits and at each loop's check
  point (D174). `spite program --hot_reload` runs the
  program attached to the terminal, like a REPL build.
- **The swap mechanism: one slot per function.** In a `--hot_reload` build every function of the program's own
  classes (not `library/` or `launcher/`), and each such class's `_init` (its attribute defaults), is written as
  `<name>_hot`, with a function pointer `<name>_slot` holding it, and `<name>` becomes a one-line forwarder through
  the slot. Every call site, function value and REPL thunk still names `<name>`, so all of them follow a swap;
  library functions stay direct calls. Any other build is unchanged: direct calls and tree shaking. The cost is one
  indirect call that the C compiler cannot inline, about a nanosecond: 200 million calls to a one-line function
  took about 0.62 s instead of 0.44 s at `-O0` and 0.40 s instead of 0.18 s with `--optimized` (Windows, clang).
- **What the build records.** Beside the executable, `<program>.reload_host` lists every function the executable
  defines with its C prototype, its slots, its class ids in order and the C layout of every class and enum;
  `<program>.reload_files` holds a generation number and a hash of each of the program's files. The executable
  holds a table of its functions' addresses by name, the path of the compiler that built it and that compiler's
  options, so it must run from the directory it was built from.
- **Rebuilding: `spite reload <folder> <options> --executable_path=<program>`** (a command since D128, when
  `--mode` went; proposed by Claude, unconfirmed). A reload runs that compiler with those options, less the
  outputs, and prints its errors to standard output, where the running program reads them.
  It reads the whole program again (milliseconds), with the class ids seeded from the manifest so every class keeps
  the id its instances carry, and compares file hashes: nothing changed answers `unchanged`. The rebuilt set is the
  classes the changed files declare, plus every class whose code calls a function of the set that the running
  program has with another prototype or does not have at all (the running caller would still call the old one),
  repeated until nothing is added. It writes C only for the rebuilt classes' functions and what they reach that the
  running program lacks, reaching every other function through a pointer the library is handed when it is loaded,
  and compiles `<program>_reload_<n>.dll` (`.so`, `.dylib`). It prints `rebuilt A, B`, `removed A.f` when a function
  the running program has is gone, and the library's path.
- **Swapping at a drain point** (D37). The running program opens the library with `LoadLibraryA`/`dlopen`, the
  calls `DynamicLibrary` opens a library with, and calls its `spite_reload_bind`, which looks up each running
  function it uses by name (the allocator included, so the library allocates and frees through the program) and then
  re-points the slots of the rebuilt functions whose prototypes are unchanged, with an atomic store. `reload` is a
  REPL command, so it runs where the program waits; the watcher's signal is handled in the scheduler's idle, beside
  the REPL's commands. While a reload runs, waits block instead of suspending, so nothing else runs until the swap
  is done; the program waits for the compile (under a second for a small program). After a swap the file hashes
  are updated. A library is never unloaded: values it made, such as its text literals, may still be referenced.
- **What reloads.** A function body (the next call runs it; a call already running finishes the old one). A new
  function or a new class (the new code calls it; the REPL's `functions` and reflection keep what the program
  started with). A changed parameter list or return type (a new function: the rebuilt class and its callers are
  rebuilt). A deleted function keeps its last code for whatever still holds it, a function value or the REPL, and
  the answer names it. An attribute's default value (through the `_init` slot, for instances made afterwards).
- **What needs a restart: a class's attributes or enums.** When the layout of a class or enum the running
  program has differs, the reload is refused -- `the attributes of Monster changed, and the running program's
  instances were made with the old ones: restart the program to change a class's attributes or enums, or undo that
  part of the change to reload the rest` -- and the program keeps all of its code. Migrating instances by attribute
  name (new attributes taking their defaults) needs every live instance and every reference to it, which the
  program cannot find today; `mortaros_missing_decisions.md` asks which to build. A file that does not compile is
  refused the same way, with the compiler's error, so a save caught half-written is harmless.
- **Watching** (D111). Each operating system's folder reopens `HotReload` (`library/hot_reload.spite`) with
  `watch_folder` and `wait_for_change`: `FindFirstChangeNotificationA`, `WaitForSingleObject` and
  `FindNextChangeNotification` from `kernel32.dll` on Windows, `inotify` and `poll` from `libc.so.6` on Linux, and
  `kqueue`/`kevent` on the folder and each of its files from `libSystem.dylib` on macOS -- no polling. The watcher
  runs on a thread of its own, waits until 100 ms pass without a change, then sets a flag and wakes the scheduler.
  Only the program's own folder is watched, not the folders it `load`s; `reload` picks those up.
- **Windows' C runtime.** When the C compiler targets MSVC (its `-dumpmachine`), the program and its libraries are
  built against the C runtime DLL (`-fms-runtime-lib=dll`) so they share one heap and one standard output.
- **Untested:** Linux and macOS -- their watchers, `.so`/`.dylib` libraries and `Program.executable_path` (which
  the compiler uses to record itself) -- are held to compiling by `check.sh`, and are written the same way as
  Windows'. A crash inside reloaded code reports the library's own assert trace. A `Parallel` running a function
  whose slot is re-pointed finishes the old code.

## 15. Standard library  **[partial]**

D1 (decided by Mortaro, 2026-09-19): every value below is reference counted, exactly like a user class (section
10) -- a `String`/`List<T>`/`Dictionary<T>` is freed the moment its last reference goes, not by a single-owner
convention. See [Memory](#10-memory-partial) for the retain/release rules and the cycle caveat.

### String  **[implemented]**

An immutable value with a length (not a bare `char*`). A literal is static and never allocates; every other
`String` owns one buffer, freed once by its owner. **Its storage is Spite** (D108; the attribute names and the
constructor proposed by Claude, unconfirmed): `library/string.spite` declares `_bytes: Long` (the address of the
characters, from `Memory`, with a 0 after the last), `_length: Long`, `_section: Spite.Memory.Section` (`'heap'`,
or `'constant'` for a literal) and `_capacity: Long`, the compiler writes the C layout from them, and `length()`,
`code_at()`, `slice()`, `sum()`, `equals()`, `less_than()`, `greater_than()`, `drop()` (which frees `_bytes`) and
the in-place append are Spite functions reading them. `String(bytes, length)` makes a `String` that owns `length`
bytes at `bytes`, which `Memory.allocate_bytes` handed out with room for one more; it is what `Memory.text` and
`sum` end in, and anything that fills a buffer itself can use it. The right side of `+` casts toward `String` (every numeric
type, `Bool`, and enum all format to text -- see [Numeric types](#numeric-types-implemented-provisional) for
`Float`/`Double`'s shortest-round-trip printing); assigning a `String` to any numeric variable parses it,
defaulting to `0`/`0.0` on failure. `==`/`!=`/`<`/`>` compare by content, and (manual.md section 5's Operators
table) also work as `equals(other)`/`less_than(other)`/`greater_than(other)`; `+` also works as `sum(other)`.

**Building text in a loop is linear** (proposed by Claude, unconfirmed; D36's hidden optimisation, 2026-09-24).
`text = text + piece`, `text = "{text}{piece}"` and any longer join that starts with the variable it is stored
back into (`text = "{text}, {name}: {count}"`) append to that text instead of copying it: when the variable is
the only holder of its `String`, the buffer grows in place, doubling like a `List`'s, and otherwise (a literal,
or a text another name or a list also holds) it is copied once, with room to grow. So a string shared by two
names never changes under the other one, and a hundred thousand appends take 0.2 s instead of 7 s. It applies to
a local variable or parameter of type `String` when no piece mentions that variable (`text = "{text}{text}"`
copies, as before); an attribute is not appended in place, since a call among the pieces could reach it.
`conformance/stage6/text_building` pins it: 200 000 appends and the sharing cases allocate 29 times (the `Launcher` included; 32 until `Memory` stopped being an allocation and the digits of a number were placed in the frame, D108), against
600 044 when every append copied.

| Method | Result | Notes |
|---|---|---|
| `length()` | `Int` | |
| `is_empty()` | `Bool` | |
| `slice(start, end)` | `String` | clamped; `""` for an empty/invalid range |
| `character_at(index)` | `String` | `""` out of range |
| `code_at(index)` | `Int` | `0` out of range |
| `contains(text)` / `starts_with(text)` / `ends_with(text)` | `Bool` | |
| `index_of(text)` | `Int` | `-1` when absent |
| `replace(from, to)` | `String` | replaces every occurrence |
| `trim()` / `upper_case()` / `lower_case()` | `String` | |
| `split(separator)` | `List<String>` | an empty separator splits into single characters |
| `lines()` | `List<String>` | splits on `\n` |
| `to_tiny()` / `to_short()` / `to_int()` / `to_long()` | `Tiny` / `Short` / `Int` / `Long` | `0` on a value that does not parse |
| `to_byte()` / `to_unsigned_short()` / `to_unsigned_int()` / `to_unsigned_long()` | `Byte` / `UnsignedShort` / `UnsignedInt` / `UnsignedLong` | `0` on a value that does not parse |
| `to_float()` / `to_double()` | `Float` / `Double` | `0.0` on a value that does not parse |
| `sum(other)` / `equals(other)` / `less_than(other)` / `greater_than(other)` | `String` / `Bool` | the explicit call form of `+`/`==`/`<`/`>` |

### List<T> additions  **[implemented]**

On top of `append`/`prepend`/`count`/index-read/index-write (iterate with `while index < list.count() { }`, since there is no
`for`):

| Method | Result | Notes |
|---|---|---|
| `append(value)` | | adds `value` to the end |
| `prepend(value)` | | adds `value` to the front (index 0) |
| `get_at(index)` / `set_at(index, value)` | `T` | the explicit call form of `list[index]`/`list[index] = value` |
| `insert(index, value)` | | clamps an out-of-range index to the nearest end |
| `remove_at(index)` | | a no-op out of range |
| `remove_last()` | `T` | removes and returns the last element; the default when empty |
| `remove_first()` | `T` | removes and returns the first element; the default when empty |
| `first()` / `last()` | `T` | the default when empty |
| `contains(value)` | `Bool` | `Int`/`Float`/`Bool`/`String`/enum elements only |
| `is_empty()` | `Bool` | |
| `clear()` | | drops every element, keeps the buffer's capacity |
| `reverse()` | | in place |
| `join(separator)` | `String` | every element that becomes text: `String`, a number, `Bool`, an enum value |
| `filter_<member>()` | `List<T>` | a new list of the elements whose Bool attribute is true (section 8) |
| `count_<member>()` | `Int` | a Bool attribute: how many elements have it true (section 8) |
| `sum_<member>()` | `Int`/`Float` | adds that attribute up across every element (section 8) |
| `find_by_<member>(value)` | `T?` | the first element whose attribute equals `value` |
| `sort_by_<member>()` | `List<T>` | a new list sorted ascending by an `Int`/`Float`/`String` attribute |
| `each_<member>()` | | `T` a class: calls that zero-argument function on every element, mutating it in place |
| `map_<member>()` | `List<U>` | an `Int`/`Float`/`Bool`/`String`/enum attribute's values, one per element |
| `any_<member>()` / `all_<member>()` | `Bool` | a `Bool` attribute, true for at least one / every element |

A `<member>` is always the element's, never the calling class's (D148, section 8). A function of the caller is
passed as a value instead, for any `T`: `each(f)`, `map(f)`, `filter(f)`, `any(f)`, `all(f)`, `count(f)`,
`find(f)` (the first element `f` is true for, a `T?`), `sort_by(f)` and `sum(f)`, where `f` takes one `T` --
`names.each(say_hello)` calls `say_hello(name)` for each element.

Naming (second batch item 3, decided 2026-09-19): `add` does not say where, so it is `append` (and `prepend`);
`pop()` became `remove_last()`, plus `remove_first()` -- writing the old names is a compile error naming the
replacement.

### Dictionary\<T\>  **[implemented]**

String-keyed, insertion-ordered, and a hash table under the hood: `get`, `has`, `set` and `[]` take the same time
however many keys there are (see the decision log's hash-table row, proposed by Claude, unconfirmed).
`Dictionary<T>()` constructs one.

| Member | Result | Notes |
|---|---|---|
| `set(key, value)` | | replaces an existing key's value |
| `get(key)` | `T?` | |
| `has(key)` | `Bool` | |
| `remove(key)` | | |
| `count()` | `Int` | |
| `keys()` | `List<String>` | a view: a fresh list of the same keys |
| `values()` | `List<T>` | a view: a fresh list of the same values |
| `dictionary["key"]` (read) | `T` | the default when the key is absent |
| `dictionary["key"] = value` (write) | | same as `set` |
| `get_at(key)` / `set_at(key, value)` | `T` | the explicit call form of `dictionary[key]`/`dictionary[key] = value` |
| `each_<member>()` | | `T` a class: calls that zero-argument function on every value, mutating it in place |

Iterate values with `while` (`dictionary.values()`, or `dictionary.keys()` plus `dictionary[key]`), or use
`each_<member>()` when a class value just needs one of its own methods called on every entry. The whole member
template family works on a `Dictionary<T>` through its values, as it does on a `List<T>`.

`deep_copy()` is implemented for classes, lists and dictionaries. A `String` is shared rather than duplicated
because it is immutable; a union or a `type` shape is shared for now; a self-referring structure is still
unsupported (section 10).

### Heap\<T\> -- removed (D1, decided by Mortaro, 2026-09-19)

`Heap<T>` is gone. References are the default now (section 10), so a class/union containing itself recursively
just declares an ordinary attribute of type `T` -- no indirection needed, and no struct-layout cycle to guard
against (a class is always a heap object referred to by pointer, so a self-referential field is just a pointer
like any other):

```gdscript
union Expression {
    NumberExpression
    BinaryExpression
}
```
```binary_expression.spite
var left: Expression? = null
var right: Expression? = null

func BinaryExpression(left_expression: Expression, right_expression: Expression) {
    left = left_expression
    right = right_expression
}
```

Writing `Heap<T>`/`Heap<T>(value)` is a compile error naming this fix.

### System classes  **[implemented]**

Built-in classes, resolved and emitted the same way an ordinary `.spite`-file class is, with hand-written bodies
instead of a parsed one. Portable C underneath (`stdio`/`stdlib`, `_popen`/`popen`, and `#ifdef _WIN32` for
directory listing and process spawning).

| Class | Members |
|---|---|
| `File(path)` | `read(): String?`, `write(text): Bool`, `append(text): Bool`, `exists(): Bool`, `remove(): Bool`; bytes (names proposed by Claude, unconfirmed): `size(): Long?`, `modified(): Instant?` (the last write, from `GetFileAttributesExA` or `stat`), `read_bytes(position, count, address): Long?` (how many were read, from any position), `write_bytes(address, count): Bool`, `append_bytes(address, count): Long?` (where they start) -- `null` when the file cannot be opened; each call opens and closes the file; `_fseeki64`/`_ftelli64` on Windows so a position past 2 GB works; `write_from` joins `read_into` among the waiting calls |
| `Directory(path)` | `path: String`, `entries(): List<Directory.Entry>` (D93: every folder and file inside it, as `Directory` and `File` values whose `path` is joined to this one -- see below), `files(): List<String>` (names, sorted), `folders(): List<String>` (sorted), `exists(): Bool`, `create(): Bool` |
| `Process(command, arguments)` | `run(): Int` (exit code; `arguments` is a `List<String>`, each shell-quoted), `output(): String` (stdout+stderr merged, valid after `run()`) |
| `Program()` | `exit(code)`: exits the process immediately with `code` |
| `Clock()` | a singleton (names proposed by Claude, unconfirmed, 2026-09-24): `elapsed_nanoseconds(): Long` and `elapsed_milliseconds(): Long` from a monotonic clock with an arbitrary start, for measuring; `now(): Instant`, the wall clock as an exact instant (D127, [Time](#time-one-stored-instant-zones-for-presentation--implemented-on-windows-the-shape-proposed-by-claude-unconfirmed); it replaced `unix_milliseconds(): Long`). `library/clock.spite` with each system's reading in `library/windows|linux|mac/clock.spite` (`QueryPerformanceCounter`/`GetSystemTimeAsFileTime`, `clock_gettime`) |
| `Console()` | `print(...values)`, `write(...values)`, `error(...values)`, `flush()`, `read_line(): String?` -- see below |
| `Socket()` (proposed by Claude, unconfirmed) | `listen_locally(port): Bool`, `accept_client(): Socket?`, `connect_locally(port): Bool`, `read_line(): String?`, `write_line(text): Bool`, `close()` -- TCP on `127.0.0.1` only, which `--repl_port` and `spite connect` use (section 14) |
| `Concurrent(function)`, `Parallel(function)` (names decided, D133) | the handle stands in for what the function returned, and reading it is the wait (D134); `finished: Bool` never waits; dropping the handle waits for it -- see "Concurrency" below |
| `ThreadPool()` (proposed by Claude, unconfirmed) | the singleton the `Parallel`s run on (D135): `size(): Int` worker threads, `worker_index(): Int` (`-1` off the pool) -- see "Concurrency" below |
| `ThreadLocal<T>()`, `Lock()`, `ThreadSlot()` (proposed by Claude, unconfirmed) | one value per thread: `get(): T?`, `set(value)`; a lock: `while_locked(function)`, `lock()`, `unlock()`; the raw per-thread `Long` both are built on: `read()`, `write(value)` -- see "Concurrency" below |
| `DynamicLibrary(file_name, naming, header)` | every foreign function, constant and type of a native library -- see [Foreign libraries](#17-foreign-libraries-planned). `library/dynamic_library.spite` holds its `file_name` and `handle`, its constructor and `drop()`; opening, closing and finding a symbol are the compiler's reopening (D82) |
| `Memory()` | the floor every other type is built on (D98, D101, D108): `allocate_bytes`, `resize`, `free`, the typed reads and writes, `copy_bytes`, `text`, ... -- see "The floor, named" below. `library/memory.spite` is the `singleton` line and `text`, in Spite; every other function is the compiler's reopening, and where an allocation lives is the compiler's choice |
| `TypedMemory<$value_type>()` | `read_value(address, index)`, `write_value(address, index, value)`, `release_value(address, index)`, `value_bytes()`: values of any type in raw memory, reference counts kept right; what `List<T>` keeps its elements with, and what a container of your own uses (D98) |

`Console` is one of them and is a singleton (D52): `Console()` is the same instance everywhere, `Console` is an
ordinary class name rather than a reserved word, and `console.class.name` is `"Console"` like any other class.
It has `print(...values)` (each value's `to_string()`, separated by a space, with a trailing newline),
`write(...values)` (the same without the trailing newline), `error(...values)` (the same as `print` but to the
error stream), `flush()`, and `read_line(): String?` (one line from the input stream without its line break, null
only at the end of input with nothing read). The entry constructor returning normally is exit code `0`.

**Printing is `to_string()`** (D109, decided by Mortaro; implemented). `print`, `write` and `error` are Spite in
`library/console.spite`, taking `...values: List<Printable>`, where

```gdscript
type Printable {
    to_string(): String
}
```

Every number, `Bool`, `String` (whose `to_string()` answers itself), `Symbol`, enum value, `Spite.Class` (its
`.name`) and `Spite.Namespace` (its `.name_with_namespaces`) answers it, so everything that printed before prints
the same. A class of your own prints once it declares `func to_string(): String`, and passing one that does not is
the ordinary shape error, naming the function: `'Pet' does not fit type 'Printable': it has no function
'to_string'` (`diagnostics/print_without_to_string`, `conformance/stage6/printable_values`). Mortaro wrote the
member as `to_string: Spite.Function<String>`; a `type` writes a required function as `to_string(): String` today,
and which form a shape uses is open question 11. What stays the compiler's is only the floor: `_write_output(text)`,
`_write_error(text)` and `flush()` have no body in Spite, and `Prelude` supplies their C (`fwrite` and `fflush`),
as it does `Memory`'s.

As implemented (proposed by Claude, unconfirmed): printing a value costs what the call says -- the list of values
is a `List` like any variadic call's, a number goes through its box and its `to_string()`, and the text is written
with its length rather than up to its first zero byte. `conformance/stage6/text_building` and
`fused_chain_allocations` pin those allocations. A `crash` still writes its operands itself, through each value's
`to_string()`, because it reports on the way out of a program that is stopping. `flush()` no longer writes a line
break after flushing, which the special case it replaces did.

**`Console.debug` and `to_debug()`** (D109, decided by Mortaro: "each class has an automatic to_debug(): String
that returns something like `Class {attribute: value, other: value}` by nesting to_debugs"; Claude chose `debug`
over `print_json`; implemented). `debug(...values: List<Debuggable>)`, where `type Debuggable { to_debug():
String }`, writes each value's `to_debug()` separated by a space, then a line break. Every value answers it:

```text
Player { name: "hero", scores: [3, 7], bag: {"gold": 2}, partner: null, mood: 'calm' }
```

What follows is Claude's reading (proposed by Claude, unconfirmed):

- **A class shows its name and its attributes**, in declaration order, as `Name { attribute: value }`, and
  `Name {}` when it has none to show. An attribute that is a class is shown by *its* `to_debug()`, so a class that
  declares its own is shown by it wherever it appears. A `List` is `[a, b]`, a `Dictionary` is `{"key": value}`,
  text is quoted with `\"`, `\\` and `\n` escaped, a `Symbol` or enum value is written the way Spite writes it
  (`'calm'`), a number and a `Bool` as they print, and an absent `T?` is `null`. A `Spite.Class` is its name.
- **Private attributes are left out.** The walk is the plural attribute template (section 8) run from
  `Spite.DebugInstance`, and a plural over another class's attributes now ranges over the ones that class lets
  others read: a `_` attribute is its own business, and reading it from outside would be the ordinary private
  error. `Json` follows the same rule.
- **A cycle ends at an object already being shown**: it is written `Name {...}`, so `first.next.next` pointing
  back at `first` shows `Node { value: 1, next: Node { value: 2, next: Node {...} } }`. Each class keeps the
  objects it is in the middle of showing, compared with `==` (identity, unless the class defines `equals`), and
  a tree of distinct objects is shown whole however deep it goes.
- **It is Spite, and it costs nothing unused.** `library/spite/debug.spite` (`Spite.Debug<$value_type>`) turns any
  value into its text, and `library/spite/debug_instance.spite` (`Spite.DebugInstance<$value_type>`) walks a class
  instance. The generator's whole part is that asking a class for a `to_debug()` it does not declare answers one
  that calls them, created only when something asks, so a program that never debugs a class compiles nothing for
  it. A `type` or union that does not require `to_debug()` still answers it, dispatched to the class the value is.
- **The REPL keeps its own display** (`Player { name: hero, ... }`: text unquoted, nested objects as `Name {...}`,
  lists as `List<String>(...)`), because it reads a running program through `Spite.Attribute`, whose values are
  already text, and the documented sessions depend on it. Proposal: make it `to_debug()` of the value once
  reflection can hand the loop a typed value (`mortaros_missing_decisions.md`).

`conformance/stage6/debug_values`, `docs/standard_library.md`.

**A directory is navigated through its entries** (D93, decided by Mortaro, 2026-09-24).  **[implemented]**
`Directory` has a `path` exactly as `File` does, and `entries()` answers a `List<Directory.Entry>`, where
`Directory.Entry` is the union of `Directory` and `File` declared in `library/directory.spite`. Each entry's `path`
is its parent's joined with its name, so a `switch` tells the two apart and a folder is walked by calling the same
function on it again (`conformance/stage4/directory_entries`, `docs/standard_library.md`):

```gdscript
func count_files(directory: Directory): Int {
    var total = 0
    var entries = directory.entries()
    var index = 0
    while index < entries.count() {
        var entry = entries.get_at(index)
        switch entry {
            Directory: total = total + count_files(entry)
            File: total = total + 1
        }
        index = index + 1
    }
    return total
}
```

What follows is Claude's reading (proposed by Claude, unconfirmed):

- **The name is `Entry`**, namespaced as `Directory.Entry`, because it is what a directory listing calls each of
  its items and it says nothing the class does not: `DirectoryEntry` would repeat the class it already lives in,
  and `Path` would claim a text value it is not.
- **Folders come first, then files, each sorted by name**; `.` and `..` are never listed. It is the order
  `folders()` then `files()` already give, so the three agree.
- **`files()` and `folders()` stay**: they answer names rather than values, which is what the compiler's own
  discovery wants, and each is one line over the same listing. With `entries()` they are redundant, and the
  proposal is to remove them once nothing in the repository reads names alone.
- Each operating system's folder still lists a directory its own way (D80); what it supplies is renamed
  `entry_names(want_folders)`, since `entries()` is now the public listing and Spite has no overloading.
- Found on the way: a reopened class that declares a `union` again, as `--final_classes` prints `Directory` back
  out, registered the union twice. A union declared again now replaces the earlier one, as a `type` already did
  (section 11's rule: the later declaration of a name wins).
### Concurrency: `Concurrent`, `Parallel` and hidden waiting  **[implemented on Windows; names and mechanism proposed by Claude, unconfirmed]**

D35 (no function colouring, join on drop), D37 (the compiler injects the REPL's drain points where the program
already waits), D99 (IO never blocks the program by the program's own hand) and D103 (async waiting and threads are
two things, and `Task` is too generic a name) are built as two classes and one scheduler. `docs/concurrency.md` is
the user's page for all of it.

- **`Concurrent(function)`** (`library/concurrent.spite`) runs a function value (D17, D39) on a **fiber** of the
  program's own thread, starting straight away; reading the handle is what waits for its value (D134, below) and
  `drop()` waits for it, so scope exit is a join point. It is for work that waits: IO, sleeps, database calls
  later.
- **`Parallel(function)`** (`library/parallel.spite`) runs one on the program's **thread pool** (D135, below), with
  the same reading and join on drop. It is for work that computes.
- The type is never written: `Concurrent(file.read)` is a `Concurrent<String?>`, worked out from the function's
  return (see the inference row in the decision log). A function that returns nothing gives a `Concurrent<Nothing>`,
  which has no value to read and is waited for by dropping it: keep such handles in a list, and clearing the list
  or leaving its function waits for all of them.
- **Every attribute of both classes is private** (`_work`, `_results`, `_state`, ...), and the one public member is
  `finished: Bool` (below). Nothing outside the class reaches the thread, the fiber or the work.

**A concurrent result joins on first use** (D134, decided by Mortaro; the reading below is proposed by Claude,
unconfirmed).  **[implemented]** There is no `wait()` and no `join()` any more: **the handle stands in for its
result wherever the result's type is expected, and the compiler inserts the join at each such use** (the first
one waits; the value is kept, so later ones do not). Exactly, a `Concurrent<T>` or `Parallel<T>` becomes its `T`:

- where it is stored or passed as a `T` -- a `var` whose type is written, an assignment, an argument (a variadic
  `Printable` one included, so `console.print(sum)` prints the value), a `return`, an element of a list literal;
- as an operand -- `+`, `==`, `and`, `not`, any operator -- and inside text, `"{sum}"`;
- as the receiver of a member the handle does not have: `greeting.upper_case()`, `greeting.length()`;
- as a condition: `if ready`, `while`, `assert`, `crash`; and when `T` is nullable, narrowing the handle narrows its
  value -- `if reading { use(reading) }`, `crash reading` -- because D63 narrows a name itself rather than a copy.
  The value is read once, into a hidden local, and the narrowed name reads that.

It stays a handle where a handle is expected (`List<Parallel<T>>.append(handle)`), in a `var` without a written
type (`var loading = Parallel(asset.load)` is the running work), and for the handle's own member, `finished`. A
`T` of `Nothing` is never read, so such a handle only joins on drop. Comparing two handles with `==` compares their
values; there is no way to compare the handles themselves (proposed: nothing has needed it). The compiler writes
each join as a call to the class's private `_result()`, which `library/concurrent.spite` and
`library/parallel.spite` declare in Spite; only a join the compiler inserted may call it.

**`finished` never waits** (proposed by Claude, unconfirmed; SlopEngine's loaders polled
`WaitForSingleObject(parallel.thread, 0)` themselves, which was Windows only and reached into a field).
`handle.finished` is `true` once the function has returned. On a `Parallel` it reads the job's state with one
atomic load; on a `Concurrent` it reads the flag the fiber sets as it finishes, so a fiber only makes progress when
the program waits somewhere (`while not reading.finished { program.sleep(1) }` is the polling loop). The same on
every system (`conformance/stage6/finished_polling`).

**The thread pool** (D135, decided by Mortaro; the shape below is proposed by Claude, unconfirmed).
**[implemented on Windows]** `library/thread_pool.spite` is a singleton, `ThreadPool()`, that the `Parallel`s share:

- **Size and start.** The first `Parallel` starts one worker thread for every core but one
  (`GetActiveProcessorCount`, `sysconf`), at least one; the program's own thread keeps the last core. It never
  starts another, and a program that makes no `Parallel` starts none. `size()` says how many; `worker_index()`
  answers `0` to `size() - 1` on a worker (read from the thread's identity, under the queue's lock) and `-1`
  elsewhere, for scratch memory kept per worker.
- **The queue.** One lock and two condition variables (`SRWLOCK` and `CONDITION_VARIABLE` on Windows,
  `pthread_mutex_t` and `pthread_cond_t` elsewhere, each system's folder reopening the class). A job is a
  `Spite.Function<Int, Int, Nothing>` with the two numbers it is given and the address of an 8-byte state its
  owner holds (queued, running, done). Workers take jobs in order; `finished` reads the state with an atomic load.
- **Joining claims.** Waiting for a job that no worker has taken yet takes it out of the queue and runs it on the
  waiting thread, so a job that starts and waits for another job -- a `Parallel` inside a `Parallel`, or a
  `parallel_each_` inside one -- cannot wait on workers that are all waiting for it. A job already running is waited
  for on the done condition. This is also the only waiting `ThreadPool.join` does, and it is the wait the scheduler
  wraps (below) instead of `Parallel.join_thread`.
- **Join on drop is kept.** The queue holds the job's function bound to a small `ParallelCall<T>` that owns the
  work and the result, never the `Parallel` itself, so dropping the last handle runs its `drop()` straight away,
  and that waits.
- **At exit** the pool is a singleton destroyed in reverse creation order (D142): it lets the workers finish what
  is queued, joins them and frees its lock. A `Parallel` whose `drop()` runs later finds its job done and does not
  touch the pool.
- **Hidden code (D147).** Starting a worker needs the address of a C function that calls the pool's private
  `_serve()`; that is the same pair of bodiless functions `Concurrent` uses, `entry_address()` and `address()`,
  moved from `Parallel` to `ThreadPool`, so the pool adds no new kind of compiler-supplied code. Everything else --
  the queue, the claim, the split -- is Spite.
- **Cost.** A `Parallel` makes about two dozen allocations, most of them the two function values (a `Spite.Function`
  is its own reflection object, D39), and no thread; the one-thread-per-call version it replaced made fewer
  allocations and one operating-system thread each. `conformance/stage6/thread_pool_reuse` runs a thousand
  `Parallel`s and checks every one ran on one of the pool's workers or on the thread that read it.
- **Not tested:** the Linux and macOS folders compile but have never run; a `Parallel` made on two non-worker
  threads at once before the pool has started (each could start it; the program's thread and a `Concurrent`'s
  helper are the only candidates).

**A value per thread, and a lock** (proposed by Claude, unconfirmed; SlopEngine keeps a command buffer per runner
through `TlsAlloc`/`TlsGetValue` and guards its entity ids with an `SRWLOCK` of its own).  **[implemented on
Windows]** Three small classes, each system's folder supplying the calls:

- **`Lock()`** (`library/lock.spite`): `while_locked(work: Spite.Function<Nothing>)` runs the function holding the
  lock, so the unlock cannot be forgotten; `lock()` and `unlock()` stay for a section that is not one function.
  `SRWLOCK` on Windows, `pthread_mutex_t` elsewhere; `drop()` frees it. Not reentrant.
- **`ThreadSlot()`** (`library/thread_slot.spite`): one `Long` per thread, `0` until written -- `TlsAlloc` or
  `pthread_key_create`. `read()`, `write(value)`; `drop()` gives the key back.
- **`ThreadLocal<T>()`** (`library/thread_local.spite`): a value of any type per thread. Its `ThreadSlot` holds
  each thread's position in a `List<T>` the `ThreadLocal` owns under its `Lock`, so values stay reference counted
  and are all released when the `ThreadLocal` is dropped, whichever threads set them and whether or not those
  threads still run. `get(): T?` is `null` on a thread that has not called `set(value)`.

The alternative considered was a value per pool worker (`worker_index()` into a list): no system call, but it
covers only the pool's workers, not the program's thread or a `Concurrent`'s helpers. `conformance/stage6/thread_locals`.

**The mechanism: stackful fibers, and a helper thread per blocking call.** A C target has no coroutines, so hidden
async/await has three honest implementations. A *state-machine transform* (what C# and Rust do) rewrites every
function that can reach a wait into a resumable object; it is the fastest per suspension, but the colour it hides
is still there inside the compiler -- every such function, and every caller up to the fiber's root, has to be
transformed, reference-counted locals have to move into the state object, and a suspension inside a
`List.each_`-style template or a foreign callback has nowhere to go. *Threads* for everything make every wait
cheap to write but every program multithreaded, which is exactly the hidden cost D36 forbids. *Stackful fibers*
(`CreateFiber`/`SwitchToFiber` on Windows, `makecontext`/`swapcontext` elsewhere) give each concurrent function its
own stack on the program's one thread: a function suspends wherever it is, with no transform and no colour, and
Spite code only ever runs on one thread at a time, so the program's state needs no locks. Their cost is a stack
per live `Concurrent` (reserved, not committed, on Windows) and a switch of about the cost of a function call. That
is the one chosen. Files cannot be waited on without blocking on any of the three systems, so a blocking call is
handed to a short-lived helper thread (what libuv does for files), and the fiber is parked until it returns.

**What the compiler writes.** Each operating system's folder names the calls that block, and the compiler knows
them by class and function: `Program.sleep`; `Console.read_line_into`, `File.read_into`, `File.write_text`,
`Socket.accept_handle` and `Socket.receive_into` (the one system call under `Console.read_line()`, `File.read()`,
`File.write()`/`append()`, `Socket.accept_client()` and `Socket.read_line()`); and `ThreadPool.join`, the wait
under reading or dropping a `Parallel`. In a
program that uses the scheduler -- one that makes a `Concurrent`, or is built with `--repl_port` -- each of them is
emitted under a `_waiting` name with a small wrapper in front: a sleep parks the fiber until its time, a blocking
call runs on a helper thread while the fiber is parked, and a `Parallel` join joins and then lets whatever is ready
run once. Every other program gets none of it: no wrapper, no scheduler, no fiber, the same C as before.

**Blocking is what the compiler picks when it is faster** (D99). The wrapper asks the scheduler first: when no
`Concurrent` is alive and no REPL is listening, or the caller is not on the scheduler's thread (a `Parallel`, a
helper, the REPL's socket thread), nothing else could run meanwhile, so the wrapper makes the plain blocking call.

**The scheduler** (`library/scheduler.spite`, a singleton; each operating system's folder reopens it with the
fiber, event and clock calls) keeps the ready fibers, the sleeping ones with their wake times and the blocking
calls in flight. When nothing is ready it answers the REPL's pending command, if any, then waits on one event that
the helper threads and the REPL's socket thread signal, with the nearest wake time as its timeout. Waiting forever
on nothing is a deadlock, and a crash.

**Soundness.** A program that starts a thread (a `Concurrent`'s helpers, a `Parallel`, or `--repl_port`) is compiled
with `SPITE_THREADS`: every retain and release is an atomic operation, and the `--debug-memory` table takes a lock.
Every other program keeps the plain counts. Spite code on the program's thread only ever changes hands at a wait,
so fibers need nothing more. D35's rule is checked for `parallel_each_` (below); what a `Parallel(function)`
touches is not, so two threads writing one field, or one writing a field another reads, is still the program's
mistake there -- and with reference-counted fields it can free a value another thread is reading.
Two smaller gaps, untested: a singleton's first use from two threads at once is not guarded (each could make
one), and a REPL client's `exit` while a helper thread is blocked reading the console may wait on the C runtime's
lock on that stream when the process exits.

**`parallel_each_`** (D135, decided by Mortaro; D35's race rule, which Claude proposed, is built as below and is
still unconfirmed).  **[implemented on Windows]** `list.parallel_each_update()` calls `update()` on every element,
split across the pool, and returns when all are done. The generator writes two functions on the list's class, as
it writes a fused chain (D105): `parallel_each_update_piece(first: Int, end: Int)`, a `while` over that range of
the buffer, and `parallel_each_update()`, which hands the piece function to `ThreadPool.run_pieces(piece, count)`.
That cuts the list into up to four pieces per thread (the workers and the caller), queues all but the first, runs
the first on the calling thread and joins the rest, claiming any no worker has taken. One allocation per call for
the pieces' states, one function value, and none per element. A list of fewer than two elements runs on the
calling thread. `filter_` steps before it fuse into the piece loop (`entities.filter_alive().parallel_each_update()`
is one pass with no list in between); a `map_` step is an error, since it reaches another object that several
elements may share.

- **The race rule, D35 as a compile check.** The member, every function of the element's class it calls
  (transitively, by name), and every `filter_` member in the chain may read and write only the element's
  attributes that hold a plain value -- a number, `Bool`, `String`, enum or `Symbol`, or a `T?` of one -- any
  singleton (the library's, and the program's own, which D183 makes safe below), and their own parameters and
  locals. An attribute holding an object, a list, a dictionary, a function value or a program's own singleton is an
  error naming the attribute, its type and the one-thread form (`diagnostics/parallel_reach`):
  `'parallel_each_follow' runs 'follow' on many elements at once, so it may reach only its own 'Boid' attributes
  that hold values, and its locals (D35): 'leader' holds a 'Boid?', which another element may share. ...`. A list
  of numbers or text has no members, so `parallel_each_` on one is an error naming `each_`.
- **What it cannot see:** one object listed twice runs on two threads at once; a local made from something the
  member was handed (it is handed nothing, so only through a library singleton such as `Memory`); a list of a
  `type` or union (the element's class is not known, so it is an error for now). D35's open questions -- shared
  state across threads and cross-element reads -- stay open.
- `conformance/stage6/parallel_each` runs ten thousand elements twice, filtered once, and a list of one and of none.

**A singleton a `Parallel` reaches takes a lock** (D183, decided by Mortaro; the fallback of D184's plan, the
details proposed by Claude, unconfirmed).  **[implemented]** In a program that makes a `Parallel` or runs a
`parallel_each_` pass, each function of a program singleton (not `library/`'s) that can change after it is made --
one of its functions assigns one of its attributes outside the constructor, or it holds an object, a list, a
dictionary or a function value -- is emitted as `<name>___unguarded`, and `<name>` becomes a wrapper that takes the
singleton's own lock around the call. The lock is reentrant by owner thread (a `_Thread_local` marker's address),
so a singleton calling itself does not take it twice. A singleton that never changes gets nothing, and a program
without `Parallel` gets no lock at all; a `--hot_reload` build is not guarded yet. Every call from anywhere locks,
not only the ones from a `Parallel`: telling them apart needs a whole-program walk that is not built. Not built:
D184's cheaper forms, and D183's check that such a singleton hands out only numbers, text, copies or other safe
singletons. `conformance/stage6/singleton_guard` (four `Parallel`s filling a program singleton's `Dictionary` while
the program's thread keeps writing to it). D179's rule for `Parallel(function)` itself -- only its own instance and
its locals -- is not checked; only `parallel_each_` is.

**Reads in a row overlap** (D134's IO half, decided by Mortaro: "all our IO classes should use it"; this reading is
proposed by Claude, unconfirmed).  **[implemented]** A `File` or `Socket` is not changed: the compiler does it at
the call site. When two or more statements in a row are each `var name = receiver.read()` on a `File` (or
`receiver.read_line()` on a `Socket`), with no written type, the receiver a name or an attribute path, no statement
naming a variable an earlier one declared, and the name never assigned again in the function, every one but the
last is compiled as `var name = Concurrent(receiver.read)`, and after the last each is joined (its private `_join()`)
before the next statement runs. Every later use of the name is an implicit join of a finished handle (D134 above),
so narrowing, printing and passing it are unchanged. Only reads, only side by side, and all finished before
anything else runs, so nothing the program does next -- writing one of those files, say -- can see a difference;
reads followed by other work, or a read into a name that is assigned later, stay where they are. Such a program
uses the scheduler, so it is compiled with `SPITE_THREADS`; a program without two reads in a row compiles as
before. `Directory` listing is not a waiting call yet (no helper thread), so starting it early would not overlap
anything, and it is left alone. `conformance/stage6/overlapped_reads`.

**Not built:** HTTP, cancelling a `Concurrent`, a `Concurrent` made on
a thread that is not the scheduler's (it runs on the spot instead), and running any of this on Linux or macOS,
whose folders are held to compiling.

### Pure Spite: dissolving the runtime  **[planned]**

D14 (decided by Mortaro, 2026-09-19): **the standard library being hand-written C is temporary, and the target
is pure Spite.** `src/runtime/spite_runtime.h` (722 lines) and `src/runtime/spite_repl.h` (1392 lines) exist
because the compiler needed a `String` before Spite could express one; they are a bootstrapping decision (logged
2026-09-19, "built-in classes with hand-written bodies"), not the design.

D6 already requires part of this: `Spite.Class` is "an ordinary standard library class with an ordinary
declaration", which cannot be true while the standard library is C.

What the current runtime is, and where each part goes:

| Lines | What | Becomes |
|---|---|---|
| ~400 | `String` methods, retain/release, number to text, `List<T>`/`Dictionary<T>` support | ordinary `.spite` sources -- pure algorithms over memory, expressible today |
| ~150 | `File`/`Directory`/`Process`/sleep | `DynamicLibrary` calls (section 17) -- exactly what the foreign function interface is for |
| ~130 | allocator wrappers and the `--debug-memory` live-pointer table | the table is Spite; `malloc`/`free` is the floor below |
| ~50 | float formatting through `snprintf("%g")` | a shortest-round-trip implementation in Spite, or an FFI call |

**The floor is a list of intrinsics, not a file.** A `String` needs allocation, allocation needs memory from the
operating system, obtaining it needs the foreign function interface, and naming a library there needs a `String`
-- so the bottom cannot route through the standard library. It resolves the way every self-hosted language
resolves it: five to ten compiler intrinsics that the compiler emits directly (raw memory in and out --
`mmap`/`VirtualAlloc` through the FFI on native, `memory.grow` as an instruction on wasm -- plus whatever the
emitted C needs before any Spite exists). Everything above that line is Spite.

- **Purity costs no performance here**, which is what makes it worth doing: the output is still C, so a
  Spite-written `trim()` goes through the same optimiser as the hand-written one. This is not a language
  rewriting its runtime in a slower language; it is the same machine code from a better source.
- It also delivers two things already wanted: standard library classes become reopenable (section 16 item 7) and
  inspectable in the REPL, because they stop being special.
- Order: the FFI (milestone 11) unlocks the system-call layer, self hosting (milestone 8b) makes it natural, and
  then the standard library moves file by file. `spite_repl.h` follows the same way -- it is an interpreter over
  generated tables, which is ordinary Spite once the reflection of milestone 10 exists.

**The floor, named** (milestone 15a; proposed by Claude, unconfirmed, 2026-09-23). With `DynamicLibrary` built
(section 17), the floor is small enough to list. Everything the runtime does today is either one of these, or
Spite above them:

1. **`Memory`**, a built-in singleton whose functions the compiler emits as single C expressions: `allocate_bytes(bytes:
   Long): Long`, `resize(address: Long, bytes: Long): Long`, `free(address: Long)`, `read_byte`/`read_int`/
   `read_long`/`read_double(address: Long, offset: Long)` -- and since 2026-09-24 `read_short`,
   `read_unsigned_short`, `read_unsigned_int` and `read_float` (proposed by Claude, unconfirmed), so a C struct's
   2-byte, unsigned and `float` fields need no `TypedMemory` -- the matching `write_*(address, offset, value)`, and
   `copy_bytes(from: Long, to: Long, bytes: Long)`. An address is a `Long`, as section 17 already says a handle is --
   there is still no `Pointer` type, and nothing outside the standard library needs `Memory` at all.
   **Since D82** `Memory` is `library/memory.spite` (its `singleton` line) plus these functions as the compiler's
   reopening, declared without a body. **Since D98/D101 it has sections** (built 2026-09-24; the names proposed by
   Claude, unconfirmed), and **since D108 the compiler chooses between them**: `allocate_stack_bytes` is removed,
   and an `allocate_bytes` is placed in the function's frame when the rule below proves the address never
   leaves it ("Placement", proposed by Claude, unconfirmed). `TypedMemory<$value_type>` puts values of any type in
   memory, and `List<T>` is written with it, so nothing about a container is the compiler's any more except its
   syntax (`[]`, list literals) and the few C paths listed in the containers row of the decision log.
2. **Text from memory**: since D108 this is Spite, not floor. `String` declares its storage (`_bytes`, `_length`,
   `_section`, `_capacity`) and its constructor `String(bytes: Long, length: Long)`, which takes bytes `Memory`
   handed out and writes the 0 after them; `Memory.text(address, length)` is three lines of Spite in
   `library/memory.spite` that allocate, copy and construct. `take_text` and `address_of` are gone: a `String`'s own
   functions read `_bytes`. Literals stay static data the compiler writes, as symbols already are (D70), in the
   `'constant'` section.
3. **The object header**: retain, release and the class id are code the compiler emits, not functions anyone
   calls, so they are part of code generation rather than a library. For `String` that includes two decisions
   only the header can make: a `'constant'` text is never counted, and `text = text + piece` grows the text in
   place only when its count is one (`SpiteString_append` asks, then calls `String._unshared()` and
   `String._append_in_place(piece)`, which are Spite).
4. **Entry and exit**: `main` is emitted by the compiler, and so is writing text out (`Console._write_output`,
   `_write_error`; D109 moved everything else about printing into Spite), so the floor also has
   `Console.flush()`: exiting through the C runtime's `exit` would drop what the program's own `stdout` still
   buffers, which is why `Program.exit` flushes first (found when every corpus program printed nothing).
5. **The C runtime through Spite, one operating system at a time** (D71 replaced Claude's `"c"` alias; D80
   replaced the one wrapper class per platform): each operating system's folder reopens the classes it changes
   and names its real file (`ucrtbase.dll`, `libc.so.6`, `libSystem.dylib`) in their own `DynamicLibrary`
   attributes, and what `File`, `Directory` and `Process` do on every system is written once in `library/`.
6. **On wasm**, `Memory.allocate` grows linear memory with `memory.grow`, and the C runtime library is the
   JavaScript host's, which is the web shim below.

That is the whole floor: `String`, `List<T>` and `Dictionary<T>` become Spite over `Memory`; `File`, `Directory`,
`Process` and `Program` become Spite over the C runtime, reached from each operating system's folder; the `--debug-memory` live table becomes Spite
over `Memory`; and float formatting is Spite over `Memory` (`library/number_text.spite`, 2026-09-24). What stays in C is exactly what the compiler emits, never a file someone maintains.

**Built so far, additively (2026-09-23):** items 1, 2's `Memory.text` and 5 -- `Memory` with `allocate_bytes`,
`resize`, `free`, the typed reads and writes and `copy_bytes` (`conformance/stage6/memory_floor`); the `"c"` alias
built beside it was removed by D71. **Moving onto it (D73):** `File`, `Directory`, `Process`, `Program` and `Console` are Spite in `library/`, over
`Memory` and the C runtime, and their C is deleted. Since D80 there is no `CRuntime` class in between: `library/file.spite`
holds what `File` does everywhere, and `library/windows/file.spite`, `library/linux/file.spite` and
`library/mac/file.spite` reopen it with the few functions that call the system's library (section 11), and the same
for `directory`, `process`, `program`, `console`, `string` (`to_double` through `strtod`) and `build`
(`target_operating_system`). `Console`'s `print`, `write`, `error` and
`flush` stay operations the compiler emits (they are variadic, and they must reach the program's own `stdout`),
while `read_line` is Spite over `fgets` on the console's own handle of the input stream. Only `library/windows/` runs
here; the Linux and macOS folders have the same functions with the same signatures, follow `dirent`'s layout on those
systems, and are held only to compiling (`check.sh` writes the compiler out with each) until `check.sh` runs there.
Found on the way: the `'windows'` naming rule is lossy -- Win32 abbreviates some names (`SetCursorPos`) and
not others (`GetFileAttributesA`, which the rule would call `GetFileAttrsA`) -- so the Windows files name kernel32's
functions with `'identity'`. Nothing has moved onto them yet: moving `String`, `File` and the rest waits on this
proposal being accepted, since that is where changing the floor later would cost a rewrite.
The `--debug-memory` live table is Spite too: `library/allocation_table.spite` (`AllocationTable`) keeps the
live allocations in an open-addressing hash set over `Memory`, each live object's class id in the slot beside
its address (forgotten on free, so the leak summary names only what is still alive, the same on every run), and
prints the report; the compiler emits the small functions `SPITE_MALLOC`, `SPITE_REALLOC` and `SPITE_FREE` call
under `--debug-memory`. What stays C, and why, is in the decision log's allocation-table row (proposed by Claude,
unconfirmed).
`List<T>` and `Dictionary<T>` are Spite as well: `library/list.spite` is a generic class over `Memory` (growth,
insertion, removal, `reverse`, `contains`, `join`, `copy`, and releasing its elements when it is dropped), and
`library/dictionary.spite` is a generic class over two lists and a hash index into them; `split` and `lines` moved into
`library/string.spite`. The syntax stays the compiler's (`[]`, list literals, `List<T>()`), mapped onto those
classes' functions. Per element type the compiler still supplies four one-line functions a generic class cannot
write, plus `deep_copy`; the list and the reasons are in the decision log's containers row (proposed by Claude,
unconfirmed). The `<member>` templates are Spite in `library/list.spite` too (D91, [Standard library
metaprogramming](#standard-library-metaprogramming-partial)).

**The web shim is the one honest exception, and its target is zero hand-written lines.** A file that runs in the
JavaScript virtual machine cannot be Spite by construction -- it is on the far side of a boundary Spite does not
own, and every language targeting the browser has one. What is achievable is that nobody writes it: the imports
come from `external js` declarations and the command-buffer drain loop is a switch over an opcode table, both of
which are data the compiler can emit. The source of truth stays Spite even though the artifact is JavaScript, so
nothing in the repository is written in a second language and nothing rots out of sync. Today that file would be
150-300 hand-written lines; generating it fully is a goal, not a day-one requirement, and it shrinks on its own
as wasm proposals land.

### JSON is reflection, not a library  **[implemented]**

D22 and D23 (decided by Mortaro, 2026-09-19), amended by D31: writing JSON is walking a value's attributes and
reading it is a series of symbol keyed writes, so JSON needs no machinery of its own: a `type` shape is the JSON
shape. It is written WITH the metaprogramming once the standard library lives in Spite, which is the other half
of [Pure Spite](#pure-spite-dissolving-the-runtime-planned). JSON is a format for talking to foreign systems; it is
not what Spite programs send each other (see [The wire format](#the-wire-format-planned)).

- Writing never fails at run time: a class that cannot be serialised is a compile error, the same check
  isomorphic classes already need. Only parsing can fail, and it follows the failure model: a parse that may not
  succeed returns a `T?`, and a variant that crashes exists for callers who want the report instead.
- Open: the names of that pair. Mortaro sketched `to_json`/`to_crashing_json`; Claude prefers
  `parse_json()`/`parse_json_or_crash()` (unconfirmed).

**`Json<T>`** (D95, decided by Mortaro, 2026-09-24: "json with a generic should be part of our standard library
using metaprograming to easily convert to and from json").  **[implemented]** `library/json.spite` is one generic
class, and every conversion it makes is a function the compiler writes from it for the types a program actually
uses, so a program that never names `Json` carries none of it (D42).

```gdscript
var json = Json<Order>()
var text = json.write(order)            # String: never fails
var read = json.read(text)              # Order?: null on text that is not an Order
var order = json.read_or_crash(text)    # Order: halts, naming the position and what was expected
```

- **What it is written with** is section 8 and section 9's metaprogramming and nothing else: `if $value_type ==
  List { }` chooses what a type becomes at compile time, `Json<$value_type.element_type>()` recurses into what a
  container holds, and a class is walked by a Symbol codegen template over `Symbol<$value_type>`, called for every
  attribute at once by its plural (`write_attributes`, `read_attributes`). Reading keeps a `JsonReader`
  (`library/json_reader.spite`), a cursor over the text holding the first failure.
- **What each type becomes:** `String` is text (`"`, `\` and control characters escaped, `\uXXXX` read back as
  UTF-8, surrogate pairs included); every number type is a number; `Bool` is `true`/`false`; an enum is its value's
  name as text; a class is an object with one key per attribute in declaration order; `List<T>` is an array;
  `Dictionary<T>` is an object; `T?` is `null` or what `T` becomes. Nested classes, lists of lists and
  dictionaries of classes are the same rules again.

What follows is Claude's reading where D22 and D95 are not specific (proposed by Claude, unconfirmed):

- **The API is a class you make once per type**, `Json<Order>()`, with `write`, `read` and `read_or_crash`. The
  generic is on the class because that is where Spite puts generics, and naming the type once at the top reads
  better than naming it on every call. The pair D22 asked for is `read`/`read_or_crash`, following the suffix
  Claude proposed for `parse_json_or_crash`; `write` needs no pair, since it cannot fail.
- **Reading input is strict about what it cannot use and lenient about what it does not need** (section 5's
  three outcomes). A key the class does not have is skipped, whatever it holds, because foreign systems send more
  than a program asks for. An attribute the text does not mention keeps the default its class declares, which is
  the value section 4 already gives anything never set. A value of the wrong kind -- text for a number, `null` for
  an attribute that is not a `T?`, an enum name the enum lacks, a missing brace, anything after the value -- makes
  `read` answer `null`: the object would be wrong, and a wrong object that looks right is the surprise D27 exists
  to prevent. `read_or_crash` crashes on the same inputs, and its crash line carries
  `failure=expected <what> at character <n>`.
- **A number reads into whatever number type the attribute has** through the ordinary text-to-number cast, so
  `3.7` read into an `Int` is `3`; JSON has one number type and the class already says which one it wants.
- **Not handled yet:** a `Float` or `Double` holding infinity or not-a-number is written as `inf`/`nan`, which is
  not JSON; a `Symbol` attribute (as opposed to an enum) does not read, since text becomes a `Symbol` only through
  `Symbol(text)` (D70); and `--final_classes` does not print the functions `Json<Order>` generated, because a
  generic class's file is shared by all its instances.

`tests/json_tests.spite`, `conformance/stage6/json_crash`, `docs/json.md`.

### Time: one stored instant, zones for presentation  **[implemented on Windows; the shape proposed by Claude, unconfirmed]**

D127 (decided by Mortaro): the best date and time support there is, so that Spite never repeats JavaScript's
`Date` and the wrappers it bred, with **one stored format -- an exact instant -- and time zones only a
presentation layer**, copied from the best modern design. `docs/time.md` compares `Temporal`, `java.time`,
`jiff`/`chrono`, NodaTime and Go's `time` and argues the choice. Everything below is Claude's proposal
(`mortaros_missing_decisions.md` items 115-122): NodaTime's model with Temporal's vocabulary, and no stored zoned
type.

| Class | What it is |
|---|---|
| `Instant(since_1970: Duration)` | a point on the universal time line; `+ Duration`, `- Instant` (a `Duration`), `==`, `<`, `>`; prints in UTC with `Z` |
| `Duration(amount, unit)` | exact time, whole seconds and nanoseconds; units `'nanoseconds'` to `'hours'` and never days; `total(unit)`, `part(unit)`, `+`, `-`, `* Long`, unary `-`, `==`, `<`, `>` |
| `Period(amount, unit)` | calendar time, years, months and days (`'weeks'` is seven days); `+`, unary `-`, `==`, and no `<` |
| `LocalDate(year, month, day)` | a proleptic Gregorian date with no zone; `+ Period`, `weekday`, `day_of_year`, `days_in_month`, `is_leap_year`, `days_since_1970`, `days_until`, `period_until` |
| `LocalTime(hour, minute, second, nanosecond)` | a clock reading with no date and no zone |
| `LocalDateTime(date, time)` | both, with no zone; `+ Period` |
| `TimeZone` | `to_local(instant)`, `to_instant(local, ambiguity)`, `to_text(instant)`, `offset_at(instant)`, `name` |
| `TimeZones()` | the database, a singleton: `find(name): TimeZone?`, `utc()`, `fixed_offset(duration)`, `system()`, `read_tzif(name, data): TimeZone?` |
| `TimeText()` | ISO 8601 (RFC 3339, RFC 9557), a singleton: `read_instant`, `read_local_date_time`, `read_local_date`, `read_local_time`, `read_duration`, `read_period`, each answering `T?`; writing is each type's `to_string()` |

- **A local reading never becomes an instant without a zone.** No local type has a function answering an
  `Instant`, `==` between the two kinds is a type error, and `zone.to_instant(local, ambiguity)` is the only
  bridge. `TimeText.read_instant` refuses text without an offset, and `read_local_date_time` refuses text with one.
- **Exact and calendar lengths never mix.** A `Duration` has no days, since a day is 23 to 25 hours where clocks
  change; a `Period` has no hours and adds only to the local types, months first (a day past the new month's end
  becomes its last day: January 31 plus a month is February 29 in 2024), then days. `period_until` is
  `java.time`'s `Period.between`.
- **Every gap and overlap is resolved by a rule the call names**: `'compatible'` (a gap resolves after it, an
  overlap to its first instant -- RFC 5545's rule, and `java.time`'s and `Temporal`'s default), `'earlier'` or
  `'later'`. The zone finds the offsets a day either side of the reading and keeps the ones that map back to it:
  both for an overlap, neither for a gap.
- **A value that cannot exist halts; text that cannot exist is `null`.** `LocalDate(2023, 2, 29)` and
  `LocalTime(24, 0, 0, 0)` crash, since a program that builds one from its own numbers has a bug (D24); reading
  text answers `T?`. A leap second in text reads as the second before it.
- **The database is the operating system's, and nothing is embedded.** Windows reads IANA zones through
  `icu.dll` (Windows 10 1903 and later), loaded only when a named zone is asked for
  (`library/windows/zone_calendar.spite`); Linux and macOS read the TZif files under `/usr/share/zoneinfo` in Spite
  (`library/tzif_reader.spite`: RFC 8536 versions 1 to 4 and the POSIX rule in the footer), and `system()` follows
  `TZ`, then `/etc/localtime`. `read_tzif` is the same reader for a file a program brings. **The Linux and macOS
  path is untested**: `check.sh` holds it to compiling (it writes `conformance/stage6/daylight_saving` for each
  system), and `conformance/stage6/zone_files` runs the TZif reader on Windows over three files written for the
  test and checked with Python's `zoneinfo`.
- **Clock** keeps `elapsed_nanoseconds()` and `elapsed_milliseconds()` for measuring, and `now()` answers an
  `Instant`.

Not built: calendars other than ISO 8601's, leap seconds, formatting patterns and localized names, zone
abbreviations. `conformance/stage6/instant_arithmetic`, `calendar_math`, `daylight_saving`, `zone_files`,
`fixed_offsets`, `time_text_round_trips` and `time_text_errors`; `docs/time.md`.

### examples/calculator  **[implemented]**

A recursive-descent arithmetic interpreter (`token.spite`, `lexer.spite`, `expression.spite` + one file per union
member, `parser.spite`, `evaluator.spite`, `calculator.spite`), reading its program from `program.txt` next to it.
Supports `+ - * /`, unary `-`, parentheses, variable assignment (`x = 1 + 2`) and reference, one statement per
line. The lexer scans `source.split("")` with a `while` loop (accumulating each token's start/end index as a class
field, sliced out on a token boundary); the parser is genuinely recursive, including precedence climbing written
as tail recursion (`parse_expression_rest`/`parse_term_rest`), a style that reads well independently of `for`
being gone. Every place the parser hands a freshly parsed operand to `BinaryExpression(...)` does so inline
(never through an intermediate `var`): `left_expression`/`right_expression` are ordinary by-reference parameters
now (D1), stored directly into the union-typed field with no `Heap<T>` wrapper needed.

## 16. Decided by Mortaro, being implemented  **[planned]**

Moved here from `mortaros_notes.md` on 2026-09-18. Each item moves into its proper section above once
implemented. Items 2-5 moved out on 2026-09-19 (assert narrowing into section 5, Operators into a new section
5 subsection, the `Spite` namespace into section 8, numeric types into section 4); item 1 moved out on
2026-09-19 too (the formatter/linter, into the new [Style](#12-style-implemented) section). Item 6 (reference
semantics) moved out on 2026-09-19 -- decided (a) (D1) and implemented in milestone 9a; see [Memory](#10-memory-partial).
The second batch moved out on 2026-09-20 with milestone 9c: item 1 (running a script loads its parent folder)
into section 11, item 2 (`do` removed, plain-`if` narrowing) into sections 2/5/6, item 3 (no ambiguous standard
library names) into section 15, item 4 (variable shadowing) into section 4, item 5 (unused is an error) into
section 5, item 6 (the compiler has no warnings) into section 12, item 8 (`count()` vs `sum_<member>()`)
into sections 8 and 15. What remains from the second batch is item 7 (renumbered 1 below).

From `mortaros_notes.md` on 2026-09-19 (second batch):

1. **Standard library classes can be reopened** like any other class: the way to try out a package before upstreaming it.
2. **Comprehensive, Ruby grade reflection, at compile time.** Every object gets its reflection from source that Spite
   generates and that is VISIBLE in the final class output: `attributes` returning a `List<Spite.Attribute>`, `class`,
   `functions`, and so on are real generated members, not compiler magic. The goal is to cover most of what Ruby
   metaprogramming covers (enumerate and call functions by Symbol, respond-to checks, defining members from data,
   hooks when a class is reopened) resolved at compile time. Needs a design pass: see PLAN.md milestones 10a/10b.
3. **Errors: there are none to handle, only `crash`.** (third batch, 2026-09-20.) The main author of Spite code is an LLM, and
   bubbling errors up for someone to eventually log only makes code defensive. So there are no exceptions and no result
   types. Things that can simply not work return the default or a `T?` and are handled with `assert`.
   Things that stop the program from functioning call `crash("message")` (name decided by Mortaro: it is clear the program
   crashes and the AI needs to recode something). A crash stops the program with a report written for an LLM: the message,
   the Spite stack trace with `file:line`, and the attribute and local values of each frame, as text and as JSON in
   `.spite-cache/crash.json`. The same word works at compile time: lint rules and metaprogramming can `crash` the
   compilation when code does not follow the required structure, so structure is enforced instead of suggested.
   (proposed by Claude, unconfirmed: development builds log every `assert` that returned early, visible in the crash
   report and the REPL, so silent defaults stay debuggable; compile errors get a `--diagnostics=json` form with a rule id
   and the suggested fix.)
4. **The compiler is also the language server**, so editors get real time validation. After the bootstrap, written in Spite.
5. **Bootstrap as soon as possible** to start the repository; fancy features wait until after.

## 17. Foreign libraries  **[partial]**

**Built (milestone 11a, 2026-09-23):** `DynamicLibrary(file, naming, header)` with the three naming rules, calls
of any function the library exports, the import table of exactly the called symbols resolved once when the
library opens, and a program-stopping message naming the file, or the symbol and the Spite function that wanted
it, when either is missing (`conformance/stage6/foreign_library`, `diagnostics/foreign_library_mistakes`,
`diagnostics/foreign_call_mistakes`). What crosses is the table below, minus structs and lists. Choices Claude made
while building it (proposed, unconfirmed): one library per distinct file and naming rule, following D8's
"one instance per literal argument list", opened on first use and closed at exit; the file is named exactly as it is on disk and a name without an
extension is a compile error (D71 -- a wrapper for another platform names that platform's file), which `check.sh`
meets by building each fixture's C into `fixture.dll` beside its program on every platform; `_as_long` beside
`_as_double` and `_as_text`, since a plain call returns a 32-bit `Int` and a handle or pointer needs 64; the
naming rule is a symbol literal (D70); a foreign function is called only through the attribute or variable that
holds its `DynamicLibrary(...)`, so the compiler knows which table binds it; and `--final_classes` writes no
resolved-name comments, which D34 would reject (how to show them is open question 10). A constant reads from the header (`user32.mouseeventf_leftdown` is `MOUSEEVENTF_LEFTDOWN`, an `Int`); a header path
that exists relative to the working directory is included as a file, anything else as a system header. A `List<T>` of numbers crosses as its element array (C may write into it), and a value of a `type` whose attributes are all numbers crosses by address as a C struct
in its declared order, and C's writes come back into the value afterwards; under the `'windows'` rule with a header,
`_Static_assert` checks the layout against the header's struct, named `PointPair` -> `POINT_PAIR` (Claude: only
there, since other headers do not name structs that way, and a missing name would be a C error). Not built:
`Type.size`, reading a header's types as Spite reflection, `missing_function`/`missing_attribute` as reopenable Spite, and a
user-written naming rule (11c).

D4 (decided by Mortaro, 2026-09-19): **a native library is a class, not a keyword.** There is no `external`
keyword, no per-symbol binding string, no generated-binding step and no hand-written C shim. `DynamicLibrary`
is an ordinary standard library class, and its Symbol codegen (section 8) resolves every foreign function,
constant and type at compile time.

```mouse.spite
var user32 = DynamicLibrary("user32.dll", 'windows', "windows.h")

func move_to(x_position: Int, y_position: Int): Bool {
    return user32.set_cursor_position(x_position, y_position) != 0
}
```

`set_cursor_position` becomes `SetCursorPos`. Nothing in that file is spelled the way C spells it.

### What a member of a library means

Spite already forces PascalCase on every type and snake_case on everything else, and the linter (section 12)
enforces it, so the language's own naming rule is what tells the compiler which kind of foreign symbol is being
asked for. No annotation is needed, and none is accepted.

| Written | Is | Emits |
|---|---|---|
| `user32.send_input(...)` | a call | `SendInput(...)` |
| `user32.input_mouse` | a read, snake_case | the constant `INPUT_MOUSE` |
| `user32.Input` | a read, PascalCase | the C type `INPUT` (reflection only -- see Structs below) |

A constant or a type is only available when the constructor was given a header; without one a library exposes
functions and nothing else, and a constant read is a diagnostic naming the missing header.
`DynamicLibrary`'s own members (`file_name`, `handle` and its functions) are never foreign symbols, as the
reflection members never are (proposed by Claude, unconfirmed, 2026-09-25): a `List<DynamicLibrary>` built its
member templates over them the moment a program read any class object's `.functions`, and `sort_by_handle` failed
inside `library/list.spite` as a constant read (`conformance/stage6/settings_and_libraries_reflected`).

### The class

`DynamicLibrary(file_name, naming, header)`. Every argument must be a literal (the same rule `load(...)` already
has, section 11), because the compiler reads them during codegen.

```dynamic_library.spite
enum Naming {
    'identity'
    'windows'
    'camel_case'
}

var _file_name = ""
var _naming = 'identity'
var _handle = 0

func DynamicLibrary(file_name: String, naming: Naming, header: String) {
    _file_name = file_name
    _naming = naming
    _handle = _open(file_name)
    assert _handle
}

func symbol_name(symbol: Symbol): String {
    if symbol.kind == 'constant' {
        return symbol.name.upper_case()
    }
    if _naming == 'windows' {
        return symbol.name.abbreviated().pascal_case()
    }
    if _naming == 'camel_case' {
        return symbol.name.camel_case()
    }
    return symbol.name
}

func missing_function(function: Symbol, arguments: Arguments): Int {
    var name = symbol_name(function)
    return _call(_handle, name, arguments)
}

func missing_attribute(attribute: Symbol): attribute.class {
    var name = symbol_name(attribute)
    return _resolve(name)
}

func drop() {
    _close(_handle)
}
```

`_open`/`_call`/`_resolve`/`_close` are compiler intrinsics; everything above them is ordinary Spite, and
`--final_classes` prints the whole class with every generated binding, exactly as section 16 item 2 requires of
all reflection.

**What is built** (2026-09-24, D81/D82): `library/dynamic_library.spite` is the `singleton` line, `file_name` and
`handle`, a constructor that stores the file and calls `open_library(file)`, and a `drop()` that calls
`close_library(handle)`; `open_library`, `close_library` and `find_symbol(name, wanted_by)` are the compiler's
reopening, declared without a body. The naming rule and the calls themselves are still the compiler's
(`missing_function`/`missing_attribute` are not built), and every call through a `DynamicLibrary` value is a
foreign call, since `remove` and `exit` are names both a class and a C library could have.

- **`missing_function` and `missing_attribute` are the only two new names in the entire foreign function
  interface.** They are Ruby's `method_missing` resolved at compile time: section 8's rule ("a parameter of class
  `Symbol` whose name is a segment of its function's name turns the function into codegen for every name that
  fits") taken to its limit, where the segment is the whole name. They get reserved names rather than falling out
  of the segment rule so that a class opts in by defining them, instead of silently swallowing every misspelled
  call. Section 8's existing precedence is unchanged: a function with the exact name always wins.
- `missing_attribute` returning `attribute.class` is the same form as section 8's `get_attribute(attribute:
  Symbol): attribute.class` -- the return type is whatever that symbol turns out to be: an `Int` for a `#define`,
  a `Spite.Class` for a type name.
- **The naming rule is a function, not a table.** Three rules cover a whole library, and binding one more symbol
  is one more call site, never a second line. `'windows'` runs `abbreviated()` before `pascal_case()`, so
  `set_cursor_position` -> `set_cursor_pos` -> `SetCursorPos`: the linter's abbreviation table (section 12, "kept
  in one place so it is easy to extend") read backwards *is* the Win32 name generator. The table that makes C's
  spellings illegal in Spite is the table that translates back into them.
- **Constant reads are the C spelling, lowercased**, with no prefix rule -- `user32.mouseeventf_leftdown`. This is
  deliberate: C constant prefixes (`MOUSEEVENTF_`, `INPUT_`, `WM_`, `SW_`) follow no rule any function could infer.
  Give them Spite names once, in a `var`, and the ugly spelling stops there.

### Structs

A `type` (section 7) whose fields are all scalars is a C struct when it reaches a foreign call: passed by
address, never as a Spite heap object. There is no annotation -- the value's type comes from the `var`'s own
annotation or from the parameter it feeds, exactly like any other object literal matched by shape.

```mouse.spite
type Input {
    kind: UnsignedInt
    padding: UnsignedInt
    x_movement: Int
    y_movement: Int
    wheel_amount: UnsignedInt
    event: UnsignedInt
    time_stamp: UnsignedInt
    extra_information: UnsignedLong
}

func _send(event: UnsignedInt, wheel_amount: UnsignedInt) {
    var input: Input = {
        kind: _mouse
        padding: 0
        x_movement: 0
        y_movement: 0
        wheel_amount: wheel_amount
        event: event
        time_stamp: 0
        extra_information: 0
    }
    user32.send_input(1, input, Input.size)
}
```

- **Spite lays the struct out itself**, so the layout is yours to get right, including padding a C union leaves
  behind (the `padding` field above is there because `INPUT` is a `DWORD` followed by a union that aligns to 8).
  This is the same bargain `#[repr(C)]`, `extern struct` and `ctypes` make, and it is the price of never naming
  the C type in your own source.
- `Input.size` is `sizeof` of the struct the compiler emitted -- ordinary reflection on a Spite type, with no
  library in the expression.
- **The compiler checks your work for free.** When the library was given a header, it derives the C name through
  the same naming rule (`Input` -> `INPUT`) and emits
  `_Static_assert(sizeof(INPUT) == sizeof(Mouse_Input), "mouse.spite:12 type Input does not match INPUT");`.
  A forgotten padding field fails the build at the Spite line, naming both types. **The derived C name is used to
  verify, never to generate.** Size is checked; field order is not, because C's own field names (`dwFlags`, `dx`,
  `mouseData`) are unmappable by any rule, and a table for them is exactly what this design refuses to have.
- A `type` containing a `String`, `List<T>`, `Dictionary<T>` or class field is a diagnostic at a foreign call,
  naming the field: the all-scalars condition is enforced, not assumed.

### What crosses

| Spite | C | Cost |
|---|---|---|
| `Tiny`..`Long`, `Byte`..`UnsignedLong`, `Float`/`Double`, `Bool` | `int8_t`..`uint64_t`, `float`/`double`, `bool` | zero -- section 4's numeric types already are the C types |
| `String` (argument) | `const char*` | zero -- every `String` buffer is already NUL terminated |
| `String` (result) | `const char*` | one copy; Spite never frees a buffer C owns |
| an enum value | its integer | zero |
| a scalar-only `type` | a struct, by address | zero |
| `List<T>` of scalars | the element array, by address | zero |
| a class instance | -- | never. Its layout (`SpiteHeader`, ref count) is the compiler's business |

**Argument widths** (proposed by Claude, unconfirmed; implemented 2026-09-24). With a header, a call goes through
the header's prototype (`__typeof__(&Symbol)`), so C checks the argument count and converts each argument to its
parameter's type; integer-to-pointer conversion is allowed, because a handle is a `Long`, and a `void` function's
call reads as `0`. Without one, every integer-like argument (`Tiny`..`Long`, `Bool`, enum values) is passed as
`int64_t`, and every unsigned one as `uint64_t`: the width x64 and 64-bit ARM pass in a register or stack slot
anyway, so a literal `0` for a 64-bit parameter no longer leaves the upper half of the register undefined.
`Float` and `Double` pass as themselves. Left open: a `Double` where C wants `float`, and Apple ARM's stack
arguments past the eighth, which it packs at their own width -- both need the header.

**Return types.** A foreign call returns `Int` (integer/pointer width) and section 4's right-to-left casting takes
it from there. A `Double` comes back in a different register and cannot be inferred from context, so it is asked
for by suffix -- `user32.get_scale_as_double(...)`, and `..._as_text()` for a copied `const char*` -- which is
section 8's ordinary segment template, not a new mechanism.

### Lifetime, and what gets linked

- The library handle is a plain `Long`. **There is no `Pointer` type in Spite**, and this design does not add one.
- `DynamicLibrary` is a **singleton** (open question 7 -- the syntax is not decided yet, which is why the class above
  does not show one), keyed by its constructor arguments: every class asking for
  `DynamicLibrary("user32.dll", ...)` shares one object and one import table; `"gdi32.dll"` is a second one.
- The import table is the set of call sites that survived tree shaking: two symbols are resolved because two were
  called. They are resolved once, in the constructor, so every later call is one indirect call -- never a lookup
  per call.
- A missing library or a missing symbol aborts at construction, naming the file, the symbol and the Spite function
  that wanted it. This is a program-stopping error and is not reported any other way.
- `drop()` closes the library when its last reference goes, like any other class.
- `--final_classes` prints each binding with its resolved name and library as a `#` comment, the same way it
  already annotates which root won a reopened function -- so the mapping is reviewable generated output rather
  than something maintained by hand.

### Other environments  **[planned]**

The same mechanism is how Spite reaches anything that is not C. Nothing here is new machinery: it is section 11's
loading and reopening, plus a codegen value.

- **`$target`** is a compiler-provided program variable (`--target=native|web`), reserved like every other flag
  name in section 13.
- **Platform code is chosen by reopening, not by an abstraction layer.** `platform_web/console.spite` and
  `platform_native/console.spite` reopen the same `Console`; the entry constructor does
  `if $target == 'web' { load("platform_web") } else { load("platform_native") }`, and section 9's compile-time
  folding plus tree shaking remove the loser entirely. This is the game-mod mechanism from section 11 aimed at
  platforms.
- **The web bridge is `DynamicLibrary` reopened.** `platform_web/dynamic_library.spite` replaces `_open`/`_call`
  with a JavaScript module's exports; a foreign object that cannot cross into wasm is a handle into a JS-side
  table, freed by the same `drop()`. A class written against a library compiles for both targets unchanged.
- The C backend emits wasm through a C compiler targeting `wasm32-freestanding`, **plus the JavaScript glue module**,
  generated from the same declarations -- hand-written glue is what makes wasm painful everywhere else, and this
  compiler already generates C.
- Which classes exist on which target is not a special rule: a class declares it (see [Targets](#targets-planned)),
  so `File`, `Directory` and `Process` on the web are a compile error like any other class used off its target.
- **(D76 ended `$target` as a program variable; its build-time home is a `Build` field, D85 -- see [Build
  settings](#build-settings-build--implemented).)**
- The isomorphic direction (`environment.name` of `server` or `client` in `examples/arsenal`) is what the next section
  describes: a class compiled into both bundles, whose server-only functions become a network call on the client
  and a route registration on the server.

### Isomorphic classes: where a function lives  **[planned]**

D13 (decided by Mortaro, 2026-09-19): **a function's first parameter decides which bundle it is compiled into.**
There is no annotation, no naming convention and no compiler trick -- the framework reads it off the signature
with ordinary compile-time reflection (section 8).

```user_repository.spite
func find_user_by_id(context: ServerContext, id: Int): User { }

func load_users(context: ClientContext) { }

func format_name(first_name: String, last_name: String): String { }
```

| First parameter | Compiled into |
|---|---|
| `ServerContext` | the server bundle only; the client gets a generated stub that calls it over the network |
| `ClientContext` | the client bundle only |
| neither | both -- isomorphic |

- **The marker is the capability.** A server function needs a server context (database handle, request, session)
  and a client function needs the client one, so the parameter that says where the function lives is the same
  parameter that carries what it is allowed to do. It cannot drift from the truth: a function cannot claim to be
  server-side without holding server capabilities, and cannot reach a server-only API without asking for the
  token that provides it. The same shape extends to any capability (`DatabaseContext`, `FileSystemContext`)
  without another language feature.
- **The test is identity, not a name.** `function.arguments[0].class == ServerContext` compares two
  `Spite.Class` values, so a renamed or misspelled class is a compile error rather than a predicate that quietly
  turns false and drops an endpoint. Matching on `.name.starts_with("Server")` would work and is what a language
  without real reflection has to do; Spite has the class object, so it uses it.
- **A union gives exhaustiveness.** With a `union Context` of `ServerContext` and `ClientContext`, a `switch` over it
  must cover every member (section 7), so adding a third environment later -- a worker, an edge runtime -- fails
  to compile everywhere that has to change, instead of silently taking a default branch.
- **The body is never emitted into the client bundle.** Only the generated call stub is, so a secret, a
  connection string, or a server-only helper reachable from that body is removed by the per-bundle tree shaking
  section 11 already does. This is the property that makes the split safe rather than merely convenient, and it
  costs nothing extra here.
- Routes are generated deterministically from the class and function name, and the wire format is already in the
  language: a `type` shape (section 7) is duck typed and JSON shaped, which is what it was introduced for.
- **A function crossing the boundary must have serialisable parameters and return type** -- scalars, `String`, a
  `type` shape, or a list of those. A class instance cannot cross (its identity and reference count are local),
  and the diagnostic names the parameter or field that does not qualify.
- **Open, and blocking:** what a failed call does. A network call fails in ways a local call cannot, and Spite's
  position is that only errors which stop the program deserve to exist (`mortaros_notes.md`). This is the case
  where that has to be answered concretely, so the errors design gates this milestone and nothing else does.

### Targets  **[planned]**

D20 (decided by Mortaro, 2026-09-19): a class says where it can run by overriding an ordinary member of
`Spite.Class`, `func targets(): List<Symbol>`. The default is every target; `DynamicLibrary` answers `['native']`
and `Html` answers `['web']`. Using a class against the wrong `$target` is a compile error naming the class, the
target and the flag. This replaces any special rule about `File`, `Directory` and `Process` on the web: they
declare their targets like everything else. The check runs after compile time folding and tree shaking, so a
`load(...)` behind `if $target == 'web'` stays legal: only a class still reachable in the built program is checked.

### Html: the browser as a library  **[planned]**

D19 (decided by Mortaro, 2026-09-19): `Html` is the web counterpart of `DynamicLibrary`: a singleton whose
`missing_function` and `missing_attribute` resolve against the browser. `Html.Node` wraps one node handle (an index
into a table on the JavaScript side, freed by `drop()`). Names convert with the `'camel_case'` naming rule:
`document.create_element("div")` is `createElement`, `node.text_content = "Hello"` is `textContent`,
`root.append_child(node)` is `appendChild`. A call is a method and a read or write is a property, the same split
foreign libraries use for functions and constants. `Html` is the low level layer the markup builder compiles down
to, as `Mouse` sits on `DynamicLibrary`.

Open: the markup builder below was sketched as `html.div(...)`, which now collides with this class. It needs
another name (`Markup`? `View`?). Mortaro has not picked.

### Markup  **[planned]**

D18 (decided by Mortaro, 2026-09-19): there is no JSX and no trailing block. Markup is `missing_function` plus
literals, with as many children as the call has arguments. That costs no new feature, because a Symbol codegen
function is generated per call site:

```
html.div({ class: "card" }, html.h1(title), TodoForm({ ondone: add_todo }))
```

It pulls together what already exists: tags come from `missing_function`, a list of children is
`todos.map_render()`, a component is any class that fits a `type` requiring `render()`, a handler is a function
value bound to its instance, and `when(condition, element)` returns an `Element?` that the framework skips.

Rejected, so nobody proposes them again: a JSX like literal is a second grammar and fixes neither conditionals
nor mapping; a trailing block (`f(x) { a b c }`) serves exactly one shape, since queries are
`database.filter_age_greater_than(10).sort_by_name()` and routes are plain statements, so it would be one more
meaning for `{ }` bought for a single case. Mortaro on the verbosity: annoying for a human to write, cheap for an
AI to write, and a human can still easily read it, which is the first line of the philosophy.

### The wire format  **[planned]**

D31 (decided by Mortaro, 2026-09-19): JSON is not what isomorphic classes send. Both bundles compile from the same
source, so there are no unknown consumers and nothing needs to describe itself: the wire carries a binary packing
derived at compile time, attributes in declaration order, no keys and no parsing. It compounds on the web, where
nothing has to be turned into strings to cross into JavaScript: the DOM command buffer and the remote call channel
are both opaque byte buffers. Proposed by Claude, unconfirmed: a schema hash in the handshake so an old client
meeting a new server crashes with a clear report instead of misreading bytes, and `--development` decoding
payloads to JSON on demand, since the compiler knows the schema.

### Not designed yet

- **Callbacks** (handing a Spite function to C as a function pointer). Needs a decision on function references,
  which the language does not have.
- **A C union past its first member.** Positional layout reaches the first member only; the rest needs C's own
  field names.
- **Varargs**, structs returned by value, and `#define`s that are function-like macros rather than constants.

---

## Open questions

1. `var damage: $damage_type = null`: `null` otherwise only exists for `T?`. PROVISIONAL: the compiler treats
   `= null` on a `$generic`-typed variable/field as "the default value of whatever type the generic is bound to" (not
   `T?`). This is implemented but still provisional -- revisit if it reads confusingly once more code exists.
   When the bound type is a `type` whose members are all attributes, its default is a real object -- the object
   literal with each attribute at its own default, admitted to the shape -- so writes through it are kept
   (proposed by Claude, unconfirmed, 2026-09-24; `conformance/stage6/shape_defaults`). A `type` that requires a
   function has no default object, since no literal can supply the function.
3. Right-to-left casting makes `age > 0.5` with an Int `age` mean `age > 0`. Accept, or make comparisons cast toward the wider type. D162 settled arithmetic (a wider right operand is an error); comparisons still cast right to left and are not checked (proposed by Claude, unconfirmed), so this stays open for them.
   - The abbreviation lint has no escape hatch for names that must mirror an external spelling (`keyword_var`). Keep it absolute, or allow a per line `# spelled: keyword_var` style exemption.
6. `_` now means two things: private (section 2) and intentionally unused (section 5). They mostly agree (an unused
   private function is fine either way), but an unused PUBLIC function cannot be an error (libraries are full of them; tree
   shaking removes them), so "unused" is only enforced for locals, parameters and private functions. Confirm.
   D118 adds attributes, where `_` already meant private: an unread `_name` attribute is never reported, since the
   prefix says both things at once, so a dead private attribute passes (section 5, `mortaros_missing_decisions.md`
   item 111).
8. **An unrelated `get_<attribute>()` silently intercepts a read.** Found by the 2026-09-20 reflection port, in
   this compiler's own `NullableType`: a class with an attribute `inner` and a zero-argument function `get_inner()`
   written for an unrelated purpose has every outside read of `.inner` routed through that function, because that
   is exactly what attribute interception (section 5) says to do. It is the rule working as specified and it is
   still a trap, since nothing announces it.
   - **Keep it.** The rule is uniform and a reader who knows it can see the collision.
   - **Make it a compile error when the function's return type differs from the attribute's** (proposed by
     Claude). A real getter returns what the attribute holds; an unrelated function usually does not, so this
     catches the accidents and leaves the genuinely ambiguous case -- same name, same type -- being treated as
     a getter, which is defensible. It is also the shape this language reaches for everywhere else: something
     the compiler can know becomes a compile error naming the fix, rather than a warning or a convention
     (section 16 item 6, D24).
   The residual case neither option catches is an unrelated function whose return type coincides with the
   attribute's. Claude would accept that.
9. **Text with `${name}` in it prints literally** (found 2026-09-20 by writing the programs an AI would write).
   Spite builds text with `+`, and `"hello ${name}"` is simply those characters, so the mistake is silent -- the
   one outcome section 16 item 6 says a compiler should never produce. `$` is already the codegen sigil, so
   `${` inside text is unlikely to be meant literally.
   - **Make `${` inside text a compile error** naming `+` (proposed by Claude). A dollar before a brace has no
     other use, and text that genuinely needs it can be built with `+` or written `$` `{` apart.
   - **Leave it.** Text is text, and a rule about what may appear inside it is a rule to remember.
10. **How `--final_classes` shows which root supplied a declaration.** D7 and milestone 10a both say a
    reopening must not be silent, and `--final_classes` is where it stops being silent -- but what it writes is
    a *program*: running the printed entry file runs the same program, which is what makes it proof rather than
    a report. Provenance cannot be a comment, because a comment is only ever a link to a markdown heading
    (section 12), and it cannot be a declaration without changing the program.
    - **A separate manifest** beside the printed classes (proposed by Claude): one line per declaration with the
      root it came from. Keeps the printed source a program, and the thing you grep is a table rather than
      prose scattered through files.
    - **Print it to the console** as the classes are written, so it is read once and not stored.
    - **Give the comment rule one more form**, a provenance line the compiler writes and a human never does.
11. **Whether a `type`'s function members should be written as function-valued attributes** (Mortaro, 2026-09-21,
    thinking aloud rather than deciding). Today a shape writes `hit(): Int`; the alternative is
    `hit: Spite.Function<Int>`, which would make a required function an ordinary attribute whose type happens to
    be a function, and would leave a `type` with exactly one kind of member instead of two.
    - **It needs D39 first**: the typed `Spite.Function<Arguments..., Return>` does not exist yet, so the form
      cannot be written or printed.
    - Against: `hit(): Int` reads like the declaration it matches, and a shape is matched against functions
      written `func hit(): Int`.
    - For: one kind of member, and it composes -- a shape could then require a function value it will *store*,
      which `hit(): Int` cannot express.

12. **(Answered by D87: `generic $name` header lines. Built 2026-09-24, section 9.)** **A header form for generics, with constraints** (Mortaro, 2026-09-23, asked to be argued with). The proposal:
    `generic $type` lines at the top of the file beside `singleton`, and a described generic
    `generic $sub_type { initial_value: Spite.Function<$sub_type> }` that narrows what may be supplied. The
    problem it solves is real: a class that takes codegen values must have a constructor to declare them, even
    when it has nothing to construct, and the constructor form has nowhere to say what a supplied class must be
    able to do. And the distinction Mortaro draws is the right one -- a described generic is *one* class for every
    place that uses it, where a `type` accepts anything that quacks, afresh at each call.
    Claude's argument, for and against (proposed by Claude, unconfirmed):
    - **For the header line:** `singleton` already made a header line a pattern, so parity is a real argument, and
      the objection that removed the old `generics` line (SPITE.md: "feels outside of our patterns") was about
      an ordered *list* in a line of its own; one `generic` per line is a declaration like `var`, not a header.
    - **Against the inline block:** it is an anonymous `type`. Write the constraint as a shape the program
      already has -- `generic $sub_type: Openable` -- so one mechanism describes what a class must be able to do.
      An inline block is then just sugar for an unnamed `type`, and can come later if it is missed.
    - **Against `gen`:** it is an abbreviation, and the abbreviation rule has no exceptions for keywords.
      `generic` says what it is.
    - **What becomes implicit:** a caller's `Journal<String, Chapter>()` is positional, so the order of the
      `generic` lines becomes the order callers write. The constructor form made that order visible in one
      signature; the header form spreads it over lines. Acceptable, since D67 fixes where the lines go.
    - **The example's `var opened = $sub_type`** reads as assigning a class to a variable. Claude assumes
      `var opened: $sub_type` was meant.
13. **How casting works, so a class can define its own casts, and how to name a variable's class as a type**
    (Mortaro, 2026-09-23: "a thing for you to ask me later"). Example shape: `func from_type(type: Symbol, value:
    type.class)`. Not argued yet; waiting to be asked. D59 (arguments cast to their parameter type) is where it
    will first matter.
14. **(Answered by D90: `...args: List<Type or Class>`. Built 2026-09-24, section 5.)** **An ABI for variadic arguments** (Mortaro, 2026-09-23, "fight me on this before we implement"). Proposed:
    `func hello(world: String, ...args: List<Spite.Argument<String>>)`, which would make `Spite.Argument` generic.
    Claude argues against the `Spite.Argument` part (proposed by Claude, unconfirmed): `Spite.Argument` is the
    *reflection* of a parameter -- a name and a class, known at compile time -- and a variadic argument is a
    *value* at runtime. Making one class carry both merges the two levels D11 keeps apart. `...args: List<String>`
    already says everything: the `...` means "the caller writes these one by one", the list is the type the
    function receives. For `Console.print`, which takes anything, the element type is a `type` every printable
    value satisfies -- `...values: List<Printable>` -- so the language needs no new class, only `...`.
15. **Whether `while` can go** (Mortaro, 2026-09-23: investigate every use; if it can be rewritten with
    metaprogramming, make it an error). Measured on 2026-09-23 over the compiler, `library/`, `scripts/`, the
    tests, the examples and the corpus: 259 `while` loops, and 199 of them are the same shape -- an index from 0
    to `list.count()`, reading `list[index]`. Every one of those is a member template (`each_`, `map_`,
    `filter_`, `find_by_`, `any_`, `count_`, `sum_`). The other 60 are genuinely loops over state: the lexer
    scanning characters, the parser consuming tokens, walking backwards, settling until nothing changes.
    Proposal (Claude, unconfirmed): make the 199-shape an error naming the template -- detected as a `while`
    whose condition compares a counter with `.count()` of a list the body indexes by that counter -- and keep
    `while` for the rest. This pairs with D64: an index read is where `[]` becomes `T?`, and removing the
    index loops removes almost all of the narrowing D64 would otherwise demand. What the templates still lack
    for the compiler's own loops: the index inside the body, and stopping early.
16. **Two versions of one dependency** (Mortaro, 2026-09-23). When two packages load the same git dependency at
    different commits and the difference changes nothing either package uses, the compiler should just use one,
    without asking. When the versions differ in members that are used, the compiler treats them as two packages
    in two namespaces, so both keep working, and a command lists these splits for whoever wants to unify them --
    "not actually broken, just annoying". Belongs to D38 (a dependency is a git URL plus a commit in `load`),
    which is not implemented; recorded so the design starts here.
17. **(Answered by D83/D88: `this` where a class names itself.)** **A syntax for `this`** (Mortaro, 2026-09-23): to be discussed when something needs it. Today bare names
    reach attributes and `class` is the instance's class, so nothing does yet.
18. **(Answered by D89.)** **The entry file is always the file named after its folder** (Mortaro, 2026-09-23, asked to be argued for).
    `spite hello` runs the `hello` folder's entry file; a file name on the command line is no longer accepted.
    The case for it (Claude): discovery already requires an entry folder, so the file name on the command line
    only restates the folder -- two ways to say one thing, and the second can disagree (`spite hello/other.spite`).
    One way to run a program is the rule SPITE.md states under "more than one way to do a thing". The cost is
    small and known: editors that pass the current file will pass its folder instead, and a compile error naming
    the folder covers the habit. Claude would do it. **Half built (2026-09-23):** `spite hello` runs `hello/hello.spite` now; a file argument still works, and making it an error is the part that waits on Mortaro.
19. **(Answered by D88: getter-only functions.)** **Constants without a `const` keyword, and reflection attributes that cannot be overwritten** (Mortaro,
    2026-09-23). Constants: yes, without a keyword -- the compiler sees the whole program, so an attribute that
    nothing assigns after its default (no assignment, no `set_` call, no reflective write) is a constant and
    folds. Nothing is declared; it is just what the optimiser finds (D36). Read-only attributes: Mortaro's idea
    is to declare no attribute at all, only `get_attributes()`, which attribute interception already turns into
    a readable `.attributes`; with no `set_attributes()` there is nothing to assign through, so writing it is an
    error. What the compiler needs (Claude, unconfirmed): a read of `.name` that finds no attribute but finds
    `get_name()` is an attribute read, and a write to it with no `set_name()` is an error saying the attribute
    is read-only. The storage behind it is a private `_attributes`, which the compiler fills as it fills
    `attributes` today. **The mechanism is built (2026-09-23):** a read that finds no attribute but finds `get_name()` reads through it, and a write with no `set_name()` is an error calling the attribute read-only (`tests/interception_tests`, `diagnostics/read_only_attribute`). Moving `Spite.Class`'s own members onto it is left for Mortaro to confirm.
20. **Whether a nested `if`/`else` is an error** (Mortaro, 2026-09-23: "we should discuss"). Claude's view: nesting
    is where generated code becomes unreadable, and the language already removes the common cases -- a
    precondition is `assert` (D54), a choice between kinds is a `switch` over a union or enum. What remains is a
    genuine decision tree, and forbidding it pushes it into a helper function, which is usually the right call.
    A concrete rule to decide on: an `if` with an `else`, inside a branch of another `if` with an `else`, is an
    error that names extracting a function.

## Decision log

| Date | Decision |
|---|---|
| 2026-09-18 | `#` comments; newlines and commas both separate entries; only `func` (not `function`). |
| 2026-09-18 | Entry point is the entry file's constructor, not `main`. |
| 2026-09-18 | Packages are loaded by their root (`load("package")`); the root is not part of the namespace. |
| 2026-09-18 | Folders are never uppercase; namespaces are the PascalCase of lowercase folder names. Linter rule later. |
| 2026-09-18 | `load` is a bundle boundary (webpack async import); inside an `if` it lazy loads. Argument is a literal. |
| 2026-09-18 | Enums, types and unions declared in a file are namespaced under its class (`Player.Job`). |
| 2026-09-18 | Enum values resolve from usage; shared value names are fine; ambiguity is a compile error. |
| 2026-09-18 | `.attributes` is a list of `SpiteAttribute`; `.class` is a `SpiteClass`; `attributes[symbol]` indexes the former. |
| 2026-09-18 | `return` is always explicit; return type is always `(): Type`. |
| 2026-09-18 | `$` means "replaced at codegen", fed by generics or by compiler flags. |
| 2026-09-18 | Classes and the instances registry conceptually live on the heap; unused parts are removed at compile time. |
| 2026-09-18 | No file level statements: only declarations. |
| 2026-09-18 | (proposed by Claude, unconfirmed) Compiler flag names are reserved; an unmatched `--name=value` is an error. |
| 2026-09-19 | The reflection loop is written `for instance.attributes do attribute`; the old bare `for instance do attribute` form is removed. |
| 2026-09-19 | Reserved compiler flag names (`development`, `optimized`, `output`, `help`, `debug_memory`, `emit_c`, `dump_tokens`, `dump_ast`, `repl`, `repl_port`) may not be used as a `generics $name` or a `--name=value` program variable. |
| 2026-09-19 | A `--name=value` that matches no `$name` used anywhere in the program is a compile error (typo protection). |
| 2026-09-19 | (proposed by Claude, unconfirmed) String is a single-owner immutable value (a length-prefixed buffer, not a true reference count); `Dictionary<T>` is String-keyed, insertion-ordered, and implemented as a linear scan; `Heap<T>` is a plain pointer, freed by whichever single thing owns it -- see [Standard library](#15-standard-library-partial) and open question 2. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `File`/`Directory`/`Process`/`Program` are built-in classes (hand-written bodies, not parsed from `.spite` source) rather than a new kind of special-cased value, so ordinary member access, method calls, and drop chaining all apply to them for free. |
| 2026-09-19 | (proposed by Claude, unconfirmed) A class or union's C struct is now forward-declared (`typedef struct Name Name;`) before its own fields/members are resolved, and a union is registered before its members resolve (not after): both are what let a class or union recursively contain itself through `Heap<T>`, fixing the previously documented "unions not resolved in dependency order" limitation for the self-recursive case. |
| 2026-09-19 | `null` on a `$generic`-typed variable/field means that generic's bound type's default value (provisional; see open question 1). |
| 2026-09-19 | Spite has only the `while` loop; `for` is removed entirely (decided by Christian: "only the while loop, no for; that makes people favor the metaprogramming"). `do` stays for `if nullable do value`. Writing `for` is a parse error naming `while` and the `List<T>`/`Dictionary<T>` metaprogramming helpers. |
| 2026-09-19 | `value.attributes` is a real runtime `List<SpiteAttribute>` (`.name`/`.class`/`.value`, all `String`), built inline only where `.attributes` is actually used; the old compile-time-unrolled `for instance.attributes do attribute` loop is gone along with `for` (decided by Christian, as a consequence of the `while`-only decision). |
| 2026-09-19 | `List<T>` gains `each_<member>()`, `map_<member>()`, `any_<member>()`, `all_<member>()`; `Dictionary<T>` gains `each_<member>()` -- compiler built-ins in the same style as `filter_`/`sort_by_`, meant to make `while` rarely necessary (decided by Christian). |
| 2026-09-19 | (proposed by Claude, unconfirmed) Ownership rules (moves, temporaries, overwrite-drop -- manual.md's Memory section): a move is sound-by-construction only in the exact scope that declared the local, so a move from inside a conditional/loop body is a compile error rather than a runtime-tracked flag -- the simpler of the two options considered. An alias stored a second time is deep copied only for `String`; every other owning type keeps the pre-4d shallow-alias behavior (a documented, unfixed gap) rather than risk an unsound generalization. `--debug-memory`'s free wrapper now tracks every live pointer and aborts on a double free or an unknown pointer, instead of only counting allocations/frees. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `load(...)` roots are discovered by a purely static, pre-codegen AST scan of every function body reachable from the entry file's own root, not by a runtime/reachability-precise pass; every root it finds is over-inclusive rather than under-inclusive (a `load` inside a function tree-shaking would later remove still counts). Its path is always resolved relative to the entry file's own folder, even when the `load` call itself lives inside already-loaded code. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Only the entry file itself is scanned for `load(...)` calls from the entry root; an unrelated sibling file in that same flat folder (a second, independent entry-style program living next to it, e.g. two of this compiler's own examples sharing a folder) is not, even though it may still be pulled in as an ordinary tree-shaken class if referenced. Every file in an explicitly `load`-ed root is always scanned, since loading a root is itself the deliberate act that makes its files part of the program. A `load` reachable only through an ordinary (non-`load`) function call from the entry constructor into a *different* sibling file in the entry folder is a known, undocumented-until-now gap of this simplification. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Every class, and every enum/type/union declared inside one, is keyed everywhere (the C identifiers `resolveClass` emits, and the map key `classes`/`enums`/`unions`/`raw_type_declarations` use) by its full namespace-mangled name (`.` replaced with `_`); a class's own file-given simple name (`ClassInfo.source_name`) is kept separately, purely to recognize its constructor. A folder's entrypoint file is registered under two keys pointing at the same class (`Engine.Renderer` and `Engine.Renderer.Renderer`); an enum/union/type declared with the exact same name as its own owning class (e.g. `expression.spite`'s `union Expression`, next to that file's own otherwise-unused implicit empty `Expression` class) gets the same treatment, aliased at the owner's own bare name -- without it, that bare name would resolve to the coincidental empty class instead, since a class and its own like-named nested declaration are checked in the same unqualified-name search chain (enums/unions/types checked before classes). |
| 2026-09-19 | (proposed by Claude, unconfirmed) Reopening (the discovery pass's merge pass, before any class is even registered with `codegen`): a later root's `func`/`var` of the same name replaces the earlier one and is what `codegen` ever sees (never both); origin metadata (which root, and which earlier root(s) it replaced) is tracked on the side, only for `--final-classes` to print as `#` comments. `enum`/`type`/`union` declarations inside a reopened class are simply concatenated across every contributing root rather than merged by name -- a genuine duplicate is still caught (as "declared more than once"), by the same registration check an ordinary duplicate hits, but reopening *adds to* a class's own nested enum/type/union, not *replacing* one, is not attempted. A `var`'s replacement is checked for an explicit-type mismatch by comparing the two parsed `ast.Type` trees structurally; a `var` inferring its type from its value (no explicit annotation) on either side is not checked, since that would need real type resolution before any class is registered. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `--final-classes` prints the merged, winning source the discovery pass already builds (via a new the canonical printer AST -> source printer, every binary/unary expression fully parenthesized so print -> parse -> print is trivially stable) with `#` origin comments before each reopened `func`/`var`; it does not yet also list a class's instantiated Symbol codegen functions (`set_age`, ...) or used `List<T>`/`Dictionary<T>` helper signatures, since that information only exists deep inside `codegen`'s own resolution pass, after the program model the discovery pass builds has already been consumed -- left for a later milestone. |
| 2026-09-19 | `assert value` (and `assert value and other_condition`) on a `Nullable<T>` local/parameter/field narrows it to `T` for the rest of that block and any nested block, reading/writing/calling through to the value held inside the `Nullable` in place; reassigning the narrowed name to `null` is a diagnostic. A function whose body's last statement is `if value do name { ... }` -- wrapping the rest of the function only to check existence -- is a lint error naming the `assert` rewrite; `if ... do` stays legal everywhere else (decided by Mortaro). |
| 2026-09-19 | Every operator is a function (full table in the new [Operators](#operators-implemented) section): `+`/`-`/`*`/`/`/`%` are `sum`/`subtract`/`multiply`/`divide`/`remainder`; `==`/`<`/`>` are `equals`/`less_than`/`greater_than` (`!=`/`<=`/`>=` derived); `a[x]`/`a[x] = v` are `get_at`/`set_at`; unary `-` is `negate`. `not`/`and`/`or` stay built in. A user class or union answers these the same way it already answers a Symbol-codegen method call (decided by Mortaro). |
| 2026-09-19 | (proposed by Claude, unconfirmed) Attribute access from outside a class also goes through operator functions: `person.age = 1` calls `set_age`; `person.age` (a read) calls `get_age` the same way, except when the getter's return type owns something (a getter for a `String`/`List<T>`/... field falls back to a raw field read instead, to avoid leaking a getter that hands back a *fresh* owned value through a read that is, syntactically, still a plain `.member` expression to every ownership-tracking call site). The manual only decided the setter half of this; the read half and its owning-type restriction are Claude's own extension and known limitation, not confirmed by Mortaro. |
| 2026-09-19 | (proposed by Claude, unconfirmed) A method's own parameter/return type may now name its own class by value (`func sum(other: Vector): Vector` inside `vector.spite`), needed for operator functions to read naturally: `Generator.self_reference_class` lets `resolveClass`'s reentrancy guard tell that case apart from a genuine field-cycle (a class embedding itself by value), which still errors. |
| 2026-09-19 | `Spite` is a reserved root namespace: `SpiteClass`/`SpiteAttribute` are renamed to `Spite.Class`/`Spite.Attribute`, and a user folder (or `load(...)` root) named `spite` is a diagnostic instead of silently colliding with it (decided by Mortaro). |
| 2026-09-19 | (proposed by Claude, unconfirmed) Numeric types: `Tiny`/`Short`/`Int`/`Long` (`int8_t`/`int16_t`/`int32_t`/`int64_t`), `Byte`/`UnsignedShort`/`UnsignedInt`/`UnsignedLong` (`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`), `Float`/`Double` (32-/64-bit) -- `Int` and `Float` change from this milestone's earlier `int64_t`/`double` to `int32_t`/32-bit `float`, per Mortaro's instruction that all of them get written names. Converting between numeric types wraps on overflow (an ordinary C narrowing cast). `Float`/`Double` print with shortest-round-trip formatting (increasing `%g` precision until the parsed-back value matches exactly, skipping a low-precision match that only round-trips in scientific notation) rather than a fixed precision, so `Float` becoming 32-bit does not make ordinary values print with ugly trailing digits. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `List<T>`/`Dictionary<T>`'s internal storage (`count`/`capacity`/index parameters) stays `int64_t` regardless of the numeric type table above -- only the type `Type` values codegen exposes to Spite source changed; the C already narrows/widens implicitly at every call site, so this needed no changes to `List<T>`/`Dictionary<T>`'s generated support functions. |
| 2026-09-19 | (proposed by Claude, unconfirmed) 6a reflection tables are pure data (`SpiteReflectAttribute`/`SpiteReflectListType`/... in `src/runtime/spite_repl.h`): every offset/size is the real C `offsetof`/`sizeof` of the concrete generated type, computed once per class/enum/`List<T>`/`Dictionary<T>`/`Nullable<T>`/union actually in the program, so the interpreter is one hand-written file driven entirely by generated constants -- it never special-cases a particular program's types. `Heap<T>` is folded into whatever `T` reflects as, plus one `via_pointer` bit saying the field itself is already the pointer to walk through. |
| 2026-09-19 | (proposed by Claude, unconfirmed) A REPL build (`--repl` and/or `--repl-port`) always compiles with the debug allocator enabled (`SPITE_DEBUG_MEMORY`), independent of `--debug-memory`, so the `memory` command has something to report and the `--repl` end-to-end test can assert balanced memory the same way every other example's `--debug-memory` test does. The debug allocator's live-pointer table (added in 4d for double-free detection) now also records each pointer's size, so it can report live bytes, not only a live count. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `--repl-port`'s "exit" command calls C `exit(0)` directly from the server thread rather than signaling the main thread to unwind: the entry constructor may be running its own infinite `$serve` loop and never return control to `main`, so there is no other point guaranteed to ever run. `--repl` (local, stdin) has no such problem -- its loop runs after the constructor already returned, on the main thread, so it can fall through to the ordinary drop-and-return-0 path for a clean, balanced exit instead. |
| 2026-09-19 | (proposed by Claude, unconfirmed) A REPL build's own `main()` is run with inherited (not captured) stdio: unlike a normal `spite file.spite` build (whose child process's output is captured and printed after it exits, for the compiler's own non-interactive use), a `--repl`/`--repl-port` program is interactive or long-running, so the driver's `compileAndRunWithOptions` spawns it with `std.process.spawn`'s default inherited stdin/stdout/stderr instead of `std.process.run`. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `spite connect` is a client built into the compiler binary (the compiler's own process already runs on `std.Io`), not a libc socket call through `@cImport` -- `std.Io.net.IpAddress.connect` gave everything needed (connect, buffered line reader/writer) without guessing an unfamiliar 0.16 API. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `Program().sleep(milliseconds)` (used by `examples/dungeon`'s `$serve` loop) is a normal built-in on the existing `Program` system class, not REPL-specific -- `spite_sleep_milliseconds` lives in `src/runtime/spite_runtime.h` (always embedded) rather than `spite_repl.h` (REPL-only), so it works in an ordinary build too. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `examples/dungeon` requires `--serve=true`/`--serve=false` on every run (like `examples/arsenal` already requires `--environment=...`): `$serve` is a plain program variable, and this compiler diagnoses a `$name` used anywhere in the program but never supplied on the command line, with no built-in notion of an optional/defaulted program variable. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Milestone 7a's formatting/style rules (full list in the new [Style](#12-style-implemented) section): 4-space indentation, K&R braces, `generics` moved first, one blank line between file-level declarations (adjacent `var`s may stay grouped), minimum-necessary parentheses, and inline vs. multi-line list/object/call breaking at 120 columns -- "one true style" always, not "keep whatever the source already did," except that a list/object literal already short enough stays on one line regardless of how it was written. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Lossless formatting is implemented by attaching comment/blank-line `Trivia` to the AST at the specific points the parser already visits one entry at a time (declarations, statements, and list/object/enum/union/type-field/switch-case entries) rather than trying to preserve arbitrary comment placement everywhere; a comment the parser has nowhere to attach it (mid-expression, or with nothing following it before a closing bracket) is caught by the safety check (below) refusing to format that file, rather than silently dropped. the canonical printer (the older, comment-blind AST printer `--final-classes` already used) is unchanged and reused as the safety check's canonical-equality oracle; the new lossless printer lives in the formatter. |
| 2026-09-19 | (proposed by Claude, unconfirmed) The formatter's safety check re-lexes and re-parses its own output and compares the canonical printer's canonical rendering of the reformatted-and-reparsed AST against the same rendering of the original AST (after applying the one sanctioned `generics`-first reordering to both), plus a full, ordered comparison of every comment's trimmed text -- rather than a raw token-stream diff -- since the canonical printer already normalizes away exactly the whitespace/comma-vs-newline differences formatting is allowed to make. |
| 2026-09-19 | (proposed by Claude, unconfirmed) The naming/abbreviation linter (the linter) runs on every discovered class file's AST (the discovery pass's merged, pre-codegen `ClassFile` list) rather than only the entry class, so a lint anywhere in the program (including a `load`-ed root) is always caught; like every other diagnostic this compiler prints, its `file:line:column` is still reported against the entry file's own path (a pre-existing, compiler-wide limitation, not new to this milestone). The terminal-`if ... do` lint (manual.md section 5) moved here unchanged from the generator, since it only ever needed the raw AST. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `spite file.spite`'s automatic formatting pass discovers "every file belonging to the program" with its own light, purely textual scan for `load("name")` call sites (not the discovery pass's real AST-based discovery), so that a parse error in one file never blocks formatting the other, valid files of the same program -- the discovery pass's own discovery still runs immediately afterward and is the one that actually gates compilation. Since `load` only ever accepts a literal string, this heuristic cannot miss a real root. |
| 2026-09-19 | **D1** (decided by Mortaro): reference counting is the default memory model, JavaScript-like -- every non-scalar value is a reference; passing/assigning/storing/returning share the same object; `&` is removed; a copy is explicit (`copy()`/`deep_copy()`). Milestone 9a. |
| 2026-09-19 | **D2** (decided by Mortaro): no `break`/`continue` -- Spite keeps only `while`, permanently (nothing to implement; see section 6). |
| 2026-09-19 | **D3** (decided by Mortaro): `if value do name { } else { }` -- `if ... do` gets an `else`. Milestone 9a. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Object model (milestone 9a): every class instance/`List<T>`/`Dictionary<T>`/`String`/object literal is a heap object whose first field is a `SpiteHeader { ref_count; class_id; }`; a union is a small value-typed `{ tag; pointer }` pair (never itself heap-allocated); `Nullable<T>` of a reference `T` collapses into being the exact same pointer (null = absent), and only a scalar-element `Nullable<T>` keeps the old `{ has_value; value; }` wrapper. `copy()` is shallow (fields/elements retained, not duplicated); `deep_copy()` recurses and does not support cycles. `==` on a class falls back to identity (pointer equality) when no `equals` is defined; `String` always compares by content. `--debug-memory` gained a leaked-object-by-class-name summary, printed once at exit when allocations and frees do not balance. |
| 2026-09-19 | **D4** (decided by Mortaro): the foreign function interface is a class, not a keyword -- `DynamicLibrary("user32.dll", 'windows', "windows.h")`, with Symbol codegen resolving every foreign function, constant and type at compile time. No `external` keyword, no per-symbol binding strings, no hand-written C shim, and no `Pointer` type. Rejected along the way, in order: a file-level `external c "header.h"` declaration with a `binds "SetCursorPos"` string per function (a mapping table by another name); a compile-time `external_name` hook plus `external type` (better, but still a keyword); and tying native libraries to `load(...)` (a library has a handle and a lifetime, so it is an object, not a bundle boundary). Milestone 11; see [Foreign libraries](#17-foreign-libraries-planned). |
| 2026-09-19 | (decided by Mortaro) The class is named `DynamicLibrary` -- not `DLL` (the linter's own abbreviation rule forbids it, and it is wrong on Linux/macOS) and not `Library` (too generic). |
| 2026-09-19 | (decided by Mortaro) No `external` keyword anywhere: which kind of foreign symbol a member access means is derived from the language's own enforced capitalization (PascalCase = type, snake_case = constant, snake_case call = function), and a scalar-only `type` is a C struct implicitly, from the usage that types the literal. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Foreign library details (section 17): `missing_function`/`missing_attribute` as the two reserved hooks (Ruby's `method_missing`, as compile-time Symbol codegen); `symbol_name` as a naming *function* with a `Naming` enum, reusing the linter's abbreviation table backwards to generate Win32 spellings; constant reads being the C spelling lowercased, with no prefix table; Spite laying out a foreign struct itself (so a C union's padding is the author's job) with a generated `_Static_assert(sizeof(C_TYPE) == sizeof(spite_type))`, derived through the naming rule, to verify -- never to generate; `Int` as the default foreign return type with `_as_double`/`_as_text` suffix templates for the rest; constructor arguments restricted to literals; and the import table being exactly the tree-shaken call sites, resolved once in the constructor. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Other environments (section 17): `$target` as a reserved, compiler-provided program variable; platform implementations chosen by section 11 reopening plus section 9 compile-time folding rather than by any abstraction layer; the web bridge being `DynamicLibrary` reopened over a JavaScript module's exports; wasm emitted through a C compiler targeting `wasm32-freestanding` together with a generated JS glue module; `File`/`Directory`/`Process` a compile error under `--target=web` instead of a silent stub. |
| 2026-09-19 | (decided by Mortaro) Singletons are wanted, must reuse the metaprogramming rather than be a special case, and must be invisible at the call site -- `var console = Console()` reads the same, and only the AI writing the code needs to know. The leading candidate, and Mortaro's favourite so far but still undecided, is the constructor declaring it by returning its own class (`func Console(): Console`), keyed by its literal constructor arguments and implemented as constructor monomorphization; `singleton` as a file-level line and `$singleton`/`$instances` are rejected. See open question 7. |
| 2026-09-19 | **D5** (decided by Mortaro): the `generics` header line is removed and **nothing replaces it** -- a codegen value is never declared. Writing `$name` anywhere in a class is what makes it a hole, and the hole is filled from outside, which is already exactly how a flag-fed value like `$serve` works; the header line was the only thing that made call-site values different. At a call site, **one unfilled hole is given positionally and two or more are named**: `List<Int>()`, `Weapon<$damage_type: Magic, $is_magic: true>(10)`, with the name written exactly as it appears in the class, `$` and all. Naming removes the argument order, which was the header line's only job. There are no defaults: every hole must be filled, and a call leaving one unfilled is an error listing every `$` the class uses with the file and line of its first use (which is also what a typo inside a class body produces). A flag fills its hole program-wide, so "one hole" at a call site means one still unfilled. `List<T>`/`Dictionary<T>`/`Nullable<T>` each have one, so they follow the rule rather than being exceptions to it. An intermediate `$name = default` declaration form was considered and rejected in turn: it kept a declaration alive for its own sake, and a codegen value is a hole, not a variable. Milestone 13; see [Codegen values](#9-codegen-values-). |
| 2026-09-19 | (decided by Mortaro) `$singleton = true` is rejected: `$` means "replaced at code generation", and a directive the compiler reads and then deletes is not that -- it would make the sigil mean two things. The leading candidate is now a function of `Spite.Class`, `func is_singleton(): Bool` (Mortaro, choosing among five further proposals), still **not decided**; the constructor returning its own class is the runner-up. See open question 7. |
| 2026-09-19 | **D6** (decided by Mortaro): **Spite has no static class functions and will not get any.** A class is an instance of `Spite.Class`, which is an ordinary standard library class with an ordinary declaration, so there is nothing for `static` to mean -- a function belonging to the class rather than to its instances is simply a function on that `Spite.Class` object. The set of class-level hooks is exactly the set of functions `Spite.Class` declares; a class file defining one of those names overrides it for its own class object (section 11 reopening), anything else is an ordinary instance function, and a collision is a diagnostic naming `Spite.Class`. Overrides are folded at compile time (a literal `return` always folds; `return $codegen_value` folds too, so a class-level fact can differ per build), and `--final-classes` prints each one with the value it folded to. The reasoning is Mortaro's: a configuration file is just a class being reopened, the way Rails patches its internal options through code that runs instead of a static configuration file -- a static line can only state a value, a function can compute one. See [Class-level functions](#class-level-functions-and-why-there-are-no-static-functions-planned). |
| 2026-09-19 | **D7** (decided by Mortaro): `Spite.Class` is reopenable like any other standard library class -- "by all means shoot the foot". Reopening it changes a class-level default (D6) for the whole program: a root returning `true` from `is_singleton()` makes every class a singleton, `List<Int>()` included, and adding a new function to `Spite.Class` creates a new class-level hook name program-wide, turning any same-named instance function elsewhere into an override of it. Allowed because nothing about it is silent: a collision is a compile error naming `Spite.Class`, and a changed default appears per class in `--final-classes` with the root it came from. The compiler has no warnings (section 12), so visible generated output is the whole mitigation. Blocked on section 16 item 1 (reopening standard library classes at all). |
| 2026-09-19 | **D8** (decided by Mortaro): a singleton is declared with the function of `Spite.Class` `func is_singleton(): Bool { return true }`, overriding the default `Spite.Class` declares (D6). Call sites never change (`var console = Console()`); one instance per distinct literal constructor argument list, emitted as one static slot per argument list, so there is no runtime registry walk -- only a guarded branch on first use. The slot holds one reference, `drop()` runs at program exit in reverse creation order, and `--debug-memory` counts singletons as roots rather than leaks. `Console`, `Program` and `DynamicLibrary` are singletons; `File`, `Directory` and `Process` are not. Chosen over a bare `singleton` line, `$singleton`/`$instances`, a constructor return type (`func Console(): Console` / `: Spite.Singleton`), a renamed initialiser (`shared_instance`), a private constructor, and a parenthesis-free call, for the reason Mortaro gave: a configuration file is a class being reopened, the way Rails patches its options through code that runs rather than a static file. Named `is_singleton` rather than `instances` because `instances` is already the live-instance registry. See [Singletons](#singletons-planned). |
| 2026-09-19 | **D9** (decided by Mortaro): **the constructor declares every codegen value a caller supplies, always, even when there is only one** -- `func Weapon<$damage_type, $is_magic>(new_damage: $damage_type)`, called `Weapon<Magic, true>(10)`. Skimming a constructor is how a human sees what a class accepts, and a list of one is still a list. Consequences: call sites are positional with no named form (the constructor fixes the order, so nothing is restated at the call); a `$name` used but *not* declared is a program variable filled by `--name=value` (`$serve`, `$environment`), which is what finally distinguishes a call-site hole from a flag-fed one; a class taking codegen values always has a constructor, even one existing only to declare them; the built-in containers declare theirs the same way (`List<$element_type>`); a wrong-arity call is an error naming the values in order; and reordering a constructor's `<...>` silently changes same-kinded call sites, which tests are accepted as the thing that catches. Supersedes the named call form and the proposed class-level `generics()` hook, neither of which survived. Milestone 13; see [Codegen values](#9-codegen-values-). |
| 2026-09-19 | **D10** (decided by Mortaro): **an enum is a closed list of symbols.** `'knight'` is a symbol (a compile-time name) and an `enum` declares which symbols are accepted where it is expected; the integer is a representation detail. A symbol literal is legal only where something says what it may be -- an enum (it must be a member; `set_weapon('orange')` is an error listing the accepted symbols) or `Symbol` (any name). No widening, and no untyped symbol literal. `Symbol` is the open, compile-time-only form and an enum is the closed, runtime-representable one, so a bare `Symbol` cannot be stored in a field. What a `Symbol` names is still checked by whatever consumes it. The gain: symbol literals make `person.set_attribute('age', 2)` and `Weapon.attributes['damage']` writable in source, which the language cannot express today. See [Enums](#enums-implemented). |
| 2026-09-19 | **D11** (decided by Mortaro): **reflection reads at two levels and the same word is right at both.** A class object is an instance of `Spite.Class` (D6), so `weapon.attributes['damage']` is the value while `Weapon.attributes['damage']` is the `Spite.Attribute` describing the field; a PascalCase receiver is the class, the same convention section 17 uses for `user32.Input`. This also stops `attributes[symbol]` being a special compile-time-only form bolted onto Symbol codegen -- it is ordinary indexing at whichever level the receiver names. The shape of the class-level mapping still needs design (milestone 10). |
| 2026-09-19 | **D12** (decided by Mortaro): **every reflection object lives in the `Spite` namespace** -- `Spite.Class`, `Spite.Function`, `Spite.Argument`, `Spite.Attribute`, and whatever reflection grows next: "super flexible but very clear". `Spite.Function` carries `.name`/`.arguments`/`.returns`, `Spite.Argument` carries `.name`/`.class`. And `.class` everywhere is a real `Spite.Class`, not the type name as a `String` (which is what the compiler has today), because identity comparison is what makes reflection composable -- `function.arguments[0].class == ServerContext` is checked, while `.class.name.starts_with("Server")` quietly turns false when a class is renamed. Printing a `Spite.Class` still prints its `.name`. Milestone 10; see [Reflection objects](#reflection-objects-partial). |
| 2026-09-19 | **D13** (decided by Mortaro): **a function's first parameter decides which bundle it is compiled into** -- `ServerContext` means server-only (the client gets a generated network stub), `ClientContext` means client-only, neither means isomorphic. No annotation and no compiler trick: the framework reads it off the signature with D12 reflection, comparing class identity. The marker is the capability, so it cannot drift -- a function cannot claim to be server-side without holding server capabilities, and the same shape extends to `DatabaseContext`/`FileSystemContext` with no new language feature. A a `union Context` of `ServerContext` and `ClientContext` gives exhaustiveness when a third environment appears. The server function's body is never emitted into the client bundle, so secrets and server-only helpers are removed by the per-bundle tree shaking section 11 already does -- which is what makes the split safe rather than merely convenient. Routes generate from class and function name; a `type` shape is the wire format; parameters and return types crossing the boundary must be serialisable, with a diagnostic naming what is not. Blocked on the errors design (a network call fails in ways a local call cannot). Milestone 12; see [Isomorphic classes](#isomorphic-classes-where-a-function-lives-planned). |
| 2026-09-20 | (proposed by Claude, unconfirmed) Milestone 9c, second batch item 1: `spite folder/file.spite` treats `folder/` as a loaded root -- the entry file's own folder is loaded and every sub folder is a namespace, recursively, exactly as a `load("folder")` would (section 11). Implemented as the discovery pass's `discoverProgram` walking the entry folder as root 0, with `tests/entry_folder_namespace/` and `examples/game` covering it. |
| 2026-09-20 | (proposed by Claude, unconfirmed) Milestone 9c, second batch items 2, 4, 5: `do` is fully removed (`if value do name { }` is a parse error naming the plain-`if` narrowing form; no leftover `do` token kind in bootstrap), shadowing is allowed in an inner scope (same-scope redeclaration stays an error), and the unused rule is enforced with the four dictated-signature exemptions -- operator functions, Symbol codegen templates, setters/getters answering attribute access, and functions that replace another through class reopening -- each now under test (section 5). |
| 2026-09-20 | (proposed by Claude, unconfirmed) Milestone 9c, second batch items 3, 6, 8: `add`/`pop` compile errors name `append`/`prepend`/`remove_last`/`remove_first` (section 15); the compiler prints nothing called a "warning" -- the last one, a `--development`-only skipped-class notice, is now plain informational `note:` text (sections 12/13); and `count_<member>()` counts the true elements of a Bool attribute while a numeric attribute is a compile error naming `sum_<member>()` (sections 8/15). |
| 2026-09-20 | Milestone 9c's implemented second-batch items (1, 2, 3, 4, 5, 6, 8) moved out of section 16 into their proper sections; section 16 keeps item 7 (now numbered 1) plus the third batch. Milestone 10 splits into 10a "reopen standard library classes" (section 16 item 1) and 10b "Ruby grade compile time reflection visible in final classes" (section 16 item 2); see PLAN.md. |
| 2026-09-19 | **D15** (decided by Mortaro): **every standard library template names a `<member>`, and a member is a field or a zero-argument function without distinction** -- `map_age` and `map_get_age` are the same call. Reading an attribute already goes through `get_<attribute>()` (section 5), so the field case is the function case with an optimisation the writer never thinks about; what a template constrains is arity and return type, never whether a member is stored or computed. `filter_`/`count_`/`any_`/`all_` want `Bool`, `sum_` a numeric, `sort_by_` something ordered, `find_by_(value)` something comparable, `map_` anything, `each_` nothing in particular. `each_<function>()` and `map_<attribute>()` stop being different families. Consequence: `todos.map_render()` -- a list of components -- needs no new feature, and a member that does not fit is a compile error naming the member, its return type and the requirement. This makes the uniform access principle, which section 5 already committed to for `person.age`, true for the standard library too. |
| 2026-09-19 | **D16** (decided by Mortaro): **a `type` may require functions, not only attributes** -- a `type Renderable` requiring `render(): Element` -- and a value matches it by shape exactly as an attribute-only `type` already does. This is what lets a collection hold "any class that responds to `render()`" (a children list of components, section 17) without a union listing every class, and it makes the respond-to check section 16 item 9 promises expressible as an ordinary type rather than as reflection. |
| 2026-09-19 | **D17** (decided by Mortaro): **functions are first-class values, and always bound to an instance.** Passing `pretty_print` inside a class passes that function paired with the instance doing the passing, so `console.log(pretty_print)` calls it exactly as the owning instance would have. There are no free functions and no closures: a function value is `{instance, function}`, which is one retain in a reference-counted language and captures nothing else, so no local ever escapes its scope. Consequences: section 17's "Not designed yet" callbacks become solvable, since `{instance, function}` is precisely what a C callback plus its `void*` user data wants; and an event handler can be written `onclick: increment` (signature-checked) rather than as the symbol `'increment'` (D10). **Open:** how a function-valued parameter's type is written -- a one-function `type` (D16), or a signature form such as `func(value: String): String`. |
| 2026-09-19 | **D18** (decided by Mortaro): **Spite has no JSX and no trailing-block syntax; markup is ordinary metaprogramming.** A tag comes from `missing_function` (D4), attributes and children are ordinary literals, components are matched by shape (D16), handlers are bound functions (D17), a list of children is `map_<member>()` (D15), and a conditional child is a `when(condition, element)` call returning `Nullable<Element>` that the framework skips. **Children are variadic at no cost**, because a Symbol-codegen template monomorphizes per call site -- `html.div({ class: "card" }, html.h1(title), TodoForm({ ondone: add_todo }))` needs no varargs feature and no list brackets. Rejected: a JSX-like literal (a second grammar, in a language whose philosophy is a very small feature set; it also fixes neither the conditional nor the mapping problem, which are what actually make markup awkward); and a Ruby-style trailing block `f(x) { a b c }` == `f(x, [a b c])` -- it serves exactly one shape, since a query is `database.filter_age_greater_than(10).sort_by_name()` and routes are plain statements, so it would be a sixth meaning for `{ }` bought for a single case. The justification for accepting the verbosity is the language's first philosophy line: this is annoying for a human to *write* and cheap for an AI to write, while a human can still easily *read* it -- and the verbosity buys compile-time checking of tags, attributes and handler names that JSX cannot do, plus static subtrees folding into constant instruction buffers. |
| 2026-09-19 | **D19** (decided by Mortaro): **the DOM is bridged exactly like a native library.** `Html` is the web counterpart of `DynamicLibrary` (D4): a singleton (D8) whose `missing_function`/`missing_attribute` hooks resolve names against the browser instead of against a `.dll`, with `Html.Node` wrapping one node handle (an index into the JavaScript-side table, freed by `drop()`). The naming rule is `'camel_case'`, which the `Naming` enum already has, because the DOM is camelCase: `document.create_element("div")` is `createElement`, `node.text_content = "Hello"` is the `textContent` property, `root.append_child(node)` is `appendChild`. A call is a method and a read or write is a property, the same split D4 uses for functions versus constants. So there is one foreign-bridge pattern with two backends, and `Html` is the low-level layer that D18's markup builder compiles down to, exactly as `Mouse` sits on `DynamicLibrary`. **Open:** D18 writes the markup builder as `html.div(...)`, which collides with this name -- the builder probably needs a different one (`Markup`, `View`), since `div` is an element to create and not a DOM method. |
| 2026-09-19 | **D20** (decided by Mortaro): **a class declares which targets it compiles for, and using it anywhere else is a compile error.** A function of `Spite.Class` (D6) -- `func targets(): List<Symbol> { return ['native'] }` -- defaulting to every target, so `DynamicLibrary` returns `['native']` and `Html` returns `['web']` and each crashes the build against the wrong `$target`, naming the class, the target and the flag. This replaces section 17's ad-hoc rule that `File`/`Directory`/`Process` are an error under `--target=web`: they declare `targets()` like anything else, and the special case disappears. The check runs after compile-time folding and tree shaking, so `if $target == 'web' { load("platform_web") } else { load("platform_native") }` is not an error -- only a class still reachable in the built program is checked. Uses D10 symbol literals and D6's hook mechanism, so it adds nothing new to the language. |
| 2026-09-19 | **D21** (decided by Mortaro): **the markup builder and the browser bridge are two different classes**, which resolves D19's open item. One produces element trees (`var html = HypertextLanguage()`, so `html.div(...)` is an element to create) and the other is the DOM itself (`BrowserLibrary`, where a call is a DOM method and a read or write is a property, D19). Names are not final; the concept is. The builder compiles down to the bridge, exactly as `Mouse` sits on `DynamicLibrary`. |
| 2026-09-19 | **D22** (decided by Mortaro): **JSON is a use of reflection, not a library.** Writing is a string generated by walking a class's attributes (D11/D12) and is emitted per class as visible generated source (section 16 item 9), only for classes that use it; reading fills a class through symbol-keyed writes (`set_attribute('name', value)`, D10 plus Symbol codegen), and a `type` shape is the natural JSON shape, which is what section 7 said inline types were for. It is the same machinery D13's wire format needs, so the two share one implementation. **Refinement (proposed by Claude):** `to_json()` needs no failing variant at all -- the compiler already knows every attribute's type, so an unserialisable class is a compile error (the check D13 already requires), which makes writing total. Only *parsing* can fail at runtime, on malformed input, so the pair Mortaro described applies there alone: a soft form returning `Nullable<T>` that `assert` narrows, and a loud form that crashes with position and reason for when an AI wants the report instead of a null. **Open:** the naming (`to_crashing_json` was Mortaro's sketch, "or something like that"); Claude prefers a suffix, `parse_json()` / `parse_json_or_crash()`. |
| 2026-09-19 | **D23** (decided by Mortaro): **after the bootstrap, the standard library is written *with* the metaprogramming, not merely alongside it.** JSON is the worked example: reflection generates it rather than a hand-written serialiser per type. This is the other half of D14 -- moving the standard library into Spite is not a transcription of the C, it is a rewrite that uses `Spite.Class`/`Spite.Attribute` (D12), symbol literals (D10), member templates (D15) and function of `Spite.Class`s (D6) as its ordinary tools. |
| 2026-09-19 | **D24** (decided by Mortaro): **Spite has three outcomes and no others -- a compile error, an `assert`, or a crash.** There are no exceptions, no error unions, no bubbling and no error values carrying a message, because "errors and exceptions tend to be useless: they tell us a message we have no action to take about them". (1) **Compile error** for anything the compiler can know -- already six decisions deep: unserialisable types (D13/D22), the wrong `$target` (D20), an unfilled codegen hole (D9), a missing foreign symbol (D4), a member that does not fit a template (D15), a symbol outside its enum (D10). (2) **`assert`** is for when the developer wants the program to keep running: it returns the default value of the return type, which is a deliberate substitution rather than a check that fires. (3) **A crash** is for when everything should halt so the code gets rewritten. **`Nullable<T>` is the only runtime failure value and it carries no reason** -- the caller either narrows it with `assert` and continues, or handles the absent case with `if ... do`. **The rule that makes this survive real code (proposed by Claude): if a distinction is actionable, it is data, not an error** -- a caller that must tell a timeout from a rejection takes back a `type`/`union`/enum modelling that, which is ordinary data with ordinary handling, not an error channel in disguise. Consequences: D13's failed network call returns `Nullable<T>` and does not crash, since the program is not wrong; D22's parse pair is exactly rule 2 versus rule 3; and the FFI keeps crashing on a missing library or symbol (D4). **Open:** how a deliberate crash is spelled, and the requirement that its report name the Spite file, line and call chain -- a crash is only useful to an AI if it does, which is a real demand on the C backend. |
| 2026-09-19 | **D25** (decided by Mortaro): **a crash always reports backend information and the trace of every `assert` that failed before it.** This answers the one reservation Claude raised against D24 -- that `assert` substituting a default is a silent failure, the same swallowed-null the model exists to prevent, and the one an AI can never learn about -- without changing what `assert` does. The developer still chooses to keep running; the choice simply stops being invisible. A crash report carries: the message, the Spite file, line and function, the call chain, the generated-C location, and the preceding failed asserts, each with its source condition, the default it substituted and where it fired. Those asserts are exactly the causal trail explaining how the program reached the state that crashed. **Cost is only on the failure path** -- a passing `assert` already branches, so nothing is added to the normal path, and the trace is a fixed-size ring buffer plus a total count, so it allocates nothing and cannot grow without bound. The same applies to compile-time evaluation (D6 overrides, D22 generation, milestone 10's evaluator): a failure there reports the compile-time asserts that fired, which is where an AI in an edit-compile loop actually reads it. **Requirement on the backend:** Spite file and line must survive into the generated C -- `#line` directives are the ordinary way, and they also make the C compiler's own errors point back at Spite source, which the project wants regardless. **Open:** the ring buffer size, and whether a `--development` build records passing asserts too. |
| 2026-09-19 | **D26** (decided by Mortaro): **`assert` is control flow, not validation** -- a guard clause meaning "this value is absent, so there is no more logic to do here, but the program is fine and keeps serving everything else". It is for cases known to be nullable where absence is not a problem, which is why a web server or a game built in Spite should rarely crash. **Two refinements (proposed by Claude, unconfirmed):** (1) `assert` is sound exactly when the return type can express "nothing" -- `Nullable<T>`, an empty `List<T>`, an empty `String`. A bare scalar return cannot: `func count_user_orders(): Int` guarded by `assert find_user(...)` returns `0` both for "no such user" and for "a user with zero orders", so absence and answer become indistinguishable at the caller. Proposed as a lint: `assert` in a function returning a bare scalar is an error, escaped by returning `Nullable<T>` or handling it with `if ... do` -- noting the language has no warnings, so it is error or silence, and sometimes `0` genuinely is the answer. (2) D25's crash trace should record **predicate asserts only, not narrowing ones**. If narrowing is routine control flow then a busy server fires thousands of them, and a ring buffer full of ordinary "not found" entries buries the signal; a predicate assert firing means an assumption was wrong, which is exactly what a crash report wants. |
| 2026-09-19 | **D27** (decided by Mortaro): **`assert` is legal only in a function that returns nothing, or returns `Nullable<T>`** -- the two cases where the substituted default is honest, since void has nothing to say and `null` says exactly "absent". Anything else is a compile error naming the return type. This supersedes D26 refinement (1), which only caught bare scalars: an empty `List<T>` or an empty `String` has the same defect as `0`, because the caller cannot tell "no user" from "a user with no orders". A function that wants a guard and returns something else must choose deliberately -- widen the return to `Nullable<T>`, or handle absence with `if ... do` and return the value it actually means. **Edge (proposed by Claude, unconfirmed): a constructor is banned too.** It has no return type, so the rule as stated admits it, but "returns nothing" is false there -- the caller receives an object with every field at its default and cannot distinguish it from a properly built one, which is exactly the surprise this rule exists to prevent. The diagnostic should name the two alternatives: check before constructing, or move the lookup into a function returning `Nullable<T>`. **Second-order effect (Mortaro): the rule forces smaller functions.** A function that deals with a reference and wants a guard cannot keep everything in one body -- it has to split, so the lookup lives in a small function whose signature honestly says `Nullable<T>` and the caller handles absence with `if ... do`. Decomposition then comes from the type system rather than from style advice, which is what works on an AI: there is no "should" to ignore. Fragmentation is cheap here (the extra functions are private, tree-shaken and visible in `--final-classes`), and smaller honestly-typed functions also make D13's signature-driven bundle split cleaner. |
| 2026-09-19 | **D28** (decided by Mortaro): **`assert` is banned in a constructor.** D27 admits it only because a constructor has no return type, but "returns nothing" is false there -- the caller receives an object with every field at its default and cannot tell it from a properly built one. Mortaro's reasoning is the general form: **a constructor is setup, not logic** -- if something can be invalid inside one, function logic has been put where only setup belongs. The push this creates is a good one: a constructor takes already-resolved values rather than identifiers to resolve, so the caller does the lookup (`var user = find_user(id)`, then `if user do found { Profile(found) }`) and the constructor just sets fields. **Two things this surfaces.** (1) Section 17's `DynamicLibrary` constructor ends with `assert _handle`, which is now illegal -- and it was wrong anyway: a missing library is not "absence is fine, stop here" but "halt to fix this", so it is a crash (D24 rule 3), which is what section 17's own prose already says. The `Html`/`BrowserLibrary` constructor (D19) has the same fix. (2) **The entry constructor is exempt (proposed by Claude, unconfirmed)**: the 2026-09-18 decision makes the entry file's constructor the program itself, so it is `main` wearing a constructor's name, and a program body is logic by definition. Applying "setup only" to it would contradict that decision or force a ceremonial one-line delegation. **Open (proposed by Claude):** whether the rationale generalises past `assert` into "a constructor performs no fallible work at all" -- no file reads, no lookups, no network -- which is the same principle stated as a rule rather than as a consequence.
| 2026-09-19 | **D29** (decided by Mortaro): **the entry constructor is not exempt from D28** -- Claude's proposed exemption is rejected. A program constructor that grows complex is split into smaller functions that the constructor just calls, which is the same decomposition D27 already forces everywhere else, so what looked like ceremonial delegation is simply the correct shape. D28 therefore stays one uniform rule with no special case in the compiler: no constructor asserts, the entry class included. A side benefit for the human-skimming goal (philosophy line 1): the entry constructor ends up reading as a table of contents for the program -- the sequence of things it does -- with the logic one level down in functions that may guard freely, since they return nothing or `Nullable<T>`. |
| 2026-09-19 | **D30** (decided by Mortaro): **`crash` is a keyword, not a standard library call**, so the compiler can take context from the backend rather than from a message string. **Form (proposed by Claude): it mirrors `assert` exactly** -- same polarity, same shape, different severity. `assert database.connect()` returns the default on falsey and keeps going; `crash database.connect()` halts on falsey. The compiler captures the condition's source text and every operand value, so the report needs no written message and nothing can drift out of sync; bare `crash` stays legal for an unreachable branch and reports its enclosing context. Mortaro also sketched `crash not database.connect()`; Claude argues against that polarity, since `assert` firing on falsey and `crash` firing on truthy makes two statements that look alike behave oppositely. **What makes it more than sugar: `crash user` narrows a `Nullable<T>` like `assert user` does but never returns, which fills the hole D27 opens.** A function returning `Int` that needs a guard previously had to widen to `Nullable<Int>` or use `if ... do`; now absence-is-a-bug has its own honest form. The three cases cover the space: absence is fine -> `assert`, absence is a bug -> `crash`, absence is meaningful -> `if ... do`. |
| 2026-09-19 | **D31** (decided by Mortaro): **JSON is not the wire format.** "I do not really need JSON in 99% of the cases, the main consumer of the language is AI... it is a web convention that creates inefficiency for human readability, but a human will never read those." D13 sends a compile-time-derived binary packing instead: raw fields in declaration order, no keys and no parsing, because **JSON's entire cost is self-description and there are no unknown consumers** -- the client and server bundles compile from the same source, so both ends already know the exact `type` layout. This compounds with the wasm decision, since JSON would force string marshalling across the JavaScript boundary on every request, which is the expensive part of wasm interop; with it gone, the DOM command buffer and the RPC channel are both opaque byte buffers. D22 is amended: JSON stays in the standard library as a format for foreign systems that demand one, not as the default. **Two requirements (proposed by Claude):** a schema hash in the handshake, crashed on mismatch, since an old client against a new server is the one case that can skew; and `--development` decoding a payload to JSON on demand (the compiler knows the schema), so readability is available exactly when a human is looking and never paid for otherwise. |
| 2026-09-19 | **D32** (decided by Mortaro): **crash sites are identified by a compile-time id, and the compiler emits an id-to-source map as a build artifact** rather than embedding the information in the program. A crash prints its id and the runtime values; the source location, the condition text and the operand names live in the sidecar, so the binary carries none of it. Two payoffs: the program shrinks (condition text for every `crash`/`assert` site is kilobytes that matter most in a wasm module, where size is startup time), and **the same logical site carries the same id across every `$target`, so a crash from the server bundle and one from the browser bundle can be compared directly** -- which is exactly what D13's isomorphic classes need, since the same class is compiled into both. It is D31's principle applied again: the static half is known at compile time, so it never travels. **Requirements (proposed by Claude):** (1) **the id must be content-derived, not a counter** -- a hash of namespace, class, function, the per-function ordinal of the site and the condition source. A counter renumbers everything when a line is added, which breaks both old reports and cross-target identity, which is the whole point. (2) The map is an artifact to archive, like a PDB, carrying a build hash so a report can be matched to the build that produced it -- pairing with D31's schema hash. (3) `--development` embeds the text directly, since a local run should not need a lookup; only optimised builds emit bare ids. (4) D25's assert trace becomes a ring buffer of `(id, values)`, which is far cheaper than holding strings. (5) D25's `#line` requirement narrows to compile-time diagnostics only -- the runtime path no longer needs it. **Format (Claude's preference): a greppable line-based text table, one line per id**, so `grep 4172 program.crashes` answers the question without a parser. |
| 2026-09-19 | (decided by Mortaro, confirming D30) **`crash` as a keyword is accepted, in the mirror-of-`assert` form** Claude proposed: same polarity, `crash <condition>` halts on falsey, bare `crash` for an unreachable branch, and `crash user` narrows a `Nullable<T>` without returning. `crash not database.connect()` is dropped. `crash` joins the keyword list in section 2. |
| 2026-09-19 | **D33** (decided by Mortaro, adopting Claude's preference): **the D32 crash map is a greppable, line-based, tab-separated text table**, written next to the output as `<output-name>.crashes`, so `grep 4172 program.crashes` answers the question with no parser and no tooling. One line per site, **sorted by id** so archived maps diff cleanly between releases, with fields: id (content-derived, hex), file, line, column, class and function, kind (`crash`, `assert-predicate`, `assert-narrowing`), the condition's source text, and the operand names with their types. Tabs are safe as the separator because a condition is a single-line expression and the formatter forbids tab indentation. The file opens with a `#` comment line -- Spite's own comment character -- carrying the format version and the build hash that D32 requires for matching a report to its build. **Runtime output:** one tab-separated line per event, prefixed `spite.crash` or `spite.assert`, so a crash is greppable out of mixed program output. **Values are rendered as text at crash time but stored raw in the assert ring buffer** -- a crash happens once and then the program is dead, so it should be informative rather than fast, while a narrowing assert may fire thousands of times an hour (D26) and must stay cheap until something actually dumps it. |
| 2026-09-20 | **D34** (decided by Mortaro): **a comment is one line, and it is nothing but a link to a markdown section.** `# bootstrap/BOOTSTRAP_PLAN.md#ownership-aliasing` -- a path to a markdown file in the repository plus a GitHub-format anchor, and no other text. The compiler validates that the file and the anchor both resolve. Anything else is an error, including an attempt to escape into `//` or `/* */`, which the lexer recognises specifically in order to reject them with the explanation rather than a parse failure. **The error message teaches rather than refuses** (Mortaro): it states the one legal form and then asks whether the note is necessary at all -- if a reader could derive it from the code, delete it; if it is durable knowledge, write the section first and link to it; if it warns against a change, a test or a compiler diagnostic pushes harder than prose. **Why this shape.** Prose in source rots into a lie, while a link either resolves or fails the build. Re-derivable notes should not exist, because an AI reconstructs them from the code for free. And zero comments was rejected on token economics: a grep costs tokens on every edit to discover *absence*, which is the common case, while a link costs about ten tokens only where there is something to say -- absence becomes free and trustworthy. **A comment may only appear outside function scope** (decided by Mortaro): a line that seems to need explaining becomes an extracted, named function instead, which D27 already forces elsewhere -- and unlike a comment, the name is visible to `Class.functions`, to `--final-classes` and to every tool. **Migration is real: 405 comment lines across 59 `.spite` files, plus every fenced sample in `docs/`, which `check.sh` compiles.** |
| 2026-09-19 | **D14** (decided by Mortaro): **pure Spite is the target -- the hand-written C runtime is a bootstrapping stage, not the design.** `src/runtime/spite_runtime.h` (722 lines) and `src/runtime/spite_repl.h` (1392 lines) exist because the compiler needed a `String` before Spite could express one; D6 already makes the current arrangement untenable, since `Spite.Class` cannot be "an ordinary standard library class with an ordinary declaration" while the standard library is C. Destination: `String`/`List<T>`/`Dictionary<T>` and the reflection tables become ordinary `.spite` sources, `File`/`Directory`/`Process` become `DynamicLibrary` calls (D4), and the floor is five to ten compiler intrinsics for raw memory rather than a runtime file -- because a `String` needs allocation, allocation needs pages from the operating system, and asking for them through the FFI needs a `String`. Purity costs no performance: the output is still C, so a Spite-written `trim()` meets the same optimiser. It also makes standard library classes reopenable (section 16 item 7) and REPL-inspectable by removing their special status. The web shim is the one exception by construction -- it runs in the JavaScript virtual machine -- but its target is zero hand-written lines, generated from `external js` declarations and an opcode table, so no second language lives in the repository. Milestone 15; see [Pure Spite](#pure-spite-dissolving-the-runtime-planned). |
| 2026-09-20 | **D35** (decided by Mortaro): **the concurrency model ships in the language, singular, and there is no bring-your-own-runtime.** Rust's split is the anti-goal: `async fn` desugars to a state machine implementing `Future`, the language shipped the trait but deliberately no executor, and three incompatible runtimes filled the hole -- after which every library had to declare which one it targeted. The mechanism to avoid is precise: **never define a protocol that requires an external driver.** Two constraints Mortaro set agree with each other here -- the only model simple enough to ship *as* the one true implementation without hiding costs is a thread pool plus fork-join, which is exactly why a work-stealing green-thread executor cannot be blessed and why Rust did not try. **Mechanism (proposed by Claude, unconfirmed):** (1) **No function coloring, ever -- concurrency is a property of the call site, not of the function.** `file.read` stays an ordinary function; a caller that wants it concurrent writes `var reading = Task(file.read)` and later `reading.wait()`, which works because D17 already made a bound function a first-class value. No `await`, no wrapper return type, nothing viral. (2) **Join on drop:** a `Task` handle is refcounted, so `drop()` joins and scope exit is a join point -- structured concurrency falls out of D1 with no new machinery. (3) **Data parallelism is a template family** (D15): `entities.parallel_each_update()` monomorphizes into a fork-join over `List<T>`'s contiguous buffer, no per-item allocation and no scheduler. (4) **Race safety by a syntactic rule, not a borrow checker:** a parallel member may only reach its own instance's state and its locals, which is checkable by walking the body because Spite has no globals and no pointers; reaching through a field holding another class is an error naming the field. (5) **Native only** (D20): wasm cannot block, so `wait()` has no implementation there and web concurrency is D13's request/response instead. **Open:** shared mutable state across threads (a channel or an owning mutex -- ECS mostly avoids needing it), and cross-entity reads in a parallel pass, which rule (4) does not cover. |
| 2026-09-20 | **D36** (decided by Mortaro): **hidden costs are bad, hidden optimisations are good** -- the rule is not symmetric. Code that runs faster than a reader expects is a pure win and needs no announcement; code that runs slower than a reader expects is the only real surprise. So the compiler may freely make a program faster behind the source (copy elision into views, deferring an independent read onto a task, folding a static markup subtree) without asking or marking it. **Consequence for the build report:** it lists the *failures*, not the successes -- not "400 copies elided" but "3 copies could not be elided, and the callee that writes the field". Every line is then actionable, and the report is small enough to read. |
| 2026-09-20 | **D37** (decided by Mortaro): **the compiler injects the REPL drain points; the user never writes one.** A user will forget, and a forgotten drain means a dead REPL discovered much later. The injection rule is principled rather than game-specific: **a drain goes wherever the program is already waiting** -- `Program().sleep(...)`, a blocking IO call, a `Task` join (D35), and the join at the end of a parallel pass. Those are exactly the points where state is consistent and no parallel pass is writing, so a command sees a whole world rather than a half-stepped one -- correctness, not only latency. Mechanism: the REPL thread does the blocking socket read and pushes parsed commands into a lock-free queue, so an injected drain is one uncontended atomic load (about a nanosecond) when nothing is pending, rather than a syscall. Cheap enough to leave in a shipping build. A game frame ends in a sleep or a present, so a frame loop is covered without the engine doing anything; a server is covered by its IO waits. **If a REPL build has no injection point reachable from the entry constructor, that is a compile error** naming the problem -- a program that never waits anywhere would have a REPL that never answers, and the author should learn that at build time. The same injected points serve live reload (milestone 6b) and anything else that needs an idle moment. |
| 2026-09-20 | (decided by Mortaro, extending D37) **`Console.read_line()` is a REPL drain point too.** A REPL, and any interactive tool, spends nearly all its time waiting on console input, so a blocking read from stdin is the same kind of already-waiting moment as `sleep`, a file read, a `Task` join or a parallel-pass barrier. It is also the one that covers interactive programs, which none of the others do -- a tool that only ever waits on a person would otherwise have no injection point at all and would fail D37's compile-time check. |
| 2026-09-20 | **D38** (decided by Mortaro): **a dependency is a git URL pinned to a commit hash, written in the `load` call, and the ordinary compile fetches it.** No package manager, no registry, no lockfile, no separate `spite fetch` step: `load("github.com/mortaro/engine@a3f2c91")` is the whole mechanism, and compiling a program that names a dependency it does not have fetches it then. **The commit hash is mandatory** -- the compiler rejects a git dependency without one, because a moving reference is how a dependency changes under you (`event-stream`, `colors.js`), and a pinned hash makes it immutable. The pin lives in the source, which is already versioned, so **the lockfile problem disappears rather than being solved**: there is no manifest to drift from. The spite being answered is twofold -- C++'s thousand ways to set up a project, and the way "every day a new ecosystem arises to compete with npm". Git is the registry, so there is no central index for a competitor to fork from, which is D35's structural argument applied to packaging. Two properties come free from Spite's existing rules: a package is `.spite` source and nothing else, so **there are no install scripts** -- npm's `postinstall`, the single biggest supply-chain vector, has nowhere to exist; and `load` already takes a literal, so every dependency in a program is known at compile time and can be reported. **A version mismatch is a compile error that calls the user's attention** (decided by Mortaro): two pins of the same package would otherwise silently monkey patch each other through section 11's reopening rule, replacing one version's functions with another's -- the worst possible outcome, so the compiler names both pins and the human picks. **Open (proposed by Claude):** whether fetched source is committed into the repository (auditable, offline, reproducible from a clone) or kept in a cache such as `.spite-cache/` (smaller repository, needs the network on a fresh clone). Note that fetch-on-compile means the first build of a program is not hermetic and needs git and a network. |
| 2026-09-20 | **D39** (decided by Mortaro): **a function value is an instance of `Spite.Function`, generic over its arguments and its return** -- `Spite.Function<String, Bool>` -- so a function-valued parameter is typed the way any other generic type is, which answers the question D17 left open. The codegen values are positional and the **last one is the return**; everything before it is an argument, in order, so `Spite.Function<Bool>` takes nothing and `Spite.Function<String, Int, Bool>` takes two. They line up with `Spite.Function`'s own reflection members (D12): the arguments are `.arguments`, the last is `.returns`. This is the language's first variadic generic -- every other takes a fixed count declared on its constructor (D9), while `Spite.Function` is a built-in taking as many as the signature has. **The value and its reflection are the same class**, which is the point: `Weapon.functions[0]` and a `pretty_print` passed as an argument are both `Spite.Function` instances, so reflection stops being a mirror held alongside the language and becomes the language describing itself -- what "everything is a class, including classes" has claimed since section 8 was written. A function value carries `.name`/`.arguments`/`.returns` because it *is* the reflection object, and D17's `{instance, function}` pair is simply what that instance holds. **Open (proposed by Claude):** how a function returning nothing is written, which is now REQUIRED rather than optional: with the return last, `Spite.Function<String>` reads as "takes nothing, returns a String", so a function taking a `String` and returning nothing cannot be written without a class meaning "returns nothing" -- and that class is also what `.returns` reports for every such function; and whether calling a function value goes through an operator (`printer(value)` as `call`, in section 5's operator table) or a named member. |
| 2026-09-20 | **D40** (decided by Mortaro): **a `Spite.Function` knows its owner, and every call of a function value goes through `call_function()`.** `.owner` is the instance the function is bound to, so D17's `{instance, function}` pair stops being a hidden representation and becomes an ordinary readable field -- `pretty_print.owner` is the `Console` that will run it, and bound-ness is reflectable like everything else. `printer(value)` is `printer.call_function(value)`, which answers D39's open question by applying section 5's operator rule once more: the call operator maps to a named function exactly as `+` maps to `sum` and `a[x]` maps to `get_at`. The value of one chokepoint is that every invocation of a function value passes through it -- an event handler firing, a foreign callback arriving from C (section 17), a framework dispatching -- so there is one place to instrument and one place where the owner is applied, and because `Spite.Function` is an ordinary standard library class, `call_function` can be reopened (D7) to trace every callback in a program. The indirection is paid only where it was already accepted: an ordinary call such as `person.set_age(2)` is direct and never builds a `Spite.Function`; only a function used *as a value* routes through `call_function()`. |
| 2026-09-20 | **D41** (decided by Mortaro): **a namespace is an instance of `Spite.Namespace`**, for the same reason D12 made `.class` a real class: a string can only be matched, an object can be compared, walked and enumerated. `Spite.Class.namespace` is one of these rather than a dotted `String`, and printing it prints the dotted name the way printing a `Spite.Class` prints its `.name`. It mirrors what section 11 already builds -- namespaces come from folders, so `Spite.Namespace` is that tree made readable, with `.parent` walking up and `.classes`/`.namespaces` walking down. The consequence worth having: a `load(...)` root becomes something a program can enumerate, which is what D13's isomorphic split needs when it partitions a package's functions by their first parameter, and what the deferred compile-time class generation (Mortaro, 2026-09-19) would extend. **Proposed by Claude, unconfirmed:** the global namespace is a real `Spite.Namespace` with an empty `.name` rather than `null`, so `.namespace` never needs narrowing and `.parent` terminates at a root instead of at an absence; and its `.classes` reflects section 11's merged result, after every root that reopened into it has been applied -- the same thing `--final-classes` prints. |
| 2026-09-20 | **D42** (decided by Mortaro): **reflection may be as detailed as it likes, because what is not used is not emitted.** Most of the metadata will never be reached by a given program, and reaching for it is the only thing that makes it cost anything -- "if it has a cost it is because we needed it anyway". This holds here in a way it does not elsewhere: Spite's reflection is resolved at compile time, so which parts a program touches is **statically decidable** and tree shaking is exact rather than conservative. A language with runtime reflection cannot prove a class is never reflected on and must keep the tables for everything; Spite can prove it. Section 8 already worked this way for `value.attributes`; D42 extends the rule to the whole family (`Spite.Class`, `Spite.Function`, `Spite.Argument`, `Spite.Attribute`, `Spite.Namespace`, and whatever follows) and gives milestone 10 its direction: **err toward more detail**, since an unused field costs a program nothing while a missing one costs a design. It is also what keeps D32's wasm size argument intact -- a rich reflection family does not inflate a module that never reads it. |
| 2026-09-20 | (decided by Mortaro, amending D41) **`Spite.Class.namespace` and `Spite.Namespace.parent` are `Nullable<Spite.Namespace>`, and the chain ends at `null`** -- rejecting Claude's proposal of a root namespace object. A root object would have read as though every namespace were nested inside something, and inside the metaprogramming package in particular: a namespace *uses* `Spite.Namespace`, it is not contained by it. A top-level namespace has no parent, so you walk up with `class.namespace.namespace` for as long as that can be satisfied. |
| 2026-09-20 | **D43** (decided by Mortaro): **`assert` narrows every link of a chain, not only the last.** Walking a nullable structure would otherwise need one `assert` per hop, and "a thousand asserts" is a tax rather than a language -- the need became obvious the moment D41 made namespaces a nullable chain. `assert class.namespace.namespace` proves the whole path, so every prefix of it is a plain value for the rest of that block and any nested block. It is the rule section 5 already had for `assert value and other_condition`, applied along a member chain instead of across an `and`. It narrows the asserted path and its prefixes only -- a sibling path stays `Nullable<T>` -- and `crash` narrows a chain identically, since it narrows exactly as `assert` does but never returns. Reading through a link that has not been narrowed is still an error: this removes the verbosity of proving a path, not the requirement to prove it. |
| 2026-09-20 | **D44** (decided by Mortaro): **Spite emits C and takes no dependency on LLVM; the C compiler is the user's to bring.** "Each person brings its own C compiler." The language does not emit LLVM IR and is not bound to it as a backend -- which is a different statement from refusing to *use* clang, since a user may well bring it (this machine's own compiler is the clang inside Visual Studio). What is refused is binding the language to one backend. The reason is philosophy line 5, compilation speed and live reload: emitting C keeps the final step swappable, so a fast compiler can be used while iterating and an optimising one for release, and emitting LLVM IR directly would trade that away for LLVM's codegen speed, which is the slow half of every toolchain built on it. It is also what makes the bootstrap honest -- the committed seed is C, so any machine with a ubiquitous tool can rebuild the compiler from source rather than trusting a prebuilt binary. `check.sh` finds `cc`, `clang` or `gcc`, looks inside Visual Studio on Windows, and otherwise takes `CC`. |
| 2026-09-20 | **D45** (decided by Mortaro): **`Monster?` is sugar for `union { Monster, Null }`, and `Nullable<T>` is gone.** `Null` is an ordinary class whose only value is the literal `null`, as `true` and `false` are the values of `Bool`. The point is not the spelling but the semantics Mortaro wanted underneath it: nullability stops being a compiler special case and becomes a union like any other, so `switch` covers it exhaustively, `assert`/`crash`/`if ... do` narrow it through union narrowing that already exists, and the `Nullable<T>` exceptions collapse into rules the language already had. Section 10's representation survives as a codegen detail -- a union of a reference and `Null` is still just the pointer, and only a scalar needs a wrapper. Rejected on the way: `Nullable<T>` (a generic wrapper is what keeps it special), `Monster or Null` (`or` short-circuits everywhere else, so a return type reads as an expression yielding the truthy side), `maybe Monster` (a keyword bought for a shortcut), `Maybe<Monster>` as a standard library generic union (generics over unions hide the members and collide with `$` codegen replacement), and `Monster.Maybe` as a class-level member. |
| 2026-09-20 | **D46** (decided by Mortaro): **a test is a package that crashes, not a framework.** There is no `expect`, no matcher vocabulary, no reporter and no summary -- those exist to make a failure pleasant for a human reading a terminal, and "my ass that I am running them myself". A test is ordinary Spite shipped alongside the project as a package (`load(...)`), and an expectation is `crash <condition>` (D30), which already halts on falsey and reports the condition's source text with every operand value (D25, D32, D33). **The testing story therefore needs no new language feature**: crash, load and reflection are the whole of it, and `Class.functions` (D12) is enough to discover every `test_`-prefixed member without registration. **Fail-fast is deliberate and is the opposite of what a human suite wants**: a person wants all thirty failures so the work can be batched, while an AI wants one precise failure to fix before re-running, since a list of mostly-cascading failures invites shotgun fixes. The crash report is the test output. |
| 2026-09-20 | **D47** (decided by Mortaro): **`enum`, `type` and `union` declarations drop the `=` and must break lines.** `enum Job {` ... `}`, one entry per line, no commas. The `=` was only ever there so a single-line form would not look odd, and that single-line form is what creates the need for commas -- removing both removes a choice. `=` now means assignment and nothing else. The reasoning is the language's own: it reads better multi-line, and "having less options make ai less fucked up" -- a rule with one form cannot be got wrong. Writing `enum Job {` is a parse error naming the fix, the way `for`, `&`, `Heap<T>` and `generics` were retired, rather than something the formatter quietly rewrites: it is syntax, not style. **Scope:** declarations only. A list or object *literal* keeps both forms (`[1, 2, 3]` inline, or one entry per line), because a literal is data at a use site while a declaration defines a type and is read far more often than it is written. Migration is 22 declarations across `.spite` files -- none of them single-line already -- plus 13 in `manual.md` and `docs/`. |
| 2026-09-20 | **D48** (decided by Mortaro): **`Nothing` is the class a function returns when it returns nothing**, which D39 left open and which turned out to be required rather than optional: with the return in the last generic position, `Spite.Function<String>` already reads as "takes nothing, returns a `String`", so a function taking a `String` and returning nothing could not otherwise be written. `Spite.Function<Nothing>` takes nothing and returns nothing. It is also what `.returns` reports for every function declared without a return type, so it is not notation invented for the type syntax -- it fills a hole the reflection already had. Chosen over `None` because it says "no value" rather than reading like an empty collection. This unblocks milestone 16d, the zero-argument subset of `Spite.Function` that lets the test entry discover its `test_` functions instead of hand-listing them. |
| 2026-09-20 | **D49** (decided by Mortaro): **`Spite.Class.instances` is every class in the program.** A class is an instance of `Spite.Class` (D6), so the registry of its instances is the whole program's classes -- whole-program enumeration falls out of "everything is a class, including classes" rather than needing a mechanism. A test runner finds every test class with it instead of being handed them, so there is no registration, no manifest and no `run(...)` call per class. D42 pays for it: a program that never enumerates its classes never generates the registry. |
| 2026-09-20 | **D50** (decided by Mortaro): **a class with no declared constructor has an implicit one**, taking no arguments and doing nothing, so `func DictionaryTests() { }` never needs writing. Every field already has a default value (section 4), so there is nothing for such a constructor to do. This already worked: a class with no constructor function is constructed with its field defaults. **An explicitly written empty constructor is therefore a compile error naming the deletion** (decided by Mortaro, 2026-09-20), the way an unused local is -- a constructor that only declares codegen values (`func Pair<$left_type, $right_type>() { }`) is of course still legal, the way an unused local is -- the language removes redundancy rather than tolerating it. A constructor that declares codegen values (D9) or does real setup is of course still written. |
| 2026-09-20 | **D51** (decided by Mortaro): **a `var` may shadow a name in the same scope**, not only in a nested one, and the previous value is dropped as part of the shadowing. The second binding may hold a different type, which is the point: `var content = content.trim()` turns a `String?` that has been narrowed into a `String` without inventing a second name. **The order is evaluate, then drop, then bind** -- the new value is computed first, so the expression may read the binding it is about to replace, then the previous value is released (running `drop()` if that takes it to zero), then the new binding takes effect. Releasing first would make the most common use of the feature a use-after-free. The unused rule (D-log, second batch item 5) still applies to the shadowed binding, so shadowing a name that was never read is an error -- which separates a deliberate reuse from an accidental one. |
| 2026-09-20 | (decided by Mortaro, confirming D50) **An explicitly written empty constructor is a compile error**: "'Holder' has an empty constructor: delete it. A class without a constructor is already made from its defaults." A constructor that only declares codegen values (`func Pair<$left_type, $right_type>() { }`) stays legal, since it is declaring something. Implemented the same day, deleting 22 empty constructors across the compiler, `library/`, `conformance/`, `examples/` and `tests/` -- `library/nothing.spite` is now an empty file, which is a valid class and a fitting one for `Nothing`. |
| 2026-09-20 | (proposed by Claude, unconfirmed; implements D49) **`Spite.Class.instances` is built, and a class object answers `.functions`.** `Spite.Class.instances` lists the program's own classes: not the library's `Spite.*` classes or `Nothing`, not the anonymous classes behind object literals, and not a generic class before its codegen values are supplied. `some_class.functions` lists that class's functions bound to a fresh instance, made by running its constructor when it takes no arguments and from its defaults otherwise, so `call_function()` works on them; this is what lets `tests/tests.spite` find every class whose name ends in `Tests` and run its `test_` functions with nothing registered. The entry class is listed but its `.functions` is empty, because making an instance of it would run the program again. `Person.instances` (the live instances of one class) is still not built and is a compile error saying so. |
| 2026-09-20 | (implements the 2026-09-18 decision and D8; details proposed by Claude, unconfirmed) **`Person.instances` and singletons are built.** `Person.instances` lists the instances of `Person` that are alive at that moment, in the order they were made, including copies; a class is only tracked when some part of the program asks for its instances, and asking holds them alive only as long as the returned list lives. A class is a singleton when it has `func is_singleton(): Bool { return true }` written exactly so: `Journal()` anywhere returns the one instance, its constructor runs the first time it is asked for, it takes no arguments (giving it some is a compile error), and it is released when the program ends. |
| 2026-09-20 | **D52** (decided by Mortaro): **`Console` is a standard library class, not a reserved word** -- "its not a reserved word its a singleton that gives us access to the Console to print to it or to read input and what console classes normally do in other languages". It is a singleton (D8), so `var console = Console()` is the same instance everywhere and costs one object for the whole program; `Console` resolves as an ordinary class name, so it has a class object, a `.class.name` of `"Console"`, a namespace, and a place in the type system that is a class reference like any other rather than a shape of its own. The immediate gains are `read_line()`, which D37 already needed as a REPL drain point, and an attribute holding a `Console` becoming an ordinary attribute instead of one the generator deletes from the struct. `print`, `write` and `error` stay compiler-written for now because they take any number of arguments of any type, which the language cannot yet declare; everything else about `Console` is now ordinary. This is D14 continuing: the class is real first, the bodies move into Spite as the language grows to hold them. |
| 2026-09-20 | (implements D47) **`enum`, `type` and `union` declarations take no `=` and break lines.** Writing `enum Job = {` is a parse error naming the fix (`diagnostics/old_declaration_equal`), and so is putting two entries on one line or separating them with a comma (`diagnostics/declaration_one_per_line`). A list or object literal is untouched, as D47 scoped it. The 24 declarations in `.spite` files and the three in `docs/for_ai_writers.md` were migrated with it. |
| 2026-09-20 | (implements what section 5, section 9 and `docs/packages.md` already promised; found by compiling the documentation) **Four rules the manual states are now enforced.** A statement at file scope is an error naming the fix, instead of being parsed and silently dropped. A return type written without its colon (`func add_one(value: Int) Int`) says so, instead of "expected '{' but found 'Int'". A folder named `spite`, or `load("spite")`, is an error: that namespace belongs to `Spite.Class` and the rest. And the terminal-`if` lint of section 5 exists -- a function whose last statement is a narrowing `if` with no `else` is an error naming `assert` -- which caught three places in the compiler's own sources on its first run. |
| 2026-09-20 | (found by writing tests; proposed by Claude, unconfirmed) **Three gaps the test package turned up.** `copy()` existed on a class but not on a `List<T>` or `Dictionary<T>`, although section 10 says every non-scalar value has both `copy()` and `deep_copy()`; it does now, shallow, sharing the elements. A name that is reserved in the C Spite emits (`unsigned`, `static`, `int`, `stdout` and the rest) used to emit C that would not compile; it is now a naming error like snake_case and the abbreviation list, which is where the rule belongs. And `value == null` used to say "unsupported expression kind in this stage"; it now names the four ways to narrow, since `if value { } else { }` is already the one way and a second one is what section 5 removed. |
| 2026-09-20 | (proposed by Claude, unconfirmed; found by writing the programs an AI would write) **Syntax borrowed from other languages is a compile error naming the Spite form.** `&&`/`||` name `and`/`or`; `!` names `not`; `++`/`--` and `+=`/`-=`/`*=`/`/=`/`%=` name the assignment written out; `condition ? a : b` names an `if` with an `else`; `new Creature()` names the plain call; `this.name`/`self.name` name the bare attribute; `import`/`require` name `load`; `elif` names `else if`; a parameter with `= default` says every argument is written at the call site; and a bare `print(...)` that resolves to nothing names the `Console`. Each of these was previously a parse error pointing at a character, which tells a writer what the compiler could not read rather than what to write. |
| 2026-09-20 | (implements section 16 item 1 and the reopening half of D7; the `Spite` rule proposed by Claude, unconfirmed) **The standard library is discovered before the program, so a file of your own reopens it.** `library/` used to be walked last, which made the load order backwards: a library class would have reopened a user class rather than the other way round. A `spite/` folder in your own package now reopens `Spite.Class` and the rest -- `func full_name()` written there is answered by every class object in the program, which is what "a new hook name creates a new hook program-wide" means. A file under `Spite` that is **not** reopening an existing class is an error naming the fix, so the namespace stays the standard library's without the folder being forbidden outright; `load("spite")` stays an error on its own name, which is the one asymmetry. |
| 2026-09-20 | **D53** (decided by Mortaro): **text is joined by writing the value inside it, not with `+`.** `"hello {name}"` places `name` in the text at that point; `{ }` holds one value, which may be any expression (`"{person.name} is {person.age + 1}"`), and a brace meant literally is written `\{` the way a quote is written `\"`. Joining a *literal* to a value with `+` becomes an error naming the interpolation -- `"hello " + name` is wrong where `name + other_name` is right, because the first has a written-down piece of text with a hole in it and the second does not. One way to write the common thing, and the shape reads as the sentence it produces rather than as an expression that assembles one. |
| 2026-09-20 | **D54** (decided by Mortaro): **an `if` whose only statement is a bare `return` is a compile error naming `assert`.** `if not directory.exists() { return }` is an `if` written for the sake of asserting something, and section 5 already has the way to say it: `assert directory.exists()`, which returns quietly and leaves the rest of the function unindented. The error names the condition to assert, negating it when the guard was not already negative. It is the same rule as the terminal-`if` lint one step further: an `if` that only decides whether the function continues is not control flow, it is a precondition. The compiler's own sources had 32 of them. |
| 2026-09-20 | **D55** (decided by Mortaro): **a function body holds no empty lines.** A blank line inside a function is a compile error, because it is where a second function wants to be: the part below it is a step with a name, and the way to break a body into pieces is to name them and call them, not to space them apart. Blank lines stay legal between declarations, where they separate one thing from the next rather than one half of a thing from the other. The compiler's own sources had 164 of them, and the documentation's samples 25. It is the same push D27 makes with `assert`: decomposition is forced by the compiler rather than suggested by a style guide. |
| 2026-09-20 | (implements the first step of milestone 10b's design; started by Codex, finished by Claude) **`--final-classes` writes the program back out as Spite source**, one file per class under its namespace folders, after discovery has merged every reopening. `--final-classes` alone writes to `.spite-cache/final`; `--final-classes=<folder>` chooses where. What it writes is a program rather than a report: `check.sh` prints a corpus program that reopens a standard library class, runs what came out, and requires the same output. That is what makes the milestone's "visible, not compiler magic" checkable at all. Not yet shown: which root supplied each declaration (open question 10). |
| 2026-09-20 | (extends the previous row; proposed by Claude, unconfirmed) **`--final-classes` also writes the classes the compiler provides**, under `built_in/`, as `type` declarations -- a `type` is how Spite already names members without bodies, so the view needs no new notation. `Console`, `File`, `Directory`, `Process` and `Program` come out of the generator's own table, so nothing is written twice. **Still missing, and reported rather than faked:** `Int`, `String`, `List<T>` and `Dictionary<T>` have no class table in the compiler at all -- they are shapes the generator knows by name, which is exactly what D14's "Pure Spite" is for, so `Int` cannot be printed until it is a class; `Console.print`/`write`/`error` are variadic and cannot be written as a signature yet (D39's variadic generics); and a generic prints as its template rather than once per instantiation, because the instantiations only exist after generation while this runs after discovery. |
| 2026-09-20 | (extends the previous two rows; proposed by Claude, unconfirmed) **`--final-classes` writes what the program ends up with, not what was written down.** It runs the generator and reads the class table afterwards, so a class tree shaking removed is not written, a generic *template* is not written, and each of its *instantiations* is, in `instantiated/`, named for the values it was given (`WeaponIntTrue`). That is what the flag's name claims and what milestone 10b needs: the thing you read is the program's final shape, arrived at by the same pass that emits it, rather than the source re-printed. What a class made of is still shown as a `type` for a class the compiler provides; a class with a file is still printed from its own source, so nothing about a written class is paraphrased. |
| 2026-09-21 | (implements the D6 half of milestone 10b's second step; proposed by Claude, unconfirmed) **`is_singleton()` is an ordinary member of `Spite.Class`, written in Spite.** `library/spite/class.spite` declares `var singleton = false` and `func is_singleton(): Bool { return singleton }`; the compiler sets that one attribute when it writes a class object, and answers nothing at the call site. So `some_class.is_singleton()` is a real call to a real function you can read, `--final-classes` shows it, and reopening `Spite.Class` can see what it is overriding. **The compiler fills data, never behaviour** -- that is the shape the rest of D6 follows for `attributes`, `functions` and `instances`, each of which is still answered by name at the call site. |
| 2026-09-21 | **D56** (decided by Mortaro): **a shape names the types it requires, never the names they are given.** `render(Int): String`, not `render(scale: Int): String` -- "the name you give an argument does not matter to type". A class satisfies the shape whatever it calls its own parameters, and the parser was already throwing the name away, so the syntax now says what the language already meant. Writing a name is an error that shows the type to write in its place. **A shape also never carries a constructor:** an entry whose name is capitalised would mean requiring a class to be *constructible*, which is forcing a class rather than describing a shape, so `--final-classes` no longer writes one into the `type` views it prints. Still open: whether a required function should instead be written as an attribute holding a `Spite.Function` (open question 11). |
| 2026-09-21 | (found by writing an ordinary program; proposed by Claude, unconfirmed) **`join(separator)` works on every list whose elements become text**, not only `List<String>`: a list of numbers, of `Bool`s or of enum values joins with the same rule `+` and `"{value}"` already use. It used to emit a call to a function it never generated, so `counts.values().join(",")` produced C that would not compile -- the worst outcome a compiler can have, and one an ordinary program hits immediately. |
| 2026-09-21 | **D57** (decided by Mortaro): **class-level reflection is tree-shaken, not made lazy.** Asked whether `Spite.Class.functions` should be filled on demand, the answer is no: a program that never asks for it has none of it emitted, and a program that does asks at compile time and gets it whole. That is the compile-time guarantee the rest of the language already makes, and it is what lets the REPL have everything without deciding what to keep. So `functions` is an ordinary attribute of `Spite.Class`, declared in `library/spite/class.spite`, filled when the class object is written; the hidden C member it used to be is gone. `instances` already worked this way -- a class is only tracked when some part of the program asks for its instances -- and stays a live registry rather than a filled attribute, because which instances exist is not a compile-time fact. **One consequence to know:** a class object holding its functions makes a cycle, since a `Spite.Function` names a `Spite.Class` for what it returns, so the program's exit clears the lists before releasing the class objects -- the same "clear one side" section 10 prescribes for any cycle. |
| 2026-09-22 | (implements D41, milestone 10b; proposed by Claude, unconfirmed) **`Spite.Namespace` is written, and `Spite.Class.namespace` is now one of them.** `library/spite/namespace.spite` declares `.name`/`.full_name`/`.parent`/`.classes`/`.namespaces` with a two-argument constructor; the compiler fills the *data* when it writes a class object -- `->namespace` is the cached object for that class's dotted namespace, `null` for a global class (Mortaro's settled rejection of the root object, section 8) -- so two classes of one namespace share one object and `.parent` walks up to `null`. `Spite.Class`'s constructor drops its namespace parameter (`Class(name)` only). `.classes`/`.namespaces` answer from the merged class list, and both the fill and the tree are tree-shaken behind reads (`namespace_read_used`/`namespace_tree_used`), the same rule D42/D57 prescribe: a program that never reads a namespace emits no namespace objects at all. Printing a `Spite.Namespace` prints its `.full_name`, as D41 says. Every `.namespace` reader in `conformance/`, `tests/` and `docs/` migrated in the same change (`if containing { ... }` narrowing), and `conformance/stage6/namespace_objects` covers identity, the parent chain and both tree walks. |
| 2026-09-22 | (implements the members half of D11/D6, milestone 10b; proposed by Claude, unconfirmed) **`Spite.Class.attributes` is a declared `List<Spite.Attribute>` in `library/spite/class.spite`, filled when the class object is written**, next to `functions` (D57) and `is_singleton` (D8): the compiler assigns `spite_class_attributes_<Class>()`, which builds the list off a default-constructed instance rather than any constructed one, so **a class-level entry's `.value` is the field's declared default** (section 4). This answers the question D11 left open -- what the class-level mapping says for a field the instance has not set -- and it is the half needing Mortaro's confirmation; the symbol-keyed mapping `Weapon.attributes['damage']` (D10) stays milestone 14. Tree-shaken behind `class_level_attributes_used`, and the program's exit clears the lists the same way D57 clears `functions`, since a `Spite.Attribute` holds a `Spite.Class` for `.class` and the cycle is real. `conformance/stage6/class_attributes` walks both levels: `Sticker.attributes` beside `sticker.attributes`. |
| 2026-09-22 | **(decided by Mortaro): `class` is not a keyword -- it is an inherited attribute of any instance, pointing to that instance's class.** "class should not be a keyword instead its just a inherited attribute of any instance that points to the class of that instance." So a bare `class` inside any function of a class answers where `self.class` would: resolved after a local or an attribute of that name (a declared field wins -- that is how `Spite.Attribute.class` reads its own field) and before a class name of the same spelling, returning the class object with the same ownership `monster.class` returns it with, so `class.name` composes. The parser keeps its diagnostic -- `class ClassKeyword {` at file scope still fails at 1:1 with "there is no 'class' keyword" (`diagnostics/class_keyword`), because a file is a class named after the file -- and `class` was never in the reserved-name list. `tests/reflection_tests` covers it bare (`class.name`), through a local (`var mine = class`), and in an instance method (`creature.class_name()`).  **[implemented]** |
| 2026-09-22 | **(decided by Mortaro): a class name reads its class object member by member -- `Hello.name` is the class's name -- and the `Namespace?` narrowing error gets tests.** "i should also be able to directly say Hello.name to get class name"; "we should have tests for it." The first falls out of D6 (a class name is an ordinary `Spite.Class` value): the static-path substitution that made `Sticker.attributes` work now serves every member read and method receiver on a static path -- `Creature.name`, `Creature.namespace`, `Creature.attributes` answer the same class object as `creature.class.*`, and `Console.is_singleton()` calls -- while `Creature.instances` still means its live registry and `Creature.debug()` still reports "'Creature' is a class, not a value: write 'Creature()'", since there are no static functions. The second pins the existing nullability rule rather than changing it: `diagnostics/namespace_nullable` is the reported shape verbatim (assert a local copy, then re-read `Hello.namespace.name` -- narrowing narrows the variable you hold, so the fresh read must be narrowed in its own right), and `tests/reflection_tests` exercises all three narrowing forms (`if`, `assert`, `crash`) on `Spite.Class.namespace`.  **[implemented]** |
| 2026-09-23 | **(decided by Mortaro): D43's chain narrowing is the ordinary rule -- no special case for a class path, because every attribute Spite adds by default is a normal attribute and the normal null rules apply to it.** "the chain form for a class should not be a special case at all, namespace should be returning a Spite.Namespace which should follow same null coalecense rules, the beauty of spite metaprograming is that every attribute spite adds by default is just a normal attribute so any normal rule apply." Implemented as section 5 already specifies: `Scope` keeps a set of narrowed paths beside its name overrides, and a member read whose path is in the set answers with its plain type -- the same thing `lookup_override` already does for a bare name, consulted where a member generates instead of where an identifier resolves, so nothing downstream (member reads, method calls, stores, conditions) knows that paths exist. `if`, `assert`/`crash` and a switch subject each record the path in the block they narrow (then-scope, current scope, case-scope); while the condition itself generates its strict prefixes are visible in a throwaway scope, since the condition reads through them, which is what makes a deep chain provable in one statement; every link is tested before the next is read, so a null link fails the narrowing instead of dereferencing anything, and a condition that owns what it read releases it in the guard itself. A local copy stays its own path (`diagnostics/namespace_nullable`), the `else` of a narrowed `if` does not inherit the narrowing (`diagnostics/path_narrow_else`), `conformance/stage6/namespace_objects` walks a nullable prefix (`gadget.class.namespace.parent`) with `crash`, since a constructor bans `assert`, and `tests/reflection_tests` has the `if`/`assert`/`crash` forms, a deep `Spite.Attribute.class.namespace` chain, and a null path that fails the `if` without being dereferenced. **Open:** the manual walks up with `class.namespace.namespace` (section 5, section 8, D43, the 2026-09-20 row), but `Spite.Namespace` declares `.parent` and has no `.namespace` (section 8 itself: "`.parent` walks up it") -- either the member is meant to exist and the library is missing it, or every example means `.parent`; Mortaro to say which.  **[implemented]** |
| 2026-09-23 | **Review of the D43 implementation (Claude Opus 5.5, reviewing a commit written by another model).** Two defects, both fixed. First, the row above says every link is tested before the next is read; the guard actually generated the whole path with its prefixes narrowed and tested only the last link, so `if outer.inner.inner` with a null `outer.inner` dereferenced null. The guard now tests each nullable link in order, joined with `&&`, so a null link stops the test (`conformance/stage6/path_narrowing`). Second, nothing undid a path narrowing: `assert outer.inner` then `outer.inner = null` (or `outer = Box()`) then `outer.inner.label` compiled and crashed. **(proposed by Claude, unconfirmed):** an assignment undoes every narrowing at or beneath the path it writes -- except that a value which cannot be null keeps the written path itself narrowed -- and inside a `while`, undoing a narrowing made before the loop that the loop has read is an error naming the fix, since generation is one pass and the read earlier in the body was already emitted as proven (`diagnostics/path_narrow_assignment`, `diagnostics/path_narrow_loop`). **Still open, for Mortaro:** a function call can change an attribute behind a narrowing (`assert tracker.target` then `reset()` which sets it to null). This is not new -- a narrowed bare attribute name has the same gap -- and closing it means either re-testing after every call or treating narrowing of an attribute as valid only until the next call. Also found, older than D43: assigning `null` to a narrowed *name* (`assert maybe` then `maybe = null`) stores `Box_default()` instead of null, silently. |
| 2026-09-23 | **D58** (decided by Mortaro): **`join` is not special to `List<String>`.** It converts every item to text and joins the results; converting text to text is folded away as a no-op. So `join` is one function for every element type rather than a text-only case the other types borrow.  **[implemented]** |
| 2026-09-23 | **D59** (decided by Mortaro): **no function overloading; arguments cast to the parameter's type.** "I don't like how ambiguous it is." One name means one function; calling `takes_a_float(i)` with an `Int` casts `i` to `Float` by the ordinary casting rule (section 4), exactly as an assignment would. A class that wants to accept several kinds of value accepts a union or a `type`. How a class defines its own casts is open question 13. Already true when decided: an argument casts by the rule an assignment uses, and declaring a function name twice is an error.  **[implemented]** |
| 2026-09-23 | **D60** (decided by Mortaro): **a `switch` stays exhaustive, and `_:` covers every case not written.** As in Rust, a last `_:` case answers for the rest. And because a call on a union works when every member has the function (section 7), `_: shared.method()` calls the function all remaining cases share. **A `switch` that repeats a case body is a compile error:** cases with the same answer are written once, through `_:`. As implemented (Claude's reading, unconfirmed): `_:` expands to its body once per remaining member, each narrowed to that member, because "the same text" is only "the same thing" when each copy sees its own member -- which is also what lets `_: shared.method()` work when only the remaining members share it. Migrating the tree folded the repeated cases of the compiler's generated shape helpers, its printers and parser, and `examples/battle`.  **[implemented]** |
| 2026-09-23 | **D61** (decided by Mortaro): **every function Spite adds to a class appears in `--final-classes`**, the member templates included (`sum_price()` and the rest, D15): a function you can call is a function you can read. **Partly implemented:** every function Symbol codegen made for a class is printed as an ordinary function under the class's own source, with the symbol spelled out (`func describe_age(): String { return "age is {age}" }`), and check.sh runs the printed `conformance/stage6/symbol_codegen` to the same output. The `List<T>` member templates are not printed yet, because `List<T>` has no class of its own to print them into until milestone 15c writes it in Spite.  **[partial]** |
| 2026-09-23 | **D62** (decided by Mortaro): **no destructuring and no lambdas, anywhere.** "Writing it is fun, but it's not a human who will write code in this language, so it's a pointless readability sacrifice." A function value is a named, bound function (D17); a value is taken apart by reading its members by name. |
| 2026-09-23 | **D63** (decided by Mortaro): **a local that only copies a name or a path so it can be narrowed is a compile error.** `var watcher = tracker; assert watcher; watcher.note_a_drop()` is written `assert tracker; tracker.note_a_drop()` -- narrowing works on the name and on the path (D43), so the copy is a human habit with no reason in Spite. Its exact scope (proposed by Claude, unconfirmed): a `var` whose value is a bare name or member path of a `T?` type, which is then narrowed, is never assigned again, and whose source is not assigned later in the function either -- a snapshot taken before the source changes is not a copy for narrowing. Migrating the tree removed 110 such copies from the compiler and a dozen from the corpus, and turned up the check-proves-nothing error of section 5.  **[implemented]** |
| 2026-09-23 | **D64** (decided by Mortaro): **reading with `[]` answers `T?`** and has to be narrowed before use, so an out-of-range read is never a hidden runtime crash. From a test that read `attributes[0]` and `attributes[1]` after checking only the count: "we should either have an assert per case but also ideally extend the language to understand that if count > 2 it means 0 and 1 are safe" -- **a proven count proves the indices below it** (decided in intent; the exact rule is Claude's to propose). Most index reads live in the index loops of open question 15, which is why the two should land together. Writing through `[]` is unchanged. As implemented (rules proposed by Claude, unconfirmed; section 5): a proven count, a bound in a condition (`while index < names.count()` proves `names[index]` in the body) and an explicit `crash names[index]` narrow a read, and assigning the list or the index, or shrinking the list, undoes it. The bound rule alone proved all but about 45 of the compiler's 369 index reads, so open question 15 is no longer needed for D64 to be livable; the rest were parallel lists read by one counter, and got a `crash`. A `Bool?` in a condition became an error on the way, because `if flags[index]` would have silently changed from 'is true' to 'is there'. Two bootstrap steps: an intermediate compiler that tolerates `crash` on a plain value compiled the migrated sources into the new seed.  **[implemented]** |
| 2026-09-23 | **D65** (decided by Mortaro): **`full_name` is renamed, because it is ambiguous**: the name must say what it holds, "something like `name_with_namespaces`". Applies to `Spite.Namespace.full_name` and the `full_name()` a reopening adds in `conformance/stage6/reopen_library`. |
| 2026-09-23 | **D66** (decided by Mortaro): **the test package proves there are no memory leaks.** A test run that ends with allocations and frees unequal fails, as every corpus program already does. |
| 2026-09-23 | **D67** (decided by Mortaro): **the order of a file is enforced**: `singleton`, then `generic` lines (open question 12), then `enum`, then `type`, then variables, then the constructor, then functions. Out of order is a compile error (section 12: the compiler formats or errors). Implemented for what exists today, with `union` placed between `enum` and `type` (Claude, unconfirmed); nine files in the tree were reordered.  **[implemented]** |
| 2026-09-23 | **D68** (decided by Mortaro): **a class name is a `Symbol`, not a `String`.** Symbols are easier to tree-shake, and for the reader a `Symbol` answers every `String` method as if it were text, even though `symbol.class == Symbol`. Text can become a `Symbol` only when that symbol already exists in the program's table -- enough for metaprogramming, without Ruby's attack of minting symbols from input. Part of milestone 14. **Not implemented, because it conflicts with D10** (found by Claude, 2026-09-23): D10 says `Symbol` is compile-time only and that storing a bare `Symbol` in a field is an error, while a class object holding its name as a `Symbol` is exactly a stored, runtime `Symbol`. Claude's proposal (unconfirmed): D68 supersedes that clause of D10 -- a `Symbol` value at runtime is an entry of the program's symbol table, which the compiler writes the way it writes an enum (tree-shaken to the symbols the program uses), so storing one is fine and comparing two is comparing two numbers; a symbol *literal* still needs something to say what it may be (D10's other clause stands); and `Symbol("Creature")` answers `Symbol?` -- null unless that symbol is already in the table, which is D68's "no minting from input". Mortaro to confirm before it is built. |
| 2026-09-23 | **D69** (decided by Mortaro): **comparing a `T?` with `==` needs no narrowing: null is simply not equal**, so `crash Spite.Class.namespace == "Spite"` is the whole of a test that used to be five lines of copying, narrowing and comparing. "If AI can do this messy code, it will do this messy code" -- the language should accept only the short form (D63 rejects the copy). How a `Spite.Namespace` compares with text is an operator function in the library (operators are functions, section 5), not a special case; under D68 that compares its name. As implemented it compares `name_with_namespaces`, so a nested namespace is compared by its dotted path (Claude's reading, unconfirmed).  **[implemented]** |
| 2026-09-23 | **D70** (decided by Mortaro): **symbols are completely tree-shakable runtime values.** Answering the D68/D10 conflict: "symbols should be completely treeshakable, that way all internal metaprograming can use symbols which are strings guaranteed to make no new allocations, from a precompiled list, but they allow for tree shaking so unused symbols simply disappear, and if they are used that means we would need them anyway. symbols can also be optimized into TinyString most of the cases because they are normally small." So D10's "a `Symbol` is compile-time only; storing one is an error" is superseded: a `Symbol` value is an entry of the program's symbol table, written by the compiler with only the symbols the program uses, and never allocates. D68 (class names are symbols) is unblocked by it, and both are built the same day: `Symbol` is a type, a symbol literal where one is expected is an entry of a table the compiler writes only for the symbols a program uses (`static` text, so retaining or releasing one does nothing), a `Symbol` reads as `String` wherever text is expected, `Symbol(text)` searches that table, and `Spite.Class.name` is a `Symbol` (`tests/symbol_tests`, including one that proves storing and comparing symbols allocates nothing). The names of every reflection object followed the same day (Claude, on Mortaro's 'all internal metaprogramming can use symbols'): `Spite.Function.name`, `Spite.Argument.name`, `Spite.Attribute.name`, `Spite.Namespace.name` and `.name_with_namespaces` are symbols too, so `functions.map_name().contains('label')` takes a symbol literal. `symbol.class` is `Symbol` (`tests/symbol_tests`). Not built: TinyString. TinyString -- a small inline string instead of a pointer into the table -- is recorded as the intended representation for short symbols, an optimisation below the language. |
| 2026-09-23 | **D71** (decided by Mortaro): **a program is made of `.spite` files; a native library is reached through a Spite wrapper that names its real file.** Answering Claude's guessed platform extension: "all files should be .spite or error on compile. ... a consumer of a dll that was not made in spite would create a spite wrapper, and DynamicLibrary metaprogramming allows us to code in spite and get converted to the dll conventions" (the `mouse.spite` sample in section 17). So `DynamicLibrary` names the library file exactly as it is on disk -- no extension is added and no alias like `"c"` is resolved -- and a name without an extension is a compile error naming the fix; choosing a different file per platform is the wrapper's job, not the compiler's. |
| 2026-09-23 | **D72** (decided by Mortaro): **the REPL is written in pure Spite.** `runtime/spite_repl.h` is not wired in; it is deleted once the Spite REPL replaces it. |
| 2026-09-23 | **D73** (decided by Mortaro): **the hand-written C runtime goes as soon as possible.** D14's direction, now the priority: "we should get rid of the c runtime as soon as possible". Claude takes the 15a floor (section 15, "The floor, named") as the plan for it, minus the `"c"` alias D71 rules out: what stays C is what the compiler emits (object headers, the `Memory` and `DynamicLibrary` intrinsics, `main`), and everything else moves into `library/` as Spite. |
| 2026-09-23 | **D74** (decided by Mortaro): **`upper()` and `lower()` are abbreviations of what they do, so they are gone.** "`.upper()` is ugly, does not match our no abbreviation policy, we should have `.to_uppercase()` or `.uppercase()` depending of the convention or `upper_case`." Claude chose **`upper_case()` and `lower_case()`** by the convention the language already has for naming a case -- `pascal_case()` and `camel_case()` (section 17) -- so the family reads alike; calling `upper()` or `lower()` is an error naming the replacement (`diagnostics/upper_is_upper_case`). Every use in the compiler, the corpus and the documentation moved in the same change. |
| 2026-09-23 | (proposed by Claude, unconfirmed) **`library/string.spite` gives `String` its methods in Spite, and inside it a string's own members are called unqualified, like any class's.** `String` stays a value the compiler lays out (it has no attributes, and one is an error), so `trim()` is written `slice(start, end)` and reads its receiver through `length()` and `code_at(index)`; the functions are ordinary Spite functions, so their arguments are owned the usual way, and each keeps the name the C one had, which lets the C be deleted one function at a time. Moved so far: `is_empty`, `character_at`, `index_of`, `contains`, `starts_with`, `ends_with`, `replace` (as `split(from).join(to)`), `trim`, `upper_case`, `lower_case`, and parsing numbers -- `to_long` reads a sign and digits in Spite, and `to_double` is `strtod` through the platform wrapper. The conversion names (`to_int`, ...) are the language's own, so the abbreviation check leaves a `to_` function on `String` alone. Still C: `length`, `code_at`, `slice`, comparison, concatenation, and formatting numbers. The same file is where a program reopens `String` (D7). Compiling the compiler takes the same time as before. |
| 2026-09-23 | (proposed by Claude, unconfirmed) **Writing a whole number as text is Spite: `library/number_text.spite`, a singleton the compiler calls for every interpolated or printed integer.** A number has no way yet to name its own value from inside a method written for it (`String`'s methods reach theirs through `length()` and `code_at()`), so the digits cannot be a method on `Long` until Mortaro decides how a number's method refers to the number; a singleton the compiler calls needs no new language. `long_text` writes the digits backwards into a `Memory` buffer, so `-9223372036854775808` needs no special case. Floats still go through `snprintf` in C. |
| 2026-09-23 | **D75** (decided by Mortaro): **`value == Class` is a class test, and a switch that only answers it is an error.** "this should be an error from compiler that does not allow over engineering ... if some code can be expressed simpler with no readability cost, it should be forced to do so. we just need to avoid letting it do shitty one liners like python that noone can read back." The example was `is_true_literal`, a switch with `Syntax.Expressions.TrueLiteral: return true` and `_: return false`, which becomes `return expression == Syntax.Expressions.TrueLiteral`. Asked which switches the error covers, Mortaro chose the narrow reading: exactly one class case plus `_:`, both returning `Bool` literals. The `as_*` shape (`C: return value`, `_: return null`) stays legal. See [Unions](#unions--implemented). |
| 2026-09-23 | **D76** (decided by Mortaro): **`$name` is for generics only; a program's settings come from an `Environment` singleton the program reopens.** "using $variables for both command line environment setup and generics was a bad idea from my end, lets use it just for generics. instead lets have a singleton for dealing with Environment arguments so entrypoint is not forced to receive the args of the console." `var environment = Environment()` anywhere, and a program reopens `Environment` to declare which fields it has, so what it reads is deterministic and nothing it never uses is loaded. It replaces packages like Node's dotenv, but only for declared variables; it mirrors Nullstack's `context.environment`; the compiler uses it too; and it is how the game engine gets several environments. Supersedes D9's "undeclared means supplied by a compiler flag". **[planned]** |
| 2026-09-24 | (refines D75, decided by Mortaro) **The switch error covers a switch that is one early return, not only one returning `Bool`s.** "does not need to return a bool in the end, if the if is just 1 condition that could be an early return it should be a condition with early return, so if the entire switch could be expressed with 1 return it should be a return"; "if something needed an else it can stay a switch, it was only to rewrite if it needed a single if statement as a return." So one class case plus `_:`, both a single `return`, is an error: two `Bool` literals become `return value == Class`, anything else becomes `if value == Class { return ... }` then the rest's return. `if value == Class` narrows `value` inside its block so the early return can use it. The compiler's own `as_*` helpers are written that way now (`scripts/regenerate_type_shape.py` writes them). |
| 2026-09-24 | (proposed by Claude, unconfirmed) **Writing a `Float` or `Double` as text is Spite too: `double_text` and `float_text` in `library/number_text.spite`, and the C that called `snprintf("%g")` and `strtod` is deleted.** No foreign call: `snprintf` is variadic, and calling it through a plain function pointer is undefined for doubles on Windows x64. The value's bits come from `Memory`, its exact decimal expansion (up to 768 digits for the smallest subnormal) is built in base 10^9 limbs, and each precision from 1 to 17 (9 for `Float`) is rounded half to even and kept if it lies strictly between the value and its neighbours' midpoints (or on one, when the significand is even: what `strtod` does). The text is the same as before for every finite value -- checked against the C on 1.4 million values -- except that not a number prints `nan` everywhere, where the C printed the platform's spelling (`-nan(ind)` on Windows). Compiling the compiler takes the same time as before. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **The `--debug-memory` live table is Spite: `library/allocation_table.spite`; the allocator itself stays what the compiler emits.** `AllocationTable` holds the live allocations (an open-addressing hash set with Fibonacci hashing, since Spite has no bit operators), the class id of every object for the leak summary, and the counts, and prints `allocations: N frees: N` and the leak report in the same format as before. Under `--debug-memory` the compiler emits `spite_debug_realloc`, `spite_debug_free`, `spite_debug_register_object`, `spite_debug_set_class_names`, `spite_debug_report` and `spite_live_allocation_count` as one call each into the table, around a busy flag: while the table runs, an allocation goes straight to the C allocator, so the table's own memory is never tracked and tracking never recurses. **What stays C, and why:** (1) the allocator macros and those six entry points, because generated C and the prelude call them before any Spite object exists, and the first allocation of a program is what creates the table; (2) the counted allocator of an ordinary build (`realloc` and a counter behind `SPITE_MALLOC`), because it runs on every allocation of every program and is the floor itself -- a Spite function there would need a Spite object to exist before the first allocation, for no gain over one C increment. Found on the way: the table holds the `Memory`, `Console` and `ForeignText` singletons it uses (and `Console` holds `CRuntime` and its libraries), so they are made by the table on the first allocation and are no longer counted in `allocations: N frees: N` -- the totals are lower by those objects, and still balance. The cost of that: if one of those singletons ever keeps a growing `List` or `String`, the program resizing it would reach the table with a pointer it never saw and crash, so the table should keep depending only on stateless classes. A reallocation or free of an unknown pointer prints the same message and then halts with a `crash` report instead of `abort()`. |
| 2026-09-23 | (proposed by Claude, unconfirmed) **In a `--repl` build a `Spite.Attribute` stays linked to the live value it describes, so the REPL written in Spite (D72) walks, assigns and calls through reflection alone.** Four functions the compiler supplies, because only it knows the layout: `value_attributes()` and `value_functions()` on `Spite.Attribute` (a list's attributes are its elements, named `0`, `1`, ...), `assign(text): Bool` for a number, Bool, text or enum attribute -- through `set_<attribute>` when the class declares one, per section 14 -- and `call_with_text(arguments: List<String>): Spite.Attribute?` on `Spite.Function`. The emitted `main` hands the loop one `Spite.Attribute` named `program` for the entry instance instead of two lists. The loop parses the literals in Spite; the compiler only converts checked text into the attribute's type. The REPL's own output choices where section 14 is silent are listed there under the command language. Outside `--repl` the four functions answer an empty list, `false` and `null`, and ordinary reflection emits exactly what it did before. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **D76 implemented, with the settings read at run time.** `library/environment.spite` is the `Environment` singleton; a program reopens it in its own `environment.spite` with one `var` per setting and a literal default that is the setting's type (`false` Bool, `0` Int, `""` String; anything else is a compile error). Each field is read once, when the singleton is made, from the program's own `--name=value` (after `--` when the compiler runs it), else the upper-case environment variable (`SERVE`), else the default; text that is not of the type crashes. The compiler prepends `name = boolean_setting("name", name)` (or `integer_setting`/`text_setting`) to `Environment`'s constructor, and `Arguments()` answers the command line in any function. Values are runtime, so `if environment.serve` is no longer tree-shaken: the trade-off, and the compile-time home that `$target` and D13 still need, are in [Program settings](#program-settings-environment). An undeclared `$name` is an error pointing at `Environment`, and a `--name=value` before `--` that is not a compiler flag is an error showing where it belongs; `examples/arsenal`, `examples/dungeon`, the docs and the corpus moved over. |
| 2026-09-24 | **D77** (decided by Mortaro): **only a constructor call may be an argument, one level deep.** From `flush()` in the tokenizer: "having functions called inline during the same line that passes it as parameter is ugly." Asked which nesting is illegal: "only constructors can be used as arguments of a call, any other functions need to be done outside, even in constructor case a 1 depth limit is needed." So `append(Token(kind, text))` is legal, `append(Token(kind, source.slice(a, b)))` is not, and a method or function call as an argument is always an error. See [One call per line](#one-call-per-line-and-nothing-said-twice--planned). |
| 2026-09-24 | **D78** (decided by Mortaro): **an `if` and its `else` must not repeat the same logic**: "repeating the same logic twice on simple if and else" is hard to read and illegal. Mortaro's fix for `flush()` computes `var token_slice = source.slice(token_start, end_index)` once before the `if`. Which repetitions count (the same call in both branches, or any identical subexpression) is not settled yet. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **Joining, comparing and slicing text is Spite: `library/text_bytes.spite`, over three more `Memory` floor functions.** `Memory.address_of(text)` lends a string's bytes (the floor already listed it), `compare_bytes(first, second, bytes)` is `memcmp`, and `take_text(address, length)` makes a `String` that owns a buffer Spite filled, so joining copies once. `TextBytes` is a singleton the compiler calls through `SpiteString_concat`, `_equals`, `_less`, `_greater` and `_slice`, which it now emits as three-line wrappers, so nothing that calls them changed. Still C: `length` and `code_at` (a field read and a byte read the compiler could emit inline), making a `String` from bytes, retain and release (the object header is code generation), and the `true`/`false`/`""` constants. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **`runtime/` is deleted: the C the compiler still needs is written by the compiler, from `bootstrap/source/generation/prelude.spite`.** What was left of the hand-written runtime (the allocator switch, the object header, `String`'s layout and its making and freeing, `Arguments`, and the `Memory` and `DynamicLibrary` floor) is now text the compiler carries, so an installed compiler no longer reads `runtime/*.h` from the directory it runs in, and "what stays C is what the compiler emits" (section 15) holds literally. The prelude is still emitted whole; emitting only the parts a program uses is the next step. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **D78 implemented, narrowly.** The compiler reports a call with arguments (not a constructor) that prints identically in every branch of an `if`/`else` or of an `else if` chain ending in `else`, and only where computing it once before the `if` keeps the program's meaning: the call is among the first things each branch evaluates, mentions nothing the branch changed or the condition narrows, and is not a statement on its own. Which other repetitions count is still Mortaro's to settle. See [One call per line](#one-call-per-line-and-nothing-said-twice--planned). |
| 2026-09-24 | **D79** (decided by Mortaro): **`List`, `Dictionary`, `Int` and the rest are classes in `library/`**, "so people learning spite can see all the things that come with it out of the box." Every type a program can name is read from a Spite file in `library/`, the way `String`'s methods now are (`library/string.spite`), instead of being a shape the generator knows by name. How a number's method names the number itself is not decided yet. |
| 2026-09-24 | **D80** (decided by Mortaro): **no single C-runtime wrapper per operating system; each operating system reopens the classes it changes.** "we shouldnt have one big CRuntime per OS, instead each OS should reopen each class that they need to change so for example Linux would have its own definition of File ... if a thing needs a class for each environment do it." The compiler loads `library/`, then the folder of the target operating system chosen by `Environment().operational_system`, whose files add to or replace members of the same classes (section 11 reopening), and `--final-classes` shows the result. Supersedes the `CRuntime` wrappers of D71's implementation; D71's rule (a wrapper names its real library file) stands. |
| 2026-09-24 | **D81** (decided by Mortaro): **the compiler registers no classes by hand.** `register_system_class("DynamicLibrary")` with `one_parameter(...)` lists "is weird, it should just have those classes written in spite, everything should be transparent just like our metaprogramming." `DynamicLibrary` and `Memory` are declared in `library/` like everything else. How a Spite file declares a function whose body the compiler supplies (the floor that stays C) is not decided yet. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **The generated C is tree-shaken: a function nothing reaches is not written.** `bootstrap/source/generation/tree_shaker.spite` splits the emitted bodies into top-level pieces, keeps every piece that is not a function (tables, statics, preprocessor lines) and every function named from `main`, the declarations or a kept piece, and drops the prototypes of the rest. Names inside strings and comments do not count. It applies to everything the compiler writes -- the prelude, library classes, reflection helpers -- so a program carries only what it calls: the compiler's own C shrank by a tenth. `--development` (section 13: keep everything) should turn it off; that flag is not built yet. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **`List<T>` and `Dictionary<T>` are Spite: `library/list.spite` and `library/dictionary.spite`; per element type the compiler supplies only typed slot access.** A `List<$element_type>` keeps its elements in a `Memory` buffer (`items`, `item_count`, `capacity`), and growth, insertion and removal move bytes with `copy_bytes`; a `Dictionary<$value_type>` is two lists, keys and values, searched in order as before. The syntax stays the compiler's: `[]`, list literals and `List<T>()` are mapped onto those classes' functions, and a program can only call the documented ones. A generic class cannot read or write a `$element_type` at an address -- `Memory` reads bytes, `Int`, `Long` and `Double`, while an element may be a class, a `String`, a nullable number or a union -- so the compiler supplies four functions on each instantiated list, and on nothing else: `read_item(index)` (the element, retained), `write_item(index, value)`, `release_item(index)` and `item_bytes()`. **What stays C, and why:** (1) those four; (2) `deep_copy()`, because copying an element deeply is chosen by its type, as retain and release are; (3) the `<member>` templates (`filter_`, `sort_by_`, ...), because Symbol codegen binds to a class's own attributes and these read the element's; (4) the reflection and foreign-call code that reads a list's buffer directly. `contains` and `join` exist only for the element types that had them before: every function of a generic class is compiled for every type it is instantiated with, so the compiler drops those two from a list whose element cannot be compared or written as text. A class that declares its own `copy()` no longer also gets a generated one (before, that was a C error). Found on the way: `join` concatenated one piece at a time, copying the text so far at every step; writing each piece once into one buffer takes compiling the compiler from about 13 s to 0.7 s. A container's index and count are `Int` now, superseding the `int64_t` storage row of 2026-09-19. |
| 2026-09-24 | (implements D80; the choices below proposed by Claude, unconfirmed) **`CRuntime` is gone: each operating system's folder reopens the classes it changes.** `library/<system>/` holds `file`, `directory`, `process`, `program`, `console`, `string` and `environment`, each reopening the class of that name with only the members that differ, and each holding its own `DynamicLibrary(...)` attribute (`String` has no attributes, so its `to_double` makes the library in a local). What is the same everywhere stays in `library/<class>.spite` and calls the platform functions (`open_file`, `entries`, `exit_process`, `read_line_into`, ...) without declaring them, since exactly one folder is loaded; a platform file defines public members directly where the whole member differs (`Directory.exists`, `create`; `Process.run_attached`; `Program.sleep`; `File.remove`). `Program.platform()` is removed. The folder is chosen by `Environment().operational_system`, a setting the platform folder declares (`var operational_system = "windows"`), and the compiler reads its own: `--operational_system=linux` (a compiler flag now) writes C for another system, and `check.sh` does so for all three to hold the folders it cannot run to compiling. Because settings are read at run time (D76), a built program's `Environment().operational_system` can be overridden too; a compile-time home for build facts is still the open part of section 9. `--final-classes` prints the merged classes but still not which folder supplied each member (open question 10). |
| 2026-09-24 | (proposed by Claude, unconfirmed) **`--development` turns the tree shaker off**, as section 13 already promised ("keep everything (no tree shaking), for live reload"). A small program's C goes from 5,833 lines kept to 2,064 shaken (`conformance/stage1/arithmetic`). |
| 2026-09-24 | (proposed by Claude, unconfirmed) **`Dictionary<T>` is a hash table, and nothing a program sees changes.** `library/dictionary.spite` keeps the two ordered lists (keys and values), so `keys()`, `values()`, iteration order, `copy()` and the generated `deep_copy()` are exactly as before, and adds an index: a `Memory` buffer of `Int` slots, a power-of-two count of them, each holding 0 (empty), a list position plus one, or -1 (a removed key). A key's home slot is a polynomial hash of its codes (`* 31`, `% 1000000007` on a `Long`; Spite has no bit operators) modulo the slot count, and a collision takes the next slot. Lookup, `has`, `set` of a new or existing key and `[]` are constant time; the table is rebuilt when an insertion would fill more than half its slots (removed ones count), to the smallest power of two, at least 8, that holds four times the key count, so a table full of removals shrinks back. `remove` stays linear, as it was: removing from the ordered lists shifts every later position, so it marks the slot removed and lowers every stored position past it in one pass over the slots, rather than rehashing. An empty dictionary allocates no table; `drop()` frees it. Measured: 20 000 inserts and 20 000 lookups went from 3.2 s to 0.28 s; the compiler's own compile is unchanged at about 0.40 s, because the compiler now keeps one dictionary (`symbol_positions`) and it holds two keys -- the tree shaker, whose `Dictionary` scan cost seconds, already moved to its own hash table. Supersedes the linear scan of the 2026-09-19 String row and of the containers row above. |
| 2026-09-24 | (implements D77; the readings below proposed by Claude, unconfirmed) **A call passed as an argument is a compile error, except a constructor, one level deep.** The generator checks every statement and attribute value: inside an argument, a call whose callee's last name is not upper-case is an error, and so is any call inside the arguments of a constructor that is itself an argument, the same for every call, a constructor included (`var token = Token(kind, Text(x))` is legal, `Token(kind, source.slice(a, b))` is not). Anything inside an argument counts (`append(count() + 1)` is an error), a text with holes is not an argument and each hole is read like a line of its own, and a method called on a call's result is not an argument. The whole repository was rewritten to it -- the compiler, `library/`, the corpus, the examples, `docs/` and the diagnostics programs, about a thousand calls, each computed first into a named `var` -- keeping every output and the memory balance; where the call sat on the right of `and`/`or` or in a `while` condition it was computed before the condition only where that is harmless (a getter, `length()`, `code_at`, which answers 0 past the end). See [One call per line](#one-call-per-line-and-nothing-said-twice--implemented). |
| 2026-09-24 | **D82** (decided by Mortaro, answering D81): **a function whose body the compiler supplies is written by the compiler as a reopening of its class**, so `--final-classes` shows the class's actual content. "spite writes that function as if it was reopening that class, so in --final-classes it should show the actual content of that class." `Memory`, `DynamicLibrary` and the rest are declared in `library/` like any class; the floor the compiler writes is added to them the way any reopening adds members, and nothing is registered by hand. |
| 2026-09-24 | **D83** (decided by Mortaro, answering D79): **a number is a class whose methods call its internal members, with a `this` keyword if needed.** "if needed add a this keyword but it should be able to just call methods from its internal class, a number in the end is just a nice wrapper around a memory in stack or heap so it has some internal representation." `Int`, `Long`, `Float` and the rest are library classes like `String`: their methods call the value's own members unqualified, and `this` names the value itself where a method needs to pass or return it. |
| 2026-09-24 | **D84** (decided by Mortaro, settling D76's open question): **an `Environment` field supplied as a flag when compiling is hardcoded into the build; every other field is read at run time.** "Variables supplied as flags during compilation stay hardcoded the others are runtime." So `spite program.spite --serve=true` folds `environment.serve` to `true` (and its branches are tree-shaken), while a field not given to the compiler is read from the program's own arguments or environment when it runs. Mortaro asked to be reminded later that this may split into two kinds of environment, one for run time and one for compile time. |
| 2026-09-24 | (decided by Mortaro) **Markup is out of scope for now**: "we dont need to worry about any markup code now we dont have usage for it yet." D18's `html.div(...)` example conflicting with D77 is left as it is until there is a use for it. |
| 2026-09-24 | **D85** (decided by Mortaro, superseding D84's single `Environment`): **two environments, one for the build and one for the run, and compiler options are fields of the build one.** "lets already implement Environment into two things, one for RuntimeEnvironment and one for CompileEnvironment but make better names for it, we can just change later, so compiler flags are explicitely its own thing. make all our compiler options pass as a normal program from CompileEnvironment so people can reopen to force them with defaults." Every compiler option (`mode`, `output`, `optimized`, `development`, `repl`, `repl_port`, `format`, `debug_memory`, ...) is a field of the build environment with a literal default; a program reopens it to force its own defaults; a flag given to the compiler sets a field and is folded as a constant. The run environment keeps D76's run-time reading. Names (proposed by Claude, unconfirmed, "we can just change later"): **`Build`** for compile time (`var build = Build()`) and **`Environment`** for run time. |
| 2026-09-24 | **D86** (decided by Mortaro, part of D85): **the operating system the compiler runs on and the one the program targets are two fields**: "we need to differentiate the current operating_system from the target_operating_system, because we can compile to a target that is not our current one and we should be able to tree shake on that const." `Build().operating_system` is the machine compiling; `Build().target_operating_system` defaults to it, picks the OS library folder, and folds, so a branch on it is tree-shaken. |
| 2026-09-24 | **D87** (decided by Mortaro, answering open question 12 and superseding D9's constructor declaration): **a class declares its generics with header lines**: `generic $varname` then `generic $other_varname`, at the top of the file, "instead of constructor." The constructor no longer lists codegen values; call sites stay positional in declaration order (`Weapon<Magic, true>(10)`). |
| 2026-09-24 | **D88** (decided by Mortaro, answering open questions 17 and 19): **metaprogramming a developer cannot change is a getter-only function.** "metaprogramming things we cannot change as developers, should be a getter only function." `Spite.Class.name`, `.attributes`, `.functions`, `.namespace` and the rest are `get_<name>()` with no setter, so reading `.name` works (attribute interception) and writing it is the read-only error. `this` is added where a class needs to name itself (D83). |
| 2026-09-24 | **D89** (decided by Mortaro, answering open question 18): **the entry file is the file named after its folder, and its constructor takes no arguments**, "because Environments are singletons": a program reads its command line and settings through `Environment()`/`Build()`, so `func Hello(arguments: Arguments)` is gone, and a file argument to `spite` is no longer the way to name a program. |
| 2026-09-24 | **D90** (decided by Mortaro, answering open question 14): **variadic arguments are a generic list**: `...args: List<Type or Class>`. "this means a generic can be done over both a type or a class, but in the end monomorphised into each class." The caller writes the arguments one by one; the function receives a `List`; a generic parameter may be bound to a `type` (a shape) or a class, and is monomorphised per class. |
| 2026-09-24 | **D91** (decided by Mortaro): **the `List` member templates are an implementation change, not a language change**: "instead of a magic runtime thing its just using the proper underlying Memory object and our metaprograming syntax for iterating things that can be tree shaken outside repl." `filter_`, `sort_by_` and the rest are written in `library/list.spite` over `Memory` with the existing metaprogramming, and tree-shaken unless the REPL needs them. |
| 2026-09-24 | **D92** (decided by Mortaro): **the whole reflection system is ordinary code in the classes, usable from the REPL**: "our entire reflection system should be exposable as just code in the class, i need to be able to use it on repl its the language main feature." Everything `Spite.Class`, `Spite.Attribute`, `Spite.Function` and `Spite.Namespace` offer is declared in `library/spite/`, reachable by name at run time, and callable from `--repl`/`--repl-port`. Documentation must be extensive and current across `docs/`. Open decisions go to `mortaros_missing_decisions.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **`--repl-port` and `spite connect` are Spite, over a new `Socket` library class.** `library/socket.spite` (`listen_locally`, `accept_client`, `connect_locally`, `read_line`, `write_line`, `close`) holds what every system shares, including the `127.0.0.1` address, so nothing can bind another interface; `library/windows/socket.spite` (`ws2_32.dll`), `library/linux/socket.spite` (`libc.so.6`) and `library/mac/socket.spite` (`libSystem.dylib`) reopen it per D80. The server thread is started from Spite (`CreateThread`/`pthread_create` in each system's `read_evaluate_print_loop.spite`); its C entry point is two lines the compiler writes only in a `--repl-port` build, since a thread needs a C function address and Spite has none. No `Thread` class: D35 is how a program is concurrent, and this thread belongs to the REPL. The port is folded into the build (as D84 folds compile flags); `main` listens before the constructor and stops with exit code 1 when the port is taken; after the constructor it waits for the server thread, and `exit` from a client answers, then `Program().exit(0)`. Details in section 14. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **The console and socket loops share one `answer(command, program): ReadEvaluatePrintLoop.Answer`** (`succeeded`, `text`, `class_name`, a `type` declared in the loop's file so it takes no global name). A complaint the console loop prints is `"ok":false` on the wire; `"type"` is the value's class, empty for `help`, `attributes` and `functions`, `Nothing` for a call that returns nothing. **D37's drain points are not built:** the remote loop evaluates each command on its own thread the moment it arrives, as section 14's "data races are accepted" allows; a queue drained where the program waits is still D37's design and would replace this without changing the wire. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **A ` ```wire <folder> ` block in `docs/` is a remote REPL session `check.sh` replays** against that folder's documented program built with `--repl-port` (the port from the run's process id): every `$ spite connect <port> --command="..."` line is sent by the compiler's own client and must be answered with the line under it; the program must then exit 0 and have printed its ` ```output `. `docs/repl.md`'s worked session is the first. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **A `type` declared again in a reopening of its class replaces the earlier one**, as a function does, instead of being emitted twice. Found because `--final-classes` prints `ReadEvaluatePrintLoop` (with its `Answer`) beside the library's own, and compiling the printed program reopened it. Enums declared again still register twice; nothing reopens one yet. |
| 2026-09-24 | **D93** (decided by Mortaro): **a `Directory` has a path like a `File`, and its entries are a list of a union of `Directory` and `File`**, "so we can easily navigate it." |
| 2026-09-24 | **D94** (decided by Mortaro, on open question 15): **every `while` that one of the iterators (member templates) can replace is replaced, and where the replacement is obvious it becomes a compiler rule.** "investigate every while in the codebase check which ones can be replaced with our iterators, if its obvious make it a compiler rule." Mortaro also asked for a review file listing the `while` and `else if` cases that cannot be simplified, with examples, plus examples of simplified ones "to make sure it didnt become esoteric": `mortaros_review_while_and_else_if.md`. |
| 2026-09-24 | **D95** (decided by Mortaro): **JSON with a generic is part of the standard library, written with metaprogramming**, converting any class to and from JSON through reflection (see [JSON is reflection, not a library](#json-is-reflection-not-a-library-planned)). |
| 2026-09-24 | **D96** (decided by Mortaro): **the remote REPL's wire format need not be JSON**: "if AI is going to be communicating with it it can be any more efficient format for ai even a binary." The format is free to change to whatever an AI client reads best. |
| 2026-09-24 | **D97** (decided by Mortaro): **running a program is Spite code that loads it, with nothing hidden.** "running a spite program should have a entrypoint INSIDE our language code, that entrypoint basically just load() the folder the user provides. nothing of the process should be hidden, spite is just itself a spite application that loads the user application, no magic runtime code injected its a must! i should be able to see the project spite is running and completely understand how things are loaded, this project itself is loading the standard library and the current os files." The standard library, the operating system's folder and the user's folder are all loaded by visible `load()` calls in a Spite entry. |
| 2026-09-24 | **D98** (decided by Mortaro): **`List` and `Dictionary` are only abstractions over `Memory`, and `Memory` handles both heap and stack**, "so anyone can create efficient data structures, we will need efficient data structures for our ECS framework later on." Nothing about the containers is special to the compiler. |
| 2026-09-24 | (Mortaro, 2026-09-24, on D35/D37) **Hidden async/await must be built and documented**: "Make sure you made good on your promise of hidden async await with injected code into proper pause modes, its a important feature but ive seen not mention of it in documentation." D35's `Task` (no function colouring, join on drop) and D37's injected drain points are decided but not built, and `docs/` does not describe them; the remote REPL answers at once instead of at drain points. |
| 2026-09-24 | **D99** (decided by Mortaro, sharpening D35): **IO never blocks the program by the program's own hand: waiting on IO is hidden async/await.** "io async await is EXTREMELY IMPORTANT, user should not be able to block himself, unless compiler judges for some scripts its an optimization to block instead of await, but user does not know compiler does the fast thing, same for http and so on." A file read, a socket, an HTTP call is written as an ordinary call and the compiler turns the wait into a suspension; blocking is only what the compiler picks when it is faster, never something the user chooses or sees. |
| 2026-09-24 | **D100** (decided by Mortaro, answering open question 13): **a class defines its casts with `func from_type(type: Symbol, value: type.class)`**, "lets go with this approach for now and implement it in the standard library so we convert the internal memory of a int to a float and so on." The numeric conversions are written this way in the number classes, over their internal memory. |
| 2026-09-24 | **D101** (decided by Mortaro, extending D98): **`Memory` is the one real basic type, with visible addresses and sections.** "we need some kind of abstraction on Memory so memory can have an address people can see, and each variable can see its own memory address by meta relating to memory, and memory can have multiple memory sections, of course all of this can be optimized away and tree shaken but user does not need to think about this, they just create their efficient wrappers just like our standard library does for all basic types, the only real basic type is Memory everything else composes on it." Every value can reach its memory through reflection, `Memory` has an address and sections, and every other type -- numbers, `String`, `List`, `Dictionary` -- is a wrapper over it; the compiler optimises the abstraction away. |
| 2026-09-24 | **D102** (decided by Mortaro): **every change to the code updates the documentation in the same change.** "make sure agents always update documentation when updating the code, eventually plan and my notes goes away and docs is all that we have left the plan becomes git history when all is done." `docs/` is the lasting record; `PLAN.md` and the notes files are temporary. |
| 2026-09-24 | **D103** (decided by Mortaro, on D35): **`Task` is too generic a name, and async waiting and threads are two things.** "we need to make a separation between async/await tasks and plain threaded tasks, unless they all are threads but in that case we need to make sure its efficient. we will later need for the nullstack clone efficient hidden async/await on db calls and so on, but super efficient threaded performance on our game engine." Hidden async/await (D99) for IO and database calls, and a separate, fast threaded form for parallel work. |
| 2026-09-24 | **D104** (Mortaro, correcting an omission): **`singleton` is a header keyword at the top of the file**, first in D67's enforced order (`singleton`, then `generic` lines, then `enum`, ...), and `func is_singleton(): Bool { return true }` is no longer how a class says it. "you ignored my decision to use the singleton keyword on top of file with the enforced order of declarations." `is_singleton()` stays readable on `Spite.Class` as a getter (D88). |
| 2026-09-24 | **D105** (decided by Mortaro, on D94's review): **the review's findings are confirmed and get built; the remaining proposals are decided later.** "your findings on while and if already confirmed can be worked on, and we just decide tomorrow the other ones." That covers: rewriting the 31 loops that an existing iterator replaces, flattening the 7 nested `if`/`else` that flatten, and the chains that become a `switch` today. The two proposed rules (`while` over a list, nested `if`/`else`) and `switch` over enums wait (`mortaros_review_while_and_else_if.md`). **Iterators are cumulative**: "keep in mind iterators should be cumulative (and we can optimize them into single loops on compiler time) like `map_repositories().filter_active().sum_stars()`." A chain of member templates reads as separate steps, and the compiler fuses it into one loop with no intermediate lists. |
| 2026-09-24 | (implements D87; details proposed by Claude, unconfirmed) **`generic $name` lines are built.** `generic` is a reserved keyword; each line declares exactly one value (a comma is an error naming the next line to write); the lines come after `singleton` and before enums in D67's order; the constructor's `<...>` list is a parse error listing the `generic` lines to write; a `generic` line inside a function is an error; a generic class needs no constructor, so `library/list.spite` and `library/dictionary.spite` lost their empty ones. The seed learned the lines before `library/` used them, and the constructor form was removed a generation later. |
| 2026-09-24 | (implements D90; details proposed by Claude, unconfirmed) **`...name: List<Type>` is built.** `...` is its own token; only the last parameter may take it, and its type must be written `List<...>`; a call gathers every argument from that position on into a new list, zero included; passing a whole list to it is an error unless the element type is itself a list; a function value keeps the spread; D77 applies to each value. `Console.print`/`write`/`error` stay special-cased, because `...values: List<Printable>` needs a decision on what `Printable` requires (`mortaros_missing_decisions.md`). |
| 2026-09-24 | (implements D104; details proposed by Claude, unconfirmed) **the `singleton` line is built.** It is a keyword only as a file-level line holding nothing else, and stays usable as a name, since `Spite.Class` keeps an attribute called `singleton` behind `is_singleton()`; declaring `is_singleton()` in any class but `Spite.Class` is an error naming the line; a reopening may add the line. Every standard library singleton (`Console`, `Environment`, `ForeignText`, `NumberText`, `TextBytes`) moved to it. |
| 2026-09-24 | (proposed by Claude, unconfirmed; answers D99 and D103) **Hidden async/await is stackful fibers on the program's own thread, and a blocking call runs on a helper thread while its fiber is parked.** Chosen over a state-machine transform (the colour survives inside the compiler: every function that can reach a wait, and every caller up to the fiber's root, is rewritten, and reference-counted locals move into the state object) and over threads for everything (every program multithreaded, the hidden cost D36 forbids). Spite code only ever runs on one thread at a time, so fibers need no locks; files cannot be waited on without blocking anywhere, so the one blocking call goes to a helper thread, as libuv does. The waits the compiler turns into suspensions are named by class and function (`Program.sleep`, and the system call under `Console.read_line()`, `File.read()`/`write()`/`append()`, `Socket.accept_client()` and `Socket.read_line()`), and are wrapped only in a program that uses the scheduler (it makes a `Concurrent` or is built with `--repl-port`); every other program's C is unchanged. **Blocking is what the compiler picks when it is faster (D99):** with no `Concurrent` alive and no REPL listening, or off the scheduler's thread, the wrapper makes the plain call. Built on Windows (`CreateFiber`, `SwitchToFiber`, an event); the Linux and macOS folders use `makecontext`/`swapcontext`, a pipe and `poll`, held to compiling. Section 15, "Concurrency". |
| 2026-09-24 | (proposed by Claude, unconfirmed; D103 asked for the names) **`Task` is split into `Concurrent` and `Parallel`.** `Concurrent(function)` runs on a fiber of the program's thread and is for work that waits; `Parallel(function)` runs on a thread of its own and is for work that computes. Both answer `.wait()` and join when dropped (D35). The pair is the usual distinction -- concurrency interleaves, parallelism runs at once -- so each name says what the program gets. `Parallel` starts one thread per call today; a pool is the engine's next step. Waiting on Mortaro in `mortaros_missing_decisions.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; builds D37) **The REPL's commands are answered at the scheduler's waits, on the program's thread.** The socket thread only reads a command, hands it over and waits for the answer; the scheduler answers it the next time the program waits (a sleep, a blocking call, a `Concurrent` wait, a `Parallel` join) and, once the entry constructor returns, waits for nothing else until `exit`. So a command sees the program between two steps. `exit` is handed over the same way: the program's thread answers it and stops, and the socket thread writes the answer and calls `Program().exit(0)`. **The compile error D37 asks for, as built:** a `--repl-port` build is an error when none of the program's own (non-library) code calls a wait (`Program().sleep`, `Console.read_line`, `File.read`/`write`/`append`, `Socket.accept_client`/`read_line`, anything on a `Concurrent` or `Parallel`) and the program has a `while` loop, which is named in the error; a program without a loop always reaches the end of its constructor, where it is served. This can reject a loop that does end (`diagnostics/remote_loop_never_waits`). Supersedes the "D37's drain points are not built" part of the REPL answer row above. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **Reference counts are atomic only in a program that starts a thread.** A program that makes a `Concurrent` (its helper threads), a `Parallel`, or is built with `--repl-port` is compiled with `SPITE_THREADS`: retain and release are atomic (`__atomic_add_fetch`/`__atomic_sub_fetch`), the live-allocation counter too, and the `--debug-memory` table takes a spin lock with a per-thread re-entry flag. Every other program keeps plain arithmetic, so the cost exists only where threads do. `Memory` gains `exchange_long`, `read_long_atomically` and `write_long_atomically`, which the scheduler and the REPL's hand-over use. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **A generic class's codegen values are inferred from its constructor's arguments when they are left out.** `Concurrent(file.read)` is `Concurrent<String?>`: each constructor parameter whose declared type mentions a `$name` -- directly, or as an argument or the return of a `Spite.Function<...>` -- is matched against the argument's type. Only when every `$name` is found this way; otherwise the D9 error asks for them between `<` and `>`, as before. A function returning nothing fills a `$name` with `Nothing`, and where a value of type `Nothing` is needed, such a call gives a fresh `Nothing` (so `Concurrent(ring)` works). Mortaro's D35 example writes `Task(file.read)`, which only works this way; D9's "call sites are positional, always" still holds for every value written. |
| 2026-09-24 | (implements D91; the readings below proposed by Claude, unconfirmed) **The `<member>` templates are Spite in `library/list.spite`, and the generator writes none of their C.** `filter_`, `count_`, `any_`, `all_`, `sum_`, `each_`, `map_`, `find_by_` and `sort_by_` are Symbol codegen templates (`func sum_member(member: Symbol): member.class`) looping over the list's `Memory` buffer with `read_item`. The symbol in a `List` template names a member of the element, read as `item.attributes[member]` (a field, or a call to a zero-argument function); a list's own fields are its implementation, so a template of a container never binds to them. The D15 requirements are checked before the body is compiled, with the same messages, and now also cover `sort_by_` (a number or a `String`), `find_by_` (a number, `Bool`, `String`, `Symbol` or enum) and `map_` (a member that returns something). A `Dictionary` answers the names through `values()`, as before. Tree shaking is the templates' own: only called names are instantiated. A `--repl`/`--repl-port` build instantiates every fitting template for every class element of a list the loop reaches and exposes them as that list's `functions`, so the prompt can call `monsters.sum_health()` (`conformance/stage6/interactive_templates`). A program may add its own by reopening `List` in a `list.spite`. Output and memory are unchanged across the corpus; compiling the compiler is unchanged (it calls no templates). Supersedes item (3) of the containers row. Not done: D61's printing of a list's instantiated templates in `--final-classes` (the templates themselves are now readable Spite in `library/list.spite`). |
| 2026-09-24 | (implements D105's fusion; proposed by Claude, unconfirmed) **A chain of member templates compiles to one loop.** A template called directly on a `map_`/`filter_` call chain of a `List` or `Dictionary` is compiled as one generated Spite function on the first list's class (`filter_active_then_map_lead_then_sum_age`): one `while` over the buffer, an `if` per `filter_`, a `var` per `map_`, then the last template's step, and no intermediate list. The chain's meaning stays the step-by-step one; a chain the rule cannot write (a `map_` to a member that is not a plain class, a type it cannot spell) is compiled step by step. Only the order in which member functions run differs, element by element. `conformance/stage6/fused_chains` compares chains of two to four steps with the same steps written out, and `conformance/stage6/fused_chain_allocations`, whose `allocations.txt` check.sh now enforces, runs four chains a thousand times in 10 allocations (16 010 step by step). |
| 2026-09-24 | **D106** (decided by Mortaro, overriding D27's limit where they meet): **an `if` whose whole body returns the default value is an `assert`, and the compiler forces it.** "how comes we keep getting away with `if handle == -1 { return false }` -- compiler should be as pissed at you as i am right now it should force `assert handle != -1`." An `if` with no `else` whose body is only `return` with the return type's default (`false`, `0`, `""`, `null`, or nothing) is an error naming the `assert` of the opposite condition, wherever it stands in the function, not only as its last statement. `assert` therefore works in a function returning any type and returns that type's default, which relaxes D27 ("assert only in functions returning nothing or `T?`"). |
| 2026-09-24 | (Mortaro) **A comparison of Go's standard library with Spite's**, "so i decide what we offer": `mortaros_go_standard_library_comparison.md`. |
| 2026-09-24 | (Mortaro, correcting his own spelling) **`operating_system`, not `operational_system`**: "i think the correct term is operating_system and i misspelled this, correct me when i misspell." Every `operational_system` (the `Environment` field of D80, the compiler flag) becomes `operating_system`, which D86 already uses. |
| 2026-09-24 | (implements D93; proposed by Claude, unconfirmed) **`Directory.entries()` answers a `List<Directory.Entry>`**, the union of `Directory` and `File` declared in `library/directory.spite`, each with its `path` joined to its parent's; folders first, then files, each sorted by name. `files()` and `folders()` stay, answering names, and are proposed for removal once nothing reads names alone; each operating system's listing is renamed `entry_names(want_folders)`. Section 15, "System classes". |
| 2026-09-24 | (for D95; proposed by Claude, unconfirmed) **The metaprogramming `Json` needs, built as general features.** (1) `attribute: Symbol<Label>` makes a Symbol codegen template range over `Label`'s attributes, with `label.attributes[attribute]` reading and writing that attribute; (2) calling a template by the plural of its symbol (`show_attributes`) calls it once for every attribute in declaration order, as a generated function of those calls, and requires the template to return nothing; (3) `$value_type == X` on a type-bound codegen value folds at compile time, with `List`, `Dictionary`, `Null` (any `T?`) and `Symbol` (any enum) naming kinds and a union naming its members; (4) `$value_type.element_type` / `.value_type` read the codegen values a type was built from; (5) text casts to an enum by name, the first value when none matches. Fixed on the way: a generic instance's name now tells `Int?` from `Sailor?` (both were `Value`, so `Json<Int?>` reused `Json<Sailor?>`), a `T?` of an enum has a C type, and `--final-classes` no longer prints a generic instance's generated functions into the generic's file. Sections 4, 8 and 9. |
| 2026-09-24 | (implements D95; proposed by Claude, unconfirmed) **`Json<T>` is `library/json.spite`**, with `write(value): String`, `read(text): T?` and `read_or_crash(text): T`, written with the metaprogramming of the previous row and reading through `JsonReader`. Reading skips keys the class does not have, keeps the default of an attribute the text omits, and answers `null` (or crashes, naming the position and what was expected) on a value of the wrong kind or text that is not JSON. Numbers read through the ordinary text cast; `inf`/`nan` and `Symbol` attributes are not handled yet. Section 15, "JSON is reflection, not a library"; `docs/json.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; unifies the D91 and D95 rows above) **One mechanism for a template over another class's members: `Symbol<Class>`.** The `List` templates are now written `member: Symbol<$element_type>`, the same form `Json` uses (`attribute: Symbol<$value_type>`), so which class a symbol ranges over is always written where the symbol is declared and never implied by the class it sits in; a container's template without the range answers nothing. The member may be an attribute or a function that takes no arguments, for any `Symbol<Class>` template, and `x.attributes[member]` reads the field or calls the function (and writes a field). What `List` adds on top is its own: the D15 requirement checks, the `--repl` instantiation of every fitting template, and chain fusion. The generator keeps one representation (the instance records its member's type and owning class) and one reader of `x.attributes[member]`. The plural call (`show_attributes`) still walks attributes only. Sections 8 and 15. |
| 2026-09-24 | (proposed by Claude, unconfirmed; applies D36) **Appending to a text the variable alone holds grows it in place.** `text = text + piece` copied the whole text on every append, a hidden O(n²) in the most common loop there is. The generator now recognises an assignment whose value is a join starting with the variable itself (`text + a + b`, or `"{text}..."`, which parses as `"" + text + ...`) on a local variable or parameter of type `String`, and emits one `SpiteString_append(text, piece)` per piece instead: the prelude function grows the buffer in place (doubling, a new `capacity` field on `SpiteString`) when the reference count is 1, and otherwise copies once with room to grow and releases the variable's old reference. Immutability as observed is kept by the count: a text also held by another name, a list or a literal (static, never counted) is never written to. Not applied to attributes (a call among the pieces could read or replace the attribute mid-append) or when a piece mentions the variable (`"{text}{text}"`), which compile as before. Chosen over giving every concatenation spare capacity, which cannot tell `other = text + piece` (must not touch `text`) from `text = text + piece`. The append is C in the prelude beside retain and release, since it reads the object header (the floor's item 3). 100 000 appends: 6.9 s and 1 100 000 allocations before, 0.23 s and 300 003 after; the compiler compiling itself, about 10% faster. Section 15, `conformance/stage6/text_building`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; follows the `Symbol<Class>` row above) **A plain `Symbol` template on `List` is a compile error naming `Symbol<$element_type>`.** Since the templates range over the class written inside the `Symbol`, a plain one on `List` would range over the list's own attributes (its buffer) and silently answer nothing. Reported where the template is declared. `Dictionary` is not checked yet. Section 8, `diagnostics/plain_symbol_on_list`. |
| 2026-09-24 | **D106 as built** (interpretations proposed by Claude, unconfirmed): **the defaults the rule covers are the literals `false`, `0`, `0.0`, `""` and `null`, and a bare `return`** -- `null` only in a function returning `T?`, where it is the default, and not an enum's first value, an empty list or a fresh object, which are defaults too but are not written as one literal. **The condition is turned around by swapping the comparison** (`==`/`!=`, `<`/`>=`, `>`/`<=`), dropping a leading `not`, adding `not` otherwise, and by De Morgan for `and`/`or`, side by side (`a and b or not c` becomes `(not a or not b) and c`), which reads better than `not (...)` wrapping when the sides are comparisons, as they mostly are. **An `else if` whose body only returns the default is covered too**, as the parser's older bare-`return` rule already did; that rule moved from the parser into the generator, which knows the return type. **In a constructor the message names `crash`**, since D28 still bans `assert` there. The D27 error is gone. **138 sites were rewritten** across `bootstrap/`, `library/` (the linux and mac folders included), `examples/`, `conformance/` and `docs/`, plus the `all_` step of fused chains, which the compiler writes as Spite and now writes as `assert`; 19 `crash x` and `assert x` lines and one `if x` that repeated an `assert x` the rewrite had just made were removed or unwrapped, since they proved nothing. **Every rewritten guard is now an `assert` site**: a failed one enters the D25 trace ring, and a crash after it reports it. That showed at once in `conformance/stage6/json_crash`: `TextBytes.equals` had become `assert left.length() == right.length()`, so every comparison of two texts of different lengths wrote a line into the ring and the crash report grew one. A length mismatch is the answer `false`, not a guard, so `equals` now returns `left_length == right.length() and ...` as one expression, and no expected output changed. **Open (proposed by Claude):** the library still has predicate asserts on hot paths that fail routinely (`String.matches_at`, `TextBytes.slice`, `Dictionary` probing), which will crowd the 32-entry ring in a long-running program; either the ring should skip asserts in `library/`, or those should be written as expressions like `equals`. |
| 2026-09-24 | (implements D81/D82; the form proposed by Claude, unconfirmed) **The compiler registers no classes: what it supplies is a reopening, and a supplied member is a `func` without a body.** `register_system_classes` and `add_builtin_function` are gone. `Memory`, `DynamicLibrary` and `TypedMemory` are `library/` files, and the members whose bodies stay C are Spite declarations with no block -- `func allocate_bytes(bytes: Long): Long` -- that the compiler merges into their classes right after `library/`, the same merge a later root gets (section 11), so a program can reopen them and `--final-classes` prints them. Reading a bodiless `func` back is what keeps the printed program compiling (check.sh's round trip). A bodiless `func` the compiler supplies nothing for is an error; supplied names skip the naming lint. The same form now covers the REPL's hooks on `Spite.Attribute`/`Spite.Function` (D92), `Concurrent`/`Parallel`'s `entry_address()`/`address()`, a number's `from_type` (D100) and `TypedMemory`'s slots, whose bodies the generator writes per instantiation. A supplied body starting `#define` is emitted inline (`Memory.allocate_stack_bytes`). A reopening that repeats an `enum`, a `generic` line or the `singleton` line replaces it instead of declaring it twice. A singleton's `copy()` and `deep_copy()` answer itself. Section 11, and section 17 for `DynamicLibrary`. |
| 2026-09-24 | (implements D79/D83; the readings proposed by Claude, unconfirmed) **Numbers are value classes in `library/`, and `this` is a keyword.** `library/int.spite`, `long`, `float`, `double`, `bool`, `tiny`, `short`, `byte`, `unsigned_short`, `unsigned_int` and `unsigned_long` are classes whose functions take the plain C value as their receiver; `this` names it (and, in any class, the instance itself: `registry.append(this)`), while `this.member` is an error naming the plain form. `text()` is Spite on `Long`, `UnsignedLong` and `Double` (the shortest-round-trip digits, over `library/number_text.spite`'s arithmetic), and interpolation calls the class's `text()`; `library/number_text.spite`'s `long_text`/`double_text` are gone. `String` gained the `to_tiny()` ... `to_unsigned_long()` the manual already promised (they did not exist), and casting text into a number calls them. Found on the way: a decimal literal such as `1.0` was written to C as `1`, so `1.0 / 3.0` divided integers; it is written as a decimal now. Compiling the compiler takes the same time as before. Section 4. |
| 2026-09-24 | (implements D88 and D92; proposed by Claude, unconfirmed) **Reflection keeps its data in private fields behind getters, and `_` means private.** `Spite.Class`, `Spite.Attribute`, `Spite.Function`, `Spite.Argument` and `Spite.Namespace` hold `_name`, `_namespace`, `_attributes`, ... and answer `get_name()` and the rest with no setter, so a write is the read-only error (`diagnostics/reflection_read_only`). A name starting with `_` is used only inside its own class -- the manual said `_name` is private without a rule, and without one `klass._name = ...` would undo D88. A member template's member may be a getter (`functions.map_name()`), and the REPL's templates list getters rather than private fields. A class of the standard library lists its functions through reflection (`Memory.functions`), where before only a program's own classes did. Section 8. |
| 2026-09-24 | (implements D98 and D101; the names proposed by Claude, unconfirmed) **`Memory` has a stack section, `TypedMemory<$value_type>` holds values of any type, and `value.memory` is where a named value lives.** `Memory.allocate_stack_bytes(bytes)` takes bytes in the calling function's frame (inline `alloca`, gone when the function returns). `TypedMemory`'s `read_value`, `write_value`, `release_value` and `value_bytes` keep reference counts right for their type, one shared instance per type; `List<T>` keeps its elements with it, so the compiler no longer adds `read_item`/`write_item`/`release_item`/`item_bytes` to every list, and `fused_chain_allocations` counts the one `TypedMemory` its lists share (11, was 10). `value.memory` answers a `Spite.Memory` (`library/spite/memory.spite`) with `address`, `bytes` and `section` (`'heap'`, `'stack'`, `'constant'`; `static` is a C word the name lint refuses). `docs/memory.md` builds a ring buffer and a stack sum on them. Sections 8 and 15. |
| 2026-09-24 | (implements D100; proposed by Claude, unconfirmed) **A Symbol parameter named `type` ranges over the program's types, and the number casts go through `from_type`.** Every number class has the compiler's `func from_type(type: Symbol, value: type.class)`; `from_int`, `from_unsigned_long` and the rest are instantiated when a cast needs them, with a body that is an inline C cast, and a right-to-left cast between two numbers calls the target's. `type` may name a parameter and begin a type path (`type.class`) for this, although it is a keyword elsewhere. The instances are not printed by `--final-classes` (the template is). A cast written before its target class is resolved (an attribute default in a class discovered earlier) still emits the plain C cast; the value is the same. Section 4. |
| 2026-09-24 | (implements D84, then D85 the same day; proposed by Claude, unconfirmed) **D84 was built on `Environment` first and then moved to `Build`.** The folding itself carried over: a field of the compile-time class compiles to its literal, `constant_condition` decides an `if` on it (alone, under `not`, `and`/`or`, or compared `==`/`!=` with a literal), the untaken branch is not generated, and no reader is emitted for it. `Environment` went back to D76's run-time reading only. |
| 2026-09-24 | (implements D85 and D86; the readings below proposed by Claude, unconfirmed) **`Build` is built, and every compiler option is one of its fields.** `library/build.spite` declares `mode`, `output`, `optimized`, `development`, `repl`, `repl_port`, `format`, `debug_memory`, `final_classes`, `operating_system`, `target_operating_system` and `program`; a program reopens it in `build.spite`. **Every** `Build` field is a constant in the built program -- one no flag sets folds to its declared default -- so a build fact is never read at run time. The flags follow the field names, which renames three: `--final-classes` is `--final_classes=folder` (no bare form: a bare flag is only for a `Bool`, `--optimized` meaning `--optimized=true`), `--repl-port 4000` is `--repl_port=4000`, `--no-format` is `--format=false`, and `--file=` is gone (D89). The compiler reads its options from the program's resolved `Build` rather than from its own `Build()`, because its own is folded when the compiler is built (`check.sh` builds it with `--mode=c`, which would make that its default); only `mode` and `format` come from the flag alone, being needed before the program is read. A flag naming an `Environment` field, a flag naming nothing, a value of the wrong type, `--operating_system` and `--program` are compile errors. `operating_system` is supplied by the compiler from its own folded `Build().target_operating_system`; `target_operating_system` defaults to it; each `library/<system>/build.spite` reopens `Build` with its own name, which matters only to a compiler that does not fold `Build` (the seed, while bootstrapping). `--final_classes` prints `Build` with its declared defaults, so the printed program does not re-run `--final_classes` itself. `library/<system>/environment.spite` (D80's `operational_system`) is deleted. |
| 2026-09-24 | (implements D89; proposed by Claude, unconfirmed) **An entry constructor with parameters is a compile error** naming `Environment()`, `Build()` and `Arguments()` (`diagnostics/entry_constructor_arguments`), and `spite folder` names a program by its folder. A path to a `.spite` file still names its file as the entry -- the compiler needs it for `bootstrap/spite_compiler.spite`, whose folder is `bootstrap/`, and `--mode=format` for the one file it formats -- while `check.sh`, `bin/spite` and the docs now name folders. `scripts/docs_corpus.spite` moved to `scripts/docs_corpus/` to be a folder program. `Arguments()` stays callable anywhere for positional arguments. |
| 2026-09-24 | (implements D97; the design proposed by Claude, unconfirmed) **A program is loaded by `launcher/launcher.spite`**, whose constructor is `load("library")`, `load("library/{Build().target_operating_system}")`, `load(Build().program)`. Discovery reads it first and follows its loads in order instead of walking `library/` by itself; a load there may use text and `Build` fields the compiler knows before reading anything (elsewhere `load` still takes a literal); `library/windows`, `linux` and `mac` are loaded only by name. The `load` of the program's folder is the one `load` that does something at run time: it constructs the program's entry class (and runs a `--repl` loop or `--repl_port` server around it, as `main` used to), so the generated `main` constructs `Launcher` and nothing else of the program. What stays C in `main` is the floor: handing `argv` to `Arguments()`, binary standard output on Windows, and the release of singletons and class objects and the `--debug_memory` report after `Launcher` returns -- moving those into Spite is open (`mortaros_missing_decisions.md`). `Launcher` counts as library code: `Spite.Class.instances` does not list it, and it is the one extra allocation `conformance/stage6/fused_chain_allocations` now counts. |
| 2026-09-24 | (applies Mortaro's spelling row above) **`operational_system` is renamed `operating_system`** everywhere outside quoted decision-log text: the manual's prose, `docs/`, `check.sh`'s messages. With D86 the only fields left are `Build().operating_system` and `Build().target_operating_system`. |
| 2026-09-24 | **D107** (decided by Mortaro): **a value turns into text with `to_string()`**, not `text()`: "all these text should probably be the to_string() to match our casting pattern." Converting to text is a cast like `to_int()` or `to_long()`, so every number class, `Bool` and anything else that answers `text()` answers `to_string()` instead, and interpolation calls it. |
| 2026-09-24 | **D108** (decided by Mortaro, extending D101): **every type's storage is visible Spite over `Memory`, and `Memory` chooses nothing itself -- the compiler places it.** "i dont see the storage model of things this way so we are probably hiding it behind a runtime, where is the string managing Memory? and Int and so on. Memory should be a abstraction that lets us allocate heap/stack/register but our compiler decides best use placement." So `String` declares the memory it holds (its bytes, length and capacity) as ordinary attributes in `library/string.spite`, `Int` and the other numbers declare theirs in their files, and `Memory` is the one abstraction all of them allocate through. Whether a given allocation lives on the heap, on the stack or in a register is the compiler's choice, not the program's; the C the compiler writes for the floor shrinks to what `Memory` itself needs. |
| 2026-09-24 | (implements D107; the diagnostic proposed by Claude, unconfirmed) **`text()` is `to_string()` everywhere.** Every number class and `Bool` declare `func to_string(): String`, the smaller integers widen and call `Long.to_string()`, and the generator's `string_conversion` (interpolation, `+` onto a `String`, `join`) calls `to_string` by name. Calling `.text()` on a number is an error naming the new name, the way `upper()` names `upper_case()` (`diagnostics/text_is_to_string`). `Memory.text(address, length)` keeps its name: it is not a conversion but the floor making a `String` from bytes. |
| 2026-09-24 | (implements D108, first step; the forms proposed by Claude, unconfirmed) **`String` and every number declare their storage in their own files.** `library/string.spite` starts with `var _bytes: Long` (the address of its characters, from `Memory`), `var _length: Long`, `var _section: Spite.Memory.Section` (where the compiler placed the characters: `'heap'`, or `'constant'` for a literal) and `var _capacity: Long`, and the compiler writes `SpiteString`'s C layout, and the static form a literal is compiled to, from those declarations instead of the prelude spelling them; the three it reads by name (`_bytes`, `_length`, `_section`) are an error when missing or of another type. Each number file starts with `var _memory = Memory().allocate_bytes(N)`: its width, allocated through `Memory` like anything else and placed by the compiler in a register, so the attribute is never a field -- the compiler checks `N` against the C type it emits (`diagnostics/number_memory`), `.memory.bytes` reads it, and reading `_memory` inside the class is an error naming `this` (`diagnostics/number_memory_read`). A number declares nothing else. |
| 2026-09-24 | (implements D108, second step; proposed by Claude, unconfirmed) **`String`'s functions are Spite over its own storage, and `TextBytes` is gone.** `library/string.spite` has a constructor `String(bytes: Long, length: Long)` (the only way Spite makes a `String` from memory; it writes the 0 after the bytes), `drop()` (frees `_bytes` through `Memory`), `length()`, `code_at()` (past either end it reads the 0 after the last byte, so it answers 0 without an `assert` in the trace ring), `slice()`, `sum()`, `equals()`, `less_than()`, `greater_than()` and two private helpers the in-place append calls. `+`, `==`, `<` and `>` on text call `sum`/`equals`/`less_than`/`greater_than` directly. `Memory.text` is Spite in `library/memory.spite`; `Memory.take_text` and `Memory.address_of` are removed, since only `TextBytes` used them. What stays generated C for `String` is the object header's part: allocate and initialise from the declarations, retain and release (skipping the `'constant'` section, and calling `drop()`), the uniqueness test in `SpiteString_append`, and `spite_string_from_bytes` for the C that hands text in (`argv`, enum names, foreign results), which calls `Memory.text`. **Found on the way, a hidden optimisation (D36): a singleton that holds nothing and has no `drop()` -- `Memory` -- is one static object**, never allocated, counted or freed, so `Memory()` costs nothing and every program allocates one time fewer (`text_building` 32 to 31, `fused_chain_allocations` 12 to 11). Without it `String.drop()` would reach a `Memory` singleton the program's exit had already released. The self-compile went from about 0.80 s to 0.72 s. |
| 2026-09-24 | (implements D108, third step; the rule proposed by Claude, unconfirmed) **The compiler places `Memory.allocate_bytes`, and `allocate_stack_bytes` is removed.** An allocation a function frees in the same block, whose address it only lends to `memory`'s reads, writes, copies, comparisons and `text`, gets a slot in the function's frame (256 bytes, or the literal size), falling back to the heap at run time for a larger size; everything else stays on the heap (section 10, "Placement"). The check is `bootstrap/source/generation/placement.spite`, over the syntax tree before a block is generated. Eight library functions qualify today (among them `Long.to_string`, `UnsignedLong.to_string`, `Double.bits`, `String.upper_case`/`lower_case`, `List.join` and two in `NumberText`), so `text_building` allocates 29 times (31 before). `allocate_stack_bytes` asked the program to choose, which D108 gives to the compiler, and was a second way to allocate; `docs/memory.md`'s sum is now `allocate_bytes` and `free`, and prints that nothing was allocated while it ran. |
| 2026-09-24 | **D109** (decided by Mortaro): **`Console.print` takes anything printable and calls its `to_string()`, and `Console.debug` prints any value's automatic `to_debug()`.** "Console.print should call the to_string automatically of anything thats passed so `type Printable { to_string: Spite.Function<String> }` but we should also have a Console.debug() and each class has an automatic to_debug(): String that returns something like `Class {attribute: value, other: value}` by nesting to_debugs until its satisfied, unless a to_json would always cover the same thing then Console.print_json could be the case but i think debug makes more sense, choose." Claude chose **`debug`**: JSON is a wire format for other systems (D95) and `to_debug()` is for a person or an AI reading state, so they stay two things. `print(...values: List<Printable>)` replaces the generator's special case (answers `mortaros_missing_decisions.md` items 13 and 56). |
| 2026-09-24 | **D110** (decided by Mortaro): **a singleton is always bound to a variable before it is used; calling a member on `Singleton()` inline is an error.** "multiple usages of inline singletons should be a compiler crime, force it to call it on top as a var first ... actually always force singletons to be used as variables first so AI does not get tempted to inline it." `load("library/{Build().target_operating_system}")` becomes `var build = Build()` beside the attributes and `build.target_operating_system`. |
| 2026-09-24 | (Mortaro: "foreign_text looks like a leaky abstraction, make sure we actually need that") **`ForeignText` goes; reading a zero-terminated text is a `Memory` function.** Its one function, `read(address)`, turns a C string at an address into a `String` -- a result of a foreign call, a name inside a C struct. The need is real (`Console.read_line`, `Program.environment`, each system's `Directory` listing, the leak report), but it is raw memory work, so it belongs on `Memory` beside `text(address, length)` rather than in a class of its own. |
| 2026-09-24 | **D111** (decided by Mortaro): **live reload watches files through the operating system and rebuilds only what changed.** "we should observe file changes for hotreload based on os instead of reloading everything naively everytime." Each operating system's folder supplies its own change notification (`ReadDirectoryChangesW` on Windows, `inotify` on Linux, `FSEvents`/`kqueue` on macOS, through `DynamicLibrary`, D80), with no polling; a change rebuilds the changed classes and what depends on their layout, not the whole program, and swaps them at the next drain point (D37). |
| 2026-09-24 | **D112** (decided by Mortaro): **hot reload has a flag of its own, `--hot_reload`**, "in case we want a simpler --repl without it." `--repl` and `--repl_port` stay plain -- no call table, no file watcher, no `reload` command -- and `--hot_reload` (a `Build` field, spelled with an underscore like every compiler option) turns those on. |
| 2026-09-24 | (Mortaro asked; the language chosen by Claude, unconfirmed) **Spite code blocks are fenced `gdscript` so GitHub highlights them.** GitHub highlights through Linguist, which only accepts a new language once it is used across a few hundred repositories, so `spite` cannot be added yet. Mortaro suggested Go as a middle ground; rendering one Spite sample through GitHub's own renderer as Go, Swift, Kotlin, TypeScript, Python and GDScript showed GDScript reads it best -- `#` comments, `func`/`var`/`assert`/`not`/`return`, both kinds of quotes and `{}` holes -- while Go treats `#` as text and `'symbol'` as a broken character. Titled blocks are ```` ```gdscript title=... ````; a ```` ```spite ```` fence is now an error in the docs corpus; `.gitattributes` maps `*.spite` to GDScript for the repository view. |
| 2026-09-24 | **D113** (decided by Mortaro, settling part of D94's `while` rule): **a member template can call a function of the caller for each element, and a `while` written only to do that is an error.** "we should have a way to iterate just for sake of console log for example a function `func say_hello(name: String) {...}` and then instead of say_hello_to_everyone we just map_say_hello() ... whiles created just for a thing that should be a function instead should be a compiler error i suspect it will narrow a lot our whiles." So `names.each_say_hello()` calls the caller's own `say_hello(name)` once per element (`each_` rather than `map_`, since nothing is collected; `map_` does the same and keeps each result when the function returns one). D94's review counted 145 loops of this shape -- the largest group -- so the error names the template to write. |
| 2026-09-24 | (implements D113; the readings below proposed by Claude, unconfirmed) **A template's member may be a function of the caller that takes the element alone, and the element's own member and such a function never both answer.** The caller is the class the call is written in; its function must have one parameter whose type equals the element type, and D15's requirements apply to its return type (`map_` of one returning nothing is the error, now naming `each_`). It works on a list or dictionary of any element type, `String` and numbers included. **When the element has a member of the same name, the call is an error naming both** rather than an order deciding: D113 allowed either, and an order would let a member added later change what an unrelated caller runs (`mortaros_missing_decisions.md` item 58). No template changed: the `library/list.spite` template is instantiated per calling class with a hidden last parameter for the caller, passed as `self`, and `item.attributes[member]` reads as `caller.<function>(item)`; such instances are not listed in the list's `functions` or offered at the REPL prompt. Fused chains (D105) take these steps too, after a `map_` to any type the fusion can spell, and pass the caller last. Section 8, `conformance/stage6/caller_templates`. |
| 2026-09-24 | (implements D113's error; the shape proposed by Claude, unconfirmed) **The `while` that only passes each element to a caller function is an error, detected only where the rewrite is exact**: the statement before sets the counter to `0`; the condition is `counter < list.count()` on a name or path of type `List<T>`; the body is `f(list[counter])` or `var item = list[counter]` then `f(item)`, plus `counter = counter + 1` (last, or right after the `var`); `f` is a function of the class taking one `T`, and `T` has no member `f`. The message names the template: `this 'while' only calls 'say_hello' with each element of 'names': write 'names.each_say_hello()'` (`diagnostics/caller_templates`). **2 loops were rewritten**, `generator.spite`'s `collect_body_facts` and `docs/reflection.md`'s `function_reflection`: D94's 145 were almost all loops that pass more than the element (`depth`, `scope`), which D113 does not cover and which stay `while`; how to carry them is `mortaros_missing_decisions.md` item 57. The review file's counts are updated. |
| 2026-09-24 | (implements D110; the readings proposed by Claude, unconfirmed) **A singleton's constructor call may only be the whole value of a `var`.** A call constructing a class with a `singleton` line (a generic one too) that is anything else -- the receiver of a member read or call, an argument, a returned or assigned value, an operand, a list element -- is an error: "'Build' is a singleton: bind it once beside the attributes, 'var build = Build()', and use 'build.target_operating_system'" (or "and use 'console'" when nothing is read from it). The binding may be an attribute or a local `var`: `String` keeps `Memory` in locals because a value class has no attribute to spare, and an error path binds `Program` where it exits. D77's constructor-as-argument does not extend to singletons. Checked on every statement the compiler generates and every attribute default, beside D77's check (`diagnostics/inline_singleton`, `diagnostics/inline_singleton_attribute`). On the way: **`Program` has its `singleton` line** -- D8 and section 15 already called it a singleton, but its file never said so, so every `Program().exit(1)` made an object; **`Build` is a static object** like `Memory` (D108's third row), since each of its fields is folded and it holds nothing at run time, so the launcher's `var build = Build()` costs no allocation and no allocation count changed; **the launcher** reads `load("library/{build.target_operating_system}")` and `load(build.program)` through a launcher `var` bound to `Build()`; **a number declares its storage in two lines**, `var memory = Memory()` and `var _memory = memory.allocate_bytes(4)`, where the binding is never a field and the compiler requires the allocation to go through it -- chosen over exempting `var _memory = Memory().allocate_bytes(4)`, because a number's file is where an AI learns how memory is declared. 58 sites were rewritten in `bootstrap/`, `launcher/`, `library/` (the eleven numbers, `environment`, the REPL), `scripts/`, `conformance/`, `diagnostics/` and `docs/`, 16 of them the compiler's own `Program()` calls. Sections 3, 8 to 12 and 14. |
| 2026-09-24 | **D114** (decided by Mortaro, in the SlopEngine session, relayed by that session): **a generic class reflects on a class's functions at compile time.** SlopEngine's two gaps -- an engine cannot tell whether a system class has `run_each` or `run_all`, and one generic cannot cover functions of different arity -- are closed by compile-time function reflection, "not variadic generics and not engine workarounds." Inside a generic class, asking whether `$system_type` has a function of a given name is decided at compile time and folds like `if $is_magic`; that function's `.arguments` is a compile-time list whose entries give a `.class` usable as a type and a `.name` usable as a `Symbol`, walked by a plural template the way attributes are, so one `Runner<$system_type>` runs a `run_each` of any arity. The engine's systems are `run_each(potion: Potion, target: Target)` (once per matching combination) or `run_all(potions: List<Potion>, targets: List<Target>)` (once with every match -- Mortaro chose lists of rows), the rows being `type`s declared in the system's file, and the engine derives its queries from those parameter types with nothing registered. **The spelling** -- `$system_type.has('run_each')` and `$system_type.run_each.arguments` -- was proposed by that session and is not yet confirmed. Scheduled after the SlopEngine HIGH bugs. |
| 2026-09-24 | (implements D111 and D112; the mechanism proposed by Claude, unconfirmed) **Live reload swaps functions through a slot per function, and rebuilds changed classes into a library the program loads.** In a `--hot_reload` build every function of the program's own classes, and each class's `_init`, is called through a function pointer the program can re-point, behind a forwarder with the function's own name; other builds keep direct calls and tree shaking. About a nanosecond per call. The build writes `<program>.reload_host` (functions and prototypes, slots, class ids, layouts) and `<program>.reload_files` (file hashes). A reload runs the compiler that built the program with its options and `--mode=reload --output=<program>`, which seeds class ids from the manifest, rebuilds the classes whose files changed plus the callers of any of their functions the running program has with another prototype or lacks, repeated to a fixed point, writes C only for those and what they reach that the program lacks, and compiles `<program>_reload_<n>.dll`/`.so`/`.dylib`. The program opens it with `LoadLibraryA`/`dlopen`, hands it its functions by name (the allocator included), and re-points the slots, at a drain point (D37), with waits blocking until the swap is done. Libraries are never unloaded. Section 14, "Live reload and 6b"; `docs/repl.md`; `check.sh`'s live reload step. |
| 2026-09-24 | (on D111; the rule proposed by Claude, unconfirmed) **A change to a class's attributes or enums is refused with an error saying to restart, and the program keeps all of its code**; so is a file that does not compile. D111 says a change rebuilds "what depends on their layout", but the instances already in memory have the old layout, and migrating them by attribute name needs every live instance and every reference to it, which nothing finds yet (`mortaros_missing_decisions.md` item 64). A new function or class, a changed signature (a new function, whose callers are rebuilt), a changed default (through `_init`) reload; a deleted function keeps its last code for whatever still holds it, a function value or the REPL, and the answer names it (`removed Monster.roar`). The REPL's reflection keeps the functions the program started with. |
| 2026-09-24 | (on D112; proposed by Claude, unconfirmed) **`--hot_reload` implies `--development`, works without a REPL, and is answered by two REPL commands.** Without a REPL the watcher still swaps and each swap or refusal is one line on standard error (`spite: rebuilt Monster`). With `--repl`/`--repl_port`, `reload` rebuilds and swaps what changed and answers `rebuilt A, B` (or `nothing changed since the code the program runs`, or the refusal), and `last_reload` answers what the last swap did, the watcher's included. A non-hot build answers both with an error naming `--hot_reload`. A `--hot_reload` build whose code never waits and has a `while` is a compile error, like `--repl_port`'s. |
| 2026-09-24 | (implements D111's watcher; proposed by Claude, unconfirmed) **Each operating system's folder reopens `HotReload` with `watch_folder` and `wait_for_change`**: `FindFirstChangeNotificationA` on Windows (D111 named `ReadDirectoryChangesW`; which file changed is found by hash, so the simpler call is enough), `inotify` on Linux, `kqueue` on the folder and each file on macOS (not FSEvents, which needs a run loop). The watcher thread waits until 100 ms pass without a change, then flags the scheduler; only the program's own folder is watched, not `load`ed folders. On Windows, a program and its libraries built by an MSVC-targeting clang share the C runtime DLL. `Program.executable_path()` is added to each folder so the compiler can record itself. |
| 2026-09-24 | **D115** (decided by Mortaro, in the SlopEngine session, relayed; extends D114): **a program finds its parts by folder convention, through compile-time reflection over its classes.** SlopEngine finds every system with no list: a system is every class whose namespace ends in `System` (a `system/` folder, e.g. `Ui.System.Interact`), and the program is its entry file, so plugin and composition files go. The language need is a compile-time plural template over the program's classes, filtered by namespace, instantiating a generic (`Runner<ThatClass>`) for each -- a sibling of D114's function reflection. The mechanism is decided; the spelling is open (the SlopEngine session proposed `Symbol<Spite.Namespace>` or a `classes` plural over a namespace pattern). |
| 2026-09-24 | **D116** (decided by Mortaro, in the SlopEngine session, relayed; extends D114): **a function's name can say when it runs, and a template reads that name.** Instead of a generic `run_each`, a system's function name places it: `update_each`, `render_all`, `run_each_before_input`. The engine owns a list of phases and a template matches the name pattern -- descriptive names as the ordering mechanism, with no hand-written after/before lists and no plugin order; within one phase, systems with no conflicts run in parallel. The language need is D114's `has(...)` taking a name pattern, with the matched part (the phase) available at compile time as a `Symbol` or text. The grammar (`<phase>_each` / `<phase>_all`, `run_each_before_<phase>` / `run_each_after_<phase>`) was proposed by the SlopEngine session and is for Mortaro to settle. |
| 2026-09-24 | (implements D109's `print`; the readings below proposed by Claude, unconfirmed) **`Console.print`, `write` and `error` are Spite, taking `...values: List<Printable>`, and the generator's printing special case is gone.** `type Printable { to_string(): String }` lives in `library/console.spite`, written with the form a `type` has today rather than Mortaro's `to_string: Spite.Function<String>` (open question 11). `String.to_string()` answers itself, `Spite.Class` its `.name`, `Spite.Namespace` its `.name_with_namespaces`, and every enum value answers `to_string()`, its name. For numbers, `Bool` and enum values to fit a `type` at all, **a plain value passed where a shape is wanted is boxed** -- one allocation, released like an object -- and a call through the shape reaches its class's function; `String` and `Symbol` are objects already. What the compiler still writes is the floor: `Console._write_output`, `_write_error` and `flush` are bodiless and `Prelude` supplies `fwrite`/`fflush`, and a `crash` writes its own operands. Output is byte for byte what it was, except that `flush()` no longer writes a stray line break; the self-compile time is unchanged within noise, and the two allocation pins moved (29 to 50, 11 to 17) because a variadic call builds a `List` (`mortaros_missing_decisions.md` 72 to 74). |
| 2026-09-24 | (implements D109's `debug`; the readings below proposed by Claude, unconfirmed) **`Console.debug(...values: List<Debuggable>)` writes each value's `to_debug()`, and every value has one.** A class that does not declare `to_debug()` answers one the moment something asks, and it calls `Spite.DebugInstance<$value_type>` (`library/spite/debug_instance.spite`), which walks the attributes with the plural template; every other value goes through `Spite.Debug<$value_type>` (`library/spite/debug.spite`). The format: `Name { attribute: value }` (`Name {}` when empty), `[a, b]`, `{"key": value}`, text quoted and escaped, a `Symbol` or enum value written `'name'`, a number or `Bool` as printed, `null`. **Cycles:** an object already being shown further up is written `Name {...}`, found by `==` against the objects its class is in the middle of showing. **Private attributes are left out**, because a plural over another class's attributes now skips the `_` ones instead of failing on them (`Json` too). Along the way: a `Symbol` passed to a `type` is boxed like an enum value, so it keeps its class; a union value passed to a `type` admits the union's members; a `type` or union answers `to_debug()` even when it does not require it; and a `DynamicLibrary`'s own declared functions (`to_debug`) are called as Spite rather than as foreign symbols. The REPL's display is unchanged, and becoming `to_debug()` is a proposal. `conformance/stage6/debug_values`; `mortaros_missing_decisions.md` 75 to 78. |
| 2026-09-24 | (proposed by Claude, unconfirmed; reads D104/D8 for `generic` lines, found building an ECS) **A generic singleton has one instance per set of codegen values.** `singleton` with `generic $component_type` was accepted, but each instantiation lost the line, so every `Column<Health>()` made a new object. Codegen values are literals like D8's constructor arguments, so each distinct set gets its own static slot: `Column<Health>() == Column<Health>()` and `Column<Label>()` is another. `TypedMemory<T>`, the one generic singleton in the library, is now one stateless static object per element type instead of an allocation per list. `conformance/stage6/generic_singletons`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; found building an ECS whose query rows are `type`s) **A Symbol template may range over a `type`.** `attribute: Symbol<$row_type>` with `$row_type` bound to `type Target { health: Health  alive: Alive }` answered nothing, so `fill_attributes(row, store)` was "no function". A shape now ranges like a class: its attributes, plus required functions that take no arguments for a single name; `row.attributes[attribute]` goes through the shape's reader, writer or caller; the plural walks its attributes in declaration order. `conformance/stage6/shape_attributes`, `docs/metaprogramming.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; extends D75) **`value == Storage<Health>` names a generic class without calling it.** `Storage<Health> {` used to parse as a comparison chain ending in an object literal, and with a `$` argument the object-literal loop never advanced past its first error, so the compiler hung. The right side of `==`/`!=` now reads `Name<values>` without parentheses as the class, `$` values included, so `if found == Storage<$component_type> { return found }` narrows a shape back to the generic class; the form anywhere else is an error saying to call it. A test through a `type` admits a fitting class to it. Every parser loop now stops at its first error, so a parse error can no longer hang (`diagnostics/generic_class_in_comparison`). `conformance/stage6/generic_class_test`, `docs/control_flow.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; the ECS needed a type's name without a value) **`$component_type.name` reads the bound class.** A member read through a codegen value that is a type goes to that type's `Spite.Class`, the way a class name does (`Hello.name`), so `var sample: $component_type = null` and `sample.class.name` are no longer needed to name it. The bare `$component_type` stays an error as a value. `conformance/stage6/codegen_class_name`, `docs/metaprogramming.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; the ECS runs systems held in a `List` of a `type`) **A shape's required function is a function value, and a `List` of a shape has member templates.** `Parallel(stage[index].run_once)` on a `List<Runnable>` read `run_once` as an attribute, so the codegen value could not be inferred; it is now a `Spite.Function` bound to the value, whose call goes through the shape's dispatcher. `stages.map_name()`, `filter_active()` and the other templates range over the shape's attributes and argument-free required functions, read through the shape. `conformance/stage6/shape_members`, `docs/values_and_types.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; open question 1 applied to a shape, found building an ECS) **The default of a `type` is an object.** `var second: $second_type = null` with `$second_type` bound to `type Target { health: Health }` held a null pointer, so every read made a fresh default `Health` and every write was lost. The default is now the object literal of the type's attributes, each at its own default, admitted to the shape: reads and writes through it are kept, and the plural Symbol template fills it. A `type` that requires a function keeps no default object. `conformance/stage6/shape_defaults`, `docs/values_and_types.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; D36, measured on the ECS's hot path) **A singleton is not reference counted.** Every fetch retained and released the one shared object, atomically once a program makes a `Parallel`, so two threads using `Memory()` or `Slot<Position>()` contended on one counter. A singleton never dies before the program ends, so its retain and release are now empty; its accessor records it when it is first made, and the program destroys every singleton in reverse creation order at exit (before, the counts decided the order). Measured: two threads fetching a generic singleton 20 million times each, 0.8 s before and 0.04 s after. `conformance/stage6/singleton_counts` pins that fetching allocates nothing and that `drop()` still runs at exit. |
| 2026-09-24 | (proposed by Claude, unconfirmed; found in SlopEngine's `window/system` and a Vulkan renderer) **A dotted name resolves relative to the class's folder in every position.** `Component.Created()` found `Window.Component.Created` from `Window.System`, but `Remove<Component.Requested>` and a parameter `swapchain: Component.Swapchain` were "unknown type": a type with a dot was only looked up as a full path. Types, generic arguments, union members and a class's enums now take the constructor's walk -- own namespace, folder, parents, program -- and a generic class named by a dotted path is instantiated. `conformance/stage6/relative_namespaces`, `docs/packages.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **Each side of an `and` narrows.** `crash a[position] and b[position]` narrowed neither path while two `crash` lines narrowed both. `assert` and `crash` over an `and` now check each side in order as its own statement (the crash line names the side that failed), and an `if` over an `and` tests each side in turn, narrowing the then-branch by every side. `conformance/stage6/and_narrowing`, `docs/failure.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's `Slot.fetch`) **A use in a branch a codegen test rules out counts.** The unused check ran after `if $slot_type == Entity { ... }` was folded away, so a parameter read only there was "never used" in every other instantiation. The untaken branch's names are now marked used before it is dropped, so the source is judged, not one instantiation. `conformance/stage6/folded_uses`. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **Reflection members win over a library's constants.** `current.class` on a `DynamicLibrary` (a generic instantiated over an attribute's type) was read as a foreign constant; `.class`, `.attributes`, `.functions` and `.memory` now always mean what they mean on any value. `conformance/stage6/library_reflection`, `docs/foreign_libraries.md`. |
| 2026-09-24 | (fixes D108's placement; proposed by Claude, unconfirmed) **A block placed in the frame is used by its `free`.** `var block = memory.allocate_bytes(4)` then `memory.free(block)` reported `block` as never used, because the placed `free` was written without reading the name; it now marks it used. `conformance/stage6/placed_free`. |
| 2026-09-24 | (proposed by Claude, unconfirmed) **A word Spite reads as a mistake cannot name a value.** `var none: Long = 0` was accepted, and every use of it was then "the empty value is written 'null'". `none`, `nil`, `undefined`, `self`, `new`, `import`, `require` and `elif` are now rejected where a variable or parameter is named, with the same lesson. `diagnostics/borrowed_name`, `docs/for_ai_writers.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; D109's printable, applied to text) **A text hole calls `to_string()`.** `"{attribute.class}"` was "a Class cannot be used where a String is needed" though the docs said it prints like the type name. A value in a `{}` hole whose class declares `to_string()` with no arguments is now turned into text by it, as `console.print` already does. `conformance/stage6/class_text`, `docs/metaprogramming.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's `potion` example) **No file may reopen the program's entry class.** An entry `potion/potion.spite` that loads a bundle holding its own `potion.spite` merged the bundle's class into the entry class `Potion`, so the entry's `Potion()` call recursed with no error. Another file of the entry class's name is now an error naming both files. `diagnostics/entry_class_reopened`, `docs/packages.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine had no way to time a frame) **`Clock()` reads time.** A singleton with `elapsed_nanoseconds()` and `elapsed_milliseconds()` (monotonic, for measuring) and `unix_milliseconds()` (the wall clock since 1970 UTC); each system reopens it with its own reading through `DynamicLibrary` -- `QueryPerformanceCounter` and `GetSystemTimeAsFileTime` on Windows, `clock_gettime` on Linux and macOS. The names, and whether a calendar breakdown (year, month, day) belongs here, wait on Mortaro. `conformance/stage6/clock_reading`, `docs/standard_library.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; a bug found building SlopEngine's Vulkan renderer) **A program runs in the folder `spite` was run from.** The compiler finds `launcher/` and `library/` from its own executable (the first folder above it holding `launcher/launcher.spite`, falling back to the working directory), reads the launcher's `load` paths against that folder while keeping `library/...` as the names every message and crash report shows, and builds into that folder's `.spite-cache/`. `bin/spite` stops changing directory. Section 13, `docs/compiler.md`; `check.sh` runs `conformance/stage6/working_directory` from another folder. |
| 2026-09-24 | (proposed by Claude, unconfirmed; a bug found building SlopEngine) **The compiler's own C names join with `___`, and a Spite name has one `_` between words.** A method named `allocate` failed in clang ("conflicting types for 'X_allocate'") because the generator wrote `<Class>_allocate` beside the class's `<Class>_<function>`. Instead of reserving each word, every name the generator makes for a class or function of its own -- allocate, init, default, make, retain, release, class_of, attributes, functions, instances, deep_copy, read_/write_/assign_/call_ accessors, an enum's name, and a function's hot, slot, waiting, perform, call and text_call -- is now `<owner>___<helper>`, and the naming lint rejects `__` inside a name, a trailing `_`, and more than one leading `_`, so no Spite name produces `___`. `copy`, `to_string` and `to_debug` keep their plain names: they are Spite functions every class answers and may declare. `init` stays an error as an abbreviation and `default` as a C keyword. Section 12; `conformance/stage6/generated_names`, `diagnostics/doubled_underscore`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; a bug found building SlopEngine's Vulkan renderer) **A foreign call's arguments cross at the width C wants.** `vkMapMemory(device, memory, 0, size, 0, out)` passed the literal `0` as a 32-bit `int` where C wanted a 64-bit `VkDeviceSize`, leaving the register's upper half undefined, silently. With a header, the call now goes through the header's prototype (`__typeof__(&Symbol)`, with `__builtin_choose_expr` for a `void` result), so C checks the count and converts every argument, allowing only integer-to-pointer conversion (a handle is a `Long`); without one, every signed integer, `Bool` and enum value is passed as `int64_t` and every unsigned one as `uint64_t`, which is what x64 and 64-bit ARM pass in a register or 8-byte slot anyway. Section 17, `docs/foreign_libraries.md`; `conformance/stage6/foreign_library` passes -1 to a `long long` with and without a header, a `Double` to a `float`, a `Long` to a `void*` and calls a `void` function. |
| 2026-09-24 | (proposed by Claude, unconfirmed; found building SlopEngine's Vulkan renderer, which needed `TypedMemory<Float>` to write a float into a C struct) **`Memory` reads and writes every width a C struct uses**: `read_short`/`write_short` (`Short`), `read_unsigned_short`/`write_unsigned_short` (`UnsignedShort`), `read_unsigned_int`/`write_unsigned_int` (`UnsignedInt`) and `read_float`/`write_float` (`Float`) join the byte, int, long and double ones as bodiless functions the compiler supplies, and lend an address the way the others do (D108). `Tiny` and `UnsignedLong` are left out: `read_byte` and `read_long` hold the same bits, and a cast reads them. Section 15, `docs/memory.md`; `conformance/stage6/memory_floor`. |
| 2026-09-24 | **D117** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 87): **bitwise operations are functions on the number classes**: "lets add the bitwise functions." Shifts, and, or, exclusive or and not are named functions every whole-number class answers (D83), not operator symbols -- the spelled-out form the binary-format ports asked for -- compiled to the single C operation, so zstd, Huffman and PSD code stops faking them with division by powers of two. The names are proposed by Claude and waiting for Mortaro's confirmation. |
| 2026-09-24 | (implements D117; the names and rules proposed by Claude, unconfirmed) **The whole numbers answer `shifted_left(count)`, `shifted_right(count)`, `bits_and(other)`, `bits_or(other)`, `bits_exclusive_or(other)`, `bits_inverted()`, `set_bit_count()`, `leading_zero_count()` and `trailing_zero_count()`.** One family: a shift is a past participle like `to_int()` is a conversion, and the combining functions start `bits_` because `and`, `or` and `not` alone are keywords and `xor`, `shl` and `popcount` are abbreviations. They are bodiless members the compiler supplies to the eight whole-number classes (D82), each body the single C operation, inlined at `-O2`. `shifted_right` is arithmetic on a signed type and logical on an unsigned one; a count of the width or more shifts every bit out (0, or -1 for a negative value shifted right), chosen over masking the count because a mask makes `1.shifted_left(32)` answer 1 and a binary format reader wants 0; a negative count halts with the function and the count named, since it is always a bug. `other` is cast to the receiver's type like any argument, which keeps them safe whatever item 88 decides. `Float`, `Double` and `Bool` have none, and calling one on them is an error naming the whole numbers. Rewritten with them: `Double`'s exponent and significand fields, JSON's UTF-8 encoding, `Socket`'s port bytes, the Windows folder attribute test and `pclose`'s exit status on Linux and macOS; the prime-modulus hashes (`Dictionary`, the tree shaker, the crash map) and the hexadecimal digit writers are arithmetic, not faked bits, and stay as they were. Section 4, `docs/values_and_types.md`; `conformance/stage6/bitwise_functions`, `negative_shift`, `diagnostics/bitwise_on_float`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; the spelling D114 left open) **A generic asks its type for a function with `$system_type.has_function('run_each')`, and `Symbol<$system_type.run_each>` ranges over that function's arguments.** `has_function` is an ordinary member of `Spite.Class` (it also answers at run time), and in a condition on a codegen type with a literal name it folds like `if $is_magic`; a non-literal name there is an error. `Symbol<X>` already ranged over X's members, so a function's are its arguments: `argument.name`, `argument.class` as a type or a `Spite.Class`, `argument.class.element_type`, and the plural calling the template per argument in order. A template over arguments that returns a value has one use for its plural: the whole argument list of a call to that function, `system.run_each(row_arguments())`, which passes one value per argument, evaluated left to right, and is exempt from D77 because the compiler writes the call. `has` was not taken alone (vague, SPITE.md), nor `$system_type.run_each.arguments` (a run-time list read at compile time, with no way to reach a call). Section 8; `conformance/stage6/system_functions`, `diagnostics/function_reflection`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; the spelling D115 left open) **`Symbol<System>` ranges over the classes of every folder named `system`.** A range naming no class or type is the end of a dotted namespace, and the classes of every namespace ending in it (`System`, `Ui.System`) are walked in order of their dotted names, generic classes skipped; `system.class` is the class and `system.name` its dotted name, and a range matching no folder is an error. A folder's members are its classes, as a function's are its arguments. `Symbol<Spite.Namespace>` was not taken because `Symbol<Spite.Class>` already means `Spite.Class`'s own attributes, nor a `classes` plural keyword, which would be a second loop form beside the plural. Section 8; `conformance/stage6/system_folder`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; the grammar D116 left open) **A name pattern is written the way a template's own name is: the parameter's name is the hole.** `phase: Symbol<$system_type.phase_each>` ranges over the functions whose names fit `<phase>_each`, `phase.name` is the matched text, `system.phase_each(...)` inside calls the matched function, and a template over `$system_type.phase_each` called from inside walks that function's arguments, its instances named for it (`row_potion_in_update_each`). `has_function("<phase>_each")` asks the same question as a condition. The pattern grammar is only this one hole; what `_each`, `_all` or `run_each_before_<phase>` mean is the engine's, which owns the list of phases. Section 8; `conformance/stage6/system_phases`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; found writing `Sprinkler.has_function('drain')`) **A symbol literal passed where text is wanted is that text, when no enum has a value by that name.** A `Symbol` already read as text everywhere; the literal `'drain'` did not, because an unknown literal was taken for an enum value and failed with "cannot tell which enum". A literal an enum does name keeps meaning that enum value. Section 8, `docs/reflection.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's Vulkan examples segfaulted at exit after the row above that stopped counting singletons) **Libraries are unloaded after every singleton, and a `drop()` cannot use a singleton already destroyed.** Singletons were already destroyed newest first, counted from when a constructor finishes, but `DynamicLibrary("vulkan-1.dll")` was first fetched in `Renderer.ensure()`, after `Renderer` was made, so it was unloaded first and `Renderer.drop()` called `vkDeviceWaitIdle` through an unloaded library; the count used to keep such things alive by accident. Every `DynamicLibrary` is now unloaded after all singletons, since anything may call through one and it calls nothing back. For any other singleton first made after the one whose `drop()` uses it there is no right order to find, so the fetch halts naming it and saying to keep it in an attribute, where it is made first. Standard output is flushed before teardown, so a crash in a `drop()` keeps what was printed. `conformance/stage6/singleton_teardown`, `conformance/stage6/singleton_used_after_exit`, `docs/classes_and_files.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's `Slot<T>` and `Column<T>` are generic singletons first touched by `Parallel` systems) **A singleton is made once even when two threads ask first.** The accessor was `if (cache == 0) cache = make()` with no lock, so two threads could both make it, and both append to the teardown list at once. In a program that starts threads, a fetch now loads the slot with acquire ordering and returns it when set (a plain load on x86); only when it is empty does it take the singleton's own spin lock, check again, make it, record it under the list's lock and publish it. `DynamicLibrary` slots do the same. A program with no threads keeps the plain check. Two threads first touching `Shelf<Int>()`, whose constructor is slow, made it twice before and leaked one copy. `conformance/stage6/singleton_race`, `docs/classes_and_files.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's click test, `click_test/click_test.spite`) **A name is looked up from the class's own namespace first, and a folder's entry class owns its folder.** The walk started at a class's folder, which for `ClickTest` (the entry of `click_test/`) is the program root, so `System.DriveClicks()` there was "unknown identifier 'System'" while every sibling file reached it; the walk now starts at the class's own qualified name, as the manual already said, and so do `type` and `union` members. The same report said the compiler segfaulted on this layout (the class referenced as `var click_test = ClickTest()` from `ClickTest.Composition`, its attributes spelled `ClickTest.System.X()`); a copy of SlopEngine laid out that way compiles and passes its click test with master and with the compiler before the recent SlopEngine fixes, so the crash was not reproduced and is not claimed fixed. `conformance/stage6/folder_class_namespace`, `docs/packages.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; D64, SlopEngine's `Psd.Layers.skipped_by_prefix`) **A check on an index read already proven says so.** `crash skipped[index]` inside `while not found and index < skipped.count()` failed with "the condition of 'crash' is a String", which read as if the attribute's read were typed wrongly. It was proven, correctly: the loop body runs only when both sides of the `and` hold, and a list in an attribute, a parameter or a local is proven the same way (checked side by side). The error now names the read and says a count or bound already proves it, so the check goes. `diagnostics/proven_index_check`, `docs/failure.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's PSD, zstd and .blend ports) **A generic class in a namespace can be constructed, and a failed constructor is one error.** `Asset.Pack<Asset.Texture>()` was "this generic constructor is not supported in this stage", because only a bare name was looked up, and the value it failed to make then cascaded into "a Value has no function", "'path' is never used" and the rest. The class name now takes section 11's walk (own namespace, folder, parents, program) like every other name, dotted or not, as its arguments already did. When a generic constructor cannot be made -- no such class, the wrong number of values, or `<...>` on a class that takes none -- the statement reports once, the variable it declares is poisoned, and later statements that read a poisoned name report nothing and mark what they read as used. `conformance/stage6/namespaced_generics`, `diagnostics/failed_constructor`, `docs/packages.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's PSD, zstd and .blend ports) **Any index without a call is a path that narrows.** `crash list[code - 32]` did not prove `list[code - 32]` on the next line; only a name, a path, a number or quoted text did, so every port wrote `var index = code - 32` first. An index made of names, member reads, literals, enum values, operators and `[]` reads is now a path, compared by D78's printed form, and assigning any whole name it reads undoes it, as assigning the list or a plain index does. A call inside the index keeps it a plain `T?`. As for a plain index, a call *between* the check and the read does not undo it; whether it should is open (mortaros_missing_decisions.md item 106). `conformance/stage6/structural_index`, `diagnostics/structural_index_undone`, `docs/failure.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's PSD, zstd and .blend ports) **A generic class compiles only what survives folding.** `Field<Int>.write_list`, reached only from `if $value_type == List { }`, was still type-checked for `Int` ("a Int has no function 'count'"), and so was the code after a folded `if`/`else if` chain, so the ports wrote one generic class per kind. A function of an instantiated generic class is now emitted only once the C already emitted names it (a surviving call, a function value, a reflection or shape table), repeated until nothing new is named; a folded branch that ends in `return` ends its block, and the rest of the block is not compiled but its names count as used. Classes that are not generic still compile every function, so a mistake in an uncalled function is still reported there. `conformance/stage6/folded_helpers`, `docs/metaprogramming.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's PSD, zstd and .blend ports) **A check on an element already proven says what proved it.** `crash list[index]` inside `while index < list.count()` stays an error, but the message was "the condition of 'crash' is a Int, but it must be a Bool", which sent the ports looking for a comparison to add; the row **A check on an index read already proven says so** above fixed that in parallel with a message naming no proof. A proof by a bound or a count now remembers its condition, so the error reads "'list[index]' is already proven by the loop condition 'index < list.count()', so this 'crash' proves nothing: remove it" (or names the `crash`/`assert`/`if` check, or "an earlier check"). `diagnostics/proven_element`, `docs/failure.md`, `docs/for_ai_writers.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's PSD, zstd and .blend ports, where `func short()` was rejected) **The reserved-name error lists every name C reserves.** It said only that the name was reserved, so a port renamed `short` to `int` and was told again. The message now ends with the whole list, which `docs/for_ai_writers.md` and section 12 repeat. The list is master's at merge time (the `___` join of D-row "The compiler's own C names" made `allocate`, `make` and `release` legal, and `default` stays reserved). Whether functions and attributes need the list at all is item 107. `diagnostics/reserved_name`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; D115, SlopEngine's `Cook` walking `Symbol<Recipe>` in a program with no recipes; supersedes the "matches no folder is an error" reading in `mortaros_missing_decisions.md` item 99) **A folder range that matches nothing walks nothing.** The error fired while generating every class's functions, so it failed a program that loaded `Cook` and never called it. An empty walk is what `Symbol<Label>` over a class with no attributes already does, and what `Symbol<$system_type.run_each>` does for a type without `run_each`. The typo check moves to where a single class is named: `cook_recipe_bread()` for a class no folder holds says which classes the folders do hold, or that there is no such folder. `conformance/stage6/empty_folder_range`, `diagnostics/empty_folder_range`, `docs/metaprogramming.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; SlopEngine's `type Healing` in `examples/healing/system/regenerate.spite`) **The nearest `type`, `union` or `enum` wins over a class further out.** A bare name in a type position looked for a class along the whole walk first and for a declared type only after it, so the program's entry class `Healing` shadowed the `type Healing` the system file declared ("'Healing' has no attribute 'health'"). A declaration now sits in the walk at the level of the class that declares it: the using class first, then its folder's entry class, then each parent. This also closes `docs/KNOWN_ISSUES.md`'s "a class of your own can hide a class the library nests": a program's class `Entry` no longer hides `Directory`'s union `Entry`. `conformance/stage6/nearest_type`, `docs/packages.md`. |
| 2026-09-24 | (proposed by Claude, unconfirmed; D114-D116, SlopEngine's runner could not move its combination loop out of `run_phase_each`) **A template's symbol can be passed to a helper, which is compiled once per symbol.** `run_combination(phase)` treated `run_combination` as an ordinary function taking a run-time `Symbol`, so inside it `system.phase_each(...)` had no matched function ("does not define 'phase_each'"). A function whose ranged `Symbol<...>` parameter is not a word of its name is now a template reached only by passing it the calling template's own symbol, by name, over the same range; the instance (`run_combination_for_update`) keeps the binding, pattern and argument templates included. Passing anything else, or calling it from outside such a template, is an error. A plain `Symbol` parameter is unchanged. `conformance/stage6/passed_symbol`, `diagnostics/passed_symbol`, `docs/metaprogramming.md`. |
| 2026-09-24 | **D118** (decided by Mortaro): **an attribute nothing uses is an error, like an unused local.** From a SlopEngine system file holding `var world = Resource.World()` that no function reads: "world is never used, unused variable declarations should cause a compiler error." Section 5's unused rule, which covered locals, parameters and private functions, now covers a class's attributes too. |
| 2026-09-24 | **D119** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 48): **a bare `crash` is the one form for a branch that cannot happen.** The formatter used to rewrite it to `crash false`; it now writes `crash` for both, so `crash false` is formatted to the bare form. |
| 2026-09-24 | **D120** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 40): **a `Bool` compiler option may be given bare**: `--optimized` means `--optimized=true`, and `--optimized=false` stays allowed -- the convention of every command line. |
| 2026-09-24 | **D121** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 61): **`Program` is a singleton**, as D8 always said: `library/program.spite` carries the `singleton` line, and a program binds it once (`var program = Program()`). |
| 2026-09-24 | **D122** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 94): **`Int` is spelled `Integer`, and every name built from it follows**: "it should be read_integer, but the type should also be called Integer. correct me when i abbreviate things, sometimes im just so used to saying like that i forget its not the word, and its a bad practice for lazy devs." So `Int` -> `Integer`, `UnsignedInt` -> `UnsignedInteger`, `to_int()` -> `to_integer()`, `Memory.read_int`/`write_int` -> `read_integer`/`write_integer` (and `read_unsigned_integer`), `library/int.spite` -> `library/integer.spite`. By the same rule (Claude's correction, per Mortaro's request to point these out): `Bool` is an abbreviation of `Boolean`, so `Bool` -> `Boolean` and `to_bool()` -> `to_boolean()` as well. The language's own type names get no exemption from the abbreviation rule. |
| 2026-09-24 | **D123** (decided by Mortaro, in the SlopEngine session, relayed): **no function-level generics, ever -- a function that takes anything takes a `type`.** "we do not need a generic here, we can use a type instead that is more flexible, function generics can ONLY come if the class has a generic declared for it." SlopEngine's entity API follows it: `world.create_entity()`, `world.create_entity_from_bundle(bundle)`, `entity.add_component(component)`, `entity.remove_component(...)` and `entity.remove()` replace `Spawn(bundle)` ("Spawn seems hidden") and `Insert`/`Remove<T>`; `add_component(component: Anything)` takes an empty `type`, which accepts any class, and `component.class` is the real class. Two language needs come with it, as requests (spellings proposed by the SlopEngine session): **a class test against a codegen type** -- `if item == $wanted_type { }` narrowing, the existing `value == Monster` rule extended to `$` types -- and **an attribute's value as an object** at run time (`attribute.object`, typed as the language's "any"), so a bundle's attributes can be walked and each handed to `add_component`. |
| 2026-09-25 | **D124** (decided by Mortaro, for D123's `remove_component`): **a class name passed as an argument is its class object**: `entity.remove_component(Ui.Component.Pressed)`, received as `component_class: Spite.Class`. "its metaprograming all around, we are not actually passing a class we are passing a instance of the class class that represents the component class." A class name already reads as its `Spite.Class` (`Gadget.name`); this lets it be passed like any value. |
| 2026-09-25 | **D125** (decided by Mortaro, answering half of `mortaros_missing_decisions.md` item 12): **the remote REPL's port is fixed when the program is built**: `--repl_port` stays a `Build` field, folded, with no run-time override; changing the port means rebuilding. |
| 2026-09-25 | **D126** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 12): **`Socket` is public library surface**, and the networking standard library grows from it: "make it public, later we want to build http and websocket as standard library and tcp/udp for games." Planned on top of it: HTTP and WebSocket in the standard library, and TCP and UDP for games. |
| 2026-09-25 | **D127** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 86): **Spite gets the best date and time support it can, with time zones only a presentation layer.** "we want to have the best date support possible to avoid JS fiasco that made them change date implementation and now ecosystem has a shitton of wrappers for things. we should also force a correct way of dealing with timezones as having data in a single format and timezones being just a presentation layer. we can look into which languages do it best modernly and copy their api." Time is stored in one format -- an exact instant -- and a time zone only turns an instant into a calendar date and clock time for display (or parses one back); the API is copied from the best modern design after comparing them (JavaScript's `Temporal`, `java.time`, Rust's `jiff` and the like). `Clock()`'s monotonic `elapsed_*` functions stay for frame timing. |
| 2026-09-25 | **D128** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 41): **the compiler first assembles the whole program in memory, then decides what to produce, and the outputs combine.** "the compiler needing them first is a smell. we should first assemble the instruction in memory then decide what to do with the output. that way we can both mix and match like generate a exe AND generate the c code for me to see, but we also always make sure to have the complete picture before we start thinking of things like tree shake." So no compiler option is read before the program is: the program's own `build.spite` can set every option, `mode` included; the single `mode` becomes a set of outputs (an executable, the C, a run, formatted sources, final classes) chosen together; and whole-program steps like tree shaking run only on the complete program. Supersedes the D85 implementation's exception for `mode` and `format`. |
| 2026-09-25 | **D129** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 91): **each output has its own path flag, and without one it goes into the program's own folder.** "it should be a flag, if no flag is passed we consider to build to the same folder, but we should be able to pass an output for the exe and an output for the c separately as flags if we want to have things done at same time." So the executable and the C each take a path option (with D128, asking for both in one build writes both), and by default they are written beside the program rather than into the language repository's cache. Built together with D128. |
| 2026-09-25 | **D130** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 42): **a program is named only by its folder.** The compiler's own entry is renamed `bootstrap/bootstrap.spite` (class `Bootstrap`), and a `.spite` file path given to `spite` becomes an error naming the folder form -- one way to name a program. Built together with D128/D129. |
| 2026-09-25 | **D131** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 37): **the launcher keeps its name and place**: `launcher/launcher.spite`, class `Launcher`, in its own folder at the repository root beside `library/` -- the one file that shows how every program is loaded. |
| 2026-09-25 | **D132** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 1): **the two environments keep their names**: `Build` for what is known at compile time and `Environment` for what is read when the program runs. |
| 2026-09-25 | **D133** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 15): **the concurrency classes keep their names**: `Concurrent(function)` runs on a fiber of the program's thread, for work that waits; `Parallel(function)` runs on a thread of its own, for work that computes; both answer `wait()` and join when dropped. |
| 2026-09-25 | **D134** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 19): **a concurrent result joins on first use, and every IO class uses it.** "implicit, and all our IO classes should use it to force things to be efficient." SPITE.md's rule wins over the built `wait()`: reading a `Concurrent`'s (or `Parallel`'s) value is what waits for it, so ordinary code never writes `.wait()`; and `File`, `Directory`, `Socket` and the other IO classes start their work concurrently themselves and hand back values that join where first used, so independent reads overlap without the program asking (D99's hidden async). |
| 2026-09-25 | **D135** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 20): **`Parallel` runs on a thread pool, and the `parallel_each_` list templates are built now.** A fixed set of worker threads is reused by every `Parallel` instead of one new OS thread per call, and D35's data-parallel templates (`list.parallel_each_update()`, splitting a list's elements across the pool, joined when the call returns) come with it, ahead of the engine needing them. |
| 2026-09-25 | **D118, the ruling on walks** (decided by Mortaro, relayed by the coordinating session, 2026-09-25: "keep the unused as error"): **a compile-time walk makes an attribute used only when it reads the attribute's value** -- `x.attributes[attribute]`, and so `Json`, `to_debug()` and the REPL's display. `attribute.name` and `attribute.class` are metadata and do not count, so SlopEngine's scheduling markers (`var world = Resource.World()`, `var main_thread = Resource.MainThread()`, read only by a classify walk over `attribute.class`) are errors. Built with the rest of the interpretation below, which is **(proposed by Claude, unconfirmed)**: a read is anything that takes the value (own functions and defaults, `thing.world`, union and `type` reads, a getter answering the read, `thing.attributes`, a `load(...)` argument, the REPL in a REPL build); writing is not reading, unlike locals today (item 110); the rule is source-level, so an uncalled function's reads count and bodies the compiler never compiles (folded branches, unemitted generic functions, unreached Symbol templates) count their names, own bare names precisely and `x.name` by name; `_name` exempts an attribute (it is private too, item 111), including a binding kept for its constructor's effect; number and `String` storage and `Build`/`Environment` settings are never checked; the check runs only on an otherwise clean compile. Fixing the repo removed 44 attributes and renamed none (the compiler's write-only `overrides_declaration` on both declarations and the formatter's unread `shape`; example and fixture fields; `var console = Console()` bindings nothing used) and kept 6 more by giving four fixtures a function that reads what only a metadata walk or the REPL looked at (`class_text`, `codegen_class_name`, `docs/repl.md`'s two programs). `diagnostics/unused_attributes`, `conformance/stage6/attribute_uses`, `docs/style.md`. |
| 2026-09-25 | **D136** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 110): **only a read counts as using a name; inside a function body that is judged strictly, and public attributes carefully.** "only read is count as use, but we should also be careful about things that external classes might be reading since attributes are public. inside function bodies its safe to say something is never read." So a local or parameter that is only ever assigned is an error, like an unread attribute; and a public attribute is only reported where every possible reader is visible -- which is why D118 checks the program's own classes and private attributes, and exempts a loaded package's public attributes. |
| 2026-09-25 | **D137** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 111 and open question 6): **`_` means "unused on purpose" only on a parameter; everywhere else it means private, and an unused variable is always an error.** "_ means only unused on purpose if thats in the name of a parameter, else it means private, if a user try to make an unused variable then it should be an error why did you waste resources making a variable you wont use." So a parameter a signature must declare but the body ignores is written `_name`; a local or attribute nobody reads is an error with no spelling that silences it, and D118's message stops suggesting `_name`. |
| 2026-09-25 | **D138** (decided by Mortaro): **`Json` takes the object in its constructor and infers its generic from it**: `var json = Json(order)` then `var text = json.write()`, instead of `Json<Order>()` and `json.write(order)` -- "should take the object at constructor, unless it causes a problem, then it would be able to infer its type and not need to argue the generic." Reading keeps a way to name the class it reads into, since there is no object yet. |
| 2026-09-25 | **D139** (decided by Mortaro): **a constructed object must be kept and used.** "we need to enforce that constructed objects need to be referenced and it cannot be by a unused variable. the spite agent is way to happy to use Constructors to mimic functional programming." A constructor call written as a statement on its own (`Spawn(bundle)`, `Report(text)`) is an error, and one stored in a variable nothing reads already is (D118, D136); a class whose construction is the whole point is written as a function instead. |
| 2026-09-25 | **D140** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 112): **a function nobody calls is not reported**: "a function nobody calls is probably impossible to know, because it could be a library meant to be read externally." Its callers may be code the compiler cannot see, as with public attributes (D136), so uncalled functions stay legal (tree shaking removes them from the build) and the reads inside them still count. |
| 2026-09-25 | **D141** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 104): **a `drop()` that fetches an already destroyed singleton at exit halts with a message** telling the program to keep that singleton in an attribute (so it is made first and destroyed last); every `DynamicLibrary` is unloaded after all other singletons. |
| 2026-09-25 | **D142** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 85): **singletons are not reference counted**, and each is destroyed at exit in reverse creation order even when a leaked object still points at it; `--debug_memory` still reports the leaked object. |
| 2026-09-25 | **D143** (decided by Mortaro, answering `mortaros_missing_decisions.md` items 55 and 63): **internals are ordinary, inspectable classes in REPL and development builds, and optimised away only in production.** "we should be tree shaking the things we do not use at optimized production builds, but at repl we should be able to investigate any of the internals as normal classes, so possibly we wanna make them normal and tree shake. unless it carries performance problems." So in a `--repl`, `--repl_port`, `--hot_reload` or `--development` build, `Memory`, `Build` and the other singletons that hold nothing are normal objects that reflection (`.instances`, `.attributes`) sees; in an `--optimized` build they may be static objects and anything unused is tree-shaken. |
| 2026-09-25 | **D144** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 60): **a singleton is bound as an attribute, never a local**: every singleton a class uses is visible at the top of its file (`var program = Program()` beside the other attributes). The one exception is a value class such as `String`, which has no attribute to spare and keeps the local form. |
| 2026-09-25 | **D145** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 62): **a number declares its storage in two ordinary lines**, `var memory = Memory()` and `var _memory = memory.allocate_bytes(4)`, with no special header keyword. |
| 2026-09-25 | **D146** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 30): **`this` works in every class**, only to pass or return the object itself (`registry.append(this)`); a member is still reached by its bare name, and `this.name` stays an error. |
| 2026-09-25 | **D147** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 28): **zero hidden code: everything the compiler supplies must end up as explicit Spite, and nothing may tie Spite to C.** "why do they have no body? we should aim to have ZERO (0) hidden code in the compiler everything is explicit spite in the end and the code is just written there ... we should possibly be able to do this with spite, why are we being forced to write C? i want the flexibility to get rid of C if we decide to use another compiler later." Rejected: bodiless compiler-supplied functions, visible C inside a body, and routing even byte reads through C calls. Direction (the concrete form is open, item 113): whatever exists as a library function becomes a real Spite body that calls it; the few operations the machine does directly (reading and writing the value at an address, atomics) are named in Spite source and documented in one place, and each backend -- C today, another later -- lowers that same small set, so no Spite code changes with the backend. |
| 2026-09-25 | **D148** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 58; supersedes D113's caller-function templates): **an iterator sees only the element and the list, never the calling class; the caller's functions are passed as bound function values.** "we should not have the caller class influence on this ... Greeter should never leak into the iterators, it only uses the methods and attributes of Person or if List itself defined a method with that complete name like each_is_adult. the only way greeter would be able to use its own methods on a iterator of list would be passing the reference of that function which is bound to the greeter this like `people.map(say_hello).filter_active()` -- say_hello comes from Greeter while active must come from Person." So `people.each_say_hello()` resolving to the caller's `say_hello(person)` is removed (along with the ambiguity error and D113's `while` rule that named it); `map`, `each`, `filter` and the rest also take a function value (`people.each(say_hello)`, `people.filter(is_invited)`) bound to its owner (D17/D39), and still chain and fuse (D105). |
| 2026-09-25 | **D149** (decided by Mortaro, answering `mortaros_allocators_proposal.md` question 1): **no value classes: every class is passed by reference, `copy()` gives an independent copy, and the compiler optimises behind that.** "each class always pass as a reference, BUT we can call copy on a instance to pass a reference to the copy by value. this is very simple for coders to understand. BUT again we need compiler to be super smart and understand that if a copy is only used in that single reference it can be passed by value, or if the copy isnt even mutated, it can just reference original if cheaper and so on. up to you to dig all possible optimizations for compiler, while keeping the api surface super simple." So the performance the ECS needs (packed component data, no counting) comes from the compiler proving what the source does not say: a copy used once passed by value, an unmutated copy sharing the original, objects that never escape laid out inline or in registers, reference counting elided where ownership is provable. The API stays reference-only. |
| 2026-09-25 | **D150** (decided by Mortaro, answering `mortaros_allocators_proposal.md` question 4, names open): **one class is the place in memory and allocators are its owners, and choosing an allocator never means redeclaring types.** "i agree but we need better names then, Memory and Address do not tell me i have to use either Memory or Arena ... how do we change allocators if we want? ... does that mean we need to redeclare EVERY type in spite system for each allocator possible? sounds like overengineering." Answer built into the rule: types are allocator-agnostic; a `List`, a `String` or any object takes memory from whichever allocator is current, and a program or engine sets the current allocator around a scope, so switching is one line where the engine runs a system. The names for the place and for the allocators are still to choose (`mortaros_missing_decisions.md`). |
| 2026-09-25 | **D151** (decided by Mortaro, naming D150): **memory lives in its own namespace, and each allocator is named for what it is.** "memory is such a basic construct and a dangerous one i think it needs its own namespace so its Memory.Address and Memory.Allocator but then instead of Allocator have which allocator it is." So the place is `Memory.Address`, and the owners are `Memory.Heap` (the default), `Memory.Arena`, `Memory.Frame` and so on, each answering the same shape so any of them can be the current allocator; there is no class called `Allocator` a program uses directly. |
| 2026-09-25 | (implements D136 and D137, and the D118 fixes SlopEngine needed; the readings below proposed by Claude, unconfirmed) **Only a read uses a name, only a parameter may be `_name`, and a loaded package's public attributes are not judged by one program.** An assignment no longer marks a local or parameter used (`x = "{x} more"` still reads it); an unread local is `'total' is never read: remove it` whatever its name; an unread parameter says `remove it, or name it '_amount' if the signature needs it`; the attribute message drops its `_name` suggestion, and a private attribute nothing reads is reported. A declaration whose own statement failed is not also reported as unread. **Packages:** D118 skips a public attribute declared in a folder the entry file `load`s -- judged by the declaration's own file, so an attribute a program adds by reopening a loaded class is still checked -- and keeps checking the program's folder, the standard library and every private attribute; this is what makes SlopEngine's `flex_layout` compile (`Ui.Component.BackgroundColor.color`, read only by the render package). **The Spawn walk was not missed:** `Spawn`'s `insert_attribute` reads `bundle.attributes[attribute]` and does count for every bundle a program spawns (`click_counter`, which spawns `Window.Bundle.Window`, compiled under D118); `flex_layout` and `render_parity` load the window package but never spawn a window, so no walk is bound to that bundle there -- a package case, fixed by the rule above rather than by counting a generic template's walk for every class. Repo rewrites: `var _ringing = Concurrent(ring)` and `var _counting = Parallel(say_done)` now read their handle; fixtures that kept `_` locals renamed them. `diagnostics/written_not_read`, `diagnostics/package_attributes`, `conformance/stage6/package_attributes`, `mortaros_missing_decisions.md` item 114. |
| 2026-09-25 | **D152** (decided by Mortaro, answering how a program picks an allocator for one object): **every object has a tree-shakeable `.memory`, and setting its allocator right after construction is compile-time intent.** "each object has a tree shakeable reference to its memory and then we can instance.memory.address and we can even instance.memory.allocator = Memory.Arena() or something like that, but then we need to be able to figure out at compile time that if something is allocated on standard options then moved to another allocator we always intended to use that allocator ... we need to make sure it will not be expensive, it should be free in production since this feature is mostly debug reflection." So `var scratch = List<Integer>()` followed by `scratch.memory.allocator = frame` allocates `scratch` in `frame` from the start -- the compiler reads the assignment made before the object escapes as where it was always meant to live, so nothing is allocated twice or moved; an object whose allocator is never set uses `Memory.Heap`; the stack is never chosen by the program (D108: the compiler places anything that provably does not outlive its function); and `.memory` costs nothing in a production build. |
| 2026-09-25 | **D153** (decided by Mortaro, extending D152): **setting an allocator after an object was used is a compile error**: "its a compiler error, if we wanted to copy we should manually call copy() and set allocator on the new copy before passing around." Moving an object to another allocator is written as a `copy()` whose allocator is set right after it is made. |
| 2026-09-25 | **D154** (decided by Mortaro, extending D152): **a `List` holds references, so only an object whose own allocator was set lives in that allocator; a `Vector` holds its items inline.** "only the object that had allocator change on a list goes into that allocator, because a list by nature is just a bunch of pointers. but we should have a Vector type that has every item of the list inline for fast lookup, javascript only has the Array, we have both options unless we can make one with advantages of both." Setting a `List`'s allocator moves its buffer of references, not the objects it points at. `Vector<T>` stores each item's data contiguously (the packed layout the ECS needs, D149's optimisations made explicit), unless a single type turns out to give both. |
| 2026-09-25 | (proposed by Claude, unconfirmed; SlopEngine's `var fields = value.class.attributes`, and `docs/KNOWN_ISSUES.md` item 1 / `mortaros_missing_decisions.md` item 44) **A class object's stand-in is neither leaked nor counted.** A class object describes its `.attributes` and `.functions` from a stand-in instance at its defaults. Once any program read a class object's `.attributes`, every class object built one, including those of the singletons its attributes name: `Console___default()` made a second console, and a singleton's release does nothing, so `--debug_memory` reported `Console` and `DynamicLibrary` leaked. A singleton's stand-in is now destroyed after its attributes are read, or at exit with the singletons when its functions are bound to it. The same stand-in was the extra member of `.instances` item 44 describes: it is taken out of the registry as soon as it is made. Whether a function of `Spite.Class` should be bound to an instance at all is still item 44's question. `conformance/stage6/reflection_stand_in`, `docs/reflection.md`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; SlopEngine's `use_potion` reported `World`, `List_Int`, `List_World_Command` and a class that changed from run to run as leaked) **A singleton's stand-in never runs its `drop()`, and the leak summary names only live objects.** The leak was not in singleton teardown: the real `World` was made once, recorded once and destroyed at exit. The leaked `World` and its two lists were the stand-in its class object is described from, which SlopEngine began building once `Columns` keyed its tables by class object; the stand-in fix above covers it. That fix destroyed a singleton's stand-in with its `drop()`, so a singleton that prints or closes something in `drop()` did it twice more at exit for an object that was never the singleton; a singleton now has `___discard` (release its attributes and free it) beside `___destroy` (`drop()`, then discard), and the stand-in is discarded. The class that changed from run to run was the leak summary itself: it kept a list of every object ever registered and counted a record whenever its address was live again, so a freed `Console` or `List_Spite_Function` whose memory was reused by a string was reported, depending on where the allocator put things. `AllocationTable` now keeps the class id in the slot beside the live address and drops it on free, which also stops that list growing with every object a program ever made. `conformance/stage6/singleton_stand_in`, `docs/memory.md`, `docs/reflection.md`. |
| 2026-09-25 | **D155** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 82): **load order stays the rule, and a package is configured through methods the program provides, not by out-reopening it.** "things can become dll later which we only can have them load the time they actually load. it should be transparent what is loading when ... if something that loads later is intended to be replaced by something that loads first perhaps it should just call a second method which the config passes but the current class does not have, that way we also raise an error if a expected config method is not there, and if we want to have a default we use metaprograming to check if method does not exist." So a later `load` still replaces earlier declarations; a package that wants a setting calls a method the program declares (for example `build.base_folder()` defined in the program's `build.spite`), which is a compile error when missing unless the package supplies a default behind `has_function` (D114). There is no command-line form that starts a framework around a program (`spite slop game`), which would be a second, forgettable way to start one. |
| 2026-09-25 | **D156** (decided by Mortaro): **a worktree is a folder that `load`s its base and reopens what it changes**: "a worktree should simply be a load("./master_folder") this is more an agent instruction to have me test things in cheap fake worktrees then when approved merge things into the actual code, unless we can find a way to integrate git worktrees into the language since load already accepts git paths." No overlay feature: an agent proposing a change to a Spite program makes a small folder whose entry file loads the original and reopens only the classes it changes, for Mortaro to test before the change is merged into the real code. Integrating git worktrees waits on D38 (a `load` of a git path), which is not built. |
| 2026-09-25 | **D157** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 114): **a loaded package's unused public attributes are tree-shaken, not reported; an unused singleton binding is an error everywhere.** "at that point in which we compile its not necessary, we can simply tree shake them off. its safe to unallow singletons to be declared and not used tho if a consumer wanted to use it would just declare the singleton again." So D118's package exemption stands for public data, while a singleton bound as an attribute that nothing in its own class uses (`var world = World()`) is an error even inside a loaded package -- a class that needs it binds it itself. |
| 2026-09-25 | (proposed by Claude, unconfirmed; D127's design, `mortaros_missing_decisions.md` items 115-122) **Time is an `Instant`, and a zone only shows it.** `Instant`, `Duration` (exact, no days) and `Period` (years, months, days), the zone-less `LocalDate`, `LocalTime` and `LocalDateTime`, `TimeZone` with `to_local`, `to_text` and `to_instant(local, ambiguity)` naming `'compatible'`, `'earlier'` or `'later'` every time, the `TimeZones()` database (the operating system's: `icu.dll` on Windows, `/usr/share/zoneinfo` on Linux and macOS, nothing embedded) and `TimeText()` for ISO 8601; no stored zoned type. `Clock.now()` answers an `Instant` in place of `unix_milliseconds()`. NodaTime's model with Temporal's vocabulary, argued in `docs/time.md`. |
| 2026-09-25 | **D158** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 115): **the time types keep their built names**: `Instant`, `Duration`, `Period`, and for zone-less calendar readings `LocalDate`, `LocalTime` and `LocalDateTime` (java.time's and NodaTime's words), with `TimeZones()` and `TimeText()`. Considered and passed over: Temporal's `Plain*` and Go's `civil`; Go and Ruby themselves have no separate zone-less types, which is the model D127 rules out. |
| 2026-09-25 | **D159** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 117): **every `to_instant` names its daylight-saving rule** (`'compatible'`, `'earlier'` or `'later'`); there is no default, so each conversion from a calendar reading decides the gap and overlap case where it is written. |
| 2026-09-25 | **D160** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 116 and revising D158): **the zone-less calendar types are plainly `Date`, `Time` and `DateTime`, and nothing stores a zone.** "i suspect just Date is the most clear thing if its not attached to a timezone, JS couldnt do it for legacy reasons. when we present a time with a timezone then it might need a prefix to represent it." `LocalDate`/`LocalTime`/`LocalDateTime` become `Date`/`Time`/`DateTime` (Mortaro read "Local" as "has a zone", which is the confusion Temporal avoided). A `Date` is a calendar reading -- year, month, day -- with no zone and no offset, not UTC; an `Instant` (nanoseconds since 1970 UTC) is what a program stores; a meeting is one `Instant` that each user sees through their own zone, so no stored zoned type exists. Anything that carries a zone for presentation gets a prefixed name. |
| 2026-09-25 | **D161** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 119): **time zones come from the operating system, and from the browser under WebAssembly**: "take from the OS, but we need to know that webassembly will not have access to DLLs and still needs dates, so at that point it needs to get them from browser." Native targets read the OS database (Windows' `icu.dll`, `/usr/share/zoneinfo`), nothing is embedded, and a program may ship its own zone file for systems without one; the web target (section 17) reads zones through the browser's own time-zone support instead. |
| 2026-09-25 | (proposed by Claude, unconfirmed; D123's first request, SlopEngine's `Slot<$slot_type>`) **A codegen value bound to a class is a class test on the right of `==`/`!=`.** `if item == $wanted_type { found = item }` was "'$wanted_type' is a type here, so it cannot be used as a value"; it now tests for the bound class and narrows `item` as D75's `if item == Health` does. A number, `Bool` or enum binding tests for its boxed class (what it is inside a `type`, D109) and narrows back to the plain value; `String`, `List` and `Dictionary` bindings test for their own classes. Where the value's static type answers, the test folds: a `Health` against a `Health` binding is `true`, against `Label` `false`, and a union without the bound class `false` instead of D75's "never true" error, which a generic class cannot avoid across its bindings. A `type` or union binding stays the "is a type here" error. `conformance/stage6/codegen_class_test`, `docs/control_flow.md`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; SlopEngine, a class with `func load(id: String): Int` calling `return load(id)`) **A class's own `load` function wins over the package `load`.** Every `load(...)` call compiled to nothing, so the call became `(void)0` where an `Int` was wanted and clang failed; in the entry file, a `load("level")` statement was read as a folder to load. The alternative was making `load` a reserved name; `load` is an ordinary word a program will want, and a file that declares it is not one that loads packages. Now a class that declares `func load` calls it, and discovery reads no folder from that file. `conformance/stage6/load_function`, `docs/packages.md`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; the readings of D124) **A class name is its class object wherever a `Spite.Class` is wanted.** An argument to a `Spite.Class` parameter, a `var` declared `Spite.Class`, an assignment to one and a `return` from a function returning one; a codegen value bound to a class passes the same way (`Storage<Health>` without parentheses is still read only on the right of `==`). Elsewhere a bare class name stays the "is a class, not a value" error, which now names the `Spite.Class` form, because an untyped `var kind = Health` is nearly always a missing `()`. D77 is untouched: `f(Health())` is an instance, `f(Health)` a class object, and the parameter's type decides. **`==` with a `Spite.Class` on the left compares class objects** instead of being D75's class test, which asked whether a class object was an instance of `Health` and was silently `false`. `conformance/stage6/class_argument`, `diagnostics/class_as_value`, `docs/reflection.md`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; D123's second request, SlopEngine's `create_entity_from_bundle(bundle: Anything)`) **`attribute.object` is an attribute's value as an object, and `.attributes` works through a `type`.** `Spite.Attribute` gains `_object: Object?` and `get_object()`, where `type Object { }` is declared in `library/spite/attribute.spite`, so the type reads `Spite.Attribute.Object?`; no built-in "any" type is added, since an empty `type` already accepts any class (`mortaros_missing_decisions.md` item 124). A number, `Bool` or enum attribute is boxed, as D109 boxes a plain value passed to a shape, and an attribute holding `null` answers `null` (item 125). `bundle.attributes` on a `type` or union value dispatches on the object's class at run time to that class's attribute list. The object is filled only in a program that reads `.object`; elsewhere it stays `null` and admits nothing (D42). Built with two mechanisms: a class admitted to a `type` is also admitted to every `type` a value of it was passed on to, so the classes that reach `Object` late (when the attribute lists are written) still reach `Anything`'s release and dispatch; and a class test against a number class (`if component == Int`) makes its box, which before this was only made by a program that passed a number into a shape first, so the test was false. `conformance/stage6/attribute_object`, `docs/reflection.md`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; SlopEngine reading `build.class.attributes` and each attribute's `.name` and `.value` in a loaded package) **`DynamicLibrary`'s own members are never foreign constants, and a settings singleton's attributes answer its settings.** Reading a class object's `.functions` anywhere makes every class object describe its functions, including a `List<DynamicLibrary>`'s, whose member templates (`sum_handle`, `map_file_name`, `find_by_handle`, `sort_by_handle`) read `item.handle` -- and a member read on a `DynamicLibrary` was taken as a constant from the library's header, so the build failed inside `library/list.spite`. A `DynamicLibrary`'s own attributes and functions now read as themselves, like the reflection members already did. Past that, the program crashed: `Build` holds nothing at run time (every attribute is a setting the compiler folds), so its one static object had null attributes and `.value` read through them; its attribute list now takes each value from the folded setting. `conformance/stage6/settings_and_libraries_reflected`, `docs/foreign_libraries.md`, `docs/reflection.md`. |
| 2026-09-25 | **D162** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 88 and open question 3): **arithmetic keeps right-to-left casting -- the right operand becomes the left operand's type -- and wider arithmetic is written wider operand first.** "if we need to use wider, it means we should put long * int, we need a way for AI to understand this is the behavior. else it always casts as is now right cast to left." So `Integer * Long` is an `Integer` multiply and `Long * Integer` a `Long` one. How the rule is made learnable (proposed by Claude, unconfirmed): a right operand wider than the left is a compile error that names the rule and the fix ("the right side is a Long, so this is an Integer multiply: write the Long first"), and a constant expression that overflows its type is a compile error, so the silent `(65536 - 120) * 65536` wrap SlopEngine hit twice cannot happen. |
| 2026-09-25 | **D163** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 124): **`Anything` is a built-in `type`, the counterpart of `Nothing`**: "we have a Nothing, so might make sense to also have a Anything which is a type instead of a class." The library declares `type Anything { }` once; a parameter that accepts any object is `component: Anything`, programs stop declaring their own empty types, and `Spite.Attribute.object` is typed `Anything?`. |
| 2026-09-25 | **D164** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 125): **`attribute.value` is the actual instance the attribute refers to, and `attribute.object` goes.** "value should return the actual instance that the attribute refers to. we should try to optimize this as much as we can on compile time to, mostly tree shake it off, but in case of small numbers we could possibly just copy its value around during compile." `Spite.Attribute.value` is typed `Anything` (`Anything?` when the attribute can be null), so `entity.add_component(attribute.value)` works; text is `attribute.value.to_string()` (the REPL and `to_debug()` call it). The compiler resolves as much as it can at compile time and tree-shakes the rest; a number is passed as its plain value where the compiler can see its type, and boxed only where it must travel as `Anything`. |
| 2026-09-25 | **D165** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 126): **a bare class name is its class object only where a `Spite.Class` is expected**; `var kind = Health` with no type written stays an error pointing at the typed form, so a forgotten `()` is caught rather than silently becoming a class object. |
| 2026-09-25 | **D166** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 127): **`load` is a reserved word**: it always means loading a package, so a function named `load` is an error suggesting a descriptive name (`load_texture`); the built behaviour that let a class's own `load` shadow it is removed. |
| 2026-09-25 | **D167** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 128): **inside a generic, a class test that can never be true for one instantiation folds to `false` and its branch is removed for that copy; it is not an error.** "generics being possibly unused code is used for tree shake, its on purpose not an error." Outside generics a never-true class test stays D75's error, because there it is always a mistake. |
| 2026-09-25 | **D168** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 107): **Spite carries no limitation from C**: "we should never carry limitations from C as C might be eventually removed its just one of our possible compilers." The C-reserved name list goes: `register`, `short`, `default`, `int` and the rest are ordinary Spite names wherever Spite's own rules allow them (the abbreviation rule still rejects `int`), and the C backend renames every local, parameter and member it emits so no Spite name can collide with a C word or macro (`far`, `near`). Follows D147: nothing ties Spite to C. |
| 2026-09-25 | **D169** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 106): **a call between a proof and a read undoes the proof only if the compiler finds the call may change what the proof depends on.** "we need to be smart enough at compiler to figure out if going down into that function has any chances of altering that value (without needing to have a const keyword). if theres a chance it will alter the value we need to prove it again with a compiler error in case we do not. if it does not change the value then no proof is needed." So the compiler follows the called function (and what it calls) to see whether it can write the list or any field the index reads; if it can, the read needs proving again and not doing so is a compile error; if it provably cannot, the proof stands. No `const` keyword is involved. |
| 2026-09-25 | **D170** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 3 and open question 20): **an `if` with an `else` directly inside a branch of another `if` with an `else` is a compile error**, naming the fix: move it into a function named for what it decides, or, when both test which member of a union or enum a value is, use one `switch`. A flat `else if` chain and an `if` without an `else` stay legal (`mortaros_review_while_and_else_if.md`: 16 cases in the compiler, 7 flatten, 9 become named functions). |
| 2026-09-25 | **D171** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 2 and open question 15): **`while` stays for loops over state; a `while` that only walks every element of a list and does what an iterator chain does is a compile error naming the chain.** The detectable shape is the review's: a counter from 0 to `list.count()`, the body reading `list[counter]` and doing only what a member template or a function value passed to `each`/`map`/`filter` (D148) expresses, the counter's only write being `counter = counter + 1`. Loops that pass extra arguments, walk state, scan text or stop early stay `while`. |
| 2026-09-25 | **D172** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 7): **D77's readings (1)-(4) stand, and a variable may never shadow a function name visible in its scope**: "we should never be able to shadow a function name from that scope, as functions can be passed around by reference it would become super confusing." So `var file_stem = file_stem(path)` is a compile error -- a local, parameter or attribute named like a function of its class (which names a function value, D17/D39) must be renamed (`var stem = file_stem(path)`). The rule covers constructor calls; any call anywhere inside an argument counts; text holes are not arguments; calls were moved out of `while` conditions and the right of `and`/`or` only where harmless. |
| 2026-09-25 | **D173** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 8): **D78 stays narrow**: only a call with arguments that appears in every branch of an `if`, at the start of the branches, is reported, so everything it reports can be computed once before the `if` without changing behaviour. |
| 2026-09-25 | **D174** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 11): **in REPL builds, every loop of the program's own code gets an automatic check point**, so a program that never waits still answers the remote REPL. The compiler adds a one-flag check at the end of each loop iteration in `--repl`, `--repl_port` and `--hot_reload` builds only (production builds are unchanged), and the compile error for a `--repl_port` build with a loop that never waits goes away. Commands are still answered where the program's state is consistent: at a wait or at the end of an iteration. |
| 2026-09-25 | **D175** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 14): **a `generic` line may name a constraint**: `generic $item_type: Printable` accepts only classes that satisfy the `type` `Printable`, and a use that does not (`List<Pet>` for a constrained list) is an error at the use site naming the constraint, instead of an error deep inside the generic's body. The constraint is optional and uses an existing `type`. |
| 2026-09-25 | **D176** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 16): **hidden async/await is a compile-time transform into state machines, not fibers**, because "our goal is to have no runtime only compile time -- look at how go is useless for wasm because of the size of the wasm by default." Fibers need a scheduler, a stack per fiber and stack switching, which WebAssembly cannot do without a whole-program transform that bloats the binary. Instead the compiler rewrites each function that can reach a wait into a resumable state machine; what remains at run time is a minimal event loop continuing work when IO completes, which in a browser is the browser's own. No function colouring (D35): the transform is invisible in the source. Supersedes the fiber mechanism built for D99/D103. |
| 2026-09-25 | **D177** (decided by Mortaro): **everything must be tree-shakeable and keep Spite at zero runtime**: "always ask yourself if anything we do can be treeshaken, and if it will keep us at zero runtime." A program that does not use a feature carries none of it, and no feature may require a shipped scheduler, interpreter or registry -- the work is done at compile time (D147, D176); debug and REPL features may cost something at run time only in those builds (D143). Every proposal states how it tree-shakes and what it costs when it runs. |
| 2026-09-25 | **D178** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 113): **reading and writing the value at an address are language primitives on `Memory.Address`**, like `+` on numbers: `address.read_long(16)`, `address.write_float(8, value)` and the atomics are lowered by whichever backend compiles the program (about 10-15 operations each backend implements), usable only from `library/`; everything else -- allocation from OS pages, copying, comparing, loading libraries -- is plain Spite calling the operating system through `DynamicLibrary`. No C in any Spite source (D147). |
| 2026-09-25 | **D179** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 17): **a `Parallel` function may reach only its own instance and its locals**, checked at compile time -- but "threads will want to do things like log things to console or get input from hardware, how can we make sure our singletons are thread safe without all the annoying rust-ness?" Proposal (Claude, unconfirmed; item 130): the library singletons threads need (`Console`, input, `Clock`) are thread-safe inside -- each call serialised by a lock or a per-thread queue flushed in order -- and a `Parallel` may call them; a program's own singleton may be reached from a `Parallel` only if its file declares itself thread-safe with a header line (`shared`), making its author responsible; nothing is annotated at call sites, and a program that never uses `Parallel` pays nothing (D177). |
| 2026-09-25 | **D180** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 123): **an engine's phases are an enum a program can reopen, and a name pattern's hole matches only that enum's values**: "phases should be a enum, just like later which environments we will have. which can be reopened by the user. we should check against the phases values of enum, so our language needs good enum reflection." So `Symbol<$system_type.phase_all>` is constrained by the engine's `Phase` enum, and `interact_all` is an ordinary function unless `interact` is a `Phase` value; enums gain reflection (their values, names and order, walkable at compile time) and reopening (a program adds values). The same will hold for environments. |
| 2026-09-25 | **D181** (decided by Mortaro, for SlopEngine's plugins): **every folder name is lowercase snake_case, package roots included** (`slop_window_plugin`, never `slop-window-plugin`); one naming rule for every folder. |
| 2026-09-25 | **D182** (decided by Mortaro, for SlopEngine's plugins): **every subfolder of a program still loads automatically, so swappable plugins live beside the program, not inside it**: a project's themes and platform plugins are sibling folders it `load`s, so only the one it loads is compiled. |
| 2026-09-25 | **D183** (decided by Mortaro, answering `mortaros_missing_decisions.md` item 130): **a singleton reached from a `Parallel` is made thread-safe by the compiler, with no keyword.** One whose functions never write its attributes after construction needs nothing; one that changes its own state gets a lock of its own around every call from outside it (a call it makes to itself does not lock again), emitted only in programs that use `Parallel` (D177); and its functions may hand out only numbers, text, copies (`copy()`) or other singletons made safe the same way, so nothing it owns escapes the lock -- a compile-time check at `return`. Library singletons threads use (`Console`, input, `Clock`) may do better by hand (a buffer per thread written in order, input snapshots). No `shared` header line. |
| 2026-09-25 | **D184** (decided by Mortaro, extending D183): **the compiler picks the cheapest safe form for each singleton, not only a lock**: "the compiler should also try to do better for each singleton, so its as optimized as possible under the hood." The lock is the fallback, chosen only when nothing cheaper is proven safe. Examples of what the compiler looks for, per singleton, from what its functions actually do: read-only state needs nothing; a single counter or flag uses an atomic instead of a lock; state only appended to (a log, a command queue) gets a buffer per thread merged in order; state each thread only touches its own part of gets split per thread; reads that vastly outnumber writes use a reader-writer lock. All of it is invisible in the source, tree-shaken when unused, and absent from programs that never use `Parallel` (D177). |
| 2026-09-25 | **D185** (decided by Mortaro): **every optimisation the compiler does on its own is documented for users**: "we need all our out of the box optimizations to be documented well in our docs so users understand what may happen and dont worry about it." `docs/optimizations.md` lists each one -- what it does, when it applies, whether it is built or planned, and anything a user could ever observe (allocation counts under `--debug_memory`, reflection in production builds) -- and every new optimisation adds its entry in the same change (D102). |
| 2026-09-25 | (implements D128; the spelling and the readings below proposed by Claude, unconfirmed) **The outputs are `Bool` fields of `Build`, read only after the whole program is.** `mode` and `output` are gone. `run` (default `true`) builds the executable and runs it; `executable` builds it without running; `c_source` writes the C; `format` (default `true`) rewrites the program's own files; `final_classes` stays a folder (`""` is off), since no folder has a safe default -- one inside the program would be read back as part of it. Every output that is on comes from the same compile (`spite game --executable --c_source --run=false` builds and writes the C), and a run builds its executable where `executable` would, so asking for one never implies another. With every output off the compiler still compiles the whole program and reports its errors (what `check.sh`'s diagnostics use). **Formatting before compiling** becomes: read everything unformatted, then, if `format` is on and a program file's formatted text differs, rewrite it and read the program again, so the compiled program and every error line are the file on disk; discovery no longer formats as it reads. **`target_operating_system` is still read from the flag alone**: the launcher needs it to know which library folder is part of the program before reading the rest, so a program's `build.spite` cannot set it (it is silently overridden today; `mortaros_missing_decisions.md` asks). `--mode=tokens`, `--mode=tree` and `--mode=canonical` are dropped rather than made outputs, since nothing used them and `--final_classes` shows the tree as source. `spite format [--check] <file-or-folder> ...` and `spite reload` are commands of the compiler, like `spite connect`, not options: `format` works on files that need not be a program (`library/`), and `reload` is what a `--hot_reload` program runs, printing its errors to standard output where the program reads them. **D177: a built program carries none of this** -- choosing and writing outputs is the compiler's driver; in the program `Build` stays a folded singleton whose static struct gains three unread, zero-initialised fields (16 instead of 13), and no code. |
| 2026-09-25 | (implements D129; proposed by Claude, unconfirmed) **`executable_path` and `c_path` place the two outputs, and `""` means beside the program**: `game/game.exe` (no extension when the target is not Windows) and `game/game.c`. A path option given on the command line for an output that is off is an error naming the missing flag. What describes an executable goes beside it wherever it is: `.crashes`, and a `--hot_reload` build's `.reload_host`, `.reload_files` and `_reload_<n>` libraries with their C. The only intermediate, the C the C compiler reads when `c_source` is off, goes to the language repository's `.spite-cache/<executable name>.c`. Paths are folded into the build like every `Build` field, so the compiler's own C is written to its default `bootstrap/bootstrap.c` by `check.sh` and moved from there: a `--c_path` naming a per-run folder would differ in every generation and the fixpoint would never hold. A `--hot_reload` program built with its default path keeps its reload libraries inside its own watched folder, so each reload wakes the watcher once more and costs one compile that answers "unchanged". |
| 2026-09-25 | (implements D130; proposed by Claude, unconfirmed) **The compiler's entry is `bootstrap/bootstrap.spite`, class `Bootstrap`, and a `.spite` path is an error**: `error: 'game/game.spite' is a file, and a program is named by its folder: spite game`; a folder with no entry named after it is an error naming the file looked for. `check.sh`, `bin/spite`, the docs, the README, the seed's README and `COMPILER_PLAN.md` name `spite bootstrap`. The seed keeps its file name, `bootstrap/seed/spite_compiler.c`: it is the compiler, not the entry, and renaming a file every concurrent branch regenerates would turn each merge into a conflict. |
| 2026-09-25 | **D186** (decided by Mortaro, superseding D166's reading of `load`): **`load` is a keyword written without parentheses**: `load "../../plugins/render_vulkan"`. "load(...) makes me think load is a function of the class, we should make load a proper keyword." `load(...)` with parentheses becomes a parse error naming the keyword form. |
| 2026-09-25 | **D187** (decided by Mortaro): **no line-width limit, but a line that would be too long may not construct an object inside a call**: "i dont want to just simply have a limit on width of a line, just a limit on how things need to behave at different sizes. if a line is going to be to big and has a instantiation happening, it should require it to be instantiated into a variable first." So `var button = world.create_entity_from_bundle(Bundle.CounterButton(screen.id))`, over the formatter's width, is an error asking for `var counter_button = Bundle.CounterButton(screen.id)` on its own line first; a short line may keep D77's one level of constructor inside a call. |
| 2026-09-25 | **D188** (decided by Mortaro, revising the flag spelling of D85/D120): **command-line flags are kebab-case, and the `Build` field behind each stays snake_case**: "all languages use --repl-port instead of --repl_port, the _ feels weird for flags, we should use - for flags only but the variable name becomes snake case." `--repl-port=4000` sets `Build.repl_port`, `--hot-reload` sets `hot_reload`, `--final-classes`, `--c-source`, `--executable-path` and so on; an underscore in a flag is an error naming the hyphen form. |
| 2026-09-25 | **D189** (delegated by Mortaro: "you will be the one debugging, do whatever you think is best for AI at this one"; the design is Claude's): **an `assert` in `library/` never enters the D25 crash trace.** The trace exists to explain how *the program* reached a crash: which of its own assumptions failed first. The standard library's asserts are routine control flow (probing a `Dictionary`, matching text, reading past an end), fire thousands of times in a long-running program, and crowd out the program's own entries in the 32-entry ring, so an AI reading a crash sees library noise instead of the cause. The compiler therefore writes no ring entry for an `assert` site whose file is in `library/`: it is filtered at compile time, so it costs nothing at run time (D177) and a library guard is exactly as cheap as an `if`. The program's own folder and every `load`ed package keep full recording. A crash *inside* the library still reports its own site and the call chain, so nothing about the crash itself is lost; only the library's passing-through guards are left out. This settles D106-as-built's open question (skip the library, rather than rewriting each library guard as an expression). |
| 2026-09-25 | **D190** (decided by Mortaro, revising D128's `format` output): **formatting a file never needs it to compile, and compiling always formats first -- there is no way to turn it off.** "we should be able to use --format (or --lint whichever makes more sense) to format files without having to fix all the code to compile, but we should never be able to compile without it formating the code first". `spite format <file-or-folder>` (already built: it needs a file to parse, not to compile) stays the name, since formatting is what it does. `--format=false` and `var format = false` in `build.spite` are removed: every compile formats the program's own files (the entry folder and every `load`ed root, not `library/`) before anything else, and a file the formatter refuses (D55's empty line, a failed safety check) is still left alone with its reason, which is an error in the compile it belongs to rather than a skipped step. Test inputs that must stay unformatted (`diagnostics/`) are compiled from a copy by `check.sh`, never by a flag. |
| 2026-09-25 | **D191** (decided by Mortaro, relayed by the SlopEngine session, extending D135): **any code may hand work to the program's one thread pool.** "language should allow new things to ask for participation in the threadpool" -- an engine system, an asset cooker, a texture loader or a library class all submit work the same way, by `Parallel(function)`, and get back a handle whose `finished` never blocks and whose value joins on first use (D134); no submission starts an OS thread of its own, and there is no second pool to create or pass around. Mortaro also rejected a child process for SlopEngine's re-cooking: "you shouldnt need another process" -- long work runs in-process on the pool while frames continue. A singleton such work reaches is made safe by the compiler (D183), in the cheapest proven form (D184). A program that never uses `Parallel` carries no pool (D177). |
| 2026-09-25 | (implements D189 and D190; the readings below proposed by Claude, unconfirmed) **A library `assert` is compiled without its ring write**: a site whose file path starts `library/` (the operating-system folders included, a program file reopening a library class not) keeps its branch, its drops and its default return, and loses only the store into the 32-entry ring, so it is exactly an `if`; its `.crashes` line stays, and `crash` sites are unchanged. `conformance/stage5/library_guards_untraced` fails three library guards (`String.slice` past the end, `matches_at` under `starts_with` and `ends_with`) around one program `assert`, then crashes: the report lists the program's assert alone. **`format` is gone from `library/build.spite`**, and naming it is a compile error rather than the generic unknown-flag one, whose advice (declare `var format = ...` in `build.spite`) would otherwise quietly add a program setting that formats nothing: `--format=...` is "not a compiler option: every compile formats ... first" (`diagnostics/format_flag`), and a `var format` in a program's `build.spite` is an error at its line (`diagnostics/format_setting`). **A file the formatter refuses stops the compile** with `path: error: the formatter refuses it, ...` and exit status 1; a file that does not lex or parse never reaches the formatter, because reading the program reports it first. `check.sh` compiles `diagnostics/` from a copy in its work folder, run from there so the paths in the expected errors are unchanged; three of those programs were unformatted (`empty_constructor`, `shape_mismatch`, `unused_names`), so their expected errors now open with the `formatted <path>` line the compile prints, and their line numbers did not move. The docs programs, the remote-REPL sessions and the live-reload copy are already copies in `.spite-cache`, so they just lost the flag. |
| 2026-09-25 | (implements D143, and settles `mortaros_missing_decisions.md` items 139 and 140; the readings proposed by Claude, unconfirmed) **Inspectable builds keep the internals as ordinary objects; production builds hide them.** A `--development`, `--hot_reload`, `--repl` or `--repl_port` build is inspectable: the C is not tree-shaken (before, only `--development` and `--hot_reload` turned the shaker off), and a singleton that holds nothing (`Memory`, `Build`, `TypedMemory<T>`) is an ordinary singleton -- allocated at first use, one of `.instances`, destroyed at exit -- where every other build makes it one static object. Readings: every build that is not inspectable is a production build, the ordinary one included, not only `--optimized`; `--optimized` with a REPL flag is inspectable; only tree shaking and static singletons count as hiding an internal, so every speed-only optimisation applies in both kinds of build; a holds-nothing singleton may be made again when something destroyed after it at exit asks for it, since it has no state and no `drop()` (`Memory`'s own `.instances` list is untracked through a `TypedMemory` destroyed before it); and the list behind `.instances` is the compiler's bookkeeping, allocated outside `--debug_memory`'s counts like the list of singletons to destroy (a `Memory` first made inside the allocation table had its list allocated outside it and then freed through it). Item 140: section 9's sentence that `--development` keeps conditions on codegen values as run-time values is replaced by what is built, folding in every build. `conformance/stage6/development_internals`, `docs/optimizations.md` (a Builds column), `docs/compiler.md`, `docs/metaprogramming.md`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; implements D135's pool) **`Parallel` runs on `ThreadPool()`, a singleton of one worker thread per core but one, started by the first `Parallel` and never grown.** Jobs are `Spite.Function<Int, Int, Nothing>` values with their two numbers and an 8-byte state (queued, running, done) owned by whoever waits; workers take them in order under one lock and two condition variables (`SRWLOCK`/`CONDITION_VARIABLE`, `pthread_mutex_t`/`pthread_cond_t`). Waiting for a job no worker has taken runs it on the waiting thread, so nested parallel work cannot deadlock the pool. The queue holds a `ParallelCall<T>` that owns the work and the result, never the `Parallel`, so join on drop is unchanged. `size()` and `worker_index()` (`-1` off the pool) are public; `ThreadPool.join` replaces `Parallel.join_thread` as the wait the scheduler wraps; `entry_address()`/`address()` moved from `Parallel` to `ThreadPool`, so no new compiler-supplied code (D147). Linux and macOS compile, untested. Section 15, "Concurrency"; `conformance/stage6/thread_pool_reuse`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; implements D134 for `Concurrent` and `Parallel`) **The handle stands in for its result wherever the result's type is expected, and `wait()` and `join()` are gone.** The compiler inserts a call to the class's private `_result()` wherever a `Concurrent<T>`/`Parallel<T>` is stored or passed as a `T` (typed `var`, assignment, argument, `return`), used as an operand or inside text, given a member it does not have (`greeting.length()`), or used as a condition; narrowing the handle of a nullable `T` (`if reading`, `crash reading`) narrows its value, read once into a hidden local. An untyped `var` keeps the handle; `finished` is the handle's; a `Nothing` handle joins only on drop. `==` on two handles compares values. SPITE.md's rule, "no `.wait()` to remember", wins over keeping `wait()` as an explicit form. Section 15, "Concurrency"; `conformance/stage6/implicit_joins`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; SlopEngine polled `WaitForSingleObject(parallel.thread, 0)`) **`finished` on `Concurrent` and `Parallel` answers whether the function has returned and never waits, on every system; every other attribute of both is private.** A `Parallel`'s is one atomic load of its job's state; a `Concurrent`'s is the flag its fiber sets, so a polling loop still has to wait somewhere for the fiber to run. `conformance/stage6/finished_polling`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; SlopEngine's per-runner command buffers and entity-id lock used `TlsAlloc` and an `SRWLOCK` directly) **`ThreadLocal<T>()`, `Lock()` and `ThreadSlot()` are library classes.** `Lock` has `while_locked(function)` (the unlock cannot be forgotten) beside `lock()`/`unlock()`; `ThreadSlot` is the raw per-thread `Long` (`TlsAlloc`, `pthread_key_create`); `ThreadLocal<T>` keeps each thread's value in a list it owns, found through its `ThreadSlot` under its `Lock`, so every value is released when the `ThreadLocal` is dropped. Chosen over a value per pool worker, which would not cover the program's own thread. Linux and macOS compile, untested. Section 15, "Concurrency"; `conformance/stage6/thread_locals`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; builds D135's templates and D35's race rule) **`list.parallel_each_<member>()` runs the member on every element across the pool and returns when all are done, and the compiler checks D35's rule.** The generator writes a piece function over a range of the buffer and an entry that hands it to `ThreadPool.run_pieces` (up to four pieces per thread, the first on the calling thread); `filter_` steps fuse into the piece loop and `map_` steps are an error. The member, the functions of its class it calls, and the filters may reach only the element's plain-value attributes (numbers, `Bool`, `String`, enums, `Symbol`s, and `T?` of them), library singletons and their own locals; anything else is an error naming the attribute. Not seen: an element listed twice, a list of a `type` (an error for now). Section 15, "Concurrency"; `conformance/stage6/parallel_each`, `diagnostics/parallel_reach`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; D134's IO half) **Reads in a row overlap.** Two or more untyped `var name = receiver.read()` (`File`) or `receiver.read_line()` (`Socket`) in a row, on a name or attribute path, none naming an earlier one and none reassigned later, become `Concurrent`s except the last, and all are joined before the next statement. Chosen over making `File.read()` itself answer a `Concurrent` (every read, the compiler's own included, would pay for a fiber, and a field or path narrowed through a handle is not supported) and over overlapping a read with the statements after it (the next statement may write the file). Expressed with `Concurrent` in the syntax tree, so it follows `Concurrent` when D176 replaces fibers. `conformance/stage6/overlapped_reads`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; SlopEngine's cache used `fopen`/`fread` from `ucrtbase` directly) **`File` reads and writes bytes**: `size(): Long?`, `read_bytes(position, count, address): Long?`, `write_bytes(address, count): Bool`, `append_bytes(address, count): Long?` (the position the bytes start at). Positions replace seeking, so a `File` stays a path with no open handle; each call opens and closes the file, which suits reading a whole file and walking it in memory. Windows seeks with `_fseeki64`. `conformance/stage6/binary_files`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; builds D174) **The check point is a call to `Scheduler.check_point()` at the end of each pass of each `while` in the program's own code, in `--repl_port` and `--hot_reload` builds only**; on the scheduler's thread it answers a pending command and runs a pending reload, elsewhere it does nothing. The compile error for a REPL build whose loop never waits, and `diagnostics/remote_loop_never_waits`, are removed; `docs/concurrency.md` replays a busy loop answered between passes. Production C is unchanged. |
| 2026-09-25 | (proposed by Claude, unconfirmed; SlopEngine's asset cache checks whether a source changed) **`File.modified(): Instant?`** answers the file's last write as a D127 `Instant`, `null` when it does not exist: `GetFileAttributesExA`'s `ftLastWriteTime` on Windows, `stat`'s modification time on Linux (offset 88) and macOS (offset 48, untested). `conformance/stage6/binary_files`. |
| 2026-09-25 | (proposed by Claude, unconfirmed; builds D183's lock, the fallback of D184) **A program singleton that can change after it is made takes its own reentrant lock around every function, in programs that use `Parallel` only.** Its functions become `<name>___unguarded` behind a locking wrapper (the owner is a `_Thread_local` marker's address, so calls to itself do not lock twice); a singleton that never changes, a library singleton and every program without `Parallel` are untouched. `parallel_each_`'s check now allows every singleton. Not built: D184's cheaper forms, the `return` check, telling calls from a `Parallel` apart, guarding in `--hot_reload` builds, and D179's check on `Parallel(function)`. `conformance/stage6/singleton_guard`, `docs/optimizations.md`. |
| 2026-09-25 | (implements D148; the readings below proposed by Claude, unconfirmed) **The caller's function is passed: `each(f)`, `map(f)`, `filter(f)`, `any(f)`, `all(f)`, `count(f)`, `find(f)`, `sort_by(f)` and `sum(f)` take a function of one argument, the element's exact type, bound to its owner.** `find(f)` answers the first element `f` is true for (`find_by_` with `true`); `count()` with no argument stays the size. A function written by name (`say_hello`, `greeter.greet`) instantiates the element template once per function and owner class with the owner as a hidden last parameter, so it allocates nothing and calls directly -- tree-shaken like every template, and free at run time; a function held in a variable is called through its `Spite.Function`. `map(f)`/`filter(f)` fuse in chains with the member templates (D105), the fused function taking each owner. D113's resolution through the caller, its ambiguity error and its `while` rule are removed; an element-template call whose member is missing while the caller has a function of that name says to pass it (`'each(say_hello)'`). Rewritten: 7 uses (`collect_body_facts` and two `map_class_member_name` calls in the compiler, `conformance/stage6/system_phases`, and `docs/`'s `name_phases`, `function_reflection` and `function_questions` -- 3 doc blocks), besides `conformance/stage6/caller_templates` and `diagnostics/caller_templates`, which became `passed_functions`. Section 8, `docs/collections.md`. |
| 2026-09-25 | (implements D162; the readings below proposed by Claude, unconfirmed) **An arithmetic operator whose right operand is wider than its left is a compile error, and so is a constant expression that overflows `Int`.** Wider is more bits (8: `Tiny`, `Byte`; 16: `Short`, `UnsignedShort`; 32: `Int`, `UnsignedInt`, `Float`; 64: `Long`, `UnsignedLong`, `Double`), or a `Float`/`Double` right side under a whole-number left; signedness alone is not wider. An integer literal (or a negated one) on the right that fits the left type does not count, so `small + 1` on a `Byte` stays legal. **A comparison does not count**: it is not arithmetic, and it keeps casting the right side toward the left exactly as before (so `age > -0.5` on an `Int` is still `age > 0`, open question 3); the C for comparisons is unchanged. The message names the rule and the fix -- `'count * total' is a multiplication in Int, since arithmetic takes the left side's type, and the right side is a Long, which would be cut to fit: write the Long first ('total * count'), or store the right side in an Int first if it fits one`; for `-`, `/` and `%` the fix is `store the left side in a Long first ('var wide: Long = count')`. A constant is a tree of `Int` literals under `+ - * / %`; the first operation whose value leaves `Int` is reported once, with its value: `'(65536 - 120) * 65536' is 4287102976, which does not fit in an Int, the type its arithmetic is done in, so it would wrap: write the number itself, 4287102976, which is a Long`. Both checks happen at compile time and emit nothing. Rewritten: 17 sites, all in `library/` (`daylight_change`, `duration` three times through a new `_nanoseconds_per_unit`, `local_date` three, `long` two, `unsigned_long` two, `number_text`, `tzif_reader` four) plus `examples/hello`; outputs and allocations are unchanged. `diagnostics/wider_right_operand`, `docs/values_and_types.md`. |
| 2026-09-25 | (implements D139; the message and scope proposed by Claude, unconfirmed) **An expression statement that is a constructor call -- plain or generic, `Report(text)` or `Box<Int>(3)` -- is an error**: `'Report(text)' makes a 'Report' and drops it: a constructed object must be kept and used, so a class whose construction is the whole point should be a function instead -- turn 'Report' into a function of the class that needs it, or keep the object in a variable that is read`. A singleton's constructor is left to D110's error, and `load(...)` is not a constructor. The check is syntactic plus a class lookup at compile time and emits nothing. No program in the repository did this, so no site was rewritten (the constructors-as-actions D139 names are SlopEngine's, which migrates itself). `diagnostics/dropped_construction`, section 3, `docs/classes_and_files.md`. |
| 2026-09-25 | (implements D157; the reading below proposed by Claude, unconfirmed) **D118's loaded-package exemption no longer covers an attribute whose default is a singleton construction** (`var world = World()`): unread, it is `the attribute 'world' is never read: remove it` in a loaded folder too. "Unread" is D118's usual reading -- a read from any class counts -- rather than only its own class's, since a read from outside can only hide the error, never cause one. The check is compile time only. No binding in the repository was unread, so no site was rewritten. `diagnostics/package_attributes` (a `var console = Console()` in the loaded `paint` folder). |
| 2026-09-25 | (implements D166; the message proposed by Claude, unconfirmed) **A function named `load` is an error, and `load(...)` always loads a package**: `'load' is reserved: it always loads a package, so a function cannot be named 'load'. Name it for what it loads, such as 'load_texture'`. Removed: the generator's check that let a class's own `load` win, and discovery's rule that read no folder from a file declaring `load`. `conformance/stage6/load_function`, which proved the removed behaviour, became `diagnostics/load_function`; no other program declared `load`. Compile time only. `docs/packages.md`. |
