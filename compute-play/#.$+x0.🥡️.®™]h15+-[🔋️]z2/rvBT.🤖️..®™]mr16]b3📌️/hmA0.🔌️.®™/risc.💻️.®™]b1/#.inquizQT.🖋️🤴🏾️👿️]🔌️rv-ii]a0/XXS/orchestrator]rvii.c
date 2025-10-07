#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 512
#define MAX_GATES 100

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

// Write inputs to tmp/input.txt, supporting RV-II and preserving original logic
int write_inputs(const char* input_file, int cycle, int num_inputs, int quantum_mode, const char* hash, const char* netlist_file, int rv_ii_mode) {
    FILE* fp = fopen(input_file, "w");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", input_file);
        return 1;
    }

    if (rv_ii_mode) {
        // RV-II state
        static unsigned char reg[4] = {0};
        static unsigned char mem[16] = {0};
        static unsigned char pc = 0;

        // Cycle 0: Load program from in.txt
        if (cycle == 0) {
            FILE* in = fopen("in.txt", "r");
            if (in) {
                for (int i = 0; i < 16 && !feof(in); i++) {
                    fscanf(in, "%hhu", &mem[i]);
                }
                fclose(in);
            }
        }

        // Write inputs: 8 reg bits, 4 PC bits, 4 INST bits, 2 OP2 bits, 8 mem bits (4 locations)
        for (int i = 0; i < 4; i++) {
            fprintf(fp, "%d\n", (reg[i] >> 0) & 1); // REGi_0
            fprintf(fp, "%d\n", (reg[i] >> 1) & 1); // REGi_1
        }
        for (int i = 0; i < 4; i++) {
            fprintf(fp, "%d\n", (pc >> i) & 1); // PC_i
        }
        for (int i = 0; i < 4; i++) {
            fprintf(fp, "%d\n", (mem[pc] >> i) & 1); // INST_i
        }
        for (int i = 0; i < 2; i++) {
            fprintf(fp, "%d\n", (mem[pc + 1] >> i) & 1); // OP2_i
        }
        for (int i = 0; i < 4; i++) {
            fprintf(fp, "%d\n", (mem[i] >> 0) & 1); // MEMi_0
            fprintf(fp, "%d\n", (mem[i] >> 1) & 1); // MEMi_1
        }

        // Update state from previous cycle (if not cycle 0)
        if (cycle > 0) {
            FILE* io_out = fopen("tmp/io_manager_out.txt", "r");
            if (io_out) {
                char line[MAX_LINE];
                while (fgets(line, MAX_LINE, io_out)) {
                    line[strcspn(line, "\n")] = '\0';
                    FILE* out_fp = fopen(line, "r");
                    if (out_fp) {
                        char val[10];
                        if (fgets(val, 10, out_fp)) {
                            int bit = atoi(val);
                            if (strstr(line, "OUTPUT_REG0_0")) reg[0] = (reg[0] & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_REG0_1")) reg[0] = (reg[0] & ~2) | (bit << 1);
                            else if (strstr(line, "OUTPUT_REG1_0")) reg[1] = (reg[1] & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_REG1_1")) reg[1] = (reg[1] & ~2) | (bit << 1);
                            else if (strstr(line, "OUTPUT_REG2_0")) reg[2] = (reg[2] & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_REG2_1")) reg[2] = (reg[2] & ~2) | (bit << 1);
                            else if (strstr(line, "OUTPUT_REG3_0")) reg[3] = (reg[3] & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_REG3_1")) reg[3] = (reg[3] & ~2) | (bit << 1);
                            else if (strstr(line, "OUTPUT_PC_0")) pc = (pc & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_PC_1")) pc = (pc & ~2) | (bit << 1);
                            else if (strstr(line, "OUTPUT_PC_2")) pc = (pc & ~4) | (bit << 2);
                            else if (strstr(line, "OUTPUT_PC_3")) pc = (pc & ~8) | (bit << 3);
                            else if (strstr(line, "OUTPUT_MEM0_0")) mem[0] = (mem[0] & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_MEM0_1")) mem[0] = (mem[0] & ~2) | (bit << 1);
                            else if (strstr(line, "OUTPUT_MEM1_0")) mem[1] = (mem[1] & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_MEM1_1")) mem[1] = (mem[1] & ~2) | (bit << 1);
                            else if (strstr(line, "OUTPUT_MEM2_0")) mem[2] = (mem[2] & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_MEM2_1")) mem[2] = (mem[2] & ~2) | (bit << 1);
                            else if (strstr(line, "OUTPUT_MEM3_0")) mem[3] = (mem[3] & ~1) | (bit << 0);
                            else if (strstr(line, "OUTPUT_MEM3_1")) mem[3] = (mem[3] & ~2) | (bit << 1);
                        }
                        fclose(out_fp);
                    }
                }
                fclose(io_out);
            }
        }
    } else {
        // Original logic for other netlists
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

// Reverse netlist and write to tmp/netlist_reverse.txt
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
    if (argc < 5 || argc > 9) {
        printf("Usage: %s <netlist_file> <output_file> <expected_lines> <num_cycles> [--quantum] [--reverse] [--rv-ii] [manual]\n", argv[0]);
        return 1;
    }

    const char* netlist_file = argv[1];
    const char* output_file = argv[2];
    int expected_lines = atoi(argv[3]);
    int num_cycles = atoi(argv[4]);
    int quantum_mode = 0;
    int reverse_mode = 0;
    int rv_ii_mode = 0;
    int manual_mode = 0;
    for (int i = 5; i < argc; i++) {
        if (strcmp(argv[i], "--quantum") == 0) quantum_mode = 1;
        if (strcmp(argv[i], "--reverse") == 0) reverse_mode = 1;
        if (strcmp(argv[i], "--rv-ii") == 0) rv_ii_mode = 1;
        if (strcmp(argv[i], "manual") == 0) manual_mode = 1;
    }

    if (num_cycles <= 0) {
        printf("ERROR: Number of cycles must be positive\n");
        return 1;
    }

    char command[MAX_LINE];
    char input_file[] = "tmp/input.txt";
    int num_inputs = rv_ii_mode ? 22 : 5; // RV-II: 8 reg + 4 PC + 4 INST + 2 OP2 + 8 mem; others: 5 inputs
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

        if (write_inputs(input_file, cycle, num_inputs, quantum_mode, hash, netlist_file, rv_ii_mode)) {
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

        if (manual_mode) {
            printf("Press Enter to advance to cycle %d (or Ctrl+C to stop)...\n", cycle + 2);
            getchar();
        }

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
