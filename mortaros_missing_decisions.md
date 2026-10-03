# Decisions waiting on Mortaro

Only questions Mortaro still has to answer. An agent adds one here with a link to where the docs argue it; when he
answers (inline or in `mortaros_notes.md`), the answer goes into the page that teaches it and a row of
`design/decisions.md`, and the item is deleted. Item numbers never change. Each item: the question, the options,
the recommendation, what it blocks. "D244" marks something that can go wrong silently today; "D205" one Claude
could decide itself. The principles that answer most questions before they are asked: the moron (anyone who uses
Spite) never chooses, the language does; performance choices belong to the compiler; whatever can be written in
Spite is written in Spite (D350); reflection exposes everything and tree-shakes what is not read (D353); one way to do each
thing; prevent mistakes rather than offer options; storage owns its items and other references are `T?` (D354).

Every other item is answered: the 14 principle answers by D369, and every confirmation of what agents decided by
D370, 76 by D371, 235 by D372, 161 by D373 and 105 by
D374 (2026-10-01); 264 by D378 and 250 by D379, 252 by D380, 254 by D381, 256 by D383, 257 by D382, 258 by D384, 261 by D385, 253 by D386, 255 by D387, 259 by D388, 260 by D389, 262 by D390, 251 by D391, 263 by D394 (2026-10-02); 266 to 296 by D468 to D488, and 268, 269, 271 and 276 to 279 by D493 to D497, 298 by D498 (2026-10-03). Earlier answers are listed in each row of `design/decisions.md`.

## Open

275. **Loading a subfolder of a package** (D364 says never `load "kal/physics"`). It is not enforced: when only the
     subfolder is loaded the compiler cannot tell that `kal/` is a package root. Options: (a) leave it unenforced;
     (b) refuse a load whose folder lies inside another loaded root, once both loads are seen. Recommend (a) now
     and (b) when both loads are visible.
