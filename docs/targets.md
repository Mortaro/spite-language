# Targets, the web and isomorphic classes

What is decided about compiling one Spite program for more than one place: platform code chosen by
reopening, a class compiled into a server bundle and a browser bundle at once, the browser reached as a
library, markup without a second grammar, and the binary format the two bundles talk in. None of it is built
yet. These are the normative rules, moved whole from the language manual when [D193](decisions.md) dissolved
it; a `D` number is a row of the [decision log](decisions.md). What runs today on one machine is on
[foreign_libraries.md](foreign_libraries.md) and [programs.md](programs.md).

Every piece of it is chosen while compiling, so a bundle carries only what its target uses
([D177](decisions.md)): the losing platform's code is folded away before tree shaking, the client bundle holds no
server body, the wire has nothing to parse, and a web build ships no scheduler of its own -- hidden waiting is
compile-time state machines ([D176](decisions.md), [concurrency.md](concurrency.md)), and in a browser the event
loop they return to is the browser's. That last point is why D176 rejected fibers: they would need a stack per
fiber and stack switching that WebAssembly cannot do without a whole-program transform that bloats the binary.

## Other environments  **[planned]**

The same mechanism is how Spite reaches anything that is not C. Nothing here is new machinery: it is [Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)'s
loading and reopening, plus a `Build` field folded while compiling.

