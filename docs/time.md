# Time

Spite stores time in one form: an `Instant`, an exact point on the universal time line. A time zone never
changes what is stored. It is a presentation layer that turns an `Instant` into the date and clock time someone
in that zone would read, and turns such a reading back into an `Instant`. The calendar has its own types
(`Date`, `Time`, `DateTime`) that cannot be mistaken for an instant, and the two kinds of length
are separate: a `Duration` is an exact amount of time, and a `Period` is an amount of calendar (years, months,
days), whose real length depends on where it is applied.

The model is D127's and the type names are Mortaro's (D158, D160); the rest of the shape -- the members, the
ambiguity rules, where the zones come from -- is proposed by Claude and waits on Mortaro
([the rules in full](#time-one-stored-instant-zones-for-presentation--implemented-on-windows-the-shape-proposed-by-claude-unconfirmed)).

All of it is library code, written in Spite, with nothing running behind it: a program that never names a time
class carries none of it, and one that only measures with `Clock()` carries only that ([D177](decisions.md)).
Each value is an ordinary small object, and arithmetic makes a new one for its result, which lives wherever the
compiler places it ([memory.md](memory.md#where-a-value-lives-memory)).

| Class | What it is | Stored as |
|---|---|---|
| `Instant` | a point on the universal time line | a `Duration` since 1970-01-01T00:00:00Z |
| `Duration` | an exact length of time, to the nanosecond | whole seconds and nanoseconds |
| `Date` | a calendar date with no zone: `2024-03-10` | year, month, day |
| `Time` | a clock reading with no date and no zone: `02:30:00` | hour, minute, second, nanosecond |
| `DateTime` | a date and a clock reading, with no zone | a `Date` and a `Time` |
| `Period` | an amount of calendar: `P1Y2M3D` | years, months, days |
| `TimeZone` | the rules that map instants to local readings | a name and its rules |
| `TimeZones()` | the time zone database, a singleton | the operating system's |
| `TimeText()` | ISO 8601 text, read and written, a singleton | |
| `Clock()` | `now()` for the wall clock, and elapsed time for measuring | |

What cannot happen, by construction:

- **A local reading is never an instant.** `DateTime` has no function that answers an `Instant`, and
  comparing one with an `Instant` is a type error. The only way across is `zone.to_instant(local, ...)`, which
  names the zone.
- **A day is never 24 hours by accident.** `Duration` has no days; `Period(1, 'days')` is one calendar day and is
  added to dates, never to instants. Adding "one day" to an instant is written through a zone, where it means
  what the zone says it means.
- **A daylight-saving gap or overlap is never resolved silently by a default you did not choose.** Every
  `to_instant` names the rule for the ambiguous case.

The first of them is the type checker's to enforce, so the mistake never runs:

```gdscript title=time_reading_is_not_instant/time_reading_is_not_instant.spite entry error
var console = Console()

func TimeReadingIsNotInstant() {
    var meeting_day = Date(2024, 3, 10)
    var meeting_time = Time(9, 30, 0, 0)
    var meeting = DateTime(meeting_day, meeting_time)
    var launch_since_1970 = Duration(1710054000, 'seconds')
    var launch = Instant(launch_since_1970)
    console.print(meeting == launch)
}
```
```diagnostic
an Instant cannot be used where a DateTime is needed
```

## Instants and durations

`Instant(since_1970)` is the instant a `Duration` after 1970-01-01T00:00:00Z (before it, for a negative one).
`clock.now()` is the current one. `+` adds a `Duration`, `-` between two instants answers the `Duration` between
them, and to go back you add a negative `Duration`: `moment + -hour`. An `Instant` prints in UTC, with a `Z`.

`Duration(amount, unit)` takes `'nanoseconds'`, `'microseconds'`, `'milliseconds'`, `'seconds'`, `'minutes'` or
`'hours'`. `total(unit)` is the whole length in one unit, and `part(unit)` is one component of it written as
hours, minutes, seconds and a fraction; both round toward zero and keep the sign.

```gdscript title=time_instants/time_instants.spite entry
var console = Console()

func TimeInstants() {
    var launch_since_1970 = Duration(1710054000, 'seconds')
    var launch = Instant(launch_since_1970)
    var hour = Duration(1, 'hours')
    var later = launch + hour
    var earlier = launch + -hour
    var between = later - earlier
    console.print(launch, later, earlier, between)
    var lap = Duration(90500, 'milliseconds')
    var minutes = lap.total('minutes')
    var seconds = lap.part('seconds')
    var half = Duration(-500, 'milliseconds')
    console.print(lap, minutes, seconds, half, lap * 2)
    var in_order = earlier < launch and launch < later
    var two_hours = Duration(120, 'minutes')
    console.print(in_order, between == two_hours)
}
```
```output
2024-03-10T07:00:00Z 2024-03-10T08:00:00Z 2024-03-10T06:00:00Z PT2H
PT1M30.5S 1 30 -PT0.5S PT3M1S
true true
```

`Clock()` keeps its monotonic readings, `elapsed_nanoseconds()` and `elapsed_milliseconds()`, for measuring a
frame or a request: the wall clock can jump when the machine's time is corrected, and a monotonic clock cannot.
`now()` is the wall clock, and each system reads it its own way (`GetSystemTimeAsFileTime` on Windows,
`clock_gettime` on Linux and macOS).

## Dates and periods

`Date(year, month, day)` is a date in the proleptic Gregorian calendar, the one ISO 8601 uses, for any year
(year 0 is 1 BC). A date that does not exist -- `Date(2023, 2, 29)` -- halts the program, since a program that
builds one from its own numbers has a bug; text from outside is read with `TimeText` instead, which answers
`null`. `Time(hour, minute, second, nanosecond)` is the same for a clock reading, and
`DateTime(date, time)` joins the two.

A `Period` adds to a date by months first and then by days, and a day of the month that the new month does not
have becomes its last day: January 31 plus one month is February 29 in a leap year. `period_until` answers the
years, months and days between two dates the same way, so adding it back lands on the later date.

```gdscript title=time_calendar/time_calendar.spite entry
var console = Console()

func TimeCalendar() {
    var end_of_january = Date(2024, 1, 31)
    var month = Period(1, 'months')
    var leap = end_of_january + month
    var common = Date(2023, 1, 31) + month
    var fortnight = leap + Period(2, 'weeks')
    console.print(leap, common, fortnight, leap.weekday, leap.day_of_year)
    var born = Date(1990, 5, 15)
    var today = Date(2024, 3, 10)
    var age = born.period_until(today)
    var days = born.days_until(today)
    console.print(age, days, today.is_leap_year, today.days_in_month)
    var half_past_nine = Time(9, 30, 0, 0)
    var meeting = DateTime(today, half_past_nine)
    var next_meeting = meeting + Period(1, 'months')
    console.print(meeting, next_meeting)
}
```
```output
2024-02-29 2023-02-28 2024-03-14 thursday 60
P33Y9M24D 12353 true 31
2024-03-10T09:30:00 2024-04-10T09:30:00
```

A `Period` is not compared with `<`: whether one month is longer than 30 days depends on the month. Two periods
are equal when their years, months and days are.

## Time zones

`TimeZones()` is the database. `find(name)` answers the zone with an IANA name such as `"America/New_York"`,
or `null` when the database has none by that name; `utc()` is UTC; `fixed_offset(offset)` is a zone that is
always the same distance from UTC; `system()` is the zone the machine is set to, for showing times to the person
at it -- and nothing uses it unless it is asked for by name, so a program never depends on the machine's zone by
accident.

What a zone answers:

| Member | Answers |
|---|---|
| `to_local(instant)` | the `DateTime` a clock in the zone reads at that instant |
| `to_instant(local, ambiguity)` | the `Instant` at which the zone's clocks read `local` |
| `to_text(instant)` | the local reading with its offset: `2024-03-10T03:00:00-04:00` |
| `offset_at(instant)` | the zone's distance from UTC at that instant, as a `Duration` |
| `name` | the name it was found by |

A local reading is not always one instant. When clocks spring forward, a reading in the gap never happens; when
they fall back, a reading in the overlap happens twice. `to_instant` takes what to do about it, and there is no
default:

| Ambiguity | In an overlap | In a gap |
|---|---|---|
| `'earlier'` | the first of the two instants | the reading taken with the offset after the gap, which lands before it |
| `'later'` | the second of the two instants | the reading taken with the offset before the gap, which lands after it |
| `'compatible'` | the first, like `'earlier'` | the one after, like `'later'` |

`'compatible'` is what RFC 5545 calendars, `java.time` and JavaScript do by default: a meeting at 02:30 on the
night the clocks skip 02:00-03:00 happens at 03:30, and one at 01:30 on the night 01:00-02:00 happens twice is the
first 01:30. It is still written at every call.

```gdscript title=time_zones_named/time_zones_named.spite entry
var console = Console()
var zones = TimeZones()

func TimeZonesNamed() {
    var new_york = zones.find("America/New_York")
    crash new_york
    var launch_since_1970 = Duration(1710054000, 'seconds')
    var launch = Instant(launch_since_1970)
    var shown = new_york.to_text(launch)
    var offset = new_york.offset_at(launch)
    console.print(launch, shown, offset)
    var skipped_day = Date(2024, 3, 10)
    var skipped_time = Time(2, 30, 0, 0)
    var skipped = DateTime(skipped_day, skipped_time)
    var repeated_day = Date(2024, 11, 3)
    var repeated_time = Time(1, 30, 0, 0)
    var repeated = DateTime(repeated_day, repeated_time)
    var after_gap = new_york.to_instant(skipped, 'compatible')
    var first = new_york.to_instant(repeated, 'earlier')
    var second = new_york.to_instant(repeated, 'later')
    var after_gap_text = new_york.to_text(after_gap)
    var first_text = new_york.to_text(first)
    var second_text = new_york.to_text(second)
    console.print(after_gap_text, first_text, second_text)
    var nowhere = zones.find("Atlantis/Capital")
    if nowhere {
        console.print("found", nowhere.name)
    } else {
        console.print("no such zone")
    }
}
```
```output
2024-03-10T07:00:00Z 2024-03-10T03:00:00-04:00 -PT4H
2024-03-10T03:30:00-04:00 2024-11-03T01:30:00-04:00 2024-11-03T01:30:00-05:00
no such zone
```

"The same time tomorrow" is a question for a zone, because tomorrow may be 23 or 25 hours away: read the local
date and time, add a `Period`, and turn it back into an instant.

```gdscript title=time_tomorrow/time_tomorrow.spite entry
var console = Console()
var zones = TimeZones()

func TimeTomorrow() {
    var new_york = zones.find("America/New_York")
    crash new_york
    var saturday_since_1970 = Duration(1709996400, 'seconds')
    var saturday = Instant(saturday_since_1970)
    var local = new_york.to_local(saturday)
    var tomorrow = local + Period(1, 'days')
    var sunday = new_york.to_instant(tomorrow, 'compatible')
    var elapsed = sunday - saturday
    var sunday_text = new_york.to_text(sunday)
    console.print(local, sunday_text, elapsed)
}
```
```output
2024-03-09T10:00:00 2024-03-10T10:00:00-04:00 PT23H
```

### Fixed offsets and UTC

A fixed offset has no rules at all: `zones.fixed_offset(india_offset)`, with `india_offset` a
`Duration(330, 'minutes')`, is named `+05:30` and is five
and a half hours ahead of UTC at every instant. It is the right zone for text that carries its own offset and
nothing more, and the wrong one for a place, whose offset changes.

```gdscript title=time_offsets/time_offsets.spite entry
var console = Console()
var zones = TimeZones()

func TimeOffsets() {
    var launch_since_1970 = Duration(1710054000, 'seconds')
    var launch = Instant(launch_since_1970)
    var india_offset = Duration(330, 'minutes')
    var india = zones.fixed_offset(india_offset)
    var universal = zones.utc()
    var india_text = india.to_text(launch)
    var universal_text = universal.to_text(launch)
    console.print(india.name, india_text, universal.name, universal_text)
}
```
```output
+05:30 2024-03-10T12:30:00+05:30 UTC 2024-03-10T07:00:00+00:00
```

### Where zones come from

The rules are the operating system's, so a change in a country's daylight-saving law reaches every Spite program
with the system's own updates, and a program carries no copy of the database:

- **Windows** reads them through `icu.dll`, the ICU library Windows 10 (version 1903 and later) and Windows 11
  ship and update, which holds the IANA database under its IANA names (`library/windows/zone_calendar.spite`, over
  `DynamicLibrary`). Windows' own registry zones are not used: they have Windows names, and less history.
- **Linux and macOS** read the compiled database in `/usr/share/zoneinfo`, one TZif file per zone (RFC 8536),
  through `File` and parsed in Spite (`library/tzif_reader.spite`); `system()` follows `TZ`, then the
  `/etc/localtime` link. This path is written but not yet run on Linux or macOS
  ([the rules](#time-one-stored-instant-zones-for-presentation--implemented-on-windows-the-shape-proposed-by-claude-unconfirmed)
  say how it is tested).

What a zone costs when it runs: on Windows, `icu.dll` is loaded only by a program that asks for a named zone or
`system()`, a zone holds an ICU calendar open until it is dropped, and each offset it answers is one call into
ICU; on Linux and macOS, `find` reads and parses the zone's file once, and each answer looks through the
transitions it read. UTC and fixed offsets need neither.

`zones.read_tzif(name, data)` reads a TZif file you bring, which is the answer for a machine with no database (a
bare container) or a program that must pin one version of the rules. It is the same reader Linux and macOS use,
so it runs on Windows too.

A TZif file has the transitions its zone has had, and a POSIX rule for the years after them
(`EST5EDT,M3.2.0,M11.1.0`), which is how the reader answers for 2100. A time zone's rules are only
known up to when they were written, on every system: a future law change is not in them yet.

## Text

`TimeText()` reads and writes ISO 8601 as RFC 3339 and RFC 9557 profile it. Writing is every type's
`to_string()`; reading is one function per type, each answering `null` for text that is not exactly that type:

| Function | Reads | Refuses |
|---|---|---|
| `read_instant(text)` | `2024-03-10T07:00:00Z`, `2024-03-10T03:00:00-04:00[America/New_York]` | a reading with no offset |
| `read_date_time(text)` | `2024-03-10T02:30:00`, `2024-03-10 02:30` | a reading with an offset |
| `read_date(text)` | `2024-02-29`, `+012345-06-07` | `2023-02-29` |
| `read_time(text)` | `09:30`, `23:59:59.5` | `24:00` |
| `read_duration(text)` | `PT1H30M`, `-PT0.5S` | `P1D`: a day is a `Period` |
| `read_period(text)` | `P1Y2M3D`, `P2W` | `PT1H`: an hour is a `Duration` |

The refusals are the point: text that carries an offset is an instant, and reading it as a local reading would
drop the one fact that makes it exact, while text without one is a local reading and needs a zone to become an
instant. A bracketed zone name after an offset is read and ignored, since the offset already says exactly when.
A leap second (`23:59:60`) reads as `23:59:59`; `T` may be written `t` or a space, and a fraction may follow
`,` as well as `.`.

```gdscript title=time_text_reading/time_text_reading.spite entry
var console = Console()
var time_text = TimeText()

func TimeTextReading() {
    var exact = time_text.read_instant("2024-03-10T03:00:00-04:00[America/New_York]")
    crash exact
    var local = time_text.read_date_time("2024-03-10 02:30")
    crash local
    var lap = time_text.read_duration("PT90M")
    crash lap
    console.print(exact, local, lap)
    var no_offset = time_text.read_instant("2024-03-10T02:30:00")
    if no_offset {
        console.print("unexpected")
    } else {
        console.print("a local reading is not an instant")
    }
    var no_such_day = time_text.read_date("2023-02-29")
    if no_such_day {
        console.print("unexpected")
    } else {
        console.print("2023 has no February 29")
    }
}
```
```output
2024-03-10T07:00:00Z 2024-03-10T02:30:00 PT1H30M
a local reading is not an instant
2023 has no February 29
```

## Not here yet

Calendars other than the ISO one, leap seconds (a leap second reads as the second before it, as everywhere
else), formatting patterns and localized month names, and a zone's abbreviations (`EST`).

## Why this design

D127 asked for the best date support there is, so that Spite never repeats JavaScript's `Date` -- a type that is
an instant and a local reading at once, whose months count from zero and whose replacement took most of a decade,
with a wrapper library in every project meanwhile -- and for one stored format with zones only for presentation. Five
modern designs were compared:

- **JavaScript's `Temporal`** (the replacement for `Date`, in browsers from 2025) gets the split right:
  `Instant` for exact time, `PlainDate`/`PlainTime`/`PlainDateTime` for readings, zones by IANA name, RFC 9557
  text, and the `'compatible'`/`'earlier'`/`'later'`/`'reject'` choice for gaps and overlaps, which Spite copies.
  It gets two things wrong for Spite: its `Duration` holds years and months beside hours and nanoseconds, so
  whether a day is 24 hours depends on a `relativeTo` option; and `ZonedDateTime` makes an instant-plus-zone a
  thing to store, which is the opposite of zones as presentation. Its surface -- non-ISO calendars, year-month
  and month-day types, rounding options -- is several ways to do each thing.
- **Java's `java.time`** (JSR-310, from Joda-Time) has the separation Spite wants between `Duration` (exact
  seconds and nanoseconds) and `Period` (years, months, days), immutable values, and zone rules that answer
  transitions, read from the JDK's own copy of the database; its zone-less types are `LocalDate`, `LocalTime`
  and `LocalDateTime`. It also has `ZonedDateTime`,
  `OffsetDateTime` and `OffsetTime` besides, and it resolves a gap or an overlap by a default nobody chose at the
  call (`atZone`), with `ZoneId.systemDefault()` a line away.
- **Rust's `jiff`** is Temporal in Rust: `Timestamp`, `civil::Date`/`Time`/`DateTime`, `Zoned`, `Span` and
  `SignedDuration`, and it reads `/usr/share/zoneinfo` rather than shipping a copy -- the approach Spite copies
  for Linux and macOS. It keeps Temporal's two faults (`Span` mixes calendar and exact units; `Zoned` is a stored
  type). **`chrono`**, its predecessor, has `NaiveDate`/`NaiveDateTime` and a `LocalResult` that makes an
  ambiguous reading a value, but carries the zone as a type parameter, added month arithmetic late, and left the
  database to competing crates -- the ecosystem split D127 was written against.
- **.NET's NodaTime** (by Jon Skeet, written because `DateTime` has the same fault as JavaScript's `Date`) is the
  most principled: `Instant`, NodaTime's `LocalDate`/`LocalTime`/`LocalDateTime`, `Duration` apart from `Period`, a clock
  that is a service rather than a global, and gap and overlap resolution that is always explicit
  (`AtStrictly`, `AtLeniently`, a resolver). It ships its own copy of the database, and it too has
  `ZonedDateTime` and `OffsetDateTime`.
- **Go's `time`** is small and has the right monotonic-clock idea (a `Time` carries a monotonic reading for
  measuring), but one `Time` is both an instant and a local reading with a location attached, so `==` compares
  locations too and `Equal` must be remembered; there is no date-only type; a `Duration` is nanoseconds in 64 bits
  (292 years); formatting is by a magic reference date. `LoadLocation` reads the system database, with an
  embeddable copy as a fallback.

**Spite copies NodaTime's model with Temporal's vocabulary.** From NodaTime and `java.time`: an `Instant`, the
three zone-less types, and a `Duration` that can never hold a day beside a `Period` that can never hold an hour.
From Temporal: the ambiguity words, the ISO 8601 and RFC 9557 text, and printing an `Instant` in UTC with `Z`.
From jiff: reading the operating system's database. What all of them have and Spite leaves out is a stored
zoned type (`ZonedDateTime`, `Zoned`, `OffsetDateTime`): D127 makes a zone presentation, so an instant is stored
and a zone is applied when it is shown, and the one conversion that needs a zone -- local reading to instant --
names it and names its ambiguity rule every time. The zone-less types are plainly `Date`, `Time` and `DateTime`
(D160), jiff's words, rather than NodaTime's and `java.time`'s `Local*` or Temporal's `Plain*`: a reader takes
"local" to mean "has a zone", which is the confusion Temporal's `Plain` avoided, and a type that does carry a zone
for presentation gets a prefix.

**Why not embed a copy of the database**, as NodaTime, Go's fallback and jiff on Windows do: a copy is out of date
the day a country changes its law, and every program built with it stays wrong until it is rebuilt, while the
operating system's copy is updated for every program at once. It would also add a few hundred kilobytes to every
program that names a zone. The costs are a machine with no database, where `find` answers `null` and
`read_tzif` takes a file you ship, and Windows before 10 version 1903, which has no `icu.dll`.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Time: one stored instant, zones for presentation  **[implemented on Windows; the shape proposed by Claude, unconfirmed]**

D127 (decided by Mortaro): the best date and time support there is, so that Spite never repeats JavaScript's
`Date` and the wrappers it bred, with **one stored format -- an exact instant -- and time zones only a
presentation layer**, copied from the best modern design; [Why this design](#why-this-design) compares the
candidates and argues the choice. The names are Mortaro's: `Instant`, `Duration` and `Period` (D158), and for a
zone-less reading `Date`, `Time` and `DateTime` (D160), with no `Local` prefix (the old spellings are errors that
name the new ones: `'LocalDate' is spelled 'Date'`). Everything else below is Claude's proposal, NodaTime's model
with Temporal's vocabulary and no stored zoned type (proposed by Claude, unconfirmed; the constructors, `now()`
and the lenient reading of text are still open as `mortaros_missing_decisions.md` items 118, 120 and 121).

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
  overlap to its first instant -- RFC 5545's rule, and `java.time`'s and `Temporal`'s default), `'earlier'` or
  `'later'`. The zone finds the offsets a day either side of the reading and keeps the ones that map back to it:
  both for an overlap, neither for a gap.
- **A value that cannot exist halts; text that cannot exist is `null`.** `Date(2023, 2, 29)` and
  `Time(24, 0, 0, 0)` crash, since a program that builds one from its own numbers has a bug (D24); reading
  text answers `T?`. A leap second in text reads as the second before it.
- **The database is the operating system's, and nothing is embedded.** Windows reads IANA zones through
  `icu.dll` (Windows 10 1903 and later), loaded only when a named zone is asked for
  (`library/windows/zone_calendar.spite`); Linux and macOS read the TZif files under `/usr/share/zoneinfo` in Spite
  (`library/tzif_reader.spite`: RFC 8536 versions 1 to 4 and the POSIX rule in the footer), and `system()` follows
  `TZ`, then `/etc/localtime`. `read_tzif` is the same reader for a file a program brings. **The Linux and macOS
  path is untested**: `check.sh` holds it to compiling (it writes `conformance/stage6/daylight_saving` for each
  system), and `conformance/stage6/zone_files` runs the TZif reader on Windows over three files written for the
  test and checked with Python's `zoneinfo`.
- **Clock** keeps `elapsed_nanoseconds()` and `elapsed_milliseconds()` for measuring, and `now()` answers an
  `Instant`.

Not built: calendars other than ISO 8601's, leap seconds, formatting patterns and localized names, zone
abbreviations. `conformance/stage6/instant_arithmetic`, `calendar_math`, `daylight_saving`, `zone_files`,
`fixed_offsets`, `time_text_round_trips` and `time_text_errors`; `docs/time.md`.
