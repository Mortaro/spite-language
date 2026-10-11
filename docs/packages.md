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
- The loaded folder itself does **not** appear in the namespace; folders *inside* it do, and every folder is
  only a namespace: `package/engine/renderer.spite` is `Engine.Renderer()`, and
  `package/engine/physics/body.spite` is `Engine.Physics.Body()`. No file is special: a file named like its
  folder is an ordinary class inside it, so `package/engine/physics/physics.spite` is `Engine.Physics.Physics()`,
  reopened by another root's `engine/physics/physics.spite` like any class. A file and a folder of one name side
  by side, `engine/renderer.spite` beside `engine/renderer/`, would make `Engine.Renderer` both a class and a
  namespace, so that is an error ([the specification](../specs/packages.md#packages-namespaces-and-loading)).
- `load` only ever takes a literal string: a variable or expression there is an error, so the compiler
  always knows every bundle statically. The one exception is the launcher, the Spite program that loads the
  standard library and then yours ([programs.md](programs.md#how-a-program-is-loaded)): its `load` may also use
  the `Build` fields the compiler already knows, like `build.target_operating_system` after `var build = Build()`.
- Folder names are always lowercase `snake_case`, package roots included: `window_plugin`, never
  `window-plugin`, and a folder that is not is an error before anything compiles
  ([the specification](../specs/packages.md#packages-namespaces-and-loading)).
- Resolving a bare `Name` from inside a class tries, in order: that class's own namespace, its folder, each
  parent folder, then the whole program. A dotted `Component.Requested` is looked up the same way, and so is
  every name wherever it is written: a constructor call, a parameter, a return type, an attribute or local
  annotation, a generic argument (`Remove<Component.Requested>`), a `type` or `union` member, a `==` class test
  and an enum inside a class (`Component.Requested.Size`), so from `window/system/` the name reaches
  `window/component/requested.spite` without writing `Window.` (`conformance/stage6/relative_namespaces`).
  A class's folder is where the walk starts, so `click_test/click_test.spite` (`ClickTest.ClickTest`) reaches
  `click_test/system/verify.spite` as `System.Verify()` (`conformance/stage6/folder_class_namespace`).
- A generic class is found the same way, so it may live in any folder: `Asset.Pack<Asset.Texture>("textures")`
  and, from inside `game/`, `Pack<Rule>("rules")` for `game/pack.spite` both work
  (`conformance/stage6/namespaced_generics`). A generic name that finds nothing is one error, and the lines that
  use what it would have made are not reported again (`diagnostics/failed_constructor`).
- A `type`, `union` or `enum` is part of the walk at the level of the class that declares it: one declared in
  the using class wins over every class. So `type Healing` in `system/regenerate.spite` is what `Healing` means
  there, even when the program's entry class is also `Healing` (`conformance/stage6/nearest_type`). The library is held to the same walk, so a
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
earlier one (in load order), and a name not seen before is simply added. A `union`, `type` or `enum` declared
again replaces the earlier declaration the same way ([below](#reopening-an-enum-replaces-it)). This is how game
mods work.

```gdscript title=package_demo/package/monster.spite
func describe(): String {
    return "a wild monster"
}
```
```gdscript title=package_demo/package/engine/renderer.spite
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

A reopening file declares only the names it adds or replaces, and a `var`'s replacement must keep the same type. Within one file each name is declared once; replacing is what a
*later* file does.

The program's own folder is merged first and the folders it loads come after it, so "later" means a loaded
folder, not the program. A file in the program root that reopens a class a loaded folder declares keeps every
name the loaded folder does not have (an added attribute or function works), but where both declare the same
name, the constructor included, the loaded folder's version is the one that stays. To replace something a
loaded folder declares, put the replacement in a folder loaded after it, as `mods` is above.

The program's entry class is the one class nothing reopens: a loaded `bundle/potion.spite` in the program
`potion/` would otherwise become part of the entry class `Potion`, so it is an error naming both files
(`diagnostics/entry_class_reopened`).

The standard library is loaded before the program, so a program's own file reopens any class of it the same way:
`environment.spite` adds settings to `Environment` ([programs.md](programs.md#run-time-settings-environment)),
`list.spite` adds a member template to `List` ([collections.md](collections.md#write-your-own-member-template)),
`integer.spite` adds a function to every `Integer` ([values_and_types.md](values_and_types.md#numbers-are-classes)), and
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

### Reopening an enum replaces it

An enum a class declares is open the same way, and like a function it is replaced whole: a reopening file
that declares the enum again lists every value the enum has from then on, in its own order. Nothing is
appended and nothing merged, so a later folder (in the order above: the program's own folder first, then each
loaded folder in load order) that wants to add a value restates the list with the new one in it. An engine's
phases are an enum for exactly this: a mod restates the phases with its own among them, and everything that
walks the enum's values, `Phase.values`
([metaprogramming.md](metaprogramming.md#walking-a-programs-structure)), walks the new list.

```gdscript title=phase_mod/engine/schedule.spite
enum Phase {
    'update'
    'render'
}

var console = Console()

func run_every_phase() {
    Phase.values.each(run_phase)
}

func run_phase(phase: Phase) {
    console.print("running", phase)
}
```
```gdscript title=phase_mod/mods/schedule.spite
enum Phase {
    'input'
    'update'
    'render'
}
```
```gdscript title=phase_mod/phase_mod.spite entry
func PhaseMod() {
    load "engine"
    load "mods"
    var schedule = Schedule()
    schedule.run_every_phase()
}
```
```output
running input
running update
running render
```

A value the reopening leaves out is gone: code that still names it is a compile error, the same `'fog' is not
a value of this enum` as for any value the enum never had (`diagnostics/enum_reopening_replaces`). One
declaration is the whole truth about an enum, so a reader never has to add up several files to know its values.
Since the values are walked while compiling, a new one costs nothing at run time beyond what walking it
generates.

## The `Spite` namespace is reserved

`Spite` is the root namespace for reflection (`Spite.Class`, `Spite.Attribute`; see
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
that folder reopened it. A *new* class under `Spite` is a diagnostic instead: that namespace holds the
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

`Spite.Internal` is the one namespace of the standard library that a program may not name at all: it holds the
machinery under the library (raw memory, threads, locks), and the library offers plain forms instead, such as
`List<Byte>` and `BinaryReader` for bytes. Naming it from your own code is a compile error that says which plain form
to use.

## `load` is a bundle boundary

`load` marks where a dynamic library or lazy-loaded bundle can split, the way an async `import()` does in
webpack. A `load` line compiles to nothing at run time, except the launcher's `load build.program`, which runs the
program by constructing its entry class. So a `load` costs nothing when the program runs; what it loads is
tree-shaken like the rest of the program.
`load` is a keyword written without parentheses, on a line of its own, and nothing else may be named `load`: a
function that loads something says what, `load_texture` ([the specification](../specs/packages.md#packages-namespaces-and-loading)).

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

```gdscript
var build = Build()

func Engine() {
    if build.target_operating_system == "windows" {
        load "../plugins/windows_renderer"
    }
}
```

This works in every file of the program's folder and of a loaded package, so a package can pick its own plugins.
Each path is relative to the folder of the file it is written in, unless it is absolute: `load
"D:/Projects/engine/core"` or `load "/home/example/engine/core"` loads a package that lives in another
repository. The program then builds only on a machine that has that folder, which the path in its source
already says. A condition the compiler cannot decide, such as
one that reads `Arguments()`, is an error rather than a folder that is quietly left out.

Each loaded root can become a separate dynamic library, tree shaking is computed per bundle, and a `load` inside an
`if` is loaded lazily when that line runs ([the specification](../specs/packages.md#packages-namespaces-and-loading)).

## Loading a repository pinned to a commit

A dependency is a repository and a commit, written in the `load` line itself. There is no package
manager, no registry, no lockfile and no fetch step: the ordinary compile fetches what a `load` names, and the pin
lives in the source, which is already versioned.

```gdscript
func Game() {
    load "github.com/example/engine@6c7dca9/core"
    load "../audio@b41e0d2/plugins/ogg_plugin"
    load "D:/Projects/spite_truetype@41c09e2/truetype"
}
```

The text is `<repository>@<commit>`, optionally followed by `/<folder inside it>`, which is loaded as a root
exactly as a folder on disk would be; with no folder, the repository's own top folder is the root. The repository
is either a URL, `github.com/example/engine` (fetched over `https://`) or one written with its scheme (`https://`,
`ssh://`, `git://` or `file://`), or a repository on this machine, named by a path that starts with
`./`, `../`, `/` or a drive letter, relative to the folder of the file the `load` is written in like any `load`.
Nothing else is a repository, so `load "engine@6c7dca9"` is an error that shows both forms, and a folder
without an `@` is an ordinary folder.

The commit is required, and it is a commit: 7 to 40 lowercase hexadecimal digits. A branch or a tag moves, and a
pin must not, so `@main` is an error, and so is a URL without an `@`. A pinned commit never changes silently: the
compiler checks the commit out once, then compiles from that copy.

**Where the copy lives.** The first compile that meets a pin checks the commit's files out into `.spite/git/` in the
folder `spite` was run from ([compiler.md](compiler.md#where-the-outputs-go)), as
`.spite/git/<repository name>_<number>/<commit>/`, the number worked out from the repository's whole path or URL,
and prints `fetched <repository>@<commit> into <folder>`. Every later compile finds it there and runs no git at all.
The copy is plain files, with no `.git` in it, kept out of the program's folders, so nothing mistakes it for source
to edit. From a repository on this machine the files are written straight from its objects: its working files, its
index, its branches and its worktrees are left exactly as they were, so the repository you are working in can be
the one you pin. From a URL the repository is cloned once, bare, into `.spite/git/<repository name>_<number>/repository`,
and fetched again only when a pin names a commit it does not hold yet. The compiler runs the `git` on the `PATH`.

**The copy is read-only.** Fetching records every file of the commit beside the copy, and each compile checks the
copy against that record before reading it: a file changed or removed there, or a `.spite` file added, is an error
naming it, never code that quietly differs from the commit. To change a package, change its repository, commit, and pin the
new commit; to fetch a copy again, delete its folder. A copy is never formatted, and live reload never watches it
([repl.md](repl.md#live-reload---hot-reload)): what it holds cannot change while the program runs.

**Pins inside a package.** A fetched package may pin repositories of its own. A relative repository path in it is
read from where the file sits in *its* repository, not in the copy, so an engine at `D:/Projects/engine` that
loads `../../../../spite_truetype@41c09e2/truetype` from `plugins/ui_plugin/ui/` reaches
`D:/Projects/spite_truetype` both when you build the engine itself and when a game loads the engine at a pin. A
package fetched from a URL names the repositories it loads by URL, since a path on its author's machine means
nothing on yours. An ordinary `load` in a fetched package stays inside that package's repository: one that leaves
it is an error, since the copy holds only its own commit's files. **Two pins of one repository** at different
commits are two libraries ([below](#two-versions-of-one-repository)).

This program loads a folder of the language's own repository as it was at a commit. The documentation's programs
are written out into `.spite/docs/<name>/` of that repository and compiled from there, so `../../..` is the
repository itself:

```gdscript title=pinned_widgets/pinned_widgets.spite entry
var console = Console()

func PinnedWidgets() {
    load "../../..@6cb29ccf/conformance/stage2/namespaces_load/package"
    var widget = Widgets.Widget("gadget", 3)
    var description = widget.describe()
    console.print(description)
}
```
```output
gadget x3
```

### Two versions of one repository

Two pins of one repository at different commits are two different libraries. Nothing
unifies them, nothing compares what differs between them, and it is not an error: a game that pins
`engine@6c7dca9` and a plugin that pins `engine@41c09e2` each compile against the engine they pinned.

```gdscript
func Game() {
    load "../engine@6c7dca9/core"
    load "../minimap@b41e0d2/minimap"
}
```
```gdscript
func Minimap() {
    load "../../engine@41c09e2/core"
}
```

A **package** here is a repository at a commit, or the program itself: the program's folder and every folder it
loads without a pin belong to the program, and every folder of a pinned copy belongs to that copy. A name written
in a package means the classes of the version that package pinned, so `Renderer` in the game is the engine at
`6c7dca9` and `Renderer` in the minimap is the engine at `41c09e2`. Each version's classes are its own: a value of
one version's `Renderer` is not accepted where the other's is needed, and a singleton of one is not the other's.
A reopening reopens the version its package pinned, so a game's `mods/renderer.spite` changes the engine the game
loads and leaves the minimap's alone. A class of the standard library that a version reopens is still the one
class, as it is for any two packages.

One package may pin a repository at two commits, and that is not two versions: the two pins are two ordinary
loads into the same namespace, and the later one reopens the earlier exactly as a later folder does ([monkey
patching](#monkey-patching-mods)), overriding what both define. A package that pins `engine@6c7dca9/core` and then
`engine@41c09e2/tools` reads the tools of the later commit on top of the core of the earlier. If the two commits do
not fit together, the compiler reports whatever breaks, as it would for any two folders, and the answer is to pin
commits that fit (or fork the repository, which is all git). The pair is one version, spelled by its first commit,
and a commit belongs to one version only: a commit that one package reads alongside another cannot also be read
alone by a second package, which is an error naming both lines.

A package that pins
neither version and names a class that only the versions have is an error listing each version and the line that
pinned it, and so is a package that reopens such a class: it loads the version it means.

Messages, reflection and `--final-classes` spell a version's class with the version first, the repository's name
and the commit (`engine_6c7dca9.Renderer`), and `--final-classes` prints each version into a folder of that
name. The code the versions share unchanged is folded into one by identical code folding
([optimizations.md](optimizations.md#identical-functions-are-folded-into-one)).

## Files beside a package's source

A package does not know where the program that loads it lives, so a relative path it opens (`File("shaders/
sky.spv")`) is read from the folder the program runs in, not from the package. A class asks where it
comes from instead: `class.source_files` inside it, `Recipe.source_files` for a class by name, and
`$item_type.source_files` for a generic's class answer a `List<File>`, every file that declares or reopens the class
in load order, and a namespace's `source_directories` answers a `List<Directory>` the same way
([reflection.md](reflection.md#the-spite-classes)). The paths are absolute, worked out while compiling, so a plugin
finds the files beside it wherever it is loaded from and whatever folder the program runs in.

Those paths are on the machine that **built** the program. It is the right answer while developing, and the wrong
one for a program shipped to someone else, whose machine has no such folder: a shipped program copies or cooks the
files it needs into its own output and opens them from there. Asking a `Spite.Class` held in a variable
(`kind.source_files`, with `var kind: Spite.Class = Recipe`) answers the same files when the program runs, and only a
program that asks that way carries the paths of its class objects.

## Final classes

Because patching is dangerous to read silently, `--final-classes=folder` writes every class as it ended up, after
every root is merged and every reopening resolved, as a program that runs the same as the one it was printed from
([compiler.md](compiler.md#inspect-merged-classes)). This is the file to read when you are not sure which mod won.

---

Next: [Concurrency: waiting without colouring](concurrency.md), running work in parallel without marking functions.
