#include <stdio.h>
#include <string.h>

// Convert RV-16 instruction to HDLb0 16-bit binary
void generate_instruction(char *opcode, int rd, int rs1, int rs2, int imm) {
    int chip_location = 0, ram_output = 0, input_a = 0, input_b = 0;

    if (strcmp(opcode, "ADD") == 0) {
        // ADD r[rd] = r[rs1] + r[rs2]: Use NAND-based addition
        printf("0000000000000001 %016u %016u %016u # NAND r%d(%d), r%d(%d) -> RAM[%d]\n",
               rd, rs1, rs2, rs1, rs1, rs2, rs2, rd);
        printf("0000000000000001 %016u %016u %016u # NAND r%d(%d), RAM[%d] -> RAM[%d]\n",
               rd, rs1, rd, rs1, rs1, rd, rd);
        printf("0000000000000000 %000000000000000 %016u %0000000000000011 # Pass RAM[%d] to tape\n",
               rd, rd);
    } else if (strcmp(opcode, "ECALL") == 0) {
        printf("0000000000000101 %000000000000000 %0000000000000101 %0000000000000011 # ECALL\n");
    } else if (strcmp(opcode, "WRITE") == 0) {
        printf("0000000000000000 %000000000000000 %016u %0000000000000011 # Write %d to tape\n",
               imm, imm);
    } else {
        printf("Unknown opcode: %s\n", opcode);
    }
}

int main() {
    // Example: Generate ADD r1, r2, r3
    generate_instruction("ADD", 1, 2, 3, 0);
    // Example: Generate ECALL and WRITE 0x48 (H)
    generate_instruction("ECALL", 0, 0, 0, 0);
    generate_instruction("WRITE", 0, 0, 0, 0x48);
    return 0;
}