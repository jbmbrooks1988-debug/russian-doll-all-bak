#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_LINE 1024
#define MAX_STATE 512
#define MAX_SIGNALS 32

// Parse netlist to initialize state array
int init_state(const char* netlist_file, int state[], char* signal_names[], int* state_count) {
    FILE* fp = fopen(netlist_file, "r");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", netlist_file);
        return 1;
    }

    *state_count = 0;
    char line[MAX_LINE];
    char signals[MAX_STATE][MAX_LINE];
    int signal_count = 0;

    // Collect unique signal names
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char in1[MAX_LINE], in2[MAX_LINE], out[MAX_LINE];
        if (sscanf(line, "./+x/nand.+x %s %s %s", in1, in2, out) != 3) {
            printf("ERROR: Invalid netlist line: %s\n", line);
            fclose(fp);
            return 1;
        }

        // Check each signal
        for (int i = 0; i < signal_count; i++) {
            if (strcmp(signals[i], in1) == 0) in1[0] = '\0';
            if (strcmp(signals[i], in2) == 0) in2[0] = '\0';
            if (strcmp(signals[i], out) == 0) out[0] = '\0';
        }
        if (in1[0] && signal_count < MAX_STATE) {
            strncpy(signals[signal_count], in1, MAX_LINE - 1);
            signal_count++;
        }
        if (in2[0] && signal_count < MAX_STATE) {
            strncpy(signals[signal_count], in2, MAX_LINE - 1);
            signal_count++;
        }
        if (out[0] && signal_count < MAX_STATE) {
            strncpy(signals[signal_count], out, MAX_LINE - 1);
            signal_count++;
        }
    }
    fclose(fp);

    // Initialize state and signal_names
    for (int i = 0; i < signal_count; i++) {
        state[i] = 0;
        signal_names[i] = strdup(signals[i]);
    }
    for (int i = signal_count; i < MAX_STATE; i++) {
        state[i] = 0;
        signal_names[i] = NULL;
    }
    *state_count = signal_count;
    printf("DEBUG: Initialized %d signals\n", signal_count);
    return 0;
}

// Free state
void free_state(char* signal_names[]) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (signal_names[i]) free(signal_names[i]);
    }
}

// Ensure tmp directory exists
int ensure_tmp_dir() {
    struct stat st = {0};
    if (stat("tmp", &st) == -1) {
        if (mkdir("tmp", 0755) == -1) {
            printf("ERROR: Failed to create tmp directory\n");
            return 1;
        }
        printf("DEBUG: Created tmp directory\n");
    }
    return 0;
}

// Write state to tmp/state.txt
int write_state(const char* state_file, int state[], char* signal_names[], int state_count) {
    FILE* fp = fopen(state_file, "w");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", state_file);
        return 1;
    }
    for (int i = 0; i < state_count; i++) {
        fprintf(fp, "%s=%d\n", signal_names[i], state[i]);
    }
    fclose(fp);
    printf("DEBUG: Wrote state to %s\n", state_file);
    return 0;
}

// Write inputs for one NAND operation
int write_inputs(const char* input_file, int state[], char* signal_names[], int state_count, const char* netlist_file, int cycle, int stateful_mode, int* netlist_idx) {
    FILE* fp = fopen(netlist_file, "r");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", netlist_file);
        return 1;
    }

    char line[MAX_LINE];
    int current_line = 0;
    *netlist_idx = cycle % 12; // 12 lines in netlist_rv_ii.txt
    while (fgets(line, MAX_LINE, fp)) {
        if (current_line == *netlist_idx) {
            break;
        }
        current_line++;
    }
    fclose(fp);
    if (!line[0]) {
        printf("ERROR: No netlist line for cycle %d\n", cycle);
        return 1;
    }
    line[strcspn(line, "\n")] = '\0';

    char in1[MAX_LINE], in2[MAX_LINE], out[MAX_LINE];
    if (sscanf(line, "./+x/nand.+x %s %s %s", in1, in2, out) != 3) {
        printf("ERROR: Invalid netlist line: %s\n", line);
        return 1;
    }

    FILE* fp_out = fopen(input_file, "w");
    if (!fp_out) {
        printf("ERROR: Failed to open %s\n", input_file);
        return 1;
    }

    if (stateful_mode) {
        int found_in1 = 0, found_in2 = 0;
        for (int i = 0; i < state_count; i++) {
            if (strcmp(signal_names[i], in1) == 0) {
                fprintf(fp_out, "%s=%d\n", in1, state[i]);
                printf("DEBUG: Wrote %s=%d to %s\n", in1, state[i], input_file);
                found_in1 = 1;
            }
            if (strcmp(signal_names[i], in2) == 0) {
                fprintf(fp_out, "%s=%d\n", in2, state[i]);
                printf("DEBUG: Wrote %s=%d to %s\n", in2, state[i], input_file);
                found_in2 = 1;
            }
        }
        if (!found_in1 || !found_in2) {
            printf("ERROR: Missing input signal %s or %s\n", in1, in2);
            fclose(fp_out);
            return 1;
        }
    } else {
        // Stateless: XOR example
        int values[] = {1, 1};
        char* signals[] = {"A", "B"};
        for (int i = 0; i < 2; i++) {
            fprintf(fp_out, "%s=%d\n", signals[i], values[i]);
            printf("DEBUG: Wrote %s=%d to %s\n", signals[i], values[i], input_file);
        }
        *netlist_idx = 0;
    }
    fclose(fp_out);
    return 0;
}

