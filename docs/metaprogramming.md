# Metaprogramming

There are no macros. Spite's answer to "write this for every attribute" is **Symbol codegen**: a function whose
name has a hole in it, which the compiler fills once for every name a program calls. Its answer to "one class for
many types" is **generics**, codegen values declared with `generic` lines. Both are ordinary functions and
classes, so what they generate is typed, visible in `--final_classes`, and removed when nothing calls it.

Reading a program's structure at run time -- `.class`, `.attributes`, `.functions` -- is
[reflection.md](reflection.md). The standard library's member templates (`filter_<member>()`,
`sum_<member>()`, ...) are Symbol codegen written in Spite, in [collections.md](collections.md).

## Symbol codegen, step by step

A parameter of type `Symbol` whose name is a segment of its own function's name turns that function into a
*template*: it answers every call whose name fits the pattern, one per attribute. `set_attribute`/
`get_attribute` (with a `Symbol` parameter named `attribute`) answer `set_age`/`get_age`, `set_name`/
`get_name`, and so on, for each field of the class -- generated only for the names actually called.

Inside the template, the `Symbol` names that attribute two different ways: written as a **type**
(`value: attribute.class`, `): attribute.class`), it *is* the attribute's real type; written as an
**expression**, `attribute.class` is a `Spite.Class` naming that type (it prints just like the type name
would, in `console.print` and inside a text's `{}` alike: a value in a hole whose class has `to_string()` is
turned into text by it). `attributes[attribute]` is a separate, compile-time-only form: that same field, indexed by the
`Symbol`.

```gdscript title=symbol_codegen/person.spite
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

```gdscript title=symbol_codegen/symbol_codegen.spite entry
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

```gdscript title=every_attribute/label.spite
var text = "fragile"
var copies = 2
var urgent = true
```
```gdscript title=every_attribute/every_attribute.spite entry
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
([standard_library.md](collections.md#how-the-member-templates-are-written)) -- and the standard library's
[`Json`](json.md) writes and reads any class over `attribute: Symbol<$value_type>`.

The class inside the `Symbol` may also be a `type`. The template then ranges over the attributes the type names,
and over the functions it requires that take no arguments; `row.attributes[attribute]` reads or writes that member
of whatever class the value really is, and the plural calls the template once per attribute the type lists, in
its order. This is how a query row declared as a `type` is filled attribute by attribute.

```gdscript title=shape_attributes/filler.spite
generic $row_type

func fill_attribute(attribute: Symbol<$row_type>, row: $row_type, source: $row_type) {
    row.attributes[attribute] = source.attributes[attribute]
}
```
```gdscript title=shape_attributes/shape_attributes.spite entry
type Position {
    x: Int
    y: Int
}

var console = Console()

func ShapeAttributes() {
    var filler = Filler<Position>()
    var row = {x: 0, y: 0, name: "row"}
    var source = {x: 3, y: 4}
    filler.fill_attributes(row, source)
    console.print(row.x, row.y, row.name)
}
```
```output
3 4 row
```

## Generics and codegen values (`$`)

`$name` means "replaced at code generation", and it is for generics only. A class declares each one on a
`generic` line of its own at the top of the file, and a caller gives the values positionally, in the order of
those lines: `Pair<String, Int>(...)`. A `$name` with no `generic` line is an error that points at
[`Environment`](programs.md#run-time-settings-environment), which is where a program's settings live.

```gdscript title=generics_basics/pair.spite
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
```gdscript title=generics_basics/generics_basics.spite entry
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

- **One line per value**, after a `singleton` line and before everything else, so skimming the top of a file
  shows what a class accepts. A comma on a `generic` line is an error naming the next line to write, and so is
  listing the values on the constructor, `func Pair<$left_type, $right_type>(...)`.
- **Call sites are positional, always**, in the order of the lines. Reordering the lines changes what every call
  means; where the values are of different kinds the compiler notices, and where they are not, the tests have to.
- **Every hole is filled.** There are no defaults: the wrong number of values is an error naming the class's
  values in order, and a `$name` with no `generic` line is an error too.
- **A generic class needs no constructor.** `library/list.spite` is `generic $element_type` and its functions,
  and `List<Int>()` is made from its defaults.
- **The values can be read from the arguments.** When every `$name` appears in the constructor's parameter
  types, a call may leave the `<...>` out: `Pair("Hero", 7)` is `Pair<String, Int>`, and a `$name` inside a
  function value's type is read from the function, so `Concurrent(file.read)` is a `Concurrent<String?>`
  ([concurrency.md](concurrency.md)). When one cannot be read, the error asks for them between `<` and `>`.
- `null` as the default of a field typed by a codegen value means that type's own default, not a `T?`
  (provisional: [manual, open question 1](../manual.md#open-questions)).

A codegen value may be a value rather than a type. A condition on it is decided while compiling, so each set of
values is its own class with only its own branch in it:

```gdscript title=codegen_values_doc/weapon.spite
generic $damage_type
generic $is_magic

var damage: $damage_type = 0