- **The target is a `Build` field**, set by a `--target=native|web` flag and folded as a constant like every
  compiler option ([Build settings](programs.md#build-settings-build--implemented)). D76 ended the `$target`
  program variable (`$` is for generics only) and D85 made compiler options fields of `Build`; the field's name is
  not decided, and `build.target` below is a placeholder (proposed by Claude, unconfirmed).
- **Platform code is chosen by reopening, not by an abstraction layer.** `platform_web/console.spite` and
  `platform_native/console.spite` reopen the same `Console`; the entry constructor does
  `if build.target == "web" { load "platform_web" } else { load "platform_native" }`, and the compiler folds the
  condition and loads only the taken branch, so the loser is never compiled at all. This is the game-mod mechanism
  from [Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial) aimed at
  platforms, and it is how operating systems are chosen today: `load "library/{build.target_operating_system}"`
  and a `load` under an `if` on a `Build` field are both folded while compiling
  ([foreign_libraries.md](foreign_libraries.md#each-operating-system-reopens-what-it-changes)).
- **The web bridge is `DynamicLibrary` reopened.** `platform_web/dynamic_library.spite` replaces `_open`/`_call`
  with a JavaScript module's exports; a foreign object that cannot cross into wasm is a handle into a JS-side
  table, freed by the same `drop()`. A class written against a library compiles for both targets unchanged.
- The C backend emits wasm through a C compiler targeting `wasm32-freestanding`, **plus the JavaScript glue module**,
  generated from the same declarations -- hand-written glue is what makes wasm painful everywhere else, and this
  compiler already generates C.
- Which classes exist on which target is not a special rule: a class declares it (see [Targets](#targets--planned)),
  so `File`, `Directory` and `Process` on the web are a compile error like any other class used off its target.
- The isomorphic direction (`environment.name` of `server` or `client` in `examples/arsenal`) is what the next section
  describes: a class compiled into both bundles, whose server-only functions become a network call on the client
  and a route registration on the server.

## Isomorphic classes: where a function lives  **[planned]**

D13 (decided by Mortaro, 2026-09-19): **a function's first parameter decides which bundle it is compiled into.**
There is no annotation, no naming convention and no compiler trick -- the framework reads it off the signature
with ordinary compile-time reflection ([Reflection objects](reflection.md#reflection-objects--partial)).

```user_repository.spite
func find_user_by_id(context: ServerContext, id: Integer): User { }

func load_users(context: ClientContext) { }

func format_name(first_name: String, last_name: String): String { }
```

| First parameter | Compiled into |
|---|---|
| `ServerContext` | the server bundle only; the client gets a generated stub that calls it over the network |
| `ClientContext` | the client bundle only |
| neither | both -- isomorphic |

- **The marker is the capability.** A server function needs a server context (database handle, request, session)
  and a client function needs the client one, so the parameter that says where the function lives is the same
  parameter that carries what it is allowed to do. It cannot drift from the truth: a function cannot claim to be
  server-side without holding server capabilities, and cannot reach a server-only API without asking for the
  token that provides it. The same shape extends to any capability (`DatabaseContext`, `FileSystemContext`)
  without another language feature.
- **The test is identity, not a name.** `function.arguments[0].class == ServerContext` compares two
  `Spite.Class` values, so a renamed or misspelled class is a compile error rather than a predicate that quietly
  turns false and drops an endpoint. Matching on `.name.starts_with("Server")` would work and is what a language
  without real reflection has to do; Spite has the class object, so it uses it.
- **A union gives exhaustiveness.** With a `union Context` of `ServerContext` and `ClientContext`, a `switch` over it
  must cover every member ([Types](values_and_types.md#types)), so adding a third environment later -- a worker, an edge runtime -- fails
  to compile everywhere that has to change, instead of silently taking a default branch.
- **The body is never emitted into the client bundle.** Only the generated call stub is, so a secret, a
  connection string, or a server-only helper reachable from that body is removed by the per-bundle tree shaking
  [Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial) already does. This is the property that makes the split safe rather than merely convenient, and it
  costs nothing extra here.
- Routes are generated deterministically from the class and function name, and what crosses is already in the
  language: a `type` shape ([Types](values_and_types.md#types)). It travels in [the wire format](#the-wire-format--planned)
  below, a binary packing, not JSON (D31).
- **A function crossing the boundary must have serialisable parameters and return type** -- scalars, `String`, a
  `type` shape, or a list of those. A class instance cannot cross (its identity and reference count are local),
  and the diagnostic names the parameter or field that does not qualify.
- **A failed call returns null and does not crash** (D24): the network can fail where a local call cannot, but the
  program is not wrong, so the client's stub of a function returning `T` answers a `T?`
  ([failure.md](failure.md#failure-three-outcomes-and-no-others--partial)). A caller that must tell a timeout from a rejection takes back a `type`, `union` or
  enum that models it -- ordinary data, not an error channel. How the stub's signature is spelled is not decided.

## Targets  **[planned]**

D20 (decided by Mortaro, 2026-09-19): a class says where it can run by overriding an ordinary member of
`Spite.Class`, `func targets(): List<Symbol>`. The default is every target; `DynamicLibrary` answers `['native']`
and `Html` answers `['web']`. Using a class against the wrong target is a compile error naming the class, the
target and the flag. This replaces any special rule about `File`, `Directory` and `Process` on the web: they
declare their targets like everything else. The check runs after compile time folding and tree shaking, so a
`load` behind `if build.target == "web"` stays legal: only a class still reachable in the built program is
checked. It is compile time only: `targets()` is never called by the running program.

## Html: the browser as a library  **[planned]**

D19 (decided by Mortaro, 2026-09-19): `Html` is the web counterpart of `DynamicLibrary`: a singleton whose
`missing_function` and `missing_attribute` resolve against the browser. `Html.Node` wraps one node handle (an index
into a table on the JavaScript side, freed by `drop()`). Names convert with the `'camel_case'` naming rule:
`document.create_element("div")` is `createElement`, `node.text_content = "Hello"` is `textContent`,
`root.append_child(node)` is `appendChild`. A call is a method and a read or write is a property, the same split
foreign libraries use for functions and constants. `Html` is the low level layer the markup builder compiles down
to, as `Mouse` sits on `DynamicLibrary`.

Open: the markup builder below was sketched as `html.div(...)`, which now collides with this class. It needs
another name (`Markup`? `View`?). Mortaro has not picked.

## Markup  **[planned]**

D18 (decided by Mortaro, 2026-09-19): there is no JSX and no trailing block. Markup is `missing_function` plus
literals, with as many children as the call has arguments. That costs no new feature, because a Symbol codegen
function is generated per call site:

```
html.div({ class: "card" }, html.h1(title), TodoForm({ on_done: add_todo }))
```

It pulls together what already exists: tags come from `missing_function`, a list of children is
`todos.map_render()`, a component is any class that fits a `type` requiring `render()`, a handler is a function
value bound to its instance, and `when(condition, element)` returns an `Element?` that the framework skips.

Rejected, so nobody proposes them again: a JSX like literal is a second grammar and fixes neither conditionals
nor mapping; a trailing block (`f(x) { a b c }`) serves exactly one shape, since queries are
`database.filter_age_greater_than(10).sort_by_name()` and routes are plain statements, so it would be one more
meaning for `{ }` bought for a single case. Mortaro on the verbosity: annoying for a human to write, cheap for an
AI to write, and a human can still easily read it, which is the first line of the philosophy.

## The wire format  **[planned]**

D31 (decided by Mortaro, 2026-09-19): JSON is not what isomorphic classes send. Both bundles compile from the same
source, so there are no unknown consumers and nothing needs to describe itself: the wire carries a binary packing
derived at compile time, attributes in declaration order, no keys and no parsing. It compounds on the web, where
nothing has to be turned into strings to cross into JavaScript: the DOM command buffer and the remote call channel
are both opaque byte buffers. Proposed by Claude, unconfirmed: a schema hash in the handshake so an old client
meeting a new server crashes with a clear report instead of misreading bytes, and `--development` decoding
payloads to JSON on demand, since the compiler knows the schema.

The packing itself is built (D208): `BinaryWriter` and `BinaryReader` write and read any value in exactly this form,
and a program can already send their bytes over a `Socket` ([json.md](json.md#the-binary-format)). What is still
planned is the rest -- isomorphic classes sending them on their own, the handshake's schema hash, and the
`--development` decoding.
