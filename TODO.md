# To do

What is decided, or wished for, and not built yet: one line per missing thing, with the decisions behind it in
parentheses. Build it, then delete its line in the same commit. The rule is on its page in `specs/`, the reason in
[DECISIONS.md](DECISIONS.md), and the work's size and order in [design/backlog.md](design/backlog.md).

## Language rules

- A `switch` case of two statements or more is a compile error; about 530 cases in the compiler and `library/` are rewritten first (D576)
- Three or more classes in a folder sharing a name prefix are an error; the compiler's own `backend/high_*.spite` break it (D588)
- Environments are a reopenable, walkable enum (D180, D373)
- Every enum is an instance of `Spite.Enum` and declares its own functions (D371)
- A template called with a symbol written out: `person.set_attribute('age', 2)` (D10, D11)
- A numbered enum's number is its identity in binary files and on the network (D386)
- Storage owns its items, a `T?` holding an object owned elsewhere is weak, and `Weak<T>` goes (D354, D380)
- A generic constrained by a union, "one of these classes" (D175)
- `Fraction<$number_type>` in signatures, so a whole-number vector's `normalized()` answers the fractional class (D486, D510)
- A class that declares `equals` keyed by its `equals` (D532)
- An object key or a fraction key written as JSON (D532)
- `fits_vector()` removed in favour of `is_fixed_size`, and a walk over `schema()` counted as a read (D537)
- `Vector<T>` stops being a storage a program picks, and `Items<T>` folds into `List<T>` (D400, D401)
- The layout internals (`TypedMemory`, `InlineMemory`, `Raw`, `Memory.Bytes`) are an error outside `library/` and leave the docs (D399)

## Standard library

- TLS, so HTTPS and `wss` (D294, D394, D395)
- TLS written in Spite, after an outside security review (D395)
- HTTP: request bodies sent chunked, and a server that reads several clients' requests at once (D294)
- WebSocket: a server choosing a subprotocol, `permessage-deflate`, closing with another code, a message sent in pieces, a ping that waits for its pong (D294)
- Library helpers stop looking public: `Socket`'s address helpers, `BinaryInput`/`BinaryOutput`, `NumberText`, `JsonCursor`, `ZoneRules` (D241)
- Console flushing chosen per destination: a line at a time to a terminal, large buffers to files and pipes (D346)
- `JsonValue`, for reading JSON whose shape is not known (D476)
- The rename map keyed by attribute objects, and the `keys` attribute of `JsonWriter`/`JsonReader` (D320, D329, D478)
- `BinaryFormat` walks with `each` instead of `Symbol<$value_type>` plurals (D319, D384)
- A `Vector<T>` attribute in what the JSON and binary writers and readers see; they test only for `List` and `Dictionary` (D208)
- `fused_multiply_add`, and waits for targets that name their processor
- Time: calendars other than ISO 8601, leap seconds, formatting patterns, localized names, zone abbreviations (D127)
- The test runner calls each test through `Spite.Call` instead of `call_function()` (D496)

## Memory and the internal namespace

- The memory floor, `Concurrent`, `Parallel`, `ThreadPool`, `Lock`, `Atomic` and `ThreadLocal` move into `Spite.Internal` (D566)
- The library above the floor (`BinaryReader`, JSON, codecs, `Benchmark`) moves off `Memory` onto `Buffer` and plain values (D566, D590)
- Using `Memory` directly outside the library is a compile error, once plain foreign structs and a lock between processes exist (D565)
- `Memory.Heap` asks the operating system for pages itself (D147, D178)
- `copy_to` and `compare_bytes` written as Spite through `DynamicLibrary` (D178)
- `Memory.Frame`, its reset chosen per use, and the escape rule (D352)
- `reset()` on an arena (D152)
- The compiler chooses frame arenas, rings, structure of arrays and deferred freeing (D485, D501)

## Concurrency

- Cancelling a `Concurrent` (D480)
- Every wait in a `Concurrent` is a point to return from, also in a function that cannot be a state machine and in a `--hot-reload` build (D369)
- IO is concurrent by default: sockets, files, HTTP and the watcher rebuilt on `Concurrent` (D378)
- Sockets on the system's readiness (IOCP, epoll, kqueue) where it measures faster; `UdpSocket.receive()` and `Directory` listing become waits (D210)
- A buffer per thread for append-only state, and state split per thread (D184)
- The check at `return` that a locked singleton, or a callback, hands out only numbers, text, copies or safe singletons (D183)
- Singleton guarding in `--hot-reload` builds (D183)
- The locked-wait check sees escaping handles, function values, unions, `parallel_each_` passes and generic singletons (D264)
- Calls in a row run at once with arguments too, and tell apart objects not held by attributes made for their owner (D505, D556)
- A loop over a list of different classes run at once when written as a `while`, with counts per creation site (D539, D555)
- Bands: reductions, a scratch per band, `while` loops, passes that count references, an `Items` of a class, a threshold measured per machine (D542, D557)
- A counted `while` run in chunks after one check per loop (D559)
- A wait in a frame: within one pass, several waiting calls in flight, calls with arguments, frame loops paced by anything but `Program.sleep` (D554)

