# HDLb0 Emulator and NAND Chip 🐉💾

*Crafted in the Cosmic Dojo, August 13, 2025* 🌌🙏

Welcome, bit guardians, to the HDLb0 emulator (`0.hdlb0.☮️16]pr3]HOLY.+x`) and its NAND chip (`nand]z0]FIXD.c`, compiled as `nand`)! This README confirms their working state, outlines usage, and notes reserved inputs for the RV-16 architecture. The emulator runs programs like `nand_switch_test.txt` and `d_ff.txt`, using NAND logic to process and store bits, with output prepended to `cli_tape.txt`.

## 🛠️ Working State

- **Emulator (`0.hdlb0.☮️16]pr3]HOLY.c`)**:
  - Reads programs (e.g., `nand_switch_test.txt`) with 16-bit instructions: chip_location, ram_output_address, input_a, input_b.
  - Supports inputs: 0 (0), 1 (1), switch_0 (5), switch_1 (6), clock (7), RAM[0-255] (>15).
  - **Reserved Inputs**: 2 and 3 are blank (future use: 2=high-Z, 3=saturation cutoff), causing NAND errors if used.
  - Toggles clock (0/1) per cycle, writes RAM to `ram_output_address.txt`, and prepends tape output to `cli_tape.txt`.
  - **Verification**: `nand_switch_test.txt` outputs correct NAND(switch_0, switch_1) to tape (e.g., `01111` for NAND(1,1)=0).

- **NAND Chip (`nand.c`)**:
  - Computes `!(input_a & input_b)` for inputs from emulator.
  - For `ram_output_address=0`, prepends output to `cli_tape.txt` without newlines, matching emulator’s format.
  - For `ram_output_address>0`, updates `ram_output_address.txt` at specified address.
  - **Verification**: Correctly prepends NAND results (e.g., `0` for NAND(1,1), `1` for NAND(0,0)) in `nand_switch_test.txt`.

- **Dependencies**:
  - Files: `chip_bank.txt` (lists `nand`), `ram_output_address.txt`, `cli_tape.txt`.
  - Compiler: `gcc`.

## 🚀 Usage

1. **Compile**:
   ```bash
   gcc -o +x/0.hdlb0.☮️16]pr3]HOLY.+x 0.hdlb0.☮️16]pr3]HOLY.c
   gcc -o nand nand]z0]FIXD.c
   ```

2. **Setup**:
   ```bash
   echo "nand" > chip_bank.txt
   echo "0" > ram_output_address.txt
   echo "" > cli_tape.txt
   ```

3. **Run Test**:
   ```bash
   ./+x/0.hdlb0.☮️16]pr3]HOLY.+x nand_switch_test.txt
   ```
   - Input `1` (flip switch_0), `2` (flip switch_1), `s` (step).
   - Observe tape: `01111` for switch_0=1, switch_1=1 (NAND(1,1)=0).

4. **Notes**:
   - Use RAM addresses ≥16 to avoid reserved inputs (2, 3).
   - Tape output prepends (newest bit first).
   - Check `ram_output_address.txt` for RAM state.

## ⚠️ Reserved Inputs
- Inputs 2 and 3 are blank (return -1, marked blank) for future use:
  - 2: High-Z (planned).
  - 3: Saturation cutoff (planned).
- Use `RAM[16]` and above for safe memory access.

## 🐞 Troubleshooting
- **"Logic chip requires non-blank inputs"**: Avoid inputs 2, 3 in NAND instructions; use `RAM[16]+`.
- **Incorrect Tape Output**: Ensure `nand.c` prepends to `cli_tape.txt`. Check `chip_bank.txt` lists `nand`.
- **Debug**: Step (`s`), check `cli_tape.txt` and `ram_output_address.txt`.

*“Each bit is a spark in the RV-16 flame!”* — Grand Master Dahyun 🥋🙏