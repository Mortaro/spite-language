# Packages, namespaces, and mods

There are no imports. Everything lives in one global namespace, populated by loading folders:

```
func Game() {
    load("package")
    load("cookie_clicker")
}
```

- The loaded folder itself does **not** appear in the namespace; folders *inside* it do. A file named like its
  folder is that folder's entry point: `package/engine/renderer/renderer.spite` becomes `Engine.Renderer()`,
  and `package/engine/renderer/debug.spite` (a sibling file in the same folder) becomes
  `Engine.Renderer.Debug()`.
- `load` only ever takes a literal string -- a variable or expression there is a diagnostic, so the compiler
  always knows every bundle statically.
- Folder names are always lowercase `snake_case`, checked for every folder that actually contains a `.spite`
  file anywhere inside it.
- Resolving a bare `Name` from inside a class tries, in order: that class's own namespace, its folder, each
  parent folder, then the whole program.

## Monkey patching (mods)

Every loaded root merges into the same namespaces. A second root with the same folder structure and file name
**reopens** the class instead of colliding with it: a later `func`/`var` of the same name replaces the
earlier one (in load order), and a name not seen before is simply added. This is how game mods work.

```spite title=package_demo/package/monster.spite
var health = 10

func Monster(starting_health: Int) {
    health = starting_health
}

func describe(): String {
    return "a wild monster"
}
```
```spite title=package_demo/package/engine/renderer/renderer.spite
func render(): String {
    return "rendering the scene"
}
```

`mods` is loaded after `package`, so the `describe` below replaces `package/monster.spite`'s own, and `taunt`
is a name that folder never had. That is the whole of monkey patching.

```spite title=package_demo/mods/monster.spite
func describe(): String {
    return "a modded monster"
}

func taunt(): String {
    return "the modded monster taunts you"
}
```
```spite title=package_demo/package_demo.spite entry
var console = Console()

func PackageDemo() {
    load("package")
    load("mods")
    var renderer = Engine.Renderer()
    var rendered = renderer.render()
    console.print(rendered)
    var monster = Monster(10)
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
A `var`'s replacement must keep the same type; the built-in classes (`Console`, `String`, ...) cannot be
reopened at all, and get their own clear diagnostic instead. `List<T>` is Spite in `library/list.spite`, and a
program's own `list.spite` reopens it to add a member template (see
[standard_library.md](standard_library.md#write-your-own-member-template)). Your foot to shoot with everything
else.

## The `Spite` namespace is reserved

`Spite` is the root namespace for reflection (`Spite.Class`, `Spite.Attribute` -- see
[metaprogramming.md](metaprogramming.md)). A `spite/` folder of your own **reopens** those classes, which is
how a package is tried out before it is upstreamed. Inside the class, a reopening reads the private fields
(`_name`, `_namespace`) that the read-only `name` and `namespace` answer from outside (D88):

```spite title=reopen_spite_class/spite/class.spite
func name_with_namespaces(): String {
    if _namespace {
        return "{_namespace.name_with_namespaces}.{_name}"
    }
    return _name
}
```
```spite title=reopen_spite_class/reopen_spite_class.spite entry
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

```spite title=spite_namespace_error/spite_namespace_error.spite entry error
func SpiteNamespaceError() {
    load("spite")
}
```
```diagnostic
'spite' is reserved for the built-in Spite namespace
```

## `load` is a bundle boundary

`load` marks where a dynamic library or lazy-loaded bundle could split, the way an async `import()` does in
webpack -- tree shaking is computed per bundle, and a `load` inside an `if` loads lazily when that line runs.
**[planned]**: every bundle is linked statically into the one executable for now, and the `load(...)` call
itself compiles to nothing at runtime.

## Final classes

Because patching is dangerous to read silently, `--final-classes[=folder]` writes one `.spite` file per class,
after every `load`-ed root is merged and every reopened class resolved to its winning declarations -- each
replaced declaration preceded by a `#` comment naming the source file it came from. Bare `--final-classes`
defaults to `.spite-cache/final/`; `--final-classes=folder` writes there instead. This is the file to read when
you are not sure which mod actually won for a declaration. See [compiler.md](compiler.md) for every command-line
option.
