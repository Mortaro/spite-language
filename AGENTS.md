# Working on Spite

**Read [`mortaros_notes.md`](mortaros_notes.md) first, every time.** It is Mortaro's inbox. For each note: record the
decision (the rule in the spec page for that part of the language, the teaching in its docs page, and a row in
[`design/decisions.md`](design/decisions.md)) or act on it, then delete the note, so the inbox only ever holds
what nobody has handled yet. Questions only Mortaro can answer go to
[`mortaros_missing_decisions.md`](mortaros_missing_decisions.md).

**Ask of everything you build: can it be tree-shaken, and does it keep Spite at zero runtime?** A program that does
not use a feature must carry none of it, and nothing may need a shipped scheduler, interpreter or registry: the
work happens while compiling instead (Mortaro, 2026-09-25; D147, D176). Debug and REPL features may cost something
only in those builds (D143). Say how a proposal tree-shakes and what it costs at run time.

**Anything that can go wrong silently is a bug** (Mortaro, 2026-09-27; D244). Every failure is loud: a compile
error, or a crash that names its cause, and never a wrong value, a lost write, a skipped step, a leak or a hang.
Judge everything you build, and every open item, against it: a compiler rule that lets one of those through, a
library function that answers a default a caller cannot tell from a real answer, a report that prints a value where
something is missing. Each is a bug to fix, not a style to document. When you find one you cannot fix now, write
it into the "still open" list of [design/status.md](design/status.md).

