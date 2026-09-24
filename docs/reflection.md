# Reflection

Everything is a class, including classes. A class is an instance of `Spite.Class`; a function is an instance of
`Spite.Function`; a namespace, an attribute and an argument are instances of `Spite.Namespace`,
`Spite.Attribute` and `Spite.Argument`. They are ordinary classes in `library/spite/`, so reflection is not a
second language beside Spite: it is Spite describing itself, reachable by name at run time and from the REPL.

Reflection is resolved at compile time wherever it can be, and **what a program does not read is not
emitted**: `value.attributes` is built only for a class something reads `.attributes` of, and the table of every
class exists only in a program that asks for it. So reflection can be as detailed as it likes -- an unused member
costs nothing.

## The objects

| Object | Members |
|---|---|
| `Spite.Class` | `.name: Symbol`, `.namespace: Spite.Namespace?`, `.attributes`, `.functions`, `.instances`, `is_singleton()` |
| `Spite.Function` | `.name`, `.arguments: List<Spite.Argument>`, `.returns: Spite.Class` (`Nothing` when none is declared), `call_function()` |
| `Spite.Argument` | `.name`, `.class: Spite.Class` |
| `Spite.Attribute` | `.name`, `.class: Spite.Class`, `.value: String` |
| `Spite.Namespace` | `.name` (the segment), `.name_with_namespaces` (dotted), `.parent: Spite.Namespace?`, `.classes`, `.namespaces` |
| `Spite.Memory` | `.address: Long`, `.bytes: Long`, `.section` (`'heap'`, `'stack'`, `'constant'`) -- see [memory.md](memory.md#where-a-value-lives-memory) |

A name is a `Symbol`: text from the program's own table of symbols, which reads as text anywhere text is
expected (every `String` function answers on it) and costs no allocation to pass around.

## `.class` and `.attributes`

`value.class` is a `Spite.Class`, and printing one prints its `.name`. `value.attributes` is a real, run-time
`List<Spite.Attribute>`, one entry per field, each with its `.name`, its `.class`, and its `.value` rendered as
text (`""` for a field with no plain textual form, such as a class or a list):

```gdscript title=reflection_basics/gadget.spite
var name = ""
var power = 0

func Gadget(new_name: String, new_power: Int) {
    name = new_name
    power = new_power
}
```
```gdscript title=reflection_basics/reflection_basics.spite entry
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

- **`class` is not a keyword.** Inside any function of a class it is the class of the instance that function
  answers on -- an attribute every instance inherits -- so `own class` above names the class the function was
  written in. A local or attribute actually named `class` takes precedence.
- **A class name reads its own class object**, member by member, with no instance anywhere: `Gadget.name` is
  `"Gadget"`, and `Gadget.namespace` is the same namespace `gadget.class.namespace` gives. A class name is not a
  value you can call a function of the class on, though: there are no static functions, so `Gadget.boost()` is
  an error saying to make a `Gadget()` first.
- **`.class` through a shape answers the real class.** Read through a `type` or a union, `.class` comes from the
  object's own tag at run time, so it names the class the value really is; an object literal answers `Object`.
- `value.attributes` holds the values; `Gadget.attributes` describes the declarations. The same word is right at
  both levels, and the case of the receiver says which one you mean.

## Namespaces

A class in a folder has a namespace ([packages.md](packages.md)); a class at the root of its program has none.
`.namespace` is a `Spite.Namespace?` -- narrow it before reading its members -- and printing a namespace prints its
dotted `.name_with_namespaces`. `.parent` walks up the tree and `.classes` and `.namespaces` walk down it.
Comparing needs no narrowing: a namespace compares with text through its `name_with_namespaces`, and a class
with no namespace simply compares unequal.

```gdscript title=namespace_walk/shop/tools/hammer.spite
var weight = 2
```
```gdscript title=namespace_walk/namespace_walk.spite entry
var console = Console()

func NamespaceWalk() {
    describe(Shop.Tools.Hammer())
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
var name = ""
var power = 0

func Gadget(new_name: String) {
    name = new_name
}

func boost(amount: Int, times: Int): Int {
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
    functions.each_describe()
    var library_names = Memory.functions.map_name()
    var has_allocate = library_names.contains('allocate_bytes')
    console.print("Memory has allocate_bytes", has_allocate)
}

func describe(function: Spite.Function) {
    var arguments = function.arguments
    var argument_names = arguments.map_name()
    var joined_names = argument_names.join(", ")
    console.print(function.name, "({joined_names})", function.returns)
}
```
```output
boost (amount, times) Int
reset () Nothing
Memory has allocate_bytes true
```

A class of the standard library describes itself the same way: `Memory.functions` lists `allocate_bytes`,
`resize`, `free` and the rest, including the functions whose bodies the compiler supplies.

## Instances, and every class in the program

`Monster.instances` is every instance of `Monster` alive at that moment, in the order they were made.
`Spite.Class.instances` is every class in the program, because a class is an instance of `Spite.Class`. That
is how the test package finds its tests without being told about them ([testing.md](testing.md)).

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
    kept.append(Monster("rat"))
    kept.append(Monster("bat"))
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

The few members only the compiler can write -- `value_attributes()`, `value_functions()` and `assign(text)` on
`Spite.Attribute`, `call_function()` and `call_with_text(arguments)` on `Spite.Function`, which reach the live
value behind them for the REPL -- are the compiler's own reopening of those classes, and `--final_classes`
prints them like any other member ([compiler.md](compiler.md#inspect-merged-classes)).

## Class-level functions, and no static functions

There are no static functions and there will not be any: a class is an instance of `Spite.Class`, so a function
that belongs to the class rather than to its instances is a function of `Spite.Class`, declared there with its
default. `is_singleton()` is one: every class answers it, from its `singleton` line
([classes_and_files.md](classes_and_files.md#singletons)). Reopening `Spite.Class` changes a class-level
default for the whole program -- a foot you are allowed to shoot, and one that shows in `--final_classes`.
