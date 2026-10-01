# Spite: the whole language on one page

Everything here is enforced by the compiler. When it rejects something it says what to write instead, and it
never warns: it errors or it is fine. A file that does not parse is reported as
`path:line:column: message`; once everything parses, every error of the program comes in one run as
`path:line: error: message (in Class.function)`. Every compile first rewrites your files into the one style
(`formatted path` is printed), so what you read back may differ from what you wrote.
The other pages are the reference, each ending in its rules in full ([README.md](https://github.com/Mortaro/spite-language/blob/master/docs/README.md)); this page is the
working set.

Code blocks are fenced as `gdscript` only so GitHub colours them: GitHub has no Spite highlighter yet. Every
one of them is Spite, not GDScript: write `.spite` files.

## Run it

```
spite program                           build .spite/build/program/program.exe and run it
spite program --optimized               optimized build (a Build field)
spite program --debug-memory            print the allocation balance at the end
spite program --repl-port=4000          serve the REPL; spite connect 4000 --command="..." asks it
spite program --c-source --run=false    write .spite/build/program/program.c instead (--c-path= puts it elsewhere)
spite program --run=false               only compile: the errors, if any (writes no executable)
spite program --executable --run=false  build .spite/build/program/program.exe without running it
spite program --player-name=ada         a setting the program's Environment declares (kebab-case, no '--')
spite format game                       format files without compiling them (every compile formats first anyway)
bash check.sh                           the compiler still compiles itself, and every corpus passes
```

**Debug the running program instead of rebuilding and logging** (D242). Run it with
`--hot-reload --repl-port=4000` and keep it running: save a file and send `wait_reload` to learn what was swapped
in, read any value by its path (`World().player.health`, `describe Monster`, `memory`), and set a breakpoint with
`break monster.spite:42`: the program stops before that line, `locals` and any path through them answer what is in
scope there, and `continue` goes on (`clear` removes it). Every answer is one JSON line, so an agent can script it.
The commands are in [repl.md](https://github.com/Mortaro/spite-language/blob/master/docs/repl.md#breakpoints).

A program lives in its own folder, and `spite game` runs it: `launcher/launcher.spite` loads `library/`, then the
target system's folder of it, then `game/`, whose every sub folder is a
namespace (`game/engine/renderer/debug.spite` is `Engine.Renderer.Debug`; a file named like its folder is the
folder's own class). `load "folder"` inside a function loads another package (the path is relative to the file,
or absolute: `load "D:/Projects/engine/core"`); a file at the same namespace path
reopens the class: same-named functions and attributes replace, the rest are added, and an enum declared again
gains the values it lists. `Build` is the exception: a field the program's own `build.spite` declares keeps the
program's value over a loaded package's (a flag, then the program, then the package). A package opens the files
beside its own source through `class.source_folder()` (or `$item_type.source_folder()`), the absolute folder of
the class's file on the machine that built it, never through a path relative to where the program runs.
A name that resolves to no class is `unknown type 'X'`, with `did you mean
'Component.Eye'?` when a class ends the same way: an environment's folder (`server/`) is not part of the name.
A program's settings and arguments reach it as typed, from bash, PowerShell or `cmd` alike:
`/Game/Legacy/` stays `/Game/Legacy/`, so do not work around Git for Windows' path rewriting.

## A file is a class

```gdscript
var name = ""
var health = 10
var target: Monster? = null
var friends = List<Monster>()

func Monster(starting_name: String) {
    name = starting_name
}

func hurt(amount: Integer) {
    health = health - amount
}

func is_alive(): Boolean {
    return health > 0
}
```

- The file name is the class name (`monster.spite` is `Monster`). A file holds declarations only, in this
  order: a `singleton` line, `generic $name` lines, enums, unions, types, variables, the constructor, then
  functions (`a file holds declarations and nothing else` for a statement at file level; `a file is ordered` for
  the wrong order). Every `var` has a default value (`expected '=' (every variable needs a default value)`). A
  name starting with `_` is private to its class (`'_hidden' is private to 'Secret'`).
- The function named like the class is the constructor. Do not write an empty one: a class without a constructor
  is made from its defaults, and `func Monster() { }` is an error (`has an empty constructor: delete it`).
- A constructed object is kept and used: `Report(text)` written as a statement of its own is an error. A class
  whose construction is the whole point is a function instead (`report(text)` on the class that needs it).
- A program is a folder, and it starts by constructing the class of the file named after the folder
  (`game/game.spite` is `Game`). That constructor takes no arguments: settings come from `Environment()` and
  `Build()` below, and `Arguments()` is the raw command line anywhere (`.count()`, `.get(0)`, and `.player` for
  `--player=value`, a `String?`). `spite game/game.spite` is an error: a program is named by its folder.
- `return` is always written. The return type is written `(): Type`. No return type means it returns `Nothing`,
  so `return 1` there is `'one' returns nothing, so 'return' cannot carry a value here`.
- Only `while` exists. There is no `for`, `break` or `continue`: an early exit is part of the loop's condition.
  Prefer the member templates below.
- There is no `self.`: members are read by name and functions called by name. `this` is the instance itself,
  only for handing it to something (`registry.record(this)`); `this.name` and `this.helper()` are errors.
- One function per name, no overloading (`'Shop' declares 'price' twice`): an argument is cast to the
  parameter's type. The last parameter may be
  `...values: List<Type>`, and the caller writes the values one by one (passing a whole list there is an error).
- A call is never passed straight into another call: compute it first into a named `var` and pass the name,
  `var token_text = source.slice(start, end)`. The error: `'text.slice(0, 1)' is called inside an argument of
  'console.print': compute it first into a named 'var' and pass the name`. The holes of a text are not
  arguments: `"{names.count()} names"` is fine.
- A constructor is not an argument either, on any line: make the object on a line of its own and pass its name,
  `var token = Token("number", token_text)`, then `tokens.append(token)`. The error:
  `'Bundle.CounterButton(screen.id)' is constructed inside an argument of 'world.create_entity_from_bundle': a
  constructor call is never an argument, so make it first on a line of its own, 'var counter_button =
  Bundle.CounterButton(screen.id)', and pass 'counter_button'`. That goes for `Label(Font())` and
  `buffers.set(List<String>())` too. A constructor read at once is fine (`JsonWriter(order).write()`), and
  `Parallel(worker.run)` passes a function, not an object.
- The two branches of an `if`/`else` never compute the same call (`'measure(2)' is computed in both branches`):
  compute it once before the `if`.
- An `if`/`else` never sits directly inside a branch of another `if`/`else`: move the inner decision into a
  function named for what it decides, or use one `switch` when both test which member of a union or which value
  of an enum a value is. A
  flat `else if` chain, and an `if` with no `else`, are fine. The error: `this 'if'/'else' is inside a branch of
  another 'if'/'else': move it into a function named for what it decides`.

## Names, comments, unused things

- A variable, parameter or attribute never has the name of a function of its class: `var stem = file_stem(path)`,
  never `var file_stem = file_stem(path)` (`the variable 'file_stem' has the name of a function of this class`).
- `snake_case` for variables, attributes, parameters, functions and enum values; `PascalCase` for classes,
  enums, unions and types; never a single letter; never an abbreviation (`message` not `msg`, `index` not `idx`,
  `value` not `val`). The error names the word to write: `'msg' abbreviates: write 'message' instead of 'msg'`,
  `the variable 'x' is a single letter`, `the variable 'myValue' must be written in snake_case`. The language's own
  type names follow the rule: `Integer`, `Boolean`, never `Int`, `Bool`. Folder names are snake_case too.
- A local that is never read is an error (`'total' is never read: remove it`). Assigning is not reading, and no
  spelling silences it. A parameter the signature needs but the body ignores is named `_name` (`the parameter
  'amount' is never read: remove it, or name it '_amount' if the signature needs it`); a `_name` that is read is
  an error too. A function nobody calls is never reported. A use inside a branch a codegen test rules out (`if $slot_type == Entity { ... }`) still counts, so
  a parameter only that branch reads is not unused in the other instantiations.
- An attribute nothing reads is an error too (`the attribute 'world' is never read: remove it`). A private `_name` attribute is checked too.
  Assigning it is not reading it. A public attribute of a folder the program `load`s is not checked, since
  code the program does not load may read it, except a singleton binding (`var world = World()`), which is an
  error unread anywhere: a class that needs the singleton binds it itself. A Symbol template counts only when it reads the value
  (`x.attributes[attribute]`). An attribute kept as a marker that a walk inspects through `attribute.name` or
  `attribute.class` is unused, so say what the marker means some other way. A function nobody calls still
  reads what it names.
- The words other languages use for things Spite writes differently (`none`, `nil`, `undefined`, `self`,
  `new`, `import`, `require`, `elif`) are errors wherever they appear, naming the Spite form, so none of them
  can name a variable or a parameter either: `var none: Long = 0` says to write `null`. Pick another name
  (`no_handle`, `empty`). `load` is reserved too: it always loads a package, so a function that loads something
  says what (`load_texture`).
- `class`, `attributes`, `functions`, `instances` and `memory` never name an attribute or a function (nor a
  `get_`/`set_` one): reflection gives every object those members, and `JsonReader` and every attribute walk read
  through `value.attributes`. The error suggests the class's name in front (`table_attributes`). A local may use
  them.
- No name is taken by the C that Spite compiles to (D168): `short`, `default`, `register`, `static`, `unsigned`,
  `stdout`, `near`, `far` and `pascal` are ordinary names for a variable, attribute, parameter or function, and
  so are `allocate`, `make`, `retain` and `release`. Do not rename around C. (`int`, `char`, `bool`, `min` and
  `max` are still errors, as abbreviations.)
- A function body holds no empty lines: the blank line is where a second function wants to be, so name the part
  below it and call it. The formatter (and so every compile) deletes one you write. An `if` whose only statement is a bare `return`
  is an error too: that is a precondition, and a precondition is written `assert condition`.
- A comment is one line, outside functions, and nothing but a link to a markdown heading:
  `# notes.md#why-this-exists` (relative to the entry file's folder; the file and the heading must exist).
  Anything else, including `//` and `/* */`, is an error. If the code already says it, do not write it.

## Values

- Numbers: `Integer` (32 bit, the default), `Long`, `Tiny`, `Short`, `Byte`, `UnsignedShort`, `UnsignedInteger`,
  `UnsignedLong`, `Float` (the default for decimals), `Double`. `Boolean`. `String` (double quotes only; escapes
  `\n`, `\t`, `\r`, `\\`, `\"` and `\{` for a literal brace, in any order: `"\\\{"` is a backslash and a brace; a
  text inside a hole keeps its own escapes and holes).
- No cast syntax: the right side is cast toward the left. `"age {3}"` is `"age 3"`; `var total: Integer = "12"` parses
  it. Anything with a `to_string()` (numbers, `Boolean`, enums, a class that declares one) casts to text wherever a
  `String` is wanted: `label.text = clicks`, `show(clicks)`, `return clicks`, `names[badge]` on a `Dictionary` (a whole number there keys it by numbers instead). So a
  text of one hole and nothing else, `"{clicks}"`, is an error naming the direct form; elsewhere write
  `clicks.to_string()`. Arithmetic is done in the left side's type, so write the wider operand first: `total * count` with a `Long`
  `total`, never `count * total`, which is an error (so is an `Integer` plus a `Float`); a literal on the right that
  fits is fine. A constant that overflows `Integer` (`65536 * 65536`) is an error: write the number. Comparisons follow
  the same rule: `count < total` with a `Long` `total` is an error, write `total > count`. A value converted to a narrower type wraps, and so does
  unsigned arithmetic (use `UnsignedInteger`/`UnsignedLong` for a hash); signed `+ - *` that does not fit halts
  naming the line in a `--debug-memory` or development build and wraps in production. A whole number divided by zero (`/` or `%`) halts naming the line, and a divisor written as zero is an error; after `assert divisor != 0` the check is gone. Floats keep infinity and not-a-number.
- Bits are functions on the whole numbers, never symbols: `value.shifted_left(count)`, `shifted_right(count)`
  (arithmetic on a signed type, logical on an unsigned one), `bits_and(mask)`, `bits_or(mask)`,
  `bits_exclusive_or(mask)`, `bits_inverted()`, `set_bit_count()`, `leading_zero_count()`, `trailing_zero_count()`.
  The mask is cast to the receiver's type; a count of the width or more shifts everything out, a negative one
  halts. Do not fake them with `/` and `%` by powers of two.
- Maths is functions on the numbers too, every name in full, never a `Math` class: on a `Float` or `Double`
  `square_root()`, `sine()`, `cosine()`, `tangent()`, `arc_sine()`, `arc_cosine()`, `arc_tangent()`,
  `rise.arc_tangent_over(run)` (C's `atan2(rise, run)`), `power(exponent)`, `exponential()`, `logarithm()`
  (natural), `logarithm_base_2()`, `logarithm_base_10()`, `floor()`, `ceiling()`, `round()`, `truncate()`,
  `absolute()`, `minimum(other)`, `maximum(other)`, `clamp(low, high)`, `is_finite()`, `is_infinite()`,
  `is_not_a_number()`; on a whole number `absolute()`, `minimum`, `maximum`, `clamp`. Constants are asked of the
  class, as get-only attributes read without parentheses: `Float.pi`, `tau`, `euler_number`, `infinity`,
  `not_a_number`, `largest`, `smallest` (the most negative), and `Integer.largest`, `Long.smallest` and so on. Each is the C library's function, called
  inline; do not write your own `sine` or square root from a series. Nothing halts: `(-1.0).square_root()` is
  `nan`.
- Everything that is not a number, a `Boolean` or an enum value is a reference: passing, assigning and storing share
  the same object. `copy()` copies one level, `deep_copy()` all the way down. `drop()` runs when the last reference
  goes. Two objects that refer to each other leak: hold the back reference as a `Weak<T>` (`get()` is a `T?`, `null` once the object is freed), or clear one side.
- `List<T>`: `[1, 2, 3]`, `append`, `prepend`, `insert`, `remove_at`, `remove_last`, `remove_first`, `first`,
  `last` (both a `T?`, `null` on an empty list, like `[]`: `var first = names.first()` then `crash first`), `count`, `contains` (elements that are numbers, `Boolean`, `String`
  or an enum only: on a list of a class use `any(f)` or `find_by_<member>`), `is_empty`, `clear`, `reverse`,
  `join` (text, numbers, `Boolean` and enum values all join), never `add` or `pop`. `list[index]` is a `T?` (out of range gives nothing), and `crash names[index]` narrows it like a path,
  and so does `crash glyphs[code - 32]`, or any index with no call in it, with no copy into a local first;
  `crash names.count() == 3` (or `>= 3`, or `> 2`) proves `names[0]` to `names[2]`, so does
  `var names = ["a", "b", "c"]`, `crash not names.is_empty()` proves `names[0]` (a list's, never a dictionary's),
  and `while index < names.count()` proves `names[index]` in the loop body, so a `crash names[index]` inside that
  loop is an error saying so: delete it). `get_at(index)` is the same function as `[]` and answers the same `T?`;
  write `names[index]`, which proofs can narrow. Every `[]` answers a `T?`; a `Vector`'s and an `Items`' too. A
  list of numbers, `Boolean`, enums or `Memory.Address` is always a `List` (`Vector<Integer>` is an error naming
  `List<Integer>`); it has `remove_swapping(index)` and `reserve(count)` too.
  `Dictionary<T>` (insertion order): `set`, `get` (a `T?`), `has`, `remove`, `count`, `keys`,
  `values`, `dictionary["key"]` (a `T?`, like `list[index]`: `inventory["shield"] == 0` is false for an absent
  key). Keys are text or whole numbers, decided from the keys you give it: key by the number itself
  (`created_layouts[entity]`), never by text made from it (no `"{entity}"`). `keys()` answers the numbers.
  One dictionary never takes both kinds (a compile error naming both places). A list or a dictionary is not printable: `console.print(list)` is `'List<Integer>' does not fit type
  'Printable'`; print `list.join(", ")`, or `console.debug(list)`.
- A chain of templates, `teams.filter_active().map_lead().sum_age()`, runs as one loop with no list in between.
- To remove many elements, never call `remove_at` or `remove_swapping` in a loop: `creatures.remove_where_dead()`
  or `numbers.remove_where(is_odd)` removes every one the test is true for in one pass and keeps the rest in
  order (`List`, `Vector`, `Items`; the passed-function form on a `List` only, never on a chain or a
  `Dictionary`). `truncate(count)` drops the tail. When the test is not a function of the element (a mask of
  rows), walk the rows, `swap(row, kept)` each one that stays, then `truncate(kept)`.
- `Vector<T>` holds objects inline for fast walks (`append`, `vector[index]`, `set_at`, `remove_at`, `count`,
  `clear`, `copy`, and `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_`, `parallel_each_` templates).
  An item is a `String` or a class whose attributes are only numbers, `Boolean`, enums and `String` (a `List`
  attribute is an error naming it). `velocities[index]` is a `Velocity?`: after `crash velocities[index]` (or
  inside `while index < velocities.count()`), `var velocity = velocities[index]` is the item itself, borrowed:
  `velocity.across = 3.0` writes the vector.
  A borrowed item is never kept: storing it in an attribute or a list, returning it, `velocity.integrate` as a
  function value, `var alias = velocity`, and reading it after a line that may resize the vector (`append`,
  `remove_at`, `clear`, or a call that may do one) are errors, each naming the fix: `velocity.copy()`, an
  independent object, or reading `velocities[index]` again.
- Pass a borrowed item (a name read from a vector, `velocities[index]`, a row's attribute, an item a lookup lent
  you) to your own functions freely: it is lent for the call (D257). `apply(event, mouse, keyboard)` writes the
  stored items in place and may pass them on to `press(mouse)`; nothing is copied. The function may not keep it
  (attribute, list, return, function value) or reach a call that appends to or removes from its collection;
  each is an error naming the parameter and the call that lent it. Never pass `.copy()` to a function that
  writes the item: the writes would land on the copy and vanish (D244). A library function or a union parameter
  is not lent an item: pass the attributes it needs.
  A class kept in a `Vector` may not use `this` as a value, nor have a `drop()`.
- A `Vector` or an `Items` takes no passed function (`each(f)`, `map(f)`, ...): its items are borrowed, or text.
  Numbers go in a `List`, which takes them all: `numbers.each(found.append)`, `weights.filter(is_heavy).sum(double_of)`.
- A row of borrowed items for a system: `var row: Moving = {position: positions[index], velocity:
  velocities[index]}` (`Moving` a `type`), then `mover.update_each(row)`. The row costs nothing (it lives in the
  frame) and may be passed only to a function called by name whose parameter is a `type`; inside it,
  `moving.position.left = 3.0` writes the vector's item. Keeping the row (`'moving' holds items borrowed from
  'positions' and 'velocities' for this call and cannot be kept in the attribute 'kept'`), returning it, putting
  it in a list, `var alias = row`, assigning `moving.position = ...`, or a called function that may append to or
  remove from those vectors (`... may move the items of 'positions': a function that is handed a row may not
  append to or remove from the vectors it borrows from`) are errors. Build a row only of borrowed items and plain
  values.
- A generic runner builds the same row with a walk: `var row: $row_type = null`, then on the next line
  `fill_attributes(row, index)` whose template is `crash columns.attributes[attribute][index]` and then the line
  `row.attributes[attribute] = columns.attributes[attribute][index]`: a `[]` read answers a `T?` and a row holds
  items, so the template states each read with a `crash` line before the fill. The compiler writes those checks
  and the literal in its place (D212), with the same rules.
- Over sparse sets (each entity at a different place in each column), the runner first walks the attributes to
  fill `found: List<Integer>` with each one's place, then the fill template reads each attribute from its own
  generic singleton, after a `crash` line for each read (the places arrive as the parameter `rows`): `crash
  rows[attribute.index]`, `var stored_row = rows[attribute.index]`, `crash
  Column<attribute.class>().values[stored_row]`, then `row.attributes[attribute] =
  Column<attribute.class>().values[stored_row]`. An index never holds another `[]` read (D285). Choose
  per attribute with an `if` on `attribute.class == Entity` or `attribute.class.fits_vector()` (both decided while
  compiling), each branch one such line: `Entity(entity)` is made in the frame, a reference column's value
  (`ReferenceColumn<attribute.class>().at(rows[attribute.index])`, after `crash rows[attribute.index]`) is counted
  for the row (D217). Keep the index
  expression to parameters, `attribute.index` and whole numbers; work anything else out before the walk. The
  places may live in another object: pass its list straight in (`fill_attributes(row, matcher.rows)`) or read
  `matcher.rows[attribute.index]` into the line's `var` (its items are numbers in a `List`), and never copy them into a
  list of the runner's first.
- In a generic class that keeps values of a type it does not know (an engine's `Column<$component_type>`), use
  `Items<$component_type>()` (D218, name provisional): inline and borrowed like a `Vector` when the type fits
  one, references like a `List` when not, with one set of members (`append`, `items[index]` (a `T?`, `null` out
  of range, narrowed like a list's), `set_at`, `remove_at`, `remove_swapping(index)` (the last item moves into `index`), `count`, `is_empty`,
  `clear`, `copy`, `deep_copy`, and the `each_`/`map_`/`filter_`/`count_`/`any_`/`all_`/`sum_`/`parallel_each_`
  templates; no `each(f)` forms). Then one column class serves every component,
  and the walked row's line is
  `Column<attribute.class>().values[stored_row]` (after `var stored_row = found[attribute.index]`) for all of them, with no `fits_vector()` branch. The
  borrow rules above apply only to a type that fits; the error then starts `'Velocity' fits a Vector, so the
  items of 'velocities' are borrowed: ...`.
- A system may take components as arguments (`update_each(position: Position, velocity: Velocity)`); the runner
  passes them with `system.phase_each(made_arguments(found))` on a line of its own, where `made_argument(argument:
  Symbol<$system_type.phase_each>, rows: List<Integer>): argument.class` is `crash rows[argument.index]`, `var
  stored_row = rows[argument.index]`, `crash Column<argument.class>().values[stored_row]` and `return
  Column<argument.class>().values[stored_row]` (an `if` on `argument.class == Entity` may choose another
  line, such as `return Entity(entity)`). The items are borrowed for that one call only (D220): the system may not
  keep them or resize their columns (it may lend them on to its own helpers, D257), and calling `made_position(...)` anywhere else is the error for returning a
  borrowed item. The template's `Symbol` parameter must be a word of its name (`made_argument(argument: ...)`), or
  it has no plural.
- A dictionary's key kind (text or whole numbers) comes from the keys your own code gives it and follows it
  wherever it is assigned or passed. An error naming `a Dictionary<String> keyed by whole numbers` against one
  `keyed by text` says which key decided each: give both the same kind of key.
- `null` is only for `T?`: `var target: Monster = null` is an error (D236); write `var target: Monster? = null`
  for none yet, or `var target = Monster()` for a default. In a generic class write `var system = $system_type()`,
  in a template `var made = argument.class()`. `= null` stays only on an attribute the constructor assigns and on
  a local a walk over its attributes fills (`var row: $row_type = null` then `fill_attributes(row, ...)`).
- To hand one stored component to a caller, return it straight from the singleton's storage: `func of(entity:
  Integer): $component_type?` ending `return column.values[row]` (D230). The caller gets the stored item, so
  `layout.order = 3` writes the column; never return `.copy()` for this, since writes to a copy are lost. The
  caller follows every borrow rule (no keeping, second name, reading after a line that may grow or shrink the
  column; lending it to a call is fine), may not return it further, and gives it a name not used before in the block. A function that lends
  returns only such items or `null`.
- A runner that treats systems of one row and of several differently asks `if phase.argument_count() == 1 { }`
  inside its walk over `phase: Symbol<$system_type.phase_each>` (or `$system_type.argument_count("update_each")`
  of one function; D219, name provisional). It is decided while compiling, so the branch for the other arity is
  never compiled: a `Stream<$system_type, argument.class>` that calls `system.phase_each(row)` is made only for
  one-row systems. With a pattern whose functions take different counts, ask inside the walk instead.
- `assert` and `crash` on a compile-time question fold like `if`: `assert $slot_type == Entity` then `return
  value.id`, in a function returning `Integer?`, compiles for every `Slot<T>`, answering `null` where `T` is not
  `Entity`, with no run-time test (in one returning `Integer`, write `if $slot_type != Entity { return 0 }`);
  `crash $component_type.fits_vector()` halts every call in an instance whose type does not fit.
- Do not hand-optimise: the compiler folds `Build` fields and codegen tests, fuses chains, appends to text in
  place, puts short-lived buffers in the frame and shakes out what is unused, on its own. Every such optimisation,
  built or planned, and what it could ever change that you see, is in [optimizations.md](https://github.com/Mortaro/spite-language/blob/master/docs/optimizations.md).
- On a list or dictionary of a class: `filter_<member>()`, `count_<member>()`, `any_`, `all_` (a `Boolean` member),
  `sum_<member>()` (a number), `sort_by_<member>()`, `find_by_<member>(value)` (a `T?`), `map_<member>()`,
  `each_<member>()` (a function). A member is an attribute or a function that takes nothing. A member that does
  not fit is an error naming what the template needs: `count_stars()` on a number member says `but 'count_' needs
  it to return Boolean (to add up a numeric member use 'sum_stars')`, and `each_size()` on an attribute says
  `'each_' needs it to be a function`.
- A `while` that only walks a list doing what one of these does (`var index = 0`, `while index <
  items.count()`, `total = total + items[index].price`, `index = index + 1`) is an error naming
  `items.sum_price()`: `this 'while' walks every element of 'items' only to add up 'price': write 'var total =
  items.sum_price()'`. The same goes for a `while` that only passes each element to one function of yours:
  `say_hello(names[index])` is `names.each(say_hello)`, and likewise `map`, `filter`, `count`, `sum`, `find`,
  `any` and `all`, on a list of anything, numbers and text included. Keep `while` for loops that need the index,
  pass more than the element, or walk state.
- A template sees only the element and the list, never your class: `names.each_say_hello()` looks for a member
  `say_hello` of each name, and says `a template reads a member of each element, never a function of this class,
  so pass this class's 'say_hello' instead: 'each(say_hello)'`. To call a function of yours with each element, pass it: `names.each(say_hello)`,
  `names.map(measure)`, `names.filter(is_short)`, and `any`, `all`, `count`, `find` (the first element it is true
  for, a `T?`), `sort_by` and `sum` the same way, on a list or dictionary of anything, chained with the member
  templates or not (`people.filter_active().map(greeter.label)`). The function takes the element as its only
  argument and is bound to its owner: `greeter.label` is `greeter`'s, and a library value's works the same:
  `keys.filter(counts.has)`, `words.filter(greeting.contains)`. `counts.get` answers `T?`, so it cannot order a
  `sort_by`: pass a function of yours that narrows it. A function that needs more than the element
  (`print_statement(statement, depth)`) keeps its `while`; `map(f)` of a function that returns nothing is an
  error naming `each(f)`.
- `enum`, `union` and `type` declarations take no `=`, one entry per line, no commas:

  ```gdscript
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
- A `type` declares a shape (`label: String` and `render(Integer): String`, one per line, a required function
  naming the *types* it takes and never the names) and accepts any class, or
  object literal `{ label: "x" }`, with those attributes and functions.
- `Anything` is the built-in empty `type`, the counterpart of `Nothing`: `component: Anything` and
  `List<Anything>()` accept any object (a number is boxed). Never declare an empty `type` of your own.
- `Number` is the library `type` every number class fits (`Tiny` to `Double`, not `Boolean`): its operators and
  `to_long()`, `to_double()`. Write number code as `generic $number_type: Number`, and ask
  `$value_type == Number` (never ten comparisons) to tell a number apart in a walk. Inside a `type`, its own name
  in a signature is the class that fits.

## Nothing, null, and failure

- **Anything that can go wrong silently is a bug** (D244). Write every failure loud: a compile error where the
  compiler can know, a `crash` naming its cause where only the run can. Never let a caller get a default it cannot
  tell from a real answer, lose a write, skip a step, leak or hang. When a function cannot answer, its result says
  so (`T?`, or an empty list), or it returns a value you chose on purpose, or it crashes because a caller broke the
  rule ([failure.md](https://github.com/Mortaro/spite-language/blob/master/docs/failure.md#nothing-fails-silently)).
- `crash` and `assert` take a condition and never a message (D297): the report names the file and line and shows
  the values there. Never add a clause only to get text printed (`crash found or name == ""`).
- `Monster?` is a value that may be `null`. It must be narrowed before use: `if target { }` (with `else`),
  `assert target`, `crash target`, `while target { }`, or `switch target { Monster: ... Null: ... }`. One
  `assert a.b.c` narrows the whole path. `null` is never compared against: `value == null` is an error.
  Narrow the name or the path itself (`assert target`, `assert target.weapon`), never a local copied from it,
  which is an error. Comparing needs no narrowing: `target.name == "rat"` needs `target` narrowed, but
  `maybe_name == "rat"` is simply false when it is null.
- A call between a proof and a read undoes the proof when the called code (followed all the way down) may assign
  an attribute the proof reads or shrink a list it reads; prove it again after such a call, or read what you need
  before it. A call that cannot change it keeps the proof. Calling a function value keeps no proof about
  attributes or lists.
- Every proof the compiler makes (narrowing, proven `[]` reads, a proven divisor, frame objects, borrowed items,
  singleton locks) is listed in [proofs.md](https://github.com/Mortaro/spite-language/blob/master/docs/proofs.md), with when it does **not** apply. Read its "Which proofs
  apply to my code" first: where a proof does not hold, you write the check yourself.
- A `switch` is over a union or a `T?` and covers every member; `_:` as the last case answers for the rest, and
  two cases doing the same thing are an error: write it once as `_:`. A `switch` over an enum, a whole number or
  a text has value cases: `'red': return 30`, `-1: ...`, `"es": ...`; over an enum it covers every value or ends
  with `_:` (`has no case for 'amber'`), over a number or a text it always ends with `_:`. Three `if`s in a row
  that each compare one name with a constant and only return are an error that writes the switch for you (`these 3
  'if's each compare 'index' with a value and return, which is what a 'switch' says: write 'switch index { ... }'`);
  an `else if` chain of three is the same.
- `value == Monster` is a class test (false for `null`), and `if value == Monster { }` narrows `value` inside. A
  switch that is one class case and `_:`, each a `return`, is an error: write the `if`, or `return value == Monster`.
  In a generic class, `value == $wanted_type` tests for the class the codegen value is bound to.
- Proving what is already proven is an error too: `assert` on a value that cannot be null here (`so 'assert' on it
  proves nothing: remove the check`), and `crash list[index]` inside `while index < list.count()` (`'list[index]'
  is already proven by the loop condition`). Delete the line.
- A `Boolean?` is not a condition (`which would only test that it is there, not that it is true`): narrow it
  first, or compare it `== true`.
- There are no exceptions and no error values. Three outcomes only:
  - the compiler can know it: a compile error;
  - absence is fine: `assert condition` stops the function and answers "nothing": it returns, answers `null`
    from a `T?`, or an empty `List`/`Dictionary`/`Vector`/`Items`. It is allowed only there, never in a
    constructor (`'assert' is not allowed in a constructor`: take resolved values, or `crash`). In a function
    returning a number, `Boolean`, `String`, enum or object it is an error, since its default would pass for a real
    answer (`this 'assert' would answer a default Integer (0) that a caller cannot tell from a real one: return a
    value ('if not found { return -1 }'), make the result 'Integer?', or 'crash found' if this is a developer
    mistake`): write the answer (`if handle == -1 { return false }`), make the result a `T?`, or `crash`. Where
    `assert` is allowed, `if x { return null }` or a bare `if x { return }` is an error naming the `assert` to write
    (`write 'assert not x'`), and so is a last `if` with no `else` that only checks a value is there (`write
    'assert maybe_name'`). An `if` with no `else` that ends in `return` proves the opposite of its condition after
    it: `if not found { return -1 }` narrows `found`, `if index >= names.count() { return "" }` proves
    `names[index]`;
  - absence is a bug: `crash condition` halts with
    `spite.crash<TAB>id<TAB>path:line<TAB>Class<TAB>function<TAB>name=value...`, followed by the
    asserts that failed before it. Every name and call the condition read is there with its value, whatever its
    shape: `crash record or cooked.count() > 2` reports `record is null	cooked.count()=1`; a part an `and` or `or`
    skipped is left out. Then come the other texts, numbers and enums in scope (parameters, locals, the object's
    attributes). The condition is not on the line: open the line it names, or `grep <id> program.crashes`. A bare `crash` marks a branch that cannot happen (`crash false` is formatted to it).
    A failed narrowing names what is absent instead of a value: `clip.keys[start + 9] is missing: index 11990,
    count 11500`, `scores[key] is missing: key "bea"`, `rig.skeleton is null`. Read it as an index past the end or
    a key never set, not as a zero. A failed `assert` prints nothing as it fails; it shows up only in a crash's
    trace, unless the program is built with `--trace-asserts`, which prints each one as it fails.
- Narrowing tests presence, never the value: `crash keys[index]` passes on an element holding `0.0`, and `if count`
  runs on an `Integer?` holding `0`.
- Reading a number from text answers a `T?`: `"42".to_integer()` is an `Integer?`, `null` for `"forty two"`,
  `"12abc"`, `""` or a number the type cannot hold, so narrow it (`crash count` where the text is yours, `if`/`assert`
  where it came from outside). `var age: Integer = "42"` is an error; `var age: Integer? = "42"` reads it.
- A function that declares a result ends every path with a `return` (or a bare `crash`): a path that reaches the
  closing `}` is an error naming it (`'sign_of' answers a String, but when 'value < 0' is false (line 12, an 'if'
  with no 'else') it reaches its end without a 'return'`). An `if`/`else` whose branches both return, a `switch`
  whose cases all return and a `while true` end the path; an `if` with no `else` and any other `while` do not, so
  write the last `return` after them, `return null` included.
- A native fault (a null read or an illegal instruction inside a foreign library, a stack overflow) is never
  silent either: the program prints `spite.fault<TAB>kind<TAB>path:line<TAB>Class<TAB>function<TAB>...`, then
  `spite.frame` lines, innermost Spite function first, and ends. `-	-	-` in the place means the fault is outside
  Spite: `at=` names the library and offset, `foreign=` the last C function this thread called, and `from=` the
  Spite line that called it (start there). `stack-overflow` with `repeated=` on a frame is a recursion that never
  stops. The line in `path:line` is where the function starts, not the faulting line
  ([failure.md](https://github.com/Mortaro/spite-language/blob/master/docs/failure.md#what-a-native-fault-reports)).
- A test is a function named `test_...` that takes nothing and crashes when wrong, in a class of its own (the entry
  class's functions are not found). `tests/tests.spite` finds them all by itself ([testing.md](https://github.com/Mortaro/spite-language/blob/master/docs/testing.md)).

## Metaprogramming

- A `Symbol` parameter named inside its function's name answers every attribute:
  `func set_attribute(attribute: Symbol, value: attribute.class) { attributes[attribute] = value }` makes
  `person.set_age(2)` and `person.set_name("x")` work. An exact function always wins.
- `attribute: Symbol<Label>` ranges over another class's members, read as `label.attributes[attribute]`, and the
  plural (`show_attributes(label)` for `show_attribute`) calls the template once per attribute, in order.
- In a generic class, `if $value_type == List { }` (also `Dictionary`, `Null` for any `T?`, `Symbol` for any enum or `Symbol`, `Enum` for an enum only,
  or an exact type) is decided while compiling, and `$value_type.element_type` names what the type holds. It is
  tested the same way (`else if $value_type.element_type == Float`) in any branch of a chain. Only
  what the taken branch reaches is compiled (helper functions, and the code after a chain whose branch returns),
  so keep one generic class with a helper per kind, not one class per kind.
- `assert`/`crash` on such a condition folds too, and a `crash` that folds to false in a function the program calls
  is a compile error naming the instance (`Slot<List<String>>`): write a library's rules as `crash $row_type.has_function("update_each")`
  and a class that breaks one fails the build.
- A getter with no setter makes a read-only attribute: `get_fahrenheit()` answers `.fahrenheit`, and assigning
  it is an error.
- `person.age = 1` calls `set_age(1)` and `person.age` calls `get_age()` when the class has them, with or without
  an attribute named `age` (`get_id()` and `set_id(value)` over a private `_id` make a checked `.id`).
- Operators are functions a class may define: `sum`, `subtract`, `multiply`, `divide`, `remainder`, `equals`,
  `less_than`, `greater_than`, `negate`, `get_at(index)`, `set_at(index, value)`. Without `equals`, `==` compares
  identity. `a[x]` calls `get_at(x)`, which must answer a `T?` (`func get_at(index: Integer): Integer?`, `null` when
  nothing is there); a plain `T` is an error. A read through your own class is narrowed by what you write (`crash
  shelf[0]`): a loop over `shelf.count()` proves nothing about it.
- Codegen values: `generic $damage_type` and `generic $is_magic`, one per line at the top of `weapon.spite`, and
  `Weapon<Integer, true>(10)` supplies them in that order; `Pair("a", 1)` may leave them out when the constructor's
  arguments say them (through `$T`, `$T?`, `List<$T>`, `Dictionary<$T>` or a function value). The constructor
  lists none: `func Weapon(damage: $damage_type)`. `$` is for generics only: a `$name` with no `generic` line is
  an error. A function never has codegen values of its own (`func pick<$value_type>(...)` is an error): a
  function that takes any object takes `Anything` or a `type`. `if $is_magic { }` is decided at compile time. `generic $item_type: Printable` accepts only classes
  that fit the `type` `Printable`; any other is an error where the class is named, not inside the generic.
- Settings: reopen `Environment` in the program's `environment.spite` with one `var` per setting and a literal
  default (`var serve = false`), then bind `var environment = Environment()` and read `environment.serve`. The
  value comes from `--serve=true` (or bare `--serve` for a `Boolean`) on the command line, written beside the
  compiler's flags and kebab-case like them (`spite game --optimized --player-name=x` sets
  `environment.player_name`; there is no `--` separator), else the `SERVE` environment variable, else the
  default. A setting named like a `Build` field is a compile error naming both.
- Build settings: reopen `Build` in `build.spite` the same way. A `Build` field is decided when compiling (`spite game --serve=true`, else its default) and is a constant in the program, so `if build.serve { }`
  keeps only one branch. The compiler's own options (`optimized`, `debug_memory`, `run`, `c_source`, ...) and
  `build.target_operating_system` are `Build` fields too. A flag is kebab-case (`--debug-memory`) and sets the
  snake_case field; an unknown flag is an error. There is no `format` option: every compile formats.
- Reflection: `value.class` (a `Spite.Class`: `.name`, `.namespace` (a `Spite.Namespace?`; narrow it before
  reading its members: `assert value.class.namespace` narrows the path itself and its prefixes for the rest of
  the block; its members are `.name_with_namespaces`, `.parent`, `.classes`,
  `.namespaces`), `.functions`), `value.attributes`
  (`.name`, `.class`, `.value`: the value itself, an `Anything?` whose text is `.value.to_string()`), `value.functions` (`.name`, `.arguments`, `.returns`, `call_function()` for
  functions that take nothing and return `Nothing`), a function named without calling it (`shouter.shout`, a
  `Spite.Function<String, String>` bound to `shouter`, called as `change(text)`), `Monster.instances` (live instances), and
  `Spite.Class.instances` (every class of the program and the packages it loads, not the standard library's). `class`, bare inside a class's function, is the class
  of the instance it answers on, and a class name reads its own class object: `Monster.name` is `"Monster"`.
- A class whose file starts with a `singleton` line has one instance: `Journal()` always returns it. A
  singleton whose constructor takes arguments has one instance per literal argument list: `Channel(1)` is the same
  object everywhere, `Channel(2)` a second one, and a non-literal argument is an error. A singleton is bound as an attribute (`var journal = Journal()` beside the
  others) and used through the name. `Journal().record(entry)`, `Build().program`, `keep(Console())` and
  `return Console()` are errors (`'Console' is a singleton: bind it once beside the attributes`), and so are a
  binding nothing reads, even in a loaded package, and a binding inside a function (`'Build' is a singleton, bound
  here as a local`). One of the program's own that a `Parallel` reaches is made thread-safe by the
  compiler; write no lock for it. Singletons whose attributes make each other, even through an ordinary object
  (`World` binds `Column<Entity>`, which makes an `Entity`, which binds `World`), are an error naming the circle
  (`singletons initialise each other in a circle`): bind one of them in the class that uses both instead.
- `value.memory` is where a named value lives (`.address`, `.bytes`, `.section`: `'heap'` or `'constant'`); a
  computed value has none (`give this value a name with 'var' first`). A container of your own is a generic class over `var heap = Memory.Heap()` (`allocate`,
  `resize`, `free`, each on a `Memory.Address`; the compiler places each allocation) and a
  `TypedMemory<$value_type>` (`read_value`, `write_value`, `release_value`, `value_bytes`), as
  `library/list.spite` is. `address.read_long(offset)` and the other reads and writes are for `library/` only
  (`'read_long' reads or writes the memory at an address, which only library/ does`); there is no `Memory()`.
- An object is made on the heap unless the line right after it names another allocator:
  `var spark = Particle()` then `spark.memory.allocator = arena` (`var arena = Memory.Arena(65536)` beforehand)
  makes it in the arena from the start. Later, it is an error (`'spark' was already used, so its allocator can no
  longer change`): copy it and set the copy's allocator. A number or a `String` gets none.

## Built in classes

`Console()` (`print`, `write`, `error`, `debug`, `read_line(): String?`; each value printed is its `to_string()`, so
a class prints once it declares `func to_string(): String`, and `debug` shows any value's state, a class as
`Name { attribute: value }`, through the `to_debug()` every value has), `File(path)` (`map(): MappedFile?` for a file too big to read: `size()`, `mapped[position]`, `read_long(position)`, ... each a `T?`; `read(): String?`, `write`,
`append`, `exists`, `remove`), `Directory(path)` (`path`, `entries(): List<Directory.Entry>` (each entry a `Directory` or a `File`, switched on),
`files`, `folders`, `exists`, `create`),
`Process(command, arguments)` (`run(): Integer`, `output()`: standard output only; each argument reaches the child whole, `-key=value with spaces` as `-key="value with spaces"` on Windows; `working_directory` and `environment_variables["NAME"] = "value"` set for the child alone), `Program()` (`exit(code)`, `sleep(milliseconds)`,
`environment(name): String?`). `Console` is a singleton: `Console()` is the same instance everywhere, bound once
as `var console = Console()`.
`print`, `error` and `debug` write their line out at once, so a log redirected to a file shows every line as it
happens; `write` waits for the next line end or `flush()`.
`Socket()` is TCP over IPv4 on every system: `listen_locally(port)`, `listen_everywhere(port)`, `listen_at(host,
port)`, `connect_locally(port)`, `connect(host, port)`, then lines (`read_line(): String?`, `write_line(text)`) or
bytes at a `Memory.Address` (`read_bytes(address, count): Integer`, `write_bytes(address, count)`), which wait.
A loop that must not wait (a game server's tick) calls `accept_client_now(): Socket?`, `read_line_now():
String?`, `read_bytes_now(address, count): Integer` (`0` is nothing yet) and `write_bytes_now(address, count):
Integer` (how many the system took). A peer that hung up is not an error: `socket.closed` turns `true`, reads
answer `0` or `null` and writes send nothing. Check `closed`, never a count of `-1`.
`Concurrent(function)` runs a function as a compile-time state machine and `Parallel(function)` on the thread pool: the handle stands
in for what the function returns and reading it is the wait (there is no `.wait()`: `an Integer has no function
'wait'`), `finished` answers without waiting, `finished_value(): T?` is the value once finished and `null` before
(never a wait, so a system that only collects finished work keeps `function_waits` false: `var found =
handle.finished_value()`, then `if found { }`), and dropping the handle waits for it. A `parallel_each_<member>()`
member may read only its own element's plain values, and a `Parallel(f)` only its own instance's plain values, its locals, singletons, a `Lock`, a `ThreadLocal` or an `Atomic<T>` (a shared whole number or `Boolean`: `read`, `write`, `add`, `exchange`, `compare_and_swap`); anything else is an error naming the attribute. One exception: `var crafter = Crafter(first)` then `var run = Parallel(crafter.craft)` hands the object over, so its task may keep lists of values (`List<Integer>`, `List<String>`) and objects of its own that it made itself; touching `crafter` after that line is `'crafter' was handed to 'Parallel(crafter.craft)', which keeps its 'recipe_ids' on another thread, so it is not used after that line`. Keep a list of ids as a `List<Integer>`, never as comma-joined text. A singleton a `Parallel` reaches locks each of its functions that touch its state, so never make and wait for a `Parallel` inside such a function when the work calls back into the same singleton: `'Columns.despawn_all' waits for 'Parallel(remover.run)' while it holds Columns's lock, ...`. Start and read the `Parallel` from a class that is not that singleton. Many calls to one singleton from a counted loop (`while index < count { columns.remove_row(index); index = index + 1 }`, nothing else locked or waited on in it) take its lock once for the whole loop, so such a loop is the cheap way to call a singleton many times; keep other singletons and waits out of it. A singleton's function that only reads (a lookup such as `at(row)`: locals, reads of lists, text and numbers, no calls out) runs beside other readers on other threads and waits only for writers, so keep lookups in small functions of their own, apart from the ones that change the singleton. There is no `async`/`await`: a function
that reads, sleeps or waits is an ordinary function, and the compiler suspends it there when something else can
run ([concurrency.md](https://github.com/Mortaro/spite-language/blob/master/docs/concurrency.md)).
`$system_type.function_waits("update_each")` is decided while compiling like `has_function` (and
`system.class.function_waits(...)` in a `Symbol<...>` walk): `true` when the function can reach a wait, so an engine
starts it as a `Concurrent` and polls `finished`, and calls it directly otherwise; `klass.function_waits(name)` asks
the same at run time. A system never says that it does IO. `Scheduler().resume_only_when_asked()` keeps
`Concurrent`s out of the stages: they then resume only at `Scheduler().run_ready()`, which the frame loop calls
between frames, or where one's value is read or its handle dropped. A loop polling `finished` in that mode must call
`run_ready()`, or it never ends.
`$system_type.function_writes_parameter("last_each", 1)` is decided while compiling the same way: `true` when the
function, or anything it calls, writes what its parameter number 1 (from 0) is given or anything reached through it
(`true` too where it cannot tell, such as a call through a function value); an engine writes `crash not
$system_type.function_writes_parameter("last_each", 1)` to make a system that breaks its rule a compile error that
names the writing line ([metaprogramming.md](https://github.com/Mortaro/spite-language/blob/master/docs/metaprogramming.md#asking-whether-a-function-writes-a-parameter)).
`JsonWriter(value).write(): String` writes JSON and `JsonReader<T>(text).read(): T?` reads it
(`read_or_crash(): T` halts instead), for any class, list, dictionary, enum, number, `Boolean`, `String` or `T?`;
`read` skips unknown keys, keeps defaults for missing ones, reads a camelCase or PascalCase key (`buyPrice`,
`BuyPrice`) into its snake_case attribute (`buy_price`) when no attribute has the key's exact name, so never
rename attributes to match foreign JSON, and is `null` on a value of the wrong kind; a `Symbol`
reads back only as a name the program already uses. Writing a `Float` or `Double` that is infinity or not-a-number
to JSON crashes showing the value and the attribute (`shown=inf	path=Order.price`): check the number first
if `null` is wanted. **Between Spite programs, and for files a Spite program reads back, use bytes instead**:
`BinaryWriter(value).write(): List<Byte>` (or `append_to(bytes)` to add to a buffer you have) and
`BinaryReader<T>(bytes).read(): T?`, which reads the next value each call and is `null` on bytes that are not a `T`;
a quarter of JSON's size and more than ten times faster, with no keys, so both ends must be built from the same
classes. `read_memory(address, count)` reads straight from a socket's buffer. To catch two ends built from
different classes, write `writer.schema()` (a `Long` the compiler works out from the classes, free to ask) once at
the start of a file or connection and compare it with `reader.schema()` before reading. Attributes named `_...` are left out
of both. A union, a `type` (`Anything` included) or a function value anywhere in what they see is a compile error at
the line that makes the writer or reader (`JsonWriter cannot write 'Owner': 'Owner.pet' is the union Pet, ...`):
keep what they see to the kinds above ([json.md](https://github.com/Mortaro/spite-language/blob/master/docs/json.md)). `Json` no longer exists: it is the two classes above.
Measure with `clock.elapsed_nanoseconds()`, the monotonic clock: a `Long`, no allocation, subtract two readings.
Time is stored as an `Instant` and nothing else: `clock.now()`, or `Instant(since_1970)` with
`var since_1970 = Duration(1710054000, 'seconds')`.
`Duration(90, 'minutes')` is exact time (no days: `Duration(1, 'days')` is an error); `Period(1, 'months')` is
calendar time, added to a `Date`,
`Time` is a clock reading and `DateTime(date, time)` both, none of them an instant. A zone only shows or
reads a local reading: `var zones = TimeZones()`, `zones.find("America/New_York"): TimeZone?`, `zones.utc()`,
`zones.fixed_offset(duration)`, then `zone.to_local(instant)`, `zone.to_text(instant)` and
`zone.to_instant(local, 'compatible')` (or `'earlier'`/`'later'`, always written). `TimeText()` reads ISO 8601
(`read_instant(text): Instant?`, `read_date`, `read_duration`, ...) and `to_string()` writes it. `Date(2023, 2,
29)` halts, so read outside text with `TimeText`, which answers `null` ([time.md](https://github.com/Mortaro/spite-language/blob/master/docs/time.md)).
`DynamicLibrary("ucrtbase.dll", 'identity', "")` calls a native library's functions as members
(`c_runtime.strlen(text)`, `_as_long`/`_as_double`/`_as_text` for wider results); the standard library's
`library/windows/`, `linux/` and `mac/` folders reopen the classes each system changes (docs/foreign_libraries.md).
C calls back into Spite without any C written: a function value given as a foreign call's argument is a C function
pointer for that call only (`c_runtime.qsort(values, count, 4, ascending)`); a function C keeps or calls from its
own thread is handed over as `var handler = ForeignCallback(owner.function, 'context_last')` (or `'context_first'`),
passing `handler.address` as the pointer and `handler.context` as C's user data, or `'no_context'` for a function
of a singleton when C gives none (a window procedure; the singleton tells objects apart by the handle C passes). A
callback takes and returns only numbers, `Boolean` (a 32-bit `BOOL`) and `Memory.Address`; text C passes is an
address read with `terminated_text()`. Keep the `ForeignCallback` in an attribute of the object that owns the C
registration and unregister in its `drop()`: C may call only while it is kept. A handed-over function follows a
`Parallel`'s rules (its own value attributes, locals, singletons). A COM-style interface is a `Memory.Heap` block
whose first word points at a table filled with `TypedMemory<Long>` and each `ForeignCallback`'s `address`.
Game maths (docs/game_maths.md): `Vector2`, `Vector3`, `Vector4` are made with their parts, `Vector3(1.0, 2.0, 3.0)`,
read as `x_value`, `y_value`, `z_value`, `w_value` (never `.x`), with `+ - * /` part by part, `scaled(factor)`,
`dot`, `cross`, `length()`, `normalized()`, `distance_to`, `linear_interpolate(target, amount)`. `Matrix4()` and
`Quaternion()` are the identity and are set in place: `matrix.set_transform(translation, rotation, scale)`,
`set_perspective(field_of_view, aspect, near, far)` (Vulkan: y down, depth 0 to 1), `set_look_at`,
`rotation.set_axis_angle(axis, angle)`, `set_euler(angles, 'xyz')`; `a * b` applies `b` first;
`matrix.transform_point(point)`, `rotation.rotate(vector)`, `inverse()` a `Matrix4?`. Matrices are column-major,
parts `column_0_row_0` to `column_3_row_3`. Each answer is a new object, so one step per line:
`var moved = velocity.scaled(delta)` then `position = position + moved`. Culling and picking: `AxisAlignedBox(lowest,
highest)`, `Plane()` with `set_point_normal`, `Frustum()` with `set_from_view_projection(matrix)` then
`intersects_box`/`intersects_sphere`, and `Ray(origin, direction)` whose `hit_plane`, `hit_box` and `hit_triangle`
answer a `Float?` distance, `null` for a miss. Colours: `Color(red, green, blue, alpha)` from 0 to 1, read from
text with `var color_text = ColorText()` then `color_text.read("#ff6347")` (also `rgb()`, `rgba()`, `hsl()`,
`hsla()` and CSS names; a `Color?`), written with `to_hex()`, `to_rgb_text()`, `to_hsl_text()`, converted with
`to_linear()` and `to_standard_rgb()`. Animation and shading: `CubicBezier(start, first_handle, second_handle, end)`
with `y_at_x(x)`, `var easing = Easing()` then `easing.in_out_cubic(amount)` and the rest, `Noise(seed)` with
`gradient_2d(x, y)` and `interleaved_gradient`, and `value.to_half_precision()` / `bits.half_precision_to_float()`
for 16-bit floats. A number's raw bits are `Float.bits(): UnsignedInteger` and `Double.bits(): Long`, and back
`UnsignedInteger.bits_as_float()` and `Long`'s or `UnsignedLong`'s `bits_as_double()`; they cost nothing (a C
union), so use them rather than writing a value into memory to read its bytes.
Every class here, the numbers and `List` included, is a Spite file in `library/`, and a program's own file of the
same name reopens it: `list.spite` adds a member template, `integer.spite` a function on every `Integer`.

A statement is one line: anything after it on the same line (after a `return` value, a call, or the `}` that
closes an `if`, `while` or `switch`) is a parse error (`'attribute' is left over after the end of the statement`),
never a second statement.

## Habits from other languages that Spite rejects

Almost every one of these is a compile error naming the Spite form (the middle column is the start of what the
compiler says), so it will not pass silently; but each costs a round trip, and this table is cheaper to read than
to rediscover. The rows marked *silent* compile, and do something you did not mean.

| Written elsewhere | The compiler says | Written in Spite |
|---|---|---|
| `a && b`, `a \|\| b`, `!a` | `Spite writes 'and' and 'or' as words` / `Spite writes 'not' as a word` | `a and b`, `a or b`, `not a` |
| `a << 3`, `a >> 3`, `a & mask`, `a \| mask`, `a ^ mask`, `~a` | `Spite has no '<<': bits are functions on the whole numbers` (each names its function) | `a.shifted_left(3)`, `a.shifted_right(3)`, `a.bits_and(mask)`, `a.bits_or(mask)`, `a.bits_exclusive_or(mask)`, `a.bits_inverted()` |
| `x.sqrt()`, `x.sin()`, `x.atan2(y)`, `x.pow(y)`, `x.abs()`, `a.min(b)`, `x.ceil()`, `x.isnan()`, ... | `Float has no function 'sqrt': Spite spells it 'square_root', since no name is abbreviated` | every name in full: `square_root()`, `sine()`, `cosine()`, `tangent()`, `arc_sine()`, `arc_cosine()`, `arc_tangent()`, `power(y)`, `exponential()`, `logarithm()`, `logarithm_base_2()`, `logarithm_base_10()`, `absolute()`, `minimum(b)`, `maximum(b)`, `ceiling()`, `truncate()`, `is_not_a_number()`, `is_infinite()`, `is_finite()`; `floor()`, `round()` and `clamp(low, high)` keep their names |
| `Math.atan2(y, x)`, `atan2f(y, x)` | `Spite has no 'Math'` | `y.arc_tangent_over(x)`: the angle of the point `(x, y)` |
| `Math.sqrt(x)`, `Math.PI` | `Spite has no 'Math': maths is a function of the number itself, like 'value.square_root()'` | `x.square_root()`, `Float.pi` |
| `sqrt(x)`, `sqrtf(x)`, `pow(x, y)` | `this class has no function 'sqrt': maths is a function of the number itself` | `x.square_root()`, `x.power(y)` |
| `M_PI`, `f32::consts::PI`, `FLT_MAX`, `INT_MAX`, `INFINITY`, `NAN` | `unknown identifier 'M_PI'` | `Float.pi` (`Double.pi` for the 64-bit one), `Float.largest`, `Integer.largest`, `Float.infinity`, `Float.not_a_number` |
| `angle.pi`, `Float.pi()` | `'pi' is a constant of the class Float, not of a value` | `Float.pi` |
| `count++`, `count += 1` | `Spite has no '++': write 'count = count + 1'` | `count = count + 1` |
| `condition ? a : b` | `Spite has no 'condition ? a : b'` | an `if` with an `else`, or a function that returns one or the other |
| `int`, `Int`, `bool`, `Bool`, `to_int()` | `'Int' is spelled 'Integer'`, `'int' abbreviates: write 'integer'` | `Integer`, `Boolean`, `to_integer()`: no name is abbreviated, the language's own included (D122) |
| `LocalDate`, `PlainDate`, `NaiveDate` | `'LocalDate' is spelled 'Date'` | `Date`, `Time`, `DateTime`: a calendar reading with no zone (D160) |
| `let x = 1`, `const x = 1` | `Spite has no 'let': a variable is declared 'var name = value'` | `var value = 1`: every `var` has a default, and there is no `const` |
| `var total: Integer` | `expected '=' (every variable needs a default value)` | `var total = 0` |
| `new Monster()` | `there is no 'new' in Spite` | `Monster()` |
| `this.name`, `self.name`, `this.helper()` | `a class reads its own attributes by name: write 'count', not 'this.count'` | `name`, `helper()`; `this` only passes the object itself (`registry.append(this)`) |
| `import`, `require`, `load("folder")` | `there are no imports`; `'load' is a keyword, not a function` | `load "folder"`, a keyword on its own line inside a function |
| `elif` | `Spite writes 'else if'` | `else if` |
| `class Monster { }` | `there is no 'class' keyword: a file is a class` | nothing: the file *is* the class |
| `func greet(name: String = "world")` | `a parameter has no default value in Spite` | a second function, or an attribute holding the value |
| `func one() { return 1 }` | `'one' returns nothing, so 'return' cannot carry a value here` | `func one(): Integer { return 1 }` |
| `print(value)` | `this class has no function 'print': printing goes through the console` | `var console = Console()` beside the attributes, then `console.print(value)` |
| `Console().print(value)`, `Build().program` | `'Console' is a singleton: bind it once beside the attributes` | `var console = Console()`, `var build = Build()`, then `console.print(value)`, `build.program` |
| `Report(text)` as a statement, to "do" something | `'Report(text)' makes a 'Report' and drops it` | a function (`report(text)`) on the class that needs it |
| `toString()`, `__str__`, `Display` | (nothing calls it) | `func to_string(): String` in the class, which `console.print` and text holes call |
| `console.log(object)`, `dbg!`, `__repr__`, `{:?}` | | `console.debug(value)`: every value has `to_debug()`, and a class may declare its own |
| `"hello " + name` | `text written down is not joined with '+'` | `"hello {name}"`; two values still join with `+` |
| `label.text = "{clicks}"`, to turn a value into text | `'"{clicks}"' is a text of one value and nothing else: assign the value directly, 'label.text = clicks'` | `label.text = clicks`: a value casts to text where text is wanted; `clicks.to_string()` elsewhere |
| `"hello ${name}"` | *silent*: prints `hello $` and the name | `"hello {name}"` |
| `'hello'` for text | `'hello' in single quotes is a symbol, and text is wanted here: text is always written in double quotes` | `"hello"`: single quotes are symbols and enum values |
| `flags.bits_and(mask)` with a wider `mask` | `'flags.bits_and(mask)' works in a Byte, since a bitwise function takes its receiver's type` | widen the receiver first: `var wide: Long = flags` |
| `var held: $held_type = null` bound to a `type` that requires a function | `'held' is a Weapon, a type that requires the function 'strike', so '= null' has no default to make` | `$held_type?`, or set it in the constructor |
| a `while true` frame loop in a singleton a `Parallel` reaches | `'Window.run' holds Window's lock for the whole call ... and this loop never ends` | put the loop in a plain object that calls the singleton each pass |
| a `Weak` in something a `Parallel` runs | `'Parallel(tree.grow)' runs on another thread, and what it runs reaches 'Branch', which is a Weak or is held by one` | hold the object itself in the work; keep `Weak` on the program's thread |
| `for item in list` | `Spite only has 'while' loops; there is no 'for'` | `each_<member>()`, `map_`/`filter_<member>()`, or `list.each(function)` with a function of yours; `while index < list.count()` when the body needs more |
| `break`, `continue` | `Spite has no 'break': a loop stops in its own condition` | the loop's own condition: `while index < count and not found` |
| `list.add(x)`, `list.pop()` | `List has no method 'add', which does not say where` | `append`/`prepend`, `remove_last()`/`remove_first()` |
| `Heap<Node>`, `&Node` | `there is no 'Heap<T>'`, `Spite has no '&Type'` | `Node`: every class is a reference already |
| `if value do name { }` | `Spite has no 'do'` | `if value { }`: the name itself is narrowed inside |
| `items.map(item => item * 2)` | `expected ')' to close the argument list` | no lambdas: pass a named function, `items.map(doubled)` |
| `value == null`, `value != null` | `'null' is not a value to compare against or pass around` | `if value { } else { }`, `assert value`, `crash value`, or `switch` |
| `task.wait()`, `await task` | `an Integer has no function 'wait'` | read the handle: it is the result, and reading it waits |
| `new Date()`, `DateTime.Now`, `datetime.now()` | | `clock.now()`, an `Instant`; shown through a zone from `TimeZones()`, never stored as a local reading |
| `timestamp + 86400000` for tomorrow | *silent*: wrong across a daylight-saving change | `zone.to_local(instant) + Period(1, 'days')`, then `zone.to_instant(tomorrow, 'compatible')` |
| `text[0]` | `text is not indexed with [ ]` | `text.character_at(0)`, or `text.slice(start, end)` |
| `// comment`, `/* comment */` | `Spite has no '//' or '/* */' comments` | nothing, or `# docs/page.md#section` on its own line outside a function |
| `;` at the end of a line | `';' is not something Spite reads` | nothing |
| `var memory = Memory()` | `this class has no function 'Memory'` | `var heap = Memory.Heap()` (inside a container of your own; see [memory.md](https://github.com/Mortaro/spite-language/blob/master/docs/memory.md)) |
| `--repl_port=4000` | `'--repl_port' is written '--repl-port'` | kebab-case flags; the `Build` field behind it stays `repl_port` |
| `spite game -- --player-name=ada` | `'--' is not a separator` | `spite game --player-name=ada`: settings sit beside the compiler's flags |
| `spite game --player_name=ada` | `'--player_name' is written '--player-name'` | settings are kebab-case on the command line, like flags |
| `spite game/game.spite` | `'game/game.spite' is a file, and a program is named by its folder` | `spite game` |
| renaming `short`, `static` or `near` because C takes them | | the name you meant: Spite reserves nothing for C (D168) |

Text is written with its values inside it: `"hello {name}"`, where `{ }` holds one value of any type and
`\{` is a brace meant literally.
