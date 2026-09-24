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

### Another class's attributes, and all of them at once

A template can answer for the attributes of a class other than its own: write the class inside the `Symbol`,
`attribute: Symbol<Label>`. Inside, `label.attributes[attribute]` is that attribute of the `Label` passed in,
for reading and for writing. Calling the template with the symbol's name made plural -- `show_attributes(...)`
for `show_attribute` -- calls it once for every attribute, in the order they are declared, so the template has
to return nothing. It is how a class walks another one attribute by attribute without a loop or reflection at
run time: every call is an ordinary typed function the compiler wrote.

```spite title=every_attribute/label.spite
var text = "fragile"
var copies = 2
var urgent = true
```
```spite title=every_attribute/every_attribute.spite entry
var console = Console()

func EveryAttribute() {
    var label = Label()
    var lines = List<String>()
    show_attributes(label, lines)
    var joined = lines.join(", ")
    console.print(joined)
    show_copies(label, lines)
    var last_line = lines.last()
    console.print(last_line)
}

func show_attribute(attribute: Symbol<Label>, label: Label, lines: List<String>) {
    lines.append("{attribute.name}: {label.attributes[attribute]}")
}
```
```output
text: fragile, copies: 2, urgent: true
copies: 2
```

The member may also be a function that takes no arguments: `label.attributes[attribute]` is then a call to it.
In a generic class the class inside the `Symbol` is usually a codegen value. `List<$element_type>` writes its
member templates over `member: Symbol<$element_type>`, so `item.attributes[member]` reads a member of the element
-- that is how `filter_<member>()`, `sum_<member>()` and the rest are written, in Spite, in `library/list.spite`
([standard_library.md](standard_library.md#how-the-member-templates-are-written)) -- and the standard library's
[`Json`](json.md) writes and reads any class over `attribute: Symbol<$value_type>`.

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
`Spite.Function` -- are the compiler's reopening of those classes, printed by `--final_classes` like any other
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

`$name` means "replaced at code generation", and it is for generics only. A class declares each one on a
`generic` line of its own at the top of the file, and a caller gives the values positionally, in the order of
those lines: `Pair<String, Int>(...)`. A `$name` with no `generic` line is an error that points at
[`Environment`](#program-settings-environment), which is where a program's settings live.

```spite title=generics_basics/pair.spite
generic $left_type
generic $right_type

var left: $left_type = null
var right: $right_type = null

func Pair(new_left: $left_type, new_right: $right_type) {
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
    var inferred = Pair("Hero", 7)
    var inferred_description = inferred.describe()
    console.print(inferred_description)
}
```
```output
Aria and 42
Hero and 7
```

The `generic` lines come first in the file, before enums, unions, types and variables. A generic class needs
no constructor of its own: `library/list.spite` is `generic $element_type` and its functions, and
`List<Int>()` is made from its defaults. Writing the old `func Pair<$left_type, $right_type>(...)` form is a
parse error that names the `generic` lines to write instead.

When every `$name` appears in the constructor's parameter types, a call may leave the `<...>` out and the
compiler reads the values from the arguments: `Pair("Hero", 7)` is `Pair<String, Int>`, and a `$name` inside a
function value's type is read from the function, so `Concurrent(file.read)` is a `Concurrent<String?>`
([concurrency.md](concurrency.md)). When one cannot be read, the error asks for them between `<` and `>` (proposed
by Claude, unconfirmed).

`null` on a `$generic`-typed field means that generic's bound type's default value, not a literal `T?`
-- provisional, see manual.md open question 1.

### Asking what a generic was given

When a codegen value is a type, `if $value_type == String { }` asks which type it is. The answer is known when
the class is made, so only the branch taken is compiled, and each branch may use what only that type has. Besides
naming a type exactly, four names ask for a kind:

| Test | True when the type is |
|---|---|
| `$value_type == List` | any `List<T>` |
| `$value_type == Dictionary` | any `Dictionary<T>` |
| `$value_type == Null` | any `T?` |
| `$value_type == Symbol` | an enum (a closed list of symbols), or `Symbol` itself |

Inside such a branch the types it was built from are named after the container's own codegen values:
`$value_type.element_type` for a `List<$element_type>`, `$value_type.value_type` for a `Dictionary<$value_type>`
or a `$value_type?`, and a generic class's own names for one of its instances.

```spite title=describe_kind/kind.spite
generic $kind_type

func name(): String {
    if $kind_type == String {
        return "text"
    } else if $kind_type == Int {
        return "a whole number"
    } else if $kind_type == Null {
        var inner = Kind<$kind_type.value_type>()
        var inner_name = inner.name()
        return "{inner_name}, or nothing"
    } else if $kind_type == List {
        var element = Kind<$kind_type.element_type>()
        var element_name = element.name()
        return "a list of {element_name}"
    } else {
        return "something else"
    }
}
```
```spite title=describe_kind/describe_kind.spite entry
var console = Console()

func DescribeKind() {
    var nested = Kind<List<List<Int>>>()
    var nested_name = nested.name()
    console.print(nested_name)
    var maybe = Kind<String?>()
    var maybe_name = maybe.name()
    console.print(maybe_name)
    var flag = Kind<Bool>()
    var flag_name = flag.name()
    console.print(flag_name)
}
```
```output
a list of a list of a whole number
text, or nothing
something else
```

A test on a type always decides at compile time, `--development` included, since the branch it rules out would
not compile.

Text also becomes an enum value by assignment, the way it becomes a number: `var course: Recipe.Course = name`
is the value spelled `name`, or the enum's first value when there is none, as `"x"` becomes `0` for an `Int`.

## Tree shaking

Conditions on codegen values and on [`Build`](#build-settings-build) fields are decided **at compile time**, and
the untaken branch is removed from the generated C entirely: `Weapon<Int, true>` and `Weapon<Int, false>` are two classes, and each keeps only the
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
command line (after `--` when the compiler runs it: `spite program_settings -- --endpoint=production`),
otherwise from the `ENDPOINT` environment variable, otherwise the declared default. Only declared fields are
read, so nothing the program does not name is ever loaded. The default's literal is the setting's type --
`false` a `Bool`, `0` an `Int`, `""` a `String` -- and a value that is not one (`--verbose=maybe`) crashes at
startup. The values are read when the program runs, so `if environment.verbose` is an ordinary `if`: both
branches are compiled in.

## Build settings: `Build`

`Environment` is read when the program **runs**. `Build` is its twin for when the program is **compiled**: a
singleton in the standard library whose fields are decided by the compiler and written into the program as
constants. Every compiler option is one of them -- `mode`, `output`, `optimized`, `development`, `repl`,
`repl_port`, `format`, `debug_memory`, `final_classes`, and the two operating systems below -- and a program adds
its own by reopening `Build` in a file named `build.spite`:

```spite title=build_settings/build.spite
var serve = false
```
```spite title=build_settings/environment.spite
var name = "client"
```
```spite title=build_settings/build_settings.spite entry build=serve:true vars=serve:false,name:tester
var console = Console()
var build = Build()
var environment = Environment()

func BuildSettings() {
    if build.serve {
        console.print("serving as", environment.name)
    } else {
        console.print("this branch is not in the build")
    }
}
```
```output
serving as tester
```

That is `spite build_settings --serve=true -- --serve=false --name=tester`. A `--name=value` given to the
**compiler** -- before the `--` -- sets the `Build` field of that name; a field nobody sets keeps the default
it was declared with. Either way `build.serve` is the literal `true` in the built program: `if build.serve { }`
is decided while compiling, the branch it does not take is never generated, and the program's own
`--serve=false` changes nothing. `name` belongs to `Environment`, so it is still read when the program runs.

- A `Bool` field may be given bare: `--optimized` is `--optimized=true`.
- The value has to be of the field's type (`--workers=many` for an `Int` is a compile error), a flag naming a
  field of `Environment` is a compile error that says to pass it after `--`, and a flag naming no field at all
  is a compile error listing the fields `Build` has. A typo never passes silently.
- A program's own `build.spite` may also give a compiler option a different default: `var optimized = true`
  builds it optimized unless `--optimized=false` is given. `mode` and `format` are the exceptions: the compiler
  needs them before it has read the program, so only the flag changes them.
- `Build().operating_system` is the system doing the compiling, and `Build().target_operating_system` the one
  the program is compiled for. The target defaults to the compiling system; `--target_operating_system=linux`
  compiles for Linux from any machine, loads `library/linux/` instead of this machine's folder, and folds, so
  `if build.target_operating_system == "windows" { }` keeps only the branch for the target.
