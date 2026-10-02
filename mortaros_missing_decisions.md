# Decisions waiting on Mortaro

Only questions Mortaro still has to answer. An agent adds one here with a link to where the docs argue it; when he
answers (inline or in `mortaros_notes.md`), the answer goes into the page that teaches it and a row of
`design/decisions.md`, and the item is deleted. Item numbers never change. Each item: the question, the options,
the recommendation, what it blocks. "D244" marks something that can go wrong silently today; "D205" one Claude
could decide itself. The principles that answer most questions before they are asked: the moron (anyone who uses
Spite) never chooses, the language does; performance choices belong to the compiler; whatever can be written in
Spite is written in Spite (D350); reflection exposes everything and tree-shakes what is not read (D353); one way to do each
thing; prevent mistakes rather than offer options; storage owns its items and other references are `T?` (D354).

Every other item is answered: the 14 principle answers by D369, and every confirmation of what agents decided by
D370, 76 by D371, 235 by D372, 161 by D373 and 105 by
D374 (2026-10-01). Earlier answers are listed in each row of `design/decisions.md`.

## Open

250. **Plain IO that is concurrent on its own** (D99, D176, D336; [concurrency.md](docs/concurrency.md#what-the-compiler-does-at-a-wait)).
D99 says waiting on IO is hidden async/await that the moron never chooses, but what is built only parks inside a
`Concurrent`: straight-line code outside one (the entry constructor and everything it calls) waits where it is,
letting other `Concurrent`s run meanwhile, and only two reads side by side overlap on their own. So a server written
as plain code (`next_request`, handle, `respond`, again) serves one request at a time, and `HttpServer` says so
([standard_library.md](docs/standard_library.md#http)): a slow client holds up every other. The docs and WHY.md now
say this plainly. Options: (a) **the library serves each client in a `Concurrent` of its own**: `HttpServer` and a
`Socket` listener take the named function that answers one request or one connection and start it as a
`Concurrent` per client, so the plain server is concurrent with nothing written; it costs the scheduler only in
programs that serve. (b) **the compiler overlaps loop passes that wait**: extend "reads in a row overlap" to a
`while` whose passes it can prove independent (an accept loop whose body reaches no state another pass writes),
starting each pass as a `Concurrent`; nothing new to learn, but the proof is narrow and order of output changes.
(c) **keep it explicit**: plain code waits in order, concurrency is always the caller's `Concurrent`, and D99 is read
as "a wait never stops the other `Concurrent`s". Recommendation: (a) for servers and listeners, with (c) as the
rule everywhere else, since it keeps one way to start concurrent work and needs no proof that can silently stop
applying. Blocks: rewriting concurrency.md to lead with plain IO, and any claim that a plain server is concurrent.
