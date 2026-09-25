# Lists, dictionaries and member templates

`List<T>` and `Dictionary<T>` are not built into the compiler. They are ordinary generic classes written in Spite
-- `library/list.spite` and `library/dictionary.spite` -- over `Memory.Heap` and `Memory.Address`, the floor
everything else is built on. The compiler keeps only the syntax: `[1, 2, 3]`, `list[index]`, and `List<T>()`. Read those two files to see
exactly what a list does; write your own container the same way ([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)).

## `List<T>`

A list literal is written with commas on one line, or one entry per line with no commas:

```gdscript
var primes = [2, 3, 5, 7]
var names = [
    "ada"
    "grace"
]
var empty = List<String>()
```

| Member | Result | Notes |
|---|---|---|
| `append(value)` / `prepend(value)` | | adds to the end / to the front |
| `insert(index, value)` | | an index out of range is clamped to the nearest end |
| `list[index]` | `T?` | may not be there, so it is narrowed ([failure.md](failure.md#reading-with--answers-t)) |
| `list[index] = value` | | the explicit form is `set_at(index, value)` |
| `get_at(index)` | `T` | the default when out of range |
| `remove_at(index)` | | nothing happens out of range |
| `remove_first()` / `remove_last()` | `T` | removes and returns it; the default when empty |
| `first()` / `last()` | `T` | the default when empty |
| `count()` / `is_empty()` | `Integer` / `Boolean` | `count()` is only ever the list's size |
| `contains(value)` | `Boolean` | |
| `clear()` / `reverse()` | | in place |
| `join(separator)` | `String` | every element becomes text: a `String`, a number, a `Boolean`, an enum value |
| `copy()` / `deep_copy()` | `List<T>` | one level, or all the way down ([memory.md](memory.md)) |

Names say where: `append` and `prepend`, never `add`; `remove_last`, never `pop`. The old names are errors that
name the new ones.

## `Dictionary<T>`

Keys are `String`, entries keep the order they were inserted in, and `get`, `has`, `set` and `[]` take the same
time however many keys there are -- it is a hash table over two lists.

| Member | Result | Notes |
|---|---|---|
| `set(key, value)` / `dictionary[key] = value` | | replaces the value of a key already there |
| `get(key)` / `dictionary[key]` | `T?` | `null` when the key is absent |
| `has(key)` | `Boolean` | |
| `remove(key)` | | |
| `count()` | `Integer` | |
| `keys()` / `values()` | `List<String>` / `List<T>` | a fresh list, in insertion order |
| `copy()` / `deep_copy()` | `Dictionary<T>` | |

```gdscript title=dictionary_tasks/dictionary_tasks.spite entry
var console = Console()

func DictionaryTasks() {
    var inventory = Dictionary<Integer>()
    inventory.set("sword", 1)
    inventory.set("potion", 4)
    inventory.set("potion", 6)
    var inventory_count = inventory.count()
    console.print("count", inventory_count)
    var has_shield = inventory.has("shield")
    console.print("has shield", has_shield)
    crash inventory["potion"]
    console.print("potions", inventory["potion"])
    console.print("shields", inventory["shield"] == 0)
    var total = 0
    var values = inventory.values()
    var index = 0
    while index < values.count() {
        total = total + values[index]
        index = index + 1
    }
    console.print("total items", total)
}
```
```output
count 2
has shield false
potions 6
shields false
total items 7
```

`inventory["shield"]` is `null`, and `null` equals nothing, so `inventory["shield"] == 0` is `false`: absent is
not zero. `has(key)` asks the question directly.

## Member templates: loops you do not write

A list of a class answers a family of functions named after the element's members (a function of your own is
passed instead, [below](#passing-a-function-for-each-element)). `chores.count_done()` counts
the chores whose `done` is true, `items.sum_price()` adds up their prices, and `repositories.map_name()` collects
their names. A **member** is an attribute or a function that takes no arguments -- the two are the same to a
template, since reading an attribute already goes through its getter -- and a template is compiled only for the
names a program calls.

| Template | Result | The member must |
|---|---|---|
| `filter_<member>()` | `List<T>`, the elements where it is true | take nothing, return `Boolean` |
| `count_<member>()` | `Integer`, how many are true | take nothing, return `Boolean` |
| `any_<member>()` / `all_<member>()` | `Boolean` | take nothing, return `Boolean` |
| `sum_<member>()` | the member's number type | take nothing, return a number |
| `find_by_<member>(value)` | `T?`, the first element whose member equals `value` | return something comparable to `value` |
| `sort_by_<member>()` | `List<T>`, sorted ascending | return a number or a `String` |
| `map_<member>()` | `List<U>`, one value per element | return a value |
| `each_<member>()` | nothing: calls it on every element | be a function |

A member that does not fit is a compile error naming the member, what it is, and what the template needs.
`count_` on a number is one of them, and names `sum_` instead: `count()` is only ever a collection's size.

```gdscript title=list_helpers/chore.spite
var title = ""
var done = false

func Chore(new_title: String, new_done: Boolean) {
    title = new_title
    done = new_done
}

func finish() {
    done = true
}
```
```gdscript title=list_helpers/list_helpers.spite entry
var console = Console()

func ListHelpers() {
    var chores = List<Chore>()
    chores.append(Chore("write docs", false))
    chores.append(Chore("ship release", false))
    chores.append(Chore("rest", true))
    var done_count = chores.count_done()
    console.print("done count", done_count)
    var any_done = chores.any_done()
    console.print("any done", any_done)
    var all_done = chores.all_done()
    console.print("all done", all_done)
    var titles = chores.map_title()
    crash titles[0]
    console.print("first title", titles[0])
    chores.each_finish()
    all_done = chores.all_done()
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

```gdscript title=list_query/item.spite
var name = ""
var price = 0
var in_stock = false

func Item(new_name: String, new_price: Integer, new_in_stock: Boolean) {
    name = new_name
    price = new_price
    in_stock = new_in_stock
}
```
```gdscript title=list_query/list_query.spite entry
var console = Console()

func ListQuery() {
    var items = List<Item>()
    items.append(Item("sword", 50, true))
    items.append(Item("shield", 30, false))
    items.append(Item("potion", 10, true))
    var in_stock_count = items.filter_in_stock().count()
    console.print("in stock count", in_stock_count)
    var in_stock_price = items.filter_in_stock().sum_price()
    console.print("in stock price total", in_stock_price)
    var found = items.find_by_name("shield")
    if found {
        console.print("found", found.name, found.price)
    }
    var by_price = items.sort_by_price()
    var index = 0
    while index < by_price.count() {
        console.print(by_price[index].name, by_price[index].price)
        index = index + 1
    }
}
```
```output
in stock count 2
in stock price total 60
found shield 30
potion 10
shield 30
sword 50
```

`filter_<member>()` and `sort_by_<member>()` return a new list holding the same elements -- each one referenced
once more, not copied. A `Dictionary<T>` answers every template through its values: `inventory.sum_price()` is
`inventory.values().sum_price()`.

```gdscript title=template_mistake/repository.spite
var stars = 0
```
```gdscript title=template_mistake/template_mistake.spite entry error
var console = Console()

func TemplateMistake() {
    var repositories = List<Repository>()
    var starred = repositories.count_stars()
    console.print(starred)
}
```
```diagnostic
but 'count_' needs it to return Boolean (to add up a numeric member use 'sum_stars')
```

**A `while` that only does what a template does is an error naming the template**
([D171](decisions.md)). The shape is exact: a counter declared `0` just before the loop, the
condition `counter < list.count()`, the last statement `counter = counter + 1` and the counter read nowhere
else, and a body that reads `list[counter]` -- directly or through one `var` -- and does only one of these with a
member of the element: calls it, appends it (or the element) to a list declared empty before the loop, keeps the
elements where it is true, counts them, adds it up into a number that starts at `0`, or returns the first element
whose member equals a value (then `null`), or `true` when any is true (then `false`). Loops that need the index,
pass more than the element, stop early any other way, or walk state keep their `while`.

```gdscript title=template_loop_error/item.spite
var price = 0

func Item(item_price: Integer) {
    price = item_price
}
```
```gdscript title=template_loop_error/template_loop_error.spite entry error
var console = Console()

func TemplateLoopError() {
    var items = [Item(3), Item(5)]
    var total = 0
    var index = 0
    while index < items.count() {
        total = total + items[index].price
        index = index + 1
    }
    console.print(total)
}
```
```diagnostic
this 'while' walks every element of 'items' only to add up 'price': write 'var total = items.sum_price()'
```

## Passing a function for each element

A template sees only the element and the list: `names.each_say_hello()` looks for a member `say_hello` of each
name, never for a function of the class the call is written in. A function of yours is passed as a value
instead: `names.each(say_hello)` calls `say_hello` once for every name, in order, and there is no
`say_hello_to_everyone` to write. The function is bound to whoever owns it ([functions_and_operators.md](functions_and_operators.md#functions-are-values)):
`say_hello` alone is this instance's, and `people.each(greeter.greet)` calls `greet` on `greeter`.

`each`, `map`, `filter`, `any`, `all`, `count`, `find`, `sort_by` and `sum` each take such a function, which takes
the element as its only argument: `filter`, `any`, `all`, `count` and `find` want one returning `Boolean` (`find`
answers the first element it is true for, or `null`), `sum` one returning a number, `sort_by` one returning a
number or text, `map` one returning anything. This works on a list of anything -- text and numbers included, which
have no members of their own for a template to name -- and on a `Dictionary`, through its values.

```gdscript title=passed_function/greeter.spite
var console = Console()

func greet(name: String) {
    console.print("welcome, {name}")
}
```
```gdscript title=passed_function/passed_function.spite entry
var console = Console()
var greeter = Greeter()

func PassedFunction() {
    var names = ["Ada", "Grace", "Barbara"]
    names.each(say_hello)
    names.filter(is_short).each(greeter.greet)
    var short_names = names.filter(is_short)
    var joined = short_names.join(", ")
    console.print("short:", joined)
    var letters = names.filter(is_short).map(measure).sum(double_of)
    console.print("letters, doubled:", letters)
    var first_long = names.find(is_long)
    crash first_long
    console.print("first long name:", first_long)
}

func say_hello(name: String) {
    console.print("hello, {name}")
}

func is_short(name: String): Boolean {
    return name.length() < 6
}

func is_long(name: String): Boolean {
    return name.length() > 6
}

func measure(name: String): Integer {
    return name.length()
}

func double_of(value: Integer): Integer {
    return value * 2
}
```
```output
hello, Ada
hello, Grace
hello, Barbara
welcome, Ada
welcome, Grace
short: Ada, Grace
letters, doubled: 16
first long name: Barbara
```

A function written by name is compiled straight into the loop, so passing it costs nothing; a function value held
in a variable (`var shout = greeter.shout`, then `names.map(shout)`) is called through the value. A function that
does not fit is an error naming what the template needs:

```gdscript title=passed_function_mistake/passed_function_mistake.spite entry error
var console = Console()

func PassedFunctionMistake() {
    var names = ["Ada", "Grace"]
    var greetings = names.map(say_hello)
    console.print(greetings)
}

func say_hello(name: String) {
    console.print("hello, {name}")
}
```
```diagnostic
'map(say_hello)': 'say_hello' returns nothing, but 'map' needs it to return a value (to only call it for each element, write 'each(say_hello)')
```

The template still sees only the element: a function that needs more, such as `print_statement(statement, depth)`,
is called from a `while`.

## Chains run as one loop

Every template takes what the one before it gives, so they chain: `map_<member>()` turns a list of teams into a
list of their leads, `filter_<member>()` keeps some of them, and anything else finishes the chain. Read a chain
as the separate steps it is written as -- that is what it means -- but the compiler runs it as **one loop over
the first list**, with no list in between: `teams.filter_active().map_lead().sum_age()` visits each team once,
reads its lead, and adds the age, allocating nothing.

```gdscript title=fused_chain/person.spite
var age = 0

func Person(new_age: Integer) {
    age = new_age
}
```
```gdscript title=fused_chain/team.spite
var active = false
var lead = Person(0)

func Team(new_active: Boolean, new_lead: Person) {
    active = new_active
    lead = new_lead
}
```
```gdscript title=fused_chain/fused_chain.spite entry
var console = Console()

func FusedChain() {
    var teams = List<Team>()
    var ann = Person(34)
    teams.append(Team(true, ann))
    var bob = Person(67)
    teams.append(Team(false, bob))
    var ages = teams.filter_active().map_lead().sum_age()
    var active = teams.filter_active()
    var leads = active.map_lead()
    var ages_step_by_step = leads.sum_age()
    console.print(ages, ages_step_by_step)
}
```
```output
34 34
```

Only a call made directly on another template call is part of the chain: `active.map_lead()` above starts a new
one, because `active` is a list the program named and kept. The steps in the middle are `map_` (to a member that
is a class) and `filter_`; the last call can be any template, and a chain the compiler cannot write as one loop
is run step by step, which means the same. The one visible difference is order: a member function in a fused
chain runs element by element, where the steps written out would run it on every element before the next step
starts. `conformance/stage6/fused_chain_allocations` runs four chains a thousand times each and pins its
allocation count at 11; written step by step, the same program allocates 16 010 times.

## How the member templates are written

The templates are ordinary Spite in `library/list.spite`, over the list's own buffer on the heap. Each one is a
Symbol codegen template ([metaprogramming.md](metaprogramming.md)) whose parameter is
`member: Symbol<$element_type>`: the symbol names a member of the *element*, and `item.attributes[member]` reads
it -- the field itself, or a call to the zero-argument function. `filter_member` answers `filter_in_stock`,
`filter_is_popular` and every other `filter_<member>` call:

```gdscript
func filter_member(member: Symbol<$element_type>): List<$element_type> {
    var filtered = List<$element_type>()
    var index = 0
    while index < item_count {
        var item = values.read_value(items, index)
        if item.attributes[member] {
            filtered.append(item)
        }
        index = index + 1
    }
    return filtered
}
```

The same template answers a passed function. For `names.each(say_hello)` the compiler compiles `each_member`
once more with the function's owner passed in beside the list, and `item.attributes[member]` reads as
`say_hello(item)` on that owner (or as a call through the value, for a function held in a variable) -- so
`filter_member` also answers `filter(is_short)` without being written twice. A chain that passes functions is still
one loop.

`values` is the list's `TypedMemory<$element_type>`: `values.read_value(items, index)` reads the element in slot
`index` of the list's buffer, retained, the same way a container of your own would
([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)); everything else is written in the file. A
build with `--repl` or `--repl-port` compiles every template that fits every element class of a list the loop
can reach, so `monsters.sum_health()` can be typed at the prompt.

## Write your own member template

A program reopens `List` by putting a `list.spite` in its own folder, and a function there with a
`Symbol<$element_type>` parameter named after a segment of its name becomes one more template, exactly like the
library's:

```gdscript title=list_average/list.spite
func average_member(member: Symbol<$element_type>): Float {
    assert item_count != 0
    var total = 0.0
    var index = 0
    while index < item_count {
        var item = values.read_value(items, index)
        total = total + item.attributes[member]
        index = index + 1
    }
    return total / item_count
}
```
```gdscript title=list_average/score.spite
var points = 0

func Score(new_points: Integer) {
    points = new_points
}
```
```gdscript title=list_average/list_average.spite entry
var console = Console()

func ListAverage() {
    var scores = List<Score>()
    scores.append(Score(3))
    scores.append(Score(4))
    var average = scores.average_points()
    console.print(average)
}
```
```output
3.5
```

The `<$element_type>` is what makes it a member of the element. A plain `member: Symbol` would name one of the
list's own attributes, which are its buffer, so it is an error that says what to write:

```gdscript title=plain_symbol_template/list.spite
func total_member(member: Symbol): Integer {
    var total = 0
    var index = 0
    while index < item_count {
        var item = values.read_value(items, index)
        total = total + item.attributes[member]
        index = index + 1
    }
    return total
}
```
```gdscript title=plain_symbol_template/plain_symbol_template.spite entry error
var console = Console()

func PlainSymbolTemplate() {
    var scores = List<Integer>()
    scores.append(3)
    var count = scores.count()
    console.print(count)
}
```
```diagnostic
write 'member: Symbol<$element_type>' to name a member of the element
```

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Standard library metaprogramming  **[partial]**

D15 (decided by Mortaro, 2026-09-19): **every standard library template names a `<member>`, and a member is a
field or a zero-argument function without distinction.** `map_age` and `map_get_age` are the same call, because
reading an attribute already goes through `get_<attribute>()` ([Operators](functions_and_operators.md#operators--implemented)) -- the compiler may answer the field
case with a direct read, but that is an optimisation, not something the writer thinks about. What a template
requires of a member is its **arity and its return type**, never whether it is stored or computed.

`List<T>` of a class: `filter_<member>()`, `count_<member>()`, `sum_<member>()`, `find_by_<member>(value)`,
`sort_by_<member>()`, `each_<member>()`, `map_<member>()`, `any_<member>()`, `all_<member>()` ([List<T> additions](#listt-additions--implemented) has
the full table). `Dictionary<T>` of a class has the same. These, together with `while`, are meant to cover the
cases that used to reach for `for`.

| Template | The member must |
|---|---|
| `filter_`, `any_`, `all_`, `count_` | take no arguments and return `Boolean` |
| `sum_` | take no arguments and return a numeric type |
| `sort_by_` | take no arguments and return something ordered (`Integer`/`Float`/`String`) |
| `find_by_(value)` | take no arguments and return something comparable to `value` |
| `map_` | take no arguments and return anything; the result is a `List<U>` of that |
| `each_` | take no arguments; its result, if any, is discarded |

This is why a list of components renders with nothing new in the language: `todos.map_render()` calls `render()`
on every element and collects the results, exactly as `todos.map_title()` collects a field. A member that does
not fit the template's requirement is a compile error naming the member, what it returns, and what the template
needs -- and `each_<member>()` on a plain field is one of those, since reading a field and discarding it does
nothing.

`count_<member>()` counts how many elements have a Boolean member true; `sum_<member>()` adds up a numeric member
across every element (second batch item 8, decided 2026-09-19: `count()` is only ever a collection's own size,
and `count_<member>()` on a numeric member is a compile error naming the `sum_<member>()` fix).

```gdscript
var repositories = List<Repository>()
repositories.filter_active().sum_stars()
repositories.each_bump_stars()
```

**How the templates are written** (D91, decided by Mortaro; the binding rule is proposed by Claude,
unconfirmed). They are Symbol codegen templates (above) in `library/list.spite`, each a `while` over the list's
`Memory` buffer: `func filter_member(member: Symbol<$element_type>): List<$element_type>` answers every
`filter_<member>` call. The symbol names a member of the *element*, not of the list (whose own attributes are
its buffer), because it says so -- `Symbol<$element_type>` is [Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s `Symbol<Label>`, one mechanism for both
-- and `item.attributes[member]` reads it: the field, or a call to the zero-argument function -- the
D11 reading, "the value held in that field", applied to D15's members. The generator binds the template to the
element's member and checks the table above before it compiles the body, so a member that does not fit is still
the error naming the member, its type and what the template needs; it writes none of the templates' C. Only the
names a program calls are compiled. A `--repl`/`--repl-port` build compiles every template that fits every
element class of a list the loop can reach, and lists them as that list's functions, so `monsters.sum_health()`
works at the prompt. `Dictionary<T>` answers the same names through its values (`inventory.sum_price()` is
`inventory.values().sum_price()`), and a program's own `list.spite` reopens `List` to add a template of its own.
A template there with a plain `member: Symbol` would range over `List`'s own attributes, its buffer, and answer
nothing, so it is a compile error naming `Symbol<$element_type>` (proposed by Claude, unconfirmed;
`diagnostics/plain_symbol_on_list`).

**Chains are one loop** (D105, decided by Mortaro; the form below is proposed by Claude, unconfirmed).
Each template takes what the previous one returns, and a chain means exactly its steps written out one by one.
The compiler runs a chain as one loop over the first list with no list in between: when a template is called
directly on a `map_`/`filter_` call (on a `map_`/`filter_` call, and so on) of a `List` or `Dictionary`, the
generator writes one Spite function on the first list's class -- a `while` over its buffer, an `if` per
`filter_`, a `var` per `map_`, and the last template's step -- and calls that instead. A `map_` in the middle
must reach a class; anything the rule cannot write (a nullable member, a union) is compiled step by step, which
means the same. The difference a program can see is only order: a member function in a fused chain runs element
by element. `conformance/stage6/fused_chain_allocations` pins it: four chains run a thousand times allocate
nothing (15 allocations in all, the `Launcher` and printing the total included -- the `TypedMemory` its lists
share is a static singleton -- against more than 16 000 step by step).

**Passing a function for each element** (D148, decided by Mortaro, superseding D113's caller-function templates;
the forms' details below are proposed by Claude, unconfirmed).  **[implemented]** An iterator sees only the
element and the list, never the class the call is written in: `people.each_say_hello()` names a member
`say_hello` of each `Person`, and a function of the caller never answers
it. When the calling class has a function of that name, the error says to pass it
(`'String' has no attribute or zero argument function 'say_hello' for 'each_say_hello': a template reads a member
of each element, never a function of this class, so pass this class's 'say_hello' instead: 'each(say_hello)'`).
The caller's function is passed as a bound function value (D17/D39), owned by whoever it is bound to:

- **The forms.** `each(f)`, `map(f)`, `filter(f)`, `any(f)`, `all(f)`, `count(f)`, `find(f)`, `sort_by(f)` and
  `sum(f)` on a `List` or a `Dictionary` (through its values), for an element of any type. `f` takes the element
  as its only argument, with exactly the element's type; D15's table applies to what it returns (`filter`, `any`,
  `all`, `count` and `find` want `Boolean`, `sum` a number, `sort_by` a number or a `String`, `map` a value, `each`
  anything). `find(f)` answers the first element `f` is true for, or `null` -- the `find_by_` template with
  `true` as its value. `count` with no argument stays the collection's size.
- **The owner.** `say_hello` alone is bound to this instance; `greeter.greet` to `greeter`; a variable holding a
  `Spite.Function<T, R>` is called through the value. `people.map(greeter.label).filter(is_short)` mixes owners.
- **How it is written.** No template changes: the same `library/list.spite` template (`each_member(member:
  Symbol<$element_type>)`) is instantiated once per function and owner class, with a last hidden parameter holding
  the owner (the function's instance, passed at the call site), and `item.attributes[member]` reads as
  `owner.say_hello(item)` -- or `owner(item)` when the owner is a held function value. A function written by name
  therefore costs no allocation and no indirect call; only a held value is called through `Spite.Function`. Only
  the forms a program calls are instantiated, and these instances are not listed among the list's `functions`
  nor offered at a `--repl` prompt.
- **Chains.** A passed function chains and fuses with the member templates (D105): `map(f)` and `filter(f)` may
  sit in the middle of a chain and any form may end one, so `people.filter_active().map(greeter.label)` and
  `names.filter(is_short).map(measure).sum(double_of)` are each one loop; the fused function takes each owner as
  a parameter. The element's own members still come only from the element (`people.map(say_hello).filter_active()`
  reads `active` of whatever `say_hello` returns).
- **Mistakes** name the form: `'map(say_hello)': 'say_hello' returns nothing, but 'map' needs it to return a value
  (to only call it for each element, write 'each(say_hello)')`, `'each' calls 'greet_twice' with each 'String' as
  its only argument, but 'greet_twice' takes 2`, `'each' calls 'count_to' with each 'String', but 'count_to' takes
  'Integer'`, and an argument that is no function at all.
- **No loop rule.** D113's error for a `while` that only passes each element to a caller function is removed with
  it. A function that needs more than the element (`print_statement(statement, depth)`) keeps its `while`.

`conformance/stage6/passed_functions`, `diagnostics/passed_functions`, `docs/collections.md`.

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
| `contains(value)` | `Boolean` | `Integer`/`Float`/`Boolean`/`String`/enum elements only |
| `is_empty()` | `Boolean` | |
| `clear()` | | drops every element, keeps the buffer's capacity |
| `reverse()` | | in place |
| `join(separator)` | `String` | every element that becomes text: `String`, a number, `Boolean`, an enum value |
| `filter_<member>()` | `List<T>` | a new list of the elements whose Boolean attribute is true ([Standard library metaprogramming](#standard-library-metaprogramming--partial)) |
| `count_<member>()` | `Integer` | a Boolean attribute: how many elements have it true ([Standard library metaprogramming](#standard-library-metaprogramming--partial)) |
| `sum_<member>()` | `Integer`/`Float` | adds that attribute up across every element ([Standard library metaprogramming](#standard-library-metaprogramming--partial)) |
| `find_by_<member>(value)` | `T?` | the first element whose attribute equals `value` |
| `sort_by_<member>()` | `List<T>` | a new list sorted ascending by an `Integer`/`Float`/`String` attribute |
| `each_<member>()` | | `T` a class: calls that zero-argument function on every element, mutating it in place |
| `map_<member>()` | `List<U>` | an `Integer`/`Float`/`Boolean`/`String`/enum attribute's values, one per element |
| `any_<member>()` / `all_<member>()` | `Boolean` | a `Boolean` attribute, true for at least one / every element |

A `<member>` is always the element's, never the calling class's (D148, [Standard library metaprogramming](#standard-library-metaprogramming--partial)). A function of the caller is
passed as a value instead, for any `T`: `each(f)`, `map(f)`, `filter(f)`, `any(f)`, `all(f)`, `count(f)`,
`find(f)` (the first element `f` is true for, a `T?`), `sort_by(f)` and `sum(f)`, where `f` takes one `T` --
`names.each(say_hello)` calls `say_hello(name)` for each element.

Naming (second batch item 3, decided 2026-09-19): `add` does not say where, so it is `append` (and `prepend`);
`pop()` became `remove_last()`, plus `remove_first()` -- writing the old names is a compile error naming the
replacement.

### Dictionary\<T\>  **[implemented]**

String-keyed, insertion-ordered, and a hash table under the hood: `get`, `has`, `set` and `[]` take the same time
however many keys there are (see the decision log's hash-table row, proposed by Claude, unconfirmed).
`Dictionary<T>()` constructs one.

| Member | Result | Notes |
|---|---|---|
| `set(key, value)` | | replaces an existing key's value |
| `get(key)` | `T?` | |
| `has(key)` | `Boolean` | |
| `remove(key)` | | |
| `count()` | `Integer` | |
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
unsupported ([Memory](memory.md#memory--implemented)).

### Heap\<T\> -- removed (D1, decided by Mortaro, 2026-09-19)

`Heap<T>` is gone. References are the default now ([Memory](memory.md#memory--implemented)), so a class/union containing itself recursively
just declares an ordinary attribute of type `T` -- no indirection needed, and no struct-layout cycle to guard
against (a class is always a heap object referred to by pointer, so a self-referential field is just a pointer
like any other):

```gdscript
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
