# JSON

`Json<T>` turns any class into JSON text and back. It is not a serialiser written per type: it is one generic
class in `library/json.spite`, written with Spite's own metaprogramming, and the compiler makes the functions a
class needs only when a program uses `Json` with it. A program that never names `Json` carries none of it.

JSON is for talking to systems that are not Spite. Two Spite programs do not need it: they compile from the same
source, so both ends already know every type (manual section 15, "The wire format").

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

func Item(starting_name: String, starting_count: Int) {
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
    var json = Json<Order>()
    var text = json.write(order)
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

- `write(value): String` never fails: every attribute has a type the compiler knows, so there is nothing to
  go wrong at run time. Attributes are written in the order the class declares them.
- `read(text): T?` answers `null` when the text is not JSON, or not this class's JSON. Narrow it like any other
  `T?`: `crash` when bad input is a bug, `assert` when the program should carry on, `if` when absence is a case.
- `read_or_crash(text): T` halts on bad input instead, and the crash line says where and what was expected:
  `failure=expected a number at character 10`.

## What each type becomes

| Spite | JSON |
|---|---|
| `String` | text, with `"`, `\`, and control characters escaped |
| every number type | a number |
| `Bool` | `true` or `false` |
| an enum | its value's name, as text |
| a class | an object, one key per attribute |
| `List<T>` | an array |
| `Dictionary<T>` | an object, one key per entry |
| `T?` | `null`, or what `T` becomes |

Anything else a class holds -- another class, a list of lists, a dictionary of classes -- is one of these again,
so `Json` goes as deep as the class does.

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
    var json = Json<Reading>()
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

`Json` takes any type, not only classes: `Json<List<Int>>`, `Json<Dictionary<Item>>`, `Json<String>`.

```gdscript title=json_values/json_values.spite entry
var console = Console()

func JsonValues() {
    var scores = Dictionary<Int>()
    scores.set("ada", 3)
    scores.set("bo", 5)
    var json = Json<Dictionary<Int>>()
    var text = json.write(scores)
    console.print(text)
    var numbers = Json<List<Double>>()
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

- `if $value_type == List { }` asks at compile time what `T` is, and `Json<$value_type.element_type>()` makes the
  `Json` for its elements. Only the branch that fits `T` is compiled.
- A class is written by a Symbol codegen template that ranges over `T`'s attributes, called for every one of them
  at once by its plural:

```gdscript
func write_attribute(attribute: Symbol<$value_type>, value: $value_type, members: List<String>) {
    var attribute_json = Json<attribute.class>()
    var attribute_text = attribute_json.write(value.attributes[attribute])
    members.append("\"{attribute.name}\":{attribute_text}")
}
```

`write_attributes(value, members)` calls it once per attribute; `read_attributes` does the same with the key it
just read, and the attribute whose name matches takes the value. `--final-classes` does not print these
instances into `json.spite` (a generic class's file is shared by all of its instances), but they are ordinary
typed functions in the C the program compiles to.

Reading text goes through `JsonReader` (`library/json_reader.spite`), a cursor over the text that remembers the
first thing that went wrong.
