# Known issues

Places where the real compiler's behavior surprised careful reading of `manual.md`, found while writing
`docs/`. None of these are fixed here (milestone 7b is documentation only) -- each is documented in the
relevant page and repeated here with a minimal repro.

## 1. `.class` does not resolve through a `type`-typed variable or parameter

manual.md section 7 says "`.class` of the value still points to its original class," without qualification.
In practice, `.class` only resolves when the variable/parameter's *declared* type is the concrete class
itself. The exact same instance, read through a variable whose declared type is a structural `type`, is a
compile error:

```spite title=class_via_type_error/widget.spite
var label = "gadget"

func Widget(new_label: String) {
    label = new_label
}
```
```spite title=class_via_type_error/class_via_type_error.spite entry error
type Labeled = {
    label: String
}

func ClassViaTypeError() {
    var widget = Widget("thing")
    var labeled: Labeled = widget
    var widget_class = labeled.class
}
```
```diagnostic
unknown field 'class' on type
```

Workaround: read `.class` off the original class-typed variable before assigning it into the `type`-typed one
(see [values_and_types.md](values_and_types.md)'s duck typing section for a working example side by side with
this failure).

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
`tests/attribute_interception/`.
