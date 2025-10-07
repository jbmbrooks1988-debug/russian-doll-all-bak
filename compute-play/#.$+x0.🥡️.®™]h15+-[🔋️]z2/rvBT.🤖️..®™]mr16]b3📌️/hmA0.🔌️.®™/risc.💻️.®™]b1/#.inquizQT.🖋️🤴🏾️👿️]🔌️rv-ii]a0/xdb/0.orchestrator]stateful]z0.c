#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 512
#define MAX_GATES 100
#define MAX_STATE 256

// Write quantum mode to tmp/quantum_mode.txt
int set_quantum_mode(int quantum_mode) {
    FILE* fp = fopen("tmp/quantum_mode.txt", "w");
    if (!fp) {
        printf("ERROR: Failed to open tmp/quantum_mode.txt\n");
        return 1;
    }
    fprintf(fp, "%s\n", quantum_mode ? "quantum" : "classical");
    fclose(fp);
    printf("DEBUG: Set mode to %s in tmp/quantum_mode.txt\n", quantum_mode ? "quantum" : "classical");
    return 0;
}

// Write inputs to tmp/input.txt, supporting stateful and classic modes
int write_inputs(const char* input_file, int cycle, int num_inputs, int quantum_mode, const char* hash, const char* netlist_file, int stateful_mode) {
    FILE* fp = fopen(input_file, "w");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", input_file);
        return 1;
    }

    if (stateful_mode) {
        // Stateful mode (e.g., RV-II): read from in.txt (cycle 0) or tmp/state.txt
        char state[MAX_STATE][MAX_LINE];
        int state_count = 0;
        FILE* state_fp = (cycle == 0) ? fopen("in.txt", "r") : fopen("tmp/state.txt", "r");
        if (state_fp) {
            char line[MAX_LINE];
            while (fgets(line, MAX_LINE, state_fp) && state_count < MAX_STATE) {
                line[strcspn(line, "\n")] = '\0';
                strncpy(state[state_count], line, MAX_LINE - 1);
                state[state_count][MAX_LINE - 1] = '\0';
                state_count++;
            }
            fclose(state_fp);
        } else if (cycle > 0) {
            printf("ERROR: No tmp/state.txt for cycle %d\n", cycle);
            fclose(fp);
            return 1;
        }

        // Write inputs based on state
        for (int i = 0; i < state_count; i++) {
            char* value = strchr(state[i], '=');
            if (value) {
                fprintf(fp, "%s\n", value + 1); // Write value (e.g., "0" or "1")
            }
        }
    } else {
        // Classic mode (e.g., XOR, quantum): original logic
        int toffoli_inputs[] = {1, 1, 0, 0, 0};
        int bell_inputs[] = {0, 0, 0, 0, 0};
        char* signal_names[] = {"INPUT_A", "INPUT_B", "INPUT_C", "ANCILLA_1", "ANCILLA_2"};
        char* output_names[] = {"OUTPUT_A", "OUTPUT_B", "OUTPUT_C", "OUTPUT_QUBIT_1", "OUTPUT_QUBIT_2"};
        int* inputs = (cycle == 0) ? toffoli_inputs : bell_inputs;
        int is_t_reversibility = (strstr(netlist_file, "t_reversibility") != NULL);
        if (is_t_reversibility) {
            signal_names[3] = "INPUT_T";
            inputs = toffoli_inputs;
        }
        for (int i = 0; i < num_inputs; i++) {
            int bit = inputs[i];
            if (quantum_mode && cycle > 0 && !is_t_reversibility) {
                char state_file[MAX_LINE];
                snprintf(state_file, MAX_LINE, "tmp/STATE_%s.%s.txt", output_names[i], hash);
                FILE* state_fp = fopen(state_file, "r");
                if (state_fp) {
                    double real0, imag0, real1, imag1;
                    if (fscanf(state_fp, "%lf %lf %lf %lf", &real0, &imag0, &real1, &imag1) == 4) {
                        double prob_1 = real1 * real1 + imag1 * imag1;
                        bit = (prob_1 > 0.5) ? 1 : 0;
                        printf("DEBUG: Measured %s from %s: prob_1=%f, bit=%d\n", output_names[i], state_file, prob_1, bit);
                    } else {
                        printf("DEBUG: Invalid state in %s, using default bit=%d\n", state_file, bit);
                    }
                    fclose(state_fp);
                } else {
                    printf("DEBUG: No state file %s, using default bit=%d\n", state_file, bit);
                }
            }
            fprintf(fp, "%d\n", bit);
        }
        fprintf(fp, "%d\n", cycle % 2); // CLOCK
    }

    fclose(fp);
    printf("DEBUG: Wrote inputs to %s for cycle %d\n", input_file, cycle);
    return 0;
}

