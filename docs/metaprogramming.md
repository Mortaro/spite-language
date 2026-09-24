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

func set_attribute(attribute: Symbol, value: attribute.class) {
    attributes[attribute] = value
}

func get_attribute(attribute: Symbol): attribute.class {
    return attributes[attribute]
}

func set_name(new_name: String) {
    name = new_name.upper_case()
}
```

`age` is reached through that pair of templates: calling `set_age`/`get_age` from outside the class
instantiates `set_attribute`/`get_attribute` with `attribute` bound to the `'age'` symbol. `set_name` is an
exact function, and an exact function always wins over a template -- but only for that one direction of that
one name: writing `person.name` goes through `set_name`, while reading it still goes through the
`get_attribute` template, since no exact `get_name` exists.

```spite title=symbol_codegen/symbol_codegen.spite entry
var console = Console()

func SymbolCodegen() {
    var person = Person(20, "ann")
    var person_age = person.get_age()
    person.set_age(person_age + 1)
    console.print("age", person.age)
    person.set_name("bea")
    console.print("name", person.name)
}
```
```output
age 21
name BEA
```

`set_age` and `get_age` are generated on demand by those two calls.

Before milestone 9a's reference-counting model, generating (or writing) a `get_<attribute>()` for an owning
attribute (`String`/`List<T>`/`Dictionary<T>`) and calling it -- including implicitly, the way `person.name`
above does -- double-freed at runtime; see [KNOWN_ISSUES.md](KNOWN_ISSUES.md) item 3 for that history. It is
fixed now: scalar/enum/owning attributes all intercept safely in both directions.

## Reflection: `Spite.Class` and `Spite.Attribute`

`value.class` is a `Spite.Class` (`.name`, a `Symbol` -- text from the program's own symbol table, which reads as text anywhere text is expected -- and `.namespace` -- a `Spite.Namespace?` with `.name`, `.name_with_namespaces`,
`.parent`, `.classes` and `.namespaces`, `null` for a global class; printing a namespace prints its `.name_with_namespaces`);
printing a class prints just its `.name`. `value.attributes`
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

func ReflectionBasics() {
    describe(Gadget("wrench", 3))
}

func describe(gadget: Gadget) {
    console.print("class", gadget.class)
    console.print("class name", gadget.class.name)
    console.print("own class", class.name)
    console.print("declared name", Gadget.name)
    var attributes = gadget.attributes
    var index = 0
    while index < attributes.count() {
        var attribute = attributes[index]
        console.print(attribute.name, attribute.class, attribute.value)
        index = index + 1
    }
}
```
```output
class Gadget
class name Gadget
own class ReflectionBasics
declared name Gadget
name String wrench
power Int 3
```

`class` is an ordinary identifier, not a keyword: inside any function of a class it is the class of the
instance that function answers on -- an inherited attribute every instance has -- so `own class` above names
the class the function was written in, and an attribute explicitly named `class` shadows it. A class name read
directly reads the same class object member by member: `declared name` is `"Gadget"` with no instance anywhere.
`.namespace` is the one `Spite.Namespace?`, so it needs narrowing before its members are read. Assert the path
itself, `assert Gadget.namespace`, and every read of that path and its prefixes is a plain value for the rest
of the block -- the attribute is an ordinary nullable value, so it follows the ordinary narrowing rules like
any other. Copying it into a local just to narrow the local is a compile error (D63), and comparing needs no
narrowing at all: `Gadget.namespace == "Shop"` is false when there is no namespace (D69).

**Reflection is read-only** (D88). `.name`, `.namespace`, `.attributes`, `.functions` and the rest are
getter functions -- `get_name()`, `get_namespace()`, ... in `library/spite/class.spite` and its neighbours --
with no setter, so reading `.name` works through the ordinary attribute read and writing it is an error. The
values live in private fields (`_name`, `_namespace`): a name starting with `_` is used only inside its own
class, which is how a reopening of `Spite.Class` reads them (see [packages.md](packages.md)). Everything
reflection offers is ordinary code in `library/spite/`, so it is reachable by name at run time, from the REPL
too; the few functions only the compiler can write -- `value_attributes()`, `value_functions()`, `assign(text)`,
`call_function()` and `call_with_text(arguments)`, which reach the live value behind a `Spite.Attribute` or a
`Spite.Function` -- are the compiler's reopening of those classes, printed by `--final-classes` like any other
member (see [compiler.md](compiler.md)).

