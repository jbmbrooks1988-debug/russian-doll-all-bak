#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char reg[4]; // 4 registers, 2-bit each
unsigned char mem[16]; // 16-byte memory
unsigned char pc; // Program counter

void nand_op(int bitwidth, unsigned char *a, unsigned char *b, unsigned char *out) {
    char cmd[100];
    sprintf(cmd, "./+x/nand.+x %d %u %u > temp.txt", bitwidth, *a, *b);
    system(cmd);
    FILE *fp = fopen("temp.txt", "r");
    fscanf(fp, "%hhu", out);
    fclose(fp);
}

int main() {
    // Initialize
    memset(reg, 0, sizeof(reg));
    memset(mem, 0, sizeof(mem));
    pc = 0;

    // Load program from in.txt
    FILE *in = fopen("in.txt", "r");
    for (int i = 0; i < 16 && !feof(in); i++) {
        fscanf(in, "%hhu", &mem[i]);
    }
    fclose(in);

    // Execute
    FILE *out = fopen("out.txt", "w");
    while (pc < 16) {
        unsigned char inst = mem[pc];
        unsigned char opcode = (inst >> 2) & 0x3; // 2-bit opcode
        unsigned char op1 = inst & 0x3; // 2-bit operand
        unsigned char op2 = mem[pc + 1] & 0x3; // Next byte for second operand
        pc++;

        if (opcode == 0) { // NAND reg[op1], reg[op2]
            nand_op(2, &reg[op1], &reg[op2], &reg[op1]);
        } else if (opcode == 1) { // LOAD reg[op1], mem[op2]
            reg[op1] = mem[op2] & 0x3;
        } else if (opcode == 2) { // STORE reg[op1], mem[op2]
            mem[op2] = reg[op1] & 0x3;
        } else if (opcode == 3) { // JUMP reg[op1]
            pc = reg[op1] & 0xF;
            continue;
        }

        fprintf(out, "PC: %hhu, REG: [%hhu,%hhu,%hhu,%hhu], MEM[0]: %hhu\n",
                pc, reg[0], reg[1], reg[2], reg[3], mem[0]);
    }
    fclose(out);
    return 0;
}
