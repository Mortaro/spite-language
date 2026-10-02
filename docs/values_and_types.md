# Values and types

## Numeric types and the casting rule

There is no cast syntax. **The right side always casts toward the left side**: in a binary operation,
assignment, a function argument (toward the parameter type), and `return` (toward the return type).

| Type | C type | | Type | C type |
|---|---|---|---|---|
| `Tiny` | `int8_t` | | `Byte` | `uint8_t` |
| `Short` | `int16_t` | | `UnsignedShort` | `uint16_t` |
| `Integer` (default integer) | `int32_t` | | `UnsignedInteger` | `uint32_t` |
| `Long` | `int64_t` | | `UnsignedLong` | `uint64_t` |
| `Float` (default decimal) | `float` (32-bit) | | `Double` | `double` (64-bit) |

An integer literal is `Integer`; one too big for `Integer` becomes `Long` automatically. **A number never
silently wraps.** Arithmetic whose answer does not fit its type halts, unsigned exactly as signed, and so does a
wider value put into a narrower name ([below](#arithmetic-that-does-not-fit-halts)). Where wrapping is the point,
as in a hash or a noise function, the code says so with a function named for it: `wrapping_sum`,
`wrapping_subtract` and `wrapping_multiply` on every whole number keep the low bits of the answer:

```gdscript title=numeric_overflow/numeric_overflow.spite entry
var console = Console()

func NumericOverflow() {
    var wide = 200
    var cut: Byte = wide
    console.print("fits", cut)
    var hash: UnsignedInteger = 4000000000
    hash = hash.wrapping_multiply(3)
    console.print("wraps to", hash)
}
```
```output
fits 200
wraps to 3410065408
```

### Arithmetic that does not fit halts

`+`, `-` and `*` on a whole number (`Tiny`, `Short`, `Integer`, `Long`, `Byte`, `UnsignedShort`,
`UnsignedInteger`, `UnsignedLong`) whose answer does not fit its type is a developer's mistake, like dividing by
zero. Every build checks each one, the one you ship included, and halts at the first that does not fit, naming
the operation, the type, the operands and the line:

```gdscript
func scaled(amount: Integer, factor: Integer): Integer {
    return amount * factor          # scaled(2000000000, 2)
}
```
```
spite: 'amount * factor' does not fit in an Integer (2000000000 * 2), at game/game.spite:17 in Game.scaled
```

The same holds for `-` in front of a value (`-lowest` with the smallest `Integer`, or any unsigned value but `0`),
for the smallest signed value divided by `-1`, and for a value put into a narrower name, by assignment, as an
argument or by `return`: `var small: Tiny = count` halts when `count` is 200 (`spite: 200, an Integer, does not fit
in a Tiny, at ...`), and so does a decimal outside the whole number it is put into. A number written into a name
it does not fit (`var small: Byte = 300`) is a compile error. The compiler leaves a check out wherever it can
prove the answer fits ([proofs.md](proofs.md#arithmetic-that-does-not-fit-halts)), so what remains costs little.
The fix is a wider type written first (`Long` for a total of `Integer`s), or, when wrapping is what you meant, the
function that says so:

```gdscript title=wrapping_by_name/wrapping_by_name.spite entry
var console = Console()

func WrappingByName() {
    var hash: UnsignedLong = 1469598103934665603
    var text = "spite"
    var index = 0
    while index < text.length() {
        var code = text.code_at(index)
        var mixed = hash.bits_exclusive_or(code)
        hash = mixed.wrapping_multiply(1099511628211)
        index = index + 1
    }
    console.print(hash)
}
```
```output
16868922117198771000
```

### Wider arithmetic goes wider operand first

Arithmetic is done in the left side's type, and the right side is cast to it: `count * total` with an `Integer`
`count` and a `Long` `total` is an `Integer` multiply. So a right side wider than the left (more bits, or a
`Float`/`Double` under a whole number) is a compile error naming the fix: write the wider one first
(`total * count`), or, for `-`, `/` and `%`, store the left side in the wider type first. A literal on the right
that fits the left type is fine (`small + 1` stays a `Byte` addition). A constant that overflows the `Integer` its
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
'width * area' is a multiplication in Integer, since arithmetic takes the left side's type, and the right side is a Long, which would be cut to fit: write the Long first ('area * width')
```

```gdscript title=constant_overflow/constant_overflow.spite entry error
var console = Console()

func ConstantOverflow() {
    var pixels: Long = (65536 - 120) * 65536
    console.print(pixels)
}
```
```diagnostic
'(65536 - 120) * 65536' is 4287102976, which does not fit in an Integer, the type its arithmetic is done in, so it would wrap: write the number itself, 4287102976, which is a Long
```

### Comparisons are checked the same way

A comparison casts its right side toward the left too, so an `Integer` compared with a `Float` would compare
`Integer`s: `progress > -0.5` would cut `-0.5` to `0` and answer `0 > 0`, false, where the mathematics says true.
So a comparison whose right side is wider than its left is a compile error naming the fix, as arithmetic is:
write the wider side first, with the comparison turned around:

```gdscript title=casting_edge/casting_edge.spite entry error
var console = Console()

func CastingEdge() {
    var progress: Integer = 0
    console.print("mathematically true", progress > -0.5)
}
```
```diagnostic
'progress > -0.5' compares in Integer, since a comparison takes the left side's type, and the right side is a Float, which would be cut to fit: write the Float first ('-0.5 < progress')
```

Written the other way round the comparison is done in `Float`, and answers what the mathematics does. An integer
literal that fits the left side is not wider (`small < 10` with a `Byte` `small`):

```gdscript title=casting_fixed/casting_fixed.spite entry
var console = Console()

func CastingFixed() {
    var progress: Integer = 0
    console.print("mathematically true", -0.5 < progress)
    var price: Float = 3.0
    var quantity: Integer = 2
    console.print("total", price * quantity)
}
```
```output
mathematically true true
total 6
```

`String` converts both ways. Text is read as a number by `String`'s `to_<name>()` for that type:
`to_integer()`, `to_long()`, `to_double()`, ... (the full list is in [the rules](#numeric-types)). Each answers a
`T?`, `null` when the text is not a number that fits, since a `0` there would read as a real zero; assigning text
to a `T?` of a number calls it too. A number becomes text with its own
`to_string()` ([below](#numbers-are-classes)).

### Numbers are classes

`Integer`, `Long`, `Float`, `Double`, `Boolean` and the rest are classes in `library/` (`library/integer.spite`,
`library/double.spite`, ...), the way `String` is. Their functions are called on a value like any class's,
and inside one of them `this` is the number itself. Turning a number into text is `to_string()`, written in Spite in
`library/long.spite` and `library/double.spite`, and it is what a text hole (`"count {count}"`) calls. A program reopens a number
class the way it reopens any class ([packages.md](packages.md#monkey-patching-mods)), with a file named after it:

```gdscript title=number_methods/integer.spite
func doubled(): Integer {
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
    console.print(third, bits)
}
```
```output
42 21
0.3333333333333333 4599676419421066581
```

None of this costs anything at run time: a number is still a plain value (an `Integer` is an `int32_t`), the
class gives it functions, not a header, and a function the program never calls is not emitted. A cast from one
number type to another is written by assignment, `var half: Float = count`, or by passing the value where the
other type is expected; it compiles to the one machine conversion. A conversion belongs to the value converted:
its function is the source's `to_<type>()`, so `count.to_float()` is the call form, and a function named
`from_...` is an error ([rules](#numbers-are-classes-and-this)).

`this` works in every class, not only numbers, to hand the object itself to something:
`registry.append(this)`. Reading your own members through it is an error, because a class already reads them by
name: write `name`, not `this.name`.

A decimal is worked out for speed, and its last bits are never a promise. A decimal literal is a `Float`, so
`tenth * 3.0` with a `Float` `tenth` multiplies in `Float` precision, and `tenth == 0.1` is `true` after `var tenth:
Float = 0.1`. Inside a loop the compiler may fuse a multiply and an add into one step and add up a sum in another
order, which is what lets a loop over decimals run several at a time; `Float.not_a_number` and the infinities still
behave as they always do. A loop that compares decimals with `==` or `!=` keeps every step in the order written,
so what an exact comparison sees is exact.

### Bitwise functions

There are no bitwise operator symbols. Every whole-number class (`Tiny` to `UnsignedLong`, not `Float`, `Double`
or `Boolean`) answers them as functions, each compiled to the one C operation and inlined in an optimised build:

| Function | Answers |
|---|---|
| `shifted_left(count)` | the bits moved `count` places up, zeros coming in |
| `shifted_right(count)` | moved down: a signed type copies its sign bit in (arithmetic), an unsigned type zeros (logical) |
| `bits_and(other)`, `bits_or(other)`, `bits_exclusive_or(other)` | the bits set in both, in either, in exactly one |
| `bits_inverted()` | every bit flipped |
| `set_bit_count()`, `leading_zero_count()`, `trailing_zero_count()` | how many bits are set, and how many zeros stand above the highest set bit and below the lowest, as an `Integer` (the width, for 0) |

Each answers the receiver's type, with `other` cast to it like any argument: a `Byte` and a `Long` mask give a
`Byte`. No shift is undefined: a count of the width or more moves every bit out, and a negative count halts the
program ([the rules](#numbers-are-classes-and-this)).

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

### Maths functions

The maths a game or a renderer needs are functions of the number classes too, spelled out in full: on a `Float`
or a `Double`, `square_root()`, `sine()`, `cosine()`, `tangent()`, `arc_sine()`, `arc_cosine()`,
`arc_tangent()`, `rise.arc_tangent_over(run)` (the angle of the point `(run, rise)`, in the right quadrant),
`power(exponent)`, `exponential()`, `logarithm()` (natural), `logarithm_base_2()`, `logarithm_base_10()`,
`floor()`, `ceiling()`, `round()` (half away from zero), `truncate()`, `absolute()`, `minimum(other)`,
`maximum(other)`, `clamp(low, high)`, `is_finite()`, `is_infinite()` and `is_not_a_number()`. The whole numbers
have `absolute()`, `minimum(other)`, `maximum(other)` and `clamp(low, high)`. Each answers the receiver's type
(the three questions a `Boolean`), with the other operands cast to it like any argument, and each is the C
library's function written where it is called (`sqrtf` on a `Float`, `sqrt` on a `Double`), with no call of
Spite's own around it.

A number class also answers its constants on the class itself, `Float.pi`, since they belong to no one value:
`pi`, `tau`, `euler_number`, `infinity`, `not_a_number`, `largest` and `smallest` on `Float` and `Double`, and
`largest` and `smallest` on every whole number. `Integer.largest` is 2147483647 and `Float.smallest` the
most negative `Float` there is. A constant is a get-only attribute of the class: it is read without parentheses,
and nothing can assign it.

```gdscript title=maths_basics/maths_basics.spite entry
var console = Console()

func MathsBasics() {
    var side: Double = 2.0
    var diagonal = side.square_root()
    console.print("{diagonal} {Float.pi} {Double.pi}")
    var speed = 7.5
    var capped = speed.clamp(0.0, 5.0)
    var ahead = speed.round()
    var behind = -2.5
    console.print("{capped} {ahead} {behind.round()} {behind.floor()} {behind.absolute()}")
    var negative = -1.0
    var root = negative.square_root()
    console.print("{root} {root.is_not_a_number()} {Integer.largest}")
}
```
```output
1.4142135623730951 3.1415927 3.141592653589793
5 8 -3 -3 2.5
nan true 2147483647
```

As with `/` on a float, nothing here halts: the square root of `-1` is not-a-number and the logarithm of
`0` is minus infinity, the IEEE answers. What each function answers at its edges is
[standard_library.md](standard_library.md#maths)'s.

### Every number fits `Number`

The library declares `type Number` once (`library/number.spite`), a [shape](#inline-types-and-duck-typing) that
every number class fits: `Tiny`, `Short`, `Integer`, `Long`, `Byte`, `UnsignedShort`, `UnsignedInteger`,
`UnsignedLong`, `Float` and `Double`. `Boolean` and `Memory.Address` do not. It is what every number has in
common, and nothing more:

```gdscript
type Number {
    sum(Number): Number
    subtract(Number): Number
    multiply(Number): Number
    divide(Number): Number
    remainder(Number): Number
    less_than(Number): Boolean
    greater_than(Number): Boolean
    to_long(): Long
    to_double(): Double
}
```

The first seven are the operators `+`, `-`, `*`, `/`, `%`, `<` and `>` (with `<=` and `>=`), and `Number` in them
stands for the class that fits: an `Integer` adds an `Integer` and answers one. The last two are conversions every
number has. A number library is written over it as a generic constrained by `Number`, compiled once per number
class it is given, so the values stay plain machine numbers and nothing is boxed. `$value_type == Number` asks,
while compiling, whether a type is a number, which is how a walk over types tells numbers apart
([metaprogramming.md](metaprogramming.md#asking-what-a-generic-was-given)):

```gdscript title=number_type_doc/statistics.spite
generic $number_type: Number

func largest(values: List<$number_type>): $number_type {
    crash values[0]
    var best = values[0]
    var index = 1
    while index < values.count() {
        if values[index] > best {
            best = values[index]
        }
        index = index + 1
    }
    return best
}

func mean(values: List<$number_type>): Double {
    var total: Double = 0.0
    var index = 0
    while index < values.count() {
        total = total + values[index].to_double()
        index = index + 1
    }
    return total / values.count().to_double()
}
```
```gdscript title=number_type_doc/describe.spite
generic $value_type

func kind(): String {
    if $value_type == Number {
        return "a number"
    }
    return "not a number"
}
```
```gdscript title=number_type_doc/number_type_doc.spite entry
var console = Console()

func NumberTypeDoc() {
    var scores: List<Integer> = [4, 9, 2]
    var score_statistics = Statistics<Integer>()
    var best_score = score_statistics.largest(scores)
    var mean_score = score_statistics.mean(scores)
    console.print(best_score, mean_score)
    var weights: List<Float> = [0.5, 2.5]
    var weight_statistics = Statistics<Float>()
    var heaviest = weight_statistics.largest(weights)
    console.print(heaviest)
    var byte_kind = Describe<Byte>()
    var flag_kind = Describe<Boolean>()
    var byte_text = byte_kind.kind()
    var flag_text = flag_kind.kind()
    console.print(byte_text, flag_text)
}
```
```output
9 5
2.5
a number not a number
```

A parameter can be typed `Number` too, and its operators are the number's own. The function is compiled once for
each number class that reaches it ([optimizations.md](optimizations.md#a-function-taking-a-type-is-compiled-per-class)),
so in the copy that takes an `Integer`, `value + value` is an `Integer` addition, checked like any other:

```gdscript title=number_parameter_doc/number_parameter_doc.spite entry
var console = Console()

func NumberParameterDoc() {
    var twice_four = twice(4)
    var twice_half = twice(0.5)
    console.print(twice_four, twice_half)
    var bigger = larger(3, 9)
    console.print(bigger)
}

func twice(value: Number): Number {
    return value + value
}

func larger(first: Number, second: Number): Number {
    if first > second {
        return first
    }
    return second
}
```
```output
8 1
9
```

Since each copy is compiled for its class, `value + 0.5` is the error it would be on an `Integer` when an `Integer`
reaches it. A value whose class is known only while the program runs (read from a `List<Number>`) reaches the copy
for its class. In a `--hot-reload` or REPL build, where new code may bring a class later, the function is compiled
once as written, and an operator there tests both classes while the program runs: the right side is turned into
the left side's class, and if it does not fit exactly, the program halts naming the operation.

## `String`

Immutable, length-prefixed (not a bare `char*`), reference counted. A value is placed inside written text rather
than joined to it with `+`: `"hello {name}"`, where `{ }` holds one value of any type (every numeric type,
`Boolean`, and enum format themselves) and `\{` is a brace meant literally. Two values still join with `+`, and
joining written text with `+` is an error naming the form above. `==`/`!=`/`<`/`>` compare by content.

A backslash writes a character a text cannot hold as itself: `\n` (line break), `\t` (tab), `\r` (carriage
return), `\\` (backslash), `\"` (double quote) and `\{` (brace). Escapes follow each other freely, so `"\\\{"` is a
backslash and a brace, and a text written inside a hole keeps its own escapes and holes
([the rule](#text-escapes)).

A text that is one hole and nothing else, `"{clicks}"`, is an error: it is the value
itself, so the value is written directly, and where text is wanted it becomes text on its own
([the rules](#casting)). The error gives the line to write:

```gdscript title=lone_hole/lone_hole.spite entry error
var console = Console()
var label = ""

func LoneHole() {
    var clicks = 3
    label = "{clicks}"
    console.print(label)
}
```
```diagnostic
'"{clicks}"' is a text of one value and nothing else: assign the value directly, 'label = clicks'
```

```gdscript title=direct_text/direct_text.spite entry
var console = Console()
var label = ""

func DirectText() {
    var clicks = 3
    label = clicks
    var line = "clicks: {clicks}"
    console.print(label, line)
}
```
```output
3 clicks: 3
```

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
    var age = "42".to_integer()
    crash age
    console.print("parsed", age)
    var not_a_number = "not a number".to_integer()
    if not_a_number {
        console.print("parsed", not_a_number)
    } else {
        console.print("not a number")
    }
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
not a number
```

Reading a number that is not there, like `"not a number"` above, answers `null`, never a `0` that would read as a
real zero. Reading with `[]` is the exception: an index or a key that may not be there answers a `T?`
([failure.md](failure.md#reading-with--answers-t)). See [standard_library.md](standard_library.md) for the full
method table.

## `T?`: values that may be null

`Monster?` is a `Monster` or `null`, and `null` exists for nothing else. A `T?` is narrowed before it is used
(`if value { } else { }`, `assert value`, `crash value`, `while value`, or a `switch` with a `Null:` case), and
reading with `[]` answers one. A `Boolean?` is the one `T?` that is no condition, since its `false` would read as
missing; `assert flag` alone takes one, and asks that it is there and `true`. All of it is in
[failure.md](failure.md), with the three outcomes a failure can have. Since `null` belongs to `T?` alone, `var target: Monster = null` is an error that asks for `Monster?` (none
yet) or `Monster()` (a default), and a generic class makes the default of what it is bound to with `$name()`
([the rules](#variables-and-values)).

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

func is_knight(): Boolean {
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

An enum declaration takes no `=` and lists one value per line, with no commas (a value may be given a number,
[below](#numbering-an-enums-values)). A value is written in single
quotes (single quotes mean an enum value and nothing else), and `'mage'` alone resolves without writing
`Player.Job`, because the parameter, the annotation, the assignment or the comparison it is used in says which
enum it belongs to ([how it resolves](#enums-in-full)).

An enum is a closed list of **symbols**: a value the enum does not list is a compile error naming the ones it
does, and `var choice = 'orange'`, with nothing to check it against, is an error too. A `Symbol` on its own is
text from the program's table of names (reflection answers class and function names as symbols) and reads as
text anywhere text is expected. At run time an enum value is a small integer and a `Symbol` a pointer into a table
holding only the symbols the program uses ([the rules](#enums-in-full)).

Text is read as an enum value the way it is read as a number: `"soup".to_course()` answers a `Course?`, the value
spelled that way, or `null` when the enum has none, since any value of the enum there would read as a real one.
The reading is named after the enum (`to_` and its name in `snake_case`, `to_course_kind()` for `CourseKind`), and
assigning text to a `Course?` calls it too. Assigning text to a plain `Course` is an error naming `to_course()`:

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
        var course = name.to_course()
        if course {
            console.print(name, "is a course:", course)
        } else {
            console.print(name, "is no course")
        }
        index = index + 1
    }
}
```
```output
dessert is a course: dessert
brunch is no course
```

### Walking an enum's values

An enum is a class, so `Course` read as a value is its `Spite.Class`, and `Course.values` lists its values in the
order the enum declares them. It is an ordinary list, walked with `each` like any other
([metaprogramming.md](metaprogramming.md#walking-a-programs-structure)).

```gdscript title=enum_walk/enum_walk.spite entry
enum Course {
    'starter'
    'soup'
    'dessert'
}

var served: Course = 'soup'
var console = Console()

func EnumWalk() {
    Course.values.each(list_course)
}

func list_course(course: Course) {
    var is_served = course == served
    console.print(course, is_served)
}
```
```output
starter false
soup true
dessert false
```

It is all decided while compiling: the walk becomes three calls, and no list of an enum's values exists at run
time ([rules](#enums-in-full)). An enum is also open to the program that loads it: reopening its
class declares the enum again with the whole new list of values
([packages.md](packages.md#reopening-an-enum-replaces-it)), and a walk then walks that list.

### Numbering an enum's values

Inside a program an enum's values are only names. When something outside it already has a number for each
value (a C library, a binary file, a database, the network), give the value its number with `=` on its own line:

```gdscript title=numbered_roles/numbered_roles.spite entry
enum Role {
    'admin' = 99
    'user'
    'guest' = 7
}

var console = Console()

func NumberedRoles() {
    var role: Role = 'user'
    console.print(role, role == 'user')
}
```
```output
user true
```

`=` because giving a value is assignment, as everywhere else. A value without a number counts on from the one
before, so `'user'` is 100, and an enum's first value without a number is 0. Two values with one number are a
compile error, since the number is the value's identity outside the program:

```gdscript title=role_number_twice/role_number_twice.spite entry error
enum Role {
    'admin' = 7
    'user'
    'guest' = 8
}

var console = Console()

func RoleNumberTwice() {
    var role: Role = 'guest'
    console.print(role)
}
```
```diagnostic
'user' and 'guest' both have the number 8 in enum 'Role': a value's number is its identity, so give one of them another
```

## Unions

A tagged union. A `switch` must cover every member, and narrows the value inside each case; when every member
shares a function/attribute of the same shape, it can be called directly on the union value without a
`switch` at all (duck-typed, the same way a `type` is; see below).

```gdscript title=union_basics/player.spite
var health = 10

func Player(starting_health: Integer) {
    health = starting_health
}

func is_alive(): Boolean {
    return health > 0
}
```
```gdscript title=union_basics/monster.spite
var health = 6

func Monster(starting_health: Integer) {
    health = starting_health
}

func is_alive(): Boolean {
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

A union costs nothing to hold: the value is the object itself, and a `switch` or a call on it compares the class
its header already names against the members ([what it costs](#unions-in-full)).

## Inline types and duck typing

A `type` is a class matched by shape: any value with the same attribute names and types is accepted,
including a plain object literal:

```gdscript title=duck_typing/player.spite
var weapon = "sword"
var power = 5

func Player(starting_power: Integer) {
    power = starting_power
}
```
```gdscript title=duck_typing/duck_typing.spite entry
type Loadout {
    weapon: String
    power: Integer
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
plain `{ key: value }` literal work. The empty `type`, which every object fits, is built in as `Anything`
(`component: Anything`, `List<Anything>()`; see [functions_and_operators.md](functions_and_operators.md)):
never declare an empty `type` of your own.

A `type` of attributes only has a default like a class does: the object literal with each attribute at its own
default, whose `.class` answers `Object`. So `var target = $target_type()` in a generic class bound to such a
`type` holds a real object, and what is written through it stays written.

A `type` may require functions as well as attributes. A required function names the types it takes and returns,
never the names of its parameters (`render(Integer): String`), because the name a class gives its own parameter
does not matter to the shape. Any class with a function of that signature fits, which is how a list holds "any
class that can render" without a union naming every class in advance:

```gdscript title=shape_functions_doc/badge.spite
var label = ""

func Badge(new_label: String) {
    label = new_label
}

func render(width: Integer): String {
    return "[{label}] ({width})"
}
```
```gdscript title=shape_functions_doc/banner.spite
func render(columns: Integer): String {
    return "== banner {columns} =="
}
```
```gdscript title=shape_functions_doc/shape_functions_doc.spite entry
type Renderable {
    render(Integer): String
}

var console = Console()

func ShapeFunctionsDoc() {
    var badge = Badge("new")
    show(badge)
    var banner = Banner()
    show(banner)
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

`show` above is compiled twice, once for `Badge` and once for `Banner`: a function that takes a `type` gets a
copy for each class that reaches it, and in that copy `item` is that class, so `item.render(12)` is a direct call.
A number, `Boolean` or enum value passed to it arrives as the plain value. When the class is known only as the
program runs (a value read from a `List<Anything>`, say), the call tests the value's class against every class the
program ever gives that `type` and runs the matching copy. A class that never reaches the function gets no copy,
so nothing is compiled for it.

A `type` can also be the element of a variadic parameter, `...items: List<Renderable>`, so a call takes any number
of values of any classes that fit ([functions_and_operators.md](functions_and_operators.md#variadic-arguments)).

Read through a `type`, a required function named without calling it is a function value bound to the value, as
it is for a class ([functions_and_operators.md](functions_and_operators.md#functions-are-values)), so
`Parallel(stage.run_once)` works on a `List<Stage>`'s element typed by shape. A `List` of a `type` answers the
member templates too, over the attributes and the argument-free functions the type names: `map_names()`,
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
    done: Boolean
    finish()
}

var console = Console()

func ShapeValuesDoc() {
    var jobs = List<Work>()
    var wash = Job("wash")
    jobs.append(wash)
    var dry = Job("dry")
    jobs.append(dry)
    finish_first(jobs)
    var names = jobs.map_names()
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
cases, the exact error texts and the notes on how it is built. Where the teaching above and these rules
disagree, the rules win.

### Variables and values

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
- Every class has a default value (`Integer` 0, `Float` 0.0, `Boolean` false, `String` "", a class: its attribute defaults).
  Operations that cannot succeed produce the default instead of crashing, except reading with `[]`, which
  answers `T?`: an index or key that may not be there is a value that may be null, narrowed like any other.
- `null` exists only as the empty state of `T?`. **`var x: T = null` on a non-nullable `T` is a compile error**:
  it made a default `T` (a whole object, for a class) while reading as "nothing yet", so a later `assert x` proved nothing. The error names both ways out:
  `the attribute 'target' is declared 'Monster', not 'Monster?', so '= null' would make a default Monster rather
  than nothing: write 'var target: Monster? = null' for none yet, narrowed before use, or 'var target =
  Monster()' for a default, or keep '= null' and assign it at the top of the constructor` (the default of a plain
  value is its literal: `0`, `0.0`, `false`, `""`). Two readings follow:
  - In a generic class, `$name()` makes the default of whatever `$name` is bound to (a class through its
    constructor with no arguments, an empty `List` or `Dictionary`, a `type`'s default object, `0`), and in a
    template, `argument.class()` (any walked symbol's `.class()`) makes the walked class's. So `var system:
    $system_type = null` is written `var system = $system_type()`.
  - Two forms keep `= null`, since what they declare is filled rather than thrown away: an attribute the
    constructor assigns at its top level (`var left: $left_type = null` with `left = new_left` in the constructor),
    and a local that a walk over its type's attributes fills later in the same function: a walked row
    (`var row: $row_type = null` then `fill_attributes(row, index)`), `var made: argument.class = null`
    filled and returned, and a reader filling an object attribute by attribute.
  `diagnostics/null_defaults`, `diagnostics/null_default_locals`, `conformance/stage6/codegen_defaults`.
- An inner `var` may shadow an outer local, parameter, or attribute with the same name, **and a `var` may shadow
  a name in the same scope too**. It may never shadow a function it can see
  ([functions_and_operators.md](functions_and_operators.md)). The second binding may hold a different type, which
  is what makes it worth having ([failure.md](failure.md#a-new-name-for-a-new-type) teaches it):

```gdscript
var content = file.read()        # String?
assert content
var content = content.trim()     # String
```

  **The order is: evaluate, then drop, then bind.** The new value is computed first (so `var content =
  content.trim()` reads the binding it is about to replace), then the previous value is released, running its
  `drop()` if that takes it to zero, and only then does the new binding take effect. Releasing first would make
  the common case a use-after-free.

  The unused rule ([Unused is an error](style.md#unused-is-an-error)) still applies to the binding being shadowed: shadowing a name that was never read
  is an error, which is what catches an accidental reuse rather than a deliberate one.

#### Text escapes

- Inside `"..."`, a backslash and the character after it are one character of the text: `\n` line feed, `\t`
  tab, `\r` carriage return, `\\` backslash, `\"` double quote, `\{` an opening brace that starts no hole. Any
  other character after a backslash is that character; the formatter writes `\'` as `'`.
- A `{` not escaped opens a hole, which ends at its matching `}`. A text written inside a hole is read as a text
  of its own, with its own escapes and holes, to any depth: `"{name.replace("x", "a{name.replace("x",
  "\\\{")}b")}"` is one text. A hole that reaches the end of the file without its `}` is `this '{' inside text
  never closes: ...`, and a text that reaches the end of its line is `this text never closes: ...`.
- Every escape alone, in pairs and in threes (plain, after a hole, inside a hole, and inside a text inside a
  hole) is `conformance/stage3/text_escapes`.

#### Casting

There is no cast syntax. The right side is always cast toward the left side.

```gdscript
func whole_part(value: Float): Integer {
    return value        # value is cast to Integer
}
```

This applies to binary operations, assignment, arguments (toward the parameter type) and `return` (toward the return type).

**Wider arithmetic is written wider operand first.** `Integer * Long` is an `Integer` multiply and `Long * Integer` a `Long` one, so an
arithmetic operator (`+`, `-`, `*`, `/`, `%`) whose right operand is wider than its left is a compile error that
names the rule and the fix: `'count * total' is a multiplication in Integer, since arithmetic takes the left side's
type, and the right side is a Long, which would be cut to fit: write the Long first ('total * count'), or store the
right side in an Integer first if it fits one`. For `-`, `/` and `%`, where order matters, the fix it names is
`store the left side in a Long first ('var wide: Long = count')`. Wider means more bits (`Tiny`/`Byte` 8, `Short`/`UnsignedShort` 16,
`Integer`/`UnsignedInteger`/`Float` 32, `Long`/`UnsignedLong`/`Double`/`Memory.Address` 64), or a `Float`/`Double` right side under a whole
number left side, which would lose its fraction; signedness alone is not wider. An integer literal on the right
that fits the left type is not wider (`small + 1` with a `Byte` `small` is a `Byte` addition). **A comparison
(`==`, `!=`, `<`, `<=`, `>`, `>=`) is held to the same rule**: it casts the right side toward the left, so
`age > 0.5` with an `Integer` `age` would mean `age > 0`, and a right side wider than the left is a compile error
naming the comparison turned around: `'progress > -0.5' compares in Integer, since a comparison takes the left
side's type, and the right side is a Float, which would be cut to fit: write the Float first ('-0.5 < progress'),
or store the right side in an Integer first if it fits one`. An integer literal that fits the left type is exempt
(`small < 10`); a float literal under a whole number never fits (`diagnostics/wider_comparison`). A constant expression that overflows the `Integer` its arithmetic is done in is an
error too, naming its value: `'(65536 - 120) * 65536' is 4287102976, which does not fit in an Integer, the type its
arithmetic is done in, so it would wrap: write the number itself, 4287102976, which is a Long`.
`diagnostics/wider_right_operand`. Both checks happen while compiling and change nothing in what is emitted.

**Anything with a `to_string()` casts to text.** Where a `String` (or a
`String?`) is wanted (assignment, `var name: String = value`, an argument, a `return`, a `Dictionary` key that
is not a whole number (a whole-number key keys the dictionary by numbers)) and
the value is not text, it becomes text through its `to_string()`, the conversion a text hole makes: a number, a
`Boolean`, an enum value (its name), a class object (`Spite.Class`, its name) and an object of any class that
declares `func to_string(): String`. `label.text = clicks` with an `Integer` `clicks` stores `"42"`, and
`show(badge)` with `func show(text: String)` passes `badge.to_string()`. It is the cast the right-to-left rule
already made for numbers, extended to classes; the text made is released like any other (`conformance/stage6/direct_text`).
A class with no `to_string()` stays an error: `a Pet cannot be used where a String is needed`.

**A text of one hole and nothing else is an error.**
`"{value}"` (no written character before or after, one hole) is the value itself, so writing it is refused
with the exact line to write instead, the value directly when it is text and cast by the rule above when it is
not (`diagnostics/lone_hole`):

| Where the text stands | The rewrite named |
|---|---|
| `counter.label.text = "{counter.click_count.clicks}"` | `assign the value directly, 'counter.label.text = counter.click_count.clicks'` |
| `var shown = "{clicks}"` | `declare the value directly, 'var shown: String = clicks'` (`'var shown = name'` when `name` is text; a written type is kept) |
| `return "{clicks}"` | `return the value directly, 'return clicks'` |
| `show("{clicks}")` | `pass the value directly, 'show(clicks)'` |
| `keyed["{clicks}"]` on a `Dictionary` | `look it up by the value directly, 'keyed[clicks]'` (a whole number then keys it by numbers) |
| `line + "{clicks}"` with a text `line` | `join the value directly, 'line + clicks'` |
| anywhere else, as `"{clicks}" == "0"` | `write its text as 'clicks.to_string()'` (`'write the value directly'` when it is text) |

The message starts `'"{clicks}"' is a text of one value and nothing else:`. A text with any written character
(`"clicks {clicks}"`, `"{clicks} "`) or two holes (`"{clicks}{label}"`) is unaffected. The check reads each
statement of a function body as written, before it is generated, so it costs nothing in what is emitted; an
attribute's default is not checked.

**Text is read as an enum by its name**:
`name.to_course()` answers a `Recipe.Course?`: the value spelled `name`, or `null` when the enum has none, exactly as
`to_integer()` answers `null` for text that is not a number. The reading is `to_` and the enum's name in
`snake_case`, resolved from where it is called as the enum's name written there would be, and a `String` function
of the same name comes first. `var course: Recipe.Course? = name` calls it too, and `var course: Recipe.Course =
name` is an error: `text is not a Course until it is read as one, and text that names none of its values has none
to give: read it with 'to_course()', which answers 'Course?', null when the text names none of its values, and
narrow it, or declare the value 'Course?'` (`diagnostics/enum_from_text`).

#### Numeric types

All the basic types a language has, with written names ("never `u64`"), and never abbreviated:
`Integer`, `UnsignedInteger` and `Boolean`, not `Int` and `Bool`, with every name built from them following
(`to_integer()`, `read_unsigned_integer`, `library/integer.spite`). The old spelling is an error that names the
new one: `'Int' is spelled 'Integer'` (`diagnostics/old_type_spellings`). The ten types and the machine types
they are built on are [the table at the top of this page](#numeric-types-and-the-casting-rule).

An integer literal defaults to `Integer`; one too large to fit becomes a `Long` instead. A decimal literal
defaults to `Float`. All of them follow the same right-side-casts-toward-left-side rule as everything else
(`var tiny: Tiny = some_integer_variable` narrows with an ordinary cast); a value that does not fit the narrower
type halts, as [Arithmetic that does not fit halts](#arithmetic-that-does-not-fit-halts) says, rather than wrapping or saturating. `List<T>`, `Dictionary<T>`, `T?`, and generics all work
with every numeric type; so does `String` conversion both ways: text is read as a number through `String`'s one
`to_<name>()` method per type, `to_tiny()`, `to_short()`, `to_integer()`, `to_long()`, `to_byte()`,
`to_unsigned_short()`, `to_unsigned_integer()`, `to_unsigned_long()`, `to_float()`, `to_double()`, each answering a
`T?` that is `null` when the text is not a number the type can hold ([standard_library.md](standard_library.md#string)
has what each accepts). Assigning a `String` to a `T?` of a number calls the same function; assigning it to a plain
number is an error, since text that is not a number would have nothing to give: `text is not an Integer until it
is read as one, and text that is not a number has none to give: read it with 'to_integer()', which answers
'Integer?', null when the text is not a number that fits, and narrow it, or declare the value 'Integer?'`. The
same goes for the compiler's own readings of text: an integer literal too large for a `Long` is a compile error,
and a remote REPL command assigning or passing a number that does not parse is refused. `count()`, `length()`, and
`index_of()` always return `Integer`, never a wider type.

**Arithmetic that does not fit halts, in every build.** Every `+`, `-` and `*` whose left side (the type the
arithmetic is done in) is a whole number, signed (`Tiny`, `Short`, `Integer`, `Long`) or unsigned (`Byte`,
`UnsignedShort`, `UnsignedInteger`, `UnsignedLong`), is compiled with the C compiler's overflow builtin
(`__builtin_add_overflow`, `__builtin_sub_overflow`, `__builtin_mul_overflow`) in that type, and an answer that
does not fit halts, since it is the moron's mistake: `spite: 'amount * factor' does not fit in an Integer
(2000000000 * 2), at game/game.spite:17 in Game.scaled` (`conformance/stage6/integer_overflow`); an unsigned one
prints its operands unsigned (`spite: 'left - 1' does not fit in an UnsignedInteger (0 - 1), at ...`,
`conformance/stage6/unsigned_overflow`). There is no build, flag or mode in which the operators wrap: the
ordinary build and `--optimized` check exactly as `--debug-memory` does. The same check covers:
  - `-` in front of a whole number that is not a literal: `-value` halts for the smallest signed value and for any
    unsigned value but `0`, printed as `(0 - value)`.
  - The smallest signed value divided by `-1` (`spite: 'lowest / divisor' does not fit in an Integer
    (-2147483648 / -1), at ...`, `conformance/stage6/smallest_divided`); its remainder, `%` by `-1`, is `0`.
  - A value put into a narrower name, by assignment, as an argument or by `return`: narrower is wider
    turned around (fewer bits, or a whole number under a `Float`/`Double`), and the value is checked against the
    whole range of the name's type (`spite: 200, an Integer, does not fit in a Tiny, at ...`,
    `conformance/stage6/narrowing_overflow`). A decimal is checked the same way, so not-a-number, an infinity and
    anything outside the range halts instead of becoming a value C leaves undefined (`spite: 99999997952, a Float,
    does not fit in an Integer, at ...`, `conformance/stage6/decimal_narrowing`); inside the range its fraction is
    cut as before. `to_<type>()` on a number checks the same way. A change of signedness at the same width or wider
    (`var bits: UnsignedLong = word` with a `Long` `word`) keeps the same bits and is not checked.
  - An attribute of a singleton counted from many threads (`hits = hits + 1`) checks the atomic addition's answer.
  - A number written where it does not fit is a compile error naming the range:
    `300 does not fit in a Byte, whose numbers run from 0 to 255: store it in a wider type, or use a number that
    fits` (`diagnostics/literal_does_not_fit`).

**Wrapping is a function named for it.** Every whole number has `wrapping_sum(other)`, `wrapping_subtract(other)`
and `wrapping_multiply(other)`, which work in the receiver's type, take `other` as a parameter of that type, and
keep the low bits of the answer (`UnsignedInteger 4000000000.wrapping_multiply(3)` is `3410065408`,
`Long.largest.wrapping_sum(1)` is `Long.smallest`). Each is one C operation on the bits
(`conformance/stage6/wrapping_functions`). The hashes and codecs of the library (`Dictionary`'s and the
allocation table's hash, `Sha256`, `Argon2`, `Noise`) call them where they wrap on purpose. A constant expression
that overflows is still a compile error, and `/` and `%` keep the check described next. The compiler leaves the
check out of `counter + 1` while a `<` on the local `counter` is in force, out of `counter - 1` while a `>` is, and
out of arithmetic on constants ([proofs.md](proofs.md#arithmetic-that-does-not-fit-halts),
`conformance/stage6/counter_room`); what the rest costs is measured in
[optimizations.md](optimizations.md#arithmetic-is-checked-in-every-build).

**Division by zero follows Go.** A whole-number `/` or `%` whose divisor is zero halts
the program, naming the operation and the line (`spite: 'total / parts' divided by zero, at
game/game.spite:22 in Game.share`), since dividing by zero is the moron's mistake
(`conformance/stage6/division_by_zero`). A divisor the compiler can see is zero, a constant (`total / 0`,
`total % (2 - 2)`) or a codegen value folded to `0`, is a compile error instead: `'total / 0' divides by zero,
which always halts the program` (`diagnostics/division_by_constant_zero`). Where a proof already shows the
divisor is not zero (`assert parts != 0`, `crash parts != 0`, `if parts != 0 { }`, `parts > 0`, or a `while`
condition saying so, kept or undone by a call as any proof is), no check is emitted
([optimizations.md](optimizations.md#a-proven-divisor-is-not-checked)). The smallest signed value divided by `-1`
does not fit and halts as above, and its remainder is `0`, where C would trap. Float division stays IEEE 754: `x / 0.0` is infinity or not-a-number.
`%` on a `Float` or `Double` is the remainder of the division truncated toward zero, so it keeps the sign of the
left side as a whole number's does (`-7.5 % 2.0` is `-1.5`, `730.5 % 360.0` is `10.5`), and `x % 0.0` is
not-a-number.
- **Floats keep infinity and not-a-number**: `Float` and `Double` are IEEE 754 as the
  hardware gives them, with no check after an operation that can overflow; they are dealt with where they cannot
  be represented, as `JsonWriter` does ([json.md](json.md)).

Printing a `Float`/`Double` uses shortest-round-trip formatting (try the fewest significant digits that parse
back to the exact same value) rather than a fixed number of digits, so an ordinary value like `0.1` prints `0.1`
on a 32-bit `Float`, never `0.100000001`. A value whose shortest digits would need an exponent prints with
`%.17g` (`%.9g` for `Float`) instead, as `1e+21` or `1.0000000000000001e-05`; infinity prints `inf` and not a
number prints `nan` on every platform.

#### Numbers are classes, and `this`

Every type in the table above, and `Boolean`, is a class in `library/`
(`library/integer.spite`, `library/double.spite`, ...), the way `String` is. A number is still a plain C value in
the emitted code (the class gives it functions, not a header), and a function of `Integer` receives its `int32_t`
as the receiver. Inside it, **`this`** is that value: `func doubled(): Integer { return this * 2 }`, and
`count.doubled()` calls it. A number class is reopened like any other ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)), by a file named after it.

- **Writing a number as text is its `to_string()`** (converting to text is a cast like
  `to_integer()`), in Spite: `Long.to_string()` writes the digits, `Double.to_string()` is the
  shortest-round-trip formatting above (over the digit arithmetic in `library/number_text.spite`), and the
  smaller types widen and call `Long.to_string()`. `Boolean.to_string()` answers `"true"` or `"false"`.
  Interpolation (`"count {count}"`), `+` onto a `String` and `console.print` call it
  ([standard_library.md](standard_library.md)).
- **A conversion belongs to the value converted**: there is no `from_` conversion anywhere: never `Class.from_x()`, never
  `from_class`, not for the compiler's own number casts either. Each number class and `Memory.Address` has `func
  to_type(type: Symbol): type.class`, a Symbol codegen function whose symbol ranges over the program's types, and
  the compiler makes each right-to-left cast with the source's `to_<type>()` (`count.to_float()`,
  `wide.to_short()`, `number.to_memory_address()`). Its body is the compiler's (a C cast, emitted inline, so a cast
  costs only the conversion), and `--final-classes` prints the declaration in every number class. A cast is
  written by assignment or by passing (`var half: Float = count`, or `count` handed to a parameter typed `Float`)
  or as the call `count.to_float()`; both are allowed.
- **Declaring a function whose name starts with `from_` is an error** (`diagnostics/from_conversion`): "'Celsius.from_integer' makes a Celsius from another value, and a
  conversion belongs to the value converted: declare 'to_celsius()' on the class it converts from, or take
  the value in the constructor". Calling one by name on a number class is the same rule
  (`diagnostics/cast_by_name`): "there is no 'Float.from_integer()': a conversion belongs to the value
  converted, so write 'value.to_float()' on an Integer, or cast by assignment: 'var converted: Float =
  value'". `type` may name a parameter and begin a type path, although it is a keyword elsewhere.
- **Bitwise operations are functions of the whole-number classes.** `Tiny`, `Short`, `Integer`, `Long`, `Byte`, `UnsignedShort`,
  `UnsignedInteger` and `UnsignedLong` each answer the functions in [the table above](#bitwise-functions):
  `shifted_left(count: Integer)`, `shifted_right(count: Integer)` and the three `bits_` functions of `other` and
  `bits_inverted()` return the receiver's type, and the three counts return an `Integer` (the width for 0). There
  are no operator symbols for them, and writing one is an error naming its function: `<<` and `>>` (`Spite has no
  '<<': bits are functions on the whole numbers, write 'value.shifted_left(count)'`), and a lone `&`, `|`, `^` or
  `~` (`value.bits_and(mask)`, `bits_or`, `bits_exclusive_or`, `bits_inverted()`); `&&` and `||` keep their own
  message (`diagnostics/old_shift`, `diagnostics/old_ampersand`). They are bodiless declarations the compiler
  supplies, so `--final-classes` prints them in each class, and each body is the single C operation,
  inlined in an optimised build. The rules, so no undefined C behaviour reaches a program:
  - `shifted_right` is **arithmetic on a signed type** (the sign bit is copied in: an `Integer` -20 shifted right by 2
    is -5) and **logical on an unsigned one** (zeros come in). `shifted_left` always brings zeros in, and a bit
    moved past the top is lost, so the result keeps the low bits, as a bit operation does (a `Tiny` 1 shifted left by 7 is -128).
  - **A count of the width or more shifts every bit out**: the answer is 0, or -1 for a negative signed value
    shifted right. **A negative count halts the program**, naming the function and the count
    (`spite: UnsignedShort.shifted_right was given the count -2, and a shift count is 0 or more`).
  - **The other operand is cast to the receiver's type**, the ordinary argument-to-parameter cast: a `Long`'s
    `bits_and` of a `Byte` widens the `Byte` and answers a `Long`. **A wider other operand is a compile error**
    (as it is for arithmetic), since it would be cut to fit: `'flags.bits_and(mask)' works in a Byte,
    since a bitwise function takes its receiver's type, and 'mask' is a Long, which would be cut to fit: store the
    receiver in a Long first ('var wide: Long = flags'), or store 'mask' in a Byte first if it fits one`. Wider means
    what it means for arithmetic, and an integer literal that fits the receiver is not wider (`byte.bits_and(15)`)
    (`diagnostics/wider_bitwise_operand`). The count is an `Integer`.
  - Called on `Float`, `Double` or `Boolean`, they are an error naming the whole numbers
    (`diagnostics/bitwise_on_float`). `conformance/stage6/bitwise_functions` and `negative_shift` pin them.
- **A number's bits are read as another type in place**: `Float.bits(): UnsignedInteger`, `Double.bits(): Long`,
  `UnsignedInteger.bits_as_float(): Float`, `Long.bits_as_double(): Double` and
  `UnsignedLong.bits_as_double(): Double` give the same bits as the other type, with nothing converted. They are
  bodiless declarations the compiler supplies, each a C macro over a union of the two types, so a call is
  written where it is made, touches no memory and allocates nothing, and a program that never calls one carries
  none of it. `conformance/stage6/half_precision` pins them.
- **The maths functions and the constants are members of the number classes** ([the teaching above](#maths-functions)).
  Which there are, what each answers at its edges and how each is lowered is
  [standard_library.md](standard_library.md#maths)'s, their one home. A constant is answered only by
  the class: `angle.pi` is "'pi' is a constant of the class Float, not of a value: write 'Float.pi'". A constant
  is a get-only attribute: `Float.pi()` is "'Float.pi' is a constant, read as an attribute and never called: write
  'Float.pi'", and `Float.pi = 3.0` is "'Float.pi' is a constant, and a constant is get-only: nothing can assign
  it" (`diagnostics/maths_constant_assigned`). A
  function of a value called on the class, `Float.square_root()`, names the constants the class answers
  (`diagnostics/maths_constant_on_value`). The short names other languages use are errors naming the Spite one:
  `side.sqrt()` is "Float has no function 'sqrt': Spite spells it 'square_root', since no name is abbreviated"
  (`diagnostics/maths_other_spellings`).
- **`this` works in every class**, only to pass or return the object itself, as in
  `registry.append(this)`. A member is still reached by its bare name, so reading one through `this` is an
  error: `this.name` is "a class reads its own attributes by name: write 'name', not 'this.name'", and
  `this.to_string()` is "a class calls its own functions by name: write 'to_string()', not 'this.to_string()'".
- A decimal literal is a decimal in what is emitted (`1.0`, not `1`), so `1.0 / 3.0` divides as decimals.
- **A decimal literal beside a `Float` is a `Float`**: an operator or comparison whose one side is a decimal literal
  (or its negation) and whose other side is a `Float` that is not a literal works in `Float` precision, so after
  `var tenth: Float = 0.1`, `tenth == 0.1` is `true`. Beside a `Double` the literal is a `Double`, and two literals
  together keep the precision of a `Double` until they are stored (`conformance/stage6/float_precision`).
- **The last bits of a decimal result are never a guarantee.** In a loop whose count is read once
  ([optimizations.md](optimizations.md#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked)), the
  compiler lets the C compiler fuse a multiply and an add and reorder a sum of decimals; not-a-number and the
  infinities still come out where they did. A loop that compares a decimal with
  `==` or `!=` (or a value whose type the compiler does not know there) keeps its operations as written, so an
  exact comparison sees exact values; nothing that writes to a file can run inside such a loop.
- **Every number class fits the library's `type Number`** (`library/number.spite`): `sum`, `subtract`, `multiply`,
  `divide`, `less_than` and `greater_than`, each taking and answering the class itself (`Boolean` for the two
  comparisons), and `to_long(): Long` and `to_double(): Double`. A number class declares no function for an
  operator: the compiler's own arithmetic answers it, so the class fits. `Boolean` and `Memory.Address` do not fit,
  and neither does anything else unless it declares those functions. `$value_type == Number` is decided while
  compiling ([metaprogramming.md](metaprogramming.md#codegen-values-)). A generic constrained by `Number` is compiled
  once per number class it is given, with plain values and no box. **A parameter typed `Number` takes the
  operators `+`, `-`, `*`, `/`, `<`, `>`, `<=` and `>=`**: the function is compiled once per number class that
  reaches it, and in each copy the operator is that class's arithmetic, with that class's rules (an `Integer` copy
  of `value + 0.5` is the "would be cut to fit" error). A conversion is called on the value's class. In a
  `--hot-reload`, `--repl` or `--repl-port` build the function is compiled as written, and an operator on a value
  typed `Number` tests the classes of both sides while the program runs: the right side is turned into the left
  side's class, and a value that does not fit it exactly halts the program with `spite: '<operation>' needs its
  right side to fit in <class>, at <place>` (`diagnostics/number_type_mistakes`, `conformance/stage6/number_type`,
  `conformance/stage6/number_parameter`). A `type` that does not require an operator's function still refuses the
  operator: `'+' on a value read as '<type>' needs the class that fits the type, which is not known while
  compiling: take the value through a codegen value the type constrains, 'generic $value_type: <type>'`.
- None of it costs anything at run time: a number stays a plain machine value, a cast is the one conversion written
  inline, a bitwise function is the single operation, and a number class's Spite functions (`to_string()`, a reopening's
  `doubled()`) are emitted only when the program calls them.

### Types

#### Enums in full

An `enum`, `type` or `union` declaration takes no `=` and always breaks lines, one entry per line, no commas. `=` means assignment and nothing else. Writing `enum Job = {` is a parse error naming the fix.

```gdscript player.spite
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
- From inside `Player` itself, plain `Job` also works; from elsewhere, both `Player.Job` and, when unambiguous across the whole program, plain `Job` work (searched: the referencing class's own namespace first, then its containing folder, then each parent folder, then the whole program by simple name).
- An enum value is resolved from where it is used (parameter, annotation, assignment target, comparison). Two enums may share a value name; with no expected type in hand, the value resolves when exactly one enum in the program has it, otherwise it is a compile error listing every enum that does.

**An enum is a closed list of symbols.** `'knight'` is a symbol (a name
known at compile time), and an `enum Job` declaration lists which symbols are accepted where a `Job` is expected.
That is all an enum is; the integer it compiles to is a representation detail.

- **A symbol literal is legal only where something says what it may be.** Where an enum is expected it must be
  one of that enum's symbols: `set_weapon('orange')`, when the parameter is an enum without `'orange'`, is a
  compile error listing the symbols that enum accepts. There is no widening from a symbol to an enum, and there
  is no untyped symbol literal: `var choice = 'orange'`, with nothing to check it against, is an error naming
  the missing context.
- **Text is always written in double quotes.** A symbol literal where a `String` is wanted (an argument, a `var`) is an
  error naming the fix, whether or not an enum has that value: `'world' in single quotes is a symbol, and text is
  wanted here: text is always written in double quotes, so write "world"` (`diagnostics/symbol_for_text`). Adding
  an enum value somewhere can then never change what a line passes. Where a `Symbol` is wanted, `'name'` stays
  the way to write one.
- **A `Symbol` is text from a precompiled, tree-shaken table.** Every symbol the program uses is an entry in one table the compiler
  writes, so a `Symbol` value costs no allocation (reading, storing, passing or comparing one never makes new
  text), and a symbol the program never uses does not exist in it. So all of Spite's own metaprogramming can use
  symbols freely: a used symbol was needed anyway, and an unused one disappears. A `Symbol` reads as text
  wherever text is expected (every `String` function answers on it) while `symbol.class` is `Symbol`, and text
  becomes a `Symbol` only through `Symbol(text)`, which answers `Symbol?`, `null` unless that symbol is already
  in the table. An enum is still the closed form: the list of symbols a place accepts.
  A symbol of 15 bytes or fewer is held inline, its bytes inside the value itself, like any short text, and a
  longer one points at constant text ("TinyString"); that is an optimisation the language does not observe
  ([optimizations.md](optimizations.md#short-symbols-are-inline-text)).
- **What a `Symbol` names is still checked by whatever consumes it**, at compile time, the way the Symbol
  codegen path checks `set_age(2)` against `Person`'s real attributes
  ([Symbol codegen](metaprogramming.md#templates)).

**An enum can be reopened, and walked.**

- **Reopening replaces the enum.** A file that reopens a class ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)) and declares one of its enums again
  replaces it whole, as a later function replaces an earlier one: the later declaration, in merge order (the
  program's own folder, then each loaded folder in load order), is the enum's whole list of values, in its
  order. Nothing is appended or merged, so adding a value means restating the list with it; a value only an
  earlier declaration listed is no value of the enum, and naming it is an error. A hot reload replaces it the
  same way. `conformance/stage6/enum_reopening` replaces one from two loaded folders, and
  `diagnostics/enum_reopening_replaces` names a value the replacement left out.
- **`Course.values` lists the values** of an enum in its order, an ordinary list of `Course`
  ([Walking a program's structure](metaprogramming.md#walking-a-programs-structure)): `each` over it is unrolled into one
  call per value, `Course.values[1]` is `'soup'`, and `find_by_name` finds one by its name.
- **A generic walks the enum it is given the same way**: in a generic class, `$value_type.values` lists the values
  when `$value_type` is an enum, decided for each instance while compiling.
- **All of it is compile time and tree-shaken.** A walk expands into one ordinary call per value, and
  each value into its constant, as `--final-classes` shows; no table of an enum's values, names or order
  exists at run time, and a program that never walks an enum carries nothing for it.
- Environments are an enum too.

**An enum value may be given a number with `=`.**

- One value per line as always: `'admin' = 99`. The number is a whole number, negative allowed, and fits an
  `Integer`; a value written without one counts on from the value before it, and the first value without one is 0.
  An enum with no number written is numbered 0, 1, 2 in its order.
- Two values with one number are a compile error naming both and the number
  (`diagnostics/enum_numbers`), and so is a number, written or counted on, that does not fit an `Integer`:
  `'large' would be number 2147483648 of enum 'Size', which does not fit an Integer: an enum value's number is an
  Integer`.
- The numbers are the enum's identity for C bindings, binary files, databases and the network
  ([foreign_libraries.md](foreign_libraries.md)); inside the program they change nothing the program can observe:
  names, comparisons, `switch`, `Course.values` and the reading from text work as for any enum
  (`conformance/stage6/numbered_enums`), and the compiler still stores the value in the smallest number class that
  fits.
- A hot reload cannot change the number of a value the running program already has: it is a compile error naming
  the value and both numbers, and a restart renumbers it.

#### Unions in full

Tagged unions. A `switch` must cover every member and narrows the value inside each case.
When every member has the same function or attribute (same signature), it can be used directly on the union.

```gdscript
union Enemy {
    Player
    Monster
}

func attack(enemy: Enemy): Boolean {
    switch enemy {
        Player: enemy.hurt()
        Monster: enemy.die()
    }
    return enemy.is_alive()
}
```

**`_:` answers for every member without a case of its own.** It is the
last case, and it means exactly "this body, written once for each remaining member": the value is narrowed to
each of those members in turn, so `_: creature.sound()` needs `sound()` only on the members `_` answers for,
not on the whole union. A switch is still exhaustive: `_` is how it covers the rest, not a way to skip it.
**A repeated case body is an error** whenever `_` could absorb it: two cases doing the same thing when there is
no `_` yet, or a case doing what `_` already does. `_` after every member already has a case, or anywhere but
last, is an error too (`diagnostics/switch_rest_case`, `conformance/stage6/rest_case`). A switch over a `T?`
keeps its two cases, `<Type>:` and `Null:`.

**`value == Circle` asks what class a value is.** A class name on the right
of `==` or `!=` is a class test, true when the value is an instance of that class: `expression ==
Syntax.Expressions.TrueLiteral`. A `T?` that is null is no class, so the test is false. Naming a class that is not
a member of the value's union is an error, since the answer could only be false (`conformance/stage6/class_test`).
`if value == Circle { }` narrows `value` to a `Circle` inside the block, the way a switch case does.
A generic class is named with its codegen values and no parentheses, `found == Storage<$component_type>`:
only the right side of `==`/`!=` reads that form, and anywhere
else it is an error saying to call it. Tested through a `type`, a class that fits is admitted to the shape
by the test itself, so a value read back from a `Dictionary<AnyStorage>` can be narrowed before this function
has stored one (`conformance/stage6/generic_class_test`).
**A codegen value bound to a class is a class test too**: inside `Fetch<$wanted_type>`, `if item == $wanted_type { found = item }` narrows `item`
to the bound class, as `if item == Health` would. A binding that is a number, `Boolean` or enum tests for its
class, since such a value keeps it inside a `type`, and the narrowed name is the plain value again;
`String`, a `List` or a `Dictionary` test for their own classes. Where the value's static type already answers,
the test is decided while compiling instead: a `Health` against `$wanted_type` bound to `Health` is `true`, bound
to `Label` is `false`, and a union that does not hold the bound class is `false` rather than the "never true"
error, since another binding of the same class may make it true. A `T?` holding the bound class still tests for
null. A binding that is a `type` or a union is no class, so it is still "'$wanted_type' is a type here"
(`conformance/stage6/codegen_class_test`).
**So a switch that is one early return is an error** (`diagnostics/switch_single_case`): one class case and
`_:`, each a single `return`, is `if value == Class { return ... }` followed by what `_:` returns, and when both
return `Boolean` literals it is `return value == Class` (or `!=`). Code that can be written
simpler with no cost to reading is made to be, but a one-liner nobody can read back is not the goal. Anything
that would need an `else` stays a switch, and so does a switch with more cases: `_:` narrows to each remaining
member, which an `else` cannot.

Memory safety is done with unions instead of borrow checking noise: `T?` is the union of `T` and `Null`, and it is narrowed before use with `if ... else`, `assert`, `crash` or `switch` ([Null safety and `assert` narrowing](failure.md#narrowing)).
**`Monster?` is a union.** A `?` suffix is
sugar for the union of a type and nothing:

```gdscript
var target: Monster? = null

func find_target(): Monster? {
    return null
}
```

`Monster?` *is* `union { Monster, Null }`. `Null` is an ordinary class whose only value is the literal `null`,
the way `true` and `false` are the values of `Boolean`, so nullability is no special case: `switch target {
Monster: ... Null: ... }` is a switch over a union, and `assert`, `crash` and `if` narrow it by union narrowing.

At run time a union of a reference and `Null` is just the pointer, null or not; only a scalar `T?` (an
`Integer?`, a `Boolean?`) needs a wrapper, a flag beside the value, passed by value with no allocation
([Memory](memory.md#memory)). A union of classes is the object itself, its class read from the
header it already has, and every test on it is a comparison of that class id against the members the compiler
knows of.


#### Inline types and duck typing in full

```gdscript
type System {
    query: Query
    with: Dictionary<Class>
}
```

Like `enum` and `union`, a `type` declaration takes no `=` and always breaks lines, one entry per line, with no
commas. The single-line form is what would have needed commas, so removing it removes the choice, and a
`type` is read far more often than it is written, which is the case where the extra lines pay.

A `type` is a class matched by shape: any value with the same attributes and types is accepted, including object
literals. `.class` read through a `type`-shaped or union-typed value is answered from the object's own tag at run
time, so it names the class the value really is (`Widget`), not the shape it is being read through (`Labeled`).
An object literal has no class of its own, so it answers `Object`. A call through a `type` compares that tag
against the classes admitted to the shape (only the classes the program actually passes to it), so nothing
is registered or looked up by name at run time.

**A `type` may require functions, not only attributes**, matched by shape
exactly as an attribute-only `type` is:

```gdscript
type Renderable {
    render(): Element
}
```

Any class with a `render()` of that signature is accepted. **The type's own name inside a required signature
stands for the class that fits**: `sum(Number): Number` in `type Number` is met by an `Integer`'s
`sum(Integer): Integer`, so a type can ask for a function that takes and answers the class itself. This is what lets a collection hold "any class that
responds to `render()`" (a list of components, [Markup](targets.md#markup)) without a union naming every class in advance,
and it makes the respond-to check expressible as an ordinary type rather than as reflection.

**A `type` can be the element of a variadic parameter**: `...children: List<Renderable>` takes any number
of values of any classes that fit, written one by one. A generic can be written over a `type` or a class, and is
in the end compiled separately for each class. Each class that is passed is admitted to the shape, and each call on an
element is compiled once per admitted class ([Variadic arguments](functions_and_operators.md#variadic-arguments)).

**A function that takes a `type` is compiled once for each class that reaches it**, following calls through the
whole program. A call whose argument's class is known while compiling runs the copy made for that class: there the
parameter is that class, a number, `Boolean` or enum value arrives unboxed, calls through it are direct, a class
test on it is decided while compiling, and a copy that passes the parameter on reaches the copy made for the same
class. A value whose class is known only at run time (from a list, a dictionary, parsed data) goes through a test
of its class against the closed set of classes the program admits to that `type`, which runs the matching copy;
no general version of the function is in the program. The function as written is still compiled, so every error
in it is reported against the `type` it names; a class test that only a copy decides is not the "decided while
compiling" error. Not copied: a parameter the function assigns to, a `T?` of a `type`, a row of borrowed items, a
function that waits, and the functions of `List`, `Dictionary` and the other containers of the library. A
`--repl`, `--repl-port` or `--hot-reload` build compiles the function as written and no copies, since a class the
compiler has not seen may reach it later.

**A shape's members behave as a class's do**: a required function
read without calling it is a function value bound to the value, dispatched on the value's class when it is
called, so `Parallel(stages[index].run_once)` infers its codegen value from it; and a `List` of a `type` answers
the member templates over the attributes and the argument-free functions the type names
(`conformance/stage6/shape_members`).

---

Next: [Nullable values and failure](failure.md), how a missing value and a failed operation are handled.
