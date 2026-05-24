# pRISC+RM - Persistent RAM/ROM RISC Emulator

This version of pRISC adds support for persistent memory storage using Key-Value Pair (KVP) text files.

## Architecture
- **Registers**: 16 general-purpose registers (`x0` - `x15`).
- **x0**: Hardwired to 0.
- **Memory**: 4096 words of memory, optionally loaded from and saved to `.txt` files.

## CLI Usage
```bash
./prisc+rm <source.asm> [mem_in.txt] [mem_out.txt]
```
- `<source.asm>`: The assembly file to execute.
- `[mem_in.txt]`: (Optional) Read at startup to initialize memory.
- `[mem_out.txt]`: (Optional) Non-zero memory state is saved here at `halt`.

## Instruction Set
- `addi rd, rs1, imm`: `R[rd] = R[rs1] + imm`
- `beq rs1, rs2, label`: Jump if `R[rs1] == R[rs2]`
- `lw rd, imm(rs1)`: Load word from `mem[R[rs1] + imm]`
- `sw rs2, imm(rs1)`: Store word to `mem[R[rs1] + imm]`
- `jalr rd, rs1, imm`: `R[rd] = PC + 1; PC = R[rs1] + imm`
- `out rs1`: Print register value to stdout.
- `halt`: Stop execution.

## Persistent Memory Format (KVP)
`mem_in.txt` and `mem_out.txt` use space-separated pairs:
```text
10 99
11 1024
```
(Address 10 contains value 99, Address 11 contains value 1024.)

## Example: Sharing Data Between Programs
```bash
# Program 1 writes 99 to memory address 10 and saves it
./prisc+rm p1.asm /dev/null state.txt

# Program 2 loads address 10 from the saved state and prints it
./prisc+rm p2.asm state.txt /dev/null
```
