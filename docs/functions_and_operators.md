# Functions and operators

```
func function_name(first: Reference, second: Value): Tiny {
    return 1
}
```

- The return type is written `(): Type` -- always with the colon. `() Type` is a parse error.
- `return` is always explicit. There is no implicit return of the last expression. A function that falls off
  the end without a `return` gets its return type's defensive default, with no diagnostic.
- A function with no return type returns nothing.

```spite title=missing_colon_error/missing_colon_error.spite entry error
func add_one(value: Int) Int {
    return value + 1
}
```
```diagnostic
a return type is written '(): Int'
```

## Functions are values

Naming a function without calling it gives a value: the function bound to the instance it was named on, so it
runs exactly as that instance would have run it. There are no free functions and no closures -- the value holds
its owner and nothing else. Its type is written like any generic, with the return last:
`Spite.Function<String, String>` takes a `String` and returns a `String`, and `Spite.Function<Nothing>` takes
nothing and returns nothing. Calling it is `change(text)` or `change.call_function(text)`, which are the same
call; and the value is also its own reflection, with `.name`, `.arguments` and `.returns`.

```spite title=function_values_doc/shouter.spite
func shout(text: String): String {
    return "{text.upper_case()}!"
}
```
```spite title=function_values_doc/function_values_doc.spite entry
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
side (see [memory.md](memory.md)).

## Variadic arguments

A last parameter written `...name: List<Type>` takes every remaining argument, written one by one at the call,
and the function receives them as an ordinary `List<Type>`. `Type` may be a class or a `type`: with a `type`,
each argument is any class that fits it, and a call on an element answers for the class it really is.

```spite title=variadic_doc/variadic_doc.spite entry
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
```spite title=variadic_doc/guest.spite
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

```spite title=variadic_not_a_list/variadic_not_a_list.spite entry error
func VariadicNotAList() {
    shout("a", "b")
}

func shout(...words: String) {
    var _count = 0
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

`not`/`and`/`or` stay built-in keywords, never functions. For `Int`/`Float`/`Bool`/enum/`String`/`List<T>`/
`Dictionary<T>` these compile to the same C as always; for a user class or union, the operator compiles to a
call of the matching function, and the right side still casts toward the parameter type like any other
argument:

```spite title=operators_as_functions/money.spite
var cents = 0

func Money(starting_cents: Int) {
    cents = starting_cents
}

func sum(other: Money): Money {
    return Money(cents + other.cents)
}

func equals(other: Money): Bool {
    return cents == other.cents
}

func greater_than(other: Money): Bool {
    return cents > other.cents
}
```
```spite title=operators_as_functions/operators_as_functions.spite entry
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

```spite title=operator_missing_error/operator_missing_error.spite entry error
var console = Console()
var cents = 0

func OperatorMissingError(starting_cents: Int) {
    cents = starting_cents
}

func use_it() {
    console.print(OperatorMissingError(1) + OperatorMissingError(2))
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

```spite title=interception/account.spite
var balance = 0
var owner = ""

func Account(starting_balance: Int, starting_owner: String) {
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

```spite title=interception/interception.spite entry
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

One restriction on the read half: if a getter's return type *owns* something (a `String`, `List<T>`, or a
class that owns one), reading `person.field` stays a raw field read even when a matching getter exists --
otherwise a getter that hands back a fresh owned value would leak, since a plain `.field` read is exactly what
every ownership-tracking part of the compiler expects not to need dropping. Call a String/List-returning getter
explicitly (`person.get_full_name()`) instead; that call is an ordinary method call and has no such
restriction. Inside a class's own functions, a bare `age = 1` (no receiver) always stays raw -- interception
only applies to `receiver.field` written from outside.