func Weapon(new_damage: $damage_type) {
    damage = new_damage
}

func hit(): $damage_type {
    if $is_magic {
        return damage * 2
    }
    return damage
}
```
```gdscript title=codegen_values_doc/codegen_values_doc.spite entry
var console = Console()

func CodegenValuesDoc() {
    var staff = Weapon<Int, true>(10)
    var club = Weapon<Float, false>(2.5)
    var staff_hit = staff.hit()
    var club_hit = club.hit()
    console.print(staff_hit, club_hit)
}
```
```output
20 2.5
```

```gdscript title=codegen_count_error/codegen_count_error.spite entry error
var console = Console()

func CodegenCountError() {
    var pairs = List<String, Int>()
    var pairs_count = pairs.count()
    console.print(pairs_count)
}
```
```diagnostic
takes 1 codegen value(s), in this order: $element_type
```

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

```gdscript title=describe_kind/kind.spite
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
```gdscript title=describe_kind/describe_kind.spite entry
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

A codegen value that is a type reads as its class wherever a class name would: `$component_type.name` is the
bound class's name the way `Health.name` is, and `$component_type.attributes` its attributes. It allocates
nothing, since the class object already exists; a value of the type is not needed to ask. On its own, `$name` is
still not a value -- only a member read through it is.

```gdscript title=codegen_class_name/column.spite
generic $component_type

func label(): String {
    var name = $component_type.name
    return "a column of {name}"
}
```
```gdscript title=codegen_class_name/codegen_class_name.spite entry
var console = Console()

func CodegenClassName() {
    var words = Column<String>()
    var label = words.label()
    console.print(label)
}
```
```output
a column of String
```

## A class's functions, a folder's classes and a name's pattern

The templates above range over a class's attributes. They range over three more things a program already has:
the arguments of one function, the classes of every folder with a given name, and the functions whose names fit
a pattern. With `has_function`, which asks a generic's type a question while compiling, this is how an engine
finds its systems and calls them with nothing registered. It all happens at compile time, and only what a program
calls is generated. These spellings are proposals (manual section 8), not yet confirmed.

### Asking for a function, and walking its arguments

`$target_type.has_function('run_each')` in a condition is decided while compiling, like `if $is_magic`. Only the
branch taken is compiled, so it may call what only that type has. The name must be written as a literal.

`argument: Symbol<$target_type.run_each>` ranges over the arguments of `run_each`. Inside, `argument.name` is the
argument's name and `argument.class` is its type. The plural, `describe_arguments()`, calls the template once per
argument, in order. A template like this that returns a value has one more use: its plural, written as the whole
argument list of a call to that same function, passes one value per argument. So `target.run_each(made_arguments())`
means `target.run_each(made_hero(), made_pet())`, evaluated left to right, and one generic calls a function of
any arity.

```gdscript title=function_arguments/hero.spite
var name = "Ann"
```
```gdscript title=function_arguments/pet.spite
var name = "Rex"
```
```gdscript title=function_arguments/walk.spite
var console = Console()

func run_each(hero: Hero, pet: Pet) {
    console.print(hero.name, "walks", pet.name)
}
```
```gdscript title=function_arguments/caller.spite
generic $target_type

var target: $target_type = null
var console = Console()

func call() {
    if $target_type.has_function('run_each') {
        describe_arguments()
        target.run_each(made_arguments())
    } else {
        var name = $target_type.name
        console.print(name, "has no run_each")
    }
}

func describe_argument(argument: Symbol<$target_type.run_each>) {
    console.print(argument.name, "is a", argument.class)
}

func made_argument(argument: Symbol<$target_type.run_each>): argument.class {
    var made: argument.class = null
    return made
}
```
```gdscript title=function_arguments/function_arguments.spite entry
func FunctionArguments() {
    var walking = Caller<Walk>()
    walking.call()
    var heroic = Caller<Hero>()
    heroic.call()
}
```
```output
hero is a Hero
pet is a Pet
Ann walks Rex
Hero has no run_each
```

`Caller<Hero>` keeps only its `else` branch, and the templates over `run_each` are never generated for it. An
argument of type `List<Row>` names its row type as `argument.class.element_type`. Asking with a name that is not
written in the code cannot be decided while compiling:

```gdscript title=function_name_error/asker.spite
generic $target_type

var wanted = "run_each"

func check() {
    if $target_type.has_function(wanted) {
        return
    }
}
```
```gdscript title=function_name_error/function_name_error.spite entry error
func FunctionNameError() {
    var asker = Asker<Console>()
    asker.check()
}
```
```diagnostic
is decided while compiling, so the name it asks for is written as a literal
```

### Every class in a folder

`system: Symbol<System>` ranges over the classes of every folder named `system`, however deep:
`System.Greet` and `Tools.System.Sweep`, in order of their dotted names. A range that names no class or type is
read as the end of a folder's namespace. Inside, `system.class` is the class, as a type or as a value, and
`system.name` is its dotted name. The plural calls the template for each class, so adding a file to a `system/`
folder is how a system is added, with no list anywhere. A range that matches no folder at all walks nothing, so
an engine that walks `Symbol<Recipe>` still compiles for a program with no recipes; naming one class it does not
hold, as in `cook_recipe_bread()`, is an error that lists the classes it does hold.

```gdscript title=folder_walk/system/greet.spite
var console = Console()

