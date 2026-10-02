# Style: the compiler is the formatter and the linter

Style in Spite is not a matter of taste, and there is no style guide to follow: the compiler either rewrites your
code into the one style or refuses it with an error that names the fix. It has no warnings and no switch to turn
a rule off. What cannot be rewritten safely, such as a name or a structure, is an error; everything else is rewritten.

## What the formatter rewrites

Every compile formats the program's own files, printing `formatted <path>` for each one it changed, and then reads
the program again, so what it compiles is the formatted file. Nothing turns this off, and there is no command
that only formats: `--check` formats and checks without building ([compiler.md](compiler.md#formatting)):

- 4 spaces per level, never a tab; no trailing whitespace; one line break at the end of the file.
- K&R braces: `func name() {`, `} else {`, `} else if other {`. An empty body is `{ }`.
- One space around binary operators and after `,` and `:`; none inside `( )` and `[ ]`.
- The fewest parentheses that keep the meaning: `1 + 2 * 3`, not `1 + (2 * 3)`.
- A list, object or call that fits in 120 columns goes on one line with `, `; a longer one is broken one entry
  per line, with no commas for a list or object literal and with trailing commas for a call.
- One blank line between declarations at file level; consecutive `var`s may stay grouped.
- A switch case with one short statement stays on its line; `crash false` becomes a bare `crash`, the one form
  for a branch that cannot happen ([failure.md](failure.md#crash)).

It never reorders code or drops a comment: it re-reads its own output and checks that the program and every
comment survived, and if it cannot prove that, it leaves the file alone and says why, which is an error that stops
the compile, so a program is never compiled from text that is not in the one style.

Everything on this page happens while compiling. None of it reaches the built program, which carries no trace
of the formatter or of any of these checks.

## A file is ordered

A file holds, in this order: the `singleton` line, `generic` lines, enums, unions, types, variables, the
constructor, then functions ([classes_and_files.md](classes_and_files.md#what-a-file-holds)). Anything out of
order is an error naming what came before it:

```gdscript title=declaration_order_error/declaration_order_error.spite entry error
var console = Console()

func DeclarationOrderError() {
    console.print(label)
}

var label = "late"
```
```diagnostic
but a file is ordered: the singleton line, generic lines, enums, unions, types, variables, the constructor, then functions
```

## Names

A name is never rewritten for you, because renaming a symbol can change what a program means to someone
searching for the old name. These are errors, each with the fix:

- `snake_case` for variables, attributes, parameters, functions, enum values, files and folders (a package's
  root folder too: `window_plugin`, never `window-plugin`); `PascalCase` for classes, enums, unions and
  types.
- **Never a single letter.** No exceptions.
- **Never an abbreviation.** Every word of a name is checked against a list (`msg`, `idx`, `val`, `cfg`, `tmp`,
  `str`, `len`, `max`, `min`, `init`, `env`, `dir`, `doc`, ...; the full table is in
  [Naming and abbreviations](#naming-and-abbreviations-compile-errors-not-auto-fixed) below),
  and the error spells the name out. `id` is allowed.
- **Any word C uses is yours too**: `unsigned`, `static`, `stdout` or `near` are ordinary names, because the C a
  program compiles to writes every local, parameter and attribute with a `_` after it.
- **One underscore between words**, and at most one in front to make a name private: `hit__count`, `__strike`
  and `strike_` are errors. That is what keeps the C the compiler makes for a class (how it allocates, counts
  and releases one, under names with `___` in them) out of the way of yours, so `allocate`, `make`, `release`
  or `class_of` are ordinary function names.
- **Never a function's name.** A local, parameter or attribute named like a function of its class is an error,
  since the function's name is already a value ([functions_and_operators.md](functions_and_operators.md#functions-are-values)):
  `var stem = file_stem(path)`, not `var file_stem = file_stem(path)`.
- **Names say what they do.** The standard library follows the same rule: `append` and `prepend` rather than
  `add`, `upper_case()` rather than `upper()`, `remove_last()` rather than `pop()`.

```gdscript title=lint_error/lint_error.spite entry error
var console = Console()
var msg = ""

func LintError() {
    console.print(msg)
}
```
```diagnostic
'msg' abbreviates: write 'message' instead of 'msg'
```

```gdscript title=single_letter/single_letter.spite entry error
var console = Console()
var x = 0

func SingleLetter() {
    console.print(x)
}
```
```diagnostic
is a single letter: give it a name that says what it holds
```

## Nothing unused

A local variable that is never read is an error: remove it. Only reading counts, so a variable that is
assigned and never read is unused too, and there is no spelling that silences it: a variable nobody reads
is work the program did for nothing. A parameter is the one exception, because a signature can need a parameter
its body ignores: name it `_name` to say so. A `_name` that *is* read is an error, since the prefix says it is
not, and a parameter whose signature is dictated from outside (an operator function's, for one) needs no `_` at
all ([the exemptions](#unused-is-an-error)). Everywhere else a leading `_` means private to its
class ([reflection.md](reflection.md#reflection-is-read-only)).

```gdscript title=unused_error/unused_error.spite entry error
var console = Console()

func UnusedError() {
    var forgotten = 3
    console.print("hello")
}
```
```diagnostic
'forgotten' is never read: remove it
```

```gdscript title=written_not_read/written_not_read.spite entry error
var console = Console()

func WrittenNotRead() {
    var total = 1
    total = 2
    console.print("hello")
}
```
```diagnostic
'total' is never read: remove it
```

An attribute nothing reads is an error the same way: remove it. A private `_name` attribute is no
exception, since only its own class can read it. Writing an attribute is not reading it, so one that is only
assigned is still unused. A read is anything that takes the attribute's value: its name in the class's own
functions, `thing.world` from another class, `x.attributes[attribute]` in a template or a walk (which is how the JSON
and binary writers and `to_debug()` read), and the rest [in the rules](#unused-is-an-error). A walk that only looks
at `attribute.name` or `attribute.class` reads the attribute's description, not the attribute, so an attribute
kept only as a marker for such a walk is an error:

```gdscript title=unused_marker/world.spite
singleton
```
```gdscript title=unused_marker/mover.spite
var world = World()
```
```gdscript title=unused_marker/unused_marker.spite entry error
var console = Console()
var mover = Mover()

func UnusedMarker() {
    Mover.attributes.each(classify)
    console.print(mover.class)
}

func classify(attribute: Spite.AttributeDeclaration) {
    console.print(attribute.name, "is a", attribute.class)
}
```
```diagnostic
the attribute 'world' is never read: remove it
```

**A package's public attributes are judged where their readers are.** A public attribute is data a class offers
to code it cannot see, so a folder the program `load`s is not held to what this one program reads of it: a
`ui` package's `color` that only a `render` package reads is fine in a program that loads `ui` without `render`.
The check covers the program's own folder, the standard library (always compiled whole, so its reads are the
same in every program), and every private attribute wherever it lives, since its own class is its only reader
. A singleton binding is checked everywhere, loaded
packages included: `var world = World()` offers nothing another class could not bind itself, so an unread
one is the error above wherever it is.

Unused means unread anywhere in the source, not unreachable. A function nobody calls is not an error, private or
public: its callers may be code this compile cannot see, and the production build drops it anyway
([compiler.md](compiler.md#development-builds-and-tree-shaking)). It still reads what it names, so what it
reads counts as used. A few attributes are never checked, because the compiler reads them itself: a number
class's or `String`'s storage, and `Build` and `Environment` settings.

## Comments are links

A comment is one line, outside functions and declaration bodies, and it holds nothing but a link to a heading in
a markdown file:

```gdscript
# notes.md#why-this-program-exists
func Game() {
```

The path is resolved from the entry file's folder, and the compiler checks that the file exists and has that
heading, so a link cannot rot silently. Prose in a comment, `//` and `/* */` are errors, and each one asks
whether the note is needed at all: if a reader could work it out from the code, delete it; if it is lasting
knowledge, write the section and link to it; if it warns against a change, a test or a compile error pushes
harder than prose. A line inside a function that seems to need explaining becomes a named function instead:
a name, unlike a comment, is visible to reflection, `--final-classes` and every tool.

```gdscript title=prose_comment_error/prose_comment_error.spite entry error
var console = Console()

# prints a greeting
func ProseCommentError() {
    console.print("hello")
}
```
```diagnostic
this comment is not a link: a comment is one line holding only a link to a markdown section
```

## A function body holds no empty lines

A blank line inside a function is where a second function wants to be: the part below it is a step with a
name. Name it and call it. The formatter deletes an empty line inside a function body, and turns a run of empty
lines outside functions into one; every compile formats first, so a body written with an empty line
compiles, and the file comes back without it (`formatted path`). What stays is the rule that no formatted file has
one, and the push to name the part below is yours, since the formatter only joins the halves. Between
declarations one blank line stays.

## One call per line

**No call is an argument.** A call is computed first into a named `var`, and the name is passed:

```gdscript title=call_argument_error/call_argument_error.spite entry error
var console = Console()

func CallArgumentError() {
    var source = "spite is small"
    console.print(source.slice(0, 5))
}
```
```diagnostic
is called inside an argument of 'console.print': compute it first into a named 'var' and pass the name
```

```gdscript title=call_argument_fixed/token.spite
var text = ""

func Token(new_text: String) {
    text = new_text
}
```
```gdscript title=call_argument_fixed/call_argument_fixed.spite entry
var console = Console()

func CallArgumentFixed() {
    var source = "spite is small"
    var first_word = source.slice(0, 5)
    console.print(first_word)
    var tokens = List<Token>()
    var token = Token(first_word)
    tokens.append(token)
    var word_count = source.split(" ").count()
    var first_token = tokens.first()
    crash first_token
    console.print("{word_count} words, first token {first_token.text}")
}
```
```output
spite
3 words, first token spite
```

A method called on a call's result (`source.split(" ").count()`) and the holes of a text
(`"{names.count()} names"`) are not arguments. Every case is
[in the rules](#one-call-per-line-and-nothing-said-twice).

**A constructor call is not an argument either.** An object is made on a line of its own and passed by
name, however short the line:

```gdscript title=constructor_argument_error/label.spite
var text = ""

func Label(starting_text: String) {
    text = starting_text
}
```
```gdscript title=constructor_argument_error/constructor_argument_error.spite entry error
var console = Console()

func ConstructorArgumentError() {
    var labels = List<Label>()
    labels.append(Label("new"))
    var count = labels.count()
    console.print(count)
}
```
```diagnostic
'Label("new")' is constructed inside an argument of 'labels.append': a constructor call is never an argument, so make it first on a line of its own, 'var label = Label("new")', and pass 'label'
```

A constructor is a call whose last name starts with an upper-case letter, so this covers a constructor inside
another constructor's arguments (`Label(Font())`) and a collection made for a call
(`buffers.set(List<String>())`). An object made to be read at once is not an argument: `JsonWriter(order).write()` is
fine, and so is `var label = Label("new")` itself. `Parallel(worker.run)` passes a function value, not an object.

**An `if` and its `else` do not repeat the same work.** When both branches compute the same call, it is
computed once before the `if`:

```gdscript title=repeated_branch_error/repeated_branch_error.spite entry error
var console = Console()

func RepeatedBranchError() {
    var source = "spite"
    var shout = true
    if shout {
        var word = source.slice(0, 3)
        console.print(word.upper_case())
    } else {
        var word = source.slice(0, 3)
        console.print(word)
    }
}
```
```diagnostic
'source.slice(0, 3)' is computed in both branches: compute it once before the 'if'
```

## The short form is the only form

When the language has a short way to say something, the long way is an error that names it. Each has its own
page:

| Written | Is an error naming | Page |
|---|---|---|
| `if not ready { return }`, `if names.is_empty() { return null }` | `assert ready`, `assert not names.is_empty()` | [failure.md](failure.md#an-if-that-only-returns-the-default-is-an-assert) |
| a last `if value { ... }` with no `else` | `assert value` | [failure.md](failure.md#the-last-if-of-a-function) |
| `var watcher = tracker` then `assert watcher` | `assert tracker` | [failure.md](failure.md#narrowing-a-path) |
| `assert tracker` on a value that cannot be null | removing it | [failure.md](failure.md#narrowing-a-path) |
| a switch that is one class case and `_:`, each a `return` | `if value == Class` or `return value == Class` | [control_flow.md](control_flow.md#value--class) |
| two switch cases with the same body | `_:` | [control_flow.md](control_flow.md#switch-over-a-union) |
| three `if`s comparing one value with a constant, each only returning | the `switch` it spells | [control_flow.md](control_flow.md#switch-over-values) |
| a `while` over `items` whose body only adds up `items[index].price` | `var total = items.sum_price()` | [collections.md](collections.md#member-templates-loops-you-do-not-write) |
| an `if`/`else` directly inside a branch of another `if`/`else` | a function named for what the inner one decides, or one `switch` | [control_flow.md](control_flow.md#if) |
| `"hello " + name` | `"hello {name}"` | [values_and_types.md](values_and_types.md#string) |
| `count * total` with an `Integer` `count` and a `Long` `total` | `total * count` | [values_and_types.md](values_and_types.md#wider-arithmetic-goes-wider-operand-first) |
| `(65536 - 120) * 65536` | `4287102976` | [values_and_types.md](values_and_types.md#wider-arithmetic-goes-wider-operand-first) |
| `func Holder() { }` | deleting it: a class without a constructor is made from its defaults | [classes_and_files.md](classes_and_files.md#constructors) |
| `Report(text)` as a statement of its own | a function, `report(text)`, on the class that needs it | [classes_and_files.md](classes_and_files.md#constructors) |
| `this.name` | `name` | [classes_and_files.md](classes_and_files.md#this) |
| `Console().print(value)`, `Build().program`, `greet(Console())` | `var console = Console()` beside the attributes, then `console.print(value)` | [classes_and_files.md](classes_and_files.md#singletons) |
| `enum Job = { 'knight', 'mage' }` | one entry per line, no `=`, no commas | [values_and_types.md](values_and_types.md#enums) |

The reason is the same everywhere: Spite is written mostly by AI and read by people, and a long way round that the
compiler accepts is a pattern that spreads.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge cases, the
exact error texts and the notes on how each rule is compiled. Where the teaching above and these rules disagree, the
rules win.

### Unused is an error

A local variable or parameter that is never read is a compile error.
**Only a read counts**: a local or parameter that is only ever assigned is unused,
`'total' is never read: remove it`; a read in the new value, `label = "{label} of things"`, is a read
(`diagnostics/written_not_read`). **Only a parameter may be `_name`**: a signature can need a parameter
its body ignores, and `_name` says so: `the parameter 'amount' is never read: remove it, or name it '_amount'
if the signature needs it`; a local has no such reason, so an unread local is an error whatever its name, and
the message only says to remove it. A `_name` that *is* read is also an error: `'_counted' is read, so it must
not start with an underscore: only a parameter the body ignores is named that way` (`diagnostics/unused_names`).
A declaration whose own statement already failed is not reported as unread as well.
**A function nobody calls is not reported**, private or public: its callers may be code
the compiler cannot see, tree shaking removes it from a production build, and the reads inside it still count.
Parameters whose signature is dictated from outside are **exempt** from the read requirement, because the author
never chose them:

- operator functions (`sum`, `subtract`, `equals`, `get_at`, ...;
  [Operators](functions_and_operators.md#operators)): `a + b` calls
  `sum(b)`, so the parameter is the operator's, not the author's
- Symbol codegen templates (`set_attribute`, `get_attribute`, ...): the whole signature is the template
  mechanism's, with one instantiation per attribute, never emitted as the author wrote it
- setters that answer attribute access: a one-parameter `set_age` naming a real `age` attribute answers
  `person.age = 1` ([Setter/getter interception](functions_and_operators.md#settergetter-interception)), so its
  parameter is the write's, not the author's
- functions that replace another through class reopening
  ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)): the signature is
  dictated by the earlier root's call sites

A `set_age` with two parameters never answers attribute access (the dispatch passes exactly one value), so its
parameters are the author's free choice and the ordinary rule applies.

**An attribute nothing reads is an error too.** The message names the attribute and the fix, `the attribute 'world'
is never read: remove it`, and no spelling keeps an unread attribute (`diagnostics/unused_attributes`,
`conformance/stage6/attribute_uses`). **A compile-time walk counts only when it reads the attribute's value**:
`x.attributes[attribute]` read in a Symbol template, and so the JSON and binary writers and
`to_debug()`, and the REPL's display. `attribute.name` and `attribute.class` are the attribute's description, not
its value, so an engine's scheduling markers, such as `var world = Resource.World()` that only a classify walk
inspects, are errors.

What counts as a read, and the exceptions:

- **A read takes the value**: the attribute's name in its own class's functions or attribute defaults,
  `thing.world` from another class (including through a union's members or a `type` a class is admitted to), a
  getter answering that read, `x.attributes[attribute]` or `attributes[attribute]` in a template, `thing.attributes`
  (the values), a `load` line's folder the compiler reads, and the REPL's `attributes` in a `--repl`/`--repl-port`
  build. A class object's `.attributes` (`Mover.attributes`: names and types) does not count.
- **Writing is not reading.** An attribute that is only assigned, from inside or outside, is unused, the same
  rule as for locals.
- **Source-level, not reachability.** A function nothing calls still reads what it names; tree shaking removes it
  later. Where the compiler does not compile a body (a branch a codegen or `Build` test folds away, a function of
  a generic class no instance emits, a Symbol template no range reaches), its reads still count: bare names as
  the class's own attributes, and `x.name` by name for any class (so a fold can hide an unused attribute of the
  same name elsewhere, never report a used one).
- **`_name` is only private**: a private attribute nothing reads is an error like a public one, judged by
  its own class, which is its only reader. An attribute kept only for its constructor's side effect (a binding
  such as `var server = Server()` whose construction starts something) is an error too, whatever its name.
- **Never checked**: a number class's or `String`'s storage (`_memory`, `_bytes`, ...; the compiler reads
  them), and every attribute of `Build` and `Environment`, which the compiler and the program's
  flags read.
- **A library is judged on its own code.** Every non-generic library class is compiled whole in every program,
  and a generic one's uncompiled functions count as above, so the library's own reads are the same in every
  program: an attribute the library itself reads is never an error for a program that ignores it, and every
  library attribute is read by library code. A program's read of a library
  attribute counts too, which can only hide an error, never cause one.
- **A loaded package's public attributes are not checked.** A public attribute is reported only where every reader
  is visible, and it is data offered to code its package cannot see: an engine's `ui` package declares
  `BackgroundColor.color` for its `render` package to read, and a program that loads `ui` without `render` must
  still compile. So the check covers the program's own folder (its entry folder, minus the folders it `load`s), the
  standard library (as above), and private attributes everywhere, whose only reader is their own class. An
  attribute a program adds by reopening a loaded class is the program's and is checked
  (`conformance/stage6/package_attributes`, `diagnostics/package_attributes`). A loaded package's unused public
  data is tree-shaken rather than reported.
- **An unused singleton binding is an error everywhere**, a loaded package included:
  `var world = World()` that nothing reads is `the attribute 'world' is never read: remove it` even in a folder
  the program `load`s, since a class that needs a singleton binds it itself. A
  read of the binding from another class still counts, as it does for any attribute.
- The check runs only when the program has no other error, so a failed statement does not report the attributes
  it would have read.

### Style

The compiler is the linter and the formatter. Style is arbitrary and there is only one way: the compiler rewrites
source files to the one style automatically instead of complaining. Formatting on
every compile is documented in [Command line](compiler.md#command-line); the naming/abbreviation
lints that cannot be auto-fixed are their own subsection below.

#### One call per line, and nothing said twice

From the tokenizer's `flush()`:

```gdscript
tokens.append(Token('number', source.slice(token_start, end_index)))    # error: slice() is passed as an argument
```

- **No call is an argument.** Only constructors could once be used as
  arguments of a call, and any other function needs to be done outside; `source.slice(...)` inside
  `tokens.append(...)` is an error; it is computed first and named.
  The error names the call and the call it is passed to: "'source.slice(0, 2)' is
  called inside an argument of 'console.print': compute it first into a named 'var' and pass the name"
  (`diagnostics/call_as_argument`). A constructor is a call whose callee's last name starts with an upper-case
  letter (`Token(...)`, `List<String>()`, `Syntax.Expressions.IdentifierExpression(...)`).
  The readings are:
  - The rule is the same for every call, a constructor included: `var token = Token(kind, source.slice(a, b))`
    is an error.
  - Anything inside an argument counts, not only the argument itself: `counts.append(count_words(text) + 1)`,
    `print(names[index_of(name)])` and `print(first == Displacement(1, 2))` put a call inside an argument.
  - A method called on a call's result is not an argument: `source.slice(0, 2).upper_case()` is fine on its own,
    and an error only when it is itself passed to something.
  - A text with holes is not a call argument, and each hole is read like a line of its own:
    `console.print("{count_words(text)} words")` is legal, `console.print("{shout(count_words(text))} words")` is not.
  - A call in an `if` or `while` condition, a `return`, an assignment or an index is not an argument.
  - A singleton's constructor is never an argument, nor anywhere but the whole value of a `var`
    ([Singletons](classes_and_files.md#singleton-rules)).
- **A constructor call is never an argument.** A call such as
  `var button = world.create_entity_from_bundle(Bundle.CounterButton(screen.id))` looks plainly weird, so whatever
  the line's width, an object is made on a line of its own and passed by name. The error quotes the
  whole construction and names the variable to make, the class's name in snake_case: "'CounterButton(screen.id)'
  is constructed inside an argument of 'screen.add': a constructor call is never an argument, so make it first on
  a line of its own, 'var counter_button = CounterButton(screen.id)', and pass 'counter_button'"
  (`diagnostics/constructor_as_argument`, and `diagnostics/long_line_construction` for a long line). It covers a
  constructor inside a constructor's arguments (`Label(Font())`), a generic collection made for a call
  (`buffers.set(List<String>())`), and every argument list: of a `var`, an assignment, a call standing alone, a
  `return`, an `assert` or `crash`, an `if`, `while` or `switch` condition, an attribute's default, and a call
  inside a text's hole. A constructor used as a
  receiver (`JsonWriter(order).write()`) and the whole value of a `var` are not arguments; `Parallel(worker.run)` and
  `Concurrent(worker.run)` take a function value, so they are fine; a singleton's constructor passed as an
  argument keeps its own error instead of this one.
- **An `if` and its `else` do not repeat the same work**: when both branches compute the same thing,
  it is computed once before the `if`. In the example, both branches sliced the same range. **It stays narrow**:
  only a call with arguments that appears in every branch, at the start of the branches, is reported, so
  everything it reports can be computed once before the `if` without changing behaviour. The error is
  "'source.slice(token_start, end_index)' is computed in both branches: compute it once before the 'if'"
  (`diagnostics/repeated_branch_call`). The exact reading follows.
  It counts a call with at least one argument to a function or method (not a constructor, whose name starts
  with an upper-case letter) that prints identically in every branch: both branches of an `if`/`else`, or
  every branch of an `else if` chain that ends in `else` ("computed in every branch"). A chain where only some
  branches share the call is not reported, since computing it before the `if` would run it on paths that never
  did. Only the calls evaluated once and first thing in a branch count: the ones in the branch's statements
  up to its first nested `if` (whose condition counts), `while` or `switch`, not a call standing alone as a
  statement (it has no value to compute once), not the right side of `and`/`or`, and not a call that mentions
  a name the branch declared, assigned, asserted or called a method on before it, or a name the `if`'s
  conditions may narrow (tested for truth, or compared with `null` or a class). Names, literals, attribute
  reads and wider repetitions are not reported.

#### Formatting

- 4 spaces per indentation level; never a tab. No trailing whitespace on any line. Exactly one newline at the
  end of the file.
- A `<...>` list holding a single named codegen value is rewritten to the positional form
  ([Codegen values (`$`)](metaprogramming.md#codegen-values-)), so there is one way to write each call.
- The formatter is the compiler, and it runs on every compile; there is no command that only formats. It needs
  a file to parse, not to compile. It does: 4-space indentation, one space around binary
  operators, the minimum parentheses (a receiver that is an operation always keeps them), `else if` on one line,
  a switch case with one short statement on its own line, a call or a signature wider than 120 columns broken
  one argument per line with trailing commas, floats as written, `: Nothing` dropped from a shape function,
  top-level link comments (the only comments allowed) kept before the declaration they preceded, and the
  blank-line rule for grouped `var`s. **Safety check:** the formatted text must lex, parse, print to the same
  fully-parenthesised program as the original, and keep every comment, or the file is left alone and the reason
  printed. **Every compile formats** the entry folder's files and every `load`-ed root once the whole program is
  read (not `library/`), printing `formatted <path>` for each file it rewrote and reading the program again when one
  was; nothing turns it off (the errors for trying are in
  [compiler.md](compiler.md#formatting-before-compiling)). `crash false` prints as a bare `crash`.
  A file whose function body has an empty line has it deleted, and a run of empty lines outside functions becomes
  one, so compiling fixes it. A file the formatter refuses is left alone with its reason, as an error that stops
  the compile, so a program is never compiled from text that is not in the one style.
  `--final-classes` writes its classes already formatted. A list or object literal over 120 columns is written
  one entry per line with no commas.
- **A file is ordered**: the `singleton` line, `generic` lines, enums, unions, types, variables, the constructor,
  then functions. Anything out of that order is a compile error naming what came before it
  (`diagnostics/declaration_order`). `union` goes beside `enum` and before `type`. Consecutive `generic` lines stay
  together with no blank line between them; one blank line follows the `singleton` line and the last `generic` line.
- One blank line between declarations at file level, except that consecutive `var` declarations may stay
  grouped with no blank line between them if they were already written that way (a blank line the source did
  put between two `var`s is kept; one between any other pair of declarations is always exactly one, inserted
  if missing and collapsed if there were several).
- Inside a function/`if`/`while`/switch-case body: at most one consecutive blank line, and never one right
  after the opening `{`.
- K&R braces: `func name() {`, `if condition {`, `} else {`, `} else if other {`. `else` always continues on
  the closing `}`'s own line, and a nested `if` in an `else` branch collapses onto one `else if` line whenever
  it does not itself carry a comment that would need its own line. An empty body prints as `{ }` on one line
  rather than an empty `{\n}`.
- One space around every binary operator (`a + b`, `a and b`) and after `:`/`,`; no space before `:`/`,`, and
  none just inside `(`/`)`/`[`/`]` (`f(a, b)`, `list[0]`, not `f( a, b )` or `list[ 0 ]`).
- The minimum parentheses needed to preserve meaning are kept (`1 + 2 * 3`, not `1 + (2 * 3)`); parentheses
  that only restated the language's own precedence are dropped, and ones that change it are always kept.
- An inline list/object literal uses `, ` between entries on one line; a multi-line one uses one entry per
  line with no commas (newlines already separate entries;
  [Lexical structure](classes_and_files.md#lexical-structure)). A list/object literal (or a function
  call's argument list) that would be longer than 120 columns on one line is broken into the multi-line form
  instead, one entry per line; short ones stay inline regardless of how the source originally wrote them (one
  true style, not "preserve what you typed"). `enum`/`union`/`type` bodies are always printed multi-line (one
  member per line).
- Comments are `# text`, exactly one space after `#` (a comment with no text is just `#`). String contents are
  never reformatted: whatever is between the quotes is untouched, escapes and all.
- **Lossless.** A leading (own-line) comment before a declaration/statement/list-or-object-entry/enum-value/
  union-member/type-field/switch-case, a trailing same-line comment after one, and whether a blank line
  preceded one are all attached to that AST node by the parser (`ast.Trivia`) and reproduced by the formatter;
  formatting a file never drops or reorders a comment. **Limitation:** a comment is only ever attached at
  those specific points; one written in the middle of an expression, an argument list, or with nothing
  following it before a closing bracket/brace (an "orphan" comment) is not tracked anywhere. The safety check
  below catches this rather than silently losing the comment.
- **Safety check.** After formatting, the formatter re-lexes and re-parses its own output and confirms, before
  ever writing a file: (a) the formatted-and-reparsed AST prints (via the older, comment-blind but
  round-trip-stable canonical printer) to exactly the same text as the original AST does, proving no code was
  added, dropped, or reordered; and (b) every comment's
  text appears, in the same order, in the formatted output. Either check failing refuses to write the file and
  reports an internal formatter error instead.

#### Comments

**A comment is one line, and it is nothing but a link to a markdown section**:

```gdscript
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

#### Naming and abbreviations: compile errors, not auto-fixed

A naming or abbreviation problem cannot be silently rewritten (renaming a symbol can change what a program
means to a reader who searches for its old name), so these are compile **errors** with `file:line:column` and
a suggested fix, from a linter that runs on the parsed AST of every class file, before codegen.
There is no `--no-lint`: the compiler already **is** the linter.

The compiler has no warnings at all: it either reformats your code or gives an
error, and nothing is left to the moron's taste. The `--development`-only "skipped class" notice
([Command line](compiler.md#command-line)) is plain informational `note:` text, not a warning.

- Variables, attributes, functions, and parameters: lowercase `snake_case`. Classes, `type`s, `enum`s, and
  `union`s: `PascalCase` (a class's own name is computed from its file name and therefore always correct; a
  function that happens to share its own class's name, the constructor, is exempt from the function-naming
  check for the same reason). Enum values: lowercase `snake_case`. File and folder names: lowercase
  `snake_case`, every folder including a package's root (`window_plugin`, never `window-plugin`).
- **One underscore between words**: a snake_case name
  joins its words with one `_` each and may start with one `_` to be private, so `hit__count`, `__strike` and
  `strike_` are errors (`diagnostics/doubled_underscore`). The C the compiler writes for a class or function of
  its own (`Pool___allocate`, `Pool___make`, `Pool___retain`, `Pool___release`, a singleton's `___destroy` and
  `___discard`, `Pool___init`,
  `Pool___default`, `Pool___class_of`, `Pool___attributes`, `Pool___functions`, `Pool___instances`,
  `Pool___deep_copy`, `Pool___read_<attribute>`, an enum's `___name`, a function's `___hot`, `___slot`,
  `___waiting`, `___perform`) joins with `___`, which no Spite name can produce, so none of those words is
  reserved: a class may declare `allocate`, `make` or `release` (`conformance/stage6/generated_names`). A class's
  `copy()`, `to_string()` and `to_debug()` are Spite functions every class answers and a class may declare, and
  keep their plain names.
- **No name is taken by C**: `register`, `short`, `default`, `static`, `unsigned`, `stdout`, and the
  words Windows headers define as macros (`near`, `far`, `pascal`, `cdecl`, `interface`), are ordinary names for
  a variable, attribute, parameter or function wherever Spite's own rules allow them; `int`, `char`, `bool`,
  `min` and `max` are still abbreviations. The C backend writes every local, parameter and attribute with a `_`
  after it (`near` is `near_` in the C, `self->near_` for an attribute), and no snake_case name ends in `_`, so
  no Spite name can meet a C keyword, a header's macro, a C library function or a name the compiler makes up.
  A function's C name is always joined to its class's. Reflection, `--final-classes`, crash traces and every
  error keep the Spite name (`conformance/stage6/backend_words`).
- Single-letter names are always errors, with no exceptions, because they read either as a stray leftover or as
  a puzzle for whoever reads the code next.
- **Abbreviations.** An identifier is split into `_`-separated words and each word is checked against a
  denylist, suggesting the identifier with every matching word spelled out in full. Kept in one place
  (one table in the linter) so it is easy to extend:

  | Abbreviation | Full word | | Abbreviation | Full word |
  |---|---|---|---|---|
  | `expr` | `expression` | | `pos` | `position` |
  | `stmt` | `statement` | | `prev` | `previous` |
  | `param` | `parameter` | | `cur`/`curr` | `current` |
  | `params` | `parameters` | | `attrs` | `attributes` |
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

  The rest of the table holds words Win32 spells out in full, so the `'windows'` naming rule
  ([Naming rules](foreign_libraries.md#naming-rules)) reads only the part above backwards:

  | Abbreviation | Full word | | Abbreviation | Full word |
  |---|---|---|---|---|
  | `cnt` | `count` | | `pwd` | `password` |
  | `buf` | `buffer` | | `usr` | `user` |
  | `addr` | `address` | | `repo` | `repository` |
  | `mgr` | `manager` | | `qty` | `quantity` |
  | `evt` | `event` | | `amt` | `amount` |
  | `img` | `image` | | `cb` | `callback` |
  | `hdr` | `header` | | `sz` | `size` |
  | `arr` | `array` | | `lst` | `list` |
  | `vec` | `vector` | | `rect` | `rectangle` |
  | `dict` | `dictionary` | | `dbg` | `debug` |
  | `coord`/`coords` | `coordinate`/`coordinates` | | `ver` | `version` |
  | `opt`/`opts` | `option`/`options` | | `nav` | `navigation` |
  | `ext` | `extension` | | `wnd` | `window` |
  | `seq` | `sequence` | | `ctrl` | `control` |
  | `sep` | `separator` | | `fmt` | `format` |
  | `avg` | `average` | | `proc` | `process` |
  | `tbl` | `table` | | `lang` | `language` |

  Plurals of abbreviations are refused the same way: `nums`, `vals`, `msgs`, `strs`, `ptrs`, `bufs`, `elems`,
  `objs`, `dirs`, `exprs`, `stmts`, `errs`, `cmds`, `funcs`/`fns`, `vars`, `btns`, `imgs`, `evts` and `idxs`
  (`indices`).

  `id` is explicitly **allowed** even though it is short, since it has no ambiguity and no natural longer form.
  The language's own type names get no exemption: `Int`, `Bool` and `UnsignedInt` are spelled `Integer`,
  `Boolean` and `UnsignedInteger`, and the old spellings are errors naming the new ones, `'Int' is spelled
  'Integer'` ([Numeric types](values_and_types.md#numeric-types)).
- The terminal-`if` lint ([The last `if` of a function](failure.md#the-last-if-of-a-function): a function whose
  body's last statement is `if value { ... }`, with no `else`, wrapping the rest of the function only to check
  existence, which must be written with `assert`) runs in this same linter, on the parsed tree.
- A diagnostic names the file the problem is in (`registry.spite:3`), not the entry file.

---

Next: [Memory](memory.md), how values are allocated, counted and released.
