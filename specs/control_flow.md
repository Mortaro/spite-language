# Control flow

The specification of [Control flow](../docs/control_flow.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Control flow in full

```gdscript
if condition { } else { }
if nullable_value { } else { }     # runs with nullable_value unwrapped in place when it is not null; else when it is
while condition { }
switch enemy {
    Player: enemy.hurt()
    Monster: { enemy.die() }
}
```

An `if` on a `T?` narrows the value in place ([Null safety and `assert` narrowing](../docs/failure.md#narrowing)): the block runs with `value` as a plain `T`,
and the `else` runs exactly when it is null/absent: one rule for narrowing everywhere, `assert`/`if`/`switch`
alike. `do` is not a keyword ([Lexical structure](classes_and_files.md#lexical-structure)). The terminal-`if` lint
([The last `if` of a function](../docs/failure.md#the-last-if-of-a-function)) applies only when the `if` has no `else`:
one with an `else` already handles the missing case explicitly, so there is nothing left to rewrite with
`assert`. `else if` is written on one line; the formatter joins an `else { if ... }` into it.

**A statement ends with its line.** Anything left on the line
after a statement is a parse error at the first word left over, so nothing written there is silently dropped or
read as a second statement: `return every attribute at its default` is `'attribute' is left over after the end of
the statement: a statement ends with its line, so remove it, or put it on a line of its own if it is a statement`
(`diagnostics/left_over_after_return`). An `if`, `while` or `switch` ends with the `}` that closes it (after its
`else` chain, for an `if`), and a word after that `}` on the same line is `'console' is left over after the '}'
that closes this statement: ...` (`diagnostics/left_over_after_block`).

**A local declared only to be returned is a compile error.** A `var name = value` or `var name: T = value`,
with `T` written as the function's result type, followed at once by `return name`, is `'name' is declared only to
be returned on the next line: write 'return value', since 'return' already makes the value the function's T`
(`diagnostics/returned_local`). A local declared with any other type is a conversion of its own and is not
refused.

**A `while true` that can never leave and calls nothing is a compile error.** No `return`, `assert` or `crash`
anywhere inside its body, and no call (a function, a method, a constructor) in it: `this 'while true' never ends:
nothing inside it returns, asserts or crashes, and it calls nothing, so the program would spin here for good: leave
it with a 'return' ('if done { return }'), or loop 'while' a condition` (`diagnostics/spinning_loop`). One that
calls something is left alone, since a call may end the program, except in work a `Parallel` runs
([concurrency.md](../docs/concurrency.md#the-thread-pool)), and inside a locked singleton function
([concurrency.md](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting)).

There is deliberately no `break`/`continue`: `while` is the only loop construct Spite has, full stop.
Neither is a keyword, but either written as a statement of its own is an error naming the form (`Spite has no
'break': a loop stops in its own condition, like 'while index < count and not found', ...`,
`diagnostics/old_break`), and both stay usable as names. An
early exit says so in the loop's own condition: a flag (`var stopped = false` ... `while not stopped { ... }`)
or the bound itself (`while index < words.count() and words[index] != "stop"`).

`while` is the only loop. There is no `for`, so that people favor metaprogramming: reach for `List<T>`/`Dictionary<Key, Value>` metaprogramming
([Member templates](../docs/collections.md#member-templates-loops-you-do-not-write),
[Passing a function for each element](../docs/collections.md#passing-a-function-for-each-element)) first, and index with
`while index < list.count() { }` when a loop is genuinely needed. Writing `for` is a parse error rather than
silently doing something else: "Spite only has 'while' loops; there is no 'for'. Use List<T>'s metaprogramming
helpers or 'while index < list.count() { ... }' instead." Loops and ifs are otherwise discouraged in application
code.

`while` costs nothing at run time beyond the loop itself, with one exception in debugging builds: in a
`--repl-port` or `--hot-reload` build, each pass of a `while` in the program's own code (not `library/` or
`launcher/`) ends with a check point that answers a waiting REPL command or reload
([repl.md](../docs/repl.md#remote---repl-port)); every other build has none. A local `--repl` build gets none either: it
reads its commands from the console only after the entry constructor returns, so a check point there would have
nothing to answer and would cost a call per pass for nothing.

### Nested `if`/`else`

**An `if` with an `else`, directly inside a branch of another `if` with an `else`, is a compile error.**
The message names the fix: "this 'if'/'else' is inside a branch of another 'if'/'else': move it into a function
named for what it decides, or, when both test which member of a union a value is, use one 'switch'"
(`diagnostics/nested_if_else`). An `else if` link counts as a branch of its chain, so an `if`/`else` inside the
body of an `else if` is caught too. Not counted: a flat `else if` chain; an `if` without an `else` inside a
branch; and an `if`/`else` inside a `while` or `switch` inside the branch, since the loop or the switch is the
unit. The message names "a union or an enum", since `switch` takes an enum's values.
Compile time only.

### `switch` over values

A `switch` whose subject is an enum value, a whole number or a `String` takes value cases: an enum literal
(`'red':`), a whole-number literal, negative included (`-1:`), or a text literal without holes (`"es":`), and `_:`
as its last case for the values without one.

- A switch over an enum names only its values; one that leaves a value out without `_:` is "this switch over
  'light' has no case for 'amber': a switch over an enum covers every value, so a value added later cannot be
  forgotten; add the cases, or end with '_:' for the rest", and a `_:` after every value is "every value of
  'Light' already has a case, so '_:' answers for nothing: remove it".
- A switch over a number or a text always ends with `_:` ("this switch over 'number' has no '_:': an Integer has
  more values than a switch can list, ...").
- A value named twice is "this switch has two cases for 'red': keep one"; a value outside the enum is the usual
  "'blue' is not a value of this enum"; a class as a case of such a switch, a name or a call as a case, a value
  case in a switch over a union, and a value case over a `T?` (narrow it first) are errors saying so.
- `_:`, two cases with the same body, and a switch of one case and `_:` that each only return follow the rules of
  union switches ([Unions](values_and_types.md#unions-in-full)): the last one names `return light ==
  'red'`, "this switch only asks whether 'light' is 'red'".
- The subject is evaluated once, into a temporary; the cases are tested in order as an `if`/`else if` chain on
  it, with `==` as it compares that type (a text by its bytes). Nothing is added at run time beyond those tests.
- A `switch` whose every case ends by leaving the function leaves too, so after `if not found { switch ... }`
  whose cases all return, `found` is narrowed ([failure.md](../docs/failure.md#an-if-that-leaves-proves-the-rest)).
- `conformance/stage6/value_switch`, `diagnostics/value_switch`.

### A chain of `if`s on one value is a `switch`

Three or more `if`s in a row, each with no `else`, whose condition is `subject == constant` and whose
whole block is one `return`, with the same `subject` (a name or a member path, no call) and a constant that is
an enum literal, a whole-number literal (negative included) or a text literal, are a compile error at the first
`if`. So is an `if`/`else if` chain of three or more such links. The message writes the switch out:

```
these 6 'if's each compare 'index' with a value and return, which is what a 'switch' says: write 'switch index { 1: return 'tuesday'  2: return 'wednesday'  ...  _: return 'monday' }'
```

`_:` answers with the `return` that follows the chain (or the final `else`), and is `_: ...` when something else
follows; a chain that covers every value of an enum gets no `_:`. Two such `if`s, a comparison with anything but
a constant, and an `if` doing more than return are left alone. Compile time only (`diagnostics/switch_chain`).

### A `while` that a member template already says

**A `while` that only walks every element of a list, doing what a member template does, is a compile error naming
the template**; `while` stays for loops over state. The shape is exact: the statement before the loop is `var counter = 0`; the condition is
`counter < list.count()` with `list` a name or a path of type `List<T>`, `Vector<T>` or `Items<T>`; the last statement is
`counter = counter + 1`; the counter is read nowhere else in the body and not at all after the loop; and the
rest of the body reads `list[counter]` (directly, or through one `var item = list[counter]` first) and is
exactly one of these, with `m` and `n` members of `T` that are not private (an attribute or a function taking
nothing), which needs `T` to be a class, or with `f` a function that takes the element as its only argument,
which fits a list of anything, numbers and `String` included:

| Body | The error names |
|---|---|
| `item.m()` | `list.each_m()` |
| `result.append(item.m)`, or `var value = item.m` then `result.append(value)` | `var result = list.map_ms()` |
| `result.append(item)` | `var result = list.copy()` |
| `if item.m { result.append(item) }` | `var result = list.filter_m()` |
| `if item.m { result.append(item.n) }` | `var result = list.filter_m().map_ns()` |
| `if item.m { total = total + 1 }` | `var total = list.count_m()` |
| `total = total + item.m` | `var total = list.sum_m()` |
| `if item.m == value { return item }`, the loop followed by `return null` | `return list.find_by_m(value)` |
| `if item.m { return true }`, the loop followed by `return false` | `return list.any_m()` |
| `if not item.m { return false }`, the loop followed by `return true` | `return list.all_m()` |
| `f(item)` | `list.each(f)` |
| `if f(item) { result.append(item) }` | `var result = list.filter(f)` |
| `if f(item) { total = total + 1 }` | `var total = list.count(f)` |
| `total = total + f(item)` | `var total = list.sum(f)` |
| `if f(item) { return item }`, the loop followed by `return null` | `return list.find(f)` |
| `if f(item) { return true }`, the loop followed by `return false` | `return list.any(f)` |
| `if not f(item) { return false }`, the loop followed by `return true` | `return list.all(f)` |

On a `Vector<T>` or an `Items<T>` the rows that name a member template are the rule, and the rows that pass a
function or append the element itself (`copy()`, `filter_m()`) are not: a `Vector` and an `Items` take no passed
function, and an item appended to a list is the error for keeping a borrowed item instead
(`diagnostics/vector_template_walk`).

`result` must be a local declared `List<...>()` earlier in the same block and `total` one declared `0`, neither
mentioned between its declaration and the loop, and `value` may not mention the counter or the element. The
message reads `this 'while' walks every element of 'items' only to add up 'price': write 'var total =
items.sum_price()'` (`diagnostics/template_walk`). `f` is a function of the class (`say_hello`), a function of an
attribute or a local (`evaluator.process_line`), or a local or attribute holding a function value; it
takes exactly one argument, and where the loop tests it (`filter`, `count`, `find`, `any`, `all`) it returns
`Boolean` (a function answering a `T?` there is a presence test, which no passed-function form says, so that loop
stays). The message names it: `this 'while' walks every element of 'names' only to call 'say_hello' with each:
write 'names.each(say_hello)'` (`diagnostics/walk_with_function`). Loops that pass extra arguments, need the index, walk
two lists, scan text, stop early any other way or walk state are not touched. Compile time only.

---

Next: [Style: the compiler is the formatter and the linter](style.md).
