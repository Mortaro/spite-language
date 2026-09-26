# Metaprogramming

There are no macros. Spite's answer to "write this for every attribute" is **Symbol codegen**: a function whose
name has a hole in it, which the compiler fills once for every name a program calls. Its answer to "one class for
many types" is **generics**, codegen values declared with `generic` lines. Both are ordinary functions and
classes, so what they generate is typed, visible in `--final-classes`, and removed when nothing calls it. All of
it happens while compiling: a program pays at run time only for the ordinary functions and classes it was
expanded into, and nothing for a template, a walk or a generic it never uses ([Tree shaking](#tree-shaking)).

Reading a program's structure at run time -- `.class`, `.attributes`, `.functions` -- is
[reflection.md](reflection.md). The standard library's member templates (`filter_<member>()`,
`sum_<member>()`, ...) are Symbol codegen written in Spite, in [collections.md](collections.md). They look only at
the element: a function of the calling class is passed as a value, `people.each(say_hello)`, never found by name
([D148](decisions.md), [collections.md](collections.md#passing-a-function-for-each-element)).

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

func Person(new_age: Integer, new_name: String) {
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
    person.name = "bea"
    console.print("name", person.name)
}
```
```output
age 21
name BEA
```

`set_age` and `get_age` are generated on demand by those two calls, and `get_name` by the read of `person.name`;
no other instance exists. The exact rules are in [Symbol codegen](#symbol-codegen--implemented).

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
    crash last_line
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
([collections.md](collections.md#how-the-member-templates-are-written)) -- and the standard library's
[`JsonWriter`, `JsonReader` and the binary pair](json.md) write and read any class over `attribute: Symbol<$value_type>`. When there is nothing to walk
-- a class with no attributes, a marker, or a codegen value that is no class at all, an `Integer` or a `T?` -- the
plural is still there and calls nothing, so `Pack<Marker>` and `Pack<Integer?>` compile like any other
(`conformance/stage6/empty_plural`).

Reading `label.attributes[attribute]` reads every attribute the walk reaches, so they count as used;
`attribute.name` and `attribute.class` do not ([the rule](#symbol-codegen--implemented),
[style.md](style.md#nothing-unused)).

The class inside the `Symbol` may also be a `type`. The template then ranges over the attributes the type names,
and over the functions it requires that take no arguments; `row.attributes[attribute]` reads or writes that member
of whatever class the value really is, and the plural calls the template once per attribute the type lists, in
its order. This is how a query row declared as a `type` is filled attribute by attribute. The walk is still
expanded while compiling; what is left for run time is the read or write itself, answered from the value's class
the way every read through a `type` is ([values_and_types.md](values_and_types.md#inline-types-and-duck-typing)).

```gdscript title=shape_attributes/filler.spite
generic $row_type

func fill_attribute(attribute: Symbol<$row_type>, row: $row_type, source: $row_type) {
    row.attributes[attribute] = source.attributes[attribute]
}
```
```gdscript title=shape_attributes/shape_attributes.spite entry
type Position {
    x: Integer
    y: Integer
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
those lines: `Pair<String, Integer>(...)`. A `$name` with no `generic` line is an error; used as a value, as in
`if $verbose { }`, the error points at [`Environment`](programs.md#run-time-settings-environment), which is where
a program's settings live.

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
    var scoreboard = Pair<String, Integer>("Aria", 42)
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

One line declares one value, and the lines sit at the top of the file, so skimming it shows what a class
accepts. Every hole is filled at every call: there are no defaults, and the wrong number of values is an error
that lists the class's values in order (the second program below). A generic class needs no constructor --
`library/list.spite` is `generic $element_type` and its functions -- and when the constructor's arguments say
every value, the `<...>` may be left out: `Pair("Hero", 7)` is a `Pair<String, Integer>`, `JsonWriter(order)` a
`JsonWriter<Order>` ([json.md](json.md)) and `Concurrent(file.read)` a `Concurrent<String?>`
([concurrency.md](concurrency.md)). `null` as the default of a field typed `$left_type` means that type's own
default, not a `T?` (provisional: [open question 1](open_questions.md#open-questions)).

Only a class has codegen values. A function never lists its own (`func pick<$value_type>(...)` is an error), and
there are no generic functions to write instead ([D123](decisions.md)): a function that takes any class takes a
`type`, such as the built-in empty `Anything` ([reflection.md](reflection.md#an-attributes-value)), or it belongs to a
generic class. The exact forms and error texts are in [Codegen values](#codegen-values---implemented).

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
    var staff = Weapon<Integer, true>(10)
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
    var pairs = List<String, Integer>()
    var pairs_count = pairs.count()
    console.print(pairs_count)
}
```
```diagnostic
takes 1 codegen value(s), in this order: $element_type
```

### Constraining what a generic accepts

A `generic` line may name a `type` after a colon. The class accepts only types that fit it, and a class that does
not is an error where it is named, saying which function or attribute is missing -- not an error deep inside the
generic's body, the first time some function happens to call what the class lacks.

```gdscript title=generic_constraint/shelf.spite
generic $item_type: Printable

var items = List<$item_type>()

func add(item: $item_type) {
    items.append(item)
}

func describe(): String {
    var texts = List<String>()
    var index = 0
    while index < items.count() {
        var item = items.get_at(index)
        var text = item.to_string()
        texts.append(text)
        index = index + 1
    }
    return texts.join(", ")
}
```
```gdscript title=generic_constraint/book.spite
var title = ""

func Book(new_title: String) {
    title = new_title
}

func to_string(): String {
    return "'{title}'"
}
```
```gdscript title=generic_constraint/generic_constraint.spite entry
var console = Console()

func GenericConstraint() {
    var books = Shelf<Book>()
    var book = Book("Dune")
    books.add(book)
    var numbers = Shelf<Integer>()
    numbers.add(3)
    var book_text = books.describe()
    var number_text = numbers.describe()
    console.print(book_text, number_text)
}
```
```output
'Dune' 3
```

```gdscript title=generic_constraint_error/generic_constraint_error.spite entry error
var console = Console()

func GenericConstraintError() {
    var sounds = Chorus<Rock>()
    var count = sounds.singers.count()
    console.print(count)
}
```
```gdscript title=generic_constraint_error/chorus.spite
generic $singer_type: Singer

type Singer {
    sing(): String
}

var singers = List<$singer_type>()
```
```gdscript title=generic_constraint_error/rock.spite
var weight = 3
```
```diagnostic
'Rock' does not fit type 'Singer', which 'Chorus' requires of $singer_type: it has no function 'sing'
```

The constraint is optional and names an existing `type` -- `Printable` is `Console`'s, found like any type name,
and a `type` of the generic's own file works too. What fits is what the `type` would accept anywhere, so
`Shelf<Integer>` above is fine, while a `T?` or a function value is not. It is checked wherever a class is given,
written out or read from the constructor's arguments, and it costs nothing at run time: the check is the
compiler's, and the class made is the one an unconstrained line would make
([the rules](#codegen-values---implemented)).

### Asking what a generic was given

When a codegen value is a type, `if $value_type == String { }` asks which type it is. The answer is known when
the class is made, so only the branch taken is compiled, and each branch may use what only that type has. Besides
naming a type exactly, four names ask for a kind: `List`, `Dictionary`, `Null` (any `T?`) and `Symbol` (an enum)
-- the table is in [the rules](#codegen-values---implemented).

What the branch ruled out is not compiled at all, and neither is what only it reaches. Below, `Field<Integer>`
compiles `write` without the `List` branch, never compiles `write_list`, and so never checks its `.count()`
against an `Integer`; the statements after a taken branch that returns are left out the same way. One generic
class can therefore keep a helper per kind (`conformance/stage6/folded_helpers`):

```gdscript
func write(value: $value_type): String {
    if $value_type == List {
        return write_list(value)
    }
    return write_number(value)
}
```

Inside such a branch the types it was built from are named after the container's own codegen values:
`$value_type.element_type` for a `List<$element_type>`, `$value_type.value_type` for a `Dictionary<$value_type>`
or a `$value_type?`, and a generic class's own names for one of its instances.

```gdscript title=describe_kind/kind.spite
generic $kind_type

func name(): String {
    if $kind_type == String {
        return "text"
    } else if $kind_type == Integer {
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
    var nested = Kind<List<List<Integer>>>()
    var nested_name = nested.name()
    console.print(nested_name)
    var maybe = Kind<String?>()
    var maybe_name = maybe.name()
    console.print(maybe_name)
    var flag = Kind<Boolean>()
    var flag_name = flag.name()
    console.print(flag_name)
}
```
```output
a list of a list of a whole number
text, or nothing
something else
```

A class test on a value folds the same way inside a generic. In `Detector<Label>`, `item == Health` can never be
true, so it is `false` and its branch is removed from that copy, while `Detector<Health>` keeps it. Outside a
generic the same never-true test is an error, since there it is always a mistake; inside one, code a particular
copy cannot use is what tree shaking is for ([D167](decisions.md), [control_flow.md](control_flow.md#value--class)).

```gdscript title=generic_never_true/detector.spite
generic $item_type

func is_health(item: $item_type): Boolean {
    if item == Health {
        return true
    }
    return false
}
```
```gdscript title=generic_never_true/health.spite
var points = 3

func total(): Integer {
    return points
}
```
```gdscript title=generic_never_true/label.spite
var text = "fragile"

func shown(): String {
    return text
}
```
```gdscript title=generic_never_true/generic_never_true.spite entry
var console = Console()

func GenericNeverTrue() {
    var labels = Detector<Label>()
    var healths = Detector<Health>()
    var label = Label()
    var label_answer = labels.is_health(label)
    var health = Health()
    var health_answer = healths.is_health(health)
    console.print(label_answer, health_answer)
}
```
```output
false true
```

A codegen value that is a type reads as its class wherever a class name would: `$component_type.name` is the
bound class's name the way `Health.name` is, and `$component_type.attributes` its attributes, with no value of
the type made to ask. On its own, `$name` is still not a value -- only a member read through it is.

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

The templates above range over a class's attributes. They range over four more things a program already has:
the arguments of one function, the classes of every folder with a given name, the functions whose names fit a
pattern, and an enum's values ([values_and_types.md](values_and_types.md#walking-an-enums-values)). With
`has_function`, which asks a generic's type a question while compiling, this is how an engine finds its systems
and calls them with nothing registered. It all happens at compile time: each walk becomes ordinary calls, no
list of functions or classes exists at run time, and only what a program calls is generated. What these ranges
do was decided by Mortaro (D114-D116, and D180 for the enum a pattern's hole matches); the spellings are
proposals, not yet confirmed ([the rules in full](#a-classs-functions-a-folders-classes-and-a-names-pattern--implemented-the-spellings-proposed-by-claude-unconfirmed)).

### Asking for a function, and walking its arguments

`$target_type.has_function('run_each')` in a condition is decided while compiling, like `if $is_magic`. Only the
branch taken is compiled, so it may call what only that type has. The name must be written as a literal. It is
decided wherever it is written, not only in an `if`: `return $target_type.has_function('run_each') or
$target_type.has_function('run_all')` returns a constant, and no class's table of functions is built to answer it.
Inside a template over a folder's classes, `system.class.has_function('run_each')` is decided the same way for
each class walked ([below](#every-class-in-a-folder)).

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

### Asking whether a function waits

`$system_type.function_waits("update_each")` asks a second question the same way: can this function reach a wait
-- a file or socket read, a sleep, anything that calls one ([concurrency.md](concurrency.md#what-the-compiler-does-at-a-wait))?
The compiler already knows, since it compiles exactly those functions into state machines, so the answer is a
constant like `has_function`'s, and only the branch taken is compiled. An engine uses it to start a system that
does IO as a `Concurrent` it polls between frames, and to call every other system directly, while each system is
written as straight-line code ([D209](decisions.md); the spelling is proposed, unconfirmed):

```gdscript title=waiting_question/loader.spite
var console = Console()
var notes = File(".spite-cache/documentation_waiting_notes.txt")

func update_each() {
    var text = notes.read()
    crash text
    console.print("loaded", text)
}
```
```gdscript title=waiting_question/spinner.spite
var console = Console()
var turns = 0

func update_each() {
    turns = turns + 1
    console.print("spun", turns)
}
```
```gdscript title=waiting_question/stage.spite
generic $system_type

var system: $system_type = null
var console = Console()

func run() {
    var name = $system_type.name
    if $system_type.function_waits("update_each") {
        var loading = Concurrent(system.update_each)
        console.print(name, "started between frames, finished:", loading.finished)
    } else {
        system.update_each()
        console.print(name, "ran inside the frame")
    }
}
```
```gdscript title=waiting_question/waiting_question.spite entry
func WaitingQuestion() {
    var notes = File(".spite-cache/documentation_waiting_notes.txt")
    notes.write("the map")
    var loading = Stage<Loader>()
    loading.run()
    var spinning = Stage<Spinner>()
    spinning.run()
}
```
```output
Loader started between frames, finished: false
loaded the map
spun 1
Spinner ran inside the frame
```

`Stage<Spinner>` has no `Concurrent` in it at all. Inside a template over a folder's classes,
`system.class.function_waits("update_each")` is decided for each class walked, and `klass.function_waits(name)`
answers the same at run time. A function that asks cannot be asked about itself: its own answer would decide
whether it waits, and that is an error ([the rules](#a-classs-functions-a-folders-classes-and-a-names-pattern--implemented-the-spellings-proposed-by-claude-unconfirmed)).
[concurrency.md](concurrency.md#choosing-where-concurrents-resume) shows the frame loop that polls what it starts.

### Every class in a folder

`system: Symbol<System>` ranges over the classes of every folder named `system`, however deep:
`System.Greet` and `Tools.System.Sweep`, in order of their dotted names. A range that names no class or type is
read as the end of a folder's namespace. Inside, `system.class` is the class, as a type or as a value, and
`system.name` is its dotted name. The plural calls the template for each class, so adding a file to a `system/`
folder is how a system is added, with no list anywhere. A range that matches no folder walks nothing, and naming
one class it does not hold is an error ([the rules](#a-classs-functions-a-folders-classes-and-a-names-pattern--implemented-the-spellings-proposed-by-claude-unconfirmed)).

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
func bristles(): Integer {
    return 40
}
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

`Tools.Broom` is not in a `system` folder, so it is not walked. In `--final-classes`, `add_systems()` is two
plain calls, `add_system_greet()` and `add_tools_system_sweep()`: the symbol's word in the name is replaced by the
class's dotted name in snake_case. Written with a generic, `Runner<system.class>()` makes one `Runner` per class
found.

Each class walked is known while compiling, so `system.class.has_function('run')` is decided for each one, as
`$system_type.has_function` is in a generic class (D167): only the branch taken is compiled for that class, and it
may call what only that class has.

```gdscript title=folder_ask/system/wave.spite
var console = Console()

func run() {
    console.print("wave runs")
}
```
```gdscript title=folder_ask/system/idle.spite
var seconds = 3
```
```gdscript title=folder_ask/folder_ask.spite entry
var console = Console()

func FolderAsk() {
    ask_systems()
}

func ask_system(system: Symbol<System>) {
    var made: system.class = null
    if system.class.has_function('run') {
        made.run()
    } else {
        console.print(system.name, "waits", made.seconds)
    }
}
```
```output
System.Idle waits 3
wave runs
```

### A name that says when it runs

When the parameter's own name is a word of the function named in the range, that word is a hole, exactly as it
is in a template's own name: `phase: Symbol<$system_type.phase_all>` ranges over the functions whose names fit
`<phase>_all`. The hole matches only the values of the enum named for it, `Phase`, found the way a type written
`Phase` would be from the template's class (D180). So the engine's phases are an enum, and a function whose name
fits the pattern without naming one of its values -- `count_all` below -- is an ordinary function. For
`update_all`, `phase.name` is `"update"` and `phase.value` is the enum value `'update'`. Inside the template, the
pattern written as a member, `system.phase_all()`, calls the matched function. Another template ranging over
`$system_type.phase_all` that is called from inside walks the matched function's arguments. The plural walks the
matched functions in the enum's order, and the engine walks the enum itself (see
[values_and_types.md](values_and_types.md#walking-an-enums-values)), so systems run in the engine's order,
whatever order their files are in:

```gdscript title=name_phases/system/draw.spite
var console = Console()

func render_all() {
    var shapes = count_all()
    console.print("draw", shapes, "shapes")
}

func count_all(): Integer {
    return 3
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
var placed_in: NamePhases.Phase = 'update'

func Runner() {
    place_phases_all()
}

func place_phase_all(phase: Symbol<$system_type.phase_all>) {
    placed_in = phase.value
}

func run() {
    run_phases_all()
}

func run_phase_all(phase: Symbol<$system_type.phase_all>) {
    system.phase_all()
}
```
```gdscript title=name_phases/name_phases.spite entry
enum Phase {
    'update'
    'render'
}

type Runnable {
    placed_in: Phase
    run()
}

var runners = List<Runnable>()
var console = Console()

func NamePhases() {
    add_systems()
    run_phases()
}

func add_system(system: Symbol<System>) {
    var runner = Runner<system.class>()
    runners.append(runner)
    console.print(system.name, "runs in", runner.placed_in)
}

func run_phase(phase: Symbol<Phase>) {
    var index = 0
    while index < runners.count() {
        run_placed(runners[index], phase.value)
        index = index + 1
    }
}

func run_placed(runner: Runnable, phase: Phase) {
    if runner.placed_in == phase {
        runner.run()
    }
}
```
```output
System.Draw runs in render
System.Move runs in update
move
draw 3 shapes
```

`--final-classes` shows what was made: `RunnerMove` has `place_update_all()` and `run_update_all()`,
`add_systems()` calls `add_system_draw()` and `add_system_move()`, and `run_phases()` calls
`run_update()` and `run_render()`. `RunnerDraw` has no `place_count_all()`, since `count` is not a `Phase`. A
program adds a phase by reopening the enum ([packages.md](packages.md#reopening-an-enum-adds-values)), and every
system with a function for it is then run in it.

Calling a single instance names a value, and one the enum does not have is an error listing the ones it does:
`run_updat_all()` here is "'updat' is not a value of 'NamePhases.Phase', which has 'update', 'render'", and
`run_render_all()` in `RunnerMove` says 'Move' has no function 'render_all'. A pattern whose
hole names no enum is an error at the template, asking for `enum Phase` (`diagnostics/hole_values`). As a
condition, `$system_type.has_function("<phase>_all")` asks whether any function fits the pattern, its hole read
the same way. A fuller engine, with rows queried per argument and `run_each` called once per combination, is
`conformance/stage6/system_phases`.

### Passing the symbol to a helper

A function whose `Symbol<...>` parameter is not a word of its own name is not called by a spelled-out name.
Instead the template's own symbol is passed to it, by name, and it is compiled once for each symbol passed, like
the template itself. The range must be the same as the template's, and the helper keeps everything the symbol
means: `phase.name`, the matched function, and the templates over its arguments. That lets a long template be cut
into functions:

```gdscript title=passed_phase/system/wave.spite
var console = Console()

func greet_all() {
    console.print("wave")
}

func leave_all() {
    console.print("wave goodbye")
}
```
```gdscript title=passed_phase/runner.spite
generic $system_type

enum Phase {
    'greet'
    'leave'
}

var system: $system_type = null
var console = Console()

func run() {
    run_phases_all()
}

func run_phase_all(phase: Symbol<$system_type.phase_all>) {
    announce(phase, "before")
    system.phase_all()
    announce(phase, "after")
}

func announce(phase: Symbol<$system_type.phase_all>, word: String) {
    console.print(word, phase.name)
}
```
```gdscript title=passed_phase/passed_phase.spite entry
func PassedPhase() {
    var runner = Runner<System.Wave>()
    runner.run()
}
```
```output
before greet
wave
after greet
before leave
wave goodbye
after leave
```

`RunnerWave` has `announce_for_greet(word: String)` and `announce_for_leave(word: String)`. Anything else passed
there is an error, since there is no symbol to compile it for (`diagnostics/passed_symbol`); a `Symbol` without a
range is still an ordinary value, and a function taking one is an ordinary function.

## Tree shaking

Spite removes what a program does not use, and it can do so exactly, because everything it generates is decided
at compile time:

- A **condition on a codegen value** or on a [`Build`](programs.md#compile-time-settings-build) field is decided
  while compiling, and the branch not taken is never generated: `Weapon<Integer, true>` and `Weapon<Integer, false>` are
  two classes, and each keeps only the branch of `if $is_magic { }` that it takes.
- A **Symbol codegen template** is compiled only for the names a program calls: a program that never calls
  `sum_price()` has no `sum_price`.
- **`has_function`** is decided while compiling, and templates over a function's arguments, a folder's classes
  or a name pattern are expanded into ordinary calls. No list of functions or classes exists at run time.
- **An enum's values** are walked the same way: `Symbol<Phase>` and a pattern's hole become one call per value,
  so no table of an enum's values exists at run time, and a program that never walks an enum pays nothing for it.
- A **generic class** exists only for the values a program gives it, and each of its functions is compiled for
  one set of values only when surviving code calls it; a [constraint](#constraining-what-a-generic-accepts)
  is checked by the compiler and emits nothing.
- **Reflection** is built only where it is read ([reflection.md](reflection.md)), and a generic class such as
  [`JsonWriter` and the rest of json.md](json.md) exist only for the types a program uses them with.
- In a production build, a **function or class** nothing reachable uses is not emitted
  ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).
- The **concurrency machinery** -- the state machines of a `Concurrent` and the loop that runs them, the thread
  pool, atomic reference counts -- exists only in a program that makes a `Concurrent` or a `Parallel`, or is
  built with `--repl-port` or `--hot-reload`
  ([optimizations.md](optimizations.md#concurrency-machinery-only-where-it-is-used)).

This is tree shaking over the program's own model, not dead-code elimination left to the C compiler.

An inspectable build -- `--development`, `--hot-reload`, `--repl` or `--repl-port` -- keeps every function and
class the program generated instead of only the ones reachable from `main`, so live reload has all of them to
swap and the REPL can reach them ([D143](decisions.md), [compiler.md](compiler.md#development-builds-and-tree-shaking)).
What is decided while compiling is decided there too: templates are still generated only for the names called,
and [conditions on codegen values](#codegen-values---implemented) still fold. Every optimisation the compiler
makes on its own, and the builds it applies in, is listed in [optimizations.md](optimizations.md).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Symbol codegen  **[implemented]**

There are no macros. A parameter of class `Symbol` whose name is a segment of its function's name turns the
function into codegen for every name that fits: below, `set_attribute` answers `set_age`, `set_name`, and so on,
for each attribute of the class. Inside, the symbol names that attribute: written as a type (`value:
attribute.class`, `): attribute.class`), it is the attribute's actual type; written as an expression,
`attribute.class` is a `Spite.Class` naming that type, printing just like the type name would --
`"{attribute.class}"` included, since a text hole
holding a value whose class declares `to_string()` calls it (D109's reading of printable, applied to text;
proposed by Claude, unconfirmed; `conformance/stage6/class_text`).

```person.spite
func set_attribute(attribute: Symbol, value: attribute.class) {
    attributes[attribute] = value
}

func get_attribute(attribute: Symbol): attribute.class {
    return attributes[attribute]
}
```

```gdscript
var person = Person(1)
person.set_age(2)
```

A function with the exact name always wins over codegen, for that one name: an exact `set_name` answers a write
of `name` while its read still goes through `get_attribute`. Only the names actually called are generated, in
every build; the template itself emits nothing, and each instance is an ordinary function that costs what its
body costs. A generated `get_<attribute>()` of an owning attribute (a `String`, `List<T>` or `Dictionary<T>`)
returns a retained, independent value, exactly as an explicit getter does, which is what lets D15 treat a field
and a zero-argument function as the same member.

A walk makes an attribute used only when it reads its value: `x.attributes[attribute]` counts as a read of every
attribute it reaches, while `attribute.name` and `attribute.class` describe the attribute without reading it, so
an attribute only they look at is the unread-attribute error (D118, [Unused is an
error](style.md#unused-is-an-error--implemented)).

**Another class's attributes, and every attribute at once** (proposed by Claude, unconfirmed; built for D95's
`Json`).  **[implemented]**

- **`attribute: Symbol<Label>` ranges over `Label`'s members** instead of the template's own class: its
  attributes, and its functions that take no arguments (D15). Inside, `label.attributes[attribute]` is that member
  of the `Label` passed in -- the field, read or written, or a call to the function -- and `attribute.name` and
  `attribute.class` mean what they always did. `List`'s member templates are written this way,
  `member: Symbol<$element_type>` (D91). In a generic class the class is usually the codegen value,
  `Symbol<$value_type>`; when that value is not a class the template answers nothing.
- **`Symbol<Row>` over a `type`** (proposed by Claude, unconfirmed) ranges over the attributes the type names
  and the functions it requires with no arguments; `row.attributes[attribute]` is the shape's own read, write or
  call, answered from the value's class at run time like any read through a `type`, and the plural walks the
  type's attributes in its order (`conformance/stage6/shape_attributes`).
- **The plural calls it for every attribute.** `show_attributes(label, lines)`, for a template `show_attribute`
  whose symbol is `attribute`, calls `show_<name>(label, lines)` once per attribute, in declaration order. It is
  an ordinary generated function whose body is those calls, so it is typed, visible and shaken like any other,
  and the template it repeats must return nothing: "'count_attributes' calls 'count_attribute' once for every
  attribute, so 'count_attribute' has to return nothing" (`diagnostics/every_attribute`). It replaces the
  compile-time `for instance.attributes`, which went with `for`. Over another class's attributes it calls only
  the ones that class lets others read: a private `_` attribute is skipped rather than being the private error
  (proposed by Claude, unconfirmed). **A plural over nothing is an empty function**, not a missing one: over a class
  with no attributes, and over a `Symbol<$value_type>` whose value is no class or `type` -- a number, a `T?`, a
  `List` -- where the single template answers nothing, the plural calls nothing, so one generic compiles for a
  marker and for a number alike (`conformance/stage6/empty_plural`).

```gdscript
func show_attribute(attribute: Symbol<Label>, label: Label, lines: List<String>) {
    lines.append("{attribute.name}: {label.attributes[attribute]}")
}
```

`conformance/stage6/every_attribute`, `docs/metaprogramming.md`.

### A class's functions, a folder's classes and a name's pattern  **[implemented; the spellings proposed by Claude, unconfirmed]**

D114, D115 and D116 (decided by Mortaro) are one mechanism: the Symbol templates above, ranging over three more
things a program already has -- a function's arguments, a folder's classes, and the functions whose names fit a
pattern -- plus one question a generic asks of its type. Everything is decided while compiling: each walk expands
into ordinary calls, there is no registry and no list walked at run time, and nothing is generated for a name no
program calls (D177). The spellings below are Claude's, chosen to be the existing forms read one step further:

- **`$system_type.has_function('run_each')` in a condition folds like `if $is_magic`** (D114). The name is a
  literal -- a symbol, or text holding a pattern such as `"<phase>_each"` (D116), which is true when some
  function's name fits it with a non-empty middle. Only the taken branch is compiled, so it may call what only
  that type has; any other argument is an error: "'$system_type.has_function(...)' is decided while compiling, so
  the name it asks for is written as a literal" (`diagnostics/function_reflection`). It is an ordinary member of
  `Spite.Class` (`library/spite/class.spite`, D92), so `klass.has_function("boost")` also answers at run time,
  from `.functions` -- which exists only in a program that reads it ([reflection.md](reflection.md)). Folded,
  it counts the functions the class declares: not its constructor, not a `_` function, and not what a template
  generated for it. **It folds wherever it is written**, not only as an `if`'s condition: in a `return`, a `var`,
  or on either side of `and`, `or` and `not`, `$system_type.has_function("run_each")` is the constant `true` or
  `false`, exactly the value the `if` form decides on, and no class's table of functions is built to answer it
  (`conformance/stage6/folded_function_value`, whose allocations are pinned). **The class a template walks is asked
  the same way** (D167 read for walks): inside a template over `Symbol<System>` (or any range whose symbol is a
  class), `system.class.has_function("run_each")` with a literal name folds for each class walked, and only the
  branch taken is compiled for it; with a name that is not a literal it is the run-time question of
  `Spite.Class`, since `system.class` is also an ordinary value (`conformance/stage6/symbol_class_function`).
- **`$system_type.function_waits("update_each")` folds exactly like `has_function`** (D209, decided by Mortaro;
  the spelling and the reading below are proposed by Claude, unconfirmed).  **[implemented]** It is `true` when a
  function the class declares, whose name fits the literal (a name, or a pattern whose hole is read as
  `has_function`'s), can reach a wait, and `false` otherwise -- also for a name the class does not declare. It
  folds wherever it is written (an `if`, a `var`, a `return`, either side of `and`, `or` and `not`), for each class
  a `Symbol<...>` walk visits as `system.class.function_waits(...)`, and a name that is not a literal is the error
  `has_function` gives, naming `function_waits`. It is a function of `Spite.Class` too, so
  `klass.function_waits(name)` answers at run time from `.functions`, through `Spite.Function.waits()`.
  - **What can wait.** A function waits when it is one of the waits a state machine returns from -- `Program.sleep`,
    reading the console, reading or writing a `File`, a `Socket`'s `accept_client`, `read_line` and `read_bytes`,
    reading or dropping a `Concurrent` ([concurrency.md](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows)) --
    or calls one, transitively. Over calls made by name this is exactly the set D176 compiles into state
    machines. Three more calls count, conservatively, where D176 runs the wait in place instead of returning from
    it: a constructor, a call through a union's dispatch or a `type`, and a call through a function value. A call
    through a function value of one signature may wait when some function of that signature that the program
    makes into a value can wait; a function value handed straight to `Concurrent(...)` or `Parallel(...)` is not
    counted, since those run it as a state machine or on the pool, never through such a call. So a system whose
    `update_each` runs `guard.while_locked(tick)`, where `tick` sleeps, waits, and a `Concurrent` of it runs it to
    the end where it is made, as for any function without a state machine. Joining a `Parallel` is not a wait
    here, as it is not in D176.
  - **A question cannot decide itself.** A function that asks is compiled after every function that does not, so
    its answer sees all of them. When the answer depends on a function that itself asks (or on a function value
    made in one), the branch chosen could change the answer, so the compiler checks every answer again once
    everything is compiled, and one that changed is an error at the question: "whether 'Looper.update_each' can
    wait was answered false here, before the functions that ask 'function_waits' were compiled, and it is true
    once they are: ... (D209) -- ask from a function that 'Looper.update_each' does not reach"
    (`diagnostics/function_waits_paradox`).
  - **Cost.** Folded, nothing: no table, no class object. Asked at run time, the program carries one function
    comparing a function's address against those of the functions it makes into values that can wait, and keeps
    those functions (`conformance/stage6/waiting_systems`).
- **`argument: Symbol<$system_type.run_each>` ranges over the arguments of `run_each`** (D114). A `Symbol<X>`
  already ranged over X's members; a function's members are its arguments. Inside, `argument.name` is the
  argument's name and `argument.class` its type, written as a type (`Query<argument.class>()`,
  `): argument.class`, and `argument.class.element_type` for a `List<Row>` argument) or read as a
  `Spite.Class`. The plural (`count_arguments()`) calls the template once per argument, in order. When
  `$system_type` has no `run_each`, the template answers nothing and the plural calls nothing.
- **A template over arguments that returns a value fills a call** (D114): its plural, written as the whole
  argument list of a call to that same function, passes one value per argument --
  `system.run_each(row_arguments())` is `system.run_each(row_potion(), row_target())`, evaluated left to
  right. That is how one generic calls a function of any arity without variadic generics. Anywhere else such a
  plural is an error naming the call it belongs in, and so is passing it to a different function
  (`diagnostics/function_reflection`). D77's rule does not apply to it: the call it stands for is
  written by the compiler.
- **`system: Symbol<System>` ranges over the classes of every folder named `system`** (D115): `System.Heal`,
  `Ui.System.Interact` -- a range that names no class or type is read as the end of a dotted namespace, and the
  classes of every namespace ending in it are walked in order of their dotted names. Inside, `system.class` is
  the class (so `Runner<system.class>()` instantiates the generic per class) and `system.name` its dotted name;
  the single instance replaces the symbol's word with the whole path in snake_case (`add_system` for
  `Ui.System.Interact` is `add_ui_system_interact`, for `System.Heal` `add_system_heal`). A generic class there
  is skipped, since it has no values to be made with. A range that matches no folder at all walks nothing, like a
  class with no attributes, so an engine that walks `Symbol<Recipe>` compiles for a program with no recipes
  (proposed by Claude, unconfirmed). A typo is caught where a single class is named instead: "'cook_recipe_bread'
  is 'cook_recipe' for a class 'recipe_bread', but no folder is named for 'Recipe'", or, when the folders exist,
  a list of the classes they do hold (`conformance/stage6/empty_folder_range`, `diagnostics/empty_folder_range`).
- **`phase: Symbol<$system_type.phase_each>` ranges over the functions whose names fit `<phase>_each`** (D116):
  when the parameter's own name is a word of the function name in the range, that word is the hole, exactly as
  it is in a template's own name. `update_each` gives `phase` = `'update'`; `run_each_before_phase` would match
  `run_each_before_render`. Inside, `phase.name` is the matched text, and the pattern written as a member --
  `system.phase_each(...)` -- calls the matched function; another template ranging over `$system_type.phase_each`
  from inside walks that function's arguments, its instances named for it (`row_potion_in_update_each`) so two
  matched functions never share one. The engine owns the list of phases and decides what they mean; the language
  only reads the name.
- **The hole matches only the values of an enum** (D180, decided by Mortaro; how the enum is found proposed by
  Claude, unconfirmed). The engine's phases are an enum, and the enum is the one named for the hole --
  `Phase` for `phase`, `RenderStep` for `render_step` -- resolved as a type written that way would be from the
  template's class (its own class, its folders, then the whole program when only one class declares it). A
  function that fits the pattern without naming a value, such as a helper `count_all` when `count` is not a
  `Phase`, is an ordinary function. The plural (`run_phases_each()`) walks the matching functions **in the enum's
  order**, not their declaration order, and `phase.value` is the matched value, typed as the enum, so an engine
  stores and compares phases as values rather than text. A hole no enum is named for is an error at the template:
  "the hole '<stage>' of '<stage>_all' matches only the values of an enum named for it, and there is no enum
  'Stage'"; calling an instance for a name the enum does not have is an error listing its values ("'updat' is not
  a value of 'Runner.Phase', which has 'update', 'render'"), and one for a value the type has no function for
  says so (`diagnostics/hole_values`). A folded `$type.has_function("<phase>_each")` reads its hole the same way;
  `has_function` and `name_fits` answered at run time, from `.functions`, still take any text in the hole. A
  program adds a phase by reopening the enum ([packages.md](packages.md#reopening-an-enum-adds-values)), and
  every system with a function for it is then run in it (`conformance/stage6/system_phases`, where a system's
  `sum_all` helper stays ordinary). Walking the enum emits one call per value and no table of its values (D177).
- **A template's symbol is passed to a helper by name** (proposed by Claude, unconfirmed). A function whose
  `Symbol<...>` parameter is not a word of its own name --
  `run_combination(phase: Symbol<$system_type.phase_each>, combination: Integer)` -- is not
  an ordinary function taking a run-time `Symbol`: it is a template compiled once for each symbol passed to it,
  and the only thing its symbol argument may be is the calling template's own symbol over the same range, written
  by name: `run_combination(phase, combination)`. Its instance is `run_combination_for_update`, keyed
  `_in_<function>` as above when its range is a function's arguments inside a pattern; inside it `phase` means
  everything it meant in the caller, so `system.phase_each(row_arguments())` works there too. Anything else passed
  is an error, and so is a call from outside such a template: "'announce' is compiled once for each 'phase' of
  'Symbol<$system_type.phase_all>', so its 'phase' can only be the symbol of a template over that same range,
  passed by name from inside it" (`conformance/stage6/passed_symbol`, `diagnostics/passed_symbol`). A plain
  `Symbol` parameter, with no range, stays a run-time value.

```gdscript
func add_system(system: Symbol<System>) {
    var runner = Runner<system.class>()
    runners.append(runner)
}

func run_phase_each(phase: Symbol<$system_type.phase_each>) {
    counts.clear()
    count_arguments()
    ...
    system.phase_each(row_arguments())
}

func row_argument(argument: Symbol<$system_type.phase_each>): argument.class {
    var query = Query<argument.class>()
    ...
}
```

`--final-classes` shows the result as ordinary functions: `add_system_drink_potion()` holding
`Runner<System.DrinkPotion>()`, and `RunnerDrinkPotion` with `run_update_each()`, `row_potion_in_update_each():
Potion` and the rest. `conformance/stage6/system_functions` (a `run_each` of two rows and a `run_all` of a list,
chosen by `has_function`), `conformance/stage6/system_folder` (`system/` and `ui/system/` found with no list),
`conformance/stage6/system_phases` (two phases run in the engine's order, not the folder's),
`docs/metaprogramming.md`, `docs/reflection.md`.

### Codegen values (`$`)  **[implemented]**

`$name` means "replaced at code generation". That is its only meaning, everywhere it appears.

D87 (decided by Mortaro, superseding D9's constructor list): **a class declares each codegen value on a `generic`
line of its own, at the top of the file**, "instead of constructor":

```weapon.spite
generic $damage_type
generic $is_magic

var damage: $damage_type = 0

func Weapon(new_damage: $damage_type) {
    damage = new_damage
}

func hit(): Integer {
    if $is_magic {
        return 1
    }
    return 2
}
```

```gdscript
var sword = Weapon<Integer, true>(10)
```

- **Every value is declared, even a single one**, so skimming the top of a file is how a human sees what a class
  accepts. The lines come after a `singleton` line and before everything else (D67); one line declares one value,
  and a comma is an error naming the line to write: "a 'generic' line declares one codegen value: write 'generic
  $left_type' and put the next one on its own line".
- **Call sites are positional, always, in the order of the lines.** There is no named form: `List<Integer>()`,
  `Weapon<Integer, true>(10)`.
- **`$` is for generics only** (D76, decided by Mortaro, superseding D9's "undeclared means supplied by a
  compiler flag"). Every `$name` a class uses has a `generic` line and is supplied by the caller, so the lines
  are exactly "what a caller must pass". A `$name` with no line is a compile error. Used as a value it points at
  [`Environment`](programs.md#program-settings-environment--implemented), which is where a program's settings
  live: "'Game' has no 'generic $verbose' line, and '$' is for generics only: a program's settings are fields of
  Environment -- ..."; used as a type it is "this class has no 'generic $other_type' line, so '$other_type' cannot
  be used as a type" (`diagnostics/codegen_values`).
- **Only a class has codegen values; there are no generic functions** (D123, decided by Mortaro: "function
  generics can ONLY come if the class has a generic declared for it"). A function reads the `generic` lines of its
  class and declares none of its own; a function that must accept any class takes a `type` -- the built-in empty
  `Anything` (D163) accepts every class -- and `value.class` is the real class
  ([reflection.md](reflection.md#an-attributes-value)). Listing values on a function is the D9 error below.
- **A class needs no constructor to take codegen values.** That was D9's cost, and the reason for D87:
  `library/list.spite` is `generic $element_type` and its functions, and `library/dictionary.spite` is `generic
  $value_type`; an empty constructor is still an error.
- **Every hole must be filled.** There are no defaults. A call supplying the wrong number is a compile error
  naming the class's codegen values, in order -- "'Box' takes 1 codegen value(s), in this order: $held_type --
  but 2 were given" -- so the mistake is corrected from the message rather than by opening the class. A `$name`
  that is not declared is an error too -- which is what a typo like `$is_magik` produces.
- Reordering the `generic` lines changes what every existing positional call site means. Where the values
  have different kinds (a class versus a `Boolean`) the compiler catches it immediately; where they are the same
  kind (`Pair<Integer, String>` swapped) it compiles and means something else, and the tests are what catch it. This
  is a deliberate, accepted trade (Mortaro).
- **Each set of values is its own class**, made only where a program names it, with its own copy of every
  function surviving code calls. Nothing chooses between them at run time; what a generic costs is that code,
  once per set of values used, and a generic nobody makes costs nothing (D177). `--final-classes` prints each
  class with its codegen values already bound, which is where `true` reads as `is_magic` again.
- Writing `generics` is a parse error naming this form (as `for`, `&` and `Heap<T>` already are), and so is D9's
  `func Weapon<$damage_type, $is_magic>(...)`, on a constructor or any other function: "a function no longer
  lists codegen values between '<' and '>': declare each at the top of the file on its own line ('generic
  $damage_type', 'generic $is_magic'), and write 'func Weapon(' here" (`diagnostics/constructor_codegen_list`).
  A `generic` line inside a function is an error too.
- **The values can be left out when the constructor's arguments say them** (proposed by Claude, unconfirmed;
  generalised for D138): each parameter whose declared type mentions a `$name` is
  matched against its argument's type -- `$name` itself, `$name?` (a `T` or a `T?` both give `T`), `List<$name>`,
  `Dictionary<$name>`, and the arguments and return of a `Spite.Function<...>`. A `null` argument says nothing.
  Only when every `$name` is found; otherwise the error asks for them between `<` and `>`: "'Box' takes 1 codegen
  value(s), in this order: $held_type -- write them between < and > before the arguments". `Pair("Hero", 7)`,
  `Concurrent(file.read)`, `JsonWriter(order)`.
- `null` as the default of an attribute typed by a codegen value means that type's own default, not a `T?`
  (provisional, [open question 1](open_questions.md#open-questions)).

**A `generic` line may name a constraint** (D175, decided by Mortaro). **[implemented]**
`generic $item_type: Printable` accepts only types that fit the `type` `Printable`, and a use that does not
(`Shelf<Pet>`) is an error where the class is named, naming the constraint, the class and what it lacks --
"'Pet' does not fit type 'Printable', which 'Shelf' requires of $item_type: it has no function 'to_string'" --
instead of an error deep inside the generic's body. The constraint is optional and uses an existing `type`,
resolved like any type name from the generic's file. What follows is Claude's reading where D175 is not specific
(proposed by Claude, unconfirmed):

- **Fitting is what [Types](values_and_types.md#types) already means by it**: the class a `type` would admit
  fits -- a class with the listed functions and attributes, `String`, a number, an enum, a `List<T>` or
  `Dictionary<T>` with what the `type` needs, and the `type` itself. A `T?` does not fit ("'Integer?' does not fit
  type 'Printable', which 'Shelf' requires of $item_type: it may be null, and null has none of what the type
  needs"), and neither does a function value.
- **A constraint names a `type`, not a class, a union or a number type**: anything else is an error on the
  `generic` line -- "'$count_type' is constrained by 'Integer', which is not a 'type': a constraint names a
  'type' the class given must fit, like 'generic $count_type: Printable'". Constraining by a union ("one of these
  classes") is not built.
- **It is checked once per class given**, where the compiler first makes that instance, written out or read
  from the constructor's arguments. After the error the generic is compiled as if it had been given the `type`
  itself, so its body adds no errors of its own.
- **It costs nothing at run time and tree-shakes as before**: the check is the compiler's alone, and the
  instance made is the one an unconstrained line would make.

`conformance/stage6/generic_constraints`, `diagnostics/generic_constraints`,
`diagnostics/generic_constraint_not_a_type`, `docs/metaprogramming.md`.

**Conditions on codegen values fold.** They are decided at compile time and the untaken branch is removed (tree
shaking), in every build, `--development`, `--hot-reload` and the REPL builds included (the D143 row that settled
what `--development` does): a codegen value is part of which class this is -- `Weapon<Integer, true>` and
`Weapon<Integer, false>` are two classes -- so there is no run-time value for live reload to change. To change
one, change the call site.

**A class test that can never be true for one instantiation folds to `false`** (D167, decided by Mortaro): inside
a generic class, `if item == Health { }` where `item`'s type comes from a codegen value that is not `Health` in
this copy is `false`, and its branch is removed from that copy only -- "generics being possibly unused code is
used for tree shake, its on purpose not an error". Outside generics the never-true test stays D75's error
([control_flow.md](control_flow.md#control-flow--implemented)). A codegen value bound to a class is itself a
class test on the right of `==`, `item == $wanted_type`
([values_and_types.md](values_and_types.md#unions--implemented)).

**Asking what type a generic was given** (proposed by Claude, unconfirmed; built for D95's `Json`).
**[implemented]** When a codegen value is a type, `$value_type == String` is decided at compile time like any
other condition on a codegen value, and only the branch taken is compiled -- so each branch may use what only
that type has, which is what lets one generic class treat text, numbers, lists and classes differently. It is
the class test (D75, [control_flow.md](control_flow.md#value--class)) asked of a type instead of a value. A type
name asks for exactly that type; four names ask for a kind, since the type has arguments the test does not want
to spell:

| Test | True when the type is |
|---|---|
| `$value_type == List` | any `List<T>` |
| `$value_type == Dictionary` | any `Dictionary<T>` |
| `$value_type == Null` | any `T?` -- `Null` is a member of the union a `T?` is (D45) |
| `$value_type == Symbol` | an enum, or `Symbol` -- an enum is a closed list of symbols (D10) |
| `$value_type == Enum` | an enum only, not a plain `Symbol` (proposed by Claude, unconfirmed: `JsonReader` and `BinaryFormat` need it to read a plain `Symbol` through `Symbol(text)` and an enum through the text cast) |

A union name is true for any of its members. Such a test always folds, `--development` included, because the
branch it rules out would not compile.

**Only what survives folding is compiled** (proposed by Claude, unconfirmed). A function of a generic class is
compiled, and so type-checked, for one instantiation only when code already compiled for the program names it --
a call that survived folding, a function value, a reflection table, or a call through a `type` the class is a
member of -- so a helper reached only from a branch the instantiation rules out is never checked against that
type. When a folded `if` (or `else if` chain) takes a branch that ends in `return`, the statements after it in the
same block are not compiled either, and the names they read count as used, as for the untaken branch. This holds
in every build, inspectable ones included. A class that is not generic still compiles every function, so a
mistake in an uncalled one is still reported (D140). `conformance/stage6/folded_helpers`.

**The types a type was built from are read by their codegen names**: `$value_type.element_type` for a
`List<$element_type>`, `$value_type.value_type` for a `Dictionary<$value_type>` or a `$value_type?`, and a generic
class's own names for one of its instances -- the names this section already gives the containers. Reading a name
the type does not have is an error listing them: "a List<Integer> has no codegen value named '$value_type' --
List<$element_type>, Dictionary<$value_type> and $value_type? name theirs, and a generic class names its own"
(`diagnostics/every_attribute`). `conformance/stage6/every_attribute`, `docs/metaprogramming.md`.

**A codegen value that is a type reads as its class** (proposed by Claude, unconfirmed): a member
read through it, `$component_type.name` or `$component_type.attributes`, is read from the bound class's
`Spite.Class`, exactly as `Health.name` is -- no value of the type is made. `$component_type` alone in an
expression is still the "is a type here" error (`conformance/stage6/codegen_class_name`).

The `generic` line replaced the `generics` header, which "feels outside of our patterns" (D5), and D9's list on
the constructor, which forced a constructor on every generic class (D87); it is a declaration like `var`, and a
header pattern shared with `singleton` (D104). The alternatives tried on the way are in the decision log.
