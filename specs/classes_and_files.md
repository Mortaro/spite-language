# Classes and files

The specification of [Classes and files](../docs/classes_and_files.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Lexical structure

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
  ([Null safety and `assert` narrowing](../docs/failure.md#narrowing)). Writing `do`
  is a parse error naming that form: `Spite has no 'do': an 'if' on a value that may be null narrows it in place,
  'if value { ... } else { ... }', and inside the block 'value' is the value itself` (`diagnostics/old_do`).
- There is no `let` and no `const`: `let total = 1` is `Spite has no 'let': a variable is declared 'var name =
  value', always with a default, and there is no 'const'` (`diagnostics/old_let`). Both stay usable as names.
- There is no `class` keyword: `there is no 'class' keyword: a file is a class, named after the file, and its
  attributes and functions are written at the top level of that file` (`diagnostics/class_keyword`). `class`
  written bare inside a function is the instance's own `Spite.Class` ([reflection.md](../docs/reflection.md)), and no
  attribute or function may be named `class`, nor `attributes`, `functions`, `instances` or `memory`, the other
  names reflection gives every object
  ([reflection.md](reflection.md#the-names-reflection-gives-every-object)).

## Files are classes

Each `.spite` file is exactly one class, named by the file name in PascalCase (`repository_list.spite` is `RepositoryList`).
This cannot be changed.

A file contains only declarations, in the order the compiler enforces ([A file is ordered](../docs/style.md#a-file-is-ordered)):

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

**A class never hides another class.** A class hides another when both end in the same name and the other's
namespace encloses this one's, so that inside this one's namespace the name reaches this one instead:
`Physics.Plugin` hides a root `Plugin`, and `Game.Memory.Heap` hides `Memory.Heap`. That is an error naming both:
"physics/plugin.spite: error: the class 'Physics.Plugin' hides the class 'Plugin' (plugin.spite): inside 'Physics'
the name 'Plugin' would mean this class, so give one of them a clearer name" (`diagnostics/hidden_class`). It holds
within the program, between the program and the packages it loads, and between two packages. Two classes of one
name in namespaces side by side (`Physics.Plugin` and `Audio.Plugin`) hide nothing, and neither does an `enum`,
`union` or `type` declared inside a class, which is named through that class.

**A class named like a class of the standard library is an error** wherever it is, naming the library's class to
use: "the class 'Geometry.Vector3' is named like the standard library's class 'Vector3': use 'Vector3', reopening it
to add what it lacks, or give this class a clearer name" (`diagnostics/named_like_library`). Only a library class
written by its bare name counts (`Vector3`, `Color`, `List`): one inside a namespace (`Spite.Function`,
`Memory.Arena`, `String.Inflection`) is always written with it outside its own folder, and inside that folder a
file with its name reopens it, so a program's `Function` or `Arena` is legal (`conformance/stage6/namespaced_library_names`). A file with the library class's whole dotted
name is not a new class but a reopening of it ([Reopening a class](../docs/classes_and_files.md#reopening-a-class)).

**A reopening of a standard library class may not add a hand copy of what the library already answers.** A
function that a program's file adds to a library class is an error when it takes a parameter of the reopened class,
assigns the class's own attributes, and the library class already declares an operator function (`sum`,
`subtract`, `multiply`, `divide`, `remainder` or `negate`) whose parameter types the added function also takes.
The error names the operator to write: "matrix4.spite:1: error: 'Matrix4.set_product', added
by this reopening, rewrites this Matrix4 from two Matrix4 values, which the library's '*' already answers: write
'target = left * right' (the answer is built in target's place)" (`diagnostics/hand_copy_of_library`). A function
on other types, or one that reads without writing the object, stays legal.

## Constructor rules

- A function named exactly like its class is the constructor, and a call of the class's name makes an object;
  there is no `new`.
- A class with no constructor has an implicit one that takes no arguments and does nothing: `Member()`
  gives every field its declared default. An explicitly written empty constructor is an error: `'Holder' has an
  empty constructor: delete it. A class without a constructor is already made from its defaults`
  (`diagnostics/empty_constructor`). A generic class needs no constructor either: its codegen values are its
  `generic` lines.
- `assert` is not allowed in any constructor
  ([failure.md](../docs/failure.md#assert-is-not-allowed-in-a-constructor)).
- One file declares each name once, with no overloading: `'DuplicateFunction' declares 'twice' twice: a later
  file may reopen a class and replace a function, but one file declares each name once`
  (`diagnostics/duplicate_function`). An argument casts to the parameter's type instead
  ([functions_and_operators.md](../docs/functions_and_operators.md)).

## A constructed object must be kept and used

**A constructed object must be kept and used.** A constructor call written as a statement on its own (`Spawn(bundle)`,
`Report(text)`, `Box<Integer>(3)`) is an error: `'Report(text)' makes a 'Report' and drops it: a constructed object
must be kept and used, so a class whose construction is the whole point should be a function instead; turn
'Report' into a function of the class that needs it, or keep the object in a variable that is read`. One stored in
a variable nothing reads is already the unused-name error. A singleton is exempt: it is bound as an
attribute, and a bare `Console()` statement is the inline-singleton error instead. The check is
compile time only (`diagnostics/dropped_construction`).

## Singleton rules

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
  for threads to contend on ([optimizations.md](../docs/optimizations.md#singletons-made-on-first-use-never-counted),
  `conformance/stage6/singleton_counts`).
- **Teardown order.** A singleton counts as made
  when its constructor *finishes*, so a singleton its attributes or constructor made is made before it and
  outlives it. At exit every `Concurrent` still running is run to its end first
  ([concurrency.md](concurrency.md)), then standard output is flushed, then singletons are destroyed newest first, and every
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
  [optimizations.md](../docs/optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form) and
  [concurrency.md](../docs/concurrency.md).
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

Next: [Programs: the entry, the launcher, and settings](programs.md).
