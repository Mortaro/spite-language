# Known issues

Where the compiler falls short of the [documentation](README.md) today, or does something a careful reader would not
expect. Each runnable repro below pins the current behaviour, so when an issue is fixed, `check.sh` fails here and
this page gets updated in the same change. Features that are decided but simply not built yet are named on the
page that describes them, not here.

## Everything that is known

- **Only Windows runs.** The Linux and macOS folders of the library, and the concurrency built on them, are held to
  compiling by `check.sh`, not run.
- **An enum cannot be switched over**; compare it with `==` in an `if` chain.
