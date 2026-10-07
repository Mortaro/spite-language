# Lists, dictionaries and member templates

The specification of [Lists, dictionaries and member templates](../docs/collections.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Standard library metaprogramming

**Every standard library template names a `<member>`, and a member is a field or a zero-argument function without
distinction.** `map_age` and `map_get_age` are the same call, because reading an attribute already goes through
`get_<attribute>()` ([Operators](functions_and_operators.md#operators)). The compiler may answer the
field case with a direct read, but that is an optimisation, not something the writer thinks about. What a template
requires of a member is its **arity and its return type**, never whether it is stored or computed.

The templates, what each answers and what it requires of the member are [the table on the page](../docs/collections.md#member-templates-loops-you-do-not-write),
which is normative; a `List<T>` of a class and a `Dictionary<T>` of a class answer all of them. These, together
with `while` and the passed functions below, cover the cases a `for` loop covers elsewhere. This is why a list of
components renders with nothing new in the language: `todos.map_renders()` calls `render()` on every element and
collects the results, exactly as `todos.map_titles()` collects a field.

A member that does not fit is a compile error naming the member, what it is, and what the template needs. Two
of them name a fix: `count()` is only ever a collection's own size, so `count_<member>()` on a numeric member says
`but 'count_' needs it to return Boolean (to add up a numeric member use 'sum_stars')`; and `each_<member>()` on an
attribute says `'each_size': the member 'size' of 'Thing' is an Integer, but 'each_' needs it to be a function:
reading an attribute and discarding it does nothing`. `sort_by_` is stable (equal keys keep their order) and
is a merge sort, so its time grows as `n log n`: it reads each key once, then merges runs of an index list, and
makes the key list, two index lists and the result (`sort_by(f)` is the same template, and `Directory`'s
`entries()` sorts the names through it; `conformance/stage6/sorting_many`).

**How the templates are written.** They are templates in `library/list.spite`, each a `while` over the list's
`Memory` buffer: `func filter_member(member: Spite.AttributeDeclaration<$element_type>): List<$element_type>` answers every
`filter_<member>` call. The parameter names a member of the *element*, not of the list (whose own attributes are
its buffer), because it says so: `Spite.AttributeDeclaration<$element_type>` is a [template](metaprogramming.md#templates)'s
`Spite.AttributeDeclaration<Label>`, one mechanism for both. `item.attributes[member]` reads it: the field, or a call to the
zero-argument function, which is "the value held in that field" applied to members. A `String` element's
members are `String`'s own zero-argument functions (`upper_case`, `length`, `is_empty`, ...). The generator binds the template to the
element's member and checks [the table](../docs/collections.md#member-templates-loops-you-do-not-write) before it compiles the body, so a member that does not fit is still
the error naming the member, its type and what the template needs; it writes none of the templates' C. Only the
names a program calls are compiled. A `--repl`/`--repl-port` build compiles every template that fits every
element class of a list the loop can reach, and lists them as that list's functions, so `monsters.sum_health()`
works at the prompt. `Dictionary<T>` answers the same names through its values (`inventory.sum_price()` is
`inventory.values().sum_price()`), and a program's own `list.spite` reopens `List` to add a template of its own,
as its `dictionary.spite` reopens `Dictionary` (over `Spite.AttributeDeclaration<$value_type>`).
A template there with a plain `member: Symbol` would range over `List`'s own attributes, its buffer, and answer
nothing, so it is a compile error naming `Symbol<$element_type>` (`diagnostics/plain_symbol_on_list`).

**Chains are one loop.** Each template takes what the previous one returns, and a chain means exactly its steps written out one by one.
The compiler runs a chain as one loop over the first list with no list in between: when a template is called
directly on a `map_`/`filter_` call (on a `map_`/`filter_` call, and so on) of a `List` or `Dictionary`, the
generator writes one Spite function on the first list's class (a `while` over its buffer, an `if` per
`filter_`, a `var` per `map_`, and the last template's step) and calls that instead. A `map_` in the middle
must reach a class; anything the rule cannot write (a nullable member, a union) is compiled step by step, which
means the same. The difference a program can see is only order: a member function in a fused chain runs element
by element. `conformance/stage6/fused_chain_allocations` pins what it saves: four chains run a thousand times
allocate nothing, so the whole program makes 15 allocations (its objects, the list and printing the total)
against more than 16 000 step by step.

**Passing a function for each element.** An iterator sees only the
element and the list, never the class the call is written in: `people.each_say_hello()` names a member
`say_hello` of each `Person`, and a function of the caller never answers
it. When the calling class has a function of that name, the error says to pass it
(`'String' has no attribute or zero argument function 'say_hello' for 'each_say_hello': a template reads a member
of each element, never a function of this class, so pass this class's 'say_hello' instead: 'each(say_hello)'`).
The caller's function is passed as a bound function value, owned by whoever it is bound to:

- **The forms.** `each(f)`, `filter(f)`, `any(f)`, `all(f)`, `count(f)`, `find(f)`, `sort_by(f)` and
  `sum(f)` on a `List` or a `Dictionary` (through its values), for an element of any type, never on a `Vector` or an `Items`,
  whose items are borrowed or text; `remove_where(f)` on a
  `List`, never on a `Dictionary`. `f` takes the element
  as its only argument, with exactly the element's type; [the table of templates](../docs/collections.md#member-templates-loops-you-do-not-write) applies to what it returns (`filter`, `any`,
  `all`, `count` and `find` want `Boolean`, `sum` a number, `sort_by` a number or a `String`, `each`
  anything). `find(f)` answers the first element `f` is true for, or `null`, which is the `find_by_` template with
  `true` as its value. `count` with no argument stays the collection's size.
- **No `map(f)`.** A list has no `map` taking a function: `names.map(measure)` is "there is no 'map(function)':
  collect a member of each element with 'map_<member>()', and a value computed from each element by giving the
  element's class a read-only attribute (a 'get_<name>()' with no setter) and writing 'map_<name>()'". The value
  becomes a named member of the element's class, which every list of that class can collect and a chain fuses.
  A value that needs more than the element (a function of another instance, or of the caller's state) is
  collected by a `while`, which the loop rule leaves alone.
- **`map_` on a test.** `map_<member>()` collects values, and its member must not be a `Boolean`: the plural
  rule for collections applies to nouns, and a test (`is_alive`, `has_target`) is a question about each element.
  `monsters.map_is_alive()` is "'map_is_alive': the member 'is_alive' of 'Monster' is a Boolean, but 'map_'
  needs it to be a value to collect, not a test: write 'filter_is_alive()' to keep the elements it is true for,
  'count_is_alive()' to count them, or 'any_is_alive()' or 'all_is_alive()' to ask whether any or all of them
  pass".
- **The owner.** `say_hello` alone is bound to this instance; `greeter.greet` to `greeter`; a variable holding a
  `Spite.Function<T, R>` is called through the value. `people.filter_active().each(greeter.greet)` mixes a
  member template with another instance's function.
  A list's own function is bound to the list the same way (`numbers.each(found.append)`); a chain that passes one
  is not fused, and runs step by step. So is every library class's: a `Dictionary`'s (`keys.filter(counts.has)`, `keys.count(counts.has)`), and a
  `String`'s or a number's (`words.filter(greeting.contains)`). Any of them may also be held as a value: `var
  lookup = counts.get_at` is bound to that dictionary, and `var check = greeting.contains` to the text `greeting`
  held then. A text or a number is a value, not an object, so the function value keeps its own copy of it, in a
  small box the value lets go with itself: assigning `greeting` anew afterwards does not change what `check` asks
  (`conformance/stage6/held_value_functions`). What the function answers is what the form sees, so `counts.get_at`
  answers `Integer?`, and `keys.sort_by(counts.get_at)` is an error naming the fix: a function of your own
  that narrows it (`conformance/stage6/library_functions_passed`, `diagnostics/library_function_mistakes`).
- **How it is written.** No template changes: the same `library/list.spite` template (`each_member(member:
  Spite.AttributeDeclaration<$element_type>)`) is instantiated once per function and owner class, with a last hidden parameter holding
  the owner (the function's instance, passed at the call site), and `item.attributes[member]` reads as
  `owner.say_hello(item)`, or `owner(item)` when the owner is a held function value. A function written by name
  therefore costs no allocation and no indirect call; only a held value is called through `Spite.Function`. Only
  the forms a program calls are instantiated, and these instances are not listed among the list's `functions`
  nor offered at a `--repl` prompt.
- **Chains.** A passed function chains and fuses with the member templates: `filter(f)` may sit in the middle
  of a chain and any form may end one, so `people.filter_active().each(greeter.greet)` and
  `names.filter(is_short).sum(doubled_length)` are each one loop; the fused function takes each owner as a
  parameter. The element's own members still come only from the element.
- **Mistakes** name the form: `'filter(say_hello)': 'say_hello' returns nothing, but 'filter' needs it to return
  Boolean`, `'each' calls 'greet_twice' with each 'String' as
  its only argument, but 'greet_twice' takes 2`, `'each' calls 'count_to' with each 'String', but 'count_to' takes
  'Integer'`, and an argument that is no function at all.
- **The loop rule.** A `while` that only does what `each(f)` does is under the same rule as the member templates
  ([control_flow.md](control_flow.md#a-while-that-a-member-template-already-says)). A function that needs more than the element (`print_statement(statement, depth)`) keeps its `while`, and so
  does a `while` that collects what a passed function answers for each element, since no form collects one.

`conformance/stage6/passed_functions`, `diagnostics/passed_functions`.

## Member templates over an enum value

**The templates that ask a yes-or-no question of each element also work on a named enum's values.**

- **Which templates.** `filter_`, `count_`, `any_`, `all_` and `remove_where_`, the ones whose member must
  "take nothing, return `Boolean`" in [the table](../docs/collections.md#member-templates-loops-you-do-not-write). The others already
  take a member and compare or read it (`find_by_kind('files')`, `map_kind()`, `sort_by_kind()`), so an enum value
  adds nothing to them, and `sum_<value>` or `each_<value>` is the ordinary "no member" error.
- **How the name is read.** For `list.filter_<name>()` over elements of class `T`: when `T` has a member named
  `<name>`, it is that member, exactly as for any template. Otherwise the compiler looks at `T`'s members typed with a named
  enum (attributes and functions taking no arguments, non-nullable) and at the values each enum lists. Exactly
  one such member whose enum lists `<name>` makes the template read `element.<member> == '<name>'`, and nothing
  else changes: same result type, same fusion, same order, same cost as a `Boolean` member, one comparison of two
  small integers. A reopened enum's values are the reopening's list ([packages.md](../docs/packages.md#reopening-an-enum-replaces-it)).
- **A union element** is read through a member every class of the union answers, with the same enum type; a
  member only some of them answer is not a candidate.
- **Errors, never a guess.** No member and no enum value: the usual error, which then also says no enum of
  `T`'s members lists `<name>`: "'Post' has no attribute or zero argument function 'deleted' for 'any_deleted', and
  no enum of its members lists 'deleted'". Two or more members whose enums list `<name>`: "'filter_<name>' could
  read '<a>' or '<b>', whose enums both list '<name>': write the comparison as a Boolean member of <T>, or pass a
  function". A member named `<name>` that is also a value of such an enum: an error naming both, since which one is
  read must not depend on what else the class declares: "'count_published' could read the member 'published' of
  'Post' or compare 'stage' with 'published', a value of its enum: rename the member 'published'".
- **A member a union's classes answer differently** (an attribute in one, a getter `get_<member>()` in another) is
  read through each class's own, for these templates as for every other.
- **No helper answers one kind of a listing.** A function that answers the part of a listing of one kind, such as
  `Directory.files()` or `Directory.folders()`, would be a second way to write `entries().filter_files()`, so there
  is none.
- **Tree shaking and run time.** Resolved while compiling; nothing exists at run time that a `Boolean` member's
  template would not have, and an enum no template reads costs nothing.

## List<T> additions

A list's members, their results and their edge cases are [the table under `List<T>`](../docs/collections.md#listt), which is normative:
`list[index]` and `get_at(index)` are one function and answer `T?`, `null` out of range (`[]` is only
a shortcut for `get_at`, [functions_and_operators.md](functions_and_operators.md#operators); an out of
range read answers `null` and never a default that would read as a real element); `first`, `last`, `remove_first`
and `remove_last` answer `null` on an empty
list (like `[]`), `insert` clamps to the nearest end, and `set_at`, `remove_at` and `remove_swapping` halt,
naming the line. `reserve(count)` grows the buffer to hold `count` elements and adds none. A `List` of numbers,
`Boolean`, enums or `Memory.Address` is the one way to keep a list of them: its buffer holds the values themselves, which is
what a `Vector` of them held (`conformance/stage6/plain_items`). There is
no `for`: a list is walked with a template, a passed function ([above](#standard-library-metaprogramming)),
or a `while` that does more than they do.

`contains(value)` compares with `==`, so it is there only for elements that are numbers, `Boolean`, `String` or an
enum; on a list of a class the call is `List has no method 'contains'`: ask with `any(f)` or `find_by_<member>`.
`index_of(value)` is the same comparison answering where: the first element's index, or `null` when no element is
equal, so a missing value is never read as a place. On a list of a class, `find_index_by_<member>(value)` answers
the index of the first element whose member equals `value`, the place `find_by_<member>` would have found.

Names say where: `add` does not, so it is `append` (and `prepend`); `pop()` is `remove_last()`, beside
`remove_first()`. Writing `add` or `pop` is a compile error naming the replacement: `List has no method 'add', which
does not say where: write 'append' to add at the end, or 'prepend' at the start`, and `List has no method 'pop':
write 'remove_last()', or 'remove_first()' to take from the start` (`diagnostics/old_list_names`).

## Dictionary\<T\>

Insertion-ordered, keyed by text or by whole numbers; its members are [the table under
`Dictionary<T>`](../docs/collections.md#dictionaryt), which is normative. `dictionary[key]` is `get_at(key)`, a `T?` that is `null` for an
absent key, and `dictionary[key] = value` is `set_at(key, value)`, the same functions as every other `[]`.
`keys()` and `values()` answer fresh copies, in insertion order.

**A dictionary literal** is `{` then entries apart with commas or newlines (a last comma allowed), each a key, `:`
and a value, then `}`. A key is a text literal or a whole-number literal, and the first key decides which: a brace
that opens on a name is an object literal instead. The entries are set in the order written; the value type is the
declared type's when the literal is stored where a `Dictionary<T>` is declared, and the first value's otherwise.
The same key twice is `"key" is written twice in this dictionary: keep one entry for each key`, and a text key and
a number key in one literal are the mixed-key error below. `{}` is an empty object literal, never a dictionary:
an empty dictionary is `Dictionary<T>()`.

**The key kind is decided while compiling.** Each dictionary is keyed by text or by whole numbers, never both, and nothing is written for
it: `Dictionary<T>` stays the one spelling.

- The keys the program gives a dictionary decide it: the key of `[]`, `[] =`, `set`, `get`, `has` and
  `remove`. A `String`, a symbol or an enum value is a text key (an enum value becomes its name); a
  `Tiny`, `Byte`, `Short`, `UnsignedShort`, `Integer`, `UnsignedInteger`, `Long` or `UnsignedLong` is a number
  key. A dictionary given no key at all is keyed by text.
- The kind follows the dictionary wherever it goes, like its value type: a local it is assigned to, a parameter it
  is passed to, an attribute that holds it, a function that returns it, a list of dictionaries, and a generic
  class it is handed to (`BinaryWriter<Shelf>`, `BinaryReader<Shelf>`). A key given anywhere along that path
  decides it for all of them.
- One dictionary given a text key and a number key is a compile error at the number key, naming the text key's
  place: `this dictionary is given a whole-number key here and a text key at <file>:<line> (in <class>.<function>):
  a dictionary is keyed by text or by whole numbers, never both, so give every key of it the same kind`, with the
  places that tied the two before the colon when they were given to different dictionaries (below;
  `diagnostics/mixed_dictionary_keys`).
- **Only the program's own flows decide it.** The descriptions
  the compiler writes for `to_debug()` (`Spite.Debug<T>` and `Spite.DebugInstance<T>`, one of each per type for
  the whole program) take a dictionary as the kind it already has and never tie it to another. Otherwise every
  `Dictionary<String>` attribute of every class described would pass through the one `Spite.Debug<Dictionary<String>>`
  and be tied to all the others, so a number key given to one would change the kind of an unrelated one, and
  removing code that made some class be described would change what compiled
  (`conformance/stage6/debug_dictionary_keys`).
  The same holds for the member templates a build compiles only so the prompt can call them (a `--repl`,
  `--repl-port` or `--hot-reload` build, [on the page](../docs/collections.md#how-the-member-templates-are-written)): `map_<members>` over a
  dictionary attribute appends it to the one `List<Dictionary<String>>` of the program, so each such template would
  tie every dictionary attribute of every listed class together, and a `--hot-reload` build of a large game would
  fail with a kind its `--optimized` build never had. A template kept for the prompt neither ties dictionaries nor
  gives one a key; one the program calls does both (`conformance/stage6/kept_templates`). The kinds are the same in
  every build of one program.
- **Two kinds that meet are named.** Where a dictionary of one kind is given where one of the other kind is
  wanted, the error says which is which and what decided each, rather than naming two `Dictionary<String>`s:
  `a Dictionary<String> keyed by whole numbers (Long), from the key at engine/columns.spite:47 (in
  Columns.name_of_header) cannot be used where a Dictionary<String> keyed by text, from the key at
  engine/recipes/cache_reader.spite:25 (in Recipes.CacheReader.fingerprint_of) is needed: a dictionary is keyed by
  text or by whole numbers, decided while compiling by the keys it is given, and these two were decided apart --
  give both the same kind of key, or copy the entries across one by one`. A kind nothing decided reads `text,
  since nothing gives it a whole-number key`. **A key that reaches a dictionary through others names the way it
  came**: when the deciding key was given to another dictionary tied to this one,
  the kind (here and in the mixed-keys error above) adds `, which reaches it through <file>:<line> (in
  <class>.<function>), ...`, each place that tied two of them (an assignment, an argument, a return), up to four
  and `and N more`. So a dictionary tied to an unrelated one by code the reader never wrote, such as a template kept
  for the prompt, says how the far key got there.
- The error that a dictionary never settles names the class and function that make it, not whichever function the
  compiler read last.
- **The kinds always settle, or it is an error.** They are found by compiling again with what the last pass
  learned, at most eight times; a program whose kinds are still changing then is a compile error at a dictionary
  that keeps changing, `the dictionary made here never settles on text or whole-number keys: each pass of the
  compiler decides it the other way (last as whole numbers, from the key at ...), since what decides it flows
  back into it; give it a key the program itself writes, of one kind`, never whatever the last pass compiled.
- A number-keyed dictionary's key type is the widest whole-number type any of its keys has (`Integer` keys and one
  `Long` key make a `Long`-keyed dictionary); a narrower key is widened as an argument is. `keys()` answers a
  `List` of that type, and `copy()` and `deep_copy()` are keyed the same way.
- A number key is stored and hashed as the number: no `String` is made for it, in a lookup or in the table.
  Everything else is as for text keys: insertion order, `null` for an absent key, `remove` moving later entries
  down, the member templates through the values (`conformance/stage6/number_keys`).
- Written as JSON, a number key is the number in quotes (`{"7":"SEVEN"}`), and reading JSON into a number-keyed
  dictionary reads each key's text as the number. Binary bytes carry the number as its type is written, and
  `schema()` counts the key's type in, so text-keyed and number-keyed dictionaries have different schemas.

It is a hash table over two ordered lists:
`get`, `has`, `set` and `[]` take the same time however many keys there are, and `remove` takes time in
proportion to the dictionary's size. An empty dictionary allocates no table.

The member templates, declared in `library/dictionary.spite`, and the passed-function forms work on a
`Dictionary<T>` through a copy of its values, as they do on a `List<T>`, all but `remove_where`: removing from
that copy would change nothing, so `remove_where_<member>()` and `remove_where(f)` on a dictionary are an error
naming `remove(key)`. To walk keys and values together, use `while` over `dictionary.keys()` and read
`dictionary[key]`.

`deep_copy()` works for classes, lists, dictionaries, unions and `type` shapes, and follows them all the way
down. A `String` is shared rather than duplicated because it is immutable; the rest of what it does, cycles
included, is [Memory's rule](memory.md#the-memory-model).

## Vector\<T\>

A `Vector<T>` holds its items inline, where a `List<T>` holds references, and reading an item gives a borrowed
reference into the vector. Its members are [the table
under `Vector<T>`](../docs/collections.md#vectort-items-inline), which is normative. The readings:

- **What an item may be.** A `String`, or a class whose attributes are only numbers, `Boolean`s, enums and
  `String`s (a `T?` of one of them, or a singleton such as `Console`, included). A number, a `Boolean`, an enum or a
  `Memory.Address` (or a `T?` of one) is not an item: a `List` holds them flat
  already, so `Vector<Integer>` is `'Vector<Integer>' holds plain values, and a List keeps numbers, Boolean, enums
  and Memory.Address flat just as it did, so a list of them has one spelling: write 'List<Integer>'`
  (`diagnostics/plain_items`), and `Items<Integer>` the same with `Items`. Anything else is an error when the vector
  type is written: `'Holder' cannot be an item of a Vector: its attribute 'scores' is a List<Integer>, and a
  Vector holds each item inline, so every attribute is a number, a Boolean, an enum or a String (keep 'Holder' in
  a 'List<Holder>', or keep 'scores' somewhere else)`; `Vector<List<Integer>>` is `a Vector holds each item
  inline, so an item is a number, a Boolean, an enum, a String or a class made only of those, and a List<Integer>
  is not: keep it in a 'List<List<Integer>>'`. A class with a `drop()` is `'Handle' cannot be an item of a Vector:
  it has a 'drop()', and an item inline in a Vector is never an object of its own to drop (keep 'Handle' in a
  'List<Handle>')`, and a class whose functions use `this` as a value (return it, pass it, keep it, or name one of
  its own functions as a value) is `'Selfish.itself' uses 'this' as a value, and an item of a Vector is borrowed
  from the Vector, never an object to keep or to pass on: read and write its attributes instead`.
- **Reading.** `vector[index]` and `get_at(index)` are one function and answer a `T?`: `null` out of range, and the
  item itself, borrowed, once the read is narrowed (by `crash`, `assert` or `if`, or by a proof the compiler already
  holds), exactly as for a list
  ([failure.md](../docs/failure.md#reading-with--answers-t)): `while index < velocities.count()` proves
  `velocities[index]` in the loop, and `crash velocities.count() >= 2` proves `velocities[0]` and `velocities[1]`.
  A proven read writes no check of its own; the read itself still compares the index once. A vector of `String`s
  answers a counted reference, as a list does. What a borrowed item may do, and
  what a row of them passed to a system may do, is
  [memory.md's rule](memory.md#borrowed-items-of-a-vectort).
- **Writing.** `append(value)` and `set_at(index, value)` (`vector[index] = value`) copy the value's attributes
  into the block, counting each `String` attribute once more; the value itself stays an ordinary object. A constructor
  cannot be the argument, so an appended item is made on its own line first: `var slow =
  Velocity(1.0, 0.5)`, then `velocities.append(slow)`.
- **Removing.** `remove_at(index)` releases the item's `String` attributes and moves every later item down, and
  halts on an index out of range; `clear()` releases every item's and keeps the capacity; dropping the vector
  releases them and frees the block.
- **Templates.** `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_`, `find_by_` and
  `sort_by_<member>()` are in `library/vector.spite`, written over the borrowed items; `filter_` answers a
  `Vector<T>` of copies and `map_` a `List` of the members' values. `find_by_<member>(value)` answers the first
  item whose member equals `value` as `vector[index]` does: a `T?`, the item itself, borrowed once narrowed, with
  every rule of a borrowed item ([memory.md](memory.md#borrowed-items-of-a-vectort)). `sort_by_<member>()` answers a
  new `Vector<T>` with a copy of every item, sorted as a list's is (stable, a merge sort, each key read once). A
  chain of them is one loop over the block, with `filter_` steps in the middle; a chain ending in `find_by_` or
  `sort_by_` on the items themselves runs step by step, which means the same.
  `parallel_each_<member>()` splits the walk across the thread pool as it does a list's.
  A program adds a template of its own by reopening `Vector` in a `vector.spite` of its folder, as for a
  [list](../docs/collections.md#write-your-own-member-template), reading each item with `values.item_at(items, index)`. Its body is
  checked like any other code of the program: an item it reads is borrowed, so it is not returned, kept or put in
  a list (`conformance/stage6/own_collection_templates`).
  No passed-function form is offered, since a `List` of numbers
  takes every form. A passed function would take a class item as an argument, which a borrowed item never is, so
  `velocities.each(f)` is `'each' passes each item to a function, and an item of a
  Vector is borrowed from it, never passed on: give the item's class a function and call it with a member
  template, such as 'each_<function>()'`; and a vector of `String`s, whose items are references to text, has
  `names.each(f)` as `'each' passes each item to a function, which a Vector never does: keep the values in a
  'List<String>' to pass them on` (`diagnostics/text_items_passed`).
- **Its allocator.** `velocities.memory.allocator = arena` on the next line places the `Vector` object in the
  arena, as for any object, and its block of items with it, each time it grows.

`diagnostics/vector_borrows`, `diagnostics/vector_items`, `diagnostics/plain_items`,
`diagnostics/text_items_passed`, `diagnostics/found_item_borrows`, `conformance/stage6/vector_items`,
`conformance/stage6/plain_items`, `conformance/stage6/vector_find_sort`.

## Items\<T\>

`Items<T>` chooses its storage while compiling, inline like a
`Vector<T>` when `T.is_fixed_size` and references like a `List<T>` otherwise, behind one set of members. Its
members are [the table under `Items<T>`](../docs/collections.md#itemst-the-storage-chosen-for-you), which is normative. The readings:

- **One class, folded.** `library/items.spite` is one generic class. Its storage is the same for both kinds (a
  heap block, a count and a capacity), and each body that touches an item folds on `$element_type.is_fixed_size`
  ([metaprogramming.md](../docs/metaprogramming.md#asking-whether-a-class-fits-a-vector)): the inline branch goes through
  `InlineMemory<T>`, the reference branch through `TypedMemory<T>`, and the branch not taken is not compiled. So a
  `T` that does not fit never makes a `Vector<T>`, never meets the item errors of
  [Vector](#vectort), and never has an item function written for it. Both helpers are singletons
  bound as attributes, so an `Items` object holds one pointer more than a `Vector` or a `List`, one per
  collection and never per item. No syntax is needed: the language already folds a body on `is_fixed_size`, and
  the storage needs no attribute of its own per kind.
- **Which kind.** `T` fits exactly when a `Vector<T>` could be made (a `Vector`'s rule: a `String`, or a class made
  only of numbers, `Boolean`s, enums and `String`s, with no `drop()` and no function that uses `this` as a value).
  A union, a `type`, a `List`, a `Dictionary` or a class holding one is kept by reference. `Items<String>` is a
  plain array of the text's references, as a vector's is. A number, a `Boolean`, an enum or a `Memory.Address` is
  never an item: `Items<Integer>` is the error naming `List<Integer>`, as for a `Vector`.
- **Reading.** `items[index]` and `get_at(index)` are one function and answer a `T?` for both kinds,
  `null` out of range. A read is
  narrowed as a list's is, by `crash`, `assert`, `if` or a proof the compiler holds. Then, inline, the item is
  borrowed, and every rule of [memory.md's borrowed items](memory.md#borrowed-items-of-a-vectort)
  applies to it, rows included. By reference, it is a counted reference like a list element: it may be kept,
  passed, returned and read after the collection changes size. Code reading an item still compiles the same
  whichever kind a class falls into, since both kinds answer `T?`, and a walked row states its reads with
  `crash` lines ([memory.md](../docs/memory.md#a-row-of-borrowed-items-for-one-call)).
- **Items of a `T?`.** `Items<String?>`'s `[]` answers the `String?` that was
  stored, and `crash names[0]`, `assert names[0]` or `if names[index] { }` narrows that item in place until the
  collection or the index changes, as a path is narrowed (`conformance/stage6/items_narrowed`). A loop's
  `index < names.count()` proves only that the index is in range, not that the item is there: reading
  `names[index].upper_case()` under it alone is still `this value may be null (it is a String?)`
  (`diagnostics/items_nullable_unproven`).
- **The borrow checks apply only where the storage is inline.** A class that changes kind can meet new errors
  where it is read, and each names the choice first: `'Velocity' fits a Vector, so the items of 'velocities' are
  borrowed: 'stored' is borrowed from 'velocities' and cannot be kept in the attribute 'kept': keep
  'stored.copy()', an independent object`, and the same for a return, an argument, a list, a second name, a
  function value, a row that is kept, and a read after a line that may change the size
  (`diagnostics/items_borrows`). What may change the size is what may change a `Vector`'s, with
  `remove_swapping` beside `remove_at`: called on the collection, or through a function that calls one, where a call
  that swaps an item out counts as a removal.
- **Writing and removing.** `append` and `set_at` copy the value's attributes in when inline (counting each
  `String` attribute once more) and keep the value when by reference; `remove_at`, `remove_swapping`, `clear` and
  dropping the collection release what they remove. `remove_swapping(index)` moves the last item into `index`
  and halts on an index out of range, as `remove_at` does; it keeps no order, and costs the same however many
  items there are.
- **Templates.** `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_`, `find_by_` and
  `sort_by_<member>()` are in `library/items.spite`, each folded the same way; `filter_` and `sort_by_` answer an
  `Items<T>` (copies when inline, the same references otherwise) and `map_` a `List` of the members' values.
  `find_by_<member>(value)` answers a `T?` read as `items[index]` is: inline, the item, borrowed once narrowed; by
  reference, a counted reference. A program's own template goes in an `items.spite` of its folder, folded on
  `$element_type.fits_vector()` as the library's are. A chain is one loop over the block,
  and `parallel_each_<member>()` splits it across the thread pool, as for a vector. The passed-function forms
  (`each(f)`, `filter(f)`, ...) are not offered for either kind, since an inline item is never passed on and plain
  values live in a `List`: `'each'
  passes each item to a function, and 'Velocity' fits a Vector, so an item of these Items is borrowed from them,
  never passed on: give the item's class a function and call it with a member template, such as
  'each_<function>()'`; by reference the error says that `Items` does not do it whichever storage it chose.
- **Cost.** Nothing runs to choose: the folds are decided while compiling and the untaken branch is absent from
  the C, so an `Items<Velocity>` compiles to a `Vector<Velocity>`'s code and an `Items<Trail>` to a
  `List<Trail>`'s, reading its elements uncounted in the templates as a list does
  ([optimizations.md](../docs/optimizations.md#a-lists-templates-read-its-elements-without-counting-them)). The range
  check of `[]` is one comparison that answers `null`, small enough for the C compiler to inline, and a proven
  read uses the item without testing that answer again. A program that makes no `Items` carries none of it. Measured in
  `benchmarks/items_storage`.

`conformance/stage6/items_columns`, `conformance/stage6/plain_items`, `conformance/stage6/vector_find_sort`,
`diagnostics/items_borrows`, `diagnostics/plain_items`, `diagnostics/text_items_passed`.

## Removing many at once

`List`, `Vector` and `Items` remove many elements in one pass:

- **`remove_where(test)` and `remove_where_<member>()`.** One template, `remove_where_member(member:
  Spite.AttributeDeclaration<$element_type>)` in each of `library/list.spite`, `vector.spite` and `items.spite`, so it answers both a
  member (`creatures.remove_where_dead()`: the member returns `Boolean`) and a passed function
  (`numbers.remove_where(is_odd)`: on a `List` of anything, never on a `Vector` or `Items`, whose items
  are borrowed or text, and a borrowed item is never passed on). It walks the collection once; an element that stays is
  exchanged with the first place not yet kept, and when the walk ends everything past the kept ones is released.
  The survivors keep their order, and the test sees every element once, first to last.
- **Why exchange, not move.** At every moment of the walk the collection holds each of its original elements
  exactly once, some of them already moved down: no slot is ever empty, a copy, or released while the collection
  still counts it. So whatever the test does to the collection (read it, append to it, even remove from it),
  nothing can be released twice or read after it was released; it only sees the elements in a different order.
  Moving an element down would leave its old slot holding a second copy that a test could release, or a blank that
  is no valid element. The exchange costs one more copy per element that stays, which measured about a third
  slower than moving at half removed and the same at nine in ten (`benchmarks/bulk_removal`).
- **`truncate(count)`** keeps the first `count` elements and releases the rest; a `count` below zero or not below
  the size does nothing. **`swap(first, second)`** exchanges two elements, doing nothing when either index is out
  of range; with `truncate` it is how a pass of the program's own removes by something other than a function of
  the element, such as a mask of rows.
- **Chains.** `remove_where` changes its receiver, so it is not a step of a fused chain, and calling it on
  a chain is an error: `'remove_where' removes from the collection it is called on, and 'names.filter(is_short)'
  makes a new one that nothing keeps: call it on the collection itself, with one test that says everything to
  remove`. A test returning anything but `Boolean` is `'remove_where(measure)': 'measure' is an Integer, but
  'remove_where' needs it to return Boolean`.
- **Borrows and proofs.** All three count as shrinking wherever `remove_at` does: on the collection itself, and
  through the call effects of functions (a function that calls one of them records `shrink:` on the collection). So an item
  borrowed from a `Vector` or `Items` is not read after one (`'first' is borrowed from 'velocities', and
  'velocities.truncate()' on line 34 may move the items of 'velocities', ...`), a row's system may not reach
  one for a vector it borrows from, and a proof about an element is undone by one. `swap` changes no size but
  changes which element an index names, so it counts too.
- **Cost.** Only what a program calls is compiled. `swap` is three copies of the element through a temporary on the
  C stack (`TypedMemory.swap_values`, `InlineMemory.swap_items`, written per element type by the generator), with
  no allocation. Measured at 200 000 `Items<Velocity>` items and a `List<Integer>` beside them: half removed,
  340 µs against 370 µs for a `remove_swapping` per removed row while walking the rows and 530 µs for one per
  entity of a despawn list; nine in ten removed, 290 µs against 490 µs and 930 µs.

`conformance/stage6/bulk_removal`, `diagnostics/bulk_removal`, `benchmarks/bulk_removal`.

## There is no `Heap<T>`

`Heap<T>`, a box for a value, does not exist (it is not `Memory.Heap`, the allocator). References are the default
([Memory](memory.md#the-memory-model)), so a class or union containing itself recursively just declares an
ordinary attribute of type `T`: no indirection is needed, and there is no struct-layout cycle to guard against,
since a class is always a heap object referred to by pointer. A `union Expression` of `NumberExpression` and
`BinaryExpression`, with `var left: Expression? = null` in `binary_expression.spite`, is the whole of it;
[memory.md](../docs/memory.md#self-referential-classes-and-unions-just-work) has a tree that runs.

Writing `Heap<T>`/`Heap<T>(value)` is a compile error naming this fix: `there is no 'Heap<T>': a class, a list and
a text are references already, so a class that holds one of its own kind declares an ordinary attribute, like
'var left: Expression? = null' ('Memory.Heap' is the allocator, for containers of your own)`
(`diagnostics/old_list_names`).

---

Next: [JSON and binary](json.md).
