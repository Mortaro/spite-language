# Style: the compiler is the formatter and the linter

Style in Spite is not a matter of taste, and there is no style guide to follow: the compiler either rewrites your
code into the one style or refuses it with an error that names the fix. It has no warnings and no switch to turn
a rule off. What cannot be rewritten safely -- a name, a structure -- is an error; everything else is rewritten.

## What the formatter rewrites

Every compile formats the program's own files first, printing `formatted <path>` for each one it changed
(`--format=false` skips it, and `spite format` runs it alone -- [compiler.md](compiler.md#formatting)):

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

```spite title=declaration_order_error/declaration_order_error.spite entry error
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
- **Names say what they do.** The standard library follows the same rule: `append` and `prepend` rather than
  `add`, `upper_case()` rather than `upper()`, `remove_last()` rather than `pop()`.

```spite title=lint_error/lint_error.spite entry error
var console = Console()
var msg = ""

func LintError() {
    console.print(msg)
}
```
```diagnostic
'msg' abbreviates: write 'message' instead of 'msg'
```

```spite title=single_letter/single_letter.spite entry error
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

A local variable or parameter that is never read is an error: remove it, or start its name with `_` to say that
is intended. A `_name` that *is* read is an error too, since the prefix says it is not. Parameters whose shape
is dictated from outside -- an operator function's, a Symbol codegen template's, a setter answering
`person.age = 1`, a function replacing one in a reopened class -- are exempt. A name starting with `_` is also
private to its class ([reflection.md](reflection.md#reflection-is-read-only)).

```spite title=unused_error/unused_error.spite entry error
var console = Console()

func UnusedError() {
    var forgotten = 3
    console.print("hello")
}
```
```diagnostic
'forgotten' is never used: remove it, or name it '_forgotten' to say that is intended
```

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
a name, unlike a comment, is visible to reflection, `--final_classes` and every tool.

```spite title=prose_comment_error/prose_comment_error.spite entry error
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

```spite title=blank_line_error/blank_line_error.spite entry error
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

```spite title=call_argument_error/call_argument_error.spite entry error
var console = Console()

func CallArgumentError() {
    var source = "spite is small"
    console.print(source.slice(0, 5))
}
```
```diagnostic
is called inside an argument of 'console.print': compute it first into a named 'var' and pass the name
```

```spite title=call_argument_fixed/token.spite
var kind = ""
var text = ""

func Token(new_kind: String, new_text: String) {
    kind = new_kind
    text = new_text
}
```
```spite title=call_argument_fixed/call_argument_fixed.spite entry
var console = Console()

func CallArgumentFixed() {
    var source = "spite is small"
    var first_word = source.slice(0, 5)
    console.print(first_word)
    var tokens = List<Token>()
    tokens.append(Token("word", first_word))
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
  `tokens.append(Token("word", first_word))` is legal; a constructor inside that constructor is not.
- Anything *inside* an argument counts: `counts.append(count_words(text) + 1)` puts a call inside an argument.
- A method called on a call's result is not an argument: `source.split(" ").count()` is fine on its own line.
- The holes of a text are not arguments, and each is read like a line of its own:
  `"{word_count} words"` is fine, and so is `"{names.count()} names"`.
- A call in an `if` or `while` condition, a `return`, an assignment or an index is not an argument.

**An `if` and its `else` do not repeat the same work.** When both branches compute the same call, it is
computed once before the `if`:

```spite title=repeated_branch_error/repeated_branch_error.spite entry error
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
| `enum Job = { 'knight', 'mage' }` | one entry per line, no `=`, no commas | [values_and_types.md](values_and_types.md#enums) |

The reason is the same everywhere: Spite is written mostly by AI and read by people, and a long way round that the
compiler accepts is a pattern that spreads.
