# Write it plainly: the compiler decides how it runs

A Spite program says what it means. It does not say how the machine should do it. You write lists, loops and
plain classes; the compiler proves what is safe and chooses the fastest form: which work runs at once, how a
list's items sit in memory, where an object lives, when a wait starts. This page says why, what the compiler does
with a plain program, and how to write one it can make fast.

## Why the moron writes naive code

Every hand optimisation is a decision a person makes once, with what they knew that day, and then has to keep
true forever. A lock taken in the wrong order, a thread band sized for last year's machine, a structure of arrays
kept in step by hand, a scratch pool someone forgot to clear: each is a place to be wrong, and most of them are
wrong silently. They also hide the intention. Code full of `Parallel`, arenas and offsets says how it was made
fast, not what it does, so the next reader (often an AI) has to reverse it before changing it.

The compiler sees the whole program at once. It knows every function a call reaches, every field a loop reads,
every place a value goes. A person tuning one system sees one system. So the work is split the other way round:

- **The moron writes the intention.** A list of particles, a loop that moves each one, a system that runs every
  tick.
- **Spite writes the implementation.** It proves the facts that make a faster form safe and generates that form.
  When a fact cannot be proven, it generates the plain form, which is always correct.

Speed is not a skill the moron is expected to have. A program written by someone who has never heard of a cache
line should run as fast as one written by an expert who has, and the aim of the language is to find out how far
past the expert it can go.

## What the compiler does with a plain program

Each of these is decided while compiling, from proofs about your code. [Optimisations](optimizations.md) lists every
one with the proof that enables it, and [Proofs](proofs.md) lists every fact the compiler proves.

- **Threads.** Calls in a row that share nothing written run at once. A loop whose passes are independent can run
  on several cores. You write neither `Parallel` nor a lock.
- **Waiting.** A function that waits is known to wait; the waiting is arranged by the compiler, with no `async`,
  `await` or handle in your code.
- **Layout.** How a class's fields sit in memory, how a list stores its items, and how both line up with the
  processor's cache lines are the compiler's choice, made from which fields are read together.
- **Memory.** Where a value lives (the frame, a constant, the heap) is decided from how long it is proven to live.
  You never free anything and never pick an allocator.
- **Checks.** A read, a division or a narrowing the compiler has proven safe carries no run-time check.
- **Size.** Anything your program does not use is not in it.

None of this changes what your program prints or computes. It changes only how fast it runs and how big it is.

## No runtime, so it runs anywhere

All of that happens while compiling. Nothing ships beside your program to do it while it runs: no scheduler, no
collector, no interpreter, no registry. That is what lets one language reach from an operating system kernel to a
web page: a microcontroller with a few kilobytes has no room for a runtime, and a WebAssembly module downloaded
with a page should not carry one. A program that uses no threads has no thread code in it; a program that never
waits has no waiting machinery. Where only the run can know something (how long a list read from a file is), the
compiler may leave one choice to the run, a branch between two forms it compiled, and
[Optimisations](optimizations.md) says so wherever it does.

## How to write a program the compiler can make fast

Write the obvious thing, and let each piece of state belong to the thing that owns it.

```gdscript
var particles = List<Particle>()

func step() {
    particles.each_move()
}
```

- **Use lists and loops, not machinery.** A loop over a list is the most optimisable thing you can write. A loop
  that keeps an index into another list by hand, or a hand-made pool, hides from the compiler what it needs to see.
- **Give separate work separate state.** Two systems that each keep their own lists can run at once; two systems
  that both bump one shared counter cannot. If two things do not need to share, do not make them share.
- **Keep an item's identity in its list.** An item nobody keeps a reference to outside its list can be stored
  however is fastest, field by field if that is what the loops read.
- **Do not split, pack or pad classes by hand.** Write the class with the fields it means to have. The compiler
  splits it, packs it or aligns it where the loops say it pays.
- **Measure only a production build**, and read `--optimization-report` to see where a proof did not hold
  ([compiler.md](compiler.md#read-what-was-not-optimised)). A fallback is the plain form, never a wrong answer;
  the report names what stopped the proof, so you can change that one thing.

`Concurrent`, `Parallel` and the `Memory` classes are documented in [Concurrency](concurrency.md) and
[Memory](memory.md). They exist for the standard library and for code that measures the compiler against a
hand-written form. A plain program does not need them, and should not reach for them to go faster: if the plain
form is slower than the hand form, that is the compiler's bug to fix, not the program's.

---

Next: [Classes and files](classes_and_files.md), what a file, a class and a folder are.
