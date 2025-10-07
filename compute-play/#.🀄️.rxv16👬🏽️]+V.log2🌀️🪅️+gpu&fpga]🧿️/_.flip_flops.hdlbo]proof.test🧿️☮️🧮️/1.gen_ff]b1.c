#include <stdio.h>
#include <string.h>

// Generate HDLb0 instructions for flip-flops
void generate_flip_flop(const char *type, const char *output_file) {
    FILE *out = fopen(output_file, "w");
    if (!out) {
        printf("Error opening %s\n", output_file);
        return;
    }

    if (strcmp(type, "D_FF") == 0) {
        fprintf(out, "# RV-16 Basic D Flip-Flop: Store switch_0(5) when switch_1(6)=1\n");
        fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass 0 -> RAM[16]\n", 16, 0);
        fprintf(out, "0000000000000001 %016u %016u %016u # NAND switch_0(5), switch_1(6) -> RAM[16]\n", 16, 5, 6);
        fprintf(out, "0000000000000011 %016u %016u %0000000000000000 # STORE r16 -> RAM[100]\n", 100, 16);
        fprintf(out, "0000000000000000 %000000000000000 %016u %0000000000000011 # Pass RAM[100] to tape\n", 100);
    } else if (strcmp(type, "MS_FF") == 0) {
        fprintf(out, "# RV-16 Master-Slave D Flip-Flop: Store switch_0(5) on switch_1(6) 0->1\n");
        fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass 0 -> RAM[16]\n", 16, 0);
        fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass 0 -> RAM[17]\n", 17, 0);
        fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass switch_0(5) -> RAM[18]\n", 18, 5);
        fprintf(out, "0000000000000000 %016u %016u %0000000000000011 # Pass switch_1(6) -> RAM[19]\n", 19, 6);
        fprintf(out, "0000000000000001 %016u %016u %016u # NAND r18(18), r19(19) -> RAM[16]\n", 16, 18, 19);
        fprintf(out, "0000000000000001 %016u %016u %016u # NAND r16(16), r19(19) -> RAM[17]\n", 17, 16, 19);
        fprintf(out, "0000000000000011 %016u %016u %0000000000000000 # STORE r17 -> RAM[101]\n", 101, 17);
        fprintf(out, "0000000000000000 %000000000000000 %016u %0000000000000011 # Pass RAM[101] to tape\n", 101);
    } else {
        printf("Unsupported flip-flop type: %s\n", type);
    }

    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s type output.txt\n", argv[0]);
        return 1;
    }
    generate_flip_flop(argv[1], argv[2]);
    printf("Generated flip-flop program for %s to %s\n", argv[1], argv[2]);
    return 0;
}
