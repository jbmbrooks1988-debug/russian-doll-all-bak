#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTIONS 100
#define MAX_CHIPS 65536

char chip_names[MAX_CHIPS][32];
unsigned short program[MAX_INSTRUCTIONS][4];
unsigned char ram[256];
unsigned char switch_0 = 0, switch_1 = 0;
unsigned char clock = 0;
int clock_iterations = 0;
int num_instructions = 0;

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
                    printf("Wrote %d to cli_tape.txt\n", input_b); // Debug output
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
                    printf("Wrote %d to cli_tape.txt\n", input_a); // Debug output
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
            printf("Executing: %s\n", cmd); // Debug output
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

    // Read program.txt
    FILE *prog_fp = fopen(argv[1], "r");
    if (prog_fp == NULL) {
        printf("Error opening %s\n", argv[1]);
        return 1;
    }
    int instruction_part = 0;
    while (fgets(line, sizeof(line), prog_fp) && num_instructions < MAX_INSTRUCTIONS) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#' || strlen(line) != 16) continue;
        int is_binary = 1;
        for (int i = 0; i < 16; i++) {
            if (line[i] != '0' && line[i] != '1') {
                is_binary = 0;
                break;
            }
        }
        if (!is_binary) continue;
        unsigned short value = 0;
        for (int i = 0; i < 16; i++) {
            value = (value << 1) | (line[i] - '0');
        }
        program[num_instructions][instruction_part] = value;
        instruction_part++;
        if (instruction_part == 4) {
            num_instructions++;
            instruction_part = 0;
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

        printf("Current switch_0 = %d, switch_1 = %d\n", switch_0, switch_1);
        printf("Change switches? (y/n): ");
        char change_switches;
        scanf(" %c", &change_switches);
        if (change_switches == 'y' || change_switches == 'Y') {
            printf("Set switch_0 (0/1): ");
            scanf("%hhu", &switch_0);
            printf("Set switch_1 (0/1): ");
            scanf("%hhu", &switch_1);
        }

        printf("Options: 's' to step, 'r' to run, 'q' to quit: ");
        char choice;
        scanf(" %c", &choice);

        if (choice == 'q') break;
        else if (choice == 's') {
            running = 0;
            run_cycle();
        } else if (choice == 'r') {
            running = 1;
        }

        if (running) {
            run_cycle();
            printf("\033[2J\033[1;1H"); // Clear screen
        }
    }

    printf("Emulator stopped.\n");
    return 0;
}
