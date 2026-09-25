# JSON

`Json<T>` turns any class into JSON text and back. It is not a serialiser written per type: it is one generic
class in `library/json.spite`, written with Spite's own metaprogramming, and the compiler makes the functions a
class needs only when a program uses `Json` with it. A program that never names `Json` carries none of it.

`Json` takes the value in its constructor and reads its `T` from it, so writing names no type: `Json(order)`,
then `write()`. Reading has no value yet, so it names the class it reads into and gives no value:
`Json<Order>(null)`, then `read(text)`.

JSON is for talking to systems that are not Spite. Two Spite programs do not need it: they compile from the same
source, so both ends already know every type ([The wire format](targets.md#the-wire-format--planned)).

## Write and read a class

```gdscript title=json_basics/order.spite
enum Status {
    'open'
    'shipped'
}

var id = 0
var customer = ""
var total = 0.0
var status: Status = 'open'
var items = List<Item>()
var note: String? = null
```
```gdscript title=json_basics/item.spite
var name = ""
var count = 1

func Item(starting_name: String, starting_count: Integer) {
    name = starting_name
    count = starting_count
}
```
```gdscript title=json_basics/json_basics.spite entry
var console = Console()

func JsonBasics() {
    var order = Order()
    order.id = 7
    order.customer = "Ada \"the\" first"
    order.total = 12.5
    order.status = 'shipped'
    order.items.append(Item("tea", 2))
    var json = Json(order)
    var text = json.write()
    console.print(text)
    var read = json.read(text)
    crash read
    var first_item = read.items.first()
    console.print(read.customer, read.status, first_item.name, first_item.count)
}
```
```output
{"id":7,"customer":"Ada \"the\" first","total":12.5,"status":"shipped","items":[{"name":"tea","count":2}],"note":null}
Ada "the" first shipped tea 2
```

- `Json(value)` is `Json<T>` for the value's type, read from the argument like any generic class whose
  constructor takes its `$T` ([metaprogramming.md](metaprogramming.md#generics-and-codegen-values-)). A `T?`
  gives `Json<T>`, and writes `null` when it is empty.
- `write(): String` never fails: every attribute has a type the compiler knows, so there is nothing to
  go wrong at run time. Attributes are written in the order the class declares them.
- `read(text): T?` answers `null` when the text is not JSON, or not this class's JSON. It makes a new value and
  leaves the one the `Json` was made with alone, so the `json` above reads an `Order` back. Narrow it like any
  other `T?`: `crash` when bad input is a bug, `assert` when the program should carry on, `if` when absence is a
  case.
- `read_or_crash(text): T` halts on bad input instead, and the crash line says where and what was expected:
  `failure=expected a number at character 10`.

## What each type becomes

| Spite | JSON |
|---|---|
| `String` | text, with `"`, `\`, and control characters escaped |
| every number type | a number |
| `Boolean` | `true` or `false` |
| an enum | its value's name, as text |
| a `Symbol` | its name, as text (written only: [not read yet](#json-is-reflection-not-a-library--implemented)) |
| a class | an object, one key per attribute |
| `List<T>` | an array |
| `Dictionary<T>` | an object, one key per entry |
| `T?` | `null`, or what `T` becomes |

Anything else a class holds -- another class, a list of lists, a dictionary of classes -- is one of these again,
so `Json` goes as deep as the class does. An attribute typed as a `union`, a `type` such as `Anything`, or a
function is not handled yet ([below](#json-is-reflection-not-a-library--implemented)): keep what `Json` sees to
the types in this table.

## Reading input you did not write

Text from another system rarely matches a class exactly. `read` is strict about what it cannot use and lenient
about what it does not need:

- **A key the class does not have is skipped**, whatever its value.
- **An attribute the text does not mention keeps its default**, the value the class declares for it.
- **A value of the wrong kind makes the whole read `null`**: text where a number belongs, `null` for an
  attribute that is not a `T?`, an enum name the enum does not have.

```gdscript title=json_input/reading.spite
var name = "unnamed"
var level = 1
var tags = List<String>()
```
```gdscript title=json_input/json_input.spite entry
var console = Console()

func JsonInput() {
    var json = Json<Reading>(null)
    var partial = json.read("\{ \"name\": \"probe\", \"vendor\": \{ \"id\": [1, 2] } }")
    crash partial
    var tag_count = partial.tags.count()
    console.print(partial.name, partial.level, tag_count)
    var mistyped = json.read("\{\"level\": \"high\"}")
    if mistyped {
        console.print("unexpected")
    } else {
        console.print("level must be a number")
    }
    var broken = json.read("\{\"name\": \"probe\"")
    if broken {
        console.print("unexpected")
    } else {
        console.print("unclosed object")
    }
}
```
```output
probe 1 0
level must be a number
unclosed object
```

## Values that are not classes

`Json` takes any type, not only classes: `Json(scores)` for a `Dictionary<Integer>`, `Json<List<Double>>(null)` to
read a list, `Json("text")`.

```gdscript title=json_values/json_values.spite entry
var console = Console()

func JsonValues() {
    var scores = Dictionary<Integer>()
    scores.set("ada", 3)
    scores.set("bo", 5)
    var json = Json(scores)
    var text = json.write()
    console.print(text)
    var numbers = Json<List<Double>>(null)
    var read = numbers.read("[1.5, -2, 3e2]")
    crash read
    var joined = read.join(" ")
    console.print(joined)
}
```
```output
{"ada":3,"bo":5}
1.5 -2 300
```

## How it is written

`Json<T>` is ordinary Spite, and reading it is the best way to learn the two metaprogramming forms it rests on
([metaprogramming.md](metaprogramming.md)):

- `if $value_type == List { }` asks at compile time what `T` is, and `Json<$value_type.element_type>(null)` makes
  the `Json` for its elements, once per list: each element is given to it in turn as its `value`. Only the branch
  that fits `T` is compiled.
- A class is written by a Symbol codegen template that ranges over `T`'s attributes, called for every one of them
  at once by its plural:

```gdscript
func write_attribute(attribute: Symbol<$value_type>, shown: $value_type, members: List<String>) {
    var attribute_json = Json(shown.attributes[attribute])
    var attribute_text = attribute_json.write()
    members.append("\"{attribute.name}\":{attribute_text}")
}
```

`write_attributes(shown, members)` calls it once per attribute; `read_attributes` does the same with the key it
just read, and the attribute whose name matches takes the value. `--final-classes` does not print these
instances into `json.spite` (a generic class's file is shared by all of its instances), but they are ordinary
typed functions in the C the program compiles to.

Reading text goes through `JsonReader` (`library/json_reader.spite`), a cursor over the text that remembers the
first thing that went wrong.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### JSON is reflection, not a library  **[implemented]**

D22 and D23 (decided by Mortaro, 2026-09-19), amended by D31: writing JSON is walking a value's attributes and
reading it is a series of symbol keyed writes, so JSON needs no machinery of its own: a `type` shape is the JSON
shape, and `Json<T>` is written with the language's own metaprogramming in `library/`, like the rest of the
standard library ([Pure Spite](standard_library.md#pure-spite-dissolving-the-runtime--partial)). JSON is a format
for talking to foreign systems; it is not what Spite programs send each other
([The wire format](targets.md#the-wire-format--planned)).

- **Writing never fails at run time**: a class that cannot be serialised is a compile error, the same check
  isomorphic classes need. Only parsing can fail, and it follows the failure model: a parse that may not succeed
  returns a `T?`, and a variant that crashes exists for callers who want the report instead. The compile-time
  check is not built yet: see "Not handled yet" below.
- D22 left the names of that pair open (Mortaro sketched `to_json`/`to_crashing_json`); what is built is
  `read`/`read_or_crash`, below (proposed by Claude, unconfirmed).

**`Json<T>`** (D95, decided by Mortaro, 2026-09-24: "json with a generic should be part of our standard library
using metaprograming to easily convert to and from json").  **[implemented]** `library/json.spite` is one generic
class, and every conversion it makes is a function the compiler writes from it for the types a program actually
uses, so a program that never names `Json` carries none of it (D42, D177). At run time a conversion costs what its
Spite says: `write()` makes one `String` per member and joins them, and `read` walks the text once through one
`JsonReader`.

```gdscript
var json = Json(order)                  # Json<Order>, read from the argument (D138)
var text = json.write()                 # String: never fails
var reading = Json<Order>(null)         # no object yet, so the class is named
var read = reading.read(text)           # Order?: null on text that is not an Order
var order = reading.read_or_crash(text) # Order: halts, naming the position and what was expected
```

**`Json` takes the object in its constructor** (D138, decided by Mortaro, 2026-09-25) and infers its generic
from it, by the general rule of [Codegen values (`$`)](metaprogramming.md#codegen-values---implemented) for a
constructor that takes a `$name`; nothing about `Json` is special to the compiler. Reading keeps a way to name
the class it reads into, since there is no object yet. As built (proposed by Claude, unconfirmed): the constructor takes a `$value_type?`, so reading names the class and passes
`null`, `Json<Order>(null)`; a `T?` argument gives `Json<T>`, and `write()` of an empty one is `null`. `read`
makes a new value and leaves the one the `Json` holds alone, so one `Json(order)` can write and read.

- **What it is written with** is [Symbol codegen](metaprogramming.md#symbol-codegen--implemented) and
  [Codegen values (`$`)](metaprogramming.md#codegen-values---implemented) and nothing else, as
  [How it is written](#how-it-is-written) shows: a container makes one `Json` for what it holds and gives it each
  element in turn as its `value`, and a class is walked by the plurals `write_attributes` and `read_attributes`.
  `JsonReader` (`library/json_reader.spite`) is a cursor over the text holding the first failure.
- **What each type becomes** is [the table above](#what-each-type-becomes), applied again to whatever a value
  holds. Text escapes `"`, `\`, line feed, carriage return and tab as `\"`, `\\`, `\n`, `\r`, `\t` and every other
  control character as `\u00XX`, and reading turns any `\uXXXX` back into UTF-8, surrogate pairs included. A
  class's keys are its attributes in declaration order, leaving out private (`_`) ones as `to_debug()` does
  ([standard_library.md](standard_library.md#system-classes--implemented)); a `Dictionary`'s are its keys in the
  order they were set.
- **A walk reads every attribute**, so an attribute a program only ever hands to `Json` counts as read, and the
  unused-attribute error does not fire for it ([D118](decisions.md), the ruling on walks).

What follows is Claude's reading where D22 and D95 are not specific (proposed by Claude, unconfirmed):

- **The API is a class you make once per value, or once per type to read**, `Json(order)` or
  `Json<Order>(null)`, with `write`, `read` and `read_or_crash`. The generic is on the class because that is
  where Spite puts generics (D138 moved the value to the constructor, so writing names no type at all). `write`
  needs no crashing twin, since it cannot fail.
- **Reading is strict about what it cannot use and lenient about what it does not need**
  ([Reading input you did not write](#reading-input-you-did-not-write), and
  [Failure](failure.md#failure-three-outcomes-and-no-others--partial)'s three outcomes). An unknown key is skipped,
  whatever it holds, because foreign systems send more than a program asks for; a missing attribute keeps the
  default its class declares, the value [Variables and values](values_and_types.md#variables-and-values--implemented)
  gives anything never set. Besides a value of the wrong kind, a missing brace and anything after the value
  (`{} x`) make `read` answer `null`: the object would be wrong, and a wrong object that looks right is the
  surprise D27 exists to prevent. `read_or_crash` crashes on the same inputs, and its crash line carries
  `failure=expected <what> at character <n>`.
- **A number reads into whatever number type the attribute has** through the ordinary text-to-number cast, so
  `3.7` read into an `Integer` is `3`; JSON has one number type and the class already says which one it wants.
- **Not handled yet:** a `Float` or `Double` holding infinity or not-a-number is written as `inf`/`nan`, which is
  not JSON. D22's compile-time check is not built: an attribute typed as a `type` (`Anything`) is written as the
  empty object `{}` whatever it holds, and so is a function value, while an attribute typed as a `union` stops the
  compile with an error inside `library/json.spite` (`this class has no function 'write_attributes'`) rather than
  one at the program's line. A plain `Symbol` attribute (as opposed to an enum) is written as its name, but
  reading one is an error inside `library/json.spite` (`text is not a Symbol: ...`), since text becomes a
  `Symbol` only through `Symbol(text)` (D70). `--final-classes` does not print the functions `Json<Order>`
  generated, because a generic class's file is shared by all its instances.

`tests/json_tests.spite`, `conformance/stage6/json_crash`, `docs/json.md`.