280. **How a walk passes values beyond the element.** `each` hands only the element; a walk that needs more (a
     value, a writer) keeps it in attributes of the walking object today. Options: (a) that is the way (one
     argument, state in the walker, as D384's serializers do); (b) `each` with extra arguments. Recommend (a).
     Found building backlog J1 (D244): (a) is fine where the walker is made per call (`JsonWriter`, `JsonReader`),
     but `BinaryFormat<T>` and `Spite.DebugInstance<T>` are singletons every thread shares, so state in them
     corrupts memory when two `Parallel`s write or show a value at once (tried), and a walker made per value would
     cost an allocation per nested object and per enum value that the binary format does not make today. With (a),
     the binary walk needs a walker made per call, or a reentrant per-call lock from the compiler; with (b), it stays
     a stateless singleton at no cost. `BinaryFormat` keeps its plural walk until this is answered; now recommend
     (b), the only form that keeps the binary format at zero allocations without shared state. Blocks backlog J1's
     binary half and R7.
281. **When a waiting loop counts as a hang** (backlog B6 and B15; failure.md's open list). The polling half is
     settled by D475: a loop that reads `finished` and calls nothing is a compile error, and the million-poll halt
     stays as the backstop. Left: a `while true` that can never leave but calls something hangs without a word
     outside `Parallel` work and locked singletons. Options: (a) a compile error wherever no exit is reachable and
     no called function can end the program or wait; (b) a run-time halt after a long time with no wait and no
     output; (c) both. Recommend (a): what the compiler can see is refused while compiling (D474), and a halt
     guessed from time would misfire on long computations.
284. **A dropped `ForeignCallback`'s ticket table is C** (D361; backlog F3, built). A context callback now hands C
     a ticket (a table slot and a reuse count), so a call through a dropped context halts naming the function. The
     table is C the generator writes, carried only by programs that make context callbacks. Options: (a) keep it C
     until C9 gives the backend primitives a home; (b) write it now as a library singleton the emitted C calls.
     Recommend (a): it is a few lines beside the callback C that already exists, and C9 moves both together.
291. **Which values a failed assert keeps for the crash report** (backlog X1). Text and objects may be freed before a
     crash prints them. Options: (a) numbers, Booleans and enum values kept as they are, text as its length only;
     (b) keep a reference to each, a count on every failed assert; (c) no values, as the docs say today. Recommend
     (a).
297. **Values and generic classes as union members** (D459, D244; values_and_types.md "Unions in full"). A union
     with `String`, a number, `Boolean` or `List<Byte>` among its members was accepted and then failed later with
     an error that did not name the cause. It is now refused where the union is declared, saying to wrap the value
     in a class of the program's own. Options: (a) **refuse them**, as built: a union is a set of classes a `switch`
     tests by the header's class, which a value does not have; (b) make them work: a value member boxed with a tag
     (as `String?` already is), and a generic member tested with its codegen values (`List<Byte>`, as
     `found == Storage<$component_type>` is). Recommend (a): one representation of a union, and wrapping costs one
     small class. Blocks nothing; (b) can be built later without breaking a program written for (a).
299. **Frame arenas: the first step of automatic memory** (D485; [proposal](design/proposals/automatic_memory.md)).
     The proposal makes "a frame" a pass of a loop that ends in a wait (`program.sleep`, a present, a `Concurrent`
     wait), and puts a value made in a pass and proven never to outlive it into an arena the compiler resets at the
     end of the pass; anything else stays on the heap, so nothing is freed early. Options: (a) build that first, as
     the proposal orders it; (b) start with structure of arrays for lists, the larger win for the engine; (c) another
     order. Recommend (a): it reuses the placement proof and deletes the engine's hand scratch pools. Blocks
     backlog E1 and the rest of the proposal.
300. **How a signature names a number's fractional class** (D486, answering item 290). D486 makes
     `Vector3(3, 4, 12).normalized()` a `Vector3<Float>`, but a generic class can only name `$number_type` itself
     in a signature, so `Vector3<$number_type>.normalized()` has no way to say "the fractional class of
     `$number_type`". Options: (a) a type the compiler works out, `Fraction<$number_type>` (`Float` for every whole
     number up to 32 bits, `Double` for `Long` and `UnsignedLong`, the class itself for `Float` and `Double`), so
     the library writes `func normalized(): Vector3<Fraction<$number_type>>`; (b) a codegen value with a default
     worked out from another, `generic $fraction_type = $number_type.fraction`. Recommend (a): one type, readable where it is used, and nothing a program
     writes changes. Blocks building D486.
301. **Should the binary pair be reused the same way?** (D478.) `JsonWriter<T>()` and `JsonReader<T>()` are now made
     once and called with each value or text. `BinaryWriter(value)` and `BinaryReader<T>(bytes)` still take their
     input in the constructor, since a binary reader is a cursor over one buffer (`position`, `remaining()`, many
     values read in turn) and `append_to(bytes)` writes the value into a list the program has. Options: (a) keep the
     binary pair as it is: the reader is a cursor, and the writer is one value; (b) the same shape as JSON,
     `BinaryWriter<T>()` with `write(value)` and `append_to(value, bytes)`, and `BinaryReader<T>()` with
     `read(bytes)`, giving up the cursor; (c) a writer like JSON's and the reader as a cursor. Recommend (c): a
     reusable writer costs nothing, and the cursor is what reading many values from one socket buffer needs.
302. **How a class file overrides a function of its class object** (D483, answering item 270: "anything can be
     overridden"). A class file's functions are its instances' functions: `func to_string()` in `gadget.spite` is how
     a gadget prints. `Spite.Class` declares functions of the class object too (`Gadget.has_function(name)`,
     `Gadget.get_name()`, `to_string()` of the class itself), and today a class file cannot replace those, since the
     same name in the file means the instance's function. Options: (a) a function the file marks as the class's own,
     `func Gadget.has_function(name: String): Boolean`, a dotted name naming the class object; (b) a file
     `gadget/class.spite` beside the class that reopens its class object; (c) only through reopening `Spite.Class`
     for every class at once. Recommend (a): one line says what it replaces, and it reads like the call it changes.
     Blocks the class-object half of D483 (backlog R8); the instance half (getters, setters, `to_string`, `copy`) is
     replaceable today.
