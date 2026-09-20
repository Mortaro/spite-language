# Known issues

Places where the real compiler's behavior surprised careful reading of `manual.md`, found while writing
`docs/`. None of these are fixed here (milestone 7b is documentation only) -- each is documented in the
relevant page and repeated here with a minimal repro.

## 1. `.class` through a `type`-typed variable or parameter (fixed)

manual.md section 7 says "`.class` of the value still points to its original class". It did not: reading
`.class` through a variable whose declared type was a structural `type` answered the shape's name, or failed
outright. It now reads the object's own tag at runtime, so it answers the class the value really is:

```spite title=class_via_type_error/widget.spite
var label = "gadget"

func Widget(new_label: String) {
    label = new_label
}
```
```spite title=class_via_type_error/class_via_type_error.spite entry
type Labeled {
    label: String
}

var console = Console()

func ClassViaTypeError() {
    var widget = Widget("thing")
    var labeled: Labeled = widget
    console.print(labeled.class)
}
```
```output
Widget
```

The same holds for a union-typed value. An object literal has no class of its own and answers `Object`.

## 2. `get_<attribute>()` on an owning attribute (fixed in milestone 9a)

Under the old move-based memory model, a Symbol-codegen-generated getter for an owning attribute (a `String`,
`List<T>`, ...) double-freed at runtime, because attribute-read interception deliberately skipped a getter
whose return type "owned something" -- but calling one *explicitly* went through no such guard. Milestone 9a's
reference-counting model (manual.md section 10) removes the whole class of bug: every function/method return
is now a properly retained, independent reference, so a getter returning a fresh String/List/class is exactly
as safe as one returning the field itself. The `person.age` (implicit interception) read path's old
owning-type restriction is removed too (manual.md section 5) -- getter interception now applies uniformly,
regardless of what the getter returns.

A residual gap here (a getter-answered `person.age` read used directly as a call/print argument leaking the
getter's own extra reference) was fixed later in the same milestone: `ExpressionResult.is_owning` now lets
`codegenMember`'s getter-intercepted read tell every caller it already produced a fresh, independent reference,
regardless of the read's own `.member` AST shape (which otherwise reads as a stable alias) -- see
`conformance/`.
