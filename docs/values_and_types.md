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
a crash), and every numeric type gets a `to_<name>()` method (`to_int()`, `to_long()`, `to_float()`, ...).

## `String`

Immutable, length-prefixed (not a bare `char*`), single-owner. `+` concatenates and casts its right side to
text (every numeric type, `Bool`, and enum format themselves); `==`/`!=`/`<`/`>` compare by content.

```spite title=string_basics/string_basics.spite entry
var console = Console()

func StringBasics() {
    var greeting = "Hello" + ", " + "Spite"
    console.print(greeting)
    console.print("length", greeting.length())
    console.print("upper", greeting.upper())
    console.print("contains", greeting.contains("Spite"))

    var parts = greeting.split(", ")
    console.print("parts count", parts.count())
    console.print("joined", parts.join(" - "))

    var age: Int = "42"
    console.print("parsed", age)
    console.print("bad parse", "not a number".to_int())
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

## `T?` and `assert` narrowing

`null` exists only as the empty state of a `T?`. Unwrap it with `if value { }` (narrows `value` itself
to `T` inside the block, with an `else` for when it is null), or with `assert value`, which narrows `value`
itself from `T?` to `T` for the rest of the current block (and any block nested inside it) -- no
rebinding to a new name needed either way:

```spite title=nullable_narrowing/monster.spite
var health = 10

func Monster(starting_health: Int) {
    health = starting_health
}
```
```spite title=nullable_narrowing/nullable_narrowing.spite entry
var console = Console()

func find_monster(missing: Bool): Monster? {
    if missing {
        return null
    }
    return Monster(30)
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

func NullableNarrowing() {
    report_health()
}
```
```output
health 30
done
```

`assert` is a production feature, not a debug-only one: when the condition is falsey, the function returns its
return type's default value immediately -- there is no panic, and no code after the `assert` runs.

**Lint:** if a function's *last* statement is `if value { ... }` (no `else`) used only to check existence, that
is a compile error naming the `assert` rewrite -- write it with `assert` instead:

```spite title=terminal_if_do_error/terminal_if_do_error.spite entry error
var console = Console()

func try_get_name(): String? {
    return null
}

func announce() {
    var maybe_name = try_get_name()
    if maybe_name {
        console.print(maybe_name)
    }
}

func TerminalIfDoError() {
    announce()
}
```
```diagnostic
write 'assert maybe_name' and let the rest of the function run unindented
```

```spite title=terminal_if_do_fixed/terminal_if_do_fixed.spite entry
var console = Console()

func try_get_name(): String? {
    return "Aria"
}

func announce() {
    var name = try_get_name()
    assert name
    console.print(name)
}

func TerminalIfDoFixed() {
    announce()
}
```
```output
Aria
```

A narrowing `if` stays perfectly legal everywhere else -- only "the whole rest of the function, as its last
statement, with no `else`" triggers the lint.

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
    console.print("is knight", hero.is_knight())
}
```
```output
job mage
is knight false
```

`'mage'` alone resolves without writing `Player.Job` -- Spite looks in the referencing class's own namespace
first, then its folder, then each parent folder, then the whole program. Two enums may share a value name;
with no expected type in hand (a bare comparison with nothing typed on either side, for instance) that is only
an error if more than one enum in the program actually has that value.

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
    console.print("alive", enemy.is_alive())
}
```
```output
a monster 6
alive true
```

`T?` is, conceptually, exactly this: the union of `T` and `null`.

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

    var literal_loadout: Loadout = { weapon: "bow", power: 4 }
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
