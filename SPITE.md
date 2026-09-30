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
carries no reason. If a distinction is actionable it is data — an enum or a union — not an error. A crash carries
no message either (D297): its report points at the line of code and shows the memory there, which an AI reads
instead of prose someone wrote about it.

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
This one is personal: two ways of doing one thing pisses Mortaro off, and he will spend hours deciding which one is
best rather than live with both. Every pair is a decision somebody has to make again at every call site -- "who
owns a conversion, `Float.from_integer` or `to_float`?" was never clear, so only `to_<type>()` on the value being
converted survives (D275).
*Instead:* before adding anything, look for the way that already exists and use it. When two ways exist, pick one
and remove the other, and when you cannot pick, ask him -- never leave both "for flexibility". Where he keeps both
for now, it is on the list to hard-limit later, and new code uses the preferred one.

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

**Em dashes, anywhere.** "Absolutely no emdashes in the entire codebase for spite/slop/theseus. I really hate
emdashes, my grandmother was killed by an emdash when I was a child, even though she is still alive." This covers
the `—` character and the ` -- ` stand-in written in prose, in code, comments, docs and commit messages across
SpiteLanguage, SlopEngine and SlopTheseus (command-line flags like `--optimized` are not dashes).
*Instead:* end the sentence, or use a colon, a comma or parentheses.

**Docs that talk to agents.** `docs/` is documentation for people learning Spite: no notes to AI writers, no
implementation status, no decision bookkeeping in the middle of a page. Each page ends with a link to the next
thing to learn.
*Instead:* agent guidance, decisions, proposals, open questions and implementation status live outside `docs/`.

**Naming a third-party package inside the language.** SlopEngine and Sword of Theseus are packages built on Spite,
third-party even though Mortaro maintains them. The Spite repository never mentions them: not in docs, examples,
tests, benchmarks, diagnostics, comments or decision rows.
*Instead:* write examples, tests and benchmarks in Spite's own terms. A need found through a package is recorded
as the language need it is, without naming the package.

**Abbreviations, anywhere.** "Avoid abbreviations like the devil, even if I accidentally abbreviated something we
shouldn't." `DLL` became `DynamicLibrary`; `Library` was rejected separately for being too generic.

**Vague names.** `add` does not say where, so it is `append` and `prepend`. A name should not need a comment to
disambiguate it.

**Giant comments explaining obvious things.** The behaviour that started D34: eleven lines of prose above a
two-line function, explaining the compiler rather than the code.
*Instead:* a comment is one line and is nothing but a markdown link (D34). If a reader could derive it from the
code, delete it. If it warns against a change, write a test or a compiler diagnostic — both push harder than
prose.

**Destructuring and lambdas.** "Writing it is fun, but it's not a human who will write code in this language,
so it's a pointless readability sacrifice" (D62).
*Instead:* read members by name; pass a named function, which is bound to its instance (D17).

**Function overloading.** Ambiguous: one name, several meanings, chosen by argument types the reader has to
work out (D59).
*Instead:* one name is one function, and an argument casts to the parameter's type.

**Copying a value into a local just to narrow it.** `var watcher = tracker; assert watcher` is "a human practice,
and in real life a war crime against the Geneva convention" (D63).
*Instead:* narrow the name or the path itself: `assert tracker`, `assert tracker.target` (D43).

**Messy code that an AI will copy.** "If AI can do this messy code, it will do this messy code." A long way
round that the compiler accepts is a pattern that spreads.
*Instead:* make the short form the only form the compiler accepts (D63, D69), and prefer a compile error to a
convention.

**Clever code.** "We don't need clever code for anything since AI will write most programs."

**`for` loops.** Removed permanently, to push people toward the metaprogramming (D2 era).
*Instead:* `while`, or the `<member>` templates (D15).

---

## Working on this repository

**`main` as the default branch.** "I do not like main." The default branch here is `master` and stays that way;
do not rename it, and do not suggest renaming it.

**Clobbering shared files.** Two agents whole-file-writing the old `manual.md` and `PLAN.md` lost a day's decisions once
already (a `robocopy /MIR` restore rolled them back an hour).
*Instead:* targeted edits matching on surrounding text, never line numbers; re-read immediately before writing;
the decision log, `docs/decisions.md`, is append-only and rows are never renumbered or reordered.

**Treating `mortaros_notes.md` as anything but an inbox.** It is where Mortaro drops notes; an agent moves them
into the docs and clears it. Agent-owned state belongs in an agent-owned file.

**Losing decisions.** The docs are normative (D193), and every language decision goes into the page that teaches
that part of the language and into the decision log, `docs/decisions.md`, when it is made. A decision that exists only in a conversation is a decision that will be re-litigated.

**Delegating work to budget subagents when it is slower.** If the round-trips and cold contexts cost more quota
than doing the work inline, do it inline.

**Asking instead of deciding, or deciding instead of asking.** Mortaro decides language semantics. An agent
proposes, marks its own proposals as unconfirmed, and records what was actually decided by whom.
