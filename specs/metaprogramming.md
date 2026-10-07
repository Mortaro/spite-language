# Metaprogramming

The specification of [Metaprogramming](../docs/metaprogramming.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Templates

**A parameter of type `Spite.AttributeDeclaration<Owner>` whose name is a word of its function's name makes the
function a template** over `Owner`'s members: its attributes, and its functions that take no arguments.
`show_attribute(attribute: Spite.AttributeDeclaration<Monster>, monster: Monster)` answers `show_name(troll)` and `show_health(troll)`. A template is
instantiated only for the names a program calls, in every build; the template itself emits nothing, and each
instance is an ordinary function that costs what its body costs. `--final-classes` prints each instance.

- **The parameter is a declaration**: `attribute: Spite.Attribute<Monster>` is "'attribute: Spite.Attribute<...>'
  would be bound to one instance, and a template is made once for each member of the class, with the instance as
  its own argument: write 'attribute: Spite.AttributeDeclaration<...>'".
- **Inside an instance, the parameter is the constant declaration of that member**, a
  `Spite.AttributeDeclaration`; the bound attribute is `owner.attributes[attribute]`. Written as a type
  (`value: attribute.class`, `): attribute.class`) it is the member's real type; written as an expression,
  `attribute.class` is its `Spite.Class`, which prints as the type's name in `console.print` and in a text's `{}`.
  `attribute.name`, `attribute.camel_case_name` and `attribute.pascal_case_name` are text constants, and
  `attribute.index` is the member's place in declaration order.
- **`owner.attributes[attribute]` is that member of the `owner` passed in**: the field, read or written, or a call to
  the function. A class reads its own members by name, `attributes[attribute]`, never `this.attributes[attribute]`,
  and that is the field itself, read or written without its getter or setter, so a `get_` or `set_` template over
  its own class never calls itself.
- **`Owner` may be a codegen value**: `member: Spite.AttributeDeclaration<$element_type>` is how `library/list.spite` writes
  its member templates. When that value is not a class or a `type` (a number, a `T?`, a `List`), the template
  answers nothing.
- **`Owner` may be a `type`**: the template ranges over the attributes the type names and the functions it requires
  with no arguments, and `row.attributes[attribute]` is the shape's own read, write or call, answered from the
  value's class at run time like any read through a `type`.
- **An exact function always wins over a template, for that one name and direction**: an exact `set_name` answers a
  write of `name` while its read still goes through `get_attribute`. A generated `get_<attribute>()` of an owning
  attribute (a `String`, `List<T>` or `Dictionary<T>`) returns a retained, independent value, exactly as an explicit
  getter does.
- **A template that makes a function the class already has is an error** at that function's line: "the template
  'store_attribute' makes 'store_row' for the attribute 'row' of 'RowView', which is already a function of 'Row':
  rename the function or the template". An error raised in code the compiler wrote for a template names the
  template's line.
- **Reading a member counts as using it**: `x.attributes[attribute]` counts as a read of the member it reaches,
  while `attribute.name` and `attribute.class` describe it without reading it, so an attribute only they look at is
  the unread-attribute error ([Unused is an error](style.md#unused-is-an-error)).
- **A walk sees private attributes**, and may read and write them through the walked attribute; naming `_x` outside
  its class stays the private error.

**Plural and singular.** A template whose result is a collection of the member's values (`List<member.class>`)
takes the member in the plural, through `String.pluralize()`; every other template takes it singular. The template
declares which by its own name: `func map_members(member: Spite.AttributeDeclaration<$element_type>): List<member.class>`. The
compiler finds the member by inflecting every member name of the element's class and picking the one whose plural
is the word called, never by guessing from the call:

- one match is the member: `map_names` is `name`, `map_people` is `person`, `map_health` is `health`;
- the singular where the plural differs is "'map_name' answers every 'name', so it is written in the plural:
  'map_names'";
- two members with one plural is an error naming both and asking to rename one;
- a name `pluralize()` cannot inflect so that `singularize()` gives it back is an error naming the member and the
  fix: add the word to the irregulars table by reopening `String.Inflection`, or rename the member;
- a name `pluralize()` leaves as it is, already plural or uncountable, is its own plural: `map_name_with_namespaces`;
- a template that answers `List<member.class>` but names its member in the singular is an error naming the plural:
  "'map_member' answers every 'member' of the element, a 'List<member.class>', so its name holds the member in
  the plural: 'map_members'";
- a member that is a question (`is_alive`, `has_target`, `can_fly`) is never collected: `map_is_alive()` is an error
  naming `filter_is_alive()`, `count_is_alive()`, `any_is_alive()` and `all_is_alive()`.

**A member template chains through a member's own function.** `filter_<member>_<function>(arguments)` keeps the
items whose `member` answers `function(arguments)`: `players.filter_name_starts_with("a")`,
`functions.filter_name_ends_with("_each")`. It works on any list of any class, and a reflection object does not
answer its name's text functions itself.

**A list of a union** takes a member class in the plural as a filter that keeps that class and narrows the list to
it (`entries.filter_files()` answers a `List<File>`), and every other template takes it the same way, in the plural:
`count_files()` answers how many items are `File`s, `any_directories()` whether one is, `all_files()` whether every
one is (`true` for an empty list), and `remove_where_directories()` removes the `Directory` items in place; on a
`Dictionary`'s values, `remove_where_` by class is an error naming `keys()` and `remove(key)`
(`conformance/stage6/union_member_templates`). It templates an attribute every member class has as on any
list, the result typed from that common attribute. A name that matches both a member class and an attribute is a
compile error naming the fix.

**A `Dictionary` takes the same member templates as a `List`, over its values**: `accesses.filter_written()`.

**A question member is asked without its `is_`.** The templates that ask a question of each element (`filter_`,
`count_`, `any_`, `all_`, `remove_where_`) name a member `is_<word>` as `<word>` when the element has no member
`<word>` of its own: `accesses.filter_written()` keeps the accesses whose `is_written` is true, as
`monsters.filter_alive()` would for an `is_alive`. Naming it in full, `filter_is_written()`, is the same template.

**There is no plain `map(function)`, and a class has no function values.** A member's values are collected with
`map_<members>()`; a value computed from an element becomes a get-only attribute of the element's class and is
collected the same way. `Monster.is_alive` names nothing, since `Monster` is a `Spite.Class` object; a function value
belongs to an instance ([functions_and_operators.md](../docs/functions_and_operators.md#functions-are-values)).

## Walks

A walk is a call on a list of reflection objects, under [reflection.md's rules for constants](reflection.md#known-while-compiling):

- **Names are selected, never built.** `[]` takes a name written in the code and answers a `T?`; anything else is a
  filter over the names that exist. A text with holes is never a member name.
- **A walk over a constant list is unrolled**, one call per element in the list's order (declaration order for
  attributes and arguments, the order made for functions, an enum's order for its values, dotted-name order for
  classes and namespaces), and the function it calls is compiled once per element. A walk over an empty list calls
  nothing.
- **A question on a constant folds**: `.is_resumable`, `.is_mutated`, `.is_stateful`, `.is_fixed_size`,
  `.is_singleton`, the kind questions, `.count()` of a constant list, `[]` with a literal name, and `==` between
  constant classes. **`.is_resumable` cannot be asked of a function by itself**: its own answer would decide whether
  it waits, and that is an error.
- **`.is_mutated` follows every call**: an attribute set on what the argument was given, an item written or a list
  grown, a function called on it that changes its own object, or the argument handed to another function that does
  any of these, however deep, recursion included. Reading does not count, and neither does giving the argument's
  name a new object. A function whose body the compiler supplies counts as changing what it is handed unless it is
  known only to read, since an answer the study cannot prove is `true`. **`.is_stateful`** is the same study asked of the object: some function besides the constructor
  and `drop()` writes the class's own attributes or anything reached through them, or a singleton, or a singleton the
  class binds has state.
- **An empty walk is loud when it must not be**: `crash Runner.functions.filter_name_ends_with("_each").count() > 0`
  folds, and is the compile error below when it is false.

## Codegen values (`$`)

`$name` means "replaced at code generation". That is its only meaning, everywhere it appears.

**A class declares each codegen value on a `generic` line of its own, at the top of the file**:

```gdscript weapon.spite
generic $damage_type
generic $is_magic

var damage: $damage_type = 0

func Weapon(new_damage: $damage_type) {
    damage = new_damage
}

func hit(): Integer {
    if $is_magic {
        return 1
    }
    return 2
}
```

```gdscript
var sword = Weapon<Integer, true>(10)
```

- **Every value is declared, even a single one**, so skimming the top of a file is how a human sees what a class
  accepts. The lines come after a `singleton` line and before everything else; one line declares one value,
  and a comma is an error naming the line to write: "a 'generic' line declares one codegen value: write 'generic
  $left_type' and put the next one on its own line".
- **Call sites are positional, always, in the order of the lines.** There is no named form: `List<Integer>()`,
  `Weapon<Integer, true>(10)`.
- **`$` is for generics only.** Every `$name` a class uses has a `generic` line and is supplied by the caller, so
  the lines are exactly "what a caller must pass". A `$name` with no line is a compile error. Used as a value it
  points at [`Environment`](../docs/programs.md#run-time-settings-environment), which is where a program's settings
  live: "'Game' has no 'generic $verbose' line, and '$' is for generics only: a program's settings are fields of
  Environment, so ..."; used as a type it is "this class has no 'generic $other_type' line, so '$other_type' cannot
  be used as a type" (`diagnostics/codegen_values`).
- **Only a class has codegen values; there are no generic functions.** A function reads the `generic` lines of its
  class and declares none of its own; a function that must accept any class takes a `type` (the built-in empty
  `Anything` accepts every class), and `value.class` is the real class
  ([reflection.md](../docs/reflection.md#known-only-at-run-time)). Listing values on a function is the error below.
- **A class needs no constructor to take codegen values.** `library/list.spite` is `generic $element_type` and its
  functions, and `library/dictionary.spite` is `generic $value_type`; an empty constructor is still an error.
- **Every hole must be filled.** There are no defaults. A call supplying the wrong number is a compile error
  naming the class's codegen values, in order: "'Box' takes 1 codegen value(s), in this order: $held_type,but 2 were given", so the mistake is corrected from the message rather than by opening the class. A `$name`
  that is not declared is an error too, which is what a typo like `$is_magik` produces.
- Reordering the `generic` lines changes what every existing positional call site means. Where the values
  have different kinds (a class versus a `Boolean`) the compiler catches it immediately; where they are the same
  kind (`Pair<Integer, String>` swapped) it compiles and means something else, and the tests are what catch it. This
  is a deliberate, accepted trade.
- **Each set of values is its own class**, made only where a program names it, with its own copy of every
  function surviving code calls. Nothing chooses between them at run time; what a generic costs is that code,
  once per set of values used, and a generic nobody makes costs nothing. `--final-classes` prints each
  class with its codegen values already bound, which is where `true` reads as `is_magic` again.
- Writing `generics` is a parse error naming this form, and so is listing codegen values on a function,
  `func Weapon<$damage_type, $is_magic>(...)`, on a constructor or any other function: "a function no longer
  lists codegen values between '<' and '>': declare each at the top of the file on its own line ('generic
  $damage_type', 'generic $is_magic'), and write 'func Weapon(' here" (`diagnostics/constructor_codegen_list`).
  A `generic` line inside a function is an error too.
- **The values can be left out when the constructor's arguments say them**: each parameter whose declared type
  mentions a `$name` is
  matched against its argument's type (`$name` itself, `$name?` (a `T` or a `T?` both give `T`), `List<$name>`,
  `Dictionary<$name>`, and the arguments and return of a `Spite.Function<...>`). A `null` argument says nothing.
  Only when every `$name` is found; otherwise the error asks for them between `<` and `>`: "'Box' takes 1 codegen
  value(s), in this order: $held_type. Write them between < and > before the arguments". `Pair("Hero", 7)`,
  `Concurrent(file.read)`.
- `$name()` makes the default of what `$name` is bound to: a class through
  its constructor with no arguments, an empty `List` or `Dictionary`, `0`, `""`, and for a `type` whose members
  are all attributes a real object. `var held: $held_type = null` is an error, since `null` belongs to `T?`
  alone ([values_and_types.md](values_and_types.md#variables-and-values)), except on an attribute the
  constructor assigns and a local a walk fills. **Bound to a `type` that requires a function, `= null` is a
  compile error naming the attribute**, unless the class's constructor assigns the attribute: no object
  can supply the function, so there is no default to make. `'held' is a Weapon, a type that requires the function 'strike', so
  '= null' has no default to make: no object can supply a function it does not have. Make its type nullable, with
  a '?', and narrow it before use, or give it a real object in the constructor` (`diagnostics/function_shape_default`).

**A `generic` line may name a constraint.**
`generic $item_type: Printable` accepts only types that fit the `type` `Printable`, and a use that does not
(`Shelf<Pet>`) is an error where the class is named, naming the constraint, the class and what it lacks:
"'Pet' does not fit type 'Printable', which 'Shelf' requires of $item_type: it has no function 'to_string'",
instead of an error deep inside the generic's body. The constraint is optional and uses an existing `type`,
resolved like any type name from the generic's file. In detail:

- **Fitting is what [Types](values_and_types.md#types) already means by it**: the class a `type` would admit
  fits: a class with the listed functions and attributes, `String`, a number, an enum, a `List<T>` or
  `Dictionary<T>` with what the `type` needs, and the `type` itself. A `T?` does not fit ("'Integer?' does not fit
  type 'Printable', which 'Shelf' requires of $item_type: it may be null, and null has none of what the type
  needs"), and neither does a function value.
- **A constraint names a `type`, not a class, a union or a number type**: anything else is an error on the
  `generic` line: "'$count_type' is constrained by 'Integer', which is not a 'type': a constraint names a
  'type' the class given must fit, like 'generic $count_type: Printable'".
- **It is checked once per class given**, where the compiler first makes that instance, written out or read
  from the constructor's arguments. After the error the generic is compiled as if it had been given the `type`
  itself, so its body adds no errors of its own.
- **It costs nothing at run time and tree-shakes as before**: the check is the compiler's alone, and the
  instance made is the one an unconstrained line would make.

`conformance/stage6/generic_constraints`, `diagnostics/generic_constraints`,
`diagnostics/generic_constraint_not_a_type`.

**Conditions on codegen values fold.** They are decided at compile time and the untaken branch is removed (tree
shaking), in every build, `--development`, `--hot-reload` and the REPL builds included: a codegen value is part of
which class this is (`Weapon<Integer, true>` and `Weapon<Integer, false>` are two classes), so there is no
run-time value for live reload to change. To change one, change the call site.

**An `assert` or `crash` on such a condition folds the same way.** When the condition of an `assert` or `crash`
asks only what is decided while compiling (a `$flag`, `$slot_type == Entity` or any other test of a codegen type,
a question asked of a reflection constant (`$T.functions['run_each']`, `.is_resumable`, `.is_mutated`,
`.is_fixed_size`, `.arguments.count()`, of a codegen type or of a walked member's class), `argument.class == $row_type`,
a class test the value's type already answers (`item == $wanted_type`), a field of
[`Build`](../docs/programs.md#compile-time-settings-build), and `not`, `and` and `or` over them), it is decided
for each instance, with no test at run time: when it holds,
nothing is written for it; when it does not, the `assert` answers "nothing" (recording its trace line), and the
statements after it in the same block are not compiled for that instance, exactly as after an `if` whose taken
branch returns. A folded `assert` is held to the same rule as any other: it is allowed only in a function whose
result can say "nothing" (a `T?`, `Nothing`, or a collection), so `assert $slot_type == Entity` followed by
`return value.id` is written in a function returning `Integer?`; in one returning `Integer`, the answer for the
other instances is written down instead, `if $slot_type != Entity { return 0 }`, which folds the same way
(`conformance/stage6/folded_checks`, [failure.md](../docs/failure.md#a-default-that-looks-like-an-answer-is-an-error)).

**A `crash` that folds to false is a compile error where the program can reach it.** A `crash` whose condition is
decided while compiling and is false would halt every time its function runs, so it is a developer's mistake the
compiler can prove, and it is reported while compiling, at the `crash`, naming the instance: `'crash
$slot_type.is_fixed_size' always halts in Slot<List<String>>: its condition is decided while compiling and is
false, so the program would stop here every time this function runs: call it only where the condition holds, or
change what the condition asks` (`diagnostics/folded_crash`, and `diagnostics/folded_build_crash` for a `Build`
field and a class test). Only a function the program reaches counts (the
same reach tree shaking keeps in a production build, worked out for an inspectable build too, where nothing is
shaken), so an instance whose function nobody calls compiles (`conformance/stage6/folded_crash_uncalled`). This
is how a library turns its rules into compile errors: a `crash $system_type.functions['update_each']` in the
function that runs a system fails the build for the class that breaks the rule, not the run. The sides of an `and`
are checked one at a time, as two `crash` lines would be, so in `crash $system_type.functions['update_each'] and
ready` the first side folds on its own and is this compile error when it is false, whatever `ready` holds; only a
condition that mixes a run-time value in through `or` (or `not` over an `and`) is tested at run time as a whole.

**A class test that can never be true for one instantiation folds to `false`.** Inside
a generic class, `if item == Health { }` where `item`'s type comes from a codegen value that is not `Health` in
this copy is `false`, and its branch is removed from that copy only, since generic code that a particular copy does
not use is what tree shaking removes, so it is on purpose not an error. Outside generics the never-true test stays
an error ([control_flow.md](control_flow.md#control-flow-in-full)). A codegen value bound to a class is itself
a class test on the right of `==`, `item == $wanted_type`
([values_and_types.md](values_and_types.md#unions-in-full)).

**Asking what type a generic was given.** When a codegen value is a type, `$value_type == String` is decided at
compile time like any other condition on a codegen value, and only the branch taken is compiled, so each branch
may use what only that type has, which is what lets one generic class treat text, numbers, lists and classes
differently. It is the class test ([control_flow.md](../docs/control_flow.md#value--class)) asked of a type instead of a
value. A type name asks for exactly that type; four names ask for a kind, since the type has arguments the test
does not want to spell:

| Test | True when the type is |
|---|---|
| `$value_type == List` | any `List<T>` |
| `$value_type == Dictionary` | any `Dictionary<T>` |
| `$value_type == Null` | any `T?` (`Null` is a member of the union a `T?` is) |
| `$value_type == Symbol` | an enum, or `Symbol` (an enum is a closed list of symbols) |
| `$value_type == Number` | any number class, `Tiny` to `Double` (a `type` name asks whether the type fits it) |
| `$value_type == Enum` | an enum only, not a plain `Symbol` (`JsonReader` and `BinaryFormat` need it to read a plain `Symbol` through `Symbol(text)` and an enum through the text cast) |

A union name is true for any of its members, and a `type` name for any type that fits it: `$value_type ==
Number` is true for every number class ([values_and_types.md](../docs/values_and_types.md#every-number-fits-number)).
Such a test always folds, `--development` included, because the
branch it rules out would not compile.

**Only what survives folding is compiled.** A function of a generic class is
compiled, and so type-checked, for one instantiation only when code already compiled for the program names it (a
call that survived folding, a function value, a reflection table, or a call through a `type` the class is a
member of), so a helper reached only from a branch the instantiation rules out is never checked against that
type. When a folded `if` (or `else if` chain) takes a branch that ends in `return`, the statements after it in the
same block are not compiled either, and the names they read count as used, as for the untaken branch. This holds
in every build, inspectable ones included. A class that is not generic still compiles every function, so a
mistake in an uncalled one is still reported. `conformance/stage6/folded_helpers`.

**The types a type was built from are read by their codegen names**: `$value_type.element_type` for a
`List<$element_type>`, `$value_type.value_type` for a `Dictionary<$value_type>` or a `$value_type?`, and a generic
class's own names for one of its instances, the names this section already gives the containers. Reading a name
the type does not have is an error listing them: "a List<Integer> has no codegen value named '$value_type': List<$element_type>, Dictionary<$value_type> and $value_type? name theirs, and a generic class names its own"
(`diagnostics/every_attribute`). `conformance/stage6/every_attribute`.
Compared in a condition, a name read this way folds like `$value_type` itself: `if $list_type.element_type ==
Float`, `else if $map_type.key_type == String`, `$holder_type.held_type != Item`, to any depth and in every
branch of an `else if` chain, so a branch that does not fit the instantiation is not compiled
(`conformance/stage6/codegen_member_fold`). An `and` whose left side folds to
`false`, or an `or` whose left folds to `true`, folds without its right side, which may then ask what the type does
not have: `$list_type.element_type == List and $list_type.element_type.element_type == Float`.

**A codegen value that is a type reads as its class**: a member
read through it, `$component_type.name` or `$component_type.attributes`, is read from the bound class's
`Spite.Class`, exactly as `Health.name` is, and no value of the type is made. `$component_type` alone in an
expression is still the "is a type here" error (`conformance/stage6/codegen_class_name`).

The `generic` line is a declaration like `var`, and follows a header pattern shared with `singleton`.

---

Next: [Reflection](reflection.md).
