# Packages, namespaces, and mods

The specification of [Packages, namespaces, and mods](../docs/packages.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Packages, namespaces and loading

There are no imports. Everything lives in one global namespace, populated by loading folders (`load "package"`).

- The loaded folder is a package root and does **not** appear in the namespace. Folders inside it do:

```
package/engine/renderer.spite            ->  Engine.Renderer()
package/engine/physics/body.spite        ->  Engine.Physics.Body()
package/engine/physics/physics.spite     ->  Engine.Physics.Physics()   (no file is special: every folder is a namespace)
```

- **Every folder is a namespace and nothing else; no file is special.** A file named like its folder is an ordinary
  class inside the folder's namespace (`Engine.Physics.Physics`), reopened by a file of the same path in a later
  root like any other class (`conformance/stage6/folder_namesake`).
- **A class and a namespace never share a dotted name.** A file and a folder of one name side by side,
  `engine/renderer.spite` beside `engine/renderer/`, is "engine/renderer.spite: error: 'Engine.Renderer' is this
  class and also the namespace of the folder 'engine/renderer': a dotted name means one thing, so rename the file
  or the folder" (`diagnostics/class_and_namespace`). Every root merges into the same namespaces, so a file in one
  root and a folder in another clash the same way, and a folder named like a class of the standard library is
  "'Color' is a class of the standard library and also the namespace of the folder 'game/color': a dotted name
  means one thing, so rename the folder". A class may not hide another class either
  ([A class name means one class](../docs/classes_and_files.md#a-class-name-means-one-class)).

- `spite game` **loads the program's folder as a root** (a program is named by its folder): the program's
  folder is a real root exactly like a `load`-ed one, so every subfolder inside it is a namespace, recursively,
  with no explicit `load` needed (a swappable plugin therefore lives beside the program, not inside it).
  The difference from a `load`-ed root: a subfolder that a file at the top of the program's folder explicitly
  `load`s is left to that line (a `load`-ed root's own folder is never itself a namespace segment). Every file of
  the program's folder has its `load`s followed, the entry file's first, like every file of a loaded root, so a
  sibling file's `load "tools"` loads `tools/` as a package root (a `load` works in any file, and the entry
  file only names what runs first; `conformance/stage6/sibling_load`). The launcher class's own `load`s, printed
  into a `--final-classes` folder, are the launcher's and are not followed from there.
- **No file is loaded twice.** Two `load`s whose folders overlap, one inside the other, would reach the same files
  twice, and that is a compile error at the later one: "game.spite:5: error: 'load \"kingdom/physics\"' and the
  load at game.spite:4 both reach the files of 'kingdom/physics', which lies inside 'kingdom', so those files would
  be loaded twice: load only 'kingdom', whose subfolders are already its namespaces" (`diagnostics/overlapping_load`).
  Loading the same folder again is not an overlap: two packages that both load a third load it once, however their
  paths spell it. The one nesting that is allowed is a package's own `load` of a folder inside its own tree, below,
  since that folder is then taken out of the package and loaded on its own.
- **A folder named by a `load` inside its own tree is never also a namespace.** A package `kitchen/` whose file
  loads `"garnish/pepper"` gets `kitchen/garnish/pepper/` as a root of its
  own, and not also as the namespace `Garnish.Pepper`, as the program's own folder already did for what its top
  files load; the same holds for a file in any folder of the program. Every `load` written in the package counts,
  including one under an `if` the build decides the other way, so `kitchen/garnish/sugar/` is neither a root nor a
  namespace then. The compiler finds them by reading the package's `load` lines before it walks its folders
  (`diagnostics/loaded_folder_namespace`, `conformance/stage6/load_on_build`). `Build` is still read before any
  package is loaded, so a package's `build.spite` cannot add a field that decides a `load`.
- **Folder names are always lowercase snake_case, package roots included**: `window_plugin`, never
  `window-plugin`, for every folder that actually contains a `.spite` file anywhere inside it (an unrelated
  folder with none, such as a build output directory, is never descended for classes, and `.git`, `.spite` and
  `.spite-cache` are never walked at all). A folder that
  is not is an error before anything compiles, naming the snake_case to rename it to: "the folder 'BadFolder' is
  not named in snake_case: every folder of a program, a package's own folder included, is lowercase words joined
  by '_', so rename it 'bad_folder'" (`diagnostics/folder_name`, and `diagnostics/load_folder_name` for `load
  "Window-Plugin"`). What is checked: the program's own folder, every folder
  inside it and inside a loaded root, and a loaded root's own folder, not the folders above them that a path
  such as `../../plugins/render_vulkan` passes through, which are not part of the program. snake_case is a
  lowercase letter, then lowercase letters, digits and single `_`, not ending in `_`.
- Resolving an unqualified `Name` from inside a class tries, in order: that class's own namespace (so a nested enum/type/union
  resolves by its plain name from inside its own class), the same folder's namespace, each parent folder's namespace, then the
  whole program globally. Ambiguity *between roots* at the same level is never an error, because of the next rule.
  A dotted name (`Component.Requested`) takes the same walk, in every position a name is written: constructor
  call, parameter, return type, attribute or local annotation, generic argument, `type`/`union` member, class test
  and a class's enum (`conformance/stage6/relative_namespaces`).
  A class's own namespace holds only what it declares (its `enum`, `union` and `type`), and its folder holds its
  sibling classes and folders: `click_test/click_test.spite` (`ClickTest.ClickTest`) reaches
  `click_test/system/verify.spite` as `System.Verify()` (`conformance/stage6/folder_class_namespace`).
  A name the walk does not find is `unknown type 'Server.Component.Eye'` at the line that writes it, in every
  position above, a `type`'s attribute included; when dropping its leading parts names a class, or a plain name
  is the last part of exactly one class, the error says which: `unknown type 'Server.Component.Eye': did you mean
  'Component.Eye'?`. The usual slip is an environment's folder written into the name, when the folder joins
  the program's own namespaces (`diagnostics/unknown_type_in_shape`, `diagnostics/unknown_type_suggestion`).
  A `type`, `union` or `enum` sits in the walk at the level of the class that declares it, so the nearest
  declaration wins: one in the using class before any class, so `type Healing` in `system/regenerate.spite` is
  what `Healing` means there even when the program's entry class is `Healing`, and a program's class `Entry` does not hide `Directory`'s union `Entry`
  (`conformance/stage6/nearest_type`).
  From outside its class a declared type is named through its owner, generic arguments included
  (`List<Recipes.Cookbook.Buildable>`, `conformance/stage6/nested_type_from_outside`). **When an error prints
  two types that would read the same, both are named through their owners**: two classes' own `type Buildable`
  are different types, so the message reads "a
  List<Recipes.CookTask.Buildable> cannot be used where a List<Recipes.Cookbook.Buildable> is needed"
  (`diagnostics/same_named_types`).
  A generic class's constructor takes the walk too, class and arguments alike: `Asset.Pack<Asset.Texture>()`, or
  `Pack<Rule>()` from inside `game/` for `game/pack.spite` (`conformance/stage6/namespaced_generics`).
  A generic constructor that cannot be made reports once, "there is
  no generic class 'Asset.Pak' here: a name is looked up in this class's own namespace, its folder, each parent
  folder, then the whole program", and every later line that reads the value it would have made is not
  reported again, so one mistake is one error (`diagnostics/failed_constructor`).
- Every loaded root merges into the same namespaces. A second root with the same folder structure and file name **reopens** the
  class, and this is how monkey patching and game mods work: a later `func`/`var` with the same name replaces the earlier one (in
  load order: the entry folder first, then loads in the order they were discovered), a `var`'s replacement type must match, and
  a new `func`/`var`/`enum`/`type` not seen before is simply added. An `enum` declared again replaces the
  earlier declaration whole, exactly as a function does: its values are the later list, in the later order, and
  a value only an earlier declaration listed is not a value any more ([Types](values_and_types.md#types)). A
  `--hot-reload` reload replaces it the same way. Load order stays the rule even where a package would rather be
  configured by the program it is loaded into: such a package calls a function the program declares (a
  `build.base_folder()` in the program's `build.spite`), which is an error when missing unless the package
  supplies a default behind `Build.functions['base_folder']`. **`Build` is the one exception**: a field the program's
  own `build.spite` declares is not replaced by a loaded package's declaration of it, so the program decides its
  build and a package's field is only a default ([programs.md](programs.md#build-settings-build),
  `conformance/stage6/build_precedence`). The standard library (`library/`) is discovered
  before the program, so a program's own file reopens `Console`, `String`, `File` and the rest the same way.
  `Environment` is made to be reopened: a program's own `environment.spite` adds its settings to it ([Program settings: `Environment`](programs.md#program-settings-environment)).
  Your foot to shoot. That includes `Spite.Class` ([Functions of `Spite.Class`](reflection.md#functions-of-spiteclass-and-why-there-are-no-static-functions)): reopening it changes what every class object answers, for the whole program.
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
- **`Spite.Internal` is for the standard library alone.** The namespace holds what a program must not use directly.
  Any file outside `library/` and the compiler's own sources that names it is refused: a type, a call, an
  inheritance, a class written in a `spite/internal/` folder (new or reopening one), or the name inside a string
  interpolation. The message names the class written (or `Spite.Internal` alone when no class follows) and the plain
  form to use: "Spite.Internal.Memory is internal to Spite and cannot be used outside the standard library: read
  bytes with List<Byte> or BinaryReader, describe foreign data with a plain type, and leave threads to the compiler"
  (`diagnostics/internal_use`, `diagnostics/internal_class`). Inside `library/` it is ordinary code. A program that
  does not use the namespace carries none of it.
- **Each operating system reopens the classes it changes.** `library/` holds what every system shares, and
  `library/windows/`, `library/linux/` and `library/mac/` hold only what differs: `library/linux/file.spite` reopens
  `File` with the functions that call `libc.so.6`, `library/windows/file.spite` the same functions over
  `ucrtbase.dll`, and so on for `Directory`, `Process`, `Program`, `Console`, `String` (`to_double`) and
  `Build` (`target_operating_system`). There is no wrapper class in between: a platform file holds its own
  `DynamicLibrary("libc.so.6", 'identity', "")`, which is one shared instance per literal argument list, and
  a one-line foreign call two classes both need is written in both. The launcher ([Constructors and the entrypoint](programs.md#constructors-and-the-entrypoint)) loads `library/`
  first and then the one folder named by `build.target_operating_system`; that folder adds no namespace segment,
  and its files are read after `library/`'s own, so they replace or add members by the reopening rule above. `--final-classes`
  prints each class as it came out, platform functions included. Because exactly one folder is loaded,
  `library/file.spite` calls `open_file` without declaring it: every system's folder defines it, with the same
  signature.
- **`load` is a keyword, written without parentheses**: `load "package"`, on a line of its own inside a
  function. `load("package")` is a parse error naming the keyword form ("'load' is a keyword, not a function:
  write it without parentheses, 'load "package"'"), and so is `load` used as a value: "'load' is a keyword that
  starts a line of its own, like 'load "folder"': it is not a function, and it has no value"
  (`diagnostics/load_parentheses`, `diagnostics/load_as_value`). It stays reserved: a function, variable
  or parameter named `load` is "'load' is a keyword and cannot name a function: give it a name that says what it
  loads, like 'load_texture'" (`variable` or `parameter` in place of `function`; `diagnostics/load_function`).
  The launcher's own lines use it the same way, with a `Build` field:
  `load "library/{build.target_operating_system}"`, `load build.program`. Compile time only: the line compiles to
  nothing.
- `load` takes a literal string, so the compiler always knows every bundle; anything else (a variable, an
  expression) is "a program's 'load' names its folder with text, like 'load "engine"': the compiler reads every
  loaded folder before it compiles, so the folder cannot be computed". The text is a path relative to the folder
  of the file it is written in, or an absolute one: one starting with `/`,
  or with a drive letter, `D:/` or `D:\`. An absolute root is never also a namespace of the folder that loads it,
  and only its own folder's name is held to snake_case. The compiler finds every `load` reachable from the entry
  file's own constructor at compile time (a `load` inside
  already-loaded code counts too), and records each root as a bundle (its name and whether the `load` that introduced it sits
  inside an `if`) in the program model, for dynamic libraries and lazy loading to build on.
- **A `load` under an `if` is decided when compiling.** The compiler
  follows the `load` lines of the entry file and of every file of a loaded root, in every function, relative to
  that file's folder, and folds the condition of each `if` that holds one: it may read `Build` fields through the
  class's `var build = Build()`, text, whole numbers, `true` and `false`, joined by `==`, `!=`, `and`, `or` and
  `not`. Only the taken branch's folders are loaded, so `if build.target_operating_system == "windows" { load
  "../plugins/windows" }` brings in the plugin on Windows alone, and a package can choose its own plugins the same way
  (`conformance/stage6/load_on_build`). A condition it cannot fold is an error, never a silent no-load: "this 'if'
  holds a 'load', so the compiler decides it, and it cannot decide this condition: ..."
  (`diagnostics/load_unknown_condition`), and so is a `load` inside a `while` or a `switch`, or a program `load` whose
  folder is not text. The `if` itself stays in the program, where `Build` folds it again. A loaded package cannot
  add `Build` fields that decide loads: the fields are read before the first package is. Compile time only.
- `load` marks a **bundle boundary**, like an async import in webpack: each loaded root can become a separate dynamic library,
  tree shaking is computed per bundle, and a `load` inside an `if` is loaded lazily when that line runs.
- **A class can ask where it comes from**: `class.source_files`, `Name.source_files`, `$item_type.source_files` and
  `value.class.source_files` of a value whose class is known while compiling are folded to a `List<File>`: every file
  that declares or reopens the class, in load order, each an absolute path with `/` separators; a namespace's
  `source_directories` is a `List<Directory>` the same way. A question about a path is asked of those values. A program
  file's path is joined to the folder the compiler runs in, a library file's to the language's folder, and `.`
  and `..` are resolved. A `Spite.Class` the compiler cannot name answers the same at run time from its class object,
  which holds them only in a program that asks that way. They are the build machine's paths: a shipped program copies
  or cooks the files it needs instead.
- **A dependency is a repository pinned to a commit in the `load` line itself**: `load
  "github.com/example/engine@a3f2c91"`, fetched by the ordinary compile, with no package manager, registry,
  lockfile or fetch step. The readings:
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
    ordinary folder load. The pin is resolved with `git rev-parse --verify <commit>^{commit}`, and a name
    that resolves to a commit not starting with it (a branch or tag spelled in hexadecimal) is an error too.
  - **Fetching is the ordinary compile**: the commit's files are written into the working folder's
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
  - **Every failure names the `load` line**: `<file>:<line>: error: ...` for no `git` on the `PATH`
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
    so a package's assets cost a directory listing per compile, not a read.
  - **A copy is read-only in the compile too**: its files are never formatted ([compiler.md](compiler.md#formatting-before-compiling)),
    its root is not held to snake_case (the folder is named by the commit; a folder after the commit is), and a
    `--hot-reload` build does not watch it ([repl.md](../docs/repl.md#live-reload---hot-reload)).
  - **Pins inside a fetched package**: a relative repository path in a file of a copy is resolved from the
    folder that file has in the repository the copy came from, so sibling repositories stay siblings; in a copy
    fetched from a URL it is "... fetched from a URL, and names a repository by a path on this machine". An
    ordinary `load` in a copy whose folder leaves the copy is "'load "<text>"' is written in
    <repository>@<commit> and leaves that repository".
  - **Two versions of one repository are two libraries**: two pins of one repository that resolve to different
    commits are never
    unified and never an error. A package is a pinned copy (every folder of it), or the program (its own folder
    and every folder it loads without a pin). Each version's classes are its own: a name written in a package
    resolves, at each step of the namespace walk, first as it does without versions and then in each version the
    package pinned; a value of one version's class is not the other's type, a singleton of one version is not
    the other's, and a file outside a version that has the dotted name of one of its classes reopens the version
    its package pinned. A class of the standard library a version reopens stays the one class. **One package
    pinning two commits** of one repository reads them as two ordinary loads into one namespace, the later
    reopening the earlier under the load-order rule; together they are that package's version, spelled by its
    first commit. A commit is read once, into one version, so a commit one package reads alongside another and
    a second package reads into a different version is "'load "<text>"' reads <repository> at <commit> as part
    of its package's version from <commit>, and <file>:<line> ('load "..."') reads that commit as part of the
    version from <commit>: a commit's files are read once, into one version, so pin it the same way in both
    packages".
    A name, or a reopening, that only the versions hold in a package that pinned none of them is an error listing
    every version as "<repository>@<commit>, pinned by <file>:<line> ('load "..."')". A version's classes are
    spelled `<repository name>_<commit>.<dotted name>` in messages, reflection and `--final-classes`, which
    prints each version into its own folder; the commit is 7 digits, more when two versions share them. Two
    spellings of one commit share nothing but agree. Built by discovering the program twice when a repository is
    pinned at two commits: the first pass learns which package pins what, the second puts each version's
    classes under its own name. The duplicate code costs nothing because identical functions are folded into one
    ([optimizations.md](../docs/optimizations.md#identical-functions-are-folded-into-one)).
  - Compile time only: a git load costs what a folder load costs at run time, nothing.
- Because patching is dangerous to read, the toolchain writes a **final class** folder: every class after all codegen, with the
  winning function of every replacement, each preceded by a `#` comment naming the root it came from (and which roots it
  replaced); see `--final-classes` in [Command line](compiler.md#command-line). The language server reads it too.
- **What the compiler supplies is a reopening too.** The members whose bodies the compiler supplies are `Console`'s
  raw writes, `Memory.Heap`'s allocation and `Memory.Address`'s reads, writes and atomics, `DynamicLibrary`'s opening
  and symbol lookup, `TypedMemory`'s typed slots, a number's casts (the source's `to_<type>()`) and bit operations,
  the REPL's hooks on `Spite.Attribute`/`Spite.Function`, `HotReload`'s build facts, a `Concurrent`'s state machine,
  `ThreadPool`'s entry address and `Scheduler`'s step. They are declarations the compiler merges into their classes
  right after `library/` (and its operating system's folder), exactly as a later root merges a file, so a program
  can still reopen them, and `--final-classes` shows the actual content of the class. Nothing is registered by hand.
  **The form:** **a `func` with a signature and no block** is a member whose body the compiler supplies, which is how
  `--final-classes` prints it (`func allocate(bytes: Long): Memory.Address`), and reading that back is what lets the
  printed program compile. It borrows the shape a `type` already uses for a member without a body. Anywhere the
  compiler supplies nothing by that name, a bodiless `func` is an error ("give it a body"), so it is not a way to
  declare anything else. Supplied names skip the naming lint (`read_integer` names the type `Integer`). No supplied
  body is hidden code or ties Spite to C: each is explicit Spite, a library function called from a real body, or one
  of the few machine operations that each backend lowers.

---

Next: [Concurrency: waiting without colouring](concurrency.md).
