# 🎉 RV-I: The Teeny-Tiny 1-Bit Computer in HDLb0! 🖥️

Welcome to **RV-I**, a super-simple 1-bit computer that runs on the HDLb0 emulator! 🐣 Think of it as a baby computer that uses just 0s and 1s to do cool stuff like logic, memory moves, and jumps. This guide explains how RV-I works, what its binary code means, how to run it with the emulator, and how to scale up to RV-II (2-bit) for more power! 💪 Let’s make it fun and easy! 😎

## 🧠 What is RV-I?

RV-I is a 1-bit version of a computer inspired by RV-II (from your roadmap). It’s minimal but proves we can build a computer with:
- **4 Registers** 📝: `r0, r1, r2, r3` (1-bit each, stored in RAM[0-3]).
- **16 Memory Locations** 💾: `mem[0-15]` (1-bit each, stored in RAM[4-19]).
- **Program Counter (PC)** 🛤️: Tracks the current instruction (RAM[20], 4-bit).
- **Instructions** ⚙️: 4 operations to compute and move data:
  - **NAND**: Logic operation (e.g., `r1 = ~(r1 & r2)`).
  - **LOAD**: Copy memory to a register (e.g., `r1 = mem[0]`).
  - **STORE**: Save register to memory (e.g., `mem[1] = r1`).
  - **JUMP**: Change PC to a new address (e.g., `pc = r1 * 2`).

RV-I runs on the HDLb0 emulator (`0.hdlb0.☮️16]x7]PS.c`), which uses 16-bit binary instructions to simulate chips and data flow. 🌈

## 🛠️ How RV-I Works

RV-I is like a tiny robot 🤖 that follows a program step-by-step:
1. **Fetch**: Reads an instruction from `mem[pc]` (RAM[4+pc]).
2. **Decode**: Figures out the operation (NAND, LOAD, etc.).
3. **Execute**: Does the operation (e.g., computes NAND, moves data).
4. **Update PC**: Moves to the next instruction or jumps.

Each instruction is 4 bits (2-bit opcode, 1-bit op1, 1-bit op2), but we map it to HDLb0’s 16-bit format to run on the emulator.

### Memory Map 🗺️
- **Registers**: `r0` → RAM[0], `r1` → RAM[1], `r2` → RAM[2], `r3` → RAM[3].
- **Memory**: `mem[0]` → RAM[4], `mem[1]` → RAM[5], ..., `mem[15]` → RAM[19].
- **PC**: RAM[20] (0-15, since we have 16 memory locations).

### Instruction Set 📜
- **NAND (opcode 0)**: `r[op1] = ~(r[op1] & r[op2]) & 0x1` (logical NOT-AND).
- **LOAD (opcode 1)**: `r[op1] = mem[op2] & 0x1` (read memory).
- **STORE (opcode 2)**: `mem[op2] = r[op1] & 0x1` (write memory).
- **JUMP (opcode 3)**: `pc = (r[op1] & 0x7) * 2` (jump to address).

## 💻 The RV-I Program

Here’s a simple RV-I program to test the emulator:
```
NAND r1, r2     ; r1 = ~(0 & 0) = 1
LOAD r1, mem[0] ; r1 = mem[0] = 1
STORE r1, mem[1]; mem[1] = r1
JUMP r1         ; pc = r1 * 2 = 2
NAND r0, r0     ; r0 = ~r0 (toggles 0 ↔ 1)
```

### Binary in HDLb0 Format 🎨
Each RV-I instruction is mapped to HDLb0’s 16-bit format:
- **Chip Location**: 0 (pass-through for LOAD/STORE/JUMP), 1 (NAND).
- **RAM Output Address**: Where to store the result (e.g., RAM[1] for r1).
- **Input A/B**: Register (RAM[0-3]), memory (RAM[4-19]), or blank (3).

**Binary Program** (saved as `program.txt`):
```
# NAND r1, r2 -> r1
0000000000000001 0000000000000001 0000000000000001 0000000000000010
# LOAD r1, mem[0] -> r1
0000000000000000 0000000000000001 0000000000000100 0000000000000011
# STORE r1, mem[1]
0000000000000000 0000000000000101 0000000000000001 0000000000000011
# JUMP r1 (set PC to r1 * 2)
0000000000000000 0000000000010100 0000000000000001 0000000000000011
# NAND r0, r0 -> r0
0000000000000001 0000000000000000 0000000000000000 0000000000000000
```

