# Writing Spite: the one page to read first

Everything here is enforced by the compiler. When it rejects something it says what to write instead, reports
every error in one run as `path:line: error: message (in Class.function)`, and never warns: it errors or it is fine.
`manual.md` is the reference; this page is the working set.

Code blocks are fenced as `gdscript` only so GitHub colours them: GitHub has no Spite highlighter yet. Every
one of them is Spite, not GDScript -- write `.spite` files.

## Run it

```
spite program                           build program/program.exe beside it and run it
spite program --optimized               optimized build (a Build field)
spite program --debug-memory            print the allocation balance at the end
spite program --repl-port=4000          serve the REPL; spite connect 4000 --command="..." asks it
spite program --c-source --run=false    write program/program.c instead (--c-path= puts it elsewhere)
spite program --run=false               only compile: the errors, if any
spite program -- --serve=true           the program's own arguments, read by Environment
bash check.sh                           the compiler still compiles itself, and every corpus passes
```

A program lives in its own folder, and `spite game` runs it: `launcher/launcher.spite` loads `library/`, then the
target system's folder of it, then `game/`, whose every sub folder is a
namespace (`game/engine/renderer/debug.spite` is `Engine.Renderer.Debug`; a file named like its folder is the
folder's own class). `load "folder"` inside a function loads another package; a file at the same namespace path
reopens the class: same-named functions and attributes replace, the rest are added, and an enum declared again
gains the values it lists.

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
- An `if`/`else` never sits directly inside a branch of another `if`/`else`: move the inner decision into a
  function named for what it decides, or use one `switch` when both test which member of a union a value is. A
  flat `else if` chain, and an `if` with no `else`, are fine.

## Names, comments, unused things

- A variable, parameter or attribute never has the name of a function of its class: `var stem = file_stem(path)`,
  never `var file_stem = file_stem(path)`.
- `snake_case` for variables, attributes, parameters, functions and enum values; `PascalCase` for classes,
  enums, unions and types; never a single letter; never an abbreviation (`message` not `msg`, `index` not `idx`,
  `value` not `val`). The error names the word to write.
- A local that is never read is an error: remove it. Assigning is not reading, and no spelling silences it. A
  parameter the signature needs but the body ignores is named `_name`; a `_name` that is read is an error too. A use inside a branch a codegen test rules out (`if $slot_type == Entity { ... }`) still counts, so
  a parameter only that branch reads is not unused in the other instantiations.
- An attribute nothing reads is an error too (D118): remove it. A private `_name` attribute is checked too.
  Assigning it is not reading it. A public attribute of a folder the program `load`s is not checked, since
  code the program does not load may read it. A Symbol template counts only when it reads the value,
  `x.attributes[attribute]`: an attribute kept as a marker that a walk inspects through `attribute.name` or
  `attribute.class` is unused, so say what the marker means some other way. A function nobody calls still
  reads what it names.
- The words other languages use for things Spite writes differently -- `none`, `nil`, `undefined`, `self`,
  `new`, `import`, `require`, `elif` -- are errors wherever they appear, naming the Spite form, so none of them
  can name a variable or a parameter either: `var none: Long = 0` says to write `null`. Pick another name
  (`no_handle`, `empty`).
- No name is taken by the C that Spite compiles to (D168): `short`, `default`, `register`, `static`, `unsigned`,
  `stdout`, `near`, `far` and `pascal` are ordinary names for a variable, attribute, parameter or function, and
  so are `allocate`, `make`, `retain` and `release`. Do not rename around C. (`int`, `char`, `bool`, `min` and
  `max` are still errors, as abbreviations.)
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
- Bits are functions on the whole numbers, never symbols: `value.shifted_left(count)`, `shifted_right(count)`
  (arithmetic on a signed type, logical on an unsigned one), `bits_and(mask)`, `bits_or(mask)`,
  `bits_exclusive_or(mask)`, `bits_inverted()`, `set_bit_count()`, `leading_zero_count()`, `trailing_zero_count()`.
  The mask is cast to the receiver's type; a count of the width or more shifts everything out, a negative one
  halts. Do not fake them with `/` and `%` by powers of two.
- Everything that is not a number, a `Bool` or an enum value is a reference: passing, assigning and storing share
  the same object. `copy()` copies one level, `deep_copy()` all the way down. `drop()` runs when the last reference
  goes. Two objects that refer to each other leak: clear one side.
- `List<T>`: `[1, 2, 3]`, `append`, `prepend`, `insert`, `remove_at`, `remove_last`, `remove_first`, `first`,
  `last`, `count`, `contains`, `is_empty`, `clear`, `reverse`, `join` (text, numbers, `Bool` and enum values
  all join), `list[index]` (a `T?`: out of range gives nothing -- `crash names[index]` narrows it like a path,
  and so does `crash glyphs[code - 32]`, or any index with no call in it, with no copy into a local first;
  `crash names.count() == 3` proves `names[0]` to `names[2]`, and `while index < names.count()` proves
  `names[index]` in the loop body, so a `crash names[index]` inside that loop is an error saying so: delete it).
  `Dictionary<T>` (String keys, insertion order): `set`, `get` (a `T?`), `has`, `remove`, `count`, `keys`,
  `values`, `dictionary["key"]` (a `T?`, like `list[index]`).
- A chain of templates, `teams.filter_active().map_lead().sum_age()`, runs as one loop with no list in between.
- Do not hand-optimise: the compiler folds `Build` fields and codegen tests, fuses chains, appends to text in
  place, puts short-lived buffers in the frame and shakes out what is unused, on its own. Every such optimisation,
  built or planned, and what it could ever change that you see, is in [optimizations.md](optimizations.md).
- On a list or dictionary of a class: `filter_<member>()`, `count_<member>()`, `any_`, `all_` (a `Bool` member),
  `sum_<member>()` (a number), `sort_by_<member>()`, `find_by_<member>(value)` (a `T?`), `map_<member>()`,
  `each_<member>()` (a function). A member is an attribute or a function that takes nothing.
- A `while` that only walks a list doing what one of these does -- `var index = 0`, `while index <
  items.count()`, `total = total + items[index].price`, `index = index + 1` -- is an error naming
  `items.sum_price()`. Keep `while` for loops that need the index, pass more than the element, or walk state.
- The member can also be a function of the class you are writing in that takes the element and nothing else:
  with `func say_hello(name: String)`, `names.each_say_hello()` calls it once per name, and `filter_`, `map_`,
  `sum_` and the rest take such a function the same way, on a list of anything, chained or not. When the element
  has a member of that name too, it is an error: rename one.
- A `while` whose whole body passes each element of a list to one such function -- `var index = 0`,
  `while index < names.count()`, `say_hello(names[index])`, `index = index + 1` -- is an error naming
  `names.each_say_hello()`. A function that needs more than the element (`print_statement(statement, depth)`)
  keeps its `while`.
- `enum`, `union` and `type` declarations take no `=`, one entry per line, no commas:

  ```spite
  enum Job {
      'knight'
      'mage'
  }
  ```

- An enum's values are single quoted and resolve from where they are used. A reopening file that declares the
  enum again adds the values it lists that it did not have. `course: Symbol<Course>` walks the values in order:
  `course.name` is the text, `course.value` the value, and `list_courses()` calls `list_course` once per value.
  A name pattern's hole (`phase: Symbol<$system_type.phase_all>`) matches only the values of the enum named for
  it, `Phase`.
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
- A call between a proof and a read undoes the proof when the called code (followed all the way down) may assign
  an attribute the proof reads or shrink a list it reads; prove it again after such a call, or read what you need
  before it. A call that cannot change it keeps the proof. Calling a function value keeps no proof about
  attributes or lists.
- A `switch` is over a union or a `T?` and covers every member; `_:` as the last case answers for the rest, and
  two cases doing the same thing are an error: write it once as `_:`. An enum is compared with `==`, not switched.
- `value == Monster` is a class test (false for `null`), and `if value == Monster { }` narrows `value` inside. A
  switch that is one class case and `_:`, each a `return`, is an error: write the `if`, or `return value == Monster`.
  In a generic class, `value == $wanted_type` tests for the class the codegen value is bound to.
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
  or an exact type) is decided while compiling, and `$value_type.element_type` names what the type holds. Only
  what the taken branch reaches is compiled -- helper functions, and the code after a chain whose branch returns
  -- so keep one generic class with a helper per kind, not one class per kind.
