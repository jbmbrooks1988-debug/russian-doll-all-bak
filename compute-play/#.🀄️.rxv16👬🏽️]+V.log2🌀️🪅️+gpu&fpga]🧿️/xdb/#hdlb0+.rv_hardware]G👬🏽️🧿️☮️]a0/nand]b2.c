#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Print raw arguments with lengths
    printf("NAND: Received args (argc=%d):\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d]=%s (length=%zu)\n", i, argv[i] ? argv[i] : "NULL", argv[i] ? strlen(argv[i]) : 0);
    }

    // Validate argument count
    if (argc != 4) {
        fprintf(stderr, "Error: Expected 3 arguments, got %d\n", argc - 1);
        return 1;
    }

    // Validate non-NULL arguments
    if (!argv[1] || !argv[2] || !argv[3]) {
        fprintf(stderr, "Error: One or more arguments are NULL\n");
        return 1;
    }

    // Parse ram_addr
    char *endptr;
    long ram_addr = strtol(argv[1], &endptr, 10);
    printf("NAND: Parsed ram_addr=%ld (binary: ", ram_addr);
    for (int i = 15; i >= 0; i--) {
        printf("%d", (ram_addr >> i) & 1);
    }
    printf(")\n");
    if (*endptr != '\0' || ram_addr < 0 || ram_addr >= 256) {
        fprintf(stderr, "Invalid ram_addr: %s (parsed as %ld)\n", argv[1], ram_addr);
        return 1;
    }

    // Read RAM for resolving inputs
    unsigned char ram[256];
    FILE *fp = fopen("ram_output_address.txt", "r");
    if (!fp) {
        fprintf(stderr, "Error opening ram_output_address.txt for reading\n");
        return 1;
    }
    for (int i = 0; i < 256; i++) {
        if (fscanf(fp, "%hhu", &ram[i]) != 1) {
            fprintf(stderr, "Error reading RAM at index %d\n", i);
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);

    // Parse and resolve input_a
    long input_a = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid input_a: %s\n", argv[2]);
        return 1;
    }
    long resolved_input_a = input_a;
    if (input_a >= 15) {
        if (input_a < 256) {
            resolved_input_a = ram[input_a];
            printf("NAND: Resolved input_a=%ld (RAM[%ld]=%ld)\n", input_a, input_a, resolved_input_a);
        } else {
            fprintf(stderr, "Invalid RAM address for input_a: %ld\n", input_a);
            return 1;
        }
    }
    if (resolved_input_a < 0 || resolved_input_a > 3) {
        fprintf(stderr, "Invalid resolved input_a value: %ld\n", resolved_input_a);
        return 1;
    }

    // Parse and resolve input_b
    long input_b = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid input_b: %s\n", argv[3]);
        return 1;
    }
    long resolved_input_b = input_b;
    if (input_b >= 15) {
        if (input_b < 256) {
            resolved_input_b = ram[input_b];
            printf("NAND: Resolved input_b=%ld (RAM[%ld]=%ld)\n", input_b, input_b, resolved_input_b);
        } else {
            fprintf(stderr, "Invalid RAM address for input_b: %ld\n", input_b);
            return 1;
        }
    }
    if (resolved_input_b < 0 || resolved_input_b > 3) {
        fprintf(stderr, "Invalid resolved input_b value: %ld\n", resolved_input_b);
        return 1;
    }

    // Compute NAND (2-bit)
    int result = (~(resolved_input_a & resolved_input_b)) & 0x3;
    printf("NAND: Inputs %ld,%ld -> Result %d to RAM[%ld]\n", resolved_input_a, resolved_input_b, result, ram_addr);

    // Update RAM
    ram[ram_addr] = result;

    // Write back to RAM file
    fp = fopen("ram_output_address.txt", "w");
    if (!fp) {
        fprintf(stderr, "Error opening ram_output_address.txt for writing\n");
        return 1;
    }
    for (int i = 0; i < 256; i++) {
        fprintf(fp, "%d\n", ram[i]);
    }
    fclose(fp);

    return 0;
}
