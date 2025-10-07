#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_LINE 512  // Buffer for file paths
#define MAX_STATE 256 // Max state size (real/imag pairs)

// Check if quantum mode is enabled via tmp/quantum_mode.txt
int is_quantum_mode() {
    FILE* fp = fopen("tmp/quantum_mode.txt", "r");
    if (!fp) return 0; // Default to classical mode
    char line[MAX_LINE];
    if (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "quantum") == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

// Matrix-vector multiplication for quantum gates
void mat_vec_mult(double* matrix, double* input, double* output, int size) {
    for (int i = 0; i < size; i++) {
        output[i * 2] = 0;      // Real part
        output[i * 2 + 1] = 0;  // Imag part
        for (int j = 0; j < size; j++) {
            double r = (matrix[(i * size + j) * 2] * input[j * 2] -
                        matrix[(i * size + j) * 2 + 1] * input[j * 2 + 1]);
            double img = (matrix[(i * size + j) * 2] * input[j * 2 + 1] +
                          matrix[(i * size + j) * 2 + 1] * input[j * 2]);
            output[i * 2] += r;
            output[i * 2 + 1] += img;
        }
    }
}

// Hadamard gate (1 qubit, 2x2 matrix)
void apply_hadamard(double* input, double* output) {
    double H[8] = {
        1/sqrt(2), 0, 1/sqrt(2), 0,
        1/sqrt(2), 0, -1/sqrt(2), 0
    };
    mat_vec_mult(H, input, output, 2);
}

// T gate (1 qubit, 2x2 matrix)
void apply_t_gate(double* input, double* output) {
    double T[8] = {
        1, 0, 0, 0,
        0, 0, M_SQRT1_2, M_SQRT1_2  // e^(iπ/4) = (1+i)/√2
    };
    mat_vec_mult(T, input, output, 2);
}

// T dagger gate (1 qubit, 2x2 matrix)
void apply_t_dagger(double* input, double* output) {
    double T_DAGGER[8] = {
        1, 0, 0, 0,
        0, 0, M_SQRT1_2, -M_SQRT1_2  // e^(-iπ/4) = (1-i)/√2
    };
    mat_vec_mult(T_DAGGER, input, output, 2);
}

// CNOT gate (2 qubits, 4x4 matrix)
void apply_cnot(double* input, double* output) {
    double CNOT[32] = {
        1, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 1, 0, 0, 0
    };
    mat_vec_mult(CNOT, input, output, 4);
}

// Toffoli gate (3 qubits, 8x8 matrix)
void apply_toffoli(double* input, double* output) {
    double TOFFOLI[128] = {
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0
    };
    mat_vec_mult(TOFFOLI, input, output, 8);
}

// Read classical bit or quantum state from file
int read_input_state(const char* file, double* state, int quantum_mode) {
    FILE* fp = fopen(file, "r");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", file);
        return 1;
    }
    char line[MAX_LINE];
    if (!fgets(line, MAX_LINE, fp)) {
        printf("ERROR: Failed to read %s\n", file);
        fclose(fp);
        return 1;
    }
    line[strcspn(line, "\n")] = '\0';
    if (quantum_mode) {
        // Try reading as quantum state from STATE_<signal>.<hash>.txt
        char state_file[MAX_LINE];
        snprintf(state_file, MAX_LINE, "tmp/STATE_%s.txt", strrchr(file, '/') + 1);
        FILE* state_fp = fopen(state_file, "r");
        if (state_fp) {
            double real, imag;
            for (int i = 0; i < 2; i++) { // 1-qubit state
                if (fscanf(state_fp, "%lf %lf", &real, &imag) != 2) {
                    printf("ERROR: Invalid quantum state in %s\n", state_file);
                    fclose(state_fp);
                    fclose(fp);
                    return 1;
                }
                state[i * 2] = real;
                state[i * 2 + 1] = imag;
            }
            fclose(state_fp);
            fclose(fp);
            return 0;
        }
        // Fall back to classical bit if no state file
    }
    // Read classical bit (0 or 1)
    int bit = atoi(line);
    if (bit != 0 && bit != 1) {
        printf("ERROR: Invalid bit value in %s, expected 0 or 1\n", file);
        fclose(fp);
        return 1;
    }
    state[0] = (bit == 0) ? 1.0 : 0.0;
    state[1] = 0.0;
    state[2] = (bit == 1) ? 1.0 : 0.0;
    state[3] = 0.0;
    fclose(fp);
    return 0;
}