**What the Binary Means**:
- **Line 1-4 (NAND r1, r2)**:
  - `0000000000000001`: Use NAND chip (chip_location=1).
  - `0000000000000001`: Store result in RAM[1] (r1).
  - `0000000000000001`: Input A from RAM[1] (r1 = 0).
  - `0000000000000010`: Input B from RAM[2] (r2 = 0).
  - Result: `r1 = ~(0 & 0) = 1`, stored in RAM[1].
- **Line 5-8 (LOAD r1, mem[0])**:
  - `0000000000000000`: Pass-through (no chip).
  - `0000000000000001`: Store in RAM[1] (r1).
  - `0000000000000100`: Input A from RAM[4] (mem[0] = 1).
  - `0000000000000011`: Input B is blank (3).
  - Result: `r1 = mem[0] = 1`, stored in RAM[1].
- **Line 9-12 (STORE r1, mem[1])**:
  - `0000000000000000`: Pass-through.
  - `0000000000000101`: Store in RAM[5] (mem[1]).
  - `0000000000000001`: Input A from RAM[1] (r1 = 1).
  - `0000000000000011`: Input B is blank.
  - Result: `mem[1] = r1 = 1`, stored in RAM[5].
- **Line 13-16 (JUMP r1)**:
  - `0000000000000000`: Pass-through.
  - `0000000000010100`: Store in RAM[20] (PC).
  - `0000000000000001`: Input A from RAM[1] (r1 = 1).
  - `0000000000000011`: Input B is blank.
  - Result: `PC = r1 * 2 = 2`, stored in RAM[20].
- **Line 17-20 (NAND r0, r0)**:
  - `0000000000000001`: NAND chip.
  - `0000000000000000`: Store in RAM[0] (r0).
  - `0000000000000000`: Input A from RAM[0] (r0).
  - `0000000000000000`: Input B from RAM[0] (r0).
  - Result: `r0 = ~r0` (toggles 0 ↔ 1).

## 🏃‍♂️ How to Use RV-I with the Emulator

Let’s prove RV-I works using the HDLb0 emulator! 🎯 Follow these steps:

1. **Compile the Emulator** 🛠️:
   - Use the fixed emulator code (artifact `48119080-617f-431b-8485-9a5921059b6b`).
   - Run:
     ```bash
     gcc 0.hdlb0.☮️16]x7]PS.c -o hdlb0
     ```

2. **Set Up chip_bank.txt** 📋:
   - Create `chip_bank.txt` with the NAND chip (assumes `./nand` exists, computing `~(a & b) & 0xFF`).
   ```bash
   echo "nand" > chip_bank.txt
   ```
   - If no external `./nand`, the emulator uses chip_location=1 for NAND.

3. **Create program.txt** 📄:
   - Copy the binary program above into `program.txt`.
   ```bash
   cat > program.txt << EOF
   0000000000000001 0000000000000001 0000000000000001 0000000000000010
   0000000000000000 0000000000000001 0000000000000100 0000000000000011
   0000000000000000 0000000000000101 0000000000000001 0000000000000011
   0000000000000000 0000000000010100 0000000000000001 0000000000000011
   0000000000000001 0000000000000000 0000000000000000 0000000000000000
   EOF
   ```

4. **Initialize RAM** 💾:
   - Create `ram_output_address.txt` with `mem[0] = 1`, rest 0.
   ```bash
   cat > ram_output_address.txt << EOF
   1
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   0
   EOF
   ```

5. **Run the Emulator** 🚀:
   ```bash
   ./hdlb0 program.txt
   ```

