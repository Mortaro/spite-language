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
([rules](#files-are-classes)):

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
function instead ([rules](#a-constructed-object-must-be-kept-and-used)):

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
[the rules](#singleton-rules).

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
(a class instance, `List<T>`, `Dictionary<T>`, `String`, a union, an object literal) is a reference: passing one
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
to something else or returning it ([rules](values_and_types.md#numbers-are-classes-and-this)):

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
one ([rules](#lexical-structure)). On a parameter, and only there, the prefix says
instead that the body ignores it on purpose; a private attribute nothing reads is an error like any other, and
no spelling silences it ([style.md](style.md#nothing-unused)).

## Reopening a class

A second file with the same name, in a later loaded root, **reopens** the class instead of colliding with it:
its functions and attributes replace the earlier ones of the same name and add the rest. That is how a program
adds settings to `Environment`, a member template to `List`, a function to `Integer`, and how a mod changes a game
([packages.md](packages.md#monkey-patching-mods)). The program's entry class is the one class nothing may
reopen.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge cases, the
exact error texts and the notes on how each rule is compiled. Where the teaching above and these rules disagree, the
rules win.

### Lexical structure

- Newlines end statements and separate entries in lists, objects, enums, unions and types. Commas separate entries written on one line.
- Comments start with `#` and run to the end of the line.
- `"text"` is a String. Strings only use double quotes.
- `'value'` is an enum value. Single quotes are only for enum values.
- `name` (lowercase, snake_case) is a variable, attribute, function or folder.
- `Name` (uppercase first letter) is a class or type, including the whole standard library.
- `$name` is a codegen value: it is replaced at code generation time (see [Codegen values](metaprogramming.md#codegen-values-)).
- `_name` is private: read, written or called only inside its own class, a reopening included. On a
  parameter, and only there, it means instead that the body ignores the parameter on purpose
  ([style.md](style.md#unused-is-an-error)). Using a private name from another class is an error
  naming the getter when there is one: `'_hidden' is private to 'Secret': a name starting with '_' is used only
  inside its own class, and 'hidden' reads it from outside`
  ([Reflection objects](reflection.md#reflection-objects)).
- Keywords: `var func return if else while switch type enum union generic assert crash and or not null true false this`
  (`this` is in every class: it passes or returns the object itself,
  [Numbers are classes, and `this`](values_and_types.md#numbers-are-classes-and-this)).
  - `load` is a keyword at the start of a statement, written without parentheses (`load "engine"`), and
    cannot name a function, variable or parameter
    ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)).
  - `singleton` is a keyword only as a line of its own at the top of a file; it stays usable as a name,
    since `Spite.Class` has an attribute called `singleton`.
  - `generics` is reserved only so that writing a header list gets this error: `there is no 'generics' list: a class
    declares each codegen value on its own line at the top of the file, like 'generic $damage_type'`.
- There is no `do`: `if value { } else { }` narrows in place instead
  ([Null safety and `assert` narrowing](failure.md#narrowing)). Writing `do`
  is a parse error naming that form: `Spite has no 'do': an 'if' on a value that may be null narrows it in place,
  'if value { ... } else { ... }', and inside the block 'value' is the value itself` (`diagnostics/old_do`).
- There is no `let` and no `const`: `let total = 1` is `Spite has no 'let': a variable is declared 'var name =
  value', always with a default, and there is no 'const'` (`diagnostics/old_let`). Both stay usable as names.
- There is no `class` keyword: `there is no 'class' keyword: a file is a class, named after the file, and its
  attributes and functions are written at the top level of that file` (`diagnostics/class_keyword`). `class`
  written bare inside a function is the instance's own `Spite.Class` ([reflection.md](reflection.md)), and no
  attribute or function may be named `class`, nor `attributes`, `functions`, `instances` or `memory`, the other
  names reflection gives every object
  ([reflection.md](reflection.md#the-names-reflection-gives-every-object-1)).

### Files are classes

Each `.spite` file is exactly one class, named by the file name in PascalCase (`repository_list.spite` is `RepositoryList`).
This cannot be changed.

A file contains only declarations, in the order the compiler enforces ([A file is ordered](style.md#a-file-is-ordered)):

- a `singleton` line, when the class has one instance ([Singleton rules](#singleton-rules))
- `generic $name` lines: the codegen values a caller supplies, each optionally constrained by a `type`,
  `generic $name: Printable` ([Codegen values (`$`)](metaprogramming.md#codegen-values-))
- `enum`, `union` and `type` declarations, which are namespaced under the class (`Player.Job`)
- `var` declarations: the attributes of the class, each with a default
- `func` declarations: the constructor, then the other functions of the class

There are **no file level statements**: no loops, ifs or calls outside a function. One is a parse error:
`a file holds declarations and nothing else: move this statement into a function, or into the class's
constructor` (`diagnostics/file_scope_statement`).

The program's entry class may not be reopened by another file
([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading),
`diagnostics/entry_class_reopened`).

### Constructor rules

- A function named exactly like its class is the constructor, and a call of the class's name makes an object;
  there is no `new`.
- A class with no constructor has an implicit one that takes no arguments and does nothing: `Member()`
  gives every field its declared default. An explicitly written empty constructor is an error: `'Holder' has an
  empty constructor: delete it. A class without a constructor is already made from its defaults`
  (`diagnostics/empty_constructor`). A generic class needs no constructor either: its codegen values are its
  `generic` lines.
- `assert` is not allowed in any constructor
  ([failure.md](failure.md#assert-is-not-allowed-in-a-constructor)).
- One file declares each name once, with no overloading: `'DuplicateFunction' declares 'twice' twice: a later
  file may reopen a class and replace a function, but one file declares each name once`
  (`diagnostics/duplicate_function`). An argument casts to the parameter's type instead
  ([functions_and_operators.md](functions_and_operators.md)).

### A constructed object must be kept and used

**A constructed object must be kept and used.** A constructor call written as a statement on its own (`Spawn(bundle)`,
`Report(text)`, `Box<Integer>(3)`) is an error: `'Report(text)' makes a 'Report' and drops it: a constructed object
must be kept and used, so a class whose construction is the whole point should be a function instead; turn
'Report' into a function of the class that needs it, or keep the object in a variable that is read`. One stored in
a variable nothing reads is already the unused-name error. A singleton is exempt: it is bound as an
attribute, and a bare `Console()` statement is the inline-singleton error instead. The check is
compile time only (`diagnostics/dropped_construction`).

### Singleton rules

**A singleton says so with a `singleton` line at the top of its file**, first in the file's order.
`is_singleton()` stays readable on `Spite.Class`, answered from the line; declaring `func is_singleton()` in any
class but `Spite.Class` is an error naming the line: `a class says it is a singleton with a 'singleton' line at
the top of its file, not with a function: delete 'is_singleton()' and write 'singleton' as the file's first line`
(`diagnostics/singleton_function`).

```gdscript console.spite
singleton

var heap = Memory.Heap()
```

- **Call sites never change.** `var console = Console()` reads the same for a singleton and for any other
  class; the class file is where the difference is said.
- **One instance per distinct constructor argument values.** A singleton constructed with different
  argument values is a different instance, and the same values give the same instance, for every singleton:
  `var a = Channel(1)` and `var b = Channel(2)` are two objects, `Channel(1)` asked for anywhere else is `a`. The
  constructor runs once per argument values, on first use; each instance is a root until exit, like any
  singleton. `DynamicLibrary` is this rule's first case, not an exception: `DynamicLibrary("user32.dll",
  'windows', "windows.h")` is one object however many classes ask for it, and `"gdi32.dll"` is a second one.
  The arguments are literals the compiler reads while compiling (strings, numbers, `true`, `false`, enum
  values), so each distinct list is its own static slot and nothing is looked up at run time; any other
  argument is "'starting_name' is not a literal: a singleton's arguments are literals the compiler reads, because
  each distinct argument list is its own instance of 'Registry', made the first time it is asked for"
  (`diagnostics/singleton_arguments`).
- **A generic singleton has one instance per set of codegen values**: `Column<Health>()` is one object wherever it
  is called and `Column<Label>()` is another (`conformance/stage6/generic_singletons`).
- **A singleton is bound as an attribute, never a local**: every singleton a class uses is visible at the
  top of its file. `var build = Build()` inside a function is "'Build' is a singleton, bound here as a local: bind
  it once beside the attributes, 'var build = Build()', so every singleton the class uses is at the top of its
  file" (`diagnostics/local_singleton`). The one exception is a value class such as `String`, which has no
  attribute to spare and binds it as a local `var` in the function that needs it. The exception covers every class
  the compiler treats as a value (`String` and the numbers) and
  `List`, `Vector` and `Dictionary`, since an attribute there would be carried by every list; and a generic singleton whose
  codegen values come from the function's own codegen or Symbol (`Query<argument.class>()`,
  `Debug<$value_type.element_type>()` inside a codegen `if`) stays a local, since no attribute can name that type.
  The singleton is then made when the object that binds it is, not when the function first runs.
- **A singleton is always bound to a variable before it is used.** A singleton's constructor call may
  only be the whole value of a `var`; a member read or call on it, passing it (`greet(Console())`, which
  reports this rather than the constructor-as-argument error), returning it, assigning it, putting it in an
  operation, or writing it
  as a statement of its own is an error naming the fix (`diagnostics/inline_singleton`,
  `diagnostics/inline_singleton_attribute`):

  ```gdscript
  console.print(Build().target_operating_system)
  # error: 'Build' is a singleton: bind it once beside the attributes, 'var build = Build()', and use
  # 'build.target_operating_system'
  ```

  A generic singleton (`TypedMemory<Integer>()`) is covered the same way. As with the call-argument rule, only code
  the compiler generates is checked: a function nothing calls is not. A member read on a
  generic singleton made from a walked attribute's class (`Column<attribute.class>().values`) is not an error,
  since no attribute can name that type: it is how a walked row reads a sparse column
  ([memory.md](memory.md#borrowed-items-of-a-vectort)); passing or returning one still is.
- **An unread singleton binding is an error everywhere**: `var world = World()` that nothing in the
  program reads is `the attribute 'world' is never read: remove it`, even inside a loaded package, whose other
  unread public attributes are tree-shaken rather than reported ([style.md](style.md#unused-is-an-error)).
- **Not counted.** A singleton's retain and release do nothing: the program records each singleton as it
  is made and destroys them at exit, so it is destroyed even when a leaked object still points at it, and
  `--debug-memory` still reports that leaked object. Fetching one is a load from its static slot, with no count
  for threads to contend on ([optimizations.md](optimizations.md#singletons-made-on-first-use-never-counted),
  `conformance/stage6/singleton_counts`).
- **Teardown order.** A singleton counts as made
  when its constructor *finishes*, so a singleton its attributes or constructor made is made before it and
  outlives it. At exit standard output is flushed, then singletons are destroyed newest first, and every
  `DynamicLibrary` is unloaded after all of them. A `drop()` may therefore use any singleton made before its
  own, and any library. A `drop()` that fetches a singleton already destroyed, one first made *after* the
  dropping singleton, halts with `spite: a drop() at exit used the singleton Archive after it was destroyed:
  singletons are destroyed in reverse order of when they were made, so keep Archive in an attribute of the
  singleton whose drop() uses it`. Since a singleton is bound as an attribute, a class's own singletons
  are made with it; the halt is left for a local binding the rule allows, such as one in a function of `String`
  (`conformance/stage6/singleton_used_after_exit` reopens `String` for it). A singleton first made inside a
  `drop()` is destroyed right after it (`conformance/stage6/singleton_teardown`). A singleton's memory is given
  back only after every singleton is destroyed, so an older singleton that holds a newer one, such as a registry that
  generic singletons record themselves in, like an engine's `Columns` and its `Column<T>`s, lets go of it
  without reading freed memory (`conformance/stage6/singleton_held_at_exit`, `singleton_freed_after_teardown`).
- **Never made in a circle.** A hang is a bug.
  A singleton's attributes are made with it, so singletons whose attributes make each other (directly, through
  a generic singleton, or through an ordinary object whose own attributes bind one, as in `var sample: Entity = ...`
  making an `Entity` that binds `World`) could never finish. The compiler follows every attribute of every
  singleton the program makes, through the objects those attributes make, and a path back to the start is an
  error at the attribute that begins it: `singleton 'World' binds 'Column<Entity>', which binds 'Columns', which
  binds 'World': singletons initialise each other in a circle, so one of them has to reach the other some other
  way, such as by binding it in the class that uses both` ("binds" for a singleton, "makes" for any other object;
  `diagnostics/singleton_circle`). A circle that only a constructor's body closes, which the compiler does not
  follow, halts at run time instead of spinning: each thread keeps the singletons it is in the middle of making,
  and asking for one of them again halts with `spite: singleton 'Registry' binds 'Catalog', which binds
  'Registry': singletons initialise each other in a circle` (or `... is asked for while it is being made: a
  singleton cannot reach itself as it initialises` for one singleton), naming only the singletons, which are all
  it knows (`conformance/stage6/singleton_circle`). Cost: the list is read and written only on a singleton's
  first fetch, the path that already takes its lock; every later fetch is the one load it was.
- **Made once, whichever thread asks first.** In a program that starts
  threads, the first fetch of a singleton takes a lock of its own, checks again and makes it; every later fetch
  is still one load. A program with no threads keeps the plain check (`conformance/stage6/singleton_race`).
- **Safe for threads without a keyword.** A singleton a `Parallel` can reach is made thread-safe
  by the compiler in the cheapest form proven safe, a lock of its own being the fallback, only in programs that
  use `Parallel`. The details are in
  [optimizations.md](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form) and
  [concurrency.md](concurrency.md).
- **A singleton that holds nothing**, with no attributes but settings the compiler folds, and no `drop()`, such as
  `Memory.Heap`, `Build` and `TypedMemory<T>`, is one static object in a production build and an ordinary
  singleton in an inspectable one ([Command line](compiler.md#command-line)). There it may be made again
  if something destroyed after it at exit asks for it, since it has nothing to lose.
- **Run-time cost**: one static slot per singleton (per argument list, per set of codegen values) and one
  guarded branch at its first fetch; a singleton nothing reaches is tree-shaken with its class.
- **Standard library:** `Console`, `Program`, `Environment`, `Build`, `Clock`, `Memory.Heap`, `ThreadPool` and
  `DynamicLibrary`, among others, are singletons. `File`, `Directory` and `Process` are not: they are values (a
  path, a spawned command), and several may exist at once.
- **Reopening** ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)): a
  reopening may add the `singleton` line, which makes the class a
  singleton everywhere: every construction of it, in any file, then follows the rules above. Reopening `Spite.Class`
  itself to move the default for every class at once is allowed, at the author's own risk.

---

Next: [Programs](programs.md), the entry file, the launcher that loads a program and its settings.
