#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 32
#define REG_COUNT 32
#define MEM_SIZE 32768

int Nand(int a, int b) { return !(a && b); }
int Not(int in) { return Nand(in, in); }
int And(int a, int b) { int temp = Nand(a, b); return Not(temp); }
int Or(int a, int b) { int notA = Not(a); int notB = Not(b); return Nand(notA, notB); }
int Xor(int a, int b) { int temp1 = Nand(a, b); int temp2 = Nand(a, temp1); int temp3 = Nand(b, temp1); return Nand(temp2, temp3); }
int Mux(int a, int b, int sel) { int notSel = Not(sel); int temp1 = Nand(a, notSel); int temp2 = Nand(b, sel); return Nand(temp1, temp2); }

void Mux32(int a[WIDTH], int b[WIDTH], int sel, int out[WIDTH]) {
    for (int i = 0; i < WIDTH; i++) out[i] = Mux(a[i], b[i], sel);
}

void FullAdder(int a, int b, int c, int* sum, int* carry) {
    int temp1 = Xor(a, b);
    *sum = Xor(temp1, c);
    int temp2 = And(a, b);
    int temp3 = And(temp1, c);
    *carry = Or(temp2, temp3);
}

void Add32(int a[WIDTH], int b[WIDTH], int out[WIDTH]) {
    int carry = 0;
    for (int i = 0; i < WIDTH; i++) FullAdder(a[i], b[i], carry, &out[i], &carry);
}

void Sub32(int a[WIDTH], int b[WIDTH], int out[WIDTH]) {
    int neg_b[WIDTH], one[WIDTH] = {0}; one[0] = 1;
    for (int i = 0; i < WIDTH; i++) neg_b[i] = Not(b[i]);
    Add32(neg_b, one, neg_b);
    Add32(a, neg_b, out);
}

void And32(int a[WIDTH], int b[WIDTH], int out[WIDTH]) {
    for (int i = 0; i < WIDTH; i++) out[i] = And(a[i], b[i]);
}

void Or32(int a[WIDTH], int b[WIDTH], int out[WIDTH]) {
    for (int i = 0; i < WIDTH; i++) out[i] = Or(a[i], b[i]);
}

void Xor32(int a[WIDTH], int b[WIDTH], int out[WIDTH]) {
    for (int i = 0; i < WIDTH; i++) out[i] = Xor(a[i], b[i]);
}

void Sll32(int a[WIDTH], int shamt[5], int out[WIDTH]) {
    int shift = shamt[0] + (shamt[1] << 1) + (shamt[2] << 2) + (shamt[3] << 3) + (shamt[4] << 4);
    for (int i = 0; i < WIDTH; i++) out[i] = (i - shift >= 0 && i - shift < WIDTH) ? a[i - shift] : 0;
}

void Srl32(int a[WIDTH], int shamt[5], int out[WIDTH]) {
    int shift = shamt[0] + (shamt[1] << 1) + (shamt[2] << 2) + (shamt[3] << 3) + (shamt[4] << 4);
    for (int i = 0; i < WIDTH; i++) out[i] = (i + shift < WIDTH) ? a[i + shift] : 0;
}

void Sra32(int a[WIDTH], int shamt[5], int out[WIDTH]) {
    int shift = shamt[0] + (shamt[1] << 1) + (shamt[2] << 2) + (shamt[3] << 3) + (shamt[4] << 4);
    int sign = a[WIDTH - 1];
    for (int i = 0; i < WIDTH; i++) out[i] = (i + shift < WIDTH) ? a[i + shift] : sign;
}

int LessThan32(int a[WIDTH], int b[WIDTH]) {
    int diff[WIDTH];
    Sub32(a, b, diff);
    return diff[WIDTH - 1];
}

int LessThanU32(int a[WIDTH], int b[WIDTH]) {
    for (int i = WIDTH - 1; i >= 0; i--) {
        if (a[i] < b[i]) return 1;
        if (a[i] > b[i]) return 0;
    }
    return 0;
}

