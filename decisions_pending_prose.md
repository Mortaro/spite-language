# Decisions pending prose

Language decisions D4-D34 are recorded in `manual.md`'s decision log, but their prose sections were not
written: two agents were editing `manual.md` and `PLAN.md` at the same time on 2026-09-19, so this file held
them instead. It is owned by the C/DLL and web session; `mortaros_notes.md` stays an inbox for Mortaro.

Each item says what was decided and which section still needs writing. Delete an item once its prose lands.

FROM THE C/DLL + WEB SESSION (2026-09-19): three things were decided but are NOT in manual.md or PLAN.md right now.
D14 was written and then lost when two agents wrote manual.md/PLAN.md at the same time; the other two were never added.
Re-add them, or tell that session to.

1. [DONE 2026-09-20 -- restored: manual.md section 15 subsection, the philosophy bullet, the D14 decision row, and PLAN.md milestone 15] D14 PURE SPITE (decided by Mortaro: "yes, make sure we achieve pure spite"). Belongs as manual.md section 15
   subsection "Pure Spite: dissolving the runtime", a decision-log row, a philosophy bullet in section 1, and
   PLAN.md milestone 15.
   - The hand-written C runtime is a bootstrapping stage, NOT the design. src/runtime/spite_runtime.h (722 lines)
     and spite_repl.h (1392 lines) exist only because the compiler needed a String before Spite could express one.
   - D6 already forces this: Spite.Class cannot be "an ordinary standard library class with an ordinary
     declaration" while the standard library is hand-written C.
   - Where the 722 lines go: ~400 (String methods, retain/release, number-to-text, List/Dictionary support) become
     ordinary .spite sources; ~150 (File/Directory/Process/sleep) become DynamicLibrary calls (D4); ~130
     (allocator wrappers + --debug-memory live-pointer table) mostly become Spite, malloc/free is the floor;
     ~50 (float formatting via snprintf %g) becomes a shortest-round-trip implementation in Spite or an FFI call.
   - THE FLOOR IS A LIST OF INTRINSICS, NOT A FILE: five to ten compiler intrinsics for raw memory
     (mmap/VirtualAlloc through the FFI on native, memory.grow on wasm). Needed because a String needs allocation,
     allocation needs OS pages, and asking for them through the FFI needs a String -- the bottom cannot route
     through the standard library.
   - Purity costs no performance: the output is still C, so a Spite-written trim() meets the same optimiser.
   - Side benefit: standard library classes become reopenable (section 16 item 7) and REPL-inspectable, because
     they stop being special.
   - The web JS shim is the one exception BY CONSTRUCTION (it runs in the JS VM), but its target is ZERO
     HAND-WRITTEN LINES: generated from the external js declarations and the DOM opcode table, so no second
     language lives in the repo. ~150-300 lines today.
   - PLAN milestone 15, ordered: (a) name the intrinsic floor -- design step, gates the rest; (b) File/Directory/
     Process/Program.sleep become DynamicLibrary calls, needs milestone 11; (c) String/List/Dictionary/the Spite.*
     reflection classes move to .spite under a spite/ root, wants milestone 8b; (d) spite_repl.h, needs milestone
     10's reflection; (e) drive the web shim's hand-written portion to zero, with milestone 12.

2. D15 NEEDS A PLAN ENTRY (the manual has D15, PLAN has nothing). Standard library templates all name a <member>,
   where a member is a field or a zero-argument function with no distinction (map_age == map_get_age, because
   attribute reads already go through get_<attribute>()). Compiler work: the template matcher currently only
   matches fields, so map_/filter_/sum_/sort_by_/find_by_/any_/all_/each_ must accept a zero-argument function
   too, and each template checks arity + return type instead of field-ness. Makes todos.map_render() work with no
   new feature. Migration: every "<attribute>"/"<function>" template name in docs/ and src/ becomes "<member>".

3. STILL OPEN, IN PRIORITY ORDER, ALL BLOCKING REAL WORK:
   - ERRORS: RESOLVED 2026-09-19/20 as D24-D34, prose written into manual.md section 5. What is left open inside
     it: whether D25's trace records predicate asserts only, and whether "a constructor is setup, not logic"
     generalises past assert into "a constructor performs no fallible work at all".
   - Trailing block sugar for builder calls -- f(x) { a b c } == f(x, [a b c]) -- proposed for markup readability
     (Ruby builder pattern, useful for SVG/XML/tests/config/terminal UI, not just HTML). Pure sugar, no closures.
     Deferred deliberately: write a real component first and see whether the [ ] nesting hurts.
   - type declarations requiring functions: DECIDED as D16.
   - Function references: DECIDED as D17 (first-class, always bound to an instance), which also unblocks the
     FFI callbacks that section 17 lists as not designed yet.

