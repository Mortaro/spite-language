# Reflection

**Every class is an instance of `Spite.Class`.** A class is not a special thing beside the values of a program: it
is an ordinary object, made from an ordinary class in `library/spite/`, and writing its name reads that object.
Every namespace is an instance of `Spite.Namespace`, every function an instance of `Spite.Function`, and every
attribute and argument an instance of `Spite.Attribute` and `Spite.Argument`. Reflection is not a second language
beside Spite: it is Spite describing itself, with the same lists, the same member templates and the same `[]` the
moron already uses everywhere else.

```gdscript title=every_class_an_object/monster.spite
var name = "troll"
var health = 10

func alive(): Boolean {
    return health > 0
}
```
```gdscript title=every_class_an_object/every_class_an_object.spite entry
var console = Console()

func EveryClassAnObject() {
    var kind: Spite.Class = Monster
    console.print("a class is a value:", kind)
    Monster.attributes.each(describe)
    var alive = Monster.functions['alive']
    if alive {
        console.print("function", alive.name, "returns", alive.returns)
    }
    if not Monster.functions['fly'] {
        console.print("no function fly")
    }
    Monster.source_files.each(show_file)
}

func describe(attribute: Spite.Attribute) {
    console.print("attribute", attribute.name, attribute.class)
}

func show_file(file: File) {
    var from_monster = file.path.ends_with("monster.spite")
    console.print("declared in monster.spite:", from_monster)
}
```
```output
a class is a value: Monster
attribute name String
attribute health Integer
function alive returns Boolean
no function fly
declared in monster.spite: true
```

`Monster` read as a value is its `Spite.Class`. `.attributes` and `.functions` are ordinary lists, so `each` walks
them and `[]` with a name answers the member or `null`. Every member of a reflection object is read as an
attribute, with no `()`, and none of them can be assigned. A list member template works on them like on any list,
so the names of every attribute are one call:

```gdscript
var names = Monster.attributes.map_names()
```

