# Classes and files

Every `.spite` file is exactly one class, named by PascalCasing the file name: `person.spite` is `Person`,
`repository_list.spite` is `RepositoryList`. This cannot be changed -- there is no `class` keyword and no way to
put two classes in one file. Every other file of the program is another class, and a folder is a namespace
([packages.md](packages.md)).

## What a file holds

A file contains only declarations, in this order:

1. a `singleton` line, when the class has one instance ([below](#singletons));
2. `generic $name` lines, one per codegen value a caller supplies ([metaprogramming.md](metaprogramming.md#generics-and-codegen-values-));
3. `enum`, `union` and `type` declarations, namespaced under the class (`Player.Job`);
4. `var` declarations: the attributes, each with a default value;
5. the constructor;
6. the other `func`s.

Anything out of that order is an error naming what came before it ([style.md](style.md#a-file-is-ordered)).
There are **no file-level statements** -- no loop, `if` or call outside a function:

```gdscript title=file_scope_error/file_scope_error.spite entry error
var console = Console()

console.print("not allowed")

func FileScopeError() {
    console.print("a statement inside a function is fine")
}
```
```diagnostic
a file holds declarations and nothing else
```

A whole file in order, with a codegen value, an enum and a `type`:

```gdscript title=ordered_file/inventory.spite
generic $item_type

enum Sorting {
    'by_name'
    'by_count'
}

type Counted {
    name: String
    count: Int
}

var items = List<$item_type>()
var sorting: Sorting = 'by_name'

func add(item: $item_type) {
    items.append(item)
}

func total(): Int {
    return items.sum_count()
}
```
```gdscript title=ordered_file/crate.spite
var name = ""
var count = 0

func Crate(new_name: String, new_count: Int) {
    name = new_name
    count = new_count
}
```
```gdscript title=ordered_file/ordered_file.spite entry
var console = Console()

func OrderedFile() {
    var inventory = Inventory<Crate>()
    inventory.add(Crate("apples", 3))
    inventory.add(Crate("pears", 4))
    var total = inventory.total()
    console.print(total, inventory.sorting)
}
```
```output
7 by_name
```

## Constructors

A function named exactly like its class is the constructor:

```gdscript title=person_basics/person.spite
var age = 0

func Person(new_age: Int) {
    age = new_age
}
```
```gdscript title=person_basics/person_basics.spite entry
var console = Console()

func PersonBasics() {
    var person = Person(30)
    console.print("age", person.age)
}
```
```output
age 30
```

A class with no constructor is made from its defaults with `Member()` -- and writing an empty one is an error,
because it says nothing:

```gdscript title=team_roster/member.spite
var name = "unnamed"
var level = 1
```
```gdscript title=team_roster/team_roster.spite entry
var console = Console()

func TeamRoster() {
    var members = List<Member>()
    members.append(Member())
    members.append(Member())
    var members_count = members.count()
    console.print("count", members_count)
    crash members.count() == 2
    console.print("first name", members[0].name, "level", members[0].level)
}
```
```output
count 2
first name unnamed level 1
```

```gdscript title=empty_constructor_error/holder.spite
var held = 0

func Holder() { }
```
```gdscript title=empty_constructor_error/empty_constructor_error.spite entry error
var console = Console()

func EmptyConstructorError() {
    var holder = Holder()
    console.print(holder.held)
}
```
```diagnostic
'Holder' has an empty constructor: delete it
```

Every value has a default: `Int` is `0`, `Float` `0.0`, `Bool` `false`, `String` `""`, an enum its first value,
and a class its fields' defaults. There is no `null` for anything but a `T?` ([failure.md](failure.md)). A
constructor is setup, not logic: `assert` is not allowed in one, and `crash` is how it says something is a bug
([failure.md](failure.md#assert-is-not-allowed-in-a-constructor)). There is no `new`: `Person(30)` makes one.

There is one function per name. A file declaring a name twice is an error, and there is no overloading: an
argument is cast to the parameter's type instead ([functions_and_operators.md](functions_and_operators.md)).

## Singletons

A file whose first line is `singleton` has one instance: calling its constructor anywhere answers that same
instance, made the first time it is asked for, and the constructor takes no arguments. `Console`, `Environment`,
`Build` and `Memory` are singletons; `File`, `Directory` and `Process` are not, since several may exist at once.

```gdscript title=singleton_basics/scoreboard.spite
singleton

var points = 0

func score(amount: Int) {
    points = points + amount
}
```
```gdscript title=singleton_basics/singleton_basics.spite entry
var console = Console()

func SingletonBasics() {
    var home = Scoreboard()
    home.score(2)
    var away = Scoreboard()
    away.score(3)
    var is_singleton = Scoreboard.is_singleton()
    console.print(home.points, home == away, is_singleton)
}
```
```output
5 true true
```

Call sites never change: `var console = Console()` reads the same whether or not the class is a singleton, and
the class file is where that is said. A singleton lives until the program ends, and its `drop()` runs then, in
reverse order of creation. `is_singleton()` is answered by `Spite.Class` for every class, from the line;
declaring it yourself is an error that names the `singleton` line. `DynamicLibrary` is the one singleton that
takes arguments: it has one instance per distinct list of literal arguments
([foreign_libraries.md](foreign_libraries.md)).

A singleton with `generic` lines has one instance per set of codegen values, the way `DynamicLibrary` has one
per argument list: `Column<Health>()` is the same object everywhere, and `Column<Label>()` is a second one. This
is what per-type storage is written with.

```gdscript title=generic_singleton/column.spite
singleton

generic $component_type

var values = List<$component_type>()
```
```gdscript title=generic_singleton/generic_singleton.spite entry
var console = Console()

func GenericSingleton() {
    var numbers = Column<Int>()
    numbers.values.append(1)
    var again = Column<Int>()
    again.values.append(2)
    var words = Column<String>()
    var number_count = again.values.count()
    var word_count = words.values.count()
    console.print(numbers == again, number_count, word_count)
}
```
```output
true 2 0
```

## Classes are references

A scalar (`Int`, `Float`, `Bool`, an enum value) is passed by value, copied at the call site. Everything else --
a class instance, `List<T>`, `Dictionary<T>`, `String`, a union, an object literal -- is a reference: passing one
shares the exact same object, so a function can change it and the caller sees the change. There is nothing
special to write at the call site or in the parameter's type ([memory.md](memory.md)):

```gdscript title=level_up/member.spite
var name = "unnamed"
var level = 1

func Member(new_name: String) {
    name = new_name
}
```
```gdscript title=level_up/level_up.spite entry
var console = Console()

func LevelUp() {
    var hero = Member("Aria")
    bump_level(hero)
    console.print("name", hero.name, "level", hero.level)
}

func bump_level(member: Member) {
    member.level = member.level + 1
}
```
```output
name Aria level 2
```

Two names can hold the same object, and a change through either shows up through the other. `copy()` makes an
independent object with the same field values when that sharing is not what you want:

```gdscript title=shared_member/member.spite
var name = "unnamed"
var level = 1
```
```gdscript title=shared_member/shared_member.spite entry
var console = Console()

func SharedMember() {
    var original = Member()
    var alias = original
    var independent = original.copy()
    alias.level = 5
    independent.level = 9
    console.print("original level", original.level)
    console.print("independent level", independent.level)
}
```
```output
original level 5
independent level 9
```

## `this`

A class reads and calls its own members by name: `level`, `score(2)`. There is no `self.` to write, and writing
`this.level` is an error that says `level`. What `this` is for is handing the instance itself to something else:

```gdscript title=this_basics/registry.spite
var names = List<String>()

func record(drink: Drink) {
    names.append(drink.name)
}
```
```gdscript title=this_basics/drink.spite
var name = ""

func Drink(new_name: String) {
    name = new_name
}

func join(registry: Registry) {
    registry.record(this)
}
```
```gdscript title=this_basics/this_basics.spite entry
var console = Console()

func ThisBasics() {
    var registry = Registry()
    var tea = Drink("tea")
    tea.join(registry)
    var coffee = Drink("coffee")
    coffee.join(registry)
    var joined_names = registry.names.join(", ")
    console.print(joined_names)
}
```
```output
tea, coffee
```

Inside a function of a number class, `this` is the number itself ([values_and_types.md](values_and_types.md#numbers-are-classes)).
Likewise `class`, written bare inside any function, is the instance's own `Spite.Class`
([reflection.md](reflection.md)).

## Private names

A name starting with `_` is private: it is read, written or called only inside its own class -- a reopening of
the class counts as inside -- and using it from anywhere else is an error that names the getter when there is
one. The same prefix marks a local or parameter as intentionally unused ([style.md](style.md#nothing-unused)).

## Reopening a class

A second file with the same name, in a later loaded root, **reopens** the class instead of colliding with it:
its functions and attributes replace the earlier ones of the same name and add the rest. That is how a program
adds settings to `Environment`, a member template to `List`, a function to `Int`, and how a mod changes a game
([packages.md](packages.md#monkey-patching-mods)).
