# Values and types

The specification of [Values and types](../docs/values_and_types.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Variables and values

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
    filled and returned, and a reader filling an object attribute by attribute (`var created: $value_type = null`
    then `created.attributes.each(read_attribute)`).
  `diagnostics/null_defaults`, `diagnostics/null_default_locals`, `conformance/stage6/codegen_defaults`.
- An inner `var` may shadow an outer local, parameter, or attribute with the same name, **and a `var` may shadow
  a name in the same scope too**. It may never shadow a function it can see
  ([functions_and_operators.md](../docs/functions_and_operators.md)). The second binding may hold a different type, which
  is what makes it worth having ([failure.md](../docs/failure.md#a-new-name-for-a-new-type) teaches it):

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

### Text escapes

- Inside `"..."`, a backslash and the character after it are one character of the text: `\n` line feed, `\t`
  tab, `\r` carriage return, `\\` backslash, `\"` double quote, `\{` an opening brace that starts no hole. Any
  other character after a backslash is that character; the formatter writes `\'` as `'`.
- A `{` not escaped opens a hole, which ends at its matching `}`. A text written inside a hole is read as a text
  of its own, with its own escapes and holes, to any depth: `"{name.replace("x", "a{name.replace("x",
  "\\\{")}b")}"` is one text. A hole that reaches the end of the file without its `}` is `this '{' inside text
  never closes: ...`, and a text that reaches the end of its line is `this text never closes: ...`.
- Every escape alone, in pairs and in threes (plain, after a hole, inside a hole, and inside a text inside a
  hole) is `conformance/stage3/text_escapes`.

### Casting

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

### Numeric types

All the basic types a language has, with written names ("never `u64`"), and never abbreviated:
`Integer`, `UnsignedInteger` and `Boolean`, not `Int` and `Bool`, with every name built from them following
(`to_integer()`, `read_unsigned_integer`, `library/integer.spite`). The old spelling is an error that names the
new one: `'Int' is spelled 'Integer'` (`diagnostics/old_type_spellings`). The ten types and the machine types
they are built on are [the table at the top of the page](../docs/values_and_types.md#numeric-types-and-the-casting-rule).

An integer literal defaults to `Integer`; one too large to fit becomes a `Long` instead. A decimal literal
defaults to `Float`. All of them follow the same right-side-casts-toward-left-side rule as everything else
(`var tiny: Tiny = some_integer_variable` narrows with an ordinary cast); a value that does not fit the narrower
type halts, as [Arithmetic that does not fit halts](../docs/values_and_types.md#arithmetic-that-does-not-fit-halts) says, rather than wrapping or saturating. `List<T>`, `Dictionary<T>`, `T?`, and generics all work
with every numeric type; so does `String` conversion both ways: text is read as a number through `String`'s one
`to_<name>()` method per type, `to_tiny()`, `to_short()`, `to_integer()`, `to_long()`, `to_byte()`,
`to_unsigned_short()`, `to_unsigned_integer()`, `to_unsigned_long()`, `to_float()`, `to_double()`, each answering a
`T?` that is `null` when the text is not a number the type can hold ([standard_library.md](../docs/standard_library.md#string)
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
    cut as before. `to_<type>()` on a number checks the same way. A change of signedness is checked like any
    other: `-1` stored in an `UnsignedInteger`, or an `UnsignedLong` past the `Long` range stored in a `Long`, halts
    (`spite: -3, an Integer, does not fit in an UnsignedInteger, at ...`, `conformance/stage6/signedness_change`). Only an unsigned value stored in a wider
    signed type is never checked, since it always fits. To read the same bits with the other signedness, call
    `bits_as_unsigned()` or `bits_as_signed()` (below).
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
out of arithmetic on constants ([proofs.md](../docs/proofs.md#arithmetic-that-does-not-fit-halts),
`conformance/stage6/counter_room`); what the rest costs is measured in
[optimizations.md](../docs/optimizations.md#arithmetic-is-checked-in-every-build).

**Division by zero follows Go.** A whole-number `/` or `%` whose divisor is zero halts
the program, naming the operation and the line (`spite: 'total / parts' divided by zero, at
game/game.spite:22 in Game.share`), since dividing by zero is the moron's mistake
(`conformance/stage6/division_by_zero`). A divisor the compiler can see is zero, a constant (`total / 0`,
`total % (2 - 2)`) or a codegen value folded to `0`, is a compile error instead: `'total / 0' divides by zero,
which always halts the program` (`diagnostics/division_by_constant_zero`). Where a proof already shows the
divisor is not zero (`assert parts != 0`, `crash parts != 0`, `if parts != 0 { }`, `parts > 0`, or a `while`
condition saying so, kept or undone by a call as any proof is), no check is emitted
([optimizations.md](../docs/optimizations.md#a-proven-divisor-is-not-checked)). The smallest signed value divided by `-1`
does not fit and halts as above, and its remainder is `0`, where C would trap. Float division stays IEEE 754: `x / 0.0` is infinity or not-a-number.
`%` on a `Float` or `Double` is the remainder of the division truncated toward zero, so it keeps the sign of the
left side as a whole number's does (`-7.5 % 2.0` is `-1.5`, `730.5 % 360.0` is `10.5`), and `x % 0.0` is
not-a-number.
- **Floats keep infinity and not-a-number**: `Float` and `Double` are IEEE 754 as the
  hardware gives them, with no check after an operation that can overflow; they are dealt with where they cannot
  be represented, as `JsonWriter` does ([json.md](../docs/json.md)).

Printing a `Float`/`Double` uses shortest-round-trip formatting (try the fewest significant digits that parse
back to the exact same value) rather than a fixed number of digits, so an ordinary value like `0.1` prints `0.1`
on a 32-bit `Float`, never `0.100000001`. A value whose shortest digits would need an exponent prints with
`%.17g` (`%.9g` for `Float`) instead, as `1e+21` or `1.0000000000000001e-05`; infinity prints `inf` and not a
number prints `nan` on every platform.

### Numbers are classes, and `this`

Every type in [the table](../docs/values_and_types.md#numeric-types-and-the-casting-rule), and `Boolean`, is a class in `library/`
(`library/integer.spite`, `library/double.spite`, ...), the way `String` is. A number is still a plain C value in
the emitted code (the class gives it functions, not a header), and a function of `Integer` receives its `int32_t`
as the receiver. Inside it, **`this`** is that value: `func doubled(): Integer { return this * 2 }`, and
`count.doubled()` calls it. A number class is reopened like any other ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)), by a file named after it.

- **Writing a number as text is its `to_string()`** (converting to text is a cast like
  `to_integer()`), in Spite: `Long.to_string()` writes the digits, `Double.to_string()` is the
  shortest-round-trip formatting above (over the digit arithmetic in `library/number_text.spite`), and the
  smaller types widen and call `Long.to_string()`. `Boolean.to_string()` answers `"true"` or `"false"`.
  Interpolation (`"count {count}"`), `+` onto a `String` and `console.print` call it
  ([standard_library.md](../docs/standard_library.md)).
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
  `UnsignedInteger` and `UnsignedLong` each answer the functions in [the table on the page](../docs/values_and_types.md#bitwise-functions):
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
  `UnsignedLong.bits_as_double(): Double` give the same bits as the other type, with nothing converted. So do
  `bits_as_unsigned()` on each signed whole number (`Tiny` to `Byte`, `Short` to `UnsignedShort`, `Integer` to
  `UnsignedInteger`, `Long` to `UnsignedLong`) and `bits_as_signed()` on each unsigned one, back the other way:
  `(-1).bits_as_unsigned()` is 4294967295, where storing `-1` in an `UnsignedInteger` halts. Hashes and binary
  formats read words this way. They are
  bodiless declarations the compiler supplies, each a C macro over a union of the two types, so a call is
  written where it is made, touches no memory and allocates nothing, and a program that never calls one carries
  none of it. `conformance/stage6/half_precision` pins them.
- **The maths functions and the constants are members of the number classes** ([taught on the page](../docs/values_and_types.md#maths-functions)).
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
  ([optimizations.md](../docs/optimizations.md#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked)), the
  compiler lets the C compiler fuse a multiply and an add and reorder a sum of decimals; not-a-number and the
  infinities still come out where they did. A loop that compares a decimal with
  `==` or `!=` (or a value whose type the compiler does not know there) keeps its operations as written, so an
  exact comparison sees exact values; nothing that writes to a file can run inside such a loop.
- **Every number class fits the library's `type Number`** (`library/number.spite`): `sum`, `subtract`, `multiply`,
  `divide`, `remainder`, `less_than` and `greater_than`, each taking and answering the class itself (`Boolean` for the two
  comparisons), and `to_long(): Long` and `to_double(): Double`. A number class declares no function for an
  operator: the compiler's own arithmetic answers it, so the class fits. `Boolean` and `Memory.Address` do not fit,
  and neither does anything else unless it declares those functions. `$value_type == Number` is decided while
  compiling ([metaprogramming.md](metaprogramming.md#codegen-values-)). A generic constrained by `Number` is compiled
  once per number class it is given, with plain values and no box. **A parameter typed `Number` takes the
  operators `+`, `-`, `*`, `/`, `%`, `<`, `>`, `<=` and `>=`**: the function is compiled once per number class that
  reaches it, and in each copy the operator is that class's arithmetic, with that class's rules (an `Integer` copy
  of `value + 0.5` is the "would be cut to fit" error). A conversion is called on the value's class. In a
  `--hot-reload`, `--repl` or `--repl-port` build the function is compiled as written, and an operator on a value
  typed `Number` finds the classes of both sides while the program runs and is the left side's class's own operator,
  under the casting rules above: a right side that is not wider is converted as the copy would convert it (an
  integer literal that fits the left side's class is not wider), and a wider one, the copy's compile error, halts
  with `spite: '<operation>' works in <class>, the class of its left side, and its right side is <class>, which would
  be cut to fit, at <place>`. `Number` has no operators of its own. An operator through any `type` that requires its
  function on a class instance is that class's own function (`sum` for `+`, `less_than` for `<` and `>=`,
  `greater_than` for `>` and `<=`), and a right side of another class halts with `which is not one`
  (`diagnostics/number_type_mistakes`, `conformance/stage6/number_type`, `conformance/stage6/number_parameter`,
  `conformance/stage6/shape_operators`, `conformance/stage6/shape_operator_cut`,
  `conformance/stage6/shape_operator_unlike`). A `type` that does not require an operator's function still refuses the
  operator: `'+' on a value read as '<type>' needs the class that fits the type, which is not known while
  compiling: take the value through a codegen value the type constrains, 'generic $value_type: <type>'`.
- None of it costs anything at run time: a number stays a plain machine value, a cast is the one conversion written
  inline, a bitwise function is the single operation, and a number class's Spite functions (`to_string()`, a reopening's
  `doubled()`) are emitted only when the program calls them.

## Types

### Enums in full

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
  ([optimizations.md](../docs/optimizations.md#short-symbols-are-inline-text)).
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
  ([Walking a program's structure](../docs/metaprogramming.md#walking-a-programs-structure)): `each` over it is unrolled into one
  call per value, `Course.values[1]` is `'soup'`, and `find_by_name` finds one by its name.
- **A generic walks the enum it is given the same way**: in a generic class, `$value_type.values` lists the values
  when `$value_type` is an enum, decided for each instance while compiling.
- **All of it is compile time and tree-shaken.** A walk expands into one ordinary call per value, and
  each value into its constant, as `--final-classes` shows; no table of an enum's values, names or order
  exists at run time, and a program that never walks an enum carries nothing for it.
- Environments are an enum too.

**An enum value may be given a number with `=`.**

- One value per line as always: `'admin' = 99`. The number belongs to the declaration only: everywhere else the
  value is written as any value is, `'admin'`, with no number after it. The number is a whole number, negative allowed, and fits an
  `Integer`; a value written without one counts on from the value before it, and the first value without one is 0.
  An enum with no number written is numbered 0, 1, 2 in its order.
- **`to_integer()` answers a value's number**, the written one or the counted one: `role.to_integer()` is `99` for
  `'admin'` above, and `100` for the value after it. It is how library code writes a value as its number (a binary
  file, a C binding) (`conformance/stage6/enum_to_integer`).
- Two values with one number are a compile error naming both and the number
  (`diagnostics/enum_numbers`), and so is a number, written or counted on, that does not fit an `Integer`:
  `'large' would be number 2147483648 of enum 'Size', which does not fit an Integer: an enum value's number is an
  Integer`.
- The numbers are the enum's identity for C bindings, binary files, databases and the network
  ([foreign_libraries.md](../docs/foreign_libraries.md)); inside the program they change nothing the program can observe:
  names, comparisons, `switch`, `Course.values` and the reading from text work as for any enum
  (`conformance/stage6/numbered_enums`), and the compiler still stores the value in the smallest number class that
  fits.
- A hot reload cannot change the number of a value the running program already has: it is a compile error naming
  the value and both numbers, and a restart renumbers it.

### Unions in full

Tagged unions. A `switch` must cover every member and narrows the value inside each case.
When every member has the same function or attribute (same signature), it can be used directly on the union.
**Every member is a class of objects.** `String`, a number class, `Boolean` and a generic class with its values
(`List<Byte>`) are refused where the union is declared: a value has no class for a `switch` to test, and a test
could tell only that a generic member is a `List`, not of what. Make the member a class of your own that keeps
the value in an attribute (`diagnostics/union_value_members`).

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

Memory safety is done with unions instead of borrow checking noise: `T?` is the union of `T` and `Null`, and it is narrowed before use with `if ... else`, `assert`, `crash` or `switch` ([Null safety and `assert` narrowing](../docs/failure.md#narrowing)).
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
([Memory](../docs/memory.md#memory)). A union of classes is the object itself, its class read from the
header it already has, and every test on it is a comparison of that class id against the members the compiler
knows of.


### Inline types and duck typing in full

```gdscript
type Labeled {
    label: String
    width: Integer
}
```

Like `enum` and `union`, a `type` declaration takes no `=` and always breaks lines, one entry per line, with no
commas. The single-line form is what would have needed commas, so removing it removes the choice, and a
`type` is read far more often than it is written, which is the case where the extra lines pay.

A `type` is a class matched by shape: any value with the same attributes and types is accepted, including object
literals. `.class` read through a `type`-shaped or union-typed value is answered from the object's own tag at run
time, so it names the class the value really is (`Widget`), not the shape it is being read through (`Labeled`).
An object literal has no class of its own, so it answers `Object`. A call through a `type` on a value whose class
is known only at run time compares that tag
against the classes admitted to the shape (only the classes the program actually passes to it), so nothing
is registered or looked up by name at run time. A class test admits nothing: `item == Ghost` on an `Anything` that
no `Ghost` ever reaches is false for every value, and the program carries no copy, no case and no code for `Ghost`
because of it (`conformance/stage6/tested_classes`). A `--repl`, `--repl-port` or `--hot-reload` build, where code
the compiler has not seen may bring a `Ghost` later, admits it.

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
responds to `render()`" (a list of components, [Markup](../docs/targets.md#markup)) without a union naming every class in advance,
and it makes the respond-to check expressible as an ordinary type rather than as reflection.

**A `type` can be the element of a variadic parameter**: `...children: List<Renderable>` takes any number
of values of any classes that fit, written one by one. A generic can be written over a `type` or a class, and is
in the end compiled separately for each class. Each class that is passed is admitted to the shape, and each call on an
element is compiled once per admitted class ([Variadic arguments](../docs/functions_and_operators.md#variadic-arguments)).

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

**A `type` has no functions of its own.** A required function is not an attribute holding a function value, and a
`type` has no dispatch object: a call through a `type` is always the function of the class the value really is,
compiled in that class's copy, and a `--repl`, `--repl-port` or `--hot-reload` build reaches that same function
through the dispatch the compiler writes. A required function named without calling it (`sleepy.nap`) is that
class's function bound to the value, as on any instance
([functions_and_operators.md](../docs/functions_and_operators.md#functions-are-values)); the value is held in a box of its
own, released with the function value (`conformance/stage6/type_function_values`). A `List` of a `type` answers
the member templates over the attributes and the argument-free functions the type names
(`conformance/stage6/shape_members`).

---

Next: [Nullable values and failure](failure.md).
