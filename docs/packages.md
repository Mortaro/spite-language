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
- `load` only ever takes a literal string -- a variable or expression there is an error, so the compiler
  always knows every bundle statically. The one exception is the launcher, the Spite program that loads the
  standard library and then yours ([programs.md](programs.md#how-a-program-is-loaded)): its `load` may also use
  the `Build` fields the compiler already knows, like `build.target_operating_system` after `var build = Build()`.
- Folder names are always lowercase `snake_case`, package roots included: `slop_window_plugin`, never
  `slop-window-plugin` (D181), and a folder that is not is an error before anything compiles
  ([the rules in full](#packages-namespaces-and-loading--partial)).
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

A reopening file declares only the names it adds or replaces, and a `var`'s replacement must keep the same type. Within one file each name is declared once; replacing is what a
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
time -- except the launcher's `load build.program`, which runs the program by constructing its entry class. So
a `load` costs nothing when the program runs; what it loads is tree-shaken like the rest of the program.
`load` is a keyword written without parentheses, on a line of its own, and nothing else may be named `load`: a
function that loads something says what, `load_texture` ([the rules in full](#packages-namespaces-and-loading--partial)).

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

This works in every file of the program's folder and of a loaded package, so a package can pick its own plugins.
Each path is relative to the folder of the file it is written in, unless it is absolute: `load
"D:/Projects/slop_engine/slop"` or `load "/home/mortaro/slop_engine/slop"` loads a package that lives in another
repository (D227). The program then builds only on a machine that has that folder, which the path in its source
already says. A condition the compiler cannot decide, such as
one that reads `Arguments()`, is an error rather than a folder that is quietly left out.

Splitting bundles, and loading one lazily when a `load` inside an `if` runs, are decided but not built
([the rules in full](#packages-namespaces-and-loading--partial)).

## Loading a repository pinned to a commit

A dependency is a repository and a commit, written in the `load` line itself (D38, D283). There is no package
manager, no registry, no lockfile and no fetch step: the ordinary compile fetches what a `load` names, and the pin
lives in the source, which is already versioned.

```
func Game() {
    load "github.com/mortaro/slop_engine@6c7dca9/slop"
    load "../slop_audio@b41e0d2/plugins/slop_ogg_plugin"
    load "D:/Projects/spite_truetype@41c09e2/truetype"
}
```

The text is `<repository>@<commit>`, optionally followed by `/<folder inside it>`, which is loaded as a root
exactly as a folder on disk would be; with no folder, the repository's own top folder is the root. The repository
is either a URL -- `github.com/mortaro/slop_engine` (fetched over `https://`), or one written with its scheme,
`https://`, `ssh://`, `git://` or `file://` -- or a repository on this machine, named by a path that starts with
`./`, `../`, `/` or a drive letter, relative to the folder of the file the `load` is written in like any `load`.
Nothing else is a repository, so `load "slop_engine@6c7dca9"` is an error that shows both forms, and a folder
without an `@` is an ordinary folder, as before.

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
read from where the file sits in *its* repository, not in the copy, so an engine at `D:/Projects/slop_engine` that
loads `../../../../spite_truetype@41c09e2/truetype` from `plugins/slop_ui_plugin/ui/` reaches
`D:/Projects/spite_truetype` both when you build the engine itself and when a game loads the engine at a pin. A
package fetched from a URL names the repositories it loads by URL, since a path on its author's machine means
nothing on yours. An ordinary `load` in a fetched package stays inside that package's repository: one that leaves
it is an error, since the copy holds only its own commit's files. **Two pins of one repository** at different
commits are an error naming both lines today; that rule is decided to go ([below](#two-versions-of-one-repository)).

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

**Decided, not built** ([D296](decisions.md)). Two pins of one repository at different commits are two different
libraries. Nothing unifies them, nothing compares what differs between them, and it is not an error: a game that
pins `slop_engine@6c7dca9` and a plugin that pins `slop_engine@41c09e2` each compile against the engine they
pinned, its classes are not the other's, and a value of one version's class is not accepted where the other's is
expected. What that duplicates costs nothing: the compiler folds every generated C function that is exactly
identical to another into one ([optimizations.md](optimizations.md#identical-functions-are-folded-into-one)), so
the functions two versions share unchanged are in the executable once. Until it is built, two pins of one
repository at different commits are an error naming both `load` lines.

## Files beside a package's source

A package does not know where the program that loads it lives, so a relative path it opens -- `File("shaders/
sky.spv")` -- is read from the folder the program runs in, not from the package. A class asks for its own folder
instead: `class.source_folder()` inside it, `Recipe.source_folder()` for a class by name, and `$item_type.source_folder()`
for a generic's class (D228). The answer is the absolute folder of the file that declares the class, worked out
while compiling and written into the program as text, so a plugin finds the files beside it wherever it is loaded
from and whatever folder the program runs in.

That folder is on the machine that **built** the program. It is the right answer while developing, and the wrong
one for a program shipped to someone else, whose machine has no such folder: a shipped program copies or cooks the
files it needs into its own output and opens them from there. Asking a `Spite.Class` held in a variable
(`kind.source_folder()`, with `var kind: Spite.Class = Recipe`) answers the same folder when the program runs, and
only a program that asks that way carries the folders of its class objects.

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

There are no imports. Everything lives in one global namespace, populated by loading folders (`load "package"`).

- The loaded folder is a package root and does **not** appear in the namespace. Folders inside it do:

```
package/engine/renderer/renderer.spite   ->  Engine.Renderer()   (a file named like its folder is the folder's entrypoint)
package/engine/renderer/debug.spite      ->  Engine.Renderer.Debug()
```

- `spite game` **loads the program's folder as a root** (a program is named by its folder, D130): the program's
  folder is a real root exactly like a `load`-ed one, so every subfolder inside it is a namespace, recursively,
  with no explicit `load` needed (D182: a swappable plugin therefore lives beside the program, not inside it).
  The difference from a `load`-ed root: a subfolder that a file at the top of the program's folder explicitly
  `load`s is left to that line (a `load`-ed root's own folder is never itself a namespace segment). Every file of
  the program's folder has its `load`s followed, the entry file's first, like every file of a loaded root, so a
  sibling file's `load "tools"` loads `tools/` as a package root (D195: a `load` works in any file, and the entry
  file only names what runs first; `conformance/stage6/sibling_load`). The launcher class's own `load`s, printed
  into a `--final-classes` folder, are the launcher's and are not followed from there.
- **A folder named by a `load` inside its own tree is never also a namespace** (D211, decided by Claude under
  D205). A package `kitchen/` whose file loads `"garnish/pepper"` gets `kitchen/garnish/pepper/` as a root of its
  own, and not also as the namespace `Garnish.Pepper`, as the program's own folder already did for what its top
  files load; the same holds for a file in any folder of the program. Every `load` written in the package counts,
  including one under an `if` the build decides the other way, so `kitchen/garnish/sugar/` is neither a root nor a
  namespace then. The compiler finds them by reading the package's `load` lines before it walks its folders
  (`diagnostics/loaded_folder_namespace`, `conformance/stage6/load_on_build`). `Build` is still read before any
  package is loaded, so a package's `build.spite` cannot add a field that decides a `load`.
- **Folder names are always lowercase snake_case, package roots included** (D181): `slop_window_plugin`, never
  `slop-window-plugin`, for every folder that actually contains a `.spite` file anywhere inside it (an unrelated
  folder with none, such as a build output directory, is never descended for classes, and `.git`, `.spite` and
  `.spite-cache` are never walked at all, D283). A folder that
  is not is an error before anything compiles, naming the snake_case to rename it to: "the folder 'BadFolder' is
  not named in snake_case: every folder of a program, a package's own folder included, is lowercase words joined
  by '_', so rename it 'bad_folder'" (`diagnostics/folder_name`, and `diagnostics/load_folder_name` for `load
  "Slop-Plugin"`). What is checked (proposed by Claude, unconfirmed): the program's own folder, every folder
  inside it and inside a loaded root, and a loaded root's own folder -- not the folders above them that a path
  such as `../../plugins/render_vulkan` passes through, which are not part of the program. snake_case is a
  lowercase letter, then lowercase letters, digits and single `_`, not ending in `_`.
- Resolving an unqualified `Name` from inside a class tries, in order: that class's own namespace (so a nested enum/type/union
  resolves by its plain name from inside its own class), the same folder's namespace, each parent folder's namespace, then the
  whole program globally. Ambiguity *between roots* at the same level is never an error, because of the next rule.
  A dotted name (`Component.Requested`) takes the same walk, in every position a name is written -- constructor
  call, parameter, return type, attribute or local annotation, generic argument, `type`/`union` member, class test
  and a class's enum (proposed by Claude, unconfirmed, 2026-09-24; `conformance/stage6/relative_namespaces`).
  A class's own namespace holds the classes named under it, so for a folder's entry file it is that folder:
  `click_test/click_test.spite` (`ClickTest`) reaches `click_test/system/verify.spite` as `System.Verify()`
  (proposed by Claude, unconfirmed, 2026-09-24; `conformance/stage6/folder_class_namespace`).
  A name the walk does not find is `unknown type 'Server.Component.Eye'` at the line that writes it, in every
  position above, a `type`'s attribute included; when dropping its leading parts names a class, or a plain name
  is the last part of exactly one class, the error says which: `unknown type 'Server.Component.Eye': did you mean
  'Component.Eye'?` -- the usual slip being an environment's folder written into the name, when the folder joins
  the program's own namespaces (fixed 2026-09-26: a `type` attribute of an unknown type crashed the compiler
  when the shape needed a default; proposed by Claude, unconfirmed; `diagnostics/unknown_type_in_shape`,
  `diagnostics/unknown_type_suggestion`).
  A `type`, `union` or `enum` sits in the walk at the level of the class that declares it, so the nearest
  declaration wins: one in the using class before any class, one in a folder's entry file before a class further
  out, so `type Healing` in `system/regenerate.spite` is what `Healing` means there even when the program's entry
  class is `Healing`, and a program's class `Entry` does not hide `Directory`'s union `Entry` (proposed by Claude,
  unconfirmed, 2026-09-24; `conformance/stage6/nearest_type`).
  From outside its class a declared type is named through its owner, generic arguments included
  (`List<Recipes.Cookbook.Buildable>`, `conformance/stage6/nested_type_from_outside`). **When an error prints
  two types that would read the same, both are named through their owners** (proposed by Claude, unconfirmed,
  2026-09-25): two classes' own `type Buildable` are different types, so the message reads "a
  List<Recipes.CookTask.Buildable> cannot be used where a List<Recipes.Cookbook.Buildable> is needed"
  (`diagnostics/same_named_types`).
  A generic class's constructor takes the walk too, class and arguments alike: `Asset.Pack<Asset.Texture>()`, or
  `Pack<Rule>()` from inside `game/` for `game/pack.spite` (proposed by Claude, unconfirmed, 2026-09-24;
  `conformance/stage6/namespaced_generics`). A generic constructor that cannot be made reports once -- "there is
  no generic class 'Asset.Pak' here: a name is looked up in this class's own namespace, its folder, each parent
  folder, then the whole program" -- and every later line that reads the value it would have made is not
  reported again, so one mistake is one error (`diagnostics/failed_constructor`).
- Every loaded root merges into the same namespaces. A second root with the same folder structure and file name **reopens** the
  class: this is how monkey patching and game mods work -- later `func`/`var` with the same name replaces the earlier one (in
  load order: the entry folder first, then loads in the order they were discovered), a `var`'s replacement type must match, and
  a new `func`/`var`/`enum`/`type` not seen before is simply added. An `enum` declared again gains the values it
  lists that it did not have, after its own, and a value it restates stays where it was; nothing removes one
  (D180, [Types](values_and_types.md#types)). Load order stays the rule even where a package would rather be
  configured by the program it is loaded into: such a package calls a function the program declares (a
  `build.base_folder()` in the program's `build.spite`), which is an error when missing unless the package
  supplies a default behind `has_function` (D155). **`Build` is the one exception** (D227): a field the program's
  own `build.spite` declares is not replaced by a loaded package's declaration of it, so the program decides its
  build and a package's field is only a default ([programs.md](programs.md#build-settings-build--implemented),
  `conformance/stage6/build_precedence`). The standard library (`library/`) is discovered
  before the program, so a program's own file reopens `Console`, `String`, `File` and the rest the same way.
  `Environment` is made to be reopened: a program's own `environment.spite` adds its settings to it (D76, [Program settings: `Environment`](programs.md#program-settings-environment--implemented)).
  Your foot to shoot. That includes `Spite.Class` (D7, [Functions of `Spite.Class`](reflection.md#functions-of-spiteclass-and-why-there-are-no-static-functions--partial)): reopening it changes what every class object answers, for the whole program.
- **The program's entry class is the one class nothing reopens**: a loaded `bundle/potion.spite` in the program
  `potion/` is "'Potion' is the program's entry class (potion/potion.spite), so no other file may be a class of
  that name: rename this file, or the program's folder and its entry file" (`diagnostics/entry_class_reopened`).
  A program file that reopens a class a loaded folder declares keeps every name the loaded folder lacks, but
  where both declare a name, the constructor included, the loaded folder's comes later and stays.
- A program's `spite/` folder may only reopen the `Spite` classes: a new class there is "'Spite' holds the
  standard library's own classes, so a file in a 'spite' folder may only reopen one of them: 'Spite.Gadget' is not
  one, and a new class of your own belongs in a namespace of your own" (`diagnostics/new_spite_class`), and
  `load "spite"` is "'spite' is reserved for the built-in Spite namespace: name the folder something else"
  (`diagnostics/reserved_spite_namespace`).
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
  write it without parentheses, 'load "package"'"), and so is `load` used as a value: "'load' is a keyword that
  starts a line of its own, like 'load "folder"': it is not a function, and it has no value"
  (`diagnostics/load_parentheses`, `diagnostics/load_as_value`). It stays reserved (D166): a function, variable
  or parameter named `load` is "'load' is a keyword and cannot name a function: give it a name that says what it
  loads, like 'load_texture'" (`variable` or `parameter` in place of `function`; `diagnostics/load_function`).
  The launcher's own lines use it the same way, with a `Build` field:
  `load "library/{build.target_operating_system}"`, `load build.program`. Compile time only: the line compiles to
  nothing (D177).
- `load` takes a literal string, so the compiler always knows every bundle; anything else (a variable, an
  expression) is "a program's 'load' names its folder with text, like 'load "engine"': the compiler reads every
  loaded folder before it compiles, so the folder cannot be computed". The text is a path relative to the folder
  of the file it is written in, or an absolute one (D227; proposed by Claude, unconfirmed): one starting with `/`,
  or with a drive letter, `D:/` or `D:\`. An absolute root is never also a namespace of the folder that loads it,
  and only its own folder's name is held to snake_case (`check.sh` builds a program that loads a folder by its
  absolute path). The compiler finds every `load` reachable from the entry file's own constructor at compile time (a `load` inside
  already-loaded code counts too), and records each root as a bundle (its name and whether the `load` that introduced it sits
  inside an `if`) in the program model, for dynamic libraries/lazy loading to build on later.
- **A `load` under an `if` is decided when compiling** (proposed by Claude, unconfirmed, 2026-09-25). The compiler
  follows the `load` lines of the entry file and of every file of a loaded root, in every function, relative to
  that file's folder, and folds the condition of each `if` that holds one: it may read `Build` fields through the
  class's `var build = Build()`, text, whole numbers, `true` and `false`, joined by `==`, `!=`, `and`, `or` and
  `not`. Only the taken branch's folders are loaded, so `if build.target_operating_system == "windows" { load
  "../plugins/windows" }` brings in the plugin on Windows alone, and a package can choose its own plugins the same way
  (`conformance/stage6/load_on_build`). A condition it cannot fold is an error, never a silent no-load: "this 'if'
  holds a 'load', so the compiler decides it, and it cannot decide this condition: ..."
  (`diagnostics/load_unknown_condition`), and so is a `load` inside a `while` or a `switch`, or a program `load` whose
  folder is not text. The `if` itself stays in the program, where `Build` folds it again. A loaded package cannot
  add `Build` fields that decide loads: the fields are read before the first package is. Compile time only (D177).
- `load` marks a **bundle boundary**, like an async import in webpack: each loaded root can become a separate dynamic library,
  tree shaking is computed per bundle, and a `load` inside an `if` is loaded lazily when that line runs.  **[planned: every
  bundle is linked statically into the one executable for now, and the `load` line itself compiles to nothing]**
- **A class can ask for its own source folder** (D228; the name `source_folder` provisional, the readings below
  proposed by Claude, unconfirmed): `class.source_folder()` in a class's function, `Name.source_folder()` for a
  class named statically, `$item_type.source_folder()` for a codegen type and `value.class.source_folder()` for a
  value whose class is known while compiling are folded to text: the absolute folder, with `/` separators, of the
  file that declares the class (the first file merged into it, so a mod's reopening does not move it). A program
  file's path is joined to the folder the compiler runs in, a library file's to the language's folder, and `.`
  and `..` are resolved. A `Spite.Class` the compiler cannot name answers the same text at run time from its class
  object, which holds it only in a program that asks that way (`conformance/stage6/source_folder`). It is the build
  machine's folder: a shipped program copies or cooks the files it needs instead. A class that declares its own
  `source_folder` function is called as usual.
- **A dependency is a repository pinned to a commit in the `load` line itself** (D38, D283; built): `load
  "github.com/mortaro/engine@a3f2c91"`, fetched by the ordinary compile, with no package manager, registry,
  lockfile or fetch step. The spelling and the readings below are proposed by Claude, unconfirmed, under D205:
  - A `load` whose text holds an `@` is a git load: the text before the last `@` is the repository, then the
    commit, then optionally `/` and a folder inside the repository, which is the root loaded (the repository's top
    folder when there is none). The folder may not start with `..` or be absolute: "'load "<text>"' names the
    folder '<folder>', which is not inside the repository".
  - The repository is local when it starts with `./`, `../`, `/`, `\` or a drive letter, and is then resolved
    like any `load` path, from the folder of the file the `load` is written in; it is a URL when it starts with
    `https://`, `http://`, `ssh://`, `git://` or `file://`, or when its first segment holds a `.` (a host,
    `github.com/...`, which is fetched as `https://github.com/...`). Anything else is "'load "<text>"' names no
    repository: '<repository>' is neither a URL nor a path starting with ./, ../, / or a drive letter", followed
    by both forms. A local repository must be the repository's own top folder: a folder inside one is "... which
    is the folder '<folder>' inside a repository: name the repository's own folder, and the folder inside it after
    the commit".
  - The commit is 7 to 40 lowercase hexadecimal digits, or "'load "<text>"' pins '<commit>', which is not a commit:
    a branch or a tag moves, and a pin must not". A load with no `@` whose text is a URL, or starts with a host
    and names no folder that exists, is "'load "<text>"' names a repository without its commit, and a load from a
    repository is pinned: write its commit after the repository". A load with no `@` of a local folder stays the
    ordinary folder load (D227). The pin is resolved with `git rev-parse --verify <commit>^{commit}`, and a name
    that resolves to a commit not starting with it (a branch or tag spelled in hexadecimal) is an error too.
  - **Fetching is the ordinary compile** (D283): the commit's files are written into the working folder's
    `.spite/git/<label>_<number>/<commit>/`, the label the repository's last path segment in snake_case (without
    `.git`), the number `(n * 31 + code) mod 1 000 000 007` from 7 over the codes of the repository's absolute path
    (lowercased when it has a drive letter) or its URL. From a local repository the compiler runs
    `git -C <repository> read-tree <commit>` and `git -C <repository> --work-tree=<copy> checkout-index --all
    --force` with `GIT_INDEX_FILE` pointing at a file of its own in `.spite/git/`, so the repository's working
    files, index, `HEAD`, branches and worktrees are untouched. From a URL it keeps a bare clone in
    `.spite/git/<label>_<number>/repository`, cloned at the first pin (`git clone --bare`) and fetched again
    (`+refs/heads/*` and `+refs/tags/*`) only when it lacks a pinned commit, and writes the files from it the same
    way. Last it writes `<commit>.files` beside the copy: the full commit, then every file of the copy, a `.spite`
    file with its length and two hashes of its text, any other with its size and modification time. A copy counts as fetched only once that file exists, so an interrupted fetch is
    redone; a compile that finds both runs no git. The compiler prints `fetched <repository>@<commit> into <folder>`
    to the error output when it fetches, as it prints `formatted <path>`.
  - **Every failure names the `load` line** (D244): `<file>:<line>: error: ...` for no `git` on the `PATH`
    ("'load "<text>"' fetches <repository> with git, and there is no 'git' on the PATH"), a local repository that
    does not exist or is not a repository, a URL git cannot clone (git's own first line quoted), a commit the
    repository does not hold after fetching ("<repository> has no commit <commit>"), a folder after the commit that
    the commit lacks, and a copy that git could not write.
  - **A pinned commit never changes silently**: each compile checks the copy against `<commit>.files` again, once
    per copy, and a file of the commit that is changed or removed, or a `.spite` file that was added, is "'<file>'
    is not what the commit <commit> of <repository> holds: a checkout is read-only, so a pinned commit never
    changes. Delete the folder '<copy>' to fetch it again". A file other than `.spite` that appears in the copy
    later (a cache a plugin writes beside its source at run time) is not the commit's and is left alone; the
    compiler reads only `.spite` files. Other files are compared by size and modification time rather than read,
    so a package's assets cost a directory listing per compile, not a read (proposed by Claude, unconfirmed).
  - **A copy is read-only in the compile too**: its files are never formatted ([compiler.md](compiler.md#formatting-before-compiling-and-spite-format)),
    its root is not held to snake_case (the folder is named by the commit; a folder after the commit is), and a
    `--hot-reload` build does not watch it ([repl.md](repl.md#live-reload---hot-reload)).
  - **Pins inside a fetched package**: a relative repository path in a file of a copy is resolved from the
    folder that file has in the repository the copy came from, so sibling repositories stay siblings; in a copy
    fetched from a URL it is "... fetched from a URL, and names a repository by a path on this machine". An
    ordinary `load` in a copy whose folder leaves the copy is "'load "<text>"' is written in
    <repository>@<commit> and leaves that repository".
  - **Two versions of one repository are two libraries** (D296, decided by Mortaro, 2026-09-30; **not built**):
    two pins of one repository that resolve to different commits are never unified and never an error. Each
    version's classes are its own, as if the two repositories were unrelated: a package's names resolve to the
    classes of the version it pinned, a value of one version's class is not the other's type, a singleton of one
    version is not the other's, and a reopening reopens the version its root pinned. Proposed by Claude,
    unconfirmed: both versions keep their dotted names in their own sources, and where one root must name both (a
    program pinning two commits of one repository directly) that is still an error, since a name there could mean
    either; `--final-classes` prints each version into its own folder. The duplicate code costs nothing because
    identical C functions are folded into one ([optimizations.md](optimizations.md#identical-functions-are-folded-into-one)).
    **As built** (D38's first rule, which D296 replaces), such pins are an error at the second: "'load "<text>"' pins <repository> at <commit>, and <file>:<line> ('load "..."') pins
    it at another commit: two commits of one repository would reopen each other's classes, so pin the same commit
    in both" (D38's version mismatch). Two spellings of one commit share nothing but agree.
  - Compile time only (D177): a git load costs what a folder load costs at run time, nothing
    (`check.sh` builds a program from a local repository pinned to a commit that is no longer its `HEAD`, from a
    second compile with no git work, with `--hot-reload`, and fails it on an unknown commit, without git on the
    `PATH` and after editing the copy).
- Because patching is dangerous to read, the toolchain writes a **final class** folder: every class after all codegen, with the
  winning function of every replacement, each preceded by a `#` comment naming the root it came from (and which roots it
  replaced) -- see `--final-classes` in [Command line](compiler.md#command-line). The language server reads it too.
  **[partial: the folder is written and recompiles, but no `#` comment names a root yet, and an instantiated Symbol
  codegen function and a used `List<T>`/`Dictionary<T>` helper signature do not appear -- only the source
  classes are declared with]**
- **What the compiler supplies is a reopening too** (D82, decided by Mortaro: "spite writes that function as if it
  was reopening that class, so in --final-classes it should show the actual content of that class"). The members
  whose bodies the compiler supplies -- `Console`'s raw writes, `Memory.Heap`'s allocation and `Memory.Address`'s
  reads, writes and atomics (D178), `DynamicLibrary`'s opening and symbol lookup, `TypedMemory`'s typed slots, a
  number's casts (`from_type` as built; the source's `to_<type>()` once D275/D293 are built) and bit operations, the REPL's hooks on `Spite.Attribute`/`Spite.Function`, `HotReload`'s
  build facts, a `Concurrent`'s state machine (D176), `ThreadPool`'s entry address and `Scheduler`'s step -- are
  declarations the compiler merges into their classes right after `library/` (and its operating system's
  folder), exactly as a later root merges a file, so a program can still reopen them. Nothing is registered by
  hand (D81). **The form** (proposed by Claude, unconfirmed): **a `func` with a
  signature and no block** is a member whose body the compiler supplies, which is how `--final-classes` prints it
  (`func allocate(bytes: Long): Memory.Address`), and reading that back is what lets the printed program compile. It
  borrows the shape a `type` already uses for a member without a body. Anywhere the compiler supplies nothing by
  that name, a bodiless `func` is an error ("give it a body"), so it is not a way to declare anything else. The
  compiler's own reopening is Spite source in `bootstrap/source/generation/prelude.spite`, beside the C each body
  is; a body the compiler writes per instantiation (`TypedMemory<Integer>`, a number cast) is written by the
  generator. Supplied names skip the naming lint (`read_integer` names the type `Integer`), as conversions already did.
  **D147 (decided by Mortaro) rejects this as the end state**: no hidden code, no bodiless supplied functions and
  nothing tying Spite to C, so every supplied body is to become explicit Spite -- a library function called from a
  real body, or one of the few machine operations D178 names, which each backend lowers.  **[partial: the bodies
  above are still C or generator-written, and printed bodiless]**
