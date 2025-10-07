#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Generate HDLb0 instructions for 2-input LUT
void generate_fpga_lut(const char *logic, const char *output_file) {
    FILE *out = fopen(output_file, "w");
    if (!out) {
        printf("Error opening %s\n", output_file);
        return;
    }

    // Map logic to 4-bit truth table
    int lut[4] = {0, 0, 0, 0}; // Default: all 0
    if (strcmp(logic, "AND") == 0) {
        lut[3] = 1; // 0001
    } else if (strcmp(logic, "OR") == 0) {
        lut[1] = lut[2] = lut[3] = 1; // 0111
    } else if (strcmp(logic, "XOR") == 0) {
        lut[1] = lut[2] = 1; // 0110
    } else {
        printf("Unsupported logic: %s\n", logic);
        fclose(out);
        return;
    }

    // Write LUT to RAM[100-103]
    fprintf(out, "# RV-16 FPGA: 2-input LUT for %s (%d%d%d%d)\n", logic,
            lut[0], lut[1], lut[2], lut[3]);
    for (int i = 0; i < 4; i++) {
        fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass %d -> RAM[%d]\n",
                100 + i, lut[i], lut[i], 100 + i);
    }

    // Set inputs: r2 = switch_0(5), r3 = switch_1(6)
    fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass switch_0(5) -> RAM[2] (r2)\n",
            2, 5);
    fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass switch_1(6) -> RAM[3] (r3)\n",
            3, 6);

    // Compute LUT index: r4 = (r2 << 1) | r3 (simplified with NANDs)
    fprintf(out, "0000000000000001 %016u %016u %016u # NAND r2(2), r3(3) -> RAM[4] (temp)\n",
            4, 2, 3);
    fprintf(out, "0000000000000001 %016u %016u %016u # NAND r2(2), RAM[4] -> RAM[4] (index)\n",
            4, 2, 4);

    // Select LUT output: r1 = RAM[100+r4]
    fprintf(out, "0000000000000010 %016u %016u %016u # LOAD r1, RAM[100+r4]\n",
            1, 100, 4);

    // Output to tape
    fprintf(out, "0000000000000000 %000000000000000 %016u %0000000000000011 # Pass RAM[1] to tape\n",
            1);

    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s logic output.txt\n", argv[0]);
        return 1;
    }
    generate_fpga_lut(argv[1], argv[2]);
    printf("Generated FPGA program for %s to %s\n", argv[1], argv[2]);
    return 0;
}