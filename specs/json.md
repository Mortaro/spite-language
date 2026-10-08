# JSON and binary

The specification of [JSON and binary](../docs/json.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## JSON is reflection, not a library

Writing JSON is walking a value's attributes and
reading it is a series of symbol keyed writes, so JSON needs no machinery of its own: a `type` shape is the JSON
shape, and the JSON pair is written with the language's own metaprogramming in `library/`, like the rest of the
standard library ([Pure Spite](standard_library.md#pure-spite-dissolving-the-runtime)). JSON is a format
for talking to foreign systems; it is not what Spite programs send each other
([The wire format](../docs/targets.md#the-wire-format)).

- **Writing never fails at run time**: a class that cannot be serialised is a compile error, the same check
  isomorphic classes need. Only parsing can fail, and it follows the failure model: a parse that may not succeed
  returns a `T?`, and a variant that crashes exists for callers who want the report instead. The compile-time
  check: when a program's line makes a `JsonWriter<T>`,
  `JsonReader<T>`, `BinaryWriter<T>` or `BinaryReader<T>`, the compiler walks `T` (through `T?`, list
  elements, dictionary values and every attribute of every class it reaches) and a `union`, a `type`
  (`Anything` included) or a function value is an error at that line naming the attribute path; the rest of the
  four classes' files is then not reported on, so one mistake is one error (`diagnostics/json_unwritable`).
  Compile time only. The error names the class and what it does: `<class> cannot write '<T>'` for a writer,
  `cannot read` for a reader, and the reason ends `JSON does not say which member a value is` or `the bytes do not
  say which member a value is`, and `has no JSON form` or `has no binary form`.
- The one exception to "writing never fails" is below: a `Float` or `Double` JSON cannot hold.

**Four classes: `JsonWriter`, `JsonReader`, `BinaryWriter`, `BinaryReader`.** Each names its type and is made once
and reused, and each call carries its own input. Each class is generic, and every conversion it makes is a function the compiler writes from it for
the types a program actually uses, so a program that never names one carries none of it: no table of
types, no reflection and no registry exists at run time. At run time a conversion costs what its Spite says:
`JsonWriter.write(value)` makes one `String` per member and joins them, `JsonReader.read(text)` walks the text once through
one `JsonCursor`, and the binary pair writes and reads each number with one machine store or load.

```gdscript
var writer = JsonWriter<Order>()        # made once, reused for every order
var text = writer.write(order)          # String: never fails
var reader = JsonReader<Order>()        # made once, reused for every text
var read = reader.read(text)            # Order?: null on text that is not an Order
var order = reader.read_or_crash(text)  # Order: halts, naming the position and what was expected

var packer = BinaryWriter<Order>()      # made once, reused for every order
var bytes = packer.write(order)         # List<Byte>: never fails
packer.append_to(order, frame)          # the same bytes, at the end of a List<Byte> the program has
var unpacker = BinaryReader<Order>()    # made once, reused for every buffer
var read = unpacker.read(bytes)         # Order?: null on bytes that are not exactly one Order
var next = unpacker.read_from(frame, unpacker.position)  # the next value in a buffer of many
var from_socket = unpacker.read_memory(buffer, received)
```

The names: `write`, `read` and `read_or_crash`; `append_to(value, bytes)` says where the bytes go ("append", not
"add"); `read_from(bytes, start)` and `read_memory(address, count)` say what they read from; `position` is where
the last value read ended. The old name `Json` is an error that names the new
pair: `there is no 'Json': it is two classes, 'JsonWriter<T>()' whose 'write(value)'
makes the text, and 'JsonReader<T>()' whose 'read(text)' answers a 'T?'` (`diagnostics/json_split`).

- **A JSON serializer is made with no arguments and reused**: `JsonWriter<T>()` and `JsonReader<T>()` name their
  type, and each call carries its own input: `write(value: T?): String`, which writes `null` for an empty `T?`,
  `read(text: String): T?` and `read_or_crash(text: String): T`, each reading the whole text as one value. Nothing
  is held between calls but the serializer's settings, so one writer or reader serves a whole program.
- **A binary serializer is made with no arguments and reused, as a JSON one is**: `BinaryWriter<T>()` with
  `write(value: T): List<Byte>` and `append_to(value: T, bytes: List<Byte>)`, and `BinaryReader<T>()` with
  `read(bytes: List<Byte>): T?`, which reads one value that must fill the bytes, `read_from(bytes: List<Byte>,
  start: Integer): T?`, which reads one value from `start` and leaves what follows, and `read_memory(address,
  count): T?`. The value a writer takes is a `T`, not a `T?`: an empty `T?` is narrowed by the caller, who then
  knows whether anything was written (a `T?` inside a value has its presence byte). `position` is where the last
  successful read ended, so a buffer of many values is read by starting each `read_from` there; nothing else is
  held between calls.
- **What it is written with** is reflection, a walk of the value's attributes with `each`
  ([reflection.md](../docs/reflection.md#known-while-compiling)), and
  [Codegen values (`$`)](metaprogramming.md#codegen-values-) and nothing else, as
  [How it is written](../docs/json.md#how-it-is-written) shows. The binary walk's helper, `BinaryFormat<T>`, is a `singleton`
  that holds nothing, bound where it is used (`BinaryFormat<attribute.class>()`), which is allowed since a
  singleton made from a `$` or `.class` type cannot be an attribute; in a production build it is a static object
  ([optimizations.md](../docs/optimizations.md#singletons-that-hold-nothing-are-static-objects)).
- **An enum is written by walking its values**, which a generic may do for the enum it is given
  ([values_and_types.md](../docs/values_and_types.md#enums)): that is how `BinaryFormat` finds an enum value's
  index and how many values the enum has, with no table at run time: writing one costs one comparison per value
  of the enum, and reading one a comparison per value and a text-to-enum cast.
- **What each type becomes** is [the table on the page](../docs/json.md#what-each-type-becomes), applied again to whatever a value
  holds. JSON text escapes `"`, `\`, line feed, carriage return and tab as `\"`, `\\`, `\n`, `\r`, `\t` and every
  other control character as `\u00XX`, and reading turns any `\uXXXX` back into UTF-8, surrogate pairs included. A
  class's attributes are written in declaration order, leaving out private (`_`) ones as `to_debug()` does
  ([standard_library.md](standard_library.md#system-classes)) and ones holding a singleton, which are never read
  either; a `Dictionary`'s entries in the order they were set. Both questions fold where the walk is compiled, so a
  skipped attribute costs nothing.
- **A walk reads every attribute**, so an attribute a program only ever hands to a writer counts as read, and the
  unused-attribute error does not fire for it .

The details:

- **The binary format** is [The binary format](../docs/json.md#the-binary-format) and [the table](../docs/json.md#what-each-type-becomes):
  little-endian fixed-width numbers, `Boolean` as one byte, an enum as its index in the smallest of 1, 2 or 4
  bytes that holds every index, text as a varint length and UTF-8, `T?` as a presence byte, lists and dictionaries
  as a varint count and the items, nested objects inline, and no header. The varint
  is unsigned LEB128, and a count or length above what an `Integer` holds, or above the bytes left, fails the read.
- **`BinaryWriter<T>.schema()` and `BinaryReader<T>.schema()` answer a `Long` the compiler works out** (the
  name is provisional): the 64-bit FNV-1a hash, as a signed `Long`, of the
  walk's text: the class's qualified name, then `name:type` for each attribute the bytes carry (not `_...`, and
  not one holding a singleton), in
  order, joined by `;` inside `{ }`, a nested class written the same way, `List<T>`, `Dictionary<Key, Value>`, `T?`, an
  enum as its name and `(` its values joined by `,` `)`, a class already being written by its name alone, and any
  other type by its name. It is a bodiless declaration the compiler supplies, a C macro that is the constant,
  so it costs nothing at run time and a program that never asks carries none of it. A writer and a reader of the
  same `T` answer the same value (`conformance/stage6/binary_schema`).
- **The bytes are a `List<Byte>`**: one block, no count per byte, and a reader borrows nothing, since each
  call reads the block of the list it is given and keeps nothing of it. `read_memory` covers memory the program did
  not put in a list. `List<T>` has `reserve(count)` for this, making room for `count` items without making any;
  the library's binary classes grow the list and write into its block directly, which only `library/` may do.
- **`BinaryReader` answers `null` for every bad input** (bad input is a normal condition, never a crash):
  bytes that end inside a value, a `Boolean` byte other than 0 or 1, a presence byte other than 0 or 1, an enum
  index past the enum's last value, a plain `Symbol` name the program does not use, and a count past the bytes
  left, and for `read`, bytes left over after the value. A failed read leaves `position` where it was, and a
  negative `start` for `read_from` halts, since it is a mistake in the program and not bad bytes. There is no `read_or_crash` for bytes: the bytes a Spite
  program reads were written by a Spite program, and what went wrong is not something a position would explain.
- **The API is a class you make once and reuse**, with `write` and `read`. The generic is on
  the class because that is where Spite puts generics. `write` needs no crashing twin, since it cannot fail.
- **`JsonReader` is strict about what it cannot use and lenient about what it does not need**
  ([Reading input you did not write](../docs/json.md#reading-input-you-did-not-write), and
  [Failure](failure.md#failure-three-outcomes-and-no-others)'s three outcomes). An unknown key is skipped,
  whatever it holds, because foreign systems send more than a program asks for; a missing attribute keeps the
  default its class declares, the value [Variables and values](values_and_types.md#variables-and-values)
  gives anything never set. Besides a value of the wrong kind, a missing brace and anything after the value
  (`{} x`) make `read` answer `null`: the object would be wrong, and a wrong object that looks right is a
  surprise. `read_or_crash` crashes on the same inputs, and its crash line carries
  `wanted=<what>` and `position=<n>`.
- **A key in camelCase or PascalCase reads into the snake_case attribute it spells.** The exact name wins:
  `JsonReader` first compares the key with every attribute's name, as it always did, and only a key that matched
  none is compared with each attribute's name written in camelCase and in PascalCase (the words after the
  first, or every word, capitalised and the underscores dropped: `buy_price` is `buyPrice` and `BuyPrice`). A key
  that matches neither is skipped as before. A snake_case key never matches a spelling (those have no
  underscores), and only a snake_case name has one, so no key can match two attributes; when the text holds both
  `buy_price` and `buyPrice`, the later one wins, as a repeated key does. `JsonWriter` writes the attribute's own
  name. The spellings are compile-time constants a walked attribute answers, `attribute.camel_case_name` and
  `attribute.pascal_case_name` ([metaprogramming.md](metaprogramming.md#templates)),
  and `JsonReader` compares them in a second walk, `created.attributes.each(read_camel_attribute)`, that runs only
  for a key the first walk did not match: snake_case input runs the same comparisons it did before, and a program that reads
  no JSON carries none of it (`conformance/stage6/json_camel_case`).
- **A key that is not an attribute's name comes from the serializer's optional `keys` attribute, a map keyed by
  attribute objects and set before the serializer's first use**: `{MeshRecord.attributes['minimum_level_of_detail']: "min_lod"}`. `JsonWriter` writes the attribute
  under its key and `JsonReader` reads that key into it, so the text round-trips. **The key replaces the
  attribute's name**: with one, neither the name nor its camelCase and PascalCase spellings read into the
  attribute any more, so each attribute has exactly one key; attributes without one keep the camelCase and
  PascalCase reading. Every serializer takes the same kind of map. A constant map folds into the code the
  serializer is compiled to: the keys are literal text, and reading matches them as it matches a name. A map made
  at run time fills a key table once per attribute when the serializer is made (an array by attribute, and a small
  hash for reading), so each record then costs what the constant case costs. Naming an attribute the class does
  not have, a private one or one holding a singleton is an error. Nothing in a class names its own keys.
- **A function `json_key_<attribute>()`**, where `<attribute>` is one of the class's attributes, is the old way
  to name a key and an error at the function, so a key it gave is never silently lost: "'MeshRecord.json_key_minimum_level_of_detail'
  would name the JSON key of 'minimum_level_of_detail', and JsonReader and JsonWriter do not read a function for
  it: a key that is not an attribute's name is given to the serializer in a map keyed by the attribute,
  '{MeshRecord.attributes['minimum_level_of_detail']: "..."}'" (`diagnostics/json_key`).
- **A number reads into whatever number type the attribute has** through the ordinary text-to-number cast, so
  `3.7` read into an `Integer` is `3`; JSON has one number type and the class already says which one it wants.
- **Infinity and not-a-number crash `JsonWriter`**: JSON
  (RFC 8259) holds neither, and a float became one through a division by zero or an overflow the program did not
  guard, which is the moron's mistake ([failure.md](../docs/failure.md)); floats themselves keep them,
  and `BinaryWriter` writes their bits like any other. The crash shows the value and the attribute's path,
  `Class.attribute`, with `[index]` or `["key"]` for an element: `shown=inf	path=Order.price` (and the
  same for negative infinity and not-a-number; `conformance/stage6/json_infinity`). A program that wants `null`
  there checks the number first.
- `--final-classes` does not print the functions `JsonWriter<Order>` generated, because a generic class's file
  is shared by all its instances.
- **A plain `Symbol` attribute** (as opposed to an enum) is written as its name and read back through
  `Symbol(text)`, so a name the program does not already use as a symbol is a value of the wrong kind: `read`
  answers `null` (`conformance/stage6/json_symbols`). The library tells the two
  apart with the codegen test `$value_type == Enum`, true for an enum only, where `$value_type == Symbol` is true
  for both ([metaprogramming.md](../docs/metaprogramming.md)).

---

Next: [Time](time.md).