## Reflection and metaprogramming

- `Spite.FunctionDeclaration`: a class's `.functions` are declarations, and `.owner` is the instance on a bound member (D391)
- `Spite.Call(declaration, instance)` replaces `call_with`, so a reflected function is called with arguments (D391, D393)
- `.owner` readable on a function value (D40)
- `.owner`, `.element_type` and `.value_type` on run-time objects, not only constants (D316, D317)
- An enum parameter specialised for values from reflection (D497)
- The old `Symbol` machinery removed: `Symbol<...>` templates, plural walks, name patterns, folder ranges, name-keyed questions (D316, D317)
- `function.accesses` for standard library functions, function values through a `type`, and `--hot-reload` builds (D335, D362)
- Every function of a class overridable, `call_function` included, and the compiler-added ones (`get_<attribute>`, `copy`, `deep_copy`) printed as Spite (D483)
- Reopening `Spite.Class` changes what it decides (`is_singleton()`), and `--final-classes` names the root of a changed default (D7, D504)
- `$system_type.function_runs_in_pieces("<phase>_each")` (D229)
- `each_<member>(...)` passing extra arguments to the member (D510)
- Defining members from data, and hooks that run when a class is reopened (open question 2)
- Generating class files at compile time, from pure cached generators, printed by `--final-classes` (Mortaro's wish, 2026-09-19)

## Foreign libraries and targets

- `DynamicLibrary` as Spite: `missing_function`, `missing_attribute`, opening and lookup written in Spite (D4, D147)
- A naming rule of your own: a user-written `symbol_name(symbol: Symbol): String` (D4)
- A header's types read through reflection, and C's variadic functions (D4)
- A C enum result answered as a Spite enum the binding writes, used and switched, an unlisted value crashing (D351)
- Bindings checked against a header, the error naming the C type (D369)
- Plain foreign structs: a `type` holding text, a nested `type`, a `type?`, a `List`, an enum or a fixed array across a foreign call (D536)
- The foreign-type error names the offending field
- Callbacks: a `Boolean` as C's `bool`, enums and structs by value, a function value inside a `type`, widths checked against the header
- The web target: `--target=web`, wasm, the JavaScript glue module, `platform_web` reopenings, `DynamicLibrary` over a JS module's exports (D19, D20)
- The web shim generated from `external js` declarations and an opcode table
- Isomorphic classes: the `ServerContext`/`ClientContext` bundle split, routes, generated stubs, the wire format and its handshake (D13)
- `Html`, `Html.Node` and the markup builder (D19)
- `Spite.Class.targets()` and the wrong-target compile error (D20)
- Bundles split into dynamic libraries, tree-shaken per bundle, and a `load` inside an `if` loaded lazily

## Live reload and the REPL

- A reload library for one class writes the class's state machine entries, instead of compiling the whole program

## Compiler and backend

- Every line of C written from the high-level IR: arithmetic, calls, references, `assert`/`crash`, texts, lists, dictionaries, layouts, dispatch and `main` (D560, D586, D587)
- The high IR desugared and monomorphised, every decision of the generator a pass over it, and the switch-over made whole (D587)
- The passes that read or rewrite the C text moved onto the IR (D586, D587)
- The low level of the IR (D560)
- The whole program's IR kept in memory and cached on disk, rebuilding only what changed (D569)
- The fast check built on that cache: a quick level while working, the full level before `master` (D569)
- `spite compiler --verify` step 2, so `check.sh` goes: `--release`, benchmarks, the other systems' seeds, hot reload, REPL, sockets, final classes, the launcher (D580, D581, D582)
- A program's tests run while compiling it, and a release build cannot skip them (D577)
- The repository's scripts become Spite, a non-Spite script fails the compile, and the seeds go once Linux and macOS have releases (D578)
- No bodiless function: every compiler-supplied body (`Console`'s raw writes, `Memory.Heap`, `Memory.Address`'s copies, `String.sum`, `String.code_at`, `TypedMemory`, number bit operations, `Concurrent`/`ThreadPool`/`Scheduler` frames, `HotReload`'s hand-off, reflection dispatch) becomes Spite over named primitives (D147, D240)
- The C left in `main` moves into Spite: the arguments, `_setmode`, releasing singletons and class objects, the memory report (D342, D361)
- `Arguments` written in Spite instead of C in the prelude (D147)
- The per-element-type functions and `deep_copy` the compiler still writes, written once by the generic class (D147)
- Spite's own backend: no C runtime, then released builds, then a development backend, then optimising backends (D397, D558)
- Learning from a test run: a profiling build, a profile file, and choices that read it (D534)
- Collection types as intention: each `Dictionary`'s representation chosen per use (D533)

## Optimisations and proofs

- Naive programs reach hand-written speed (D506, D507, D508, D550)
- Independent loops written as a `while`, and reductions, run at once (D506)
- Layout chosen by the compiler: structure of arrays, hot and cold splitting, field order, cache-line alignment (D402, D485)
- List storage by concrete class for a list of a `type` or union (D331)
- Escape analysis: objects that never escape in registers or the frame, no count where ownership is provable, objects laid inside their only owner (D398)
- `restrict` emitted where ownership proves two pointers never alias (D398)
- A function taking a `type`: text and `Symbol` tagged not boxed, the closed set per spot, copies for nullable, assigned and waiting parameters and library containers, a never-made class not emitted (D321)
- Overflow checks dropped by ranges of attributes, list items, call answers, `bits_and` masks and narrowing (D360, D543)
- Frame objects holding text, lists or objects as a result, a temporary or a copy, and attribute objects laid inline
- An item written back to its own slot: across blocks, a copy written back, a `Dictionary` slot, per instance of a generic class (D515, D519)
- A write-back of what the slot holds, across calls: store statements, loops, objects made, unknown classes (D562)
- An attribute a call cannot assign: through templates' items, singletons, getters, and per object (D531)
- A store counts only when it changes the slot: values made in the store, a `Dictionary`, an attribute given the object it holds (D544)
- An item passed to a call uncounted: a `Dictionary` entry, a computed index, a receiver of a program function (D538)
- An item read only to test it: a `Dictionary` entry, an attribute through an item, an index that calls a function (D518)
- A test against a value a list never holds: a `switch`, `<` and friends, `Boolean`/`Float`/text lists, literal and library lists, folded branches removed before tree shaking (D523)
- A table filled once: setups in loops or branches, per-parameter copies, tables of objects, non-singletons in threaded programs, the runner made direct (D551)
- Pools for classes no list holds, a pool per thread, and runs of blocks given back (D517, D570)
- A list held only by another list in its slot: per list rather than per element type, `list[index] = fresh`, lists from calls or literals, through a `Dictionary` or `Items` (D552)
- A list of lists filled again: lists that keep references, the inner items' own room, a bound after shrinking (D553)
- Plain counts: objects of one class told apart per thread, text and boxes, function values matched by type, inspectable builds (D513)
- Inlined releases: list accessors folded across units, a shape's or union's retain and release (D514)
- Identical-function folding: classes passed by reference, boxed text constants, foreign callback sites (D340)
- The optimisation report: temporaries and arguments on the heap, call results not fresh, `Items` choosing references, narrowing checks (D36, D332, D389)
- A byte read reuses the block let go before it, and a proven read outside a counted loop is one load (D536)
- A function value a library constructor only calls is framed in the caller, and a value no thread can reach makes nothing atomic (D528, D545)
- Several singletons' locks held once for a counted loop (D485)
- A read overlaps the statements after it until its name is used (D211)
- The call-effects study types a local made by an operator, `var start = address + offset`
- A text literal containing `instances` no longer makes every class read with `.instances`
- Every section of optimizations.md names its proof in proofs.md
- The smaller gaps each benchmark case's README records

## Tooling

- `--final-classes` prints each class with its generics as written, and the generated functions of a generic instance and a list's member templates as source (D61, D366)
- `--final-classes` prints a plural walk over another class once, not twice
- A user generic instantiated with `Integer` printed into `instantiated/` and read back past the name lint (not checked by any corpus program)
- The compiler as the language server (open question 4)

## Platforms

- Run, not only compile, the Linux folder's live reload, file watcher, threads, sockets over IPv6, file mapping and time zones (D80, D376)
- Run anything of the macOS folder; nothing there has run (D80)
- WebSocket run on Windows and macOS, not only on Linux
- The call chain of an `--optimized` crash on Linux and macOS (D25, D479)
- Releases for Linux and macOS (D574, D578)

## Bugs that fail silently (D244)

- `Integer.smallest.absolute()` may answer the negative value (suspected, not reproduced)
- `parallel_each_` over a list holding one object twice runs it on two threads at once
- A local read from a list and assigned a value nothing reads still compiles inside a `while` with the local outside, or through a function the rule cannot follow (D515, D516)
- Reference cycles leak without a word unless built with `--debug-memory`
- Some waits in a `Concurrent` still run the event loop in place: one dropped by `remove_at` or `remove_where`, a wait through a text or number function held as a value, a join in a locked singleton function
- A Windows `__fastfail` ends the program without Spite's report
- A `while true` that can never leave but calls something is refused only in `Parallel` work and locked singleton functions (D510)
- `Process.run` reads pipes in text mode on Windows, so a byte 0x1A ends the output
- `HttpClient.send` writes `host` without the port when the port is not 80
- A list's templates read elements uncounted without checking the `drop()` functions that may free one
- `read_value`, `write_value`, `release_value` and `swap_values` are accepted by name on any receiver
- A copy made to narrow is caught only when the condition is the bare name
- Statements after a `return` in the same block compile without an error
- A `crash` on a `Build` field is tested at run time instead of failing the compile (D250)
- A supplied function outside a fixed list answers "does not write a parameter"
- Reading `Spite.Class.functions` anywhere in the library changes how every program is compiled
- Two threads writing one number attribute of a shared instance is refused only for a handed-over local (D35)
- A lent-element function is not checked by the guard lint
- A write to a copy that dies unread compiles
- The union `switch` message "every member is covered, so a member added later cannot be forgotten" says the opposite of what it means
