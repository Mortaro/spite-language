# Metaprogramming

There are no macros. **Every class is an instance of `Spite.Class`**, every namespace an instance of
`Spite.Namespace` and every function an instance of `Spite.Function`, so "do this for every attribute" is ordinary
code over ordinary objects: `Monster.attributes.each(show)`. The compiler knows those objects while compiling, so
it runs that code there: a walk is unrolled into one call per attribute, each call compiled for its own attribute
and typed by it, and nothing is looked up when the program runs.

Two more tools complete it. A **template** is a function whose name says what it is given, like `show_health(troll)`
or `filter_alive()`: the compiler writes one copy per name a program calls. A **generic** is a class that takes codegen
values, `List<Integer>`. All three are ordinary functions and classes, so what they make is typed, visible in
`--final-classes`, and removed when nothing calls it ([Tree shaking](#tree-shaking)).

```gdscript title=walks_and_templates/monster.spite
var name = "troll"
var health = 10

func Monster(new_name: String, new_health: Integer) {
    name = new_name
    health = new_health
}

func alive(): Boolean {
    return health > 0
}
```
```gdscript title=walks_and_templates/walks_and_templates.spite entry
var console = Console()

func WalksAndTemplates() {
    Monster.attributes.each(describe)
    var troll = Monster("troll", 10)
    show_health(troll)
    var monsters = List<Monster>()
    monsters.append(troll)
    var ghost = Monster("ghost", 0)
    monsters.append(ghost)
    var living = monsters.filter_alive()
    var total = living.sum_health()
    var living_count = living.count()
    console.print("alive", living_count, "with", total, "health")
}

func describe(attribute: Spite.Attribute) {
    console.print("Monster has", attribute.name, "of class", attribute.class)
}

func show_attribute(attribute: Spite.Attribute<Monster>, monster: Monster) {
    console.print(monster.name, attribute.name, monster.attributes[attribute])
}
```
```output
Monster has name of class String
Monster has health of class Integer
troll health 10
alive 1 with 10 health
```

`describe` is compiled twice, once per attribute, and the `each` is two calls to those copies. `show_health` is the
template `show_attribute` written for `health`, and `filter_alive` and `sum_health` are the standard library's
member templates written for `alive` and `health`. The objects and every member they answer are
[reflection.md](reflection.md)'s; this page is what a program builds with them.

## Walking a program's structure

A walk is a call on a list of reflection objects. Every list a reflection object has is an ordinary `List`, so a
walk is `each`, a selection is a filter or a member template, and a member by name is `[]`, which answers it or
`null`:

| To reach | Write |
|---|---|
| every attribute of a class | `Monster.attributes.each(show)` |
| one function, if the class has it | `Runner.functions['run_each']`, a `Spite.Function?` narrowed by `if` or `assert` |
| the functions whose names end in `_each` | `Runner.functions.filter_name_ends_with("_each")` |
| a function's arguments | `function.arguments.each(describe)` |
| an enum's values, in order | `Phase.values` |
| every folder named `part`, anywhere | `Spite.Namespace.instances.filter(holds_parts)`, then `namespace.classes` |
| every class of the program | `Spite.Class.instances` |

A name is selected, never built from text: `[]` takes a name written in the code, and anything else is a filter
over the names that exist. Each of these is a constant while compiling, so a walk is unrolled and the function it
calls is compiled once for each element ([reflection.md](reflection.md#known-while-compiling)). A function whose
result is typed by its reflection parameter answers a different class in each copy:

```gdscript title=walked_arguments/runner.spite
var total = 0

func update_each(amount: Integer, label: String) {
    total = total + amount + label.length()
}

func render_each(scale: Float) {
    if scale > 1.0 {
        total = total + 1
    }
}

func reset() {
    total = 0
}
```
```gdscript title=walked_arguments/walked_arguments.spite entry
var console = Console()

func WalkedArguments() {
    var phases = Runner.functions.filter_name_ends_with("_each")
    phases.each(describe)
    var count = Runner.functions.count_name_starts_with("re")
    console.print(count, "functions start with re")
}

func describe(function: Spite.Function) {
    console.print(function.name)
    function.arguments.each(describe_argument)
}

func describe_argument(argument: Spite.Argument) {
    var fresh = made(argument)
    console.print(" ", argument.index, argument.name, "starts as '{fresh}'")
}

func made(argument: Spite.Argument): argument.class {
    return argument.class()
}
```
```output
update_each
  0 amount starts as '0'
  1 label starts as ''
render_each
  0 scale starts as '0'
2 functions start with re
```

`count_name_starts_with("re")` counts `render_each` and `reset`. A walk over nothing walks nothing: a class with no
attributes, or a filter that keeps none, calls the function zero times.

### Asking a question while compiling

Every question a reflection object answers is a get-only attribute, so asking one is reading it, and on a constant
the answer is a `true` or `false` in the generated code. An `if` on it keeps only the branch taken:

```gdscript title=asking_functions/loader.spite
var loaded = List<String>()

func load_each(path: String, into: List<String>) {
    var file = File(path)
    var text = file.read()
    assert text
    into.append(path)
    loaded.append(text)
}

func tick_each(amount: Integer) {
    loaded.append("tick {amount}")
}
```
```gdscript title=asking_functions/asking_functions.spite entry
var console = Console()

func AskingFunctions() {
    var loading = Loader.functions['load_each']
    if loading {
        console.print("load_each can wait:", loading.is_resumable)
        console.print("it changes 'into':", loading.arguments[1].is_mutated)
        console.print("it changes 'path':", loading.arguments[0].is_mutated)
    }
    var ticking = Loader.functions['tick_each']
    if ticking {
        console.print("tick_each can wait:", ticking.is_resumable)
    }
    console.print("a Loader has state:", Loader.is_stateful)
}
```
```output
load_each can wait: true
it changes 'into': true
it changes 'path': false
tick_each can wait: false
a Loader has state: true
```

`is_resumable` is true when a function can reach a wait (here the file read), so it is also compiled as a state
machine that pauses there; an engine asks it to start such a system as a `Concurrent` it polls between frames and
to call every other system directly ([concurrency.md](concurrency.md)). `is_mutated` follows every call, however deep:
`into.append(path)` changes the list passed as `into`, and nothing changes `path`.

What a function does with each of its attributes and arguments is one dictionary, keyed by name, whose values are
`Spite.Access`es, and a dictionary takes the member templates over its values:

```gdscript
var update = Movement.functions['update_each']
assert update
var written = update.accesses.filter_written()
var only_read = update.accesses.count_read()
```

**A question that means a program is wrong is a `crash`**, and a `crash` whose condition is decided while compiling
and is false is a compile error where the program reaches it ([the rules](#codegen-values-)). A library writes the
rule it keeps, and a class that breaks it fails the build at the line that breaks it, not the run:

```gdscript
func run_between_frames(system: $system_type) {
    var update = $system_type.functions['update_each']
    crash update
    crash not update.arguments[0].is_mutated
}
```

In a generic class, `$T` is a constant too, so a question on it folds for each class the generic is given, and the
branch it rules out may call what that class does not have:

```gdscript title=generic_questions/caller.spite
generic $target_type

func call(target: $target_type): String {
    if $target_type.functions['run_each'] {
        target.run_each(3)
        return "{$target_type.name} runs"
    }
    return "{$target_type.name} has no run_each"
}
```
```gdscript title=generic_questions/walker.spite
var steps = 0

func run_each(amount: Integer) {
    steps = steps + amount
}
```
```gdscript title=generic_questions/rock.spite
var weight = 3
```
```gdscript title=generic_questions/generic_questions.spite entry
var console = Console()

func GenericQuestions() {
    var walker = Walker()
    var walkers = Caller<Walker>()
    var walked = walkers.call(walker)
    console.print(walked, walker.steps)
    var rock = Rock()
    var rocks = Caller<Rock>()
    var rocked = rocks.call(rock)
    console.print(rocked, rock.weight)
}
```
```output
Walker runs 3
Rock has no run_each 3
```

`Caller<Rock>` keeps only its last `return`, so `target.run_each(3)` is never compiled for a `Rock`.

## Templates: when the name says what it is given

A walk is for "every member". A **template** is for one member named at the call, where the name reads as well as
an argument would: `show_health(troll)` reads like `show(troll.health)`, and `filter_alive()` like "those that are
alive". A parameter of type `Spite.Attribute<Monster>` whose name is a word of its own function's name makes the
function a template: `show_attribute(attribute: Spite.Attribute<Monster>, ...)` answers `show_name` and
`show_health`, one copy for each name a program calls. Inside, `attribute` is the constant `Spite.Attribute` of that
member, so `attribute.class` is its type, and `monster.attributes[attribute]` is that member of the `Monster` passed
in, read or written. The member may also be a function that takes no arguments: the read is then a call to it.

```gdscript title=name_templates/label.spite
var text = "fragile"
var copies = 2

func urgent(): Boolean {
    return copies > 1
}
```
```gdscript title=name_templates/name_templates.spite entry
var console = Console()

func NameTemplates() {
    var label = Label()
    show_text(label)
    show_urgent(label)
    double_copies(label)
    show_copies(label)
}

func show_attribute(attribute: Spite.Attribute<Label>, label: Label) {
    console.print(attribute.name, "is", label.attributes[attribute])
}

func double_attribute(attribute: Spite.Attribute<Label>, label: Label) {
    label.attributes[attribute] = label.attributes[attribute] * 2
}
```
```output
text is fragile
urgent is true
copies is 4
```

A template over a class's own members is how a class answers `set_age`, `get_age` and the rest without writing
each one. An exact function always wins over a template, for that one name and direction:

```gdscript
var age = 0
var name = ""

func set_attribute(attribute: Spite.Attribute<Person>, value: attribute.class) {
    attributes[attribute] = value
}

func get_attribute(attribute: Spite.Attribute<Person>): attribute.class {
    return attributes[attribute]
}

func set_name(new_name: String) {
    name = new_name.upper_case()
}
```

Writing `person.name` goes through `set_name`, while reading it goes through the `get_attribute` template, since no
exact `get_name` exists. A template whose parameter is not a word of its name is not a template: it is an ordinary
function taking a `Spite.Attribute`, which a walk calls or a caller hands a constant to.

### Member templates

The standard library's list functions `filter_<member>()`, `sum_<member>()`, `count_<member>()` and the rest are
templates over the element's members ([collections.md](collections.md)), written in Spite in `library/list.spite`
with a parameter `member: Spite.Attribute<$element_type>`, so `item.attributes[member]` reads a member of an
element. A generic class writes its own the same way:

```gdscript
func average_member(member: Spite.Attribute<$element_type>): Float? {
    if item_count == 0 {
        return null
    }
    var total = sum_member(member)
    return total.to_float() / item_count.to_float()
}
```

A member template chains through a function of the member: `filter_<member>_<function>(arguments)` keeps the
items whose `member` answers `function(arguments)`, so `players.filter_name_starts_with("a")` keeps the players
whose `name` starts with `a`, and `functions.filter_name_ends_with("_each")` does the same for functions. Nothing in
it is special to reflection.

**A template that collects the members' values names the member in the plural**, because that is what it gives
back. Every other template keeps the member singular, because its name asks a question of each element:

| Template | Member | Reads as |
|---|---|---|
| `map_<members>` | plural | "the names": `classes.map_names()` answers a `List` of names |
| `filter_<member>`, `remove_where_<member>` | singular | "those that are alive": `filter_alive()` |
| `count_<member>`, `any_<member>`, `all_<member>` | singular | "how many / any / all are alive" |
| `sum_<member>` | singular | "the sum of health": `sum_health()` |
| `find_by_<member>`, `sort_by_<member>` | singular | "find by name", "sort by health" |

The plural comes from `String.pluralize()`, the rules every English plural follows plus a table of irregular and
uncountable words a program can add to ([standard_library.md](standard_library.md#the-string-class)): `map_names` is
`name`, `map_people` is `person`, and `map_health` is `health`, its own plural. A question is never collected:
`monsters.map_is_alive()` is an error pointing at `filter_is_alive()`, `count_is_alive()`, `any_is_alive()` and
`all_is_alive()`, which are what a question is asked for. A value worth collecting that is not already a member
becomes one: a get-only attribute of the element's class, collected with its plural.

A list of a union keeps one member class with that class in the plural, and narrows to it, while an attribute every
member has is templated as on any list:

```gdscript
var entries = Directory("levels").entries()
var files = entries.filter_files()
var sources = entries.filter_name_ends_with(".spite")
var names = entries.map_names()
```

`entries()` is a `List<Directory or File>`; `filter_files()` answers a `List<File>`. A name that matches both a member
class and an attribute is a compile error naming the fix.

### Templates and walks, side by side

A template is not a loop in disguise. "Every attribute" is a walk, `label.attributes.each(show)`, and "this
attribute" is a template, `show_copies(label)`; there is no third form. The two meet in the same function: a
function taking a `Spite.Attribute` is a template when its parameter is a word of its name and is called by the
member's name, and an ordinary function when a walk calls it. Either way it is compiled once per member it is used
for.

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
that lists the class's values in order (the second program below). A generic class needs no constructor
(`library/list.spite` is `generic $element_type` and its functions), and when the constructor's arguments say
every value, the `<...>` may be left out: `Pair("Hero", 7)` is a `Pair<String, Integer>`, `JsonWriter(order)` a
`JsonWriter<Order>` ([json.md](json.md)) and `Concurrent(file.read)` a `Concurrent<String?>`
([concurrency.md](concurrency.md)). `Pair` may declare `var left: $left_type = null` because its constructor
assigns `left` at once; anywhere else `null` belongs to `T?` alone, and `var left = $left_type()` makes
the default of whatever `$left_type` is bound to.

Only a class has codegen values. A function never lists its own (`func pick<$value_type>(...)` is an error), and
there are no generic functions to write instead: a function that takes any class takes a
`type`, such as the built-in empty `Anything` ([reflection.md](reflection.md#known-only-at-run-time)), or it belongs to a
generic class. The exact forms and error texts are in [Codegen values](#codegen-values-).

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
not is an error where it is named, saying which function or attribute is missing, rather than an error deep inside the
generic's body, the first time some function happens to call what the class lacks.

```gdscript title=generic_constraint/shelf.spite
generic $item_type: Printable

var items = List<$item_type>()

func add(item: $item_type) {
    items.append(item)
}

func describe(): String {
    var first = items.first()
    crash first
    return first.to_string()
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

The constraint is optional and names an existing `type` (`Printable` is `Console`'s, found like any type name,
and a `type` of the generic's own file works too). What fits is what the `type` would accept anywhere, so
`Shelf<Integer>` above is fine, while a `T?` or a function value is not. It is checked wherever a class is given,
written out or read from the constructor's arguments, and it costs nothing at run time: the check is the
compiler's, and the class made is the one an unconstrained line would make
([the rules](#codegen-values-)).

### Asking what a generic was given

When a codegen value is a type, `if $value_type == String { }` asks which type it is. The answer is known when
the class is made, so only the branch taken is compiled, and each branch may use what only that type has. Besides
naming a type exactly, four names ask for a kind: `List`, `Dictionary`, `Null` (any `T?`) and `Symbol` (an enum),
and a `type` name asks whether the type fits it (`$value_type == Number` for any number class).
The table is in [the rules](#codegen-values-).

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
or a `$value_type?`, and a generic class's own names for one of its instances. They are tested like `$value_type`
itself, `else if $list_type.element_type == Float`, and fold in every branch of the chain.

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
copy cannot use is what tree shaking is for ([control_flow.md](control_flow.md#value--class)).

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
the type made to ask. On its own, `$name` is still not a value: only a member read through it is.

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

### Asking whether a class fits a Vector

A `Vector` keeps its items inline, so only a class of known size can be one
([collections.md](collections.md#vectort)). `$component_type.is_fixed_size` asks that while
compiling, so a generic keeps a class that fits in a `Vector` and any other as references in
a `List`, and never makes the `Vector` it could not:

```gdscript title=fits_vector_doc/store.spite
generic $component_type

var inline = $component_type.is_fixed_size

func describe(): String {
    if $component_type.is_fixed_size {
        var packed = Vector<$component_type>()
        var packed_count = packed.count()
        return "inline, {packed_count} so far"
    }
    var listed = List<$component_type>()
    var listed_count = listed.count()
    return "as references, {listed_count} so far"
}
```
```gdscript title=fits_vector_doc/fits_vector_doc.spite entry
var console = Console()

func FitsVectorDoc() {
    var points = Store<Vector3>()
    var point_answer = points.describe()
    var lists = Store<List<Integer>>()
    var list_answer = lists.describe()
    console.print(points.inline, point_answer)
    console.print(lists.inline, list_answer)
}
```
```output
true inline, 0 so far
false as references, 0 so far
```

The standard library makes this choice once, in `Items<T>`, a collection whose every body folds on it
([collections.md](collections.md#itemst-the-storage-chosen-for-you)), so a generic that only needs to keep its
values writes `Items<$component_type>()` and never asks.

Inside a walk of attributes, `attribute.class.is_fixed_size` asks the same of each attribute, and
`attribute.index` is the attribute's place in the walk, 0 for the first: together they let a runner keep a
sparse set per component and fill a row of borrowed items from it
([memory.md](memory.md#a-row-of-borrowed-items-for-one-call)).

## Tree shaking

Spite removes what a program does not use, and it can do so exactly, because everything it generates is decided
at compile time:

- A **condition on a codegen value** or on a [`Build`](programs.md#compile-time-settings-build) field is decided
  while compiling, and the branch not taken is never generated: `Weapon<Integer, true>` and `Weapon<Integer, false>`
  are two classes, and each keeps only the branch of `if $is_magic { }` that it takes.
- A **template** is compiled only for the names a program calls: a program that never calls `sum_price()` has no
  `sum_price`.
- **Reflection on a constant** folds: a question is `true` or `false`, a walk is unrolled into one call per member,
  and each function a walk calls is compiled once per member. No list of attributes, functions, classes or enum
  values exists at run time, and a program that walks nothing carries nothing.
- **Reflection at run time** is built only where it is read, for the classes such a read can reach
  ([reflection.md](reflection.md#known-only-at-run-time)), and a generic class such as
  [`JsonWriter` and the rest of json.md](json.md) exists only for the types a program uses it with.
- A **generic class** exists only for the values a program gives it, and each of its functions is compiled for one
  set of values only when surviving code calls it; a [constraint](#constraining-what-a-generic-accepts) is checked
  by the compiler and emits nothing.
- In a production build, a **function or class** nothing reachable uses is not emitted
  ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).
- The **concurrency machinery** (the state machines of a `Concurrent` and the loop that runs them, the thread pool,
  atomic reference counts) exists only in a program that makes a `Concurrent` or a `Parallel`, or is built with
  `--repl-port` or `--hot-reload`
  ([optimizations.md](optimizations.md#concurrency-machinery-only-where-it-is-used)).

This is tree shaking over the program's own model, not dead-code elimination left to the C compiler.

An inspectable build (`--development`, `--hot-reload`, `--repl` or `--repl-port`) keeps every function and class the
program generated instead of only the ones reachable from `main`, so live reload has all of them to swap and the REPL
can reach them, along with the run-time tables of every class
([compiler.md](compiler.md#development-builds-and-tree-shaking)). What is decided while compiling is decided there
too: templates are still generated only for the names called, and [conditions on codegen values](#codegen-values-)
still fold. Every optimisation the compiler makes on its own, and the builds it applies in, is listed in
[optimizations.md](optimizations.md).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge cases and
the exact error texts. Where the teaching above and these rules disagree, the rules win.

### Templates

**A parameter of type `Spite.Attribute<Owner>` whose name is a word of its function's name makes the function a
template** over `Owner`'s members: its attributes, and its functions that take no arguments. `show_attribute(attribute:
Spite.Attribute<Monster>, monster: Monster)` answers `show_name(troll)` and `show_health(troll)`. A template is
instantiated only for the names a program calls, in every build; the template itself emits nothing, and each
instance is an ordinary function that costs what its body costs. `--final-classes` prints each instance.

- **Inside an instance, the parameter is the constant `Spite.Attribute` of that member.** Written as a type
  (`value: attribute.class`, `): attribute.class`) it is the member's real type; written as an expression,
  `attribute.class` is its `Spite.Class`, which prints as the type's name in `console.print` and in a text's `{}`.
  `attribute.name`, `attribute.camel_case_name` and `attribute.pascal_case_name` are text constants, and
  `attribute.index` is the member's place in declaration order.
- **`owner.attributes[attribute]` is that member of the `owner` passed in**: the field, read or written, or a call to
  the function. A class reads its own members by name, `attributes[attribute]`, never `this.attributes[attribute]`.
- **`Owner` may be a codegen value**: `member: Spite.Attribute<$element_type>` is how `library/list.spite` writes
  its member templates. When that value is not a class or a `type` (a number, a `T?`, a `List`), the template
  answers nothing.
- **`Owner` may be a `type`**: the template ranges over the attributes the type names and the functions it requires
  with no arguments, and `row.attributes[attribute]` is the shape's own read, write or call, answered from the
  value's class at run time like any read through a `type`.
- **An exact function always wins over a template, for that one name and direction**: an exact `set_name` answers a
  write of `name` while its read still goes through `get_attribute`. A generated `get_<attribute>()` of an owning
  attribute (a `String`, `List<T>` or `Dictionary<T>`) returns a retained, independent value, exactly as an explicit
  getter does.
- **A template that makes a function the class already has is an error** at that function's line: "the template
  'store_attribute' makes 'store_row' for the attribute 'row' of 'RowView', which is already a function of 'Row':
  rename the function or the template". An error raised in code the compiler wrote for a template names the
  template's line.
- **Reading a member counts as using it**: `x.attributes[attribute]` counts as a read of the member it reaches,
  while `attribute.name` and `attribute.class` describe it without reading it, so an attribute only they look at is
  the unread-attribute error ([Unused is an error](style.md#unused-is-an-error)).
- **A walk sees private attributes**, and may read and write them through the walked attribute; naming `_x` outside
  its class stays the private error.

**Plural and singular.** A template whose result is a collection of the member's values (`List<member.class>`)
takes the member in the plural, through `String.pluralize()`; every other template takes it singular. The template
declares which by its own name: `func map_members(member: Spite.Attribute<$element_type>): List<member.class>`. The
compiler finds the member by inflecting every member name of the element's class and picking the one whose plural
is the word called, never by guessing from the call:

- one match is the member: `map_names` is `name`, `map_people` is `person`, `map_health` is `health`;
- the singular where the plural differs is "'map_name' answers every 'name', so it is written in the plural:
  'map_names'";
- two members with one plural is an error naming both and asking to rename one;
- a name `pluralize()` cannot inflect so that `singularize()` gives it back is an error naming the member and the
  fix: add the word to the irregulars table by reopening `String.Inflection`, or rename the member;
- a member that is a question (`is_alive`, `has_target`, `can_fly`) is never collected: `map_is_alive()` is an error
  naming `filter_is_alive()`, `count_is_alive()`, `any_is_alive()` and `all_is_alive()`.

**A member template chains through a member's own function.** `filter_<member>_<function>(arguments)` keeps the
items whose `member` answers `function(arguments)`: `players.filter_name_starts_with("a")`,
`functions.filter_name_ends_with("_each")`. It works on any list of any class, and a reflection object does not
answer its name's text functions itself.

**A list of a union** takes a member class in the plural as a filter that keeps that class and narrows the list to
it (`entries.filter_files()` answers a `List<File>`), and templates an attribute every member class has as on any
list, the result typed from that common attribute. A name that matches both a member class and an attribute is a
compile error naming the fix.

**A `Dictionary` takes the same member templates as a `List`, over its values**: `accesses.filter_written()`.

**There is no plain `map(function)`, and a class has no function values.** A member's values are collected with
`map_<members>()`; a value computed from an element becomes a get-only attribute of the element's class and is
collected the same way. `Monster.is_alive` names nothing, since `Monster` is a `Spite.Class` object; a function value
belongs to an instance ([functions_and_operators.md](functions_and_operators.md#functions-are-values)).

### Walks

A walk is a call on a list of reflection objects, under [reflection.md's rules for constants](reflection.md#known-while-compiling-1):

- **Names are selected, never built.** `[]` takes a name written in the code and answers a `T?`; anything else is a
  filter over the names that exist. A text with holes is never a member name.
- **A walk over a constant list is unrolled**, one call per element in the list's order (declaration order for
  attributes and arguments, the order made for functions, an enum's order for its values, dotted-name order for
  classes and namespaces), and the function it calls is compiled once per element. A walk over an empty list calls
  nothing.
- **A question on a constant folds**: `.is_resumable`, `.is_mutated`, `.is_stateful`, `.is_fixed_size`,
  `.is_singleton`, the kind questions, `.count()` of a constant list, `[]` with a literal name, and `==` between
  constant classes. **`.is_resumable` cannot be asked of a function by itself**: its own answer would decide whether
  it waits, and that is an error.
- **`.is_mutated` follows every call**: an attribute set on what the argument was given, an item written or a list
  grown, a function called on it that changes its own object, or the argument handed to another function that does
  any of these, however deep, recursion included. Reading does not count, and neither does giving the argument's
  name a new object. **`.is_stateful`** is the same study asked of the object: some function besides the constructor
  and `drop()` writes the class's own attributes or anything reached through them, or a singleton, or a singleton the
  class binds has state.
- **An empty walk is loud when it must not be**: `crash Runner.functions.filter_name_ends_with("_each").count() > 0`
  folds, and is the compile error below when it is false.

### Codegen values (`$`)

`$name` means "replaced at code generation". That is its only meaning, everywhere it appears.

**A class declares each codegen value on a `generic` line of its own, at the top of the file**:

```gdscript weapon.spite
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
  accepts. The lines come after a `singleton` line and before everything else; one line declares one value,
  and a comma is an error naming the line to write: "a 'generic' line declares one codegen value: write 'generic
  $left_type' and put the next one on its own line".
- **Call sites are positional, always, in the order of the lines.** There is no named form: `List<Integer>()`,
  `Weapon<Integer, true>(10)`.
- **`$` is for generics only.** Every `$name` a class uses has a `generic` line and is supplied by the caller, so
  the lines are exactly "what a caller must pass". A `$name` with no line is a compile error. Used as a value it
  points at [`Environment`](programs.md#run-time-settings-environment), which is where a program's settings
  live: "'Game' has no 'generic $verbose' line, and '$' is for generics only: a program's settings are fields of
  Environment, so ..."; used as a type it is "this class has no 'generic $other_type' line, so '$other_type' cannot
  be used as a type" (`diagnostics/codegen_values`).
- **Only a class has codegen values; there are no generic functions.** A function reads the `generic` lines of its
  class and declares none of its own; a function that must accept any class takes a `type` (the built-in empty
  `Anything` accepts every class), and `value.class` is the real class
  ([reflection.md](reflection.md#known-only-at-run-time)). Listing values on a function is the error below.
- **A class needs no constructor to take codegen values.** `library/list.spite` is `generic $element_type` and its
  functions, and `library/dictionary.spite` is `generic $value_type`; an empty constructor is still an error.
- **Every hole must be filled.** There are no defaults. A call supplying the wrong number is a compile error
  naming the class's codegen values, in order: "'Box' takes 1 codegen value(s), in this order: $held_type,but 2 were given", so the mistake is corrected from the message rather than by opening the class. A `$name`
  that is not declared is an error too, which is what a typo like `$is_magik` produces.
- Reordering the `generic` lines changes what every existing positional call site means. Where the values
  have different kinds (a class versus a `Boolean`) the compiler catches it immediately; where they are the same
  kind (`Pair<Integer, String>` swapped) it compiles and means something else, and the tests are what catch it. This
  is a deliberate, accepted trade.
- **Each set of values is its own class**, made only where a program names it, with its own copy of every
  function surviving code calls. Nothing chooses between them at run time; what a generic costs is that code,
  once per set of values used, and a generic nobody makes costs nothing. `--final-classes` prints each
  class with its codegen values already bound, which is where `true` reads as `is_magic` again.
- Writing `generics` is a parse error naming this form, and so is listing codegen values on a function,
  `func Weapon<$damage_type, $is_magic>(...)`, on a constructor or any other function: "a function no longer
  lists codegen values between '<' and '>': declare each at the top of the file on its own line ('generic
  $damage_type', 'generic $is_magic'), and write 'func Weapon(' here" (`diagnostics/constructor_codegen_list`).
  A `generic` line inside a function is an error too.
- **The values can be left out when the constructor's arguments say them**: each parameter whose declared type
  mentions a `$name` is
  matched against its argument's type (`$name` itself, `$name?` (a `T` or a `T?` both give `T`), `List<$name>`,
  `Dictionary<$name>`, and the arguments and return of a `Spite.Function<...>`). A `null` argument says nothing.
  Only when every `$name` is found; otherwise the error asks for them between `<` and `>`: "'Box' takes 1 codegen
  value(s), in this order: $held_type. Write them between < and > before the arguments". `Pair("Hero", 7)`,
  `Concurrent(file.read)`, `JsonWriter(order)`.
- `$name()` makes the default of what `$name` is bound to: a class through
  its constructor with no arguments, an empty `List` or `Dictionary`, `0`, `""`, and for a `type` whose members
  are all attributes a real object. `var held: $held_type = null` is an error, since `null` belongs to `T?`
  alone ([values_and_types.md](values_and_types.md#variables-and-values)), except on an attribute the
  constructor assigns and a local a walk fills. **Bound to a `type` that requires a function, `= null` is a
  compile error naming the attribute**, unless the class's constructor assigns the attribute: no object
  can supply the function, so there is no default to make. `'held' is a Weapon, a type that requires the function 'strike', so
  '= null' has no default to make: no object can supply a function it does not have. Make its type nullable, with
  a '?', and narrow it before use, or give it a real object in the constructor` (`diagnostics/function_shape_default`).

**A `generic` line may name a constraint.**
`generic $item_type: Printable` accepts only types that fit the `type` `Printable`, and a use that does not
(`Shelf<Pet>`) is an error where the class is named, naming the constraint, the class and what it lacks:
"'Pet' does not fit type 'Printable', which 'Shelf' requires of $item_type: it has no function 'to_string'",
instead of an error deep inside the generic's body. The constraint is optional and uses an existing `type`,
resolved like any type name from the generic's file. In detail:

- **Fitting is what [Types](values_and_types.md#types) already means by it**: the class a `type` would admit
  fits: a class with the listed functions and attributes, `String`, a number, an enum, a `List<T>` or
  `Dictionary<T>` with what the `type` needs, and the `type` itself. A `T?` does not fit ("'Integer?' does not fit
  type 'Printable', which 'Shelf' requires of $item_type: it may be null, and null has none of what the type
  needs"), and neither does a function value.
- **A constraint names a `type`, not a class, a union or a number type**: anything else is an error on the
  `generic` line: "'$count_type' is constrained by 'Integer', which is not a 'type': a constraint names a
  'type' the class given must fit, like 'generic $count_type: Printable'".
- **It is checked once per class given**, where the compiler first makes that instance, written out or read
  from the constructor's arguments. After the error the generic is compiled as if it had been given the `type`
  itself, so its body adds no errors of its own.
- **It costs nothing at run time and tree-shakes as before**: the check is the compiler's alone, and the
  instance made is the one an unconstrained line would make.

`conformance/stage6/generic_constraints`, `diagnostics/generic_constraints`,
`diagnostics/generic_constraint_not_a_type`.

**Conditions on codegen values fold.** They are decided at compile time and the untaken branch is removed (tree
shaking), in every build, `--development`, `--hot-reload` and the REPL builds included: a codegen value is part of
which class this is (`Weapon<Integer, true>` and `Weapon<Integer, false>` are two classes), so there is no
run-time value for live reload to change. To change one, change the call site.

**An `assert` or `crash` on such a condition folds the same way.** When the condition of an `assert` or `crash`
asks only what is decided while compiling (a `$flag`, `$slot_type == Entity` or any other test of a codegen type,
a question asked of a reflection constant (`$T.functions['run_each']`, `.is_resumable`, `.is_mutated`,
`.is_fixed_size`, `.arguments.count()`, of a codegen type or of a walked member's class), `argument.class == $row_type`, and `not`, `and` and `or` over them), it is decided
for each instance, with no test at run time: when it holds,
nothing is written for it; when it does not, the `assert` answers "nothing" (recording its trace line), and the
statements after it in the same block are not compiled for that instance, exactly as after an `if` whose taken
branch returns. A folded `assert` is held to the same rule as any other: it is allowed only in a function whose
result can say "nothing" (a `T?`, `Nothing`, or a collection), so `assert $slot_type == Entity` followed by
`return value.id` is written in a function returning `Integer?`; in one returning `Integer`, the answer for the
other instances is written down instead, `if $slot_type != Entity { return 0 }`, which folds the same way
(`conformance/stage6/folded_checks`, [failure.md](failure.md#a-default-that-looks-like-an-answer-is-an-error)).

**A `crash` that folds to false is a compile error where the program can reach it.** A `crash` whose condition is
decided while compiling and is false would halt every time its function runs, so it is a developer's mistake the
compiler can prove, and it is reported while compiling, at the `crash`, naming the instance: `'crash
$slot_type.is_fixed_size' always halts in Slot<List<String>>: its condition is decided while compiling and is
false, so the program would stop here every time this function runs: call it only where the condition holds, or
change what the condition asks` (`diagnostics/folded_crash`). Only a function the program reaches counts (the
same reach tree shaking keeps in a production build, worked out for an inspectable build too, where nothing is
shaken), so an instance whose function nobody calls compiles (`conformance/stage6/folded_crash_uncalled`). This
is how a library turns its rules into compile errors: a `crash $system_type.functions['update_each']` in the
function that runs a system fails the build for the class that breaks the rule, not the run. The sides of an `and`
are checked one at a time, as two `crash` lines would be, so in `crash $system_type.functions['update_each'] and
ready` the first side folds on its own and is this compile error when it is false, whatever `ready` holds; only a
condition that mixes a run-time value in through `or` (or `not` over an `and`) is tested at run time as a whole.

**A class test that can never be true for one instantiation folds to `false`.** Inside
a generic class, `if item == Health { }` where `item`'s type comes from a codegen value that is not `Health` in
this copy is `false`, and its branch is removed from that copy only, since generic code that a particular copy does
not use is what tree shaking removes, so it is on purpose not an error. Outside generics the never-true test stays
an error ([control_flow.md](control_flow.md#control-flow-in-full)). A codegen value bound to a class is itself
a class test on the right of `==`, `item == $wanted_type`
([values_and_types.md](values_and_types.md#unions-in-full)).

**Asking what type a generic was given.** When a codegen value is a type, `$value_type == String` is decided at
compile time like any other condition on a codegen value, and only the branch taken is compiled, so each branch
may use what only that type has, which is what lets one generic class treat text, numbers, lists and classes
differently. It is the class test ([control_flow.md](control_flow.md#value--class)) asked of a type instead of a
value. A type name asks for exactly that type; four names ask for a kind, since the type has arguments the test
does not want to spell:

| Test | True when the type is |
|---|---|
| `$value_type == List` | any `List<T>` |
| `$value_type == Dictionary` | any `Dictionary<T>` |
| `$value_type == Null` | any `T?` (`Null` is a member of the union a `T?` is) |
| `$value_type == Symbol` | an enum, or `Symbol` (an enum is a closed list of symbols) |
| `$value_type == Number` | any number class, `Tiny` to `Double` (a `type` name asks whether the type fits it) |
| `$value_type == Enum` | an enum only, not a plain `Symbol` (`JsonReader` and `BinaryFormat` need it to read a plain `Symbol` through `Symbol(text)` and an enum through the text cast) |

A union name is true for any of its members, and a `type` name for any type that fits it: `$value_type ==
Number` is true for every number class ([values_and_types.md](values_and_types.md#every-number-fits-number)).
Such a test always folds, `--development` included, because the
branch it rules out would not compile.

**Only what survives folding is compiled.** A function of a generic class is
compiled, and so type-checked, for one instantiation only when code already compiled for the program names it (a
call that survived folding, a function value, a reflection table, or a call through a `type` the class is a
member of), so a helper reached only from a branch the instantiation rules out is never checked against that
type. When a folded `if` (or `else if` chain) takes a branch that ends in `return`, the statements after it in the
same block are not compiled either, and the names they read count as used, as for the untaken branch. This holds
in every build, inspectable ones included. A class that is not generic still compiles every function, so a
mistake in an uncalled one is still reported. `conformance/stage6/folded_helpers`.

**The types a type was built from are read by their codegen names**: `$value_type.element_type` for a
`List<$element_type>`, `$value_type.value_type` for a `Dictionary<$value_type>` or a `$value_type?`, and a generic
class's own names for one of its instances, the names this section already gives the containers. Reading a name
the type does not have is an error listing them: "a List<Integer> has no codegen value named '$value_type': List<$element_type>, Dictionary<$value_type> and $value_type? name theirs, and a generic class names its own"
(`diagnostics/every_attribute`). `conformance/stage6/every_attribute`.
Compared in a condition, a name read this way folds like `$value_type` itself: `if $list_type.element_type ==
Float`, `else if $map_type.key_type == String`, `$holder_type.held_type != Item`, to any depth and in every
branch of an `else if` chain, so a branch that does not fit the instantiation is not compiled
(`conformance/stage6/codegen_member_fold`). An `and` whose left side folds to
`false`, or an `or` whose left folds to `true`, folds without its right side, which may then ask what the type does
not have: `$list_type.element_type == List and $list_type.element_type.element_type == Float`.

**A codegen value that is a type reads as its class**: a member
read through it, `$component_type.name` or `$component_type.attributes`, is read from the bound class's
`Spite.Class`, exactly as `Health.name` is, and no value of the type is made. `$component_type` alone in an
expression is still the "is a type here" error (`conformance/stage6/codegen_class_name`).

The `generic` line is a declaration like `var`, and follows a header pattern shared with `singleton`.


---

Next: [Reflection](reflection.md), the `Spite` classes every class, namespace and function is an instance of.
