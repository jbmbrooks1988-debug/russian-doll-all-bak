#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;
    int ram_output_address = atoi(argv[1]);
    int input_value = atoi(argv[2]); // Value to output
    // argv[3] is ignored

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
        // Construct modem file name from ram_output_address
        char modem_file[20];
        sprintf(modem_file, "%d.txt", ram_output_address);

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