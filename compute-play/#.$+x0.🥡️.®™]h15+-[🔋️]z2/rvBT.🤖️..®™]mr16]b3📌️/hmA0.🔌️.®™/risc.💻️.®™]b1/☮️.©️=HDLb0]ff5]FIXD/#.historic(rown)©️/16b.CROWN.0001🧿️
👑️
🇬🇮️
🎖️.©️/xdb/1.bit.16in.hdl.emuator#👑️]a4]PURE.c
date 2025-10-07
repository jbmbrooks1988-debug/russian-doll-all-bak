#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTIONS 100
#define MAX_CHIPS 65536

// Global arrays and variables
char chip_names[MAX_CHIPS][32];
unsigned short program[MAX_INSTRUCTIONS][4];
unsigned char ram[256];
unsigned char switch_0 = 0, switch_1 = 0;
unsigned char clock = 0;
int clock_iterations = 0;
int num_instructions = 0;

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

// Resolve an input value
int resolve_input(unsigned short raw_input, unsigned char *ram, unsigned char switch_0, unsigned char switch_1, unsigned char clock, int *is_blank) {
    *is_blank = 0;
    if (raw_input == 2 || raw_input == 3) { // Blank inputs
        *is_blank = 1;
        return -1;
    } else if (raw_input == 0) return 0;
    else if (raw_input == 1) return 1;
    else if (raw_input == 5) return switch_0;
    else if (raw_input == 6) return switch_1;
    else if (raw_input == 7) return clock;
    else if (raw_input >= 16 && raw_input < 272) { // RAM addresses (16 to 271 map to 0-255)
        int addr = raw_input % 256;
        int value = ram[addr];
        ram[addr] = 0; // Clear the RAM location after reading
        write_ram(ram);
        return value;
    } else {
        printf("Invalid input: %d\n", raw_input);
        return -1;
    }
}

