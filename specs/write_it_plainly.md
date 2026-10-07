# Write it plainly: the compiler decides how it runs

The specification of [Write it plainly: the compiler decides how it runs](../docs/write_it_plainly.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

- A program states what it computes; how it runs (threads, waiting, layout, alignment, placement, checks) is the
  compiler's choice, made while compiling from what it proves.
- An optimisation never changes what a program prints, computes or crashes on. When its proof does not hold, the
  compiler generates the plain form and `--optimization-report` names the place and what stopped the proof.
- Every optimisation is documented in [Optimisations](../docs/optimizations.md) together with the proof that enables it,
  and every proof in [Proofs](../docs/proofs.md).
- Nothing runs beside a program to optimise it: no scheduler, collector, interpreter or registry ships with it. A
  feature a program does not use is not in it. The only run-time choice an optimisation may leave is a branch
  between forms compiled in advance, on a fact only the run can know, and its page says so.
- A class is a unit of meaning, not of storage. The compiler chooses a representation for each use of a value
  (each list, each loop, each point of the program), not one per class: it may split a class into several, merge
  an object into its owner, keep one list in different shapes in different places with a conversion between
  them, or restructure a procedural program into passes over columns, whenever the result is the same.
- A plain program written with lists, loops and classes must be able to reach the speed of the same program
  written with `Concurrent`, `Parallel` and `Memory` by hand. Where it does not, the compiler is wrong.

---

Next: [Classes and files](classes_and_files.md).