func run() {
    console.print("greet runs")
}
```
```gdscript title=folder_walk/tools/system/sweep.spite
var console = Console()

func run() {
    console.print("sweep runs")
}
```
```gdscript title=folder_walk/tools/broom.spite
var bristles = 40
```
```gdscript title=folder_walk/folder_walk.spite entry
type Runnable {
    run()
}

var runners = List<Runnable>()
var console = Console()

func FolderWalk() {
    add_systems()
    runners.each_run()
}

func add_system(system: Symbol<System>) {
    var made: system.class = null
    runners.append(made)
    console.print("found", system.name)
}
```
```output
found System.Greet
found Tools.System.Sweep
greet runs
sweep runs
```

`Tools.Broom` is not in a `system` folder, so it is not walked. Written with a generic,
`Runner<system.class>()` makes one `Runner` per class found.

### A name that says when it runs

When the parameter's own name is a word of the function named in the range, that word is a hole, exactly as it
is in a template's own name: `phase: Symbol<$system_type.phase_all>` ranges over the functions whose names fit
`<phase>_all`. For `update_all`, `phase.name` is `"update"`. Inside the template, the pattern written as a
member, `system.phase_all()`, calls the matched function. Another template ranging over
`$system_type.phase_all` that is called from inside walks the matched function's arguments. The engine owns the
list of phases, so systems run in the engine's order, whatever order their files are in:

```gdscript title=name_phases/system/draw.spite
var console = Console()

func render_all() {
    console.print("draw")
}
```
```gdscript title=name_phases/system/move.spite
var console = Console()

func update_all() {
    console.print("move")
}
```
```gdscript title=name_phases/runner.spite
generic $system_type

var system: $system_type = null
var placed_in = ""

func Runner() {
    place_phases_all()
}

func place_phase_all(phase: Symbol<$system_type.phase_all>) {
    placed_in = phase.name
}

func run() {
    run_phases_all()
}

func run_phase_all(phase: Symbol<$system_type.phase_all>) {
    system.phase_all()
}
```
```gdscript title=name_phases/name_phases.spite entry
type Runnable {
    placed_in: String
    run()
}

var phases = ["update", "render"]
var runners = List<Runnable>()
var console = Console()

func NamePhases() {
    add_systems()
    phases.each_run_phase()
}

func add_system(system: Symbol<System>) {
    var runner = Runner<system.class>()
    runners.append(runner)
    console.print(system.name, "runs in", runner.placed_in)
}

func run_phase(phase: String) {
    var index = 0
    while index < runners.count() {
        run_placed(runners[index], phase)
        index = index + 1
    }
}

func run_placed(runner: Runnable, phase: String) {
    if runner.placed_in == phase {
        runner.run()
    }
}
```
```output
System.Draw runs in render
System.Move runs in update
move
draw
```

`--final_classes` shows what was made: `RunnerMove` has `place_update_all()` and `run_update_all()`, and
`add_systems()` calls `add_system_system_draw()` and `add_system_system_move()`. As a condition,
`$system_type.has_function("<phase>_all")` asks whether any function fits the pattern. A fuller engine, with
rows queried per argument and `run_each` called once per combination, is
`conformance/stage6/system_phases`.

## Tree shaking

Spite removes what a program does not use, and it can do so exactly, because everything it generates is decided
at compile time:

- A **condition on a codegen value** or on a [`Build`](programs.md#compile-time-settings-build) field is decided
  while compiling, and the branch not taken is never generated: `Weapon<Int, true>` and `Weapon<Int, false>` are
  two classes, and each keeps only the branch of `if $is_magic { }` that it takes.
- A **Symbol codegen template** is compiled only for the names a program calls: a program that never calls
  `sum_price()` has no `sum_price`.
- **`has_function`** is decided while compiling, and templates over a function's arguments, a folder's classes
  or a name pattern are expanded into ordinary calls. No list of functions or classes exists at run time.
- **Reflection** is built only where it is read ([reflection.md](reflection.md)), and a generic class such as
  [`Json`](json.md) exists only for the types a program uses it with.
- A **class** nothing reaches is not emitted.
- The **concurrency scheduler**, fibers and atomic reference counts exist only in a program that makes a
  `Concurrent` or a `Parallel`, or is built with `--repl_port` ([concurrency.md](concurrency.md)).

This is tree shaking over the program's own model, not dead-code elimination left to the C compiler.

`--development` keeps everything instead: a condition on a codegen value becomes a real run-time `if`, with
both branches compiled in, so live reload can flip it without recompiling, and every class in the folder is
emitted rather than only the ones reachable from the entry class. A test on a *type* (`$value_type == List`)
still folds under `--development`, because the branch it rules out would not compile.
