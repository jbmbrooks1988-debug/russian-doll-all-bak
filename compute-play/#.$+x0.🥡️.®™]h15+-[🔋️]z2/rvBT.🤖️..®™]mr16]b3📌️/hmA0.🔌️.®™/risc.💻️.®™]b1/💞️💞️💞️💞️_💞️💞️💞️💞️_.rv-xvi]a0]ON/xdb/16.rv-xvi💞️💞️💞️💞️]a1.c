#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned short reg[32]; // 32 registers, 16-bit each
unsigned char mem[65536]; // 65536-byte memory
unsigned int pc; // Program counter

void nand_op(int bitwidth, unsigned short *a, unsigned short *b, unsigned short *out) {
    char cmd[100];
    sprintf(cmd, "./+x/nand.+x %d %u %u > temp.txt", bitwidth, *a, *b);
    system(cmd);
    FILE *fp = fopen("temp.txt", "r");
    if (!fp) {
        printf("Error: Cannot open temp.txt\n");
        exit(1);
    }
    fscanf(fp, "%hu", out);
    fclose(fp);
}

int main(int argc, char *argv[]) {
    // Initialize
    memset(reg, 0, sizeof(reg));
    memset(mem, 0, sizeof(mem));
    pc = 0;

    // Use input file from argument or default to "in.txt"
    const char *input_file = (argc > 1) ? argv[1] : "in.txt";
    FILE *in = fopen(input_file, "r");
    if (!in) {
        printf("Error: Cannot open input file %s\n", input_file);
        return 1;
    }

    // Load program from input file
    for (int i = 0; i < 65536 && !feof(in); i++) {
        fscanf(in, "%hhu", &mem[i]);
    }
    fclose(in);

    // Execute
    FILE *out = fopen("out.txt", "w");
    if (!out) {
        printf("Error: Cannot open out.txt\n");
        return 1;
    }
    int steps = 0;
    while (pc < 65536 && steps < 100) {
        unsigned char inst = mem[pc];
        unsigned char opcode = (inst >> 6) & 0x3;
        unsigned short op1 = mem[pc] & 0x1F; // 5 bits for register index
        unsigned short op2 = (mem[pc + 2] << 8) | mem[pc + 3]; // Full address for LOAD/STORE/JUMP
        if (opcode == 0) op2 = mem[pc + 2] & 0x1F; // Limit to register index for NAND
        unsigned int old_pc = pc;
        pc += 4;

        if (opcode == 0) { // NAND reg[op1], reg[op2]
            if (op1 >= 32 || op2 >= 32) {
                fprintf(out, "EXEC: opcode=%hhu, op1=%hu, op2=%hu\n", opcode, op1, op2);
                fprintf(out, "Error: Invalid register r%hu\n", op1 >= 32 ? op1 : op2);
                break;
            }
            nand_op(16, &reg[op1], &reg[op2], &reg[op1]);
            fprintf(out, "EXEC: opcode=%hhu, op1=%hu, op2=%hu\n", opcode, op1, op2);
            fprintf(out, "NAND r%hu, r%hu -> r%hu\n", op1, op2, op1);
        } else if (opcode == 1) { // LOAD reg[op1], mem[op2]
            if (op1 >= 32 || op2 + 1 >= 65536) {
                fprintf(out, "EXEC: opcode=%hhu, op1=%hu, op2=%hu\n", opcode, op1, op2);
                fprintf(out, "Error: Invalid %s\n", op1 >= 32 ? "register" : "memory address");
                break;
            }
            reg[op1] = (mem[op2] << 8) | (op2 + 1 < 65536 ? mem[op2 + 1] : 0);
            reg[op1] &= 0xFFFF;
            fprintf(out, "EXEC: opcode=%hhu, op1=%hu, op2=%hu\n", opcode, op1, op2);
            fprintf(out, "LOAD r%hu, mem[%hu]\n", op1, op2);
        } else if (opcode == 2) { // STORE reg[op1], mem[op2]
            if (op1 >= 32 || op2 + 1 >= 65536) {
                fprintf(out, "EXEC: opcode=%hhu, op1=%hu, op2=%hu\n", opcode, op1, op2);
                fprintf(out, "Error: Invalid %s\n", op1 >= 32 ? "register" : "memory address");
                break;
            }
            mem[op2] = (reg[op1] >> 8) & 0xFF;
            mem[op2 + 1] = reg[op1] & 0xFF;
            fprintf(out, "EXEC: opcode=%hhu, op1=%hu, op2=%hu\n", opcode, op1, op2);
            fprintf(out, "STORE r%hu, mem[%hu]\n", op1, op2);
        } else if (opcode == 3) { // JUMP reg[op1]
            if (op1 >= 32) {
                fprintf(out, "EXEC: opcode=%hhu, op1=%hu, op2=%hu\n", opcode, op1, op2);
                fprintf(out, "Error: Invalid register r%hu\n", op1);
                break;
            }
            pc = (reg[op1] & 0x7FFF) * 2;
            fprintf(out, "EXEC: opcode=%hhu, op1=%hu, op2=%hu\n", opcode, op1, op2);
            fprintf(out, "JUMP r%hu -> PC=%u\n", op1, pc);
        }
        fprintf(out, "PC: %u, REG: [%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu], MEM[0]: %hhu\n",
                pc, reg[0], reg[1], reg[2], reg[3], reg[4], reg[5], reg[6], reg[7],
                reg[8], reg[9], reg[10], reg[11], reg[12], reg[13], reg[14], reg[15],
                reg[16], reg[17], reg[18], reg[19], reg[20], reg[21], reg[22], reg[23],
                reg[24], reg[25], reg[26], reg[27], reg[28], reg[29], reg[30], reg[31], mem[0]);
        steps++;
    }
    if (steps >= 100) {
        fprintf(out, "Halted: Possible infinite loop\n");
    } else if (pc >= 65536) {
        fprintf(out, "Halted: PC out of bounds\n");
    }
    fclose(out);
    return 0;
}
