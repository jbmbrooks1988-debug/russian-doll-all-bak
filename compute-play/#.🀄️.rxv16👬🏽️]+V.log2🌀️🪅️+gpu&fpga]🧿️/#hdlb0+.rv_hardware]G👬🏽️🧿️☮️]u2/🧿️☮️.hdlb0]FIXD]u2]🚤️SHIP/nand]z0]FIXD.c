#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;
    int ram_output_address = atoi(argv[1]);
    int input_a = atoi(argv[2]);
    int input_b = atoi(argv[3]);
    int output = !(input_a & input_b);

    if (ram_output_address == 0) {
        char existing[1024] = "";
        FILE *tape_fp = fopen("cli_tape.txt", "r");
        if (tape_fp) {
            if (fgets(existing, sizeof(existing), tape_fp)) {
                existing[strcspn(existing, "\n")] = 0;
            }
            fclose(tape_fp);
        }
        tape_fp = fopen("cli_tape.txt", "w");
        if (tape_fp == NULL) {
            printf("Error opening cli_tape.txt for writing\n");
            return 1;
        }
        fprintf(tape_fp, "%d%s", output, existing);
        fclose(tape_fp);
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
