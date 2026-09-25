# Style: the compiler is the formatter and the linter

Style in Spite is not a matter of taste, and there is no style guide to follow: the compiler either rewrites your
code into the one style or refuses it with an error that names the fix. It has no warnings and no switch to turn
a rule off. What cannot be rewritten safely -- a name, a structure -- is an error; everything else is rewritten.

## What the formatter rewrites

Every compile formats the program's own files, printing `formatted <path>` for each one it changed, and then reads
the program again, so what it compiles is the formatted file (`--format=false` skips it, and `spite format` runs
it alone on files -- [compiler.md](compiler.md#formatting)):

- 4 spaces per level, never a tab; no trailing whitespace; one line break at the end of the file.
- K&R braces: `func name() {`, `} else {`, `} else if other {`. An empty body is `{ }`.
- One space around binary operators and after `,` and `:`; none inside `( )` and `[ ]`.
- The fewest parentheses that keep the meaning: `1 + 2 * 3`, not `1 + (2 * 3)`.
- A list, object or call that fits in 120 columns goes on one line with `, `; a longer one is broken one entry
  per line -- with no commas for a list or object literal, and with trailing commas for a call.
- One blank line between declarations at file level; consecutive `var`s may stay grouped.
- A switch case with one short statement stays on its line; a bare `crash` becomes `crash false`.

It never reorders code or drops a comment: it re-reads its own output and checks that the program and every
comment survived, and if it cannot prove that, it leaves the file alone and says why.

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

- `snake_case` for variables, attributes, parameters, functions, enum values, files and folders; `PascalCase`
  for classes, enums, unions and types.
- **Never a single letter.** No exceptions.
- **Never an abbreviation.** Every word of a name is checked against a list (`msg`, `idx`, `val`, `cfg`, `tmp`,
  `str`, `len`, `max`, `min`, `init`, `env`, `dir`, `doc`, ... -- the full table is in
  [manual section 12](../manual.md#naming-and-abbreviations----compile-errors-not-auto-fixed--implemented)),
  and the error spells the name out. `id` is allowed.
- **Not a word C reserves**: a name such as `unsigned`, `static` or `stdout` means something else in the C a
  program compiles to, so it is an error asking for a name of your own.
- **One underscore between words**, and at most one in front to make a name private: `hit__count`, `__strike`
  and `strike_` are errors. That is what keeps the C the compiler makes for a class -- how it allocates, counts
  and releases one, under names with `___` in them -- out of the way of yours, so `allocate`, `make`, `release`
  or `class_of` are ordinary function names.
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

A local variable that is never read is an error: remove it. Only reading counts (D136), so a variable that is
assigned and never read is unused too, and there is no spelling that silences it (D137): a variable nobody reads
is work the program did for nothing. A parameter is the one exception, because a signature can need a parameter
its body ignores: name it `_name` to say so. A `_name` that *is* read is an error, since the prefix says it is
not. Parameters whose shape is dictated from outside -- an operator function's, a Symbol codegen template's, a
setter answering `person.age = 1`, a function replacing one in a reopened class -- are exempt. Everywhere else a
leading `_` means private to its class ([reflection.md](reflection.md#reflection-is-read-only)).

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

An attribute nothing reads is an error the same way (D118): remove it. A private `_name` attribute is no
exception, since only its own class can read it. Writing an attribute is not reading it, so one that is only
assigned is still unused. A read is anything that takes the attribute's value: its name in one of the class's own functions,
`thing.world` from another class, a getter answering that read, `x.attributes[attribute]` in a Symbol template
(which is how `Json` and `to_debug()` read), `thing.attributes`, and the REPL in a build that has one. A template
that only looks at `attribute.name` or `attribute.class` reads the attribute's description, not the attribute, so
an attribute kept only as a marker for such a walk is an error:

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
    var lines = List<String>()
    classify_attributes(mover, lines)
    var joined = lines.join(", ")
    console.print(joined)
}

func classify_attribute(attribute: Symbol<Mover>, classified: Mover, lines: List<String>) {
    lines.append("{attribute.name} is a {attribute.class}")
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
(D136; which folders count is proposed by Claude, unconfirmed).

Unused means unread anywhere in the source, not unreachable: a function nothing calls still reads what it names,
and the compiler removes it later ([compiler.md](compiler.md#development-builds-and-tree-shaking)). A few
attributes are never checked, because the compiler reads them itself: a number class's or `String`'s storage,
and `Build` and `Environment` settings.

## Comments are links

A comment is one line, outside functions and declaration bodies, and it holds nothing but a link to a heading in
a markdown file:

```
# notes.md#why-this-program-exists
func Game() {
```

The path is resolved from the entry file's folder, and the compiler checks that the file exists and has that
heading, so a link cannot rot silently. Prose in a comment, `//` and `/* */` are errors, and each one asks
whether the note is needed at all: if a reader could work it out from the code, delete it; if it is lasting
knowledge, write the section and link to it; if it warns against a change, a test or a compile error pushes
harder than prose. A line inside a function that seems to need explaining becomes a named function instead --
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
name. Name it and call it. Blank lines stay legal between declarations.

```gdscript title=blank_line_error/blank_line_error.spite entry error
var console = Console()

func BlankLineError() {
    console.print("setup")

    console.print("the part that wants a name")
}
```
```diagnostic
a function body holds no empty lines
```

## One call per line

**Only a constructor call may be an argument, and only one level deep.** Anything else is computed first into
a named `var`, and the name is passed:

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
    tokens.append(Token(first_word))
    var word_count = source.split(" ").count()
    var first_token = tokens.first()
    console.print("{word_count} words, first token {first_token.text}")
}
```
```output
spite
3 words, first token spite
```

What counts, precisely:

- A constructor is a call whose last name starts with an upper-case letter (`Token(...)`, `List<String>()`).
  `tokens.append(Token(first_word))` is legal; a constructor inside that constructor is not.
- Anything *inside* an argument counts: `counts.append(count_words(text) + 1)` puts a call inside an argument.
- A method called on a call's result is not an argument: `source.split(" ").count()` is fine on its own line.
- The holes of a text are not arguments, and each is read like a line of its own:
  `"{word_count} words"` is fine, and so is `"{names.count()} names"`.
- A call in an `if` or `while` condition, a `return`, an assignment or an index is not an argument.
- A singleton's constructor is the exception: `greet(Console())` is an error, because a singleton is always bound
  to a `var` first ([classes_and_files.md](classes_and_files.md#singletons)).

**A long line makes no object inside a call** (D187). There is no limit on how long a line may be. But if a line
would be wider than the formatter's 120 columns when written on one line, it may not construct an object inside a
call's arguments. Make the object first, on a line of its own, and pass its name. A short line may still pass one
constructor, as above.

```gdscript title=long_line_error/label.spite
var text = ""

func Label(starting_text: String) {
    text = starting_text
}
```
```gdscript title=long_line_error/long_line_error.spite entry error
var console = Console()

func LongLineError() {
    var labels = List<Label>()
    labels.append(Label("a label whose text is long enough to carry this line past the width that the formatter uses for lines"))
    var count = labels.count()
    console.print(count)
}
```
```diagnostic
a long line makes no object inside a call, so make it first on a line of its own, 'var label = Label(
```

The line is measured as the formatter would print it on one line, indentation included. Breaking it over several
lines does not change the result. The fix is `var label = Label("...")` followed by `labels.append(label)`.

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
| `if not ready { return }`, `if handle == -1 { return false }` | `assert ready`, `assert handle != -1` | [failure.md](failure.md#an-if-that-only-returns-the-default-is-an-assert) |
| a last `if value { ... }` with no `else` | `assert value` | [failure.md](failure.md#the-last-if-of-a-function) |
| `var watcher = tracker` then `assert watcher` | `assert tracker` | [failure.md](failure.md#narrowing-a-path) |
| `assert tracker` on a value that cannot be null | removing it | [failure.md](failure.md#narrowing-a-path) |
| a switch that is one class case and `_:`, each a `return` | `if value == Class` or `return value == Class` | [control_flow.md](control_flow.md#value--class) |
| two switch cases with the same body | `_:` | [control_flow.md](control_flow.md#switch-over-a-union) |
| `"hello " + name` | `"hello {name}"` | [values_and_types.md](values_and_types.md#string) |
| `func Holder() { }` | deleting it: a class without a constructor is made from its defaults | [classes_and_files.md](classes_and_files.md#constructors) |
| `this.name` | `name` | [classes_and_files.md](classes_and_files.md#this) |
| `Console().print(value)`, `Build().program`, `greet(Console())` | `var console = Console()` beside the attributes, then `console.print(value)` | [classes_and_files.md](classes_and_files.md#singletons) |
| `enum Job = { 'knight', 'mage' }` | one entry per line, no `=`, no commas | [values_and_types.md](values_and_types.md#enums) |
| a `while` whose body only passes each element of `names` to `say_hello` | `names.each_say_hello()` | [collections.md](collections.md#a-function-of-yours-for-each-element) |

The reason is the same everywhere: Spite is written mostly by AI and read by people, and a long way round that the
compiler accepts is a pattern that spreads.
