#include <stdio.h>
#include <string.h>

// Generate HDLb0 instructions for GPU FILL command
void generate_gpu_fill(int color, const char *output_file) {
    FILE *out = fopen(output_file, "w");
    if (!out) {
        printf("Error opening %s\n", output_file);
        return;
    }

    // Initialize: r1 = color (from switch_0=5), r2 = 0 (counter)
    fprintf(out, "# RV-16 GPU: Fill 4x4 pixel grid with color %d\n", color);
    fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass switch_0(5) -> RAM[1] (r1 = color)\n",
            1, 5);
    fprintf(out, "0000000000000000 %016u %0000000000000000 %0000000000000011 # Pass 0 -> RAM[2] (r2 = counter)\n",
            2);

    // Loop: Store r1 to RAM[100+r2], output to tape, increment r2
    for (int i = 0; i < 16; i++) {
        fprintf(out, "0000000000000011 %016u %016u %016u # STORE r1 -> RAM[%d]\n",
                100 + i, 1, 2, 100 + i);
        fprintf(out, "0000000000000000 %000000000000000 %016u %0000000000000011 # Pass RAM[%d] to tape\n",
                100 + i, 100 + i);
    }
    fprintf(out, "0000000000000001 %016u %016u %0000000000000001 # NAND r2(2), 1 -> RAM[2] (r2 = r2 + 1)\n",
            2, 2);
    fprintf(out, "0000000000000100 %000000000000000 %016u %0000000000000000 # JUMP to 0 if r2 < 16\n",
            2);

    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s color output.txt\n", argv[0]);
        return 1;
    }
    int color = atoi(argv[1]);
    generate_gpu_fill(color, argv[2]);
    printf("Generated GPU program to %s\n", argv[2]);
    return 0;
}