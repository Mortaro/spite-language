# Time

The specification of [Time](../docs/time.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Time: one stored instant, zones for presentation

The aim is the best date and time support there is, so that Spite never repeats JavaScript's
`Date` and the wrappers it bred, with **one stored format, an exact instant, and time zones only a
presentation layer**, copied from the best modern design; [Why this design](../docs/time.md#why-this-design) compares the
candidates and argues the choice. The names are `Instant`, `Duration` and `Period`, and for a
zone-less reading `Date`, `Time` and `DateTime`, with no `Local` prefix (the old spellings are errors that
name the new ones: `'LocalDate' is spelled 'Date'`). Everything else below is NodaTime's model
with Temporal's vocabulary and no stored zoned type.

| Class | What it is |
|---|---|
| `Instant(since_1970: Duration)` | a point on the universal time line; `+ Duration`, `- Instant` (a `Duration`), `==`, `<`, `>`; prints in UTC with `Z` |
| `Duration(amount, unit)` | exact time, whole seconds and nanoseconds; units `'nanoseconds'` to `'hours'` and never days; `total(unit)`, `part(unit)`, `is_negative`, `is_zero`, `+`, `-`, `* Long`, unary `-`, `==`, `<`, `>` |
| `Period(amount, unit)` | calendar time, years, months and days (`'weeks'` is seven days); `years`, `months`, `days`, `is_zero`, `+`, unary `-`, `==`, and no `<` |
| `Date(year, month, day)` | a proleptic Gregorian date with no zone; `year`, `month`, `day`, `+ Period`, `weekday`, `day_of_year`, `days_in_month`, `days_in_year`, `is_leap_year`, `days_since_1970`, `days_until`, `period_until`, `==`, `<`, `>` |
| `Time(hour, minute, second, nanosecond)` | a clock reading with no date and no zone; `hour`, `minute`, `second`, `nanosecond`, `second_of_day`, `==`, `<`, `>` |
| `DateTime(date, time)` | both, with no zone; `date`, `time`, `+ Period`, `==`, `<`, `>` |
| `TimeZone` | `to_local(instant)`, `to_instant(local, ambiguity)`, `to_text(instant)`, `offset_at(instant)`, `name` |
| `TimeZones()` | the database, a singleton: `find(name): TimeZone?`, `utc()`, `fixed_offset(duration)`, `system()`, `read_tzif(name, data): TimeZone?` |
| `TimeText()` | ISO 8601 (RFC 3339, RFC 9557), a singleton: `read_instant`, `read_date_time`, `read_date`, `read_time`, `read_duration`, `read_period`, each answering `T?`; writing is each type's `to_string()` |
| `Benchmark(work: Spite.Function<T>)` | one timed run of `work`: `answer: T`, what it answered, and `duration: Duration`, the monotonic time it took |

- **A local reading never becomes an instant without a zone.** No local type has a function answering an
  `Instant`, `==` between the two kinds is a type error (`an Instant cannot be used where a DateTime is
  needed`), and `zone.to_instant(local, ambiguity)` is the only
  bridge. `TimeText.read_instant` refuses text without an offset, and `read_date_time` refuses text with one.
- **Exact and calendar lengths never mix.** A `Duration` has no days, since a day is 23 to 25 hours where clocks
  change; a `Period` has no hours and adds only to the local types, months first (a day past the new month's end
  becomes its last day: January 31 plus a month is February 29 in 2024), then days. `period_until` is
  `java.time`'s `Period.between`. `Duration(1, 'days')` is `'days' is not a value of this enum`, and comparing
  two periods with `<` is `this operator on a 'Period' needs it to define 'less_than(other)'`.
- **Every gap and overlap is resolved by a rule the call names**: `'compatible'` (a gap resolves after it, an
  overlap to its first instant: RFC 5545's rule, and `java.time`'s and `Temporal`'s default), `'earlier'` or
  `'later'`. The zone finds the offsets a day either side of the reading and keeps the ones that map back to it:
  both for an overlap, neither for a gap.
- **A value that cannot exist halts; text that cannot exist is `null`.** `Date(2023, 2, 29)` and
  `Time(24, 0, 0, 0)` crash, since a program that builds one from its own numbers has a bug; reading
  text answers `T?`. A leap second in text reads as the second before it.
- **The database is the operating system's, and nothing is embedded.** Windows reads IANA zones through
  `icu.dll` (Windows 10 1903 and later), loaded only when a named zone is asked for
  (`library/windows/zone_calendar.spite`); Linux and macOS read the TZif files under `/usr/share/zoneinfo` in Spite
  (`library/tzif_reader.spite`: RFC 8536 versions 1 to 4 and the POSIX rule in the footer), and `system()` follows
  `TZ`, then `/etc/localtime`. `read_tzif` is the same reader for a file a program brings. `conformance/stage6/zone_files` runs the
  TZif reader on Windows over three files written for the test and checked with Python's `zoneinfo`.
- **Clock** keeps `elapsed_nanoseconds()` (the monotonic clock: a `Long` of nanoseconds, no allocation per reading) and `elapsed_milliseconds()` for measuring, and `now()` answers an
  `Instant`.
- **`Benchmark(work)`** (`library/benchmark.spite`, `generic $answer_type`) is how a piece of work is timed. Its
  constructor reads `Clock.elapsed_nanoseconds()`, calls `work` exactly once with no arguments, reads the clock
  again, and keeps the result in `answer` and the difference in `duration`, a `Duration` in nanoseconds; nothing
  else runs between the readings but the call and the store of its answer. `$answer_type` is read from the
  argument (`Benchmark(sum_to_a_million)` is a `Benchmark<Long>`), and a function returning nothing makes a
  `Benchmark<Nothing>`. Keeping the answer is what keeps the work: it is reachable from the benchmark, so it is never
  removed as unused. There is no `clock.benchmark(...)`: only a class takes codegen values, so the answer's type
  can be carried only by a class made from the work ([metaprogramming.md](metaprogramming.md#codegen-values-)). It costs the
  function value made for `work` and the result's two objects (the `Benchmark` and its `Duration`), all made
  outside the measured time, and one call through the value inside it; a program that names no `Benchmark`
  carries none of it. `duration.total('microseconds')` rounds toward zero, so it is the difference of the two
  readings divided by 1000. In a program that starts a `Parallel`, `work` counts as code that may run on another
  thread, since a function value is matched to the calls a thread makes by its count of arguments and a worker
  calls values of none ([optimizations.md](../docs/optimizations.md#plain-reference-counts-where-no-thread-reaches-a-class)):
  the classes `work` counts are counted atomically and the singletons it calls keep their locks, so work whose point
  is the cost of those is timed with two `elapsed_nanoseconds()` readings instead.

The conformance programs are `conformance/stage6/instant_arithmetic`, `calendar_math`, `daylight_saving`,
`zone_files`, `fixed_offsets`, `time_text_round_trips` and `time_text_errors`.

---

Next: [Game maths](game_maths.md).
