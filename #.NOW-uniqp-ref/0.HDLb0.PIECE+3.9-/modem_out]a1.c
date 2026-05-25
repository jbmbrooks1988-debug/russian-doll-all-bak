#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For mkdir
#include <sys/types.h> // For mkdir

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;
    int ram_output_address = atoi(argv[1]);
    int input_value = atoi(argv[2]); // Value to output
    // argv[3] is ignored

    // Create modem directory if it doesn't exist
    struct stat st = {0};
    if (stat("📳️", &st) == -1) {
        mkdir("📳️", 0755); // Create directory with read/write/execute permissions
    }

    // Write to tape if address is 0
    if (ram_output_address == 0) {
        FILE *tape_fp = fopen("cli_tape.txt", "a");
        if (tape_fp == NULL) {
            printf("Error: Cannot open cli_tape.txt\n");
            return 1;
        }
        fprintf(tape_fp, "%d\n", input_value);
        fclose(tape_fp);
    } else {
        // Construct modem file name with modem/ prefix
        char modem_file[30];
        sprintf(modem_file, "📳️/%d.txt", ram_output_address);

        // Write input_value to modem file
        FILE *modem_fp = fopen(modem_file, "w");
        if (modem_fp == NULL) {
            printf("Error: Cannot open modem file %s\n", modem_file);
            return 1;
        }
        fprintf(modem_fp, "%d", input_value);
        fclose(modem_fp);
    }

    return 0;
}
