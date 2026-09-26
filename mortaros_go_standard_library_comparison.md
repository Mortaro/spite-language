# Go's standard library against Spite's

Mortaro asked (2026-09-24): "give me a comprehensive list of Go land standard library versus ours, whats missing
in ours so i decide what we offer". Written by Claude Opus 5.5 from `master` at 7f5600f. Nothing here is decided:
every "fit" note and every suggestion is **(proposed by Claude, unconfirmed)**. The decisions it cites are in
the decision log, `docs/decisions.md`.

**What Spite has today**, for reference: `library/` holds `File`, `Directory`, `Process`, `Program`, `Console`,
`Environment`, `Socket`, `Concurrent`, `Parallel`, `Scheduler`, `List<T>`, `Dictionary<T>`, `String` (reopened in
`library/string.spite`), `TextBytes`, `NumberText`, `ForeignText`, `AllocationTable` and the REPL
(`library/read_evaluate_print_loop.spite`). `library/spite/` holds the reflection objects (`Spite.Class`,
`Spite.Function`, `Spite.Argument`, `Spite.Attribute`, `Spite.Namespace`). `library/windows|linux|mac/` reopen those
classes with the calls into each system's own library (D80). `Memory` and `DynamicLibrary` are the floor the
compiler emits (`docs/standard_library.md` "The floor, named", `docs/foreign_libraries.md`). Only Windows runs; Linux and macOS compile.

## Three things that cut across many packages

1. **Spite has no bit operators.** The operators table (`docs/functions_and_operators.md`) has `+ - * / %`, comparisons, `and`,
   `or`, `not` -- no and/or/xor/not on bits and no shifts. `library/number_text.spite` gets a `Double`'s bits by
   writing it to `Memory` and reading it back as a `Long`, and `library/windows/directory.spite` tests an attribute
   flag with `% 32 >= 16`. Every hash, checksum, cipher, compressor, UTF-8 decoder, binary encoding and ECS
   component mask needs them. This is a language decision before it is a library one, and it gates about a
   third of the "missing" group below.
2. **There is no byte buffer class.** `Memory` is addresses as `Long`s with `read_byte`/`read_int`/`read_long`/
   `read_double` and the matching writes (no `Short`, `Float` or unsigned widths, no fill). Go's `[]byte` is the
   currency of `io`, `bytes`, `encoding/*`, `crypto/*`, `compress/*` and `net`. Spite's equivalent would be a
   `Bytes` (or `ByteList`) class over `Memory`, the way D98 says `List` is.
3. **Sockets, time and randomness are reachable only inside the library.** `Socket` is TCP on `127.0.0.1` for the
   REPL; `Scheduler.clock()` is a millisecond tick count for fibers; there is no random source at all. The upcoming
   users need all three first.

## Have

Go's package is covered for the common cases, by a differently shaped Spite API.

| Go | Offers | Spite today | Missing | Fit |
|---|---|---|---|---|
| `slices` | generic slice helpers: sort, search, contains, index, insert, delete, reverse, compact | `List<T>` (`library/list.spite`): `append`/`prepend`/`insert`/`remove_at`/`remove_first`/`remove_last`, `contains`, `reverse`, `first`/`last`, `clear`, `join`, and the D15 templates `filter_`/`map_`/`find_by_`/`sort_by_`/`sum_`/`count_`/`any_`/`all_`/`each_` with fused chains (D105) | `index_of(value)`, binary search, `slice(start, end)` of a list, `sort()` of a list of scalars, remove-duplicates, `min_`/`max_<member>` | templates are Spite in `list.spite`, so each gap is one more Symbol codegen function; a program can add its own by reopening `List` today |
| `maps` | keys, values, clone, equality of maps | `Dictionary<T>` (`library/dictionary.spite`): `set`/`get`/`has`/`remove`/`count`/`keys`/`values`, `copy`/`deep_copy`, templates through its values | keys other than `String` | a key type needs a hash function per type, which D100's `from_type` and D83's number classes could supply |
| `strconv` | numbers to text and back, quoting, bases | `String.to_int()` and one `to_<type>()` per numeric type; `+` on a `String` formats any number, shortest round trip in `library/number_text.spite` | other bases (hex, binary), quoting/escaping a string for source, a parse that tells "not a number" from `0` | the name could never exist (abbreviation); a parse answering `Int?` beside today's default-`0` form fits D24 |
| `container/list` | doubly linked list | a class with `next`/`previous` fields -- every class is a reference (D1) | nothing a program cannot write in ten lines; cycles leak (section 10) until weak references exist | not worth a library class |
| `cmp` | `Compare`, `Less`, `Or` | `less_than`/`greater_than`/`equals` are the operator functions (section 5); `sort_by_<member>` uses them | a three-way compare | not needed as a package |

