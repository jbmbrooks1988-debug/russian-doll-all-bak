#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Function to convert an unsigned short to a 16-bit binary string
void ushort_to_binary(unsigned short value, char *binary_str) {
    for (int i = 15; i >= 0; i--) {
        binary_str[15 - i] = ((value >> i) & 1) ? '1' : '0';
    }
    binary_str[16] = '\0';
}

// Function to get RAM address for a register
int get_reg_addr(const char *reg_str) {
    if (strlen(reg_str) != 2 || toupper(reg_str[0]) != 'R' || !isdigit(reg_str[1])) {
        fprintf(stderr, "Error: Invalid register format: %s\n", reg_str);
        exit(1);
    }
    int reg_num = atoi(&reg_str[1]);
    if (reg_num < 0 || reg_num > 3) {
        fprintf(stderr, "Error: Register number out of range (0-3): %s\n", reg_str);
        exit(1);
    }
    return reg_num + 16; // R0-R3 map to RAM[16-19]
}

// Function to get RAM address for a memory location
int get_mem_addr(const char *mem_str) {
    if (strlen(mem_str) < 4 || strncasecmp(mem_str, "MEM", 3) != 0 || !isdigit(mem_str[3])) {
        fprintf(stderr, "Error: Invalid memory format: %s\n", mem_str);
        exit(1);
    }
    int mem_num = atoi(&mem_str[3]);
    if (mem_num < 0 || mem_num > 15) {
        fprintf(stderr, "Error: Memory address out of range (0-15): %s\n", mem_str);
        exit(1);
    }
    return mem_num + 20; // MEM0-MEM15 map to RAM[20-35]
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_assembly_file> <output_binary_file>\n", argv[0]);
        return 1;
    }

    FILE *infile = fopen(argv[1], "r");
    if (infile == NULL) {
        perror("Error opening input file");
        return 1;
    }

    FILE *outfile = fopen(argv[2], "w");
    if (outfile == NULL) {
        perror("Error opening output file");
        fclose(infile);
        return 1;
    }

    char line[256];
    char binary_output[17];
    int line_num = 0;

    while (fgets(line, sizeof(line), infile) != NULL) {
        line_num++;
        // Remove leading/trailing whitespace and comments
        char *trimmed_line = line;
        while (isspace((unsigned char)*trimmed_line)) trimmed_line++;
        char *comment_pos = strchr(trimmed_line, '#');
        if (comment_pos != NULL) *comment_pos = '\0';
        size_t len = strlen(trimmed_line);
        while (len > 0 && isspace((unsigned char)trimmed_line[len - 1])) len--;
        trimmed_line[len] = '\0';

        if (strlen(trimmed_line) == 0) continue; // Skip empty lines

        char opcode[10];
        char arg1[10], arg2[10], arg3[10];
        int num_args;

        // Try to parse based on expected instruction format
        if (strncasecmp(trimmed_line, "NAND", 4) == 0) {
            num_args = sscanf(trimmed_line, "%s %[^,], %[^,], %s", opcode, arg1, arg2, arg3);
            if (num_args != 4) {
                fprintf(stderr, "Error at line %d: NAND expects 3 arguments (dest, src1, src2) in format 'NAND dest, src1, src2'\n", line_num);
                return 1;
            }
        } else if (strncasecmp(trimmed_line, "LOAD", 4) == 0 || strncasecmp(trimmed_line, "STORE", 5) == 0) {
            num_args = sscanf(trimmed_line, "%s %[^,], %s", opcode, arg1, arg2);
            if (num_args != 3) {
                fprintf(stderr, "Error at line %d: %s expects 2 arguments (reg, mem) in format '%s reg, mem'\n", line_num, opcode, opcode);
                return 1;
            }
        } else if (strncasecmp(trimmed_line, "JUMP", 4) == 0) {
            num_args = sscanf(trimmed_line, "%s %s", opcode, arg1);
            if (num_args != 2) {
                fprintf(stderr, "Error at line %d: JUMP expects 1 argument (reg) in format 'JUMP reg'\n", line_num);
                return 1;
            }
        } else {
            fprintf(stderr, "Error at line %d: Unknown opcode '%s'\n", line_num, trimmed_line);
            return 1;
        }

        unsigned short chip_location = 0;
        unsigned short ram_output_address = 0;
        unsigned short input_a = 0;
        unsigned short input_b = 0;

        if (strcasecmp(opcode, "NAND") == 0) {
            chip_location = 1; // NAND chip
            ram_output_address = get_reg_addr(arg1);
            input_a = get_reg_addr(arg2);
            input_b = get_reg_addr(arg3);
        } else if (strcasecmp(opcode, "LOAD") == 0) {
            chip_location = 0; // Pass-through
            ram_output_address = get_reg_addr(arg1);
            input_a = get_mem_addr(arg2);
            input_b = 3; // Blank
        } else if (strcasecmp(opcode, "STORE") == 0) {
            chip_location = 0; // Pass-through
            ram_output_address = get_mem_addr(arg2);
            input_a = get_reg_addr(arg1);
            input_b = 3; // Blank
        } else if (strcasecmp(opcode, "JUMP") == 0) {
            chip_location = 0; // Pass-through
            ram_output_address = 36; // PC address (RAM[36])
            input_a = get_reg_addr(arg1);
            input_b = 3; // Blank
        }

        // Write HDLb0 instruction to output file
        ushort_to_binary(chip_location, binary_output);
        fprintf(outfile, "%s ", binary_output);
        ushort_to_binary(ram_output_address, binary_output);
        fprintf(outfile, "%s ", binary_output);
        ushort_to_binary(input_a, binary_output);
        fprintf(outfile, "%s ", binary_output);
        ushort_to_binary(input_b, binary_output);
        fprintf(outfile, "%s\n", binary_output);
    }

    fclose(infile);
    fclose(outfile);
    printf("Assembly successful! Output written to %s\n", argv[2]);

    return 0;
}