# Review: `while`, `else if` and nested `if`/`else` (D94)

Mortaro asked for this in D94: every `while` checked against the member templates, the `else if` and `while`
cases that cannot be simplified listed with examples, and some simplified ones "to make sure it didnt become
esoteric shit". Open questions 15 and 20 in `manual.md`, items 2 and 3 in `mortaros_missing_decisions.md`.

Everything below is analysis. No code was changed. The rules at the end are **(proposed by Claude,
unconfirmed)**.

## Built (D105)

Mortaro confirmed the findings (D105), so the rewrites below are done. The two proposed rules, the enum switches
and the lookups still wait.

- **(a) loops: 28 of 31 rewritten.** `find_by_` (`class_info` x2, `union_info.has_member`, `generator`
  `find_union_by_c_name`, `find_class_by_qualified`, `find_class_by_c_name`, the enum name at `:5043`,
  `program_discovery` `add_parsed_file` and `environment_file`, `reflection_tests`, `namespace_objects` x2),
  `filter_is_builtin().map_simple_name()`, `map_qualified_dotted()`, `map_get_type()`, `map_name()` then
  `prepend("self")`, `copy()` then `prepend` (six in the generator), `copy()` (`copy_segments`,
  `copied_names`), `copy()` then `remove_last()` (`all_but_last`, `:2102`, `:4926`) and `split`,
  `remove_last()`, `join` (`final_class_directory`). One correction to the text below: `!= null` is a compile
  error (`null` is not a value to compare against), so "is there one" is written `crash list.find_by_x(v)` in a
  test and `var found = list.find_by_x(v)`, `if found { return true }` in `has_member`. A `Symbol` member
  (`Spite.Class.name`, `Spite.Namespace.name`) takes a symbol literal: `find_by_name('Creature')`.
- **Skipped, 3 loops.** `library/read_evaluate_print_loop.spite:412` and `:424` compare a `Symbol` member with
  text typed at the prompt; `find_by_name` wants a `Symbol`, and `Symbol(text)` answers `Symbol?`, so the
  rewrite needs a narrowing step the loop does not. `conformance/stage6/shapes/shapes.spite:23`:
  `things.sum_size()` is not generated for a list of a `type` (`List has no method 'sum_size'`).
- **Union chains: 6 of 8 became a `switch`.** `:5386` and `:5441` (`SpiteType`), `:7143` (`NullableType`,
  `VoidType`, `_:`), `:8784` and `:9061` (`CallExpression`, `GenericCallExpression`; `:9061` joined the
  `switch` already under it), and `:8716`, through a new `names_a_class_or_null(expression)` because a `switch`
  narrows a name, not a path like `binary.right`. Skipped: `:5111` (the caution below) and `:8866`, the same
  trap: `IdentifierExpression` and `MemberExpression` share a body and there is nothing for `_:` to do, and
  two equal cases are only allowed beside a `_:` with a body.
- **Nested `if`/`else`: 6 of 7 flattened.** The two switches above (`:5393`, `:8716`), `narrow_by_guard` at all
  four copies (`:8244`, `:8394`, `:9490`, `:9720`), the `crash`/`assert` text computed once (`:9731`), and
  `one_line_case` in the source printer (`:358`). `:3147` waits for `switch` over an enum.

Line numbers are still those of 122c39f.

## Built (D113)

Templates now take a function of the caller that receives the element and nothing else
(`names.each_say_hello()`), and a `while` written only to do that is an error naming the template (manual
section 8, "A function of the caller for each element"). What that did to the 145 loops of the first (b) row:

- **Rewritten: 2.** `generator.spite` `collect_body_facts` is `statements.each_collect_statement_facts()`, and
  `docs/reflection.md`'s `function_reflection` is `functions.each_describe()`. They are the only two loops in the
  tree that pass *just* the element to a function of the *caller*; the rule found no others. The review's
  estimate of "about 4" also counted `examples/calculator/calculator.spite:8` (`evaluator.process_line(...)`,
  a function of another object) and `emit_functions_function_bodies` (a worklist that grows while it is drained,
  with an attribute as the counter), which the rule rightly leaves alone.
- **Unchanged: 143.** They pass more than the element (`depth`, `scope`, `class_information`, an index), call a
  function of another object, or do more than one thing per element. The (b) row now reads 143, the (b) total
  308, and the grand total 470. How a template could carry `depth` is item 57 in
  `mortaros_missing_decisions.md`.

