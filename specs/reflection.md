# Reflection

The specification of [Reflection](../docs/reflection.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Reflection objects

**Every class is an instance of `Spite.Class`**, every namespace of `Spite.Namespace`, every function of
`Spite.Function`, and every attribute, argument and access of `Spite.Attribute`, `Spite.Argument` and
`Spite.Access`. They live in the `Spite` namespace (`Spite.Class`, not `SpiteClass`), so nothing in it can be
mistaken for a class of a program. `Spite` is a reserved root namespace: a program's `spite/` folder may only
reopen its classes, and `load "spite"` is an error ([packages.md](../docs/packages.md#the-spite-namespace-is-reserved)).
Their members are the tables of [The `Spite` classes](../docs/reflection.md#the-spite-classes).

**Every member is a get-only attribute named for what it answers**, read without `()`: a question is `is_...`, a
collection is plural. A question that takes something is a collection indexed or filtered instead
(`Monster.functions['alive']`, `hurt.arguments[1].is_mutated`). What acts rather than answers stays a function:
`call_function()`, a `Spite.Call`'s `call()`, and a list's own `count()`. A name is a `Symbol`: text from the program's own
table of symbols, which reads as text anywhere text is expected and costs no allocation to pass around.

**Every member is a getter with no setter**: the data sits in private fields and `get_name()` and the rest answer
it, so `described.name = ...` is "'name' is read-only". **A name starting with `_` is private** (everywhere but
on a parameter, where `_` means unused on purpose; [Lexical structure](classes_and_files.md#lexical-structure)): it
is read, written or called only inside its own class (a reopening is inside), and from anywhere else it is an error
naming the getter when there is one: "'_name' is private to 'Class': a name starting with '_' is used only inside
its own class, and 'name' reads it from outside". The members only the compiler can write (`call_function()`, the
value of a bound attribute) are the compiler's reopening of those classes, printed by `--final-classes` like any
member.

**What `.functions` contains**: the functions a class declares, plus the template instances generated for it,
because those are functions of the class in the program as built; reflection describes the program that exists,
not the source as written. **The list holds every function the class has once compiling ends**, in the order they
were made, so it does not depend on the order classes were compiled in. **A constructor is not one of them**: it does
not answer on an instance, it makes one. A class of the standard library describes its members too:
`Memory.Heap.functions` lists `allocate`, `resize`, `free` and `live_allocations`, the functions whose bodies the
compiler supplies.

**`.class` is a real `Spite.Class`, not the class's name as text**, and so are `Spite.Attribute.class` and
`Spite.Argument.class`: `function.arguments[0].class == ServerContext` is an identity comparison the compiler
checks, where a test on the name's text quietly turns false when a class is renamed. Printing a `Spite.Class`
prints its `.name`, and printing a `Spite.Namespace` its dotted name.

**A namespace is an instance of `Spite.Namespace`**: the tree of folders made readable, `.parent` walking up,
`.classes` and `.namespaces` walking down. `.namespace` and `.parent` are `Spite.Namespace?`, and the chain ends
at `null`; there is no root object. `Spite.Namespace.instances` is every namespace of the program's own classes,
parents first, in the order the classes were found. `.enums` is the enums declared in the files directly in the
namespace's folder, each a `Spite.Class`, in the order those files were loaded; an enum at the root of a program
is in no namespace's list. **A class and a namespace with the same dotted name is a compile
error.**

**`.source_files` and `.source_directories` are where a class and a namespace come from**: a `File` for every file
that declares or reopens the class, and a `Directory` for every directory a namespace's classes come from, each in
load order. A question about a path is asked of those `File` and `Directory` values; reflection has no other path
member. They are build-machine paths: a shipped program that needs the files beside a package copies them into its
output ([packages.md](../docs/packages.md#files-beside-a-packages-source)).

**`value.memory` is where a named value lives**: a class instance, a list or a dictionary answers with its object,
a `String` with its characters (`'constant'` for a literal), and a number held in a local with the local itself
(`'stack'`) or in an attribute with that attribute (`'heap'`). Only a named value has an address: a number computed
on the spot is "only a named value has memory of its own: give this value a name with 'var' first".

## A class and an instance

- **`Monster.attributes` and `Monster.functions` are declarations, `Spite.AttributeDeclaration` and
  `Spite.FunctionDeclaration`; `troll.attributes` and `troll.functions` are the same members bound to `troll`,
  `Spite.Attribute` and `Spite.Function`.** A class's members are information and an instance's are data: a
  declaration never gives a value. A PascalCase receiver is the class and a lowercase one an instance, the
  convention [Foreign libraries](foreign_libraries.md#foreign-libraries) uses for `user32.Input` against
  `user32.input_mouse`.
- **A function walked over, or handed, one kind takes that kind.** A declaration given to a `Spite.Attribute`
  parameter is "'describe' takes a 'Spite.Attribute', an attribute bound to an instance, and 'health' here is a
  declaration of 'Monster', which has no value: declare the parameter 'Spite.AttributeDeclaration', or walk an
  instance's attributes", and a bound attribute given to a `Spite.AttributeDeclaration` parameter is the error
  naming `Spite.Attribute` the other way round.
- **A bound attribute's `.value`** reads and writes the instance's field, typed as the field when the attribute is a
  constant. `.value` of an attribute that describes a declaration is "'attribute.value' reads an attribute of an
  instance, and 'attribute' describes a declaration: walk the instance's own attributes, 'monster.attributes', or
  read 'monster.attributes[attribute]'".
- **`value.attributes[attribute]`**, with a declaration, is that field of `value`, read or written: a plain typed
  field access when `attribute` is a constant, and a call to the getter where the class declares one.
- **A bound function** is a function value of that instance ([functions_and_operators.md](../docs/functions_and_operators.md#functions-are-values)),
  so a function named on an instance and its reflection are the same object. Calling it always calls.
- **`Spite.Call(declaration, instance)` builds a call without running it**, the only way to build one: the
  declaration is a `Spite.FunctionDeclaration`, and `instance` must be of the class that declares it.
  `.arguments['name'] = value`
  fills each argument once, and `call()` runs it. With the function and the arguments known while compiling it is
  the plain direct call, and no object exists. An argument left unfilled is "'call' leaves the argument 'velocity'
  of 'run_each' unfilled: set 'call.arguments['velocity']' before 'call()'" where the compiler sees it; otherwise
  `call()` halts naming it. An instance of another class is "'rock' is a 'Rock', and 'hurt' is declared by
  'Monster': a call runs a declaration on an instance of its own class".
- **A walk over another class's attributes sees its private ones**, since a walk that skips some silently builds an
  incomplete copy, column or layout: `.attributes` lists them in declaration order with the rest, and
  `value.attributes[attribute]` and a bound attribute's `.value` read and write them. Naming `_x` outside its class
  stays the private error. A serializer and `to_debug()` leave them out by testing `attribute.name`, which folds.
- **A number's, a `Boolean`'s and a `String`'s `.attributes` is empty**: the compiler stores those values itself,
  so the fields their library classes declare to say so are not attributes a walk could read or write.
- **A `List<T>`'s or `Dictionary<T>`'s attributes are its entries** (named by index or by key), not the fields of the
  class that stores them, so a class object's `.attributes` is empty for a collection.

## Reaching a class

- **A class name read as a value is its `Spite.Class`**, member by member, with no instance anywhere:
  `Weapon.name` is `Weapon`, and `Weapon.namespace` is the same namespace `weapon.class.namespace` gives. A function
  of the class described called on its name (`Weapon.debug()`) is "'Weapon' is a class, not a value: write
  'Weapon()' to make one, and give it a name with 'var ... = '", since there are no static functions.
- **A class name where a `Spite.Class` is wanted is its class object**: an argument, `var kind: Spite.Class =
  Health`, an assignment to such a name and a `return` from a function returning one; a codegen value bound to a
  class passes the same way. Anywhere else (`var kind = Health`, an argument whose parameter is a `type`) a bare
  class name is "'Health' is a class, not a value", naming the `Spite.Class` form.
- **On the right of `==` a class name is a class test, except when the left side is itself a `Spite.Class`**: then
  the two class objects are compared, so `component.class == component_class` and `kind == Health` both mean "the
  same class".
- **`class` is an inherited attribute of every instance, not a keyword**: inside a function it is the class the
  function answers on. A local or an attribute named `class` takes precedence (that is how `Spite.Attribute.class`
  reads its own field), and `class ClassKeyword {` at file scope fails at 1:1 with "there is no 'class' keyword",
  because a file is a class named after the file. **`namespace`** is the same kind of name for the class's
  namespace.
- **`.class` through a `type` or a union answers the real class**, from the object's own tag at run time; an
  object literal answers `Object`.
- **`Person.instances`** lists every live instance of a class; only a class whose `.instances` some code reads is
  tracked. A class object's `.attributes` are its declarations, made with no object; its `.functions` are read
  from a stand-in at its defaults, made without
  running any constructor, which is not one of `.instances`; a singleton's stand-in never runs its `drop()`. The
  program's entry class has no stand-in: its `.functions` is empty. A stand-in of `Concurrent<T>` or `Parallel<T>`
  is finished and joins nothing. A class object and a namespace object are each made once, whichever thread asks
  first, behind one lock that a program without threads does not carry.
- **`Spite.Class.instances` is every class in the program**: the classes of the program's own folder and of every
  package it loads, not the standard library's (`Launcher` included, [programs.md](../docs/programs.md)), not the anonymous
  classes behind object literals, and not a generic class before its codegen values are supplied.

## Known while compiling

- **A constant** is a reflection object the compiler can identify: a class, enum or namespace named in the code (a
  name that is both an enum and a class is the enum), `$T`, `class` and `namespace` inside a function; `value.class`,
  `value.attributes` and `value.functions` when `value` is a local, parameter or attribute whose declared type is a
  class (not a `type`, a union, an optional or a reflection class); any member of a constant; `[]` on a constant
  list with a literal name or number; `count()`, `is_empty()`, `first()`, `last()`, `filter(function)` and the member
  templates on a constant list; a text function on a constant name; `==`, `!=`, `and`, `or` and `not` of constants;
  and a `var` initialised with a constant and never assigned again, when the function never needs its value at run
  time, `null` from a `[]` that finds nothing included. `filter(function)` asks a function whose body is one `return`, with its parameter bound to each element.
- **What folds.** A question on a constant is `true` or `false` in the generated code, a count is a number and a
  name is a literal. An `if`, `assert` or `crash` on a constant keeps only what runs, and one that always holds is
  not an error. `[]` with a literal name answers a constant `T?`: present, a narrowing `if`, `assert` or `crash` on it
  folds to nothing and still narrows that path for what follows; absent, its branch is never compiled.
- **`each(function)` over a constant list is unrolled** when the function is one of the class's own, takes one
  parameter, and every element fits it: one call per element, and no list.
- **A function called with a constant for a reflection parameter is compiled once for it**, whether it is the
  calling class's own or one called on another object (`helper.describe(attribute)`), a
  *specialisation* named `<function>_for_<member>` in `--final-classes` (a number is added when two would share a
  name). Inside it the parameter is the constant: `attribute.class` and `argument.class` are types, `part()` makes
  an instance of a constant class `part`, `monster.attributes[attribute]` reads and writes the field, and a function
  whose result is typed by its reflection parameter (`func made(argument: Spite.Argument): argument.class`) answers
  that type. A specialisation is not one of the class's `.functions`; the function itself is compiled only when
  something calls it with a run-time object or uses it as a value.
- **A run-time object** (a value typed `Anything` or a union, a reflection object kept in an attribute, anything
  typed at the REPL) is answered from the run-time tables, built only for the classes such a read can reach, and for
  every class in a `--repl`, `--repl-port` or `--development` build. A run-time function's `.accesses` comes from
  a table of every function the program makes an object for, emitted only when some code reads `.accesses` at run
  time. A standard library function, a function value taken through a `type` and every function of a `--hot-reload`
  build are not in it, and asking them halts. A run-time object answers the questions a constant does
  (`.is_stateful`, `.is_list`, `.index`, `argument.is_mutated` and the rest); the answers to the kind questions,
  `.is_mutated` and `.returned_literal` are written into the tables only when some code asks one of them at run
  time, and `.returned_literal` of a function whose body is not one `return` of a text literal halts. A run-time
  `attribute.value` is `Anything?`, a number
  held with its class's tag and text boxed. Handing one to a function whose result is typed by its reflection parameter is the error shown
  [on the page](../docs/reflection.md#known-only-at-run-time); naming a type through a reflection object that is not a constant is "'<path>'
  is not a class known while compiling, so it cannot be a type here"; and using a specialisation's parameter as a
  run-time value is "'<name>' is a reflection object known while compiling, so this function is compiled once for
  it, and it has no run-time value to pass on here".
- **Reflection may be as detailed as it likes, because what is not used is not emitted.** Which parts of it a
  program touches is decidable while compiling, so tree shaking is exact: a program keeps the tables only for what
  it reads at run time, and a program that only asks constants keeps none.

## The names reflection gives every object

**`class`, `attributes`, `functions`, `instances` and `memory` cannot name an attribute or a function of a class.**
Neither can a `get_` or `set_` function whose member would be one of them (`get_attributes`). A class member of the
same name would be read in place of reflection's, silently: a class with `var attributes = Table.Attributes()` would
make every `JsonReader` of it read each key as one the class does not have. The error is

`the attribute 'attributes' has a name reflection gives every object: 'value.attributes' lists its attributes, and
JsonReader, JsonWriter and every attribute walk read through it, and one of its own would hide it.
Name it something else, like 'table_attributes'`

with the suggestion made from the class's own name, and for an accessor `the function 'get_attributes' is read as
'attributes', a name reflection gives every object: ...`. The classes of the `Spite` namespace are exempt: they are
reflection. Locals and parameters may use the names. A class of the standard library never declares one.

## Functions of `Spite.Class`, and why there are no static functions

**Spite has no static class functions.** A class is an instance of `Spite.Class`, an ordinary standard library
class with an ordinary declaration, so a member that belongs to the class rather than to its instances is a member
of that object, and `Spite.Class` is where it is declared, with its default. `library/spite/class.spite` declares
`.is_singleton` that way:

```gdscript
var _singleton = false

func get_is_singleton(): Boolean {
    return _singleton
}
```

**A class file cannot replace a function of its class object.** A class's name is a constant naming an instance
of `Spite.Class`, as any constant names a value, so `Gadget.has_function(name)` is a call on that instance and runs
`Spite.Class`'s function. Every function a class file declares belongs to the class's instances, whatever its name:
a `to_string()` in `gadget.spite` is how a gadget prints, and `Gadget.to_string()` still answers what
`Spite.Class` says. A singleton says what it is with its `singleton` line
([classes_and_files.md](../docs/classes_and_files.md#singletons)), and declaring `is_singleton` in a class is "a class says
it is a singleton with a 'singleton' line at the top of its file, not with a function: delete 'is_singleton()' and
write 'singleton' as the file's first line".

- **`Spite.Class` is reopenable, like any other standard library class, and that is the only way to change what a
  class object answers.** Reopening it changes a function or a default of every class object for the **whole
  program**, and adding a member gives every class object that member. Nothing about this is silent: a changed
  default shows per class in `--final-classes`, with the root it came from. The compiler has no warnings
  ([Style](style.md#style)), so visible generated output is the whole mitigation.

**Why a member and not a declaration**: it is the same philosophy as a configuration file being a class that gets
reopened, rather than a static file. A static line can only state a value; a function can compute one, and it costs
the language nothing because functions already exist.

---

Next: [Packages, namespaces, and mods](packages.md).
