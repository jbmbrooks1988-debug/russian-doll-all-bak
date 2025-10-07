#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

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

// Check if a key has been pressed (non-blocking)
int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

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
    if (raw_input == 0) return 0;
    else if (raw_input == 1) return 1;
    else if (raw_input == 5) return switch_0;
    else if (raw_input == 6) return switch_1;
    else if (raw_input == 7) return clock;
    else if (raw_input > 15) {
        int addr = raw_input % 256;
        int value = ram[addr]; // Read without clearing
        return value;
    } else {
        printf("Invalid input: %d\n", raw_input);
        *is_blank = 1;
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
                        continue;
                    }
                    fprintf(tape_fp, "%d%s", input_b, existing);
                    fclose(tape_fp);
                } else if (ram_output_address < 256) {
                    ram[ram_output_address] = input_b;
                    write_ram(ram);
                }
            } else if (is_blank_b && !is_blank_a && input_a != -1) {
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
                        continue;
                    }
                    fprintf(tape_fp, "%d%s", input_a, existing);
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
            system(cmd); // Assume NAND updates ram[ram_output_address]
            if (program[i][3] == 1 && ram_output_address < 256) { // Tape output
                char existing[1024] = "";
                FILE *tape_fp = fopen("cli_tape.txt", "r");
                if (tape_fp) {
                    if (fgets(existing, sizeof(existing), tape_fp)) {
                        existing[strcspn(existing, "\n")] = 0;
                    }
                    fclose(tape_fp);
                }
                tape_fp = fopen("cli_tape.txt", "w");
                if (tape_fp) {
                    fprintf(tape_fp, "%d%s", ram[ram_output_address], existing);
                    fclose(tape_fp);
                }
            }
        } else {
            printf("Error: Invalid chip_location %d at instruction %d\n", chip_location, i);
        }
    }
    clock = 1 - clock;
    clock_iterations++;
}

// Read and parse chip_bank.txt
int read_chip_bank(const char *filename, char chip_names[][32]) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening %s\n", filename);
        return 1;
    }
    char line[50];
    int chip_count = 0;
    while (fgets(line, sizeof(line), fp) && chip_count < MAX_CHIPS - 1) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#' || line[0] == '\0') continue;
        strncpy(chip_names[chip_count + 1], line, 32);
        chip_count++;
    }
    fclose(fp);
    chip_names[0][0] = 0;
    return 0;
}

// Read and parse program.txt
int read_program(const char *filename, unsigned short program[][4], int *num_instructions) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening %s\n", filename);
        return 1;
    }

    char binary_str[17];
    int count = 0;
    int instruction_part = 0;
    *num_instructions = 0;
    int in_comment = 0;

    while (*num_instructions < MAX_INSTRUCTIONS) {
        int c = fgetc(fp);
        if (c == EOF) break;

        if (c == '#') {
            in_comment = 1;
            continue;
        }
        if (c == '\n') {
            in_comment = 0;
            continue;
        }
        if (in_comment) continue;

        if (c == '0' || c == '1') {
            if (count < 16) {
                binary_str[count++] = c;
            }
            if (count == 16) {
                binary_str[count] = '\0';
                unsigned short value = strtoul(binary_str, NULL, 2);
                printf("Parsing instruction %d, part %d: %s -> %u (binary: ", *num_instructions, instruction_part, binary_str, value);
                for (int i = 15; i >= 0; i--) {
                    printf("%d", (value >> i) & 1);
                }
                printf(")\n");
                program[*num_instructions][instruction_part] = value;
                instruction_part++;
                count = 0;
                if (instruction_part == 4) {
                    (*num_instructions)++;
                    instruction_part = 0;
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

// Run the main interactive loop
void run_main_loop() {
    // Initial run
    printf("Initial run:\n");
    run_cycle();

    // Main loop
    int running = 0;
    while (1) {
        printf("\nClock cycle %d, clock = %d\n", clock_iterations, clock);
        printf("Tape contents: ");
        FILE *tape_fp = fopen("cli_tape.txt", "r");
        if (tape_fp) {
            char buf[1024];
            if (fgets(buf, sizeof(buf), tape_fp)) {
                buf[strcspn(buf, "\n")] = 0;
                printf("%s", buf);
            } else {
                printf("(empty)");
            }
            fclose(tape_fp);
        } else {
            printf("(empty)");
        }
        printf("\n");

        printf("Switches: switch_0 = %d, switch_1 = %d\n", switch_0, switch_1);
        printf("Options: '1' flip switch_0, '2' flip switch_1, 's' step, 'r' run, 'q' quit, [Enter] step: ");
        
        char input[10];
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;
        char choice = input[0];

        if (choice == 'q') break;
        else if (choice == '1') switch_0 = 1 - switch_0;
        else if (choice == '2') switch_1 = 1 - switch_1;
        else if (choice == 's' || choice == 0) {
            running = 0;
            run_cycle();
        } else if (choice == 'r') {
            running = 1;
            while (running) {
                run_cycle();
                printf("\033[2J\033[1;1H");
                printf("\nClock cycle %d, clock = %d\n", clock_iterations, clock);
                printf("Tape contents: ");
                tape_fp = fopen("cli_tape.txt", "r");
                if (tape_fp) {
                    char buf[1024];
                    if (fgets(buf, sizeof(buf), tape_fp)) {
                        buf[strcspn(buf, "\n")] = 0;
                        printf("%s", buf);
                    } else {
                        printf("(empty)");
                    }
                    fclose(tape_fp);
                } else {
                    printf("(empty)");
                }
                printf("\n");
                printf("Switches: switch_0 = %d, switch_1 = %d\n", switch_0, switch_1);
                printf("Running... Press 'p' to pause\n");

                if (kbhit()) {
                    char c = getchar();
                    if (c == 'p') {
                        running = 0;
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s program.txt\n", argv[0]);
        return 1;
    }

    // Read chip_bank.txt
    if (read_chip_bank("chip_bank.txt", chip_names) != 0) {
        return 1;
    }

    // Initialize RAM
    for (int i = 0; i < 256; i++) ram[i] = 0;
    write_ram(ram);

    // Clear cli_tape.txt only on startup
    FILE *fp = fopen("cli_tape.txt", "w");
    if (fp == NULL) {
        printf("Error creating cli_tape.txt\n");
        return 1;
    }
    fclose(fp);

    // Read program.txt
    if (read_program(argv[1], program, &num_instructions) != 0) {
        return 1;
    }

    // Run the main loop
    run_main_loop();

    printf("Emulator stopped.\n");
    return 0;
}
