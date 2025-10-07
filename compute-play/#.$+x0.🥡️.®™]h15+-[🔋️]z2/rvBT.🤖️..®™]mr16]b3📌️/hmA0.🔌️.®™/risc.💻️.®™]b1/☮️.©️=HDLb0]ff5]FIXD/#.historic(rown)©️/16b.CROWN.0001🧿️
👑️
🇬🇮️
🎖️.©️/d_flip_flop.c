#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s ram_output_address input_a input_b\n", argv[0]);
        return 1;
    }
    int ram_output_address = atoi(argv[1]);
    int input_a = atoi(argv[2]); // D
    int input_b = atoi(argv[3]); // Clock
    int output;

    if (ram_output_address == 0) {
        // If outputting to tape, just pass input_a (no state tracking)
        output = input_a;
        FILE *fp = fopen("cli_tape.txt", "a");
        if (fp) {
            fprintf(fp, "%d\n", output);
            fclose(fp);
        }
    } else {
        unsigned char ram[256] = {0};
        FILE *ram_fp = fopen("ram_output_address.txt", "r");
        if (ram_fp) {
            for (int i = 0; i < 256; i++) {
                fscanf(ram_fp, "%hhu", &ram[i]);
            }
            fclose(ram_fp);
        }
        int current_state = (ram_output_address < 256) ? ram[ram_output_address] : 0;
        if (input_b == 1) {
            output = input_a; // Latch D when clock is 1
        } else {
            output = current_state; // Hold previous state
        }
        if (ram_output_address < 256) {
            ram[ram_output_address] = output;
            ram_fp = fopen("ram_output_address.txt", "w");
            for (int i = 0; i < 256; i++) {
                fprintf(ram_fp, "%d\n", ram[i]);
            }
            fclose(ram_fp);
        }
    }
    return 0;
}
