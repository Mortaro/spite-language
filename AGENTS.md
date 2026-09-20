# Working on Spite

Read [`SPITE.md`](SPITE.md) first. It lists what Mortaro hates and what to do instead, and it is the point of the
project. Then [`manual.md`](manual.md), which is normative — when anything else disagrees with it, it wins.

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
```
## Decisions

`manual.md` is the record. Every language decision goes into the prose **and** the decision log when it is made —
a decision that exists only in a conversation will be re-litigated. Mark your own proposals "(proposed by Claude,
unconfirmed)" and never record one as decided. Mortaro decides language semantics; an agent proposes.

`mortaros_notes.md` is Mortaro's inbox, nothing else: read it, move what it contains into the manual, clear it.
Agent-owned state belongs in an agent-owned file.

## Shared files, when more than one agent is running

`manual.md`, `PLAN.md` and `mortaros_notes.md` have been lost to a whole-file overwrite once already.

- Targeted edits only, matching on surrounding text, never on line numbers — both files move under you.
- Re-read a file immediately before writing it. Do not reuse a copy read minutes ago.
- The decision log is append-only. Never renumber or reorder rows; supersede with a new row instead.
- Never restore these three files from a backup. Exclude them from any mirroring or restore you run.
- Check `ListAgents` before a large edit. If another session is live, agree who owns which files first.
- **Never `git add -A` or `git add .`** while another session may be working. Stage explicit paths only. This
  has already gone wrong once: a SPITE.md commit swept up another session's in-progress compiler changes and
  had to be split back apart.

## The project

- `SPITE.md` — what Mortaro hates, and what to do instead.
- `manual.md` — normative language reference, with the decision log at the end.
- `PLAN.md` — milestones and implementation status.
- `bootstrap/COMPILER_PLAN.md` — the compiler's own plan, design notes and progress log.
- `conformance/`, `examples/`, `tests/`, `diagnostics/` — what `bash check.sh` runs: programs with their exact
  expected output and balanced memory, the test package, and programs that must fail with exact errors.
- `docs/` — the language documentation. Every titled code block in it is a program `check.sh` runs too
  (`scripts/docs_corpus.spite` writes them out), so a page cannot drift from the compiler without failing.
