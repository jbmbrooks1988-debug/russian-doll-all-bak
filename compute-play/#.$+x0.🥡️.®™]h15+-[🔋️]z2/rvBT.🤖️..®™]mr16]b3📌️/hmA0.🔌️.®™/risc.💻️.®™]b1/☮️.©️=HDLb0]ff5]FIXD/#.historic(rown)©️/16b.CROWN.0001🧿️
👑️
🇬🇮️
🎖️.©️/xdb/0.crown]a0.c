#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTIONS 100
#define MAX_CHIPS 65536

// Global arrays and variables
char chip_names[MAX_CHIPS][32];              // Chip bank (index 0 unused, "no logic")
unsigned short program[MAX_INSTRUCTIONS][4]; // Program instructions: [chip_location, ram_output_address, input_a, input_b]
unsigned char ram[256];                      // 256-bit RAM
unsigned char switch_0 = 0, switch_1 = 0;   // User switches
unsigned char clock = 0;                     // Clock value (0 or 1)
int clock_iterations = 0;                    // Number of clock cycles
int num_instructions = 0;                    // Number of instructions in program

// Read RAM from ram_output_address.txt
void read_ram(unsigned char ram[256]) {
    FILE *fp = fopen("ram_output_address.txt", "r");
    if (fp == NULL) {
        printf("Error opening ram_output_address.txt\n");
        exit(1);
    }
    for (int i = 0; i < 256; i++) {
        fscanf(fp, "%hhu", &ram[i]);
    }
    fclose(fp);
}

// Write RAM to ram_output_address.txt
void write_ram(unsigned char ram[256]) {
    FILE *fp = fopen("ram_output_address.txt", "w");
    if (fp == NULL) {
        printf("Error opening ram_output_address.txt\n");
        exit(1);
    }
    for (int i = 0; i < 256; i++) {
        fprintf(fp, "%d\n", ram[i]);
    }
    fclose(fp);
}

// Resolve an input value based on its raw value
int resolve_input(unsigned short raw_input, unsigned char *ram, unsigned char switch_0, unsigned char switch_1, unsigned char clock, int *is_blank) {
    *is_blank = 0;

    if (raw_input == 2 || raw_input == 3) { // Blank (hi-z or cutoff)
        *is_blank = 1;
        return -1;
    } else if (raw_input == 0) {
        return 0;
    } else if (raw_input == 1) {
        return 1;
    } else if (raw_input == 5) { // switch_0
        return switch_0;
    } else if (raw_input == 6) { // switch_1
        return switch_1;
    } else if (raw_input == 7) { // clock
        return clock;
    } else if (raw_input > 15) { // RAM address
        int addr = raw_input % 256; // Wrap to 0-255
        int value = ram[addr];
        ram[addr] = 0; // Remove bit from RAM
        write_ram(ram); // Update file immediately for chips to see
        return value;
    } else {
        printf("Invalid input: %d\n", raw_input);
        exit(1);
    }
}

int main() {
    // Read chip_bank.txt
    FILE *chip_fp = fopen("chip_bank.txt", "r");
    if (chip_fp == NULL) {
        printf("Error opening chip_bank.txt\n");
        return 1;
    }
    int chip_count = 0;
    while (fgets(chip_names[chip_count + 1], 32, chip_fp) && chip_count < MAX_CHIPS - 1) {
        chip_names[chip_count + 1][strcspn(chip_names[chip_count + 1], "\n")] = 0;
        chip_count++;
    }
    fclose(chip_fp);
    // Note: chip_names[0] is unused ("no logic")

    // Read program.txt
    FILE *prog_fp = fopen("program.txt", "r");
    if (prog_fp == NULL) {
        printf("Error opening program.txt\n");
        return 1;
    }
    char line[20];
    int line_num = 0;
    while (fgets(line, 20, prog_fp) && line_num < MAX_INSTRUCTIONS * 4) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 16) {
            unsigned short value = 0;
            for (int i = 0; i < 16; i++) {
                value = (value << 1) | (line[i] - '0');
            }
            program[line_num / 4][line_num % 4] = value;
            line_num++;
        }
    }
    fclose(prog_fp);
    num_instructions = line_num / 4;

    // Initialize ram_output_address.txt with 256 zeros
    for (int i = 0; i < 256; i++) {
        ram[i] = 0;
    }
    write_ram(ram);

    // Main emulator loop
    while (1) {
        // Display current state
        printf("\nClock cycle %d, clock = %d\n", clock_iterations, clock);
        printf("Tape contents:\n");
        FILE *tape_fp = fopen("cli_tape.txt", "r");
        if (tape_fp) {
            char buf[10];
            while (fgets(buf, 10, tape_fp)) {
                printf("%s", buf);
            }
            fclose(tape_fp);
        } else {
            printf("(empty)\n");
        }

        // Get user input
        printf("Set switch_0 (0/1): ");
        scanf("%hhu", &switch_0);
        printf("Set switch_1 (0/1): ");
        scanf("%hhu", &switch_1);
        printf("Press 's' to step clock, 'q' to quit: ");
        char choice;
        scanf(" %c", &choice);

        if (choice == 'q') {
            break;
        }

        if (choice == 's') {
            // Process each instruction
            for (int i = 0; i < num_instructions; i++) {
                read_ram(ram); // Load current RAM state

                unsigned short chip_location = program[i][0];
                unsigned short ram_output_address = program[i][1];
                unsigned short raw_input_a = program[i][2];
                unsigned short raw_input_b = program[i][3];

                int is_blank_a, is_blank_b;
                int input_a = resolve_input(raw_input_a, ram, switch_0, switch_1, clock, &is_blank_a);
                int input_b = resolve_input(raw_input_b, ram, switch_0, switch_1, clock, &is_blank_b);

                if (chip_location == 0) { // No logic
                    if (is_blank_a && !is_blank_b) {
                        int value = input_b;
                        if (ram_output_address == 0) {
                            FILE *tape_fp = fopen("cli_tape.txt", "a");
                            fprintf(tape_fp, "%d\n", value);
                            fclose(tape_fp);
                        } else {
                            ram[ram_output_address] = value;
                            write_ram(ram);
                        }
                    } else if (is_blank_b && !is_blank_a) {
                        int value = input_a;
                        if (ram_output_address == 0) {
                            FILE *tape_fp = fopen("cli_tape.txt", "a");
                            fprintf(tape_fp, "%d\n", value);
                            fclose(tape_fp);
                        } else {
                            ram[ram_output_address] = value;
                            write_ram(ram);
                        }
                    } else {
                        printf("Incorrect input error at line %d in program.txt\n", i * 4);
                    }
                } else { // Logic chip
                    if (is_blank_a || is_blank_b) {
                        printf("Error: Logic chip requires non-blank inputs at instruction %d\n", i);
                        continue;
                    }
                    char cmd[100];
                    sprintf(cmd, "./%s %d %d %d", chip_names[chip_location], ram_output_address, input_a, input_b);
                    system(cmd); // Chip executable handles writing to ram.txt or cli_tape.txt
                }
            }
            // Update clock
            clock = 1 - clock;
            clock_iterations++;
        }
    }

    return 0;
}
