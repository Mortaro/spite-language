# The Spite Language Manual

This is the normative description of Spite. `PLAN.md` tracks compiler implementation details and milestones.
Every language decision gets written here (and logged in [Decision log](#decision-log)) when it is made.

Status tags: **[implemented]**, **[partial]**, **[planned]** refer to the compiler.

---

## 1. Philosophy

- Spite is written mostly by AI and skimmed by humans. Code must read easily; it does not need to be pleasant to type.
- The language is extremely opinionated and small. There is one way to do each thing. There are no macros and no clever features.
- Readability comes from metaprogramming and the standard library: prefer `repositories.filter_active().count_stars()` over loops and ifs.
- No abbreviations, anywhere, except the language keywords themselves (`var`, `func`, `enum`).
- Compilation speed and live reload matter more than anything else in the toolchain.
- Spite compiles to C (later possibly LLVM). The compiler is written in Spite and compiles itself,
  and the standard library follows it: the hand-written C runtime is a bootstrapping stage, not the design (D14,
  [Pure Spite](#pure-spite-dissolving-the-runtime-planned)).

### Self hosting  **[partial]**

The compiler is written in Spite and compiles itself. `bootstrap/seed/spite_compiler.c` is the C it emits for
its own sources, so a C compiler is all that is needed to build it, and `bash check.sh` requires generation 2
and generation 3 to be byte identical before anything else is believed. See `docs/self_hosting.md`.

The language is larger than the subset the compiler implements today; `bootstrap/COMPILER_PLAN.md` is the
progress log, and `conformance/` is the part that demonstrably works.

## 2. Lexical structure  **[implemented]**

- Newlines end statements and separate entries in lists, objects, enums, unions and types. Commas separate entries written on one line.
- Comments start with `#` and run to the end of the line.
- `"text"` is a String. Strings only use double quotes.
- `'value'` is an enum value. Single quotes are only for enum values.
- `name` (lowercase, snake_case) is a variable, attribute, function or folder.
- `Name` (uppercase first letter) is a class or type, including the whole standard library.
- `$name` is a codegen value: it is replaced at code generation time (see [Codegen values](#9-codegen-values-)).
- `_name` is private. (This may change.)
- Keywords: `var func return if else while switch type enum union assert crash and or not null true false` — there is
  no `do` (second batch item 2, decided 2026-09-19: with `for` gone, `if value do name` had nothing to match, so
  `if value { } else { }` narrows in place instead, section 5). Writing `do` is a parse error naming the
  `if value { }` form.

## 3. Files are classes  **[implemented]**

Each `.spite` file is exactly one class, named by the file name in PascalCase (`repository_list.spite` is `RepositoryList`).
This cannot be changed.

A file contains only declarations:

- `var` declarations: the attributes of the class
- `func` declarations: the functions of the class
- `type`, `enum` and `union` declarations, which are namespaced under the class (`Player.Job`)

There are **no file level statements**: no loops, ifs or calls outside a function.

```person.spite
var age = 0

func Person(new_age: Int) {
    age = new_age
}
```

### Constructors and the entrypoint

A function named like its class is the constructor. A class without one is constructed with `Person()` and just gets its defaults.
There is no `main`: a program is run by constructing the entry file's class.

```kal.spite
func Kal(arguments: Arguments) {
    if arguments.some_key {
        console.print(arguments.some_key)
    }
}
```

Configuration files are ordinary classes whose constructor sets the state.

## 4. Variables and values  **[implemented]**

```
var variable_name = "lorem ipsum"
var inline_list = [1, 2, 3, 4]
var multiline_list = [
    "no"
    "commas"
    "between"
    "lines"
]
var object_name = {
    key: "value"
    other_key: 3
}
var target: Monster? = null
```

- Every variable has a default value. The type is inferred from it, or annotated with `: Type`.
- Every class has a default value (`Int` 0, `Float` 0.0, `Bool` false, `String` "", a class: its attribute defaults).
  Operations that cannot succeed produce the default instead of crashing -- except reading with `[]`, which
  answers `T?` (D64): an index or key that may not be there is a value that may be null, narrowed like any other.
- `null` exists only as the empty state of `T?`. See [Open questions](#open-questions) for `= null` on other types.
- An inner `var` may shadow an outer local, parameter, or attribute with the same name (second batch item 4,
  decided 2026-09-19), **and a `var` may shadow a name in the same scope too** (D51, decided by Mortaro,
  2026-09-20). The second binding may hold a different type, which is what makes it worth having:

```
var content = file.read()        # String?
assert content
var content = content.trim()     # String
```

  **The order is: evaluate, then drop, then bind.** The new value is computed first -- so `var content =
  content.trim()` reads the binding it is about to replace -- then the previous value is released, running its
  `drop()` if that takes it to zero, and only then does the new binding take effect. Releasing first would make
  the common case a use-after-free.

  The unused rule (section 5) still applies to the binding being shadowed: shadowing a name that was never read
  is an error, which is what catches an accidental reuse rather than a deliberate one.

### Casting

There is no cast syntax. The right side is always cast toward the left side.

```
func sum(a: Int, b: Float): Int {
    return a + b        # b is cast to Int
}
```

This applies to binary operations, assignment, arguments (toward the parameter type) and `return` (toward the return type).

### Numeric types  **[implemented, PROVISIONAL]**

All the basic types a language has, with written names (decided 2026-09-19:
"never `u64`"). **(proposed by Claude, unconfirmed: the exact width mapping below.)**

| Type | C type | Notes |
|---|---|---|
| `Tiny` | `int8_t` | |
| `Short` | `int16_t` | |
| `Int` | `int32_t` | the default integer type |
| `Long` | `int64_t` | |
| `Byte` | `uint8_t` | |
| `UnsignedShort` | `uint16_t` | |
| `UnsignedInt` | `uint32_t` | |
| `UnsignedLong` | `uint64_t` | |
| `Float` | `float` (32-bit) | the default decimal type |
| `Double` | `double` (64-bit) | |

An integer literal defaults to `Int`; one too large to fit becomes a `Long` instead. A decimal literal
defaults to `Float`. All of them follow the same right-side-casts-toward-left-side rule as everything else
(`var tiny: Tiny = some_int_variable` narrows with an ordinary cast); converting between two numeric types
wraps on overflow (an out-of-range value assigned into a narrower type keeps its low bits, the same as a plain
C cast) rather than crashing or saturating. `List<T>`, `Dictionary<T>`, `T?`, and generics all work
with every numeric type; so does `String` conversion both ways -- casting a `String` to any numeric type by
assignment parses it (defaulting to `0`/`0.0` on failure, like `Int`/`Float` always did), and `String` gains
one `to_<name>()` method per type (`to_tiny()`, `to_short()`, `to_int()`, `to_long()`, `to_byte()`,
`to_unsigned_short()`, `to_unsigned_int()`, `to_unsigned_long()`, `to_float()`, `to_double()`) alongside the
existing `to_int()`/`to_float()`. `count()`, `length()`, and `index_of()` always return `Int`, never a wider
type.

Printing a `Float`/`Double` uses shortest-round-trip formatting (try the fewest significant digits that parse
back to the exact same value) rather than a fixed number of digits, so switching `Float` from 64-bit to 32-bit
does not make an ordinary value like `0.1` print with ugly trailing noise (`0.100000001`) -- it still prints
`0.1`, exactly as before this table existed. **PROVISIONAL** because the exact widths were not explicitly
confirmed by Mortaro; revisit if a different mapping is wanted.

## 5. Functions  **[implemented]**

```
func function_name(first: Reference, second: Value): Tiny {
    return 1
}
```

- The return type is written `(): Type`. No other form is valid.
- `return` is always explicit. There is no implicit return of the last expression. A function with a
  return type whose body falls off the end without a `return` gets the defensive default return of its
  return type, with no diagnostic.
- A function with no return type returns nothing.
- **Parameters** (D1, decided by Mortaro 2026-09-19): a scalar (every numeric type, `Bool`, an enum value) is
  passed by value, copied. Everything else -- a class instance, `List<T>`, `Dictionary<T>`, `String`, a union, an
  object literal -- is passed by reference: the caller writes nothing special, and the callee shares the exact
  same object (mutating it through the parameter is visible to the caller). `&Type` no longer exists as syntax --
  writing it is a parse error saying references are the default now. A copy is always explicit: `copy()`/
  `deep_copy()` (section 10).
- `assert condition` is a production feature, not a debug one: when the condition is falsey the function returns
  the default value of its return type immediately.

```
func sum_positives(a: Float, b: Float): Float {
    assert a > 0 and b > 0
    return a + b
}
```

### Null safety and `assert` narrowing  **[implemented]**

`assert` doubles as the way to prove a `T?` is not null without nesting: after `assert value` on a
`T?` local, parameter, or field, `value` is a plain `T` for the rest of that block and any block
nested inside it -- reads, writes, and method calls all go straight to the value the `T?` holds, and ownership
and dropping still belong to that `T?` (assigning a new `T` through the narrowed name drops the old one first,
exactly like overwriting any other owning slot). `assert value and
other_condition` narrows `value` too, and `other_condition` itself already sees the narrowed type. Reassigning
the narrowed name to `null` afterward is a diagnostic (there is no way back to `T?` in the same
scope).

```
var content = program_file.read()
assert content
console.print(content.length())        # content is a String here, not String?
```

`if` on a `T?` narrows the same way, in place, for the whole block: `if value { } else { }` runs the
block with `value` already a plain `T`, and the `else` exactly when it is null/absent. One rule for narrowing
everywhere -- `assert`, `if`, and `switch` all read/write/call straight through to the value the `T?`
still owns. (Second batch item 2, decided 2026-09-19: `if value do name { ... }` is gone -- `do` is removed, so
this is the only form.)

**Lint:** a function whose body's *last* statement is `if value { ... }` **with no `else`** -- wrapping
the rest of the function only to check existence -- is a compile error, because it should be written with
`assert` instead (a narrowing `if` with an `else`, D3, already handles the missing case explicitly, so the no-
else lint never applies to it):

```
# error: rewrite this with assert
func read_first_line(file: File): String {
    var content = file.read()
    if content {
        return content.lines().first()
    }
}

# the fix:
func read_first_line(file: File): String {
    var content = file.read()
    assert content
    return content.lines().first()
}
```

A narrowing `if` that is not the function's last statement (more statements follow it), or one nested inside
another statement (an outer `if`/`while`/switch case) rather than being the function body's own tail, is not
a lint error.

**`assert` narrows every link of a chain, not only the last** (D43, decided by Mortaro, 2026-09-20). Walking a
nullable structure would otherwise need one `assert` per hop, and a thousand asserts is not a language, it is a
tax:

```
assert class.namespace.namespace
console.print(class.namespace.namespace.name)
```

One `assert` proves the whole path, so `class.namespace` and `class.namespace.namespace` are both plain values
for the rest of that block and any block nested inside it. It is the same rule section 5 already has for
`assert value and other_condition`, applied along a member chain instead of across an `and`.  **[implemented]**

- It narrows the path asserted **and its prefixes**, nothing else. A sibling path stays `T?`: asserting
  `class.namespace.namespace` says nothing about `other_class.namespace`.
- A local holding a copy is its own path: `var ns = Hello.namespace; assert ns` narrows `ns`, and a later read
  of `Hello.namespace` is a fresh `Spite.Namespace?` that must be narrowed in its own right --
  `diagnostics/namespace_nullable` is exactly that shape. Since D63 the copy itself is an error: narrow the
  path, not a local holding it.  **[implemented]**
- A check on something that cannot be null proves nothing and is an error naming the fix: `assert tracker`
  written twice, or `crash` on a path an earlier `crash` already narrowed (`diagnostics/check_proves_nothing`).
  **[implemented; proposed by Claude, unconfirmed]**
- `crash` (below) narrows a chain the same way, since it narrows exactly as `assert` does but never returns.
- Reading through a link that may be null and has not been narrowed is still an error. This removes the verbosity of
  proving a path, not the requirement to prove it.
- Assigning to a narrowed path, or to anything it reads through, undoes the narrowing from that point on: after
  `outer = Box()` or `outer.inner = null`, `outer.inner` is a `Box?` again. Assigning a value that cannot be null
  (a constructor, a literal, a name that is not `T?`) to the narrowed path itself keeps it narrowed, and still
  undoes everything narrowed beneath it.  **[implemented; proposed by Claude, unconfirmed]**
- Inside a `while`, undoing a narrowing that was made before the loop *and read inside it* is an error, because
  the next pass would read it unproven: narrow it inside the loop instead (`diagnostics/path_narrow_loop`).
  **[implemented; proposed by Claude, unconfirmed]**


**Reading with `[]` answers `T?`** (D64, decided by Mortaro, 2026-09-23). `names[index]` and `table["key"]` may
not be there, so they are values that may be null, and an index or key that is a name, a path, a number or
quoted text makes the read a path that narrows like any other: `crash names[index]`, then `names[index].upper()`.
How a read is proven (the rules are Claude's proposal, unconfirmed -- D64 asked for them):

- **A proven count proves the indices below it.** After `crash names.count() == 3` (or `>= 3`, or `> 2`),
  `names[0]` to `names[2]` are plain values; so are they inside `if names.count() > 2 { }`.
- **A bound proves its index.** `index < names.count()` (or `names.count() > index`) in an `assert`, a
  `crash`, an `if`, or a `while` proves `names[index]` in what follows -- the loop body, for a `while` -- and
  `index < names.count() - 1` does too. On the left of an `and`, it proves the right side, so
  `while index < lines.count() and lines[index] != "end"` needs nothing more.
- **Assigning the list or the index undoes it**, as for any path (section 5, D43): `index = index + 1`
  un-proves `names[index]`, and so do `names.clear()`, `remove_at`, `remove_first` and `remove_last`. Inside a
  loop, undoing a proof made before the loop that the loop has read is an error.
- **A proven read still checks its bounds at runtime** and answers the default out of range, so a counter that
  went negative gives a wrong value rather than reading memory it does not own.
- **A `Bool?` cannot be a condition** -- not in `if`, `while`, `assert` or `crash`, and not under `not`, `and`
  or `or`: `if flags[index]` would test that the element is there, not that it is true, and the two mean
  opposite things for `false`. Prove the element is there first, or `switch` over it; `== true` also works,
  since comparing needs no narrowing.
- Writing through `[]` is unchanged, and **assigning through a `T?` is an error** naming the fix
  (`diagnostics/store_through_nullable`): until D64 `outer.inner.label = "x"` on a `Box?` was silently
  dropped, emitting nothing at all.

`first()` and `last()` still answer the default on an empty list; whether they should answer `T?` too is
open (Claude would say yes, by the same argument). `tests/list_tests`, `diagnostics/index_reads`.
**[implemented]**

**Comparing needs no narrowing** (D69, decided by Mortaro, 2026-09-23). `==` and `!=` accept a `T?` on the left:
null is not equal to anything, so `crash Spite.Class.namespace == "Spite"` is a whole test. Either side may be the `T?`. Only the comparison
is exempt -- reading a member through a `T?` still needs it narrowed first. A `Spite.Namespace` compares with
text through `equals(String)` in `library/spite/namespace.spite`, against its `name_with_namespaces`; two
namespaces still compare by identity, because a class's `equals` is used for a right side of that same class
only when `equals` takes that class.  **[implemented]**


### Unused is an error  **[implemented]**

Second batch item 5 (decided 2026-09-19): a local variable or parameter that is never read is a compile error
unless its name starts with `_`. An unused `_name` is fine; a `_name` that *is* used is also an error, naming
the fix (remove the `_` or the read). Parameters whose signature is dictated from outside are **exempt** from
the read requirement, because the author never chose them:

- operator functions (`sum`, `subtract`, `equals`, `get_at`, ... -- the Operators table above): `a + b` calls
  `sum(b)`, so the parameter is the operator's, not the author's
- Symbol codegen templates (`set_attribute`, `get_attribute`, ...): the whole signature is the template
  mechanism's -- one instantiation per attribute, never emitted as the author wrote it
- setters that answer attribute access: a one-parameter `set_age` naming a real `age` attribute answers
  `person.age = 1` (above), so its parameter is the write's, not the author's
- functions that replace another through class reopening (section 11): the signature is dictated by the
  earlier root's call sites

A `set_age` with two parameters never answers attribute access (the dispatch passes exactly one value), so its
parameters are the author's free choice and the ordinary rule applies.

### Functions are values, always bound to an instance  **[implemented, except `.owner`]**

D17 (decided by Mortaro, 2026-09-19): a function is a first-class value. Naming one inside a class passes it
together with the instance doing the passing, so it runs exactly as that instance would have run it:

```
console.log(pretty_print)        # passes this instance's pretty_print
```

- **There are no free functions and no closures.** A function value is `{instance, function}` -- one retain in a
  reference-counted language (section 10), capturing the receiver and nothing else, so no local ever escapes its
  scope. That is the whole of the feature; there is no environment to capture, no lifetime to reason about, and
  no allocation beyond the retain.
- **It makes foreign callbacks expressible.** `{instance, function}` is precisely what a C callback plus its
  `void*` user data wants, which is what section 17 lists as not designed yet.
- **An event handler can be the function itself** -- `onclick: increment` -- checked by signature, rather than
  the symbol `'increment'` (section 7) resolved by name. Symbol literals stay useful elsewhere.

**A function value is an instance of `Spite.Function`** (D39, decided by Mortaro, 2026-09-20), generic over its
arguments and its return, so the type of a function is written the same way any other generic type is:

```
func log(printer: Spite.Function<String, Bool>)
```

The codegen values are positional and the **last one is the return**; everything before it is an argument, in
order. `Spite.Function<Bool>` takes nothing and returns a `Bool`; `Spite.Function<String, Int, Bool>` takes a
`String` and an `Int` and returns a `Bool`. They line up with `Spite.Function`'s own reflection members
(section 8, D12): the arguments are `.arguments`, the last is `.returns`. So there is nothing new to learn --
the type of a function is its reflection, written down.

That is the point of the choice: **the value and its reflection are the same class.** `Weapon.functions[0]` and
a `pretty_print` passed as an argument are both `Spite.Function` instances, so reflection stops being a mirror
of the language held alongside it and becomes the language describing itself -- which is what "everything is a
class, including classes" (section 8) has always claimed. A function value carries `.name`, `.arguments` and
`.returns` because it *is* the reflection object.

This is the language's first **variadic** generic. `List<T>` and the rest take a fixed count declared on the
constructor (section 9); `Spite.Function` is a compiler built-in and takes as many as the signature has.

#### Calling one

D40 (decided by Mortaro, 2026-09-20): **a `Spite.Function` knows its owner, and every call goes through
`call_function()`.**

- `.owner` is the instance the function is bound to. D17's `{instance, function}` pair stops being a hidden
  representation and becomes an ordinary field you can read: `pretty_print.owner` is the `Console` that will
  run it. Bound-ness is reflectable, like everything else about a function.
- **`call_function()` is the one path.** Writing `printer(value)` is `printer.call_function(value)`, which is
  section 5's operator rule applied once more -- the call operator maps to a named function exactly as `+` maps
  to `sum` and `a[x]` maps to `get_at`. So there is one chokepoint every invocation of a function value passes
  through: an event handler firing, a foreign callback arriving from C (section 17), a framework dispatching.
  One place to instrument, and one place where the owner is applied.
- Because `Spite.Function` is an ordinary standard library class, `call_function` can be reopened (section 11,
  D7) to trace or count every callback in a program. That is the foot, and it is yours to shoot.

The indirection is paid only where it was already accepted. An ordinary call -- `person.set_age(2)` -- is a
direct call and never builds a `Spite.Function`; only a function used *as a value* goes through
`call_function()`.

**`Nothing` is the class a function returns when it returns nothing** (D48, decided by Mortaro, 2026-09-20).
It is required rather than optional: with the return in the last position, `Spite.Function<String>` already
reads as "takes nothing, returns a `String`", so a function taking a `String` and returning nothing could not
otherwise be written. `Spite.Function<Nothing>` takes nothing and returns nothing.

**As implemented** (2026-09-23): naming a function without calling it -- `greet` inside the class, `person.greet`
on a value -- builds a `Spite.Function` bound to that instance, whose type carries the signature; `change(text)`
and `change.call_function(text)` call it with their arguments checked; a typed value can go where a plain
`Spite.Function` (a reflected one) is expected, but not the other way round, since a reflected function's
signature is not known to the type checker (`diagnostics/function_value_mistakes`). In C it is still one
`Spite_Function` with a typed call pointer beside the owner, so a function value *is* its reflection object, as
D39 says (`conformance/stage6/typed_functions`). **Not built:** `.owner` as a readable attribute -- nothing in
`Spite.Function<...>` says what class the owner is, so there is no type to give it; calling a *reflected*
function with arguments; and a class attribute initialised with a function value.

`Nothing` is also what `.returns` reports for every function declared without a return type, so it is not a
notation invented for the type syntax -- it fills a hole the reflection already had. A function whose body falls
off the end still returns nothing in the ordinary sense (section 5); `Nothing` is how that is *named* when a
signature or a reflection object has to say it.

### Failure: three outcomes and no others  **[partial]**

D24 (decided by Mortaro, 2026-09-19): Spite has **a compile error, an `assert`, or a crash**. There are no
exceptions, no error unions, no bubbling, and no error value carrying a message, because "errors and exceptions
tend to be useless: they tell us a message we have no action to take about them".

1. **A compile error**, for anything the compiler can know. This is already where the language keeps landing:
   an unserialisable type (section 17), the wrong `$target` (D20), an unfilled codegen hole (section 9), a
   missing foreign symbol (section 17), a member that does not fit a template (section 8), a symbol outside its
   enum (section 7).
2. **`assert`**, for when the program should keep running.
3. **`crash`**, for when everything should stop so the code gets rewritten.

`T?` is the only runtime failure value, and it carries no reason. A caller narrows it with `assert`,
halts on it with `crash`, or handles the absent case with `if ... do`.

**If a distinction is actionable, it is data, not an error** (proposed by Claude, unconfirmed). A caller that
must tell a timeout from a rejection takes back a `type`, `union` or enum modelling exactly that -- ordinary
data with ordinary handling. Something you can act on was never an error; it was a value that was not modelled.

#### `assert` is control flow

D26 (decided by Mortaro, 2026-09-19): `assert` is a guard clause, not validation -- "this value is absent, so
there is no more logic to do here, but the program is fine and keeps serving everything else". It is why a web
server or a game written in Spite should rarely crash.

D27: **`assert` is legal only in a function that returns nothing, or returns `T?`** -- the two cases
where the substituted default is honest, since void has nothing to say and `null` says exactly "absent".
Anything else is a compile error naming the return type. An empty `List<T>` or an empty `String` is as
ambiguous as `0`: the caller cannot tell "no user" from "a user with no orders".

```
func count_user_orders(user_id: Int): Int {
    assert find_user(user_id)        # error: Int cannot express absence
}
```

The author then chooses deliberately -- widen the return to `Int?`, handle it with `if`, or, if
absence is a bug rather than a case, `crash`. **The rule also forces smaller functions** (Mortaro): a function
that handles a reference and wants a guard has to split, so the lookup lands in a small function whose signature
honestly says `T?`. Decomposition comes from the type system rather than from style advice, which is
what works on an AI -- there is no "should" left to ignore.

D28: **`assert` is banned in a constructor**, including the entry class's own (D29). A constructor is setup, not
logic: if something can be invalid inside one, function logic has been put where only setup belongs. The caller
receives an object with every field at its default and cannot tell it from a properly built one, which is
exactly the surprise the rule exists to prevent. A program constructor that grows complex splits into smaller
functions it calls -- and reads as a table of contents for the program, with the logic one level down.

#### `crash`

D30 (decided by Mortaro): `crash` is a keyword, so the compiler takes its context from the program rather than
from a message string. **It mirrors `assert` exactly** -- same polarity, same shape, different severity:

```
assert database.connect()        # falsey: return the default, keep going
crash  database.connect()        # falsey: halt
```

The compiler captures the condition's source text and every operand value, so nothing has to be written and
nothing can drift out of sync. Bare `crash` is legal for an unreachable branch and reports its enclosing
context. `crash user` narrows a `T?` exactly as `assert user` does, but never returns -- which is what
fills the gap D27 opens, since a function returning `Int` can then guard without widening its signature.

Absence is fine -> `assert`. Absence is a bug -> `crash`. Absence is meaningful -> `if ... do`.

#### What a crash reports

D25: a crash always reports backend information **and the trace of every `assert` that failed before it**. Those
asserts are the causal trail explaining how the program reached the state that crashed, which is usually several
frames earlier than the crash itself. The cost is only on the failure path -- a passing `assert` already
branches -- and the trace is a fixed-size ring buffer with a total count, so it never allocates and cannot grow
without bound. Compile-time evaluation (section 8's class-level functions, section 15's generated JSON) reports
the same way.

D26 refinement (proposed by Claude, unconfirmed): the trace records **predicate asserts only**. A narrowing
assert firing is routine control flow -- thousands an hour on a server -- and on concurrent work the last few
would come from unrelated requests, reading as a causal chain that does not exist. A per-site count covers them
instead, which is one increment and never consumes the buffer.

D32: **a crash site is identified by a compile-time id, and the compiler emits an id-to-source map as a build
artifact** rather than embedding the information in the program. The binary carries none of it, which matters
most in a wasm module where size is startup time, and **the same logical site keeps the same id across every
`$target`**, so a crash from the server bundle and one from the browser bundle compare directly. The id is
content-derived -- a hash of namespace, class, function, the site's ordinal and the condition source -- never a
counter, because a counter renumbers everything below an inserted line and destroys both old reports and
cross-target identity. `--development` embeds the text directly, so a local run needs no lookup.

D33: the map is a greppable tab-separated table at `<output-name>.crashes`, one line per site, sorted by id so
archived maps diff cleanly: id, file, line, column, class, function, kind (`crash`, `assert-predicate`,
`assert-narrowing`), the condition source, and the operand names with their types. It opens with a `#` comment
line carrying the format version and the build hash. At runtime a crash emits one tab-separated line prefixed
`spite.crash`, and an assert `spite.assert`. **Values are rendered as text at crash time but stored raw in the
ring buffer**: a crash happens once and then the program is dead, so it should be informative rather than fast,
while a narrowing assert may fire thousands of times an hour and must stay cheap until something dumps it.

**Current implementation (2026-09-20).** `crash` itself is implemented -- the keyword, narrowing, bare `crash`
for an unreachable branch, and the compiler enforcing D27/D28/D29 -- but the reporting above is not yet. A
firing `crash` flushes stdout, writes one line to stderr, and exits with status 1:

```
spite.crash<TAB>path:line<TAB>Class<TAB>function<TAB>condition
```

The condition is rebuilt from its own tokens and the line ends with the named operands of the failed
comparison and their values -- `value > limit<TAB>value=-9<TAB>limit=0` -- with calls never evaluated a second
time. Still missing: the crash ids, the `<output-name>.crashes` map and the assert ring buffer. The line keeps the
`spite.crash` prefix, so anything grepping for it keeps working when the id and values arrive.

### Operators  **[implemented]**

Every operator is a shortcut for a function, which a class can define to support that operator (decided
2026-09-19: "every operator is a shortcut for a function the user can replace").

| Operator | Function | Notes |
|---|---|---|
| `a + b` | `sum(b)` | |
| `a - b` | `subtract(b)` | |
| `a * b` | `multiply(b)` | |
| `a / b` | `divide(b)` | |
| `a % b` | `remainder(b)` | |
| `a == b` | `equals(b)` | |
| `a != b` | negated `equals(b)` | |
| `a < b` | `less_than(b)` | |
| `a > b` | `greater_than(b)` | |
| `a <= b` | `less_than(b) or equals(b)` | derived, not its own function |
| `a >= b` | `greater_than(b) or equals(b)` | derived, not its own function |
| unary `-a` | `negate()` | |
| `not a` | -- | stays a built-in keyword, never a function |
| `a and b` / `a or b` | -- | stay built-in, short-circuiting |
| `a[x]` | `get_at(x)` | |
| `a[x] = v` | `set_at(x, v)` | |

For `Int`/`Float`/`Bool`/enum/`String`/`List<T>`/`Dictionary<T>` these are intrinsic (they compile to exactly
the same C as before this table existed); `String`/`List<T>`/`Dictionary<T>` additionally accept the explicit
call form alongside the operator (`list.get_at(0)` next to `list[0]`, `"a".sum("b")` next to `"a" + "b"`).

For a user class or union, `a + b` compiles to `a.sum(b)` when the class defines (an exact function, or Symbol
codegen answers) `sum`; a union duck-types the same way a method call already does (every member must define
the function with the same signature). Missing the function is the existing "unsupported"-style diagnostic,
now naming the function to define instead. The right side of a binary operator still casts toward the
parameter type, exactly like an ordinary function call argument.

Attribute access from *outside* a class also goes through operator functions: `person.age = 1` calls
`person.set_age(1)` when the class defines or Symbol-codegens `set_age` (an exact function always wins over
Symbol codegen), otherwise it assigns the field directly. `person.age` (a read) calls `person.get_age()` the
same way when defined -- **(proposed by Claude, unconfirmed: the manual only decided the setter half; this
read half mirrors it)**. D1 removes the earlier restriction that skipped interception for a getter returning
an owning type: every function/method return is now a properly retained, independent reference (section 10),
so a getter that computes and hands back a fresh value is exactly as safe to intercept through as one that
just returns the field itself (this also fixes the `get_<attribute>()` double-free that used to be listed in
`docs/KNOWN_ISSUES.md`). A getter written and called explicitly (`person.get_full_name()`) was always just an
ordinary method call. Compound assignment through interception works both ways: `person.age = person.age + 1`
reads through `get_age()` and writes through `set_age(...)`. Inside a class's own functions, a bare `age = 1`
(no receiver) and `attributes[attribute]` stay raw -- interception only applies to `receiver.field` written
from outside.

## 6. Control flow  **[implemented]**

```
if condition { } else { }
if nullable_value { } else { }     # runs with nullable_value unwrapped in place when it is not null; else when it is
while condition { }
switch enemy {
    Player: enemy.hurt()
    Monster: { enemy.die() }
}
```

An `if` on a `T?` narrows the value in place (section 5): the block runs with `value` as a plain `T`,
and the `else` runs exactly when it is null/absent -- one rule for narrowing everywhere, `assert`/`if`/`switch`
alike (second batch item 2, decided 2026-09-19: `if value do name { }` is gone, and `do` is no longer a
keyword, section 2). The terminal-`if` lint (section 5) applies only when the `if` has no `else` -- one with
an `else` already handles the missing case explicitly, so there is nothing left to rewrite with `assert`. D3
(decided by Mortaro, 2026-09-19) was the decision that first gave the form its `else`; the 2026-09-20 removal
of `do` simply renamed the unwrapped value's block to the in-place narrowing above.

There is deliberately no `break`/`continue` (D2, decided by Mortaro, 2026-09-19): `while` is the only loop
construct Spite has, full stop. An early exit re-checks a boolean flag in the loop's own condition instead
(`var stopped = false` ... `while not stopped { ... }`).
writeup of what this costs in practice (found while writing the bootstrap compiler in Spite) and why the
language owner chose to keep it this way regardless.

`while` is the only loop. There is no `for`: the language owner's call (2026-09-19) is "only the
while loop, no for; that makes people favor the metaprogramming" -- reach for `List<T>`/`Dictionary<T>`
metaprogramming (section 15) first, and index with `while index < list.count() { }` when a loop is
genuinely needed. Writing `for` is a parse error naming `while` and the metaprogramming helpers instead
of silently doing something else. Loops and ifs are otherwise discouraged in application code; reach for
standard library metaprogramming first.

## 7. Types

### Enums  **[implemented]**

An `enum`, `type` or `union` declaration takes no `=` and always breaks lines, one entry per line, no commas
(D47). `=` means assignment and nothing else. Writing `enum Job = {` is a parse error naming the fix.

```player.spite
enum Job {
    'knight'
    'magician'
    'archer'
}

func set_job(player_job: Job) { }

func other_function() {
    set_job('knight')
}
```

- An enum declared in `player.spite` is `Player.Job`. It is rarely written that way, because `'knight'` alone is enough wherever the expected type is known.
- From inside `Player` itself, plain `Job` also works; from elsewhere, both `Player.Job` and -- when unambiguous across the whole program -- plain `Job` work (searched: the referencing class's own namespace first, then its containing folder, then each parent folder, then the whole program by simple name).
- An enum value is resolved from where it is used (parameter, annotation, assignment target, comparison). Two enums may share a value name; with no expected type in hand, the value resolves when exactly one enum in the program has it, otherwise it is a compile error listing every enum that does.

**An enum is a closed list of symbols** (D10, decided by Mortaro, 2026-09-19). `'knight'` is a symbol -- a name
known at compile time -- and an `enum Job` declaration lists which symbols are accepted where a `Job` is expected.
That is all an enum is; the integer it compiles to is a representation detail.

- **A symbol literal is legal only where something says what it may be.** Where an enum is expected it must be
  one of that enum's symbols: `set_weapon('orange')`, when the parameter is an enum without `'orange'`, is a
  compile error listing the symbols that enum accepts. There is no widening from a symbol to an enum, and there
  is no untyped symbol literal -- `var choice = 'orange'`, with nothing to check it against, is an error naming
  the missing context.
- **A `Symbol` is text from a precompiled, tree-shaken table** (D70, decided by Mortaro, 2026-09-23, superseding
  this clause's earlier "compile-time only"). Every symbol the program uses is an entry in one table the compiler
  writes, so a `Symbol` value costs no allocation -- reading, storing, passing or comparing one never makes new
  text -- and a symbol the program never uses does not exist in it. So all of Spite's own metaprogramming can use
  symbols freely: a used symbol was needed anyway, and an unused one disappears. A `Symbol` reads as text
  wherever text is expected (every `String` function answers on it) while `symbol.class` is `Symbol`, and text
  becomes a `Symbol` only through `Symbol(text)`, which answers `Symbol?` -- `null` unless that symbol is already
  in the table (D68). An enum is still the closed form: the list of symbols a place accepts.
  Most symbols are short, so the representation can later become a small inline string rather than a pointer
  into the table ("TinyString"); that is an optimisation the language does not observe.
- **What a `Symbol` names is still checked by whatever consumes it.** `person.set_attribute('age', 2)` is
  verified against `Person`'s real attributes at compile time, exactly as the Symbol codegen path already is
  ([Symbol codegen](#symbol-codegen-implemented)) -- a symbol literal only makes that call writable in source,
  which it is not today.

### Unions  **[implemented]**

Tagged unions. A `switch` must cover every member and narrows the value inside each case.
When every member has the same function or attribute (same signature), it can be used directly on the union.

```
union Enemy {
    Player
    Monster
}

func attack(enemy: Enemy): Bool {
    switch enemy {
        Player: enemy.hurt()
        Monster: enemy.die()
    }
    return enemy.is_alive()
}
```

**`_:` answers for every member without a case of its own** (D60, decided by Mortaro, 2026-09-23). It is the
last case, and it means exactly "this body, written once for each remaining member": the value is narrowed to
each of those members in turn, so `_: creature.sound()` needs `sound()` only on the members `_` answers for,
not on the whole union. A switch is still exhaustive -- `_` is how it covers the rest, not a way to skip it.
**A repeated case body is an error** whenever `_` could absorb it: two cases doing the same thing when there is
no `_` yet, or a case doing what `_` already does. `_` after every member already has a case, or anywhere but
last, is an error too (`diagnostics/switch_rest_case`, `conformance/stage6/rest_case`). A switch over a `T?`
keeps its two cases, `<Type>:` and `Null:`.  **[implemented]**

Memory safety is done with unions instead of borrow checking noise: `T?` is the union of `T` and `Null`, and it is narrowed before use with `if ... else`, `assert`, `crash` or `switch` (section 5).
**`Monster?` is a union, and `Nullable<T>` is gone** (D45, decided by Mortaro, 2026-09-20). A `?` suffix is
sugar for the union of a type and nothing:

```
var target: Monster? = null

func find_target(): Monster? {
    return null
}
```

`Monster?` *is* `union { Monster, Null }` -- `Null` is an ordinary class whose only value is the literal `null`,
the way `true` and `false` are the values of `Bool`. That is the point of the change: nullability stops being a
special case in the compiler and becomes a union like any other. `switch target { Monster: ... Null: ... }`
works because it is a switch over a union; `assert`, `crash` and `if` narrow it because union narrowing
already exists; and the pile of `Nullable<T>` exceptions collapses into rules the language already had.

Section 10's optimisation survives as a codegen detail rather than a language rule: a union of a reference and
`Null` is still just the pointer, and only a scalar needs a wrapper.

**Considered and rejected on the way** (2026-09-20): `Nullable<T>` (ugly, and a generic wrapper is what keeps it
a special case); `Monster or Null` (`or` short-circuits everywhere else, so a return type reads as an
expression yielding the truthy left side); `maybe Monster` (a keyword bought for a shortcut); `Maybe<Monster>`
as a standard library generic union (generics over unions hide the members, and collide with `$` codegen
replacement when a parent already declares the same name); and `Monster.Maybe` as a class-level member.


### Inline types and duck typing  **[implemented]**

```
type System {
    query: Query
    with: Dictionary<Class>
}
```

Like `enum` and `union`, a `type` declaration takes no `=` and always breaks lines, one entry per line, with no
commas (D47). The single-line form is what would have needed commas, so removing it removes the choice -- and a
`type` is read far more often than it is written, which is the case where the extra lines pay.

A `type` is a class matched by shape: any value with the same attributes and types is accepted, including object literals.
`.class` of the value still points to its original class. This is what makes fast JSON-like code possible.
`.class` read through a `type`-shaped or union-typed value is answered from the object's own tag at runtime,
so it names the class the value really is (`Widget`), not the shape it is being read through (`Labeled`).
An object literal has no class of its own, so it answers `Object`.

**A `type` may require functions, not only attributes** (D16, decided by Mortaro, 2026-09-19; implemented), matched by shape
exactly as an attribute-only `type` is:

```
type Renderable {
    render(): Element
}
```

Any class with a `render()` of that signature is accepted. This is what lets a collection hold "any class that
responds to `render()`" -- a list of components, section 17 -- without a union naming every class in advance,
and it makes the respond-to check section 16 item 9 promises expressible as an ordinary type rather than as
reflection.


## 8. Metaprogramming

Everything is a class, including classes. Reflection is resolved at compile time wherever possible.

### Reflection objects  **[partial]**

Metaprogramming classes live in the `Spite` namespace (decided 2026-09-19:
`Spite.Class`, `Spite.Attribute`, not `SpiteClass`/`SpiteAttribute`). `Spite` is a reserved root namespace: a
user folder named `spite` (or `load("spite")`) is a diagnostic.

D12 (decided by Mortaro, 2026-09-19): **every reflection object lives in the `Spite` namespace** -- `Spite.Class`,
`Spite.Function`, `Spite.Argument`, `Spite.Attribute`, and whatever else reflection grows. One namespace, and
nothing in it can be mistaken for a user class. **[implemented]**

| Object | Members |
|---|---|
| `Spite.Class` | `.name: Symbol` (D68), `.namespace: Spite.Namespace?`, `.attributes`, `.functions`, `.instances`, plus the class-level functions a class may override ([above](#class-level-functions-and-why-there-are-no-static-functions-planned)) |
| `Spite.Function` | `.name: String`, `.arguments: List<Spite.Argument>`, `.returns: Spite.Class` (`Nothing` when none is declared), `.owner`, `call_function()` |
| `Spite.Argument` | `.name: String`, `.class: Spite.Class` |
| `Spite.Attribute` | `.name: String`, `.class: Spite.Class`, `.value: String` |
| `Spite.Namespace` | `.name: String` (the segment), `.name_with_namespaces: String` (dotted), `.parent: Spite.Namespace?`, `.classes`, `.namespaces` |

**What `.functions` contains** (proposed by Claude, unconfirmed): the functions a class declares, plus the
Symbol-codegen instances that were actually generated for it -- because those are functions of the class in the
program as built, and `--final-classes` already prints them (section 16 item 9). Reflection describes the
program that exists, not the source as written, which is the same rule D42 applies everywhere else.

**A constructor is not one of them.** It does not answer on an instance, it makes one, so putting it in
`.functions` would mean every caller that walks the list has to know to skip it -- and the first thing anyone
writes is a loop that calls what it finds. If a constructor needs to be reachable it belongs on the class object
as its own member, not in the list of what an instance answers.

**Reflection may be as detailed as it likes, because what is not used is not emitted** (D42, decided by
Mortaro, 2026-09-20). Most of this metadata will never be reached by any given program, and reaching for it is
the only thing that makes it cost anything -- "if it has a cost it is because we needed it anyway".

This works here in a way it does not elsewhere. Spite's reflection is resolved at compile time, so which parts
of it a program touches is **statically decidable**, and tree shaking is exact rather than conservative. A
language with runtime reflection cannot prove a class is never reflected on, so it must keep the tables for
everything; Spite can prove it, so it keeps nothing it cannot see a use for. Section 8 already works this way --
`value.attributes` is built only for a class that actually reads `.attributes` -- and D42 says that rule covers
the whole family: `Spite.Class`, `Spite.Function`, `Spite.Argument`, `Spite.Attribute`, `Spite.Namespace` and
whatever comes next.

The practical direction, for milestone 10 and for anything designing a reflection member: **err toward more
detail.** An unused field costs a program nothing, while a missing one costs a design.

**A namespace is an instance of `Spite.Namespace`** (D41, decided by Mortaro, 2026-09-20), for the same reason
`.class` is a real class: a string can only be matched, an object can be compared, walked and enumerated.
`Spite.Class.namespace` is one of these rather than a dotted `String`, and printing it prints the dotted name,
the way printing a `Spite.Class` prints its `.name`.

It mirrors what already exists. Section 11 builds namespaces out of folders, so `Spite.Namespace` is that tree
made readable: `.parent` walks up it, `.classes` and `.namespaces` walk down. A `load(...)` root therefore
becomes something a program can enumerate -- which is what D13's isomorphic split needs when it partitions a
package's functions, and what the deferred compile-time class generation would extend.

`.namespace` and `.parent` are `Spite.Namespace?`, and the chain ends at `null` (Mortaro, 2026-09-20,
rejecting Claude's proposal of a root object). A root object would have read as though every namespace were
nested inside something -- and inside the metaprogramming package in particular, when a namespace merely *uses*
`Spite.Namespace`, it is not contained by it. A top-level namespace has no parent, so it has `null`, and you
walk up with `class.namespace.namespace` for as long as that can be satisfied.

**`.class` is a real `Spite.Class`, not the type's name as a `String`** (D12). This is the change that makes
reflection composable: `function.arguments[0].class == ServerContext` is an identity comparison the compiler
checks, where `.class.name.starts_with("Server")` is a string test that quietly turns false when a class is
renamed. Printing a `Spite.Class` still prints its `.name`, so anything that reads as text today keeps reading
that way. **[`Spite.Attribute.class` is a real `Spite.Class` as of the 2026-09-20 reflection port; see the bullet
below.]**

- `value.class` is the value's class: a `Spite.Class` with `.name: String` (the class's own declared name) and
  `.namespace: Spite.Namespace?` (its namespace object -- `.name` is the segment, `.name_with_namespaces` the dotted path,
  `.parent` the chain up, `.classes`/`.namespaces` the tree down; `null` for a global class). Printing a
  `Spite.Class` prints just its `.name`, and printing a `Spite.Namespace` prints its `.name_with_namespaces`.
- **`class` is an inherited attribute of every instance, not a keyword** (decided by Mortaro, 2026-09-22):
  inside any function of a class, a bare `class` answers with that instance's class -- the same `Spite.Class`
  `value.class` gives -- so `class.name` is the class you are writing. A local or an attribute actually named
  `class` takes precedence over the inherited one (that is how `Spite.Attribute.class` reads its own field),
  and the parser's diagnostic is untouched: `class ClassKeyword {` at file scope still fails at 1:1 with
  "there is no 'class' keyword" (`diagnostics/class_keyword`), because a file is a class named after the file.
  **[implemented]**
- **A class name reads its class object member by member** (decided by Mortaro, 2026-09-22): `Weapon.name` is
  `"Weapon"` with no instance anywhere, `Weapon.namespace` answers the same `Spite.Namespace?` as
  `weapon.class.namespace`, and a method receiver written as a class name resolves against the class object --
  `Weapon.is_singleton()` works, while `Weapon.debug()` (a function of the class being described) still reports
  "'Weapon' is a class, not a value: write 'Weapon()' to make one", since there are no static functions. This
  is D6 read literally: a class name is an ordinary `Spite.Class` value, so the substitution that already made
  `Sticker.attributes` work is what every member read and method receiver on a static path does;
  `Weapon.instances` still means its live registry.  **[implemented]**
- `value.attributes` is a real, runtime `List<Spite.Attribute>` (built fresh, one entry per field, only for a
  class that actually uses `.attributes` -- nothing is generated for a class that never does): each entry has
  `.name: String`, `.class: Spite.Class` (D12) and `.value: String` (the field
  rendered as text; `""` for a field with no obvious textual form, such as a class, list, or union). Because
  it is an ordinary `List<T>`, reading it uses `while`, not a special loop form. `attributes[symbol]` inside a
  class is a separate, compile-time-only form: that same field indexed by a `Symbol` (see [Symbol
  codegen](#symbol-codegen-implemented) below).
- `Person.instances` lists every live instance of a class. Conceptually each class is a variable living on the heap and so is
  its registry of instances; the compiler removes whatever is unused.  **[implemented]**
- **`Spite.Class.instances` is every class in the program** (D49, decided by Mortaro, 2026-09-20). A class is an
  instance of `Spite.Class` (D6), so the registry of *its* instances is the whole program's classes -- nothing
  new is needed for whole-program enumeration, it falls out of "everything is a class, including classes". A
  test runner uses it to find every test class instead of being handed them: no registration, no manifest, no
  `run(...)` per class. D42 pays for it, since a program that never enumerates its classes never generates the
  registry.  **[implemented]**
- **Reflection reads at two levels, and the same word is right at both** (D11, decided by Mortaro, 2026-09-19).
  A class object is an instance of `Spite.Class` (see [Class-level
  functions](#class-level-functions-and-why-there-are-no-static-functions-planned)), so it has attributes of its
  own -- the declarations -- while an instance has their values:

```
weapon.attributes['damage']    # the value held in that field
Weapon.attributes['damage']    # the Spite.Attribute that describes the field
```

  A PascalCase receiver is the class and a lowercase one is an instance, which is the same convention section 17
  uses for `user32.Input` versus `user32.input_mouse`, so nothing new is needed to tell them apart. This is also
  what stops `attributes[symbol]` from being a special, compile-time-only form bolted onto Symbol codegen: it is
  ordinary indexing of the attribute mapping, at whichever level the receiver names. **[planned: the exact shape
  of the class-level mapping -- keyed by symbol, and what it answers for a field the instance has not set -- still
  needs design; see PLAN.md milestone 10.]**

```
func describe(person: Person) {
    var attributes = person.attributes
    var index = 0
    while index < attributes.count() {
        var attribute = attributes[index]
        console.print(attribute.name, attribute.class)
        index = index + 1
    }
}
```

### Class-level functions, and why there are no static functions  **[implemented]**

D6 (decided by Mortaro, 2026-09-19): **Spite has no static class functions and will not get any.** A class is an
instance of `Spite.Class`, and `Spite.Class` is an ordinary standard library class with an ordinary declaration.
So there is nothing for `static` to mean: a function that belongs to the class rather than to its instances is
just a function on that `Spite.Class` object, and `Spite.Class` is where it is declared, with its default.

A class file may **override** one of those functions for its own class object, the same way any class reopens
another (section 11):

```spite/class.spite
func is_singleton(): Bool {
    return false
}
```

```console.spite
func is_singleton(): Bool {
    return true
}
```

- **The set of class-level hooks is exactly the set of functions `Spite.Class` declares.** There is no keyword
  and no marker: if the name is one of those, the definition belongs to the class object; otherwise it is an
  ordinary instance function, as every function in a file has always been. Reading `Spite.Class` in the standard
  library is how you learn the full list.
- A class wanting an ordinary instance function whose name collides with one of them is a diagnostic naming
  `Spite.Class`. The list is deliberately short.
- The override is **evaluated at compile time** and must fold to a constant. A `return` of a literal always
  folds; a `return` of a codegen value (`return $shared`) folds too, so a class-level fact can differ per build
  without any new syntax. Anything the compile-time evaluator cannot fold is a diagnostic. **[planned: until
  milestone 10's evaluator exists, only a literal `return` folds -- which covers every case the standard library
  needs.]**
- `--final-classes` prints each class's class-level functions with the value they folded to.
- **`Spite.Class` is reopenable, like any other standard library class** (D7, decided by Mortaro, 2026-09-19:
  "by all means shoot the foot"). Reopening it changes a class-level default for the **whole program**: a root
  whose `spite/class.spite` returns `true` from `is_singleton()` makes every class in the program a singleton,
  `List<Int>()` included. Adding a *new* function to `Spite.Class` is louder still -- it creates a new
  class-level hook name program-wide, so any class that already had an ordinary instance function by that name
  becomes an override of it.
  Nothing about this is silent, which is the reason it is allowed: a name that collides is a compile error
  naming `Spite.Class` (above), and a changed default shows up per class in `--final-classes`, with the root it
  came from, exactly as any other reopened function does. The compiler has no warnings (section 12), so
  visible generated output is the whole mitigation. **[planned: blocked on section 16 item 1, reopening standard
  library classes at all.]**

**Why a function and not a declaration** (Mortaro, 2026-09-19): it is the same philosophy as a configuration
file being a class that gets reopened -- like Rails patching its internal options through a block that can run
code, rather than through a static configuration file. A static line can only state a value; a function can
compute one, and it costs the language nothing because functions already exist.

### Singletons  **[implemented]**

D8 (decided by Mortaro, 2026-09-19): a singleton is declared with a class-level function (above), overriding the
default `Spite.Class` declares.

```console.spite
func is_singleton(): Bool {
    return true
}
```

- **Call sites never change.** `var console = Console()` everywhere, exactly as it reads today. A human skimming
  a call site is never told and never needs to know; the AI writing the code learns it from the class file and
  from `docs/for_ai_writers.md`.
- **One instance per distinct constructor argument list**, and a singleton's constructor arguments must be
  literals (the rule `load(...)` already has). `Console()` is one object;
  `DynamicLibrary("user32.dll", 'windows', "windows.h")` is one object however many classes ask for it, and
  `"gdi32.dll"` is a second one. The compiler emits one static slot per distinct argument list, so there is no
  runtime registry walk -- only a guarded branch the first time.
- **Memory:** the slot holds one reference, so the count never reaches zero; `drop()` runs at program exit, in
  reverse creation order, and `--debug-memory` counts singletons as roots, never as leaks.
- **Standard library:** `Console`, `Program` and `DynamicLibrary` are singletons. `File`, `Directory` and
  `Process` are not -- they are values (a path, a spawned command), and several may exist at once.
- A later root reopening the class (section 11) may not disagree about it; that is a diagnostic. Reopening
  `Spite.Class` itself to move the default for every class at once is allowed and is D7's foot to shoot.
- A class wanting an ordinary instance function named `is_singleton` hits the collision rule above: a diagnostic
  naming `Spite.Class`.

Rejected on the way here, each for a reason worth keeping: `$singleton = true` and `$instances = 1` (`$` means
"replaced at code generation", and a directive the compiler reads and deletes is never replaced by anything); a
bare `singleton` first line (the file-top declaration shape D5 removed for `generics`); `func Console(): Console`
or `func Console(): Spite.Singleton` (the constructor's return type carrying the meaning -- quiet, and the second
makes the return type describe how rather than what); `func shared_instance(): Console` (loudest and most honest,
but "the constructor is named after the class" stops being one rule); a private constructor `func _Console()`
(`_` would mean private, intentionally unused, *and* singleton); `var console = Console` without parentheses (the
class can no longer enforce it); and a constructor hand-written to return `Class.instances.first()` (boilerplate
in every singleton, plus a second way to allocate).

### Symbol codegen  **[implemented]**

There are no macros. A parameter of class `Symbol` whose name is a segment of its function's name turns the function into codegen
for every name that fits: below, `set_attribute` answers `set_age`, `set_name`, and so on, for each attribute of the class.
Inside, the symbol names that attribute: written as a type (`value: attribute.class`, `): attribute.class`), it
is the attribute's actual type; written as an expression, `attribute.class` is a `Spite.Class`
naming that type, printing just like the type name would.

```person.spite
func set_attribute(attribute: Symbol, value: attribute.class) {
    attributes[attribute] = value
}

func get_attribute(attribute: Symbol): attribute.class {
    return attributes[attribute]
}
```

```
var person = Person(1)
person.set_age(2)
```

A function with the exact name always wins over codegen. Only the names actually called are generated.

**Fixed in milestone 9a:** a `get_<attribute>()` generated this way used to double-free when the attribute was
an owning one (a `String`, `List<T>` or `Dictionary<T>` field). Reference counting made every return a properly
retained, independent value, so a generated getter is now exactly as safe as an explicit one -- which is what
lets D15 (next subsection) treat a field and a zero-argument function as the same member. See `docs/KNOWN_ISSUES.md`
item 2, closed.

### Standard library metaprogramming  **[partial]**

D15 (decided by Mortaro, 2026-09-19): **every standard library template names a `<member>`, and a member is a
field or a zero-argument function without distinction.** `map_age` and `map_get_age` are the same call, because
reading an attribute already goes through `get_<attribute>()` (section 5) -- the compiler may answer the field
case with a direct read, but that is an optimisation, not something the writer thinks about. What a template
requires of a member is its **arity and its return type**, never whether it is stored or computed.

`List<T>` of a class: `filter_<member>()`, `count_<member>()`, `sum_<member>()`, `find_by_<member>(value)`,
`sort_by_<member>()`, `each_<member>()`, `map_<member>()`, `any_<member>()`, `all_<member>()` (section 15 has
the full table). `Dictionary<T>` of a class has the same. These, together with `while`, are meant to cover the
cases that used to reach for `for`.

| Template | The member must |
|---|---|
| `filter_`, `any_`, `all_`, `count_` | take no arguments and return `Bool` |
| `sum_` | take no arguments and return a numeric type |
| `sort_by_` | take no arguments and return something ordered (`Int`/`Float`/`String`) |
| `find_by_(value)` | take no arguments and return something comparable to `value` |
| `map_` | take no arguments and return anything; the result is a `List<U>` of that |
| `each_` | take no arguments; its result, if any, is discarded |

This is why a list of components renders with nothing new in the language: `todos.map_render()` calls `render()`
on every element and collects the results, exactly as `todos.map_title()` collects a field. A member that does
not fit the template's requirement is a compile error naming the member, what it returns, and what the template
needs -- and `each_<member>()` on a plain field is one of those, since reading a field and discarding it does
nothing.

`count_<member>()` counts how many elements have a Bool member true; `sum_<member>()` adds up a numeric member
across every element (second batch item 8, decided 2026-09-19: `count()` is only ever a collection's own size,
and `count_<member>()` on a numeric member is a compile error naming the `sum_<member>()` fix).

```
var repositories = List<Repository>()
repositories.filter_active().sum_stars()
repositories.each_bump_stars()
```

## 9. Codegen values (`$`)  **[implemented; the D5/D9 form is planned]**

`$name` means "replaced at code generation". That is its only meaning, everywhere it appears.

D5 (decided by Mortaro, 2026-09-19): the `generics` header line is removed. D9 (decided by Mortaro, 2026-09-19):
**the constructor declares every codegen value a caller supplies, always, even when there is only one**, in a
`<...>` list that mirrors the call exactly.

```weapon.spite
var damage: $damage_type = 0

func Weapon<$damage_type, $is_magic>(new_damage: $damage_type) {
    damage = new_damage
}

func hit(): Int {
    if $is_magic {
        return 1
    }
    return 2
}
```

```
var sword = Weapon<Magic, true>(10)
```

- **The declaration and the call are the same shape**, so one is read against the other. The reason for declaring
  even a single value (Mortaro): skimming a constructor is how a human sees what a class accepts, and a list of
  one is still a list.
- **Call sites are positional, always.** There is no named form: the constructor fixes the order, so nothing has
  to be restated at the call. `List<Int>()`, `Weapon<Magic, true>(10)`.
- **Declared means supplied by the caller; undeclared means supplied by a compiler flag.** A `$name` used in a
  class but absent from its constructor is a program variable filled by `--name=value` for the whole program
  (`$serve`, `$environment`), never something a call site passes. This is what tells the two apart, and it is why
  the constructor list is exactly "what a caller must pass".
- A class that takes codegen values therefore has a constructor, even if it only exists to declare them
  (`func Pair<$left_type, $right_type>() { }`). The built-in containers declare theirs the same way:
  `List<$element_type>`, `Dictionary<$value_type>`, `$value_type?`.
- **Every hole must be filled.** There are no defaults. A call supplying the wrong number is a compile error
  naming the class's codegen values, in order, so the mistake is corrected from the message rather than by
  opening the class. A `$name` that is neither declared nor supplied by a flag is the same error -- which is what
  a typo like `$is_magik` produces.
- Reordering a constructor's `<...>` changes what every existing positional call site means. Where the values
  have different kinds (a class versus a `Bool`) the compiler catches it immediately; where they are the same
  kind (`Pair<Int, String>` swapped) it compiles and means something else, and the tests are what catch it. This
  is a deliberate, accepted trade (Mortaro, 2026-09-19).
- `--final-classes` prints each class with its codegen values already bound, which is where `true` reads as
  `is_magic` again.
- Writing `generics` is a parse error naming this form (as `for`, `&` and `Heap<T>` already are).

Conditions on codegen values are decided at compile time and the untaken branch is removed (tree shaking).
In development mode they are kept as runtime values so live reload can change them.

**How this got here.** The `generics` header line existed to give positional call sites an order to follow;
Mortaro's objection (2026-09-19) was that a bare first-line declaration "feels outside of our patterns". Three
replacements were tried and rejected in order: a `$name = default` declaration form (kept a declaration alive for
its own sake -- a codegen value is a hole, not a variable); no declaration at all, with call sites naming the
values (`Weapon<$damage_type: Magic, $is_magic: true>`, which made every call site restate what the class already
knew); and a class-level `generics()` function returning a list of symbols (the wrong category -- class-level
functions answer questions *about* a class, while codegen values are inputs to constructing one, and it needed a
second list of names kept in sync with the real holes). The constructor was where the ordered list belonged the
whole time.

## 10. Memory  **[implemented]**

D1 (decided by Mortaro, 2026-09-19): **reference counting is the default memory model**, JavaScript-like. Every
non-scalar value (a class instance, `List<T>`, `Dictionary<T>`, `String`, a union of classes, an object literal) is
a reference: passing it, assigning it, storing it in a field/list/dictionary, and returning it all share the exact
same object. Scalars (every numeric type, `Bool`, an enum value) are still plain values, copied as always. `&` is
removed from the language entirely -- references are the default, so a parameter that used to need `&Type` just
writes `Type`. A copy is always explicit: `copy()`/`deep_copy()` (below).

- **Retain and release.** Every reference is counted. Binding one to a `var`/parameter, or storing it into a
  field/list element/dictionary value, retains it (bumps the count); a scope ending, a value being overwritten, or
  an element being removed from a container releases it (drops the count). When a release brings the count to
  zero: the class's own `drop()` function runs first, if it declared one (new -- see below), then every attribute
  that itself needs releasing is released, then the object is freed.
- **`drop()`.** A class may define `func drop() { ... }` to run cleanup the moment its last reference goes (closing
  a file handle, logging, clearing a back-reference to help break a cycle by hand -- see below). It takes no
  parameters and returns nothing; the compiler calls it automatically, never by name.
- **Identity vs equality.** `==` on two class instances calls `equals` if the class defines one (section 5's
  Operators table, unchanged); otherwise it compares **identity** -- are these two references the same object.
  `String` always compares by content, never by identity (sharing a `String`'s buffer is unobservable, since it is
  immutable).
- **`copy()`/`deep_copy()`** (names **proposed by Claude, unconfirmed**). Every non-scalar value has both:
  `copy()` is shallow -- a fresh object, its own attributes/elements the exact same references the source had
  (retained, not duplicated; a `String` field needs no special handling either way, since it is immutable).
  `deep_copy()` recurses: every reference-kind attribute/element gets its own `deep_copy()`/independent buffer
  instead of being shared. **Cycles are not supported** by `deep_copy()` -- a self-referential (or mutually
  referential) structure recurses forever; break the cycle by hand first if you need to deep-copy one.
- **Cycles leak.** Reference counting cannot free a cycle (two objects holding a reference to each other, directly
  or through several hops): neither one's count ever reaches zero. This is a known, accepted tradeoff, not a bug --
  break a cycle by hand when you are done with it (set the back-reference to `null` inside `drop()`-time logic, or
  clear a `T?` field that closes the loop) if it matters for a long-running program. **[planned]** A future
  opt-in type (e.g. a weak reference) is the intended real fix; not implemented yet.
- Parameters follow the same rule as everything else in section 5: a scalar is passed by value (copied); anything
  else is passed by reference (the same object, retained for the callee's own binding and released when the
  callee's scope ends) -- there is no separate reference syntax to write at the call site any more.

`--debug-memory` reports total allocations/frees (balanced for every example, `tests/references`, and the
bootstrap compiler's own runs, exactly -- not just bounded), plus a **leaked-object summary by class name** when
they do not balance -- naming which classes' instances are still live, which is what makes a leaked cycle visible
instead of just an unexplained non-zero count.

Every function/method return retains its result, so a getter's returned value is always a fresh, independent
reference -- including through attribute read interception (section 5): a `person.age`-shaped read answered by a
getter, used directly as a call/print argument rather than stored, is released like any other temporary
(`ExpressionResult.is_owning` in the generator is how a read that compiles to a call, but still
parses as a `.member`/`.index` expression -- an intercepted attribute read, `arguments.some_key`, the
`attributes[attribute]` template form -- tells every caller whether it is independently owned regardless of what
the plain AST shape alone would suggest).

## 11. Packages, namespaces and loading  **[partial]**

There are no imports. Everything lives in one global namespace, populated by loading folders.

```
func Game() {
    load("package")
    load("cookie_clicker")
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
  `load`-ed root: a subfolder the entry file itself explicitly `load(...)`s is left to that call (a `load`-ed
  root's own folder is never itself a namespace segment), and only the entry file itself is scanned from the
  entry root -- an unrelated sibling file next to it is still pulled in whenever it is reachable as a class, but
  its own `load`s are not followed.
- Folder names are always lowercase snake_case, checked for every folder that actually contains a `.spite` file anywhere inside
  it (an unrelated folder with none, such as `.git` or a build output directory, is never checked or descended for classes).
- Resolving an unqualified `Name` from inside a class tries, in order: that class's own namespace (so a nested enum/type/union
  resolves by its plain name from inside its own class), the same folder's namespace, each parent folder's namespace, then the
  whole program globally. Ambiguity *between roots* at the same level is never an error, because of the next rule.
- Every loaded root merges into the same namespaces. A second root with the same folder structure and file name **reopens** the
  class: this is how monkey patching and game mods work -- later `func`/`var` with the same name replaces the earlier one (in
  load order: the entry folder first, then loads in the order they were discovered), a `var`'s replacement type must match, and
  a new `func`/`var`/`enum`/`type` not seen before is simply added. The standard library (`Console`, `String`, `List<T>`, `File`,
  ...) cannot be reopened yet -- a clear diagnostic names it instead. Your foot to shoot. That includes `Spite.Class` once it lands (D7, section 8): reopening it moves a class-level default for the whole program.
- `load` takes a literal string, so the compiler always knows every bundle; anything else (a variable, an expression) is a
  diagnostic. The compiler finds every `load` reachable from the entry file's own constructor at compile time (a `load` inside
  already-loaded code counts too), and records each root as a bundle (its name and whether the `load` that introduced it sits
  inside an `if`/`while`) in the program model, for dynamic libraries/lazy loading to build on later.
- `load` marks a **bundle boundary**, like an async import in webpack: each loaded root can become a separate dynamic library,
  tree shaking is computed per bundle, and a `load` inside an `if` is loaded lazily when that line runs.  **[planned: every
  bundle is linked statically into the one executable for now, and the `load(...)` call itself compiles to nothing]**
- Because patching is dangerous to read, the toolchain writes a **final class** folder: every class after all codegen, with the
  winning function of every replacement, each preceded by a `#` comment naming the root it came from (and which roots it
  replaced) -- see `--final-classes` in [Command line](#13-command-line). The language server reads it too.
  **[planned: an instantiated Symbol codegen function and a used `List<T>`/`Dictionary<T>` helper signature do not appear here
  yet -- only the source classes are declared with]**

## 12. Style  **[implemented]**

Decided by Mortaro: "the compiler is the linter and the formatter. Style is
arbitrary, it is Mortaro's taste, and it is the only way. The compiler rewrites source files to the one true
style automatically instead of complaining." He left the specifics open, so every concrete rule below
is **(proposed by Claude, unconfirmed)** except where noted -- revisit any of them on request. `--no-format`
and `spite format` are documented in [Command line](#13-command-line); the naming/abbreviation lints that
cannot be auto-fixed are their own subsection below.

### Formatting

- 4 spaces per indentation level; never a tab. No trailing whitespace on any line. Exactly one newline at the
  end of the file.
- A `<...>` list holding a single named codegen value is rewritten to the positional form (section 9), so
  there is one way to write each call.
- **Implemented by the Spite compiler (2026-09-23)**, as `bin/spite format [--check] <file-or-folder>` (the
  compiler's `--mode=format` and `--mode=check_format`), and `check.sh` requires every file outside
  `diagnostics/` to be formatted already. What it does today: 4-space indentation, one space around binary
  operators, the minimum parentheses (a receiver that is an operation always keeps them), `else if` on one line,
  a switch case with one short statement on its own line, a call or a signature wider than 120 columns broken
  one argument per line with trailing commas, floats as written, `: Nothing` dropped from a shape function,
  top-level link comments (the only comments D34 allows) kept before the declaration they preceded, and the
  blank-line rule for grouped `var`s. **Safety check:** the formatted text must lex, parse, print to the same
  fully-parenthesised program as the original, and keep every comment, or the file is left alone and the reason
  printed. **Every compile formats** the entry folder's files and every `load()`ed root as discovery reads
  them (not `library/`), printing `formatted <path>` for each file it rewrote; `--no-format` skips it. A file
  whose function body has an empty line is left alone, so D55's error still fires instead of the formatter
  quietly removing the line; a file the formatter refuses is left alone with its reason printed.
  `--final-classes` writes its classes already formatted. A list or object literal over 120 columns is written
  one entry per line with no commas. Every documentation program not marked `error` is formatted too, and `check.sh`
  keeps it so.
- **A file is ordered** (D67, decided by Mortaro, 2026-09-23): enums, unions, types, variables, the constructor,
  then functions. Anything out of that order is a compile error naming what came before it
  (`diagnostics/declaration_order`). Where `union` goes was not said; beside `enum` and before `type` is
  Claude's placement (unconfirmed). `singleton` and `generic` lines come first if open question 12 adds them.
  **[implemented]**
- One blank line between declarations at file level, except that consecutive `var` declarations may stay
  grouped with no blank line between them if they were already written that way (a blank line the source did
  put between two `var`s is kept; one between any other pair of declarations is always exactly one, inserted
  if missing and collapsed if there were several).
- Inside a function/`if`/`while`/switch-case body: at most one consecutive blank line, and never one right
  after the opening `{`.
- K&R braces: `func name() {`, `if condition {`, `} else {`, `} else if other {` -- `else` always continues on
  the closing `}`'s own line, and a nested `if` in an `else` branch collapses onto one `else if` line whenever
  it does not itself carry a comment that would need its own line. An empty body prints as `{ }` on one line
  rather than an empty `{\n}`.
- One space around every binary operator (`a + b`, `a and b`) and after `:`/`,`; no space before `:`/`,`, and
  none just inside `(`/`)`/`[`/`]` (`f(a, b)`, `list[0]`, not `f( a, b )` or `list[ 0 ]`).
- The minimum parentheses needed to preserve meaning are kept (`1 + 2 * 3`, not `1 + (2 * 3)`); parentheses
  that only restated the language's own precedence are dropped, and ones that change it are always kept.
- An inline list/object literal uses `, ` between entries on one line; a multi-line one uses one entry per
  line with no commas (newlines already separate entries -- section 2). A list/object literal (or a function
  call's argument list) that would be longer than 120 columns on one line is broken into the multi-line form
  instead, one entry per line; short ones stay inline regardless of how the source originally wrote them (one
  true style, not "preserve what you typed"). `enum`/`union`/`type` bodies are always printed multi-line (one
  member per line), matching every example so far.
- Comments are `# text`, exactly one space after `#` (a comment with no text is just `#`). String contents are
  never reformatted -- whatever is between the quotes is untouched, escapes and all.
- **Lossless.** A leading (own-line) comment before a declaration/statement/list-or-object-entry/enum-value/
  union-member/type-field/switch-case, a trailing same-line comment after one, and whether a blank line
  preceded one are all attached to that AST node by the parser (`ast.Trivia`) and reproduced by the formatter;
  formatting a file never drops or reorders a comment. **Known limitation:** a comment is only ever attached at
  those specific points -- one written in the middle of an expression, an argument list, or with nothing
  following it before a closing bracket/brace (an "orphan" comment) is not tracked anywhere. The safety check
  below catches this rather than silently losing the comment.
- **Safety check.** After formatting, the formatter re-lexes and re-parses its own output and confirms, before
  ever writing a file: (a) the formatted-and-reparsed AST prints (via the older, comment-blind but
  round-trip-stable canonical printer) to exactly the same text as the original AST does, proving no code was
  added, dropped, or reordered; and (b) every comment's
  text appears, in the same order, in the formatted output. Either check failing refuses to write the file and
  reports an internal formatter error instead -- see the formatter.

### Comments  **[implemented]**

**A comment is one line, and it is nothing but a link to a markdown section** (D34, decided by Mortaro, 2026-09-20):

```
# notes.md#why-this-exists
func Game() {
```

- The link is a path to a markdown file plus a GitHub format anchor (lowercase, punctuation dropped, spaces to
  hyphens, `-1`, `-2` for repeated headings) and no other text. The path is resolved from the entry file's folder,
  the same rule `load` uses, so a local run and an automated one agree.
- The compiler checks that the file exists and has a heading with that anchor. A link that does not resolve fails
  the build, which is the point: prose in source rots into a lie, a link either resolves or stops you.
- A comment may only appear outside functions and declaration bodies. A line that seems to need explaining
  becomes a named function instead, and unlike a comment a name is visible to `functions`, to `--final-classes`
  and to every tool.
- `//` and `/* */` are recognised by the lexer only to be rejected with the explanation.
- Every one of these errors teaches: it states the one legal form, then asks whether the note is needed at all.
  If a reader could work it out from the code, delete it; if it is lasting knowledge, write the section first and
  link to it; if it warns against a change, a test or a compile error pushes harder than prose.
- Why not zero comments: finding out that nothing is there costs a search on every edit, which is the common
  case, while a link costs a few tokens only where there is something to say. Absence becomes free and
  trustworthy.

### Naming and abbreviations -- compile errors, not auto-fixed  **[implemented]**

A naming or abbreviation problem cannot be silently rewritten (renaming a symbol can change what a program
means to a reader who searches for its old name), so these are compile **errors** with `file:line:column` and
a suggested fix, from a linter that runs on the parsed AST of every class file, before codegen.
There is no `--no-lint`: the compiler already **is** the linter.

The compiler has no warnings at all (second batch item 6, decided 2026-09-19): it either reformats your code or
gives an error, nothing is left to the user's taste. The only printout that used to be labeled a warning -- the
`--development`-only "skipped class" notice (section 13) -- is plain informational `note:` text, not a warning.

- Variables, attributes, functions, and parameters: lowercase `snake_case`. Classes, `type`s, `enum`s, and
  `union`s: `PascalCase` (a class's own name is computed from its file name and therefore always correct; a
  function that happens to share its own class's name -- the constructor -- is exempt from the function-naming
  check for the same reason). Enum values: lowercase `snake_case`. File and folder names: lowercase
  `snake_case` (folder names were already checked this way since milestone 5; file names are new).
- Single-letter names are always errors -- no exceptions -- because they read either as a stray leftover or as
  a puzzle for whoever reads the code next.
- **Abbreviations.** An identifier is split into `_`-separated words and each word is checked against a
  denylist, suggesting the identifier with every matching word spelled out in full. Kept in one place
  (one table in the linter) so it is easy to extend:

  | Abbreviation | Full word | | Abbreviation | Full word |
  |---|---|---|---|---|
  | `expr` | `expression` | | `pos` | `position` |
  | `stmt` | `statement` | | `prev` | `previous` |
  | `param` | `parameter` | | `cur`/`curr` | `current` |
  | `arg` | `argument` | | `max` | `maximum` |
  | `args` | `arguments` | | `min` | `minimum` |
  | `idx` | `index` | | `init` | `initialize` |
  | `ctx` | `context` | | `calc` | `calculate` |
  | `cfg`/`conf` | `configuration` | | `attr` | `attribute` |
  | `tmp`/`temp` | `temporary` | | `elem` | `element` |
  | `str` | `string` | | `char` | `character` |
  | `num` | `number` | | `int` (as a name) | `integer` |
  | `len` | `length` | | `bool` (as a name) | `boolean` |
  | `btn` | `button` | | `info` | `information` |
  | `msg` | `message` | | `spec` | `specification` |
  | `req` | `request` | | `env` | `environment` |
  | `res`/`resp` | `response` | | `db` | `database` |
  | `err` | `error` | | `auth` | `authentication` |
  | `val` | `value` | | `impl` | `implementation` |
  | `obj` | `object` | | `util`/`utils` | `utilities` |
  | `fn`/`func` (as a name) | `function` | | `lib` | `library` |
  | `var` (as a name) | `variable` | | `pkg` | `package` |
  | `ptr` | `pointer` | | `cmd` | `command` |
  | `src` | `source` | | `desc` | `description` |
  | `dst`/`dest` | `destination` | | `doc`/`docs` | `documentation` |
  | `dir` | `directory` | | | |

  `id` is explicitly **allowed** even though it is short, since it has no ambiguity and no natural longer form.
- The terminal-`if` lint (manual.md section 5: a function whose body's last statement is `if value { ... }` --
  no `else` -- wrapping the rest of the function only to check existence, which must be written with `assert`)
  now lives here too, moved unchanged from the generator -- checking a function's last statement
  needs nothing that only codegen's resolved types would provide, so this milestone's "if that is easy"
  condition held.
- **Known limitation, shared with every other diagnostic this compiler prints:** a lint's `file:line:column`
  is always reported against the entry file's own path, even when the actual violation lives in a different
  reachable file of a multi-file program -- the same limitation every codegen diagnostic already has (no
  diagnostic anywhere in this compiler carries its own source file yet).

## 13. Command line

```
spite file.spite                      build and run
spite file.spite --optimized          optimized build
spite file.spite --development        keep everything (no tree shaking), for live reload
spite file.spite --repl               run with an in-place REPL
spite file.spite --repl-port 4000     run with a remote REPL an AI can connect to, to explore memory and debug
spite file.spite --environment=server set $environment
spite file.spite --final-classes=folder write the final class folder (bare --final-classes defaults to .spite-cache/final/ when --development is set)
spite connect 4000                    talk to a running --repl-port program (see section 14)
spite connect 4000 --command="..."    send one REPL command, print its raw JSON response line, and exit
spite file.spite -- ada --player=x    run it, passing everything after -- to the program's Arguments
spite file.spite --no-format          skip the automatic formatting pass below
spite format <file-or-folder>         format without compiling (recursive on a folder)
spite format --check <path>           rewrite nothing; exit 1 listing (to stdout) every file that would change
```

Flags mix freely (a production build may keep the REPL). Building and running are the same command.

**Where the compiler's flags end** (proposed by Claude, unconfirmed; implemented 2026-09-23): at the first bare
`--`. Everything after it reaches the program's `Arguments` verbatim -- `arguments.get(0)` is the first,
`arguments.player` reads `--player=...` -- and none of it is read as a compiler flag, a file to compile or a
`$name` value. A program's own `Arguments` stops at `--` the same way, which is the ordinary meaning of `--`.
Before this, a program run by the compiler received no arguments at all (`conformance/stage6/program_arguments`).

- Compiler flag names are reserved and can not be used as codegen value names.
- A `--name=value` flag that matches no `$name` used in the program is an error, so typos never pass silently.
- **Automatic formatting (milestone 7a).** Every `spite file.spite ...` compile first formats every `.spite`
  file that belongs to the program -- the entry file's own folder (flat, matching section 11's discovery
  rule), plus every `load(...)`-ed root, recursively -- rewriting a file only when its formatted text differs
  from what is on disk, atomically (a temporary file plus one rename), and printing `formatted <path>` to
  stderr for each one. A file with a parse error is left untouched (the ordinary diagnostics still report it);
  a file the formatter's own safety check refuses to touch (see [Style](#12-style-implemented)) prints an
  "internal formatter error" instead of being rewritten, without failing the build. `--no-format` skips all of
  this -- needed by anything that asserts diagnostic positions against a fixture kept deliberately unformatted.
- `spite format <file-or-folder>` runs the same formatter standalone, without compiling: a file formats just
  itself, a folder recurses into every `.spite` file under it (no bundle/`load()` awareness -- every file
  found is formatted, unconditionally). `--check` rewrites nothing and instead lists (to stdout) every file
  that would change, exiting 1 if that list is non-empty (0 if the whole tree is already clean).

## 14. REPL and live reload  **[planned]**

Milestone 6a: a REPL that inspects and drives the *running* program, local (`--repl`) and remote
(`--repl-port`). **Status (2026-09-23): none of this section is built in the Spite compiler.**
`runtime/spite_repl.h` is the interpreter this section describes, written in C and not wired in; the
subsections below keep the design. **Open for Mortaro:** wire that C in now, or write the REPL in Spite over
the reflection that milestone 10b made real (`Spite.Class.attributes`, `.functions`, typed function values) --
which is what milestone 15d asks for, and what D14's "nothing above the floor is hand-written C" implies.
Claude would write it in Spite, since the C version would be the next thing 15d deletes. Compiling and executing arbitrary new Spite code inside the running process,
`Class.instances`, and live reload were blocked on the memory-model decision in [open question
4](#open-questions), decided as D1 (reference counting, milestone 9a, section 10) -- unblocked, not yet
started, for milestone 6b (see "Live reload and 6b" below).

### Reflection tables  **[planned]**

Emitted only when `--repl` or `--repl-port` is given (never for a normal build): for every class the
compiler actually emits, its qualified name, attributes (name, type name, C offset, kind), and every
callable function whose parameters are all scalar/String/enum -- an ordinary method, or an already-
*instantiated* Symbol-codegen function (`set_age`, ...; a template nothing calls is still never
instantiated, so tree shaking for templates is unaffected). Each reflected function gets a generated
uniform thunk, `(self pointer, parsed arguments) -> rendered String result`. Enums get their value
names; `List<T>`/`Dictionary<T>`/`T?`/a union get the element/member kind and C offsets needed
to walk them generically, all resolved with the real `offsetof`/`sizeof` operators at compile time, so
the interpreter (`src/runtime/spite_repl.h`) never has to reason about struct layout itself. D1 (milestone
9a): a class/`List<T>`/`Dictionary<T>`/`String` attribute (or a `T?` of one) is always itself a
pointer now, so every one of those is `via_pointer` (walking through it -- and through a `T?` of
one, which is just that same nullable pointer -- is always transparent); only a still-embedded scalar/
enum/union attribute (or a `T?` of one) is not.

A class itself was already emitted whenever reachable (unaffected by REPL mode); a class's own
*functions*, it turns out, were never tree-shaken to begin with (`classes.emitClassBodies` already
emits every function of a resolved class, called or not), so item 1 of the milestone -- "relax tree
shaking of functions of reachable classes" -- needed no separate mechanism.

### Command language

A small subset of Spite expressions, interpreted by `src/runtime/spite_repl.h` directly over the
reflection tables, rooted at the entry instance under the fixed name `program` (regardless of the
entry class's own Spite name):

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`, `program.settings["volume"]`,
  `program.target`. Walking through a non-null `T?` is transparent; a union shows its active
  member and walks straight into it.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, `program.player.set_age(3)` (any
  reflected function, including an instantiated Symbol-codegen one); `List<T>`'s `count()`;
  `Dictionary<T>`'s `count()`/`keys()`/`has(key)`. A call must be the last part of an expression --
  chaining a `.`/`[...]` after `()` is not supported this milestone.
- **assignment** of a scalar/String/enum literal: `program.player.age = 5`, `program.monsters[1].name =
  "rat"`, `program.player.job = 'knight'`. Always a raw field write; additionally, when a `set_<attribute>`
  function is itself reflected (i.e. some Spite code already called it, instantiating it), the REPL calls
  that instead of writing the field directly -- the same "attribute write goes through `set_<attribute>`"
  rule ordinary compiled Spite code follows.
- **meta commands**: `classes` (every reflected class's qualified name), `describe Engine.Renderer`
  (its attributes and functions with signatures), `enums`, `memory` (live allocation count/bytes from the
  debug allocator -- REPL modes always compile with it enabled, regardless of `--debug-memory`), `help`,
  `exit`.
- Printing a class instance shows `ClassName { attribute: value, ... }` one level deep: a nested class/
  union attribute prints as `ClassName {...}` (never expanded further); a list/dictionary attribute
  prints as `List<T>(count)`/`Dictionary<T>(count)` regardless of depth; a `T?` prints `null` or
  transparently passes through to its held value at the *same* depth (it is never itself a level of
  nesting).
- Every error is one line and never a crash: an unknown attribute names the ones that do exist, an index
  or key out of range says so, a scalar/String/enum-only operation on the wrong kind of value is a type
  mismatch, and calling a non-function is "not callable".

### `--repl`  **[planned]**

Runs the entry constructor normally; when it returns, instead of dropping the entry instance and
exiting, reads commands from stdin with a `spite> ` prompt until `exit` or end of input, then drops and
exits normally (memory balanced -- checked by its own end-to-end test the same way `--debug-memory`'s
own tests are).

### `--repl-port <port>`  **[planned]**

Also accepts `--repl-port=<port>`. Before the entry constructor runs, starts a background thread with a
TCP server bound to `127.0.0.1:<port>` **only** -- it never listens on any other interface, and there is
**no authentication**: anyone who can reach that port on that machine can read and mutate the running
program. This is a local debugging tool, not something to expose past `127.0.0.1`.

The line protocol is designed for an AI client: each request is one line of the command language; each
response is one line of JSON, `{"ok":true,"value":"...","type":"Int"}` on success or
`{"ok":false,"error":"..."}` on failure (values and errors are JSON-escaped strings; a multi-line value,
e.g. from `classes`/`enums`/`describe`, uses `\n` inside that one JSON string, never a real newline in the
wire bytes). The program keeps running (data races with it are accepted -- this is a debug tool) while
clients connect and disconnect one at a time; when the entry constructor returns, the process stays alive
serving the REPL until a client sends `exit`, at which point the server thread calls `exit(0)` directly
(there is no guarantee the main thread, possibly still inside the entry constructor's own `while` loop,
will ever unwind back to a clean `main` return, so this does not attempt one). Implemented with Win32
threads and Winsock under `_WIN32`, pthreads and BSD sockets otherwise; `ws2_32` is only linked when a
REPL mode is actually requested.

### `spite connect <port>`  **[planned]**

A tiny client built into the compiler binary itself (built in, using the platform sockets -- no libc socket
calls needed on the compiler's own side): with no `--command`, an interactive prompt that sends each
typed line and pretty-prints the JSON response (`value (type)` or `error: message`); with
`--command="..."`, sends that one command, prints the raw JSON response line, and exits -- this is the
form tests and AI clients use.

### Live reload and 6b  **[planned]**

Moved from the milestone list; was blocked on [open question 4](#open-questions) (reference semantics),
now decided as D1 (reference counting, milestone 9a, section 10) and therefore unblocked, but not yet
started: compiling and executing arbitrary new Spite code inside the running process, `Class.instances`,
and swapping code while the program runs.

## 15. Standard library  **[partial]**

D1 (decided by Mortaro, 2026-09-19): every value below is reference counted, exactly like a user class (section
10) -- a `String`/`List<T>`/`Dictionary<T>` is freed the moment its last reference goes, not by a single-owner
convention. See [Memory](#10-memory-partial) for the retain/release rules and the cycle caveat.

### String  **[implemented]**

An immutable value with a length (not a bare `char*`). A literal is static and never allocates; every other
`String` owns one buffer, freed once by its owner. The right side of `+` casts toward `String` (every numeric
type, `Bool`, and enum all format to text -- see [Numeric types](#numeric-types-implemented-provisional) for
`Float`/`Double`'s shortest-round-trip printing); assigning a `String` to any numeric variable parses it,
defaulting to `0`/`0.0` on failure. `==`/`!=`/`<`/`>` compare by content, and (manual.md section 5's Operators
table) also work as `equals(other)`/`less_than(other)`/`greater_than(other)`; `+` also works as `sum(other)`.

| Method | Result | Notes |
|---|---|---|
| `length()` | `Int` | |
| `is_empty()` | `Bool` | |
| `slice(start, end)` | `String` | clamped; `""` for an empty/invalid range |
| `character_at(index)` | `String` | `""` out of range |
| `code_at(index)` | `Int` | `0` out of range |
| `contains(text)` / `starts_with(text)` / `ends_with(text)` | `Bool` | |
| `index_of(text)` | `Int` | `-1` when absent |
| `replace(from, to)` | `String` | replaces every occurrence |
| `trim()` / `upper()` / `lower()` | `String` | |
| `split(separator)` | `List<String>` | an empty separator splits into single characters |
| `lines()` | `List<String>` | splits on `\n` |
| `to_tiny()` / `to_short()` / `to_int()` / `to_long()` | `Tiny` / `Short` / `Int` / `Long` | `0` on a value that does not parse |
| `to_byte()` / `to_unsigned_short()` / `to_unsigned_int()` / `to_unsigned_long()` | `Byte` / `UnsignedShort` / `UnsignedInt` / `UnsignedLong` | `0` on a value that does not parse |
| `to_float()` / `to_double()` | `Float` / `Double` | `0.0` on a value that does not parse |
| `sum(other)` / `equals(other)` / `less_than(other)` / `greater_than(other)` | `String` / `Bool` | the explicit call form of `+`/`==`/`<`/`>` |

### List<T> additions  **[implemented]**

On top of `append`/`prepend`/`count`/index-read/index-write (iterate with `while index < list.count() { }`, since there is no
`for`):

| Method | Result | Notes |
|---|---|---|
| `append(value)` | | adds `value` to the end |
| `prepend(value)` | | adds `value` to the front (index 0) |
| `get_at(index)` / `set_at(index, value)` | `T` | the explicit call form of `list[index]`/`list[index] = value` |
| `insert(index, value)` | | clamps an out-of-range index to the nearest end |
| `remove_at(index)` | | a no-op out of range |
| `remove_last()` | `T` | removes and returns the last element; the default when empty |
| `remove_first()` | `T` | removes and returns the first element; the default when empty |
| `first()` / `last()` | `T` | the default when empty |
| `contains(value)` | `Bool` | `Int`/`Float`/`Bool`/`String`/enum elements only |
| `is_empty()` | `Bool` | |
| `clear()` | | drops every element, keeps the buffer's capacity |
| `reverse()` | | in place |
| `join(separator)` | `String` | every element that becomes text: `String`, a number, `Bool`, an enum value |
| `filter_<member>()` | `List<T>` | a new list of the elements whose Bool attribute is true (section 8) |
| `count_<member>()` | `Int` | a Bool attribute: how many elements have it true (section 8) |
| `sum_<member>()` | `Int`/`Float` | adds that attribute up across every element (section 8) |
| `find_by_<member>(value)` | `T?` | the first element whose attribute equals `value` |
| `sort_by_<member>()` | `List<T>` | a new list sorted ascending by an `Int`/`Float`/`String` attribute |
| `each_<member>()` | | `T` a class: calls that zero-argument function on every element, mutating it in place |
| `map_<member>()` | `List<U>` | an `Int`/`Float`/`Bool`/`String`/enum attribute's values, one per element |
| `any_<member>()` / `all_<member>()` | `Bool` | a `Bool` attribute, true for at least one / every element |

Naming (second batch item 3, decided 2026-09-19): `add` does not say where, so it is `append` (and `prepend`);
`pop()` became `remove_last()`, plus `remove_first()` -- writing the old names is a compile error naming the
replacement.

### Dictionary\<T\>  **[implemented]**

String-keyed, insertion-ordered (a linear scan under the hood -- simple and sound; this compiler's own tables
never get large enough for that to matter). `Dictionary<T>()` constructs one.

| Member | Result | Notes |
|---|---|---|
| `set(key, value)` | | replaces an existing key's value |
| `get(key)` | `T?` | |
| `has(key)` | `Bool` | |
| `remove(key)` | | |
| `count()` | `Int` | |
| `keys()` | `List<String>` | a view: a fresh list of the same keys |
| `values()` | `List<T>` | a view: a fresh list of the same values |
| `dictionary["key"]` (read) | `T` | the default when the key is absent |
| `dictionary["key"] = value` (write) | | same as `set` |
| `get_at(key)` / `set_at(key, value)` | `T` | the explicit call form of `dictionary[key]`/`dictionary[key] = value` |
| `each_<member>()` | | `T` a class: calls that zero-argument function on every value, mutating it in place |

Iterate values with `while` (`dictionary.values()`, or `dictionary.keys()` plus `dictionary[key]`), or use
`each_<member>()` when a class value just needs one of its own methods called on every entry. The whole member
template family works on a `Dictionary<T>` through its values, as it does on a `List<T>`.

`deep_copy()` is implemented for classes, lists and dictionaries. A `String` is shared rather than duplicated
because it is immutable; a union or a `type` shape is shared for now; a self-referring structure is still
unsupported (section 10).

### Heap\<T\> -- removed (D1, decided by Mortaro, 2026-09-19)

`Heap<T>` is gone. References are the default now (section 10), so a class/union containing itself recursively
just declares an ordinary attribute of type `T` -- no indirection needed, and no struct-layout cycle to guard
against (a class is always a heap object referred to by pointer, so a self-referential field is just a pointer
like any other):

```
union Expression {
    NumberExpression
    BinaryExpression
}
```
```binary_expression.spite
var left: Expression? = null
var right: Expression? = null

func BinaryExpression(left_expression: Expression, right_expression: Expression) {
    left = left_expression
    right = right_expression
}
```

Writing `Heap<T>`/`Heap<T>(value)` is a compile error naming this fix.

### System classes  **[implemented]**

Built-in classes, resolved and emitted the same way an ordinary `.spite`-file class is, with hand-written bodies
instead of a parsed one. Portable C underneath (`stdio`/`stdlib`, `_popen`/`popen`, and `#ifdef _WIN32` for
directory listing and process spawning).

| Class | Members |
|---|---|
| `File(path)` | `read(): String?`, `write(text): Bool`, `append(text): Bool`, `exists(): Bool`, `remove(): Bool` |
| `Directory(path)` | `files(): List<String>` (names, sorted), `folders(): List<String>` (sorted), `exists(): Bool`, `create(): Bool` |
| `Process(command, arguments)` | `run(): Int` (exit code; `arguments` is a `List<String>`, each shell-quoted), `output(): String` (stdout+stderr merged, valid after `run()`) |
| `Program()` | `exit(code)`: exits the process immediately with `code` |
| `Console()` | `print(...)`, `write(...)`, `error(...)`, `read_line(): String?` -- see below |
| `DynamicLibrary(file_name, naming, header)`  **[planned]** | every foreign function, constant and type of a native library, resolved by Symbol codegen -- see [Foreign libraries](#17-foreign-libraries-planned) |

`Console` is one of them and is a singleton (D52): `Console()` is the same instance everywhere, `Console` is an
ordinary class name rather than a reserved word, and `console.class.name` is `"Console"` like any other class.
It has `print(...)` (every argument printed, separated by a space, with a trailing newline), `write(...)` (the
same without the trailing newline), `error(...)` (the same as `print` but to the error stream), and
`read_line(): String?` (one line from the input stream without its line break, null only at the end of input
with nothing read). The entry constructor returning normally is exit code `0`.

### Pure Spite: dissolving the runtime  **[planned]**

D14 (decided by Mortaro, 2026-09-19): **the standard library being hand-written C is temporary, and the target
is pure Spite.** `src/runtime/spite_runtime.h` (722 lines) and `src/runtime/spite_repl.h` (1392 lines) exist
because the compiler needed a `String` before Spite could express one; they are a bootstrapping decision (logged
2026-09-19, "built-in classes with hand-written bodies"), not the design.

D6 already requires part of this: `Spite.Class` is "an ordinary standard library class with an ordinary
declaration", which cannot be true while the standard library is C.

What the current runtime is, and where each part goes:

| Lines | What | Becomes |
|---|---|---|
| ~400 | `String` methods, retain/release, number to text, `List<T>`/`Dictionary<T>` support | ordinary `.spite` sources -- pure algorithms over memory, expressible today |
| ~150 | `File`/`Directory`/`Process`/sleep | `DynamicLibrary` calls (section 17) -- exactly what the foreign function interface is for |
| ~130 | allocator wrappers and the `--debug-memory` live-pointer table | the table is Spite; `malloc`/`free` is the floor below |
| ~50 | float formatting through `snprintf("%g")` | a shortest-round-trip implementation in Spite, or an FFI call |

**The floor is a list of intrinsics, not a file.** A `String` needs allocation, allocation needs memory from the
operating system, obtaining it needs the foreign function interface, and naming a library there needs a `String`
-- so the bottom cannot route through the standard library. It resolves the way every self-hosted language
resolves it: five to ten compiler intrinsics that the compiler emits directly (raw memory in and out --
`mmap`/`VirtualAlloc` through the FFI on native, `memory.grow` as an instruction on wasm -- plus whatever the
emitted C needs before any Spite exists). Everything above that line is Spite.

- **Purity costs no performance here**, which is what makes it worth doing: the output is still C, so a
  Spite-written `trim()` goes through the same optimiser as the hand-written one. This is not a language
  rewriting its runtime in a slower language; it is the same machine code from a better source.
- It also delivers two things already wanted: standard library classes become reopenable (section 16 item 7) and
  inspectable in the REPL, because they stop being special.
- Order: the FFI (milestone 11) unlocks the system-call layer, self hosting (milestone 8b) makes it natural, and
  then the standard library moves file by file. `spite_repl.h` follows the same way -- it is an interpreter over
  generated tables, which is ordinary Spite once the reflection of milestone 10 exists.

**The web shim is the one honest exception, and its target is zero hand-written lines.** A file that runs in the
JavaScript virtual machine cannot be Spite by construction -- it is on the far side of a boundary Spite does not
own, and every language targeting the browser has one. What is achievable is that nobody writes it: the imports
come from `external js` declarations and the command-buffer drain loop is a switch over an opcode table, both of
which are data the compiler can emit. The source of truth stays Spite even though the artifact is JavaScript, so
nothing in the repository is written in a second language and nothing rots out of sync. Today that file would be
150-300 hand-written lines; generating it fully is a goal, not a day-one requirement, and it shrinks on its own
as wasm proposals land.

### JSON is reflection, not a library  **[planned]**

D22 and D23 (decided by Mortaro, 2026-09-19), amended by D31: writing JSON is walking a value's attributes and
reading it is a series of symbol keyed writes, so JSON needs no machinery of its own: a `type` shape is the JSON
shape. It is written WITH the metaprogramming once the standard library lives in Spite, which is the other half
of [Pure Spite](#pure-spite-dissolving-the-runtime-planned). JSON is a format for talking to foreign systems; it is
not what Spite programs send each other (see [The wire format](#the-wire-format-planned)).

- Writing never fails at run time: a class that cannot be serialised is a compile error, the same check
  isomorphic classes already need. Only parsing can fail, and it follows the failure model: a parse that may not
  succeed returns a `T?`, and a variant that crashes exists for callers who want the report instead.
- Open: the names of that pair. Mortaro sketched `to_json`/`to_crashing_json`; Claude prefers
  `parse_json()`/`parse_json_or_crash()` (unconfirmed).

### examples/calculator  **[implemented]**

A recursive-descent arithmetic interpreter (`token.spite`, `lexer.spite`, `expression.spite` + one file per union
member, `parser.spite`, `evaluator.spite`, `calculator.spite`), reading its program from `program.txt` next to it.
Supports `+ - * /`, unary `-`, parentheses, variable assignment (`x = 1 + 2`) and reference, one statement per
line. The lexer scans `source.split("")` with a `while` loop (accumulating each token's start/end index as a class
field, sliced out on a token boundary); the parser is genuinely recursive, including precedence climbing written
as tail recursion (`parse_expression_rest`/`parse_term_rest`), a style that reads well independently of `for`
being gone. Every place the parser hands a freshly parsed operand to `BinaryExpression(...)` does so inline
(never through an intermediate `var`): `left_expression`/`right_expression` are ordinary by-reference parameters
now (D1), stored directly into the union-typed field with no `Heap<T>` wrapper needed.

## 16. Decided by Mortaro, being implemented  **[planned]**

Moved here from `mortaros_notes.md` on 2026-09-18. Each item moves into its proper section above once
implemented. Items 2-5 moved out on 2026-09-19 (assert narrowing into section 5, Operators into a new section
5 subsection, the `Spite` namespace into section 8, numeric types into section 4); item 1 moved out on
2026-09-19 too (the formatter/linter, into the new [Style](#12-style-implemented) section). Item 6 (reference
semantics) moved out on 2026-09-19 -- decided (a) (D1) and implemented in milestone 9a; see [Memory](#10-memory-partial).
The second batch moved out on 2026-09-20 with milestone 9c: item 1 (running a script loads its parent folder)
into section 11, item 2 (`do` removed, plain-`if` narrowing) into sections 2/5/6, item 3 (no ambiguous standard
library names) into section 15, item 4 (variable shadowing) into section 4, item 5 (unused is an error) into
section 5, item 6 (the compiler has no warnings) into section 12, item 8 (`count()` vs `sum_<member>()`)
into sections 8 and 15. What remains from the second batch is item 7 (renumbered 1 below).

From `mortaros_notes.md` on 2026-09-19 (second batch):

1. **Standard library classes can be reopened** like any other class: the way to try out a package before upstreaming it.
2. **Comprehensive, Ruby grade reflection, at compile time.** Every object gets its reflection from source that Spite
   generates and that is VISIBLE in the final class output: `attributes` returning a `List<Spite.Attribute>`, `class`,
   `functions`, and so on are real generated members, not compiler magic. The goal is to cover most of what Ruby
   metaprogramming covers (enumerate and call functions by Symbol, respond-to checks, defining members from data,
   hooks when a class is reopened) resolved at compile time. Needs a design pass: see PLAN.md milestones 10a/10b.
3. **Errors: there are none to handle, only `crash`.** (third batch, 2026-09-20.) The main author of Spite code is an LLM, and
   bubbling errors up for someone to eventually log only makes code defensive. So there are no exceptions and no result
   types. Things that can simply not work return the default or a `T?` and are handled with `assert`.
   Things that stop the program from functioning call `crash("message")` (name decided by Mortaro: it is clear the program
   crashes and the AI needs to recode something). A crash stops the program with a report written for an LLM: the message,
   the Spite stack trace with `file:line`, and the attribute and local values of each frame, as text and as JSON in
   `.spite-cache/crash.json`. The same word works at compile time: lint rules and metaprogramming can `crash` the
   compilation when code does not follow the required structure, so structure is enforced instead of suggested.
   (proposed by Claude, unconfirmed: development builds log every `assert` that returned early, visible in the crash
   report and the REPL, so silent defaults stay debuggable; compile errors get a `--diagnostics=json` form with a rule id
   and the suggested fix.)
4. **The compiler is also the language server**, so editors get real time validation. After the bootstrap, written in Spite.
5. **Bootstrap as soon as possible** to start the repository; fancy features wait until after.

## 17. Foreign libraries  **[planned]**

D4 (decided by Mortaro, 2026-09-19): **a native library is a class, not a keyword.** There is no `external`
keyword, no per-symbol binding string, no generated-binding step and no hand-written C shim. `DynamicLibrary`
is an ordinary standard library class, and its Symbol codegen (section 8) resolves every foreign function,
constant and type at compile time.

```mouse.spite
var user32 = DynamicLibrary("user32.dll", 'windows', "windows.h")

func move_to(x_position: Int, y_position: Int): Bool {
    return user32.set_cursor_position(x_position, y_position) != 0
}
```

`set_cursor_position` becomes `SetCursorPos`. Nothing in that file is spelled the way C spells it.

### What a member of a library means

Spite already forces PascalCase on every type and snake_case on everything else, and the linter (section 12)
enforces it, so the language's own naming rule is what tells the compiler which kind of foreign symbol is being
asked for. No annotation is needed, and none is accepted.

| Written | Is | Emits |
|---|---|---|
| `user32.send_input(...)` | a call | `SendInput(...)` |
| `user32.input_mouse` | a read, snake_case | the constant `INPUT_MOUSE` |
| `user32.Input` | a read, PascalCase | the C type `INPUT` (reflection only -- see Structs below) |

A constant or a type is only available when the constructor was given a header; without one a library exposes
functions and nothing else, and a constant read is a diagnostic naming the missing header.

### The class

`DynamicLibrary(file_name, naming, header)`. Every argument must be a literal (the same rule `load(...)` already
has, section 11), because the compiler reads them during codegen.

```dynamic_library.spite
enum Naming {
    'identity'
    'windows'
    'camel_case'
}

var _file_name = ""
var _naming = 'identity'
var _handle = 0

func DynamicLibrary(file_name: String, naming: Naming, header: String) {
    _file_name = file_name
    _naming = naming
    _handle = _open(file_name)
    assert _handle
}

func symbol_name(symbol: Symbol): String {
    if symbol.kind == 'constant' {
        return symbol.name.upper()
    }
    if _naming == 'windows' {
        return symbol.name.abbreviated().pascal_case()
    }
    if _naming == 'camel_case' {
        return symbol.name.camel_case()
    }
    return symbol.name
}

func missing_function(function: Symbol, arguments: Arguments): Int {
    return _call(_handle, symbol_name(function), arguments)
}

func missing_attribute(attribute: Symbol): attribute.class {
    return _resolve(symbol_name(attribute))
}

func drop() {
    _close(_handle)
}
```

`_open`/`_call`/`_resolve`/`_close` are compiler intrinsics; everything above them is ordinary Spite, and
`--final-classes` prints the whole class with every generated binding, exactly as section 16 item 2 requires of
all reflection.

- **`missing_function` and `missing_attribute` are the only two new names in the entire foreign function
  interface.** They are Ruby's `method_missing` resolved at compile time: section 8's rule ("a parameter of class
  `Symbol` whose name is a segment of its function's name turns the function into codegen for every name that
  fits") taken to its limit, where the segment is the whole name. They get reserved names rather than falling out
  of the segment rule so that a class opts in by defining them, instead of silently swallowing every misspelled
  call. Section 8's existing precedence is unchanged: a function with the exact name always wins.
- `missing_attribute` returning `attribute.class` is the same form as section 8's `get_attribute(attribute:
  Symbol): attribute.class` -- the return type is whatever that symbol turns out to be: an `Int` for a `#define`,
  a `Spite.Class` for a type name.
- **The naming rule is a function, not a table.** Three rules cover a whole library, and binding one more symbol
  is one more call site, never a second line. `'windows'` runs `abbreviated()` before `pascal_case()`, so
  `set_cursor_position` -> `set_cursor_pos` -> `SetCursorPos`: the linter's abbreviation table (section 12, "kept
  in one place so it is easy to extend") read backwards *is* the Win32 name generator. The table that makes C's
  spellings illegal in Spite is the table that translates back into them.
- **Constant reads are the C spelling, lowercased**, with no prefix rule -- `user32.mouseeventf_leftdown`. This is
  deliberate: C constant prefixes (`MOUSEEVENTF_`, `INPUT_`, `WM_`, `SW_`) follow no rule any function could infer.
  Give them Spite names once, in a `var`, and the ugly spelling stops there.

### Structs

A `type` (section 7) whose fields are all scalars is a C struct when it reaches a foreign call: passed by
address, never as a Spite heap object. There is no annotation -- the value's type comes from the `var`'s own
annotation or from the parameter it feeds, exactly like any other object literal matched by shape.

```mouse.spite
type Input {
    kind: UnsignedInt
    padding: UnsignedInt
    x_movement: Int
    y_movement: Int
    wheel_amount: UnsignedInt
    event: UnsignedInt
    time_stamp: UnsignedInt
    extra_information: UnsignedLong
}

func _send(event: UnsignedInt, wheel_amount: UnsignedInt) {
    var input: Input = {
        kind: _mouse
        padding: 0
        x_movement: 0
        y_movement: 0
        wheel_amount: wheel_amount
        event: event
        time_stamp: 0
        extra_information: 0
    }
    user32.send_input(1, input, Input.size)
}
```

- **Spite lays the struct out itself**, so the layout is yours to get right, including padding a C union leaves
  behind (the `padding` field above is there because `INPUT` is a `DWORD` followed by a union that aligns to 8).
  This is the same bargain `#[repr(C)]`, `extern struct` and `ctypes` make, and it is the price of never naming
  the C type in your own source.
- `Input.size` is `sizeof` of the struct the compiler emitted -- ordinary reflection on a Spite type, with no
  library in the expression.
- **The compiler checks your work for free.** When the library was given a header, it derives the C name through
  the same naming rule (`Input` -> `INPUT`) and emits
  `_Static_assert(sizeof(INPUT) == sizeof(Mouse_Input), "mouse.spite:12 type Input does not match INPUT");`.
  A forgotten padding field fails the build at the Spite line, naming both types. **The derived C name is used to
  verify, never to generate.** Size is checked; field order is not, because C's own field names (`dwFlags`, `dx`,
  `mouseData`) are unmappable by any rule, and a table for them is exactly what this design refuses to have.
- A `type` containing a `String`, `List<T>`, `Dictionary<T>` or class field is a diagnostic at a foreign call,
  naming the field: the all-scalars condition is enforced, not assumed.

### What crosses

| Spite | C | Cost |
|---|---|---|
| `Tiny`..`Long`, `Byte`..`UnsignedLong`, `Float`/`Double`, `Bool` | `int8_t`..`uint64_t`, `float`/`double`, `bool` | zero -- section 4's numeric types already are the C types |
| `String` (argument) | `const char*` | zero -- every `String` buffer is already NUL terminated |
| `String` (result) | `const char*` | one copy; Spite never frees a buffer C owns |
| an enum value | its integer | zero |
| a scalar-only `type` | a struct, by address | zero |
| `List<T>` of scalars | the element array, by address | zero |
| a class instance | -- | never. Its layout (`SpiteHeader`, ref count) is the compiler's business |

**Return types.** A foreign call returns `Int` (integer/pointer width) and section 4's right-to-left casting takes
it from there. A `Double` comes back in a different register and cannot be inferred from context, so it is asked
for by suffix -- `user32.get_scale_as_double(...)`, and `..._as_text()` for a copied `const char*` -- which is
section 8's ordinary segment template, not a new mechanism.

### Lifetime, and what gets linked

- The library handle is a plain `Long`. **There is no `Pointer` type in Spite**, and this design does not add one.
- `DynamicLibrary` is a **singleton** (open question 7 -- the syntax is not decided yet, which is why the class above
  does not show one), keyed by its constructor arguments: every class asking for
  `DynamicLibrary("user32.dll", ...)` shares one object and one import table; `"gdi32.dll"` is a second one.
- The import table is the set of call sites that survived tree shaking: two symbols are resolved because two were
  called. They are resolved once, in the constructor, so every later call is one indirect call -- never a lookup
  per call.
- A missing library or a missing symbol aborts at construction, naming the file, the symbol and the Spite function
  that wanted it. This is a program-stopping error and is not reported any other way.
- `drop()` closes the library when its last reference goes, like any other class.
- `--final-classes` prints each binding with its resolved name and library as a `#` comment, the same way it
  already annotates which root won a reopened function -- so the mapping is reviewable generated output rather
  than something maintained by hand.

### Other environments  **[planned]**

The same mechanism is how Spite reaches anything that is not C. Nothing here is new machinery: it is section 11's
loading and reopening, plus a codegen value.

- **`$target`** is a compiler-provided program variable (`--target=native|web`), reserved like every other flag
  name in section 13.
- **Platform code is chosen by reopening, not by an abstraction layer.** `platform_web/console.spite` and
  `platform_native/console.spite` reopen the same `Console`; the entry constructor does
  `if $target == 'web' { load("platform_web") } else { load("platform_native") }`, and section 9's compile-time
  folding plus tree shaking remove the loser entirely. This is the game-mod mechanism from section 11 aimed at
  platforms.
- **The web bridge is `DynamicLibrary` reopened.** `platform_web/dynamic_library.spite` replaces `_open`/`_call`
  with a JavaScript module's exports; a foreign object that cannot cross into wasm is a handle into a JS-side
  table, freed by the same `drop()`. A class written against a library compiles for both targets unchanged.
- The C backend emits wasm through a C compiler targeting `wasm32-freestanding`, **plus the JavaScript glue module**,
  generated from the same declarations -- hand-written glue is what makes wasm painful everywhere else, and this
  compiler already generates C.
- Which classes exist on which target is not a special rule: a class declares it (see [Targets](#targets-planned)),
  so `File`, `Directory` and `Process` on the web are a compile error like any other class used off its target.
- The isomorphic direction (`$environment=server|client`, already in `examples/arsenal`) is what the next section
  describes: a class compiled into both bundles, whose server-only functions become a network call on the client
  and a route registration on the server.

### Isomorphic classes: where a function lives  **[planned]**

D13 (decided by Mortaro, 2026-09-19): **a function's first parameter decides which bundle it is compiled into.**
There is no annotation, no naming convention and no compiler trick -- the framework reads it off the signature
with ordinary compile-time reflection (section 8).

```user_repository.spite
func find_user_by_id(context: ServerContext, id: Int): User { }

func load_users(context: ClientContext) { }

func format_name(first_name: String, last_name: String): String { }
```

| First parameter | Compiled into |
|---|---|
| `ServerContext` | the server bundle only; the client gets a generated stub that calls it over the network |
| `ClientContext` | the client bundle only |
| neither | both -- isomorphic |

- **The marker is the capability.** A server function needs a server context (database handle, request, session)
  and a client function needs the client one, so the parameter that says where the function lives is the same
  parameter that carries what it is allowed to do. It cannot drift from the truth: a function cannot claim to be
  server-side without holding server capabilities, and cannot reach a server-only API without asking for the
  token that provides it. The same shape extends to any capability (`DatabaseContext`, `FileSystemContext`)
  without another language feature.
- **The test is identity, not a name.** `function.arguments[0].class == ServerContext` compares two
  `Spite.Class` values, so a renamed or misspelled class is a compile error rather than a predicate that quietly
  turns false and drops an endpoint. Matching on `.name.starts_with("Server")` would work and is what a language
  without real reflection has to do; Spite has the class object, so it uses it.
- **A union gives exhaustiveness.** With a `union Context` of `ServerContext` and `ClientContext`, a `switch` over it
  must cover every member (section 7), so adding a third environment later -- a worker, an edge runtime -- fails
  to compile everywhere that has to change, instead of silently taking a default branch.
- **The body is never emitted into the client bundle.** Only the generated call stub is, so a secret, a
  connection string, or a server-only helper reachable from that body is removed by the per-bundle tree shaking
  section 11 already does. This is the property that makes the split safe rather than merely convenient, and it
  costs nothing extra here.
- Routes are generated deterministically from the class and function name, and the wire format is already in the
  language: a `type` shape (section 7) is duck typed and JSON shaped, which is what it was introduced for.
- **A function crossing the boundary must have serialisable parameters and return type** -- scalars, `String`, a
  `type` shape, or a list of those. A class instance cannot cross (its identity and reference count are local),
  and the diagnostic names the parameter or field that does not qualify.
- **Open, and blocking:** what a failed call does. A network call fails in ways a local call cannot, and Spite's
  position is that only errors which stop the program deserve to exist (`mortaros_notes.md`). This is the case
  where that has to be answered concretely, so the errors design gates this milestone and nothing else does.

### Targets  **[planned]**

D20 (decided by Mortaro, 2026-09-19): a class says where it can run by overriding an ordinary member of
`Spite.Class`, `func targets(): List<Symbol>`. The default is every target; `DynamicLibrary` answers `['native']`
and `Html` answers `['web']`. Using a class against the wrong `$target` is a compile error naming the class, the
target and the flag. This replaces any special rule about `File`, `Directory` and `Process` on the web: they
declare their targets like everything else. The check runs after compile time folding and tree shaking, so a
`load(...)` behind `if $target == 'web'` stays legal: only a class still reachable in the built program is checked.

### Html: the browser as a library  **[planned]**

D19 (decided by Mortaro, 2026-09-19): `Html` is the web counterpart of `DynamicLibrary`: a singleton whose
`missing_function` and `missing_attribute` resolve against the browser. `Html.Node` wraps one node handle (an index
into a table on the JavaScript side, freed by `drop()`). Names convert with the `'camel_case'` naming rule:
`document.create_element("div")` is `createElement`, `node.text_content = "Hello"` is `textContent`,
`root.append_child(node)` is `appendChild`. A call is a method and a read or write is a property, the same split
foreign libraries use for functions and constants. `Html` is the low level layer the markup builder compiles down
to, as `Mouse` sits on `DynamicLibrary`.

Open: the markup builder below was sketched as `html.div(...)`, which now collides with this class. It needs
another name (`Markup`? `View`?). Mortaro has not picked.

### Markup  **[planned]**

D18 (decided by Mortaro, 2026-09-19): there is no JSX and no trailing block. Markup is `missing_function` plus
literals, with as many children as the call has arguments. That costs no new feature, because a Symbol codegen
function is generated per call site:

```
html.div({ class: "card" }, html.h1(title), TodoForm({ ondone: add_todo }))
```

It pulls together what already exists: tags come from `missing_function`, a list of children is
`todos.map_render()`, a component is any class that fits a `type` requiring `render()`, a handler is a function
value bound to its instance, and `when(condition, element)` returns an `Element?` that the framework skips.

Rejected, so nobody proposes them again: a JSX like literal is a second grammar and fixes neither conditionals
nor mapping; a trailing block (`f(x) { a b c }`) serves exactly one shape, since queries are
`database.filter_age_greater_than(10).sort_by_name()` and routes are plain statements, so it would be one more
meaning for `{ }` bought for a single case. Mortaro on the verbosity: annoying for a human to write, cheap for an
AI to write, and a human can still easily read it, which is the first line of the philosophy.

### The wire format  **[planned]**

D31 (decided by Mortaro, 2026-09-19): JSON is not what isomorphic classes send. Both bundles compile from the same
source, so there are no unknown consumers and nothing needs to describe itself: the wire carries a binary packing
derived at compile time, attributes in declaration order, no keys and no parsing. It compounds on the web, where
nothing has to be turned into strings to cross into JavaScript: the DOM command buffer and the remote call channel
are both opaque byte buffers. Proposed by Claude, unconfirmed: a schema hash in the handshake so an old client
meeting a new server crashes with a clear report instead of misreading bytes, and `--development` decoding
payloads to JSON on demand, since the compiler knows the schema.

### Not designed yet

- **Callbacks** (handing a Spite function to C as a function pointer). Needs a decision on function references,
  which the language does not have.
- **A C union past its first member.** Positional layout reaches the first member only; the rest needs C's own
  field names.
- **Varargs**, structs returned by value, and `#define`s that are function-like macros rather than constants.

---

## Open questions

1. `var damage: $damage_type = null`: `null` otherwise only exists for `T?`. PROVISIONAL: the compiler treats
   `= null` on a `$generic`-typed variable/field as "the default value of whatever type the generic is bound to" (not
   `T?`). This is implemented but still provisional -- revisit if it reads confusingly once more code exists.
3. Right-to-left casting makes `age > 0.5` with an Int `age` mean `age > 0`. Accept, or make comparisons cast toward the wider type.
   - The abbreviation lint has no escape hatch for names that must mirror an external spelling (`keyword_var`). Keep it absolute, or allow a per line `# spelled: keyword_var` style exemption.
6. `_` now means two things: private (section 2) and intentionally unused (section 5). They mostly agree (an unused
   private function is fine either way), but an unused PUBLIC function cannot be an error (libraries are full of them; tree
   shaking removes them), so "unused" is only enforced for locals, parameters and private functions. Confirm.
8. **An unrelated `get_<attribute>()` silently intercepts a read.** Found by the 2026-09-20 reflection port, in
   this compiler's own `NullableType`: a class with an attribute `inner` and a zero-argument function `get_inner()`
   written for an unrelated purpose has every outside read of `.inner` routed through that function, because that
   is exactly what attribute interception (section 5) says to do. It is the rule working as specified and it is
   still a trap, since nothing announces it.
   - **Keep it.** The rule is uniform and a reader who knows it can see the collision.
   - **Make it a compile error when the function's return type differs from the attribute's** (proposed by
     Claude). A real getter returns what the attribute holds; an unrelated function usually does not, so this
     catches the accidents and leaves the genuinely ambiguous case -- same name, same type -- being treated as
     a getter, which is defensible. It is also the shape this language reaches for everywhere else: something
     the compiler can know becomes a compile error naming the fix, rather than a warning or a convention
     (section 16 item 6, D24).
   The residual case neither option catches is an unrelated function whose return type coincides with the
   attribute's. Claude would accept that.
9. **Text with `${name}` in it prints literally** (found 2026-09-20 by writing the programs an AI would write).
   Spite builds text with `+`, and `"hello ${name}"` is simply those characters, so the mistake is silent -- the
   one outcome section 16 item 6 says a compiler should never produce. `$` is already the codegen sigil, so
   `${` inside text is unlikely to be meant literally.
   - **Make `${` inside text a compile error** naming `+` (proposed by Claude). A dollar before a brace has no
     other use, and text that genuinely needs it can be built with `+` or written `$` `{` apart.
   - **Leave it.** Text is text, and a rule about what may appear inside it is a rule to remember.
10. **How `--final-classes` shows which root supplied a declaration.** D7 and milestone 10a both say a
    reopening must not be silent, and `--final-classes` is where it stops being silent -- but what it writes is
    a *program*: running the printed entry file runs the same program, which is what makes it proof rather than
    a report. Provenance cannot be a comment, because a comment is only ever a link to a markdown heading
    (section 12), and it cannot be a declaration without changing the program.
    - **A separate manifest** beside the printed classes (proposed by Claude): one line per declaration with the
      root it came from. Keeps the printed source a program, and the thing you grep is a table rather than
      prose scattered through files.
    - **Print it to the console** as the classes are written, so it is read once and not stored.
    - **Give the comment rule one more form**, a provenance line the compiler writes and a human never does.
11. **Whether a `type`'s function members should be written as function-valued attributes** (Mortaro, 2026-09-21,
    thinking aloud rather than deciding). Today a shape writes `hit(): Int`; the alternative is
    `hit: Spite.Function<Int>`, which would make a required function an ordinary attribute whose type happens to
    be a function, and would leave a `type` with exactly one kind of member instead of two.
    - **It needs D39 first**: the typed `Spite.Function<Arguments..., Return>` does not exist yet, so the form
      cannot be written or printed.
    - Against: `hit(): Int` reads like the declaration it matches, and a shape is matched against functions
      written `func hit(): Int`.
    - For: one kind of member, and it composes -- a shape could then require a function value it will *store*,
      which `hit(): Int` cannot express.

12. **A header form for generics, with constraints** (Mortaro, 2026-09-23, asked to be argued with). The proposal:
    `generic $type` lines at the top of the file beside `singleton`, and a described generic
    `generic $sub_type { initial_value: Spite.Function<$sub_type> }` that narrows what may be supplied. The
    problem it solves is real: a class that takes codegen values must have a constructor to declare them, even
    when it has nothing to construct, and the constructor form has nowhere to say what a supplied class must be
    able to do. And the distinction Mortaro draws is the right one -- a described generic is *one* class for every
    place that uses it, where a `type` accepts anything that quacks, afresh at each call.
    Claude's argument, for and against (proposed by Claude, unconfirmed):
    - **For the header line:** `singleton` already made a header line a pattern, so parity is a real argument, and
      the objection that removed the old `generics` line (SPITE.md: "feels outside of our patterns") was about
      an ordered *list* in a line of its own; one `generic` per line is a declaration like `var`, not a header.
    - **Against the inline block:** it is an anonymous `type`. Write the constraint as a shape the program
      already has -- `generic $sub_type: Openable` -- so one mechanism describes what a class must be able to do.
      An inline block is then just sugar for an unnamed `type`, and can come later if it is missed.
    - **Against `gen`:** it is an abbreviation, and the abbreviation rule has no exceptions for keywords.
      `generic` says what it is.
    - **What becomes implicit:** a caller's `Journal<String, Chapter>()` is positional, so the order of the
      `generic` lines becomes the order callers write. The constructor form made that order visible in one
      signature; the header form spreads it over lines. Acceptable, since D67 fixes where the lines go.
    - **The example's `var opened = $sub_type`** reads as assigning a class to a variable. Claude assumes
      `var opened: $sub_type` was meant.
13. **How casting works, so a class can define its own casts, and how to name a variable's class as a type**
    (Mortaro, 2026-09-23: "a thing for you to ask me later"). Example shape: `func from_type(type: Symbol, value:
    type.class)`. Not argued yet; waiting to be asked. D59 (arguments cast to their parameter type) is where it
    will first matter.
14. **An ABI for variadic arguments** (Mortaro, 2026-09-23, "fight me on this before we implement"). Proposed:
    `func hello(world: String, ...args: List<Spite.Argument<String>>)`, which would make `Spite.Argument` generic.
    Claude argues against the `Spite.Argument` part (proposed by Claude, unconfirmed): `Spite.Argument` is the
    *reflection* of a parameter -- a name and a class, known at compile time -- and a variadic argument is a
    *value* at runtime. Making one class carry both merges the two levels D11 keeps apart. `...args: List<String>`
    already says everything: the `...` means "the caller writes these one by one", the list is the type the
    function receives. For `Console.print`, which takes anything, the element type is a `type` every printable
    value satisfies -- `...values: List<Printable>` -- so the language needs no new class, only `...`.
15. **Whether `while` can go** (Mortaro, 2026-09-23: investigate every use; if it can be rewritten with
    metaprogramming, make it an error). Measured on 2026-09-23 over the compiler, `library/`, `scripts/`, the
    tests, the examples and the corpus: 259 `while` loops, and 199 of them are the same shape -- an index from 0
    to `list.count()`, reading `list[index]`. Every one of those is a member template (`each_`, `map_`,
    `filter_`, `find_by_`, `any_`, `count_`, `sum_`). The other 60 are genuinely loops over state: the lexer
    scanning characters, the parser consuming tokens, walking backwards, settling until nothing changes.
    Proposal (Claude, unconfirmed): make the 199-shape an error naming the template -- detected as a `while`
    whose condition compares a counter with `.count()` of a list the body indexes by that counter -- and keep
    `while` for the rest. This pairs with D64: an index read is where `[]` becomes `T?`, and removing the
    index loops removes almost all of the narrowing D64 would otherwise demand. What the templates still lack
    for the compiler's own loops: the index inside the body, and stopping early.
16. **Two versions of one dependency** (Mortaro, 2026-09-23). When two packages load the same git dependency at
    different commits and the difference changes nothing either package uses, the compiler should just use one,
    without asking. When the versions differ in members that are used, the compiler treats them as two packages
    in two namespaces, so both keep working, and a command lists these splits for whoever wants to unify them --
    "not actually broken, just annoying". Belongs to D38 (a dependency is a git URL plus a commit in `load`),
    which is not implemented; recorded so the design starts here.
17. **A syntax for `this`** (Mortaro, 2026-09-23): to be discussed when something needs it. Today bare names
    reach attributes and `class` is the instance's class, so nothing does yet.
18. **The entry file is always the file named after its folder** (Mortaro, 2026-09-23, asked to be argued for).
    `spite hello` runs the `hello` folder's entry file; a file name on the command line is no longer accepted.
    The case for it (Claude): discovery already requires an entry folder, so the file name on the command line
    only restates the folder -- two ways to say one thing, and the second can disagree (`spite hello/other.spite`).
    One way to run a program is the rule SPITE.md states under "more than one way to do a thing". The cost is
    small and known: editors that pass the current file will pass its folder instead, and a compile error naming
    the folder covers the habit. Claude would do it.
19. **Constants without a `const` keyword, and reflection attributes that cannot be overwritten** (Mortaro,
    2026-09-23). Constants: yes, without a keyword -- the compiler sees the whole program, so an attribute that
    nothing assigns after its default (no assignment, no `set_` call, no reflective write) is a constant and
    folds. Nothing is declared; it is just what the optimiser finds (D36). Read-only attributes: Mortaro's idea
    is to declare no attribute at all, only `get_attributes()`, which attribute interception already turns into
    a readable `.attributes`; with no `set_attributes()` there is nothing to assign through, so writing it is an
    error. What the compiler needs (Claude, unconfirmed): a read of `.name` that finds no attribute but finds
    `get_name()` is an attribute read, and a write to it with no `set_name()` is an error saying the attribute
    is read-only. The storage behind it is a private `_attributes`, which the compiler fills as it fills
    `attributes` today.
20. **Whether a nested `if`/`else` is an error** (Mortaro, 2026-09-23: "we should discuss"). Claude's view: nesting
    is where generated code becomes unreadable, and the language already removes the common cases -- a
    precondition is `assert` (D54), a choice between kinds is a `switch` over a union or enum. What remains is a
    genuine decision tree, and forbidding it pushes it into a helper function, which is usually the right call.
    A concrete rule to decide on: an `if` with an `else`, inside a branch of another `if` with an `else`, is an
    error that names extracting a function.

## Decision log

| Date | Decision |
|---|---|
| 2026-09-18 | `#` comments; newlines and commas both separate entries; only `func` (not `function`). |
| 2026-09-18 | Entry point is the entry file's constructor, not `main`. |
| 2026-09-18 | Packages are loaded by their root (`load("package")`); the root is not part of the namespace. |
| 2026-09-18 | Folders are never uppercase; namespaces are the PascalCase of lowercase folder names. Linter rule later. |
| 2026-09-18 | `load` is a bundle boundary (webpack async import); inside an `if` it lazy loads. Argument is a literal. |
| 2026-09-18 | Enums, types and unions declared in a file are namespaced under its class (`Player.Job`). |
| 2026-09-18 | Enum values resolve from usage; shared value names are fine; ambiguity is a compile error. |
| 2026-09-18 | `.attributes` is a list of `SpiteAttribute`; `.class` is a `SpiteClass`; `attributes[symbol]` indexes the former. |
| 2026-09-18 | `return` is always explicit; return type is always `(): Type`. |
| 2026-09-18 | `$` means "replaced at codegen", fed by generics or by compiler flags. |
| 2026-09-18 | Classes and the instances registry conceptually live on the heap; unused parts are removed at compile time. |
| 2026-09-18 | No file level statements: only declarations. |
| 2026-09-18 | (proposed by Claude, unconfirmed) Compiler flag names are reserved; an unmatched `--name=value` is an error. |
| 2026-09-19 | The reflection loop is written `for instance.attributes do attribute`; the old bare `for instance do attribute` form is removed. |
| 2026-09-19 | Reserved compiler flag names (`development`, `optimized`, `output`, `help`, `debug_memory`, `emit_c`, `dump_tokens`, `dump_ast`, `repl`, `repl_port`) may not be used as a `generics $name` or a `--name=value` program variable. |
| 2026-09-19 | A `--name=value` that matches no `$name` used anywhere in the program is a compile error (typo protection). |
| 2026-09-19 | (proposed by Claude, unconfirmed) String is a single-owner immutable value (a length-prefixed buffer, not a true reference count); `Dictionary<T>` is String-keyed, insertion-ordered, and implemented as a linear scan; `Heap<T>` is a plain pointer, freed by whichever single thing owns it -- see [Standard library](#15-standard-library-partial) and open question 2. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `File`/`Directory`/`Process`/`Program` are built-in classes (hand-written bodies, not parsed from `.spite` source) rather than a new kind of special-cased value, so ordinary member access, method calls, and drop chaining all apply to them for free. |
| 2026-09-19 | (proposed by Claude, unconfirmed) A class or union's C struct is now forward-declared (`typedef struct Name Name;`) before its own fields/members are resolved, and a union is registered before its members resolve (not after): both are what let a class or union recursively contain itself through `Heap<T>`, fixing the previously documented "unions not resolved in dependency order" limitation for the self-recursive case. |
| 2026-09-19 | `null` on a `$generic`-typed variable/field means that generic's bound type's default value (provisional; see open question 1). |
| 2026-09-19 | Spite has only the `while` loop; `for` is removed entirely (decided by Christian: "only the while loop, no for; that makes people favor the metaprogramming"). `do` stays for `if nullable do value`. Writing `for` is a parse error naming `while` and the `List<T>`/`Dictionary<T>` metaprogramming helpers. |
| 2026-09-19 | `value.attributes` is a real runtime `List<SpiteAttribute>` (`.name`/`.class`/`.value`, all `String`), built inline only where `.attributes` is actually used; the old compile-time-unrolled `for instance.attributes do attribute` loop is gone along with `for` (decided by Christian, as a consequence of the `while`-only decision). |
| 2026-09-19 | `List<T>` gains `each_<member>()`, `map_<member>()`, `any_<member>()`, `all_<member>()`; `Dictionary<T>` gains `each_<member>()` -- compiler built-ins in the same style as `filter_`/`sort_by_`, meant to make `while` rarely necessary (decided by Christian). |
| 2026-09-19 | (proposed by Claude, unconfirmed) Ownership rules (moves, temporaries, overwrite-drop -- manual.md's Memory section): a move is sound-by-construction only in the exact scope that declared the local, so a move from inside a conditional/loop body is a compile error rather than a runtime-tracked flag -- the simpler of the two options considered. An alias stored a second time is deep copied only for `String`; every other owning type keeps the pre-4d shallow-alias behavior (a documented, unfixed gap) rather than risk an unsound generalization. `--debug-memory`'s free wrapper now tracks every live pointer and aborts on a double free or an unknown pointer, instead of only counting allocations/frees. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `load(...)` roots are discovered by a purely static, pre-codegen AST scan of every function body reachable from the entry file's own root, not by a runtime/reachability-precise pass; every root it finds is over-inclusive rather than under-inclusive (a `load` inside a function tree-shaking would later remove still counts). Its path is always resolved relative to the entry file's own folder, even when the `load` call itself lives inside already-loaded code. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Only the entry file itself is scanned for `load(...)` calls from the entry root; an unrelated sibling file in that same flat folder (a second, independent entry-style program living next to it, e.g. two of this compiler's own examples sharing a folder) is not, even though it may still be pulled in as an ordinary tree-shaken class if referenced. Every file in an explicitly `load`-ed root is always scanned, since loading a root is itself the deliberate act that makes its files part of the program. A `load` reachable only through an ordinary (non-`load`) function call from the entry constructor into a *different* sibling file in the entry folder is a known, undocumented-until-now gap of this simplification. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Every class, and every enum/type/union declared inside one, is keyed everywhere (the C identifiers `resolveClass` emits, and the map key `classes`/`enums`/`unions`/`raw_type_declarations` use) by its full namespace-mangled name (`.` replaced with `_`); a class's own file-given simple name (`ClassInfo.source_name`) is kept separately, purely to recognize its constructor. A folder's entrypoint file is registered under two keys pointing at the same class (`Engine.Renderer` and `Engine.Renderer.Renderer`); an enum/union/type declared with the exact same name as its own owning class (e.g. `expression.spite`'s `union Expression`, next to that file's own otherwise-unused implicit empty `Expression` class) gets the same treatment, aliased at the owner's own bare name -- without it, that bare name would resolve to the coincidental empty class instead, since a class and its own like-named nested declaration are checked in the same unqualified-name search chain (enums/unions/types checked before classes). |
| 2026-09-19 | (proposed by Claude, unconfirmed) Reopening (the discovery pass's merge pass, before any class is even registered with `codegen`): a later root's `func`/`var` of the same name replaces the earlier one and is what `codegen` ever sees (never both); origin metadata (which root, and which earlier root(s) it replaced) is tracked on the side, only for `--final-classes` to print as `#` comments. `enum`/`type`/`union` declarations inside a reopened class are simply concatenated across every contributing root rather than merged by name -- a genuine duplicate is still caught (as "declared more than once"), by the same registration check an ordinary duplicate hits, but reopening *adds to* a class's own nested enum/type/union, not *replacing* one, is not attempted. A `var`'s replacement is checked for an explicit-type mismatch by comparing the two parsed `ast.Type` trees structurally; a `var` inferring its type from its value (no explicit annotation) on either side is not checked, since that would need real type resolution before any class is registered. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `--final-classes` prints the merged, winning source the discovery pass already builds (via a new the canonical printer AST -> source printer, every binary/unary expression fully parenthesized so print -> parse -> print is trivially stable) with `#` origin comments before each reopened `func`/`var`; it does not yet also list a class's instantiated Symbol codegen functions (`set_age`, ...) or used `List<T>`/`Dictionary<T>` helper signatures, since that information only exists deep inside `codegen`'s own resolution pass, after the program model the discovery pass builds has already been consumed -- left for a later milestone. |
| 2026-09-19 | `assert value` (and `assert value and other_condition`) on a `Nullable<T>` local/parameter/field narrows it to `T` for the rest of that block and any nested block, reading/writing/calling through to the value held inside the `Nullable` in place; reassigning the narrowed name to `null` is a diagnostic. A function whose body's last statement is `if value do name { ... }` -- wrapping the rest of the function only to check existence -- is a lint error naming the `assert` rewrite; `if ... do` stays legal everywhere else (decided by Mortaro). |
| 2026-09-19 | Every operator is a function (full table in the new [Operators](#operators-implemented) section): `+`/`-`/`*`/`/`/`%` are `sum`/`subtract`/`multiply`/`divide`/`remainder`; `==`/`<`/`>` are `equals`/`less_than`/`greater_than` (`!=`/`<=`/`>=` derived); `a[x]`/`a[x] = v` are `get_at`/`set_at`; unary `-` is `negate`. `not`/`and`/`or` stay built in. A user class or union answers these the same way it already answers a Symbol-codegen method call (decided by Mortaro). |
| 2026-09-19 | (proposed by Claude, unconfirmed) Attribute access from outside a class also goes through operator functions: `person.age = 1` calls `set_age`; `person.age` (a read) calls `get_age` the same way, except when the getter's return type owns something (a getter for a `String`/`List<T>`/... field falls back to a raw field read instead, to avoid leaking a getter that hands back a *fresh* owned value through a read that is, syntactically, still a plain `.member` expression to every ownership-tracking call site). The manual only decided the setter half of this; the read half and its owning-type restriction are Claude's own extension and known limitation, not confirmed by Mortaro. |
| 2026-09-19 | (proposed by Claude, unconfirmed) A method's own parameter/return type may now name its own class by value (`func sum(other: Vector): Vector` inside `vector.spite`), needed for operator functions to read naturally: `Generator.self_reference_class` lets `resolveClass`'s reentrancy guard tell that case apart from a genuine field-cycle (a class embedding itself by value), which still errors. |
| 2026-09-19 | `Spite` is a reserved root namespace: `SpiteClass`/`SpiteAttribute` are renamed to `Spite.Class`/`Spite.Attribute`, and a user folder (or `load(...)` root) named `spite` is a diagnostic instead of silently colliding with it (decided by Mortaro). |
| 2026-09-19 | (proposed by Claude, unconfirmed) Numeric types: `Tiny`/`Short`/`Int`/`Long` (`int8_t`/`int16_t`/`int32_t`/`int64_t`), `Byte`/`UnsignedShort`/`UnsignedInt`/`UnsignedLong` (`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`), `Float`/`Double` (32-/64-bit) -- `Int` and `Float` change from this milestone's earlier `int64_t`/`double` to `int32_t`/32-bit `float`, per Mortaro's instruction that all of them get written names. Converting between numeric types wraps on overflow (an ordinary C narrowing cast). `Float`/`Double` print with shortest-round-trip formatting (increasing `%g` precision until the parsed-back value matches exactly, skipping a low-precision match that only round-trips in scientific notation) rather than a fixed precision, so `Float` becoming 32-bit does not make ordinary values print with ugly trailing digits. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `List<T>`/`Dictionary<T>`'s internal storage (`count`/`capacity`/index parameters) stays `int64_t` regardless of the numeric type table above -- only the type `Type` values codegen exposes to Spite source changed; the C already narrows/widens implicitly at every call site, so this needed no changes to `List<T>`/`Dictionary<T>`'s generated support functions. |
| 2026-09-19 | (proposed by Claude, unconfirmed) 6a reflection tables are pure data (`SpiteReflectAttribute`/`SpiteReflectListType`/... in `src/runtime/spite_repl.h`): every offset/size is the real C `offsetof`/`sizeof` of the concrete generated type, computed once per class/enum/`List<T>`/`Dictionary<T>`/`Nullable<T>`/union actually in the program, so the interpreter is one hand-written file driven entirely by generated constants -- it never special-cases a particular program's types. `Heap<T>` is folded into whatever `T` reflects as, plus one `via_pointer` bit saying the field itself is already the pointer to walk through. |
| 2026-09-19 | (proposed by Claude, unconfirmed) A REPL build (`--repl` and/or `--repl-port`) always compiles with the debug allocator enabled (`SPITE_DEBUG_MEMORY`), independent of `--debug-memory`, so the `memory` command has something to report and the `--repl` end-to-end test can assert balanced memory the same way every other example's `--debug-memory` test does. The debug allocator's live-pointer table (added in 4d for double-free detection) now also records each pointer's size, so it can report live bytes, not only a live count. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `--repl-port`'s "exit" command calls C `exit(0)` directly from the server thread rather than signaling the main thread to unwind: the entry constructor may be running its own infinite `$serve` loop and never return control to `main`, so there is no other point guaranteed to ever run. `--repl` (local, stdin) has no such problem -- its loop runs after the constructor already returned, on the main thread, so it can fall through to the ordinary drop-and-return-0 path for a clean, balanced exit instead. |
| 2026-09-19 | (proposed by Claude, unconfirmed) A REPL build's own `main()` is run with inherited (not captured) stdio: unlike a normal `spite file.spite` build (whose child process's output is captured and printed after it exits, for the compiler's own non-interactive use), a `--repl`/`--repl-port` program is interactive or long-running, so the driver's `compileAndRunWithOptions` spawns it with `std.process.spawn`'s default inherited stdin/stdout/stderr instead of `std.process.run`. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `spite connect` is a client built into the compiler binary (the compiler's own process already runs on `std.Io`), not a libc socket call through `@cImport` -- `std.Io.net.IpAddress.connect` gave everything needed (connect, buffered line reader/writer) without guessing an unfamiliar 0.16 API. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `Program().sleep(milliseconds)` (used by `examples/dungeon`'s `$serve` loop) is a normal built-in on the existing `Program` system class, not REPL-specific -- `spite_sleep_milliseconds` lives in `src/runtime/spite_runtime.h` (always embedded) rather than `spite_repl.h` (REPL-only), so it works in an ordinary build too. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `examples/dungeon` requires `--serve=true`/`--serve=false` on every run (like `examples/arsenal` already requires `--environment=...`): `$serve` is a plain program variable, and this compiler diagnoses a `$name` used anywhere in the program but never supplied on the command line, with no built-in notion of an optional/defaulted program variable. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Milestone 7a's formatting/style rules (full list in the new [Style](#12-style-implemented) section): 4-space indentation, K&R braces, `generics` moved first, one blank line between file-level declarations (adjacent `var`s may stay grouped), minimum-necessary parentheses, and inline vs. multi-line list/object/call breaking at 120 columns -- "one true style" always, not "keep whatever the source already did," except that a list/object literal already short enough stays on one line regardless of how it was written. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Lossless formatting is implemented by attaching comment/blank-line `Trivia` to the AST at the specific points the parser already visits one entry at a time (declarations, statements, and list/object/enum/union/type-field/switch-case entries) rather than trying to preserve arbitrary comment placement everywhere; a comment the parser has nowhere to attach it (mid-expression, or with nothing following it before a closing bracket) is caught by the safety check (below) refusing to format that file, rather than silently dropped. the canonical printer (the older, comment-blind AST printer `--final-classes` already used) is unchanged and reused as the safety check's canonical-equality oracle; the new lossless printer lives in the formatter. |
| 2026-09-19 | (proposed by Claude, unconfirmed) The formatter's safety check re-lexes and re-parses its own output and compares the canonical printer's canonical rendering of the reformatted-and-reparsed AST against the same rendering of the original AST (after applying the one sanctioned `generics`-first reordering to both), plus a full, ordered comparison of every comment's trimmed text -- rather than a raw token-stream diff -- since the canonical printer already normalizes away exactly the whitespace/comma-vs-newline differences formatting is allowed to make. |
| 2026-09-19 | (proposed by Claude, unconfirmed) The naming/abbreviation linter (the linter) runs on every discovered class file's AST (the discovery pass's merged, pre-codegen `ClassFile` list) rather than only the entry class, so a lint anywhere in the program (including a `load`-ed root) is always caught; like every other diagnostic this compiler prints, its `file:line:column` is still reported against the entry file's own path (a pre-existing, compiler-wide limitation, not new to this milestone). The terminal-`if ... do` lint (manual.md section 5) moved here unchanged from the generator, since it only ever needed the raw AST. |
| 2026-09-19 | (proposed by Claude, unconfirmed) `spite file.spite`'s automatic formatting pass discovers "every file belonging to the program" with its own light, purely textual scan for `load("name")` call sites (not the discovery pass's real AST-based discovery), so that a parse error in one file never blocks formatting the other, valid files of the same program -- the discovery pass's own discovery still runs immediately afterward and is the one that actually gates compilation. Since `load` only ever accepts a literal string, this heuristic cannot miss a real root. |
| 2026-09-19 | **D1** (decided by Mortaro): reference counting is the default memory model, JavaScript-like -- every non-scalar value is a reference; passing/assigning/storing/returning share the same object; `&` is removed; a copy is explicit (`copy()`/`deep_copy()`). Milestone 9a. |
| 2026-09-19 | **D2** (decided by Mortaro): no `break`/`continue` -- Spite keeps only `while`, permanently (nothing to implement; see section 6). |
| 2026-09-19 | **D3** (decided by Mortaro): `if value do name { } else { }` -- `if ... do` gets an `else`. Milestone 9a. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Object model (milestone 9a): every class instance/`List<T>`/`Dictionary<T>`/`String`/object literal is a heap object whose first field is a `SpiteHeader { ref_count; class_id; }`; a union is a small value-typed `{ tag; pointer }` pair (never itself heap-allocated); `Nullable<T>` of a reference `T` collapses into being the exact same pointer (null = absent), and only a scalar-element `Nullable<T>` keeps the old `{ has_value; value; }` wrapper. `copy()` is shallow (fields/elements retained, not duplicated); `deep_copy()` recurses and does not support cycles. `==` on a class falls back to identity (pointer equality) when no `equals` is defined; `String` always compares by content. `--debug-memory` gained a leaked-object-by-class-name summary, printed once at exit when allocations and frees do not balance. |
| 2026-09-19 | **D4** (decided by Mortaro): the foreign function interface is a class, not a keyword -- `DynamicLibrary("user32.dll", 'windows', "windows.h")`, with Symbol codegen resolving every foreign function, constant and type at compile time. No `external` keyword, no per-symbol binding strings, no hand-written C shim, and no `Pointer` type. Rejected along the way, in order: a file-level `external c "header.h"` declaration with a `binds "SetCursorPos"` string per function (a mapping table by another name); a compile-time `external_name` hook plus `external type` (better, but still a keyword); and tying native libraries to `load(...)` (a library has a handle and a lifetime, so it is an object, not a bundle boundary). Milestone 11; see [Foreign libraries](#17-foreign-libraries-planned). |
| 2026-09-19 | (decided by Mortaro) The class is named `DynamicLibrary` -- not `DLL` (the linter's own abbreviation rule forbids it, and it is wrong on Linux/macOS) and not `Library` (too generic). |
| 2026-09-19 | (decided by Mortaro) No `external` keyword anywhere: which kind of foreign symbol a member access means is derived from the language's own enforced capitalization (PascalCase = type, snake_case = constant, snake_case call = function), and a scalar-only `type` is a C struct implicitly, from the usage that types the literal. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Foreign library details (section 17): `missing_function`/`missing_attribute` as the two reserved hooks (Ruby's `method_missing`, as compile-time Symbol codegen); `symbol_name` as a naming *function* with a `Naming` enum, reusing the linter's abbreviation table backwards to generate Win32 spellings; constant reads being the C spelling lowercased, with no prefix table; Spite laying out a foreign struct itself (so a C union's padding is the author's job) with a generated `_Static_assert(sizeof(C_TYPE) == sizeof(spite_type))`, derived through the naming rule, to verify -- never to generate; `Int` as the default foreign return type with `_as_double`/`_as_text` suffix templates for the rest; constructor arguments restricted to literals; and the import table being exactly the tree-shaken call sites, resolved once in the constructor. |
| 2026-09-19 | (proposed by Claude, unconfirmed) Other environments (section 17): `$target` as a reserved, compiler-provided program variable; platform implementations chosen by section 11 reopening plus section 9 compile-time folding rather than by any abstraction layer; the web bridge being `DynamicLibrary` reopened over a JavaScript module's exports; wasm emitted through a C compiler targeting `wasm32-freestanding` together with a generated JS glue module; `File`/`Directory`/`Process` a compile error under `--target=web` instead of a silent stub. |
| 2026-09-19 | (decided by Mortaro) Singletons are wanted, must reuse the metaprogramming rather than be a special case, and must be invisible at the call site -- `var console = Console()` reads the same, and only the AI writing the code needs to know. The leading candidate, and Mortaro's favourite so far but still undecided, is the constructor declaring it by returning its own class (`func Console(): Console`), keyed by its literal constructor arguments and implemented as constructor monomorphization; `singleton` as a file-level line and `$singleton`/`$instances` are rejected. See open question 7. |
| 2026-09-19 | **D5** (decided by Mortaro): the `generics` header line is removed and **nothing replaces it** -- a codegen value is never declared. Writing `$name` anywhere in a class is what makes it a hole, and the hole is filled from outside, which is already exactly how a flag-fed value like `$serve` works; the header line was the only thing that made call-site values different. At a call site, **one unfilled hole is given positionally and two or more are named**: `List<Int>()`, `Weapon<$damage_type: Magic, $is_magic: true>(10)`, with the name written exactly as it appears in the class, `$` and all. Naming removes the argument order, which was the header line's only job. There are no defaults: every hole must be filled, and a call leaving one unfilled is an error listing every `$` the class uses with the file and line of its first use (which is also what a typo inside a class body produces). A flag fills its hole program-wide, so "one hole" at a call site means one still unfilled. `List<T>`/`Dictionary<T>`/`Nullable<T>` each have one, so they follow the rule rather than being exceptions to it. An intermediate `$name = default` declaration form was considered and rejected in turn: it kept a declaration alive for its own sake, and a codegen value is a hole, not a variable. Milestone 13; see [Codegen values](#9-codegen-values-). |
| 2026-09-19 | (decided by Mortaro) `$singleton = true` is rejected: `$` means "replaced at code generation", and a directive the compiler reads and then deletes is not that -- it would make the sigil mean two things. The leading candidate is now a class-level function, `func is_singleton(): Bool` (Mortaro, choosing among five further proposals), still **not decided**; the constructor returning its own class is the runner-up. See open question 7. |
| 2026-09-19 | **D6** (decided by Mortaro): **Spite has no static class functions and will not get any.** A class is an instance of `Spite.Class`, which is an ordinary standard library class with an ordinary declaration, so there is nothing for `static` to mean -- a function belonging to the class rather than to its instances is simply a function on that `Spite.Class` object. The set of class-level hooks is exactly the set of functions `Spite.Class` declares; a class file defining one of those names overrides it for its own class object (section 11 reopening), anything else is an ordinary instance function, and a collision is a diagnostic naming `Spite.Class`. Overrides are folded at compile time (a literal `return` always folds; `return $codegen_value` folds too, so a class-level fact can differ per build), and `--final-classes` prints each one with the value it folded to. The reasoning is Mortaro's: a configuration file is just a class being reopened, the way Rails patches its internal options through code that runs instead of a static configuration file -- a static line can only state a value, a function can compute one. See [Class-level functions](#class-level-functions-and-why-there-are-no-static-functions-planned). |
| 2026-09-19 | **D7** (decided by Mortaro): `Spite.Class` is reopenable like any other standard library class -- "by all means shoot the foot". Reopening it changes a class-level default (D6) for the whole program: a root returning `true` from `is_singleton()` makes every class a singleton, `List<Int>()` included, and adding a new function to `Spite.Class` creates a new class-level hook name program-wide, turning any same-named instance function elsewhere into an override of it. Allowed because nothing about it is silent: a collision is a compile error naming `Spite.Class`, and a changed default appears per class in `--final-classes` with the root it came from. The compiler has no warnings (section 12), so visible generated output is the whole mitigation. Blocked on section 16 item 1 (reopening standard library classes at all). |
| 2026-09-19 | **D8** (decided by Mortaro): a singleton is declared with the class-level function `func is_singleton(): Bool { return true }`, overriding the default `Spite.Class` declares (D6). Call sites never change (`var console = Console()`); one instance per distinct literal constructor argument list, emitted as one static slot per argument list, so there is no runtime registry walk -- only a guarded branch on first use. The slot holds one reference, `drop()` runs at program exit in reverse creation order, and `--debug-memory` counts singletons as roots rather than leaks. `Console`, `Program` and `DynamicLibrary` are singletons; `File`, `Directory` and `Process` are not. Chosen over a bare `singleton` line, `$singleton`/`$instances`, a constructor return type (`func Console(): Console` / `: Spite.Singleton`), a renamed initialiser (`shared_instance`), a private constructor, and a parenthesis-free call, for the reason Mortaro gave: a configuration file is a class being reopened, the way Rails patches its options through code that runs rather than a static file. Named `is_singleton` rather than `instances` because `instances` is already the live-instance registry. See [Singletons](#singletons-planned). |
| 2026-09-19 | **D9** (decided by Mortaro): **the constructor declares every codegen value a caller supplies, always, even when there is only one** -- `func Weapon<$damage_type, $is_magic>(new_damage: $damage_type)`, called `Weapon<Magic, true>(10)`. Skimming a constructor is how a human sees what a class accepts, and a list of one is still a list. Consequences: call sites are positional with no named form (the constructor fixes the order, so nothing is restated at the call); a `$name` used but *not* declared is a program variable filled by `--name=value` (`$serve`, `$environment`), which is what finally distinguishes a call-site hole from a flag-fed one; a class taking codegen values always has a constructor, even one existing only to declare them; the built-in containers declare theirs the same way (`List<$element_type>`); a wrong-arity call is an error naming the values in order; and reordering a constructor's `<...>` silently changes same-kinded call sites, which tests are accepted as the thing that catches. Supersedes the named call form and the proposed class-level `generics()` hook, neither of which survived. Milestone 13; see [Codegen values](#9-codegen-values-). |
| 2026-09-19 | **D10** (decided by Mortaro): **an enum is a closed list of symbols.** `'knight'` is a symbol (a compile-time name) and an `enum` declares which symbols are accepted where it is expected; the integer is a representation detail. A symbol literal is legal only where something says what it may be -- an enum (it must be a member; `set_weapon('orange')` is an error listing the accepted symbols) or `Symbol` (any name). No widening, and no untyped symbol literal. `Symbol` is the open, compile-time-only form and an enum is the closed, runtime-representable one, so a bare `Symbol` cannot be stored in a field. What a `Symbol` names is still checked by whatever consumes it. The gain: symbol literals make `person.set_attribute('age', 2)` and `Weapon.attributes['damage']` writable in source, which the language cannot express today. See [Enums](#enums-implemented). |
| 2026-09-19 | **D11** (decided by Mortaro): **reflection reads at two levels and the same word is right at both.** A class object is an instance of `Spite.Class` (D6), so `weapon.attributes['damage']` is the value while `Weapon.attributes['damage']` is the `Spite.Attribute` describing the field; a PascalCase receiver is the class, the same convention section 17 uses for `user32.Input`. This also stops `attributes[symbol]` being a special compile-time-only form bolted onto Symbol codegen -- it is ordinary indexing at whichever level the receiver names. The shape of the class-level mapping still needs design (milestone 10). |
| 2026-09-19 | **D12** (decided by Mortaro): **every reflection object lives in the `Spite` namespace** -- `Spite.Class`, `Spite.Function`, `Spite.Argument`, `Spite.Attribute`, and whatever reflection grows next: "super flexible but very clear". `Spite.Function` carries `.name`/`.arguments`/`.returns`, `Spite.Argument` carries `.name`/`.class`. And `.class` everywhere is a real `Spite.Class`, not the type name as a `String` (which is what the compiler has today), because identity comparison is what makes reflection composable -- `function.arguments[0].class == ServerContext` is checked, while `.class.name.starts_with("Server")` quietly turns false when a class is renamed. Printing a `Spite.Class` still prints its `.name`. Milestone 10; see [Reflection objects](#reflection-objects-partial). |
| 2026-09-19 | **D13** (decided by Mortaro): **a function's first parameter decides which bundle it is compiled into** -- `ServerContext` means server-only (the client gets a generated network stub), `ClientContext` means client-only, neither means isomorphic. No annotation and no compiler trick: the framework reads it off the signature with D12 reflection, comparing class identity. The marker is the capability, so it cannot drift -- a function cannot claim to be server-side without holding server capabilities, and the same shape extends to `DatabaseContext`/`FileSystemContext` with no new language feature. A a `union Context` of `ServerContext` and `ClientContext` gives exhaustiveness when a third environment appears. The server function's body is never emitted into the client bundle, so secrets and server-only helpers are removed by the per-bundle tree shaking section 11 already does -- which is what makes the split safe rather than merely convenient. Routes generate from class and function name; a `type` shape is the wire format; parameters and return types crossing the boundary must be serialisable, with a diagnostic naming what is not. Blocked on the errors design (a network call fails in ways a local call cannot). Milestone 12; see [Isomorphic classes](#isomorphic-classes-where-a-function-lives-planned). |
| 2026-09-20 | (proposed by Claude, unconfirmed) Milestone 9c, second batch item 1: `spite folder/file.spite` treats `folder/` as a loaded root -- the entry file's own folder is loaded and every sub folder is a namespace, recursively, exactly as a `load("folder")` would (section 11). Implemented as the discovery pass's `discoverProgram` walking the entry folder as root 0, with `tests/entry_folder_namespace/` and `examples/game` covering it. |
| 2026-09-20 | (proposed by Claude, unconfirmed) Milestone 9c, second batch items 2, 4, 5: `do` is fully removed (`if value do name { }` is a parse error naming the plain-`if` narrowing form; no leftover `do` token kind in bootstrap), shadowing is allowed in an inner scope (same-scope redeclaration stays an error), and the unused rule is enforced with the four dictated-signature exemptions -- operator functions, Symbol codegen templates, setters/getters answering attribute access, and functions that replace another through class reopening -- each now under test (section 5). |
| 2026-09-20 | (proposed by Claude, unconfirmed) Milestone 9c, second batch items 3, 6, 8: `add`/`pop` compile errors name `append`/`prepend`/`remove_last`/`remove_first` (section 15); the compiler prints nothing called a "warning" -- the last one, a `--development`-only skipped-class notice, is now plain informational `note:` text (sections 12/13); and `count_<member>()` counts the true elements of a Bool attribute while a numeric attribute is a compile error naming `sum_<member>()` (sections 8/15). |
| 2026-09-20 | Milestone 9c's implemented second-batch items (1, 2, 3, 4, 5, 6, 8) moved out of section 16 into their proper sections; section 16 keeps item 7 (now numbered 1) plus the third batch. Milestone 10 splits into 10a "reopen standard library classes" (section 16 item 1) and 10b "Ruby grade compile time reflection visible in final classes" (section 16 item 2); see PLAN.md. |
| 2026-09-19 | **D15** (decided by Mortaro): **every standard library template names a `<member>`, and a member is a field or a zero-argument function without distinction** -- `map_age` and `map_get_age` are the same call. Reading an attribute already goes through `get_<attribute>()` (section 5), so the field case is the function case with an optimisation the writer never thinks about; what a template constrains is arity and return type, never whether a member is stored or computed. `filter_`/`count_`/`any_`/`all_` want `Bool`, `sum_` a numeric, `sort_by_` something ordered, `find_by_(value)` something comparable, `map_` anything, `each_` nothing in particular. `each_<function>()` and `map_<attribute>()` stop being different families. Consequence: `todos.map_render()` -- a list of components -- needs no new feature, and a member that does not fit is a compile error naming the member, its return type and the requirement. This makes the uniform access principle, which section 5 already committed to for `person.age`, true for the standard library too. |
| 2026-09-19 | **D16** (decided by Mortaro): **a `type` may require functions, not only attributes** -- a `type Renderable` requiring `render(): Element` -- and a value matches it by shape exactly as an attribute-only `type` already does. This is what lets a collection hold "any class that responds to `render()`" (a children list of components, section 17) without a union listing every class, and it makes the respond-to check section 16 item 9 promises expressible as an ordinary type rather than as reflection. |
| 2026-09-19 | **D17** (decided by Mortaro): **functions are first-class values, and always bound to an instance.** Passing `pretty_print` inside a class passes that function paired with the instance doing the passing, so `console.log(pretty_print)` calls it exactly as the owning instance would have. There are no free functions and no closures: a function value is `{instance, function}`, which is one retain in a reference-counted language and captures nothing else, so no local ever escapes its scope. Consequences: section 17's "Not designed yet" callbacks become solvable, since `{instance, function}` is precisely what a C callback plus its `void*` user data wants; and an event handler can be written `onclick: increment` (signature-checked) rather than as the symbol `'increment'` (D10). **Open:** how a function-valued parameter's type is written -- a one-function `type` (D16), or a signature form such as `func(value: String): String`. |
| 2026-09-19 | **D18** (decided by Mortaro): **Spite has no JSX and no trailing-block syntax; markup is ordinary metaprogramming.** A tag comes from `missing_function` (D4), attributes and children are ordinary literals, components are matched by shape (D16), handlers are bound functions (D17), a list of children is `map_<member>()` (D15), and a conditional child is a `when(condition, element)` call returning `Nullable<Element>` that the framework skips. **Children are variadic at no cost**, because a Symbol-codegen template monomorphizes per call site -- `html.div({ class: "card" }, html.h1(title), TodoForm({ ondone: add_todo }))` needs no varargs feature and no list brackets. Rejected: a JSX-like literal (a second grammar, in a language whose philosophy is a very small feature set; it also fixes neither the conditional nor the mapping problem, which are what actually make markup awkward); and a Ruby-style trailing block `f(x) { a b c }` == `f(x, [a b c])` -- it serves exactly one shape, since a query is `database.filter_age_greater_than(10).sort_by_name()` and routes are plain statements, so it would be a sixth meaning for `{ }` bought for a single case. The justification for accepting the verbosity is the language's first philosophy line: this is annoying for a human to *write* and cheap for an AI to write, while a human can still easily *read* it -- and the verbosity buys compile-time checking of tags, attributes and handler names that JSX cannot do, plus static subtrees folding into constant instruction buffers. |
| 2026-09-19 | **D19** (decided by Mortaro): **the DOM is bridged exactly like a native library.** `Html` is the web counterpart of `DynamicLibrary` (D4): a singleton (D8) whose `missing_function`/`missing_attribute` hooks resolve names against the browser instead of against a `.dll`, with `Html.Node` wrapping one node handle (an index into the JavaScript-side table, freed by `drop()`). The naming rule is `'camel_case'`, which the `Naming` enum already has, because the DOM is camelCase: `document.create_element("div")` is `createElement`, `node.text_content = "Hello"` is the `textContent` property, `root.append_child(node)` is `appendChild`. A call is a method and a read or write is a property, the same split D4 uses for functions versus constants. So there is one foreign-bridge pattern with two backends, and `Html` is the low-level layer that D18's markup builder compiles down to, exactly as `Mouse` sits on `DynamicLibrary`. **Open:** D18 writes the markup builder as `html.div(...)`, which collides with this name -- the builder probably needs a different one (`Markup`, `View`), since `div` is an element to create and not a DOM method. |
| 2026-09-19 | **D20** (decided by Mortaro): **a class declares which targets it compiles for, and using it anywhere else is a compile error.** A class-level function (D6) -- `func targets(): List<Symbol> { return ['native'] }` -- defaulting to every target, so `DynamicLibrary` returns `['native']` and `Html` returns `['web']` and each crashes the build against the wrong `$target`, naming the class, the target and the flag. This replaces section 17's ad-hoc rule that `File`/`Directory`/`Process` are an error under `--target=web`: they declare `targets()` like anything else, and the special case disappears. The check runs after compile-time folding and tree shaking, so `if $target == 'web' { load("platform_web") } else { load("platform_native") }` is not an error -- only a class still reachable in the built program is checked. Uses D10 symbol literals and D6's hook mechanism, so it adds nothing new to the language. |
| 2026-09-19 | **D21** (decided by Mortaro): **the markup builder and the browser bridge are two different classes**, which resolves D19's open item. One produces element trees (`var html = HypertextLanguage()`, so `html.div(...)` is an element to create) and the other is the DOM itself (`BrowserLibrary`, where a call is a DOM method and a read or write is a property, D19). Names are not final; the concept is. The builder compiles down to the bridge, exactly as `Mouse` sits on `DynamicLibrary`. |
| 2026-09-19 | **D22** (decided by Mortaro): **JSON is a use of reflection, not a library.** Writing is a string generated by walking a class's attributes (D11/D12) and is emitted per class as visible generated source (section 16 item 9), only for classes that use it; reading fills a class through symbol-keyed writes (`set_attribute('name', value)`, D10 plus Symbol codegen), and a `type` shape is the natural JSON shape, which is what section 7 said inline types were for. It is the same machinery D13's wire format needs, so the two share one implementation. **Refinement (proposed by Claude):** `to_json()` needs no failing variant at all -- the compiler already knows every attribute's type, so an unserialisable class is a compile error (the check D13 already requires), which makes writing total. Only *parsing* can fail at runtime, on malformed input, so the pair Mortaro described applies there alone: a soft form returning `Nullable<T>` that `assert` narrows, and a loud form that crashes with position and reason for when an AI wants the report instead of a null. **Open:** the naming (`to_crashing_json` was Mortaro's sketch, "or something like that"); Claude prefers a suffix, `parse_json()` / `parse_json_or_crash()`. |
| 2026-09-19 | **D23** (decided by Mortaro): **after the bootstrap, the standard library is written *with* the metaprogramming, not merely alongside it.** JSON is the worked example: reflection generates it rather than a hand-written serialiser per type. This is the other half of D14 -- moving the standard library into Spite is not a transcription of the C, it is a rewrite that uses `Spite.Class`/`Spite.Attribute` (D12), symbol literals (D10), member templates (D15) and class-level functions (D6) as its ordinary tools. |
| 2026-09-19 | **D24** (decided by Mortaro): **Spite has three outcomes and no others -- a compile error, an `assert`, or a crash.** There are no exceptions, no error unions, no bubbling and no error values carrying a message, because "errors and exceptions tend to be useless: they tell us a message we have no action to take about them". (1) **Compile error** for anything the compiler can know -- already six decisions deep: unserialisable types (D13/D22), the wrong `$target` (D20), an unfilled codegen hole (D9), a missing foreign symbol (D4), a member that does not fit a template (D15), a symbol outside its enum (D10). (2) **`assert`** is for when the developer wants the program to keep running: it returns the default value of the return type, which is a deliberate substitution rather than a check that fires. (3) **A crash** is for when everything should halt so the code gets rewritten. **`Nullable<T>` is the only runtime failure value and it carries no reason** -- the caller either narrows it with `assert` and continues, or handles the absent case with `if ... do`. **The rule that makes this survive real code (proposed by Claude): if a distinction is actionable, it is data, not an error** -- a caller that must tell a timeout from a rejection takes back a `type`/`union`/enum modelling that, which is ordinary data with ordinary handling, not an error channel in disguise. Consequences: D13's failed network call returns `Nullable<T>` and does not crash, since the program is not wrong; D22's parse pair is exactly rule 2 versus rule 3; and the FFI keeps crashing on a missing library or symbol (D4). **Open:** how a deliberate crash is spelled, and the requirement that its report name the Spite file, line and call chain -- a crash is only useful to an AI if it does, which is a real demand on the C backend. |
| 2026-09-19 | **D25** (decided by Mortaro): **a crash always reports backend information and the trace of every `assert` that failed before it.** This answers the one reservation Claude raised against D24 -- that `assert` substituting a default is a silent failure, the same swallowed-null the model exists to prevent, and the one an AI can never learn about -- without changing what `assert` does. The developer still chooses to keep running; the choice simply stops being invisible. A crash report carries: the message, the Spite file, line and function, the call chain, the generated-C location, and the preceding failed asserts, each with its source condition, the default it substituted and where it fired. Those asserts are exactly the causal trail explaining how the program reached the state that crashed. **Cost is only on the failure path** -- a passing `assert` already branches, so nothing is added to the normal path, and the trace is a fixed-size ring buffer plus a total count, so it allocates nothing and cannot grow without bound. The same applies to compile-time evaluation (D6 overrides, D22 generation, milestone 10's evaluator): a failure there reports the compile-time asserts that fired, which is where an AI in an edit-compile loop actually reads it. **Requirement on the backend:** Spite file and line must survive into the generated C -- `#line` directives are the ordinary way, and they also make the C compiler's own errors point back at Spite source, which the project wants regardless. **Open:** the ring buffer size, and whether a `--development` build records passing asserts too. |
| 2026-09-19 | **D26** (decided by Mortaro): **`assert` is control flow, not validation** -- a guard clause meaning "this value is absent, so there is no more logic to do here, but the program is fine and keeps serving everything else". It is for cases known to be nullable where absence is not a problem, which is why a web server or a game built in Spite should rarely crash. **Two refinements (proposed by Claude, unconfirmed):** (1) `assert` is sound exactly when the return type can express "nothing" -- `Nullable<T>`, an empty `List<T>`, an empty `String`. A bare scalar return cannot: `func count_user_orders(): Int` guarded by `assert find_user(...)` returns `0` both for "no such user" and for "a user with zero orders", so absence and answer become indistinguishable at the caller. Proposed as a lint: `assert` in a function returning a bare scalar is an error, escaped by returning `Nullable<T>` or handling it with `if ... do` -- noting the language has no warnings, so it is error or silence, and sometimes `0` genuinely is the answer. (2) D25's crash trace should record **predicate asserts only, not narrowing ones**. If narrowing is routine control flow then a busy server fires thousands of them, and a ring buffer full of ordinary "not found" entries buries the signal; a predicate assert firing means an assumption was wrong, which is exactly what a crash report wants. |
| 2026-09-19 | **D27** (decided by Mortaro): **`assert` is legal only in a function that returns nothing, or returns `Nullable<T>`** -- the two cases where the substituted default is honest, since void has nothing to say and `null` says exactly "absent". Anything else is a compile error naming the return type. This supersedes D26 refinement (1), which only caught bare scalars: an empty `List<T>` or an empty `String` has the same defect as `0`, because the caller cannot tell "no user" from "a user with no orders". A function that wants a guard and returns something else must choose deliberately -- widen the return to `Nullable<T>`, or handle absence with `if ... do` and return the value it actually means. **Edge (proposed by Claude, unconfirmed): a constructor is banned too.** It has no return type, so the rule as stated admits it, but "returns nothing" is false there -- the caller receives an object with every field at its default and cannot distinguish it from a properly built one, which is exactly the surprise this rule exists to prevent. The diagnostic should name the two alternatives: check before constructing, or move the lookup into a function returning `Nullable<T>`. **Second-order effect (Mortaro): the rule forces smaller functions.** A function that deals with a reference and wants a guard cannot keep everything in one body -- it has to split, so the lookup lives in a small function whose signature honestly says `Nullable<T>` and the caller handles absence with `if ... do`. Decomposition then comes from the type system rather than from style advice, which is what works on an AI: there is no "should" to ignore. Fragmentation is cheap here (the extra functions are private, tree-shaken and visible in `--final-classes`), and smaller honestly-typed functions also make D13's signature-driven bundle split cleaner. |
| 2026-09-19 | **D28** (decided by Mortaro): **`assert` is banned in a constructor.** D27 admits it only because a constructor has no return type, but "returns nothing" is false there -- the caller receives an object with every field at its default and cannot tell it from a properly built one. Mortaro's reasoning is the general form: **a constructor is setup, not logic** -- if something can be invalid inside one, function logic has been put where only setup belongs. The push this creates is a good one: a constructor takes already-resolved values rather than identifiers to resolve, so the caller does the lookup (`var user = find_user(id)`, then `if user do found { Profile(found) }`) and the constructor just sets fields. **Two things this surfaces.** (1) Section 17's `DynamicLibrary` constructor ends with `assert _handle`, which is now illegal -- and it was wrong anyway: a missing library is not "absence is fine, stop here" but "halt to fix this", so it is a crash (D24 rule 3), which is what section 17's own prose already says. The `Html`/`BrowserLibrary` constructor (D19) has the same fix. (2) **The entry constructor is exempt (proposed by Claude, unconfirmed)**: the 2026-09-18 decision makes the entry file's constructor the program itself, so it is `main` wearing a constructor's name, and a program body is logic by definition. Applying "setup only" to it would contradict that decision or force a ceremonial one-line delegation. **Open (proposed by Claude):** whether the rationale generalises past `assert` into "a constructor performs no fallible work at all" -- no file reads, no lookups, no network -- which is the same principle stated as a rule rather than as a consequence.
| 2026-09-19 | **D29** (decided by Mortaro): **the entry constructor is not exempt from D28** -- Claude's proposed exemption is rejected. A program constructor that grows complex is split into smaller functions that the constructor just calls, which is the same decomposition D27 already forces everywhere else, so what looked like ceremonial delegation is simply the correct shape. D28 therefore stays one uniform rule with no special case in the compiler: no constructor asserts, the entry class included. A side benefit for the human-skimming goal (philosophy line 1): the entry constructor ends up reading as a table of contents for the program -- the sequence of things it does -- with the logic one level down in functions that may guard freely, since they return nothing or `Nullable<T>`. |
| 2026-09-19 | **D30** (decided by Mortaro): **`crash` is a keyword, not a standard library call**, so the compiler can take context from the backend rather than from a message string. **Form (proposed by Claude): it mirrors `assert` exactly** -- same polarity, same shape, different severity. `assert database.connect()` returns the default on falsey and keeps going; `crash database.connect()` halts on falsey. The compiler captures the condition's source text and every operand value, so the report needs no written message and nothing can drift out of sync; bare `crash` stays legal for an unreachable branch and reports its enclosing context. Mortaro also sketched `crash not database.connect()`; Claude argues against that polarity, since `assert` firing on falsey and `crash` firing on truthy makes two statements that look alike behave oppositely. **What makes it more than sugar: `crash user` narrows a `Nullable<T>` like `assert user` does but never returns, which fills the hole D27 opens.** A function returning `Int` that needs a guard previously had to widen to `Nullable<Int>` or use `if ... do`; now absence-is-a-bug has its own honest form. The three cases cover the space: absence is fine -> `assert`, absence is a bug -> `crash`, absence is meaningful -> `if ... do`. |
| 2026-09-19 | **D31** (decided by Mortaro): **JSON is not the wire format.** "I do not really need JSON in 99% of the cases, the main consumer of the language is AI... it is a web convention that creates inefficiency for human readability, but a human will never read those." D13 sends a compile-time-derived binary packing instead: raw fields in declaration order, no keys and no parsing, because **JSON's entire cost is self-description and there are no unknown consumers** -- the client and server bundles compile from the same source, so both ends already know the exact `type` layout. This compounds with the wasm decision, since JSON would force string marshalling across the JavaScript boundary on every request, which is the expensive part of wasm interop; with it gone, the DOM command buffer and the RPC channel are both opaque byte buffers. D22 is amended: JSON stays in the standard library as a format for foreign systems that demand one, not as the default. **Two requirements (proposed by Claude):** a schema hash in the handshake, crashed on mismatch, since an old client against a new server is the one case that can skew; and `--development` decoding a payload to JSON on demand (the compiler knows the schema), so readability is available exactly when a human is looking and never paid for otherwise. |
| 2026-09-19 | **D32** (decided by Mortaro): **crash sites are identified by a compile-time id, and the compiler emits an id-to-source map as a build artifact** rather than embedding the information in the program. A crash prints its id and the runtime values; the source location, the condition text and the operand names live in the sidecar, so the binary carries none of it. Two payoffs: the program shrinks (condition text for every `crash`/`assert` site is kilobytes that matter most in a wasm module, where size is startup time), and **the same logical site carries the same id across every `$target`, so a crash from the server bundle and one from the browser bundle can be compared directly** -- which is exactly what D13's isomorphic classes need, since the same class is compiled into both. It is D31's principle applied again: the static half is known at compile time, so it never travels. **Requirements (proposed by Claude):** (1) **the id must be content-derived, not a counter** -- a hash of namespace, class, function, the per-function ordinal of the site and the condition source. A counter renumbers everything when a line is added, which breaks both old reports and cross-target identity, which is the whole point. (2) The map is an artifact to archive, like a PDB, carrying a build hash so a report can be matched to the build that produced it -- pairing with D31's schema hash. (3) `--development` embeds the text directly, since a local run should not need a lookup; only optimised builds emit bare ids. (4) D25's assert trace becomes a ring buffer of `(id, values)`, which is far cheaper than holding strings. (5) D25's `#line` requirement narrows to compile-time diagnostics only -- the runtime path no longer needs it. **Format (Claude's preference): a greppable line-based text table, one line per id**, so `grep 4172 program.crashes` answers the question without a parser. |
| 2026-09-19 | (decided by Mortaro, confirming D30) **`crash` as a keyword is accepted, in the mirror-of-`assert` form** Claude proposed: same polarity, `crash <condition>` halts on falsey, bare `crash` for an unreachable branch, and `crash user` narrows a `Nullable<T>` without returning. `crash not database.connect()` is dropped. `crash` joins the keyword list in section 2. |
| 2026-09-19 | **D33** (decided by Mortaro, adopting Claude's preference): **the D32 crash map is a greppable, line-based, tab-separated text table**, written next to the output as `<output-name>.crashes`, so `grep 4172 program.crashes` answers the question with no parser and no tooling. One line per site, **sorted by id** so archived maps diff cleanly between releases, with fields: id (content-derived, hex), file, line, column, class and function, kind (`crash`, `assert-predicate`, `assert-narrowing`), the condition's source text, and the operand names with their types. Tabs are safe as the separator because a condition is a single-line expression and the formatter forbids tab indentation. The file opens with a `#` comment line -- Spite's own comment character -- carrying the format version and the build hash that D32 requires for matching a report to its build. **Runtime output:** one tab-separated line per event, prefixed `spite.crash` or `spite.assert`, so a crash is greppable out of mixed program output. **Values are rendered as text at crash time but stored raw in the assert ring buffer** -- a crash happens once and then the program is dead, so it should be informative rather than fast, while a narrowing assert may fire thousands of times an hour (D26) and must stay cheap until something actually dumps it. |
| 2026-09-20 | **D34** (decided by Mortaro): **a comment is one line, and it is nothing but a link to a markdown section.** `# bootstrap/BOOTSTRAP_PLAN.md#ownership-aliasing` -- a path to a markdown file in the repository plus a GitHub-format anchor, and no other text. The compiler validates that the file and the anchor both resolve. Anything else is an error, including an attempt to escape into `//` or `/* */`, which the lexer recognises specifically in order to reject them with the explanation rather than a parse failure. **The error message teaches rather than refuses** (Mortaro): it states the one legal form and then asks whether the note is necessary at all -- if a reader could derive it from the code, delete it; if it is durable knowledge, write the section first and link to it; if it warns against a change, a test or a compiler diagnostic pushes harder than prose. **Why this shape.** Prose in source rots into a lie, while a link either resolves or fails the build. Re-derivable notes should not exist, because an AI reconstructs them from the code for free. And zero comments was rejected on token economics: a grep costs tokens on every edit to discover *absence*, which is the common case, while a link costs about ten tokens only where there is something to say -- absence becomes free and trustworthy. **A comment may only appear outside function scope** (decided by Mortaro): a line that seems to need explaining becomes an extracted, named function instead, which D27 already forces elsewhere -- and unlike a comment, the name is visible to `Class.functions`, to `--final-classes` and to every tool. **Migration is real: 405 comment lines across 59 `.spite` files, plus every fenced sample in `docs/`, which `check.sh` compiles.** |
| 2026-09-19 | **D14** (decided by Mortaro): **pure Spite is the target -- the hand-written C runtime is a bootstrapping stage, not the design.** `src/runtime/spite_runtime.h` (722 lines) and `src/runtime/spite_repl.h` (1392 lines) exist because the compiler needed a `String` before Spite could express one; D6 already makes the current arrangement untenable, since `Spite.Class` cannot be "an ordinary standard library class with an ordinary declaration" while the standard library is C. Destination: `String`/`List<T>`/`Dictionary<T>` and the reflection tables become ordinary `.spite` sources, `File`/`Directory`/`Process` become `DynamicLibrary` calls (D4), and the floor is five to ten compiler intrinsics for raw memory rather than a runtime file -- because a `String` needs allocation, allocation needs pages from the operating system, and asking for them through the FFI needs a `String`. Purity costs no performance: the output is still C, so a Spite-written `trim()` meets the same optimiser. It also makes standard library classes reopenable (section 16 item 7) and REPL-inspectable by removing their special status. The web shim is the one exception by construction -- it runs in the JavaScript virtual machine -- but its target is zero hand-written lines, generated from `external js` declarations and an opcode table, so no second language lives in the repository. Milestone 15; see [Pure Spite](#pure-spite-dissolving-the-runtime-planned). |
| 2026-09-20 | **D35** (decided by Mortaro): **the concurrency model ships in the language, singular, and there is no bring-your-own-runtime.** Rust's split is the anti-goal: `async fn` desugars to a state machine implementing `Future`, the language shipped the trait but deliberately no executor, and three incompatible runtimes filled the hole -- after which every library had to declare which one it targeted. The mechanism to avoid is precise: **never define a protocol that requires an external driver.** Two constraints Mortaro set agree with each other here -- the only model simple enough to ship *as* the one true implementation without hiding costs is a thread pool plus fork-join, which is exactly why a work-stealing green-thread executor cannot be blessed and why Rust did not try. **Mechanism (proposed by Claude, unconfirmed):** (1) **No function coloring, ever -- concurrency is a property of the call site, not of the function.** `file.read` stays an ordinary function; a caller that wants it concurrent writes `var reading = Task(file.read)` and later `reading.wait()`, which works because D17 already made a bound function a first-class value. No `await`, no wrapper return type, nothing viral. (2) **Join on drop:** a `Task` handle is refcounted, so `drop()` joins and scope exit is a join point -- structured concurrency falls out of D1 with no new machinery. (3) **Data parallelism is a template family** (D15): `entities.parallel_each_update()` monomorphizes into a fork-join over `List<T>`'s contiguous buffer, no per-item allocation and no scheduler. (4) **Race safety by a syntactic rule, not a borrow checker:** a parallel member may only reach its own instance's state and its locals, which is checkable by walking the body because Spite has no globals and no pointers; reaching through a field holding another class is an error naming the field. (5) **Native only** (D20): wasm cannot block, so `wait()` has no implementation there and web concurrency is D13's request/response instead. **Open:** shared mutable state across threads (a channel or an owning mutex -- ECS mostly avoids needing it), and cross-entity reads in a parallel pass, which rule (4) does not cover. |
| 2026-09-20 | **D36** (decided by Mortaro): **hidden costs are bad, hidden optimisations are good** -- the rule is not symmetric. Code that runs faster than a reader expects is a pure win and needs no announcement; code that runs slower than a reader expects is the only real surprise. So the compiler may freely make a program faster behind the source (copy elision into views, deferring an independent read onto a task, folding a static markup subtree) without asking or marking it. **Consequence for the build report:** it lists the *failures*, not the successes -- not "400 copies elided" but "3 copies could not be elided, and the callee that writes the field". Every line is then actionable, and the report is small enough to read. |
| 2026-09-20 | **D37** (decided by Mortaro): **the compiler injects the REPL drain points; the user never writes one.** A user will forget, and a forgotten drain means a dead REPL discovered much later. The injection rule is principled rather than game-specific: **a drain goes wherever the program is already waiting** -- `Program().sleep(...)`, a blocking IO call, a `Task` join (D35), and the join at the end of a parallel pass. Those are exactly the points where state is consistent and no parallel pass is writing, so a command sees a whole world rather than a half-stepped one -- correctness, not only latency. Mechanism: the REPL thread does the blocking socket read and pushes parsed commands into a lock-free queue, so an injected drain is one uncontended atomic load (about a nanosecond) when nothing is pending, rather than a syscall. Cheap enough to leave in a shipping build. A game frame ends in a sleep or a present, so a frame loop is covered without the engine doing anything; a server is covered by its IO waits. **If a REPL build has no injection point reachable from the entry constructor, that is a compile error** naming the problem -- a program that never waits anywhere would have a REPL that never answers, and the author should learn that at build time. The same injected points serve live reload (milestone 6b) and anything else that needs an idle moment. |
| 2026-09-20 | (decided by Mortaro, extending D37) **`Console.read_line()` is a REPL drain point too.** A REPL, and any interactive tool, spends nearly all its time waiting on console input, so a blocking read from stdin is the same kind of already-waiting moment as `sleep`, a file read, a `Task` join or a parallel-pass barrier. It is also the one that covers interactive programs, which none of the others do -- a tool that only ever waits on a person would otherwise have no injection point at all and would fail D37's compile-time check. |
| 2026-09-20 | **D38** (decided by Mortaro): **a dependency is a git URL pinned to a commit hash, written in the `load` call, and the ordinary compile fetches it.** No package manager, no registry, no lockfile, no separate `spite fetch` step: `load("github.com/mortaro/engine@a3f2c91")` is the whole mechanism, and compiling a program that names a dependency it does not have fetches it then. **The commit hash is mandatory** -- the compiler rejects a git dependency without one, because a moving reference is how a dependency changes under you (`event-stream`, `colors.js`), and a pinned hash makes it immutable. The pin lives in the source, which is already versioned, so **the lockfile problem disappears rather than being solved**: there is no manifest to drift from. The spite being answered is twofold -- C++'s thousand ways to set up a project, and the way "every day a new ecosystem arises to compete with npm". Git is the registry, so there is no central index for a competitor to fork from, which is D35's structural argument applied to packaging. Two properties come free from Spite's existing rules: a package is `.spite` source and nothing else, so **there are no install scripts** -- npm's `postinstall`, the single biggest supply-chain vector, has nowhere to exist; and `load` already takes a literal, so every dependency in a program is known at compile time and can be reported. **A version mismatch is a compile error that calls the user's attention** (decided by Mortaro): two pins of the same package would otherwise silently monkey patch each other through section 11's reopening rule, replacing one version's functions with another's -- the worst possible outcome, so the compiler names both pins and the human picks. **Open (proposed by Claude):** whether fetched source is committed into the repository (auditable, offline, reproducible from a clone) or kept in a cache such as `.spite-cache/` (smaller repository, needs the network on a fresh clone). Note that fetch-on-compile means the first build of a program is not hermetic and needs git and a network. |
| 2026-09-20 | **D39** (decided by Mortaro): **a function value is an instance of `Spite.Function`, generic over its arguments and its return** -- `Spite.Function<String, Bool>` -- so a function-valued parameter is typed the way any other generic type is, which answers the question D17 left open. The codegen values are positional and the **last one is the return**; everything before it is an argument, in order, so `Spite.Function<Bool>` takes nothing and `Spite.Function<String, Int, Bool>` takes two. They line up with `Spite.Function`'s own reflection members (D12): the arguments are `.arguments`, the last is `.returns`. This is the language's first variadic generic -- every other takes a fixed count declared on its constructor (D9), while `Spite.Function` is a built-in taking as many as the signature has. **The value and its reflection are the same class**, which is the point: `Weapon.functions[0]` and a `pretty_print` passed as an argument are both `Spite.Function` instances, so reflection stops being a mirror held alongside the language and becomes the language describing itself -- what "everything is a class, including classes" has claimed since section 8 was written. A function value carries `.name`/`.arguments`/`.returns` because it *is* the reflection object, and D17's `{instance, function}` pair is simply what that instance holds. **Open (proposed by Claude):** how a function returning nothing is written, which is now REQUIRED rather than optional: with the return last, `Spite.Function<String>` reads as "takes nothing, returns a String", so a function taking a `String` and returning nothing cannot be written without a class meaning "returns nothing" -- and that class is also what `.returns` reports for every such function; and whether calling a function value goes through an operator (`printer(value)` as `call`, in section 5's operator table) or a named member. |
| 2026-09-20 | **D40** (decided by Mortaro): **a `Spite.Function` knows its owner, and every call of a function value goes through `call_function()`.** `.owner` is the instance the function is bound to, so D17's `{instance, function}` pair stops being a hidden representation and becomes an ordinary readable field -- `pretty_print.owner` is the `Console` that will run it, and bound-ness is reflectable like everything else. `printer(value)` is `printer.call_function(value)`, which answers D39's open question by applying section 5's operator rule once more: the call operator maps to a named function exactly as `+` maps to `sum` and `a[x]` maps to `get_at`. The value of one chokepoint is that every invocation of a function value passes through it -- an event handler firing, a foreign callback arriving from C (section 17), a framework dispatching -- so there is one place to instrument and one place where the owner is applied, and because `Spite.Function` is an ordinary standard library class, `call_function` can be reopened (D7) to trace every callback in a program. The indirection is paid only where it was already accepted: an ordinary call such as `person.set_age(2)` is direct and never builds a `Spite.Function`; only a function used *as a value* routes through `call_function()`. |
| 2026-09-20 | **D41** (decided by Mortaro): **a namespace is an instance of `Spite.Namespace`**, for the same reason D12 made `.class` a real class: a string can only be matched, an object can be compared, walked and enumerated. `Spite.Class.namespace` is one of these rather than a dotted `String`, and printing it prints the dotted name the way printing a `Spite.Class` prints its `.name`. It mirrors what section 11 already builds -- namespaces come from folders, so `Spite.Namespace` is that tree made readable, with `.parent` walking up and `.classes`/`.namespaces` walking down. The consequence worth having: a `load(...)` root becomes something a program can enumerate, which is what D13's isomorphic split needs when it partitions a package's functions by their first parameter, and what the deferred compile-time class generation (Mortaro, 2026-09-19) would extend. **Proposed by Claude, unconfirmed:** the global namespace is a real `Spite.Namespace` with an empty `.name` rather than `null`, so `.namespace` never needs narrowing and `.parent` terminates at a root instead of at an absence; and its `.classes` reflects section 11's merged result, after every root that reopened into it has been applied -- the same thing `--final-classes` prints. |
| 2026-09-20 | **D42** (decided by Mortaro): **reflection may be as detailed as it likes, because what is not used is not emitted.** Most of the metadata will never be reached by a given program, and reaching for it is the only thing that makes it cost anything -- "if it has a cost it is because we needed it anyway". This holds here in a way it does not elsewhere: Spite's reflection is resolved at compile time, so which parts a program touches is **statically decidable** and tree shaking is exact rather than conservative. A language with runtime reflection cannot prove a class is never reflected on and must keep the tables for everything; Spite can prove it. Section 8 already worked this way for `value.attributes`; D42 extends the rule to the whole family (`Spite.Class`, `Spite.Function`, `Spite.Argument`, `Spite.Attribute`, `Spite.Namespace`, and whatever follows) and gives milestone 10 its direction: **err toward more detail**, since an unused field costs a program nothing while a missing one costs a design. It is also what keeps D32's wasm size argument intact -- a rich reflection family does not inflate a module that never reads it. |
| 2026-09-20 | (decided by Mortaro, amending D41) **`Spite.Class.namespace` and `Spite.Namespace.parent` are `Nullable<Spite.Namespace>`, and the chain ends at `null`** -- rejecting Claude's proposal of a root namespace object. A root object would have read as though every namespace were nested inside something, and inside the metaprogramming package in particular: a namespace *uses* `Spite.Namespace`, it is not contained by it. A top-level namespace has no parent, so you walk up with `class.namespace.namespace` for as long as that can be satisfied. |
| 2026-09-20 | **D43** (decided by Mortaro): **`assert` narrows every link of a chain, not only the last.** Walking a nullable structure would otherwise need one `assert` per hop, and "a thousand asserts" is a tax rather than a language -- the need became obvious the moment D41 made namespaces a nullable chain. `assert class.namespace.namespace` proves the whole path, so every prefix of it is a plain value for the rest of that block and any nested block. It is the rule section 5 already had for `assert value and other_condition`, applied along a member chain instead of across an `and`. It narrows the asserted path and its prefixes only -- a sibling path stays `Nullable<T>` -- and `crash` narrows a chain identically, since it narrows exactly as `assert` does but never returns. Reading through a link that has not been narrowed is still an error: this removes the verbosity of proving a path, not the requirement to prove it. |
| 2026-09-20 | **D44** (decided by Mortaro): **Spite emits C and takes no dependency on LLVM; the C compiler is the user's to bring.** "Each person brings its own C compiler." The language does not emit LLVM IR and is not bound to it as a backend -- which is a different statement from refusing to *use* clang, since a user may well bring it (this machine's own compiler is the clang inside Visual Studio). What is refused is binding the language to one backend. The reason is philosophy line 5, compilation speed and live reload: emitting C keeps the final step swappable, so a fast compiler can be used while iterating and an optimising one for release, and emitting LLVM IR directly would trade that away for LLVM's codegen speed, which is the slow half of every toolchain built on it. It is also what makes the bootstrap honest -- the committed seed is C, so any machine with a ubiquitous tool can rebuild the compiler from source rather than trusting a prebuilt binary. `check.sh` finds `cc`, `clang` or `gcc`, looks inside Visual Studio on Windows, and otherwise takes `CC`. |
| 2026-09-20 | **D45** (decided by Mortaro): **`Monster?` is sugar for `union { Monster, Null }`, and `Nullable<T>` is gone.** `Null` is an ordinary class whose only value is the literal `null`, as `true` and `false` are the values of `Bool`. The point is not the spelling but the semantics Mortaro wanted underneath it: nullability stops being a compiler special case and becomes a union like any other, so `switch` covers it exhaustively, `assert`/`crash`/`if ... do` narrow it through union narrowing that already exists, and the `Nullable<T>` exceptions collapse into rules the language already had. Section 10's representation survives as a codegen detail -- a union of a reference and `Null` is still just the pointer, and only a scalar needs a wrapper. Rejected on the way: `Nullable<T>` (a generic wrapper is what keeps it special), `Monster or Null` (`or` short-circuits everywhere else, so a return type reads as an expression yielding the truthy side), `maybe Monster` (a keyword bought for a shortcut), `Maybe<Monster>` as a standard library generic union (generics over unions hide the members and collide with `$` codegen replacement), and `Monster.Maybe` as a class-level member. |
| 2026-09-20 | **D46** (decided by Mortaro): **a test is a package that crashes, not a framework.** There is no `expect`, no matcher vocabulary, no reporter and no summary -- those exist to make a failure pleasant for a human reading a terminal, and "my ass that I am running them myself". A test is ordinary Spite shipped alongside the project as a package (`load(...)`), and an expectation is `crash <condition>` (D30), which already halts on falsey and reports the condition's source text with every operand value (D25, D32, D33). **The testing story therefore needs no new language feature**: crash, load and reflection are the whole of it, and `Class.functions` (D12) is enough to discover every `test_`-prefixed member without registration. **Fail-fast is deliberate and is the opposite of what a human suite wants**: a person wants all thirty failures so the work can be batched, while an AI wants one precise failure to fix before re-running, since a list of mostly-cascading failures invites shotgun fixes. The crash report is the test output. |
| 2026-09-20 | **D47** (decided by Mortaro): **`enum`, `type` and `union` declarations drop the `=` and must break lines.** `enum Job {` ... `}`, one entry per line, no commas. The `=` was only ever there so a single-line form would not look odd, and that single-line form is what creates the need for commas -- removing both removes a choice. `=` now means assignment and nothing else. The reasoning is the language's own: it reads better multi-line, and "having less options make ai less fucked up" -- a rule with one form cannot be got wrong. Writing `enum Job {` is a parse error naming the fix, the way `for`, `&`, `Heap<T>` and `generics` were retired, rather than something the formatter quietly rewrites: it is syntax, not style. **Scope:** declarations only. A list or object *literal* keeps both forms (`[1, 2, 3]` inline, or one entry per line), because a literal is data at a use site while a declaration defines a type and is read far more often than it is written. Migration is 22 declarations across `.spite` files -- none of them single-line already -- plus 13 in `manual.md` and `docs/`. |
| 2026-09-20 | **D48** (decided by Mortaro): **`Nothing` is the class a function returns when it returns nothing**, which D39 left open and which turned out to be required rather than optional: with the return in the last generic position, `Spite.Function<String>` already reads as "takes nothing, returns a `String`", so a function taking a `String` and returning nothing could not otherwise be written. `Spite.Function<Nothing>` takes nothing and returns nothing. It is also what `.returns` reports for every function declared without a return type, so it is not notation invented for the type syntax -- it fills a hole the reflection already had. Chosen over `None` because it says "no value" rather than reading like an empty collection. This unblocks milestone 16d, the zero-argument subset of `Spite.Function` that lets the test entry discover its `test_` functions instead of hand-listing them. |
| 2026-09-20 | **D49** (decided by Mortaro): **`Spite.Class.instances` is every class in the program.** A class is an instance of `Spite.Class` (D6), so the registry of its instances is the whole program's classes -- whole-program enumeration falls out of "everything is a class, including classes" rather than needing a mechanism. A test runner finds every test class with it instead of being handed them, so there is no registration, no manifest and no `run(...)` call per class. D42 pays for it: a program that never enumerates its classes never generates the registry. |
| 2026-09-20 | **D50** (decided by Mortaro): **a class with no declared constructor has an implicit one**, taking no arguments and doing nothing, so `func DictionaryTests() { }` never needs writing. Every field already has a default value (section 4), so there is nothing for such a constructor to do. This already worked: a class with no constructor function is constructed with its field defaults. **An explicitly written empty constructor is therefore a compile error naming the deletion** (decided by Mortaro, 2026-09-20), the way an unused local is -- a constructor that only declares codegen values (`func Pair<$left_type, $right_type>() { }`) is of course still legal, the way an unused local is -- the language removes redundancy rather than tolerating it. A constructor that declares codegen values (D9) or does real setup is of course still written. |
| 2026-09-20 | **D51** (decided by Mortaro): **a `var` may shadow a name in the same scope**, not only in a nested one, and the previous value is dropped as part of the shadowing. The second binding may hold a different type, which is the point: `var content = content.trim()` turns a `String?` that has been narrowed into a `String` without inventing a second name. **The order is evaluate, then drop, then bind** -- the new value is computed first, so the expression may read the binding it is about to replace, then the previous value is released (running `drop()` if that takes it to zero), then the new binding takes effect. Releasing first would make the most common use of the feature a use-after-free. The unused rule (D-log, second batch item 5) still applies to the shadowed binding, so shadowing a name that was never read is an error -- which separates a deliberate reuse from an accidental one. |
| 2026-09-20 | (decided by Mortaro, confirming D50) **An explicitly written empty constructor is a compile error**: "'Holder' has an empty constructor: delete it. A class without a constructor is already made from its defaults." A constructor that only declares codegen values (`func Pair<$left_type, $right_type>() { }`) stays legal, since it is declaring something. Implemented the same day, deleting 22 empty constructors across the compiler, `library/`, `conformance/`, `examples/` and `tests/` -- `library/nothing.spite` is now an empty file, which is a valid class and a fitting one for `Nothing`. |
| 2026-09-20 | (proposed by Claude, unconfirmed; implements D49) **`Spite.Class.instances` is built, and a class object answers `.functions`.** `Spite.Class.instances` lists the program's own classes: not the library's `Spite.*` classes or `Nothing`, not the anonymous classes behind object literals, and not a generic class before its codegen values are supplied. `some_class.functions` lists that class's functions bound to a fresh instance, made by running its constructor when it takes no arguments and from its defaults otherwise, so `call_function()` works on them; this is what lets `tests/tests.spite` find every class whose name ends in `Tests` and run its `test_` functions with nothing registered. The entry class is listed but its `.functions` is empty, because making an instance of it would run the program again. `Person.instances` (the live instances of one class) is still not built and is a compile error saying so. |
| 2026-09-20 | (implements the 2026-09-18 decision and D8; details proposed by Claude, unconfirmed) **`Person.instances` and singletons are built.** `Person.instances` lists the instances of `Person` that are alive at that moment, in the order they were made, including copies; a class is only tracked when some part of the program asks for its instances, and asking holds them alive only as long as the returned list lives. A class is a singleton when it has `func is_singleton(): Bool { return true }` written exactly so: `Journal()` anywhere returns the one instance, its constructor runs the first time it is asked for, it takes no arguments (giving it some is a compile error), and it is released when the program ends. |
| 2026-09-20 | **D52** (decided by Mortaro): **`Console` is a standard library class, not a reserved word** -- "its not a reserved word its a singleton that gives us access to the Console to print to it or to read input and what console classes normally do in other languages". It is a singleton (D8), so `var console = Console()` is the same instance everywhere and costs one object for the whole program; `Console` resolves as an ordinary class name, so it has a class object, a `.class.name` of `"Console"`, a namespace, and a place in the type system that is a class reference like any other rather than a shape of its own. The immediate gains are `read_line()`, which D37 already needed as a REPL drain point, and an attribute holding a `Console` becoming an ordinary attribute instead of one the generator deletes from the struct. `print`, `write` and `error` stay compiler-written for now because they take any number of arguments of any type, which the language cannot yet declare; everything else about `Console` is now ordinary. This is D14 continuing: the class is real first, the bodies move into Spite as the language grows to hold them. |
| 2026-09-20 | (implements D47) **`enum`, `type` and `union` declarations take no `=` and break lines.** Writing `enum Job = {` is a parse error naming the fix (`diagnostics/old_declaration_equal`), and so is putting two entries on one line or separating them with a comma (`diagnostics/declaration_one_per_line`). A list or object literal is untouched, as D47 scoped it. The 24 declarations in `.spite` files and the three in `docs/for_ai_writers.md` were migrated with it. |
| 2026-09-20 | (implements what section 5, section 9 and `docs/packages.md` already promised; found by compiling the documentation) **Four rules the manual states are now enforced.** A statement at file scope is an error naming the fix, instead of being parsed and silently dropped. A return type written without its colon (`func add_one(value: Int) Int`) says so, instead of "expected '{' but found 'Int'". A folder named `spite`, or `load("spite")`, is an error: that namespace belongs to `Spite.Class` and the rest. And the terminal-`if` lint of section 5 exists -- a function whose last statement is a narrowing `if` with no `else` is an error naming `assert` -- which caught three places in the compiler's own sources on its first run. |
| 2026-09-20 | (found by writing tests; proposed by Claude, unconfirmed) **Three gaps the test package turned up.** `copy()` existed on a class but not on a `List<T>` or `Dictionary<T>`, although section 10 says every non-scalar value has both `copy()` and `deep_copy()`; it does now, shallow, sharing the elements. A name that is reserved in the C Spite emits (`unsigned`, `static`, `int`, `stdout` and the rest) used to emit C that would not compile; it is now a naming error like snake_case and the abbreviation list, which is where the rule belongs. And `value == null` used to say "unsupported expression kind in this stage"; it now names the four ways to narrow, since `if value { } else { }` is already the one way and a second one is what section 5 removed. |
| 2026-09-20 | (proposed by Claude, unconfirmed; found by writing the programs an AI would write) **Syntax borrowed from other languages is a compile error naming the Spite form.** `&&`/`||` name `and`/`or`; `!` names `not`; `++`/`--` and `+=`/`-=`/`*=`/`/=`/`%=` name the assignment written out; `condition ? a : b` names an `if` with an `else`; `new Creature()` names the plain call; `this.name`/`self.name` name the bare attribute; `import`/`require` name `load`; `elif` names `else if`; a parameter with `= default` says every argument is written at the call site; and a bare `print(...)` that resolves to nothing names the `Console`. Each of these was previously a parse error pointing at a character, which tells a writer what the compiler could not read rather than what to write. |
| 2026-09-20 | (implements section 16 item 1 and the reopening half of D7; the `Spite` rule proposed by Claude, unconfirmed) **The standard library is discovered before the program, so a file of your own reopens it.** `library/` used to be walked last, which made the load order backwards: a library class would have reopened a user class rather than the other way round. A `spite/` folder in your own package now reopens `Spite.Class` and the rest -- `func full_name()` written there is answered by every class object in the program, which is what "a new hook name creates a new hook program-wide" means. A file under `Spite` that is **not** reopening an existing class is an error naming the fix, so the namespace stays the standard library's without the folder being forbidden outright; `load("spite")` stays an error on its own name, which is the one asymmetry. |
| 2026-09-20 | **D53** (decided by Mortaro): **text is joined by writing the value inside it, not with `+`.** `"hello {name}"` places `name` in the text at that point; `{ }` holds one value, which may be any expression (`"{person.name} is {person.age + 1}"`), and a brace meant literally is written `\{` the way a quote is written `\"`. Joining a *literal* to a value with `+` becomes an error naming the interpolation -- `"hello " + name` is wrong where `name + other_name` is right, because the first has a written-down piece of text with a hole in it and the second does not. One way to write the common thing, and the shape reads as the sentence it produces rather than as an expression that assembles one. |
| 2026-09-20 | **D54** (decided by Mortaro): **an `if` whose only statement is a bare `return` is a compile error naming `assert`.** `if not directory.exists() { return }` is an `if` written for the sake of asserting something, and section 5 already has the way to say it: `assert directory.exists()`, which returns quietly and leaves the rest of the function unindented. The error names the condition to assert, negating it when the guard was not already negative. It is the same rule as the terminal-`if` lint one step further: an `if` that only decides whether the function continues is not control flow, it is a precondition. The compiler's own sources had 32 of them. |
| 2026-09-20 | **D55** (decided by Mortaro): **a function body holds no empty lines.** A blank line inside a function is a compile error, because it is where a second function wants to be: the part below it is a step with a name, and the way to break a body into pieces is to name them and call them, not to space them apart. Blank lines stay legal between declarations, where they separate one thing from the next rather than one half of a thing from the other. The compiler's own sources had 164 of them, and the documentation's samples 25. It is the same push D27 makes with `assert`: decomposition is forced by the compiler rather than suggested by a style guide. |
| 2026-09-20 | (implements the first step of milestone 10b's design; started by Codex, finished by Claude) **`--final-classes` writes the program back out as Spite source**, one file per class under its namespace folders, after discovery has merged every reopening. `--final-classes` alone writes to `.spite-cache/final`; `--final-classes=<folder>` chooses where. What it writes is a program rather than a report: `check.sh` prints a corpus program that reopens a standard library class, runs what came out, and requires the same output. That is what makes the milestone's "visible, not compiler magic" checkable at all. Not yet shown: which root supplied each declaration (open question 10). |
| 2026-09-20 | (extends the previous row; proposed by Claude, unconfirmed) **`--final-classes` also writes the classes the compiler provides**, under `built_in/`, as `type` declarations -- a `type` is how Spite already names members without bodies, so the view needs no new notation. `Console`, `File`, `Directory`, `Process` and `Program` come out of the generator's own table, so nothing is written twice. **Still missing, and reported rather than faked:** `Int`, `String`, `List<T>` and `Dictionary<T>` have no class table in the compiler at all -- they are shapes the generator knows by name, which is exactly what D14's "Pure Spite" is for, so `Int` cannot be printed until it is a class; `Console.print`/`write`/`error` are variadic and cannot be written as a signature yet (D39's variadic generics); and a generic prints as its template rather than once per instantiation, because the instantiations only exist after generation while this runs after discovery. |
| 2026-09-20 | (extends the previous two rows; proposed by Claude, unconfirmed) **`--final-classes` writes what the program ends up with, not what was written down.** It runs the generator and reads the class table afterwards, so a class tree shaking removed is not written, a generic *template* is not written, and each of its *instantiations* is, in `instantiated/`, named for the values it was given (`WeaponIntTrue`). That is what the flag's name claims and what milestone 10b needs: the thing you read is the program's final shape, arrived at by the same pass that emits it, rather than the source re-printed. What a class made of is still shown as a `type` for a class the compiler provides; a class with a file is still printed from its own source, so nothing about a written class is paraphrased. |
| 2026-09-21 | (implements the D6 half of milestone 10b's second step; proposed by Claude, unconfirmed) **`is_singleton()` is an ordinary member of `Spite.Class`, written in Spite.** `library/spite/class.spite` declares `var singleton = false` and `func is_singleton(): Bool { return singleton }`; the compiler sets that one attribute when it writes a class object, and answers nothing at the call site. So `some_class.is_singleton()` is a real call to a real function you can read, `--final-classes` shows it, and reopening `Spite.Class` can see what it is overriding. **The compiler fills data, never behaviour** -- that is the shape the rest of D6 follows for `attributes`, `functions` and `instances`, each of which is still answered by name at the call site. |
| 2026-09-21 | **D56** (decided by Mortaro): **a shape names the types it requires, never the names they are given.** `render(Int): String`, not `render(scale: Int): String` -- "the name you give an argument does not matter to type". A class satisfies the shape whatever it calls its own parameters, and the parser was already throwing the name away, so the syntax now says what the language already meant. Writing a name is an error that shows the type to write in its place. **A shape also never carries a constructor:** an entry whose name is capitalised would mean requiring a class to be *constructible*, which is forcing a class rather than describing a shape, so `--final-classes` no longer writes one into the `type` views it prints. Still open: whether a required function should instead be written as an attribute holding a `Spite.Function` (open question 11). |
| 2026-09-21 | (found by writing an ordinary program; proposed by Claude, unconfirmed) **`join(separator)` works on every list whose elements become text**, not only `List<String>`: a list of numbers, of `Bool`s or of enum values joins with the same rule `+` and `"{value}"` already use. It used to emit a call to a function it never generated, so `counts.values().join(",")` produced C that would not compile -- the worst outcome a compiler can have, and one an ordinary program hits immediately. |
| 2026-09-21 | **D57** (decided by Mortaro): **class-level reflection is tree-shaken, not made lazy.** Asked whether `Spite.Class.functions` should be filled on demand, the answer is no: a program that never asks for it has none of it emitted, and a program that does asks at compile time and gets it whole. That is the compile-time guarantee the rest of the language already makes, and it is what lets the REPL have everything without deciding what to keep. So `functions` is an ordinary attribute of `Spite.Class`, declared in `library/spite/class.spite`, filled when the class object is written; the hidden C member it used to be is gone. `instances` already worked this way -- a class is only tracked when some part of the program asks for its instances -- and stays a live registry rather than a filled attribute, because which instances exist is not a compile-time fact. **One consequence to know:** a class object holding its functions makes a cycle, since a `Spite.Function` names a `Spite.Class` for what it returns, so the program's exit clears the lists before releasing the class objects -- the same "clear one side" section 10 prescribes for any cycle. |
| 2026-09-22 | (implements D41, milestone 10b; proposed by Claude, unconfirmed) **`Spite.Namespace` is written, and `Spite.Class.namespace` is now one of them.** `library/spite/namespace.spite` declares `.name`/`.full_name`/`.parent`/`.classes`/`.namespaces` with a two-argument constructor; the compiler fills the *data* when it writes a class object -- `->namespace` is the cached object for that class's dotted namespace, `null` for a global class (Mortaro's settled rejection of the root object, section 8) -- so two classes of one namespace share one object and `.parent` walks up to `null`. `Spite.Class`'s constructor drops its namespace parameter (`Class(name)` only). `.classes`/`.namespaces` answer from the merged class list, and both the fill and the tree are tree-shaken behind reads (`namespace_read_used`/`namespace_tree_used`), the same rule D42/D57 prescribe: a program that never reads a namespace emits no namespace objects at all. Printing a `Spite.Namespace` prints its `.full_name`, as D41 says. Every `.namespace` reader in `conformance/`, `tests/` and `docs/` migrated in the same change (`if containing { ... }` narrowing), and `conformance/stage6/namespace_objects` covers identity, the parent chain and both tree walks. |
| 2026-09-22 | (implements the members half of D11/D6, milestone 10b; proposed by Claude, unconfirmed) **`Spite.Class.attributes` is a declared `List<Spite.Attribute>` in `library/spite/class.spite`, filled when the class object is written**, next to `functions` (D57) and `is_singleton` (D8): the compiler assigns `spite_class_attributes_<Class>()`, which builds the list off a default-constructed instance rather than any constructed one, so **a class-level entry's `.value` is the field's declared default** (section 4). This answers the question D11 left open -- what the class-level mapping says for a field the instance has not set -- and it is the half needing Mortaro's confirmation; the symbol-keyed mapping `Weapon.attributes['damage']` (D10) stays milestone 14. Tree-shaken behind `class_level_attributes_used`, and the program's exit clears the lists the same way D57 clears `functions`, since a `Spite.Attribute` holds a `Spite.Class` for `.class` and the cycle is real. `conformance/stage6/class_attributes` walks both levels: `Sticker.attributes` beside `sticker.attributes`. |
| 2026-09-22 | **(decided by Mortaro): `class` is not a keyword -- it is an inherited attribute of any instance, pointing to that instance's class.** "class should not be a keyword instead its just a inherited attribute of any instance that points to the class of that instance." So a bare `class` inside any function of a class answers where `self.class` would: resolved after a local or an attribute of that name (a declared field wins -- that is how `Spite.Attribute.class` reads its own field) and before a class name of the same spelling, returning the class object with the same ownership `monster.class` returns it with, so `class.name` composes. The parser keeps its diagnostic -- `class ClassKeyword {` at file scope still fails at 1:1 with "there is no 'class' keyword" (`diagnostics/class_keyword`), because a file is a class named after the file -- and `class` was never in the reserved-name list. `tests/reflection_tests` covers it bare (`class.name`), through a local (`var mine = class`), and in an instance method (`creature.class_name()`).  **[implemented]** |
| 2026-09-22 | **(decided by Mortaro): a class name reads its class object member by member -- `Hello.name` is the class's name -- and the `Namespace?` narrowing error gets tests.** "i should also be able to directly say Hello.name to get class name"; "we should have tests for it." The first falls out of D6 (a class name is an ordinary `Spite.Class` value): the static-path substitution that made `Sticker.attributes` work now serves every member read and method receiver on a static path -- `Creature.name`, `Creature.namespace`, `Creature.attributes` answer the same class object as `creature.class.*`, and `Console.is_singleton()` calls -- while `Creature.instances` still means its live registry and `Creature.debug()` still reports "'Creature' is a class, not a value: write 'Creature()'", since there are no static functions. The second pins the existing nullability rule rather than changing it: `diagnostics/namespace_nullable` is the reported shape verbatim (assert a local copy, then re-read `Hello.namespace.name` -- narrowing narrows the variable you hold, so the fresh read must be narrowed in its own right), and `tests/reflection_tests` exercises all three narrowing forms (`if`, `assert`, `crash`) on `Spite.Class.namespace`.  **[implemented]** |
| 2026-09-23 | **(decided by Mortaro): D43's chain narrowing is the ordinary rule -- no special case for a class path, because every attribute Spite adds by default is a normal attribute and the normal null rules apply to it.** "the chain form for a class should not be a special case at all, namespace should be returning a Spite.Namespace which should follow same null coalecense rules, the beauty of spite metaprograming is that every attribute spite adds by default is just a normal attribute so any normal rule apply." Implemented as section 5 already specifies: `Scope` keeps a set of narrowed paths beside its name overrides, and a member read whose path is in the set answers with its plain type -- the same thing `lookup_override` already does for a bare name, consulted where a member generates instead of where an identifier resolves, so nothing downstream (member reads, method calls, stores, conditions) knows that paths exist. `if`, `assert`/`crash` and a switch subject each record the path in the block they narrow (then-scope, current scope, case-scope); while the condition itself generates its strict prefixes are visible in a throwaway scope, since the condition reads through them, which is what makes a deep chain provable in one statement; every link is tested before the next is read, so a null link fails the narrowing instead of dereferencing anything, and a condition that owns what it read releases it in the guard itself. A local copy stays its own path (`diagnostics/namespace_nullable`), the `else` of a narrowed `if` does not inherit the narrowing (`diagnostics/path_narrow_else`), `conformance/stage6/namespace_objects` walks a nullable prefix (`gadget.class.namespace.parent`) with `crash`, since a constructor bans `assert`, and `tests/reflection_tests` has the `if`/`assert`/`crash` forms, a deep `Spite.Attribute.class.namespace` chain, and a null path that fails the `if` without being dereferenced. **Open:** the manual walks up with `class.namespace.namespace` (section 5, section 8, D43, the 2026-09-20 row), but `Spite.Namespace` declares `.parent` and has no `.namespace` (section 8 itself: "`.parent` walks up it") -- either the member is meant to exist and the library is missing it, or every example means `.parent`; Mortaro to say which.  **[implemented]** |
| 2026-09-23 | **Review of the D43 implementation (Claude Opus 5.5, reviewing a commit written by another model).** Two defects, both fixed. First, the row above says every link is tested before the next is read; the guard actually generated the whole path with its prefixes narrowed and tested only the last link, so `if outer.inner.inner` with a null `outer.inner` dereferenced null. The guard now tests each nullable link in order, joined with `&&`, so a null link stops the test (`conformance/stage6/path_narrowing`). Second, nothing undid a path narrowing: `assert outer.inner` then `outer.inner = null` (or `outer = Box()`) then `outer.inner.label` compiled and crashed. **(proposed by Claude, unconfirmed):** an assignment undoes every narrowing at or beneath the path it writes -- except that a value which cannot be null keeps the written path itself narrowed -- and inside a `while`, undoing a narrowing made before the loop that the loop has read is an error naming the fix, since generation is one pass and the read earlier in the body was already emitted as proven (`diagnostics/path_narrow_assignment`, `diagnostics/path_narrow_loop`). **Still open, for Mortaro:** a function call can change an attribute behind a narrowing (`assert tracker.target` then `reset()` which sets it to null). This is not new -- a narrowed bare attribute name has the same gap -- and closing it means either re-testing after every call or treating narrowing of an attribute as valid only until the next call. Also found, older than D43: assigning `null` to a narrowed *name* (`assert maybe` then `maybe = null`) stores `Box_default()` instead of null, silently. |
| 2026-09-23 | **D58** (decided by Mortaro): **`join` is not special to `List<String>`.** It converts every item to text and joins the results; converting text to text is folded away as a no-op. So `join` is one function for every element type rather than a text-only case the other types borrow.  **[implemented]** |
| 2026-09-23 | **D59** (decided by Mortaro): **no function overloading; arguments cast to the parameter's type.** "I don't like how ambiguous it is." One name means one function; calling `takes_a_float(i)` with an `Int` casts `i` to `Float` by the ordinary casting rule (section 4), exactly as an assignment would. A class that wants to accept several kinds of value accepts a union or a `type`. How a class defines its own casts is open question 13. Already true when decided: an argument casts by the rule an assignment uses, and declaring a function name twice is an error.  **[implemented]** |
| 2026-09-23 | **D60** (decided by Mortaro): **a `switch` stays exhaustive, and `_:` covers every case not written.** As in Rust, a last `_:` case answers for the rest. And because a call on a union works when every member has the function (section 7), `_: shared.method()` calls the function all remaining cases share. **A `switch` that repeats a case body is a compile error:** cases with the same answer are written once, through `_:`. As implemented (Claude's reading, unconfirmed): `_:` expands to its body once per remaining member, each narrowed to that member, because "the same text" is only "the same thing" when each copy sees its own member -- which is also what lets `_: shared.method()` work when only the remaining members share it. Migrating the tree folded the repeated cases of the compiler's generated shape helpers, its printers and parser, and `examples/battle`.  **[implemented]** |
| 2026-09-23 | **D61** (decided by Mortaro): **every function Spite adds to a class appears in `--final-classes`**, the member templates included (`sum_price()` and the rest, D15): a function you can call is a function you can read. **Partly implemented:** every function Symbol codegen made for a class is printed as an ordinary function under the class's own source, with the symbol spelled out (`func describe_age(): String { return "age is {age}" }`), and check.sh runs the printed `conformance/stage6/symbol_codegen` to the same output. The `List<T>` member templates are not printed yet, because `List<T>` has no class of its own to print them into until milestone 15c writes it in Spite.  **[partial]** |
| 2026-09-23 | **D62** (decided by Mortaro): **no destructuring and no lambdas, anywhere.** "Writing it is fun, but it's not a human who will write code in this language, so it's a pointless readability sacrifice." A function value is a named, bound function (D17); a value is taken apart by reading its members by name. |
| 2026-09-23 | **D63** (decided by Mortaro): **a local that only copies a name or a path so it can be narrowed is a compile error.** `var watcher = tracker; assert watcher; watcher.note_a_drop()` is written `assert tracker; tracker.note_a_drop()` -- narrowing works on the name and on the path (D43), so the copy is a human habit with no reason in Spite. Its exact scope (proposed by Claude, unconfirmed): a `var` whose value is a bare name or member path of a `T?` type, which is then narrowed, is never assigned again, and whose source is not assigned later in the function either -- a snapshot taken before the source changes is not a copy for narrowing. Migrating the tree removed 110 such copies from the compiler and a dozen from the corpus, and turned up the check-proves-nothing error of section 5.  **[implemented]** |
| 2026-09-23 | **D64** (decided by Mortaro): **reading with `[]` answers `T?`** and has to be narrowed before use, so an out-of-range read is never a hidden runtime crash. From a test that read `attributes[0]` and `attributes[1]` after checking only the count: "we should either have an assert per case but also ideally extend the language to understand that if count > 2 it means 0 and 1 are safe" -- **a proven count proves the indices below it** (decided in intent; the exact rule is Claude's to propose). Most index reads live in the index loops of open question 15, which is why the two should land together. Writing through `[]` is unchanged. As implemented (rules proposed by Claude, unconfirmed; section 5): a proven count, a bound in a condition (`while index < names.count()` proves `names[index]` in the body) and an explicit `crash names[index]` narrow a read, and assigning the list or the index, or shrinking the list, undoes it. The bound rule alone proved all but about 45 of the compiler's 369 index reads, so open question 15 is no longer needed for D64 to be livable; the rest were parallel lists read by one counter, and got a `crash`. A `Bool?` in a condition became an error on the way, because `if flags[index]` would have silently changed from 'is true' to 'is there'. Two bootstrap steps: an intermediate compiler that tolerates `crash` on a plain value compiled the migrated sources into the new seed.  **[implemented]** |
| 2026-09-23 | **D65** (decided by Mortaro): **`full_name` is renamed, because it is ambiguous**: the name must say what it holds, "something like `name_with_namespaces`". Applies to `Spite.Namespace.full_name` and the `full_name()` a reopening adds in `conformance/stage6/reopen_library`. |
| 2026-09-23 | **D66** (decided by Mortaro): **the test package proves there are no memory leaks.** A test run that ends with allocations and frees unequal fails, as every corpus program already does. |
| 2026-09-23 | **D67** (decided by Mortaro): **the order of a file is enforced**: `singleton`, then `generic` lines (open question 12), then `enum`, then `type`, then variables, then the constructor, then functions. Out of order is a compile error (section 12: the compiler formats or errors). Implemented for what exists today, with `union` placed between `enum` and `type` (Claude, unconfirmed); nine files in the tree were reordered.  **[implemented]** |
| 2026-09-23 | **D68** (decided by Mortaro): **a class name is a `Symbol`, not a `String`.** Symbols are easier to tree-shake, and for the reader a `Symbol` answers every `String` method as if it were text, even though `symbol.class == Symbol`. Text can become a `Symbol` only when that symbol already exists in the program's table -- enough for metaprogramming, without Ruby's attack of minting symbols from input. Part of milestone 14. **Not implemented, because it conflicts with D10** (found by Claude, 2026-09-23): D10 says `Symbol` is compile-time only and that storing a bare `Symbol` in a field is an error, while a class object holding its name as a `Symbol` is exactly a stored, runtime `Symbol`. Claude's proposal (unconfirmed): D68 supersedes that clause of D10 -- a `Symbol` value at runtime is an entry of the program's symbol table, which the compiler writes the way it writes an enum (tree-shaken to the symbols the program uses), so storing one is fine and comparing two is comparing two numbers; a symbol *literal* still needs something to say what it may be (D10's other clause stands); and `Symbol("Creature")` answers `Symbol?` -- null unless that symbol is already in the table, which is D68's "no minting from input". Mortaro to confirm before it is built. |
| 2026-09-23 | **D69** (decided by Mortaro): **comparing a `T?` with `==` needs no narrowing: null is simply not equal**, so `crash Spite.Class.namespace == "Spite"` is the whole of a test that used to be five lines of copying, narrowing and comparing. "If AI can do this messy code, it will do this messy code" -- the language should accept only the short form (D63 rejects the copy). How a `Spite.Namespace` compares with text is an operator function in the library (operators are functions, section 5), not a special case; under D68 that compares its name. As implemented it compares `name_with_namespaces`, so a nested namespace is compared by its dotted path (Claude's reading, unconfirmed).  **[implemented]** |
| 2026-09-23 | **D70** (decided by Mortaro): **symbols are completely tree-shakable runtime values.** Answering the D68/D10 conflict: "symbols should be completely treeshakable, that way all internal metaprograming can use symbols which are strings guaranteed to make no new allocations, from a precompiled list, but they allow for tree shaking so unused symbols simply disappear, and if they are used that means we would need them anyway. symbols can also be optimized into TinyString most of the cases because they are normally small." So D10's "a `Symbol` is compile-time only; storing one is an error" is superseded: a `Symbol` value is an entry of the program's symbol table, written by the compiler with only the symbols the program uses, and never allocates. D68 (class names are symbols) is unblocked by it, and both are built the same day: `Symbol` is a type, a symbol literal where one is expected is an entry of a table the compiler writes only for the symbols a program uses (`static` text, so retaining or releasing one does nothing), a `Symbol` reads as `String` wherever text is expected, `Symbol(text)` searches that table, and `Spite.Class.name` is a `Symbol` (`tests/symbol_tests`, including one that proves storing and comparing symbols allocates nothing). The names of every reflection object followed the same day (Claude, on Mortaro's 'all internal metaprogramming can use symbols'): `Spite.Function.name`, `Spite.Argument.name`, `Spite.Attribute.name`, `Spite.Namespace.name` and `.name_with_namespaces` are symbols too, so `functions.map_name().contains('label')` takes a symbol literal. `symbol.class` is `Symbol` (`tests/symbol_tests`). Not built: TinyString. TinyString -- a small inline string instead of a pointer into the table -- is recorded as the intended representation for short symbols, an optimisation below the language. |
