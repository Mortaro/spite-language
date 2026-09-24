# Control flow

```
if condition { } else if other_condition { } else { }
if nullable_value { } else { }     # nullable_value is its plain type inside the first block
while condition { }
switch enemy {
    Player: enemy.hurt()
    Monster: { enemy.die() }
}
```

That is all of it: `if`, `while` and `switch`, plus `assert` and `crash` ([failure.md](failure.md)). There is no
`for`, no `break` or `continue`, no ternary `? :`, and no `do`. Before reaching for a loop at all, check whether a
[member template](collections.md#member-templates-loops-you-do-not-write) already says it:
`items.filter_in_stock().sum_price()` reads better than the loop it replaces, and compiles to the same loop.

## `if`

Conditions are `Bool`s, joined with `and`, `or` and `not`, which short-circuit. An `if` on a `T?` narrows it in
place: inside the block it is a plain value, and the `else` runs exactly when it is null
([failure.md](failure.md#narrowing)). `else if` is written on one line; the formatter joins an `else { if }` into
it.

```spite title=if_chains/if_chains.spite entry
var console = Console()

func IfChains() {
    var grades = [95, 72, 40]
    var index = 0
    while index < grades.count() {
        var grade = grades[index]
        var letter = letter_for(grade)
        console.print(grade, letter)
        index = index + 1
    }
}

func letter_for(grade: Int): String {
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

An `if` that only returns the function's default is an `assert` spelled the long way, and is an error naming
it ([failure.md](failure.md#an-if-that-only-returns-the-default-is-an-assert)).

## `while` is the only loop

There is no `for`: "only the while loop, no for; that makes people favor the metaprogramming." Writing `for` is a
parse error that names the fix instead of silently doing something else:

```spite title=for_rejected/for_rejected.spite entry error
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

When you do need to walk a list by hand, index it. The loop's condition `index < numbers.count()` proves
`numbers[index]` inside the body, so the read needs no narrowing:

```spite title=while_basics/while_basics.spite entry
var console = Console()

func WhileBasics() {
    var numbers = [10, 20, 30]
    var index = 0
    var total = 0
    while index < numbers.count() {
        total = total + numbers[index]
        index = index + 1
    }
    console.print("total", total)
}
```
```output
total 60
```

There is no `break` and no `continue`. A loop that stops early says so in its own condition, with a flag or with
the bound itself:

```spite title=early_exit/early_exit.spite entry
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

## `switch` over a union

A `switch` is over a union -- or a `T?`, which is the union of a type and `Null` -- and must cover every member.
Inside each case the value is narrowed to that member. A case is one statement on its line, or a block in `{ }`.

```spite title=switch_cases/cat.spite
func sound(): String {
    return "meow"
}
```
```spite title=switch_cases/dog.spite
func sound(): String {
    return "woof"
}

func fetch(): String {
    return "fetches the stick"
}
```
```spite title=switch_cases/fish.spite
var fins = 2
```
```spite title=switch_cases/switch_cases.spite entry
union Creature {
    Cat
    Dog
    Fish
}

var console = Console()

func SwitchCases() {
    describe(Dog())
    describe(Fish())
    describe(Cat())
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
`sound()` only on the members `_` answers for (here `Cat`), not on the whole union. A switch stays exhaustive --
`_` is how it covers the rest, not a way to skip it. Two cases that do the same thing are an error naming `_:`,
and so is a case that does what `_:` already does, or a `_:` that answers for nothing.

An enum is not switched over; compare it with `==` in an `if` chain.

## `value == Class`

A class name on the right of `==` or `!=` asks what class a value is: `creature == Fish` is true when `creature`
is a `Fish`. A `T?` that is null is no class, so the test is false. Naming a class that cannot be a member of the
value's union is an error, since the answer could only be `false`. `if value == Class { }` narrows the value
inside the block, the way a switch case does:

```spite title=class_test_doc/cat.spite
var lives = 9
```
```spite title=class_test_doc/fish.spite
var fins = 2
```
```spite title=class_test_doc/class_test_doc.spite entry
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
    var cat_fins = fins_of(Cat())
    console.print("a cat's fins", cat_fins)
}

func can_swim(creature: Creature): Bool {
    return creature == Fish
}

func fins_of(creature: Creature): Int {
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

**So a switch that is one early return is an error.** A switch with one class case and `_:`, each a single
`return`, says no more than `if value == Class { return ... }` followed by what `_:` returns -- and when both
return `Bool` literals, it is `return value == Class`. The error names the form to write:

```spite title=single_case_switch_error/cat.spite
var lives = 9
```
```spite title=single_case_switch_error/fish.spite
var fins = 2
```
```spite title=single_case_switch_error/single_case_switch_error.spite entry error
union Creature {
    Cat
    Fish
}

var console = Console()

func SingleCaseSwitchError() {
    var swims = is_fish(Fish())
    console.print(swims)
}

func is_fish(creature: Creature): Bool {
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
