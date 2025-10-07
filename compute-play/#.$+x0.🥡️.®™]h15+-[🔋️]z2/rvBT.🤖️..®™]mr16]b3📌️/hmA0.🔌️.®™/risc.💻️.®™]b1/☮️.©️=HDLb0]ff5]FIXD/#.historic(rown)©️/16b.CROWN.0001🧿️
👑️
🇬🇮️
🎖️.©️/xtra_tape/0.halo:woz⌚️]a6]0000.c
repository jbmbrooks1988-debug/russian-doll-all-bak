#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTIONS 65536
#define MAX_CHIPS 65536
#define DEBUG 1  // Enable debug output

// Global arrays and variables
char chip_names[MAX_CHIPS][32];
unsigned short program[MAX_INSTRUCTIONS][4];
unsigned char ram[256];
unsigned char switch_0 = 0, switch_1 = 0;
unsigned char clock = 0;
int clock_iterations = 0;
int num_instructions = 0;
char *extra_tape_file = NULL; // Global pointer for the optional extra tape file

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
    if (raw_input == 2 || raw_input == 3) {
        *is_blank = 1;
        return -1;
    } else if (raw_input == 0) return 0;
    else if (raw_input == 1) return 1;
    else if (raw_input == 5) return switch_0;
    else if (raw_input == 6) return switch_1;
    else if (raw_input == 7) return clock;
    else if (raw_input > 15) {
        int addr = raw_input % 256;
        int value = ram[addr];
        ram[addr] = 0;
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

#if DEBUG
        printf("Instruction %d: chip=%d, addr=%d, a=%d (blank=%d), b=%d (blank=%d)\n", 
               i, chip_location, ram_output_address, input_a, is_blank_a, input_b, is_blank_b);
#endif

        if (chip_location == 0) { // No logic
            if (is_blank_a && !is_blank_b && input_b != -1) {
                if (ram_output_address == 0) {
                    // Write to cli_tape.txt
                    FILE *tape_fp = fopen("cli_tape.txt", "a");
                    if (tape_fp == NULL) {
                        printf("Error opening cli_tape.txt for writing\n");
                        continue;
                    }
                    fprintf(tape_fp, "%d\n", input_b);
                    fclose(tape_fp);
#if DEBUG
                    printf("Wrote %d to cli_tape.txt\n", input_b);
#endif

                    // Write to extra tape file if specified
                    if (extra_tape_file != NULL) {
                        FILE *extra_fp = fopen(extra_tape_file, "a");
                        if (extra_fp == NULL) {
                            printf("Error opening %s for writing\n", extra_tape_file);
                            continue;
                        }
                        fprintf(extra_fp, "%d\n", input_b);
                        fclose(extra_fp);
#if DEBUG
                        printf("Wrote %d to %s\n", input_b, extra_tape_file);
#endif
                    }
                } else if (ram_output_address < 256) {
                    ram[ram_output_address] = input_b;
                    write_ram(ram);
#if DEBUG
                    printf("Wrote %d to RAM at addr %d\n", input_b, ram_output_address);
#endif
                }
            } else if (is_blank_b && !is_blank_a && input_a != -1) {
                if (ram_output_address == 0) {
                    // Write to cli_tape.txt
                    FILE *tape_fp = fopen("cli_tape.txt", "a");
                    if (tape_fp == NULL) {
                        printf("Error opening cli_tape.txt for writing\n");
                        continue;
                    }
                    fprintf(tape_fp, "%d\n", input_a);
                    fclose(tape_fp);
#if DEBUG
                    printf("Wrote %d to cli_tape.txt\n", input_a);
#endif

                    // Write to extra tape file if specified
                    if (extra_tape_file != NULL) {
                        FILE *extra_fp = fopen(extra_tape_file, "a");
                        if (extra_fp == NULL) {
                            printf("Error opening %s for writing\n", extra_tape_file);
                            continue;
                        }
                        fprintf(extra_fp, "%d\n", input_a);
                      //  fclose(extra_fp);
#if DEBUG
                        printf("Wrote %d to %s\n", input_a, extra_tape_file);
#endif
                    }
                } else if (ram_output_address < 256) {
                    ram[ram_output_address] = input_a;
                    write_ram(ram);
#if DEBUG
                    printf("Wrote %d to RAM at addr %d\n", input_a, ram_output_address);
#endif
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
            char cmd[1000];
            sprintf(cmd, "./%s %d %d %d", chip_names[chip_location], ram_output_address, input_a, input_b);
         //  sprintf(cmd, "./%s %d %d %d %s", chip_names[chip_location], ram_output_address, input_a, input_b, extra_tape_file);
            system(cmd);
        } else {
            printf("Error: Invalid chip_location %d at instruction %d\n", chip_location, i);
        }
    }
    clock = 1 - clock;
    clock_iterations++;
}

int main(int argc, char *argv[]) {
    int num_cycles = 1;
    
    if (argc < 2 || argc > 4) {
        printf("Usage: %s program.txt [num_cycles] [extra_tape_file]\n", argv[0]);
        return 1;
    }

    if (argc >= 3) {
        num_cycles = atoi(argv[2]);
        if (num_cycles <= 0) {
            printf("Error: Number of cycles must be positive\n");
            return 1;
        }
    }

    if (argc == 4) {
        extra_tape_file = argv[3]; // Store the extra tape file name, no clearing
    }

    // Read chip_bank.txt
    FILE *chip_fp = fopen("chip_bank.txt", "r");
    if (chip_fp == NULL) {
        printf("Error opening chip_bank.txt\n");
        return 1;
    }
    char line[50];
    int chip_count = 0;
    while (fgets(line, sizeof(line), chip_fp) && chip_count < MAX_CHIPS - 1) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#' || line[0] == '\0') continue;
        strncpy(chip_names[chip_count + 1], line, 32);
        chip_count++;
    }
    fclose(chip_fp);
    chip_names[0][0] = 0;

    // Debug: Print chip_names contents
#if DEBUG
    printf("Chip bank contents:\n");
    for (int i = 0; i <= chip_count; i++) {
        if (chip_names[i][0] != '\0') {
            printf("chip_names[%d] = '%s'\n", i, chip_names[i]);
        } else {
            printf("chip_names[%d] = (empty)\n", i);
        }
    }
    printf("Total chips loaded: %d\n", chip_count);
#endif

    // Read program.txt
    FILE *prog_fp = fopen(argv[1], "r");
    if (prog_fp == NULL) {
        printf("Error opening %s\n", argv[1]);
        return 1;
    }
    int instruction_part = 0;
    while (fgets(line, sizeof(line), prog_fp) && num_instructions < MAX_INSTRUCTIONS) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#' || strlen(line) == 0) continue;
        if (strlen(line) < 16) {
            printf("Warning: Line too short in program.txt: '%s'\n", line);
            continue;
        }
        int is_binary = 1;
        for (int i = 0; i < 16; i++) {
            if (line[i] != '0' && line[i] != '1') {
                is_binary = 0;
                break;
            }
        }
        if (!is_binary) {
            printf("Warning: Non-binary characters in first 16 chars: '%s'\n", line);
            continue;
        }
        char binary_str[17];
        strncpy(binary_str, line, 16);
        binary_str[16] = '\0';
        unsigned short value = strtoul(binary_str, NULL, 2);
        program[num_instructions][instruction_part] = value;
        instruction_part++;
        if (instruction_part == 4) {
            num_instructions++;
            instruction_part = 0;
        }
    }
    fclose(prog_fp);

    // Initialize RAM and clear cli_tape.txt
    for (int i = 0; i < 256; i++) ram[i] = 0;
    write_ram(ram);
    
    FILE *tape_fp = fopen("cli_tape.txt", "w");
    if (tape_fp == NULL) {
        printf("Error creating cli_tape.txt\n");
        return 1;
    }
    fclose(tape_fp);

    // Initial run with specified number of cycles
    printf("Initial run:\n");
    for (int i = 0; i < num_cycles; i++) {
        run_cycle();
        printf("Completed cycle %d/%d\n", i + 1, num_cycles);
        printf("Tape contents after cycle %d:\n", i + 1);
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
    }

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
        else if (choice == '1') switch_0 = 1 - switch_0;
        else if (choice == '2') switch_1 = 1 - switch_1;
        else if (choice == 's') {
            running = 0;
            run_cycle();
        } else if (choice == 'r') {
            running = 1;
        }

        if (running) {
            run_cycle();
            printf("\033[2J\033[1;1H");
        }
    }

    printf("Emulator stopped.\n");
    return 0;
}