void ALU32(int x[WIDTH], int y[WIDTH], int out[WIDTH], int funct3[3], int funct7_5) {
    int add[WIDTH], sub[WIDTH], and[WIDTH], or[WIDTH], xor[WIDTH], sll[WIDTH], srl[WIDTH], sra[WIDTH];
    int shamt[5] = {y[0], y[1], y[2], y[3], y[4]};
    Add32(x, y, add);
    Sub32(x, y, sub);
    And32(x, y, and);
    Or32(x, y, or);
    Xor32(x, y, xor);
    Sll32(x, shamt, sll);
    Srl32(x, shamt, srl);
    Sra32(x, shamt, sra);
    switch ((funct3[2] << 2) | (funct3[1] << 1) | funct3[0]) {
        case 0b000: Mux32(add, sub, funct7_5, out); break;
        case 0b010: for (int i = 0; i < WIDTH; i++) out[i] = LessThan32(x, y); break;
        case 0b011: for (int i = 0; i < WIDTH; i++) out[i] = LessThanU32(x, y); break;
        case 0b001: memcpy(out, sll, WIDTH * sizeof(int)); break;
        case 0b101: Mux32(srl, sra, funct7_5, out); break;
        case 0b100: memcpy(out, xor, WIDTH * sizeof(int)); break;
        case 0b110: memcpy(out, or, WIDTH * sizeof(int)); break;
        case 0b111: memcpy(out, and, WIDTH * sizeof(int)); break;
    }
}

void Register32(int in[WIDTH], int load, int out[WIDTH], int state[WIDTH]) {
    for (int i = 0; i < WIDTH; i++) {
        state[i] = Mux(state[i], in[i], load);
        out[i] = state[i];
    }
}

void PC32(int in[WIDTH], int load, int inc, int reset, int out[WIDTH], int state[WIDTH]) {
    int one[WIDTH] = {0}; one[0] = 1;
    int next[WIDTH], temp1[WIDTH], temp2[WIDTH];
    Add32(state, one, next);
    Mux32(state, next, inc, temp1);
    Mux32(temp1, in, load, temp2);
    Mux32(temp2, one, reset, temp1);
    Register32(temp1, 1, out, state);
}

typedef struct {
    int regs[REG_COUNT][WIDTH];
} RegisterFile;

void RegFileRead(RegisterFile* rf, int addrA[5], int addrB[5], int outA[WIDTH], int outB[WIDTH]) {
    int a_idx = addrA[0] + (addrA[1] << 1) + (addrA[2] << 2) + (addrA[3] << 3) + (addrA[4] << 4);
    int b_idx = addrB[0] + (addrB[1] << 1) + (addrB[2] << 2) + (addrB[3] << 3) + (addrB[4] << 4);
    for (int i = 0; i < WIDTH; i++) {
        outA[i] = (a_idx == 0) ? 0 : rf->regs[a_idx][i];
        outB[i] = (b_idx == 0) ? 0 : rf->regs[b_idx][i];
    }
}

void RegFileWrite(RegisterFile* rf, int addr[5], int in[WIDTH], int write) {
    int idx = addr[0] + (addr[1] << 1) + (addr[2] << 2) + (addr[3] << 3) + (addr[4] << 4);
    if (write && idx != 0) {
        for (int i = 0; i < WIDTH; i++) rf->regs[idx][i] = in[i];
    }
}

char rom_lines[MEM_SIZE][WIDTH + 1];
int rom_size = 0;

