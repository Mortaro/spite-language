# The x86-64 instructions Spite's backend encodes

What [`compiler/source/backend/x86/encoder.spite`](../compiler/source/backend/x86/encoder.spite) (class `X86.Encoder`)
writes into a `Buffer`, one instruction at a time, for Windows and Linux on x86-64 (D558, milestone M2 of
[own_backend_plan.md](proposals/own_backend_plan.md#m2-x86-64-encoder)). The page is the encoding itself: the forms each
instruction takes, the bytes each form is, and the forms that are refused, so another backend can be built from it and
the encoder and what it wrote can be checked against it.

Nothing here is read at run time and nothing is laid out for a person to read (D561): the encoder runs while compiling
and ships nothing. A program that writes no code carries none of it: each instruction is its own function, and one that
emits only `mov` carries only `mov`.

The encoder knows the address its first byte lands at, and grows it as it writes, so a jump is given the address it
goes to and never a displacement: the short or the long form is chosen from the distance, exactly as an assembler
chooses it. Nothing here is decided by a table at run time; the only choice between two forms is the value itself (an
immediate that fits in a signed byte, a target within 127 bytes), which is all the run can know.

## The operands

An instruction takes a destination and, where it takes two, a source. A destination is a register or a place in
memory (`X86.Destination`), a source is a register, an immediate or a place (`X86.Source`). Each operand is one of four
kinds (`X86.Kind`):

| Kind | What it is | Made by |
|---|---|---|
| `general_register` | one of the sixteen 64-bit registers, at 32 or 64 bits | `X86.Register(number, bits)` |
| `floating_register` | one of the sixteen SSE registers, `xmm0` to `xmm15` | `X86.Register(number, 128)` |
| `memory` | a place in memory: a base, an index scaled by 1, 2, 4 or 8, a displacement, or the instruction pointer | `X86.Place()`, then the calls under [A place in memory](#a-place-in-memory) |
| `immediate` | a number written into the instruction | `X86.Immediate(value)` |

Every operand carries its kind, so a refusal names the form that was given: the report shows `kind=floating_register`
for a register where only a whole-number one is encoded, beside the line that refuses it. Which forms an instruction
takes is in [the instructions](#the-instructions); anything else halts, and
[what is refused](#what-is-refused) lists every one of those forms.

## Registers

A register is a number and a width. The sixteen whole-number registers are numbered as the machine numbers them:

| Number | 64 bits | 32 bits | Role in an address |
|---|---|---|---|
| 0 | `rax` | `eax` | |
| 1 | `rcx` | `ecx` | |
| 2 | `rdx` | `edx` | |
| 3 | `rbx` | `ebx` | |
| 4 | `rsp` | `esp` | the stack pointer: a base always needs the byte-scaled form |
| 5 | `rbp` | `ebp` | the frame pointer: a base with no displacement still carries one byte of zero |
| 6 | `rsi` | `esi` | |
| 7 | `rdi` | `edi` | |
| 8 to 15 | `r8` to `r15` | `r8d` to `r15d` | need the extension prefix |

The floating point registers are `xmm0` to `xmm15`, made with a width of 128 bits, the width the machine has of them.
A register of eight or sixteen bits is refused: the whole-number forms below are 32 and 64 bits, and a `Spite.Tiny` or a
`Spite.Short` value is carried in a 32-bit register until M5 says otherwise.

## A place in memory

A place is a base register, an index register with a scale, a displacement, or the instruction pointer. A base or an
index that is not there is left out (zero is not a register), and the calls on a place give it each shape:

| Call | The place |
|---|---|
| `at_register(base, displacement)` | `[base + displacement]`, a 64-bit base |
| `indexed_by(base, index, scale, displacement)` | `[base + index * scale + displacement]` |
| `after_the_instruction(displacement)` | `[rip + displacement]`, four bytes always |
| `at_the_address(displacement)` | `[displacement]`, no base and no index |
| `of_bits(bits)` | says the width the instruction writes |

`bits` is the width of the place when the instruction needs one of its own, and `0` when the other operand gives it.
Only an immediate into a memory and a divisor that is a memory have no other operand to take the width from, so those
two forms need it and refuse `0`. A width that is not the one the instruction writes is refused too, so a 32-bit
register can never write eight bytes of a place.

## Immediates

`X86.Immediate(value)` carries a `Long`. The instruction decides how many bytes it is written in: an `add` or a
`compare` takes one byte when the value fits in a signed byte and four otherwise, a `push` the same way, a `mov` of a
32-bit register takes four, and a `mov` of a 64-bit register from a value wider than 32 bits takes eight. A value an
instruction cannot hold in its own form halts, naming the value.

## The conditions

`X86.Condition` is the sixteen conditions a conditional jump can test, in the order the machine numbers them, so the
opcode of a jump is `70` plus the position of its condition (or `0F 80` plus it for the long form):

| Condition | Jump | Code | Condition | Jump | Code |
|---|---|---|---|---|---|
| `overflow` | `jo` | 0 | `signed` | `js` | 8 |
| `no_overflow` | `jno` | 1 | `not_signed` | `jns` | 9 |
| `below` | `jb` | 2 | `even_parity` | `jp` | 10 |
| `above_or_equal` | `jae` | 3 | `odd_parity` | `jnp` | 11 |
| `equal` | `je` | 4 | `less` | `jl` | 12 |
| `not_equal` | `jne` | 5 | `greater_or_equal` | `jge` | 13 |
| `below_or_equal` | `jbe` | 6 | `less_or_equal` | `jle` | 14 |
| `above` | `ja` | 7 | `greater` | `jg` | 15 |

## The extension prefix

Before an instruction that names a register of eight or more, a prefix byte says so: `0100WRXB`, where `W` is that the
operand is 64 bits wide, `R` that the `ModRM` register is one of the eight high registers, `X` that the index is, and
`B` that the base or the register the opcode names is. It is written only when one of those bits is set, so an
instruction of the low registers is exactly as long as it was before the processor grew. It comes first, before the
`F3` or `F2` of a floating point instruction and before the `0F` of a two-byte opcode.

The whole-number instructions below write it for `W` only, and refuse a register of eight or more by name (a `mov` of
one of them takes a different opcode, and it is for M3 to decide what the allocator does with the eight high ones). The
floating point instructions write it for `R`, `X` and `B`.

## The place form

The bytes of a place operand are chosen the way an assembler chooses them, so the same program gives the same bytes
either way:

- a base that is `rsp` or `r12`, an index, or no base at all writes the byte-scaled form: the `ModRM` byte with
  `mod` as asked for and `rm=100`, then one byte holding the scale, the index and the base;
- a displacement of zero keeps its bytes out, unless the base is `rbp` or `r13`, which always carries one byte (of zero
  when there is no displacement);
- a displacement that fits in a signed byte is written in one, and anything else in four;
- after the instruction pointer the displacement is always four bytes, the `mod` byte is `00` with `rm=101`, and no
  index may be given with it.

The displacement is measured from the end of the instruction, so a place relative to the instruction pointer holds the
address the instruction ends at plus the displacement.

## The instruction forms

Four shapes write almost every instruction, each of them choosing the extension prefix, the opcode, and the `ModRM`
byte and the place bytes around it:

| Shape | The `ModRM` register field holds | The `ModRM` addressed field holds |
|---|---|---|
| `write_the_register_form` | the register named by the opcode's own form | the other register |
| `write_the_place_form` | the register operand | the place |
| `write_the_floating_register_form` | the `xmm` operand | the other `xmm` |
| `write_the_floating_place_form` | the `xmm` operand | the place |

An immediate that fits in a signed byte is written with the one-byte opcode (`83`, or `6A` pushed) and anything else
with the four-byte one (`81`, or `68`), which is the choice an assembler makes. A `mov` of a 32-bit register from a number
is the short `B8+r`, and a `mov` of a 64-bit register from a number that fits in 32 bits is `C7 /0`, as an assembler
writes them.

The accumulator's own short forms (`05`, `2D`, `3D` and `A8` for a comparison) are left out: they are one byte shorter
than the general form for `rax` alone and the same for every other register, so one form to write is worth more than one
byte of code, and the table's rows use `rbx` for the four-byte immediates the assembler would shorten.

## The instructions

Every form below is one function of `X86.Encoder`. Each example is a row of the table in
[backend/x86_encoding](../../backend/x86_encoding) with the bytes that assembler gave it.

### Moving

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `move` register from register | `89 /r` | `mov rax, rbx` | `48 89 D8` |
| `move` register from memory | `8B /r` | `mov rax, [rdi + 8]` | `48 8B 47 08` |
| `move` memory from register | `89 /r` | `mov [rdi + 8], rax` | `48 89 47 08` |
| `move` 32-bit register from a number | `B8+r id` | `mov ecx, 42` | `B9 2A 00 00 00` |
| `move` 64-bit register from a number | `C7 /0 id` | `mov rcx, 42` | `48 C7 C1 2A 00 00 00` |
| `move` 64-bit register from a wide number | `B8+r io` | `mov rax, 1234605616436508552` | `48 B8 88 77 66 55 44 33 22 11` |
| `move` memory from a number | `C7 /0 id` | `mov dword ptr [rdi + 8], 42` | `C7 47 08 2A 00 00 00` |

The two registers of a register-to-register form must be of the same width, and a register with a memory takes the
width of the register.

### Adding

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `add` register from register | `01 /r` | `add rax, rbx` | `48 01 D8` |
| `add` register from memory | `03 /r` | `add rax, [rdi + 8]` | `48 03 47 08` |
| `add` memory from register | `01 /r` | `add qword ptr [rdi + 8], rax` | `48 01 47 08` |
| `add` register from a number | `83 /0 ib` or `81 /0 id` | `add rax, 5` | `48 83 C0 05` |
| `add` memory from a number | `83 /0 ib` or `81 /0 id` | `add qword ptr [rdi + 8], 5000` | `48 81 47 08 88 13 00 00` |

### Subtracting

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `subtract` register from register | `29 /r` | `sub rax, rbx` | `48 29 D8` |
| `subtract` register from memory | `2B /r` | `sub rax, [rdi + 8]` | `48 2B 47 08` |
| `subtract` memory from register | `29 /r` | `sub qword ptr [rdi + 8], rax` | `48 29 47 08` |
| `subtract` register from a number | `83 /5 ib` or `81 /5 id` | `sub rsp, 8` | `48 83 EC 08` |
| `subtract` memory from a number | `83 /5 ib` or `81 /5 id` | `sub qword ptr [rdi + 8], 5000` | `48 81 6F 08 88 13 00 00` |

### Comparing

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `compare` register from register | `39 /r` | `cmp rax, rbx` | `48 39 D8` |
| `compare` register from memory | `3B /r` | `cmp rax, [rdi + 8]` | `48 3B 47 08` |
| `compare` memory from register | `39 /r` | `cmp qword ptr [rdi + 8], rax` | `48 39 47 08` |
| `compare` register from a number | `83 /7 ib` or `81 /7 id` | `cmp rax, 5` | `48 83 F8 05` |
| `compare` memory from a number | `83 /7 ib` or `81 /7 id` | `cmp dword ptr [rdi + 8], 5000` | `81 7F 08 88 13 00 00` |

The `sub rsp, 8` is the shape M0's entry code needs: the stack is aligned before a call.

### Multiplying

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `multiply_signed` register from register | `0F AF /r` | `imul rax, rbx` | `48 0F AF C3` |
| `multiply_signed` register from memory | `0F AF /r` | `imul rax, [rdi + 8]` | `48 0F AF 47 08` |
| `multiply_signed_by` a register and a number | `6B /r ib` or `69 /r id` | `imul rax, rbx, 7` | `48 6B C3 07` |
| `multiply_signed_by` a memory and a number | `6B /r ib` or `69 /r id` | `imul rax, [rdi + 8], 7` | `48 6B 47 08 07` |

`multiply_signed` takes the number it multiplies by as its source, so it is the one instruction whose source and
destination are both written in one; `multiply_signed_by` takes it apart and takes a number as a third operand, which
is what a lowering wants when the factor is a constant.

### Dividing

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `divide_signed` by a register | `F7 /7` | `idiv rbx` | `48 F7 FB` |
| `divide_signed` by a memory | `F7 /7` | `idiv qword ptr [rdi + 8]` | `48 F7 7F 08` |

`idiv` divides the `rax` of the instruction before it into `rax` and `rdx`, so the sign extension of `rax` into `rdx`
(`cqo`, or `cdq` at 32 bits) is a separate instruction of the code generator, not of this encoder. A memory divisor has
no other operand to take its width from, so it refuses a memory with no width of its own.

### Jumping

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `jump_to` a target within 127 bytes | `EB ib` | `jmp 13 bytes on` | `EB 0D` |
| `jump_to` a further target | `E9 id` | `jmp 4096 bytes on` | `E9 00 10 00 00` |
| `jump_through` a register | `FF /4` | `jmp rax` | `FF E0` |
| `jump_through` a memory | `FF /4` | `jmp [rax]` | `FF 20` |
| `jump_if` a condition within 127 bytes | `70+cc ib` | `je 13 bytes on` | `74 0D` |
| `jump_if` a condition further on | `0F 80+cc id` | `je 4096 bytes on` | `0F 84 00 10 00 00` |

A jump's displacement is measured from the end of the instruction, so the encoder is given the address it goes to. The
table's rows are the address the assembler placed its label at: 13 bytes on for the short form and 4096 for the long
one, which is what made the assembler choose it.

### Calling

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `call_to` a target | `E8 id` | `call 13 bytes on` | `E8 0D 00 00 00` |
| `call_through` a register | `FF /2` | `call rax` | `FF D0` |
| `call_through` a memory | `FF /2` | `call qword ptr [rax]` | `FF 10` |
| `return_to_caller` | `C3` | `ret` | `C3` |
| `illegal_instruction` | `0F 0B` | `ud2` | `0F 0B` |

A call's displacement is always four bytes: the machine has no short form of it. `illegal_instruction` is what a path
that must not run ends with, so a function that falls off its end stops there instead of running whatever the layout
put next: it is the only instruction here that raises a fault, and [M0's executable](../backend/hand_built_executable)
ends with it after the call it cannot come back from.

### Pushing and popping

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `push` a register | `50+r` | `push rbp` | `55` |
| `push` a memory | `FF /6` | `push [rdi + 8]` | `FF 77 08` |
| `push` a number | `6A ib` or `68 id` | `push 42` | `6A 2A` |
| `pop` a register | `58+r` | `pop rbp` | `5D` |
| `pop` a memory | `8F /0` | `pop qword ptr [rdi + 8]` | `8F 47 08` |

A number pushed is written in one byte when it fits in a signed byte and four otherwise, and becomes 64 bits. Only a
64-bit register is pushed or popped; a register of another width is refused.

### Taking an address

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `take_address_of` into a register | `8D /r` | `lea rax, [rip + 4096]` | `48 8D 05 00 10 00 00` |

Only a register and a memory: a memory has no address to compute, and a register is already its own address.

### The floating point moves

The `Float` (32-bit) forms carry the `F3` prefix and the `Double` (64-bit) forms the `F2` one, both after the extension
prefix and before the `0F` of the opcode.

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `move_float` register from register | `F3 0F 10 /r` | `movss xmm0, xmm1` | `F3 0F 10 C1` |
| `move_float` register from memory | `F3 0F 10 /r` | `movss xmm0, [rdi + 8]` | `F3 0F 10 47 08` |
| `move_float` memory from register | `F3 0F 11 /r` | `movss [rdi + 8], xmm0` | `F3 0F 11 47 08` |
| `move_double` register from register | `F2 0F 10 /r` | `movsd xmm0, xmm1` | `F2 0F 10 C1` |
| `move_double` register from memory | `F2 0F 10 /r` | `movsd xmm0, [rdi + 8]` | `F2 0F 10 47 08` |
| `move_double` memory from register | `F2 0F 11 /r` | `movsd [rsp + 16], xmm1` | `F2 0F 11 4C 24 10` |

A number is refused here, unlike with a whole number: neither assembler on the machine (the LLVM one behind `clang`,
and the one behind Microsoft's macro assembler) takes one, so no row could check what the encoder would write, and a
constant is loaded from a memory place instead, which is what a C compiler emits for it too.

### The floating point arithmetic

Each takes a floating point register and a register or a memory (a 32-bit place for the `Float` forms and a 64-bit one
for the `Double` forms). A memory destination is not a form of any of them, and neither is a number.

| Form | Bytes | Example | The bytes |
|---|---|---|---|
| `add_float` | `F3 0F 58 /r` | `addss xmm0, xmm1` | `F3 0F 58 C1` |
| `add_double` | `F2 0F 58 /r` | `addsd xmm0, [rdi + 8]` | `F2 0F 58 47 08` |
| `subtract_float` | `F3 0F 5C /r` | `subss xmm0, xmm1` | `F3 0F 5C C1` |
| `subtract_double` | `F2 0F 5C /r` | `subsd xmm0, xmm1` | `F2 0F 5C C1` |
| `multiply_float` | `F3 0F 59 /r` | `mulss xmm0, xmm1` | `F3 0F 59 C1` |
| `multiply_double` | `F2 0F 59 /r` | `mulsd xmm0, [rdi + 8]` | `F2 0F 59 47 08` |
| `divide_float` | `F3 0F 5E /r` | `divss xmm0, xmm1` | `F3 0F 5E C1` |
| `divide_double` | `F2 0F 5E /r` | `divsd xmm0, xmm1` | `F2 0F 5E C1` |
| `minimum_float` | `F3 0F 5D /r` | `minss xmm0, xmm1` | `F3 0F 5D C1` |
| `minimum_double` | `F2 0F 5D /r` | `minsd xmm0, xmm1` | `F2 0F 5D C1` |
| `maximum_float` | `F3 0F 5F /r` | `maxss xmm0, xmm1` | `F3 0F 5F C1` |
| `maximum_double` | `F2 0F 5F /r` | `maxsd xmm0, xmm1` | `F2 0F 5F C1` |
| `compare_float` | `0F 2F /r` | `comiss xmm0, xmm1` | `0F 2F C1` |
| `compare_double` | `66 0F 2F /r` | `comisd xmm0, xmm1` | `66 0F 2F C1` |
| `square_root_float` | `F3 0F 51 /r` | `sqrtss xmm0, xmm1` | `F3 0F 51 C1` |
| `square_root_double` | `F2 0F 51 /r` | `sqrtsd xmm0, xmm1` | `F2 0F 51 C1` |
| `convert_float_to_double` | `F3 0F 5A /r` | `cvtss2sd xmm0, xmm1` | `F3 0F 5A C1` |
| `convert_double_to_float` | `F2 0F 5A /r` | `cvtsd2ss xmm0, xmm1` | `F2 0F 5A C1` |

A comparison leaves its answer in the flags, so the code generator writes the whole-number jump that reads it; the
encoder writes no such pair. The `Double` comparison carries the `66` prefix, because `0F 2F` with an `F2` in front of
it is the *unordered* comparison.

## What is refused

A form the encoder does not write halts, and the halt names the form it was given: the report shows the operand's kind
and the values it carries, beside the line that refuses it. Nothing is written wrong and nothing is written at all
(D244). The forms that are refused:

| Refused | What the report shows |
|---|---|
| a register numbered outside the sixteen | `starting_number` |
| a register of eight or sixteen bits | `starting_bits` |
| a floating point register where a whole number is wanted | `kind=floating_register` |
| two registers of different widths | both `bits` |
| a number that does not fit in 32 bits where the instruction takes 32 | `value` |
| a memory whose width is not the one the instruction writes | `bits` |
| a memory with no width of its own where the instruction needs one | `bits=0` |
| a scale that is not 1, 2, 4 or 8 | `scale` |
| `rsp` as an index | `index` |
| an index after the instruction pointer | `index` |
| a register of eight or more in a whole-number form that writes no extension prefix | `number` |
| a register of eight or more where only the low eight are encoded | `number` |
| two memory operands | `kind` |
| a memory where a register must be, as `lea`'s destination | `kind=memory` |
| a register or a number where a memory must be, as `lea`'s source | `kind` |
| a number where an instruction takes none (`idiv`, `jmp`, `call`, every floating point form) | `kind=immediate` |
| a register pushed or popped at less than 64 bits | `bits` |

[`backend/x86_encoding_refusals`](../backend/x86_encoding_refusals) is the proof: `REFUSAL=<form> spite
backend/x86_encoding_refusals` asks the encoder for one refused form and stops with the report above, so every row in
this table is a form that halts rather than one that was read off the code. The nine it asks for are the ones a code
generator reaches first.

What is not encoded at all, and is left to the milestone whose lowering needs it: the sign extension of `idiv`'s
dividend (`cqo` and `cdq`), the shifts, the eight- and sixteen-bit forms of the whole-number instructions, and the
conversions between a `Float` or a `Double` and a whole number.

## The table

[`backend/x86_encoding`](../../backend/x86_encoding) is the test: one row per form, each row the instruction as an
assembler writes it, the operands that make it, and the bytes that assembler gave. Every row is compared, and a row
that does not match halts naming the bytes the encoder wrote and the bytes the table holds.

The bytes were taken once, from an assembly file assembled with the LLVM assembler `clang` drives, and read back out of
the object file's own bytes. The test never calls an assembler, so it needs only the bytes written here: a row with a
jump names the label the assembler was given, and the row's operands hold the address that label landed at.