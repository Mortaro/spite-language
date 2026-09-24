# Writing Spite: the one page to read first

Everything here is enforced by the compiler. When it rejects something it says what to write instead, reports
every error in one run as `path:line: error: message (in Class.function)`, and never warns: it errors or it is fine.
`manual.md` is the reference; this page is the working set.

Code blocks are fenced as `gdscript` only so GitHub colours them: GitHub has no Spite highlighter yet. Every
one of them is Spite, not GDScript -- write `.spite` files.

## Run it

```
spite program                           compile and run the folder program/
spite program --optimized               optimized build (a Build field)
spite program --debug_memory            print the allocation balance at the end
spite program --repl_port=4000          serve the REPL; spite connect 4000 --command="..." asks it
spite program --mode=c                  print the C instead
spite program -- --serve=true           the program's own arguments, read by Environment
bash check.sh                           the compiler still compiles itself, and every corpus passes
```

A program lives in its own folder, and `spite game` runs it: `launcher/launcher.spite` loads `library/`, then the
target system's folder of it, then `game/`, whose every sub folder is a
namespace (`game/engine/renderer/debug.spite` is `Engine.Renderer.Debug`; a file named like its folder is the
folder's own class). `load("folder")` inside a function loads another package; a file at the same namespace path
reopens the class: same-named functions and attributes replace, the rest are added.

## A file is a class

```gdscript
var name = ""
var health = 10
var target: Monster? = null
var friends = List<Monster>()

func Monster(starting_name: String) {
    name = starting_name
}

func hurt(amount: Int) {
    health = health - amount
}

func is_alive(): Bool {
    return health > 0
}
```

- The file name is the class name (`monster.spite` is `Monster`). A file holds declarations only, in this
  order: a `singleton` line, `generic $name` lines, enums, unions, types, variables, the constructor, then
  functions. Every `var` has a default value. A name starting with `_` is private to its class.
- The function named like the class is the constructor. Do not write an empty one: a class without a constructor
  is made from its defaults, and `func Monster() { }` is an error.
- A program is a folder, and it starts by constructing the class of the file named after the folder
  (`game/game.spite` is `Game`). That constructor takes no arguments: settings come from `Environment()` and
  `Build()` below, and `Arguments()` is the raw command line anywhere (`.count()`, `.get(0)`, and `.player` for
  `--player=value`, a `String?`).
- `return` is always written. The return type is written `(): Type`. No return type means it returns `Nothing`.
- Only `while` exists. There is no `for`, `break` or `continue`: an early exit is part of the loop's condition.
  Prefer the member templates below.
- There is no `self.`: members are read by name. `this` is the instance itself, for handing it to something
  (`registry.record(this)`); `this.name` is an error.
- One function per name, no overloading: an argument is cast to the parameter's type. The last parameter may be
  `...values: List<Type>`, and the caller writes the values one by one.
- A call is never passed straight into another call: compute it first into a named `var` and pass the name --
  `var token_text = source.slice(start, end)`, then `tokens.append(Token("number", token_text))`. Only a
  constructor may be an argument, one level deep (`Token("number", Text(token_text))` inside `append` is an
  error). The holes of a text are not arguments: `"{names.count()} names"` is fine.
- The two branches of an `if`/`else` never compute the same call: compute it once before the `if`.

## Names, comments, unused things

- `snake_case` for variables, attributes, parameters, functions and enum values; `PascalCase` for classes,
  enums, unions and types; never a single letter; never an abbreviation (`message` not `msg`, `index` not `idx`,
  `value` not `val`). The error names the word to write.
- A local or parameter that is never used is an error: remove it or name it `_name`. A `_name` that is used is an
  error too.
- A function body holds no empty lines: the blank line is where a second function wants to be, so name the part
  below it and call it. An `if` whose only statement is a bare `return` is an error too -- that is a
  precondition, and a precondition is written `assert condition`.
- A comment is one line, outside functions, and nothing but a link to a markdown heading:
  `# notes.md#why-this-exists` (relative to the entry file's folder; the file and the heading must exist).
  Anything else, including `//` and `/* */`, is an error. If the code already says it, do not write it.

## Values

- Numbers: `Int` (32 bit, the default), `Long`, `Tiny`, `Short`, `Byte`, `UnsignedShort`, `UnsignedInt`,
  `UnsignedLong`, `Float` (the default for decimals), `Double`. `Bool`. `String` (double quotes only).
- No cast syntax: the right side is cast toward the left. `"age {3}"` is `"age 3"`; `var total: Int = "12"` parses
  it; an `Int` plus a `Float` is an `Int`. A value that does not fit wraps.
- Everything that is not a number, a `Bool` or an enum value is a reference: passing, assigning and storing share
  the same object. `copy()` copies one level, `deep_copy()` all the way down. `drop()` runs when the last reference
  goes. Two objects that refer to each other leak: clear one side.
- `List<T>`: `[1, 2, 3]`, `append`, `prepend`, `insert`, `remove_at`, `remove_last`, `remove_first`, `first`,
  `last`, `count`, `contains`, `is_empty`, `clear`, `reverse`, `join` (text, numbers, `Bool` and enum values
  all join), `list[index]` (a `T?`: out of range gives nothing -- `crash names[index]` narrows it like a path,
  `crash names.count() == 3` proves `names[0]` to `names[2]`, and `while index < names.count()` proves
  `names[index]` in the loop body). `Dictionary<T>` (String keys, insertion order): `set`, `get` (a `T?`), `has`,
  `remove`, `count`, `keys`, `values`, `dictionary["key"]` (a `T?`, like `list[index]`).
- A chain of templates, `teams.filter_active().map_lead().sum_age()`, runs as one loop with no list in between.
- On a list or dictionary of a class: `filter_<member>()`, `count_<member>()`, `any_`, `all_` (a `Bool` member),
  `sum_<member>()` (a number), `sort_by_<member>()`, `find_by_<member>(value)` (a `T?`), `map_<member>()`,
  `each_<member>()` (a function). A member is an attribute or a function that takes nothing.
- `enum`, `union` and `type` declarations take no `=`, one entry per line, no commas:

  ```spite
  enum Job {
      'knight'
      'mage'
  }
  ```

- An enum's values are single quoted and resolve from where they are used.
- `union Enemy { Player Monster }`, written one member per line: `switch enemy { Player: ... Monster: { ... } }`
  must cover every member and narrows `enemy` inside each case; a function or attribute every member has can be
  used on the union directly.
- A `type` declares a shape -- `label: String` and `render(Int): String`, one per line, a required function
  naming the *types* it takes and never the names -- and accepts any class, or
  object literal `{ label: "x" }`, with those attributes and functions.

## Nothing, null, and failure

- `Monster?` is a value that may be `null`. It must be narrowed before use: `if target { }` (with `else`),
  `assert target`, `crash target`, `while target { }`, or `switch target { Monster: ... Null: ... }`. One
  `assert a.b.c` narrows the whole path. `null` is never compared against: `value == null` is an error.
  Narrow the name or the path itself -- `assert target`, `assert target.weapon` -- never a local copied from it,
  which is an error. Comparing needs no narrowing: `target.name == "rat"` needs `target` narrowed, but
  `maybe_name == "rat"` is simply false when it is null.
- A `switch` is over a union or a `T?` and covers every member; `_:` as the last case answers for the rest, and
  two cases doing the same thing are an error: write it once as `_:`. An enum is compared with `==`, not switched.
- `value == Monster` is a class test (false for `null`), and `if value == Monster { }` narrows `value` inside. A
  switch that is one class case and `_:`, each a `return`, is an error: write the `if`, or `return value == Monster`.
- There are no exceptions and no error values. Three outcomes only:
  - the compiler can know it: a compile error;
  - absence is fine: `assert condition` returns the function's default quietly (`false`, `0`, `""`, `null`,
    nothing), in a function returning any type, never in a constructor. `if x { return false }` -- an `if`
    whose body only returns the default -- is an error: write `assert not x`;
  - absence is a bug: `crash condition` halts with
    `spite.crash<TAB>id<TAB>path:line<TAB>Class<TAB>function<TAB>condition<TAB>name=value...`, followed by the
    asserts that failed before it. `crash false` marks a branch that cannot happen.
- A test is a function named `test_...` that crashes when wrong. `tests/tests.spite` finds them all by itself.

## Metaprogramming

- A `Symbol` parameter named inside its function's name answers every attribute:
  `func set_attribute(attribute: Symbol, value: attribute.class) { attributes[attribute] = value }` makes
  `person.set_age(2)` and `person.set_name("x")` work. An exact function always wins.
- `attribute: Symbol<Label>` ranges over another class's members, read as `label.attributes[attribute]`, and the
  plural (`show_attributes(label)` for `show_attribute`) calls the template once per attribute, in order.
- In a generic class, `if $value_type == List { }` (also `Dictionary`, `Null` for any `T?`, `Symbol` for any enum,
  or an exact type) is decided while compiling, and `$value_type.element_type` names what the type holds.
- A getter with no setter makes a read-only attribute: `get_fahrenheit()` answers `.fahrenheit`, and assigning
  it is an error.
- `person.age = 1` calls `set_age(1)` and `person.age` calls `get_age()` when the class has them.
- Operators are functions a class may define: `sum`, `subtract`, `multiply`, `divide`, `remainder`, `equals`,
  `less_than`, `greater_than`, `negate`, `get_at(index)`, `set_at(index, value)`. Without `equals`, `==` compares
  identity.
- Codegen values: `generic $damage_type` and `generic $is_magic`, one per line at the top of `weapon.spite`, and
  `Weapon<Int, true>(10)` supplies them in that order; `Pair("a", 1)` may leave them out when the constructor's
  arguments say them. The constructor lists none: `func Weapon(damage: $damage_type)`. `$` is for generics only: a
  `$name` with no `generic` line is an error. `if $is_magic { }` is decided at compile time.
- Settings: reopen `Environment` in the program's `environment.spite` with one `var` per setting and a literal
  default (`var serve = false`), then read `Environment().serve` anywhere. The value comes from `--serve=true`
  after `--` on the command line, else the `SERVE` environment variable, else the default.
- Build settings: reopen `Build` in `build.spite` the same way. A `Build` field is decided when compiling --
  `spite game --serve=true`, else its default -- and is a constant in the program, so `if Build().serve { }`
  keeps only one branch. The compiler's own options (`mode`, `optimized`, `debug_memory`, ...) and
  `Build().target_operating_system` are `Build` fields too.
- Reflection: `value.class` (a `Spite.Class`: `.name`, `.namespace` (a `Spite.Namespace?` -- narrow it before
  reading its members: `assert value.class.namespace` narrows the path itself and its prefixes for the rest of
  the block -- `.name_with_namespaces`, `.parent`, `.classes`,
  `.namespaces`), `.functions`), `value.attributes`
  (`.name`, `.class`, `.value`), `value.functions` (`.name`, `.arguments`, `.returns`, `call_function()` for
  functions that take nothing and return `Nothing`), a function named without calling it (`shouter.shout`, a
  `Spite.Function<String, String>` bound to `shouter`, called as `change(text)`), `Monster.instances` (live instances), and
  `Spite.Class.instances` (every class of the program). `class`, bare inside a class's function, is the class
  of the instance it answers on, and a class name reads its own class object: `Monster.name` is `"Monster"`.
- A class whose file starts with a `singleton` line has one instance: `Journal()` always returns it, and its
  constructor takes no arguments.
- `value.memory` is where a named value lives (`.address`, `.bytes`, `.section`: `'heap'`, `'stack'`,
  `'constant'`). A container of your own is a generic class over `Memory` (`allocate_bytes`, `resize`, `free`,
  `read_long`/`write_long`, ...; the compiler places each allocation) and a `TypedMemory<$value_type>` (`read_value`,
  `write_value`, `release_value`, `value_bytes`), exactly as `library/list.spite` is.

## Built in classes

`Console()` (`print`, `write`, `error`, `read_line(): String?`; each value printed is its `to_string()`, so a class
prints once it declares `func to_string(): String`), `File(path)` (`read(): String?`, `write`,
`append`, `exists`, `remove`), `Directory(path)` (`path`, `entries(): List<Directory.Entry>` -- each a `Directory` or a `File`, switched on --,
`files`, `folders`, `exists`, `create`),
`Process(command, arguments)` (`run(): Int`, `output()`), `Program()` (`exit(code)`, `sleep(milliseconds)`,
`environment(name): String?`). `Console` is a singleton: `Console()` is the same instance everywhere.
`Concurrent(function)` runs a function on a fiber and `Parallel(function)` on a thread: `.wait()` answers what it
returned, the type is never written, and dropping the handle waits for it. There is no `async`/`await`: a function
that reads, sleeps or waits is an ordinary function, and the compiler suspends it there when something else can
run ([concurrency.md](concurrency.md)).
`Json<T>()` (`write(value): String`, `read(text): T?`, `read_or_crash(text): T`) converts any class, list,
dictionary, enum, number, `Bool`, `String` or `T?` to JSON and back; `read` skips unknown keys, keeps defaults for
missing ones, and is `null` on a value of the wrong kind (docs/json.md).
`DynamicLibrary("ucrtbase.dll", 'identity', "")` calls a native library's functions as members
(`c_runtime.strlen(text)`, `_as_long`/`_as_double`/`_as_text` for wider results); the standard library's
`library/windows/`, `linux/` and `mac/` folders reopen the classes each system changes (docs/foreign_libraries.md).
Every class here, the numbers and `List` included, is a Spite file in `library/`, and a program's own file of the
same name reopens it: `list.spite` adds a member template, `int.spite` a function on every `Int`.

## Habits from other languages that Spite rejects

Each of these is a compile error naming the Spite form, so none of them will pass silently -- but they cost a
round trip, and this list is cheaper to read than to rediscover.

| Written elsewhere | Written in Spite |
|---|---|
| `a && b`, `a \|\| b`, `!a` | `a and b`, `a or b`, `not a` |
| `count++`, `count += 1` | `count = count + 1` |
| `condition ? a : b` | an `if` with an `else`, or a function that returns one or the other |
| `new Monster()` | `Monster()` |
| `this.name`, `self.name` | `name` |
| `import`, `require` | `load("folder")`, inside a function |
| `elif` | `else if` |
| `class Monster { }` | nothing: the file *is* the class |
| `func greet(name: String = "world")` | a second function, or an attribute holding the value |
| `print(value)` | `var console = Console()` at file level, then `console.print(value)` |
| `toString()`, `__str__`, `Display` | `func to_string(): String` in the class, which `console.print` calls; inside text, write `"{value.to_string()}"` |
| `"hello ${name}"`, `"hello " + name` | `"hello {name}"` |
| `for item in list` | `while index < list.count()`, or `map_`/`filter_`/`each_<member>()` |
| `value == null` | `if value { } else { }`, `assert value`, `crash value`, or `switch` |
| `text[0]` | `text.character_at(0)`, or `text.slice(start, end)` |
| `// comment`, `/* comment */` | nothing, or `# docs/page.md#section` on its own line outside a function |
| a variable named `int`, `static`, `unsigned`, `stdout` | any name that is not reserved in C |

Text is written with its values inside it: `"hello {name}"`, where `{ }` holds one value of any type and
`\{` is a brace meant literally. Joining written text with `+` is an error; two values still join with `+`.