void ROM32K_init() {
    FILE* fp = fopen("rom.txt", "r");
    if (!fp) { printf("Failed to open rom.txt\n"); exit(1); }
    rom_size = 0;
    char line[WIDTH + 2];
    while (fgets(line, WIDTH + 2, fp) && rom_size < MEM_SIZE) {
        int len = strlen(line);
        for (int i = len - 1; i >= 0; i--) {
            if (line[i] == '\n' || line[i] == '\r' || line[i] == ' ') line[i] = '\0';
            else break;
        }
        len = strlen(line);
        if (len != WIDTH) continue;
        int valid = 1;
        for (int i = 0; i < WIDTH; i++) {
            if (line[i] != '0' && line[i] != '1') {
                valid = 0;
                break;
            }
        }
        if (valid) {
            strcpy(rom_lines[rom_size], line);
            rom_size++;
        }
    }
    fclose(fp);
    printf("ROM size: %d\n", rom_size);
    for (int i = 0; i < rom_size; i++) printf("ROM[%d]: %s\n", i, rom_lines[i]);
}

void ROM32K(int addr[WIDTH], int out[WIDTH]) {
    int addr_int = addr[0] + (addr[1] << 1) + (addr[2] << 2);
    if (addr_int >= rom_size) {
        memset(out, 0, WIDTH * sizeof(int));
        return;
    }
    for (int i = 0; i < WIDTH; i++) out[i] = (rom_lines[addr_int][WIDTH - 1 - i] == '1') ? 1 : 0;
}

void RAM32K_init() {
    FILE* fp = fopen("ram.txt", "w");
    if (!fp) { printf("Failed to create ram.txt\n"); exit(1); }
    fprintf(fp, "00000000000000000000000000000000\n");
    fclose(fp);
}

void RAM32K_load(int addr[WIDTH], int out[WIDTH], int funct3[3]) {
    FILE* fp = fopen("ram.txt", "r");
    if (!fp) { memset(out, 0, WIDTH * sizeof(int)); return; }
    int addr_int = addr[0] + (addr[1] << 1) + (addr[2] << 2);
    int byte_offset = addr[0] + (addr[1] << 1);
    char line[WIDTH + 2];
    for (int i = 0; i < addr_int; i++) {
        if (fgets(line, WIDTH + 2, fp) == NULL) { memset(out, 0, WIDTH * sizeof(int)); fclose(fp); return; }
    }
    if (fgets(line, WIDTH + 2, fp) == NULL) {
        memset(out, 0, WIDTH * sizeof(int));
    } else {
        line[WIDTH] = '\0';
        int temp[WIDTH];
        for (int j = 0; j < WIDTH; j++) temp[j] = (line[WIDTH - 1 - j] == '1') ? 1 : 0;
        int funct3_val = (funct3[2] << 2) | (funct3[1] << 1) | funct3[0];
        switch (funct3_val) {
            case 0b000: for (int i = 0; i < 8; i++) out[i] = temp[byte_offset * 8 + i]; for (int i = 8; i < WIDTH; i++) out[i] = temp[7]; break;
            case 0b001: for (int i = 0; i < 16; i++) out[i] = temp[byte_offset * 8 + i]; for (int i = 16; i < WIDTH; i++) out[i] = temp[15]; break;
            case 0b010: memcpy(out, temp, WIDTH * sizeof(int)); break;
            case 0b100: for (int i = 0; i < 8; i++) out[i] = temp[byte_offset * 8 + i]; for (int i = 8; i < WIDTH; i++) out[i] = 0; break;
            case 0b101: for (int i = 0; i < 16; i++) out[i] = temp[byte_offset * 8 + i]; for (int i = 16; i < WIDTH; i++) out[i] = 0; break;
        }
    }
    fclose(fp);
}

