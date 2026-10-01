# Control flow

```gdscript
if condition { } else if other_condition { } else { }
if nullable_value { } else { }     # nullable_value is its plain type inside the first block
while condition { }
switch enemy {
    Player: enemy.hurt()
    Monster: { enemy.die() }
}
```

That is all of it: `if`, `while` and `switch` (over a union, a `T?`, an enum, a whole number or a text), plus
`assert` and `crash` ([failure.md](failure.md)). There is no
`for`, no `break` or `continue`, no ternary `? :`, and no `do`. Before reaching for a loop at all, check whether a
[member template](collections.md#member-templates-loops-you-do-not-write) already says it:
`items.filter_in_stock().sum_price()` reads better than the loop it replaces, and compiles to the same loop.

## `if`

Conditions are `Boolean`s, joined with `and`, `or` and `not`, which short-circuit. An `if` on a `T?` narrows it in
place: inside the block it is a plain value, and the `else` runs exactly when it is null
([failure.md](failure.md#narrowing)). `else if` is written on one line; the formatter joins an `else { if }` into
it.

```gdscript title=if_chains/if_chains.spite entry
var console = Console()

func IfChains() {
    var grades = [95, 72, 40]
    grades.each(show_grade)
}

func show_grade(grade: Integer) {
    var letter = letter_for(grade)
    console.print(grade, letter)
}

func letter_for(grade: Integer): String {
    if grade >= 90 {
        return "A"
    } else if grade >= 70 {
        return "B"
    }
    return "C"
}
```
```output
95 A
72 B
40 C
```

An `if` that only returns `null` from a function returning a `T?`, or only returns from one returning nothing, is
an `assert` spelled the long way, and is an error naming it
([failure.md](failure.md#an-if-that-only-returns-the-default-is-an-assert)). In a function returning a number, a
`Boolean`, a text or an object, `if handle == -1 { return false }` is the answer written down, since `assert` may
not answer a default there. An `if` with no `else` that ends by returning proves the opposite of its condition
for the rest of the block, so `if not found { return -1 }` narrows `found`
([failure.md](failure.md#an-if-that-leaves-proves-the-rest)).

**An `if`/`else` directly inside a branch of another `if`/`else` is an error.**
Two stacked decisions are two things to hold in your head at once, so the inner one gets a name: move it into a
function named for what it decides, or, when both test which member of a union or which value of an enum a value
is, use one `switch`. A flat
`else if` chain is fine, and so is an `if` with no `else` inside a branch, or an `if`/`else` inside a `while` or a
`switch` inside the branch, since the loop or the switch is the unit.

```gdscript title=nested_decision_error/nested_decision_error.spite entry error
var console = Console()

func NestedDecisionError() {
    var label = label_for(3, true)
    console.print(label)
}

func label_for(count: Integer, loud: Boolean): String {
    if count == 0 {
        return "none"
    } else {
        var noun = "{count} apples"
        if loud {
            return "{noun}!"
        } else {
            return noun
        }
    }
}
```
```diagnostic
this 'if'/'else' is inside a branch of another 'if'/'else': move it into a function named for what it decides
```

The inner decision is how loudly to say it, so that is the function:

```gdscript title=nested_decision/nested_decision.spite entry
var console = Console()

func NestedDecision() {
    var label = label_for(3, true)
    console.print(label)
}

func label_for(count: Integer, loud: Boolean): String {
    if count == 0 {
        return "none"
    } else {
        var noun = "{count} apples"
        return said(noun, loud)
    }
}

func said(text: String, loud: Boolean): String {
    if loud {
        return "{text}!"
    }
    return text
}
```
```output
3 apples!
```

## `while` is the only loop

There is no `for`, so that people favor metaprogramming over hand-written loops. Writing `for` is a
parse error that names the fix instead of silently doing something else:

```gdscript title=for_rejected/for_rejected.spite entry error
var console = Console()

func ForRejected() {
    var numbers = [1, 2, 3]
    for number in numbers {
        console.print(number)
    }
}
```
```diagnostic
Spite only has 'while' loops
```

A loop over a list of objects that only does what a
[member template](collections.md#member-templates-loops-you-do-not-write) does (calling, collecting, keeping,
counting, adding up or finding a member of each element) is an error naming the template
([the exact shape](#a-while-that-a-member-template-already-says)). To call one of your own
functions with each element, pass it: `items.each(restock)`
([Passing a function for each element](collections.md#passing-a-function-for-each-element)).

When you do need to walk a list by hand (because you need the index, walk two lists, or stop early), index
it. The loop's condition `index < names.count()` proves `names[index]` inside the body, so the read needs no
narrowing:

```gdscript title=while_basics/while_basics.spite entry
var console = Console()

func WhileBasics() {
    var names = ["alpha", "beta", "gamma"]
    var index = 0
    while index < names.count() {
        var name = names[index]
        var number = index + 1
        console.print("{number}. {name}")
        index = index + 1
    }
}
```
```output
1. alpha
2. beta
3. gamma
```

There is no `break` and no `continue`. A loop that stops early says so in its own condition, with a flag or with
the bound itself:

```gdscript title=early_exit/early_exit.spite entry
var console = Console()

func EarlyExit() {
    var words = ["alpha", "beta", "stop", "gamma"]
    var index = 0
    while index < words.count() and words[index] != "stop" {
        console.print(words[index])
        index = index + 1
    }
    console.print("stopped at", index)
}
```
```output
alpha
beta
stopped at 2
```

`while value` on a `T?` narrows the body the way `if value` does, which is how a linked chain is walked
([failure.md](failure.md#narrowing-a-path)).

None of this costs anything at run time beyond the branches and loops it compiles to: every rule on this page
is checked while compiling. Only a REPL build adds a check point to each loop
([the rules](#control-flow-in-full) say which builds).

## `switch` over a union

A `switch` is over a union (or a `T?`, which is the union of a type and `Null`) and must cover every member.
Inside each case the value is narrowed to that member. A case is one statement on its line, or a block in `{ }`.

```gdscript title=switch_cases/cat.spite
func sound(): String {
    return "meow"
}
```
```gdscript title=switch_cases/dog.spite
func sound(): String {
    return "woof"
}

func fetch(): String {
    return "fetches the stick"
}
```
```gdscript title=switch_cases/fish.spite
var fins = 2
```
```gdscript title=switch_cases/switch_cases.spite entry
union Creature {
    Cat
    Dog
    Fish
}

var console = Console()

func SwitchCases() {
    var dog = Dog()
    describe(dog)
    var fish = Fish()
    describe(fish)
    var cat = Cat()
    describe(cat)
}

func describe(creature: Creature) {
    switch creature {
        Dog: {
            var trick = creature.fetch()
            console.print("a dog that", trick)
        }
        Fish: console.print("a fish with", creature.fins, "fins")
        _: {
            var sound = creature.sound()
            console.print("something that says", sound)
        }
    }
}
```
```output
a dog that fetches the stick
a fish with 2 fins
something that says meow
```

**`_:` answers for every member without a case of its own.** It is the last case, and it means "this body, written
once for each remaining member": the value is narrowed to each of them in turn, so `creature.sound()` needs
`sound()` only on the members `_` answers for (here `Cat`), not on the whole union. A switch stays exhaustive:
`_` is how it covers the rest, not a way to skip it. Two cases that do the same thing are an error naming `_:`,
and so is a case that does what `_:` already does, or a `_:` that answers for nothing (the exact rules are with
[unions](values_and_types.md#unions-in-full)).

## `switch` over values

A `switch` also takes an enum value, a whole number or a text, and then each case is one value written out: an
enum's `'value'`, a number (`-1` included) or a quoted text. A switch over an enum covers every value, or ends with
`_:` for the rest, so a value added to the enum later cannot be forgotten; a number or a text has more values than
any switch lists, so its switch always ends with `_:`.

```gdscript title=value_switch_doc/value_switch_doc.spite entry
enum Light {
    'red'
    'amber'
    'green'
}

var console = Console()

func ValueSwitchDoc() {
    var wait = seconds_for('amber')
    var next = following('green')
    var word = spoken(2)
    console.print(wait, next, word)
}

func seconds_for(light: Light): Integer {
    switch light {
        'red': return 30
        'amber': return 5
        'green': return 20
    }
}

func following(light: Light): Light {
    switch light {
        'green': return 'amber'
        'amber': return 'red'
        _: return 'green'
    }
}

func spoken(count: Integer): String {
    switch count {
        1: return "one"
        2: return "two"
        _: return "many"
    }
}
```
```output
5 amber two
```

A missing value is the error that makes this worth having:

```gdscript title=missing_value_error/missing_value_error.spite entry error
enum Light {
    'red'
    'amber'
    'green'
}

var console = Console()

func MissingValueError() {
    var wait = seconds_for('red')
    console.print(wait)
}

func seconds_for(light: Light): Integer {
    switch light {
        'red': return 30
        'green': return 20
    }
}
```
```diagnostic
this switch over 'light' has no case for 'amber'
```

**Three `if`s that only compare one value with a constant and return are a `switch`.**
Each is a compile error that shows the switch to write, which says in one place what the
chain says in three:

```gdscript title=if_chain_error/if_chain_error.spite entry error
var console = Console()

func IfChainError() {
    var day = day_name(1)
    console.print(day)
}

func day_name(index: Integer): String {
    if index == 0 {
        return "monday"
    }
    if index == 1 {
        return "tuesday"
    }
    if index == 2 {
        return "wednesday"
    }
    return "later"
}
```
```diagnostic
these 3 'if's each compare 'index' with a value and return, which is what a 'switch' says: write 'switch index { 0: return "monday"  1: return "tuesday"  2: return "wednesday"  _: return "later" }'
```

The same goes for an `else if` chain. Two such `if`s stay as they are.

## `value == Class`

A class name on the right of `==` or `!=` asks what class a value is: `creature == Fish` is true when `creature`
is a `Fish`. A `T?` that is null is no class, so the test is false. Naming a class that cannot be a member of the
value's union is an error, since the answer could only be `false`, and so is testing a value whose own type already
answers (a plain `Cat` tested for `Cat` or `Fish`, or a union already narrowed to `Fish` tested again) outside
a generic class ([proofs.md](proofs.md#proving-what-is-proven-is-an-error)). `if value == Class { }` narrows
the value inside the block, the way a switch case does:

```gdscript title=class_test_doc/cat.spite
func sound(): String {
    return "meow"
}
```
```gdscript title=class_test_doc/fish.spite
var fins = 2
```
```gdscript title=class_test_doc/class_test_doc.spite entry
union Creature {
    Cat
    Fish
}

var console = Console()

func ClassTestDoc() {
    var pet: Creature = Fish()
    var swims = can_swim(pet)
    console.print("swims", swims)
    var fins = fins_of(pet)
    console.print("fins", fins)
    var cat = Cat()
    var cat_fins = fins_of(cat)
    console.print("a cat's fins", cat_fins)
}

func can_swim(creature: Creature): Boolean {
    return creature == Fish
}

func fins_of(creature: Creature): Integer {
    if creature == Fish {
        return creature.fins
    }
    return 0
}
```
```output
swims true
fins 2
a cat's fins 0
```

A generic class is named with its codegen values and no parentheses: `found == Storage<Health>`, or
`found == Storage<$component_type>` inside a generic class. This is how a value read through a `type` is narrowed
back to the generic class it really is. A class that fits the `type` may be tested for before any value of it
has been stored there; one that does not fit is the error that says why.

```gdscript title=generic_class_test_doc/storage.spite
generic $item_type

var items = List<$item_type>()

func count(): Integer {
    return items.count()
}
```
```gdscript title=generic_class_test_doc/generic_class_test_doc.spite entry
type Counted {
    count(): Integer
}

var console = Console()

func GenericClassTestDoc() {
    var words = Storage<String>()
    words.items.append("hello")
    var counted: Counted = words
    var holds_words = counted == Storage<String>
    var holds_numbers = counted == Storage<Integer>
    console.print(holds_words, holds_numbers)
    if counted == Storage<String> {
        var first = counted.items.first()
        crash first
        console.print(first)
    }
}
```
```output
true false
hello
```

Inside a generic class, a codegen value bound to a class tests for that class: `item == $wanted_type`. A value
read through a `type` that accepts anything is narrowed to the bound class, so a function can find the first
value of the class it was made for without a generic function (there are none). A number is boxed when it goes
into a `type`, so `Find<Integer>` finds it too. Where the value's own type already answers (a `Health` tested
against `$wanted_type` bound to `Health`, or a union that does not hold the bound class) the test is decided
while compiling, and it is never the "never true" error, since another binding may make it true. The same goes
for any class test inside a generic class: one that can never be true for one instantiation (`held == Health` in
a `Box<Label>`) folds to `false` and its branch is removed from that copy, while outside a generic it stays the
error. The full rules for class tests are with
[unions](values_and_types.md#unions-in-full).

```gdscript title=codegen_class_test_doc/find.spite
generic $wanted_type

func first(items: List<Anything>): $wanted_type? {
    var index = 0
    var found: $wanted_type? = null
    while index < items.count() {
        var item = items[index]
        if item == $wanted_type {
            found = item
        }
        index = index + 1
    }
    return found
}
```
```gdscript title=codegen_class_test_doc/codegen_class_test_doc.spite entry
var console = Console()

func CodegenClassTestDoc() {
    var items = List<Anything>()
    items.append("hello")
    items.append(42)
    var find_text = Find<String>()
    var text = find_text.first(items)
    crash text
    var find_number = Find<Integer>()
    var number = find_number.first(items)
    crash number
    console.print(text, number + 1)
}
```
```output
hello 43
```

**So a switch that is one early return is an error.** A switch with one class case and `_:`, each a single
`return`, says no more than `if value == Class { return ... }` followed by what `_:` returns, and when both
return `Boolean` literals, it is `return value == Class`. The error names the form to write:

```gdscript title=single_case_switch_error/cat.spite
var lives = 9
```
```gdscript title=single_case_switch_error/fish.spite
var fins = 2
```
```gdscript title=single_case_switch_error/single_case_switch_error.spite entry error
union Creature {
    Cat
    Fish
}

var console = Console()

func SingleCaseSwitchError() {
    var fish = Fish()
    var swims = is_fish(fish)
    console.print(swims)
}

func is_fish(creature: Creature): Boolean {
    switch creature {
        Fish: return true
        _: return false
    }
}
```
```diagnostic
this switch only asks what class 'creature' is: write 'return creature == Fish'
```

Anything that would need an `else` stays a switch, and so does a switch with more cases: `_:` narrows to each
remaining member, which an `else` cannot.

## Nothing after a `return`

A statement written after a `return` in the same block could never run, so it is an error rather than a step
skipped without a word:

```gdscript title=after_return_error/after_return_error.spite entry error
var console = Console()

func AfterReturnError() {
    var doubled = double(4)
    console.print(doubled)
}

func double(value: Integer): Integer {
    return value * 2
    console.print("doubled")
}
```
```diagnostic
this statement comes after a 'return', so it never runs: remove it
```

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. Where the teaching above and these rules
disagree, the rules win.

### Control flow in full

```gdscript
if condition { } else { }
if nullable_value { } else { }     # runs with nullable_value unwrapped in place when it is not null; else when it is
while condition { }
switch enemy {
    Player: enemy.hurt()
    Monster: { enemy.die() }
}
```

An `if` on a `T?` narrows the value in place ([Null safety and `assert` narrowing](failure.md#narrowing)): the block runs with `value` as a plain `T`,
and the `else` runs exactly when it is null/absent: one rule for narrowing everywhere, `assert`/`if`/`switch`
alike. `do` is not a keyword ([Lexical structure](classes_and_files.md#lexical-structure)). The terminal-`if` lint
([The last `if` of a function](failure.md#the-last-if-of-a-function)) applies only when the `if` has no `else`:
one with an `else` already handles the missing case explicitly, so there is nothing left to rewrite with
`assert`. `else if` is written on one line; the formatter joins an `else { if ... }` into it.

**A statement ends with its line.** Anything left on the line
after a statement is a parse error at the first word left over, so nothing written there is silently dropped or
read as a second statement: `return every attribute at its default` is `'attribute' is left over after the end of
the statement: a statement ends with its line, so remove it, or put it on a line of its own if it is a statement`
(`diagnostics/left_over_after_return`). An `if`, `while` or `switch` ends with the `}` that closes it (after its
`else` chain, for an `if`), and a word after that `}` on the same line is `'console' is left over after the '}'
that closes this statement: ...` (`diagnostics/left_over_after_block`).

There is deliberately no `break`/`continue`: `while` is the only loop construct Spite has, full stop.
Neither is a keyword, but either written as a statement of its own is an error naming the form (`Spite has no
'break': a loop stops in its own condition, like 'while index < count and not found', ...`,
`diagnostics/old_break`), and both stay usable as names. An
early exit says so in the loop's own condition: a flag (`var stopped = false` ... `while not stopped { ... }`)
or the bound itself (`while index < words.count() and words[index] != "stop"`).

`while` is the only loop. There is no `for`, so that people favor metaprogramming: reach for `List<T>`/`Dictionary<T>` metaprogramming
([Member templates](collections.md#member-templates-loops-you-do-not-write),
[Passing a function for each element](collections.md#passing-a-function-for-each-element)) first, and index with
`while index < list.count() { }` when a loop is genuinely needed. Writing `for` is a parse error rather than
silently doing something else: "Spite only has 'while' loops; there is no 'for'. Use List<T>'s metaprogramming
helpers or 'while index < list.count() { ... }' instead." Loops and ifs are otherwise discouraged in application
code.

`while` costs nothing at run time beyond the loop itself, with one exception in debugging builds: in a
`--repl-port` or `--hot-reload` build, each pass of a `while` in the program's own code (not `library/` or
`launcher/`) ends with a check point that answers a waiting REPL command or reload
([repl.md](repl.md#remote---repl-port)); every other build has none. A local `--repl` build gets none either: it
reads its commands from the console only after the entry constructor returns, so a check point there would have
nothing to answer and would cost a call per pass for nothing.

#### Nested `if`/`else`

**An `if` with an `else`, directly inside a branch of another `if` with an `else`, is a compile error.**
The message names the fix: "this 'if'/'else' is inside a branch of another 'if'/'else': move it into a function
named for what it decides, or, when both test which member of a union a value is, use one 'switch'"
(`diagnostics/nested_if_else`). An `else if` link counts as a branch of its chain, so an `if`/`else` inside the
body of an `else if` is caught too. Not counted: a flat `else if` chain; an `if` without an `else` inside a
branch; and an `if`/`else` inside a `while` or `switch` inside the branch, since the loop or the switch is the
unit. The message names "a union or an enum", since `switch` takes an enum's values.
Compile time only.

#### `switch` over values

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
  whose cases all return, `found` is narrowed ([failure.md](failure.md#an-if-that-leaves-proves-the-rest)).
- `conformance/stage6/value_switch`, `diagnostics/value_switch`.

#### A chain of `if`s on one value is a `switch`

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

#### A `while` that a member template already says

**A `while` that only walks every element of a list, doing what a member template does, is a compile error naming
the template**; `while` stays for loops over state. The shape is exact: the statement before the loop is `var counter = 0`; the condition is
`counter < list.count()` with `list` a name or a path of type `List<T>`; the last statement is
`counter = counter + 1`; the counter is read nowhere else in the body and not at all after the loop; and the
rest of the body reads `list[counter]` (directly, or through one `var item = list[counter]` first) and is
exactly one of these, with `m` and `n` members of `T` that are not private (an attribute or a function taking
nothing), which needs `T` to be a class, or with `f` a function that takes the element as its only argument,
which fits a list of anything, numbers and `String` included:

| Body | The error names |
|---|---|
| `item.m()` | `list.each_m()` |
| `result.append(item.m)`, or `var value = item.m` then `result.append(value)` | `var result = list.map_m()` |
| `result.append(item)` | `var result = list.copy()` |
| `if item.m { result.append(item) }` | `var result = list.filter_m()` |
| `if item.m { result.append(item.n) }` | `var result = list.filter_m().map_n()` |
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

Next: [Style: the compiler is the formatter and the linter](style.md), how the compiler formats and lints code.
