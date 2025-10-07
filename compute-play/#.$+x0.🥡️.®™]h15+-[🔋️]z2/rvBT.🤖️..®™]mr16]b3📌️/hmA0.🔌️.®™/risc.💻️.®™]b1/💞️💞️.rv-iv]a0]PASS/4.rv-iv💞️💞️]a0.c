#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char reg[8]; // 8 registers, 4-bit each
unsigned char mem[64]; // 64-byte memory
unsigned char pc; // Program counter

void nand_op(int bitwidth, unsigned char *a, unsigned char *b, unsigned char *out) {
    char cmd[100];
    sprintf(cmd, "./+x/nand.+x %d %u %u > temp.txt", bitwidth, *a, *b);
    system(cmd);
    FILE *fp = fopen("temp.txt", "r");
    if (!fp) {
        printf("Error: Cannot open temp.txt\n");
        exit(1);
    }
    fscanf(fp, "%hhu", out);
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
    for (int i = 0; i < 64 && !feof(in); i++) {
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
    while (pc < 64 && steps < 100) {
        unsigned char inst = mem[pc];
        unsigned char opcode = (inst >> 4) & 0x3;
        unsigned char op1 = inst & 0x7;
        unsigned char op2 = (pc + 1 < 64) ? mem[pc + 1] : 0;
        unsigned char old_pc = pc;
        pc += 2;

        if (opcode == 0) { // NAND reg[op1], reg[op2]
            if (op2 >= 8) {
                fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%hhu\n", opcode, op1, op2);
                fprintf(out, "Error: Invalid register r%hhu\n", op2);
                break;
            }
            nand_op(4, &reg[op1], &reg[op2], &reg[op1]);
            fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%hhu\n", opcode, op1, op2);
            fprintf(out, "NAND r%hhu, r%hhu -> r%hhu\n", op1, op2, op1);
        } else if (opcode == 1) { // LOAD reg[op1], mem[op2]
            reg[op1] = mem[op2] & 0xF;
            fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%hhu\n", opcode, op1, op2);
            fprintf(out, "LOAD r%hhu, mem[%hhu]\n", op1, op2);
        } else if (opcode == 2) { // STORE reg[op1], mem[op2]
            mem[op2] = reg[op1] & 0xF;
            fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%hhu\n", opcode, op1, op2);
            fprintf(out, "STORE r%hhu, mem[%hhu]\n", op1, op2);
        } else if (opcode == 3) { // JUMP reg[op1]
            pc = (reg[op1] & 0x1F) * 2;
            fprintf(out, "EXEC: opcode=%hhu, op1=%hhu, op2=%hhu\n", opcode, op1, op2);
            fprintf(out, "JUMP r%hhu -> PC=%hhu\n", op1, pc);
        }
        fprintf(out, "PC: %hhu, REG: [%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu], MEM[0]: %hhu\n",
                pc, reg[0], reg[1], reg[2], reg[3], reg[4], reg[5], reg[6], reg[7], mem[0]);
        steps++;
    }
    if (steps >= 100) {
        fprintf(out, "Halted: Possible infinite loop\n");
    } else if (pc >= 64) {
        fprintf(out, "Halted: PC out of bounds\n");
    }
    fclose(out);
    return 0;
}
