# Testing

A test is a package that crashes. There is no framework: no `expect`, no matchers, no reporter and no summary.
A test is an ordinary function whose name starts with `test_`, in a class whose name ends with `Tests`, and
`crash` is the assertion -- when a test is wrong, the program halts with the condition, the values of its
operands and the file and line ([failure.md](failure.md#what-a-crash-reports)), which is everything needed to fix
it. A run that prints nothing passed.

```gdscript
func test_append_and_prepend_keep_order() {
    var numbers = [2, 3]
    numbers.append(4)
    numbers.prepend(1)
    crash numbers.count() == 4
    crash numbers[0] == 1
    crash numbers.last() == 4
}
```

## The runner is twenty lines of Spite

`tests/tests.spite` finds every test itself, through reflection: `Spite.Class.instances` is every class in the
program, and each class's `.functions` its functions ([reflection.md](reflection.md)). Nothing registers a test,
and there is no manifest to keep in step. A runner of the same shape:

```gdscript title=test_package/text_tests.spite
func test_trim_removes_the_spaces_around_text() {
    var trimmed = "  spite  ".trim()
    crash trimmed == "spite"
}

func test_split_keeps_empty_pieces() {
    var pieces = "a,,b".split(",")
    crash pieces.count() == 3
}
```
```gdscript title=test_package/test_package.spite entry
var console = Console()

func TestPackage() {
    var classes = Spite.Class.instances
    var index = 0
    while index < classes.count() {
        if classes[index].name.ends_with("Tests") {
            run(classes[index].functions)
        }
        index = index + 1
    }
    console.print("every test passed")
}

func run(functions: List<Spite.Function>) {
    var index = 0
    while index < functions.count() {
        var function = functions[index]
        if function.name.starts_with("test_") {
            function.call_function()
            console.print("passed", function.name)
        }
        index = index + 1
    }
}
```
```output
passed test_trim_removes_the_spaces_around_text
passed test_split_keeps_empty_pieces
every test passed
```

`call_function()` calls a function found through reflection; today it can call one that takes no arguments,
which is what a test is.

## Tests prove there are no leaks

The repository's own runner calls each test twice and crashes when the second call leaves any allocation behind,
through `Program().live_allocations()`, so every test is a leak test as well. `check.sh` runs the package with
`--debug_memory` and requires the whole run to print nothing and balance:

```bash
spite tests --debug_memory
```
