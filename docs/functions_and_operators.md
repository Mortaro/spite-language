# Functions and operators

```gdscript
func function_name(first: Reference, second: Value): Tiny {
    return 1
}
```

The return type follows a colon, `return` is always written out, and a function with no return type returns
nothing ([the rules](#functions)). Forgetting the colon is a parse error that shows the form:

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
([what it costs](#functions-are-values-always-bound-to-an-instance)). A value that holds
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

**A getter with no setter makes a read-only attribute** ([the rule](#operators)). That is how
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
| `names.get("ann")` on a `Dictionary` | `names["ann"]` |
| `a.sum(b)`, `a.subtract(b)`, `a.multiply(b)`, `a.divide(b)`, `a.remainder(b)` | `a + b`, `a - b`, `a * b`, `a / b`, `a % b` |
| `a.equals(b)` / `not a.equals(b)` | `a == b` / `a != b` |
| `a.less_than(b)` / `a.greater_than(b)` | `a < b` / `a > b` |
| `a.negate()` | `-a` |

```gdscript
var total = wallet.sum(found)
# error: 'sum' is what '+' calls: write 'wallet + found'
```

**The one exception is the function used as a value**, where no operator can stand in: `run_callback(point.get_x)`
hands over the function itself, so it is written by name. Declaring the function stays as it is: `func
get_x()`, `func sum(other)`, `func get_at(index)` are how a class offers the operator.

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

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is compiled. Where the teaching above and these rules
disagree, the rules win.

### Functions

```gdscript
func function_name(first: Reference, second: Value): Tiny {
    return 1
}
```

- The return type is written `(): Type`. No other form is valid: `() Type` is the parse error "a return type is
  written '(): Type'".
- `drop`, `to_string`, `to_debug`, `equals`, `less_than` and `greater_than` are called by the compiler, so a class
  declares them only with the signatures [above](#functions-the-compiler-calls-by-name) (their parameter's type is
  not checked, only how many there are and what they answer); anything else is "'<name>' is called by the compiler
  <when>, so it is declared exactly '<signature>'" (`diagnostics/reserved_signature`).
- `return` is always explicit. There is no implicit return of the last expression. A function with a
  return type whose body falls off the end without a `return` gets the defensive default return of its
  return type, with no diagnostic.
- A function with no return type returns nothing.
- **Parameters**: a scalar (every numeric type, `Boolean`, an enum value) is passed by value, copied.
  Everything else (a class instance, `List<T>`, `Dictionary<T>`, `String`, a union, an object literal) is
  passed by reference: the caller writes nothing special, and the callee shares the exact same object (mutating
  it through the parameter is visible to the caller). There are no value classes: a copy is always
  explicit, `copy()`/`deep_copy()` ([Memory](memory.md#the-memory-model)), and passing a copy by value where
  that is cheaper is the compiler's business. `&Type` is not syntax; writing it is a parse error saying so,
  "Spite has no '&Type': every class, list and text is passed by reference already, so write the type alone"
  (`diagnostics/old_reference_type`; a `&` right after `:`, `<`, `(` or `,` and right before a capital letter is
  read as a type, any other lone `&` as a bit operator, [which Spite writes as a function](values_and_types.md#bitwise-functions)).
- An argument casts toward its parameter's type by the ordinary right-to-left rule
  ([Casting](values_and_types.md#casting)). There is no overloading: one name is one function, and a file that
  declares a name twice is an error, "'Shop' declares 'twice' twice: a later file may reopen a class and replace
  a function, but one file declares each name once".
- `assert condition` is a production feature, not a debug one: when the condition is false the function stops and
  answers "nothing" (returns, `null`, or an empty collection), so it is allowed only in a function whose result
  can say that; anywhere else the answer is written down, or the result made a `T?`
  ([`assert` is control flow](failure.md#assert-is-control-flow)).

```gdscript
func sum_positives(a: Float, b: Float): Float? {
    assert a > 0 and b > 0
    return a + b
}
```

### Functions are values, always bound to an instance

A function is a first-class value. Naming one inside a class passes it
together with the instance doing the passing, so it runs exactly as that instance would have run it:

```gdscript
logger.log(pretty_print)         # passes this instance's pretty_print
```

- **A variable never shadows a function name it can see**: functions are passed around by reference, so a
  name that meant both would be confusing. A local, parameter or attribute named like a function of its
  class (declared in its file or a reopening) is an error: "the variable 'file_stem' has the name of a
  function of this class, and a function is a value passed by its name: give the variable a name of its own"
  (`the attribute` and `the parameter` for the others). What is visible is read as the class's own functions,
  since those are the ones a bare name reaches; it is checked over every class
  file, generic ones, `library/` and functions nothing calls included (`diagnostics/shadowed_function`).
  Compile time only.
- **There are no free functions and no closures.** A function value is `{instance, function}`: one retain in a
  reference-counted language ([Memory](memory.md#the-memory-model)), capturing the receiver and nothing else, so no local ever escapes its
  scope. That is the whole of the feature; there is no environment to capture and no lifetime to reason about.
- **A class has no function values.** `Monster.is_alive` names nothing: `Monster` written as a value is the
  class's `Spite.Class` object ([Reflection](reflection.md)), which has no `is_alive` (the function itself is
  `Monster.functions['is_alive']`). It is the error "'Monster.is_alive' is not a function: 'Monster' is the
  class's 'Spite.Class' object, which has no 'is_alive' (the function itself is
  'Monster.functions['is_alive']'). A function value belongs to an instance: pass 'monster.is_alive', or ask
  each element of a list through its member template, such as 'filter_is_alive()'" (`diagnostics/passed_functions`).
  At run time a function value is one small object (the owner, retained, and a call pointer) and a call through
  it is one indirect call; a function passed by name to `each`, `filter` and the other list functions
  builds no value at all, because the element loop is instantiated for that function and calls it
  directly ([Passing a function for each element](collections.md#passing-a-function-for-each-element)). A
  program that uses no function value carries none of this.
- **It makes foreign callbacks expressible.** `{instance, function}` is precisely what a C callback plus its
  `void*` user data wants: a `ForeignCallback` hands C the function and gets the owner back from the user data
  ([Calling back into Spite](foreign_libraries.md#calling-back-into-spite)).
- **An event handler can be the function itself**, `onclick: increment`, checked by signature, rather than
  the symbol `'increment'` ([Types](values_and_types.md#types)) resolved by name. Symbol literals stay useful elsewhere.

**A function value is an instance of `Spite.Function`** generic over its
arguments and its return, so the type of a function is written the same way any other generic type is:

```gdscript
func log(printer: Spite.Function<String, Boolean>)
```

The codegen values are positional and the **last one is the return**; everything before it is an argument, in
order. `Spite.Function<Boolean>` takes nothing and returns a `Boolean`; `Spite.Function<String, Integer, Boolean>` takes a
`String` and an `Integer` and returns a `Boolean`. They line up with `Spite.Function`'s own reflection members
([Reflection objects](reflection.md#reflection-objects)): the arguments are `.arguments`, the last is `.returns`. So there is nothing new to learn:
the type of a function is its reflection, written down.

That is the point of the choice: **the value and its reflection are the same class.** `Weapon.functions[0]` and
a `pretty_print` passed as an argument are both `Spite.Function` instances, so reflection stops being a mirror
of the language held alongside it and becomes the language describing itself. That is what "everything is a
class, including classes" ([Reflection objects](reflection.md#reflection-objects)) has always claimed. A function value carries `.name`, `.arguments` and
`.returns` because it *is* the reflection object.

This is the language's first **variadic** generic. `List<T>` and the rest take a fixed count declared by their
`generic` lines ([Codegen values (`$`)](metaprogramming.md#codegen-values-)); `Spite.Function` is a compiler built-in and takes as many as the signature has.

#### Calling one

**A `Spite.Function` knows its owner, and every call goes through
`call_function()`.**

- `.owner` is the instance the function is bound to. The `{instance, function}` pair is not a hidden
  representation but an ordinary field you can read: `pretty_print.owner` is the instance that will
  run it. Bound-ness is reflectable, like everything else about a function.
- **`call_function()` is the one path.** Writing `printer(value)` is `printer.call_function(value)`, which is
  [the operator rule](#operators) applied once more: the call operator maps to a named function exactly as `+` maps
  to `sum` and `a[x]` maps to `get_at`. So there is one chokepoint every invocation of a function value passes
  through: an event handler firing, a foreign callback arriving from C ([Calling back into Spite](foreign_libraries.md#calling-back-into-spite): its trampoline calls the value on its owner), a framework dispatching.
  One place to instrument, and one place where the owner is applied.
- Because `Spite.Function` is an ordinary standard library class, `call_function` can be reopened ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading))
  to trace or count every callback in a program. That is the foot, and it is yours to shoot.

The indirection is paid only where it was already accepted. An ordinary call, `person.set_age(2)`, is a
direct call and never builds a `Spite.Function`; only a function used *as a value* goes through
`call_function()`. A function passed *by name* to a list's
`each`, `filter` and the rest is not a value either: the element loop is instantiated for it and calls it
directly, so it does not pass through `call_function()`; one held in a variable does.

**`Nothing` is the class a function returns when it returns nothing.**
It is required rather than optional: with the return in the last position, `Spite.Function<String>` already
reads as "takes nothing, returns a `String`", so a function taking a `String` and returning nothing could not
otherwise be written. `Spite.Function<Nothing>` takes nothing and returns nothing.

Naming a function without calling it (`greet` inside the class, `person.greet`
on a value) builds a `Spite.Function` bound to that instance, whose type carries the signature; `change(text)`
and `change.call_function(text)` call it with their arguments checked; a typed value can go where a plain
`Spite.Function` (a reflected one) is expected, but not the other way round, since a reflected function's
signature is not known to the type checker (`diagnostics/function_value_mistakes`). In C it is one
`Spite_Function` with a typed call pointer beside the owner, so a function value *is* its reflection object
(`conformance/stage6/typed_functions`). An attribute
may hold a function value (`var on_press: Spite.Function<Nothing>? = null`, set later); one whose default names
its own class's function binds it to the object itself, which is a cycle and leaks unless one side is cleared
([Memory](memory.md#the-memory-model)).

`Nothing` is also what `.returns` reports for every function declared without a return type, so it is not a
notation invented for the type syntax: it fills a hole the reflection already had. A function whose body falls
off the end still returns nothing in the ordinary sense ([Functions](#functions)); `Nothing` is how that is *named* when a
signature or a reflection object has to say it.

**`Anything` is a built-in `type`, the counterpart of `Nothing`** 
The library declares `type Anything { }` once (`library/nothing.spite`, beside `Nothing`), so a parameter or a
list that accepts any object is written `component: Anything` or `List<Anything>()`, and a program no longer
declares an empty `type` of its own for that. An empty `type` requires nothing, so every class fits it. A
function taking `Anything` is compiled once for each class that reaches it
([values_and_types.md](values_and_types.md#inline-types-and-duck-typing)), so a number, `Boolean` or enum passed
to it arrives as itself; one stored as `Anything` (in a `List<Anything>`, say) keeps its class beside it, as any
plain value stored as a `type` does, and `if component == Health` narrows it back. `Spite.Attribute.value`, the instance an attribute holds, is typed `Anything?`
([Reflection objects](reflection.md#reflection-objects)). An empty `type` costs nothing until a value is
passed to it: it holds no functions, and nothing is allocated for a number, `Boolean` or enum value stored
as `Anything`.

### Variadic arguments: the rules

Variadic arguments are mapped to a generic list. The last parameter may be written with `...`, and
the caller then writes its values one by one; the function receives them as an ordinary `List`:

```gdscript
func log(...lines: List<String>) {
    var joined = lines.join(" ")
    console.print(joined)
}

log("a", "b", "c")        # lines is ["a", "b", "c"]
log()                     # lines is empty
```

- **The element type may be a class or a `type`** ([Types](values_and_types.md#types)). With a `type`, each argument is whatever class
  fits the shape, and a call on an element is dispatched to the class it really is, exactly as a `List<Shape>`
  already dispatches: `func announce(...things: List<Describable>)` takes `announce(gear, gadget)`, a `Widget` and a `Gadget` made on lines of their own.
- **Only the last parameter**, and it must be written `List<Type>`: anything else is a parse error naming the
  form (`diagnostics/variadic_not_last`). Parameters before it are ordinary and positional.
- Each value is an argument in every sense: it casts toward the element type like any argument ([Variables and values](values_and_types.md#variables-and-values)), and,
  like any argument, it is never a call or a construction (`diagnostics/variadic_mistakes`).
- A function value keeps the spread: naming `log` and calling the value with `("x", "y")` gathers them the same
  way (`conformance/stage6/variadic_arguments`).

**Passing a whole list to a `...` parameter is an error** that
says to pass the elements ("this call passes a whole List<String>, but the last parameter is written with
'...', so it takes the values one by one: pass the elements themselves, separated by commas"), because the
caller writes them one by one and there is no spread operator to ask for the other reading, unless the element
type is itself a list. Zero values is a legal call, giving an empty list. A `...` not on the last parameter is
"'...lines' takes every argument from here on, so it is the last parameter: move it to the end", and one not
typed `List<Type>` is "'...words' receives the arguments as a list: write '...words: List<Type>'". At run time
each call builds the `List` its function receives, as the caller would have; nothing else is added.
`Console.print`, `write` and `error` are ordinary variadic functions taking `...values: List<Printable>`
([System classes](standard_library.md#system-classes)).

**A number, a `Boolean` or an enum value fits a `type` too** A
shape holds class instances, and these are plain values. Passed to a function that takes a `type`, one reaches the
copy of the function made for its class as itself; stored where a `type` is wanted (a list's element, an
attribute, the list of a variadic call such as `console.print`), it is held beside its class's tag with nothing
allocated, and a call through the shape reaches its class's function (`Integer.to_string()`, for an `Integer`).
Text and a `Symbol` travel in a small box the compiler allocates and frees like any object, so that a `Symbol`
keeps its class. An enum value answers two functions, `to_string()` (its name as text; `weather.to_string()` works on
any enum value) and `to_debug()`, so that is all a shape can ask of one. `.class` read through the shape names the
value's own class (`Integer`, `Symbol`, the enum), as it does for a class instance. A value of a union passed where a
`type` is wanted brings the union's members into the shape.

### Operators

Every operator is a shortcut for a function, which a class can define to support that operator.

| Operator | Function | Notes |
|---|---|---|
| `a + b` | `sum(b)` | |
| `a - b` | `subtract(b)` | |
| `a * b` | `multiply(b)` | |
| `a / b` | `divide(b)` | on whole numbers, a zero divisor halts ([values_and_types.md](values_and_types.md)) |
| `a % b` | `remainder(b)` | the same |
| `a == b` | `equals(b)` | |
| `a != b` | negated `equals(b)` | |
| `a < b` | `less_than(b)` | |
| `a > b` | `greater_than(b)` | |
| `a <= b` | `less_than(b) or equals(b)` | derived, not its own function |
| `a >= b` | `greater_than(b) or equals(b)` | derived, not its own function |
| unary `-a` | `negate()` | |
| `not a` | none | stays a built-in keyword, never a function |
| `a and b` / `a or b` | none | stay built-in, short-circuiting |
| `a[x]` | `get_at(x)` | answers a `T?` |
| `a[x] = v` | `set_at(x, v)` | |

For `Integer`/`Float`/`Boolean`/enum/`String`/`List<T>`/`Dictionary<T>` these are intrinsic (they compile to exactly
the same C as before this table existed).

**A direct call of an operator's function is an error naming the operator**: `a.sum(b)` is "write 'a + b'", and the same for every row of the table above, for `get_<name>()`
and `set_<name>(value)` (write `.name` and `.name = value`), and for a `Dictionary`'s `get(key)` (write
`[key]`). It holds for every class (built-in, library or the program's own) wherever the operator could be
written instead. The only exception is the function used as a value (`run_callback(point.get_x)`,
[Functions are values](#functions-are-values)), since no operator can be passed. A call with no receiver inside the class's own functions (`get_x()`) is not covered, since a bare `x` there is
the raw field and no shortcut reaches the getter; nor is `get_attribute(attribute)`/`set_attribute`, which
Symbol codegen offers per attribute rather than as one operator. This also settles `get_x()` against `.x`: there
is one spelling.

For a user class or union, `a + b` compiles to `a.sum(b)` when the class defines (an exact function, or Symbol
codegen answers) `sum`; a union duck-types the same way a method call already does (every member must define
the function with the same signature). Missing the function is an error naming it: "this operator on a 'Money'
needs it to define 'sum(other)'", "'-' on a 'Money' needs it to define 'negate()'", "indexing a 'Money' needs it
to define 'get_at(index)'", "assigning through an index on a 'Money' needs it to define 'set_at(index,
value)'". The right side of a binary operator still casts toward the parameter type, exactly like an ordinary
function call argument.

**`[]` is `get_at`, and `get_at` answers `T?`**, because `[]` is just a
shortcut for a function, so a moron can also implement indexable classes. Every `get_at` (the library's `List`,
`Vector` and `Items`, and a program's own) is declared to answer a `T?`, `null` when nothing is at the index; one
that answers a plain `T` is an error when it is declared: `'get_at' answers 'Integer', but it is what '[ ]'
calls, and every '[ ]' answers a 'T?' that is narrowed before use: declare it to answer 'Integer?', and answer null
when nothing is at the index` (`diagnostics/plain_get_at`). A read is narrowed like any `T?` path
([failure.md](failure.md#reading-with--answers-t)); the compiler's own proofs (loop bounds, counts, counted loops)
cover the library's collections, and a program's own indexable class is narrowed by what the program writes
(`conformance/stage6/indexable_class`, `diagnostics/indexable_unproven`). A `Dictionary`'s `[]` is its `get(key)`,
which already answers `T?`. `set_at` is unchanged: it answers nothing. On numbers, arithmetic is done in the left operand's type, and a right operand wider
than the left is an error ([Wider arithmetic goes wider operand first](values_and_types.md#wider-arithmetic-goes-wider-operand-first)); a
comparison is not arithmetic and still casts its right side toward the left.

At run time an operator on a built-in type is the operation itself, and one on a user class is a direct call
of its function, resolved while compiling; nothing is looked up when the program runs.

Attribute access from *outside* a class also goes through operator functions: `person.age = 1` calls
`person.set_age(1)` when the class defines or Symbol-codegens `set_age` (an exact function always wins over
Symbol codegen), otherwise it assigns the field directly. `person.age` (a read) calls `person.get_age()` the
same way when defined. The read
mirrors the write. Every function return is a properly retained, independent reference
([Memory](memory.md#the-memory-model)), so a getter that computes and hands back a fresh value is exactly as
safe to intercept through as one that just returns the field itself. Calling a getter or setter by name (`person.get_full_name()`) is refused: write `person.full_name`. Compound assignment through interception works both ways: `person.age = person.age + 1`
reads through `get_age()` and writes through `set_age(...)`. Inside a class's own functions, a bare `age = 1`
(no receiver) and `attributes[attribute]` stay raw: interception only applies to `receiver.field` written
from outside. Which reads and writes are intercepted is decided while compiling: an intercepted one is a direct
call of the getter or setter, and every other one a plain field access.

**A getter with no setter makes a read-only attribute**: a read of `.name` that finds no attribute but finds
`get_name()` reads through it, and a write finds no `set_name()` and is an error. **A setter answers a write
with no attribute of its name** `ticket.id = 4` calls
the class's own `set_id(value)` beside a private `_id`, as `get_id()` answers the read; only a function the class
declares, never one Symbol codegen would write for an attribute that does not exist (`tests/interception_tests`). The
read-only error is "'number' is read-only: 'Badge'
answers it through get_number() and has no set_number(value)" (`diagnostics/read_only_attribute`,
`tests/interception_tests`). Reflection's own members are kept read-only
this way ([Reflection is read-only](reflection.md#reflection-is-read-only)).

---

Next: [Control flow](control_flow.md), the conditions, loops and switches that steer a program.