- A getter with no setter makes a read-only attribute: `get_fahrenheit()` answers `.fahrenheit`, and assigning
  it is an error.
- `person.age = 1` calls `set_age(1)` and `person.age` calls `get_age()` when the class has them.
- Operators are functions a class may define: `sum`, `subtract`, `multiply`, `divide`, `remainder`, `equals`,
  `less_than`, `greater_than`, `negate`, `get_at(index)`, `set_at(index, value)`. Without `equals`, `==` compares
  identity.
- Codegen values: `generic $damage_type` and `generic $is_magic`, one per line at the top of `weapon.spite`, and
  `Weapon<Int, true>(10)` supplies them in that order; `Pair("a", 1)` may leave them out when the constructor's
  arguments say them (through `$T`, `$T?`, `List<$T>`, `Dictionary<$T>` or a function value). The constructor
  lists none: `func Weapon(damage: $damage_type)`. `$` is for generics only: a `$name` with no `generic` line is
  an error. `if $is_magic { }` is decided at compile time. `generic $item_type: Printable` accepts only classes
  that fit the `type` `Printable`; any other is an error where the class is named, not inside the generic.
- Settings: reopen `Environment` in the program's `environment.spite` with one `var` per setting and a literal
  default (`var serve = false`), then bind `var environment = Environment()` and read `environment.serve`. The
  value comes from `--serve=true` after `--` on the command line, else the `SERVE` environment variable, else the
  default.
