# Backlog

Everything decided and not yet built, in one place, so the work can be ordered and split between sessions. The
rule lives on its docs page, the reason in [decisions.md](decisions.md), and the gap in [status.md](status.md);
this page only says what to build, how big it is, what it waits on and who can build it at the same time. When an
item lands, delete it here and its line in status.md in the same commit.

Drafted 2026-10-02 from `master` (decisions to D376) and the unmerged branches below.
Sizes: **S** under a day, **M** a few days, **L** a week or more. File names are relative to the repository;
`generator.spite` is `bootstrap/source/generation/generator.spite` (about 43 800 lines), `bootstrap.spite` is
`bootstrap/bootstrap.spite`.

## Landing from branches (do not build again)

- **Landed:** `cloud/linux` (one seed per system, the Linux check, D376) on 2026-10-02.
- **Landed:** `cloud/operators` (D315 and D365, recorded as D396), the reload races and `wip/fastbuild` (D403) on
  2026-10-02.
- **Landed:** `cloud/nomap` (no `List.map(function)`, no `map_` over a test, no class-qualified function value,
  recorded as D415) on 2026-10-02.

## Items

### Reflection migration (D316, D317, D318, D335, D330; status.md "Reflection known while compiling")

- **R1 Plural collection leftovers** (D317, D328). The library's templates collect with `map_members` and every
  call is plural (built on master, `4291cca`), and `Spite.DebugInstance` walks with `each`. Left: `String.Inflection`, the irregulars table a program reopens,
  which the compiler must read from the program's `String` rather than its own copy (`library/string.spite`,
  the inflection lookup near generator.spite's plural errors). **S.** No dependencies left.
- **R4 Dictionary member templates over values** (D335). Built through the compiler; `library/dictionary.spite`
  declares none of them. Check they go through declared library templates like `List`'s (D240: nothing hidden).
  **S.** No dependencies.
- **R5 The rest of the object model** (D316, D317; status "Not built" and "Run time only through the old tables").
  Specialisation of enum parameters (which calls it covers needs Mortaro); `.owner`, `.element_type` and
  `.value_type` on run-time objects (`.owner` with R8).
  Files: generator.spite reflection and specialisation regions, `specialisation.spite`, `reflected.spite`,
  `library/spite/*.spite`. **L.** No dependencies.
- **R8 Declarations and bound members are different classes** (D391). Built for attributes:
  `Spite.AttributeDeclaration`, `Monster.attributes` (folded and at run time) against `troll.attributes`, the
  template parameter `Spite.AttributeDeclaration<...>` (D392) everywhere. Left: `Spite.FunctionDeclaration` for
  `Monster.functions` (today a list of `Spite.Function`s bound to a stand-in, which the run-time `.accesses` table
  and `has_function`/`function_waits` read through the hidden typed-call pointer), `.owner` the instance on bound
  members, and `call_with` removed. Files: `library/spite/function.spite`, `class.spite`, generator.spite's
  functions lists and accesses table. **M.**
- **R9 `Spite.Call`** (D391, D393). `Spite.Call(declaration, instance)`, `.arguments['name'] = value`, `call()`;
  folded into the direct
  call when everything is known while compiling; an unfilled argument is a compile error where visible, a halt
  naming it otherwise; an instance not of the declaring class is a compile error (D393). **M.**
  Depends on R8.
- **R7 Remove the old Symbol machinery and plural walks** (D316, D317; proposal section 11). Make each old form an
  error naming its new spelling, migrate, then delete: `Symbol<...>` templates and plural walks, `Symbol<$T.f>`
  argument walks, name patterns and folder ranges, `$T.has_function`, `function_waits`, `argument_count`,
  `fits_vector`, `function_writes_parameter`, `source_folder`, `name_fits`, `waits()` in `library/spite/*.spite`,
  and `template_walk.spite`, `namespace_walk.spite`, `old_spellings.spite` and about 90 generator functions. About
  100 `Symbol<` lines in 75 files (conformance/stage6, diagnostics, benchmarks, `docs/memory.md`, `json.md`,
  `collections.md`, the diagnostic "write 'member: Symbol<$element_type>'") and about 120 name-keyed questions,
  plus the game engine package (17 files, outside this repository). Retires D114, D115, D180's pattern holes, D209,
  D219, D261, D288. **L.** Depends on R1, R5, J1.