// Write quantum state to file (classical or quantum mode)
void write_state(const char* file, double* state, int size, int quantum_mode) {
    if (quantum_mode) {
        // Write full quantum state to tmp/STATE_<signal>.<hash>.txt
        char state_file[MAX_LINE];
        snprintf(state_file, MAX_LINE, "tmp/STATE_%s", strrchr(file, '/') + 1);
        FILE* state_fp = fopen(state_file, "w");
        if (!state_fp) {
            printf("ERROR: Failed to open %s\n", state_file);
            return;
        }
        for (int i = 0; i < size; i++) {
            fprintf(state_fp, "%f %f\n", state[i * 2], state[i * 2 + 1]);
        }
        fclose(state_fp);
        printf("DEBUG: Wrote quantum state to %s\n", state_file);
        // Write dummy 0 to classical file for main.c compatibility
        FILE* fp = fopen(file, "w");
        if (!fp) {
            printf("ERROR: Failed to open %s\n", file);
            return;
        }
        fprintf(fp, "0\n");
        fclose(fp);
        printf("DEBUG: Wrote dummy 0 to %s\n", file);
    } else {
        // Classical mode: measure and write 0 or 1
        FILE* fp = fopen(file, "w");
        if (!fp) {
            printf("ERROR: Failed to open %s\n", file);
            return;
        }
        double prob_0 = state[0] * state[0] + state[1] * state[1];
        double prob_1 = state[2] * state[2] + state[3] * state[3];
        int bit = (prob_1 > prob_0) ? 1 : 0;
        fprintf(fp, "%d\n", bit);
        fclose(fp);
        printf("DEBUG: Wrote state to %s\n", file);
    }
}

// Tensor product for two 1-qubit states
void tensor_product_2(double* state1, double* state2, double* result) {
    result[0] = state1[0] * state2[0];
    result[1] = state1[0] * state2[1];
    result[2] = state1[0] * state2[2];
    result[3] = state1[0] * state2[3];
    result[4] = state1[2] * state2[0];
    result[5] = state1[2] * state2[1];
    result[6] = state1[2] * state2[2];
    result[7] = state1[2] * state2[3];
}