// Execute all instructions for one clock cycle
void run_cycle() {
    for (int i = 0; i < num_instructions; i++) {
        read_ram(ram);

        unsigned short chip_location = program[i][0];
        unsigned short ram_output_address = program[i][1];
        unsigned short raw_input_a = program[i][2];
        unsigned short raw_input_b = program[i][3];

        int is_blank_a, is_blank_b;
        int input_a = resolve_input(raw_input_a, ram, switch_0, switch_1, clock, &is_blank_a);
        int input_b = resolve_input(raw_input_b, ram, switch_0, switch_1, clock, &is_blank_b);

        if (chip_location == 0) { // No logic
            if (is_blank_a && !is_blank_b && input_b != -1) {
                if (ram_output_address == 0) {
                    FILE *tape_fp = fopen("cli_tape.txt", "a");
                    if (tape_fp == NULL) {
                        printf("Error opening cli_tape.txt for writing\n");
                        continue;
                    }
                    fprintf(tape_fp, "%d\n", input_b);
                    fclose(tape_fp);
                } else if (ram_output_address < 256) {
                    ram[ram_output_address] = input_b;
                    write_ram(ram);
                }
            } else if (is_blank_b && !is_blank_a && input_a != -1) {
                if (ram_output_address == 0) {
                    FILE *tape_fp = fopen("cli_tape.txt", "a");
                    if (tape_fp == NULL) {
                        printf("Error opening cli_tape.txt for writing\n");
                        continue;
                    }
                    fprintf(tape_fp, "%d\n", input_a);
                    fclose(tape_fp);
                } else if (ram_output_address < 256) {
                    ram[ram_output_address] = input_a;
                    write_ram(ram);
                }
            } else {
                printf("Incorrect input error at instruction %d: a=%d (blank=%d), b=%d (blank=%d)\n", 
                       i, input_a, is_blank_a, input_b, is_blank_b);
            }
        } else if (chip_location < MAX_CHIPS && chip_names[chip_location][0] != '\0') { // Logic chip
            if (is_blank_a || is_blank_b) {
                printf("Error: Logic chip requires non-blank inputs at instruction %d\n", i);
                continue;
            }
            char cmd[100];
            sprintf(cmd, "./%s %d %d %d", chip_names[chip_location], ram_output_address, input_a, input_b);
            system(cmd);
        } else {
            printf("Error: Invalid chip_location %d at instruction %d\n", chip_location, i);
        }
    }
    clock = 1 - clock;
    clock_iterations++;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s program.txt\n", argv[0]);
        return 1;
    }

    // Read chip_bank.txt
    FILE *chip_fp = fopen("chip_bank.txt", "r");
    if (chip_fp == NULL) {
        printf("Error opening chip_bank.txt\n");
        return 1;
    }
    char line[1024]; // Large buffer to handle long lines
    int chip_count = 0;
    while (fgets(line, sizeof(line), chip_fp) && chip_count < MAX_CHIPS - 1) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#' || line[0] == '\0') continue;
        strncpy(chip_names[chip_count + 1], line, 32);
        chip_count++;
    }
    fclose(chip_fp);
    chip_names[0][0] = 0;

    // Read program.txt
    FILE *prog_fp = fopen(argv[1], "r");
    if (prog_fp == NULL) {
        printf("Error opening %s\n", argv[1]);
        return 1;
    }

    // Buffer to collect binary digits (0s and 1s)
    char binary_buffer[1024]; // Large enough to handle multiple instructions
    int binary_buffer_pos = 0;

    int line_num = 0;
    while (fgets(line, sizeof(line), prog_fp)) {
        line_num++;
        line[strcspn(line, "\n")] = 0; // Remove newline

        // Skip empty lines
        if (strlen(line) == 0) continue;

        // Process the line character by character
        int in_comment = 0;
        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == '#') {
                in_comment = 1; // Start of comment, ignore rest of line
                break;
            }
            if (in_comment) continue;

            // Collect only '0' or '1' into the binary buffer
            if (line[i] == '0' || line[i] == '1') {
                binary_buffer[binary_buffer_pos++] = line[i];
            }

            // Check if we've collected enough bits for a 16-bit value (and enough values for an instruction)
            if (binary_buffer_pos >= 64 && binary_buffer_pos % 16 == 0) {
                // We have at least one complete instruction (64 bits = 4 × 16-bit values)
                int instruction_start = binary_buffer_pos - 64;
                for (int part = 0; part < 4; part++) {
                    char binary_str[17];
                    strncpy(binary_str, binary_buffer + instruction_start + part * 16, 16);
                    binary_str[16] = '\0'; // Null-terminate
                    unsigned short value = strtoul(binary_str, NULL, 2);
                    program[num_instructions][part] = value;
                }
                num_instructions++;

                // Shift remaining bits in the buffer to the start
                memmove(binary_buffer, binary_buffer + 64, binary_buffer_pos - 64);
                binary_buffer_pos -= 64;
            }
        }
    }

    // Check if there are leftover bits that don't form a complete instruction
    if (binary_buffer_pos > 0) {
        if (binary_buffer_pos % 16 == 0) {
            // We have a partial instruction (multiple of 16 but less than 64)
            printf("Warning: Incomplete instruction at end of file (%d bits, expected 64)\n", binary_buffer_pos);
        } else {
            // We have a partial value (not a multiple of 16)
            printf("Warning: Incomplete 16-bit value at end of file (%d bits, expected multiple of 16)\n", binary_buffer_pos);
        }
    }

    fclose(prog_fp);

    // Initialize RAM
    for (int i = 0; i < 256; i++) ram[i] = 0;
    write_ram(ram);

    // Clear cli_tape.txt
    FILE *tape_fp = fopen("cli_tape.txt", "w");
    if (tape_fp == NULL) {
        printf("Error creating cli_tape.txt\n");
        return 1;
    }
    fclose(tape_fp);

    // Initial run
    printf("Initial run:\n");
    run_cycle();

    // Main loop
    int running = 0;
    while (1) {
        printf("\nClock cycle %d, clock = %d\n", clock_iterations, clock);
        printf("Tape contents:\n");
        tape_fp = fopen("cli_tape.txt", "r");
        if (tape_fp) {
            char buf[10];
            while (fgets(buf, sizeof(buf), tape_fp)) {
                printf("%s", buf);
            }
            fclose(tape_fp);
        } else {
            printf("(empty)\n");
        }

        printf("Switches: switch_0 = %d, switch_1 = %d\n", switch_0, switch_1);
        printf("Options: '1' flip switch_0, '2' flip switch_1, 's' step, 'r' run, 'q' quit: ");
        char choice;
        scanf(" %c", &choice);

        if (choice == 'q') break;
        else if (choice == '1') switch_0 = 1 - switch_0; // Flip switch_0
        else if (choice == '2') switch_1 = 1 - switch_1; // Flip switch_1
        else if (choice == 's') {
            running = 0;
            run_cycle();
        } else if (choice == 'r') {
            running = 1;
        }

        if (running) {
            run_cycle();
            printf("\033[2J\033[1;1H"); // Clear screen (Unix-like systems)
        }
    }

    printf("Emulator stopped.\n");
    return 0;
}
