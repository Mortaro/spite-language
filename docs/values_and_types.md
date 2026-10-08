# Values and types

## Numeric types and the casting rule

There is no cast syntax. **The right side always casts toward the left side**: in a binary operation,
assignment, a function argument (toward the parameter type), and `return` (toward the return type).

| Type | C type |
|---|---|
| `Tiny` | `int8_t` |
| `Short` | `int16_t` |
| `Integer` (default integer) | `int32_t` |
| `Long` | `int64_t` |
| `Byte` | `uint8_t` |
| `UnsignedShort` | `uint16_t` |
| `UnsignedInteger` | `uint32_t` |
| `UnsignedLong` | `uint64_t` |
| `Float` (default decimal) | `float` (32-bit) |
| `Double` | `double` (64-bit) |

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
prove the answer fits ([proofs.md](proofs.md#arithmetic-that-does-not-fit-halts)): from the ranges your locals can
hold, a counter under its loop's bound, a remainder, a clamp, and a total that adds one term a pass over a list
([proofs.md](proofs.md#a-range-proves-arithmetic-fits)), so what remains costs little.
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
`to_integer()`, `to_long()`, `to_double()`, ... (the full list is in [the rules](../specs/values_and_types.md#numeric-types)). Each answers a
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
`from_...` is an error ([rules](../specs/values_and_types.md#numbers-are-classes-and-this)).

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
program ([the rules](../specs/values_and_types.md#numbers-are-classes-and-this)).

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
[standard_library.md](../specs/standard_library.md#maths)'s.

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

`Number` has no operators of its own: an operator on a value typed `Number` is always the operator of the class the
value really is, under the casting rules every number follows. Since each copy is compiled for its class,
`value + 0.5` is the error it would be on an `Integer` when an `Integer` reaches it. A value whose class is known
only while the program runs (read from a `List<Number>`) reaches the copy for its class. In a `--hot-reload` or REPL
build, where new code may bring a class later, the function is compiled once as written, and an operator there
finds both classes while the program runs and does exactly what the copy would: a right side that widens to the
left side's class is widened, and one that is wider, which a copy refuses while compiling, halts the program instead:

```
spite: 'value + step' works in a Byte, the class of its left side, and its right side is an Integer, which would be cut to fit, at game/game.spite:31 in Game.grown
```

The same holds for a `type` of your own that requires an operator's function: `left + right` on two values read as
`type Addable { sum(Addable): Addable }` calls the `sum` of the left side's class, and a right side of another
class, a compile error in a copy, halts with `which is not one`.

## `String`

Immutable, length-prefixed (not a bare `char*`), reference counted. A value is placed inside written text rather
than joined to it with `+`: `"hello {name}"`, where `{ }` holds one value of any type (every numeric type,
`Boolean`, and enum format themselves) and `\{` is a brace meant literally. Two values still join with `+`, and
joining written text with `+` is an error naming the form above. `==`/`!=`/`<`/`>` compare by content.

A backslash writes a character a text cannot hold as itself: `\n` (line break), `\t` (tab), `\r` (carriage
return), `\\` (backslash), `\"` (double quote) and `\{` (brace). Escapes follow each other freely, so `"\\\{"` is a
backslash and a brace, and a text written inside a hole keeps its own escapes and holes
([the rule](../specs/values_and_types.md#text-escapes)).

A text that is one hole and nothing else, `"{clicks}"`, is an error: it is the value
itself, so the value is written directly, and where text is wanted it becomes text on its own
([the rules](../specs/values_and_types.md#casting)). The error gives the line to write:

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
([the rules](../specs/values_and_types.md#variables-and-values)).

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
enum it belongs to ([how it resolves](../specs/values_and_types.md#enums-in-full)).

An enum is a closed list of **symbols**: a value the enum does not list is a compile error naming the ones it
does, and `var choice = 'orange'`, with nothing to check it against, is an error too. A `Symbol` on its own is
text from the program's table of names (reflection answers class and function names as symbols) and reads as
text anywhere text is expected. At run time an enum value is a small integer and a `Symbol` a pointer into a table
holding only the symbols the program uses ([the rules](../specs/values_and_types.md#enums-in-full)).

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
time ([rules](../specs/values_and_types.md#enums-in-full)). An enum is also open to the program that loads it: reopening its
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
its header already names against the members ([what it costs](../specs/values_and_types.md#unions-in-full)).

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

A `type` has no functions of its own. A required function says only what a class must have, so a call through a
`type` is always the function of the class the value really is, compiled in that class's copy: `jobs[0].finish()`
below is `Job`'s own `finish`. A `List` of a `type` answers the member templates too, over the attributes and the
argument-free functions the type names: `map_names()`, `filter_done()`, `count_done()`:

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
    jobs[0].finish()
}
```
```output
wash, dry 1
```

---

Next: [Nullable values and failure](failure.md), how a missing value and a failed operation are handled.
