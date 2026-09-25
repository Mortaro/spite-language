# Packages, namespaces, and mods

There are no imports. Everything lives in one global namespace, populated by loading folders:

```gdscript
func Game() {
    load("package")
    load("cookie_clicker")
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

## Monkey patching (mods)

Every loaded root merges into the same namespaces. A second root with the same folder structure and file name
**reopens** the class instead of colliding with it: a later `func`/`var` of the same name replaces the
earlier one (in load order), and a name not seen before is simply added. A `union` or `type` declared again
replaces the earlier declaration the same way. This is how game mods work.

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
    load("package")
    load("mods")
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
standard library's own classes, and a class of your own belongs in a namespace of your own. `load("spite")` is
a diagnostic too:

```gdscript title=spite_namespace_error/spite_namespace_error.spite entry error
func SpiteNamespaceError() {
    load("spite")
}
```
```diagnostic
'spite' is reserved for the built-in Spite namespace
```

## `load` is a bundle boundary

`load` marks where a dynamic library or lazy-loaded bundle could split, the way an async `import()` does in
webpack. Today every root is linked into the one executable, and a `load(...)` call compiles to nothing at run
time -- except the launcher's `load(build.program)`, which runs the program by constructing its entry class.
`load` is a reserved word: it always loads a package, so a function named `load` is an error that asks for a name
saying what it loads:

```gdscript title=load_reserved/load_reserved.spite entry error
var console = Console()

func LoadReserved() {
    var loaded = load_texture("grass")
    console.print(loaded)
}

func load_texture(name: String): String {
    return "texture {name}"
}

func load(name: String): String {
    return name
}
```
```diagnostic
'load' is reserved: it always loads a package, so a function cannot be named 'load'
```

Splitting bundles, and loading one lazily when a `load` inside an `if` runs, are decided but not built
([manual section 11](../manual.md#11-packages-namespaces-and-loading--partial)).

A dependency will be a git URL pinned to a commit in the `load` call itself --
`load("github.com/mortaro/engine@a3f2c91")` -- fetched by the ordinary compile, with no package manager, registry
or lockfile. That is decided and not built ([manual, decision D38](../manual.md#decision-log)).

## Final classes

Because patching is dangerous to read silently, `--final_classes=folder` writes every class as it ended up, after
every root is merged and every reopening resolved, as a program that runs the same as the one it was printed from
([compiler.md](compiler.md#inspect-merged-classes)). This is the file to read when you are not sure which mod won.
Which root supplied each declaration is not printed yet.