## Partial

Spite has some of it. The "missing" column is what the common Go program reaches for and Spite cannot do.

| Go | Offers | Spite today | Missing | Fit |
|---|---|---|---|---|
| `fmt` | formatted printing and `Sprintf`, `Sscanf`, `Stringer` | `Console().print/write/error`, text holes `"hello {name}"`, `+` formatting numbers, `Bool`, enums | width, padding, precision, hex output; a class's own text form (the `Printable`/`to_text()` proposal in `mortaros_missing_decisions.md` item 13) | formatting is ordinary Spite over `NumberText`; `to_text()` as the one hook, no format-string mini language |
| `strings` | search, split, trim, case, `Builder`, `Fields`, `Repeat`, `EqualFold`, `Cut`, `Count`, `LastIndex` | `String` (`library/string.spite`, `library/text_bytes.spite`): `length`, `slice`, `character_at`, `code_at`, `contains`, `starts_with`/`ends_with`, `index_of`, `replace`, `trim`, `upper_case`/`lower_case`, `split`, `lines` | a text builder (every `+` allocates a new `String`, so building text in a loop is quadratic -- a hidden cost, D36), `last_index_of`, `repeat`, `count_of`, trim of the start or end only, `remove_prefix`/`remove_suffix`, split on whitespace, case-insensitive comparison | pure Spite over `Memory`; a `TextBuilder` class or a compiler rewrite of `+=` in a loop into one buffer (a hidden optimisation, which D36 welcomes) |
| `os` | files, directories, environment, args, exit, stat, rename, temp files, working directory, hostname, pid | `File` (read/write/append/exists/remove), `Directory` (files/folders/exists/create), `Program().exit`, `Program().environment(name)`, `Arguments()`, `Environment` (D76, D85) | file size and modification time, `rename`, recursive create and remove, remove a directory, temp file/folder, working directory get/set, current executable path, permissions, process id | the D80 pattern: the shared behaviour in `library/file.spite`, one system call per function in `library/<system>/file.spite`; D93's `Directory` entries as a union of `Directory` and `File` is the natural home for a walk |
| `os/exec` | run a program without a shell, pipes, separate stdout/stderr, stdin, env, working dir, timeout | `Process(command, arguments)`: `run()` goes through `system`/`_popen`, `output()` is stdout and stderr merged | no-shell spawning (`CreateProcess`/`posix_spawn`), stdin, separate streams, environment and working directory per process, a process that keeps running while the program reads it | a wait on a child is IO, so it belongs to D99's hidden async; per-OS spawning in the OS folders |
| `io` | `Reader`/`Writer` interfaces, `Copy`, `ReadAll`, pipes | whole-file `File.read()`/`write()`, `Socket.read_line()` | streaming at all: reading a large file in pieces, copying one stream to another | a `type Reader { read_into(bytes: Bytes): Int }` shape (duck typed, section 7) instead of an interface; needs the `Bytes` class |
| `io/fs` | a file system interface, `WalkDir`, `Glob` | `Directory.files()`/`folders()` (names, sorted) | walking a tree, globbing, an in-memory or embedded file system | a walk is a template over D93's entries; an abstract file system is not needed until `embed` exists |
| `bufio` | buffered readers and writers, `Scanner` by lines or words | `Console().read_line()`, `Socket.read_line()` buffer internally | buffered writing (each `File.append` opens and closes the file), reading a file line by line without loading it | falls out of a streaming `File` plus `Bytes` |
| `time` | wall clock, monotonic clock, `Duration`, timers, tickers, formatting and parsing, time zones | `Program().sleep(milliseconds)` (a hidden wait under `Concurrent`), `Scheduler.clock()` (a millisecond tick, internal) | a public clock for measuring (frame time), the date and time now, a `Duration`, a timer and a ticker, formatting (RFC 3339, HTTP dates), time zones | `Clock` over `QueryPerformanceCounter`/`clock_gettime` per OS; a timer is a `Concurrent` that sleeps; time zones through the OS rather than a bundled database |
| `sort` | sort slices, stable sort, search | `sort_by_<member>()` returns a new sorted list | sort in place, sort a list of scalars, descending, by a function value (D17), binary search | a template; an in-place form matters for the engine (no allocation) |
| `sync` | `Mutex`, `RWMutex`, `WaitGroup`, `Once`, `Pool`, `Cond`, `Map` | join on drop (`Concurrent`/`Parallel` handles, D35); fibers need no locks, because Spite code only changes hands at a wait | anything for `Parallel`: a lock, an `Once` for a singleton's first use from two threads (known gap in section 15), a channel between threads | waits on the open question of what a `Parallel` may touch (`mortaros_missing_decisions.md` item 17); a channel fits D35 better than a mutex |
| `sync/atomic` | atomic integers and pointers | the compiler emits atomic retain/release under `SPITE_THREADS` | a public atomic counter | only if `Parallel` gets shared state |
| `net` | TCP/UDP/Unix sockets, DNS, listeners, IP types | `Socket` (`library/socket.spite`): `listen_locally`, `connect_locally`, `accept_client`, `read_line`, `write_line`, `close` -- TCP on `127.0.0.1` only; accept and receive are hidden waits (D99) | any other address, DNS lookup, raw bytes rather than lines, UDP, IPv6, a listener that serves many clients (IOCP/epoll/kqueue instead of a helper thread per blocked call) | per-OS in `library/<system>/socket.spite`; D99 says the caller never sees the wait |
| `runtime` | GC control, goroutine count, `GOOS`/`GOARCH`, `Caller`, `NumCPU` | `Program().live_allocations()`, `--debug-memory` (`AllocationTable`), `Environment().operating_system`, crash reports with the Spite stack (section 5), the REPL | processor count (needed for a `Parallel` pool), architecture, stack of the current call as a value | reflection and the REPL already answer most of it |
| `log` | leveled-free logging to stderr with timestamps, `Fatal` | `Console().error(...)`; `crash` is `log.Fatal` with a report written for an LLM (D24, D25) | timestamps, a log file | small once `time` exists |
| `testing` | see "not needed"; the benchmark half is partial | `tests/` is a package of `crash` checks (D46) with a leak check (D66) | benchmarks (a time per call) | a benchmark is a test that reads the clock; nothing new |