- **R8 Overriding functions of `Spite.Class`** (status "Functions of `Spite.Class`"). The three override rules
  (only what `Spite.Class` declares, a colliding instance function is an error, the override folds), and
  `--final-classes` naming the root of a changed default (with C7). Files: generator.spite class-function
  resolution, `library/spite/class.spite`. **M.** No dependencies.

### Serialization (D319, D320, D384)

- **J1 Binary as library Spite specialised per class** (D319, D384). The JSON pair and `Spite.DebugInstance` walk
  with `each`, and all four serializers skip private and singleton attributes. Left: `binary_format.spite` still
  walks a class's attributes and an enum's values with `Symbol<$value_type>` plurals, since an `each` walk would
  keep the cursor in `BinaryFormat`, a singleton every thread shares (status.md, json.md). Waits on
  `mortaros_missing_decisions.md` item 280. **M.**
- **J2 Rename map keyed by attribute objects** (D320, D329). `JsonWriter<Monster>({Monster.attributes['health']:
  "hp"})`, the reader taking the same map, every serializer the same kind; a constant map folds into literal keys
  and a generated `switch`, a run-time map fills a key table once per serializer. Needs a `Dictionary` keyed by
  `Spite.Attribute` (hashing an attribute object). The REPL's `reload {...}` text parsing (D333) moves to the real
  map once the prompt evaluates map literals (P1). Errors for a missing, private or singleton attribute.
  Files: `library/dictionary.spite`, `library/spite/attribute.spite`, `json_writer.spite`, `json_reader.spite`,
  `binary_format.spite`. **M.** Waits on `mortaros_missing_decisions.md` item 283 (how the map is passed).
- **J3 Reading JSON whose shape is not known.** A program that inspects an unknown file (a tool, an importer) has
  no value to read it into: `JsonReader<T>` needs a class. Add an untyped value, a union of object, list, text,
  number, boolean and null (names proposed by Claude, unconfirmed), read by `JsonReader` and walked with `switch`.
  Files: `library/json_reader.spite`, a new value class, docs json.md. **M.** No dependencies.

### Types, monomorphisation and storage (D321, D331, D332, D355, D367, D368, D370, D398, D399, D400, D401, D402)

- **M1 D321 leftovers** (status "Inline types and duck typing"). (a) text and `Symbol` stored as a `type` are
  tagged, not boxed; (b) the closed set at a run-time spot is the classes that reach that spot, not every class
  admitted to the `type`; (c) a nullable `type` parameter compiled per class plus the null case, the tag carrying
  "no value" (D369 item 246); (d) a parameter the function assigns, a function that waits, the library containers'
  functions, and a program with a `Concurrent` all get copies instead of the version as written; (e) a class that is
  never instantiated is not emitted. Files: generator.spite copy and dispatch regions, `dispatch_classes.spite`,
  `type_shape.spite`, `tree_shaker.spite`. **L** (a, c, e are M each; b, d are the long part). No dependencies.
- **M2 Operators and calls through a `type` are the class's own** (D367, D368; status "Every number fits
  `Number`"). Replace D345's "converted to the left side's class, must fit exactly" with the ordinary casting
  rules (widening automatic, narrowing a compile error or a halt), and let a call or operator through a `type` on a
  class instance known only at run time reach that class's function through the dispatch in REPL and hot-reload
  builds (today it halts "was given a value of a class it is not compiled for"). Files: generator.spite's
  `___operate_` dispatch, `dispatch_classes.spite`. **M.** Depends on nothing; M5 needs it.
- **M3 List storage by concrete class** (D331, D222 study in proposals/one_list.md). Step 1: a list over a type or
  union stored as one array per concrete class plus an order array; step 2: `filter_<classes>()` answers from that
  array; step 3: partitions per value filter, only where a production benchmark wins (D322). Files:
  `library/list.spite`, `items.spite`, `vector.spite`, generator.spite list layout, `placement.spite`. **L.**
  Depends on M1(b), M4.
