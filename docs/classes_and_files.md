# Classes and files

Every `.spite` file is exactly one class, named by PascalCasing the file name: `person.spite` is `Person`,
`repository_list.spite` is `RepositoryList`. This cannot be changed: there is no `class` keyword and no way to
put two classes in one file. Every other file of the program is another class, and a folder is a namespace
([packages.md](packages.md)).

## What a file holds

A file contains only declarations, in this order:

1. a `singleton` line, when the class has one instance ([below](#singletons));
2. `generic $name` lines, one per codegen value a caller supplies, each optionally constrained by a `type`
   (`generic $name: Printable`, [metaprogramming.md](metaprogramming.md#generics-and-codegen-values-));
3. `enum`, `union` and `type` declarations, namespaced under the class (`Player.Job`);
4. `var` declarations: the attributes, each with a default value;
5. the constructor;
6. the other `func`s.

Anything out of that order is an error naming what came before it ([style.md](style.md#a-file-is-ordered)).
There are **no file-level statements**: no loop, `if` or call outside a function
([rules](../specs/classes_and_files.md#files-are-classes)):

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
    count: Integer
}

var items = List<$item_type>()
var sorting: Sorting = 'by_name'

func add(item: $item_type) {
    items.append(item)
}

func total(): Integer {
    return items.sum_count()
}
```
```gdscript title=ordered_file/crate.spite
var count = 0

func Crate(new_count: Integer) {
    count = new_count
}
```
```gdscript title=ordered_file/ordered_file.spite entry
var console = Console()

func OrderedFile() {
    var inventory = Inventory<Crate>()
    var small_crate = Crate(3)
    inventory.add(small_crate)
    var large_crate = Crate(4)
    inventory.add(large_crate)
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

func Person(new_age: Integer) {
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

A class with no constructor is made from its defaults with `Member()`, and writing an empty one is an error,
because it says nothing:

```gdscript title=team_roster/member.spite
var name = "unnamed"
var level = 1
```
```gdscript title=team_roster/team_roster.spite entry
var console = Console()

func TeamRoster() {
    var members = List<Member>()
    var first_member = Member()
    members.append(first_member)
    var second_member = Member()
    members.append(second_member)
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

Every `var` is written with its default, so an object made with no arguments is already complete: a fresh
`Member()` has every field at its declared default. There is no `null` for anything but a `T?`
([failure.md](failure.md)). A constructor is setup, not logic: `assert` is not allowed in one, and `crash` is how
it says something is a bug ([failure.md](failure.md#assert-is-not-allowed-in-a-constructor)). There is no `new`:
`Person(30)` makes one.

There is one function per name. A file declaring a name twice is an error, and there is no overloading: an
argument is cast to the parameter's type instead ([functions_and_operators.md](functions_and_operators.md)).

A constructed object is kept and used. A constructor call written as a statement of its own, `Report(text)`,
makes an object and throws it away. The construction was the whole point, so it is an error saying to write a
function instead ([rules](../specs/classes_and_files.md#a-constructed-object-must-be-kept-and-used)):

```gdscript title=dropped_report/report.spite
var console = Console()

func Report(text: String) {
    console.print("report: {text}")
}
```
```gdscript title=dropped_report/dropped_report.spite entry error
func DroppedReport() {
    Report("the door opens")
}
```
```diagnostic
'Report("the door opens")' makes a 'Report' and drops it: a constructed object must be kept and used, so a class whose construction is the whole point should be a function instead
```

## Singletons

A file whose first line is `singleton` has one instance: calling its constructor anywhere answers that same
instance, made the first time it is asked for (one instance *per argument values* when the constructor takes
arguments, [below](#one-instance-per-argument-values)). `Console`, `Program`,
`Environment`, `Build` and `Memory.Heap` are singletons; `File`, `Directory` and `Process` are not, since
several may exist at once.

```gdscript title=singleton_basics/scoreboard.spite
singleton

var points = 0

func score(amount: Integer) {
    points = points + amount
}
```
```gdscript title=singleton_basics/singleton_basics.spite entry
var console = Console()
var home = Scoreboard()
var away = Scoreboard()

func SingletonBasics() {
    home.score(2)
    away.score(3)
    var is_singleton = Scoreboard.is_singleton()
    console.print(home.points, home == away, is_singleton)
}
```
```output
5 true true
```

Call sites never change: `var console = Console()` reads the same whether or not the class is a singleton, and
the class file is where that is said. `is_singleton()` is answered by `Spite.Class` for every class, from the
line.

A singleton lives until the program ends, and its `drop()` runs then, newest singleton first, so a `drop()` may
use any singleton its class keeps in an attribute. What one costs at run time is a static slot, filled the first
time it is asked for: fetching it is one load, it is never reference counted, and a program never pays for a
singleton it does not reach ([optimizations.md](optimizations.md#singletons-made-on-first-use-never-counted)).
A singleton that a `Parallel` can reach is made safe for threads by the compiler, with nothing written in the
source, and only in programs that use `Parallel`
([optimizations.md](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form)). The details are in
[the rules](../specs/classes_and_files.md#singleton-rules).

### One instance per argument values

**A singleton constructed with different argument values is a different instance**. `Singleton(1)` and
`Singleton(2)` are two objects; `Singleton(1)` asked for again, anywhere in the program, is the first one. So a
singleton is not "one object of this class" but "one object per value of its arguments": the arguments are the
instance's name. Two bindings with the same values are the same object, and a write through one is seen through
the other; bindings with different values share nothing but the class.

```gdscript title=radio/channel.spite
singleton

var number = 0
var messages = List<String>()

func Channel(channel_number: Integer) {
    number = channel_number
}
```
```gdscript title=radio/radio.spite entry
var console = Console()
var news = Channel(1)
var music = Channel(2)
var news_again = Channel(1)

func Radio() {
    news.messages.append("storm tonight")
    music.messages.append("song")
    news_again.messages.append("roads closed")
    var news_count = news.messages.count()
    var music_count = music.messages.count()
    console.print(news == news_again, news == music, news_count, music_count)
}
```
```output
true false 2 1
```

`news` and `news_again` are one channel, so both messages land in it; `music` is a second channel with its own
list. The constructor runs once per argument values, the first time those values are asked for, and each instance
lives until the program ends like any singleton.

**The arguments are literals** the compiler reads while compiling: strings, numbers (a leading minus
included), `true`, `false` and enum values. Each distinct argument list is its own instance, made once and found
again without looking anything up at run time. Anything else is an error that names the rule:
`var registry = Registry(starting_name)` is "'starting_name' is not a literal: a singleton's arguments are literals
the compiler reads, because each distinct argument list is its own instance of 'Registry', made the first time it
is asked for" (`diagnostics/singleton_arguments`). `DynamicLibrary` follows the same rule: one library per file,
naming rule and header ([foreign_libraries.md](foreign_libraries.md)).

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
var numbers = Column<Integer>()
var again = Column<Integer>()
var words = Column<String>()

func GenericSingleton() {
    numbers.values.append(1)
    again.values.append(2)
    var number_count = again.values.count()
    var word_count = words.values.count()
    console.print(numbers == again, number_count, word_count)
}
```
```output
true 2 0
```

A singleton is bound once, as an attribute beside the others, and used through that name, so every singleton a
class uses is visible at the top of its file. Reading or calling a member on the constructor call itself is an
error, and so is passing it, returning it or storing it anywhere but a `var` of its own:

```gdscript title=singleton_inline_error/singleton_inline_error.spite entry error
var console = Console()

func SingletonInlineError() {
    console.print("built for {Build().target_operating_system}")
}
```
```diagnostic
'Build' is a singleton: bind it once beside the attributes, 'var build = Build()', and use 'build.target_operating_system'
```

## Classes are references

A scalar (`Integer`, `Float`, `Boolean`, an enum value) is passed by value, copied at the call site. Everything else
(a class instance, `List<T>`, `Dictionary<Key, Value>`, `String`, a union, an object literal) is a reference: passing one
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
independent object with the same field values when that sharing is not what you want
([memory.md](memory.md#do-call-copydeep_copy-for-an-independent-object)):

```gdscript title=shared_member/member.spite
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
`this.level` is an error that says `level`. What `this` is for, in every class, is handing the instance itself
to something else or returning it ([rules](../specs/values_and_types.md#numbers-are-classes-and-this)):

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

## Printing a class

`console.print` writes each value's `to_string()`, so a class prints once it says how, and printing one that does
not is a compile error naming `to_string`. `console.debug` needs nothing: every class has a `to_debug()` that shows
its name and attributes ([standard_library.md](standard_library.md#console)).

```gdscript title=printing_a_class/lamp.spite
var room = ""
var lit = false

func Lamp(starting_room: String) {
    room = starting_room
}

func to_string(): String {
    return "the {room} lamp"
}
```
```gdscript title=printing_a_class/printing_a_class.spite entry
var console = Console()

func PrintingAClass() {
    var lamp = Lamp("hall")
    console.print(lamp)
    console.debug(lamp)
}
```
```output
the hall lamp
Lamp { room: "hall", lit: false }
```

## Private names

A name starting with `_` is private: it is read, written or called only inside its own class (a reopening of
the class counts as inside), and using it from anywhere else is an error that names the getter when there is
one ([rules](../specs/classes_and_files.md#lexical-structure)). On a parameter, and only there, the prefix says
instead that the body ignores it on purpose; a private attribute nothing reads is an error like any other, and
no spelling silences it ([style.md](style.md#nothing-unused)).

## Reopening a class

A second file with the same name, in a later loaded root, **reopens** the class instead of colliding with it:
its functions and attributes replace the earlier ones of the same name and add the rest. That is how a program
adds settings to `Environment`, a member template to `List`, a function to `Integer`, and how a mod changes a game
([packages.md](packages.md#monkey-patching-mods)). The program's entry class is the one class nothing may
reopen.

Reopening a class of the standard library adds what it lacks; it does not copy what it has. A function that
rewrites the object from two values of its own class, beside the library's `*`, is an error that tells you to write
`target = left * right`: the compiler builds that answer in `target`'s place
([rules](../specs/classes_and_files.md)).

## A class name means one class

A class may not hide another class. If `physics/plugin.spite` (`Physics.Plugin`) sat beside a root
`plugin.spite` (`Plugin`), the name `Plugin` would mean one class inside `Physics` and the other everywhere else,
and a reader would have to work out which one each line reaches. So that is an error naming both, asking for a
clearer name. It holds against the classes of a package the program loads, and against the standard library: a
class of your own named like a library class is an error naming the library's class, since a `Vector3` or a `Color`
of your own is most likely one the library already has. To add what the library's class lacks, reopen it. A
library class that is always written with its namespace, `Spite.Function` or `Memory.Arena`, cannot be mistaken for
yours, so a `Function` or an `Arena` of your own is fine: the name is refused only where a reader could not tell at
first glance which class it means.

```gdscript title=own_vector/geometry/vector3.spite
var height = 0.0
```
```gdscript title=own_vector/own_vector.spite entry error
var console = Console()

func OwnVector() {
    var point = Geometry.Vector3()
    console.print(point.height)
}
```
```diagnostic
the class 'Geometry.Vector3' is named like the standard library's class 'Vector3': use 'Vector3', reopening it to add what it lacks, or give this class a clearer name
```

---

Next: [Programs](programs.md), the entry file, the launcher that loads a program and its settings.
