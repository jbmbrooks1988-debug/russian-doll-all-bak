#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NAND gate
int Nand(int a, int b) { return !(a && b); }

// Read inputs from file
void read_inputs(const char* filename, int* a, int* b) {
    FILE* fp = fopen(filename, "r");
    if (!fp) { fprintf(stderr, "Failed to open %s\n", filename); exit(1); }
    char line[10];
    
    // Read first input
    if (fgets(line, 10, fp) == NULL || line[0] < '0' || line[0] > '1') {
        fprintf(stderr, "Input file must contain first bit (0 or 1) for NAND\n"); fclose(fp); exit(1);
    }
    *a = (line[0] == '1') ? 1 : 0;
    // Read second input
    if (fgets(line, 10, fp) == NULL || line[0] < '0' || line[0] > '1') {
        fprintf(stderr, "Input file must contain second bit (0 or 1) for NAND\n"); fclose(fp); exit(1);
    }
    *b = (line[0] == '1') ? 1 : 0;
    printf("DEBUG: NAND inputs = %d, %d\n", *a, *b);
    fclose(fp);
}

// Write output to file
void write_output(const char* filename, int result) {
    FILE* fp = fopen(filename, "w");
    if (!fp) { fprintf(stderr, "Failed to open %s\n", filename); exit(1); }
    fprintf(fp, "%d\n", result);
    fclose(fp);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        exit(1);
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];

    int a = 0, b = 0;
    read_inputs(input_file, &a, &b);
    int result = Nand(a, b);

    printf("DEBUG: NAND result = %d\n", result);
    write_output(output_file, result);

    return 0;
}
