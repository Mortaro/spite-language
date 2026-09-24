# Control flow

```
if condition { } else { }
if nullable_value { }      # narrows nullable_value to its non-null type in place, when it is not null
while condition { }
switch enemy {
    Player: enemy.hurt()
    Monster: { enemy.die() }
}
```

`if value { }` narrows `value` in place inside the `{ }` block, exactly like `assert value` does, whenever
`value` is a `T?` local, parameter, or attribute (see [values_and_types.md](values_and_types.md)'s
`assert`-narrowing section) -- `if value and other_condition { }` narrows too. Add an `else` for what runs when
`value` is null:

```spite title=if_narrowing/if_narrowing.spite entry
var console = Console()

func IfNarrowing() {
    var name = find_name(false)
    if name {
        console.print("found", name)
    } else {
        console.print("missing")
    }
}

func find_name(missing: Bool): String? {
    if missing {
        return null
    }
    return "kal"
}
```
```output
found kal
```

## `while` is the only loop

There is no `for`. The language owner's call: "only the while loop, no for; that makes people favor the
metaprogramming." Writing `for` is a parse error that names the fix instead of silently doing something else:

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

When you do need to walk a list by hand, index it:

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

## Avoiding `while`: standard library metaprogramming first

Before reaching for `while`, check whether `List<T>`/`Dictionary<T>` already has the shape you need for a
class's field or method name. These are generated per attribute/function, only for the ones actually called:

| Helper | Does |
|---|---|
| `filter_<bool attribute>()` | a view containing only the elements where that field is `true` |
| `count_<bool attribute>()` | how many elements have that `Bool` field set to `true` |
| `sum_<numeric attribute>()` | adds up that `Int`/`Float` field across every element |
| `find_by_<attribute>(value)` | first element whose field equals `value`, as a `T?` |
| `sort_by_<attribute>()` | a view sorted by that field |
| `each_<function>()` | calls that zero-argument function on every element, mutating it in place |
| `map_<attribute>()` | a `List<U>` of just that field's values |
| `any_<attribute>()` / `all_<attribute>()` | `true` if at least one / every element's `Bool` field is `true` |

```spite title=list_helpers/task.spite
var title = ""
var done = false

func Task(new_title: String, new_done: Bool) {
    title = new_title
    done = new_done
}

func finish() {
    done = true
}
```
```spite title=list_helpers/list_helpers.spite entry
var console = Console()

func ListHelpers() {
    var tasks = List<Task>()
    tasks.append(Task("write docs", false))
    tasks.append(Task("ship release", false))
    tasks.append(Task("rest", true))
    var done_count = tasks.count_done()
    console.print("done count", done_count)
    var any_done = tasks.any_done()
    console.print("any done", any_done)
    var all_done = tasks.all_done()
    console.print("all done", all_done)
    var titles = tasks.map_title()
    crash titles[0]
    console.print("first title", titles[0])
    tasks.each_finish()
    all_done = tasks.all_done()
    console.print("all done now", all_done)
}
```
```output
done count 1
any done true
all done false
first title write docs
all done now true
```

**`count()` is always just the size of a collection.** `count_<attribute>()` only exists for a `Bool`
attribute, and counts how many elements have it set to `true` -- calling it on an `Int`/`Float` attribute is a
compile error naming `sum_<attribute>()` instead, since that is almost always what was meant.

`if`/`while`/`switch` are otherwise fine and normal -- the rule is specifically "no `for`", not "avoid
branches." Reach for a standard library helper first only when one already exists for what you are about to
write a loop for.
