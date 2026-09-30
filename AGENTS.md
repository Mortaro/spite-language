# Working on Spite

**Read [`mortaros_notes.md`](mortaros_notes.md) first, every time.** It is Mortaro's inbox. For each note: record the
decision in the docs -- the rule in the page that teaches that part of the language, and a row in
[`docs/decisions.md`](docs/decisions.md) -- or act on it, then delete the note, so the inbox only ever holds
what nobody has handled yet. Questions only Mortaro can answer go to
[`mortaros_missing_decisions.md`](mortaros_missing_decisions.md).

**Ask of everything you build: can it be tree-shaken, and does it keep Spite at zero runtime?** A program that does
not use a feature must carry none of it, and nothing may need a shipped scheduler, interpreter or registry --
compile-time work instead (Mortaro, 2026-09-25; D147, D176). Debug and REPL features may cost something only in
those builds (D143). Say how a proposal tree-shakes and what it costs at run time.

**Anything that can go wrong silently is a bug** (Mortaro, 2026-09-27; D244). Every failure is loud: a compile
error, or a crash that names its cause -- never a wrong value, a lost write, a skipped step, a leak or a hang.
Judge everything you build, and every open item, against it: a compiler rule that lets one of those through, a
library function that answers a default a caller cannot tell from a real answer, a report that prints a value where
something is missing -- each is a bug to fix, not a style to document. When you find one you cannot fix now, write
it into the "still open" list of [docs/failure.md](docs/failure.md#nothing-fails-silently--the-rule).

Read [`SPITE.md`](SPITE.md) first. It lists what Mortaro hates and what to do instead, and it is the point of the
project. Then the docs, in the [reading order](docs/README.md#reading-order). **The docs are normative** (D193):
each page teaches its part of the language and ends with its rules in full, and when anything else disagrees with
the docs, the docs win.

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
`commit-msg` hook rejects a message without it. Nothing identifies the *session* -- session names are local to
Mortaro's machine and do not belong in a public history. Hooks are versioned in `scripts/hooks`, so each clone
runs once:

```
git config core.hooksPath scripts/hooks
```

## Changing a Spite program: a worktree is a folder that loads it

To propose a change to a Spite program (SlopEngine, an example, anything written in Spite), do not edit it in
place: make a small folder beside it whose entry file `load`s the original and reopens only the classes you change
(D156). Mortaro runs that folder to test the change, and only an approved change is merged into the real code.
Nothing is copied, so a worktree costs only the files it changes.

## Changing the compiler: every optimisation and every proof is documented in the same commit

A change that makes the compiler optimise something on its own -- or builds a planned optimisation -- updates
[`docs/optimizations.md`](docs/optimizations.md) in the same commit (D185, D102): what it does, when it applies,
built or planned, and anything a user could observe (allocation counts under `--debug-memory`, reflection, order of
calls). An optimisation with a user-visible cost says so there; one that contradicts a rule in the docs goes into
`mortaros_missing_decisions.md`.

A change that adds a proof -- a fact the compiler establishes while compiling to accept code, drop a run-time check,
choose cheaper code or refuse code -- or changes what an existing proof covers updates
[`docs/proofs.md`](docs/proofs.md) in the same commit (D276): what it proves, the rule, what it buys, when it does
**not** apply and what the user writes then, its status, and a program that shows it. The rule itself stays on the
page that teaches it; `proofs.md` summarises and links.

## Decisions

The docs are the record. Every language decision goes, when it is made, into the rules of the docs page that
teaches that part of the language (its "Rules in full" section, and its teaching too where that changes) **and**
into the decision log, [`docs/decisions.md`](docs/decisions.md), as a new row -- a decision that exists only in a
conversation will be re-litigated. A rule belongs to exactly one page; a decision that fits no page yet goes into
[`docs/open_questions.md`](docs/open_questions.md) until its page exists. Mark your own proposals "(proposed by
Claude, unconfirmed)" and never record one as decided. Mortaro decides language semantics; an agent proposes.

`mortaros_notes.md` is Mortaro's inbox, nothing else: read it, move what it contains into the docs (the page and a
decision-log row), clear it. Agent-owned state belongs in an agent-owned file.

## Shared files, when more than one agent is running

`docs/decisions.md`, `PLAN.md` and `mortaros_notes.md` are written by every session. The manual they replaced,
`PLAN.md` and the notes have been lost to a whole-file overwrite once already.

- Targeted edits only, matching on surrounding text, never on line numbers — these files move under you. The
  same goes for every page of `docs/`, which many sessions change at once.
- Re-read a file immediately before writing it. Do not reuse a copy read minutes ago.
- `docs/decisions.md` is append-only. Never renumber or reorder rows; supersede with a new row instead.
- Never restore these three files, or a page of `docs/`, from a backup. Exclude them from any mirroring or
  restore you run.
- Check `ListAgents` before a large edit. If another session is live, agree who owns which files first.
- **Never `git add -A` or `git add .`** while another session may be working. Stage explicit paths only. This
  has already gone wrong once: a SPITE.md commit swept up another session's in-progress compiler changes and
  had to be split back apart.

## The project

- `SPITE.md` — what Mortaro hates, and what to do instead.
- `docs/` — the language, normative: each page teaches one part and ends with its rules in full. Every titled
  code block in it is a program `check.sh` runs too (`scripts/docs_corpus` writes them out), so a page cannot
  drift from the compiler without failing. `docs/decisions.md` is the append-only decision log.
- `PLAN.md` — milestones and implementation status.
- `bootstrap/COMPILER_PLAN.md` — the compiler's own plan, design notes and progress log.
- `conformance/`, `examples/`, `tests/`, `diagnostics/` — what `bash check.sh` runs: programs with their exact
  expected output and balanced memory, the test package, and programs that must fail with exact errors.
