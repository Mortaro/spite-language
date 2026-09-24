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

```spite title=numeric_overflow/numeric_overflow.spite entry
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

### The sharp edge

Because casting always goes right-to-left, comparing an `Int` against a `Float` literal casts the `Float` down
to `Int` *before* comparing -- not the mathematically obvious thing:

```spite title=casting_edge/casting_edge.spite entry
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
non-integer literal, if you need the mathematical answer. (This is manual.md's own open question 3 --
unresolved, and worth knowing before you write a comparison that mixes an `Int` and a fractional literal.)

`String` converts both ways: assigning a `String` to a numeric variable parses it (`0`/`0.0` on failure, never
a crash), and every numeric type gets a `to_<name>()` method on `String` (`to_tiny()`, `to_int()`, `to_long()`,
`to_unsigned_long()`, `to_float()`, `to_double()`, ...) in `library/string.spite`; assigning text to a number
calls the same method.

### Numbers are classes

`Int`, `Long`, `Float`, `Double`, `Bool` and the rest are classes in `library/` (`library/int.spite`,
`library/double.spite`, ...), the way `String` is (D83). Their functions are called on a value like any class's,
and inside one of them `this` is the number itself. Turning a number into text is `text()`, written in Spite in
`library/long.spite` and `library/double.spite`, and it is what `"{count}"` calls. A program reopens a number
class the way it reopens any class (section 11 of the manual), with a file named after it:

```spite title=number_methods/int.spite
func doubled(): Int {
    return this * 2
}
```
```spite title=number_methods/number_methods.spite entry
var console = Console()

func NumberMethods() {
    var count = 21
    var doubled = count.doubled()
    var count_text = count.text()
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

**Casting is a function of the class being cast to** (D100). Every number class has
`func from_type(type: Symbol, value: type.class)`, a Symbol codegen function whose symbol ranges over the
types (not the attributes) of the program: the right-to-left cast of an `Int` into a `Float` is
`Float.from_int(value)`, of a `Long` into a `Byte` `Byte.from_long(value)`, and so on. Its body is the one
the compiler supplies -- a C cast, written inline, so a cast costs exactly what it did -- and
`--final_classes` shows the declaration in each number class. Casting text into a number goes through
`String`'s `to_<name>()` in the same way.

`this` works in every class, not only numbers: it is the instance the function answers on, for when the
function needs to hand itself to something -- `registry.append(this)`. Reading your own members through it is
an error, because a class already reads them by name: write `name`, not `this.name`.

## `String`

Immutable, length-prefixed (not a bare `char*`), reference counted. A value is placed inside written text rather
than joined to it with `+`: `"hello {name}"`, where `{ }` holds one value of any type (every numeric type,
`Bool`, and enum format themselves) and `\{` is a brace meant literally. Two values still join with `+`, and
joining written text with `+` is an error naming the form above. `==`/`!=`/`<`/`>` compare by content.

Building text a piece at a time costs what the pieces cost, not the text so far: `text = "{text}{piece}"` (or
`text = text + piece`) on a local variable appends in place when nothing else holds that text, and copies it
once when something does, so the other holder still sees the text it had.

```spite title=text_building/text_building.spite entry
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

```spite title=string_basics/string_basics.spite entry
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

```spite title=enum_basics/player.spite
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
```spite title=enum_basics/enum_basics.spite entry
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

```spite title=enum_from_text/enum_from_text.spite entry
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

## Unions

A tagged union. A `switch` must cover every member, and narrows the value inside each case; when every member
shares a function/attribute of the same shape, it can be called directly on the union value without a
`switch` at all (duck-typed, the same way a `type` is -- see below).

```spite title=union_basics/player.spite
var health = 10

func Player(starting_health: Int) {
    health = starting_health
}

func is_alive(): Bool {
    return health > 0
}
```
```spite title=union_basics/monster.spite
var health = 6

func Monster(starting_health: Int) {
    health = starting_health
}

func is_alive(): Bool {
    return health > 0
}
```
```spite title=union_basics/union_basics.spite entry
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

```spite title=duck_typing/player.spite
var weapon = "sword"
var power = 5

func Player(starting_power: Int) {
    power = starting_power
}
```
```spite title=duck_typing/duck_typing.spite entry
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

A `type` may require functions as well as attributes. A required function names the types it takes and returns,
never the names of its parameters -- `render(Int): String` -- because the name a class gives its own parameter
does not matter to the shape. Any class with a function of that signature fits, which is how a list holds "any
class that can render" without a union naming every class in advance:

```spite title=shape_functions_doc/badge.spite
var label = ""

func Badge(new_label: String) {
    label = new_label
}

func render(width: Int): String {
    return "[{label}] ({width})"
}
```
```spite title=shape_functions_doc/banner.spite
func render(columns: Int): String {
    return "== banner {columns} =="
}
```
```spite title=shape_functions_doc/shape_functions_doc.spite entry
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