- Build settings: reopen `Build` in `build.spite` the same way. A `Build` field is decided when compiling --
  `spite game --serve=true`, else its default -- and is a constant in the program, so `if build.serve { }`
  keeps only one branch. The compiler's own options (`mode`, `optimized`, `debug_memory`, ...) and
  `build.target_operating_system` are `Build` fields too.
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
  constructor takes no arguments. A singleton is always bound to a `var` first -- `var journal = Journal()` beside
  the attributes, or in a function -- and used through the name: `Journal().record(entry)`, `Build().program`,
  `keep(Console())` and `return Console()` are errors.
- `value.memory` is where a named value lives (`.address`, `.bytes`, `.section`: `'heap'`, `'stack'`,
  `'constant'`). A container of your own is a generic class over `var heap = Memory.Heap()` (`allocate`,
  `resize`, `free`, each on a `Memory.Address`; the compiler places each allocation) and a
  `TypedMemory<$value_type>` (`read_value`, `write_value`, `release_value`, `value_bytes`), as
  `library/list.spite` is. `address.read_long(offset)` and the other reads and writes are for `library/` only.
- An object is made on the heap unless the line right after it names another allocator:
  `var spark = Particle()` then `spark.memory.allocator = arena` (`var arena = Memory.Arena(65536)` beforehand)
  makes it in the arena from the start. Later, it is an error: copy it and set the copy's allocator.

## Built in classes