// Tensor product for three 1-qubit states
void tensor_product_3(double* state1, double* state2, double* state3, double* result) {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                int idx = (i * 4 + j * 2 + k) * 2;
                result[idx] = state1[i * 2] * state2[j * 2] * state3[k * 2];
                result[idx + 1] = state1[i * 2] * state2[j * 2] * state3[k * 2 + 1];
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        printf("Usage: %s \"<command>\" <hash> <tmp_index> [--quantum]\n", argv[0]);
        return 1;
    }

    const char* command = argv[1];
    const char* hash = argv[2];
    int tmp_index = atoi(argv[3]);
    int quantum_mode = (argc == 5 && strcmp(argv[4], "--quantum") == 0) || is_quantum_mode();

    char gate[MAX_LINE];
    char input1[MAX_LINE], input2[MAX_LINE], input3[MAX_LINE];
    char output1[MAX_LINE], output2[MAX_LINE], output3[MAX_LINE];
    int parsed = 0;

    char format_toffoli[MAX_LINE];
    char format_cnot[MAX_LINE];
    char format_hadamard[MAX_LINE];
    char format_t[MAX_LINE];
    snprintf(format_toffoli, MAX_LINE, "%s %%s %%s %%s %%s %%s %%s %%s", argv[0]);
    snprintf(format_cnot, MAX_LINE, "%s %%s %%s %%s %%s %%s", argv[0]);
    snprintf(format_hadamard, MAX_LINE, "%s %%s %%s %%s", argv[0]);
    snprintf(format_t, MAX_LINE, "%s %%s %%s %%s", argv[0]);

    if (sscanf(command, format_toffoli, gate, input1, input2, input3, output1, output2, output3) == 7) {
        parsed = 3;
    } else if (sscanf(command, format_cnot, gate, input1, input2, output1, output2) == 5) {
        parsed = 2;
    } else if (sscanf(command, format_hadamard, gate, input1, output1) == 3) {
        parsed = 1;
    } else if (sscanf(command, format_t, gate, input1, output1) == 3) {
        parsed = 1;
    } else {
        printf("ERROR: Invalid command format: %s\n", command);
        return 1;
    }

    char input_file1[MAX_LINE], input_file2[MAX_LINE], input_file3[MAX_LINE];
    char output_file1[MAX_LINE], output_file2[MAX_LINE], output_file3[MAX_LINE];
    snprintf(input_file1, MAX_LINE, "tmp/%s.%s.txt", input1, hash);
    snprintf(output_file1, MAX_LINE, "tmp/%s.%s.txt", output1, hash);
    if (parsed >= 2) {
        snprintf(input_file2, MAX_LINE, "tmp/%s.%s.txt", input2, hash);
        snprintf(output_file2, MAX_LINE, "tmp/%s.%s.txt", output2, hash);
    }
    if (parsed == 3) {
        snprintf(input_file3, MAX_LINE, "tmp/%s.%s.txt", input3, hash);
        snprintf(output_file3, MAX_LINE, "tmp/%s.%s.txt", output3, hash);
    }

    double input_state[MAX_STATE * 2];
    double output_state[MAX_STATE * 2];
    int state_size = parsed == 3 ? 8 : (parsed == 2 ? 4 : 2);

    if (parsed == 3) {
        double state1[4], state2[4], state3[4];
        if (read_input_state(input_file1, state1, quantum_mode) ||
            read_input_state(input_file2, state2, quantum_mode) ||
            read_input_state(input_file3, state3, quantum_mode)) {
            printf("ERROR: Failed to read input states\n");
            return 1;
        }
        tensor_product_3(state1, state2, state3, input_state);
    } else if (parsed == 2) {
        double state1[4], state2[4];
        if (read_input_state(input_file1, state1, quantum_mode) ||
            read_input_state(input_file2, state2, quantum_mode)) {
            printf("ERROR: Failed to read input states\n");
            return 1;
        }
        tensor_product_2(state1, state2, input_state);
    } else {
        double state1[4];
        if (read_input_state(input_file1, state1, quantum_mode)) {
            printf("ERROR: Failed to read input state\n");
            return 1;
        }
        input_state[0] = state1[0];
        input_state[1] = state1[1];
        input_state[2] = state1[2];
        input_state[3] = state1[3];
    }

    if (strcmp(gate, "toffoli") == 0 && parsed == 3) {
        apply_toffoli(input_state, output_state);
    } else if (strcmp(gate, "cnot") == 0 && parsed == 2) {
        apply_cnot(input_state, output_state);
    } else if (strcmp(gate, "hadamard") == 0 && parsed == 1) {
        apply_hadamard(input_state, output_state);
    } else if (strcmp(gate, "t") == 0 && parsed == 1) {
        apply_t_gate(input_state, output_state);
    } else if (strcmp(gate, "t_dagger") == 0 && parsed == 1) {
        apply_t_dagger(input_state, output_state);
    } else {
        printf("ERROR: Unknown gate or incorrect inputs: %s\n", gate);
        return 1;
    }

    if (parsed == 3) {
        double output_a[4] = {0, 0, 0, 0};
        double output_b[4] = {0, 0, 0, 0};
        double output_c[4] = {0, 0, 0, 0};
        for (int a = 0; a < 2; a++) {
            for (int b = 0; b < 2; b++) {
                for (int c = 0; c < 2; c++) {
                    int idx = (a * 4 + b * 2 + c) * 2;
                    output_a[a * 2] += output_state[idx];
                    output_a[a * 2 + 1] += output_state[idx + 1];
                    output_b[b * 2] += output_state[idx];
                    output_b[b * 2 + 1] += output_state[idx + 1];
                    output_c[c * 2] += output_state[idx];
                    output_c[c * 2 + 1] += output_state[idx + 1];
                }
            }
        }
        write_state(output_file1, output_a, 2, quantum_mode);
        write_state(output_file2, output_b, 2, quantum_mode);
        write_state(output_file3, output_c, 2, quantum_mode);
    } else if (parsed == 2) {
        double output_a[4] = {0, 0, 0, 0};
        double output_b[4] = {0, 0, 0, 0};
        for (int a = 0; a < 2; a++) {
            for (int b = 0; b < 2; b++) {
                int idx = (a * 2 + b) * 2;
                output_a[a * 2] += output_state[idx];
                output_a[a * 2 + 1] += output_state[idx + 1];
                output_b[b * 2] += output_state[idx];
                output_b[b * 2 + 1] += output_state[idx + 1];
            }
        }
        write_state(output_file1, output_a, 2, quantum_mode);
        write_state(output_file2, output_b, 2, quantum_mode);
    } else {
        write_state(output_file1, output_state, 2, quantum_mode);
    }

    FILE* io_out = fopen("tmp/io_manager_out.txt", "w");
    if (!io_out) {
        printf("ERROR: Failed to open tmp/io_manager_out.txt\n");
        return 1;
    }
    fprintf(io_out, "%s\n", output_file1);
    if (parsed >= 2) fprintf(io_out, "%s\n", output_file2);
    if (parsed == 3) fprintf(io_out, "%s\n", output_file3);
    fclose(io_out);

    return 0;
}