**How it was measured.** A script walked every `.spite` file in `bootstrap/`, `library/` (with the OS folders),
`scripts/`, `tests/`, `examples/`, `conformance/`, `diagnostics/` and every ` ```spite ` block in `docs/`, at
`master` 122c39f. It extracted each `while`, each `if` chain with an `else if`, and each `if`/`else` sitting
directly in a branch of another `if`/`else`. The script sorted the loops by shape, and every loop placed in (a)
was checked by hand. The (b)/(c) buckets come from the script with spot checks, so they may be a few loops out
each way. Line numbers are from 122c39f and will drift while other sessions rewrite the tree.

---

## Summary

| | Count | Replaceable today | Replaceable with something new | Genuine |
|---|---|---|---|---|
| `while` loops | 470 (472 before D113) | **31** (a) | 308 (b) | 131 (c) |
| `else if` chains (91 `else if`s) | 41 | 8 by a `switch` over a union | 4 by a `switch` over an **enum** (not supported yet), 4 by a lookup | 25 |
| nested `if`/`else` in an `if`/`else` | 16 | 7 flatten or dedupe | | 9 would become a function |

**The main finding for open question 15:** the earlier estimate ("199 of 259 loops are the template shape")
counted loop headers: an index from 0 to `list.count()` reading `list[index]`. About 300 of the 472 loops
have that header. But a member template calls **a zero-argument member of the element**, and almost none of
the bodies do only that. Most of them hand the element to a function of the *caller*, usually with more
arguments (`print_statement(statements[index], depth)`, `generate_statement(statement, scope, class_information,
function_information)`). The templates cannot express that. So:

- **31 loops can be replaced today**, and a compiler rule can safely detect **about 10** of them (the exact
  shapes in the rule below). The other 21 need a small human rewrite (`copy()` then `prepend`, `find_by_` then
  one more step).
- The template addition that would reach the most loops (145) is **a template that takes a named, bound
  function (D17)** instead of a member. But in most of those loops the function needs more arguments than the
  element, and a bound function can't carry them. Making them fit would mean moving `depth` or `scope` into
  fields just so a loop can become a template. That is the esoteric outcome Mortaro warned about, and I
  recommend against it.
- `while` stays for 131 loops that are genuinely about state, 60 of them in the library's containers and text
  code, which the templates themselves are built on (D91, D98).

---

## 1. `while`

### Counts per directory

| Directory | (a) today | (b) with something new | (c) genuine | Total |
|---|---|---|---|---|
| `bootstrap/` | 25 | 243 | 60 | 328 |
| `library/` | 2 | 12 | 60 | 74 |
| `library/windows`, `linux`, `mac` | 0 | 0 | 3 | 3 |
| `scripts/` | 0 | 10 | 1 | 11 |
| `tests/` | 1 | 7 | 2 | 10 |
| `examples/` | 0 | 10 | 2 | 12 |
| `conformance/` | 3 | 19 | 1 | 23 |
| `diagnostics/` | 0 | 3 | 2 | 5 |
| `docs/` code blocks | 0 | 4 | 0 | 4 |
| **Total** | **31** | **308** | **131** | **470** |

### Counts per reason

| Category | Reason | Loops |
|---|---|---|
| (a) | `find_by_<member>(value)` (`name`, `c_name`, `qualified_dotted`) | 14 |
| (a) | `copy()` of the list, then `prepend(...)` | 6 |
| (a) | `copy()`, or `copy()` then `remove_last()` (all but the last element) | 5 |
| (a) | `map_`, `filter_().map_()`, `sum_`, `remove_last()` + `join` | 6 |
| (b) | passes each element to a function of the caller (usually with more arguments); 2 more became `each_` templates (D113) | 143 |
| (b) | needs the index in the body (returns it, prints it, compares it) | 35 |
| (b) | a range of numbers, not a list (`tick < 3`, `index < 1000`) | 28 |
| (b) | stops early (a flag in the condition, or a `return` from a test that is not one member) | 28 |
| (b) | walks two lists at once (parallel lists, pairwise comparison) | 23 |
| (b) | the body has its own loop | 18 |
| (b) | a string's characters | 17 |
| (b) | folds text or numbers the templates cannot (`List<Int>`, `List<String>`, text built through a function) | 7 |
| (b) | walks backwards | 6 |
| (b) | appends onto a list that already has elements (`append_all`) | 2 |
| (b) | all but the last element, doing something per prefix | 1 |
| (c) | the floor: `List`, `Dictionary`, `String`, number text, allocation table, sockets, over `Memory` | 60 |
| (c) | condition over state: lexer, parser, settling, polling, worklists, probing | 50 |
| (c) | the position jumps (a scanner that skips ahead, a queue that grows while drained) | 8 |
| (c) | carries state from one element to the next (brace depth, rank reached so far, insertion order) | 8 |
| (c) | walks backwards and removes as it goes | 4 |
| (c) | the list changes while it is walked | 1 |

A few loops in `diagnostics/` and `conformance/stage1` exist *to test `while`* (narrowing across passes,
`control_flow`). They keep their `while` whatever is decided.

### (a) Replaceable today

**`find_by_`: the clearest case.** `bootstrap/source/analysis/class_info.spite:38`:

```
func find_field(name: String): FieldInfo? {
    var index = 0
    while index < fields.count() {
        var field = fields[index]
        if field.name == name {
            return field
        }
        index = index + 1
    }
    return null
}
```

becomes

```
func find_field(name: String): FieldInfo? {
    return fields.find_by_name(name)
}
```

The same shape: `class_info.spite:52` (`functions.find_by_name(name)`), `generator.spite:1431` and `:1595`
(`find_by_c_name`), `generator.spite:1584` (`find_by_qualified_dotted`).

**`find_by_` followed by one more step.** `bootstrap/source/analysis/union_info.spite:28`:

```
func has_member(class_c_name: String): Bool {
    var index = 0
    while index < members.count() {
        if members[index].c_name == class_c_name {
            return true
        }
        index = index + 1
    }
    return false
}
```

becomes (D69: comparing a `T?` with `null` needs no narrowing)

```
func has_member(class_c_name: String): Bool {
    return members.find_by_c_name(class_c_name) != null
}
```

`tests/reflection_tests.spite:31` goes from a flag, a counter and a loop to one line:

```
func test_every_class_of_the_program_is_listed() {
    var classes = Spite.Class.instances
    crash classes.find_by_name("Creature") != null
}
```

This family also includes `library/read_evaluate_print_loop.spite:412` and `:424`,
`program_discovery.spite:440` and `:541`, `generator.spite:5043`, and
`conformance/stage6/namespace_objects/namespace_objects.spite:28` and `:40`.

**`filter_` then `map_`.** `bootstrap/source/generation/generator.spite:296`:

```
func built_in_class_names(): List<String> {
    var names = List<String>()
    var index = 0
    while index < classes.count() {
        if classes[index].is_builtin {
            names.append(classes[index].simple_name)
        }
        index = index + 1
    }
    return names
}
```

becomes

```
func built_in_class_names(): List<String> {
    return classes.filter_is_builtin().map_simple_name()
}
```

`generator.spite:3604` `parameter_types_of` is `return parameters.map_get_type()`, and `generator.spite:442`
is `written_in_spite = discovery.class_files.map_qualified_dotted()`, since `written_in_spite` is still empty
at that point.

**`copy()` then `prepend`.** Six times in the generator (`:3838`, `:3914`, `:3941`, `:3960`, `:4246`,
`:9344`), e.g. `generator.spite:3911`:

```
var full_arguments = List<String>()
full_arguments.append("self")
var index = 0
while index < argument_codes.count() {
    full_arguments.append(argument_codes[index])
    index = index + 1
}
```

becomes

```
var full_arguments = argument_codes.copy()
full_arguments.prepend("self")
```

`generator.spite:9993` is the same with a template: `var arguments = information.constructor.parameters.map_name()`
then `arguments.prepend("self")`.

**All but the last element.** `bootstrap/spite_compiler.spite:406`:

```
func final_class_directory(path: String): String {
    var segments = path.split("/")
    var result = ""
    var index = 0
    while index < segments.count() - 1 {
        if index > 0 {
            result = "{result}/"
        }
        result = "{result}{segments[index]}"
        index = index + 1
    }
    return result
}
```

becomes

```
func final_class_directory(path: String): String {
    var segments = path.split("/")
    segments.remove_last()
    return segments.join("/")
}
```

`program_discovery.spite:710` `all_but_last` becomes `var result = segments.copy()`, `result.remove_last()`,
`return result`. `generator.spite:2102` and `:4926` are the same shape.

### (b) Replaceable if the templates gained something

For each need: what it is, one real example, and whether I think it is worth adding.

**Passes each element to a function of the caller (145; 143 after D113, which built the 2 that pass just the element).** The template would need to take a function instead
of naming a member. The only form that fits Spite is a named function bound to its instance (D17; D62 rules
out lambdas). `examples/calculator/calculator.spite:8`:

```
var lines = content.lines()
var index = 0
while index < lines.count() {
    evaluator.process_line(lines[index])
    index = index + 1
}
```

would be something like `lines.each(evaluator.process_line)` (the name is Mortaro's to choose). But only
about 4 of the 145 pass *just* the element. The typical one is `bootstrap/source/syntax/printer.spite:27`:

```
func print_statement_list(statements: List<Syntax.Statements.Statement>, depth: Int) {
    var index = 0
    while index < statements.count() {
        print_statement(statements[index], depth)
        index = index + 1
    }
}
```

`depth` has to go along, and a bound function binds an instance, not arguments. Half of these bodies are
longer than three lines and write several outer lists at once. **Not worth it**, in my view: these stay
`while`.

**Needs the index (35).** Mostly "the position of the first match". `generator.spite:898`:

```
func shape_position(c_name: String): Int {
    var index = 0
    while index < unions.count() {
        if unions[index].c_name == c_name {
            return index
        }
        index = index + 1
    }
    return -1
}
```

needs a template like `index_by_<member>(value): Int?` (proposed name). The rest print or compare the index
(`if index > 0 { separator }`), which is `join`'s job where the element is text. **Worth it for the "position
of" case only** (about 10 loops).

**Two lists at once (23).** Almost all are parallel lists: two lists that should be one list of a class.
`generator.spite:6927`:

```
func abbreviated(word: String): String {
    var short_words = abbreviated_words()
    var full_words = unabbreviated_words()
    var position = 0
    while position < full_words.count() {
        if full_words[position] == word {
            crash position < short_words.count()
            return short_words[position]
        }
        position = position + 1
    }
    return word
}
```

Once the data is a `List<Abbreviation>` (`full`, `short`), this is `abbreviations.find_by_full(word)`. So the
thing to add is not a template but the class. `scope.spite` (`entry_names`/`entry_types`/`entry_used`,
`override_names`/`override_codes`) and the foreign-library tables in the generator are the same. The pairwise
loops (`generator.spite:8599`, every case body against every other) stay loops. **Worth doing as a data
change.** It also removes most of the `crash position < other.count()` lines D64 forces today.

**Stops early (28).** Either a flag in the condition or a `return` from a test that is not one member.
`generator.spite:1417` tests two members:

```
if candidate.simple_name == name and candidate.owner_qualified == owner_qualified {
    return candidate
}
```

`bootstrap/spite_compiler.spite:531` tests with a function (the first C compiler that answers `--version`).
Both need a find that takes a test (the bound-function template again). `generator.spite:1402` (return the one
in the current owner, else remember a fallback) is an ordered decision and stays a loop whatever is added.
**Not worth a template.** For the two-member find, a zero-argument member on the element (`qualified_name`) turns it
into (a).

**A string's characters (17).** Escaping (`generator.spite:2953`), snake case to Pascal case
(`spite_compiler.spite:459`, and a copy of it at `program_discovery.spite:645`), JSON escaping in the REPL.
Most carry state between characters (escape, quote, capitalise-next), so they stay loops. The snake case one
is the exception: with templates over `List<String>` (D79 makes `String` a library class) and a zero-argument
`String.capitalized()`, it becomes

```
var words = snake.split("_")
var capitalized_words = words.map_capitalized()
return capitalized_words.join("")
```

**Worth it only through D79**, not as a string-specific iterator.

**Range of numbers (28), walks backwards (6), body with its own loop (18).** `while tick < 3`, the two
1000-key dictionary tests, and the `settle < 4` passes in the generator are counters, not lists. A `Range`
class with templates would be ceremony. The backward walks are all `scope.spite` looking up the innermost
declaration, which becomes a `find_last_by_name` once the parallel lists are one list. **Keep `while`**,
except for `find_last_by_`, which falls out of the parallel-list change.

**Folds the templates cannot express (7), `append_all` (2).** `docs/control_flow.md:69`, the manual's own
example of a `while`, sums a `List<Int>`: `sum_` needs a member of the element, and `Int` has none to name.
`scope.spite:148` and `program_discovery.spite:532` append one list onto another that already has elements:
that is a one-line `List.append_all(other)`, not a template. **`append_all` is worth adding.**

### (c) Genuine loops over state

These can't be simplified, and each for a stated reason:

- **Scanners** (`lexer.spite`, `tree_shaker.spite:154`, `:187`). The position advances by different amounts
  depending on what was just read, and the meaning of a character depends on state (inside a quote, after a
  backslash, inside a comment). `lexer.spite:322`:

  ```
  while not at_end() and not stopped {
      var code = current_code()
      if code == quote_code {
          advance_character()
          terminated = true
          stopped = true
      } else if code == 10 {
          stopped = true
      } else if code == 92 and index + 1 < source.length() {
  ```

- **The parser consuming tokens** (`parser.spite`, 20 loops). There is no list to walk. The loop asks the token
  stream for the next thing until a stop token. `parser.spite:173`:

  ```
  while not finished {
      skip_trivia()
      if check('end_of_file') or check(stop_kind) or has_error {
          finished = true
      } else {
          var parsed_statement = parse_statement()
          statements.append(parsed_statement)
      }
  }
  ```

- **Settling until nothing changes** (`generator.spite:1865` `emit_pending_functions`). Emitting one class can
  discover another, so the pass repeats while anything was emitted. The number of passes is unknown in
  advance.
- **Worklists** (`tree_shaker.spite:21` drains `waiting` while `keep_named_in` pushes onto it), and
  `generator.spite:4765` and `:4909`, whose lists grow while they are being emitted.
- **Hash probing** (`library/dictionary.spite:91`, `allocation_table.spite:120`). Walk slots until an empty
  one. There is no list, just an address sequence.
- **Polling** (`library/read_evaluate_print_loop.spite:48` `while serving`, `:60` `while connected`,
  `spite_compiler.spite:672` `while talking`, `library/console.spite:9`). The loop waits for the outside world.
- **Carrying state across elements** (`program_discovery.spite:168` tracks brace depth token by token,
  `:102` tracks the highest declaration rank reached so far). The answer for one token depends on every token
  before it.
- **The floor** (60 loops in `library/list.spite`, `dictionary.spite`, `string.spite`, `number_text.spite`,
  `allocation_table.spite`, `socket.spite`, the OS `directory.spite` files). These *are* the iterators, or
  the text and memory code under them. Per D91 the `List` templates are written in `library/list.spite` over
  `Memory`, so at least one `while` must exist for them to stand on.

---

## 2. `else if`

41 chains, 91 `else if`s: 34 chains in `bootstrap/`, 4 in `library/`, 1 in `scripts/`, 2 in `diagnostics/`
(the D78 fixtures, which must stay as written).

| Kind | Chains | Where |
|---|---|---|
| `switch` over a union (today) | 8 | `generator.spite:5111`, `:5386`, `:5441`, `:7143`, `:8716`, `:8784`, `:8866`, `:9061` |
| `switch` over an **enum** (not supported: `switch` needs a union today) | 4 | `generator.spite:3147`, `:4679`, `:8709`, `:9429` |
| a lookup (a `Dictionary`, or Symbol codegen) | 4 | `lexer.spite:474`, `generator.spite:2955`, `:5542`, `:6690` |
| a genuine ordered decision | 25 | everything else, see below |

### A `switch` over a union

The `type_shape.as_<kind>(value)` chains test which member of the `Analysis.SpiteType` union a value is.
`generator.spite:5386` (it also contains one of the nested `if`/`else`s of section 3):

```
if type_shape.as_string(field_type) {
    lines.append("{assigned_c} assigned = SpiteString_retain(text);")
} else if type_shape.as_scalar(field_type) {
    var string_type = string_type()
    lines.append("{assigned_c} assigned = {cast("text", string_type, field_type)};")
} else {
    var enum_type = type_shape.as_enum(field_type)
    if enum_type {
        lines.append("{assigned_c} assigned;")
        var enum_from_text_lines = enum_from_text_lines(enum_type, "text", "assigned", "return false;")
        lines.append(enum_from_text_lines)
    } else {
        return ""
    }
}
```

becomes

```
switch field_type {
    Analysis.Types.StringType: lines.append("{assigned_c} assigned = SpiteString_retain(text);")
    Analysis.Types.ScalarType: {
        var string_type = string_type()
        lines.append("{assigned_c} assigned = {cast("text", string_type, field_type)};")
    }
    Analysis.Types.EnumType: {
        lines.append("{assigned_c} assigned;")
        var enum_from_text_lines = enum_from_text_lines(field_type, "text", "assigned", "return false;")
        lines.append(enum_from_text_lines)
    }
    _: return ""
}
```

This needs checking before anyone rewrites it: if `type_shape.as_string` also answers for `SymbolType`, that
member needs its own case.

`generator.spite:8784` does the same over `Syntax.Expressions.Expression` (`CallExpression`,
`GenericCallExpression`, `_:`).

**One caution:** `generator.spite:5111` looks like the same chain, but its `ScalarType` and `EnumType`
branches do the same thing, and no `_:` absorbs it. D60's "a repeated case body is an error" would then force
the shared body into `_:` and give every *other* member (`ClassRefType`, `ListType`, and so on) its own empty
case. That is worse than the chain. Not every union-shaped chain should become a `switch`.

### A `switch` over an enum (needs `switch` to accept an enum)

`generator.spite:3147`, over the `operator` enum:

```
if operator == 'equal' {
    combine = "SpiteString_equals({left_temporary}, {right_temporary})"
} else if operator == 'not_equal' {
    combine = "(!(SpiteString_equals({left_temporary}, {right_temporary})))"
} else if operator == 'less' {
    combine = "SpiteString_less({left_temporary}, {right_temporary})"
} else if operator == 'greater' {
    combine = "SpiteString_greater({left_temporary}, {right_temporary})"
} else if operator == 'less_equal' {
    combine = "(!(SpiteString_greater({left_temporary}, {right_temporary})))"
} else {
    combine = "(!(SpiteString_less({left_temporary}, {right_temporary})))"
}
```

would read, with symbols as case labels (a language addition, proposed by Claude, unconfirmed):

```
switch operator {
    'equal': combine = "SpiteString_equals({left_temporary}, {right_temporary})"
    'not_equal': combine = "(!(SpiteString_equals({left_temporary}, {right_temporary})))"
    'less': combine = "SpiteString_less({left_temporary}, {right_temporary})"
    'greater': combine = "SpiteString_greater({left_temporary}, {right_temporary})"
    'less_equal': combine = "(!(SpiteString_greater({left_temporary}, {right_temporary})))"
    _: combine = "(!(SpiteString_less({left_temporary}, {right_temporary})))"
}
```

An enum switch would be exhaustive the way a union switch is, so adding an operator would be a compile error
at every switch that forgot it. `generator.spite:4679` switches on `template_prefix` text (`"filter_"`,
`"any_"`, ...), which should itself be an enum. `:8709` needs one case for two values (`'logical_and'` and
`'logical_or'` share a body), which the D60 repeated-body rule would have to allow.

### A lookup

`lexer.spite:474` is a 20-branch chain from a character code to a token kind:

```
if code == 43 {
    kind = 'plus'
} else if code == 45 {
    kind = 'minus'
} else if code == 42 {
    kind = 'star'
```

As a lookup it becomes a `Dictionary<TokenKind>` keyed by the character (`"+"` to `'plus'`, ...), filled once,
then `var kind = single_character_kinds.get(character)`. Three branches still need their own `if` after the
lookup: `/` rejects `//` and `/*`, and `(` and `)` count paren depth. `generator.spite:6690` (the `_as_double`
/ `_as_long` / `_as_text` suffixes) is a three-row table. `generator.spite:5542` (reflection members answered
by the generator) is a Symbol codegen candidate. `generator.spite:2955` (C escapes) is a five-row table plus an
ordered range test.

### Genuine ordered decisions (25)

The tests depend on each other, or are not all about the same value:

- **Scanner states** (`tree_shaker.spite:156`, `:162`, `:189`, `:190`, `lexer.spite:324`, `:341`,
  `read_evaluate_print_loop.spite:347`, `:477`, `spite_compiler.spite:461`, `program_discovery.spite:647`).
  `tree_shaker.spite:156` checks `in_comment` first, then `quote != 0`, then the character itself. The order is
  the meaning: a `"` inside a comment is not a quote.
- **"If A report this, otherwise if B report that"**, where A and B would both fire and only the first is
  wanted. `generator.spite:9211` (`_:` not last, else `_:` answering for nothing),
  `generator.spite:7784` (`as_nullable`, else `not has_error`).
- **Mixed tests** (`generator.spite:1938` is `is_constructor`, else the name is `drop`; `:3209` is a union
  test, else not a class; `:6720` has three union members and two predicates on a `ClassRefType`; `:8231` is a
  constant `true`, else there is an `else`; `:9739`, `:9792`, `parser.spite:374`,
  `read_evaluate_print_loop.spite:211`, `:238`, `docs_corpus.spite:41`, `generator.spite:6474`). Two or three
  branches, each testing something different. A table or a switch would be longer than the chain.
- **`diagnostics/repeated_branch_call`** (2): fixtures for D78, kept as they are on purpose.

---

## 3. Nested `if`/`else` (open question 20)

An `if` with an `else` written directly inside a branch of another `if` with an `else` (or `else if`): **16**,
all in `bootstrap/`.

| Outer | Inner | What removes it |
|---|---|---|
| `generator.spite:3143` | `:3147` | the enum switch above: `'add'` becomes one more case, and the Bool `result_type` is set before the switch with `'add'` overriding it |
| `generator.spite:5386` | `:5393` | the union switch above |
| `generator.spite:8709` | `:8716` | a switch over the right side's union |
| `generator.spite:8376` | `:8394` | extract the guard-narrowing function (below) |
| `generator.spite:9718` | `:9720` | the same function |
| `generator.spite:9718` | `:9731` | D78: both branches call `check_condition` and differ only in the text `"crash"`/`"assert"`, so the text is computed first and the call made once |
| `source_printer.spite:356` | `:358` | D78: the inner `else` repeats the outer `else` |
| `generator.spite:4679` | `:4695` | genuine: `each_` of a `void` member vs a valued one; a function |
| `generator.spite:6720` | `:6721` | genuine: `Bool` passes as `int`; a function |
| `generator.spite:9210` | `:9211` | genuine ordered report; a function |
| `generator.spite:9338` | `:9340` | genuine: a member function found or reported; a function |
| `tree_shaker.spite:156` | `:162` | genuine scanner state; a function (`skip_in_quote`) |
| `tree_shaker.spite:189` | `:190` | the same scanner, again |
| `parser.spite:759` | `:761` | genuine: one dotted segment; a function |
| `parser.spite:903` | `:905` | the same, quiet variant |
| `spite_compiler.spite:675` | `:678` | genuine: the `spite connect` loop; a function |

**Simplified: the same four lines, four times.** `generator.spite:9718`:

```
if guard {
    guard_code = guard.guard_code
    if guard.override_code.is_empty() {
        scope.narrow_path(guard.name)
    } else {
        scope.set_override(guard.name, guard.override_code)
        var guard_inner = guard.get_inner()
        scope.declare(guard.name, guard_inner)
    }
} else {
```

The inner `if`/`else` appears verbatim at `generator.spite:8244`, `:8394`, `:9490` and `:9720`. Extracted:

```
func narrow_by_guard(scope: Scope, guard: NullableGuard) {
    if guard.override_code.is_empty() {
        scope.narrow_path(guard.name)
    } else {
        scope.set_override(guard.name, guard.override_code)
        var guard_inner = guard.get_inner()
        scope.declare(guard.name, guard_inner)
    }
}
```

and each site reads

```
if guard {
    guard_code = guard.guard_code
    narrow_by_guard(scope, guard)
} else {
```

**Simplified: the repeated `else`.** `source_printer.spite:356`:

```
if switch_case.body.count() == 1 {
    var single = source_statement(switch_case.body[0], depth + 1).trim()
    if not single.contains("\n") and not shape.as_if_statement(switch_case.body[0]) and not shape.as_while_statement(switch_case.body[0]) and not shape.as_switch_statement(switch_case.body[0]) {
        text = "{text}\n{label} {single}"
    } else {
        text = "{text}\n{label} {source_block(switch_case.body, depth + 1)}"
    }
} else {
    text = "{text}\n{label} {source_block(switch_case.body, depth + 1)}"
}
```

The two `else`s are the same line. With a function that answers the one-line form, or empty text when there
is none:

```
var one_line_case = one_line_case(switch_case.body, depth)
if one_line_case.is_empty() {
    text = "{text}\n{label} {source_block(switch_case.body, depth + 1)}"
} else {
    text = "{text}\n{label} {one_line_case}"
}
```

**Genuine, but a function would still help.** `tree_shaker.spite:156` puts a quote scanner inside a comment
scanner inside a character loop. With the rule, the `quote != 0` branch becomes a call to
`skip_in_quote(code)`. That is about as readable as today and makes the outer chain one level flatter. The
`spite connect` loop (`spite_compiler.spite:675`) would become `talk(line): Bool`. That works, but it is a
function extracted to satisfy a rule, not one a reader would ask for.

---

## 4. Is the simplified version still readable?

My judgement on each rewrite above, including the ones I would not make:

| Rewrite | Verdict |
|---|---|
| `find_by_` replacing a find loop (`find_field`, `find_class_by_c_name`, ...) | **Better.** Nine lines become one, and the name says what it does. No doubt about it. |
| `find_by_...(x) != null` for "is there one" (`has_member`, the reflection test) | **Better.** A little indirect ("find, then ask if found"). An `any_` that takes a value would read more directly, but it isn't worth adding. |
| `filter_is_builtin().map_simple_name()` | **Better.** Reads as a sentence. Chains of two are the sweet spot; I would stop at three. |
| `copy()` then `prepend("self")` | **Better.** It reads slightly backwards ("self" is written second but ends up first), but five lines become two. |
| `copy()` then `remove_last()` / `split`, `remove_last()`, `join` | **Better** for `final_class_directory`, where it also drops an `if index > 0` separator dance. Neutral for `all_but_last`: a named `all_but_last()` on `List` would be clearer than two mutating calls. |
| `words.map_capitalized().join("")` for snake case (after D79) | **Better**, and it removes a duplicated function. |
| switch over `SpiteType` for `type_shape.as_...` chains | **Better** where each member has its own body. It is exhaustive and says what is being decided. **Worse** where two members share a body (`generator.spite:5111`): D60 then forces the other members into their own cases. |
| enum switch for operator chains | **Better**, if `switch` gains enums. It reads like the chain without the repeated `operator ==`. |
| a `Dictionary` for the lexer's 20 single-character tokens | **Neutral.** The 20 branches become about 18 table lines plus 3 remaining `if`s. The table is easier to scan, but the reader now has to find where it is filled. I would leave this to taste. |
| extracting `narrow_by_guard` | **Better.** It removes four copies. This is what the nested rule is for. |
| a template taking a bound function (`lines.each(evaluator.process_line)`) | **Fine where it fits, esoteric where it doesn't.** It fits 4 loops. Forcing the other 141 in would mean moving `depth`, `scope` and friends into fields so a loop can become a template. **Don't.** |
| a `Range` class so counting loops become templates | **Esoteric.** `while tick < 3` is already clear. **Don't.** |
| turning scanners (lexer, tree shaker, escapes) into iterator chains | **Esoteric.** The state is the point. **Don't.** |
| extracting a function for every genuine nested decision (`talk(line)`, `skip_in_quote`) | **Neutral to slightly better.** Each extracted function has a real name, but a few exist only because of the rule. |

---

## 5. Proposed rules (proposed by Claude, unconfirmed)

### The `while` rule (D94's "if its obvious make it a compiler rule")

**Shape.** An error when all of these hold:

1. The condition is exactly `<counter> < <list>.count()`. `<list>` is a name or an attribute path whose type
   is `List<T>` with `T` a class or a `type`, and `<counter>` is a local `Int` whose last assignment before
   the loop is `= 0`.
2. The body's last statement is `<counter> = <counter> + 1`, and it is the only write to `<counter>` in the
   loop. The body reads `<counter>` only as `<list>[<counter>]` (directly, or through one
   `var <item> = <list>[<counter>]` as the first statement). `<list>` is not written in the body. `<counter>`
   is not read after the loop.
3. What remains of the body, with `m` a zero-argument member of `T` (a field or a function, D15), is exactly
   one of these:

| Body | Suggested replacement |
|---|---|
| `<item>.m()` | `<list>.each_m()` |
| `<result>.append(<item>.m)`, `<result>` declared `List<U>()` just before and empty | `var <result> = <list>.map_m()` |
| `var <value> = <item>.m()` then `<result>.append(<value>)` (the D77 hoisted form) | `var <result> = <list>.map_m()` |
| `if <item>.m { <result>.append(<item>) }` | `var <result> = <list>.filter_m()` |
| `if <item>.m { <result>.append(<item>.n) }` | `var <result> = <list>.filter_m().map_n()` |
| `if <item>.m { <total> = <total> + 1 }` | `var <total> = <list>.count_m()` |
| `<total> = <total> + <item>.m`, `<total>` starting at `0` | `var <total> = <list>.sum_m()` |
| `if <item>.m == <value> { return <item> }`, the loop followed by `return null` | `return <list>.find_by_m(<value>)` |
| `if <item>.m { return true }`, the loop followed by `return false` | `return <list>.any_m()` |
| `if not <item>.m { return false }`, the loop followed by `return true` | `return <list>.all_m()` |
| `<result>.append(<item>)` into an empty `<result>` | `var <result> = <list>.copy()` |

`<value>` must not mention `<counter>` or `<item>`.

**Error message.** It names the loop's purpose and gives the exact replacement:

```
this 'while' walks every element of 'fields' only to find the first whose 'name' equals 'name': write 'return fields.find_by_name(name)' (member templates, section 8)
```

The purpose clause follows the row: "to call 'm' on each", "to collect each 'm'", "to keep the elements whose
'm' is true", "to count the elements whose 'm' is true", "to add up 'm'", "to find the first whose 'm'
equals ...", "to ask whether any element's 'm' is true", "to ask whether every element's 'm' is true", "to
copy it".

**What it catches today: about 10 loops**: `class_info.spite:40`, `:52`; `generator.spite:1431`, `:1584`,
`:1595`, `:299`, `:3607`; `program_discovery.spite:361`; `generator.spite:8691`;
`conformance/stage6/shapes/shapes.spite:23`. The other 21 (a)-loops need one step the rule can't prove
(`prepend`, `remove_last`, a `!= null`, a flag), and stay a matter of review.

**An open choice for Mortaro:** section 12 says the compiler either reformats or errors. Every row above is a
mechanical rewrite that deletes the counter and the result declaration, so it could be applied by the
formatter instead of reported. I propose an error first, because `find_by_` compares with `==` and the loop
may have compared differently in ways the rule should be sure of before rewriting silently.

**Worth pairing with it:** `List.append_all(other)`, which removes 2 loops, and turning the parallel lists in
`scope.spite` and the generator's foreign tables into lists of a class. That turns about 25 loops into
`find_by_` / `find_last_by_` and removes most `crash index < other.count()` lines (D64).

### The nested `if`/`else` rule (open question 20)

**Shape.** An error when an `if` statement that has an `else` or `else if` stands directly among the statements
of any branch of another `if` that has an `else` or `else if`. Not counted:

- an `if` without an `else` (it is a guard, and D54 or `assert` already handle the common one);
- an `if`/`else` inside a `while` or `switch` inside the branch (the loop or the switch is the unit);
- an `else if` chain, which is flat, not nested.

**Error message:**

```
this 'if'/'else' is inside a branch of another 'if'/'else': move it into a function named for what it decides, or, when both test which member of a union or enum a value is, use one 'switch'
```

When the inner decision is the same text as another nested one elsewhere in the file (the guard narrowing
above), the message could add "the same decision is at <line>", which covers D78's "nothing said twice"
across functions.

**What it catches today: 16**, all in `bootstrap/`. Seven become flatter (three switches, two
`narrow_by_guard` calls, two D78 duplicates). Nine become a named function, most of them reasonable
(`skip_in_quote`, `parse_dotted_segment`) and one or two that exist only for the rule (`talk(line)` in
`spite connect`). The switch half of the message depends on `switch` accepting an enum; until then it should
only suggest a switch when both tests are about a union's members.
