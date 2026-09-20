# Cheat sheet for AI writers

Paste this page into context before generating Spite. Every rule below is enforced by the compiler, not a
style guideline -- violating one is a compile error, not a warning.

## Rules that differ from mainstream languages

- One `.spite` file = one class, named by PascalCasing the file name. No `class` keyword. No two classes per file.
- No file-level statements. Only `generics`/`var`/`func`/`type`/`enum`/`union` at file scope.
- No `main`. The entry file's class is constructed; that construction *is* the program.
- `spite folder/file.spite` loads `folder/` as a root: the file's own folder is the namespace root, and every
  sub folder is a namespace, recursively -- the same as `load("folder")` would load it. The entry class is the
  PascalCase of the file's own name (`game/game.spite` runs `Game`).
- `return` is always explicit, and the return type is always `(): Type` (with the colon). No implicit return of
  a trailing expression, no `() Type` without a colon.
- A function whose body falls off the end without `return` gets the return type's default -- silently, no
  diagnostic. If you meant to return something, write `return`.
- There is no `for`. Only `while`. Index with `while index < list.count() { }`, or reach for a `List<T>`/
  `Dictionary<T>` metaprogramming helper (`filter_`/`count_`/`sum_`/`find_by_`/`sort_by_`/`each_`/`map_`/
  `any_`/`all_`) first.
- Casting is always right-side-toward-left-side, with no cast syntax at all. `age > 0.5` with an `Int age`
  compares against `0` (`0.5` truncates), not `0.5`.
- `null` only exists for `Nullable<T>`. Unwrap with `if value { }` (narrows `value` itself to `T` inside the
  block, with an `else` for when it is null) or `assert value` (narrows for the rest of the block). There is no
  `do` keyword any more -- `if value do name { }` is a parse error naming the `if value { }` form.
- **Lint, not a style choice:** a function whose *last* statement is `if value { ... }` (no `else`) used only to
  check existence is a compile error. Use `assert` instead.
- **Shadowing is allowed**, redeclaring in the same scope is not: an inner `var name` may reuse an outer
  local/parameter/attribute's name, but two `var name` in the *same* scope is still an error.
- **Unused is an error**, not a warning: an unused local variable or parameter is a compile error unless its
  name starts with `_` (and a `_name` that *is* used is also an error -- remove the underscore). Exempt:
  parameters whose signature is dictated from outside -- operator functions (`sum`, `equals`, `get_at`, ...),
  Symbol codegen templates, setters/getters that answer attribute access (`person.age = 1` calls `set_age`),
  and functions that replace another through class reopening.
- Every operator (`+ - * / % == != < > <= >=`, unary `-`, `a[x]`, `a[x] = v`) is a function a class can define
  (`sum`, `subtract`, ..., `get_at`, `set_at`). `not`/`and`/`or` stay built-in keywords, never functions.
- `person.field = value` / `person.field` (read) from *outside* a class go through `set_field`/`get_field` when
  the class defines them (exact function, or Symbol codegen) -- from inside the class, `field = value` always
  stays a raw field access.
- **No abbreviations, ever**, except the keywords themselves. `msg`, `cfg`, `idx`, `str`, and roughly fifty more
  are compile errors naming the full word. Single-letter names are always errors too.
- Everything is formatted for you. Do not hand-indent, hand-wrap, or fuss over blank lines -- the compiler
  rewrites the file to the one true style before compiling. Do not fight it; write reasonable code and let it
  format.
- Reference counted, not garbage collected: a scalar is copied, everything else (a class instance, `List<T>`,
  `Dictionary<T>`, `String`, a union, an object literal) is a reference -- assigning/passing/storing one shares
  the exact same object, visibly (mutating it through one name shows up through every other name that holds it).
  `copy()`/`deep_copy()` make an independent object when you want one; `drop()` runs once, right before the last
  reference is freed, if the class defines it. A cycle (two objects holding references to each other) leaks --
  break it by hand (see [memory.md](memory.md)).
- A class/union can contain itself directly now -- no special wrapper type needed for that (`&Type`/`Heap<T>`
  no longer exist; a plain `Type` already means "a reference to it").

## Lints (compile errors, each with a suggested fix)

| Lint | Example that fails |
|---|---|
| non-`snake_case` variable/attribute/function/parameter | `var userName = ""` |
| non-`PascalCase` class/type/enum/union | already correct by construction for classes (from the file name) |
| single-letter name | `var x = 0` |
| abbreviation | `var msg = ""` (spell out `message`) |
| terminal `if value { }` (no `else`) | see [values_and_types.md](values_and_types.md)'s `assert`-narrowing section |
| `for` | parse error naming `while` and the metaprogramming helpers |
| `if value do name { }` | parse error naming `if value { }` (see [control_flow.md](control_flow.md)) |
| unused local/parameter not prefixed with `_` | `func f(count: Int) { }` where `count` is never read |
| a used `_name` local/parameter | remove the underscore |
| `) Type` return type without a colon | parse error naming `(): Type` |

## Common compile errors and their fixes

```spite fragment
# error: "'&Type' is not supported: references are the default now, so plain
# 'Type' already means what '&Type' used to -- remove the '&'"
func broken(target: &Widget) {
    target.rename("new name")
}

# fix: drop the '&' -- a reference is already the default
func fixed(target: Widget) {
    target.rename("new name")
}
```

```spite fragment
# Not a compile error, the one behavior most likely to surprise you: passing/
# assigning a class instance never copies it, so a mutation through one name
# is visible through every other name holding the same object.
func surprising() {
    var original = Widget("a")
    var alias = original
    alias.rename("b")
    console.print(original.name)     # prints "b": alias IS original, not a copy
}

# fix: copy() first for an independent object
func fixed() {
    var original = Widget("a")
    var independent = original.copy()
    independent.rename("b")
    console.print(original.name)     # prints "a"
}
```

```spite fragment
# error: "'Focused' does not define a 'sum' function needed for '+': define func sum(other: Focused): Focused"
# fix: define the operator function the diagnostic names
func sum(other: Focused): Focused {
    return Focused(value + other.value)
}
```

```spite fragment
# error: variable 'msg' uses an abbreviation; no abbreviations, spell it out: 'message'
var msg = ""

# fix
var message = ""
```

## Idioms

- Prefer `repositories.filter_active().sum_stars()` over a `while` loop and manual accumulation, whenever the
  class already has the field the helper name needs.
- `List<T>` is `append`/`prepend`/`remove_last`/`remove_first`, never `add`/`pop` -- those old names are compile
  errors naming the replacement.
- Give every class an explicit constructor only when it needs one; a class with no matching-name function gets
  a free no-argument constructor from its field defaults.
- Use `assert` for "this must be true or bail with the default" at the top of a function, not nested `if`s.
- Reach for `type` (structural matching) when you want to accept "anything shaped like this," including a
  plain object literal, instead of requiring a specific class.
- Keep a `Symbol`-codegen getter (`get_attribute`) to scalar/enum attributes; write an exact function for a
  `String`/`List<T>`/`Dictionary<T>` getter instead of letting Symbol codegen generate it (see
  [KNOWN_ISSUES.md](KNOWN_ISSUES.md)).
- `count()` is always just the size of a collection. `count_<attribute>()` only exists for a `Bool` attribute
  (counts how many elements have it `true`); on a numeric attribute it is a compile error naming
  `sum_<attribute>()` instead.

See [manual.md](../manual.md) for the normative rules this sheet compresses, and
[KNOWN_ISSUES.md](KNOWN_ISSUES.md) for where the real compiler's behavior surprised even careful reading of the
manual.
