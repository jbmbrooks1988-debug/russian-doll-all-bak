#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) return 1; // Accept 4 or 5 arguments
    int ram_output_address = atoi(argv[1]);
    int input_a = atoi(argv[2]);
    int input_b = atoi(argv[3]);
    char *extra_tape_file = (argc == 5) ? argv[4] : NULL; // Get extra_tape_file if provided
    int output = !(input_a & input_b);

    if (ram_output_address == 0) {
        // Write to cli_tape.txt
        FILE *fp = fopen("cli_tape.txt", "a");
        if (fp == NULL) {
            printf("Error opening cli_tape.txt for writing\n");
            return 1;
        }
        fprintf(fp, "%d\n", output);
        fclose(fp);

        // Write to extra_tape_file if specified
        if (extra_tape_file != NULL) {
            FILE *extra_fp = fopen(extra_tape_file, "a");
            if (extra_fp == NULL) {
                printf("Error opening %s for writing\n", extra_tape_file);
                return 1;
            }
            fprintf(extra_fp, "%d\n", output);
            fclose(extra_fp);
        }
    } else {
        unsigned char ram[256] = {0};
        FILE *ram_fp = fopen("ram_output_address.txt", "r");
        if (ram_fp) {
            for (int i = 0; i < 256; i++) fscanf(ram_fp, "%hhu", &ram[i]);
            fclose(ram_fp);
        }
        if (ram_output_address < 256) {
            ram[ram_output_address] = output;
            ram_fp = fopen("ram_output_address.txt", "w");
            for (int i = 0; i < 256; i++) fprintf(ram_fp, "%d\n", ram[i]);
            fclose(ram_fp);
        }
    }
    return 0;
}
