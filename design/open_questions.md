# Open questions and decided work

Two lists that belong to no single page: what Mortaro decided that has not reached the page it belongs on,
and the questions still open. Both were moved whole from the language manual when [D193](decisions.md)
dissolved it; a `D` number is a row of the [decision log](decisions.md). An answered question keeps its
place, marked with the decision that answered it, so the argument is not lost. Questions only Mortaro can
answer are gathered in [`mortaros_missing_decisions.md`](../mortaros_missing_decisions.md).

## Decided by Mortaro, being implemented  **[planned]**

Decisions Mortaro wrote into his notes (2026-09-18 to 2026-09-20) before they had a page. Each one moves into
the page it belongs on once it is built; the earlier batches have all moved (the decision log says where), and
these five are what is left, with where each stands today.

1. **Standard library classes can be reopened** like any other class: the way to try out a package before
   upstreaming it. **Built**: a program's own `list.spite` reopens `List` like any class
   ([packages.md](../docs/packages.md#monkey-patching-mods)); this item stays only as the record of why.
2. **Comprehensive, Ruby grade reflection, at compile time.** Every object gets its reflection from source that Spite
   generates and that is VISIBLE in the final class output: `attributes` returning a `List<Spite.Attribute>`, `class`,
   `functions`, and so on are real generated members, not compiler magic. The goal is to cover most of what Ruby
   metaprogramming covers (enumerate and call functions by Symbol, respond-to checks, defining members from data,
   hooks when a class is reopened) resolved at compile time. **Partly built**: the read-only reflection objects,
   `has_function` and the Symbol templates ([reflection.md](../docs/reflection.md), [metaprogramming.md](../docs/metaprogramming.md));
   defining members from data and hooks on reopening are not.
3. **Errors: there are none to handle, only `crash`.** (third batch, 2026-09-20.) The main author of Spite code is
   an LLM, and bubbling errors up for someone to eventually log only makes code defensive. So there are no
   exceptions and no result types: what can simply not work answers the default or a `T?` and is handled with
   `assert`, and what stops the program from functioning crashes with a report written for an LLM. **Built** as
   D24's three outcomes and D25's report ([failure.md](../docs/failure.md#three-outcomes-and-no-others)), in a different
   shape from the note: `crash condition` takes a condition, not a message; the report is one tab-separated line
   naming the site and the values the condition read, followed by the asserts that failed before it (the
   standard library's own asserts are left out, D189), not a `.spite/crash.json`. Not built from the note:
   `crash` inside a lint at compile time, and a `--diagnostics=json` form for compile errors (proposed by Claude,
   unconfirmed).
4. **The compiler is also the language server**, so editors get real time validation. After the bootstrap,
   written in Spite. **Not built.**
5. **Bootstrap as soon as possible** to start the repository; fancy features wait until after. **Done**: the
   compiler is Spite and compiles itself ([self_hosting.md](self_hosting.md)).

## Open questions

1. **(Answered by D236: `var x: T = null` on a non-nullable `T` is a compile error naming `T?` and `T()`, and a
   generic class writes `$name()` for its default; [values_and_types.md](../docs/values_and_types.md#variables-and-values--implemented).
   The history below is kept.)** `var damage: $damage_type = null`: `null` otherwise only exists for `T?`. PROVISIONAL: the compiler treats
   `= null` on a `$generic`-typed variable/field as "the default value of whatever type the generic is bound to" (not
   `T?`). This is implemented but still provisional -- revisit if it reads confusingly once more code exists.
   When the bound type is a `type` whose members are all attributes, its default is a real object -- the object
   literal with each attribute at its own default, admitted to the shape -- so writes through it are kept
   (proposed by Claude, unconfirmed, 2026-09-24; `conformance/stage6/shape_defaults`). A `type` that requires a
   function has no default object, since no literal can supply the function, so that case is a compile error
   naming the attribute (D211, [metaprogramming.md](../docs/metaprogramming.md#codegen-values---implemented)).
3. **(Answered: a comparison whose right side is wider than its left is a compile error naming the comparison turned around, as D162 made it for arithmetic; decided by Claude under D205, [values_and_types.md](../docs/values_and_types.md#casting).)** Right-to-left casting made `age > 0.5` with an Integer `age` mean `age > 0`.
   - The abbreviation lint has no escape hatch for names that must mirror an external spelling (`keyword_var`). Keep it absolute, or allow a per line `# spelled: keyword_var` style exemption.
6. **(Answered by D136 and D137: `_` means unused on purpose only on a parameter and private everywhere else, and
   an unread local, parameter or attribute is always an error, [Unused is an error](../docs/style.md#unused-is-an-error--implemented).)**
   `_` now means two things: private ([Lexical structure](../docs/classes_and_files.md#lexical-structure--implemented)) and intentionally unused ([Unused is an error](../docs/style.md#unused-is-an-error--implemented)). They mostly agree (an unused
   private function is fine either way), but an unused PUBLIC function cannot be an error (libraries are full of them; tree
   shaking removes them), so "unused" is only enforced for locals, parameters and private functions. Confirm.
   D118 adds attributes, where `_` already meant private: an unread `_name` attribute is never reported, since the
   prefix says both things at once, so a dead private attribute passes ([Unused is an error](../docs/style.md#unused-is-an-error--implemented), `mortaros_missing_decisions.md`
   item 111).
8. **An unrelated `get_<attribute>()` silently intercepts a read.** Found by the 2026-09-20 reflection port, in
   this compiler's own `NullableType`: a class with an attribute `inner` and a zero-argument function `get_inner()`
   written for an unrelated purpose has every outside read of `.inner` routed through that function, because that
   is exactly what attribute interception ([Operators](../docs/functions_and_operators.md#operators--implemented)) says to do. It is the rule working as specified and it is
   still a trap, since nothing announces it.
   - **Keep it.** The rule is uniform and a reader who knows it can see the collision.
   - **Make it a compile error when the function's return type differs from the attribute's** (proposed by
     Claude). A real getter returns what the attribute holds; an unrelated function usually does not, so this
     catches the accidents and leaves the genuinely ambiguous case -- same name, same type -- being treated as
     a getter, which is defensible. It is also the shape this language reaches for everywhere else: something
     the compiler can know becomes a compile error naming the fix, rather than a warning or a convention
     ([Style](../docs/style.md#style--implemented): the compiler has no warnings; D24).
   The residual case neither option catches is an unrelated function whose return type coincides with the
   attribute's. Claude would accept that.
9. **Text with `${name}` in it prints a stray `$`** (found 2026-09-20 by writing the programs an AI would write;
   restated 2026-09-25). Text holds its values in `{ }` (`"hello {name}"`), so JavaScript's `"hello ${name}"`
   is a literal `$` followed by a hole and prints `hello $world` -- the mistake is silent, which a compiler that
   has no warnings ([Style](../docs/style.md#style--implemented)) should never allow. `$` is already the codegen sigil, so
   `${` inside text is unlikely to be meant literally.
   - **Make `${` inside text a compile error** naming `"hello {name}"` (proposed by Claude). A dollar before a
     brace has no other use, and text that genuinely needs one can hold the dollar in a value
     (`var dollar = "$"`, then `"{dollar}{name}"`).
   - **Leave it.** Text is text, and a rule about what may appear inside it is a rule to remember.
10. **How `--final-classes` shows which root supplied a declaration.** D7 and milestone 10a both say a
    reopening must not be silent, and `--final-classes` is where it stops being silent -- but what it writes is
    a *program*: running the printed entry file runs the same program, which is what makes it proof rather than
    a report. Provenance cannot be a comment, because a comment is only ever a link to a markdown heading
    ([Style](../docs/style.md#style--implemented)), and it cannot be a declaration without changing the program.
    - **A separate manifest** beside the printed classes (proposed by Claude): one line per declaration with the
      root it came from. Keeps the printed source a program, and the thing you grep is a table rather than
      prose scattered through files.
    - **Print it to the console** as the classes are written, so it is read once and not stored.
    - **Give the comment rule one more form**, a provenance line the compiler writes and a human never does.
11. **Whether a `type`'s function members should be written as function-valued attributes** (Mortaro, 2026-09-21,
    thinking aloud rather than deciding). Today a shape writes `hit(): Integer`; the alternative is
    `hit: Spite.Function<Integer>`, which would make a required function an ordinary attribute whose type happens to
    be a function, and would leave a `type` with exactly one kind of member instead of two.
    - **It could be written now**: the typed `Spite.Function<Arguments..., Return>` (D39) exists, so a shape
      could name one; nothing has been decided.
    - Against: `hit(): Integer` reads like the declaration it matches, and a shape is matched against functions
      written `func hit(): Integer`.
    - For: one kind of member, and it composes -- a shape could then require a function value it will *store*,
      which `hit(): Integer` cannot express.

12. **(Answered by D87: `generic $name` header lines. Built 2026-09-24, [Codegen values (`$`)](../docs/metaprogramming.md#codegen-values---implemented).)** **A header form for generics, with constraints** (Mortaro, 2026-09-23, asked to be argued with). The proposal:
    `generic $type` lines at the top of the file beside `singleton`, and a described generic
    `generic $sub_type { initial_value: Spite.Function<$sub_type> }` that narrows what may be supplied. The
    problem it solves is real: a class that takes codegen values must have a constructor to declare them, even
    when it has nothing to construct, and the constructor form has nowhere to say what a supplied class must be
    able to do. And the distinction Mortaro draws is the right one -- a described generic is *one* class for every
    place that uses it, where a `type` accepts anything that quacks, afresh at each call.
    Claude's argument, for and against (proposed by Claude, unconfirmed):
    - **For the header line:** `singleton` already made a header line a pattern, so parity is a real argument, and
      the objection that removed the old `generics` line (SPITE.md: "feels outside of our patterns") was about
      an ordered *list* in a line of its own; one `generic` per line is a declaration like `var`, not a header.
    - **Against the inline block:** it is an anonymous `type`. Write the constraint as a shape the program
      already has -- `generic $sub_type: Openable` -- so one mechanism describes what a class must be able to do.
      An inline block is then just sugar for an unnamed `type`, and can come later if it is missed.
    - **Against `gen`:** it is an abbreviation, and the abbreviation rule has no exceptions for keywords.
      `generic` says what it is.
    - **What becomes implicit:** a caller's `Journal<String, Chapter>()` is positional, so the order of the
      `generic` lines becomes the order callers write. The constructor form made that order visible in one
      signature; the header form spreads it over lines. Acceptable, since D67 fixes where the lines go.
    - **The example's `var opened = $sub_type`** reads as assigning a class to a variable. Claude assumes
      `var opened: $sub_type` was meant.
13. **(Answered by D275 and D293: a conversion is the source's `to_<type>()`, never a `from_` function, and a class
    becomes castable by defining `to_<type>()`; [values_and_types.md](../docs/values_and_types.md#numbers-are-classes-and-this--implemented).)**
    **How casting works, so a class can define its own casts, and how to name a variable's class as a type**
    (Mortaro, 2026-09-23: "a thing for you to ask me later"). Example shape then: a `from_` function, now refused. Not argued yet; waiting to be asked. D59 (arguments cast to their parameter type) is where it
    will first matter.
14. **(Answered by D90: `...args: List<Type or Class>`. Built 2026-09-24, [Variadic arguments](../docs/functions_and_operators.md#variadic-arguments--implemented).)** **An ABI for variadic arguments** (Mortaro, 2026-09-23, "fight me on this before we implement"). Proposed:
    `func hello(world: String, ...args: List<Spite.Argument<String>>)`, which would make `Spite.Argument` generic.
    Claude argues against the `Spite.Argument` part (proposed by Claude, unconfirmed): `Spite.Argument` is the
    *reflection* of a parameter -- a name and a class, known at compile time -- and a variadic argument is a
    *value* at runtime. Making one class carry both merges the two levels D11 keeps apart. `...args: List<String>`
    already says everything: the `...` means "the caller writes these one by one", the list is the type the
    function receives. For `Console.print`, which takes anything, the element type is a `type` every printable
    value satisfies -- `...values: List<Printable>` -- so the language needs no new class, only `...`.
15. **(Answered by D171: it stays, and a `while` doing only what a member template does is an error. Built
    2026-09-25, [Control flow](../docs/control_flow.md#control-flow--implemented).)** **Whether `while` can go** (Mortaro, 2026-09-23: investigate every use; if it can be rewritten with
    metaprogramming, make it an error). Measured on 2026-09-23 over the compiler, `library/`, `scripts/`, the
    tests, the examples and the corpus: 259 `while` loops, and 199 of them are the same shape -- an index from 0
    to `list.count()`, reading `list[index]`. Every one of those is a member template (`each_`, `map_`,
    `filter_`, `find_by_`, `any_`, `count_`, `sum_`). The other 60 are genuinely loops over state: the lexer
    scanning characters, the parser consuming tokens, walking backwards, settling until nothing changes.
    Proposal (Claude, unconfirmed): make the 199-shape an error naming the template -- detected as a `while`
    whose condition compares a counter with `.count()` of a list the body indexes by that counter -- and keep
    `while` for the rest. This pairs with D64: an index read is where `[]` becomes `T?`, and removing the
    index loops removes almost all of the narrowing D64 would otherwise demand. What the templates still lack
    for the compiler's own loops: the index inside the body, and stopping early.
16. **(Answered by D296: two versions are two different libraries, never unified and never an error, and the
    compiler folds identical C functions into one so the duplicate costs nothing;
    [packages.md](../docs/packages.md#two-versions-of-one-repository).)** **Two versions of one dependency** (Mortaro, 2026-09-23). When two packages load the same git dependency at
    different commits and the difference changes nothing either package uses, the compiler should just use one,
    without asking. When the versions differ in members that are used, the compiler treats them as two packages
    in two namespaces, so both keep working, and a command lists these splits for whoever wants to unify them --
    "not actually broken, just annoying". Belongs to D38 (a dependency is a git URL plus a commit in `load`),
    which is built since D283/D289; D296's two libraries are built too
    ([packages.md](../docs/packages.md#two-versions-of-one-repository)).
17. **(Answered by D83/D88: `this` where a class names itself.)** **A syntax for `this`** (Mortaro, 2026-09-23): to be discussed when something needs it. Today bare names
    reach attributes and `class` is the instance's class, so nothing does yet.
18. **(Answered by D89, and finished by D130: a `.spite` file path is an error naming the folder form.)** **The entry file is always the file named after its folder** (Mortaro, 2026-09-23, asked to be argued for).
    `spite hello` runs the `hello` folder's entry file; a file name on the command line is no longer accepted.
    The case for it (Claude): discovery already requires an entry folder, so the file name on the command line
    only restates the folder -- two ways to say one thing, and the second can disagree (`spite hello/other.spite`).
    One way to run a program is the rule SPITE.md states under "more than one way to do a thing". The cost is
    small and known: editors that pass the current file will pass its folder instead, and a compile error naming
    the folder covers the habit. Claude would do it. **Half built (2026-09-23):** `spite hello` runs `hello/hello.spite` now; a file argument still works, and making it an error is the part that waits on Mortaro.
19. **(Answered by D88: getter-only functions.)** **Constants without a `const` keyword, and reflection attributes that cannot be overwritten** (Mortaro,
    2026-09-23). Constants: yes, without a keyword -- the compiler sees the whole program, so an attribute that
    nothing assigns after its default (no assignment, no `set_` call, no reflective write) is a constant and
    folds. Nothing is declared; it is just what the optimiser finds (D36). Read-only attributes: Mortaro's idea
    is to declare no attribute at all, only `get_attributes()`, which attribute interception already turns into
    a readable `.attributes`; with no `set_attributes()` there is nothing to assign through, so writing it is an
    error. What the compiler needs (Claude, unconfirmed): a read of `.name` that finds no attribute but finds
    `get_name()` is an attribute read, and a write to it with no `set_name()` is an error saying the attribute
    is read-only. The storage behind it is a private `_attributes`, which the compiler fills as it fills
    `attributes` today. **The mechanism is built (2026-09-23):** a read that finds no attribute but finds `get_name()` reads through it, and a write with no `set_name()` is an error calling the attribute read-only (`tests/interception_tests`, `diagnostics/read_only_attribute`). Moving `Spite.Class`'s own members onto it is left for Mortaro to confirm.
20. **(Answered by D170: it is. Built 2026-09-25, [Control flow](../docs/control_flow.md#control-flow--implemented).)** **Whether a nested `if`/`else` is an error** (Mortaro, 2026-09-23: "we should discuss"). Claude's view: nesting
    is where generated code becomes unreadable, and the language already removes the common cases -- a
    precondition is `assert` (D54), a choice between kinds is a `switch` over a union or enum. What remains is a
    genuine decision tree, and forbidding it pushes it into a helper function, which is usually the right call.
    A concrete rule to decide on: an `if` with an `else`, inside a branch of another `if` with an `else`, is an
    error that names extracting a function.
