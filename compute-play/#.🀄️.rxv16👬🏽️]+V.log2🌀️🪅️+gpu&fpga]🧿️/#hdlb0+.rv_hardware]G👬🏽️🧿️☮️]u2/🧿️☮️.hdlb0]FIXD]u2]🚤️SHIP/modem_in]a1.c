#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For mkdir
#include <sys/types.h> // For mkdir

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Error: Expected 4 arguments, got %d\n", argc);
        return 1;
    }
    int modem_file_number = atoi(argv[1]); // Modem file number (e.g., 26 for modem/26.txt)
    int ram_output_address = atoi(argv[2]); // RAM address to write to
    // argv[3] is ignored

    // Validate modem_file_number
    if (modem_file_number < 0) {
        printf("Error: Invalid modem file number %d\n", modem_file_number);
        return 1;
    }

    // Create modem directory if it doesn't exist
    struct stat st = {0};
    if (stat("📳️", &st) == -1) {
        mkdir("📳️", 0755); // Create directory with read/write/execute permissions
    }

    // Construct modem file name with modem/ prefix
    char modem_file[30];
    sprintf(modem_file, "📳️/%d.txt", modem_file_number);

    // Read value from modem file, create with 0 if it doesn't exist
    int value = 0;
    FILE *modem_fp = fopen(modem_file, "r");
    if (modem_fp) {
        fscanf(modem_fp, "%d", &value);
        fclose(modem_fp);
    } else {
        // Create file with default value 0
        modem_fp = fopen(modem_file, "w");
        if (modem_fp == NULL) {
            printf("Error: Cannot create modem file %s\n", modem_file);
            return 1;
        }
        fprintf(modem_fp, "0");
        fclose(modem_fp);
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