- **M4 The optimisation report** (D36, D332, D389; status "Other optimisations"). A build lists what could not be
  optimised and why: every list falling back to references (D332's to-do list), copies not elided, objects not
  placed in the frame, and later every remaining overflow check (N2). Written only when asked, by `--optimization-report` (name unconfirmed), in `--final-classes`' place and style, each entry linking its source line (D389). Files:
  generator.spite (a collector), `bootstrap.spite` (writing it). **M.** No dependencies; M3 and N2 report into it.
- **M5 Vectors and maths generic over `Number`** (D355, D381). `Vector2`, `Vector3`, `Vector4` and `Quaternion` (and
  `Matrix3`/`Matrix4`, `Plane`, `Ray`, `AxisAlignedBox`, `CubicBezier` where it makes sense) take `generic
  $number_type` constrained by `Number`, inferred from the constructor (`Vector3(1, 2, 3)` is
  `Vector3<Integer>`), one packed copy per class; every member answers the vector's own class, whole numbers truncated like
  integer division (D381); game_maths.md points grid code at `length_squared()` and notes `normalized()` snaps
  toward a grid direction. Migrate `examples/`, the corpus and the game engine package. **L.** Depends on M1(c),
  M2, M6.
- **M6 Axis names `x`, `y`, `z`, `w`** (D370 item 205). Exempt them from the single-letter error (generator.spite
  near line 28476) and rename the `x_value`-style attributes in 12 library files (`vector2/3/4`, `quaternion`,
  `matrix3/4`, `plane`, `ray`, `axis_aligned_box`, `cubic_bezier`, `color_text`, `http_client`). **S.** No
  dependencies.
- **M7 Identical-function folding leftovers** (D340; status "Identical functions"). Fold a function differing only
  in which class of another layout it passes by reference, compare a boxed text constant by its text, and stop
  writing a foreign callback's site into the function. Files: `function_folder.spite`. **M.** No dependencies.
- **M8 Benchmark gap analysis** (D398). For each benchmark, compare the generated C with the hand-written C and rank
  every source of extra work by measured cost: allocation, reference counting, checks, the library's algorithm,
  missing aliasing hints. The ranking orders M9 to M11. Files: `benchmarks/`. **M.** No dependencies; first.
- **M9 Escape analysis removes allocations and counting** (D398). An object proven not to escape or not to be
  shared lives in registers or on the stack, with no reference count. Files: `object_escape.spite`,
  `object_frames.spite`, `placement.spite`. **L.** After M8.
- **M10 Inline storage chosen by the compiler** (D398, D331). An object held by one owner is laid inside it; the
  moron never chooses a storage for it; the number classes' `var _memory = Memory.Bytes(n)` stays, library-only
  (D400). **L.** After M9, with M3.
- **M11 Aliasing hints from ownership** (D398, D354, D380). Emit `restrict` where ownership proves two pointers
  never alias. Files: generator.spite parameter emission. **M.** After E3.
- **M12 Docs stop teaching the layout internals** (D399). `TypedMemory<T>`, `InlineMemory`, `Raw` and
  `Memory.Bytes` leave the user pages (memory.md, collections.md, optimizations.md, foreign_libraries.md,
  classes_and_files.md, compiler.md, concurrency.md, packages.md, proofs.md, README.md); exact foreign layouts are
  taught through bindings and binary readers. **M.** After M3, M9 and M10 land.
- **M14 `Vector<T>` removed as a storage the moron picks** (D400). Programs write `List<T>` and the compiler
  stores it inline where it can (D331); migrate `library/`, the docs (collections.md's "`Vector<T>`: items inline"
  and its rules, the "value class" wording) and downstream packages. **M.** After M3.
- **M15 `Items<T>` folds into `List<T>`** (D401). The storage `Items<T>` chooses becomes `List<T>`'s own (M3, M10);
  migrate `library/items.spite`, collections.md's "`Items<T>`: the storage chosen for you" and its rules, and its
  users. **M.** With M14, after M3.
- **M16 A list's layout chosen from how it is iterated** (D402). From every iteration over a `List<T>` and its
  `function.accesses`, choose array of structs, struct of arrays or a hot/cold split per list; report the choice
  (M4) and guard it with benchmarks. This is what lets a large package's component columns become plain lists, so
  it gates M13 (D399's bar). **L.** After M3 and M10.
- **M13 Enforce the ban on layout internals outside `library/`** (D399). A compile error naming the higher-level
  alternative. **S.** Last: only after M3, M9 and M10 make plain classes and lists as fast as hand-chosen layouts
  and downstream packages have migrated with benchmarks showing no slowdown.

### Arithmetic (D359, D360, D357, D369 item 249)

- **N2 The rest of the checks proven away** (D360; proofs.md "Arithmetic that does not fit halts"). A counter
  stepped by one under a `<` or `>` and constants are built; still to drop: indexes already bounded (`index * 4`
  under `index < count`), known ranges (a `bits_and(255)` put into a `Byte`, a `% n` result), attributes whose
  proofs survive calls (`call_effects.spite`), and a sum the C compiler could vectorise; what stays is listed in M4's
  report. Update docs/proofs.md and docs/optimizations.md. **M.** Benchmarks measured on `--optimized` builds only.
- **N3 Floating point speed** (D357; status item 210). Let C fuse multiply-adds and reorder float sums
  (`-ffp-contract=fast`, reassociation, without giving up `nan` and infinities), keep `Float` expressions with
  decimal literals in `float`, and keep exact equality and values written to disk exact. Files: `bootstrap.spite`
  compiler flags, generator.spite literal typing. **S.** No dependencies.

### Memory (D352, D354, D380, D369 item 173, D147, D178)

- **E1 Frame arenas and the escape rule** (D352, D369 item 173; status "Allocators"). `Memory.Frame`, its reset
  chosen by the compiler per use (once a frame, a ring, or none), and storing a frame-arena object where it outlives
  the frame a compile error naming `copy()`, with a generation check in debug builds as backstop. Files:
  `library/memory/arena.spite`, a new `library/memory/frame.spite`, `placement.spite`, `object_escape.spite`.
  **L.** No dependencies.
- **E3 Storage owns its items; `Weak` goes** (D354, D380). List, dictionary and vector slots and non-nullable
  attributes own; a `T?` attribute or local holding an object owned elsewhere is weak, narrowed before use; a
  second owner is a compile error naming `copy()`; trees own children through attributes. Remove
  `library/weak.spite` and the weak table in generator.spite (around line 7230), migrate 7 `Weak<` uses and
  memory.md's "Cycles leak". **L.** No dependencies.
- **E4 Frame objects holding text, lists or objects** (status "Copies that cost nothing", proofs "Objects that
  never leave"). Locals are built; left: such objects as a result into the caller's slot, a temporary and a copy;
  an attribute object laid inline where never shared; an appended item made in place in a `Vector`'s block.
  Files: `object_frames.spite`, `owned_local.spite`, `placement.spite`. **M.** No dependencies.
- **E5 The heap and copies as Spite** (D147, D178, D240; status "The floor"). `Memory.Heap` asks the system for
  pages itself; `copy_to` and `compare_bytes` through `DynamicLibrary`; the remaining backend primitives listed in
  one place a reader finds (D240's table). Files: `library/memory/heap.spite`, `address.spite`, `prelude.spite`.
  **L.** Pairs with C9.

### Language rules (D371 to D373, D369 item 236, D336, D386)

- **L3 Every enum is an instance of `Spite.Enum`** (D371). A new `library/spite/enum.spite`, enums declaring their
  own functions (parser `enum_declaration.spite`, `analysis/enum_info.spite`, generator.spite enum emission), values
  and functions as reflection objects (with R5), and D180's environments enum on top (S6). **L.** Depends on R5
  for the reflection half; F1 needs the functions half.
- **L7 Work that can never finish in a `Parallel` is an error** (D336). A loop with no exit and no wait inside work
  given to a `Parallel`; related to failure.md's "a `while true` that can never leave". Files: generator.spite
  `Parallel` checks, `wait_facts.spite`. **M.** No dependencies.

### Compiler driver and outputs (D327, D348, D349, D356, D390, D361, D366, D369 items 134 and 138, D385)

- **C5 `--optimized` stays `-O3`** (D390, superseding D356). Already `-O3` (`bootstrap.spite` lines 902 to 925);
  nothing to measure. Only check the docs say so. **S.** No dependencies (`wip/fastbuild` has landed).
- **C6 The object cache cleans itself** (D327). LRU eviction of `.spite/objects` past a size cap. Files:
  `bootstrap/source/translation/unit_build.spite`. **S.** No dependencies.
- **C7 `--final-classes` shows the winning source** (D366, D382, open question 10, status "Final classes"). Each final
  class printed as Spite with its generics as written, each declaration marked with the file and load root that
  supplied it, as one comment line linking that file (D382); used library helpers and template instances appear as source, not C names. Files:
  generator.spite final-class printing, `syntax/source_printer.spite`. **M.** No dependencies.
- **C8 The C left in `main` moves into Spite** (D361, D342). `argv` reaches the program only through `Arguments`;
  `_setmode`, singleton teardown and the `--debug-memory` report run through singletons' `drop()`. Files:
  `code_builder.spite` (line 274 on), `native_faults.spite`, `library/program.spite`, `library/environment.spite`.
  **M.** No dependencies (K1's flushing at exit is built: `Program.exit` and the C library's exit flush).
- **C9 No bodiless function; primitives in one place** (D240, D147, D178; status "Pure Spite", "What the compiler
  supplies"). The compiler-supplied bodies (`Console`'s raw writes, `DynamicLibrary` open and lookup, `TypedMemory`,
  number casts, `Concurrent`/`ThreadPool`/`Scheduler` frames, `HotReload` hand-off, `Spite.Attribute` and
  `Spite.Function` dispatch, `String.sum`, `code_at`) become Spite over a short list of named backend primitives.
  Files: `prelude.spite`, generator.spite supplied bodies, the matching `library/` files. **L.** Pairs with E5.

### Concurrency, waiting and output (D346, D369 items 178, 179 and 211, D183, D184, D210, D378)

- **K1 The compiler picks the flushing per destination** (D346; status "System classes"). The guarantees and the
  private `flush` are built; left is the speed: line by line to a terminal, large buffers to files and pipes, with
  a flush at every place the program waits so a server's log still shows each line. Files: `library/console.spite`,
  `prelude.spite` (`_flush`), the waiting library classes, `wait_facts.spite`. **M.** Depends on K2's list of waits.
- **K2 Waits that run the loop in place never hang each other** (D369 item 179; status "Hidden async/await").
  Close every gap (right side of `and`/`or`/`==` on a nullable, through a function value, a union dispatch or a
  constructor, a `Concurrent` dropped inside a `Concurrent`), even at a cost in speed; until then a join that
  closes such a cycle halts (B8, `conformance/stage6/concurrent_wait_cycle`). Files: `state_machine.spite`,
  `wait_facts.spite`. **L.** No dependencies.
- **K3 A wait inside an expression keeps the written order** (D369 item 178). Temporaries hold what came before,
  dropped where nothing can change. Files: `state_machine.spite`. **M.** Do with K2.
- **K4 Singleton safety, the rest of the plan** (D183, D184; status "Thread safety for singletons, the rest").
  The check at `return` that a locked singleton hands out only numbers, text, copies or safe singletons (callbacks
  too); a buffer per thread for append-only state; state split per thread; guarding in `--hot-reload` builds; the
  locked-wait check's blind spots (escaping handles, function values, unions, `parallel_each_`, generic
  singletons). Files: generator.spite singleton forms. **L.** No dependencies.
- **K5 Sockets on the system's readiness** (D210 item 180: once HTTP exists, where measured faster). IOCP, epoll,
  kqueue instead of a helper thread per call; make `UdpSocket.receive()` and `Directory` listing compiler waits.
  Files: `library/socket.spite`, `udp_socket.spite`, `scheduler.spite` and their system folders, `wait_facts.spite`.
  **L.** Lands after cloud/linux so Linux runs it.
- **K6 Cancelling a `Concurrent`, and one made off the scheduler's thread** (status concurrency.md summary).
  Decided in concurrency.md, neither built. **M.** Depends on K2.
- **K7 IO is concurrent by default** (D378). Rebuild the standard library's IO (sockets, files, HTTP, the file
  watcher) on the `Concurrent` library, so an IO call that would wait parks and straight-line code needs no
  annotation; the compiler may keep a call blocking where it measures faster (nothing else to run, a tiny local
  read). Once built, concurrency.md leads with the simple story. Files: `library/socket.spite`, `udp_socket.spite`,
  `http_*.spite`, `file.spite`, the watcher, `scheduler.spite`, `wait_facts.spite`. **L.** With K5; after K2.

### Foreign libraries (D351, D369 items 93, 211 and 233)

- **F1 A C enum result is a Spite enum written in Spite** (D351, replacing D272's generated enum). The binding's
  enum maps each value to its C number (numbered with `=`, D386, built); a number the enum does not list crashes at the boundary; the
  result must be used and switched with every value. Files: generator.spite foreign-call region,
  `library/dynamic_library.spite`, docs/foreign_libraries.md (still describes D272). **M.** Depends on L3.
- **F2 Bindings checked against a header** (D369 items 93 and 233). With a header, argument and result types and
  enum numbers checked, the error naming the C type; how the header is read is the compiler's choice. **M.**
  Depends on F1.
- **F3 A call through a dropped callback context halts** (D369 item 211). The trampoline's context slot is cleared
  on `drop()` and a late call halts naming it; `'no_context'` stays a singleton's function. Files:
  `library/foreign_callback.spite`, generator.spite trampolines. **S.** No dependencies.
- **F4 Remaining foreign gaps** (status "Foreign libraries", "Callbacks"). `missing_function` and
  `missing_attribute` as reopenable Spite and a user naming rule; a header's types through reflection; C variadic
  functions; a `Boolean` as C `bool`, enums and structs by value to callbacks, a function value inside a `type`;
  the offending field named in the foreign-type error. **L** in total, each S or M. Depends on C9 for the first.

### Standard library (D294, D394, D241, D180, D369 items 220 and 250, D370 item 237)

- **S1 WebSocket** (D294). Grows from `Socket` and `HttpServer`/`HttpClient` (upgrade handshake with the existing
  `Sha256` and `Base64`, framing, masking, ping and close). New `library/web_socket.spite`. **M.** `wss` waits on S2.
- **S2 TLS** (D294, D394, D395). One API on every system for HTTPS and `wss`, over the platform's TLS: SChannel on
  Windows, the Security framework on macOS, the system's libssl on Linux (not bundled). Client first, then server,
  each tested against real servers. **L** (the unified API plus three bindings; the largest library item).
- **S10 TLS written in Spite** (D395, later). Replaces S2's platform bindings behind the same API, so no program
  changes; lands only after an outside security review. **L.** Depends on S2.
- **S11 `File.rename` and moving a file or folder.** There is no way to rename or move one without a foreign call.
  One function on `File` and `Directory` (name proposed by Claude, unconfirmed), on every system. Files:
  `library/file.spite`, `directory.spite` and their system folders, docs standard_library.md. **S.** No dependencies.
- **S12 `List.index_of(item): Integer?`.** `find_by_<member>` answers the item, never its place; add the index form
  (and its member template, `find_index_by_<member>`; names proposed by Claude, unconfirmed). Files:
  `library/list.spite`, `vector.spite`, `items.spite`, docs collections.md. **S.** No dependencies.
- **S3 HTTP leftovers** (status "HTTP"). Request bodies sent chunked to the server; the server reading one request
  at a time so a slow client holds the others; the client's resend of a `POST` over a new connection. Files:
  `library/http_server.spite`, `http_client.spite`. **M.** Benefits from K5.
- **S5 Helpers stop looking public** (D241; status "Socket"). `Socket`'s address helpers (`any_address`,
  `resolved_addresses`, `address_text`, `first_readable`), `BinaryInput`/`BinaryOutput`, `NumberText`,
  `ColorText`, `JsonCursor`, `ZoneRules` and the other helpers are folded into the class they serve or made
  private. **M.** Do after J1 (the JSON and binary helpers change there).
- **S6 Environments as a reopenable, walkable enum** (D180; status "Symbol codegen and enums"). With D373 a
  reopening restates the whole list. Files: `library/environment.spite`, `library/build.spite`. **S.** Depends on
  L3, L4.
- **S9 Collection leftovers** (status "Standard library metaprogramming", "Deep copy"). `sort_by_`, `find_by_` and
  a program's own templates on `Vector` and `Items`; the `while`-does-a-template rule over `Vector` loops;
  `deep_copy()` of unions, shapes and self-referring structures; a `String`'s or number's function held as a value.
  **M.** No dependencies.

### REPL and reload (D280, D301, D305)

- **P1 Prompt gaps** (status "Command language", "One instance per argument values"). A singleton with arguments
  (`Channel(1)`), `Dictionary`'s `keys()` and `has(key)`, assigning an object made at the prompt, escapes in text
  literals, and general map expressions (so J2 replaces D333's text parsing). Files:
  `library/read_evaluate_print_loop.spite`. **M.** No dependencies.
- **P2 Reloading a class that starts or stops fitting an `Items`' own memory** (D280; status "What a reload can
  change"). Files: `hot_reload_library.spite`, generator.spite object moves. **M.** No dependencies.
- **P3 The reload race on a loaded Linux machine** (from `cloud/linux`, a D244 bug). The watcher's reload and the
  prompt's `reload` race over `.reload_baseline`/`.reload_files`; also the move of objects while the program's own
  threads run (failure.md). **M.** Depends on landing `cloud/linux`.

### Failure reports (D25, D33, D20, D379)

- **X1 The rest of the crash report** (status "Three outcomes"). The call chain, each failed `assert`'s default, an
  assert's values in the ring, the column in `.crashes` (written `0`). Files: generator.spite crash and assert
  emission, `crash_part.spite`, `native_faults.spite`. **M.** No dependencies.
- **X2 Spite's own allocator detects a corrupted heap** (D379's direction, per D361); the report through `SIGABRT`
  is built. Files: the allocator. **L**.

### Bugs under D244 (failure.md's open list; each small and independent unless noted)

- **B3** A lent-element function is not checked by the guard lint (an empty collection made on the spot is now a
  default, `diagnostics/empty_collection_guard`). **S.**
- **B4** A frame buffer's uses are matched by name, not by `TypedMemory` receiver (`placement.spite`). **S.**
- **B6** A `while true` that can never leave is not reported (with L7). **M.**
- **B7** Two threads writing one number attribute of a shared instance is refused only for a handed-over local
  (D35, D179); the rest needs the handle's lifetime. **M.**
- **B11** Reading `.functions` anywhere turns on a whole-program flag; set it only from kept code. **S.**
- **B12** A write to a copy that dies unread (proposed, unconfirmed rule: a compile error). **M.**
- **B14** A Windows `__fastfail` ends the program without Spite's report (the Linux and macOS heap abort is D379,
  built). **M.**
- **B15** A `Concurrent` polled for `finished` under `resume_only_when_asked()` without `run_ready()` hangs. **S.**
- **B17** Reference cycles leak silently without `--debug-memory` (largely answered by E3). Depends on E3.
- **B18** A compile can write back old text over an edit made while it runs: every compile formats the program's
  files (D385), and one that read a file before someone else's edit wrote its formatted copy over that edit. Write a
  file only when it is unchanged since it was read, and otherwise report the file as changed (a D244 lost write).
  Files: the formatter's write in `bootstrap.spite`. **S.**
- **B19** Whether a walked row's ignored `_x` parameter counts as read depends on the shapes of the other
  parameters. It must not. Files: the walked-row reads in generator.spite. **S.**
- **B20** The generated C contains a bare `spite_temp_N;` statement with no effect, which clang reports under
  `-Wunused-value`. Stop emitting it. Files: generator.spite temporaries. **S.**

### Skills and docs

- **D1 `skills/spite/` kept current.** `reference.md` still teaches `Weak<T>`,
  `Symbol<...>` walks, `$T.has_function(...)`, `function_waits(...)`, `names.map(measure)`, enum reopening that
  appends, and `get_at` beside `[]`. Each item above updates it in the same commit; a first pass now fixes what is
  already decided and built (D315 once landed, `map_members`, `filter_files`, `to_<type>()`). **S** now, then part
  of every item.

### Deferred (decided, far off, not ordered here)

The targets page (D20, `--target=web`, WebAssembly, isomorphic classes, the wire format and its handshake, `Html`
and the markup builder, which needs a name); bundle splitting and lazy `load` (packages.md); the language server
(open question 4); defining members from data and hooks on reopening (open question 2); calendars, leap seconds
and formatting patterns (time.md); running and testing the macOS folders; S10, TLS written in Spite.

## Dependency order

Each line can start once everything before it that it names is done; lines with no dependency can start at once.

1. Land the four branches (operators, nomap, fastbuild, linux), renumbering three rows.
2. No dependencies: R8, M1, M2, M4, M6, M7, N1, N3, E1, E4, L7, C5, C6, C7, K2, F3, S9, P1, P2, X1, B1
   to B16, E3, D1's first pass.
3. After step 2: R5, K3 (with K2), K1 (K2), C8, N2 (N1), K6 (K2).
4. After R5: R1, J1, L3, R4, R8, then R9.
5. After J1: J2, S5. After L3: F1, S6 (with L4). After F1: F2.
6. After M1, M2 and M6: M5. After M1 and M4: M3.
7. After R1, R5 and J1: R7, the end of the reflection migration.
8. S2 and then S1's `wss`; K4, K5, S3, C9, E5, F4 at any point, best after the
   items sharing their files.

The longest chain is landing nomap, R5, J1, R7: the reflection migration is the critical path.

## Parallel streams

`generator.spite` is one 43 800-line file every compiler item touches, so "disjoint" means disjoint regions of it
plus disjoint other files; streams that both change generator.spite merge often and keep their edits to their
own functions. Splitting the regions below into their own files first (as `call_effects.spite` and
`state_machine.spite` already are) would make the streams truly disjoint and is worth a day.

| Stream | Items, in order | Files it owns |
|---|---|---|
| 1 Reflection and serialization | R5, R1, R4, J1, J2, R8, R7 | generator.spite reflection, specialisation and template regions; `specialisation.spite`, `reflected*.spite`, `template_walk.spite`, `namespace_walk.spite`, `old_spellings.spite`; `library/spite/*`, `json_*`, `binary_*`, `dictionary.spite`; stage6 walk programs; docs reflection, metaprogramming, json |
| 2 Types and storage | M2, M1, M7, M4, M3, then M6 and M5 | `dispatch_classes.spite`, `type_shape.spite`, `tree_shaker.spite`, `function_folder.spite`, generator.spite copy and dispatch regions; `library/list.spite`, `items.spite`, `vector.spite`, the maths classes |
| 3 Arithmetic | N3, N1, N2 | generator.spite operator and overflow regions, `maths_primitives.spite`, the number classes, the hash and codec files |
| 4 Memory | E1, E4, E3, E5 | `placement.spite`, `object_escape.spite`, `object_frames.spite`, `owned_local.spite`, `library/memory/*`, `typed_memory.spite`, `weak.spite` |
| 5 Driver and toolchain | C6, C5, C7, C8, C9 | `bootstrap.spite`, `bin/spite`, `check.sh`, `bootstrap/source/translation/*`, `code_builder.spite`, `native_faults.spite`, `prelude.spite`, `library/build.spite`, `program.spite` |
| 6 Waiting, IO and library | F3, K2, K1, K3, K6, S3, K5, S1, S9, S5, then S2 | `state_machine.spite`, `wait_facts.spite`, `library/console.spite`, `socket.spite`, `udp_socket.spite`, `http_*`, `scheduler.spite`, `foreign_callback.spite`, the system folders |
| 7 Language rules | L7, L3, S6, F1, F2, F4 | `bootstrap/source/discovery/*`, `syntax/*` (parser, enum declaration), `analysis/enum_info.spite`, generator.spite enum and foreign-call regions, `dynamic_library.spite`, `environment.spite` |
| 8 REPL and reports | P1, P2, P3, X1, K4 | `library/read_evaluate_print_loop.spite`, `hot_reload_library.spite`, `crash_part.spite`, generator.spite crash and singleton-form regions |
| 9 Bug sweep | B1 to B16 | small fixes, each in the file of the proof it fixes; rebase often |
| 10 Docs and skill | D1, then the docs and status lines of every landing | `skills/spite/`, `design/status.md`, `docs/` pages as items land |

Stream 1 is the longest (several weeks), then streams 2 and 3 (N1 and N2 are each a week or more), then the
memory stream. Streams 5 and 9 are many small items and the right place for a session with little context.

## Items that need an owner decision

None open: every owner question is answered (D378 to D394).


## Later, in order (D397, [proposals/own_backend.md](proposals/own_backend.md))

Each stage starts after the one before it lands.

1. **No C runtime.** The library calls the system directly (Windows `kernel32`/`ntdll`, Linux raw system calls, macOS
   `libSystem` as the platform); number formatting, memory, text and maths written in Spite. Builds on C9 and C8.
   **L.**
2. **Released builds.** One download per system, the compiler and its library, usable at once; the C seed only for
   building from source. **M.**
3. **Spite's development backend.** A shared array-based IR, instruction selection, a simple register allocator,
   PE/ELF/Mach-O written by Spite; x86-64 then ARM64; hot reload as code generated into the running program through
   the function slots. Default builds, hot reload and the REPL move to it. **L.**
4. **Spite's optimising backends.** Optimisations on the shared IR using ownership, proven-safe checks,
   whole-program specialisation and list storage; `--optimized` leaves C only once these win on the benchmarks.
   **L**, open-ended.
