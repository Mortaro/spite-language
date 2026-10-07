# Functions and operators

```gdscript
func function_name(first: Reference, second: Value): Tiny {
    return 1
}
```

The return type follows a colon, `return` is always written out, and a function with no return type returns
nothing ([the rules](../specs/functions_and_operators.md#functions)). Forgetting the colon is a parse error that shows the form:

```gdscript title=missing_colon_error/missing_colon_error.spite entry error
func add_one(value: Integer) Integer {
    return value + 1
}
```
```diagnostic
a return type is written '(): Integer'
```

## Functions are values

Naming a function without calling it gives a value: the function bound to the instance it was named on, so it
runs exactly as that instance would have run it. There are no free functions and no closures. The value holds
its owner and nothing else. Its type is written like any generic, with the return last:
`Spite.Function<String, String>` takes a `String` and returns a `String`, and `Spite.Function<Nothing>` takes
nothing and returns nothing. Calling it is `change(text)` or `change.call_function(text)`, which are the same
call; and the value is also its own reflection, with `.name`, `.arguments` and `.returns`.

```gdscript title=function_values_doc/shouter.spite
func shout(text: String): String {
    return "{text.upper_case()}!"
}
```
```gdscript title=function_values_doc/function_values_doc.spite entry
var console = Console()

func FunctionValuesDoc() {
    var shouter = Shouter()
    var shouted = apply(shouter.shout, "hi")
    var quieted = apply(quiet, "HI")
    console.print(shouted, quieted)
    var remembered = shouter.shout
    var repeated = remembered.call_function("again")
    console.print(remembered.name, repeated)
}

func apply(change: Spite.Function<String, String>, text: String): String {
    return change(text)
}

func quiet(text: String): String {
    return text.lower_case()
}
```
```output
HI! hi
shout AGAIN!
```

An ordinary call, `shouter.shout("hi")`, is a direct call and builds no function value; only a function
used *as a value* costs anything, and little
([what it costs](../specs/functions_and_operators.md#functions-are-values-always-bound-to-an-instance)). A value that holds
a function bound to the instance holding it is a cycle, and leaks like any other: clear one side (see
[memory.md](memory.md)). A function that returns nothing returns `Nothing`, which is what its type and its
`.returns` say.

**A variable never has the name of a function it can see.** Since a function's
name is already a value, a local, parameter or attribute named like a function of its class would make that name
mean two things. `var file_stem = file_stem(path)` is an error; name the result for what it holds:

```gdscript title=shadowed_function_error/shadowed_function_error.spite entry error
var console = Console()

func ShadowedFunctionError() {
    var file_stem = file_stem("notes/today.txt")
    console.print(file_stem)
}

func file_stem(path: String): String {
    var parts = path.split("/")
    var last = parts.last()
    return last.replace(".txt", "")
}
```
```diagnostic
the variable 'file_stem' has the name of a function of this class, and a function is a value passed by its name: give the variable a name of its own
```

Written `var stem = file_stem("notes/today.txt")`, it compiles.

## Variadic arguments

A last parameter written `...name: List<Type>` takes every remaining argument, written one by one at the call,
and the function receives them as an ordinary `List<Type>`. `Type` may be a class or a `type`: with a `type`,
each argument is any class that fits it, and a call on an element answers for the class it really is.

```gdscript title=variadic_doc/variadic_doc.spite entry
type Named {
    name: String
}

var console = Console()

func VariadicDoc() {
    var line = joined("-", "a", "b", "c")
    console.print(line)
    var nobody = joined("-")
    console.print("[{nobody}]")
    var aria = Guest("Aria")
    var bram = Guest("Bram")
    greet(aria, bram)
}

func joined(separator: String, ...words: List<String>): String {
    return words.join(separator)
}

func greet(...visitors: List<Named>) {
    visitors.each(say_hello)
}

func say_hello(visitor: Named) {
    console.print("hello {visitor.name}")
}
```
```gdscript title=variadic_doc/guest.spite
var name = ""

func Guest(new_name: String) {
    name = new_name
}
```
```output
a-b-c
[]
hello Aria
hello Bram
```

Only the last parameter can take `...`, and it always receives a `List`. The values are passed one by one, so
passing a whole list to it is an error; a parameter written without `...` takes a list as it is. Each call
builds the `List` its function receives, exactly as if the caller had built it, and nothing more.

```gdscript title=variadic_not_a_list/variadic_not_a_list.spite entry error
var console = Console()

func VariadicNotAList() {
    shout("a", "b")
}

func shout(...words: String) {
    console.print(words)
}
```
```diagnostic
'...words' receives the arguments as a list: write '...words: List<Type>'
```

## Every operator is a function

Every operator is a shortcut for a function a class can define to support it:

| Operator | Function | | Operator | Function |
|---|---|---|---|---|
| `a + b` | `sum(b)` | | `a == b` / `a != b` | `equals(b)` (negated for `!=`) |
| `a - b` | `subtract(b)` | | `a < b` / `a > b` | `less_than(b)` / `greater_than(b)` |
| `a * b` | `multiply(b)` | | `a <= b` / `a >= b` | derived from the two above, no own function |
| `a / b` | `divide(b)` | | unary `-a` | `negate()` |
| `a % b` | `remainder(b)` | | `a[x]` / `a[x] = v` | `get_at(x)` / `set_at(x, v)` |

`a[x]` is `get_at(x)`, so any class that declares `get_at` can be indexed, and since every `[]` answers a value
that may be absent, `get_at` answers a `T?`: `func get_at(index: Integer): Integer?`,
`null` when nothing is at the index, and the reader narrows it (`crash shelf[0]`, then `shelf[0] + 1`). A `get_at`
that answers a plain `Integer` is an error naming `Integer?`.

`not`/`and`/`or` stay built-in keywords, never functions. On numbers, `Boolean`, enums, `String`, `List<T>` and
`Dictionary<T>` the operators are built in (on numbers, the machine's own arithmetic); number arithmetic is done in the left side's
type, so a wider right side is an error that says to write the wider operand first
([Wider arithmetic goes wider operand first](values_and_types.md#wider-arithmetic-goes-wider-operand-first)).
For a class of your own, the operator is a direct call of the matching function (nothing more at run time),
and the right side casts toward the parameter's type like any other argument:

```gdscript title=operators_as_functions/money.spite
var cents = 0

func Money(starting_cents: Integer) {
    cents = starting_cents
}

func sum(other: Money): Money {
    return Money(cents + other.cents)
}

func equals(other: Money): Boolean {
    return cents == other.cents
}

func greater_than(other: Money): Boolean {
    return cents > other.cents
}
```
```gdscript title=operators_as_functions/operators_as_functions.spite entry
var console = Console()

func OperatorsAsFunctions() {
    var wallet = Money(150)
    var found = Money(50)
    var total = wallet + found
    console.print("total cents", total.cents)
    var two_hundred = Money(200)
    var is_two_hundred = total == two_hundred
    console.print("equal", is_two_hundred)
    console.print("greater", total > wallet)
}
```
```output
total cents 200
equal true
greater true
```

Missing the function is a diagnostic that names exactly what to define:

```gdscript title=operator_missing_error/money.spite
var cents = 0

func Money(starting_cents: Integer) {
    cents = starting_cents
}
```
```gdscript title=operator_missing_error/operator_missing_error.spite entry error
var console = Console()

func OperatorMissingError() {
    var one = Money(1)
    var two = Money(2)
    console.print(one + two)
}
```
```diagnostic
needs it to define 'sum(other)'
```

## Setter/getter interception

Attribute access **from outside a class** also goes through operator-style functions. `person.age = 1` calls
`set_age(1)` when the class defines it (an exact function, or Symbol codegen; see
[metaprogramming.md](metaprogramming.md)); `person.age` (a read) calls `get_age()` the same way. An exact
function always wins over Symbol codegen. Compound assignment goes through both: `person.age = person.age + 1`
reads through `get_age()` and writes through `set_age(...)`.

```gdscript title=interception/account.spite
var balance = 0
var owner = ""

func Account(starting_balance: Integer, starting_owner: String) {
    balance = starting_balance
    owner = starting_owner
}

func set_attribute(attribute: Symbol, value: attribute.class) {
    attributes[attribute] = value
}

func get_attribute(attribute: Symbol): attribute.class {
    return attributes[attribute]
}

func set_owner(new_owner: String) {
    owner = new_owner.upper_case()
}

func get_owner(): String {
    return owner
}
```
`balance` has no exact `set_balance`/`get_balance`, so Symbol codegen answers reads and writes to it from
outside the class. `owner` has exact functions instead, which always win over Symbol codegen, so every name
written through `set_owner` is capitalized.

```gdscript title=interception/interception.spite entry
var console = Console()

func Interception() {
    var account = Account(100, "ann")
    account.balance = account.balance + 20
    console.print("balance", account.balance)
    account.owner = "bob"
    console.print("owner", account.owner)
}
```
```output
balance 120
owner BOB
```

Inside a class's own functions, a bare `age = 1` (no receiver) always stays a plain field write:
interception only applies to `receiver.field` written from outside. A getter may compute and hand back a fresh
`String` or `List` as safely as it returns a field. It costs nothing hidden: an intercepted access is a direct
call, and every other access a plain field read or write.

**A getter with no setter makes a read-only attribute** ([the rule](../specs/functions_and_operators.md#operators)). That is how
reflection keeps `Spite.Class.name` from being overwritten
([reflection.md](reflection.md#reflection-is-read-only)):

```gdscript title=read_only_doc/temperature.spite
var _celsius = 0.0

func Temperature(starting_celsius: Float) {
    _celsius = starting_celsius
}

func get_fahrenheit(): Float {
    return _celsius * 9.0 / 5.0 + 32.0
}
```
```gdscript title=read_only_doc/read_only_doc.spite entry
var console = Console()

func ReadOnlyDoc() {
    var boiling = Temperature(100.0)
    console.print(boiling.fahrenheit)
}
```
```output
212
```

```gdscript title=read_only_error/temperature.spite
var _celsius = 0.0

func get_fahrenheit(): Float {
    return _celsius * 9.0 / 5.0 + 32.0
}
```
```gdscript title=read_only_error/read_only_error.spite entry error
func ReadOnlyError() {
    var boiling = Temperature()
    boiling.fahrenheit = 50.0
}
```
```diagnostic
'fahrenheit' is read-only
```

## Use the operator, not its function

When a class offers an operator for a function, **the operator is the only way to call it**. Writing the
function's name instead is a compile error that names the shortcut, so every program says the same thing the same
way and a reader never wonders whether `get_x()` and `.x` differ:

| Written | Error: write instead |
|---|---|
| `point.get_x()` | `point.x` |
| `point.set_x(4)` | `point.x = 4` |
| `shelf.get_at(0)` / `shelf.set_at(0, book)` | `shelf[0]` / `shelf[0] = book` |
| `names.get("ann")` / `names.set("ann", 3)` on a `Dictionary` | `names["ann"]` / `names["ann"] = 3` |
| `a.sum(b)`, `a.subtract(b)`, `a.multiply(b)`, `a.divide(b)`, `a.remainder(b)` | `a + b`, `a - b`, `a * b`, `a / b`, `a % b` |
| `a.equals(b)` / `not a.equals(b)` | `a == b` / `a != b` |
| `a.less_than(b)` / `a.greater_than(b)` | `a < b` / `a > b` |
| `a.negate()` | `-a` |

```gdscript title=operator_by_name/money.spite
var cents = 0

func Money(starting_cents: Integer) {
    cents = starting_cents
}

func sum(other: Money): Money {
    return Money(cents + other.cents)
}
```
```gdscript title=operator_by_name/operator_by_name.spite entry error
var console = Console()

func OperatorByName() {
    var wallet = Money(150)
    var found = Money(50)
    var total = wallet.sum(found)
    console.print(total.cents)
}
```
```diagnostic
'sum' is what '+' calls: write 'wallet + found'
```

**The one exception is the function used as a value**, where no operator can stand in: `run_callback(point.get_x)`
hands over the function itself, so it is written by name. Declaring the function stays as it is: `func
get_x()`, `func sum(other)`, `func get_at(index)` are how a class offers the operator. **After a `.`, a word is
always a member name**, never a keyword, so a getter named after a keyword is read like any other: `get_type()` is
read as `thing.type`.

## One name, one function

There is no overloading: one name is one function. An argument is cast to its parameter's type by the ordinary
right-to-left rule ([values_and_types.md](values_and_types.md#numeric-types-and-the-casting-rule)), so a
function that takes a `Float` takes an `Integer` too:

```gdscript title=argument_casting/argument_casting.spite entry
var console = Console()

func ArgumentCasting() {
    var whole = 7
    var halved = half_of(whole)
    console.print(halved)
}

func half_of(value: Float): Float {
    return value / 2.0
}
```
```output
3.5
```

A class that wants to accept several kinds of value takes a `type` or a union and says so in one signature.
A parameter that is never read is an error, as every unused name is: remove it, or name it `_name` when the
signature needs it. An operator function's, a template's or a setter's parameter needs neither, since its
signature is dictated from outside ([style.md](style.md#nothing-unused)).

## Functions the compiler calls by name

Some functions are never called by the program's own code: the compiler calls them itself, so their signatures are
fixed. `drop()` runs when the last reference to an object goes ([memory.md](memory.md)), `to_string()` wherever an
object is used as text, `to_debug()` wherever it is shown for debugging, and `equals`, `less_than` and
`greater_than` for `==`, `<` and `>`. Declaring one with another signature is an error that writes the one it must
have, since a `drop` with a parameter could never run and an `equals` with two would leave `==` comparing identity:

| Function | Signature |
|---|---|
| `drop` | `func drop()` |
| `to_string` | `func to_string(): String` |
| `to_debug` | `func to_debug(): String` |
| `equals` | `func equals(other: Class): Boolean` |
| `less_than` | `func less_than(other: Class): Boolean` |
| `greater_than` | `func greater_than(other: Class): Boolean` |

```gdscript title=drop_with_parameter/handle.spite
var open = true

func drop(_reason: Integer) {
    open = false
}
```
```gdscript title=drop_with_parameter/drop_with_parameter.spite entry error
var console = Console()

func DropWithParameter() {
    var handle = Handle()
    console.print(handle.open)
}
```
```diagnostic
'drop' is called by the compiler when the last reference to an object goes, so it is declared exactly 'func drop()'
```

---

Next: [Control flow](control_flow.md), the conditions, loops and switches that steer a program.