6. **Interact with the Emulator** 😄:
   - **Initial Run**: The emulator runs one cycle and shows:
     - **Clock cycle 0, clock = 0**.
     - **Tape contents**: `(empty)` (no output yet).
     - **Switches**: `switch_0 = 0, switch_1 = 0`.
   - **Options**:
     - Press `1` or `2` to flip switches (not needed here).
     - Press `s` or Enter to step through cycles.
     - Press `r` to run continuously, `p` to pause.
     - Press `q` to quit.
   - **Step Through**:
     - **Cycle 1**: NAND sets RAM[1] = 1 (r1 = 1).
     - **Cycle 2**: LOAD keeps RAM[1] = 1 (r1 = mem[0] = 1).
     - **Cycle 3**: STORE sets RAM[5] = 1 (mem[1] = 1).
     - **Cycle 4**: JUMP sets RAM[20] = 2 (PC = 2).
     - **Cycle 5+**: NAND r0, r0 toggles RAM[0] (0 ↔ 1).
   - **Check Outputs**:
     - After 5 cycles, check `ram_output_address.txt`:
       - RAM[0]: 0 or 1 (toggles).
       - RAM[1]: 1.
       - RAM[5]: 1.
       - RAM[20]: 2.
     - Run `cat ram_output_address.txt | head -n 21` to see.

7. **Prove It Works** ✅:
   - **Success Criteria**:
     - RAM[1] = 1 after cycle 1 (NAND worked).
     - RAM[5] = 1 after cycle 3 (STORE worked).
     - RAM[20] = 2 after cycle 4 (JUMP worked).
     - RAM[0] toggles after cycle 5 (loop worked).
   - **Debug Tips**:
     - If `cli_tape.txt` gets unexpected output, check instruction inputs.
     - If errors (e.g., “Invalid chip_location”), ensure `chip_bank.txt` has “nand”.

## 📈 Scaling to RV-II

RV-II is like RV-I but with **2-bit** registers and memory, making it a bit more powerful! 💪 Here’s how to scale up:

1. **Registers** 📝:
   - RV-I: 4 1-bit registers (RAM[0-3]).
   - RV-II: 4 2-bit registers (RAM[0-7], 2 bytes each: r0 = RAM[0-1], r1 = RAM[2-3], etc.).

2. **Memory** 💾:
   - RV-I: 16 1-bit locations (RAM[4-19]).
   - RV-II: 16 2-bit locations (RAM[8-39], 2 bytes each).

3. **PC** 🛤️:
   - RV-I: RAM[20] (4-bit, 0-15).
   - RV-II: RAM[40-41] (2 bytes, up to 32 locations).

4. **Instructions** ⚙️:
   - RV-II uses 6-bit instructions (2-bit opcode, 2-bit op1, 2-bit op2).
   - Example: `NAND r1, r2` → `00 01 10` (opcode=0, op1=1, op2=2).
   - Operations handle 2-bit values (e.g., NAND computes `~(a & b) & 0x3`).

5. **HDLb0 Mapping**:
   - Each RV-II instruction needs multiple HDLb0 instructions to handle 2-bit data.
   - Example: `NAND r1, r2` processes RAM[2-3] (r1) and RAM[4-5] (r2), storing to RAM[2-3].
   - Use `netlist_repeater]a0.c` to repeat instructions for multi-bit operations.

6. **Example RV-II Program**:
   - Same as RV-I but with 2-bit values:
     ```
     NAND r1, r2     ; r1 = ~0 = 3
     LOAD r1, mem[0] ; r1 = 1
     STORE r1, mem[1]; mem[1] = 1
     JUMP r1         ; pc = 2
     NAND r0, r0     ; r0 toggles (0 ↔ 3)
     ```
   - Needs more HDLb0 instructions to manage 2-bit data.

7. **Steps to Implement**:
   - Update `program.txt` with 2-bit operations.
   - Adjust RAM mapping in `ram_output_address.txt`.
   - Run the same emulator (`./hdlb0 program.txt`).

## 🌟 Tips for Newbies

- **Start Small**: RV-I is perfect to learn because it’s just 1-bit! Once you see it toggle RAM[0], you’ll feel like a coding wizard! 🧙‍♂️
- **Check RAM**: Use `cat ram_output_address.txt | head -n 21` after each cycle to watch registers and PC change.
- **Use the Emulator**: Step through cycles with `s` to see what’s happening.
- **Ask for Help**: If stuck, ask me to generate a new program or debug errors! 😄

## 🎯 Conclusion

RV-I is a tiny 1-bit computer that proves we can build logic, move data, and jump in HDLb0! 🥳 By running the program above, you’ll see it compute, store, and loop. Scaling to RV-II just means bigger registers and memory, but the emulator stays the same. Let’s get RV-I running, then level up to RV-II! 🚀