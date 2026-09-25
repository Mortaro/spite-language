# Classes and files

Every `.spite` file is exactly one class, named by PascalCasing the file name: `person.spite` is `Person`,
`repository_list.spite` is `RepositoryList`. This cannot be changed -- there is no `class` keyword and no way to
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
var count = 0

func Crate(new_count: Int) {
    count = new_count
}
```
```gdscript title=ordered_file/ordered_file.spite entry
var console = Console()

func OrderedFile() {
    var inventory = Inventory<Crate>()
    inventory.add(Crate(3))
    inventory.add(Crate(4))
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

A constructed object is kept and used. A constructor call written as a statement of its own, `Report(text)`,
makes an object and throws it away -- the construction was the whole point, so it is an error saying to write a
function instead; storing it in a variable nothing reads is the unused-name error. A singleton is not affected: it
is bound as an attribute (`var console = Console()`), never constructed as a statement.

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
instance, made the first time it is asked for, and the constructor takes no arguments. `Console`, `Program`,
`Environment`, `Build` and `Memory.Heap` are singletons; `File`, `Directory` and `Process` are not, since several may exist at once.

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
reverse order of creation: a singleton is made when its constructor finishes, so whatever its attributes made
outlives it, and every `DynamicLibrary` is unloaded after all of them. A `drop()` that fetches a singleton made
later than its own, one already destroyed, halts with a message saying to keep it in an attribute instead. It is not reference counted: fetching one is a load from a static slot, with no count
to raise or lower, so threads that share it never contend on it; the first fetch alone takes a lock, so two threads asking at
once still get one object. `is_singleton()` is answered by `Spite.Class` for every class, from the line;
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

A singleton is always bound to a `var` before it is used, beside the attributes or in a function, and used
through that name. Reading or calling a member on the constructor call itself is an error, and so is passing it,
returning it or storing it anywhere but a `var` of its own -- a constructor is otherwise allowed as an argument
([style.md](style.md#one-call-per-line)), but not a singleton's:

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

A name starting with `_` is private: it is read, written or called only inside its own class -- a reopening of
the class counts as inside -- and using it from anywhere else is an error that names the getter when there is
one. On a parameter, and only there, the prefix says instead that the body ignores it on purpose; a private
attribute nothing reads is an error like any other ([style.md](style.md#nothing-unused)).

## Reopening a class

A second file with the same name, in a later loaded root, **reopens** the class instead of colliding with it:
its functions and attributes replace the earlier ones of the same name and add the rest. That is how a program
adds settings to `Environment`, a member template to `List`, a function to `Int`, and how a mod changes a game
([packages.md](packages.md#monkey-patching-mods)).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Lexical structure  **[implemented]**

- Newlines end statements and separate entries in lists, objects, enums, unions and types. Commas separate entries written on one line.
- Comments start with `#` and run to the end of the line.
- `"text"` is a String. Strings only use double quotes.
- `'value'` is an enum value. Single quotes are only for enum values.
- `name` (lowercase, snake_case) is a variable, attribute, function or folder.
- `Name` (uppercase first letter) is a class or type, including the whole standard library.
- `$name` is a codegen value: it is replaced at code generation time (see [Codegen values](metaprogramming.md#codegen-values---implemented)).
- `_name` is private. (This may change.)
- Keywords: `var func return if else while switch type enum union generic assert crash and or not null true false this`
  (`generic` since D87, `this` since D83: the value a function answers on, [Variables and values](values_and_types.md#variables-and-values--implemented)), plus `singleton`, which is a keyword only as a line of its own at the top of a file
  (D104; it stays usable as a name, since `Spite.Class` has an attribute called `singleton`) — there is
  no `do` (second batch item 2, decided 2026-09-19: with `for` gone, `if value do name` had nothing to match, so
  `if value { } else { }` narrows in place instead, [Null safety and `assert` narrowing](failure.md#null-safety-and-assert-narrowing--implemented)). Writing `do` is a parse error naming the
  `if value { }` form.

### Files are classes  **[implemented]**

Each `.spite` file is exactly one class, named by the file name in PascalCase (`repository_list.spite` is `RepositoryList`).
This cannot be changed.

A file contains only declarations:

- a `singleton` line, when the class has one instance ([Singletons](#singletons--implemented), D104)
- `generic $name` lines: the codegen values a caller supplies, each optionally constrained by a `type`,
  `generic $name: Printable` ([Codegen values (`$`)](metaprogramming.md#codegen-values---implemented), D87, D175)
- `var` declarations: the attributes of the class
- `func` declarations: the functions of the class
- `type`, `enum` and `union` declarations, which are namespaced under the class (`Player.Job`)

There are **no file level statements**: no loops, ifs or calls outside a function.

```person.spite
var age = 0

func Person(new_age: Int) {
    age = new_age
}
```

### A constructed object must be kept and used

**A constructed object must be kept and used** (D139, decided by Mortaro; the message proposed by Claude,
unconfirmed). **[implemented]** A constructor call written as a statement on its own (`Spawn(bundle)`,
`Report(text)`, `Box<Int>(3)`) is an error: `'Report(text)' makes a 'Report' and drops it: a constructed object
must be kept and used, so a class whose construction is the whole point should be a function instead -- turn
'Report' into a function of the class that needs it, or keep the object in a variable that is read`. One stored in
a variable nothing reads is already the unused-name error (D118, D136). A singleton is exempt: it is bound as an
attribute (D110, D144), and a bare `Console()` statement is D110's inline-singleton error instead.
`diagnostics/dropped_construction`, `docs/classes_and_files.md`.

### Singletons  **[implemented]**

D104 (Mortaro, 2026-09-24, correcting an omission; superseding D8's function form): **a singleton says so with a
`singleton` line at the top of its file**, first in D67's order -- before `generic` lines, enums, unions, types,
variables, the constructor and functions. `is_singleton()` stays readable on `Spite.Class` (D88), answered from
the line.

```console.spite
singleton

var heap = Memory.Heap()
```

Declaring `func is_singleton()` in any class but `Spite.Class` is an error naming the line
(`diagnostics/singleton_function`). D8 (2026-09-19) had declared it with that function of `Spite.Class`, overriding
the default `Spite.Class` declares; what follows still holds for the line.

- **Call sites never change.** `var console = Console()` everywhere, exactly as it reads today. A human skimming
  a call site is never told and never needs to know; the AI writing the code learns it from the class file and
  from `docs/for_ai_writers.md`.
- **One instance per distinct constructor argument list**, and a singleton's constructor arguments must be
  literals (the rule `load` already has). `Console()` is one object;
  `DynamicLibrary("user32.dll", 'windows', "windows.h")` is one object however many classes ask for it, and
  `"gdi32.dll"` is a second one. The compiler emits one static slot per distinct argument list, so there is no
  runtime registry walk -- only a guarded branch the first time.
- **A generic singleton has one instance per set of codegen values** (proposed by Claude, unconfirmed; the
  reading of "one per literal argument list" for `generic` lines, 2026-09-24): `Column<Health>()` is one object
  wherever it is called and `Column<Label>()` is another, since codegen values are literals too. It was accepted
  and silently made a new instance per call before (`conformance/stage6/generic_singletons`).
- **Memory:** the slot holds one reference, so the count never reaches zero; `drop()` runs at program exit, in
  reverse creation order, and `--debug-memory` counts singletons as roots, never as leaks. Since a singleton never
  dies early, it is not counted at all (proposed by Claude, unconfirmed, 2026-09-24): its retain and release do
  nothing, the program records each singleton as it is made, and destroys them in reverse at exit. Two
  `Parallel` threads fetching `Slot<Int>()` 40 million times took 0.8 s with the atomic count and 0.04 s without
  (`conformance/stage6/singleton_counts`).
- **Teardown order** (proposed by Claude, unconfirmed, 2026-09-24): a singleton counts as made when its
  constructor *finishes*, so a singleton its attributes or constructor made is made before it and outlives it.
  At exit standard output is flushed, then singletons are destroyed newest first, and every `DynamicLibrary` is
  unloaded after all of them, since any code may call through a library and a library calls nothing back. A
  `drop()` may therefore use any singleton made before its own, and any library. A `drop()` that fetches a
  singleton already destroyed -- one first made *after* the dropping singleton, in a later call -- halts with
  `spite: a drop() at exit used the singleton Archive after it was destroyed: ...`, naming the fix: keep it in
  an attribute. A singleton first made inside a `drop()` is destroyed right after it
  (`conformance/stage6/singleton_teardown`, `conformance/stage6/singleton_used_after_exit`).
- **Made once, whichever thread asks first** (proposed by Claude, unconfirmed, 2026-09-24): in a program that
  starts threads, the first fetch of a singleton takes a lock of its own, checks again and makes it; every later
  fetch is still one load. Two `Parallel` threads first touching `Shelf<Int>()` made it twice before, and one
  copy leaked (`conformance/stage6/singleton_race`). A program with no threads keeps the plain check.
- **A singleton that holds nothing** -- no attributes but settings the compiler folds, and no `drop()`, such as
  `Memory`, `Build` and `TypedMemory<T>` -- is one static object in a production build and an ordinary singleton
  in an inspectable one (D143, [Command line](compiler.md#command-line)). There it may be made again if something destroyed after it at exit
  asks for it, since it has nothing to lose (proposed by Claude, unconfirmed).
- **Standard library:** `Console`, `Program` and `DynamicLibrary` are singletons. `File`, `Directory` and
  `Process` are not -- they are values (a path, a spawned command), and several may exist at once.
- A later root reopening the class ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)) may not disagree about it; that is a diagnostic. Reopening
  `Spite.Class` itself to move the default for every class at once is allowed and is D7's foot to shoot.
- A class wanting an ordinary instance function named `is_singleton` gets the error above, which names the
  `singleton` line.
- **A singleton is always bound to a variable before it is used** (D110, decided by Mortaro): "always force
  singletons to be used as variables first so AI does not get tempted to inline it." A singleton's constructor
  call may only be the whole value of a `var`; a member read or call on it, passing it, returning it, assigning
  it or putting it in an operation is an error naming the fix (`diagnostics/inline_singleton`,
  `diagnostics/inline_singleton_attribute`):

  ```
  console.print(Build().target_operating_system)
  # error: 'Build' is a singleton: bind it once beside the attributes, 'var build = Build()', and use
  # 'build.target_operating_system'
  ```

  The readings below are **(proposed by Claude, unconfirmed)**: the binding may be an attribute ("on top", which
  the message names) or a local `var` in a function -- a value class such as `String` has no attribute to
  spare, and an error path (`var program = Program()` before `program.exit(1)`) need not hold the program for
  the object's whole life. D77 lets a constructor be an argument; a singleton's constructor is the exception
  (`greet(Console())` is an error). A generic singleton (`TypedMemory<Int>()`) is covered the same way. As
  with D77, only code the compiler generates is checked: a function nothing calls is not.

Rejected on the way here, each for a reason worth keeping: `$singleton = true` and `$instances = 1` (`$` means
"replaced at code generation", and a directive the compiler reads and deletes is never replaced by anything); a
bare `singleton` first line (the file-top declaration shape D5 removed for `generics` -- adopted after all by D67
and D104, once `generic` lines made the header a pattern); `func Console(): Console`
or `func Console(): Spite.Singleton` (the constructor's return type carrying the meaning -- quiet, and the second
makes the return type describe how rather than what); `func shared_instance(): Console` (loudest and most honest,
but "the constructor is named after the class" stops being one rule); a private constructor `func _Console()`
(`_` would mean private, intentionally unused, *and* singleton); `var console = Console` without parentheses (the
class can no longer enforce it); and a constructor hand-written to return `Class.instances.first()` (boilerplate
in every singleton, plus a second way to allocate).
