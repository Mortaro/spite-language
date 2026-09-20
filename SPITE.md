# Spite

Things that cause Mortaro spite. The language is named after them.

This file exists so an agent does not have to rediscover them by proposing one and being told no. Every entry says
what is hated, why, and what to do instead — a list of dislikes without alternatives just makes agents timid.

When Mortaro rejects something new, add it here with its reason.

---

## Language design

**Exceptions, error bubbling, and defensive code.** "Errors and exceptions tend to be useless because they tell us
a message we have no action to take about them." Bubbling an error up for a developer to eventually log is
"masturbatory" — it does not help, it just makes code defensive.
*Instead:* three outcomes and no others (D24). A compile error for anything the compiler can know, `assert` when
the program should keep running, `crash` when it should halt. `T?` is the only runtime failure value and
carries no reason. If a distinction is actionable it is data — an enum or a union — not an error.

**`async`/`await` colouring.** The JS and C# pollution: the callee declares itself async, which infects every
caller transitively and changes every return type.
*Instead:* concurrency is a property of the call site, never of the function (D35). An ordinary function is made
concurrent by its caller with `Task(...)`, and the handle joins on first use — no `await`, no wrapper type, no
`.wait()` to remember.

**Bring-your-own-runtime.** Rust's `Future`-without-an-executor split "truly pisses me off": it let multiple
community packages emerge, so every library had to declare which runtime it targeted.
*Instead:* one concurrency model, in the language, with no protocol for an external driver to implement (D35).
Never define a trait that requires something else to drive it.

**JSX, and configurable transpilation generally.** "JSX is too broad and you can configure what things do under
the hood."
*Instead:* markup is ordinary metaprogramming (D18) — tags from `missing_function`, children variadic, components
matched by shape, handlers as bound functions. No second grammar, nothing to configure.

**JSON as a wire format.** "A web convention that creates inefficiency for human readability, but a human will
never read those."
*Instead:* compile-time-derived binary packing (D31). JSON's whole cost is self-description, and there are no
unknown consumers when both ends compile from the same source. JSON stays only for foreign systems.

**Static class functions.** There is nothing for `static` to mean when a class is an instance of `Spite.Class`
(D6).
*Instead:* a class-level function is a function on that class object, declared by `Spite.Class` and overridden by
the class file.

**Rust-style borrow-checking noise.** From the original spec: "we should make things memory safe by using unions,
but we can be more permissive instead of all that rust noise."
*Instead:* reference counting (D1), `Monster?`, and `assert`/`crash` narrowing.

**Hidden costs — but hidden optimisations are welcome.** The rule is not symmetric (D36). Code that runs slower
than a reader expects is the only real surprise; code that runs faster is a free win and needs no announcement.
*Instead:* optimise freely and silently. Report only what could *not* be optimised, and why.

**Anything that requires user discipline to work.** "Users are morons, they will forget." A rule that depends on
someone remembering to call something is a rule that fails.
*Instead:* the compiler does it. REPL drains are injected wherever the program already waits (D37); decomposition
is forced by the type system rather than by advice (D27).

**More than one way to do a thing.** The compiler is the formatter and the linter; style is not a matter of taste.

**Warnings.** The compiler either reformats your code or errors. Nothing is left to the user's judgement.

**Ceremony that sits outside the language's own patterns.** The `generics` header line was removed for exactly
this reason: it "feels outside of our patterns" (D5, D9).
*Instead:* before adding syntax, check whether an existing mechanism already expresses it — a class-level
function, a constructor signature, a naming convention, a template.

**Project setup with a thousand ways to do it.** C++ is avoided largely for this: too many ways to configure a
project and its dependencies, which is what pushes Mortaro toward Rust and Node despite their own problems.
*Instead:* a dependency is a git URL with a commit hash in the `load` call, fetched by the ordinary compile
(D38). No package manager, no registry, no lockfile, no fetch step, no manifest.

**Ecosystems that compete with each other.** "Every day a new ecosystem arises to compete with npm", and the
bundler story in Ruby. Same spite as bring-your-own-runtime: it fragments what should be settled.
*Instead:* git is the registry, so there is no central index for a competitor to fork from (D38).

---

## Code style

**Abbreviations, anywhere.** "Avoid abbreviations like the devil, even if I accidentally abbreviated something we
shouldn't." `DLL` became `DynamicLibrary`; `Library` was rejected separately for being too generic.

**Vague names.** `add` does not say where, so it is `append` and `prepend`. A name should not need a comment to
disambiguate it.

**Giant comments explaining obvious things.** The behaviour that started D34: eleven lines of prose above a
two-line function, explaining the compiler rather than the code.
*Instead:* a comment is one line and is nothing but a markdown link (D34). If a reader could derive it from the
code, delete it. If it warns against a change, write a test or a compiler diagnostic — both push harder than
prose.

**Clever code.** "We don't need clever code for anything since AI will write most programs."

**`for` loops.** Removed permanently, to push people toward the metaprogramming (D2 era).
*Instead:* `while`, or the `<member>` templates (D15).

---

## Working on this repository

**`main` as the default branch.** "I do not like main." The default branch here is `master` and stays that way;
do not rename it, and do not suggest renaming it.

**Clobbering shared files.** Two agents whole-file-writing `manual.md` and `PLAN.md` lost a day's decisions once
already (a `robocopy /MIR` restore rolled them back an hour).
*Instead:* targeted edits matching on surrounding text, never line numbers; re-read immediately before writing;
the decision log is append-only and rows are never renumbered or reordered.

**Treating `mortaros_notes.md` as anything but an inbox.** It is where Mortaro drops notes; an agent moves them
into the manual and clears it. Agent-owned state belongs in an agent-owned file.

**Losing decisions.** `manual.md` is normative, and every language decision goes in it and in its decision log
when it is made. A decision that exists only in a conversation is a decision that will be re-litigated.

**Delegating work to budget subagents when it is slower.** If the round-trips and cold contexts cost more quota
than doing the work inline, do it inline.

**Asking instead of deciding, or deciding instead of asking.** Mortaro decides language semantics. An agent
proposes, marks its own proposals as unconfirmed, and records what was actually decided by whom.
