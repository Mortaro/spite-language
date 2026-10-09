# The Windows executable Spite's backend writes

What [`bootstrap/source/backend/windows_executable_writer.spite`](../bootstrap/source/backend/windows_executable_writer.spite)
writes for Windows x86-64 (D558, milestone M0 of
[own_backend_plan.md](proposals/own_backend_plan.md#m0-a-hand-built-executable-windows)): one section holding the machine
code of `main`, one holding the imports of `kernel32.dll`, and the headers that put them on disk. Windows calls the
format Portable Executable and its object files Common Object File Format; the fields below are named as the format
spells them (`PE32+`, `e_lfanew`, `.text`, `.rdata`). The numbers are the ones in the code, each a fixed choice the
format allows and not a value anything computes. Nothing here is read at run time, and no field or section is laid
out to be read by a person (D561): the page is the layout the writer must produce, so the writer and what it wrote
can be checked against it.

## What the writer is given

The names of the functions imported from `kernel32.dll`, and the machine code of `main`. The imports are placed
before the code is known (the address of each is answered by `address_of_import`), because their place does not
depend on the code's length: the code sits in the first section and the imports in the second, and a code longer
than one section stops the write rather than move the imports under a call that was already written.

A name that is not in that list stops the answer, and a file the system refuses to write stops the program: a wrong
executable must never be silent (D244).

## The entry code

The code is the caller's, given as bytes. For a program whose only work is to finish, it is:

| Bytes | Instruction |
|---|---|
| `B9 2A 00 00 00` | `mov ecx, 42`, the exit code the first argument takes |
| `48 83 EC 08` | `sub rsp, 8`, so the stack is aligned for the call |
| `48 8D 05 <4 bytes>` | `lea rax, [rip + <offset of the import>]`, the offset from [Addressing an import](#addressing-an-import) |
| `FF 10` | `call qword ptr [rax]`, through the address the loader wrote there |
| `0F 0B` | `ud2`, so a call that came back is an illegal instruction and not a silent fall-through |

The stack is aligned because the loader enters the executable the way it enters any function, with the return
address on the stack, and a Windows x86-64 callee expects the stack aligned before the call.

The offset is written against the instruction after the `lea`, so the code reads the import wherever the image was
loaded and no relocation is needed. `code_address()` answers where the first code byte lands, so the caller takes
the difference between the two answers itself.

## The DOS header

64 bytes: the two letters `MZ` (`5A 4D`), 58 zero bytes, then `e_lfanew` = 64, the place the PE header starts.
Nothing else of the DOS header is read by the loader.

## The file header

The signature `PE\0\0`, then the 20-byte Common Object File Format header:

| Field | Value |
|---|---|
| Machine | `8664` (x86-64) |
| Sections | 2 |
| Time stamp | 0, so the same program always writes the same bytes |
| Symbol table | none |
| Optional header | 240 bytes, the size of a PE32+ one |
| Characteristics | `22`, an executable image with large addresses |

## The optional header

240 bytes, the 64-bit one (`PE32+`, magic `20B`):

| Field | Value |
|---|---|
| Linker version | 0 |
| Code and initialized data | each section's size on disk, rounded up to 512 |
| Entry point | 4096, the first code byte |
| Base of code | 4096 |
| Image base | 5368709120 (`140000000`) |
| Section alignment | 4096 |
| File alignment | 512 |
| Operating system version | 6.0 |
| Image version | 0.0 |
| Subsystem version | 6.0 |
| Size of the image | 12288, three sections of 4096 |
| Size of the headers | 512 |
| Subsystem | 3, a console program |
| Stack reserve and commit | 1048576 and 4096 |
| Heap reserve and commit | 1048576 and 4096 |
| Data directories | 16, of which only the import one is filled |

No base relocation directory is written and no dynamic base is asked for, so the loader loads the image where the
image base says and the code needs no fixup when it does.

## The section headers

Two, 40 bytes each. The first is `.text`: the code, at 4096, with the read and execute flags. The second is
`.rdata`: the imports, at 8192, with the read and write flags, because the loader writes the address of each
import into its slot. Headers occupy the first 512 bytes of the file, the code the 512 after it, and the imports
after the code.

## The import section

Everything below is measured from 8192.

| What | Where |
|---|---|
| One import descriptor and the empty one that ends the list | 40 bytes from 0 |
| Lookup table: one address per import, then a zero | after the descriptors |
| Address table: the same addresses, then a zero | after the lookup table |
| One hint and name per import: a zero hint, the name, a zero, and a zero byte if that made it odd | after the address table |
| `kernel32.dll` and a zero | after the last name |

The loader reads the names through the lookup table and writes the addresses it resolved into the address table,
which is why the address table is the one with the write flag and the one `address_of_import` answers: it is where
the call goes.

## Addressing an import

The address of an import is its image base (5368709120) plus 8192, plus where its slot sits in the address table:
40 bytes of descriptors, then eight bytes per import so far, then eight per import including this one. `code_address()`
answers the image base plus 4096, so the difference between the two answers is the offset a `lea` writes. A name
that was not added stops the answer, since a caller that called through a slot nothing wrote would jump into the
middle of the headers.

Nothing here depends on the code the caller writes, except its size: an import's place is fixed by the list the
writer is made with, so a caller can ask before it has written a single byte, and the write stops if the code no
longer fits beside it.