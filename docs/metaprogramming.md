# Metaprogramming

Everything is a class, including classes. There are no macros -- Spite's answer to "generate code for every
attribute" is `Symbol` codegen, and its answer to "inspect a value at runtime" is a small set of `Spite.*`
reflection types. Both are resolved at compile time wherever possible.

## Symbol codegen, step by step

A parameter of type `Symbol` whose name is a segment of its own function's name turns that function into a
*template*: it answers every call whose name fits the pattern, one per attribute. `set_attribute`/
`get_attribute` (with a `Symbol` parameter named `attribute`) answer `set_age`/`get_age`, `set_name`/
`get_name`, and so on, for each field of the class -- generated only for the names actually called.

Inside the template, the `Symbol` names that attribute two different ways: written as a **type**
(`value: attribute.class`, `): attribute.class`), it *is* the attribute's real type; written as an
**expression**, `attribute.class` is a `Spite.Class` naming that type (it prints just like the type name
would). `attributes[attribute]` is a separate, compile-time-only form: that same field, indexed by the
`Symbol`.

```spite title=symbol_codegen/person.spite
var age = 0
var name = ""

func Person(new_age: Int, new_name: String) {
    age = new_age
    name = new_name
}

# `age` (Int) is intercepted through this pair of templates: calling
# set_age/get_age from outside this class instantiates set_attribute/
# get_attribute with attribute bound to the 'age' Symbol.
func set_attribute(attribute: Symbol, value: attribute.class) {
    attributes[attribute] = value
}

func get_attribute(attribute: Symbol): attribute.class {
    return attributes[attribute]
}

# An exact function always wins over the templates above, for this one
# name only -- writing 'name' (person.name = ...) always goes through this
# exact set_name, never the set_attribute template. Reading it back still
# goes through the get_attribute template below, since no exact get_name
# exists.
func set_name(new_name: String) {
    name = new_name.upper()
}
```
```spite title=symbol_codegen/symbol_codegen.spite entry
var console = Console()

func SymbolCodegen() {
    var person = Person(20, "ann")

    # No exact set_age/get_age exists, so set_attribute/get_attribute
    # answer for "age": this call is set_age(...), generated on demand.
    person.set_age(person.get_age() + 1)
    console.print("age", person.age)

    # set_name exists exactly, so it wins over the template for the write.
    # The read (person.name) still has no exact get_name, so it goes
    # through the get_attribute template, generating one on demand -- safe
    # for a String/List<T>/Dictionary<T> attribute too, since milestone 9a
    # (reference counting, manual.md section 10): every function/method
    # return is a properly retained, independent reference now, so a
    # generated getter returning an owning attribute is exactly as safe as
    # one returning a scalar.
    person.set_name("bea")
    console.print("name", person.name)
}
```
```output
age 21
name BEA
```

Before milestone 9a's reference-counting model, generating (or writing) a `get_<attribute>()` for an owning
attribute (`String`/`List<T>`/`Dictionary<T>`) and calling it -- including implicitly, the way `person.name`
above does -- double-freed at runtime; see [KNOWN_ISSUES.md](KNOWN_ISSUES.md) item 3 for that history. It is
fixed now: scalar/enum/owning attributes all intercept safely in both directions.

## Reflection: `Spite.Class` and `Spite.Attribute`

`value.class` is a `Spite.Class` (`.name`, `.namespace`); printing one prints just its `.name`. `value.attributes`
is a real, runtime `List<Spite.Attribute>` (`.name`, `.class`, `.value` -- all `String`), built only for a class
that actually uses `.attributes`:

```spite title=reflection_basics/gadget.spite
var name = ""
var power = 0

func Gadget(new_name: String, new_power: Int) {
    name = new_name
    power = new_power
}
```
```spite title=reflection_basics/reflection_basics.spite entry
var console = Console()

func describe(gadget: Gadget) {
    console.print("class", gadget.class)
    console.print("class name", gadget.class.name)

    var attributes = gadget.attributes
    var index = 0
    while index < attributes.count() {
        var attribute = attributes[index]
        console.print(attribute.name, attribute.class, attribute.value)
        index = index + 1
    }
}

func ReflectionBasics() {
    describe(Gadget("wrench", 3))
}
```
```output
class Gadget
class name Gadget
name String wrench
power Int 3
```

`attributes[symbol]` inside a class (used by Symbol codegen above) and `.attributes` from outside are
different things reading the same data: the former is compile-time only, indexed by a `Symbol`; the latter is
an ordinary runtime `List<T>`, walked with `while` like any other list.

`Person.instances` (every live instance of a class) is **[planned]**, not implemented yet -- using it reports a
"not supported yet" diagnostic.

## Generics and codegen values (`$`)

`$name` means "replaced at code generation." Two sources feed it: `generics`, declared on a file's first line
and given positionally at the call site, and compiler flags (`--name=value`), which set `$name` for the whole
program.

```spite title=generics_basics/pair.spite
generics $left_type, $right_type

var left: $left_type = null
var right: $right_type = null

func Pair(new_left: $left_type, new_right: $right_type) {
    left = new_left
    right = new_right
}

func describe(): String {
    return left + " and " + right
}
```
```spite title=generics_basics/generics_basics.spite entry
var console = Console()

func GenericsBasics() {
    var scoreboard = Pair<String, Int>("Aria", 42)
    console.print(scoreboard.describe())
}
```
```output
Aria and 42
```

`null` on a `$generic`-typed field means that generic's bound type's default value, not literal `Nullable<T>`
-- provisional, see manual.md open question 1.

## Compiler flags and tree shaking

```spite title=compiler_flags/compiler_flags.spite entry vars=environment:production
var console = Console()

func CompilerFlags() {
    if $environment == "production" {
        console.print("using the production endpoint")
    } else {
        console.print("using the local endpoint")
    }
}
```
```output
using the production endpoint
```

Conditions on codegen values (both `generics` and `--name=value` flags) are decided **at compile time**, and
the untaken branch is removed from the generated C entirely -- this is tree shaking, not just dead-code
elimination at the C compiler level. Compiling the program above with `--environment=production --emit-c`
shows the string `"using the local endpoint"` nowhere in the generated C; compiling with
`--environment=staging` instead flips which branch survives and prints `using the local endpoint`. A
`--name=value` flag that matches no `$name` anywhere in the program is a compile error, so a typo in the flag
name is caught immediately rather than silently doing nothing.

`--development` disables all of this: every codegen-value condition becomes a real runtime `if` (both branches
compiled in, so live reload can flip the flag without recompiling), and every class in the folder is emitted
instead of only the ones reachable from the entry class. It trades binary size and tree-shaking for the
ability to swap `$name` values while the program keeps running -- reach for it during development, drop it for
a release build.
