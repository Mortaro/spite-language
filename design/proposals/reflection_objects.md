# Metaprogramming is reflection on ordinary objects (a proposal for D316)

**Status:** a design proposal, not a build. Mortaro set the direction ([D316](../decisions.md)): templates stay
only where they change a function's name; everything else that reads a program's structure goes through
`Spite.Class`, `Spite.Namespace`, `Spite.Function` and `Spite.Attribute` as ordinary instances, the way Ruby does,
and a user at the REPL reaches them the same way a program does. Mortaro reviewed the first version on 2026-09-30
("everything else is good at least on first sight") and settled its four questions and four more points, recorded
in [section 8](#8-what-mortaro-settled-on-the-first-review); the rest is proposed by Claude, unconfirmed.

## The answer in one table

| Today | Proposed | Why |
|---|---|---|
| `show_attribute(attribute: Symbol<Monster>, ...)` called as `show_health(troll)` | **stays**, its parameter written `attribute: Spite.Attribute<Monster>` | the name carries the argument; `show_health` reads as well as `show(troll.health)` |
| `filter_<member>`, `sum_<member>`, `count_<member>`... (D15, D91) | **stay** | same reason |
| `map_<member>` | **stays, named in the plural**: `classes.map_names()` | it answers the members' values, so the name says so ([section 5](#5-plural-member-templates)) |
| `get_<attribute>`/`set_<attribute>`, `to_<type>()` (D311) | **stay** | same reason |
| The plural walk, `show_attributes(label, lines)` | **goes**: `label.attributes.each(...)` / `.map(...)` | a loop in disguise; nobody ever calls the singular by name |
| `Symbol<$T.run_each>` over a function's arguments (D114) | **goes**: `$T.functions['run_each'].arguments` | the arguments are a list on a `Spite.Function` |
| `Symbol<$T.phase_each>` name patterns and holes (D116, D180) | **goes**: `$T.functions.filter_ends_with("_each")` | selection by filter; a name is never built from text |
| `Symbol<System>`, every folder named `system` (D115) | **goes**: an explicit filter over `Spite.Namespace.instances`, then `namespace.classes` | a folder is a namespace, not IO |
| `Symbol<Spite.Class>` (D287) | **goes**: `Spite.Class.instances` (D49, already there) | two spellings of one list |
| `Symbol<Phase>` over an enum's values | **goes**: `Phase.values` | a list |
| `$T.has_function("f")`, `function_waits`, `argument_count`, `function_writes_parameter`, `argument_class`, `returned_text`, `has_state()`, `fits_vector()`, `package_folder()`, `$T == List` | **become get-only attributes named for what they answer** ([section 6](#6-every-question-is-a-get-only-attribute-named-for-its-answer)) | known while compiling, not a search |
| Passing a template's symbol to a helper (D116 rules, item 108) | **goes** | a `Spite.Attribute` is an ordinary argument |

The one new rule that makes this work without giving up zero run time is **constant reflection**
([section 4](#4-the-crucial-part-nothing-is-looked-up-at-run-time)): a reflection object the compiler knows is a
compile-time constant, a list of them is unrolled, and a function handed one is compiled once for it: exactly what
a template instance is today, reached through an ordinary call instead of a spelled name.

## 1. What exists today

Two systems grew side by side and never met.

**Reflection** ([reflection.md](../../docs/reflection.md), D6, D11, D12, D41, D49, D57) already has the object model
Mortaro describes: `Spite.Class` (`.name`, `.namespace`, `.attributes`, `.functions`, `.instances`,
`is_singleton()`, `has_function(name)`...), `Spite.Function` (`.name`, `.arguments`, `.returns`,
`call_function()`), `Spite.Argument`, `Spite.Attribute` (`.name`, `.class`, `.value: Anything?`) and
`Spite.Namespace` (`.name`, `.name_with_namespaces`, `.parent`, `.classes`, `.namespaces`), ordinary classes in
`library/spite/`. `Spite.Class.instances` is every class (D49). But it is run-time only: `.value` is a boxed
`Anything?`, and a walk over `.attributes` cannot use an attribute's class as a type.

**Metaprogramming** ([metaprogramming.md](../../docs/metaprogramming.md)) is where the typed work happens, and it is built
on `Symbol` parameters instead:

| Mechanism | Syntax | Used for |
|---|---|---|
| Symbol codegen | `func get_attribute(attribute: Symbol): attribute.class`, called `get_age()` | getters, setters, casts, user name templates |
| Another class's members | `attribute: Symbol<Label>`, `label.attributes[attribute]` | typed per-attribute code |
| The plural | `show_attributes(label, lines)` calls the singular once per attribute | every attribute walk (JSON, binary, debug, the engine package rows, packs, rules) |
| Member templates | `member: Symbol<$element_type>` in `List`, `Vector`, `Items` | `filter_`, `sum_`, `map_`... |
| Argument walk (D114) | `argument: Symbol<$T.run_each>`, plural as an argument list `run_each(made_arguments())` | the engine package's runner and stream |
| Name pattern (D116, D180) | `phase: Symbol<$T.phase_each>`, hole matched against `enum Phase` | systems placed by name |
| Folder walk (D115) | `system: Symbol<System>`, every folder named `system` | finding systems, components, recipes, assets |
| Every class (D287) | `kind: Symbol<Spite.Class>` | ECS rule checks, inspection |
| Enum walk | `course: Symbol<Course>` | walking phases |
| Passed symbol | `announce(phase, "before")` | cutting a long template into helpers |
| Folded questions | `$T.has_function("f")`, `function_waits`, `argument_count`, `function_writes_parameter`, `argument_class`, `returned_text`, `has_state()`, `fits_vector()`, `package_folder()`, `$T == List`, `$T.element_type` | choosing branches while compiling |

## 2. Where it is used

Counted in `.spite` files, lines (build output and vendored packages left out):

| | library | compiler | conformance + diagnostics | benchmarks | engine package `engine/` | engine package `plugins/` | engine package `examples/` |
|---|---|---|---|---|---|---|---|
| `Symbol<` parameters | 36 | 5 | 79 | 23 | 56 | 4 | 2 |
| `.attributes`/`.functions`/`.classes` | 42 | 16 | 65 | 11 | 18 | 0 | 0 |
| `has_function(` | 4 | 3 | 12 | 0 | 9 | 5 | 0 |
| `Spite.Class`/`Namespace`/`Attribute`/`Function` | 72 | 13 | 25 | 2 | 5 | 0 | 1 |

By mechanism:

- **Member templates** (stay): 24 of the library's 36 `Symbol<` lines (`list.spite`, `vector.spite`,
  `items.spite`).
- **Attribute walks through the plural** (go): the library's other 12 (`json_writer`, `json_reader`,
  `binary_format`, `spite/debug_instance`); the engine package's `component_rule`, `row` (4), `pack` (2), `link` (4),
  `field_count`, `link_count`, `spawn_bundle`, `runner.classify_attribute`.
- **Argument and name-pattern walks** (go): the engine package's `runner.spite` (about 30 templates) and `stream.spite` (3),
  `examples/template_wait_check`.
- **Folder walks** (go): `Symbol<Component>` in `app.spite` and the network plugin's `send`, `receive` (2) and
  `forget_arrived`; `Symbol<System>` in `app.spite`; `Symbol<Asset>`, `Symbol<Recipe>` in `recipes/`;
  `Symbol<Bundle>` in `world.spite`.
- **Conformance**: 28 stage6 programs and 13 diagnostics use `Symbol<`; about 20 of them test a mechanism that goes.
- **Compiler**: the plural and range machinery is in `bootstrap/source/generation/generator.spite` (`namespace_walk`
  at 5493, the plural check `misnamed_plural` at 11640, about 90 functions touching templates, plurals or ranges)
  and `template_info`, `template_walk`, `row_walk`.

## 3. The object model

The objects are the ones reflection.md already has. What changes is how they are reached, that every member is a
get-only attribute, and that their collections are ordinary lists.

| Object | Members (new or renamed in bold; every one a get-only attribute) |
|---|---|
| `Spite.Namespace` | `.name`, `.name_with_namespaces`, `.parent: Spite.Namespace?`, `.classes`, `.namespaces`, **`.every_class`** (this namespace and every one below it), **`.enums`** |
| `Spite.Class` | `.name`, `.namespace`, `.attributes`, `.functions`, `.instances`, **`.values`** (an enum's), `.is_singleton`, **`.is_stateful`**, **`.is_fixed_size`**, **`.is_list`, `.is_dictionary`, `.is_optional`, `.is_enum`**, `.element_type`, `.value_type`, `.package_folder`, `.source_folder` |
| `Spite.Function` | `.name`, `.arguments`, `.returns`, **`.owner`** (its class), **`.is_resumable`**, **`.returned_literal`**, **`.read_attributes`, `.written_attributes`** (item 109); and `call_function()`, **`call_with(values)`**, which act and stay functions |
| `Spite.Argument` | `.name`, `.class`, **`.index`**, **`.is_mutated`**, **`.function`** |
| `Spite.Attribute` | `.name`, `.class`, **`.index`**, **`.owner`**, **`.camel_case_name`, `.pascal_case_name`**, and `.value` only when bound ([below](#an-attributes-value-bound-like-a-function)) |

**Every collection is an ordinary `List`**, so everything a list has works: `[]` by name answering `T?` (a list of
reflection objects keys by `.name`), `count()`, `each`, `map`, `filter`, and every member template:
`Monster.attributes.map_names()`, `Shop.classes.filter_is_singleton()`, `namespace.classes.map_names()`.

**A reflection object answers its name's text functions.** Its `.name` is a `Symbol`, which answers every `String`
function (D68), and the object itself does too wherever it has no member of that name, so
`functions.filter_ends_with("_each")` keeps the functions whose names end in `_each`. For that, the member templates
pass their call's arguments on to a member function that takes them (`filter_ends_with("_each")` calls
`ends_with("_each")` on each element); D15 allowed only members with no arguments.

**Names are selected, never built.** A symbol is not a string: `functions["{phase}_each"]` would turn a symbol into
text and back, so it is not proposed. `[]` takes a name written in the code (`functions['run_each']`); everything
else selects with a filter over the names that exist, and each such filter folds (section 4).

**How they are reached**, most of it already true:

- **A class name read as a value is its `Spite.Class`**: `Monster.attributes`, `Monster.functions['alive']`.
  `var kind = Monster` with no type stays an error (D165).
- **A namespace name read as a value is its `Spite.Namespace`**: `Shop.Tools.classes`. A class and a namespace with
  the same dotted name is a compile error, so the name always means one thing.
- **`class`** inside a function is the class it answers on (unchanged), and **`namespace`** is its namespace (new,
  the same kind of name).
- **`$T`** is a class object wherever a value is read, as `$T.has_function(...)` already treats it.
- **`value.class`** answers the value's real class (unchanged).
- **`Spite.Class.instances`** is every class (D49) and **`Spite.Namespace.instances`** every namespace; "every folder
  named `component` anywhere" is written out as a filter over it ([section 7](#registering-every-component-a-folder-of-classes)).
- **`Phase.values`** is an enum's values, in order.

### Every member is a get-only attribute

What reflection answers is known while compiling, and none of it is a search, so none of it is a function call:
`function.is_resumable`, `class.attributes`, `argument.is_mutated`, never with `()`. They are D88's getter-only
functions read as attributes, which Spite already does for every `get_`: `library/spite/class.spite` keeps
`func get_is_stateful(): Boolean` and a reader writes `kind.is_stateful`. **A question that took an argument
becomes a collection indexed or filtered instead:** `has_function("run_each")` is `functions['run_each']` (a
`T?`), `argument_count("f")` is `functions['f'].arguments.count()`, `function_writes_parameter("f", i)` is
`functions['f'].arguments[i].is_mutated`, `any_attribute_fits_vector(Entity)` is
`attributes.filter_is_fixed_size()` asked as the program needs. `arguments[i]` and `attributes['name']` stay
indexing. What remains a function is what acts rather than answers (`call_function()`, `call_with(...)`), and
`List`'s own `count()`.

### An attribute's value, bound like a function

A function named on an instance is a bound `Spite.Function` (D17). An attribute does the same: **`Monster.attributes`
describes declarations, `troll.attributes` holds attributes bound to `troll`** (D11 already says so). A bound
attribute's `.value` is readable and writable: `attribute.value = 3`. And an unbound attribute indexes an instance,
`troll.attributes[attribute]`, which is how a walk over a class's attributes reaches one instance's values (the
form templates use today), now taking a `Spite.Attribute` instead of a `Symbol`. D88's "read-only" stays for
everything describing the program (`.name`, `.class`); `.value` describes the instance.

### The name templates that stay

A template that changes a function's name keeps working as today, and its parameter says what it is:
`func show_attribute(attribute: Spite.Attribute<Monster>, monster: Monster)` answers `show_health(troll)`, and inside
it `attribute` is the constant `Spite.Attribute` for `health`. There is no separate `Symbol` idea any more:
`Spite.Attribute<Monster>` ranges over `Monster`'s members, `Spite.Attribute<$element_type>` is how
`library/list.spite` writes its member templates, and a bare `Symbol` stays an ordinary value (an enum's or a
class's name).

## 4. The crucial part: nothing is looked up at run time

Spite promises that nothing is looked up while a program runs and that unused code is not emitted (D147, D176,
AGENTS.md). Ruby's surface would break that if it were built the Ruby way. It is not: **it folds**.

### Constant reflection

A reflection object is a **constant** when the compiler knows which one it is:

1. a class or namespace named in the code, `$T`, `class` and `namespace`, an enum's value named in the code;
2. `value.class` when `value`'s declared type is a class (not a `type` or a union);
3. any member of a constant (`Monster.functions['alive']`, `.namespace`, `.arguments[0]`), `[]` with a name written
   in the code, and a text function answered by a constant's name (`function.ends_with("_each")`);
4. every element of a constant list (`.attributes`, `.classes`, `Spite.Class.instances`, `Phase.values`, and a
   `filter`, `map` or member template applied to one);
5. **a parameter of a reflection type (`Spite.Class`, `Spite.Attribute`, `Spite.Function`, `Spite.Argument`,
   `Spite.Namespace`, or an enum) in a call whose argument is a constant.** The function is compiled once for each
   constant it is called with (a *specialisation*), and inside it the parameter is a constant.

Rule 5 is today's template instance, reached through a call instead of a spelled name: `show_attribute_for_health`
in `--final-classes` is the same function it is now. It is what D237 already says for `attribute.class` inside a
template, extended to every reflection object.

What folds:

- **Questions** on a constant are constants (`.is_singleton`, `.is_resumable`, `== Entity`, `.count()`), so `if`
  keeps only the branch taken, as it does today (D167, D237).
- **`each` over a constant list is unrolled**: `Monster.attributes.each(show)` is `show_for_name(); show_for_health()`
  in the generated code, each call a specialisation. `filter`, `map` and member templates over a constant list
  fold to a constant list. No list exists at run time.
- **A constant class or attribute class is usable as a type**: `Column<component>()`,
  `var value: attribute.class = ...`, `): argument.class`, like `Column<component.class>()` today.
- **`troll.attributes[attribute]` with a constant `attribute`** is a plain typed field read or write (a getter
  call where one exists), exactly what the template reads today; a bound attribute's `.value` is typed
  `attribute.class`.
- **`[]` with a constant name answers a constant `T?`**: `assert Monster.functions['run_each']` folds to nothing when
  it is there. When it is not, the specialisation halts on every call, the way `crash $T.has_function(...)` does
  today, and a walk that reaches it only in a branch it rules out never compiles it. A misspelled constant name
  reached unconditionally is a compile error naming what the class does have (D244).
- **An empty constant list walks nothing**, as the plural does, but `crash components.count() > 0` now folds and
  says so, which is the loud answer item 99 lacked.

### What is not constant

`value.class` through a `type` or union, `Spite.Class.instances` filtered by a run-time value, a reflection object
kept in an attribute and read later, and everything typed at the REPL prompt. These are **run-time reflection**,
the tables reflection.md already describes, built only for the classes such a read can reach (D57), and every class
in a `--repl`/`--repl-port`/`--development` build (D143). There `.value` is `Anything?`, boxed, as today.

**Using a reflection object as a type needs a constant.** A specialisation that writes `attribute.class` as a type,
`Column<component>`, or indexes an instance typed, called with a run-time value, is a compile error naming the
call that made it run-time:

```diagnostic
'attribute' is not known while compiling here (it comes from 'value.attributes' through the type 'Anything' in describe()), so 'attribute.class' cannot be a type; walk 'Gadget.attributes' instead, or read 'attribute.value'
```

So one function body serves both worlds: called with constants it is typed and unrolled, and a REPL user calling
the same function with a live object gets the run-time path wherever the body does not need a type.

### What it costs

The same as today. A constant walk emits what the plural emits, one specialised function per member; a run-time walk
emits the tables D57 already emits, only when reached. A program that walks nothing carries nothing. The compiler
cost moves: constant evaluation of reflection expressions replaces range resolution, and it is the same
monomorphisation step template instances use.

## 5. Plural member templates

A member template that answers **the members' own values, one per element** is named with the member in the
plural, because that is what it gives back: `classes.map_names()`. An uncountable word is its own plural, so
`monsters.map_health()` is already right ([below](#5-plural-member-templates)). Every other member template keeps the
member singular, because its name reads as a question asked of each element or a quantity over them:

| Template | Member | Reads as |
|---|---|---|
| `map_<members>` | **plural** | "the names": `classes.map_names()` answers a `List` of names |
| `filter_<member>`, `remove_where_<member>` | singular | "those that are alive": `filter_alive()` |
| `count_<member>`, `any_<member>`, `all_<member>` | singular | "how many / any / all are alive" |
| `sum_<member>` | singular | "the sum of health": `sum_health()` |
| `find_by_<member>`, `sort_by_<member>` | singular | "find by name", "sort by health" |
| `each_<member>`, `parallel_each_<member>` | singular | "each runs": `runners.each_run()` |

The rule, so it holds for a template a program writes too: **a template whose result is a collection of the
member's values (`List<member.class>`) takes the member in the plural; any other takes it singular.** The template
declares which by its own name: `func map_members(member: Spite.Attribute<$element_type>): List<member.class>` (the
parameter's name made plural is a word of the function's name), and writing `map_member` for a template that
returns `List<member.class>` is an error naming `map_members`.

**How the compiler finds the member.** It never guesses from the call. For a call `map_<word>` on an element class,
it inflects every member name the class has with `String.pluralize` and picks the member whose plural is `<word>`:

- **One match** is the member: `map_names` is `name`, `map_people` is `person` (an irregular), `map_health` is
  `health` (uncountable, its own plural).
- **The singular where the plural differs** is an error naming the plural: `'map_name' answers every 'name', so it
  is written in the plural: 'map_names'`.
- **Two members with one plural** (`datum` and `data`, both `data`) is an error naming both and asking to rename
  one, or to pass a function instead: `map(datum_of)`.
- **A name `pluralize` cannot inflect so that `singularize` gives it back** is an error naming the member and the fix:
  add the word to the irregulars table by reopening `String.Inflection`, or rename the member.
- **A name that is a predicate** (`is_alive`, `has_target`, `can_fly`) has no plural: `monsters.map_is_alive()` is
  written with the member's own name, since inflecting it would give `is_alives`.
  A multi-word name inflects its last word: `camel_case_names`, `child_nodes`.

**For the `String` library:** `pluralize()` and `singularize()` in `library/string.spite`, rules and a table of
irregular and uncountable words in `library/string/inflection.spite` (`String.Inflection`, a singleton a program can
reopen to add its own words), English only. The compiler reads the same table while compiling, so a word a program
adds changes the templates it answers. Both are ordinary functions a program may call at run time too
(`"entity".pluralize()` is `"entities"`), tree-shaken when unused.

## 6. Every question is a get-only attribute named for its answer

Each old question, what it really answers, and the attribute that says it:

| Today | Answers | Proposed |
|---|---|---|
| `$T.has_function("f")` | whether the class has `f` | `$T.functions['f']`, a `Spite.Function?` narrowed by `if`/`assert` |
| `$T.function_waits("f")` | whether `f` can reach a wait (a sleep, file or socket read, a `Concurrent`'s value), so it is compiled a second time as a state machine that returns at each wait and is resumed (concurrency.md) | `$T.functions['f'].is_resumable`. Not `is_blocking`: it does the opposite, and never blocks the thread. Not `is_concurrent`: the function is not concurrent itself; it can be paused and resumed |
| `$T.argument_count("f")` | how many arguments | `$T.functions['f'].arguments.count()` |
| `$T.function_writes_parameter("f", i)` | whether `f` changes the object passed as argument `i` | `$T.functions['f'].arguments[i].is_mutated` |
| `$T.argument_class("f", i)` | the declared class of argument `i` | `$T.functions['f'].arguments[i].class` |
| `$T.returned_text("f")` | the text literal `f` returns | `$T.functions['f'].returned_literal` |
| `$T.has_state()` | whether some function besides the constructor and `drop()` changes the object, or a bound singleton does | `$T.is_stateful` |
| `$T.fits_vector()` | whether the class has a size known while compiling, so a `Vector` can hold it inline | `$T.is_fixed_size` |
| `$T.any_attribute_fits_vector(Entity)` | whether any attribute of `Entity`'s class could live inline | `$T.attributes.filter_is_entity()` or what the caller really asks, over `attribute.class.is_fixed_size` |
| `$T.package_folder()`, `source_folder()` | folders | `$T.package_folder`, `$T.source_folder` |
| `is_singleton()` | from the `singleton` line | `.is_singleton` |
| `$T == List`, `Dictionary`, `Null`, `Enum` | the kind of a type | `$T.is_list`, `.is_dictionary`, `.is_optional`, `.is_enum` (`== List` may stay as the same question) |
| `name_fits("<phase>_each")` | a pattern with a hole | gone: `ends_with("_each")` and a filter |
| (item 109, not built) | which attributes a function reads and writes | `function.read_attributes`, `function.written_attributes` |

## 7. Before and after

### The README arena

The name-changing template stays; only its parameter type is spelled out.

```gdscript
show_name(troll)
show_health(troll)

func show_attribute(attribute: Spite.Attribute<Monster>, monster: Monster) {
    console.print(attribute.name, "=", monster.attributes[attribute])
}
```

To show every attribute, instead of `show_attributes(troll)`:

```gdscript
troll.attributes.each(show)

func show(attribute: Spite.Attribute) {
    console.print(attribute.name, "=", attribute.value)
}
```

### Every attribute (metaprogramming.md)

Before:

```gdscript
show_attributes(label, lines)

func show_attribute(attribute: Symbol<Label>, label: Label, lines: List<String>) {
    lines.append("{attribute.name}: {label.attributes[attribute]}")
}
```

After, no extra parameters to thread through:

```gdscript
var lines = label.attributes.map(line)

func line(attribute: Spite.Attribute): String {
    return "{attribute.name}: {attribute.value}"
}
```

### Walking a function's arguments (the note's own example)

Before:

```gdscript
if $target_type.has_function("run_each") {
    describe_arguments()
    target.run_each(made_arguments())
}

func describe_argument(argument: Symbol<$target_type.run_each>) {
    console.print(argument.name, "is a", argument.class)
}

func made_argument(argument: Symbol<$target_type.run_each>): argument.class {
    return argument.class()
}
```

After:

```gdscript
var run_each = target.functions['run_each']
if run_each {
    run_each.arguments.each(describe)
    run_each.call_with(run_each.arguments.map(made))
}

func describe(argument: Spite.Argument) {
    console.print(argument.name, "is a", argument.class)
}

func made(argument: Spite.Argument): argument.class {
    return argument.class()
}
```

`if run_each` folds (rule 3). `map(made)` over a constant list whose function returns a different class per element
is a constant *sequence*, not a `List`: it may only be passed to `call_with`, which becomes one direct typed call,
`target.run_each(made_for_hero(), made_for_pet())`.

### The engine package's runner: systems placed by their functions' names

Before:

```gdscript
place_phases_each()

func place_phase_each(phase: Symbol<$system_type.phase_each>) {
    crash placed_in == ""
    placed_in = phase.name
    prepare_arguments()
}

func prepare_argument(argument: Symbol<$system_type.phase_each>) {
    var row = Row<argument.class>()
```

After:

```gdscript
var placing = $system_type.functions.filter_ends_with("_each")
placing.each(place)

func place(function: Spite.Function) {
    var phase = Phase.values.find_by_name(function.name.without_suffix("_each"))
    if phase {
        crash placed_in == ""
        placed_in = phase
        function.arguments.each(prepare)
    }
}

func prepare(argument: Spite.Argument) {
    var row = Row<argument.class>()
```

The pattern hole and its enum rule (D180) are gone, and so is the member that changed meaning inside a pattern
template (item 100): the runner selects the functions that end in `_each` and asks the enum which value the rest of
the name is. Every step folds (`filter_ends_with` over constant names, `without_suffix` on a constant name, and
`find_by_name` over `Phase.values`), so a `count_each` that names no phase is skipped while compiling, as today.
Running is the same selection: `function.is_resumable` replaces `function_waits("<phase>_each")`,
`function.arguments.count()` replaces `phase.argument_count()`, and `function.call_with(...)` replaces
`system.phase_each(clocked_arguments(rows, entity))`. `without_suffix` is a new `String` function beside
`ends_with`.

### The engine package's component rule

Before:

```gdscript
check_attributes(sample)

func check_attribute(attribute: Symbol<$component_type>, value: $component_type) {
    var folded = AttributeRule<$component_type, attribute.class>()
    ...
    if attribute.class == Entity {
        var linked = value.attributes[attribute].id
    } else if type_name == "Boolean" {
        ...
    } else if type_name.ends_with("?") {
```

After:

```gdscript
$component_type.attributes.each(check_attribute)

func check_attribute(attribute: Spite.Attribute) {
    var folded = AttributeRule<$component_type, attribute.class>()
    ...
    if attribute.class == Entity {
        var linked = sample.attributes[attribute].id
    } else if attribute.class == Boolean {
        ...
    } else if attribute.class.is_optional {
```

`.is_optional` replaces `type_name.ends_with("?")`, a text test on a type's name. `$component_type.has_function("stored_inline")`
becomes `if $component_type.functions['stored_inline']`.

### Registering every component (a folder of classes)

Before, `app.spite`:

```gdscript
register_components()

func register_component(component: Symbol<Component>) {
    var column = Column<component.class>()
    var rule = ComponentRule<component.class>()
```

After:

```gdscript
var component_folders = Spite.Namespace.instances.filter(holds_components)
component_folders.each(register_folder)

func holds_components(namespace: Spite.Namespace): Boolean {
    return namespace.name == 'Component'
}

func register_folder(namespace: Spite.Namespace) {
    namespace.classes.each(register_component)
}

func register_component(component: Spite.Class) {
    var column = Column<component>()
    var rule = ComponentRule<component>()
```

`Spite.Namespace.instances` is a constant list, a `filter` whose function answers a constant folds, and the nested
`each` unrolls (item 206). It is longer than `Symbol<Component>` and says what it does: every folder named
`component` at any depth, which the old walk did without saying.

### The network codec

`$component_type.has_function("mirrored_from")` becomes `if $component_type.functions['mirrored_from']`, a name
written in the code. `send.spite`'s `encode_component(component: Symbol<Component>)` becomes the folder filter
above with `Network.Codec<component>()`.

### The JSON writer

Before:

```gdscript
write_attributes(shown, members)

func write_attribute(attribute: Symbol<$value_type>, shown: $value_type, members: List<String>) {
    var attribute_json = JsonWriter(shown.attributes[attribute])
    ...
    if $value_type.has_function("json_key_{attribute.name}") {
        var key = $value_type.returned_text("json_key_{attribute.name}")
```

After:

```gdscript
var members = shown.attributes.map(member)

func member(attribute: Spite.Attribute): String {
    var attribute_json = JsonWriter(attribute.value)
    ...
    var renamed = $value_type.functions.filter_starts_with("json_key_").find_by_suffix(attribute.name)
    if renamed {
        var key = renamed.returned_literal
```

`shown.class` is `$value_type`, a constant, so `shown.attributes` is a constant list of bound attributes and
`attribute.value` is typed. The key override (D273) used to build the function's name from the attribute's; it now
selects the `json_key_` functions and pairs one with the attribute. `find_by_suffix(name)` (the element whose name
is the prefix followed by exactly `name`) is not yet a spelling anyone chose ([section 9](#9-open-questions)). The
same function called from the REPL on an `Anything` still writes JSON, through the run-time path.

### Every class in the program

`func check_kind(kind: Symbol<Spite.Class>)` called as `check_kinds()` becomes
`Spite.Class.instances.each(check_kind)` with `check_kind(kind: Spite.Class)`, reading `kind.is_stateful` and
`kind.package_folder` directly. It is the list D49 already defined; D287 made a second spelling of it.

## 8. What Mortaro settled on the first review

1. Template parameters are spelled **`Spite.Attribute<Monster>`**; there is no separate `Symbol` idea.
2. **A class and a namespace with the same dotted name is a compile error.**
3. **`function.call_with(arguments.map(made))` is accepted** for spreading a walk into one call (item 98).
4. **"Every folder named X, at any depth" is an explicit filter** over `Spite.Namespace.instances`.
5. **No names built by interpolation**: a symbol is not a string. Selection is by filter over the names that exist,
   `functions.filter_ends_with("_each")`.
6. **Member templates that answer a collection take the member in the plural** (`classes.map_names()`), with a
   Rails-like `pluralize`/`singularize` in `String` ([section 5](#5-plural-member-templates)).
7. **Every reflection question is named for what it answers** (`function.waits()` was unclear;
   [section 6](#6-every-question-is-a-get-only-attribute-named-for-its-answer)).
8. **Every reflection member is a get-only attribute**, with no `()`.

## 9. What this answers in `mortaros_missing_decisions.md`

- **Resolved:** 25 (the forms `Json` needed: `Symbol<Label>` over another class, the plural and the kind names
  become `Spite.Attribute<Label>`, `.each`, `.is_list`...), 98 (`call_with`), 99 (`Symbol<System>`'s order,
  misspelling and depth become an explicit namespace filter, and an empty walk can be asserted), 100 (no pattern
  templates), 108 (a reflection object is an ordinary argument), 188 (`$value_type == Enum` becomes `.is_enum`),
  206 (a walk nested in a walk is a nested `each`), 223 (`.classes` and `.namespaces` stay: Mortaro's note writes
  `namespace.classes`).
- **Changed:** 10's D91/D105 part (member templates stay; `map_` goes plural), 89 (`source_folder` becomes an
  attribute; the naming question remains), 109 (reads and writes per attribute get their home,
  `function.read_attributes`/`written_attributes`; still needs the answer).

## 10. Decisions it supersedes or keeps

**Superseded** (a new row each, when built): D114 (arguments walked by a plural template), D115 (the folder walk
by `Symbol<Folder>`), D116's reading of a name (the naming convention itself, `update_each`, stays), D180 (a hole
matched against an enum), D219 (`argument_count(name)`), D228/D287/D288's `$T.`-with-a-name spellings
(`source_folder`, `Symbol<Spite.Class>`, `argument_class`), D261/D268's `function_writes_parameter(name, index)`,
D209's `function_waits(name)`, D273's built key name, D15 (members with no arguments only, and `map_` singular),
D212/D217/D220 in spelling only (rows filled by `each` over `$row_type.attributes`, borrows kept), and the plural
rule of Symbol codegen.

**Kept and generalised:** D6, D11, D12, D41, D49, D57, D88 (describing members read-only, now read as attributes),
D124/D165, D237 (now every constant reflection object), D91/D148 (member templates, functions passed as values),
D278 (private attributes are in `.attributes` for the class's own walks).

## 11. Migration

**Spite.**

1. Compiler (the large part): constant evaluation of reflection expressions over `ClassInfo`/`FunctionInfo`
   (rules 1-4, including text functions on constant names), specialisation of a function per constant reflection
   argument (rule 5, reusing template instantiation), unrolling `each`/`map`/`filter` over constant lists,
   `call_with` over a constant sequence, plural member resolution through the inflection table, member templates
   passing arguments on, the `Spite.Attribute<T>` parameter, the class/namespace clash error and the
   run-time-as-type error. Estimate: 2 500-3 500 new lines in `generator.spite` and `analysis/`, then removing the
   range, plural and passed-symbol machinery (around 90 functions, `namespace_walk`, `template_walk`) once nothing
   uses them. Both forms compile side by side in between, with the old one an error naming the new spelling at the
   end (as D315 does).
2. Library: the 12 walk templates in `json_writer`, `json_reader`, `binary_format`, `spite/debug_instance`; the
   member templates' parameters become `Spite.Attribute<$element_type>` and `map_member` becomes `map_members` in
   `list`, `vector`, `items`; the reflection members become attributes in `library/spite/*.spite`, with the new ones;
   `String.pluralize`, `singularize`, `without_suffix`, `without_prefix` and `String.Inflection`.
3. Conformance: about 20 of 28 stage6 programs and 8 of 13 diagnostics rewritten, every `map_<member>` call
   renamed; new programs for constant folding, specialisation, plural resolution and its errors, the run-time-as-type
   error and the REPL path.
4. Docs: metaprogramming.md's "Another class's attributes" and "A class's functions, a folder's classes and a
   name's pattern" and their rules (about 900 lines) rewritten into reflection.md, which becomes the one page for
   both; collections.md's `map_` and the member-template rules; values_and_types.md's enum walk; for_ai_writers.md;
   README.

**The engine package:** 17 files, 62 `Symbol<` lines, plus every `map_<member>` call. `runner.spite` is most of it (about 30
argument and phase templates become `each` over `function.arguments`), then `row`, `stream`, `link`, `pack`, `app`,
`component_rule`, the network plugin's three systems, `recipes/`, `spawn_bundle`, `field_count`, `link_count`.
Mechanical once the compiler accepts both forms.

## 12. Open questions

1. **Pairing a function with an attribute by name** (JSON's `json_key_<attribute>`, D273). With names never built,
   the writer selects the `json_key_` functions and needs one that pairs with the attribute. `find_by_suffix(name)`
   as above, or should the override move off names (a function taking the attribute, `json_key(attribute)`)?
2. **Predicates in the plural rule.** `monsters.map_is_alive()` keeps the predicate's name, as proposed, or is
   `map_` over a predicate an error pointing at `filter_`/`count_`?
3. **Reflection objects answering their name's text functions** (`functions.filter_ends_with("_each")`), with
   member templates passing arguments on. Or keep the element strict and write `filter_name_ends_with("_each")`?
