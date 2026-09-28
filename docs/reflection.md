# Reflection

Everything is a class, including classes. A class is an instance of `Spite.Class`; a function is an instance of
`Spite.Function`; a namespace, an attribute and an argument are instances of `Spite.Namespace`,
`Spite.Attribute` and `Spite.Argument`. They are ordinary classes in `library/spite/`, so reflection is not a
second language beside Spite: it is Spite describing itself, reachable by name at run time and from the REPL.

Reflection is resolved at compile time wherever it can be, and **what a program does not read is not
emitted**: `value.attributes` is built only for a class something reads `.attributes` of, and the table of every
class exists only in a program that asks for it. So reflection can be as detailed as it likes -- an unused member
costs nothing.

What a read does cost at run time is what it builds, and only where it is written: each read of `.attributes`
makes a fresh list with one entry per field; reading `attribute.value` boxes a number field into an object;
`Monster.instances` makes every construction and release of a `Monster` add or remove it from a list, and a class
nobody asks the instances of keeps no list. Everything else -- names, classes, namespaces, `has_function` asked of
a generic's type -- is constant data or folds while compiling. The whole list is in
[optimizations.md](optimizations.md#reflection-symbols-and-registries-only-where-read).

## The objects

| Object | Members |
|---|---|
| `Spite.Class` | `.name: Symbol`, `.namespace: Spite.Namespace?`, `.attributes`, `.functions`, `.instances`, `is_singleton()`, `has_function(name)`, `function_waits(name)`, `argument_count(name)`, `source_folder()` |
| `Spite.Function` | `.name`, `.arguments: List<Spite.Argument>`, `.returns: Spite.Class` (`Nothing` when none is declared), `call_function()`, `name_fits(pattern)`, `waits()`, `argument_count()` |
| `Spite.Argument` | `.name`, `.class: Spite.Class` |
| `Spite.Attribute` | `.name`, `.class: Spite.Class`, `.value: Anything?` (the value itself; `.value.to_string()` is its text) |
| `Spite.Namespace` | `.name` (the segment), `.name_with_namespaces` (dotted), `.parent: Spite.Namespace?`, `.classes`, `.namespaces` |
| `Spite.Memory` | `.address: Long`, `.bytes: Long`, `.section` (`'heap'`, `'stack'`, `'constant'`) -- see [memory.md](memory.md#where-a-value-lives-memory) |

A name is a `Symbol`: text from the program's own table of symbols, which reads as text anywhere text is
expected (every `String` function answers on it) and costs no allocation to pass around.

## `.class` and `.attributes`

`value.class` is a `Spite.Class`, and printing one prints its `.name`. `value.attributes` is a real, run-time
`List<Spite.Attribute>`, one entry per field, each with its `.name`, its `.class`, and its `.value` -- the value
itself, an `Anything?` ([below](#an-attributes-value)), whose text is `.value.to_string()`:

```gdscript title=reflection_basics/gadget.spite
var name = ""
var power = 0

func Gadget(new_name: String, new_power: Integer) {
    name = new_name
    power = new_power
}
```
```gdscript title=reflection_basics/reflection_basics.spite entry
var console = Console()

func ReflectionBasics() {
    var gadget = Gadget("wrench", 3)
    describe(gadget)
}

func describe(gadget: Gadget) {
    console.print("class", gadget.class)
    console.print("class name", gadget.class.name)
    console.print("own class", class.name)
    console.print("declared name", Gadget.name)
    var attributes = gadget.attributes
    attributes.each(show_attribute)
}

func show_attribute(attribute: Spite.Attribute) {
    assert attribute.value
    var shown = attribute.value.to_string()
    console.print(attribute.name, attribute.class, shown)
}
```
```output
class Gadget
class name Gadget
own class ReflectionBasics
declared name Gadget
name String wrench
power Integer 3
```

- **`class` is not a keyword.** Inside any function of a class it is the class of the instance that function
  answers on -- an attribute every instance inherits -- so `own class` above names the class the function was
  written in. A local named `class` takes precedence inside its function; an attribute or function cannot be
  named `class` ([below](#the-names-reflection-gives-every-object)).
- **A class name reads its own class object**, member by member, with no instance anywhere: `Gadget.name` is
  `"Gadget"`, and `Gadget.namespace` is the same namespace `gadget.class.namespace` gives. A class name is not a
  value you can call a function of the class on, though: there are no static functions, so `Gadget.boost()` is
  an error saying to make a `Gadget()` first.
- **`.class` through a shape answers the real class.** Read through a `type` or a union, `.class` comes from the
  object's own tag at run time, so it names the class the value really is; an object literal answers `Object`.
- `value.attributes` holds the values; `Gadget.attributes` describes the declarations. The same word is right at
  both levels, and the case of the receiver says which one you mean.

### The names reflection gives every object

`class`, `attributes`, `functions`, `instances` and `memory` belong to reflection on every class, so a class
cannot declare an attribute or a function of its own by those names, nor a `get_` or `set_` function that would be
read as one ([D246](decisions.md), decided by Claude under D205; the list proposed by Claude, unconfirmed). One
named `attributes` would otherwise hide the real list from everything that walks a class -- `JsonReader` would read
every key as a key the class does not have. The error names what the member means and a name to use instead:

```gdscript title=reflection_names/reflection_names.spite entry error
var console = Console()
var attributes = List<String>()

func ReflectionNames() {
    attributes.append("heavy")
    console.print(attributes.count())
}
```
```diagnostic
the attribute 'attributes' has a name reflection gives every object: 'value.attributes' lists its attributes, and JsonReader, JsonWriter and every attribute walk read through it, and one of its own would hide it. Name it something else, like 'reflection_names_attributes'
```

A local variable or a parameter may still use any of these names, since nothing reads it through an object.

### An attribute's value

`.value` is the instance the attribute refers to, as an object of any class: `Anything?`. `Anything` is the
library's empty `type`, the counterpart of `Nothing`: every class fits it, so a function that takes any object
says `component: Anything`, and a program never declares its own. A number, `Boolean`, enum or `Symbol` attribute
is boxed, as it is whenever a plain value goes into a `type`; the value is `null` only when the attribute holds
`null`. Its text is `.value.to_string()`: a class's own `to_string()` when it has one, a number's or a `String`'s
usual text, and `to_debug()` for anything else (a list, or a class that does not say how to print). Only a
program that reads `.value` builds the objects; every other program's attributes carry nothing. `.attributes`
works through a `type` too, answered from the value's real class at run time, so a function taking anything can
walk what it was given and hand each attribute on:

```gdscript title=attribute_objects/health.spite
var amount = 10
```
```gdscript title=attribute_objects/player.spite
var health = Health()
var speed = 3
var target: Health? = null
```
```gdscript title=attribute_objects/attribute_objects.spite entry
var console = Console()
var components = List<Anything>()

func AttributeObjects() {
    var player = Player()
    add_every_attribute(player)
    components.each(show_component)
}

func show_component(component: Anything) {
    var described: String = component.class
    if component == Health {
        described = "Health {component.amount}"
    }
    console.print(described)
}

func add_every_attribute(bundle: Anything) {
    var attributes = bundle.attributes
    attributes.each(add_value)
}

func add_value(attribute: Spite.Attribute) {
    assert attribute.value
    components.append(attribute.value)
}
```
```output
Health 10
Integer
```

### Passing a class

A class name given where a `Spite.Class` is wanted -- an argument, a `var` declared `Spite.Class`, an assignment
to one, a `return` -- is its class object: you are not passing the class, you are passing the instance of
`Spite.Class` that describes it. Anywhere else a bare class name is still the error that says to write `Gadget()`,
since that is almost always what was meant. `==` between a `Spite.Class` and a class name compares the class
objects, so a function can ask which class it was handed:

```gdscript title=passing_a_class/ui/pressed.spite
func count(): Integer {
    return 1
}
```
```gdscript title=passing_a_class/health.spite
func amount(): Integer {
    return 10
}
```
```gdscript title=passing_a_class/passing_a_class.spite entry
var console = Console()
var components = List<Anything>()

func PassingAClass() {
    var health = Health()
    components.append(health)
    var pressed = Ui.Pressed()
    components.append(pressed)
    remove_component(Ui.Pressed)
    var kind: Spite.Class = Health
    var kept = components.count()
    crash components[0]
    console.print(kept, components[0].class == kind, kind == Health)
}

func remove_component(component_class: Spite.Class) {
    var kept = List<Anything>()
    var index = 0
    while index < components.count() {
        var component = components[index]
        if component.class != component_class {
            kept.append(component)
        }
        index = index + 1
    }
    components = kept
}
```
```output
1 true true
```

## Namespaces

A class in a folder has a namespace ([packages.md](packages.md)); a class at the root of its program has none.
`.namespace` is a `Spite.Namespace?` -- narrow it before reading its members -- and printing a namespace prints its
dotted `.name_with_namespaces`. `.parent` walks up the tree and `.classes` and `.namespaces` walk down it.
Comparing needs no narrowing: a namespace compares with text through its `name_with_namespaces`, and a class
with no namespace simply compares unequal.

```gdscript title=namespace_walk/shop/tools/hammer.spite
func weight(): Integer {
    return 2
}
```
```gdscript title=namespace_walk/namespace_walk.spite entry
var console = Console()

func NamespaceWalk() {
    var hammer = Shop.Tools.Hammer()
    describe(hammer)
    var in_shop = NamespaceWalk.namespace == "Shop"
    console.print("entry class in Shop", in_shop)
}

func describe(hammer: Shop.Tools.Hammer) {
    var described = hammer.class
    assert described.namespace.parent
    console.print(described.name, described.namespace, described.namespace.name)
    console.print("parent", described.namespace.parent)
}
```
```output
Hammer Shop.Tools Tools
parent Shop
entry class in Shop false
```

One `assert` proves the whole path it names and every prefix of it, so `described.namespace` is a plain value
too after `assert described.namespace.parent` ([failure.md](failure.md#narrowing-a-path)).

## Functions and arguments

`.functions` lists the functions a class declares, plus any a Symbol codegen template generated for it -- the
program as built, not the source as written. A constructor is not one of them: it does not answer on an
instance, it makes one. A function named without calling it is a `Spite.Function` too, bound to its instance
([functions_and_operators.md](functions_and_operators.md#functions-are-values)), so the value and its
reflection are the same object.

```gdscript title=function_reflection/gadget.spite
var power = 0

func boost(amount: Integer, times: Integer): Integer {
    power = power + amount * times
    return power
}

func reset() {
    power = 0
}
```
```gdscript title=function_reflection/function_reflection.spite entry
var console = Console()

func FunctionReflection() {
    var functions = Gadget.functions
    functions.each(describe)
    var library_names = Memory.Heap.functions.map_name()
    var has_allocate = library_names.contains('allocate')
    console.print("Memory.Heap has allocate", has_allocate)
}

func describe(function: Spite.Function) {
    var arguments = function.arguments
    var argument_names = arguments.map_name()
    var joined_names = argument_names.join(", ")
    console.print(function.name, "({joined_names})", function.returns)
}
```
```output
boost (amount, times) Integer
reset () Nothing
Memory.Heap has allocate true
```

A class of the standard library describes itself the same way: `Memory.Heap.functions` lists `allocate`,
`resize`, `free` and `live_allocations`, the functions whose bodies the compiler supplies.

### Asking for a function by name or pattern

`has_function(name)` on a class asks whether it has a function of that name. The name may also be a pattern with
one hole, `"<phase>_each"`, which is true when some function's name fits it with something in the hole.
`name_fits(pattern)` asks the same of one `Spite.Function`. Both are ordinary Spite in `library/spite/`.

```gdscript title=function_questions/sprinkler.spite
var water = 0

func update_each(amount: Integer) {
    water = water + amount
}

func drain() {
    water = 0
}
```
```gdscript title=function_questions/function_questions.spite entry
var console = Console()

func FunctionQuestions() {
    var described = Sprinkler.name
    var drains = Sprinkler.has_function("drain")
    var fills = Sprinkler.has_function("fill")
    var updates = Sprinkler.has_function("<phase>_each")
    console.print(described, drains, fills, updates)
    var functions = Sprinkler.functions
    functions.each(describe)
}

func describe(function: Spite.Function) {
    var in_phase = function.name_fits("<phase>_each")
    console.print(function.name, in_phase)
}
```
```output
Sprinkler true false true
update_each true
drain false
```

Here the answer is found at run time, from `.functions`, and the hole is any text. Asked of a generic's type, as
in `if $system_type.has_function("run_each")`, it is decided while compiling instead, and only the branch taken is
compiled; there a pattern's hole matches only the values of the enum named for it, `Phase` for `<phase>`, as a
name pattern's does (D180). Walking a function's arguments, a folder's classes or the functions that fit a pattern at compile time
is in [metaprogramming.md](metaprogramming.md#a-classs-functions-a-folders-classes-and-a-names-pattern).

## Instances, and every class in the program

`Monster.instances` is every instance of `Monster` alive at that moment, in the order they were made.
`Spite.Class.instances` is every class in the program -- its own folder and every package it loads, not the
standard library -- because a class is an instance of `Spite.Class`. That is how the test package finds its tests
without being told about them ([testing.md](testing.md)).

A class object's `.attributes` and `.functions` are read from a stand-in the program makes for the purpose, with
every attribute at its default -- its constructor never runs, whatever it takes, so describing a class prints
nothing and opens nothing -- and the stand-in is not one of `.instances`: walking `Gadget.functions` leaves
`Gadget.instances` as it was. A singleton's stand-in is not the singleton, so describing `Console` neither makes
the program's console nor keeps a second one alive. Its attributes are released when it goes, but its `drop()`
never runs: that belongs to the one real instance, at exit. A singleton whose attributes are all settings the
compiler already knows, such as `Build`, holds nothing at run time, so its `.attributes` answer those settings:
`build.class.attributes` lists `run` with the value `true`, `repl_port` with `0`, and so on. The program's entry
class is described with no stand-in at all, since making one would run the program again, so its `.functions` is
empty and `has_function` asked of it is `false`. `function_waits` asked of it still answers, from a list of its
functions described with no instance behind them, which nothing can call and only `function_waits` reads
(`conformance/stage6/entry_function_waits`).

Which singletons are objects at all depends on the build ([D143](decisions.md)): in a `--development`,
`--hot-reload`, `--repl` or `--repl-port` build a singleton that holds nothing, such as `Build`, is an ordinary
object you can find in its `.instances`; in every other build it is one static object that `.instances` does not
list ([optimizations.md](optimizations.md#singletons-that-hold-nothing-are-static-objects)).

```gdscript title=live_registry/monster.spite
var name = ""

func Monster(starting_name: String) {
    name = starting_name
}
```
```gdscript title=live_registry/live_registry.spite entry
var console = Console()
var kept = List<Monster>()

func LiveRegistry() {
    var rat = Monster("rat")
    kept.append(rat)
    var bat = Monster("bat")
    kept.append(bat)
    spawn_and_forget()
    var names = Monster.instances.map_name()
    var joined_names = names.join(", ")
    console.print("alive:", joined_names)
    var classes = Spite.Class.instances
    var class_names = classes.map_name()
    var knows_monster = class_names.contains('Monster')
    console.print("the program has Monster", knows_monster)
}

func spawn_and_forget() {
    var ghost = Monster("ghost")
    var instances_count = Monster.instances.count()
    console.print("while the ghost is alive:", instances_count, ghost.name)
}
```
```output
while the ghost is alive: 3 ghost
alive: rat, bat
the program has Monster true
```

## Reflection is read-only

Every member above is a getter with no setter: `library/spite/class.spite` keeps its data in private fields
(`_name`, `_namespace`, `_attributes`, ...) and answers `get_name()`, `get_namespace()` and the rest, so `.name`
reads through the ordinary attribute interception and writing it is an error. A name starting with `_` is used
only inside its own class, so the private fields cannot be written from outside either.

```gdscript title=reflection_read_only/reflection_read_only.spite entry error
var console = Console()

func ReflectionReadOnly() {
    var described = console.class
    described.name = "Terminal"
}
```
```diagnostic
'name' is read-only
```

A reopening of a `Spite` class is inside it, so it may read those fields; that is how a program adds a member
to every class object at once ([packages.md](packages.md#the-spite-namespace-is-reserved)).

The few members that reach the live value behind a reflection object for the REPL, such as `call_function()`,
have bodies only the compiler can write; they are its own reopening of those classes, and `--final-classes`
prints them like any other member ([the rules in full](#reflection-objects--partial)).

## Functions of `Spite.Class`, and no static functions

There are no static functions and there will not be any: a class is an instance of `Spite.Class`, so a function
that belongs to the class rather than to its instances is an ordinary function of `Spite.Class`, declared there
with its default. `is_singleton()` is one: every class answers it, from its `singleton` line
([classes_and_files.md](classes_and_files.md#singletons)), and declaring `is_singleton()` in a class is an error
that says to write the `singleton` line instead. A reopening of `Spite.Class` changes what every class object
answers, for the whole program -- a foot you are allowed to shoot, and one that shows in `--final-classes`
([the rules in full](#functions-of-spiteclass-and-why-there-are-no-static-functions--partial)).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Metaprogramming

Everything is a class, including classes. Reflection is resolved at compile time wherever possible.

#### Reflection objects  **[partial]**

D12 (decided by Mortaro, 2026-09-19): **every reflection object lives in the `Spite` namespace** -- `Spite.Class`,
`Spite.Function`, `Spite.Argument`, `Spite.Attribute`, and whatever else reflection grows (`Spite.Class`, not
`SpiteClass`). One namespace, and nothing in it can be mistaken for a user class. `Spite` is a reserved root
namespace: a program's `spite/` folder may only reopen its classes, and `load "spite"` is an error
([packages.md](packages.md#the-spite-namespace-is-reserved)). **[implemented]**

| Object | Members |
|---|---|
| `Spite.Class` | `.name: Symbol` (D68), `.namespace: Spite.Namespace?`, `.attributes`, `.functions`, `.instances`, `is_singleton()`, `has_function(name)` (D114; folds on a codegen type), `function_waits(name)` (D209; folds the same way, [metaprogramming.md](metaprogramming.md#asking-whether-a-function-waits)), `argument_count(name)` (D219: how many arguments the first function whose name fits takes, 0 for none; folds the same way, [metaprogramming.md](metaprogramming.md#asking-how-many-arguments-a-function-takes)), `source_folder()` (D228: the absolute folder of the file declaring the class, folded where the class is known, [packages.md](packages.md#files-beside-a-packages-source)) -- the functions of `Spite.Class` ([below](#functions-of-spiteclass-and-why-there-are-no-static-functions--partial)) |
| `Spite.Function` | `.name: Symbol`, `.arguments: List<Spite.Argument>`, `.returns: Spite.Class` (`Nothing` when none is declared), `call_function()`, `name_fits(pattern)` (D116), `waits()` (D209: whether it can reach a wait), `argument_count()` (D219: how many `.arguments` it has) |
| `Spite.Argument` | `.name: Symbol`, `.class: Spite.Class` |
| `Spite.Attribute` | `.name: Symbol`, `.class: Spite.Class`, `.value: Anything?` (the value itself; its text is `.value.to_string()`, below) |
| `Spite.Namespace` | `.name: Symbol` (the segment), `.name_with_namespaces: Symbol` (dotted), `.parent: Spite.Namespace?`, `.classes`, `.namespaces` |
| `Spite.Memory` | `.address: Long`, `.bytes: Long`, `.section: Spite.Memory.Section` (`'heap'`, `'stack'`, `'constant'`) -- what `value.memory` answers (D101) |

**Every member above is a getter, and none has a setter** (D88, decided by Mortaro; built 2026-09-24).
`library/spite/class.spite` and its neighbours keep the data in private fields (`_name`, `_namespace`,
`_attributes`, ...) and answer `get_name()`, `get_namespace()` and so on, so `described.name` reads through the
ordinary getter interception and `described.name = ...` is "'name' is read-only" (`diagnostics/reflection_read_only`).
Everything reflection offers is ordinary code in `library/spite/`, reachable by name at run time and from the
REPL (D92). The members only the compiler can write -- `value_attributes()`, `value_functions()` and
`assign(text)` on `Spite.Attribute`, `call_function()` and `call_with_text(arguments)` on `Spite.Function` -- are
the compiler's reopening of those classes (D82, [Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)), printed by `--final-classes` like any member. A class
of the standard library describes its members too: `Memory.Heap.functions` lists every function `Memory.Heap`
has, supplied ones included. **The list holds every function the class has once compiling ends**, in the order
they were made (proposed by Claude, unconfirmed): its C is written again at the end whenever the class gained a
function after it was first written. Before, a function the compiler made for the class after its list was
written -- a template instance another class asked for, the automatic `to_debug` -- was missing from it, and
whether it was depended on the order the classes were compiled in, which a reload cannot reproduce.

**A name starting with `_` is private** (D137, decided by Mortaro: everywhere but on a parameter, where `_`
means unused on purpose; the reach below proposed by Claude, unconfirmed, [Lexical structure](classes_and_files.md#lexical-structure--implemented)):
it is read, written or called only inside its own class (a reopening is inside), and from anywhere else it is
an error naming the getter when there is one -- "'_name' is private to 'Class': a name starting with '_' is used
only inside its own class, and 'name' reads it from outside". Without the rule, `described._name = ...` would
undo D88.

**`value.memory` is where a named value lives** (D101, decided by Mortaro; built 2026-09-24, the shape proposed
by Claude, unconfirmed): a class instance, a list or a dictionary answers with its object, a `String` with its
characters (`'constant'` for a literal, which is part of the program), and a number held in a local with the
local itself (`'stack'`) or in an attribute with that attribute (`'heap'`). Only a named value has an address: a
number computed on the spot is "only a named value has memory of its own: give this value a name with 'var'
first". It is built only where a program reads it. A class cannot have an attribute or function of its own named
`memory` (D246, [below](#the-names-reflection-gives-every-object--implemented)). Choosing an object's allocator through `.memory.allocator` (D152, D153) is
[memory.md](memory.md#choosing-an-allocator-memoryallocator)'s.

#### The names reflection gives every object  **[implemented]**

**`class`, `attributes`, `functions`, `instances` and `memory` cannot name an attribute or a function of a class**
(D246, decided by Claude under D205, from the Theseus data conversion; the list proposed by Claude, unconfirmed).
Neither can a `get_` or `set_` function whose member would be one of them (`get_attributes`). Each is a member
reflection gives every object -- `value.class`, `value.attributes`, `value.functions`, `value.memory`, and
`Monster.instances` on the class -- and a class member of the same name was read in its place, silently: a class
with `var attributes = Table.Attributes()` made every `JsonReader` of it read each key as one the class does not
have, because the generated reader writes `value.attributes[attribute]`. The error is

`the attribute 'attributes' has a name reflection gives every object: 'value.attributes' lists its attributes, and
JsonReader, JsonWriter and every attribute walk read through it, and one of its own would hide it.
Name it something else, like 'table_attributes'`

with the suggestion made from the class's own name, and for an accessor `the function 'get_attributes' is read as
'attributes', a name reflection gives every object: ...` (`diagnostics/reflection_names`). The classes of the
`Spite` namespace are exempt: they are reflection, and `Spite.Class.get_attributes()` is how `.attributes` is
answered. Locals and parameters may use the names. A class of the standard library never
declared one (the standard library's allocator attribute is `heap`).

**What `.functions` contains** (proposed by Claude, unconfirmed): the functions a class declares, plus the
Symbol-codegen instances that were actually generated for it -- because those are functions of the class in the
program as built, and `--final-classes` already prints them ([Decided by Mortaro, being implemented](open_questions.md#decided-by-mortaro-being-implemented--planned) item 9). Reflection describes the
program that exists, not the source as written, which is the same rule D42 applies everywhere else.

**A constructor is not one of them.** It does not answer on an instance, it makes one, so putting it in
`.functions` would mean every caller that walks the list has to know to skip it -- and the first thing anyone
writes is a loop that calls what it finds. If a constructor needs to be reachable it belongs on the class object
as its own member, not in the list of what an instance answers.

**Reflection may be as detailed as it likes, because what is not used is not emitted** (D42, decided by
Mortaro, 2026-09-20). Most of this metadata will never be reached by any given program, and reaching for it is
the only thing that makes it cost anything -- "if it has a cost it is because we needed it anyway".

This works here in a way it does not elsewhere. Spite's reflection is resolved at compile time, so which parts
of it a program touches is **statically decidable**, and tree shaking is exact rather than conservative. A
language with runtime reflection cannot prove a class is never reflected on, so it must keep the tables for
everything; Spite can prove it, so it keeps nothing it cannot see a use for. [Reflection objects](#reflection-objects--partial) already works this way --
`value.attributes` is built only for a class that actually reads `.attributes` -- and D42 says that rule covers
the whole family: `Spite.Class`, `Spite.Function`, `Spite.Argument`, `Spite.Attribute`, `Spite.Namespace` and
whatever comes next.

The practical direction for anything designing a reflection member: **err toward more detail.** An unused field costs a program nothing, while a missing one costs a design.

**A namespace is an instance of `Spite.Namespace`** (D41, decided by Mortaro, 2026-09-20), for the same reason
`.class` is a real class: a string can only be matched, an object can be compared, walked and enumerated.
`Spite.Class.namespace` is one of these rather than a dotted `String`, and printing it prints the dotted name,
the way printing a `Spite.Class` prints its `.name`.

It mirrors what already exists. [Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial) builds namespaces out of folders, so `Spite.Namespace` is that tree
made readable: `.parent` walks up it, `.classes` and `.namespaces` walk down. A `load`-ed root therefore
becomes something a program can enumerate -- which is what D13's isomorphic split needs when it partitions a
package's functions, and what the deferred compile-time class generation would extend.

`.namespace` and `.parent` are `Spite.Namespace?`, and the chain ends at `null` (Mortaro, 2026-09-20,
rejecting Claude's proposal of a root object). A root object would have read as though every namespace were
nested inside something -- and inside the metaprogramming package in particular, when a namespace merely *uses*
`Spite.Namespace`, it is not contained by it. A top-level namespace has no parent, so it has `null`, and you
walk up with `class.namespace.parent` for as long as that can be satisfied.

**`.class` is a real `Spite.Class`, not the type's name as a `String`** (D12). This is the change that makes
reflection composable: `function.arguments[0].class == ServerContext` is an identity comparison the compiler
checks, where `.class.name.starts_with("Server")` is a string test that quietly turns false when a class is
renamed. Printing a `Spite.Class` still prints its `.name`, so anything that reads as text keeps reading that
way. `Spite.Attribute.class` and `Spite.Argument.class` are real `Spite.Class` objects too. **[implemented]**

- `value.class` is the value's class: a `Spite.Class` with `.name: Symbol` (the class's own declared name) and
  `.namespace: Spite.Namespace?` (its namespace object -- `.name` is the segment, `.name_with_namespaces` the dotted path,
  `.parent` the chain up, `.classes`/`.namespaces` the tree down; `null` for a global class). Printing a
  `Spite.Class` prints just its `.name`, and printing a `Spite.Namespace` prints its `.name_with_namespaces`.
- **`class` is an inherited attribute of every instance, not a keyword** (decided by Mortaro, 2026-09-22):
  inside any function of a class, a bare `class` answers with that instance's class -- the same `Spite.Class`
  `value.class` gives -- so `class.name` is the class you are writing. A local or an attribute actually named
  `class` takes precedence over the inherited one (that is how `Spite.Attribute.class` reads its own field),
  and the parser's diagnostic is untouched: `class ClassKeyword {` at file scope still fails at 1:1 with
  "there is no 'class' keyword" (`diagnostics/class_keyword`), because a file is a class named after the file.
  **[implemented]**
- **A class name reads its class object member by member** (decided by Mortaro, 2026-09-22): `Weapon.name` is
  `"Weapon"` with no instance anywhere, `Weapon.namespace` answers the same `Spite.Namespace?` as
  `weapon.class.namespace`, and a method receiver written as a class name resolves against the class object --
  `Weapon.is_singleton()` works, while `Weapon.debug()` (a function of the class being described) still reports
  "'Weapon' is a class, not a value: write 'Weapon()' to make one, and give it a name with 'var ... = '", since
  there are no static functions. This is D6 read literally: a class name is an ordinary `Spite.Class` value, so
  every member read and method receiver written as a class name reads the class object, as `Sticker.attributes`
  does; `Weapon.instances` still means its live registry.  **[implemented]**
- **A class name passed as an argument is its class object** (D124, decided by Mortaro, 2026-09-25):
  `entity.remove_component(Ui.Component.Pressed)`, received as `component_class: Spite.Class` -- "we are not
  actually passing a class we are passing a instance of the class class that represents the component class".
  The readings below are **(proposed by Claude, unconfirmed)**: the rule is where a `Spite.Class` (or
  `Spite.Class?`) is wanted, so it covers an argument, `var kind: Spite.Class = Health`, an assignment to such a
  name and a `return` from a function returning one; a codegen value bound to a class (`return $component_type`)
  passes the same way. `Storage<Health>` without parentheses is still read only on the right of `==`. Anywhere else -- `var kind = Health`,
  or an argument whose parameter is a `type` -- a bare class name is still "'Health' is a class, not a value",
  since `Health()` is what was nearly always meant; the message now names the `Spite.Class` form
  (`diagnostics/class_as_value`). This does not meet D77, which is about calls: `f(health)` passes an instance
  (made first, `var health = Health()`, D202) and `f(Health)` its class object, and the parameter's type decides which one compiles. **On the right of `==`
  a class name stays a class test (D75), except when the left side is itself a `Spite.Class`**: then the two
  class objects are compared, so `component.class == component_class` and `kind == Health` both mean "the same
  class" (`conformance/stage6/class_argument`). A class test on a `Spite.Class` value could only ask whether it
  is an instance of `Health`, which a class object never is, and before this it was silently `false`.
- `value.attributes` is a real, runtime `List<Spite.Attribute>` (built fresh, one entry per field, only for a
  class that actually uses `.attributes` -- nothing is generated for a class that never does): each entry has
  `.name: Symbol`, `.class: Spite.Class` (D12) and `.value` (below). It is an ordinary `List<T>`, walked like any
  list (`attributes.each(show_attribute)`), with no special loop form. Each read builds the list again, so its
  run-time cost is one list and one `Spite.Attribute` per field per read. `attributes[symbol]` inside a class is
  a separate, compile-time-only form: that same field indexed by a `Symbol` (see [Symbol
  codegen](metaprogramming.md#symbol-codegen--implemented)).
  A `List<T>`'s or `Dictionary<T>`'s `attributes` are its entries (named by index or by key), not the fields of
  the class that stores them, whichever way it is reached -- a value's `.attributes`, an attribute holding the
  collection, or the class object's `.attributes`. The class object's, which lists what a fresh value holds, is
  therefore empty for a collection. Before, a program that read a class object's `.attributes` and reflected a
  list (every `--repl-port` or `--hot-reload` build of SlopTheseus) was given two C functions of one name for that
  list, and the C compiler refused it (`conformance/stage6/kept_templates`).
- **`attribute.value` is the actual instance the attribute refers to** (D164, decided by Mortaro, replacing the
  text `.value` and the `.object` D123 added; the readings below proposed by Claude, unconfirmed).
  **[implemented]** Its type is `Anything?`, the library's built-in empty `type` (D163), so
  `entity.add_component(attribute.value)` compiles once it is narrowed. **A number, `Boolean`, enum or `Symbol`
  attribute is boxed**, as D109 boxes a plain value passed where a shape is wanted, so its `.class` is `Integer` and
  `if value == Integer` narrows it back; it is `null` only when the attribute holds `null`. **Its text is
  `attribute.value.to_string()`**: through `Anything`, `to_string()` answers each class's own `to_string()` (a
  `String` itself, a number's usual text), and `to_debug()` for a class that has none, a `List` or a
  `Dictionary`; the REPL's display calls it for a number, text or enum. **It costs nothing unless read**: the
  objects are built only in a program whose own code reads `.value` (the REPL library's reads count only in a
  `--repl`/`--repl-port` build), every other program's attribute lists hold `null` there and box nothing, and no
  attribute carries text, so a program walking `.attributes` allocates no strings for it. A number is
  never boxed where its type is known: `x.attributes[attribute]` in a Symbol template reads the plain field.
  A box is freed with the attribute that holds it, also for a class first described late while compiling (a
  `Lock` taken by a described function brings the lists inside `Spite.Class`): letting go of an `Anything` is
  written once every class that can be boxed into it is known (`conformance/stage6/default_handles`).
  **`value.attributes` works on a `type` or union value**, answered from the value's own class at run time, so
  `create_entity_from_bundle(bundle: Anything)` walks whatever bundle it is given. An attribute holding a
  `Dictionary` lists its entries as attributes named by their keys, as a `List` lists its elements, which is
  how the REPL counts both. Two mechanisms came with `.value`: a class admitted to one `type` is admitted to
  every `type` a value of it has already been passed on to, and `.attributes` through a `type` reads every
  attribute of every class admitted to it, for D118 (`conformance/stage6/attribute_object`,
  `conformance/stage6/reflection`).
- `Person.instances` lists every live instance of a class. Conceptually each class is a variable living on the heap and so is
  its registry of instances; the compiler removes whatever is unused: only a class whose `.instances` some code
  reads is tracked, and for it each construction appends to the list and each release removes from it.
  **[implemented]**
  A class object's `.attributes` and `.functions` are read from a stand-in instance at its defaults, made
  without running any constructor (a function of it called through reflection sees those defaults), which is not
  one of `.instances`; a singleton's stand-in never runs its `drop()` and is destroyed with the singletons at exit
  rather than leaked (proposed by Claude, unconfirmed, 2026-09-25; `conformance/stage6/reflection_stand_in`,
  `conformance/stage6/every_class`,
  `conformance/stage6/singleton_stand_in`). The program's entry class has no stand-in, since making one would run
  the program again: its `.functions` is empty and `has_function` asked of it is `false`. `function_waits` asked of
  it answers from the compile-time table all the same: its class object keeps a private list of its functions
  described with no instance (`Spite.Class._unbound_functions`), made only when the program reads `.functions`,
  `has_function` or `function_waits` of some class and uses the entry class's class object, and never callable (proposed by
  Claude, unconfirmed, 2026-09-26; `conformance/stage6/entry_function_waits`). A stand-in of `Concurrent<T>` or
  `Parallel<T>` is finished and joins nothing ([concurrency.md](concurrency.md)).
  A class object and a namespace object are each made once, whichever thread asks first: in a program that starts
  threads, making them takes one lock shared by every class and namespace object (a thread may take it again
  while it makes the objects one refers to), and once made, asking again is one atomic load of a flag. A program
  without threads carries no lock (proposed by Claude, unconfirmed, 2026-09-26;
  `conformance/stage6/threaded_class_objects`).
  A singleton that holds nothing (`Build`, `TypedMemory<T>`) is an ordinary object, one of its `.instances`, only
  in an inspectable build (`--development`, `--hot-reload`, `--repl`, `--repl-port`); every other build makes it one
  static object that `.instances` does not list (D143, [optimizations.md](optimizations.md#singletons-that-hold-nothing-are-static-objects)).
- **`Spite.Class.instances` is every class in the program** (D49, decided by Mortaro, 2026-09-20). A class is an
  instance of `Spite.Class` (D6), so the registry of *its* instances is the whole program's classes -- nothing
  new is needed for whole-program enumeration, it falls out of "everything is a class, including classes". A
  test runner uses it to find every test class instead of being handed them: no registration, no manifest, no
  `run(...)` per class. D42 pays for it, since a program that never enumerates its classes never generates the
  registry. As built it lists the classes of the program's own folder and of every package it loads: not the
  standard library's (`Launcher` included, [programs.md](programs.md)), not the anonymous classes behind object
  literals, and not a generic class before its codegen values are supplied.  **[implemented]**
- **Reflection reads at two levels, and the same word is right at both** (D11, decided by Mortaro, 2026-09-19).
  A class object is an instance of `Spite.Class` (see [Functions of
  `Spite.Class`](#functions-of-spiteclass-and-why-there-are-no-static-functions--partial)), so it has attributes of its
  own -- the declarations -- while an instance has their values:

```
weapon.attributes['damage']    # the value held in that field
Weapon.attributes['damage']    # the Spite.Attribute that describes the field
```

  A PascalCase receiver is the class and a lowercase one is an instance, which is the same convention [Foreign libraries](foreign_libraries.md#foreign-libraries--partial)
  uses for `user32.Input` versus `user32.input_mouse`, so nothing new is needed to tell them apart. This is also
  what stops `attributes[symbol]` from being a special, compile-time-only form bolted onto Symbol codegen: it is
  ordinary indexing of the attribute mapping, at whichever level the receiver names. **[planned: the exact shape
  of the class object's mapping -- keyed by symbol, and what it answers for a field the instance has not set --
  still needs design; see PLAN.md milestone 10.]**

```gdscript
func describe(person: Person) {
    var attributes = person.attributes
    attributes.each(show_attribute)
}

func show_attribute(attribute: Spite.Attribute) {
    console.print(attribute.name, attribute.class)
}
```

<a id="class-level-functions-and-why-there-are-no-static-functions--implemented"></a>

#### Functions of `Spite.Class`, and why there are no static functions  **[partial]**

D6 (decided by Mortaro, 2026-09-19): **Spite has no static class functions and will not get any.** A class is an
instance of `Spite.Class`, and `Spite.Class` is an ordinary standard library class with an ordinary declaration.
So there is nothing for `static` to mean: a function that belongs to the class rather than to its instances is
just a function on that `Spite.Class` object, and `Spite.Class` is where it is declared, with its default.
`library/spite/class.spite` declares `is_singleton()` that way:

```gdscript
var _singleton = false

func is_singleton(): Boolean {
    return _singleton
}
```

A class file may **override** one of those functions for its own class object, the same way any class reopens
another ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)).
`is_singleton()` was the first such override (`console.spite` returning `true`); since D104 a singleton says so
with its `singleton` header line ([classes_and_files.md](classes_and_files.md#singletons)), and declaring
`is_singleton()` in a class is "a class says it is a singleton with a 'singleton' line at the top of its file, not
with a function: delete 'is_singleton()' and write 'singleton' as the file's first line". `is_singleton()` stays
the member `Spite.Class` answers.

- **The functions a class file can override are exactly the functions `Spite.Class` declares.** There is no
  keyword and no marker: if the name is one of those, the definition belongs to the class object; otherwise it
  is an ordinary instance function, as every function in a file has always been. Reading `Spite.Class` in the
  standard library is how you learn the full list.
- A class wanting an ordinary instance function whose name collides with one of them is a diagnostic naming
  `Spite.Class`. The list is deliberately short.
- The override is **evaluated at compile time** and must fold to a constant. A `return` of a literal always
  folds; a `return` of a codegen value (`return $shared`) folds too, so a fact about a class can differ per
  build without any new syntax. Anything the compile-time evaluator cannot fold is a diagnostic.
- `--final-classes` prints each class's overrides with the value they folded to; a singleton's is its
  `singleton` line.
- **[not built: the three rules above]** A function a class file declares is always an instance function: a
  class's own `has_function(name)` answers on its instances, with no diagnostic, while `Gadget.has_function(...)`
  still answers `Spite.Class`'s; and a class's `to_string()`, which `Spite.Class` also declares, is how its
  instances print. With `is_singleton()` now the `singleton` line, no class file overrides a function of
  `Spite.Class` today.
- **`Spite.Class` is reopenable, like any other standard library class** (D7, decided by Mortaro, 2026-09-19:
  "by all means shoot the foot"). Reopening it changes a default of every class object for the **whole
  program**: a root whose `spite/class.spite` returns `true` from `is_singleton()` makes every class in the
  program a singleton, `List<Integer>()` included. Adding a *new* function to `Spite.Class` is louder still -- it
  gives every class object a new function program-wide, so any class that already had an ordinary instance
  function by that name becomes an override of it.
  Nothing about this is silent, which is the reason it is allowed: a name that collides is a compile error
  naming `Spite.Class` (above), and a changed default shows up per class in `--final-classes`, with the root it
  came from, exactly as any other reopened function does. The compiler has no warnings ([Style](style.md#style--implemented)), so
  visible generated output is the whole mitigation. **[partial: reopening `Spite.Class` works -- a program's
  `spite/class.spite` adds and replaces its functions ([packages.md](packages.md#the-spite-namespace-is-reserved))
  -- but it changes only what the class objects answer: `is_singleton()` returning `true` does not make
  `List<Integer>()` a singleton, since the `singleton` line decides that. A replacement that no longer reads
  `_singleton` is "the attribute '_singleton' is never read: remove it" in `Spite.Class`, a private field of the
  library the program cannot remove; read it (`return _singleton or true`). `--final-classes` names no root yet.]**

**Why a function and not a declaration** (Mortaro, 2026-09-19): it is the same philosophy as a configuration
file being a class that gets reopened -- like Rails patching its internal options through a block that can run
code, rather than through a static configuration file. A static line can only state a value; a function can
compute one, and it costs the language nothing because functions already exist.