## Missing

Nothing today. "Fit" says how it would be built under Spite's rules.

| Go | Offers | Needs first | Fit |
|---|---|---|---|
| `bytes` | the `strings` API over `[]byte`, `Buffer` | -- | the `Bytes` class over `Memory` (D98, D101); its API mirrors `String`'s names |
| `unicode/utf8`, `unicode/utf16`, `unicode` | decode runes, validate, character classes | bit operators | `String` is bytes today (`character_at` is one byte); a `characters()` view and `code_point_at` in Spite; UTF-16 is what every `W` Windows call wants, and the Windows folder uses `A` calls for now |
| `path`, `path/filepath` | join, base name, extension, directory, clean, absolute, relative, match | -- | pure Spite on `String`, with the separator from the OS folder (D80); `File(path).name`, `.extension`, `.folder` as members fit better than free functions |
| `math` | `Sqrt`, `Pow`, trigonometry, `Floor`/`Ceil`/`Round`, `Abs`, `Min`/`Max`, `Inf`/`NaN`, constants | -- | members of the number classes D83 promises (`distance.square_root()`), reaching the C runtime's `libm` through the OS folder as `strtod` already is; the game engine needs this before anything else |
| `math/big` | arbitrary-precision integers and rationals | -- | `NumberText.exact_digits` already does limb arithmetic over `Memory`; a `BigInteger` would reuse it. Low demand |
| `math/rand`, `math/rand/v2` | seeded fast random numbers, shuffles | bit operators (for a good generator) | a `Random` class with a seed; a seed per instance, not a hidden global, so a game replay is deterministic |
| `math/bits` | leading/trailing zeros, popcount, rotate, add with carry | bit operators | functions on the integer classes; the C compiler turns them into single instructions |
| `container/heap` | a priority queue over any slice | -- | a `PriorityQueue<T>` over `Memory` ordered by a member (`PriorityQueue<Task>` ordered by `due`), a template like `sort_by_` |
| `container/ring` | circular list | -- | a `RingBuffer<T>` over `Memory` -- fixed size, no allocation, useful for the engine's frame history and the REPL's log |
| `context` | cancellation, deadlines, request-scoped values | -- | request values are D13's `ServerContext`; cancelling a `Concurrent` and a deadline on a wait are listed as not built in section 15. A timeout that returns `T?` fits D24 |
| `os/signal` | catch Ctrl+C / SIGTERM | -- | a server must shut down cleanly; a waitable `Program().interrupted()` is a hidden wait like any other |
| `encoding/json` | marshal structs with tags, streaming decoder | the reflection D95 describes | **decided (D22, D23, D95), not built.** `Json<T>` written with metaprogramming: attribute names are the keys, so no struct tags; writing is total (a compile error when a class cannot be written), parsing returns `T?`. The name pair (`parse_json`/`parse_json_or_crash` versus `to_crashing_json`) is still open |
| `encoding/csv` | read and write CSV | -- | the same reflection as JSON, rows into a `List<T>` of a class |
| `encoding/xml` | XML with struct tags | -- | low priority; same reflection shape as JSON if ever |
| `encoding/base64`, `encoding/hex` | binary as text | bit operators, `Bytes` | small pure Spite classes; the web needs base64 (tokens, data URLs) |
| `encoding/binary` | fixed-width integers, byte order, varints | bit operators, `Bytes`, `Memory` for every width | this is the D31 wire format's floor: the compile-time packing writes through it |
| `encoding/gob` | Go-to-Go serialisation | -- | see "not needed": D31's derived packing |
| `compress/gzip`, `compress/flate`, `compress/zlib` | deflate streams | bit operators, `Bytes`, streaming `io` | HTTP responses want gzip; through the OS (`zlib` on Linux/macOS, nothing built into Windows) or a pure Spite inflate/deflate |
| `archive/zip`, `archive/tar` | archives | compression | low priority |
| `crypto/sha256`, `crypto/sha1`, `crypto/md5`, `crypto/hmac` | hashes and message authentication | bit operators | pure Spite is a few hundred lines each, or the OS (`bcrypt.dll`, `libcrypto`) through `DynamicLibrary`; sessions and cookies need HMAC |
| `crypto/rand` | secure random bytes | -- | the OS (`BCryptGenRandom`, `getrandom`, `arc4random_buf`) in each OS folder; session ids need it |
| `crypto/aes`, `crypto/cipher` | symmetric encryption | bit operators | through the OS library, not pure Spite: constant-time crypto is not a place for clever code |
| `crypto/tls`, `crypto/x509` | TLS clients and servers, certificates | sockets beyond localhost | through the OS (SChannel on Windows, OpenSSL on Linux, Security framework on macOS) with D80's reopening; a pure Spite TLS is a project of its own. Nullstack will sit behind a proxy in development, so the client side matters first |
| `hash/crc32`, `hash/fnv`, `hash/maphash` | checksums and fast hashes | bit operators | `Dictionary` hashes with `(hash * 31 + code) % 1000000007` today; a proper hash speeds it up and serves ECS lookups |
| `net/http` | HTTP client and server, routing, cookies, headers, HTTP/2 | general sockets, `time`, `url` | the Nullstack framework's base. Each request handled on a `Concurrent`, every read and write a hidden wait (D99); routes derived from D13's first parameter, not registered by hand |
| `net/url` | parse, escape, query strings | -- | pure Spite on `String`; a query string read into a `type` through reflection, like JSON |
| `net/mail`, `net/smtp` | address parsing, sending mail | sockets, TLS | low priority |
| `net/netip` | IP address values | -- | a small value `type` |
| `html/template` | HTML with contextual escaping | -- | **designed (D18, D19), not built**: markup is `missing_function` plus literals, `Html` is the browser as a library. What is missing on the server side is escaping text into HTML, which the markup builder would do for every text child |
| `html` | escape and unescape entities | -- | the escaping above |
| `regexp` | RE2 regular expressions | -- | a `Pattern` class compiled at compile time when the pattern is a literal (Symbol codegen per pattern, like `load`'s literal rule), linear-time matching; or skipped, since an AI can write the parsing function instead |
| `log/slog` | structured, levelled logging | `time` | records as a `type`, written by reflection like JSON; a REPL-readable log may matter more than a text one (D96) |
| `embed` | files compiled into the binary | -- | compile-time evaluation: `File("logo.png").read()` inside a class-level function answered at build time, so no new directive |
| `image`, `image/png`, `image/color` | decode and encode images | compression, `Bytes` | the engine will want PNG loading; through the OS or `stb_image` via `DynamicLibrary` first |
| `mime`, `mime/multipart` | content types by extension, multipart bodies | -- | a `Dictionary` of extensions for the web server; multipart for uploads |
| `database/sql` | a driver interface, connection pool, rows, transactions | sockets, TLS for remote databases | the known need. SQLite through `DynamicLibrary` is the smallest first step; Postgres speaks a documented wire protocol over a socket, so it can be pure Spite with every wait hidden (D99). No driver interface to implement -- one database class per engine, rows filled into a `List<T>` by reflection, queries by Symbol codegen (`database.filter_age_greater_than(10)` is already the example in section 17) |
| `runtime/pprof`, `runtime/trace` | profiling | -- | the REPL could report time per function; later |
| `os/user` | current user, home directory | -- | one function per OS folder |
| `text/tabwriter` | aligned columns | `fmt` padding | small |

## Deliberately not needed in Spite

The language already covers these. Adding a library for them would be a second way to do one thing.

| Go | Why Spite does not need it |
|---|---|
| `reflect` | reflection is the language: `Spite.Class`, `.attributes`, `.functions`, Symbol codegen, `missing_function`, all at compile time and tree-shaken (D12, D42, D92) |
| struct tags (`json:"name"`) | D23: the standard library is written with the metaprogramming, so the attribute's own name is the key; a rename is a member, not a string annotation |
| `unsafe` | `Memory` is the one real basic type (D98, D101), with visible addresses |
| `syscall`, `golang.org/x/sys` | `DynamicLibrary` reaches any system call by name (D4), and each OS folder reopens the classes it changes (D80) |
| `plugin` | `load()` a folder, or `DynamicLibrary` for a native library |
| `errors` | D24: a compile error, `assert` or `crash`; `T?` carries no reason, and an actionable distinction is data |
| `testing` | D46: a test is a package of `crash` checks; D66 adds the leak check; `check.sh` runs it |
| `flag` | `Environment` (D76, D85): a reopened singleton whose fields are the settings, read from the command line or the process environment |
| `text/template` | text holes `"hello {name}"` and Symbol codegen; a template language would be a second grammar (the JSX spite) |
| `encoding/gob`, `net/rpc` | D13 and D31: isomorphic classes call across the network with a compile-time binary packing, no self-description |
| `iter` | the D15 member templates and `while` |
| `go/ast`, `go/parser`, `go/format` | the compiler is Spite (`bootstrap/source`), it is the formatter (section 12), and reflection answers questions about code; whether the parser is loadable by a program is its own question |
| `expvar`, `net/http/pprof`, `debug/*` | the remote REPL (`--repl-port`) inspects a running program |
| package management (`go mod`) | D38: a git URL with a commit hash in `load` |

## Suggestions for what to add first

For Mortaro to decide; all proposed by Claude, unconfirmed. Ordered by what the game engine (ECS, data structures
on `Memory`) and the Nullstack-style web framework (HTTP, database calls, hidden async) need soonest, and by what
unblocks the most packages.

1. **Bit operators** (a language decision). They gate hashing, crypto, compression, UTF-8, binary encoding,
   `math/bits` and ECS component masks. Following section 5 they would be operator functions with written names;
   whether they get symbols at all, or are only functions such as `flags.bits_and(mask)` and `value.shift_left(3)`,
   is the question. **Answered by D117**: functions only -- `shifted_left`, `shifted_right`, `bits_and`, `bits_or`,
   `bits_exclusive_or`, `bits_inverted`, `set_bit_count`, `leading_zero_count`, `trailing_zero_count` on every
   whole-number class (`docs/values_and_types.md`); the directory flag test now uses `bits_and`.
2. **`Memory` completed and a `Bytes` class over it**: reads and writes for every numeric width, `fill`, and a
   growable byte buffer with the `String`-like API. The engine's component storage and every encoding sit on it.
3. **`math`** as members of the number classes (D83): square root, power, trigonometry, floor/ceiling/round,
   absolute, min/max, infinity and not-a-number tests. The engine cannot move anything without them.
4. **`Clock` and time**: a monotonic clock for frame timing and benchmarks, the date and time now, a `Duration`,
   RFC 3339 and HTTP date formatting. `Scheduler.clock()` is already half of it.
5. **`Random`**: a seeded fast generator per instance for the engine, and secure random bytes from the OS for the
   web's session ids.
6. **Engine data structures on `Memory`**: in-place `sort`, `PriorityQueue<T>`, `RingBuffer<T>`, and a sparse set
   or slot map for entities -- the D98 promise that "anyone can create efficient data structures", proved by the
   standard library first.
7. **`Socket` beyond localhost, served by the OS's completion mechanism** (IOCP, epoll, kqueue) rather than a
   helper thread per blocked call, plus DNS. Everything network-shaped waits on this.
8. **HTTP server and client**, with `Url` parsing and a text builder, every wait hidden (D99). This is where
   Nullstack starts.
9. **A database**: SQLite through `DynamicLibrary` first (no network, no TLS), then Postgres in pure Spite over the
   socket, rows filled by reflection.
10. **`Json<T>`** (D95, already decided) for the foreign systems the web framework talks to, and HTML escaping for
    the markup (D18).

After those: `path` members on `File`, file metadata and `rename`, streaming `File` reads and buffered writes,
SHA-256 and HMAC, TLS through the OS, `os/signal` for clean shutdown, UTF-8 characters on `String`, base64, gzip,
logging, `embed` as compile-time evaluation, and a regular expression class if an AI writing the parsing function
turns out not to be enough.
