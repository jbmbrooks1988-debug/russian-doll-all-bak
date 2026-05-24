# pRISC+X - Custom Instruction RISC Emulator

This version of pRISC adds support for custom instructions via external binaries and auto-allocated variables.

## Architecture
- **Registers**: 16 general-purpose registers (`x0` - `x15`).
- **x0**: Hardwired to 0.
- **Memory**: 4096 words, with auto-allocated variables at low addresses.

## CLI Usage
```bash
./prisc+x <source.asm> [mem_in.txt] [mem_out.txt] [ops_file.txt]
```
- `<source.asm>`: Assembly file with optional variable declarations.
- `[mem_in.txt]`: (Optional) Initialize memory from KVP file.
- `[mem_out.txt]`: (Optional) Save non-zero memory to KVP file at halt.
- `[ops_file.txt]`: (Optional) Additional custom ops file (default: default_op.txt).

## Variable Declarations
Variables are auto-allocated to memory starting at address 0:
```asm
# At top of .asm file
player_id int 1234        # mem[0] = 1234
player_x int 10           # mem[1] = 10
player_pos array 1,2,3    # mem[2]=1, mem[3]=2, mem[4]=3
```

## Instruction Set

### Standard RISC ops:
```
addi rd, rs1, imm    # R[rd] = R[rs1] + imm
beq rs1, rs2, label  # Jump if R[rs1] == R[rs2]
lw rd, imm(rs1)      # Load word from mem[R[rs1] + imm]
sw rs2, imm(rs1)     # Store word to mem[R[rs1] + imm]
jalr rd, rs1, imm    # R[rd] = PC + 1; PC = R[rs1] + imm
j label              # Unconditional jump
```

### Custom ops (defined in default_op.txt):
```
out x1               # Print register to stdout
halt                 # Stop execution
move_player x1       # x1 points to array [x, y, z]
console_print x2     # Print value in x2
layout_op rd, rs1    # rs1=menu_id, rd=selection result
```

## Interactive Menu (layout_op)

`layout_op` displays a CHTPM-inspired terminal menu:

**Built-in menus:**
- Menu 1 (Main): Start Game, Options, Exit
- Menu 2 (Options): Sound, Back to Main

**Usage in assembly:**
```asm
start:
    addi x1, x0, 1      # x1 = menu_id (1 = main menu)
menu_loop:
    layout_op x2, x1    # show menu, result in x2
    
    addi x3, x2, -1
    beq x3, x0, start_game
    addi x3, x2, -2
    beq x3, x0, go_options
    j menu_loop
```

**Testing with pre-defined input:**
```bash
# Single selection
echo "2" > /tmp/prisc_input.txt
./prisc+x test.asm 2>&1

# Multi-step navigation (Main→Options→Main→Start→Exit)
printf "2\n2\n1\n3\n" > /tmp/prisc_input.txt
rm -f /tmp/prisc_input_count.txt  # reset line counter
./prisc+x test_menu_demo.asm 2>&1
```

**Interactive use:** When run directly in a terminal, `layout_op` reads from `/dev/tty` and supports Ctrl+C to quit.

**Adding new menus:** Edit `custom_ops/layout_op.c` and add to `get_menu()` function.

## Creating Custom +x Operations

1. Create source in `custom_ops/`:
```c
// custom_ops/my_op.c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int arg = atoi(argv[1]);  // register value passed as arg
    printf("my_op: got %d\n", arg);
    return 0;
}
```

2. Compile to `+x/`:
```bash
gcc -o +x/my_op.+x custom_ops/my_op.c
```

3. Add to `default_op.txt`:
```
my_op void +x/my_op.+x {my custom operation}
```

## Directory Structure

```
prisc-xpanse/
├── prisc+x              # emulator binary
├── prisc+x.c            # main source
├── default_op.txt       # custom op definitions
├── asm/                 # assembly programs
│   ├── test1.asm
│   ├── test_all.asm
│   └── test_menu_demo.asm
├── custom_ops/          # .c source for custom ops
│   ├── layout_op/       # layout files for menu op
│   │   ├── layout.txt
│   │   └── layout_options.txt
│   ├── move_player.c
│   └── console_print.c
└── +x/                  # compiled .+x binaries
    ├── layout_op.+x
    ├── move_player.+x
    └── console_print.+x
```

## Example
```bash
# Run programs from asm/ directory
./prisc+x asm/test_all.asm

# Interactive menu demo
printf "2\n2\n1\n3\n" > /tmp/prisc_input.txt
rm -f /tmp/prisc_input_count.txt
./prisc+x asm/test_menu_demo.asm 2>&1

# With memory persistence
./prisc+x asm/program.asm /dev/null state.txt
```