// Update state from tmp/output.txt
int update_state(int state[], char* signal_names[], int state_count, int stateful_mode) {
    if (!stateful_mode) return 0;

    FILE* fp = fopen("tmp/output.txt", "r");
    if (!fp) {
        printf("ERROR: Failed to open tmp/output.txt\n");
        return 1;
    }
    char line[MAX_LINE];
    if (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            int value = atoi(eq + 1);
            for (int i = 0; i < state_count; i++) {
                if (signal_names[i] && strcmp(line, signal_names[i]) == 0) {
                    state[i] = value;
                    printf("DEBUG: Updated %s=%d\n", signal_names[i], value);
                    break;
                }
            }
        }
    }
    fclose(fp);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 5 || argc > 6) {
        printf("Usage: %s <netlist_file> <output_file> <expected_lines> <num_cycles> [--stateful]\n", argv[0]);
        return 1;
    }

    const char* netlist_file = argv[1];
    // output_file = argv[2] (unused, using tmp/output.txt)
    int expected_lines = atoi(argv[3]);
    int num_cycles = atoi(argv[4]);
    int stateful_mode = (argc == 6 && strcmp(argv[5], "--stateful") == 0);

    if (num_cycles <= 0) {
        printf("ERROR: Number of cycles must be positive\n");
        return 1;
    }

    if (ensure_tmp_dir()) {
        return 1;
    }

    int state[MAX_STATE] = {0};
    char* signal_names[MAX_STATE] = {0};
    int state_count;
    if (init_state(netlist_file, state, signal_names, &state_count)) {
        free_state(signal_names);
        return 1;
    }
    if (write_state("tmp/state.txt", state, signal_names, state_count)) {
        free_state(signal_names);
        return 1;
    }

    char command[MAX_LINE];
    for (int cycle = 0; cycle < num_cycles; cycle++) {
        printf("Starting cycle %d/%d\n", cycle + 1, num_cycles);

        int netlist_idx;
        if (write_inputs("tmp/input.txt", state, signal_names, state_count, netlist_file, cycle, stateful_mode, &netlist_idx)) {
            printf("ERROR: Failed to write inputs for cycle %d\n", cycle);
            free_state(signal_names);
            return 1;
        }

        char tmp_netlist[MAX_LINE];
        snprintf(tmp_netlist, MAX_LINE, "tmp/netlist_%d.txt", cycle);
        FILE* fp = fopen(netlist_file, "r");
        FILE* tmp_fp = fopen(tmp_netlist, "w");
        if (!fp || !tmp_fp) {
            printf("ERROR: Failed to open netlist or tmp netlist\n");
            if (fp) fclose(fp);
            if (tmp_fp) fclose(tmp_fp);
            free_state(signal_names);
            return 1;
        }
        char line[MAX_LINE];
        int current_line = 0;
        while (fgets(line, MAX_LINE, fp)) {
            if (current_line == netlist_idx) {
                fprintf(tmp_fp, "%s", line);
                break;
            }
            current_line++;
        }
        fclose(fp);
        fclose(tmp_fp);

        snprintf(command, MAX_LINE, "./+x/main.+x %s tmp/output.txt %d", tmp_netlist, 1);
        printf("DEBUG: Executing: %s\n", command);
        int status = system(command);
        if (WEXITSTATUS(status) != 0) {
            printf("ERROR: main.+x failed at cycle %d\n", cycle);
            free_state(signal_names);
            return 1;
        }

        if (update_state(state, signal_names, state_count, stateful_mode)) {
            printf("ERROR: Failed to update state for cycle %d\n", cycle);
            free_state(signal_names);
            return 1;
        }

        if (write_state("tmp/state.txt", state, signal_names, state_count)) {
            printf("ERROR: Failed to write state for cycle %d\n", cycle);
            free_state(signal_names);
            return 1;
        }
    }

    printf("Completed %d cycles\n", num_cycles);
    free_state(signal_names);
    return 0;
}
