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
([the exact shape](../specs/control_flow.md#a-while-that-a-member-template-already-says)). To call one of your own
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

A `while true` is left only by a `return` (or an `assert` or `crash` that fails). One with none of them inside it,
that calls nothing either, can only spin until the program is killed, so it is an error: `this 'while true' never
ends: nothing inside it returns, asserts or crashes, and it calls nothing, so the program would spin here for good:
leave it with a 'return' ('if done { return }'), or loop 'while' a condition` (`diagnostics/spinning_loop`). A
`while true` that calls something may still be how a program runs until it exits, so it is left alone, except in
work given to a `Parallel`, where a loop that never ends and never waits is an error
([concurrency.md](concurrency.md#the-thread-pool)).

None of this costs anything at run time beyond the branches and loops it compiles to: every rule on this page
is checked while compiling. Only a REPL build adds a check point to each loop
([the rules](../specs/control_flow.md#control-flow-in-full) say which builds).

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
[unions](../specs/values_and_types.md#unions-in-full)).

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
value of the class it was made for without a generic function (there are none). A number held as a `type`
keeps its class, so `Find<Integer>` finds it too. Where the value's own type already answers (a `Health` tested
against `$wanted_type` bound to `Health`, or a union that does not hold the bound class) the test is decided
while compiling, and it is never the "never true" error, since another binding may make it true. The same goes
for any class test inside a generic class: one that can never be true for one instantiation (`held == Health` in
a `Box<Label>`) folds to `false` and its branch is removed from that copy, while outside a generic it stays the
error. The full rules for class tests are with
[unions](../specs/values_and_types.md#unions-in-full).

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

## `return` converts to the function's type

`return` already makes its value the function's result type, checking a number that would not fit as any
assignment does. So a local declared only to be returned on the next line says nothing the `return` does not:

```gdscript title=returned_local_error/returned_local_error.spite entry error
var console = Console()

func ReturnedLocalError() {
    console.print(rounded(2.6))
}

func rounded(scaled: Float): Integer {
    var whole: Integer = scaled.round()
    return whole
}
```
```diagnostic
'whole' is declared only to be returned on the next line: write 'return scaled.round()', since 'return' already makes the value the function's Integer
```

Write `return scaled.round()`. A local whose type differs from the function's (a narrower number, or a `T` in a
function that answers `T?`) is a different conversion, and is left alone.

---

Next: [Style: the compiler is the formatter and the linter](style.md), how the compiler formats and lints code.