ALSO FROM THAT SESSION (2026-09-19), decided just now and only in manual.md's decision log so far -- each still
needs its prose section, which was not written because two agents are writing the same files:
4. [DONE 2026-09-20 -- manual.md section 7, "Inline types and duck typing"] D16: a type may require FUNCTIONS, not only attributes -- type Renderable = { render(): Element }, matched by
   shape like any other type. Needs a paragraph in manual.md section 7 "Inline types and duck typing". This is
   what lets a children list hold any class that responds to render().
5. [DONE 2026-09-20 -- manual.md section 5, "Functions are values, always bound to an instance"] D17: functions are FIRST-CLASS and always bound to an instance. console.log(pretty_print) passes
   {instance, function} and calling it runs as if the owning instance called it. No free functions, no closures,
   nothing captured but the receiver -- one retain. Needs a subsection in manual.md section 5 "Functions".
   Two consequences to fold in when it is written:
   - section 17's "Not designed yet" callbacks are now solvable: {instance, function} is exactly a C callback
     plus its void* user data. Remove that bullet.
   - an event handler can be onclick: increment (signature-checked) instead of the symbol 'increment' from D10.
     D10's symbol literals stay useful elsewhere; the markup story should probably switch to bound functions.
   - STILL OPEN: how a function-valued parameter's type is written. Either a one-function type (D16) or a
     signature form like func(value: String): String. Mortaro has not picked.
6. D18: no JSX, no trailing block; markup is missing_function + literals, with VARIADIC CHILDREN (free, because a
   Symbol-codegen template monomorphizes per call site, so no varargs feature is needed):
       html.div({ class: "card" }, html.h1(title), TodoForm({ ondone: add_todo }))
   Needs a "Markup" subsection in manual.md section 17, next to "Isomorphic classes", pulling together D4 (tags
   from missing_function), D15 (map_<member>() for lists of children), D16 (components by shape), D17 (handlers
   as bound functions) and a when(condition, element) helper returning Nullable<Element> that the framework skips.
   Rejection reasons worth keeping so nobody re-proposes them: a JSX-like literal is a second grammar and fixes
   neither the conditional nor the mapping problem; a trailing block serves exactly ONE shape, since queries are
   database.filter_age_greater_than(10).sort_by_name() and routes are plain statements, so it would be a sixth
   meaning for { } bought for a single case. Mortaro's justification for the verbosity: annoying for a human to
   WRITE, cheap for an AI to write, and a human can still easily READ it -- which is philosophy line one.
7. D19: Html is the web counterpart of DynamicLibrary -- a singleton whose missing_function/missing_attribute
   resolve against the browser, Html.Node wrapping one node handle (index into the JS-side table, freed by
   drop()), naming rule 'camel_case' (already in the Naming enum): document.create_element("div") ->
   createElement, node.text_content = "Hello" -> textContent, root.append_child(node) -> appendChild. Call =
   method, read/write = property, the same split D4 uses for functions vs constants. Html is the low-level layer
   D18's markup builder compiles down to, like Mouse sits on DynamicLibrary. Needs a subsection in section 17.
   OPEN: D18 calls the markup builder html.div(...), which now collides -- the builder needs another name
   (Markup? View?), since div is an element to create, not a DOM method. Mortaro has not picked.
8. D20: a class declares its targets with a class-level function (D6) -- func targets(): List<Symbol> -- default
   every target, DynamicLibrary returns ['native'], Html returns ['web'], and using one against the wrong
   $target is a compile error naming class, target and flag. THIS REPLACES the ad-hoc rule in section 17 that
   File/Directory/Process are an error under --target=web: they just declare targets(), and the special case is
   deleted. Checked AFTER compile-time folding and tree shaking, so load() behind if $target == 'web' stays
   legal -- only a class still reachable in the built program is checked. When section 17 is edited, delete the
   old File/Directory/Process bullet and point it at targets() instead.
