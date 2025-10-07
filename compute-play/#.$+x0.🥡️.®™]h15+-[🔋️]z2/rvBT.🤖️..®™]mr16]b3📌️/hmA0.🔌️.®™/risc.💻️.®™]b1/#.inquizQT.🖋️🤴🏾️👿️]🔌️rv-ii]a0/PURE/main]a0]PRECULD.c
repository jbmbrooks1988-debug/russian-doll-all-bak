#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256
#define MAX_CYCLES 1000

// Read netlist and execute one cycle
int execute_cycle(FILE* netlist_fp, char* last_output_file, long cycle) {
    char line[MAX_LINE];
    fseek(netlist_fp, 0, SEEK_SET); // Reset to start of netlist
    int step = 0;
    last_output_file[0] = '\0'; // Clear last output file
    FILE* visual_mem_fp = fopen("visual_mem.txt", "a"); // Open in main dir
    if (!visual_mem_fp) {
        printf("Failed to open visual_mem.txt\n");
        return 1;
    }
    fprintf(visual_mem_fp, "Cycle %ld NAND Outputs: ", cycle);

    while (fgets(line, MAX_LINE, netlist_fp)) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) {
            printf("DEBUG: Skipping empty line at step %d\n", step);
            continue;
        }

        // Log the raw command
        printf("DEBUG: Step %d, raw command: '%s'\n", step, line);

        // Execute the command
        char command[MAX_LINE];
        snprintf(command, MAX_LINE, "%s", line);
        printf("DEBUG: Executing step %d: %s\n", step, command);
        int result = system(command);
        if (result != 0) {
            printf("Failed to execute: %s\n", command);
            fclose(visual_mem_fp);
            return 1;
        }

        // Extract the output file (last argument)
        char line_copy[MAX_LINE];
        strncpy(line_copy, line, MAX_LINE);
        char* token = strtok(line_copy, " ");
        char* last_token = NULL;
        int arg_count = 0;
        while (token) {
            last_token = token;
            token = strtok(NULL, " ");
            arg_count++;
        }
        if (last_token && arg_count == 3) { // ./+x/nand.+x <input> <output>
            strncpy(last_output_file, last_token, MAX_LINE - 1);
            last_output_file[MAX_LINE - 1] = '\0';
            printf("DEBUG: Last output file set to %s\n", last_output_file);

            // Read the NAND output from the output file
            FILE* output_fp = fopen(last_output_file, "r");
            if (output_fp) {
                char output_line[10];
                if (fgets(output_line, 10, output_fp)) {
                    output_line[strcspn(output_line, "\n")] = '\0';
                    if (strlen(output_line) == 1 && (output_line[0] == '0' || output_line[0] == '1')) {
                        fprintf(visual_mem_fp, "%s", output_line); // Append to binary string
                        fprintf(visual_mem_fp, "\n%s:%s", last_output_file, output_line); // Log name-value pair
                    } else {
                        fprintf(visual_mem_fp, "\n%s:invalid", last_output_file);
                    }
                } else {
                    fprintf(visual_mem_fp, "\n%s:empty", last_output_file);
                }
                fclose(output_fp);
            } else {
                printf("DEBUG: Failed to read output file %s\n", last_output_file);
                fprintf(visual_mem_fp, "\n%s:missing", last_output_file);
            }
        } else {
            printf("DEBUG: Insufficient arguments (%d) at step %d\n", arg_count, step);
        }
        step++;
    }
    fprintf(visual_mem_fp, "\n\n"); // Separate cycles
    fclose(visual_mem_fp);
    printf("DEBUG: Cycle completed, final last_output_file: %s\n", last_output_file);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <netlist_file> <output_file> <cycles>\n", argv[0]);
        return 1;
    }

    const char* netlist_file = argv[1];
    const char* output_file = argv[2];
    long max_cycles = atol(argv[3]);
    if (max_cycles < -1 || max_cycles > MAX_CYCLES) {
        printf("Cycles must be -1 or between 0 and %d\n", MAX_CYCLES);
        return 1;
    }
    int infinite = (max_cycles == -1);

    // Open netlist file
    FILE* netlist_fp = fopen(netlist_file, "r");
    if (!netlist_fp) {
        printf("Failed to open netlist file %s\n", netlist_file);
        return 1;
    }

    // Clear visual_mem.txt at the start
    FILE* visual_mem_fp = fopen("visual_mem.txt", "w");
    if (!visual_mem_fp) {
        printf("Failed to open visual_mem.txt\n");
        fclose(netlist_fp);
        return 1;
    }
    fclose(visual_mem_fp);

    long cycles = 0;
    int running = 1;
    char last_output_file[MAX_LINE];

    // Main emulation loop
    while (running && (infinite || cycles < max_cycles)) {
        printf("Cycle %ld\n", cycles);
        if (execute_cycle(netlist_fp, last_output_file, cycles)) {
            printf("Cycle %ld failed\n", cycles);
            fclose(netlist_fp);
            return 1;
        }
        cycles++;
    }

    fclose(netlist_fp);

    // Copy last output to final output file
    printf("DEBUG: Attempting to copy final output\n");
    if (last_output_file[0]) {
        printf("DEBUG: Copying from %s to %s\n", last_output_file, output_file);
        FILE* src = fopen(last_output_file, "r");
        FILE* dst = fopen(output_file, "w");
        if (src && dst) {
            char line[10];
            if (fgets(line, 10, src)) {
                fprintf(dst, "%s", line);
                printf("DEBUG: Copied line: %s\n", line);
            } else {
                printf("DEBUG: Failed to read from %s\n", last_output_file);
            }
            fclose(src);
            fclose(dst);
        } else {
            printf("DEBUG: Failed to open files (src=%p, dst=%p)\n", src, dst);
        }
    } else {
        printf("DEBUG: No last output file set\n");
    }

    // Debug file contents
    printf("DEBUG: Contents of %s:\n", output_file);
    char command[MAX_LINE];
    snprintf(command, MAX_LINE, "cat %s", output_file);
    system(command);
    printf("DEBUG: Output file %s written\n", output_file);

    printf("Total cycles executed: %ld\n", cycles);
    return 0;
}
