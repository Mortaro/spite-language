# Classes and files

Every `.spite` file is exactly one class, named by PascalCasing the file name: `person.spite` is `Person`. This
cannot be changed -- there is no `class` keyword and no way to put two classes in one file.

A file contains only declarations: an optional `generics` line (first line, see
[metaprogramming.md](metaprogramming.md)), `var` fields, `func`s, and namespaced `type`/`enum`/`union`
declarations. **No file-level statements** -- no loop, `if`, or call outside a function body:

```spite title=file_scope_error/file_scope_error.spite entry error
var console = Console()

console.print("not allowed")

func FileScopeError() {
    console.print("a statement inside a function is fine")
}
```
```diagnostic
a file holds declarations and nothing else
```

## Constructors

A function named exactly like its class is the constructor:

```spite title=person_basics/person.spite
var age = 0

func Person(new_age: Int) {
    age = new_age
}
```
```spite title=person_basics/person_basics.spite entry
var console = Console()

func PersonBasics() {
    var person = Person(30)
    console.print("age", person.age)
}
```
```output
age 30
```

A class with no function matching its own name gets an implicit no-argument constructor that just runs the
field defaults -- there is no need to write an empty one yourself:

```spite title=team_roster/member.spite
var name = "unnamed"
var level = 1
```
```spite title=team_roster/team_roster.spite entry
var console = Console()

func TeamRoster() {
    var members = List<Member>()
    members.append(Member())
    members.append(Member())
    console.print("count", members.count())
    console.print("first name", members[0].name, "level", members[0].level)
}
```
```output
count 2
first name unnamed level 1
```

Every value has a default: `Int` is `0`, `Float` `0.0`, `Bool` `false`, `String` `""`, and a class's default is
its fields' defaults. There is no `null` for anything but a `T?` (see
[values_and_types.md](values_and_types.md)).

## Classes are references

A scalar (`Int`, `Float`, `Bool`, an enum value) is passed by value, copied at the call site. Everything else --
a class instance, `List<T>`, `Dictionary<T>`, `String`, a union, an object literal -- is a reference: passing one
shares the exact same object, so a function can mutate it and the caller sees the change. There is nothing
special to write at the call site or in the parameter's own type; a reference is just the default:

```spite title=level_up/member.spite
var name = "unnamed"
var level = 1

func Member(new_name: String) {
    name = new_name
}
```
```spite title=level_up/level_up.spite entry
var console = Console()

func LevelUp() {
    var hero = Member("Aria")
    bump_level(hero)
    console.print("name", hero.name, "level", hero.level)
}

func bump_level(member: Member) {
    member.level = member.level + 1
}
```
```output
name Aria level 2
```

Sharing is visible both ways: two names can hold the same object, and a change through either one shows up
through the other. Call `copy()` for an independent object with the same field values (see
[memory.md](memory.md)) when that sharing is not what you want:

```spite title=shared_member/member.spite
var name = "unnamed"
var level = 1
```
```spite title=shared_member/shared_member.spite entry
var console = Console()

func SharedMember() {
    var original = Member()
    var alias = original
    var independent = original.copy()
    alias.level = 5
    independent.level = 9
    console.print("original level", original.level)
    console.print("independent level", independent.level)
}
```
```output
original level 5
independent level 9
```

## The entry point and configuration files

There is no `main`: a program runs by constructing the entry file's class. `Kal()` is the whole program if
`kal.spite`'s constructor is the only thing that does anything. A configuration file is just an ordinary class
whose constructor sets state instead of printing anything -- there is no separate "config" concept.

Multiple files sit next to each other as siblings; only files directly in the entry file's own folder are
classes (non-recursive) unless pulled in with `load(...)` (see [packages.md](packages.md), which also covers
namespacing files under sub-folders).