// Update state from tmp/output.txt for stateful mode
int update_state(const char* hash, int stateful_mode) {
    if (!stateful_mode) return 0;

    FILE* out_fp = fopen("tmp/output.txt", "r");
    FILE* state_fp = fopen("tmp/state.txt", "w");
    if (!out_fp || !state_fp) {
        printf("ERROR: Failed to open tmp/output.txt or tmp/state.txt\n");
        if (out_fp) fclose(out_fp);
        if (state_fp) fclose(state_fp);
        return 1;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, out_fp)) {
        line[strcspn(line, "\n")] = '\0';
        fprintf(state_fp, "%s\n", line);
    }

    fclose(out_fp);
    fclose(state_fp);
    return 0;
}

// Reverse netlist (unchanged)
int reverse_netlist(const char* netlist_file, int quantum_mode) {
    char lines[MAX_GATES][MAX_LINE];
    int num_lines = 0;
    FILE* fp = fopen(netlist_file, "r");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", netlist_file);
        return 1;
    }
    while (num_lines < MAX_GATES && fgets(lines[num_lines], MAX_LINE, fp)) {
        lines[num_lines][strcspn(lines[num_lines], "\n")] = '\0';
        num_lines++;
    }
    fclose(fp);
    FILE* fp_out = fopen("tmp/netlist_reverse.txt", "w");
    if (!fp_out) {
        printf("ERROR: Failed to open tmp/netlist_reverse.txt\n");
        return 1;
    }
    for (int i = num_lines - 1; i >= 0; i--) {
        char gate[MAX_LINE], in1[MAX_LINE], in2[MAX_LINE], in3[MAX_LINE];
        char out1[MAX_LINE], out2[MAX_LINE], out3[MAX_LINE];
        char new_line[MAX_LINE];
        if (sscanf(lines[i], "./+x/quantum.+x %s %s %s %s %s %s %s",
                   gate, in1, in2, in3, out1, out2, out3) == 7) {
            snprintf(new_line, MAX_LINE, "./+x/quantum.+x %s %s %s %s %s %s %s",
                     gate, out1, out2, out3, in1, in2, in3);
        } else if (sscanf(lines[i], "./+x/quantum.+x %s %s %s %s %s",
                          gate, in1, in2, out1, out2) == 5) {
            snprintf(new_line, MAX_LINE, "./+x/quantum.+x %s %s %s %s %s",
                     gate, out1, out2, in1, in2);
        } else if (sscanf(lines[i], "./+x/quantum.+x %s %s %s",
                          gate, in1, out1) == 3) {
            const char* new_gate = (strcmp(gate, "t") == 0 && quantum_mode) ? "t_dagger" : gate;
            snprintf(new_line, MAX_LINE, "./+x/quantum.+x %s %s %s",
                     new_gate, out1, in1);
        } else {
            printf("ERROR: Invalid netlist line: %s\n", lines[i]);
            fclose(fp_out);
            return 1;
        }
        fprintf(fp_out, "%s\n", new_line);
        printf("DEBUG: Reversed line: %s\n", new_line);
    }
    fclose(fp_out);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 5 || argc > 8) {
        printf("Usage: %s <netlist_file> <output_file> <expected_lines> <num_cycles> [--quantum] [--reverse] [--stateful]\n", argv[0]);
        return 1;
    }

    const char* netlist_file = argv[1];
    const char* output_file = argv[2];
    int expected_lines = atoi(argv[3]);
    int num_cycles = atoi(argv[4]);
    int quantum_mode = 0;
    int reverse_mode = 0;
    int stateful_mode = 0;
    int manual_mode = 0;
    for (int i = 5; i < argc; i++) {
        if (strcmp(argv[i], "--quantum") == 0) quantum_mode = 1;
        if (strcmp(argv[i], "--reverse") == 0) reverse_mode = 1;
        if (strcmp(argv[i], "--stateful") == 0) stateful_mode = 1;
        if (strcmp(argv[i], "manual") == 0) manual_mode = 1;
    }

    if (num_cycles <= 0) {
        printf("ERROR: Number of cycles must be positive\n");
        return 1;
    }

    char command[MAX_LINE];
    char input_file[] = "tmp/input.txt";
    int num_inputs = stateful_mode ? 18 : 5; // RV-II: 8 reg + 4 PC + 4 INST + 2 OP2
    char hash[MAX_LINE];
    snprintf(hash, MAX_LINE, "%ld", (long)getpid());

    const char* active_netlist = netlist_file;
    if (reverse_mode) {
        if (reverse_netlist(netlist_file, quantum_mode)) {
            printf("ERROR: Failed to reverse netlist\n");
            return 1;
        }
        active_netlist = "tmp/netlist_reverse.txt";
    }

    for (int cycle = 0; cycle < num_cycles; cycle++) {
        printf("Starting cycle %d/%d\n", cycle + 1, num_cycles);

        if (set_quantum_mode(quantum_mode)) {
            printf("ERROR: Failed to set quantum mode for cycle %d\n", cycle);
            return 1;
        }

        if (write_inputs(input_file, cycle, num_inputs, quantum_mode, hash, netlist_file, stateful_mode)) {
            printf("ERROR: Failed to write inputs for cycle %d\n", cycle);
            return 1;
        }

        snprintf(command, MAX_LINE, "./+x/main.+x %s %s %d", active_netlist, output_file, expected_lines);
        printf("DEBUG: Executing: %s\n", command);
        int status = system(command);
        if (WEXITSTATUS(status) != 0) {
            printf("ERROR: main.+x failed at cycle %d\n", cycle);
            return 1;
        }

        if (update_state(hash, stateful_mode)) {
            printf("ERROR: Failed to update state for cycle %d\n", cycle);
            return 1;
        }

        if (manual_mode) {
            printf("Press Enter to advance to cycle %d (or Ctrl+C to stop)...\n", cycle + 2);
            getchar();
        }

        // Display outputs (unchanged)
        char output_files[][MAX_LINE] = {
            "tmp/OUTPUT_A.*.txt",
            "tmp/OUTPUT_B.*.txt",
            "tmp/OUTPUT_C.*.txt",
            "tmp/OUTPUT_QUBIT_1.*.txt",
            "tmp/OUTPUT_QUBIT_2.*.txt",
            "tmp/OUTPUT_T.*.txt"
        };
        char state_files[][MAX_LINE] = {
            "tmp/STATE_OUTPUT_A.*.txt",
            "tmp/STATE_OUTPUT_B.*.txt",
            "tmp/STATE_OUTPUT_C.*.txt",
            "tmp/STATE_OUTPUT_QUBIT_1.*.txt",
            "tmp/STATE_OUTPUT_QUBIT_2.*.txt",
            "tmp/STATE_OUTPUT_T.*.txt"
        };
        for (int i = 0; i < 6; i++) {
            char cat_cmd[MAX_LINE];
            snprintf(cat_cmd, MAX_LINE, "cat %s 2>/dev/null", output_files[i]);
            printf("DEBUG: Output %s (cycle %d):\n", output_files[i], cycle);
            system(cat_cmd);
            if (quantum_mode) {
                snprintf(cat_cmd, MAX_LINE, "cat %s 2>/dev/null", state_files[i]);
                printf("DEBUG: Quantum state %s (cycle %d):\n", state_files[i], cycle);
                system(cat_cmd);
            }
        }
    }

    printf("Completed %d cycles\n", num_cycles);
    return 0;
}