Read [`SPITE.md`](SPITE.md) first. It lists what Mortaro hates and what to do instead, and it is the point of the
project. Then the docs, in the [reading order](docs/README.md#reading-order). **The specification is normative**
(D511): `docs/` is the tutorial, each page teaching its part of the language and ending with a link to the next, and
`specs/` holds the formal rules, one page per docs page, with every edge case and exact error text. When anything
else disagrees with the specification, the specification wins; a docs page that disagrees with it is the bug.

## Where things live

- `docs/` is documentation for people learning Spite, and nothing else. It holds no notes to AI writers, no
  implementation status ("not built", "planned", "being replaced"), no decision numbers or open-question
  bookkeeping, and no mention of third-party packages. Each page teaches the language as decided and ends with a
  link to the next page in the [reading order](docs/README.md#reading-order). It does not repeat the rules in
  full: it links the spec page for the edge cases and the error texts.
- `specs/` is the formal language specification ([`specs/README.md`](specs/README.md)): one page per docs page,
  named after it, holding every rule, edge case and exact error text of that part of the language. It is
  normative and user-facing like `docs/`, with the same limits: no status, no decision numbers, no notes to agents.
- `design/` is for the people and agents who build Spite:
  - [`design/decisions.md`](design/decisions.md): the append-only decision log, every decision with when and why;
  - [`design/status.md`](design/status.md): what is decided but not built, or only partly built, page by page,
    and the bugs still open under D244;
  - [`design/backlog.md`](design/backlog.md): everything decided and not built, as work: size, dependencies and
    parallel streams;
  - [`design/open_questions.md`](design/open_questions.md): decided work that has no page yet, and open questions;
  - [`design/proposals/`](design/proposals/): proposals under review;
  - [`design/naive_programs.md`](design/naive_programs.md): the plan for making naive programs fast, with its
    pairs of optimisation and proof in [`design/naive_programs_pairs.md`](design/naive_programs_pairs.md);
  - [`design/optimization_research.md`](design/optimization_research.md): the open notebook of optimisation
    theories, where every idea is welcome and agents record what they find;
  - [`design/KNOWN_ISSUES.md`](design/KNOWN_ISSUES.md): where the compiler falls short of the docs;
  - [`design/self_hosting.md`](design/self_hosting.md): how the compiler builds itself, and what proves it;
- `skills/spite/` is the skill for an AI that writes Spite: [`SKILL.md`](skills/spite/SKILL.md) and the whole
  language on one dense page, [`reference.md`](skills/spite/reference.md). It is user-facing like `docs/`, and it
  changes with the language.
- [`WHY.md`](WHY.md) and [`WHY_AI.md`](WHY_AI.md) say why to use Spite, to people and to language models; every
  claim in them is true of Spite today or marked as the direction.

When you build something the docs describe, delete its line from `design/status.md`. A decision that is not built
yet is still described on its page as the language; `design/status.md` is where it says it is not built.

## Writing

- **No em dashes, anywhere** (SPITE.md): not the character, and not two hyphens between spaces standing in for
  one, in code, comments, docs, diagnostics or commit messages. End the sentence, or use a colon, a comma or
  parentheses. `check.sh` and the `commit-msg` hook refuse them.
- **Tables have one set of columns** (Mortaro, 2026-10-07): never two tables side by side in one, repeating the
  columns across a row (`| Type | C type | | Type | C type |`). It is hard to read on a phone. Write one row per
  item, and split into two tables if the rows fall into two groups.
- **No third-party package names in this repository** (SPITE.md): examples, tests and benchmarks use neutral
  invented packages. `check.sh` refuses them too.

## Commits

**Always start a commit message with a gitmoji.** Mortaro likes them; this is not optional.

```
✨ add crash keyword with assert polarity
🐛 fix double free in generated get_<attribute>()
📝 write section 5 prose for D17 bound functions
♻️ port Symbol codegen to the Spite compiler
✅ add stage5 conformance programs for crash
🔥 remove a dead code path
```

Common ones: ✨ feature · 🐛 bugfix · 📝 docs · ♻️ refactor · ✅ tests · 🔥 removal · ⚡ performance · 🚨 lints ·
🏗️ structure · 🔒 security · ⏪ revert. One gitmoji, then a lowercase imperative summary.
**Every commit names the model that wrote it**, so a change that breaks something has an owner:

```
Co-authored-by: Claude Opus 5 <noreply@anthropic.com>
```

Name the model you are actually running as, not just "Claude". A human committing by hand writes their own. A
`commit-msg` hook rejects a message without it. Nothing identifies the *session*: session names are local to
Mortaro's machine and do not belong in a public history. Hooks are versioned in `scripts/hooks`, so each clone
runs once:

```
git config core.hooksPath scripts/hooks
```

## Changing a Spite program: a worktree is a folder that loads it

To propose a change to a Spite program (a package, an example, anything written in Spite), do not edit it in
place: make a small folder beside it whose entry file `load`s the original and reopens only the classes you change
(D156). Mortaro runs that folder to test the change, and only an approved change is merged into the real code.
Nothing is copied, so a worktree costs only the files it changes.

## Changing the compiler: every optimisation and every proof is documented in the same commit

**The benchmarks' generated C is committed with the change that alters it** (D525 to D527). Every folder in
`benchmarks/` keeps the whole C the compiler generates from its Spite program. A compiler change regenerates those
files in the same commit, and only the ones whose content changed, so `git log -p` on a benchmark shows exactly how
each compiler change rewrote its C. `check.sh` fails when one is out of date.

A change that makes the compiler optimise something on its own, or builds a planned optimisation, updates
[`docs/optimizations.md`](docs/optimizations.md) in the same commit (D185, D102): what it does, when it applies,
and anything a user could observe (allocation counts under `--debug-memory`, reflection, order of calls). Whether
it is built yet goes into [`design/status.md`](design/status.md). An optimisation with a user-visible cost says so
on the page; one that contradicts a rule in the docs goes into `mortaros_missing_decisions.md`.

**No optimisation is undocumented** (Mortaro, 2026-10-03). `docs/optimizations.md` is how users learn what the
compiler does for them, and it is the specification a port to another backend rebuilds from: an optimisation that
lives only in the generator is lost the day the backend changes. Give each one its section with an example program
that shows it, in the same commit that builds it, and an existing one you find without a section gets one.

**Whatever can be worked out while compiling is worked out while compiling** (Mortaro, 2026-10-03). Before building
anything that runs at run time (a table, a lookup, a check, a dispatch, a copy), ask whether the compiler can decide
it instead: fold it, specialise it per class or per call site, or prove it unneeded and leave it out. Run-time work is
the fallback for what only the run can know, and the page that documents it says why it could not be done earlier.

A change that adds a proof (a fact the compiler establishes while compiling to accept code, drop a run-time check,
choose cheaper code or refuse code) or changes what an existing proof covers updates
[`docs/proofs.md`](docs/proofs.md) in the same commit (D276): what it proves, the rule, what it buys, when it does
**not** apply and what the user writes then, and a program that shows it. The rule itself stays on the spec page for
that part of the language; `proofs.md` summarises and links.

## Decisions

The specification is the record of the language, and the decision log is the record of why. Every language decision
goes, when it is made, into the spec page that states its rule (`specs/<page>.md`), into the teaching of the docs
page of the same name where that changes, **and** into the decision log,
[`design/decisions.md`](design/decisions.md), as a new row: a decision that exists only in a conversation will be
re-litigated. The spec states the rule without its decision number. A rule belongs to exactly one page; a decision
that fits no page yet goes into [`design/open_questions.md`](design/open_questions.md) until its page exists. Mortaro decides
what cannot be undone cheaply; an agent decides anything that can be changed later without losing work (D509),
records it as "decided by Claude under D509", and lists it under "To confirm" in `mortaros_missing_decisions.md`.
A proposal not yet decided is marked "(proposed by Claude, unconfirmed)".

`mortaros_notes.md` is Mortaro's inbox, nothing else: read it, move what it contains into the specification and the
docs (the spec page, the teaching and a decision-log row), clear it. Agent-owned state belongs in an agent-owned file.

## Shared files, when more than one agent is running

`design/decisions.md`, `PLAN.md` and `mortaros_notes.md` are written by every session. The manual they replaced,
`PLAN.md` and the notes have been lost to a whole-file overwrite once already.

- Targeted edits only, matching on surrounding text, never on line numbers: these files move under you. The
  same goes for every page of `docs/`, which many sessions change at once.
- Re-read a file immediately before writing it. Do not reuse a copy read minutes ago.
- `design/decisions.md` is append-only. Never renumber or reorder rows; supersede with a new row instead.
- Never restore these three files, or a page of `docs/`, from a backup. Exclude them from any mirroring or
  restore you run.
- Check `ListAgents` before a large edit. If another session is live, agree who owns which files first.
- **Never `git add -A` or `git add .`** while another session may be working. Stage explicit paths only. This
  has already gone wrong once: a SPITE.md commit swept up another session's in-progress compiler changes and
  had to be split back apart.

## The project

- `SPITE.md`: what Mortaro hates, and what to do instead.
- `docs/`: the tutorial. Each page teaches one part of the language. Every titled code block in it is a program
  `check.sh` runs too (`scripts/docs_corpus` writes them out), so a page cannot drift from the compiler without
  failing. A program for something not built yet carries no title.
- `specs/`: the language, normative: one page per docs page with that part's rules in full.
- `design/`: decisions, status, open questions, proposals and known issues (above).
- `PLAN.md`: milestones and implementation status.
- `bootstrap/COMPILER_PLAN.md`: the compiler's own plan, design notes and progress log.
- `conformance/`, `examples/`, `tests/`, `diagnostics/`: what `bash check.sh` runs: programs with their exact
  expected output and balanced memory, the test package, and programs that must fail with exact errors.
