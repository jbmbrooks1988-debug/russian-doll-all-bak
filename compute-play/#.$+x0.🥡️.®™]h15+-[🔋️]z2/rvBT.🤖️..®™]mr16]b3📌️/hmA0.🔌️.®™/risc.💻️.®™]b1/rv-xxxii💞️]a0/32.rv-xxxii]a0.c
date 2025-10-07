
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int reg[32]; // 32-bit registers
unsigned char mem[1048576]; // 1MB memory
unsigned int pc;

void nand_op(int bitwidth, unsigned int *a, unsigned int *b, unsigned int *out) {
    char cmd[100];
    sprintf(cmd, "./+x/nand.+x %d %u %u > temp.txt", bitwidth, *a, *b);
    system(cmd);
    FILE *fp = fopen("temp.txt", "r");
    if (!fp) { printf("Error: Cannot open temp.txt\n"); exit(1); }
    fscanf(fp, "%u", out);
    fclose(fp);
}

int main(int argc, char *argv[]) {
    memset(reg, 0, sizeof(reg));
    memset(mem, 0, sizeof(mem));
    pc = 0;

    const char *input_file = (argc > 1) ? argv[1] : "in.txt";
    FILE *in = fopen(input_file, "r");
    if (!in) { printf("Error: Cannot open %s\n", input_file); return 1; }
    for (int i = 0; i < 1048576 && !feof(in); i++) fscanf(in, "%hhu", &mem[i]);
    fclose(in);

    FILE *out = fopen("out.txt", "w");
    int steps = 0;
    while (pc < 1048576 && steps < 100) {
        unsigned int inst = (mem[pc] << 24) | (mem[pc+1] << 16) | (mem[pc+2] << 8) | mem[pc+3];
        unsigned char opcode = inst & 0x7F; // Lower 7 bits
        unsigned char op1 = (inst >> 7) & 0x1F; // 5-bit register
        unsigned int op2 = (inst >> 15) & 0xFFFF; // 16-bit address
        if (opcode == 0) op2 = (inst >> 20) & 0x1F; // 5-bit register for NAND
        unsigned int old_pc = pc;
        pc += 4;

        if (opcode == 0) { // NAND
            if (op1 >= 32 || op2 >= 32) {
                fprintf(out, "Error: Invalid register\n");
                break;
            }
            nand_op(32, &reg[op1], &reg[op2], &reg[op1]);
            fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%u\nNAND r%hhu, r%u -> r%hhu\n", opcode, op1, op2, op1, op2, op1);
        } else if (opcode == 1) { // LW
            if (op1 >= 32 || op2 + 3 >= 1048576) {
                fprintf(out, "Error: Invalid %s\n", op1 >= 32 ? "register" : "memory address");
                break;
            }
            reg[op1] = (mem[op2] << 24) | (mem[op2+1] << 16) | (mem[op2+2] << 8) | mem[op2+3];
            fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%u\nLW r%hhu, mem[%u]\n", opcode, op1, op2, op1, op2);
        } else if (opcode == 2) { // SW
            if (op1 >= 32 || op2 + 3 >= 1048576) {
                fprintf(out, "Error: Invalid %s\n", op1 >= 32 ? "register" : "memory address");
                break;
            }
            mem[op2] = (reg[op1] >> 24) & 0xFF;
            mem[op2+1] = (reg[op1] >> 16) & 0xFF;
            mem[op2+2] = (reg[op1] >> 8) & 0xFF;
            mem[op2+3] = reg[op1] & 0xFF;
            fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%u\nSW r%hhu, mem[%u]\n", opcode, op1, op2, op1, op2);
        } else if (opcode == 3) { // JALR
            if (op1 >= 32) {
                fprintf(out, "Error: Invalid register\n");
                break;
            }
            pc = reg[op1] & 0xFFFFF; // 20-bit address
            fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%u\nJALR r%hhu -> PC=%u\n", opcode, op1, op2, op1, pc);
        } else {
            fprintf(out, "Error: Invalid opcode\n");
            break;
        }
        fprintf(out, "PC: %u, REG: [%u,%u,%u,%u,...], MEM[0]: %hhu\n", pc, reg[0], reg[1], reg[2], reg[3], mem[0]);
        steps++;
    }
    if (steps >= 100) fprintf(out, "Halted: Possible infinite loop\n");
    else if (pc >= 1048576) fprintf(out, "Halted: PC out of bounds\n");
    fclose(out);
    return 0;
}