void RAM32K_store(int addr[WIDTH], int in[WIDTH], int load, int funct3[3]) {
    if (!load) return;
    FILE* fp = fopen("ram.txt", "r");
    if (!fp) { fp = fopen("ram.txt", "w"); if (!fp) { printf("Failed to create ram.txt\n"); exit(1); } }
    char lines[MEM_SIZE][WIDTH + 2];
    int line_count = 0;
    while (fgets(lines[line_count], WIDTH + 2, fp) && line_count < MEM_SIZE) {
        lines[line_count][WIDTH] = '\0';
        line_count++;
    }
    fclose(fp);
    int addr_int = addr[0] + (addr[1] << 1) + (addr[2] << 2);
    int byte_offset = addr[0] + (addr[1] << 1);
    while (line_count <= addr_int) strcpy(lines[line_count++], "00000000000000000000000000000000\n");
    int temp[WIDTH];
    for (int i = 0; i < WIDTH; i++) temp[i] = (lines[addr_int][WIDTH - 1 - i] == '1') ? 1 : 0;
    int funct3_val = (funct3[2] << 2) | (funct3[1] << 1) | funct3[0];
    switch (funct3_val) {
        case 0b000: for (int i = 0; i < 8; i++) temp[byte_offset * 8 + i] = in[i]; break;
        case 0b001: for (int i = 0; i < 16; i++) temp[byte_offset * 8 + i] = in[i]; break;
        case 0b010: memcpy(temp, in, WIDTH * sizeof(int)); break;
    }
    for (int i = 0; i < WIDTH; i++) lines[addr_int][i] = temp[WIDTH - 1 - i] ? '1' : '0';
    lines[addr_int][WIDTH] = '\n';
    fp = fopen("ram.txt", "w");
    for (int i = 0; i < line_count; i++) fprintf(fp, "%s", lines[i]);
    fclose(fp);
    fp = fopen("ram.txt", "r");
    char ram_line[WIDTH + 2];
    fgets(ram_line, WIDTH + 2, fp);
    ram_line[WIDTH] = '\0';
    printf("RAM[0]: %s\n", ram_line);
    fclose(fp);
}

void str_to_bits(char* str, int bits[WIDTH]) {
    for (int i = 0; i < WIDTH; i++) bits[WIDTH - 1 - i] = (str[i] == '1') ? 1 : 0;
}

void bits_to_str(int bits[WIDTH], char* str) {
    for (int i = 0; i < WIDTH; i++) str[i] = bits[WIDTH - 1 - i] ? '1' : '0';
    str[WIDTH] = '\0';
}

void print_bits(int bits[WIDTH], const char* label) {
    char str[WIDTH + 1];
    bits_to_str(bits, str);
    printf("%s: %s\n", label, str);
}

