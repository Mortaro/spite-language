# Mortaro's inbox

Empty. Add notes here; an agent moves them into the docs (the page and `design/decisions.md`) and clears this file.

a lot of compiler/compiler.spite is just code over reading arguments. that means either our Environment/Build are not feature complete, or we are failing at looking for behavior that duplicates standard library. a lot of the behavior there also like looking for the program and formating paths should be part of our standard library, so when a moron reads it it quickly can understand the intention.

---

the engine does a lot of benchmarking using the pre Benchmark ways, we should have compile time errors when we have behavior matching standard library being reimplemented, this needs to be built iteratively as we see morons making those mistakes. this would also make migrating versions of the language super easy automatically

---

we should investigate having a "Table" construct in the standard library, i noticed a lot of the engine code is managing columns and rows, other applications would do it and it seems like a natural evolution of the lists. but being always used the way the standard library intends would allow us to make compile time optimizations with more confidence. i might be completely wrong tho!

---

the engine still extends Matrix4 thats same as bellow listed, either its a compiler error not noticing reusing logic from std, or its a lack in the standard library. we should investigate how compiler can notice "duplicated logic" in general, for sake of research, sometimes duplication is just a coincidence and its more flexible to duplicate, but sometimes its just making things harder to read as a human.

---

the engine still has a "os" folder for locks, writing os specific code is most likely a smell or a lack from our library, we cant force it at compile time because sometimes its actually needed, but in this case its just for file locking which most certainly is a flaw of our standard library not having something like file.lock()

---

do we actually need all these stages of conformance? we should only worrry about "current compiler" and not past ones if we have git history and no longer worry about bootstrap from scratch, once we have our own IR and backend and stop using C we also can stop releasing the bootstrap C in favor of just looking at tags. to be clear we only care about being able to build and run the current compiler, which makes us need the previous one, but we no longer need to bootstrap each stage in a row or even hold code or tests for old stages, as long as every test pass, we dont need binaries to be the same in the end

---

func stamp_of(path: String): String {
    var file = File(path)
    var modified = file.modified_nanoseconds()
    var size = file.size()
    if modified {
        if size {
            return "{modified}-{size}"
        }
    }
    return "missing"
}

nested ifs that could be a "and" should be a compiler error

if modified and size

---

.memory should become only getters to learn about the object and no longer a thing we can set as a move to remove manual memory management

---

a type and a generic are namespaced to the class file they are declared, weve been somehow using Number as a generic type and Nothing, so we have some leak on scope somewhere