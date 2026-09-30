# Metaprogramming is reflection on ordinary objects (a proposal for D316)

**Status:** a design proposal, not a decision and not a build. Mortaro set the direction
([D316](../decisions.md)): templates stay only where they change a function's name; everything else that reads a
program's structure goes through `Spite.Class`, `Spite.Namespace`, `Spite.Function` and `Spite.Attribute` as
ordinary instances, the way Ruby does, and a user at the REPL reaches them the same way a program does. Everything
below that reads as a rule is proposed by Claude, unconfirmed.

## The answer in one table

| Today | Proposed | Why |
|---|---|---|
| `show_attribute(attribute: Symbol<Monster>, ...)` called as `show_health(troll)` | **stays** | the name carries the argument; `show_health` reads as well as `show(troll.health)` |
| `filter_<member>`, `sum_<member>`, `map_<member>`... (D15, D91) | **stay** | same reason; nothing changes in `library/list.spite` |
| `get_<attribute>`/`set_<attribute>`, `to_<type>()` (D311) | **stay** | same reason |
| The plural, `show_attributes(label, lines)` | **goes**: `Label.attributes.each(...)` / `.map(...)` | a loop in disguise; nobody ever calls the singular by name |
| `Symbol<$T.run_each>` over a function's arguments (D114) | **goes**: `$T.functions['run_each'].arguments` | the argument walk is a list on a `Spite.Function` |
| `Symbol<$T.phase_each>` name patterns and holes (D116, D180) | **goes**: `Phase.values.each(...)` and `$T.functions["{phase}_each"]` | a lookup by a name built from a constant |
| `Symbol<System>`, every folder named `system` (D115) | **goes**: `Spite.Namespace` objects, `namespace.classes` | a folder is a namespace, not IO |
| `Symbol<Spite.Class>` (D287) | **goes**: `Spite.Class.instances` (D49, already there) | two spellings of one list |
| `Symbol<Phase>` over an enum's values | **goes**: `Phase.values` | a list |
| `$T.function_waits("f")`, `$T.argument_count("f")`, `$T.function_writes_parameter("f", i)`, `$T.argument_class("f", i)`, `$T.returned_text("f")` | **go**: members of the objects, `$T.functions['f'].waits()`, `.arguments.count()`, `.arguments[i].is_written()`, `.arguments[i].class`, `.returned_text()` | a question about a function belongs on the function |
| Passing a template's symbol to a helper (D116 rules, item 108) | **goes** | a `Spite.Attribute` is an ordinary argument |
| `$T.has_function("f")`, `$T == List`, `$T.element_type`, `class.is_singleton()`, `has_state()`, `package_folder()` | **stay**, as ordinary members of `Spite.Class` | already the shape Mortaro asks for |