```spite title=reflection_read_only/reflection_read_only.spite entry error
var console = Console()

func ReflectionReadOnly() {
    var described = console.class
    described.name = "Terminal"
}
```
```diagnostic
'name' is read-only
```

A class of the standard library describes its members the same way: `Memory.functions.map_name()` lists
`allocate_bytes`, `resize`, `free` and the rest, including the ones whose bodies the compiler supplies.

`attributes[symbol]` inside a class (used by Symbol codegen above) and `.attributes` from outside are
different things reading the same data: the former is compile-time only, indexed by a `Symbol`; the latter is
an ordinary runtime `List<T>`, walked with `while` like any other list.

`Person.instances` is every instance of `Person` that is alive at that moment, in the order they were made.
`Spite.Class.instances` is every class in the program, because a class is an instance of `Spite.Class`.

## Generics and codegen values (`$`)

`$name` means "replaced at code generation", and it is for generics only: its values are the ones a constructor
declares between `<` and `>`, given positionally at the call site. A `$name` that no constructor declares is an
error that points at [`Environment`](#program-settings-environment), which is where a program's settings live.

```spite title=generics_basics/pair.spite
var left: $left_type = null
var right: $right_type = null

func Pair<$left_type, $right_type>(new_left: $left_type, new_right: $right_type) {
    left = new_left
    right = new_right
}

func describe(): String {
    return "{left} and {right}"
}
```
```spite title=generics_basics/generics_basics.spite entry
var console = Console()

func GenericsBasics() {
    var scoreboard = Pair<String, Int>("Aria", 42)
    var description = scoreboard.describe()
    console.print(description)
}
```
```output
Aria and 42
```

`null` on a `$generic`-typed field means that generic's bound type's default value, not a literal `T?`
-- provisional, see manual.md open question 1.

## Tree shaking

Conditions on codegen values are decided **at compile time**, and the untaken branch is removed from the
generated C entirely: `Weapon<Int, true>` and `Weapon<Int, false>` are two classes, and each keeps only the
branch of `if $is_magic { }` that it takes. This is tree shaking, not just dead-code elimination at the C
compiler level.

`--development` disables all of this: every codegen-value condition becomes a real runtime `if` (both branches
compiled in, so live reload can flip it without recompiling), and every class in the folder is emitted instead
of only the ones reachable from the entry class.

## Program settings: `Environment`

A program's settings are the fields of `Environment`, a singleton in the standard library. The program reopens
it with a file of its own named `environment.spite`, holding one `var` per setting with a literal default, and
reads it with `Environment()` anywhere -- the entry constructor does not have to receive the command line and
pass it down.

```spite title=program_settings/environment.spite
var endpoint = "local"
var workers = 1
var verbose = false
```
```spite title=program_settings/program_settings.spite entry vars=endpoint:production,verbose:true
var console = Console()
var environment = Environment()

func ProgramSettings() {
    if environment.endpoint == "production" {
        console.print("using the production endpoint")
    } else {
        console.print("using the local endpoint")
    }
    console.print("workers", environment.workers, "verbose", environment.verbose)
}
```
```output
using the production endpoint
workers 1 verbose true
```

Each field is read once, when `Environment()` is first made: from `--endpoint=production` on the program's
command line (after `--` when the compiler runs it: `spite program_settings.spite -- --endpoint=production`),
otherwise from the `ENDPOINT` environment variable, otherwise the declared default. Only declared fields are
read, so nothing the program does not name is ever loaded. The default's literal is the setting's type --
`false` a `Bool`, `0` an `Int`, `""` a `String` -- and a value that is not one (`--verbose=maybe`) crashes at
startup. The values are read when the program runs, so `if environment.verbose` is an ordinary `if`: both
branches are compiled in.