int main(int argc, char* argv[]) {
    if (argc != 4) { 
        printf("Usage: %s <input file> <output file> <cycles> (-1 for infinite)\n", argv[0]); 
        return 1; 
    }

    long max_cycles = atol(argv[3]);
    if (max_cycles < -1) { 
        printf("Cycles must be -1 or a non-negative integer\n"); 
        return 1; 
    }
    int infinite = (max_cycles == -1);

    RAM32K_init();
    ROM32K_init();

    FILE* in_fp = fopen(argv[1], "r");
    if (!in_fp) { printf("Failed to open input file %s\n", argv[1]); return 1; }
    char x1_str[WIDTH + 2], x2_str[WIDTH + 2];
    if (fgets(x1_str, WIDTH + 2, in_fp) == NULL || fgets(x2_str, WIDTH + 2, in_fp) == NULL ||
        strlen(x1_str) != WIDTH + 1 || strlen(x2_str) != WIDTH + 1) {
        printf("Input file must contain two 32-bit binary strings\n");
        fclose(in_fp);
        return 1;
    }
    fclose(in_fp);
    x1_str[WIDTH] = '\0';
    x2_str[WIDTH] = '\0';

    RegisterFile rf = {0};
    str_to_bits(x1_str, rf.regs[1]);
    str_to_bits(x2_str, rf.regs[2]);
    int pc[WIDTH] = {0}, pc_state[WIDTH] = {0};

    int instr[WIDTH];
    int running = 1;
    long cycles = 0;

    while (running && (infinite || cycles < max_cycles)) {
        print_bits(pc, "PC    ");
        ROM32K(pc, instr);
        print_bits(instr, "Instr ");

        int opcode[7], rd[5], rs1[5], rs2[5], funct3[3], funct7[7];
        for (int i = 0; i < 7; i++) opcode[i] = instr[i];
        for (int i = 0; i < 5; i++) rd[i] = instr[i + 7];
        for (int i = 0; i < 5; i++) rs1[i] = instr[i + 15];
        for (int i = 0; i < 5; i++) rs2[i] = instr[i + 20];
        for (int i = 0; i < 3; i++) funct3[i] = instr[i + 12];
        for (int i = 0; i < 7; i++) funct7[i] = instr[i + 25];

        int rs1_val[WIDTH], rs2_val[WIDTH], result[WIDTH];
        int imm[WIDTH] = {0};
        int opcode_val = (opcode[6] << 6) | (opcode[5] << 5) | (opcode[4] << 4) | 
                         (opcode[3] << 3) | (opcode[2] << 2) | (opcode[1] << 1) | opcode[0];

        switch (opcode_val) {
            case 0b0110011: // R-type
                RegFileRead(&rf, rs1, rs2, rs1_val, rs2_val);
                ALU32(rs1_val, rs2_val, result, funct3, funct7[5]);
                RegFileWrite(&rf, rd, result, 1);
                print_bits(result, "Result");
                break;

            case 0b0010011: // I-type ALU
                RegFileRead(&rf, rs1, rs2, rs1_val, rs2_val);
                for (int i = 0; i < 12; i++) imm[i] = instr[i + 20];
                for (int i = 12; i < WIDTH; i++) imm[i] = imm[11];
                ALU32(rs1_val, imm, result, funct3, funct7[5]);
                RegFileWrite(&rf, rd, result, 1);
                print_bits(result, "Result");
                break;

            case 0b0000011: // I-type Load
                RegFileRead(&rf, rs1, rs2, rs1_val, rs2_val);
                for (int i = 0; i < 12; i++) imm[i] = instr[i + 20];
                for (int i = 12; i < WIDTH; i++) imm[i] = imm[11];
                int addr[WIDTH];
                Add32(rs1_val, imm, addr);
                RAM32K_load(addr, result, funct3);
                RegFileWrite(&rf, rd, result, 1);
                print_bits(result, "Loaded");
                break;

            case 0b0100011: // S-type
                RegFileRead(&rf, rs1, rs2, rs1_val, rs2_val);
                for (int i = 0; i < 5; i++) imm[i] = instr[i + 7];
                for (int i = 0; i < 7; i++) imm[i + 5] = instr[i + 25];
                for (int i = 12; i < WIDTH; i++) imm[i] = imm[11];
                int store_addr[WIDTH];
                Add32(rs1_val, imm, store_addr);
                RAM32K_store(store_addr, rs2_val, 1, funct3);
                break;

            case 0b1100011: // B-type
                RegFileRead(&rf, rs1, rs2, rs1_val, rs2_val);
                imm[12] = instr[31];
                imm[11] = instr[7];
                for (int i = 0; i < 6; i++) imm[10 - i] = instr[30 - i];
                for (int i = 0; i < 4; i++) imm[4 - i] = instr[11 - i];
                imm[0] = 0;
                for (int i = 13; i < WIDTH; i++) imm[i] = imm[12];
                int equal = !LessThanU32(rs1_val, rs2_val) && !LessThanU32(rs2_val, rs1_val);
                int lt = LessThan32(rs1_val, rs2_val);
                int ltu = LessThanU32(rs1_val, rs2_val);
                int take_branch = 0;
                switch ((funct3[2] << 2) | (funct3[1] << 1) | funct3[0]) {
                    case 0b000: take_branch = equal; break;
                    case 0b001: take_branch = !equal; break;
                    case 0b100: take_branch = lt; break;
                    case 0b101: take_branch = !lt && !equal; break;
                    case 0b110: take_branch = ltu; break;
                    case 0b111: take_branch = !ltu && !equal; break;
                }
                if (take_branch) {
                    int new_pc[WIDTH];
                    Add32(pc, imm, new_pc);
                    PC32(new_pc, 1, 0, 0, pc, pc_state);
                    cycles++;
                    continue;
                }
                break;

            case 0b0110111: // U-type (lui)
                for (int i = 12; i < 32; i++) imm[i] = instr[i];
                for (int i = 0; i < 12; i++) imm[i] = 0;
                memcpy(result, imm, WIDTH * sizeof(int));
                RegFileWrite(&rf, rd, result, 1);
                print_bits(result, "Result");
                break;

            case 0b0010111: // U-type (auipc)
                for (int i = 12; i < 32; i++) imm[i] = instr[i];
                for (int i = 0; i < 12; i++) imm[i] = 0;
                Add32(pc, imm, result);
                RegFileWrite(&rf, rd, result, 1);
                print_bits(result, "Result");
                break;

            case 0b1101111: // J-type (jal)
                imm[20] = instr[31];
                for (int i = 0; i < 8; i++) imm[19 - i] = instr[19 - i];
                imm[11] = instr[20];
                for (int i = 0; i < 10; i++) imm[10 - i] = instr[30 - i];
                imm[0] = 0;
                for (int i = 21; i < WIDTH; i++) imm[i] = imm[20];
                int new_pc[WIDTH], pc_plus_4[WIDTH];
                Add32(pc, imm, new_pc);
                int one[WIDTH] = {0}; one[0] = 1;
                Add32(pc, one, pc_plus_4);
                RegFileWrite(&rf, rd, pc_plus_4, 1);
                PC32(new_pc, 1, 0, 0, pc, pc_state);
                print_bits(pc_plus_4, "Link");
                cycles++;
                continue;

            case 0b1100111: // I-type (jalr)
                if (funct3[0] == 0 && funct3[1] == 0 && funct3[2] == 0) {
                    RegFileRead(&rf, rs1, rs2, rs1_val, rs2_val);
                    for (int i = 0; i < 12; i++) imm[i] = instr[i + 20];
                    for (int i = 12; i < WIDTH; i++) imm[i] = imm[11];
                    int target[WIDTH], pc_plus_4[WIDTH];
                    Add32(rs1_val, imm, target);
                    target[0] = 0;
                    int one[WIDTH] = {0}; one[0] = 1;
                    Add32(pc, one, pc_plus_4);
                    RegFileWrite(&rf, rd, pc_plus_4, 1);
                    PC32(target, 1, 0, 0, pc, pc_state);
                    print_bits(pc_plus_4, "Link");
                    cycles++;
                    continue;
                }
                break;

            case 0b1110011: // System
                switch ((funct3[2] << 2) | (funct3[1] << 1) | funct3[0]) {
                    case 0b000:
                        if (instr[20] == 0) running = 0; // ecall
                        else if (instr[20] == 1) running = 0; // ebreak
                        break;
                    case 0b001: case 0b010: case 0b011:
                    case 0b101: case 0b110: case 0b111:
                        printf("CSR instructions not fully implemented\n");
                        break;
                }
                break;

            default:
                printf("Unknown opcode: %d\n", opcode_val);
                break;
        }

        PC32(pc, 0, 1, 0, pc, pc_state);
        cycles++;
    }

    printf("Total cycles executed: %ld\n", cycles);

    char output_str[WIDTH + 1];
    bits_to_str(rf.regs[4], output_str);
    print_bits(rf.regs[4], "X4    ");

    FILE* out_fp = fopen(argv[2], "w");
    if (!out_fp) { printf("Failed to open output file\n"); return 1; }
    fprintf(out_fp, "%s", output_str);
    fclose(out_fp);

    return 0;
}
