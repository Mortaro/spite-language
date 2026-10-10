# Mortaro's inbox

Empty. Add notes here; an agent moves them into the docs (the page and `design/decisions.md`) and clears this file.

---

a switch case should never have 2 lines of statements, its hard to read, and probably means that path shouldve been a proper named function like

switch a {
    'lorem': do_something()
    'ipsum': do_something_else()
}

---

the existence of check.sh is a flaw of the language, it should be proven that its able to compile an thats enough, we can make running the tests part of the compilation process and build crashes by nature of how we test with crashes.
we should also allow setting the output path from the launcher class and we can use arguments there so if we pass a version bump flag that means it needs to run all the tests and optimize and so on, so flags can also be run as configuration in the program itself.
running the tests to compile should be optional if we need to do quick compilations and that option should be required if its a release build

---

we should aim to remove the seed code once we have linux/mac releases too, those become only history in the git and except benchmarks the only language the repo uses is spite.
we should also remove unused python scripts, why are you using python in the first place? we should make a compilation rule in this repo specific to loop over all directories and if a non spite file is found for scripting it crashes at compile time
we need to make sure crashes can happen at compile time

---

status table on the main readme at this point is mostly noise, and probably outdated at this point.