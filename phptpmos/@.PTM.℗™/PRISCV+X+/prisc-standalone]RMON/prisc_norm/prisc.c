#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_INST 1024
#define MAX_LABELS 128
#define MEM_SIZE 4096

typedef enum { ADDI, BEQ, LW, SW, JALR, OUT, HALT } Op;

typedef struct {
    Op op;
    int rd, rs1, rs2, imm;
    char label_ref[32];
} Inst;

typedef struct {
    char name[32];
    int addr;
} Label;

int32_t regs[16] = {0};
int32_t mem[MEM_SIZE] = {0};
Inst program[MAX_INST];
Label labels[MAX_LABELS];
int label_count = 0, inst_count = 0;

int get_reg(char *name) {
    if (name[0] == 'x') return atoi(name + 1);
    return 0;
}

void parse_line(char *line, int pass) {
    char part[32], *p = line;
    if (sscanf(p, "%s", part) != 1) return;

    if (part[strlen(part) - 1] == ':') {
        if (pass == 1) {
            part[strlen(part) - 1] = '\0';
            strcpy(labels[label_count].name, part);
            labels[label_count++].addr = inst_count;
        }
        return;
    }

    if (pass == 2) {
        Inst *i = &program[inst_count];
        if (strcmp(part, "addi") == 0) {
            i->op = ADDI; sscanf(line, "%*s x%d, x%d, %d", &i->rd, &i->rs1, &i->imm);
        } else if (strcmp(part, "beq") == 0) {
            i->op = BEQ; sscanf(line, "%*s x%d, x%d, %s", &i->rs1, &i->rs2, i->label_ref);
        } else if (strcmp(part, "lw") == 0) {
            i->op = LW; sscanf(line, "%*s x%d, %d(x%d)", &i->rd, &i->imm, &i->rs1);
        } else if (strcmp(part, "sw") == 0) {
            i->op = SW; sscanf(line, "%*s x%d, %d(x%d)", &i->rs2, &i->imm, &i->rs1);
        } else if (strcmp(part, "jalr") == 0) {
            i->op = JALR; sscanf(line, "%*s x%d, x%d, %d", &i->rd, &i->rs1, &i->imm);
        } else if (strcmp(part, "out") == 0) {
            i->op = OUT; sscanf(line, "%*s x%d", &i->rs1);
        } else if (strcmp(part, "halt") == 0) {
            i->op = HALT;
        }
    }
    inst_count++;
}

int main(int argc, char **argv) {
    if (argc < 2) return printf("Usage: %s <prog> [output]\n", argv[0]), 1;
    FILE *f = fopen(argv[1], "r"), *out_f = argc > 2 ? fopen(argv[2], "w") : NULL;
    char line[128];

    while (fgets(line, sizeof(line), f)) parse_line(line, 1);
    rewind(f); inst_count = 0;
    while (fgets(line, sizeof(line), f)) parse_line(line, 2);
    fclose(f);

    int pc = 0;
    while (pc < inst_count) {
        Inst i = program[pc];
        regs[0] = 0; // Hardwired zero
        int next_pc = pc + 1;

        switch (i.op) {
            case ADDI: regs[i.rd] = regs[i.rs1] + i.imm; break;
            case BEQ: {
                int target = -1;
                for (int l = 0; l < label_count; l++)
                    if (strcmp(labels[l].name, i.label_ref) == 0) target = labels[l].addr;
                if (regs[i.rs1] == regs[i.rs2]) next_pc = target;
                break;
            }
            case LW:  regs[i.rd] = mem[(regs[i.rs1] + i.imm) % MEM_SIZE]; break;
            case SW:  mem[(regs[i.rs1] + i.imm) % MEM_SIZE] = regs[i.rs2]; break;
            case JALR: {
                int tmp = pc + 1;
                next_pc = regs[i.rs1] + i.imm;
                regs[i.rd] = tmp;
                break;
            }
            case OUT:
                printf("OUT: %d\n", regs[i.rs1]);
                if (out_f) fprintf(out_f, "%d\n", regs[i.rs1]);
                break;
            case HALT: pc = inst_count; continue;
        }
        pc = next_pc;
    }
    if (out_f) fclose(out_f);
    return 0;
}