9. LATER, Mortaro's idea (2026-09-19), deliberately deferred: COMPILE-TIME GENERATION OF CLASS FILES. A framework
   could generate a new version of a namespace at compile time -- Nullstack emitting a HypertextLanguage with real
   ul/li/div functions rather than catching everything through missing_function.
   Why it is stronger than missing_function: generated functions are enumerable (Html.functions lists the real
   tags), reflectable (D12), and visible in --final-classes, so errors name a real function instead of a hook.
   missing_function is a catch-all that cannot say what is valid.
   Generalizes well beyond markup: a class per database table, typed route functions, a class per protocol message.
   Constraints to respect when it is designed:
   - It is only acceptable if the output is ordinary readable Spite source in --final-classes. That is the line
     between this and macros, which the philosophy forbids: a macro is an inline syntax transform, this is
     generating source a human can read. Section 16 item 9 already demands exactly that ("VISIBLE in the final
     class output, not compiler magic").
   - Generators must be pure and cached by input hash. Compilation speed is philosophy line five; running
     arbitrary Spite at compile time is the obvious way to lose it.
   - Needs a defined position in the reopen/merge order of section 11, since a generated class can itself be
     reopened by a later root.
   - Prerequisite: milestone 10's compile-time evaluator (same dependency as D6 overrides and static markup folding).
10. D22/D23: JSON is reflection, not a library (write = walk attributes, read = symbol-keyed writes, type shapes
    are the JSON shape, shares D13's wire format). Standard library after bootstrap is written WITH the
    metaprogramming, not alongside it -- that is the other half of D14. Needs a subsection in section 15.
    Claude's refinement worth keeping: to_json() needs NO failing variant, because an unserialisable class is a
    compile error (the check D13 already requires). Only PARSING can fail at runtime. OPEN: naming for the pair --
    Mortaro sketched to_json/to_crashing_json, Claude prefers parse_json()/parse_json_or_crash().
11. [DONE 2026-09-20 -- manual.md section 5, "Failure: three outcomes and no others"] THE ERRORS DESIGN (still the biggest open question, blocks D13/FFI/framework) now has a shape emerging from
    D22 -- three rules, write them up and see if Mortaro agrees:
    (a) If the compiler can know it, it is a COMPILE error, never a runtime one. Already true of: unserialisable
        types (D13), wrong $target (D20), unfilled codegen holes (D9), missing foreign symbols (D4), a member that
        does not fit a template (D15), a symbol that is not in an enum (D10).
    (b) Failure that is an expected outcome of untrusted input returns Nullable<T>, narrowed with assert. This is
        already the language's null-safety idiom, so it costs nothing new.
    (c) Everything else CRASHES with a full report, because an AI reads a precise crash better than a swallowed
        null. A paired loud variant exists only where (b) applies, for callers who want the report instead.
    Note the tension to resolve: `assert` today returns the DEFAULT value of the return type when it fails, which
    is a silent soft failure, not a crash. Decide whether that stays, or whether assert should crash in the cases
    rule (c) covers.
12. [DONE 2026-09-20 -- manual.md section 5, "Failure: three outcomes and no others"] D24 ANSWERS THE ERRORS QUESTION (notes item 11's tension is resolved: assert stays soft BY INTENT -- it is for
    when the developer wants the program to keep running -- and crashing is a separate outcome for when things
    should halt to be recoded). Three outcomes, no others: compile error / assert / crash. Nullable<T> is the only
    runtime failure value and carries no reason. Needs a section of its own in manual.md, probably a new section
    or a subsection of section 5 next to assert narrowing. TWO THINGS STILL OPEN:
    - how a deliberate crash is spelled (Program().crash("...")? a builtin crash("...")?)
    - a crash report must name the Spite file, line and call chain, or it is useless to an AI. That is a real
      requirement on the C backend (line information carried through codegen), not just a message format.
    Claude's addition that Mortaro has not confirmed: "if a distinction is actionable, it is data, not an error" --
    a caller needing to tell a timeout from a rejection gets a type/union/enum, which is ordinary data. Without
    this rule the model cannot express real networking; with it, nothing needs an error channel.
13. [DONE 2026-09-20 -- manual.md section 5, "Failure: three outcomes and no others"] D25 closes the loop on D24: a crash always reports backend info + the trace of every failed assert before it,
    which makes assert's silent substitution observable without changing its semantics. Cost is only on the
    failure path (a passing assert already branches), the trace is a fixed-size ring buffer + total count so it
    never allocates or grows unbounded, and the same applies to compile-time evaluation failures. REQUIREMENT:
    Spite file/line must survive into generated C -- #line directives, which also make the C compiler's own
    errors point at Spite source. OPEN: ring buffer size; whether --development records passing asserts too.
    Context, recorded because Mortaro asked: Claude's reservation as the consuming AI was that the compile-error
    plus crash spine is better than any error system it normally works with -- compile errors are the only
    channel it reliably consumes, and removing exceptions removes boilerplate it generates out of habit rather
    than usefulness -- but that assert returning a fabricated default propagates a wrong value with no signal
    ever reaching the AI, which is the swallowed null in a different hat. The distinction that matters:
    `assert value` for null-narrowing is fine (the default is the sensible continuation and the type system
    forced the choice); `assert condition` as input validation is where it bites (the default is a fabricated
    number meaning nothing). The worry is not that the tool exists but that it is the shortest thing to write,
    so both a human and an AI will reach for it by default. D25 is the mitigation.
14. [DONE 2026-09-20 -- manual.md section 5, "Failure: three outcomes and no others"] D26: assert is CONTROL FLOW, not validation -- a guard clause for known-nullable values where absence just
    means "no more logic here", so web servers and games rarely crash. Two Claude refinements NOT yet confirmed:
    (a) assert is only sound when the return type can express "nothing" (Nullable<T>, empty List, empty String).
        A bare scalar return cannot -- count_user_orders(): Int guarded by assert returns 0 for both "no user"
        and "zero orders". Proposed lint: assert in a function returning a bare scalar is an error, escaped by
        returning Nullable<T> or using if ... do. Tension: no warnings in this language, so error or silence.
    (b) D25's crash trace should record PREDICATE asserts only, not narrowing ones -- narrowing failures are
        routine (thousands an hour on a server) and would bury the signal; a predicate assert firing means an
        assumption was wrong, which is what the crash report wants.
15. [DONE 2026-09-20 -- manual.md section 5, "Failure: three outcomes and no others"] D27 (supersedes D26 refinement (a)): assert is legal ONLY in a function returning nothing or Nullable<T>.
    Empty List/String are as ambiguous as 0 -- caller cannot tell "no user" from "user with no orders". Anything
    else is a compile error; the author widens to Nullable<T> or uses if ... do and returns what they mean.
    OPEN (Claude, unconfirmed): a CONSTRUCTOR should be banned too -- it has no return type so the rule admits
    it, but the caller receives an all-defaults object it cannot distinguish from a real one. Diagnostic should
    say: check before constructing, or move the lookup into a function returning Nullable<T>.
16. [DONE 2026-09-20 -- manual.md section 5, "Failure: three outcomes and no others"] D28: assert is BANNED in constructors. Mortaro's reasoning: a constructor is SETUP, not logic -- if something
    can be invalid in one, function logic has been put where only setup belongs. Pushes constructors to take
    already-resolved values instead of identifiers to resolve.
    TWO FIXES THIS FORCES when the prose is written:
    - section 17's DynamicLibrary constructor currently ends with `assert _handle`, now illegal, and it was wrong
      anyway: a missing library is a CRASH (D24 rule 3), which section 17's own prose already says. Same fix for
      Html/BrowserLibrary (D19).
    - OPEN (Claude, unconfirmed): the entry constructor should be EXEMPT -- the 2026-09-18 decision makes the
      entry file's constructor the program itself, so it is main wearing a constructor's name and its body is
      logic by definition.
    OPEN (Claude): does the rationale generalise past assert into "a constructor performs no fallible work at
    all" -- no file reads, no lookups, no network? Same principle as a rule rather than as a consequence.
17. [DONE 2026-09-20 -- manual.md section 5, "Failure: three outcomes and no others"] D29: the entry constructor is NOT exempt from D28 (Claude's proposed exemption rejected). A program
    constructor that grows complex splits into smaller functions it calls -- the same decomposition D27 forces
    elsewhere, so the "ceremonial delegation" worry was wrong. D28 stays one uniform rule, no special case.
    Side benefit: the entry constructor reads as a table of contents for the program, logic one level down.
18. [DONE 2026-09-20 -- manual.md section 5, "Failure: three outcomes and no others"] D30: crash is a KEYWORD -- CONFIRMED 2026-09-19 including the mirror-of-assert form. Claude's
    recommended form, not yet confirmed: it mirrors assert exactly -- same polarity, crash <condition> halts on
    falsey, bare crash for unreachable branches. Argued against `crash not database.connect()` because assert
    firing on falsey and crash firing on truthy makes two similar statements behave oppositely.
    KEY CONSEQUENCE: `crash user` narrows Nullable<T> like assert does but never returns, which FILLS THE HOLE
    D27 OPENS -- a function returning Int can now guard without widening to Nullable<Int>. Three forms cover the
    space: absence is fine -> assert, absence is a bug -> crash, absence is meaningful -> if ... do.
    Needs: keyword added to section 2's list, a subsection next to assert narrowing in section 5.
19. D31: JSON IS NOT THE WIRE FORMAT. D13 sends compile-time-derived binary packing (fields in declaration
    order, no keys, no parsing) because JSON's whole cost is self-description and there are no unknown consumers
    -- both bundles compile from the same source. Compounds with wasm: no string marshalling across the JS
    boundary, so DOM command buffer and RPC channel are both opaque byte buffers. D22 AMENDED: JSON demoted to a
    foreign-systems format in the standard library, not the default. Claude's two requirements, unconfirmed:
    schema hash in the handshake (crash on mismatch, the old-client/new-server case), and --development decoding
    payloads to JSON on demand since the compiler knows the schema.
20. [DONE 2026-09-20 -- manual.md section 5, "What a crash reports"] D32: crash sites get a compile-time ID; the compiler emits an ID->SOURCE MAP as a build artifact instead of
    embedding location/condition text in the program. Binary shrinks (matters most for wasm startup) and the same
    site keeps the same id across $targets, so server-bundle and browser-bundle crashes compare directly -- what
    D13's isomorphic classes need. Same principle as D31: the static half never travels.
    Claude's requirements, unconfirmed: (1) the id MUST be content-derived (hash of namespace/class/function/
    per-function ordinal/condition source), never a counter -- a counter renumbers on every inserted line and
    destroys both old reports and cross-target identity; (2) the map is an archived artifact with a build hash,
    pairing with D31's schema hash; (3) --development embeds text directly, only optimised builds emit bare ids;
    (4) D25's assert trace becomes a ring buffer of (id, values), much cheaper than strings; (5) D25's #line
    requirement narrows to compile-time diagnostics only. Format preference: greppable line-based text table.
21. [DONE 2026-09-20 -- manual.md section 5, "What a crash reports"] D33: the D32 crash map format is DECIDED -- greppable tab-separated text, <output-name>.crashes, one line per
    site sorted by id (so archived maps diff cleanly), fields: id (content-derived hex), file, line, column,
    class, function, kind (crash / assert-predicate / assert-narrowing), condition source, operand names+types.
    Opens with a # comment line carrying format version + build hash. Runtime emits one tab-separated line per
    event prefixed spite.crash / spite.assert. Values rendered as text at crash time (a crash happens once, so
    be informative not fast) but stored RAW in the assert ring buffer (narrowing asserts fire thousands of times
    an hour per D26, so they must stay cheap until something dumps them).
22. D34 (2026-09-20): COMMENTS ARE ONE LINE AND NOTHING BUT A MARKDOWN LINK -- # path/to/file.md#github-anchor,
    outside function scope only. Compiler validates that file AND anchor resolve. Anything else is an error,
    including // and /* */ which the lexer recognises specifically to reject them with the explanation. The
    error message teaches: states the one legal form, then asks whether the note is needed at all (re-derivable
    -> delete; durable -> write the section first; a warning against a change -> a test or a diagnostic pushes
    harder). Rationale chain: prose rots into a lie but a link fails the build; re-derivable notes are free for
    an AI to reconstruct; zero comments was rejected on TOKEN ECONOMICS -- a grep costs tokens on every edit to
    discover absence, which is the common case, while a link costs ~10 tokens only where there is something to
    say, so absence becomes free and trustworthy.
    MIGRATION IS LARGE: 405 comment lines across 59 .spite files, plus every fenced sample in docs/ (which
    check.sh compiles them). Most should be DELETED, not converted -- that is the point of the
    rule. Needs: lexer change, linter rule, anchor resolution (GitHub slug rules: lowercase, punctuation
    stripped, spaces to hyphens, -1/-2 for duplicates), and a docs/for_ai_writers.md section stating the
    re-derivability test.