The one new rule that makes this work without giving up zero run time is **constant reflection**
([below](#the-crucial-part-nothing-is-looked-up-at-run-time)): a reflection object the compiler knows is a
compile-time constant, a list of them is unrolled, and a function handed one is compiled once for it -- which is
exactly what a template instance is today, reached through an ordinary call instead of a spelled name.

## 1. What exists today

Two systems grew side by side and never met.

**Reflection** ([reflection.md](../reflection.md), D6, D11, D12, D41, D49, D57) already has the object model
Mortaro describes: `Spite.Class` (`.name`, `.namespace`, `.attributes`, `.functions`, `.instances`,
`is_singleton()`, `has_function(name)`...), `Spite.Function` (`.name`, `.arguments`, `.returns`,
`call_function()`), `Spite.Argument`, `Spite.Attribute` (`.name`, `.class`, `.value: Anything?`) and
`Spite.Namespace` (`.name`, `.name_with_namespaces`, `.parent`, `.classes`, `.namespaces`), ordinary classes in
`library/spite/`. `Spite.Class.instances` is every class (D49). But it is run-time only: `.value` is a boxed
`Anything?`, and a walk over `.attributes` cannot use an attribute's class as a type.

**Metaprogramming** ([metaprogramming.md](../metaprogramming.md)) is where the typed work happens, and it is built
on `Symbol` parameters instead:

| Mechanism | Syntax | Used for |
|---|---|---|
| Symbol codegen | `func get_attribute(attribute: Symbol): attribute.class`, called `get_age()` | getters, setters, casts, user name templates |
| Another class's members | `attribute: Symbol<Label>`, `label.attributes[attribute]` | typed per-attribute code |
| The plural | `show_attributes(label, lines)` calls the singular once per attribute | every attribute walk (JSON, binary, debug, SlopEngine rows, packs, rules) |
| Member templates | `member: Symbol<$element_type>` in `List`, `Vector`, `Items` | `filter_`, `sum_`, `map_`... |
| Argument walk (D114) | `argument: Symbol<$T.run_each>`, plural as an argument list `run_each(made_arguments())` | SlopEngine's runner and stream |
| Name pattern (D116, D180) | `phase: Symbol<$T.phase_each>`, hole matched against `enum Phase` | systems placed by name |
| Folder walk (D115) | `system: Symbol<System>`, every folder named `system` | finding systems, components, recipes, assets |
| Every class (D287) | `kind: Symbol<Spite.Class>` | ECS rule checks, inspection |
| Enum walk | `course: Symbol<Course>` | walking phases |
| Passed symbol | `announce(phase, "before")` | cutting a long template into helpers |
| Folded questions | `$T.has_function("f")`, `function_waits`, `argument_count`, `function_writes_parameter`, `argument_class`, `returned_text`, `has_state()`, `package_folder()`, `$T == List`, `$T.element_type` | choosing branches while compiling |

## 2. Where it is used

Counted in `.spite` files, lines (build output and vendored packages left out):

| | library | compiler | conformance + diagnostics | benchmarks | SlopEngine `slop/` | SlopEngine `plugins/` | SlopEngine `examples/` |
|---|---|---|---|---|---|---|---|
| `Symbol<` parameters | 36 | 5 | 79 | 23 | 56 | 4 | 2 |
| `.attributes`/`.functions`/`.classes` | 42 | 16 | 65 | 11 | 18 | 0 | 0 |
| `has_function(` | 4 | 3 | 12 | 0 | 9 | 5 | 0 |
| `Spite.Class`/`Namespace`/`Attribute`/`Function` | 72 | 13 | 25 | 2 | 5 | 0 | 1 |

By mechanism:

- **Member templates** (stay): 24 of the library's 36 `Symbol<` lines (`list.spite`, `vector.spite`,
  `items.spite`).
- **Attribute walks through the plural** (go): the library's other 12 (`json_writer`, `json_reader`,
  `binary_format`, `spite/debug_instance`); SlopEngine's `component_rule`, `row` (4), `pack` (2), `link` (4),
  `field_count`, `link_count`, `spawn_bundle`, `runner.classify_attribute`.
- **Argument and name-pattern walks** (go): SlopEngine's `runner.spite` (about 30 templates) and `stream.spite` (3),
  `examples/template_wait_check`.
- **Folder walks** (go): `Symbol<Component>` in `app.spite` and the network plugin's `send`, `receive` (2) and
  `forget_arrived`; `Symbol<System>` in `app.spite`; `Symbol<Asset>`, `Symbol<Recipe>` in `recipes/`;
  `Symbol<Bundle>` in `world.spite`.
- **Conformance**: 28 stage6 programs and 13 diagnostics use `Symbol<`; about 20 of them test a mechanism that goes.
- **Compiler**: the plural and range machinery is in `bootstrap/source/generation/generator.spite` (`namespace_walk`
  at 5493, the plural check `misnamed_plural` at 11640, about 90 functions touching templates, plurals or ranges)
  and `template_info`, `template_walk`, `row_walk`.

## 3. The object model

The objects are the ones reflection.md already has. What changes is how they are reached, and that they are
ordinary collections.

