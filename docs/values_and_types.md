# Values and types

## Numeric types and the casting rule

There is no cast syntax. **The right side always casts toward the left side** -- in a binary operation,
assignment, a function argument (toward the parameter type), and `return` (toward the return type).

| Type | C type | | Type | C type |
|---|---|---|---|---|
| `Tiny` | `int8_t` | | `Byte` | `uint8_t` |
| `Short` | `int16_t` | | `UnsignedShort` | `uint16_t` |
| `Int` (default integer) | `int32_t` | | `UnsignedInt` | `uint32_t` |
| `Long` | `int64_t` | | `UnsignedLong` | `uint64_t` |
| `Float` (default decimal) | `float` (32-bit) | | `Double` | `double` (64-bit) |

An integer literal is `Int`; one too big for `Int` becomes `Long` automatically. Converting between numeric
types **wraps** on overflow (plain C narrowing, not a crash or a saturate):

```gdscript title=numeric_overflow/numeric_overflow.spite entry
var console = Console()

func NumericOverflow() {
    var tiny_value: Tiny = 127
    tiny_value = tiny_value + 1
    console.print("wraps to", tiny_value)
}
```
```output
wraps to -128
```

### Wider arithmetic goes wider operand first

Arithmetic is done in the left side's type, and the right side is cast to it: `count * total` with an `Int`
`count` and a `Long` `total` is an `Int` multiply. So a right side wider than the left -- more bits, or a
`Float`/`Double` under a whole number -- is a compile error naming the fix: write the wider one first
(`total * count`), or, for `-`, `/` and `%`, store the left side in the wider type first. A literal on the right
that fits the left type is fine (`small + 1` stays a `Byte` addition). A constant that overflows the `Int` its
arithmetic is done in is an error too, instead of wrapping:

```gdscript title=wider_first/wider_first.spite entry error
var console = Console()

func WiderFirst() {
    var width = 65536
    var area: Long = 4294967296
    var scaled = width * area
    console.print(scaled)
}
```
```diagnostic
'width * area' is a multiplication in Int, since arithmetic takes the left side's type, and the right side is a Long, which would be cut to fit: write the Long first ('area * width')
```

```gdscript title=constant_overflow/constant_overflow.spite entry error
var console = Console()

func ConstantOverflow() {
    var pixels: Long = (65536 - 120) * 65536
    console.print(pixels)
}
```
```diagnostic
'(65536 - 120) * 65536' is 4287102976, which does not fit in an Int, the type its arithmetic is done in, so it would wrap: write the number itself, 4287102976, which is a Long
```

### The sharp edge

A comparison is not arithmetic, so it is not checked, and because casting always goes right-to-left, comparing an `Int` against a `Float` literal casts the `Float` down
to `Int` *before* comparing -- not the mathematically obvious thing:

```gdscript title=casting_edge/casting_edge.spite entry
var console = Console()

func CastingEdge() {
    var progress: Int = 0
    console.print("mathematically true, but", progress > -0.5)
    var price: Float = 3.0
    var quantity: Int = 2
    console.print("total", price * quantity)
}
```
```output
mathematically true, but false
total 6
```

`0 > -0.5` is mathematically true, but `-0.5` casts toward the left side's type first: it truncates to `0`, so
the comparison becomes `0 > 0`, which is false.