`Console()` (`print`, `write`, `error`, `debug`, `read_line(): String?`; each value printed is its `to_string()`, so
a class prints once it declares `func to_string(): String`, and `debug` shows any value's state, a class as
`Name { attribute: value }`, through the `to_debug()` every value has), `File(path)` (`read(): String?`, `write`,
`append`, `exists`, `remove`), `Directory(path)` (`path`, `entries(): List<Directory.Entry>` -- each a `Directory` or a `File`, switched on --,
`files`, `folders`, `exists`, `create`),
`Process(command, arguments)` (`run(): Int`, `output()`), `Program()` (`exit(code)`, `sleep(milliseconds)`,
`environment(name): String?`). `Console` is a singleton: `Console()` is the same instance everywhere, bound once
as `var console = Console()`.
`Concurrent(function)` runs a function on a fiber and `Parallel(function)` on the thread pool: the handle stands
in for what the function returns and reading it is the wait (there is no `.wait()`), `finished` answers without
waiting, the type is never written, and dropping the handle waits for it. There is no `async`/`await`: a function
that reads, sleeps or waits is an ordinary function, and the compiler suspends it there when something else can
run ([concurrency.md](concurrency.md)).
`Json(value).write(): String` writes JSON and `Json<T>(null)` reads it (`read(text): T?`,
`read_or_crash(text): T`), for any class, list, dictionary, enum, number, `Bool`, `String` or `T?`; `read` skips unknown keys, keeps defaults for
missing ones, and is `null` on a value of the wrong kind (docs/json.md).
Time is stored as an `Instant` and nothing else: `clock.now()`, or `Instant(Duration(1710054000, 'seconds'))`.
`Duration(90, 'minutes')` is exact time (no days); `Period(1, 'months')` is calendar time, added to a `LocalDate`,
`LocalTime` is a clock reading and `LocalDateTime(date, time)` both, none of them an instant. A zone only shows or
reads a local reading: `var zones = TimeZones()`, `zones.find("America/New_York"): TimeZone?`, `zones.utc()`,
`zones.fixed_offset(duration)`, then `zone.to_local(instant)`, `zone.to_text(instant)` and
`zone.to_instant(local, 'compatible')` (or `'earlier'`/`'later'`, always written). `TimeText()` reads ISO 8601
(`read_instant(text): Instant?`, `read_local_date`, `read_duration`, ...) and `to_string()` writes it (docs/time.md).
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
| `a << 3`, `a >> 3`, `a & mask`, `a \| mask`, `a ^ mask`, `~a` | `a.shifted_left(3)`, `a.shifted_right(3)`, `a.bits_and(mask)`, `a.bits_or(mask)`, `a.bits_exclusive_or(mask)`, `a.bits_inverted()` |
| `count++`, `count += 1` | `count = count + 1` |
| `condition ? a : b` | an `if` with an `else`, or a function that returns one or the other |
| `new Monster()` | `Monster()` |
| `this.name`, `self.name` | `name` |
| `import`, `require` | `load "folder"`, a keyword on its own line inside a function |
| `elif` | `else if` |
| `class Monster { }` | nothing: the file *is* the class |
| `func greet(name: String = "world")` | a second function, or an attribute holding the value |
| `print(value)` | `var console = Console()` at file level, then `console.print(value)` |
| `Console().print(value)`, `Build().program` | `var console = Console()`, `var build = Build()`, then `console.print(value)`, `build.program` |
| `toString()`, `__str__`, `Display` | `func to_string(): String` in the class, which `console.print` calls; inside text, write `"{value.to_string()}"` |
| `console.log(object)`, `dbg!`, `__repr__`, `{:?}` | `console.debug(value)`: every value has `to_debug()`, and a class may declare its own |
| `"hello ${name}"`, `"hello " + name` | `"hello {name}"` |
| `for item in list` | `map_`/`filter_`/`each_<member>()`, or `list.each_<function>()` with a function of yours; `while index < list.count()` when the body needs more |
| `value == null` | `if value { } else { }`, `assert value`, `crash value`, or `switch` |
| `new Date()`, `DateTime.Now`, `datetime.now()` | `clock.now()`, an `Instant`; shown through a zone from `TimeZones()`, never stored as a local reading |
| `timestamp + 86400000` for tomorrow | `zone.to_local(instant) + Period(1, 'days')`, then `zone.to_instant(tomorrow, 'compatible')`: a day is not always 24 hours |
| `text[0]` | `text.character_at(0)`, or `text.slice(start, end)` |
| `// comment`, `/* comment */` | nothing, or `# docs/page.md#section` on its own line outside a function |
| renaming `short`, `static` or `near` because C takes them | the name you meant: Spite reserves nothing for C |

Text is written with its values inside it: `"hello {name}"`, where `{ }` holds one value of any type and
`\{` is a brace meant literally. Joining written text with `+` is an error; two values still join with `+`.
