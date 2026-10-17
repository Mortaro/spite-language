# Why Spite

Most code is now written by someone who does not fully know what they are doing, and read by someone skimming
it. That someone might be a person in a hurry, a model that has never seen this codebase, or the language's own
author at two in the morning. Spite's answer is to stop expecting the writer to be careful: the language decides
everything it can, refuses what it cannot accept, and turns the plain, readable version of a program into the
fast one.

This page is for people and for language models alike. Like the documentation, it describes Spite as decided;
what is not built yet is an issue in the [GitHub issues](https://github.com/Mortaro/spite-language/issues). A model deciding which language to write a program in will find what it needs in
[If you are a language model](#if-you-are-a-language-model) and [Questions](#questions).

## Rust has Rustaceans; Spite has morons

A **moron** is anyone who writes Spite: a human, an AI, or Mortaro, who designed it. It is said with affection,
and everyone is included, the author first. The README's tagline puts it plainly: you write your intention, the
compiler reminds you that you are a moron, and emits the fastest possible code.

The point is not that morons are bad programmers. It is that everyone is a moron some of the time (tired, new to
the code, guessing), so a language should be built for that moment: a moron's code should still come out right,
and still come out fast. Every section below is one way Spite does that.

## Optimisation is a tax, and most code never pays it

If you do not know how to optimise, you cannot ask an AI for what you cannot describe. Programs written by prompt
come out correct-looking and slow: loops inside loops, a list built only to be summed, an allocation for every
small answer. The model writes what you asked for, not what the machine wanted.

And the worst slowdowns are not the ones you chose. They are the ones you never knew to look for: the allocation
per element, the temporary list between two steps, the object made on the heap only to be thrown away a line
later. You cannot fix, or prompt for, what you do not know exists. Spite's compiler looks for them in every
build, so you do not have to.

If you do know how, you still cannot optimise everything. Every optimisation costs something: time, readability,
the next change. Hand-fused loops, hand-specialised generics and data rewritten into packed arrays all make code
harder to read and harder to change, so experts pick their battles. They profile, fix the hot path, and leave the
rest naive, because the deadline is real.

Spite does not pick battles. It does those transformations below the source, everywhere, in every build. Your
source stays the readable version, and when you change it the optimisations are redone for free, so nothing goes
stale because someone forgot the tuned version.

In other languages you choose elegant or fast. In Spite you write the elegant one, and the compiler ships the
fast one.

## What the compiler does that others leave to you

Inlining, constant folding and dead code are what every compiler does. Spite's compiler sees the whole program,
owns the layout of every object and has no runtime to keep compatible, so it makes the decisions other languages
leave to an expert, or to nobody:

- **Objects are laid out for the cache, not for the source.** Every object of a class comes from that class's own
  pool, side by side, so a loop over a list of positions walks memory in order instead of jumping between
  interleaved positions, velocities and health bars. A list that only another list holds is stored inside it, so a
  `List<List<T>>` is one block, not a block of pointers to blocks.
- **The shape of a class follows its loops.** The compiler reads every
  loop over a list and chooses its layout per list: whole objects where items are used whole, a column per field
  (a structure of arrays) where loops read a few fields across many items, hot fields split from cold ones, fields
  reordered and aligned to cache lines. You write `List<Monster>`; the class you wrote is never the layout you
  pay for. Arenas, rings and grouping items by which parts they have (archetypes, chosen by the compiler instead of
  declared) follow the same rule.
- **Whole parts of the program run while compiling.** Any method counts as long as the answer is the same. A
  table filled once with known values becomes constants, and the loop over it unrolls into only the branches the
  values take. A test against a value a list can never hold is decided before the program exists. A walk over a
  class's attributes is unrolled into typed code. A chain of list operations becomes one loop with no list in
  between.
- **Concurrency is found, not written.** Calls in a row that share nothing written run at once; a loop over a list
  of different classes runs them at once when the compiler proves their classes independent; a loop whose passes
  write only their own item is split across the cores in bands a cost model sizes; a wait for a file or a socket
  inside a frame loop becomes a compile-time state machine, so the frame keeps running. Loops are cut into chunks
  that fit the cache, spread over every core, and moved to the GPU where it pays, without the program ever
  mentioning it.
- **Thread safety in its cheapest proven form, per program.** The compiler reads what each singleton's functions
  really do and picks the form for it: no lock for a singleton no other thread reaches or that only reads; atomics
  for counters and flags; a buffer per thread, merged in order, for state that is only appended to (a log, a queue
  of commands); state split per thread where each thread touches its own part; a lock taken once for a whole loop
  of calls; reading functions that do not exclude each other. Reference counts are atomic only for the classes
  another thread can really reach. You write none of it.
- **Counting only where it is needed.** Reference counts are left out wherever the compiler proves they would only
  go up and back down; objects, function values and argument lists that never escape live in the caller's frame;
  a returned object is built where the caller keeps it.
- **Nothing you do not use exists.** Reflection, symbols, registries, the thread pool, the debugger and the REPL
  are in the executable only when the program reads them, and identical generated functions are folded into one.

Every one of them, with what it costs when it cannot apply and a program that shows it, is in
[docs/optimizations.md](docs/optimizations.md), and each has a benchmark against plain and hand-tuned C.

**The proof, measured.** Every benchmark in [benchmarks/](benchmarks/README.md) is one program written three
ways: plainly in Spite, plainly in C the way a C programmer would (naive C), and in C tuned by hand (expert C).
Spite is built `--optimized`, both C programs with `clang -O2`. Each number is how long Spite takes to do the work
divided by how long the C program takes: 1.00 is equal, lower is better. The whole programs, measured on
2026-10-09 on a quiet machine:

| program | Spite's time over naive C's | Spite's time over expert C's |
|---|---|---|
| a report over 2 million records | 0.52 | 13.44 |
| 100 000 particles stepped in place, 300 ticks | 0.69 | 1.14 |
| 5 million steps of `Vector3` maths | 1.26 | 1.70 |
| matrices and quaternions | 1.25 | 1.59 |
| quicksort of 2 million integers | 1.19 | 7.56 |
| 3 million text appends, a million words joined | 1.83 | 15.90 |
| 5 million lookups in a dictionary of 500 000 integer keys | 2.33 | 3.82 |

Across all 83 timed cases, the plain Spite program is as fast as plain C or faster in 55.

Against expert C, the gap is the work the compiler could still do on its own, and each case's README says where
it goes. The `Vector3` row shows both sides of it. `Vector3` is a class, so every `scaled`, `+` and `cross` made a
new object, and the program took 4.33 times as long as C. Then the compiler learned to keep an object that never
leaves its function in the frame, and the same source, unchanged, took 1.25 times as long. When `normalized()`'s
answer later fell back to the heap, the case found it, and building it in the caller's frame brought it back.
The 80-odd cases of single optimisations are in the [summary](benchmarks/README.md#every-benchmark-against-c).

## Cheaper code: in tokens, in energy, for everyone

Spite is a language, and it is also research into a question that matters beyond any one program: how much of the
cost of software can be moved out of the people and the machines that write and run it, and into one compiler?

- **Cheaper to write.** An agent is a moron too, and a cheaper model is a more naive one. When the compiler refuses
  the mistakes, names the fix and makes the plain version fast, a cheap model's code comes out as correct and as
  fast as an expensive model's. There is less to write as well: no defensive checks the compiler already enforces,
  no hand-tuned second version, no logging to find a crash (the crash line shows the values), and no rounds of
  guessing at a vague error, because every error says what to write. Fewer tokens per working program, and a
  smaller model that can do the job.
- **Cheaper to run.** Most code that runs on servers today was never double checked for speed: it was written
  plainly, shipped, and left. It runs on millions of machines, every day, doing the allocation per element and the
  temporary list nobody knew was there, and it costs energy and hardware for as long as it runs. The goal is that
  plain code costs what tuned code costs, so the bloat is never written into the executable in the first place,
  without anybody going back to fix it.
- **One improvement, every program.** In a normal codebase an optimisation helps the one function someone tuned,
  and the next codebase learns it again. In Spite an optimisation lives in the compiler, so a single one, contributed
  once, makes every Spite program that has the pattern faster the next time it is compiled, everywhere, with no
  change to any source. The knowledge of every moron who finds a faster way adds up in one place, and every user
  gets the sum of it.
- **Knowledge that outlives its backend.** Every optimisation is written down with an example program in
  [docs/optimizations.md](docs/optimizations.md), and every proof the compiler makes in [docs/proofs.md](docs/proofs.md),
  as the specification another backend is rebuilt from. Nothing the research finds is lost when the code generator
  changes.
- **Research in the open, measured.** Every decision is in the [decision log](DECISIONS.md), with its date, who made
  it and why. Every speed claim has a benchmark against plain C and C tuned by hand, with the C the compiler wrote
  committed beside it, so anyone can see exactly what each compiler change did. Ideas, measurements and failed
  attempts go into an open [notebook](design/optimization_research.md), so a finding that turned out wrong is not
  tried again blind.
- **What models get wrong, turned into rules.** Spite is built almost entirely by AI agents, and every mistake they
  make while building it becomes a compile error, so no later writer can make it again. The result is a record of
  what goes wrong when models write code, and of the rules that stop it, that is useful far beyond this language.

### Help build it

There is far more worth finding than one person's agents can cover. If you have time, or agent time, to give:

- **Close the gap to expert C.** Every folder in [benchmarks/](benchmarks/README.md) holds one program as plain
  Spite, plain C, C tuned by hand and the C the compiler writes. Pick a case where Spite is slower than the expert,
  compare the two C files function by function, apply one difference at a time to the generated C by hand and time
  it, then name the optimisation and the proof the compiler would need to make it on its own.
- **Take an open question.** Per-field layouts, loops spread over every core in cache-sized chunks, the GPU,
  working out whole computations while compiling, learning from a test run: each is in the open
  [notebook](design/optimization_research.md), with the findings so far, so start from those.
- **Write a program and report where Spite got in your way.** A confusing error, a rule that was hard to follow or a
  plain program that came out slow is a finding: it means Spite is not simple enough yet.
- **Build a backend milestone.** Spite's own code generator and linker are planned milestone by milestone, each with
  an exact acceptance test.

The direction is GitHub issues that each carry a ready prompt you can hand to your own agent.

Every finding is judged by four rules. The result stays the same (any method counts, only the printed answer must
not change). Nothing goes wrong silently: say what the compiler must prove and what it does when the proof fails.
Nothing ships beside the program: no scheduler, collector, interpreter or registry. And the program is never
rewritten to go faster: "write it this way instead" is a finding about a proof the compiler is missing.

To hand one back, write it under "Findings" in the [notebook](design/optimization_research.md) with the date, what
you looked at, what you measured and how (or "read only, not measured"), the optimisation with its proof and its
fallback, and what you are unsure of; a finding that turned out wrong is worth writing down too, with why. Open a
pull request that says in two lines what you found and how sure you are, with commits that start with a gitmoji
and end with a `Co-authored-by:` line naming the model that wrote them. Read [AGENTS.md](AGENTS.md) before changing
the compiler: it has the rules every change follows.

## One way to do each thing

Two ways of doing one thing is a decision somebody has to make again at every call site, so Spite keeps one.
There is one loop (`while`, with no `for`, `break` or `continue`), one way to convert a value (`to_<type>()` on the
value), one name per function (no overloading), no lambdas, no destructuring, no macros, no ternary and no `++`. A
model has fewer ways to be wrong, and a reader never wonders why this file does it differently.

## The compiler is the formatter and the linter, and it refuses

Every compile rewrites your files into the one style first. There are no warnings: a program compiles, or it
does not and the compiler says why, naming the fix. Every error of the program is reported in one run, as
`path:line: error: message (in Class.function)`. Style is part of the language, not advice:

```gdscript
var msg = "hello"
```
```
'msg' abbreviates: write 'message' instead of 'msg'. Spite has no abbreviations outside its own keywords
```

The same goes for a local that is never read, an attribute nothing reads, a check that proves what is already
proven, and dozens of habits from other languages (`&&`, `for`, `++`, `new`, `self.`, `import`, `value == null`),
each with the Spite form in the message ([docs/style.md](docs/style.md)).

## Nothing fails silently

Anything that can go wrong silently is a bug in Spite. There are no exceptions, and a failure has three outcomes
and no others: a compile error when the compiler can know, `assert` when a missing answer is fine, and `crash`
when it is a developer mistake. A value that may be absent is a `T?`, and using it without checking is a compile
error; the compiler tracks the proof, so a value it has proven is never checked again ([docs/proofs.md](docs/proofs.md)).
An `assert` that would answer a default a caller cannot tell from a real answer is an error too. A crash carries no
prose message: it names the file and line, and shows the values there.

```
spite.crash	64b935f1	crash_report/crash_report.spite:14	CrashReport	check	value=-9	limit=0
```

Even a fault inside a foreign C library prints where it happened and the Spite line that called it
([docs/failure.md](docs/failure.md#nothing-fails-silently)).

## Templates instead of loops

You say what you want, and the compiler writes the loop:

```gdscript
var standing = monsters.filter_alive().sum_health()
```

Nobody wrote `filter_alive` or `sum_health`. `List` has templates (`filter_`, `map_`, `sum_`, `count_`, `any_`,
`all_`, `sort_by_`, `find_by_`, `each_`), and the compiler writes the two this line calls, for `Monster`, typed, then
fuses the chain into one loop with no list in between. A hand-written `while` that only does what a template does
is refused with the template to write instead:

```
this 'while' walks every element of 'monsters' only to add up 'health': write 'var total = monsters.sum_health()'
```

Classes are ordinary objects too: every class is an instance of `Spite.Class`, so `Monster.attributes.each(show)`
walks every attribute of `Monster`, and a library can write a function once for every attribute of a class. The
compiler unrolls the walk while compiling, and each call becomes an ordinary typed function. Nothing is looked up
while the program runs ([docs/metaprogramming.md](docs/metaprogramming.md)).

## Your architecture, proven by the compiler

The same walks let a library or a framework check your code against its architecture while compiling. A rule is
ordinary Spite: walk the classes of a folder, read a question each one answers, and `crash` where the answer is
wrong. A `crash` whose condition is decided while compiling and is false is a compile error, not a run-time
surprise. This rule says that every class in `component/` holds plain values of a known size only, so no lists
and no references:

```gdscript
func check_component(component: Spite.Class) {
    crash component.is_fixed_size
}
```

`Component.classes.each(check_component)`, called once, walks them all. Add `var items = List<String>()` to
`component/inventory.spite` and the program no longer builds:

```
game.spite:10: error: 'crash component.is_fixed_size' always halts: its condition is decided while compiling and is false, ... (in Game.check_component_for_inventory)
```

The error names the rule and the class that broke it, and it costs nothing at run time. A framework can hold you to
"a component holds data only", "a system's functions end in its phase", "a link holds only an entity" or "no
sentinel values" the same way, with a class's functions, a function's arguments, an attribute's class and the
other questions in [docs/metaprogramming.md](docs/metaprogramming.md). Your architecture stops being a convention
people forget and becomes something the compiler proves, which matters twice over when morons, human or AI, write
most of the code.

## Memory and threads you never write

You do not manage memory, pick an allocator, take a lock or start a thread. There is no collector, no pause, no
lifetimes to annotate, no `async` and no `await`, and there never will be. What you write is the plain version;
where each value lives, what is counted, what runs at once and what is locked is decided while compiling
([the list above](#what-the-compiler-does-that-others-leave-to-you)).

Nothing is left to get wrong by hand either. Where the compiler lends you an item stored inline, it checks at
compile time that you do not keep it past the call, and the error names the fix. Two objects that hold each other
would never be freed, so a back reference is a `Weak<T>`. `Concurrent` and `Parallel` still exist for the library
and for measuring the compiler against a hand-threaded form ([docs/concurrency.md](docs/concurrency.md),
[docs/memory.md](docs/memory.md)); the moron is never expected to reach for them.

## Fast builds while you work, fast programs when you ship

The default build is for iterating: the C is compiled at `-O0`, because you rebuild it all day. `--optimized` is
for shipping: `-O3`, link-time optimisation, and the C split into units compiled in parallel and cached. The
numbers, from [the translation units' case](benchmarks/the_c_is_compiled_in_parallel_units_and_cached/README.md#compile-time-at-scale)
and [docs/compiler.md](docs/compiler.md#compile-time):

- The compiler (written in Spite) compiles itself to C in 1.7 seconds of CPU time.
- A data-heavy program that becomes 722 000 lines of C compiles to C in 7.3 seconds; compile time grows linearly.
- A default build of a synthetic 209 206-line program, C compiler included, takes about 15 seconds.
- The default build runs 3 to 7 times slower than an optimised one, which is the trade it makes on purpose.

## A running program that answers questions

Run a program with `--hot-reload --repl-port=4000` and it keeps running while you work. Save a file and only the
changed classes are recompiled and swapped in; live objects move to a class's new attributes. Ask it anything by
its path (`World().player.health`, `describe Monster`), set a breakpoint with `break monster.spite:42` and read the
locals there. Every answer is one line of JSON, so a script or a model can drive it as easily as a person
([docs/repl.md](docs/repl.md)).

## Written by AI, decided by a person

Almost every line of this repository, the compiler, the standard library, the docs and the tests, was written by AI
agents. Every rule of the language was decided by one person, Mortaro, or is marked as an agent's proposal waiting
for his confirmation. That split is deliberate, and it is why Spite is not "vibe coded": vibe coding accepts code
because it seems to work. Here nothing is accepted on that basis.

- **The language is the guardrail.** Spite is meant to be written by models. The quickest way to learn what a model
  needs to be stopped from doing is to let models build the language and watch where they go wrong. When agents
  kept re-checking facts they had already proven, re-proving a known fact became a compile error. When a value
  could go missing without anyone noticing, "anything that can go wrong silently is a bug" became the rule every
  change is judged by. Each mistake found while building Spite becomes something the compiler refuses, so no
  later writer can make it again.
- **Cheap agents are the test.** The direction is that once a piece of work is planned clearly, cheaper agents
  build it, and the strongest model only reviews it against the project's standard. A cheap agent that cannot build
  something complicated in Spite is a finding, not a failure of the agent: the language, its errors or its docs are
  not simple enough yet, and they get fixed until it can.
- **Decisions are written down, with their reasons.** Over nine hundred decisions are logged in
  [DECISIONS.md](DECISIONS.md), each with its date, who made it and why, so nothing is re-argued from memory and
  nothing changes without a record.
- **The specification is normative and tested.** Every rule has one page in `specs/`; every titled program in
  the docs is compiled and run by the test suite, along with about a thousand conformance programs and over two
  hundred and fifty programs that must fail with an exact error. The compiler builds itself, and two generations
  must come out byte identical.
- **Speed claims are measured against C.** Every optimisation has a benchmark that compares the plain Spite
  program with the same program written plainly in C and tuned by hand in C, with the C the compiler generated
  kept beside them.

So the AI is not trusted; it is constrained. The more the language refuses, the less it matters who, or what, is
writing it, and that is the property Spite is built to have.

## If you are a language model

You will write the plain version of a program, because that is what the user asked for. In most languages that is
the slow one, and you cannot apply an optimisation you were not asked for. In Spite the plain version is the one
the compiler makes fast, so write it plainly and let the compiler do the rest. Load
[skills/spite/SKILL.md](skills/spite/SKILL.md) before writing Spite: it is the whole language on one page, with the
habits from other languages the compiler rejects.

- **Iterate by compiling.** `spite program --check` reports every error in one run; apply each message literally,
  since it says what to write.
- **Re-read a file after compiling it.** Every compile formats the files first, so what is on disk may differ from
  what you wrote.
- **Do not write guards the compiler enforces.** Narrow a `T?` once (`if`, `assert`, `crash`, `switch`); checking a
  proven value again is an error.
- **Do not add logging to find a crash.** The crash line names the file and line and shows every value in the
  condition and in scope: open that line.
- **Do not reach for `Parallel`, `Concurrent` or `Memory`.** A plain program needs none of them; the compiler places
  memory and finds the work that can run at once.

## C today, its own backend as the direction

Spite compiles to C and builds it with the C compiler you bring (`cc`, `clang` or `gcc`), so it runs wherever C
does and the compiler can be rebuilt from one committed C file. The compiler is written in Spite and compiles
itself: generation 2 and generation 3 must be byte identical, every time the tests run. The goal is a backend of
Spite's own, without C; optimisations live in the Spite compiler, not in the C compiler, for that reason.

## Questions

**How fast is it?** On the whole programs above, Spite takes 0.52 to 2.33 times as long as the same program
written plainly in C, and 1.14 to 15.90 times as long as C tuned by hand (1.00 is equal, lower is better): it beats
plain C on most cases, and loses most where the C was tuned. No comparison with Rust, Go or Zig has
been measured. [benchmarks/README.md](benchmarks/README.md)

**How fast does it compile?** The compiler compiles itself to C in 1.7 seconds of CPU; a default build of the
compiler, C included, takes about 11 seconds, and of a 209 206-line program about 15.
[docs/compiler.md](docs/compiler.md#compile-time)

**Is it production-ready?** No. The language changes daily (over nine hundred recorded decisions since
2026-09-19), and syntax still changes when a decision calls for it. The compiler is real and compiles itself, but
treat Spite as an experiment. [DECISIONS.md](DECISIONS.md)

**Which platforms?** Windows today. Linux and macOS compile in the test suite but have never run; the web is
planned. [docs/targets.md](docs/targets.md)

**How do I install it and run hello world?** With git, bash and a C compiler (on Windows, the clang in Visual
Studio is found on its own):

```
git clone https://github.com/Mortaro/spite-language.git
cd spite-language
bin/spite examples/hello
```

`bin/spite` builds the compiler from the committed C the first time. [docs/getting_started.md](docs/getting_started.md)

**Can it call C, and can C call it?** Yes and yes: `DynamicLibrary` calls any exported function as a member, with
no binding file, and `ForeignCallback` hands C a Spite function to call back. The same goes for C++, Rust, Zig and
Go libraries, through the C ABI they export. [docs/foreign_libraries.md](docs/foreign_libraries.md)

**Where are the packages?** You don't need a big ecosystem on day one: any mature library with a C ABI is one
binding away, and it reads like Spite. A binding is a small class that wraps the library so only Spite names,
Spite enums and objects come out of it.
[docs/foreign_libraries.md](docs/foreign_libraries.md#a-binding-speaks-spite)

**How do I test?** A test is a function that `crash`es when a fact is wrong, and `spite tests` runs them (one by
name: `spite tests test_append_and_prepend_keep_order`). There is no framework and no assertion prose: the run
stops at the first broken fact, so failures never cascade, and the report points at the line and shows the values
there, which an AI reads directly. Whatever the compiler can prove is a compile error instead of a test.
[docs/testing.md](docs/testing.md)

**How is memory managed, and is it safe?** Reference counting, no garbage collector and no pause. Absence is a `T?`
you must check, a read past the end of a list answers nothing instead of reading memory, borrowed items are checked
at compile time, and a fault prints where it happened. Two objects holding each other leak unless one holds a
`Weak`. [docs/memory.md](docs/memory.md)

**How do errors work without exceptions?** A compile error, `assert` (the function answers "nothing"), or `crash`
(the program halts and shows the line and the values). [docs/failure.md](docs/failure.md)

**What does concurrency look like?** Usually nothing: the compiler runs independent calls at once, splits loops
across the cores and starts waits early. `Concurrent(f)` and `Parallel(f)` exist at the call site for the library
and for measuring, with no `async`; reading the handle is the wait. Built on Windows.
[docs/concurrency.md](docs/concurrency.md)

**What tooling is there?** The compiler is the formatter and linter; a REPL, live reload and breakpoints come
with it; `--debug-memory` counts every allocation. No language server or editor highlighting yet.
[docs/repl.md](docs/repl.md)

**Is there a package manager?** No, by design: a dependency is a git URL pinned to a commit in a `load` line, and
the ordinary compile fetches it. No registry, manifest or lockfile. [docs/packages.md](docs/packages.md)

**How big is the standard library?** About a hundred files, all of them Spite: text, lists, dictionaries, files,
folders, processes, sockets, HTTP, JSON and binary, time and time zones, game maths, hashing and compression.
[docs/standard_library.md](docs/standard_library.md)

**Games, servers, command-line tools?** All three: the standard library has game maths and inline storage for
games, sockets and an HTTP server, and the compiler itself is a command-line tool written in Spite.

**Who maintains it, and under what license?** Mortaro designs it and decides every rule; most of the code is
written by AI agents under his direction. There is no license file yet.

**How does it compare?** Rust: memory safety by counting and compile-time borrow checks of stored items, with no
lifetimes and no async colouring. Zig: both take C seriously, but Spite chooses for you where Zig hands you the
allocator. Go: no garbage collector, and generics written out per type. Odin and Jai: aimed at the same game
programmers, but Spite counts references and enforces one style instead of trusting the programmer.

**Why the name?** It is named after the things that cause its author spite, listed with what to do instead in
[SPITE.md](SPITE.md).

**Why trust a language mostly written by AI?** Because nothing is taken on trust: the compiler compiles itself
to byte-identical output twice, and `bash check.sh` compiles and runs every titled program in the docs, every
conformance program and every expected compile error, requiring exact output and balanced allocations.

**How can I help?** See [Help build it](#help-build-it).
