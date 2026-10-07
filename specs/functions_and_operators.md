# Functions and operators

The specification of [Functions and operators](../docs/functions_and_operators.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Functions

```gdscript
func function_name(first: Reference, second: Value): Tiny {
    return 1
}
```

- The return type is written `(): Type`. No other form is valid: `() Type` is the parse error "a return type is
  written '(): Type'".
- `drop`, `to_string`, `to_debug`, `equals`, `less_than` and `greater_than` are called by the compiler, so a class
  declares them only with the signatures [on the page](../docs/functions_and_operators.md#functions-the-compiler-calls-by-name) (their parameter's type is
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
  read as a type, any other lone `&` as a bit operator, [which Spite writes as a function](../docs/values_and_types.md#bitwise-functions)).
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

## Functions are values, always bound to an instance

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
  class's `Spite.Class` object ([Reflection](../docs/reflection.md)), which has no `is_alive` (the function itself is
  `Monster.functions['is_alive']`). It is the error "'Monster.is_alive' is not a function: 'Monster' is the
  class's 'Spite.Class' object, which has no 'is_alive' (the function itself is
  'Monster.functions['is_alive']'). A function value belongs to an instance: pass 'monster.is_alive', or ask
  each element of a list through its member template, such as 'filter_is_alive()'" (`diagnostics/passed_functions`).
  At run time a function value is one small object (the owner, retained, and a call pointer) and a call through
  it is one indirect call. A text's or a number's function (`var check = greeting.contains`, `var below =
  limit.minimum`) is bound to a copy of the value, kept in a box of its own, since a value has no count to
  retain: one more allocation when the value is made, and a forwarding call that takes the value out of the box; a function passed by name to `each`, `filter` and the other list functions
  builds no value at all, because the element loop is instantiated for that function and calls it
  directly ([Passing a function for each element](../docs/collections.md#passing-a-function-for-each-element)). A
  program that uses no function value carries none of this.
- **It makes foreign callbacks expressible.** `{instance, function}` is precisely what a C callback plus its
  `void*` user data wants: a `ForeignCallback` hands C the function and gets the owner back from the user data
  ([Calling back into Spite](../docs/foreign_libraries.md#calling-back-into-spite)).
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

### Calling one

**A `Spite.Function` knows its owner, and every call goes through
`call_function()`.**

- `.owner` is the instance the function is bound to. The `{instance, function}` pair is not a hidden
  representation but an ordinary field you can read: `pretty_print.owner` is the instance that will
  run it. Bound-ness is reflectable, like everything else about a function.
- **`call_function()` is the one path.** Writing `printer(value)` is `printer.call_function(value)`, which is
  [the operator rule](#operators) applied once more: the call operator maps to a named function exactly as `+` maps
  to `sum` and `a[x]` maps to `get_at`. So there is one chokepoint every invocation of a function value passes
  through: an event handler firing, a foreign callback arriving from C ([Calling back into Spite](../docs/foreign_libraries.md#calling-back-into-spite): its trampoline calls the value on its owner), a framework dispatching.
  One place to instrument, and one place where the owner is applied.
- Because `Spite.Function` is an ordinary standard library class, `call_function` can be reopened ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading))
  to trace or count every callback in a program. That is the foot, and it is yours to shoot.

The indirection is paid only where it was already accepted. An ordinary call, `person.grow(2)`, is a
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
([values_and_types.md](../docs/values_and_types.md#inline-types-and-duck-typing)), so a number, `Boolean` or enum passed
to it arrives as itself; one stored as `Anything` (in a `List<Anything>`, say) keeps its class beside it, as any
plain value stored as a `type` does, and `if component == Health` narrows it back. `Spite.Attribute.value`, the instance an attribute holds, is typed `Anything?`
([Reflection objects](reflection.md#reflection-objects)). An empty `type` costs nothing until a value is
passed to it: it holds no functions, and nothing is allocated for a number, `Boolean` or enum value stored
as `Anything`.

## Variadic arguments: the rules

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

## Operators

Every operator is a shortcut for a function, which a class can define to support that operator.

| Operator | Function | Notes |
|---|---|---|
| `a + b` | `sum(b)` | |
| `a - b` | `subtract(b)` | |
| `a * b` | `multiply(b)` | |
| `a / b` | `divide(b)` | on whole numbers, a zero divisor halts ([values_and_types.md](../docs/values_and_types.md)) |
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
and `set_<name>(value)` (write `.name` and `.name = value`), and for `get_at` and `set_at` on a `List` and a
`Dictionary` alike (write `[key]` and `[key] = value`). It holds for every class (built-in, library or the program's own) wherever the operator could be
written instead. The only exception is the function used as a value (`run_callback(point.get_x)`,
[Functions are values](../docs/functions_and_operators.md#functions-are-values)), since no operator can be passed. A call with no receiver inside the class's own functions (`get_x()`) is not covered, since a bare `x` there is
the raw field and no shortcut reaches the getter; nor is `get_attribute(attribute)`/`set_attribute`, which
Symbol codegen offers per attribute rather than as one operator. This also settles `get_x()` against `.x`: there
is one spelling. A union receiver is covered when every member offers the operator. After a `.` a word is always
a member name and never a keyword, so a getter or setter named after a keyword is covered like any other
(`thing.type`, `thing.type = value`). An `equals` that `==` would not call (an `equals` whose parameter is not a
class, where `==` compares identity) is called by name.

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
([failure.md](../docs/failure.md#reading-with--answers-t)); the compiler's own proofs (loop bounds, counts, counted loops)
cover the library's collections, and a program's own indexable class is narrowed by what the program writes
(`conformance/stage6/indexable_class`, `diagnostics/indexable_unproven`). A `Dictionary`'s `[]` is its `get_at(key)`,
which already answers `T?`. `set_at` is unchanged: it answers nothing. On numbers, arithmetic is done in the left operand's type, and a right operand wider
than the left is an error ([Wider arithmetic goes wider operand first](../docs/values_and_types.md#wider-arithmetic-goes-wider-operand-first)); a
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
this way ([Reflection is read-only](../docs/reflection.md#reflection-is-read-only)).

---

Next: [Control flow](control_flow.md).
