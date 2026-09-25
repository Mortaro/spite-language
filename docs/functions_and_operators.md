# Functions and operators

```gdscript
func function_name(first: Reference, second: Value): Tiny {
    return 1
}
```

- The return type is written `(): Type` -- always with the colon. `() Type` is a parse error.
- `return` is always explicit. There is no implicit return of the last expression. A function that falls off
  the end without a `return` gets its return type's defensive default, with no diagnostic.
- A function with no return type returns nothing.

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
runs exactly as that instance would have run it. There are no free functions and no closures -- the value holds
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

A value that holds a function bound to the instance holding it is a cycle, and leaks like any other: clear one
side (see [memory.md](memory.md)). An ordinary call -- `shouter.shout("hi")` -- is a direct call and builds no
function value; only a function used *as a value* goes through `call_function()`. A function that returns
nothing returns `Nothing`, which is what its type and its `.returns` say.

**Not built yet:** reading a function value's `.owner`, calling a function found through reflection with
arguments, and an attribute whose default is a function value
([Calling one](#calling-one)).

**A variable never has the name of a function it can see** ([D172](decisions.md)). Since a function's
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
    greet(Guest("Aria"), Guest("Bram"))
}

func joined(separator: String, ...words: List<String>): String {
    return words.join(separator)
}

func greet(...visitors: List<Named>) {
    var index = 0
    while index < visitors.count() {
        console.print("hello {visitors[index].name}")
        index = index + 1
    }
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
passing a whole list to it is an error; a parameter written without `...` takes a list as it is.

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

`not`/`and`/`or` stay built-in keywords, never functions. For `Integer`/`Float`/`Boolean`/enum/`String`/`List<T>`/
`Dictionary<T>` these compile to the same C as always; for a user class or union, the operator compiles to a
call of the matching function, and the right side still casts toward the parameter type like any other
argument:

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
    var is_two_hundred = total.equals(Money(200))
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
    console.print(Money(1) + Money(2))
}
```
```diagnostic
needs it to define 'sum(other)'
```

## Setter/getter interception

Attribute access **from outside a class** also goes through operator-style functions. `person.age = 1` calls
`set_age(1)` when the class defines it (an exact function, or Symbol codegen -- see
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

Inside a class's own functions, a bare `age = 1` (no receiver) always stays a plain field write --
interception only applies to `receiver.field` written from outside. A getter's result is an ordinary retained
value, so a getter may compute and hand back a fresh `String` or `List` as safely as it returns a field.

**A getter with no setter makes a read-only attribute.** A read of `.name` that finds no attribute but finds
`get_name()` reads through it, and a write finds no `set_name()` and is an error calling the attribute
read-only. That is how reflection keeps `Spite.Class.name` from being overwritten
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
Parameters that are never read are an error, as every unused name is ([style.md](style.md#nothing-unused)), except
where the signature is dictated from outside: an operator function's, a template's, a setter's, and a function
that replaces one in a reopened class.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Functions  **[implemented]**

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
- **Parameters** (D1, decided by Mortaro 2026-09-19): a scalar (every numeric type, `Boolean`, an enum value) is
  passed by value, copied. Everything else -- a class instance, `List<T>`, `Dictionary<T>`, `String`, a union, an
  object literal -- is passed by reference: the caller writes nothing special, and the callee shares the exact
  same object (mutating it through the parameter is visible to the caller). `&Type` no longer exists as syntax --
  writing it is a parse error saying references are the default now. A copy is always explicit: `copy()`/
  `deep_copy()` ([Memory](memory.md#memory--implemented)).
- `assert condition` is a production feature, not a debug one: when the condition is falsey the function returns
  the default value of its return type immediately.

```gdscript
func sum_positives(a: Float, b: Float): Float {
    assert a > 0 and b > 0
    return a + b
}
```

### Functions are values, always bound to an instance  **[implemented, except `.owner`]**

D17 (decided by Mortaro, 2026-09-19): a function is a first-class value. Naming one inside a class passes it
together with the instance doing the passing, so it runs exactly as that instance would have run it:

```
console.log(pretty_print)        # passes this instance's pretty_print
```

- **A variable never shadows a function name it can see** (D172, decided by Mortaro, 2026-09-25): "as functions
  can be passed around by reference it would become super confusing." A local, parameter or attribute named like
  a function of its class -- declared in its file or a reopening -- is an error: "the variable 'file_stem' has
  the name of a function of this class, and a function is a value passed by its name: give the variable a name
  of its own" (`attribute` and `parameter` for the others). What is visible is read as the class's own functions,
  since those are the ones a bare name reaches (proposed by Claude, unconfirmed); it is checked over every class
  file, generic ones and unused functions included (`diagnostics/shadowed_function`). About 170 names in the
  compiler, `library/`, the corpus and `docs/` were renamed, most of them D77's `var string_type = string_type()`,
  now `var string_spite_type = string_type()`. **[implemented]**
- **There are no free functions and no closures.** A function value is `{instance, function}` -- one retain in a
  reference-counted language ([Memory](memory.md#memory--implemented)), capturing the receiver and nothing else, so no local ever escapes its
  scope. That is the whole of the feature; there is no environment to capture, no lifetime to reason about, and
  no allocation beyond the retain.
- **It makes foreign callbacks expressible.** `{instance, function}` is precisely what a C callback plus its
  `void*` user data wants, which is what [Not designed yet](foreign_libraries.md#not-designed-yet) lists as not designed yet.
- **An event handler can be the function itself** -- `onclick: increment` -- checked by signature, rather than
  the symbol `'increment'` ([Types](values_and_types.md#types)) resolved by name. Symbol literals stay useful elsewhere.

**A function value is an instance of `Spite.Function`** (D39, decided by Mortaro, 2026-09-20), generic over its
arguments and its return, so the type of a function is written the same way any other generic type is:

```gdscript
func log(printer: Spite.Function<String, Boolean>)
```

The codegen values are positional and the **last one is the return**; everything before it is an argument, in
order. `Spite.Function<Boolean>` takes nothing and returns a `Boolean`; `Spite.Function<String, Integer, Boolean>` takes a
`String` and an `Integer` and returns a `Boolean`. They line up with `Spite.Function`'s own reflection members
([Reflection objects](reflection.md#reflection-objects--partial), D12): the arguments are `.arguments`, the last is `.returns`. So there is nothing new to learn --
the type of a function is its reflection, written down.

That is the point of the choice: **the value and its reflection are the same class.** `Weapon.functions[0]` and
a `pretty_print` passed as an argument are both `Spite.Function` instances, so reflection stops being a mirror
of the language held alongside it and becomes the language describing itself -- which is what "everything is a
class, including classes" ([Reflection objects](reflection.md#reflection-objects--partial)) has always claimed. A function value carries `.name`, `.arguments` and
`.returns` because it *is* the reflection object.

This is the language's first **variadic** generic. `List<T>` and the rest take a fixed count declared by their
`generic` lines ([Codegen values (`$`)](metaprogramming.md#codegen-values---implemented)); `Spite.Function` is a compiler built-in and takes as many as the signature has.

#### Calling one

D40 (decided by Mortaro, 2026-09-20): **a `Spite.Function` knows its owner, and every call goes through
`call_function()`.**

- `.owner` is the instance the function is bound to. D17's `{instance, function}` pair stops being a hidden
  representation and becomes an ordinary field you can read: `pretty_print.owner` is the `Console` that will
  run it. Bound-ness is reflectable, like everything else about a function.
- **`call_function()` is the one path.** Writing `printer(value)` is `printer.call_function(value)`, which is
  [Operators](#operators--implemented)'s operator rule applied once more -- the call operator maps to a named function exactly as `+` maps
  to `sum` and `a[x]` maps to `get_at`. So there is one chokepoint every invocation of a function value passes
  through: an event handler firing, a foreign callback arriving from C ([Not designed yet](foreign_libraries.md#not-designed-yet)), a framework dispatching.
  One place to instrument, and one place where the owner is applied.
- Because `Spite.Function` is an ordinary standard library class, `call_function` can be reopened ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial),
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
off the end still returns nothing in the ordinary sense ([Functions](#functions--implemented)); `Nothing` is how that is *named* when a
signature or a reflection object has to say it.

**`Anything` is a built-in `type`, the counterpart of `Nothing`** (D163, decided by Mortaro).  **[implemented]**
The library declares `type Anything { }` once (`library/nothing.spite`, beside `Nothing`), so a parameter or a
list that accepts any object is written `component: Anything` or `List<Anything>()`, and a program no longer
declares an empty `type` of its own for that. An empty `type` requires nothing, so every class fits it, and a
number, `Boolean` or enum passed to it is boxed like any plain value passed to a `type` (D109); `if component ==
Health` narrows it back. `Spite.Attribute`'s object is typed `Anything?` ([Reflection objects](reflection.md#reflection-objects--partial)).

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

- **The element type may be a class or a `type`** ([Types](values_and_types.md#types)). With a `type`, each argument is whatever class
  fits the shape, and a call on an element is dispatched to the class it really is, exactly as a `List<Shape>`
  already dispatches: `func announce(...things: List<Describable>)` takes `announce(Widget("gear"), Gadget(3))`.
- **Only the last parameter**, and it must be written `List<Type>`: anything else is a parse error naming the
  form (`diagnostics/variadic_not_last`). Parameters before it are ordinary and positional.
- Each value is an argument in every sense: it casts toward the element type like any argument ([Variables and values](values_and_types.md#variables-and-values--implemented)), and
  D77 still applies to it, so only a constructor may be one, one level deep (`diagnostics/variadic_mistakes`).
- A function value keeps the spread: naming `log` and calling the value with `("x", "y")` gathers them the same
  way (`conformance/stage6/variadic_arguments`).

As implemented (proposed by Claude, unconfirmed): **passing a whole list to a `...` parameter is an error** that
says to pass the elements, because the caller writes them one by one and there is no spread operator to ask for
the other reading -- unless the element type is itself a list. Zero values is a legal call, giving an empty list.
`Console.print`, `write` and `error` are ordinary variadic functions taking `...values: List<Printable>` (D109,
[System classes](standard_library.md#system-classes--implemented)).

**A number, a `Boolean` or an enum value fits a `type` too** (proposed by Claude, unconfirmed; built for D109). A
shape holds class instances, and these are plain values, so passing one where a `type` is wanted puts it in a
small box the compiler allocates and frees like any object, and a call through the shape reaches its class's
function (`Integer.to_string()`, for an `Integer`). A `String` needs no box; a `Symbol` gets one so that it keeps its
class. An enum value answers two functions, `to_string()` (its name as text -- `weather.to_string()` works on
any enum value) and `to_debug()`, so that is all a shape can ask of one. `.class` read through the shape names the
value's own class (`Integer`, `Symbol`, the enum), as it does for a class instance. A value of a union passed where a
`type` is wanted brings the union's members into the shape.

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

For `Integer`/`Float`/`Boolean`/enum/`String`/`List<T>`/`Dictionary<T>` these are intrinsic (they compile to exactly
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
same way when defined -- **(proposed by Claude, unconfirmed: the decision covered only the setter half; this
read half mirrors it)**. D1 removes the earlier restriction that skipped interception for a getter returning
an owning type: every function/method return is now a properly retained, independent reference ([Memory](memory.md#memory--implemented)),
so a getter that computes and hands back a fresh value is exactly as safe to intercept through as one that
just returns the field itself (this also fixes the `get_<attribute>()` double-free that used to be listed in
`docs/KNOWN_ISSUES.md`). A getter written and called explicitly (`person.get_full_name()`) was always just an
ordinary method call. Compound assignment through interception works both ways: `person.age = person.age + 1`
reads through `get_age()` and writes through `set_age(...)`. Inside a class's own functions, a bare `age = 1`
(no receiver) and `attributes[attribute]` stay raw -- interception only applies to `receiver.field` written
from outside.