None of this costs anything when the program runs. `Monster` is named in the code, so the compiler knows the
object, everything read from it, and every element of its lists: `each` above is two calls to `describe` written
out while compiling, each compiled for its own attribute, and no list of attributes exists in the program
([Known while compiling](#known-while-compiling)). The same expressions typed at a REPL prompt are answered from
tables only a REPL build carries ([Known only at run time](#known-only-at-run-time)).

## A class and an instance of it

Both a class and an instance have `.attributes` and `.functions`, and they answer different things. The case of the
receiver says which one is meant: `Monster` is the class, `troll` an instance.

| | The class, `Monster` | An instance, `troll` |
|---|---|---|
| `.attributes` | information: a `Spite.AttributeDeclaration` for each attribute, with its `.name`, `.class`, `.index`, and no value | data: a `Spite.Attribute` **bound to `troll`**, with everything the declaration has plus `.value`, read and written, typed as the attribute, and `.owner`, which is `troll` |
| `.functions` | information: a `Spite.FunctionDeclaration` for each function, with its `.name`, `.arguments`, `.returns` and the class that declares it | a `Spite.Function` bound to `troll`, with everything the declaration has plus `.owner`: a function value you can pass on, call, or build a `Spite.Call` from |
| `.class` | `Spite.Class`, the class every class is an instance of | `Monster`, so `troll.class == Monster` |
| What a read costs | nothing: it is known while compiling and folds | a read or write of `troll`'s own field, typed, the same as `troll.health` |

```gdscript title=class_and_instance/monster.spite
var name = "troll"
var health = 10

func alive(): Boolean {
    return health > 0
}

func hurt(amount: Integer) {
    health = health - amount
}
```
```gdscript title=class_and_instance/class_and_instance.spite entry
var console = Console()

func ClassAndInstance() {
    Monster.attributes.each(describe)
    var troll = Monster()
    troll.hurt(3)
    troll.attributes.each(show)
    troll.attributes.each(heal)
    console.print("health is now", troll.health)
    console.print("troll is a Monster:", troll.class == Monster)
    var bound = troll.functions['alive']
    if bound {
        console.print("bound to troll:", bound.name)
    }
}

func describe(attribute: Spite.Attribute) {
    console.print("declared", attribute.index, attribute.name, attribute.class)
}

func show(attribute: Spite.Attribute) {
    console.print("troll's", attribute.name, "=", attribute.value)
}

func heal(attribute: Spite.Attribute) {
    if attribute.class == Integer {
        attribute.value = attribute.value + 5
    }
}
```
```output
declared 0 name String
declared 1 health Integer
troll's name = troll
troll's health = 7
health is now 12
troll is a Monster: true
bound to troll: alive
```

In `heal`, `attribute.value` is an `Integer` in the copy compiled for `health`, so `+ 5` is ordinary arithmetic, and
the copy compiled for `name` has no `if` body at all. A declaration has no value to read, so `.value` on one of
`Monster.attributes` is an error that names the instance form:

```gdscript title=declaration_value/monster.spite
var health = 10
```
```gdscript title=declaration_value/declaration_value.spite entry error
var console = Console()

func DeclarationValue() {
    Monster.attributes.each(peek)
}

func peek(attribute: Spite.Attribute) {
    console.print(attribute.value)
}
```
```diagnostic
'attribute.value' reads an attribute of an instance, and 'attribute' describes a declaration
```

A declaration still reaches one instance's field: `troll.attributes[attribute]` indexes the instance with it, which
is how a function handed a class's attribute reads or writes it on a value it was given.

### A call, built before it runs

Calling a function value calls it, as always: `greet("Ann")`, `tick()`. To fill a call's arguments one at a time,
by name, before running it, make a `Spite.Call` from the function's declaration and the instance to run it on:

```gdscript
var call = Spite.Call(MoveSystem.functions['run_each'], system)
call.arguments['position'] = position
call.arguments['velocity'] = velocity
call.call()
```

Nothing runs until `call.call()`. `system` must be a `MoveSystem`, or the line is a compile error. This is the only
way to build a call; a bound function, `system.functions['run_each']`, is for passing the function as a value and
calling it. When the function and the arguments are known while compiling, as in a walk, the
whole thing is the plain call `system.run_each(position, velocity)`, with no object made. An argument left unfilled
is a compile error where the compiler can see it; otherwise `call.call()` halts and names it.

## Reaching a reflection object

| Written | Answers |
|---|---|
| a class name, `Monster`, `Shop.Tools.Hammer` | its `Spite.Class` |
| a namespace name, `Shop.Tools` | its `Spite.Namespace` |
| an enum name, `Phase` | its `Spite.Class`, whose `.values` are its values in order |
| `class` inside a function | the class the function answers on |
| `namespace` inside a function | that class's namespace, or `null` at the root of a program |
| `$T` in a generic class | the class it was given ([metaprogramming.md](metaprogramming.md#asking-what-a-generic-was-given)) |
| `value.class` | the class `value` really is, through a `type` or a union too |
| `Spite.Class.instances` | every class of the program |
| `Spite.Namespace.instances` | every namespace of the program |

`class` and `namespace` are not keywords: they are members every instance has, so a local of that name takes
precedence inside its function. A class and a namespace with the same dotted name is a compile error, so a name
always means one thing.

A class name given where a `Spite.Class` is wanted (an argument, a `var` declared `Spite.Class`, a `return`) is
its class object, and `==` between a `Spite.Class` and a class name compares the two objects. Anywhere else a bare
class name is the error that says to write `Health()`, since making one is nearly always what was meant, and
`var kind = Health` with no type is that error too:

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

A class name is not a receiver for the class's own functions: there are no static functions, so `Gadget.boost()`
is an error saying to make a `Gadget()` first ([Functions of `Spite.Class`](#functions-of-spiteclass-and-no-static-functions)).

A class in a folder has a namespace ([packages.md](packages.md)); a class at the root of its program has none.
`.namespace` is a `Spite.Namespace?`, narrowed before its members are read, and printing a namespace prints its
dotted name. Comparing needs no narrowing: a namespace compares with text through its dotted name, and a class
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

## The `Spite` classes

Each class below is ordinary Spite in `library/spite/`. Every member is a get-only attribute named for what it
answers: a question is `is_...` and answers `Boolean`, a collection is a `List` named in the plural. A question
that would take an argument is a collection indexed or filtered instead, so there is one way to ask it.

### `Spite.Class`

| Member | Answers | Example |
|---|---|---|
| `.name` | the class's own name, a `Symbol` | `Monster.name` is `Monster` |
| `.namespace` | its `Spite.Namespace?`, `null` at a program's root | `Shop.Tools.Hammer.namespace` is `Shop.Tools` |
| `.attributes` | a `List<Spite.Attribute>`, in declaration order | `Monster.attributes['health']` |
| `.functions` | a `List<Spite.Function>`, in the order they were made | `Monster.functions['alive']` |
| `.instances` | every live instance, in the order they were made | `Monster.instances.count()` |
| `.values` | an enum's values, in order | `Phase.values[1]` is `'update'` |
| `.is_singleton` | the class has a `singleton` line | `Console.is_singleton` is `true` |
| `.is_stateful` | a function besides the constructor and `drop()` changes the object, or a singleton it binds does | `Counter.is_stateful` |
| `.is_fixed_size` | its size is known while compiling, so a `Vector` can hold it inline | `Vector3.is_fixed_size` is `true` |
| `.is_list`, `.is_dictionary`, `.is_optional`, `.is_enum` | the kind of a type | `$value_type.is_list` |
| `.element_type` | a list's element, or what an optional holds | `$list_type.element_type` |
| `.value_type` | a dictionary's value | `$map_type.value_type` |
| `.source_files` | a `List<File>`: every file that declares or reopens the class, in load order | `Monster.source_files.first()` |

A class has no folder of its own: a reopening may live anywhere a program loads, so `.source_files` is the whole
list, and a question about a path is asked of those `File`s.

### `Spite.Namespace`

| Member | Answers | Example |
|---|---|---|
| `.name` | the last segment | `Shop.Tools.name` is `Tools` |
| `.name_with_namespaces` | the dotted name | `Shop.Tools.name_with_namespaces` is `Shop.Tools` |
| `.parent` | the namespace above, a `Spite.Namespace?` | `Shop.Tools.parent` is `Shop` |
| `.classes` | the classes declared directly in it | `Shop.Tools.classes.count()` |
| `.namespaces` | the namespaces directly below it | `Shop.namespaces` |
| `.every_class` | its classes and those of every namespace below it | `Shop.every_class` |
| `.enums` | the enums declared in it | `Shop.enums` |
| `.source_directories` | a `List<Directory>`: every directory its classes come from, in load order | `Shop.source_directories.first()` |

There is no root namespace object: a namespace at the top has `null` as its `.parent`.

### `Spite.Function`

| Member | Answers | Example |
|---|---|---|
| `.name` | the function's name | `alive.name` is `alive` |
| `.arguments` | a `List<Spite.Argument>`, in order | `hurt.arguments[0].name` is `amount` |
| `.returns` | a `Spite.Class`, `Nothing` when none is declared | `alive.returns` is `Boolean` |
| `.owner` | the class it belongs to | `alive.owner` is `Monster` |
| `.is_resumable` | it can reach a wait (a sleep, a file or socket read), so it is also compiled as a state machine that can pause there and be resumed ([concurrency.md](concurrency.md)) | `load_each.is_resumable` |
| `.returned_literal` | the text literal it returns | `greeting.returned_literal` is `grr` |
| `.accesses` | a `Dictionary<Spite.Access>` keyed by name: every attribute and argument it reads or writes | `update_each.accesses.filter_written()` |

A class's `.functions` holds `Spite.FunctionDeclaration`s, which have every member above but `.owner`'s instance:
their `.owner` is the class that declares them. An instance's `.functions` holds `Spite.Function`s bound to it, and
`call_function()` calls one. A call built argument by argument is a `Spite.Call` of a declaration and an instance
([above](#a-call-built-before-it-runs)).

### `Spite.Call`

| Member | Answers | Example |
|---|---|---|
| `Spite.Call(declaration, instance)` | a call of a `Spite.FunctionDeclaration` on an instance of its class, nothing run yet | `Spite.Call(MoveSystem.functions['run_each'], system)` |
| `.arguments` | its arguments by name, each written once before the call runs | `call.arguments['position'] = position` |
| `call()` | runs it, answering what the function answers | `call.call()` |

### `Spite.Argument`

| Member | Answers | Example |
|---|---|---|
| `.name` | the argument's name | `hurt.arguments[0].name` is `amount` |
| `.class` | its declared class | `hurt.arguments[0].class` is `Integer` |
| `.index` | its place, 0 for the first | `hurt.arguments[1].index` is `1` |
| `.is_mutated` | the function changes the object passed there, or anything reached through it | `save_each.arguments[0].is_mutated` |
| `.function` | the function it belongs to | `argument.function.name` |

### `Spite.Attribute`

| Member | Answers | Example |
|---|---|---|
| `.name` | the attribute's name | `attribute.name` is `health` |
| `.class` | its declared class | `attribute.class` is `Integer` |
| `.index` | its place in declaration order, 0 for the first | `Monster.attributes['health'].index` is `1` |
| `.owner` | the class that declares it | `attribute.owner` is `Monster` |
| `.camel_case_name`, `.pascal_case_name` | its name as other systems spell it | `buy_price` is `buyPrice`, `BuyPrice` |
| `.is_singleton` | its class is a singleton | `ReflectionObjects.attributes['console'].is_singleton` |
| `.value` | on an instance's attribute only: that instance's value, read and written | `troll.attributes['health'].value` |

A class's `.attributes` holds `Spite.AttributeDeclaration`s: every member above but `.value`, with `.owner` the
class. An instance's holds `Spite.Attribute`s bound to it, whose `.owner` is the instance.

### `Spite.Access`

What a function does with one attribute or argument, the value of `function.accesses`:

| Member | Answers | Example |
|---|---|---|
| `.is_read` | the function reads it | `access.is_read` |
| `.is_written` | the function writes it | `access.is_written` |
| `.target` | the `Spite.Attribute or Spite.Argument` it is about | `access.target.name` |

An attribute and an argument of one function cannot share a name, so one dictionary holds both. Its attributes come
first, in declaration order, then its arguments, in order. Both answers follow every call to a function of the same
class, however deep, so `remember()` below writes `trail` for `update_each`; an attribute or argument the function
neither reads nor writes has no entry. Calling a function on an attribute (`trail.append(position)`) reads it, and
writes it when that function changes the object it is called on.

```gdscript title=access_report/movement.spite
var position = 0
var speed = 1
var trail = List<Integer>()

func update_each(steps: Integer) {
    position = position + speed * steps
    remember()
}

func remember() {
    trail.append(position)
}
```
```gdscript title=access_report/access_report.spite entry
var console = Console()

func AccessReport() {
    var update = Movement.functions['update_each']
    crash update
    update.accesses.each(describe)
    var written = update.accesses.filter_written()
    var written_count = written.count()
    console.print("written:", written_count)
    var movement = Movement()
    movement.update_each(2)
    console.print(movement.position)
}

func describe(access: Spite.Access) {
    console.print(access.target.name, "read", access.is_read, "written", access.is_written)
}
```
```output
position read true written true
speed read true written false
trail read true written true
steps read true written false
written: 2
2
```

A function known only at run time answers the same dictionary: one kept in a list and read later, a function value
like `movement.remember`, or one read from `value.class.functions` through `Anything`. The program carries a table
of what each function reads and writes only when some code asks `.accesses` of such a function, and builds each
dictionary the first time it is asked. The table covers the functions of the program and the packages it loads;
a function of the standard library, a function value taken through a `type` (which has no single function behind
it) and any function in a `--hot-reload` build, where reloading would make the table stale, are not in it, and
asking their `.accesses` at run time halts.

### `Spite.Memory`

`value.memory` is where a named value lives: `.address: Long`, `.bytes: Long` and `.section` (`'heap'`,
`'stack'` or `'constant'`). It is [memory.md](memory.md#where-a-value-lives-memory)'s.

## Members are lists

Every collection above is an ordinary `List` (and `.accesses` an ordinary `Dictionary`), so everything a list has
works on it: `[]` with a name answers the member or `null`, and `count()`, `each`, `filter` and every member
template do what they do on any list.

```gdscript title=constant_selection/runner.spite
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
```gdscript title=constant_selection/constant_selection.spite entry
var console = Console()

func ConstantSelection() {
    var update = Runner.functions['update_each']
    if update {
        console.print(update.name, "changes its label:", update.arguments[1].is_mutated)
    }
    if not Runner.functions['draw_each'] {
        console.print("no draw_each")
    }
    var walked = Runner.functions.filter_name_ends_with("_each")
    walked.each(describe)
}

func describe(function: Spite.Function) {
    var count = function.arguments.count()
    console.print(function.name, "takes", count)
    function.arguments.each(describe_argument)
}

func describe_argument(argument: Spite.Argument) {
    var fresh = made(argument)
    console.print(" ", argument.index, argument.name, argument.class, "'{fresh}'")
}

func made(argument: Spite.Argument): argument.class {
    return argument.class()
}
```
```output
update_each changes its label: false
no draw_each
update_each takes 2
  0 amount Integer '0'
  1 label String ''
render_each takes 1
  0 scale Float '0'
```

`filter_name_ends_with("_each")` keeps the functions whose `name` answers `ends_with("_each")`: a member template
chains through a function of the member, and nothing in it is special to reflection
([collections.md](collections.md)). A name is selected, never built: `[]` takes a name written in the code, and
anything else is a filter over the names that exist. `made` answers `argument.class`, a different class for each
argument, which is fine because it is compiled once for each.

A member template that collects the members' values takes the member in the plural, and a dictionary's member
templates work over its values:

```gdscript
var names = Monster.attributes.map_names()
var counted = Shop.every_class.count_is_singleton()
var written = update_each.accesses.filter_written()
```

## Known while compiling

Most reflection is asked of something the compiler can already identify: a class, namespace or enum named in the
code, `$T`, `class`, `namespace`, and everything read from one of them. Such an object is a **constant**, and
reflection on it is ordinary code that costs nothing at run time:

- **A question folds.** `Monster.is_singleton`, `update.is_resumable` or `argument.is_mutated` is a `true` or a
  `false` in the generated code, and an `if` on it keeps only the branch taken.
- **`each` over a constant list is unrolled.** `Monster.attributes.each(show)` calls `show` once for `name` and
  once for `health`; no list is made. A filter or a member template over a constant list is a constant list.
- **A function handed a constant is compiled once for it.** Inside that copy the parameter is the constant
  itself, so `attribute.class` is a type, `attribute.name` is text known while compiling, and
  `monster.attributes[attribute]` is a plain read of the field.
- **A constant class is a type and makes its own instances**: `part()` makes one, `Column<part>()` a column of
  them.

```gdscript title=constant_namespaces/part/wheel.spite
var size = 3
```
```gdscript title=constant_namespaces/part/axle.spite
var length = 7
```
```gdscript title=constant_namespaces/phase.spite
enum Phase {
    'start'
    'update'
    'finish'
}
```
```gdscript title=constant_namespaces/constant_namespaces.spite entry
var console = Console()

func ConstantNamespaces() {
    var parts = Spite.Namespace.instances.filter(holds_parts)
    parts.each(make_every_class)
    var count = Phase.values.count()
    var last = Phase.values.find_by_name('finish')
    console.print(count, "phases, the last is", last)
}

func holds_parts(namespace: Spite.Namespace): Boolean {
    return namespace.name == 'Part'
}

func make_every_class(namespace: Spite.Namespace) {
    namespace.classes.each(make)
}

func make(part: Spite.Class) {
    var fresh = part()
    var shown = fresh.to_debug()
    console.print(part.name, shown)
}
```
```output
Axle Axle { length: 7 }
Wheel Wheel { size: 3 }
3 phases, the last is finish
```

`make` is compiled twice, as `make_for_axle` and `make_for_wheel` in `--final-classes`: `part()` is
`Part.Axle()` in one copy and `Part.Wheel()` in the other. Every folder named `part` anywhere is found by this
filter over every namespace, with nothing registered.

## Known only at run time

A value typed `Anything` or a union, a reflection object kept in an attribute and read later, and everything typed
at the REPL prompt are only known at run time. Reflection still answers them, from tables the program builds only
for the classes such a read can reach, and there `attribute.value` is an `Anything?`: the value itself (a number
keeps its class, held with its class's tag, and text is boxed), `null` only when the attribute holds `null`. Its text is `.value.to_string()`.

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

`Anything` is the library's empty `type`: every class fits it, so a function that takes any object says
`component: Anything`. What needs a constant says so while compiling: a function whose result is typed by its
reflection parameter cannot be handed an object known only at run time.

```gdscript title=run_time_type/gadget.spite
var power = 3
```
```gdscript title=run_time_type/run_time_type.spite entry error
var console = Console()

func RunTimeType() {
    var gadget = Gadget()
    var anything: Anything = gadget
    var attributes = anything.attributes
    var first = attributes.first()
    crash first
    var made = fresh(first)
    console.print(made)
}

func fresh(attribute: Spite.Attribute): attribute.class {
    return attribute.class()
}
```
```diagnostic
'fresh' answers a type read from a reflection object, so it is compiled once for each object the compiler can identify, and this call hands it one known only at run time: pass a class, attribute, function or argument named in the code, or walked from one
```

One function body serves both: called with a constant it is typed and unrolled, and called from the REPL with a
live object it takes the run-time path. A `--repl`, `--repl-port` or `--development` build keeps the tables for
every class, so the prompt can ask anything the program can:

```gdscript
> Monster.attributes.map_names()
name, health
> Monster.functions['alive'].returns
Boolean
> Monster.instances.count()
2
> Monster.instances.first().attributes['health'].value = 40
```

## Instances, and every class in the program

`Monster.instances` is every instance of `Monster` alive at that moment, in the order they were made.
`Spite.Class.instances` is every class of the program, its own folder and every package it loads but not the
standard library, because a class is an instance of `Spite.Class`. That is how the test package finds its tests
without being told about them ([testing.md](testing.md)).

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
    var alive_count = Monster.instances.count()
    console.print("alive:", alive_count)
    var classes = Spite.Class.instances
    crash classes['Monster']
    console.print("the program has", classes['Monster'])
}

func spawn_and_forget() {
    var ghost = Monster("ghost")
    var instances_count = Monster.instances.count()
    console.print("while the ghost is alive:", instances_count, ghost.name)
}
```
```output
while the ghost is alive: 3 ghost
alive: 2
the program has Monster
```

A class nobody asks the instances of keeps no list; one that is asked adds every instance when it is made and
removes it when it is released. Describing a class makes none: a class object's `.attributes` and `.functions` are
read from a stand-in at its defaults, made without running any constructor, which is not one of `.instances`. A
singleton's stand-in is not the singleton, so describing `Console` neither makes the program's console nor keeps a
second one alive. The program's entry class has no stand-in, since making one would run the program again, so its
`.functions` is empty.

Which singletons are objects at all depends on the build: in a `--development`, `--hot-reload`, `--repl` or
`--repl-port` build a singleton that holds nothing, such as `Build`, is an ordinary object you can find in its
`.instances`; in every other build it is one static object that `.instances` does not list
([optimizations.md](optimizations.md#singletons-that-hold-nothing-are-static-objects)).

## The names reflection gives every object

`class`, `attributes`, `functions`, `instances` and `memory` belong to reflection on every class, so a class
cannot declare an attribute or a function of its own by those names, nor a `get_` or `set_` function that would be
read as one. One named `attributes` would otherwise hide the real list from everything that walks a class:
`JsonReader` would read every key as a key the class does not have. The error names what the member means and a
name to use instead:

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

## Reflection is read-only

Every member describing the program is a getter with no setter: `library/spite/class.spite` keeps its data in
private fields (`_name`, `_namespace`, `_attributes`, ...) and answers `get_name()`, `get_namespace()` and the rest,
so `.name` reads through the ordinary getter and writing it is an error. A name starting with `_` is used only
inside its own class, so the private fields cannot be written from outside either. The one thing reflection
writes is an instance's own value, through a bound attribute's `.value`.

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

## Functions of `Spite.Class`, and no static functions

There are no static functions: a class is an instance of `Spite.Class`, so a member that belongs to the class rather
than to its instances is an ordinary member of `Spite.Class`, declared there with its default. `.is_singleton` is
one: every class answers it, from its `singleton` line ([classes_and_files.md](classes_and_files.md#singletons)),
and declaring it in a class is an error that says to write the `singleton` line instead. A reopening of
`Spite.Class` changes what every class object answers, for the whole program. It is a foot you are allowed to
shoot, and one that shows in `--final-classes`
([the rules in full](#functions-of-spiteclass-and-why-there-are-no-static-functions)).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge cases and
the exact error texts. Where the teaching above and these rules disagree, the rules win.

### Reflection objects

**Every class is an instance of `Spite.Class`**, every namespace of `Spite.Namespace`, every function of
`Spite.Function`, and every attribute, argument and access of `Spite.Attribute`, `Spite.Argument` and
`Spite.Access`. They live in the `Spite` namespace (`Spite.Class`, not `SpiteClass`), so nothing in it can be
mistaken for a class of a program. `Spite` is a reserved root namespace: a program's `spite/` folder may only
reopen its classes, and `load "spite"` is an error ([packages.md](packages.md#the-spite-namespace-is-reserved)).
Their members are the tables of [The `Spite` classes](#the-spite-classes).

**Every member is a get-only attribute named for what it answers**, read without `()`: a question is `is_...`, a
collection is plural. A question that takes something is a collection indexed or filtered instead
(`Monster.functions['alive']`, `hurt.arguments[1].is_mutated`). What acts rather than answers stays a function:
`call_function()`, a `Spite.Call`'s `call()`, and a list's own `count()`. A name is a `Symbol`: text from the program's own
table of symbols, which reads as text anywhere text is expected and costs no allocation to pass around.

**Every member is a getter with no setter**: the data sits in private fields and `get_name()` and the rest answer
it, so `described.name = ...` is "'name' is read-only". **A name starting with `_` is private** (everywhere but
on a parameter, where `_` means unused on purpose; [Lexical structure](classes_and_files.md#lexical-structure)): it
is read, written or called only inside its own class (a reopening is inside), and from anywhere else it is an error
naming the getter when there is one: "'_name' is private to 'Class': a name starting with '_' is used only inside
its own class, and 'name' reads it from outside". The members only the compiler can write (`call_function()`, the
value of a bound attribute) are the compiler's reopening of those classes, printed by `--final-classes` like any
member.

**What `.functions` contains**: the functions a class declares, plus the template instances generated for it,
because those are functions of the class in the program as built; reflection describes the program that exists,
not the source as written. **The list holds every function the class has once compiling ends**, in the order they
were made, so it does not depend on the order classes were compiled in. **A constructor is not one of them**: it does
not answer on an instance, it makes one. A class of the standard library describes its members too:
`Memory.Heap.functions` lists `allocate`, `resize`, `free` and `live_allocations`, the functions whose bodies the
compiler supplies.

**`.class` is a real `Spite.Class`, not the class's name as text**, and so are `Spite.Attribute.class` and
`Spite.Argument.class`: `function.arguments[0].class == ServerContext` is an identity comparison the compiler
checks, where a test on the name's text quietly turns false when a class is renamed. Printing a `Spite.Class`
prints its `.name`, and printing a `Spite.Namespace` its dotted name.

**A namespace is an instance of `Spite.Namespace`**: the tree of folders made readable, `.parent` walking up,
`.classes` and `.namespaces` walking down. `.namespace` and `.parent` are `Spite.Namespace?`, and the chain ends
at `null`; there is no root object. `Spite.Namespace.instances` is every namespace of the program's own classes,
parents first, in the order the classes were found. `.enums` is the enums declared in the files directly in the
namespace's folder, each a `Spite.Class`, in the order those files were loaded; an enum at the root of a program
is in no namespace's list. **A class and a namespace with the same dotted name is a compile
error.**

**`.source_files` and `.source_directories` are where a class and a namespace come from**: a `File` for every file
that declares or reopens the class, and a `Directory` for every directory a namespace's classes come from, each in
load order. A question about a path is asked of those `File` and `Directory` values; reflection has no other path
member. They are build-machine paths: a shipped program that needs the files beside a package copies them into its
output ([packages.md](packages.md#files-beside-a-packages-source)).

**`value.memory` is where a named value lives**: a class instance, a list or a dictionary answers with its object,
a `String` with its characters (`'constant'` for a literal), and a number held in a local with the local itself
(`'stack'`) or in an attribute with that attribute (`'heap'`). Only a named value has an address: a number computed
on the spot is "only a named value has memory of its own: give this value a name with 'var' first".

### A class and an instance

- **`Monster.attributes` and `Monster.functions` are declarations, `Spite.AttributeDeclaration` and
  `Spite.FunctionDeclaration`; `troll.attributes` and `troll.functions` are the same members bound to `troll`,
  `Spite.Attribute` and `Spite.Function`.** A class's members are information and an instance's are data: a
  declaration never gives a value. A PascalCase receiver is the class and a lowercase one an instance, the
  convention [Foreign libraries](foreign_libraries.md#foreign-libraries) uses for `user32.Input` against
  `user32.input_mouse`.
- **A bound attribute's `.value`** reads and writes the instance's field, typed as the field when the attribute is a
  constant. `.value` of an attribute that describes a declaration is "'attribute.value' reads an attribute of an
  instance, and 'attribute' describes a declaration: walk the instance's own attributes, 'monster.attributes', or
  read 'monster.attributes[attribute]'".
- **`value.attributes[attribute]`**, with a declaration, is that field of `value`, read or written: a plain typed
  field access when `attribute` is a constant, and a call to the getter where the class declares one.
- **A bound function** is a function value of that instance ([functions_and_operators.md](functions_and_operators.md#functions-are-values)),
  so a function named on an instance and its reflection are the same object. Calling it always calls.
- **`Spite.Call(declaration, instance)` builds a call without running it**, the only way to build one: the
  declaration is a `Spite.FunctionDeclaration`, and `instance` must be of the class that declares it.
  `.arguments['name'] = value`
  fills each argument once, and `call()` runs it. With the function and the arguments known while compiling it is
  the plain direct call, and no object exists. An argument left unfilled is "'call' leaves the argument 'velocity'
  of 'run_each' unfilled: set 'call.arguments['velocity']' before 'call()'" where the compiler sees it; otherwise
  `call()` halts naming it. An instance of another class is "'rock' is a 'Rock', and 'hurt' is declared by
  'Monster': a call runs a declaration on an instance of its own class".
- **A walk over another class's attributes sees its private ones**, since a walk that skips some silently builds an
  incomplete copy, column or layout: `.attributes` lists them in declaration order with the rest, and
  `value.attributes[attribute]` and a bound attribute's `.value` read and write them. Naming `_x` outside its class
  stays the private error. A serializer and `to_debug()` leave them out by testing `attribute.name`, which folds.
- **A number's, a `Boolean`'s and a `String`'s `.attributes` is empty**: the compiler stores those values itself,
  so the fields their library classes declare to say so are not attributes a walk could read or write.
- **A `List<T>`'s or `Dictionary<T>`'s attributes are its entries** (named by index or by key), not the fields of the
  class that stores them, so a class object's `.attributes` is empty for a collection.

### Reaching a class

- **A class name read as a value is its `Spite.Class`**, member by member, with no instance anywhere:
  `Weapon.name` is `Weapon`, and `Weapon.namespace` is the same namespace `weapon.class.namespace` gives. A function
  of the class described called on its name (`Weapon.debug()`) is "'Weapon' is a class, not a value: write
  'Weapon()' to make one, and give it a name with 'var ... = '", since there are no static functions.
- **A class name where a `Spite.Class` is wanted is its class object**: an argument, `var kind: Spite.Class =
  Health`, an assignment to such a name and a `return` from a function returning one; a codegen value bound to a
  class passes the same way. Anywhere else (`var kind = Health`, an argument whose parameter is a `type`) a bare
  class name is "'Health' is a class, not a value", naming the `Spite.Class` form.
- **On the right of `==` a class name is a class test, except when the left side is itself a `Spite.Class`**: then
  the two class objects are compared, so `component.class == component_class` and `kind == Health` both mean "the
  same class".
- **`class` is an inherited attribute of every instance, not a keyword**: inside a function it is the class the
  function answers on. A local or an attribute named `class` takes precedence (that is how `Spite.Attribute.class`
  reads its own field), and `class ClassKeyword {` at file scope fails at 1:1 with "there is no 'class' keyword",
  because a file is a class named after the file. **`namespace`** is the same kind of name for the class's
  namespace.
- **`.class` through a `type` or a union answers the real class**, from the object's own tag at run time; an
  object literal answers `Object`.
- **`Person.instances`** lists every live instance of a class; only a class whose `.instances` some code reads is
  tracked. A class object's `.attributes` and `.functions` are read from a stand-in at its defaults, made without
  running any constructor, which is not one of `.instances`; a singleton's stand-in never runs its `drop()`. The
  program's entry class has no stand-in: its `.functions` is empty. A stand-in of `Concurrent<T>` or `Parallel<T>`
  is finished and joins nothing. A class object and a namespace object are each made once, whichever thread asks
  first, behind one lock that a program without threads does not carry.
- **`Spite.Class.instances` is every class in the program**: the classes of the program's own folder and of every
  package it loads, not the standard library's (`Launcher` included, [programs.md](programs.md)), not the anonymous
  classes behind object literals, and not a generic class before its codegen values are supplied.

### Known while compiling

- **A constant** is a reflection object the compiler can identify: a class, enum or namespace named in the code (a
  name that is both an enum and a class is the enum), `$T`, `class` and `namespace` inside a function; `value.class`,
  `value.attributes` and `value.functions` when `value` is a local, parameter or attribute whose declared type is a
  class (not a `type`, a union, an optional or a reflection class); any member of a constant; `[]` on a constant
  list with a literal name or number; `count()`, `is_empty()`, `first()`, `last()`, `filter(function)` and the member
  templates on a constant list; a text function on a constant name; `==`, `!=`, `and`, `or` and `not` of constants;
  and a `var` initialised with a constant and never assigned again, when the function never needs its value at run
  time, `null` from a `[]` that finds nothing included. `filter(function)` asks a function whose body is one `return`, with its parameter bound to each element.
- **What folds.** A question on a constant is `true` or `false` in the generated code, a count is a number and a
  name is a literal. An `if`, `assert` or `crash` on a constant keeps only what runs, and one that always holds is
  not an error. `[]` with a literal name answers a constant `T?`: present, a narrowing `if`, `assert` or `crash` on it
  folds to nothing and still narrows that path for what follows; absent, its branch is never compiled.
- **`each(function)` over a constant list is unrolled** when the function is one of the class's own, takes one
  parameter, and every element fits it: one call per element, and no list.
- **A function called with a constant for a reflection parameter is compiled once for it**, whether it is the
  calling class's own or one called on another object (`helper.describe(attribute)`), a
  *specialisation* named `<function>_for_<member>` in `--final-classes` (a number is added when two would share a
  name). Inside it the parameter is the constant: `attribute.class` and `argument.class` are types, `part()` makes
  an instance of a constant class `part`, `monster.attributes[attribute]` reads and writes the field, and a function
  whose result is typed by its reflection parameter (`func made(argument: Spite.Argument): argument.class`) answers
  that type. A specialisation is not one of the class's `.functions`; the function itself is compiled only when
  something calls it with a run-time object or uses it as a value.
- **A run-time object** (a value typed `Anything` or a union, a reflection object kept in an attribute, anything
  typed at the REPL) is answered from the run-time tables, built only for the classes such a read can reach, and for
  every class in a `--repl`, `--repl-port` or `--development` build. A run-time function's `.accesses` comes from
  a table of every function the program makes an object for, emitted only when some code reads `.accesses` at run
  time. A standard library function, a function value taken through a `type` and every function of a `--hot-reload`
  build are not in it, and asking them halts. A run-time object answers the questions a constant does
  (`.is_stateful`, `.is_list`, `.index`, `argument.is_mutated` and the rest); the answers to the kind questions,
  `.is_mutated` and `.returned_literal` are written into the tables only when some code asks one of them at run
  time, and `.returned_literal` of a function whose body is not one `return` of a text literal halts. A run-time
  `attribute.value` is `Anything?`, a number
  held with its class's tag and text boxed. Handing one to a function whose result is typed by its reflection parameter is the error shown
  [above](#known-only-at-run-time); naming a type through a reflection object that is not a constant is "'<path>'
  is not a class known while compiling, so it cannot be a type here"; and using a specialisation's parameter as a
  run-time value is "'<name>' is a reflection object known while compiling, so this function is compiled once for
  it, and it has no run-time value to pass on here".
- **Reflection may be as detailed as it likes, because what is not used is not emitted.** Which parts of it a
  program touches is decidable while compiling, so tree shaking is exact: a program keeps the tables only for what
  it reads at run time, and a program that only asks constants keeps none.

### The names reflection gives every object

**`class`, `attributes`, `functions`, `instances` and `memory` cannot name an attribute or a function of a class.**
Neither can a `get_` or `set_` function whose member would be one of them (`get_attributes`). A class member of the
same name would be read in place of reflection's, silently: a class with `var attributes = Table.Attributes()` would
make every `JsonReader` of it read each key as one the class does not have. The error is

`the attribute 'attributes' has a name reflection gives every object: 'value.attributes' lists its attributes, and
JsonReader, JsonWriter and every attribute walk read through it, and one of its own would hide it.
Name it something else, like 'table_attributes'`

with the suggestion made from the class's own name, and for an accessor `the function 'get_attributes' is read as
'attributes', a name reflection gives every object: ...`. The classes of the `Spite` namespace are exempt: they are
reflection. Locals and parameters may use the names. A class of the standard library never declares one.

### Functions of `Spite.Class`, and why there are no static functions

**Spite has no static class functions.** A class is an instance of `Spite.Class`, an ordinary standard library
class with an ordinary declaration, so a member that belongs to the class rather than to its instances is a member
of that object, and `Spite.Class` is where it is declared, with its default. `library/spite/class.spite` declares
`.is_singleton` that way:

```gdscript
var _singleton = false

func get_is_singleton(): Boolean {
    return _singleton
}
```

A class file may **override** one of those members for its own class object, the same way any class reopens
another ([Packages, namespaces, and mods](packages.md#packages-namespaces-and-loading)). A singleton says so with its
`singleton` line ([classes_and_files.md](classes_and_files.md#singletons)), and declaring `is_singleton` in a class
is "a class says it is a singleton with a 'singleton' line at the top of its file, not with a function: delete
'is_singleton()' and write 'singleton' as the file's first line".

- **The members a class file can override are exactly those `Spite.Class` declares.** There is no keyword and no
  marker: if the name is one of those, the definition belongs to the class object; otherwise it is an ordinary
  instance function. Reading `Spite.Class` in the standard library is how you learn the full list.
- A class wanting an ordinary instance function whose name collides with one of them is a diagnostic naming
  `Spite.Class`. The list is deliberately short.
- The override is **evaluated at compile time** and must fold to a constant. A `return` of a literal always folds;
  a `return` of a codegen value (`return $shared`) folds too. Anything that cannot fold is a diagnostic.
- `--final-classes` prints each class's overrides with the value they folded to; a singleton's is its `singleton`
  line.
- **`Spite.Class` is reopenable, like any other standard library class.** Reopening it changes a default of every
  class object for the **whole program**, and adding a new member gives every class object that member, so a class
  that already had an ordinary function by that name becomes an override of it. Nothing about this is silent: a name
  that collides is a compile error naming `Spite.Class`, and a changed default shows per class in
  `--final-classes`, with the root it came from. The compiler has no warnings ([Style](style.md#style)), so visible
  generated output is the whole mitigation.

**Why a member and not a declaration**: it is the same philosophy as a configuration file being a class that gets
reopened, rather than a static file. A static line can only state a value; a function can compute one, and it costs
the language nothing because functions already exist.

---

Next: [Packages, namespaces, and mods](packages.md), how folders become namespaces and how a program loads packages.
