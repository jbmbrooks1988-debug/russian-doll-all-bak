# pRISC - 2-Pass Minimal RISC Emulator

A minimal 16-bit/32-bit RISC emulator implemented in a single C file. It uses a 2-pass assembly process to resolve labels and execute instructions.

## Architecture
- **Registers**: 16 general-purpose registers (`x0` - `x15`).
- **x0**: Hardwired to 0.
- **Memory**: 4096 words of memory.
- **Pass 1**: Scans for labels (`label:`) and maps them to instruction addresses.
- **Pass 2**: Parses instructions and resolves label references.

## Instruction Set
- `addi rd, rs1, imm`: `R[rd] = R[rs1] + imm`
- `beq rs1, rs2, label`: Jump to `label` if `R[rs1] == R[rs2]`
- `lw rd, imm(rs1)`: Load word from `mem[R[rs1] + imm]`
- `sw rs2, imm(rs1)`: Store word from `R[rs2]` to `mem[R[rs1] + imm]`
- `jalr rd, rs1, imm`: `R[rd] = PC + 1; PC = R[rs1] + imm` (Jump and Link)
- `out rs1`: Print the value of `R[rs1]` to CLI and output file.
- `halt`: Terminate execution.

## Usage
### Compilation
```bash
gcc prisc.c -o prisc
```

### Execution
```bash
./prisc <source.asm> [output.txt]
```
- `<source.asm>`: The assembly file to execute.
- `[output.txt]`: (Optional) File to write `out` instruction results to.

## Example (`test.asm`)
```asm
addi x1, x0, 10      # Set x1 = 10
sw x1, 0(x0)         # Store 10 at mem[0]
lw x2, 0(x0)         # Load from mem[0] to x2
addi x3, x0, 0       # Counter x3 = 0
loop:
addi x3, x3, 1       # x3++
out x3               # Print x3
beq x3, x1, end      # If x3 == 10, exit
addi x4, x0, 4       # Load address 4 (start of loop logic)
jalr x0, x4, 0       # Jump back to loop start
end:
halt
```
