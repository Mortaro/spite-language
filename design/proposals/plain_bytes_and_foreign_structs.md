# Plain bytes and plain foreign structs

**Status (2026-10-08, D536):** adopted by Mortaro (`List<Byte>` for bytes); items 305 to 315 decided as recommended, to confirm after the speed goal. Not built.

A proposal (every rule here is proposed by Claude, unconfirmed) for the two language gaps the naive engine found
that keep it on the `Memory` floor (D506, D512; [naive_programs.md](../naive_programs.md#stage-1-results-2026-10-07)):

1. **Bytes read from files and sockets** come only through a `Memory.Address`, so every decoder and store of the
   engine package keeps a hand-made byte buffer over `Memory.Heap` and `TypedMemory`.
2. **A foreign C struct that holds a pointer or an array** has no declaration, since only a `type` whose attributes
   are all numbers crosses a foreign call. The Vulkan, Windows and XInput bindings build every such struct byte by
   byte at offsets written by hand.

The answer to both is the same idea: **the plain shapes a moron already writes (`List<Byte>`, a `type`, a list
literal) are what crosses, and the compiler does the byte work at the boundary.** No pointer type, no new
collection class, and one piece of new syntax (a fixed length, [below](#a-fixed-length-array-inside-a-struct)),
which is a question for Mortaro.

## Summary

- **Bytes are a `List<Byte>`** everywhere a program sees them: `File` and `Socket` read into and write from one,
  as `UdpSocket`, `Deflate`, `Sha256`, `BinaryReader` and `BinaryWriter` already do. The address forms
  (`read_bytes(position, count, address)`, `read_bytes_now(address, count)`, `read_memory`) leave the program
  surface.
- **A `List<Byte>` reads and writes numbers at a position**, with the names `MappedFile` already answers
  (`read_integer(position)`, ...), each a `T?` that is `null` past the end, plus a big-endian twin for formats that
  are big-endian (PNG, PSD). A record of fixed-width numbers is read by `BinaryReader<T>`, whose format is already
  packed little-endian with no padding.
- **A foreign struct is a `type`** whose attributes may now also be: text (`const char*`), a `type` (the struct
  inline), a `type?` (a pointer to one struct, `null` for NULL), a `List<T>` (a pointer to its items) whose count
  the compiler writes into the attribute named `<list>_count`, an enum (its C number), and a fixed-length array.
- **What C fills is read back by name.** `properties.limits.timestamp_period` instead of `read_float(720)`: a hand
  offset that is wrong reads a wrong value without a word, which is a D244 bug in today's engine code, and the
  declaration removes the offsets.
- **Zero runtime.** All of it is code the compiler writes at the call: the struct is built in the caller's frame,
  a `List` of numbers passes its own block, and a program that crosses no such struct carries none of it.

## Where the engine uses `Memory` today

The engine package (branch `naive`, `81b0e20`) names `Memory.Address`, `Memory.Heap` or `TypedMemory` in 32 files.
Two helpers carry almost all of it: `Raw` (the core folder's `raw.spite`, `read_long`, `write_integer`,
`read_byte` and the rest over `TypedMemory<T>` at `address + offset`) and `Asset.Bytes` (`asset/bytes.spite`, a
growable byte buffer on `Memory.Heap` with a read position). For foreign structs, `Structure`
(`render_vulkan/structure.spite`) is a zeroed heap block that structs are appended to field by field, aligning as
it goes, with `append_pointer(child)` and `append_text(text)` keeping what it points to alive.

### Bytes

| What the code is doing | Where | How it is written today |
|---|---|---|
| Read a whole file into memory | `Asset.Bytes.read_file`, the blend and PSD documents | `file.size()`, `heap.allocate`, `file.read_bytes(0, size, address)` |
| Read a record or a header from a position | the recipe store and record (`recipes/store.spite`, `recipes/record.spite`) | `file.read_bytes(position, 4, header)` into an 8-byte heap block, then `raw.read_integer(header, 0)` |
| Write and append records | the recipe store, `Asset.Bytes.save` | `file.write_bytes(address, count)`, `file.append_bytes(address, count)` |
| Read big-endian numbers and single bytes | the PNG decoder's chunks and header, the PSD document | `bytes.big_endian_integer(offset)`, `bytes.byte_at(offset)`, which adds 256 to a negative byte |
| Decode a stream into a table | the PNG and zstd Huffman and FSE tables | `heap.allocate(131072)` and `raw.write_integer(entries, slot * 4, entry)` |
| Build bytes to save or send | `Asset.Bytes.write_integer`, `write_text`, `zeroes`, the texture array | a growing heap block, `heap.resize` |
| Receive and send on a socket | the network channel (`network/channel.spite`) | `socket.read_bytes_now(inbound.address + inbound.length, chunk)`, `write_bytes_now(start, left)` |
| Copy bytes to memory C hands out | the Vulkan renderer's staging uploads | `raw.copy(source, frame.staging_mapped + offset, count)` after `vkMapMemory` |
| Lay out vertices for the GPU | the draw list, the software canvas, `Matrix4.write_to` | `raw.write_integer(at, 16, color)`, `floats.write_value(address, ...)` |

A decoder today, the PNG header:

```gdscript
func read_header(bytes: Asset.Bytes, body: Integer, length: Integer) {
    broken = length < 13
    assert not broken
    width = bytes.big_endian_integer(body)
    height = bytes.big_endian_integer(body + 4)
    depth = bytes.byte_at(body + 8)
    color_type = bytes.byte_at(body + 9)
```

where `byte_at` is `crash offset >= 0 and offset < length`, a `raw.read_byte`, and a fix for a sign the bytes
never had.

### Foreign structs

The Vulkan plugins construct 391 `Structure`s (124 in the renderer, 65 in the mesh renderer, 57 in post
processing, 42 in the pipeline builder, the rest in the passes), with 133 `append_pointer` and 20 `append_text`;
the Windows and XInput plugins allocate and read their structs on the heap directly.

| What the code is doing | Example | Count |
|---|---|---|
| Fill a create-info that points at other structs and arrays | `VkInstanceCreateInfo`, `VkDeviceCreateInfo`, `VkSubmitInfo`, `VkRenderingInfo`, descriptor writes, the pipeline states | most of the 391; 133 pointers |
| Point at text | application name, layer and extension names, shader entry point, a window class's name | 20 texts, plus `lstrcpyA` into a heap block for the window class |
| Pass an array of handles or numbers | `vkCmdBindVertexBuffers(..., buffers.address, offsets.address)`, fences, queue priorities | most of the 123 `Structure(8)` |
| Receive a handle or a count C writes | `vkCreate*` results, `vkEnumeratePhysicalDevices`' count, `vkMapMemory`'s address | the rest of the 123 `Structure(8)` |
| Read an array of structs C fills | queue families (24 bytes each, read at `index * 24`), surface formats | a few, all by offset |
| Read a struct C fills, with arrays inside | `VkPhysicalDeviceProperties` (`Structure(1024)`, name at offset 20, `timestampPeriod` at 720), features (55 `VkBool32`s by index) | a few, all by offset |
| Read a number-only struct | Win32 `MSG` (48 bytes), `RECT`, `XINPUT_STATE` (16 bytes, buttons read byte by byte) | every input poll |
| Fill a struct with a callback | `WNDCLASSA.lpfnWndProc` | one |

The number-only ones (`MSG`, `RECT`, `XINPUT_STATE`, the fence and semaphore create-infos) cross as a `type` today;
the engine wrote them on the heap because everything around them was. The rest cannot.

Instance creation today:

```gdscript
var application = Structure(64)
application.append_integer(0)
application.append_long(0)
application.append_text("the engine")
application.append_integer(1)
application.append_text("the engine")
application.append_integer(1)
var version = vulkan_version()
application.append_integer(version)
var extensions = Structure(16)
extensions.append_text("VK_KHR_surface")
extensions.append_text("VK_KHR_win32_surface")
var layers = Structure(8)
layers.append_text("VK_LAYER_KHRONOS_validation")
var creation = Structure(64)
creation.append_integer(1)
creation.append_long(0)
creation.append_integer(0)
creation.append_pointer(application)
creation.append_integer(layer_count)
creation.append_pointer(layers)
creation.append_integer(2)
creation.append_pointer(extensions)
var created = Structure(8)
var result = vulkan.vkCreateInstance(creation.address, no_allocator, created.address)
if result == -6 {
    raw.write_integer(creation.address, 32, 0)
    result = vulkan.vkCreateInstance(creation.address, no_allocator, created.address)
}
crash result == 0
instance = created.read_long(0)
```

The first `append_integer(0)` is a structure kind written as a bare number, `32` is the offset of
`enabledLayerCount` worked out by hand, and the `2` beside the extensions is a count kept in step with the list by
hand. Each is a place to be silently wrong.

## Part 1: bytes are a `List<Byte>`

### Rules (proposed by Claude, unconfirmed)

- **Files.** `file.read_bytes(): List<Byte>?` reads the whole file, `null` when it cannot be read.
  `file.read_bytes_at(position, count): List<Byte>?` reads up to `count` bytes from `position` (fewer at the end,
  none past it), `null` when it cannot be opened. `file.write_bytes(bytes): Boolean` replaces the content and
  `file.append_bytes(bytes): Long?` adds to the end, answering where they start. The `Memory.Address` forms are no
  longer open to programs ([question 314](#questions-for-mortaro)).
- **Sockets.** `socket.read_bytes(): List<Byte>?` waits until at least one byte arrives and answers what arrived,
  `null` once the connection is closed; `socket.read_bytes_now(): List<Byte>` answers what has arrived, empty for
  nothing yet (and `closed` tells "nothing yet" from "closed", as today). `write_bytes(bytes): Boolean` sends all of
  it; `write_bytes_now(bytes): Integer` hands the system what it takes now and answers how many. This is
  `UdpSocket`'s shape, whose `receive_now()` already answers a fresh `List<Byte>?`
  ([question 305](#questions-for-mortaro)).
- **Reading numbers.** A `List<Byte>` answers `read_short(position)`, `read_unsigned_short`, `read_integer`,
  `read_unsigned_integer`, `read_long`, `read_float` and `read_double`, little-endian, each a `T?` that is `null`
  when the bytes it would read are not all in the list: the names and the answer `MappedFile` already has, so a
  mapped file and a list of bytes read the same way. Each has a big-endian twin, `read_integer_big_endian(position)`
  and so on ([question 306](#questions-for-mortaro)). On a list of anything but `Byte` they are a compile error, as
  `to_utf8_text()` is.
- **Writing numbers.** `bytes.append_integer(value)` and the other widths add a number at the end, with
  big-endian twins; `bytes.write_integer(position, value)` replaces one in place, halting when it is not inside the
  list (an index the program chose, not data from outside). `bytes.append_bytes(other, start, count)` copies a run
  of another list to the end, the one bulk copy.
- **Records.** A header or record of fixed-width numbers is a class or a `type` read with
  `BinaryReader<T>().read_from(bytes, position)`, whose format is already the numbers in declaration order,
  packed and little-endian ([json.md](../../docs/json.md#what-each-type-becomes)). A big-endian record is read number
  by number ([question 307](#questions-for-mortaro)).
- **A table of numbers** (a Huffman table, a decode window) is a `List<Integer>` or a `List<Byte>` grown to its
  size; nothing new is needed, since a list of numbers is already one block.
- **Bytes to and from C.** A `List<Byte>`, like every list of numbers, already crosses a foreign call as its block,
  C's writes coming back. Memory C hands out (a mapped GPU buffer) is [question 308](#questions-for-mortaro).
- `BinaryReader.read_memory(address, count)` is no longer open to programs: `read(bytes)` and
  `read_from(bytes, start)` cover a buffer a socket filled.

### Before and after

The PNG header:

```gdscript
func read_header(bytes: List<Byte>, body: Integer) {
    assert body + 13 <= bytes.count()
    width = bytes.read_unsigned_integer_big_endian(body)
    height = bytes.read_unsigned_integer_big_endian(body + 4)
    depth = bytes[body + 8]
    color_type = bytes[body + 9]
```

The `assert` is the one line of checking, and it proves every read after it (below), so none of them is a `T?`
and none tests anything at run time. `length` and `broken` go: the list knows its count, and a `Byte` is unsigned,
so the sign fix goes too.

Reading a whole asset, `Asset.Bytes.read_file` (19 lines and a heap block) becomes:

```gdscript
var bytes = File(path).read_bytes()
assert bytes
```

A recipe record's header, read today into an 8-byte heap block:

```gdscript
var header = file.read_bytes_at(position, 4)
assert header
var key_length = header.read_integer(0)
assert key_length
```

The network channel's receive loop, which today grows `inbound` by hand and passes `inbound.address +
inbound.length`:

```gdscript
func receive_all() {
    var received = socket.read_bytes_now()
    while not received.is_empty() and inbound.count() < inbound_limit {
        inbound.append_bytes(received, 0, received.count())
        received = socket.read_bytes_now()
    }
    if socket.closed {
        ended = true
    }
}
```

and a frame written for sending is `BinaryWriter<Move>().append_to(move, outbound)` followed by
`socket.write_bytes_now(outbound)`.

### How it is as fast as the hand code

- **A read at a proven position is one load.** `bytes.read_integer(position)` compiles to one unaligned 4-byte
  load from the list's block (a `memcpy` the C compiler turns into one instruction), the big-endian twin to the
  same load and one byte swap, `bytes[index]` to one byte load. That is what `Raw` compiles to today, minus its
  call.
- **The bound is proven, not checked**, by extending [a proven count or bound proves a
  read](../../docs/proofs.md#a-proven-count-or-bound-proves-a-read) to reads of a width: `position + 4 <=
  bytes.count()` proves `read_integer(position)`, and `body + 13 <= bytes.count()` proves every read of width `w`
  at `body + k` with `k + w <= 13`. Decoders need one more step the proof lists as not traced today: an index built
  from loop counters (`row * stride + column`, bounded by `row < height` and `column < stride`, under one `assert
  height * stride <= bytes.count()`). That is the proof this proposal asks the compiler to add, and it is the same
  one image and audio loops over `List<Float>` need. Unproven, the read is a `T?` and must be narrowed: the program
  says where it checks, and the check is never silent.
- **No allocation that the hand code did not make.** A list read and let go inside one block lives in the frame
  when its size is a literal up to 256 bytes ([local lists in the
  frame](../../docs/proofs.md#local-and-variadic-lists-in-the-frame)); otherwise it is one heap block, the same one
  `Asset.Bytes` allocated. A socket read answered fresh every tick is the allocation the hand code avoided by
  reusing `inbound`; question 305 is whether the compiler removes it (the block of a list let go at the end of a
  pass is reused by the next pass's read at the same site, stage 5's ring of buffers) or the program appends into a
  list it keeps.
- **Tree-shaken.** Each reading and writing function is an ordinary `List` function the program either calls or
  does not; a program that reads no bytes carries none of them.
- **Layout.** A `List<Byte>` is one block of bytes, as today; nothing about the list's storage changes.

### What stays loud (D244)

- A read past the end answers `null`, which the program must narrow; a write at a position outside the list halts,
  naming the write and the count.
- A file that cannot be read is `null`, never an empty list; an empty file is an empty list.
- A socket's "nothing yet" and "closed" are told apart by `closed`, never by a count of `-1`.
- The reading functions on a `List<Integer>` are a compile error, so a list of the wrong kind is never read as
  bytes.

## Part 2: a foreign struct is a `type`

### Rules (proposed by Claude, unconfirmed)

A `type` crosses a foreign call as a C struct, by address, when every attribute is one of:

| Attribute | Crosses as | Read back after the call |
|---|---|---|
| a number (as today) | the C type of its width | C's write comes back, as today |
| `Boolean` | a 32-bit integer (`VkBool32`, `BOOL`), as a callback takes one ([question 313](#questions-for-mortaro)) | `!= 0` |
| an enum value | its C number, 32 bits ([question 313](#questions-for-mortaro)) | a number the enum does not list halts, naming the enum and the number |
| `String` | `const char*`, the text's own bytes, which already end in a zero | a pointer C wrote is copied up to its zero |
| `String?` | the same, `NULL` for `null` | `null` for `NULL` |
| a `type` that crosses | the struct inline | field by field |
| a `type?` that crosses | a pointer to that struct, `NULL` for `null` ([question 309](#questions-for-mortaro)) | not read back: C does not write through a create-info |
| `List<T>` of numbers, of a `type` that crosses or of `String` | a pointer to its items | C's writes to the items come back |
| `List<T>?` of the same | the same, `NULL` for `null` | the same |
| `ForeignCallback?` | its function pointer, `NULL` for `null` | not read back |
| a fixed-length array, `List<T>(32)` or `String(256)` ([question 311](#questions-for-mortaro)) | the array inline | the items, or the text up to its zero |

- **The count is the compiler's.** An attribute named `<list>_count` declared directly before a `List` attribute
  `<list>` is written by the compiler from the list's count at the call. Writing it in a literal or assigning it is
  a compile error naming the list. A `List` declared directly after another `List` with no count of its own shares
  the earlier list's count (`VkSubmitInfo`'s `pWaitDstStageMask`, a subpass's resolve attachments); when the two
  counts can differ the call halts naming both lists, and a `List?` that is `null` is not compared.
- **Layout is C's.** The compiler lays the struct out with C's alignment rules, padding included, so the binding
  writes the fields in C's order and nothing else (no `padding` field for alignment; only one a union leaves behind,
  as today). With a header and the `'windows'` rule the size is checked against the header's struct, as today; a
  binding given its library's header gets the check for every struct.
- **Lifetime is the call.** Everything a struct reaches is held for the foreign call it is passed to, and no
  longer: text, lists, nested structs and callbacks. C that keeps a pointer past the call is
  [question 312](#questions-for-mortaro).
- **A list of structs C fills.** `list.grow_to(count)` appends each item's default until the list holds `count`
  (a name for the one missing piece, proposed here); the two-call enumerate idiom is then two calls with the same
  list.
- **Still refused**, with today's error naming the type: a class instance, a `Dictionary`, a list of class
  instances, a list of lists, and a union past its first member.

### Before and after

The declarations, written once in the binding, in C's field order with Spite names:

```gdscript
enum StructureKind {
    'application_information' = 0
    'instance_creation' = 1
    'device_queue_creation' = 2
    'device_creation' = 3
    'submission' = 4
}

type ApplicationInformation {
    kind: StructureKind
    next: Long
    application_name: String
    application_version: UnsignedInteger
    engine_name: String
    engine_version: UnsignedInteger
    api_version: UnsignedInteger
}

type InstanceCreation {
    kind: StructureKind
    next: Long
    flags: UnsignedInteger
    application: ApplicationInformation?
    layer_names_count: UnsignedInteger
    layer_names: List<String>
    extension_names_count: UnsignedInteger
    extension_names: List<String>
}

type Answer {
    value: Long
}
```

Instance creation:

```gdscript
func create_instance(): Boolean {
    var version = vulkan_version()
    var layers = ["VK_LAYER_KHRONOS_validation"]
    if build.optimized {
        layers.clear()
    }
    var creation: InstanceCreation = {
        kind: 'instance_creation'
        next: 0
        flags: 0
        application: {
            kind: 'application_information'
            next: 0
            application_name: "the engine"
            application_version: 1
            engine_name: "the engine"
            engine_version: 1
            api_version: version
        }
        layer_names: layers
        extension_names: ["VK_KHR_surface", "VK_KHR_win32_surface"]
    }
    var created: Answer = {value: 0}
    var result = vulkan.vkCreateInstance(creation, 0, created)
    var validated = result == 0 and not layers.is_empty()
    if result == -6 {
        creation.layer_names = []
        result = vulkan.vkCreateInstance(creation, 0, created)
    }
    crash result == 0
    instance = created.value
    return validated
}
```

The offset `32`, the counts and the structure kinds as bare numbers are gone; `next: 0` stays a number, a NULL,
until a chain is wanted, when it is a `type?` of the chained struct.

A submission, where today one `semaphore_count` is written by hand for two arrays:

```gdscript
type Submission {
    kind: StructureKind
    next: Long
    wait_semaphores_count: UnsignedInteger
    wait_semaphores: List<Long>
    wait_stages: List<UnsignedInteger>
    command_buffers_count: UnsignedInteger
    command_buffers: List<Long>
    signal_semaphores_count: UnsignedInteger
    signal_semaphores: List<Long>
}
```
```gdscript
var waits = List<Long>()
var stages = List<UnsignedInteger>()
var signals = List<Long>()
if wait_for_image {
    waits.append(frame.image_ready)
    stages.append(4096)
    signals.append(frame_done)
}
var submission: Submission = {
    kind: 'submission'
    next: 0
    wait_semaphores: waits
    wait_stages: stages
    command_buffers: [frame.command_buffer]
    signal_semaphores: signals
}
var result = vulkan.vkQueueSubmit(queue, 1, submission, frame.fence)
crash result == 0
```

An array of handles needs nothing new, since a `List` of numbers already crosses as its items:

```gdscript
vulkan.vkCmdBindVertexBuffers(frame.command_buffer, 0, 1, [frame.rectangle_buffer], [0])
```

A struct C fills, with arrays inside, and the queue families C lists:

```gdscript
type PhysicalDeviceProperties {
    api_version: UnsignedInteger
    driver_version: UnsignedInteger
    vendor: UnsignedInteger
    device: UnsignedInteger
    device_kind: UnsignedInteger
    device_name: String(256)
    pipeline_cache_identity: List<Byte>(16)
    limits: PhysicalDeviceLimits
    sparse_properties: PhysicalDeviceSparseProperties
}

type QueueFamily {
    flags: UnsignedInteger
    queue_count: UnsignedInteger
    timestamp_valid_bits: UnsignedInteger
    granularity: Extent
}
```
```gdscript
var properties = PhysicalDeviceProperties()
vulkan.vkGetPhysicalDeviceProperties(physical_device, properties)
chosen.name = properties.device_name
timestamp_period = properties.limits.timestamp_period

var found: Answer = {value: 0}
vulkan.vkGetPhysicalDeviceQueueFamilyProperties(candidate, found, 0)
var families = List<QueueFamily>()
families.grow_to(found.value)
vulkan.vkGetPhysicalDeviceQueueFamilyProperties(candidate, found, families)
```

`PhysicalDeviceLimits` is a hundred-odd fields written out once in the binding, which is the price of reading any
of them by name; the engine reads one today at the offset `720`.

The Windows window class, which today copies its name into a heap block with `lstrcpyA` and writes nine longs at
offsets:

```gdscript
type WindowClass {
    style: UnsignedInteger
    procedure: Long
    class_extra_bytes: Integer
    window_extra_bytes: Integer
    instance: Long
    icon: Long
    cursor: Long
    background: Long
    menu_name: String?
    class_name: String
}
```
```gdscript
var window_class: WindowClass = {
    style: 11
    procedure: default_procedure
    class_extra_bytes: 0
    window_extra_bytes: 0
    instance: instance
    icon: 0
    cursor: arrow
    background: 0
    menu_name: null
    class_name: class_name
}
var atom = user.RegisterClassA(window_class)
crash atom != 0
```

And a message, which crosses as a `type` today:

```gdscript
type Message {
    window: Long
    kind: UnsignedInteger
    word: UnsignedLong
    long_word: Long
    time: UnsignedInteger
    point: Point
    private_data: UnsignedInteger
}
```
```gdscript
var message = Message()
while user.PeekMessageA(message, 0, 0, 0, 1) != 0 {
    deliver(windows, message)
    user.TranslateMessage(message)
    user.DispatchMessageA(message)
}
```

### How it is as fast as the hand code

- **The struct is built in the caller's frame.** A `type` literal at a foreign call is the C struct itself, laid
  out by C and written field by field where the call is, which is what `Structure` does on the heap, without the
  heap and without its zeroing loop. A nested `type?` is a second struct in the frame and its address.
- **Pointers are what the values already are.** A `String`'s bytes already end in a zero, so `const char*` is its
  pointer; a `List` of numbers is already its block. Only `List<String>` needs a new array (one pointer per text),
  built in the frame for up to 32 texts and on the heap beyond, and freed after the call.
- **A list of structs is stored in C's layout where it crosses.** A `List<T>` of a `type` that reaches a foreign
  call keeps its items in C's layout, one block (the representation per use of D520, where the use fixes it).
  When another use of the same list has the compiler choose another layout, the conversion is written at the call,
  and `--optimization-report` names it; it is never a silent cost.
- **Counts are folded.** A list literal's count is a constant, and a list's count is one load.
- **Reading back costs only what is read.** C writes the struct in place; reading `properties.device_name` copies
  that text and nothing else, and a field nobody reads is never copied.
- **Tree-shaken.** The C struct, its builder and its read-back exist only for the types a surviving call passes;
  a program with no foreign struct carries none of it, and no registry or marshalling layer exists in any program.

What the compiler must prove: that nothing a struct points to is let go or resized during the call (it holds them
for the call, so this is by construction, as a list argument is today); that two lists sharing a count are the
same length, which it proves for literals and lists built by the same appends and checks once otherwise; and that a
list of structs crossing a call has, at that use, C's layout.

### What stays loud (D244)

- A count written by hand is a compile error, so it can never disagree with its list; two lists of one count that
  differ halt at the call, naming both.
- An enum field C fills with a number the enum does not list halts at the read, naming the enum, as a binding's
  enum does today.
- A fixed-length text C fills without a zero inside its length halts at the read, naming the attribute.
- A `type` that cannot cross is an error at the call naming the type and the attribute, as today.
- A field C writes past what the struct declares is C's fault, and the fault report names the library and the
  line, as for any foreign call. The size check against a header catches a missing field at build time; giving each
  binding its header turns that on.

## Questions for Mortaro

Each is listed in `mortaros_missing_decisions.md` under "Open" with the same number.

- **305, a fresh list or a list the program keeps.** A file or socket read answers bytes. (a) Every read answers a
  fresh `List<Byte>`, as `UdpSocket.receive_now()` does, and the compiler reuses the block when the last one is let
  go before the next read at the same place. (b) Every read appends to a `List<Byte>` the program passes, which
  allocates nothing in any build, and a stream is a list the program keeps and trims. Recommendation: (a), one
  shape for every byte source, with the reuse as stage 5 work and the cost listed in `--optimization-report` until
  it is built.
- **306, the big-endian names.** (a) `read_integer_big_endian(position)` beside `read_integer(position)`. (b) A
  byte-order enum argument, `read_integer(position, 'big_endian')`. (c) Only little-endian reads, and a number's
  `bytes_reversed()` for the rest. Recommendation: (a): no option argument, and the format's order is part of what
  the call means.
- **307, two ways to read a header.** A record of numbers can be read by `BinaryReader<T>().read_from(bytes,
  position)` or number by number. (a) Keep both: one is a record, the other one number. (b) Only number by number.
  (c) Only `BinaryReader`, which then needs a big-endian form. Recommendation: (a), since they read different things.
- **308, memory C hands out.** A mapped GPU buffer is an address C owns with a size the program knows. (a) A
  library class made from a foreign call's answer and its size, `ForeignBytes(address, count)`, answering
  `write_bytes(position, bytes)` and the same number writes as a `List<Byte>`, bounds checked. (b) It stays on the
  `Memory` floor, inside bindings only. Recommendation: (a), so the renderer's staging uploads leave `Memory`.
- **309, inline and pointer.** (a) An attribute of a `type` is the struct inline; of a `type?`, a pointer, `null`
  for NULL. (b) Every `type` attribute is a pointer, and an inline struct is written out field by field. (c) A
  library marker class for a pointer. Recommendation: (a): it reads as the meaning (a pointer may be NULL), and it
  adds no pointer type.
- **310, a value C writes through a pointer argument.** `vkCreate*` writes a handle, `vkEnumerate*` a count.
  (a) A one-attribute `type` (`Answer`), which works today. (b) A number `var` at a pointer parameter is passed by
  address, which needs the header to know the parameter is a pointer (the engine's bindings name none). (c) A
  one-item `List<Long>`. Recommendation: (a): explicit at the call, no new rule, and the binding hides it behind
  `create_instance(): Instance?`.
- **311, a fixed-length array inside a struct.** `char deviceName[256]`, `VkMemoryType memoryTypes[32]`,
  `float blendConstants[4]`. This is the one new syntax: the length must be known while compiling. (a) A length in
  parentheses on the attribute's type, `List<MemoryType>(32)` and `String(256)`, read like a constructor call.
  (b) A number as a generic argument, `List<MemoryType, 32>`. (c) No syntax: write out each item as its own
  attribute. Recommendation: (a), allowed only in a `type` that crosses into C.
- **312, C keeping a pointer past the call.** A sound buffer C plays from after the call returns. (a) Out of
  scope: the struct's lifetime is the call, and such data stays on the `Memory` floor in the binding. (b) A library
  class like `ForeignCallback` that holds a list for as long as the program keeps it and refuses, at compile time,
  any line that changes the list's size while it is held. Recommendation: (b), built only when a binding needs it;
  until then (a).
- **313, Boolean and enum widths in a struct.** (a) A `Boolean` attribute is 32 bits and an enum 32 bits, as a
  callback already takes a `Boolean`; C's one-byte `bool` is a `Byte`. (b) Their Spite widths (1 byte each), and the
  binding writes `UnsignedInteger` where C wants 32 bits. Recommendation: (a): every C API the engine binds uses
  32-bit booleans and enums, and (b) reads a wrong value silently when the binding forgets.
- **314, where the address forms go.** (a) The `Memory.Address` forms of `File`, `Socket` and `BinaryReader`
  are removed now, and `library/` keeps them as private functions. (b) They move into the `Optimizer` namespace
  with the rest of `Memory` (D506, later). Recommendation: (a): two ways to read a file is the thing to avoid, and
  nothing a program does needs them once these forms exist.
- **315, the count naming rule.** (a) The attribute named `<list>_count` directly before the list is the
  compiler's. (b) Any number attribute directly before a list is its count. (c) The count is not declared and the
  compiler inserts it, which hides a field C has. Recommendation: (a): a naming convention the reader can see, and
  a struct with a number before a list that is not its count (rare) still says what it means.

## Order of work, if accepted

1. The `List<Byte>` reading and writing functions and the file and socket forms, with the width-aware bound proof:
   library work and one proof, no syntax. The engine's `Raw`, `Asset.Bytes`, decoders, recipe store and network
   channel move to them.
2. Foreign `type`s with text, nested types, `type?`, lists with compiler counts, enums and callbacks: compiler
   work at the foreign call. The Windows and XInput plugins move first (few structs), then Vulkan.
3. Fixed-length arrays, once question 311 is answered; then `PhysicalDeviceProperties` and the feature structs.
4. `ForeignBytes` (question 308), and the renderer's uploads.

Each step updates the docs pages that teach it (`standard_library.md`, `collections.md`, `foreign_libraries.md`),
their specs, `proofs.md` for the width-aware bound and `optimizations.md` for the frame-built struct, in the same
commit as the code.
