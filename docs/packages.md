# Packages, namespaces, and mods

There are no imports. Everything lives in one global namespace, populated by loading folders:

```gdscript
func Game() {
    load "package"
    load "cookie_clicker"
}
```

- The program's own folder is a root like any loaded one: every folder inside it is a namespace, recursively,
  with no `load` needed ([programs.md](programs.md)).
- The loaded folder itself does **not** appear in the namespace; folders *inside* it do. A file named like its
  folder is that folder's entry point: `package/engine/renderer/renderer.spite` becomes `Engine.Renderer()`,
  and `package/engine/renderer/debug.spite` (a sibling file in the same folder) becomes
  `Engine.Renderer.Debug()`.
- `load` only ever takes a literal string -- a variable or expression there is a diagnostic, so the compiler
  always knows every bundle statically. The one exception is the launcher, the Spite program that loads the
  standard library and then yours ([programs.md](programs.md#how-a-program-is-loaded)): its `load` may also use
  the `Build` fields the compiler already knows, like `build.target_operating_system` after `var build = Build()`.
- Folder names are always lowercase `snake_case`, checked for every folder that actually contains a `.spite`
  file anywhere inside it.
- Resolving a bare `Name` from inside a class tries, in order: that class's own namespace, its folder, each
  parent folder, then the whole program. A dotted `Component.Requested` is looked up the same way, and so is
  every name wherever it is written -- a constructor call, a parameter, a return type, an attribute or local
  annotation, a generic argument (`Remove<Component.Requested>`), a `type` or `union` member, a `==` class test
  and an enum inside a class (`Component.Requested.Size`) -- so from `window/system/` the name reaches
  `window/component/requested.spite` without writing `Window.` (`conformance/stage6/relative_namespaces`).
  A folder's entry file owns the folder as its namespace, so `click_test/click_test.spite` reaches
  `click_test/system/verify.spite` as `System.Verify()` (`conformance/stage6/folder_class_namespace`).
- A generic class is found the same way, so it may live in any folder: `Asset.Pack<Asset.Texture>("textures")`
  and, from inside `game/`, `Pack<Rule>("rules")` for `game/pack.spite` both work
  (`conformance/stage6/namespaced_generics`). A generic name that finds nothing is one error, and the lines that
  use what it would have made are not reported again (`diagnostics/failed_constructor`).
- A `type`, `union` or `enum` is part of the walk at the level of the class that declares it: one declared in
  the using class wins over every class, and one declared in a folder's entry file wins over a class further
  out. So `type Healing` in `system/regenerate.spite` is what `Healing` means there, even when the program's
  entry class is also `Healing` (`conformance/stage6/nearest_type`). The library is held to the same walk, so a
  program's own class `Entry` does not hide the union `Entry` that `library/directory.spite` declares:

```gdscript title=own_entry_class/entry.spite
var name = "mine"
```
```gdscript title=own_entry_class/own_entry_class.spite entry
var console = Console()

func OwnEntryClass() {
    var entry = Entry()
    console.print(entry.name)
}
```
```output
mine
```

- From outside its class, a declared type is named through its owner, in every position, generic arguments
  included: `List<Recipes.Cookbook.Buildable>` for the `type Buildable` a singleton `recipes/cookbook.spite`
  declares (`conformance/stage6/nested_type_from_outside`). Two classes may each declare a `Buildable`, and they
  are different types; when an error has to print two types that would read the same, it names both through
  their owners: `a List<Recipes.CookTask.Buildable> cannot be used where a List<Recipes.Cookbook.Buildable> is
  needed` (`diagnostics/same_named_types`).

## Monkey patching (mods)

Every loaded root merges into the same namespaces. A second root with the same folder structure and file name
**reopens** the class instead of colliding with it: a later `func`/`var` of the same name replaces the
earlier one (in load order), and a name not seen before is simply added. A `union` or `type` declared again
replaces the earlier declaration the same way; an `enum` declared again adds values to it instead
([below](#reopening-an-enum-adds-values)). This is how game mods work.

```gdscript title=package_demo/package/monster.spite
func describe(): String {
    return "a wild monster"
}
```
```gdscript title=package_demo/package/engine/renderer/renderer.spite
func render(): String {
    return "rendering the scene"
}
```

`mods` is loaded after `package`, so the `describe` below replaces `package/monster.spite`'s own, and `taunt`
is a name that folder never had. That is the whole of monkey patching.

```gdscript title=package_demo/mods/monster.spite
func describe(): String {
    return "a modded monster"
}

func taunt(): String {
    return "the modded monster taunts you"
}
```
```gdscript title=package_demo/package_demo.spite entry
var console = Console()

func PackageDemo() {
    load "package"
    load "mods"
    var renderer = Engine.Renderer()
    var rendered = renderer.render()
    console.print(rendered)
    var monster = Monster()
    var description = monster.describe()
    console.print(description)
    var taunt = monster.taunt()
    console.print(taunt)
}
```
```output
rendering the scene
a modded monster
the modded monster taunts you
```

`mods/monster.spite` never redeclares `health` -- reopening only needs the names it actually adds or replaces.
A `var`'s replacement must keep the same type. Within one file each name is declared once; replacing is what a
*later* file does.

The program's own folder is merged first and the folders it loads come after it, so "later" means a loaded
folder, not the program. A file in the program root that reopens a class a loaded folder declares keeps every
name the loaded folder does not have -- an added attribute or function works -- but where both declare the same
name, the constructor included, the loaded folder's version is the one that stays. To replace something a
loaded folder declares, put the replacement in a folder loaded after it, as `mods` is above.

The program's entry class is the one class nothing reopens: a loaded `bundle/potion.spite` in the program
`potion/` would otherwise become part of the entry class `Potion`, so it is an error naming both files
(`diagnostics/entry_class_reopened`).

The standard library is loaded before the program, so a program's own file reopens any class of it the same way:
`environment.spite` adds settings to `Environment` ([programs.md](programs.md#run-time-settings-environment)),
`list.spite` adds a member template to `List` ([collections.md](collections.md#write-your-own-member-template)),
`int.spite` adds a function to every `Int` ([values_and_types.md](values_and_types.md#numbers-are-classes)), and
`string.spite` could replace how `String` trims. Your foot to shoot. Each operating system's folder of the library
is built the same way: it reopens the classes that system does differently
([foreign_libraries.md](foreign_libraries.md#each-operating-system-reopens-what-it-changes)).

```gdscript title=reopen_string/string.spite
func shouted(): String {
    var upper = upper_case()
    return "{upper}!"
}
```
```gdscript title=reopen_string/reopen_string.spite entry
var console = Console()

func ReopenString() {
    var greeting = "hello"
    var shouted = greeting.shouted()
    console.print(shouted)
}
```
```output
HELLO!
```

### Reopening an enum adds values

An enum a class declares is open the same way (D180). A reopening file that declares the enum again lists the
values it adds, and they come after the ones already merged, in the order above: the program's own folder
first, then each loaded folder in load order. An engine's phases are an enum for exactly this: a mod adds a
phase, and everything that walks the enum -- `Symbol<Phase>`, or a name pattern whose hole is `phase`
([metaprogramming.md](metaprogramming.md#a-name-that-says-when-it-runs)) -- walks the new one too.

```gdscript title=phase_mod/engine/schedule.spite
enum Phase {
    'update'
    'render'
}

var console = Console()

func run_phase(phase: Symbol<Phase>) {
    console.print("running", phase.name)
}
```
```gdscript title=phase_mod/mods/schedule.spite
enum Phase {
    'input'
}
```
```gdscript title=phase_mod/phase_mod.spite entry
func PhaseMod() {
    load "engine"
    load "mods"
    var schedule = Schedule()
    schedule.run_phases()
}
```
```output
running update
running render
running input
```

A value the enum already has stays where it was, so a reopening may list the whole enum again -- what
`--final-classes` prints does -- without changing it; there is no way to remove a value. Since the values are
walked while compiling, adding one costs nothing at run time beyond what walking it generates.

## The `Spite` namespace is reserved

`Spite` is the root namespace for reflection (`Spite.Class`, `Spite.Attribute` -- see
[metaprogramming.md](metaprogramming.md)). A `spite/` folder of your own **reopens** those classes, which is
how a package is tried out before it is upstreamed. Inside the class, a reopening reads the private fields
(`_name`, `_namespace`) that the read-only `name` and `namespace` answer from outside:

```gdscript title=reopen_spite_class/spite/class.spite
func name_with_namespaces(): String {
    if _namespace {
        return "{_namespace.name_with_namespaces}.{_name}"
    }
    return _name
}
```
```gdscript title=reopen_spite_class/reopen_spite_class.spite entry
var console = Console()

func ReopenSpiteClass() {
    var console_class_name = console.class.name_with_namespaces()
    console.print(console_class_name)
}
```
```output
Console
```

Every class object in the program answers `name_with_namespaces()` from then on, because there is one `Spite.Class` and
that folder reopened it. A *new* class under `Spite` is a diagnostic instead -- that namespace holds the
standard library's own classes, and a class of your own belongs in a namespace of your own. `load "spite"` is
a diagnostic too:

```gdscript title=spite_namespace_error/spite_namespace_error.spite entry error
func SpiteNamespaceError() {
    load "spite"
}
```
```diagnostic
'spite' is reserved for the built-in Spite namespace
```

## `load` is a bundle boundary

`load` marks where a dynamic library or lazy-loaded bundle could split, the way an async `import()` does in
webpack. Today every root is linked into the one executable, and a `load` line compiles to nothing at run
time -- except the launcher's `load build.program`, which runs the program by constructing its entry class.
`load` is a keyword written without parentheses, on a line of its own (D186): `load("folder")` is an error naming
`load "folder"`, and nothing else may be named `load` -- a function that loads something says what,
`load_texture` (D166).

```gdscript title=load_parentheses_error/load_parentheses_error.spite entry error
func LoadParenthesesError() {
    load("level")
}
```
```diagnostic
'load' is a keyword, not a function: write it without parentheses, 'load "level"'
```

A `load` under an `if` is decided while compiling. The condition may read `Build` fields, text, whole numbers,
`true` and `false`, joined by `==`, `!=`, `and`, `or` and `not`, and only the branch it picks is loaded:

```
var build = Build()

func Engine() {
    if build.target_operating_system == "windows" {
        load "../plugins/windows_renderer"
    }
}
```

This works in the program's entry file and in any file of a loaded package, so a package can pick its own plugins.
Each path is relative to the folder of the file it is written in. A condition the compiler cannot decide, such as
one that reads `Arguments()`, is an error rather than a folder that is quietly left out.

Splitting bundles, and loading one lazily when a `load` inside an `if` runs, are decided but not built
([the rules in full](#packages-namespaces-and-loading--partial)).

A dependency will be a git URL pinned to a commit in the `load` line itself --
`load "github.com/mortaro/engine@a3f2c91"` -- fetched by the ordinary compile, with no package manager, registry
or lockfile. That is decided and not built ([D38](decisions.md)).

## Final classes

Because patching is dangerous to read silently, `--final-classes=folder` writes every class as it ended up, after
every root is merged and every reopening resolved, as a program that runs the same as the one it was printed from
([compiler.md](compiler.md#inspect-merged-classes)). This is the file to read when you are not sure which mod won.
Which root supplied each declaration is not printed yet.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Packages, namespaces and loading  **[partial]**

There are no imports. Everything lives in one global namespace, populated by loading folders.

```gdscript
func Game() {
    load "package"
    load "cookie_clicker"
}
```

- The loaded folder is a package root and does **not** appear in the namespace. Folders inside it do:

```
package/engine/renderer/renderer.spite   ->  Engine.Renderer()   (a file named like its folder is the folder's entrypoint)
package/engine/renderer/debug.spite      ->  Engine.Renderer.Debug()
```

- `spite game/game.spite` **loads the entry file's parent folder** (second batch item 1, decided 2026-09-19,
  replacing "the entry folder is flat"): the entry folder is a real root exactly like a `load`-ed one, so every
  subfolder inside it is a namespace, recursively, with no explicit `load` needed. The two differences from a
  `load`-ed root: a subfolder the entry file itself explicitly `load`s is left to that line (a `load`-ed
  root's own folder is never itself a namespace segment), and only the entry file itself is scanned from the
  entry root -- an unrelated sibling file next to it is still pulled in whenever it is reachable as a class, but
  its own `load`s are not followed.
- Folder names are always lowercase snake_case, checked for every folder that actually contains a `.spite` file anywhere inside
  it (an unrelated folder with none, such as `.git` or a build output directory, is never checked or descended for classes).
- Resolving an unqualified `Name` from inside a class tries, in order: that class's own namespace (so a nested enum/type/union
  resolves by its plain name from inside its own class), the same folder's namespace, each parent folder's namespace, then the
  whole program globally. Ambiguity *between roots* at the same level is never an error, because of the next rule.
  A dotted name (`Component.Requested`) takes the same walk, in every position a name is written -- constructor
  call, parameter, return type, attribute or local annotation, generic argument, `type`/`union` member, class test
  and a class's enum (proposed by Claude, unconfirmed, 2026-09-24; before, only a call took the walk and every
  type position needed the full path; `conformance/stage6/relative_namespaces`).
  A class's own namespace holds the classes named under it, so for a folder's entry file it is that folder:
  `click_test/click_test.spite` (`ClickTest`) reaches `click_test/system/verify.spite` as `System.Verify()`
  (proposed by Claude, unconfirmed, 2026-09-24; before, the walk started at the folder's parent, so the entry
  class alone needed `ClickTest.System.Verify()`; `conformance/stage6/folder_class_namespace`).
  A `type`, `union` or `enum` sits in the walk at the level of the class that declares it, so the nearest
  declaration wins: one in the using class before any class, one in a folder's entry file before a class further
  out (proposed by Claude, unconfirmed, 2026-09-24; before, a type written in a type position was looked up only
  after every class, so SlopEngine's `type Healing` in `system/regenerate.spite` meant the program's entry class
  `Healing`; `conformance/stage6/nearest_type`).
  From outside its class a declared type is named through its owner, generic arguments included
  (`List<Recipes.Cookbook.Buildable>`, `conformance/stage6/nested_type_from_outside`). **When an error prints
  two types that would read the same, both are named through their owners** (proposed by Claude, unconfirmed,
  2026-09-25): two classes' own `type Buildable` are different types, and "a List<Buildable> cannot be used
  where a List<Buildable> is needed" said nothing, so it reads `a List<Recipes.CookTask.Buildable> ... a
  List<Recipes.Cookbook.Buildable>` (`diagnostics/same_named_types`).
  A generic class's constructor takes the walk too, class and arguments alike: `Asset.Pack<Asset.Texture>()`, or
  `Pack<Rule>()` from inside `game/` for `game/pack.spite` (proposed by Claude, unconfirmed, 2026-09-24; until
  then a generic class had to live at a package root, `conformance/stage6/namespaced_generics`). A generic
  constructor that cannot be made reports once, and every later line that reads the value it would have made is
  not reported again, so one mistake is one error (`diagnostics/failed_constructor`).
- Every loaded root merges into the same namespaces. A second root with the same folder structure and file name **reopens** the
  class: this is how monkey patching and game mods work -- later `func`/`var` with the same name replaces the earlier one (in
  load order: the entry folder first, then loads in the order they were discovered), a `var`'s replacement type must match, and
  a new `func`/`var`/`enum`/`type` not seen before is simply added. An `enum` declared again gains the values it
  lists that it did not have, after its own (D180, [Types](values_and_types.md#types)). The standard library (`library/`) is discovered
  before the program, so a program's own file reopens `Console`, `String`, `File` and the rest the same way.
  `Environment` is made to be reopened: a program's own `environment.spite` adds its settings to it (D76, [Program settings: `Environment`](programs.md#program-settings-environment--implemented)).
  Your foot to shoot. That includes `Spite.Class` (D7, [Class-level functions, and why there are no static functions](reflection.md#class-level-functions-and-why-there-are-no-static-functions--implemented)): reopening it moves a class-level default for the whole program.
- **Each operating system reopens the classes it changes** (D80). `library/` holds what every system shares, and
  `library/windows/`, `library/linux/` and `library/mac/` hold only what differs: `library/linux/file.spite` reopens
  `File` with the functions that call `libc.so.6`, `library/windows/file.spite` the same functions over
  `ucrtbase.dll`, and so on for `Directory`, `Process`, `Program`, `Console`, `String` (`to_double`) and
  `Build` (`target_operating_system`). There is no wrapper class in between: a platform file holds its own
  `DynamicLibrary("libc.so.6", 'identity', "")`, which is one shared instance per literal argument list (D8), and
  a one-line foreign call two classes both need is written in both. The launcher ([Constructors and the entrypoint](programs.md#constructors-and-the-entrypoint)) loads `library/`
  first and then the one folder named by `build.target_operating_system`; that folder adds no namespace segment,
  and its files are read after `library/`'s own, so they replace or add members by the reopening rule above. `--final-classes`
  prints each class as it came out, platform functions included. Because exactly one folder is loaded,
  `library/file.spite` calls `open_file` without declaring it: every system's folder defines it, with the same
  signature.  **[implemented; only `library/windows/` runs here -- `check.sh` holds the linux and mac
  folders to compiling, by writing the compiler out once with each]**
- **`load` is a keyword, written without parentheses** (D186): `load "package"`, on a line of its own inside a
  function. `load("package")` is a parse error naming the keyword form ("'load' is a keyword, not a function:
  write it without parentheses, 'load "package"'"), and so is `load` used as a value
  (`diagnostics/load_parentheses`, `diagnostics/load_as_value`). It stays reserved (D166): a function, variable
  or parameter named `load` is an error suggesting a descriptive name like `load_texture`
  (`diagnostics/load_function`). The launcher's own lines use it the same way, with a `Build` field:
  `load "library/{build.target_operating_system}"`, `load build.program`.
- `load` takes a literal string, so the compiler always knows every bundle; anything else (a variable, an expression) is a
  diagnostic. The compiler finds every `load` reachable from the entry file's own constructor at compile time (a `load` inside
  already-loaded code counts too), and records each root as a bundle (its name and whether the `load` that introduced it sits
  inside an `if`/`while`) in the program model, for dynamic libraries/lazy loading to build on later.
- **A `load` under an `if` is decided when compiling** (proposed by Claude, unconfirmed, 2026-09-25; before, a `load`
  anywhere but a function's top level, or in any file but the entry file, silently loaded nothing). The compiler
  follows the `load` lines of the entry file and of every file of a loaded root, in every function, relative to
  that file's folder, and folds the condition of each `if` that holds one: it may read `Build` fields through the
  class's `var build = Build()`, text, whole numbers, `true` and `false`, joined by `==`, `!=`, `and`, `or` and
  `not`. Only the taken branch's folders are loaded, so `if build.target_operating_system == "windows" { load
  "../plugins/windows" }` brings in the plugin on Windows alone, and a package can choose its own plugins the same way
  (`conformance/stage6/load_on_build`). A condition it cannot fold is an error, never a silent no-load
  (`diagnostics/load_unknown_condition`), and so is a `load` inside a `while` or a `switch`, or a program `load` whose
  folder is not text. The `if` itself stays in the program, where `Build` folds it again. A loaded package cannot
  add `Build` fields that decide loads: the fields are read before the first package is.
- `load` marks a **bundle boundary**, like an async import in webpack: each loaded root can become a separate dynamic library,
  tree shaking is computed per bundle, and a `load` inside an `if` is loaded lazily when that line runs.  **[planned: every
  bundle is linked statically into the one executable for now, and the `load` line itself compiles to nothing]**
- Because patching is dangerous to read, the toolchain writes a **final class** folder: every class after all codegen, with the
  winning function of every replacement, each preceded by a `#` comment naming the root it came from (and which roots it
  replaced) -- see `--final-classes` in [Command line](compiler.md#command-line). The language server reads it too.
  **[planned: an instantiated Symbol codegen function and a used `List<T>`/`Dictionary<T>` helper signature do not appear here
  yet -- only the source classes are declared with]**
- **What the compiler supplies is a reopening too** (D82, decided by Mortaro: "spite writes that function as if it
  was reopening that class, so in --final-classes it should show the actual content of that class"). The members
  whose bodies stay C -- `Memory.Heap`'s allocation and `Memory.Address`'s reads and writes (D178), `DynamicLibrary`'s opening and symbol lookup, `TypedMemory`'s typed
  slots, a number's `from_type`, the REPL's hooks on `Spite.Attribute`/`Spite.Function`, and the entry addresses of
  `Concurrent` and `Parallel` -- are declarations the compiler merges into their classes right after `library/` (and
  its operating system's folder), exactly as a later root merges a file, so a program can still reopen them.
  Nothing is registered by hand any more (D81). **The form** (proposed by Claude, unconfirmed): **a `func` with a
  signature and no block** is a member whose body the compiler supplies, which is how `--final-classes` prints it
  (`func allocate(bytes: Long): Memory.Address`), and reading that back is what lets the printed program compile. It
  borrows the shape a `type` already uses for a member without a body. Anywhere the compiler supplies nothing by
  that name, a bodiless `func` is an error ("give it a body"), so it is not a way to declare anything else. The
  compiler's own reopening is Spite source in `bootstrap/source/generation/prelude.spite`, beside the C each body
  is; a body the compiler writes per instantiation (`TypedMemory<Int>`, `Float.from_int`) is written by the
  generator. Supplied names skip the naming lint (`read_int` names the type `Int`), as conversions already did.