| Object | Members (new ones in bold) |
|---|---|
| `Spite.Namespace` | `.name`, `.name_with_namespaces`, `.parent: Spite.Namespace?`, `.classes`, `.namespaces`, **`.every_class`** (this namespace and every one below it), **`.enums`** |
| `Spite.Class` | `.name`, `.namespace`, `.attributes`, `.functions`, `.instances`, **`.values`** (an enum's), `is_singleton()`, `has_function(name)`, `has_state()`, `package_folder()`, `source_folder()`, **`is_list()`, `is_dictionary()`, `is_optional()`, `is_enum()`**, `.element_type`, `.value_type` |
| `Spite.Function` | `.name`, `.arguments`, `.returns`, `call_function()`, **`waits()`**, **`returned_text()`** (a function returning a text literal), **`.class`** (whose it is) |
| `Spite.Argument` | `.name`, `.class`, **`.index`**, **`is_written()`** (D261's study), **`.function`** |
| `Spite.Attribute` | `.name`, `.class`, **`.index`**, **`.owner: Spite.Class`**, **`.camel_case_name`, `.pascal_case_name`**, and `.value` only when bound ([below](#an-attributes-value-bound-like-a-function)) |

**Every member that is a collection is an ordinary `List`**, so everything a list has works: `[]` by name
answering `T?` (a `List` of reflection objects keys by `.name`, like a `Dictionary`), `count()`, `each`, `map`,
`filter`, and every member template -- `Monster.attributes.map_name()`, `Shop.classes.filter_is_singleton()`,
`namespace.classes.map_name()`, exactly Mortaro's note.

**How they are reached**, all already partly true:

- **A class name read as a value is its `Spite.Class`**: `Monster.attributes`, `Monster.functions['alive']`.
  D165 limits the bare form to where a `Spite.Class` is expected; member access on it is already allowed
  (`Gadget.name`). Unchanged: `var kind = Monster` with no type stays an error (D165).
- **A namespace name read as a value is its `Spite.Namespace`**: `Shop.Tools.classes`. Where a class and a
  namespace share a name there is a choice to make ([open question 2](#8-open-questions-for-mortaro)); with only one
  of them, as with SlopEngine's `component/` folders, there is no clash.
- **`class`** inside a function is the class it answers on (unchanged), and **`namespace`** is its namespace
  (new, the same kind of name).
- **`$T`** is a class object wherever a value is read, as `$T.has_function(...)` already treats it.
- **`value.class`** answers the value's real class (unchanged).
- **`Spite.Class.instances`** is every class (D49) and **`Spite.Namespace.instances`** every namespace, so
  "every folder named `system` anywhere" is `Spite.Namespace.instances.filter(is_system)`.
- **`Phase.values`** is an enum's values, in order.

### An attribute's value, bound like a function

A function named on an instance is a bound `Spite.Function` (D17). An attribute does the same: **`Monster.attributes`
describes declarations, `troll.attributes` holds attributes bound to `troll`** (D11 already says so). A bound
attribute's `.value` is readable and writable: `attribute.value = 3`. And an unbound attribute indexes an instance,
`troll.attributes[attribute]`, which is how a walk over a class's attributes reaches one instance's values -- the
form templates use today, now taking a `Spite.Attribute` instead of a `Symbol`. D88's "read-only" stays for
everything describing the program (`.name`, `.class`); `.value` describes the instance.

## The crucial part: nothing is looked up at run time

Spite promises that nothing is looked up while a program runs and that unused code is not emitted (D147, D176,
AGENTS.md). Ruby's surface would break that if it were built the Ruby way. It is not: **it folds**.

### Constant reflection

A reflection object is a **constant** when the compiler knows which one it is:

1. a class or namespace named in the code, `$T`, `class` and `namespace`, an enum's value named in the code;
2. `value.class` when `value`'s declared type is a class (not a `type` or a union);
3. any member of a constant (`Monster.functions['alive']`, `.namespace`, `.arguments[0]`), and `[]` with a constant
   key -- including text built only from constants, `"{phase}_each"` with `phase` a constant enum value;
4. every element of a constant list (`.attributes`, `.classes`, `Spite.Class.instances`, `Phase.values`, and a
   `filter`, `map` or member template applied to one);
5. **a parameter of a reflection type (`Spite.Class`, `Spite.Attribute`, `Spite.Function`, `Spite.Argument`,
   `Spite.Namespace`, or an enum) in a call whose argument is a constant.** The function is compiled once for each
   constant it is called with -- a *specialisation* -- and inside it the parameter is a constant.

Rule 5 is today's template instance, reached through a call instead of a spelled name: `show_attribute_for_health`
in `--final-classes` is the same function it is now. It is what D237 already says for `attribute.class` inside a
template, extended to every reflection object.

What folds:

- **Questions** on a constant are constants (`has_function`, `is_singleton()`, `waits()`, `== Entity`,
  `.count()`), so `if` keeps only the branch taken, as it does today (D167, D237).
- **`each` over a constant list is unrolled**: `Monster.attributes.each(show)` is `show_for_name(); show_for_health()`
  in the generated code, each call a specialisation. `filter`, `map` and member templates over a constant list
  fold to a constant list. No list exists at run time.
- **A constant class or attribute class is usable as a type**: `Column<component>()`,
  `var value: attribute.class = ...`, `): argument.class` -- as `Column<component.class>()` is today.
- **`troll.attributes[attribute]` with a constant `attribute`** is a plain typed field read or write (a getter
  call where one exists), exactly what the template reads today; a bound attribute's `.value` is typed
  `attribute.class`.
- **`[]` with a constant key answers a constant `T?`**: `assert Monster.functions['run_each']` folds to nothing when
  it is there. When it is not, the specialisation halts on every call, the way `crash $T.has_function(...)` does
  today, and a walk that reaches it only in a branch it rules out never compiles it. A misspelled constant key
  reached unconditionally is a compile error naming what the class does have (D244).
- **An empty constant list walks nothing**, as the plural does, but `crash components.count() > 0` now folds and
  says so -- the loud answer item 99 lacked.

### What is not constant

`value.class` through a `type` or union, `Spite.Class.instances` filtered by a run-time value, a reflection object
kept in an attribute and read later, and everything typed at the REPL prompt. These are **run-time reflection**,
the tables reflection.md already describes, built only for the classes such a read can reach (D57), and every class
in a `--repl`/`--repl-port`/`--development` build (D143). There `.value` is `Anything?`, boxed, as today.

**Using a reflection object as a type needs a constant.** A specialisation that writes `attribute.class` as a type,
`Column<component>`, or indexes an instance typed, called with a run-time value, is a compile error naming the
call that made it run-time:

```diagnostic
'attribute' is not known while compiling here -- it comes from 'value.attributes' through the type 'Anything' in describe() -- so 'attribute.class' cannot be a type; walk 'Gadget.attributes' instead, or read 'attribute.value'
```

So one function body serves both worlds: called with constants it is typed and unrolled, and a REPL user calling
the same function with a live object gets the run-time path wherever the body does not need a type.

### What it costs

The same as today. A constant walk emits what the plural emits, one specialised function per member; a run-time walk
emits the tables D57 already emits, only when reached. A program that walks nothing carries nothing. The compiler
cost moves: constant evaluation of reflection expressions replaces range resolution, and it is the same
monomorphisation step template instances use.

## 4. Before and after

### The README arena

The name-changing template stays. Only the plural changes; the arena did not use it.

```gdscript
show_name(troll)
show_health(troll)

func show_attribute(attribute: Symbol<Monster>, monster: Monster) {
    console.print(attribute.name, "=", monster.attributes[attribute])
}
```

The same, unchanged. Inside, `attribute` is now the `Spite.Attribute` for `health` -- same members, `.name` and
`.class`, so nothing to rewrite. To show every attribute, instead of `show_attributes(troll)`:

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
`target.run_each(made_for_hero(), made_for_pet())` (item 98, open question 3).

### SlopEngine's component rule

Before:

```gdscript
check_attributes(sample)

func check_attribute(attribute: Symbol<$component_type>, value: $component_type) {
    var folded = AttributeRule<$component_type, attribute.class>()
    ...
    if attribute.class == Entity {
        var linked = value.attributes[attribute].id
```

After:

```gdscript
$component_type.attributes.each(check_attribute)

func check_attribute(attribute: Spite.Attribute) {
    var folded = AttributeRule<$component_type, attribute.class>()
    ...
    if attribute.class == Entity {
        var linked = sample.attributes[attribute].id
```

`attribute.class.is_optional()` replaces `type_name.ends_with("?")`, a text test on a type's name.

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
Spite.Namespace.instances.filter(holds_components).each(register_namespace)

func holds_components(namespace: Spite.Namespace): Boolean {
    return namespace.name == 'Component'
}

func register_namespace(namespace: Spite.Namespace) {
    namespace.classes.each(register_component)
}

func register_component(component: Spite.Class) {
    var column = Column<component>()
    var rule = ComponentRule<component>()
```

`Spite.Namespace.instances` is a constant list, `filter` with a function answering a constant folds, and the
nested `each` unrolls (item 206). When the engine's components move under one root namespace, it is
`Component.every_class.each(register_component)`.

### The network codec

`$component_type.has_function("mirrored_from")` stays as written: it is already an ordinary member of
`Spite.Class` asked of a constant. `send.spite`'s `encode_component(component: Symbol<Component>)` becomes the
namespace walk above with `Network.Codec<component>()`.

### SlopEngine's runner: phases by name

Before:

```gdscript
place_phases_each()

func place_phase_each(phase: Symbol<$system_type.phase_each>) {
    placed_in = phase.name
    prepare_arguments()
}

func prepare_argument(argument: Symbol<$system_type.phase_each>) {
    var row = Row<argument.class>()
```

After:

```gdscript
Phase.values.each(place)

func place(phase: Phase) {
    var function = $system_type.functions["{phase}_each"]
    if function {
        placed_in = phase
        function.arguments.each(prepare)
    }
}

func prepare(argument: Spite.Argument) {
    var row = Row<argument.class>()
```

The pattern hole, its enum rule (D180) and the member that changes meaning inside a pattern template (item 100)
are gone: the name is built from a constant enum value and looked up. `$system_type.function_waits("<phase>_each")`
becomes `function.waits()`, and `phase.argument_count()` becomes `function.arguments.count()`.

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
    var key_function = $value_type.functions["json_key_{attribute.name}"]
    if key_function {
        var key = key_function.returned_text()
```

`shown.class` is `$value_type`, a constant, so `shown.attributes` is a constant list of bound attributes and
`attribute.value` is typed. The same function called from the REPL on an `Anything` still writes JSON, through
the run-time path.

### Every class in the program

`func check_kind(kind: Symbol<Spite.Class>)` called as `check_kinds()` becomes
`Spite.Class.instances.each(check_kind)` with `check_kind(kind: Spite.Class)`, reading `kind.has_state()` and
`kind.package_folder()` directly. It is the list D49 already defined; D287 made a second spelling of it.

## 5. What this answers in `mortaros_missing_decisions.md`

- **Resolved:** 25 (the forms `Json` needed: `Symbol<Label>` over another class, the plural and the kind names
  become `Label.attributes`, `.each`, `is_list()`...), 98 (the plural as an argument list becomes `call_with`),
  99 (`Symbol<System>`'s order, misspelling and depth become explicit namespace code, and an empty walk can be
  asserted), 100 (no pattern templates), 108 (a reflection object is an ordinary argument), 206 (a walk nested in a
  walk is a nested `each`), 223 (`.classes` and `.namespaces` stay: Mortaro's note writes `namespace.classes`).
- **Changed:** 188 (`$value_type == Enum` becomes `$value_type.is_enum()`, so the word question goes away), 10's
  D91/D105 part (member templates stay; the chain fusion is unaffected), 89 (`source_folder()` becomes a plain
  member; the naming question remains), 109 (reads and writes per attribute get their natural home,
  `function.reads`/`function.writes` as lists of `Spite.Attribute`; still needs the answer).

## 6. Decisions it supersedes or keeps

**Superseded** (a new row each, when built): D114 (arguments walked by a plural template), D115 (the folder walk
by `Symbol<Folder>`), D116's reading of a name (the naming convention itself, `update_each`, stays), D180 (a hole
matched against an enum), D219 (`argument_count(name)`), D228/D287/D288's `$T.`-with-a-name spellings
(`source_folder`, `Symbol<Spite.Class>`, `argument_class`), D261/D268's `function_writes_parameter(name, index)`,
D209's `function_waits(name)` spelling, D212/D217/D220 in spelling only (rows filled by `each` over
`$row_type.attributes`, borrows kept), and the plural rule of Symbol codegen.

**Kept and generalised:** D6, D11, D12, D41, D49, D57, D88 (describing members read-only), D124/D165, D237 (now
every constant reflection object), D15/D91/D148 (member templates, functions passed as values), D278 (private
attributes are in `.attributes` for the class's own walks).

## 7. Migration

**Spite.**

1. Compiler (the large part): constant evaluation of reflection expressions over `ClassInfo`/`FunctionInfo`
   (rules 1-4), specialisation of a function per constant reflection argument (rule 5, reusing template
   instantiation), unrolling `each`/`map`/`filter` over constant lists, `call_with` over a constant sequence, and
   the diagnostic for a run-time value used as a type. Estimate: 2 000-3 000 new lines in `generator.spite` and
   `analysis/`, then removing the range, plural and passed-symbol machinery (around 90 functions, `namespace_walk`,
   `template_walk`) once nothing uses them. Both forms compile side by side in between, with the old one an error
   naming the new spelling at the end (as D315 does).
2. Library: the 12 walk templates in `json_writer`, `json_reader`, `binary_format`, `spite/debug_instance`; new
   members in `library/spite/*.spite` (`every_class`, `values`, `index`, `waits()`, `is_written()`, `is_enum()`...).
   Member templates untouched.
3. Conformance: about 20 of 28 stage6 programs and 8 of 13 diagnostics rewritten; new ones for constant folding,
   specialisation, the run-time-as-type error and the REPL path.
4. Docs: metaprogramming.md sections "Another class's attributes", "A class's functions, a folder's classes and a
   name's pattern" and their rules (about 900 lines) rewritten into reflection.md, which becomes the one page for
   both; values_and_types.md's enum walk; for_ai_writers.md's walk rules; README's sentence on the plural.

**SlopEngine:** 17 files, 62 `Symbol<` lines. `runner.spite` is most of it (about 30 argument and phase templates
become `each` over `function.arguments`), then `row`, `stream`, `link`, `pack`, `app`, `component_rule`, the
network plugin's three systems, `recipes/`, `spawn_bundle`, `field_count`, `link_count`. Mechanical once the
compiler accepts both forms; the Theseus port is unaffected beyond its own components' walks.

## 8. Open questions for Mortaro

1. **The name template's parameter type.** A template that stays is still written `attribute: Symbol<Monster>`,
   though inside it `attribute` is a `Spite.Attribute`. Keep that spelling, or write
   `attribute: Spite.Attribute<Monster>` so the parameter says what it is?
2. **A namespace and a class with one name.** `Component` the folder and a class `Component` can both exist. Should
   the class win, with the namespace reached as `Spite.Namespace.instances['Component']`, or is having both an error?
3. **Spreading a walk into a call.** `function.call_with(function.arguments.map(made))`, a sequence that is not a
   `List` and exists only to be spread, is the one new form. Accept it, or keep the plural-as-argument-list (item 98)
   as the one surviving plural?
4. **"Every folder named `component`, at any depth"** (D115) becomes `Spite.Namespace.instances.filter(...)`,
   explicit and longer. Fine, or give `Spite.Namespace` a member for it (`Spite.Namespace.named('Component')`)?