Give the `Int` side a `Float`/`Double` type instead of comparing an `Int` variable directly against a
non-integer literal, if you need the mathematical answer. (D162 settled this for arithmetic; for comparisons it
is still [open question 3](open_questions.md#open-questions), and worth knowing before you write a comparison that mixes an `Int` and a
fractional literal.)

`String` converts both ways: assigning a `String` to a numeric variable parses it (`0`/`0.0` on failure, never
a crash), and every numeric type gets a `to_<name>()` method on `String` (`to_tiny()`, `to_int()`, `to_long()`,
`to_unsigned_long()`, `to_float()`, `to_double()`, ...) in `library/string.spite`; assigning text to a number
calls the same method.

### Numbers are classes

`Int`, `Long`, `Float`, `Double`, `Bool` and the rest are classes in `library/` (`library/int.spite`,
`library/double.spite`, ...), the way `String` is. Their functions are called on a value like any class's,
and inside one of them `this` is the number itself. Turning a number into text is `to_string()` (D107), written in Spite in
`library/long.spite` and `library/double.spite`, and it is what `"{count}"` calls. A program reopens a number
class the way it reopens any class ([packages.md](packages.md#monkey-patching-mods)), with a file named after it:

```gdscript title=number_methods/int.spite
func doubled(): Int {
    return this * 2
}
```
```gdscript title=number_methods/number_methods.spite entry
var console = Console()

func NumberMethods() {
    var count = 21
    var doubled = count.doubled()
    var count_text = count.to_string()
    console.print(doubled, count_text)
    var third: Double = 1.0 / 3.0
    var bits = third.bits()
    console.print("{third}", bits)
}
```
```output
42 21
0.3333333333333333 4599676419421066581
```

A number is still a value in the emitted C (`int32_t`, `double`, ...): the class gives it functions, not a
header, and `this` inside `Int` is that `int32_t`.

**Casting is a function of the class being cast to.** Every number class has
`func from_type(type: Symbol, value: type.class)`, a Symbol codegen function whose symbol ranges over the
types (not the attributes) of the program: the right-to-left cast of an `Int` into a `Float` is
`Float.from_int(value)`, of a `Long` into a `Byte` `Byte.from_long(value)`, and so on. Its body is the one
the compiler supplies -- a C cast, written inline, so a cast costs exactly what it did -- and
`--final-classes` shows the declaration in each number class. Casting text into a number goes through
`String`'s `to_<name>()` in the same way.

`this` works in every class, not only numbers: it is the instance the function answers on, for when the
function needs to hand itself to something -- `registry.append(this)`. Reading your own members through it is
an error, because a class already reads them by name: write `name`, not `this.name`.

### Bitwise functions

There are no bitwise operator symbols. Every whole-number class (`Tiny` to `UnsignedLong`, not `Float`, `Double`
or `Bool`) answers them as functions, each compiled to the one C operation and inlined in an optimised build
(D117; the names are proposed by Claude, unconfirmed):

| Function | Answers |
|---|---|
| `shifted_left(count)` | the bits moved `count` places up, zeros coming in |
| `shifted_right(count)` | moved down: a signed type copies its sign bit in (arithmetic), an unsigned type zeros (logical) |
| `bits_and(other)`, `bits_or(other)`, `bits_exclusive_or(other)` | the bits set in both, in either, in exactly one |
| `bits_inverted()` | every bit flipped |
| `set_bit_count()`, `leading_zero_count()`, `trailing_zero_count()` | how many bits are set, and how many zeros stand above the highest set bit and below the lowest, as an `Int` (the width, for 0) |

Each answers the receiver's type, and `other` is cast to it the way any argument is cast to its parameter: a
`Byte` and a `Long` mask give a `Byte`. A shift count of the width or more moves every bit out (0, or -1 for a
negative value shifted right), and a negative count halts the program with the function and the count named,
so C's undefined shifts never happen.

```gdscript title=bitwise_basics/bitwise_basics.spite entry
var console = Console()

func BitwiseBasics() {
    var flags = 12
    var mask = 10
    console.print("{flags.bits_and(mask)} {flags.bits_or(mask)} {flags.bits_exclusive_or(mask)}")
    var packed = flags.shifted_left(4).bits_or(3)
    console.print("{packed} {packed.shifted_right(4)} {packed.bits_and(15)}")
    var negative = -16
    var small: Byte = 240
    console.print("{negative.shifted_right(2)} {small.shifted_right(2)} {small.bits_inverted()}")
    console.print("{flags.shifted_left(32)} {negative.shifted_right(40)} {small.set_bit_count()}")
}
```
```output
8 14 6
195 12 3
-4 60 15
0 -1 4
```

## `String`

Immutable, length-prefixed (not a bare `char*`), reference counted. A value is placed inside written text rather
than joined to it with `+`: `"hello {name}"`, where `{ }` holds one value of any type (every numeric type,
`Bool`, and enum format themselves) and `\{` is a brace meant literally. Two values still join with `+`, and
joining written text with `+` is an error naming the form above. `==`/`!=`/`<`/`>` compare by content.

Building text a piece at a time costs what the pieces cost, not the text so far: `text = "{text}{piece}"` (or
`text = text + piece`) on a local variable appends in place when nothing else holds that text, and copies it
once when something does, so the other holder still sees the text it had.

```gdscript title=text_building/text_building.spite entry
var console = Console()

func TextBuilding() {
    var line = "count:"
    var index = 0
    while index < 3 {
        line = "{line} {index}"
        index = index + 1
    }
    var before = line
    line = "{line} done"
    console.print(before)
    console.print(line)
}
```
```output
count: 0 1 2
count: 0 1 2 done
```

```gdscript title=string_basics/string_basics.spite entry
var console = Console()

func StringBasics() {
    var subject = "Spite"
    var greeting = "Hello, {subject}"
    console.print(greeting)
    var greeting_length = greeting.length()
    console.print("length", greeting_length)
    var upper_case_greeting = greeting.upper_case()
    console.print("upper", upper_case_greeting)
    var mentions_spite = greeting.contains("Spite")
    console.print("contains", mentions_spite)
    var parts = greeting.split(", ")
    var parts_count = parts.count()
    console.print("parts count", parts_count)
    var joined_parts = parts.join(" - ")
    console.print("joined", joined_parts)
    var age: Int = "42"
    console.print("parsed", age)
    var not_a_number = "not a number".to_int()
    console.print("bad parse", not_a_number)
}
```
```output
Hello, Spite
length 12
upper HELLO, SPITE
contains true
parts count 2
joined Hello - Spite
parsed 42
bad parse 0
```

A value that "cannot succeed" -- a parse failure, an out-of-range index -- always produces the default instead
of crashing. See [standard_library.md](standard_library.md) for the full method table.

## `T?`: values that may be null

`Monster?` is a `Monster` or `null`, and `null` exists for nothing else. A `T?` is narrowed before it is used --
`if value { } else { }`, `assert value`, `crash value`, `while value`, or a `switch` with a `Null:` case -- and
reading with `[]` answers one. All of it is in [failure.md](failure.md), with the three outcomes a failure can
have.

## Enums

```gdscript title=enum_basics/player.spite
enum Job {
    'knight'
    'mage'
    'archer'
}

var job: Job = 'knight'

func Player(starting_job: Job) {
    job = starting_job
}

func is_knight(): Bool {
    return job == 'knight'
}
```
```gdscript title=enum_basics/enum_basics.spite entry
var console = Console()

func EnumBasics() {
    var hero = Player('mage')
    console.print("job", hero.job)
    var hero_is_knight = hero.is_knight()
    console.print("is knight", hero_is_knight)
}
```
```output
job mage
is knight false
```

An enum declaration takes no `=` and lists one value per line, with no commas. A value is written in single
quotes -- single quotes mean an enum value and nothing else -- and `'mage'` alone resolves without writing
`Player.Job`, because it is resolved from where it is used: the parameter, the annotation, the assignment or the
comparison says which enum it must belong to. Two enums may share a value name; only when nothing says which
enum is meant, and more than one has that value, is it an error listing them.

An enum is a closed list of **symbols**. A symbol literal is legal only where something says what it may be: a
value the enum does not list is a compile error naming the ones it does, and `var choice = 'orange'`, with
nothing to check it against, is an error too. A `Symbol` on its own is text from the program's table of names --
reflection answers class and function names as symbols -- and reads as text anywhere text is expected.

Text becomes an enum value by assignment, the way it becomes a number: the value spelled that way, or the enum's
first value when there is none, as `"x"` becomes `0` for an `Int`. Compare the text back to tell the two apart:

```gdscript title=enum_from_text/enum_from_text.spite entry
enum Course {
    'starter'
    'soup'
    'dessert'
}

var console = Console()

func EnumFromText() {
    var names = ["dessert", "brunch"]
    var index = 0
    while index < names.count() {
        var name = names[index]
        var course: Course = name
        var known = "{course}" == name
        console.print(name, course, known)
        index = index + 1
    }
}
```
```output
dessert dessert true
brunch starter false
```

### Walking an enum's values

`course: Symbol<Course>` makes a template over the enum's values, the way `Symbol<Label>` makes one over a
class's attributes ([metaprogramming.md](metaprogramming.md#another-classs-attributes-and-all-of-them-at-once)).
Inside, `course.name` is the value's name as text and `course.value` is the value itself, typed `Course`. The
plural, `list_courses()`, calls the template once per value, in the order the enum lists them; `list_soup()`
calls it for one.

```gdscript title=enum_walk/enum_walk.spite entry
enum Course {
    'starter'
    'soup'
    'dessert'
}

var served: Course = 'soup'
var console = Console()

func EnumWalk() {
    list_courses()
}

func list_course(course: Symbol<Course>) {
    var is_served = course.value == served
    console.print(course.name, is_served)
}
```
```output
starter false
soup true
dessert false
```

It is all decided while compiling: `list_courses()` becomes three calls, and `list_soup()` compares with
`'soup'` directly. No list of an enum's values exists at run time, so a program that never walks one pays
nothing for it. An enum is also open to the program that loads it: reopening its class declares the enum again
with more values ([packages.md](packages.md#reopening-an-enum-adds-values)), and a walk then includes them.

## Unions

A tagged union. A `switch` must cover every member, and narrows the value inside each case; when every member
shares a function/attribute of the same shape, it can be called directly on the union value without a
`switch` at all (duck-typed, the same way a `type` is -- see below).

```gdscript title=union_basics/player.spite
var health = 10

func Player(starting_health: Int) {
    health = starting_health
}

func is_alive(): Bool {
    return health > 0
}
```
```gdscript title=union_basics/monster.spite
var health = 6

func Monster(starting_health: Int) {
    health = starting_health
}

func is_alive(): Bool {
    return health > 0
}
```
```gdscript title=union_basics/union_basics.spite entry
union Enemy {
    Player
    Monster
}

var console = Console()

func UnionBasics() {
    var enemy: Enemy = Monster(6)
    switch enemy {
        Player: console.print("a player", enemy.health)
        Monster: console.print("a monster", enemy.health)
    }
    var enemy_is_alive = enemy.is_alive()
    console.print("alive", enemy_is_alive)
}
```
```output
a monster 6
alive true
```

Like `enum` and `type`, a `union` takes no `=` and lists one member per line. A `switch` may answer several
members at once with `_:`, and `enemy == Monster` asks which member a value is
([control_flow.md](control_flow.md#switch-over-a-union)). `Monster?` is exactly this kind of union: `Monster` and
`Null`, the class whose only value is `null`.

## Inline types and duck typing

A `type` is a class matched by shape: any value with the same attribute names and types is accepted,
including a plain object literal:

```gdscript title=duck_typing/player.spite
var weapon = "sword"
var power = 5

func Player(starting_power: Int) {
    power = starting_power
}
```
```gdscript title=duck_typing/duck_typing.spite entry
type Loadout {
    weapon: String
    power: Int
}

var console = Console()

func DuckTyping() {
    var hero = Player(8)
    console.print("class of hero", hero.class)
    var hero_loadout: Loadout = hero
    console.print("from class", hero_loadout.weapon, hero_loadout.power)
    var literal_loadout: Loadout = {weapon: "bow", power: 4}
    console.print("from literal", literal_loadout.weapon, literal_loadout.power)
}
```
```output
class of hero Player
from class sword 8
from literal bow 4
```

`.class` read through the `type`-shaped variable answers the same thing: the class the value really is, read
from the object's own tag rather than from the declared type. An object literal has no class of its own, so it
answers `Object`.

This is what makes fast, JSON-shaped code possible: accept a `type`, and both a real class instance and a
plain `{ key: value }` literal work.

A `type` of attributes only has a default like a class does: the object literal with each attribute at its own
default, whose `.class` answers `Object`. So `var target: $target_type = null` in a generic class bound to such a
`type` holds a real object, and what is written through it stays written.

A `type` may require functions as well as attributes. A required function names the types it takes and returns,
never the names of its parameters -- `render(Int): String` -- because the name a class gives its own parameter
does not matter to the shape. Any class with a function of that signature fits, which is how a list holds "any
class that can render" without a union naming every class in advance:

```gdscript title=shape_functions_doc/badge.spite
var label = ""

func Badge(new_label: String) {
    label = new_label
}

func render(width: Int): String {
    return "[{label}] ({width})"
}
```
```gdscript title=shape_functions_doc/banner.spite
func render(columns: Int): String {
    return "== banner {columns} =="
}
```
```gdscript title=shape_functions_doc/shape_functions_doc.spite entry
type Renderable {
    render(Int): String
}

var console = Console()

func ShapeFunctionsDoc() {
    show(Badge("new"))
    show(Banner())
}

func show(item: Renderable) {
    var rendered = item.render(12)
    console.print(rendered)
}
```
```output
[new] (12)
== banner 12 ==
```

A `type` can also be the element of a variadic parameter, `...items: List<Renderable>`, so a call takes any number
of values of any classes that fit ([functions_and_operators.md](functions_and_operators.md#variadic-arguments)).

Read through a `type`, a required function named without calling it is a function value bound to the value, as
it is for a class ([functions_and_operators.md](functions_and_operators.md#functions-are-values)), so
`Parallel(stage.run_once)` works on a `List<Stage>`'s element typed by shape. A `List` of a `type` answers the
member templates too, over the attributes and the argument-free functions the type names -- `map_name()`,
`filter_active()`, `count_active()`:

```gdscript title=shape_values_doc/job.spite
var name = ""
var done = false

func Job(new_name: String) {
    name = new_name
}

func finish() {
    done = true
}
```
```gdscript title=shape_values_doc/shape_values_doc.spite entry
type Work {
    name: String
    done: Bool
    finish()
}

var console = Console()

func ShapeValuesDoc() {
    var jobs = List<Work>()
    jobs.append(Job("wash"))
    jobs.append(Job("dry"))
    finish_first(jobs)
    var names = jobs.map_name()
    var joined = names.join(", ")
    var finished = jobs.count_done()
    console.print(joined, finished)
}

func finish_first(jobs: List<Work>) {
    assert jobs[0]
    var finishing = jobs[0].finish
    finishing()
}
```
```output
wash, dry 1
```

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Variables and values  **[implemented]**

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
- `null` exists only as the empty state of `T?`. See [Open questions](open_questions.md#open-questions) for `= null` on other types.
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

  The unused rule ([Unused is an error](style.md#unused-is-an-error--implemented)) still applies to the binding being shadowed: shadowing a name that was never read
  is an error, which is what catches an accidental reuse rather than a deliberate one.

#### Casting

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
`Int`/`UnsignedInt`/`Float` 32, `Long`/`UnsignedLong`/`Double`/`Memory.Address` 64), or a `Float`/`Double` right side under a whole
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

#### Numeric types  **[implemented, PROVISIONAL]**

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

#### Numbers are classes, and `this`  **[implemented]**

D83 (decided by Mortaro, 2026-09-24): every type in the table above, and `Bool`, is a class in `library/`
(`library/int.spite`, `library/double.spite`, ...), the way `String` is. A number is still a plain C value in
the emitted code -- the class gives it functions, not a header -- and a function of `Int` receives its `int32_t`
as the receiver. Inside it, **`this`** is that value: `func doubled(): Int { return this * 2 }`, and
`count.doubled()` calls it. A number class is reopened like any other ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)), by a file named after it.

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
  the compiler's (a C cast, emitted inline, so a cast costs what it did), and `--final-classes` prints the
  declaration in every number class. `type` may name a parameter and begin a type path for this, although it is
  a keyword elsewhere (proposed by Claude, unconfirmed).
- **Bitwise operations are functions of the whole-number classes** (D117, decided by Mortaro; the names and the
  rules below proposed by Claude, unconfirmed). `Tiny`, `Short`, `Int`, `Long`, `Byte`, `UnsignedShort`,
  `UnsignedInt` and `UnsignedLong` each answer `shifted_left(count: Int)`, `shifted_right(count: Int)`,
  `bits_and(other)`, `bits_or(other)`, `bits_exclusive_or(other)` and `bits_inverted()`, all returning the
  receiver's type, and `set_bit_count()`, `leading_zero_count()` and `trailing_zero_count()`, returning an `Int`
  (the width for 0). There are no operator symbols for them. They are bodiless declarations the compiler
  supplies (D82), so `--final-classes` prints them in each class, and each body is the single C operation,
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

### Types

#### Enums  **[implemented]**

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
  ([Symbol codegen](metaprogramming.md#symbol-codegen--implemented)) -- a symbol literal only makes that call writable in source,
  which it is not today.

**An enum can be reopened, and walked** (D180, decided by Mortaro, 2026-09-25: "phases should be a enum ... which
can be reopened by the user ... our language needs good enum reflection"). **[implemented; the spellings and the
readings below proposed by Claude, unconfirmed]**

- **Reopening adds values.** A file that reopens a class ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)) and declares one of its enums again adds
  the values it lists to that enum instead of replacing it. They come after the values already merged, in merge
  order -- the program's own folder, then each loaded folder in load order -- and a value the enum already has
  stays where it was, so a reopening may restate the whole enum (as `--final-classes` output does) without
  changing it. Nothing removes a value. `conformance/stage6/enum_reopening` reopens one from a loaded folder and
  from the program's own folder.
- **`course: Symbol<Course>` walks the values** of an enum, the way `Symbol<Label>` walks a class's attributes
  ([Symbol codegen](metaprogramming.md#symbol-codegen--implemented)): `course.name` is the value's name as text, `course.value` the value itself typed as `Course`, and
  the plural (`list_courses()` for `list_course`) calls the template once per value in the enum's order;
  `list_soup()` calls it for one, and a name the enum does not have is an error listing the ones it does.
- **All of it is compile time and tree-shaken (D177).** A walk expands into one ordinary call per value, and
  `course.value` into the constant, as `--final-classes` shows; no table of an enum's values, names or order
  exists at run time, and a program that never walks an enum carries nothing for it.
- A name pattern's hole is constrained by an enum the same way ([A class's functions, a folder's classes and a name's pattern](metaprogramming.md#a-classs-functions-a-folders-classes-and-a-names-pattern--implemented-the-spellings-proposed-by-claude-unconfirmed)). D180 says environments will be an
  enum too; that is not built.

#### Unions  **[implemented]**

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

Memory safety is done with unions instead of borrow checking noise: `T?` is the union of `T` and `Null`, and it is narrowed before use with `if ... else`, `assert`, `crash` or `switch` ([Null safety and `assert` narrowing](failure.md#null-safety-and-assert-narrowing--implemented)).
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

[Memory](memory.md#memory--implemented)'s optimisation survives as a codegen detail rather than a language rule: a union of a reference and
`Null` is still just the pointer, and only a scalar needs a wrapper.

**Considered and rejected on the way** (2026-09-20): `Nullable<T>` (ugly, and a generic wrapper is what keeps it
a special case); `Monster or Null` (`or` short-circuits everywhere else, so a return type reads as an
expression yielding the truthy left side); `maybe Monster` (a keyword bought for a shortcut); `Maybe<Monster>`
as a standard library generic union (generics over unions hide the members, and collide with `$` codegen
replacement when a parent already declares the same name); and `Monster.Maybe` as a class-level member.


#### Inline types and duck typing  **[implemented]**

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
responds to `render()`" -- a list of components, [Markup](targets.md#markup--planned) -- without a union naming every class in advance,
and it makes the respond-to check [Decided by Mortaro, being implemented](open_questions.md#decided-by-mortaro-being-implemented--planned) item 9 promises expressible as an ordinary type rather than as
reflection.

**A `type` can be the element of a variadic parameter** (D90): `...children: List<Renderable>` takes any number
of values of any classes that fit, written one by one, "a generic can be done over both a type or a class, but in
the end monomorphised into each class". Each class that is passed is admitted to the shape, and each call on an
element is compiled once per admitted class ([Variadic arguments](functions_and_operators.md#variadic-arguments--implemented), [Variadic arguments](functions_and_operators.md#variadic-arguments--implemented)).

**A shape's members behave as a class's do** (proposed by Claude, unconfirmed, 2026-09-24): a required function
read without calling it is a function value bound to the value (D17), dispatched on the value's class when it is
called, so `Parallel(stages[index].run_once)` infers its codegen value from it; and a `List` of a `type` answers
the member templates over the attributes and the argument-free functions the type names
(`conformance/stage6/shape_members`).
