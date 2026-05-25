#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;
    int ram_output_address = atoi(argv[1]);
    int input_value = atoi(argv[2]); // Ignored, but kept for consistency

    // Construct modem file name from input_value
    char modem_file[20];
    sprintf(modem_file, "%d.txt", input_value);

    // Read value from modem file
    FILE *modem_fp = fopen(modem_file, "r");
    int value = 0;
    if (modem_fp) {
        fscanf(modem_fp, "%d", &value);
        fclose(modem_fp);
    } else {
        printf("Error: Cannot open modem file %s\n", modem_file);
        return 1;
    }

    // Write to tape or RAM
    if (ram_output_address == 0) {
        FILE *tape_fp = fopen("cli_tape.txt", "a");
        if (tape_fp == NULL) {
            printf("Error: Cannot open cli_tape.txt\n");
            return 1;
        }
        fprintf(tape_fp, "%d\n", value);
        fclose(tape_fp);
    } else if (ram_output_address < 256) {
        unsigned char ram[256] = {0};
        FILE *ram_fp = fopen("ram_output_address.txt", "r");
        if (ram_fp) {
            for (int i = 0; i < 256; i++) fscanf(ram_fp, "%hhu", &ram[i]);
            fclose(ram_fp);
        }
        ram[ram_output_address] = value & 0xFF; // Ensure 8-bit value
        ram_fp = fopen("ram_output_address.txt", "w");
        if (ram_fp == NULL) {
            printf("Error: Cannot open ram_output_address.txt\n");
            return 1;
        }
        for (int i = 0; i < 256; i++) fprintf(ram_fp, "%d\n", ram[i]);
        fclose(ram_fp);
    }

    return 0;
}