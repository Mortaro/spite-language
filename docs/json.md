# JSON and binary

Four classes turn any value into something to send or store, and back: `JsonWriter` and `JsonReader` for JSON
text, `BinaryWriter` and `BinaryReader` for compact bytes. None of them is a serialiser written per type. Each is
one generic class in `library/`, written with Spite's own metaprogramming, and the compiler makes the functions a
type needs only when a program uses one with it. A program that never names them carries none of it.

A JSON serializer is made once, with no arguments, and used as often as needed: `JsonWriter<Order>()`, then
`write(order)` for each order to write, and `JsonReader<Order>()`, then `read(text)` for each text to read, which
answers `Order?`, `null` when the input is not an `Order`.

Which one to use: **binary between Spite programs, JSON for everything else.** Two Spite programs compile from
the same source, so both ends already know every type and nothing has to describe itself; the bytes are about a
quarter the size of the JSON, more than ten times faster to write and twenty-five times faster to read
([measured below](#how-fast-and-how-small)). JSON is for talking to systems that are not Spite
([The wire format](targets.md#the-wire-format)).

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
    var tea = Item("tea", 2)
    order.items.append(tea)
    var writer = JsonWriter<Order>()
    var text = writer.write(order)
    console.print(text)
    var reader = JsonReader<Order>()
    var read = reader.read(text)
    crash read
    var first_item = read.items.first()
    crash first_item
    console.print(read.customer, read.status, first_item.name, first_item.count)
}
```
```output
{"id":7,"customer":"Ada \"the\" first","total":12.5,"status":"shipped","items":[{"name":"tea","count":2}],"note":null}
Ada "the" first shipped tea 2
```

- `JsonWriter<T>()` names the type it writes and takes nothing; `write(value: T?): String` answers the text of one
  value, and writes `null` for an empty `T?`. One writer writes any number of values, one call each: every
  attribute has a type the compiler knows. Attributes are written in
  the order the class declares them. The one thing it cannot write is a `Float` or `Double` that is infinity or
  not-a-number, which JSON cannot hold, and there it crashes naming the attribute and the value (see below).
- `JsonReader<T>()` names the type it makes and takes nothing, and `read(text: String): T?` answers `null` when the
  text is not JSON, or not this class's JSON; one reader reads any number of texts. Narrow it like any other `T?`: `crash` when bad input is a bug, `assert` when the program should carry on,
  `if` when absence is a case.
- `read_or_crash(text: String): T` halts on bad input instead, at the point the reader finds it, and the crash line shows what
  was wanted there, the position and the text: `wanted=a number	text={"count": three}	position=10`.

## Write and read bytes

`BinaryWriter` and `BinaryReader` have the same shape as the JSON pair: each names its type, is made once and is
reused, and each call carries its own input. The bytes are a `List<Byte>`, the bytes themselves in one
block ([collections.md](collections.md#listt)):

```gdscript title=binary_basics/order.spite
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
```gdscript title=binary_basics/item.spite
var name = ""
var count = 1

func Item(starting_name: String, starting_count: Integer) {
    name = starting_name
    count = starting_count
}
```
```gdscript title=binary_basics/binary_basics.spite entry
var console = Console()

func BinaryBasics() {
    var order = Order()
    order.id = 7
    order.customer = "Ada"
    order.total = 12.5
    order.status = 'shipped'
    var tea = Item("tea", 2)
    order.items.append(tea)
    var writer = BinaryWriter<Order>()
    var bytes = writer.write(order)
    var size = bytes.count()
    console.print("bytes:", size)
    var reader = BinaryReader<Order>()
    var read = reader.read(bytes)
    crash read
    var first_item = read.items.first()
    crash first_item
    console.print(read.customer, read.status, first_item.name, first_item.count)
}
```
```output
bytes: 23
Ada shipped tea 2
```

The same `Order` is 23 bytes here and 104 as JSON: `id` and `total` (a `Float`) are four bytes each, `status` one,
`"Ada"` a length byte and three, the empty `note` one, and there are no keys, quotes or commas at all. [The binary format](#the-binary-format)
says exactly what each type becomes.

- `write(value): List<Byte>` answers a new list holding the value. `append_to(value, bytes)` writes it at the
  end of a list the program already has instead, so many values (a network frame's worth of messages, a file's
  records) go into one buffer with no copying.
- `read(bytes): T?` reads one value that fills the bytes exactly. It answers `null` when the bytes run out in the
  middle of the value, when they are not a `T` (a `Boolean` byte that is neither 0 nor 1, an enum index the enum
  does not have, a count larger than the bytes left), and when bytes are left over after it, as JSON's `read`
  does with text after the value.
- `read_from(bytes, start): T?` reads one value starting `start` bytes in and leaves the bytes after it alone:
  `position` (an attribute) is then where the value ended, so values written one after another are read one
  after another by starting each read at the last one's `position`. It answers `null` for the same bad bytes and
  when `start` is at or past the end, and a failed read leaves `position` where it was; a negative `start` halts.
Bytes from a `Socket` or a `File` are already a `List<Byte>`, so they are read the same way:

```gdscript
var received = connection.read_bytes_now()
var reader = BinaryReader<Move>()
var move = reader.read(received)
```

Infinity and not-a-number are ordinary bytes: only JSON cannot hold them, so `BinaryWriter` never crashes.

### Several values in one buffer

```gdscript title=binary_stream/binary_stream.spite entry
var console = Console()

func BinaryStream() {
    var bytes = List<Byte>()
    var names = ["ada", "bo", "cy"]
    var name_writer = BinaryWriter<List<String>>()
    name_writer.append_to(names, bytes)
    var turn_writer = BinaryWriter<Integer>()
    turn_writer.append_to(12, bytes)
    var size = bytes.count()
    console.print("bytes:", size)
    var name_reader = BinaryReader<List<String>>()
    var read_names = name_reader.read_from(bytes, 0)
    crash read_names
    var turn_reader = BinaryReader<Integer>()
    var read_turn = turn_reader.read_from(bytes, name_reader.position)
    crash read_turn
    var left = size - turn_reader.position
    var joined = read_names.join(" ")
    console.print(joined, read_turn, left)
}
```
```output
bytes: 15
ada bo cy 12 0
```

A reader reads one type; to read a different one from the same bytes, a second reader starts at the first one's
`position`. A header is written the same way, with a writer of its own ahead of the value.

## What each type becomes

| Spite | JSON | Binary |
|---|---|---|
| `String` | text, with `"`, `\`, and control characters escaped | its length in bytes as a count, then its UTF-8 bytes |
| `Tiny`, `Byte` | a number | 1 byte |
| `Short`, `UnsignedShort` | a number | 2 bytes, little-endian |
| `Integer`, `UnsignedInteger`, `Float` | a number | 4 bytes, little-endian (`Float` as its IEEE 754 bits) |
| `Long`, `UnsignedLong`, `Double` | a number | 8 bytes, little-endian (`Double` as its IEEE 754 bits) |
| `Boolean` | `true` or `false` | 1 byte, `1` or `0` |
| an enum | its value's name, as text | its index in the enum, in 1 byte for up to 256 values, 2 up to 65 536, else 4 |
| a `Symbol` | its name, as text; read back only as a name the program already uses as a `Symbol` | its name, as a `String` is written; read back the same way |
| a class | an object, one key per attribute | its attributes, one after another in declaration order, with nothing around them |
| `List<T>` | an array | its count as a count, then each element |
| `Dictionary<Key, Value>` | an object, one key per entry; a number key as the number in quotes, an enum key as its name | its count as a count, then each key as its type is written (a `String`, or the whole number) and its value |
| `T?` | `null`, or what `T` becomes | 1 byte, `0` for empty or `1` followed by what `T` becomes |

A **count** is an unsigned LEB128 varint: seven bits per byte, low bits first, the top bit set on every byte but
the last, so a count below 128 is one byte.

Anything else a class holds (another class, a list of lists, a dictionary of classes) is one of these again,
so each writer and reader goes as deep as the class does. A `union`, a `type` such as `Anything`, or a function
value anywhere in what they see has no form in either, and making one is a compile error at your line, naming the
attribute: `JsonWriter cannot write 'Owner': 'Owner.pet' is the union Pet, and JSON does not say which member a
value is`, or `BinaryReader cannot read 'Owner': 'Owner.pet' is the union Pet, and the bytes do not say which
member a value is` (`diagnostics/json_unwritable`). Keep what they see to the types in this table.

**Private attributes are left out** by both formats: an attribute whose name starts with `_` is the class's own
business ([style.md](style.md)), as `to_debug()` leaves it out, so it is neither written nor read and keeps its
default. It is how a class chooses which of its fields travel; a component that sends only some of its state keeps
the rest in `_` attributes, or the program sends a smaller class that holds only what travels.

**So are attributes holding a singleton** (`var console = Console()`): a singleton is state the whole program
shares, so writing it out would publish it and reading it would let whoever wrote the input replace it. Such an
attribute is neither written nor read, and it is not part of the [schema hash](#the-schema-hash) either.

## The binary format

The table above is the whole format, and it is stable: it depends only on the class's attributes, their order and
their types, never on the machine, the compiler's version or the build, so bytes written today read back
tomorrow, on another machine, from a program built another way. What changes it is changing the class (adding,
removing, reordering or retyping an attribute, or reordering an enum's values), and then both ends must be built
from the new source, since nothing in the bytes says which class they are or what it held.

- **No header, no keys, no padding.** The bytes are the value and nothing else, so values can follow each other in
  one buffer and a program frames them however its transport needs.
- **Little-endian everywhere.** It is the order of every machine Spite compiles for (x86-64, ARM64 and
  WebAssembly), so a number is one store and one load with no swapping. A big-endian
  target would have to swap each number's bytes in `BinaryOutput` and `BinaryInput`; the format would not change.
- **Reading never trusts a count.** A text length or an element count larger than the bytes left fails the read
  before anything is allocated, so a corrupt or hostile buffer cannot make a reader allocate gigabytes.

### The schema hash

A handshake carries one, so an old client meeting a new server fails with a clear report instead of
misreading bytes. It belongs to the handshake or the file header, not to every value: a value's bytes stay bare so
that thousands of them can share one buffer. `writer.schema(): Long` and `reader.schema(): Long` answer a hash of
the attribute walk the bytes follow: the class's name, then each
attribute's name and type in order, nested classes the same way, and each enum's values in order. The compiler
works it out and the C holds the constant, so asking costs nothing; a program writes it once at the start of a
file or a connection and compares it before reading.

```gdscript title=schema_header/point.spite
var across = 0
var down = 0
```
```gdscript title=schema_header/place.spite
var across = 0
var down = 0
```
```gdscript title=schema_header/schema_header.spite entry
var console = Console()

func SchemaHeader() {
    var writer = BinaryWriter<Point>()
    var reader = BinaryReader<Point>()
    var other_writer = BinaryWriter<Place>()
    var written = writer.schema()
    var expected = reader.schema()
    var other = other_writer.schema()
    console.print("{written == expected} {written == other}")
    var point = Point()
    var bytes = writer.write(point)
    var read = reader.read(bytes)
    crash read
    var place = Place()
    var corner = place.across + place.down + read.across + read.down
    console.print(corner)
}
```
```output
true false
0
```

`Place` has exactly `Point`'s bytes, but it is another class, so its hash differs. The hash is the 64-bit FNV-1a
of a text the compiler writes, and which a program never sees: `Point{across:Integer;down:Integer}`, lists as
`List<...>`, dictionaries as `Dictionary<...>` (followed by `by` and the key's type when it is keyed by numbers),
`T?` with a `?`, an enum as its name and its values in brackets, and a class already being written, a recursive
one, by its name alone. The same classes give the same hash in
every build and on every machine; renaming, adding, removing, reordering or retyping an attribute changes it.

## Reading input you did not write

Text from another system rarely matches a class exactly. `JsonReader` is strict about what it cannot use and
lenient about what it does not need:

- **A key the class does not have is skipped**, whatever its value.
- **An attribute the text does not mention keeps its default**, the value the class declares for it.
- **A key in camelCase or PascalCase** (`buyPrice`, `BuyPrice`) reads into the attribute it spells, `buy_price`
  ([below](#a-camelcase-or-pascalcase-key)).
- **A key that cannot be an attribute's name** (`min_lod`, `instances`) is paired with its attribute in a map
  the serializer is given ([below](#a-key-that-is-not-an-attributes-name)).
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
    var partial_reader = JsonReader<Reading>()
    var partial = partial_reader.read("\{ \"name\": \"probe\", \"vendor\": \{ \"id\": [1, 2] } }")
    crash partial
    var tag_count = partial.tags.count()
    console.print(partial.name, partial.level, tag_count)
    var mistyped_reader = JsonReader<Reading>()
    var mistyped = mistyped_reader.read("\{\"level\": \"high\"}")
    if mistyped {
        console.print("unexpected")
    } else {
        console.print("level must be a number")
    }
    var broken_reader = JsonReader<Reading>()
    var broken = broken_reader.read("\{\"name\": \"probe\"")
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

### A camelCase or PascalCase key

Other systems write `buyPrice` or `BuyPrice`
where Spite writes `buy_price`, so a key that names no attribute as written is matched against each attribute's
name spelled in camelCase and in PascalCase. The spellings are constants the compiler writes, so snake_case input
reads exactly as fast as before and a camelCase key costs only the second, shorter look; `JsonWriter` still writes
`buy_price`:

```gdscript title=json_camel_case/price.spite
var name = ""
var buy_price = 0
var sell_price_each = 0.0
```
```gdscript title=json_camel_case/json_camel_case.spite entry
var console = Console()

func JsonCamelCase() {
    var reader = JsonReader<Price>()
    var price = reader.read("\{\"name\": \"potion\", \"buyPrice\": 30, \"SellPriceEach\": 7.5}")
    crash price
    console.print(price.name, price.buy_price, price.sell_price_each)
    var writer = JsonWriter<Price>()
    var text = writer.write(price)
    console.print(text)
}
```
```output
potion 30 7.5
{"name":"potion","buy_price":30,"sell_price_each":7.5}
```

### A key that is not an attribute's name

Some keys cannot be attribute names: `min_lod` is an abbreviation the naming rules refuse, and `instances` is a
name reflection gives every object ([reflection.md](reflection.md)). The class does not name them: the program
sets the serializer's `keys`, a map from each attribute to its key, keyed by the attribute itself, before its
first use, and `JsonWriter` then writes the attribute under that key and `JsonReader` reads that key into it, so the
text round-trips:

```gdscript
var writer = JsonWriter<MeshRecord>()
writer.keys = {MeshRecord.attributes['minimum_level_of_detail']: "min_lod", MeshRecord.attributes['instance_count']: "instances"}
```

The key is the one way to write that attribute: `minimum_level_of_detail` and `minimumLevelOfDetail` are then
unknown keys, skipped like any other. Every serializer takes the same kind of map. A map written as a literal
costs nothing: its keys are literal text in the code the serializer is compiled to, and reading matches them as it
matches an attribute's own name. A map made while the program runs is turned into a table once, when the
`keys` is set, so each value then costs what the literal case costs. A serializer whose `keys` is never set has
no map, and writes and reads every attribute under its own name. A map naming an attribute the class does
not have, a private one or one holding a singleton is an error.

A class that still declares a function `json_key_<attribute>()`, the old way to name a key, is an error at that
function naming the map, so a key it gave is never silently dropped:

```gdscript title=json_key_function/mesh_record.spite
var name = ""
var minimum_level_of_detail = 0

func json_key_minimum_level_of_detail(): String {
    return "min_lod"
}
```
```gdscript title=json_key_function/json_key_function.spite entry error
var console = Console()

func JsonKeyFunction() {
    var reader = JsonReader<MeshRecord>()
    var mesh = reader.read("\{\"name\": \"rock\"}")
    crash mesh
    console.print(mesh.name, mesh.minimum_level_of_detail)
}
```
```diagnostic
'MeshRecord.json_key_minimum_level_of_detail' would name the JSON key of 'minimum_level_of_detail', and JsonReader and JsonWriter do not read a function for it
```

Bytes have no keys to skip, so `BinaryReader` is strict throughout: the bytes are exactly the class, or the read
is `null`.

## Values that are not classes

All four take any type, not only classes: `JsonWriter<Dictionary<String, Integer>>()` for a dictionary,
`JsonReader<List<Double>>()` to read a list, `BinaryWriter<Integer>()`.

```gdscript title=json_values/json_values.spite entry
var console = Console()

func JsonValues() {
    var scores = Dictionary<String, Integer>()
    scores["ada"] = 3
    scores["bo"] = 5
    var writer = JsonWriter<Dictionary<String, Integer>>()
    var text = writer.write(scores)
    console.print(text)
    var numbers = JsonReader<List<Double>>()
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

## How fast and how small

A small object (an `Integer` id, a short name, two `Float`s, a `Short`, a `Boolean` and an enum) is about 94 bytes
of JSON and 24 bytes of binary. What testing a binary schema costs against C is in
[its case](../benchmarks/a_binary_schema_is_a_constant/).

JSON's cost is making and parsing text: every number is formatted and parsed, every key written and matched, and
every member is a `String` of its own. The binary writer stores each number with one instruction and reads it
with one, and its walk makes no object at all; what it allocates is the writer and its cursor for each value, and
each value read back.

## How it is written

All four are ordinary Spite, and reading them is the best way to learn the two metaprogramming forms they rest on
([metaprogramming.md](metaprogramming.md)):

- `if $value_type == List { }` asks at compile time what `T` is, and `JsonWriter<$value_type.element_type>()`
  makes the writer for its elements, once per list: each element is given in turn to its `write(element)`. Only the
  branch that fits `T` is compiled.
- A class is written by walking the value's attributes with `each`
  ([reflection.md](reflection.md#a-class-and-an-instance-of-it)). The walk is unrolled while compiling into one
  call per attribute, and each call is compiled for its attribute, so `attribute.value` has the attribute's own
  type and the two questions on the attribute fold away:

```gdscript
func write_attribute(attribute: Spite.Attribute) {
    if not attribute.name.starts_with("_") and not attribute.is_singleton {
        var attribute_json = JsonWriter<attribute.class>()
        attribute_json.path = "{$value_type.name}.{attribute.name}"
        var attribute_text = attribute_json.write(attribute.value)
        crash _members
        _members.append("\"{attribute.name}\":{attribute_text}")
    }
}
```

`each` hands the function only the attribute, so what else the walk needs lives in the writer: `_written` puts a
fresh list in `_members`, calls `shown.attributes.each(write_attribute)` and joins the list. `JsonReader` walks
the object it is filling the same way, `created.attributes.each(read_attribute)`, with the key it just read in
`_key`; the attribute whose name matches parses the value into `attribute.value`, and only a key no name matched
walks again, comparing the camelCase and PascalCase spellings. A class of the `Spite` namespace, a reflection
object, is not walked (`if $value_type.namespace != Spite`): its attributes are all private, and its own
`.attributes` would be the members it describes. `--final-classes` does not print these copies (a
generic class's file is shared by all of its instances), but they are ordinary typed functions in the C the program
compiles to.

The binary pair keeps the walk in `BinaryFormat<T>` (`library/binary_format.spite`), a singleton that holds
nothing, so it is a static object and the walk makes no object at all; its `write_attribute` is the one above
with `BinaryFormat<attribute.class>()` in place of the `JsonWriter`. An enum is walked the same way, over its
values, to find its index. `BinaryOutput` and `BinaryInput` are the
cursors over the bytes. Reading JSON text goes through `JsonCursor` (`library/json_cursor.spite`), a cursor over the
text that remembers the first thing that went wrong.

---

Next: [Time](time.md), dates, durations, clocks and time zones.
