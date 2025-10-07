#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LINE 256
#define MAX_TMP_FILES 50
#define MAX_STATE 256

// Load input signals from tmp/input.txt into separate tmp/<signal>.<hash>.txt files
int load_input(const char* input_file, const char* hash, char* tmp_files[], int* tmp_count, unsigned char* state, int* state_count) {
    FILE* input_fp = fopen(input_file, "r");
    if (!input_fp) {
        printf("ERROR: Failed to open %s\n", input_file);
        return 1;
    }

    char line[128];
    int input_idx = 0;
    char* signal_names[] = {"INPUT_A", "INPUT_B", "INPUT_C", "ANCILLA_1", "ANCILLA_2", "CLOCK", NULL}; // Added CLOCK
    while (fgets(line, 128, input_fp) && *tmp_count < MAX_TMP_FILES && input_idx < MAX_STATE) {
        line[strcspn(line, "\n")] = '\0';
        if (!signal_names[input_idx]) {
            printf("ERROR: Too many input lines for available signal names\n");
            fclose(input_fp);
            return 1;
        }
        char tmp_file_path[128];
        snprintf(tmp_file_path, 128, "tmp/%s.%s.txt", signal_names[input_idx], hash);
        FILE* tmp_fp = fopen(tmp_file_path, "w");
        if (!tmp_fp) {
            printf("ERROR: Failed to create %s\n", tmp_file_path);
            fclose(input_fp);
            return 1;
        }
        // Check if line is classical (0 or 1) or quantum (space-separated amplitudes)
        if (strlen(line) == 1 && (line[0] == '0' || line[0] == '1')) {
            fprintf(tmp_fp, "%s\n", line);
            state[*state_count] = line[0] - '0';
            (*state_count)++;
        } else {
            // Quantum state: write as-is (e.g., "1 0" for |0⟩)
            fprintf(tmp_fp, "%s\n", line);
        }
        fclose(tmp_fp);

        tmp_files[*tmp_count] = strdup(tmp_file_path);
        (*tmp_count)++;
        printf("DEBUG: Created %s with value %s, tmp_index=%d\n", tmp_file_path, line, *tmp_count-1);
        input_idx++;
    }
    fclose(input_fp);
    if (input_idx == 0) {
        printf("ERROR: No valid inputs loaded from %s\n", input_file);
        return 1;
    }
    return 0;
}

// Process a netlist file, including INCLUDE directives
int process_netlist(FILE* netlist_fp, const char* hash, char* tmp_files[], int* tmp_count, unsigned char* state, int* state_count, FILE* visual_mem_fp, char* last_output_file, int* step, int expected_lines) {
    char line[MAX_LINE];
    char* include_tmp_files[MAX_TMP_FILES];
    int include_tmp_count = 0;
    for (int i = 0; i < MAX_TMP_FILES; i++) include_tmp_files[i] = NULL;

    while (fgets(line, MAX_LINE, netlist_fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) {
            printf("DEBUG: Skipping empty line at step %d\n", *step);
            continue;
        }
        if (strncmp(line, "INCLUDE ", 8) == 0) {
            char include_file[MAX_LINE];
            sscanf(line + 8, "%s", include_file);
            printf("DEBUG: Processing INCLUDE %s at step %d\n", include_file, *step);
            FILE* include_fp = fopen(include_file, "r");
            if (!include_fp) {
                printf("ERROR: Failed to open included netlist %s\n", include_file);
                fprintf(visual_mem_fp, "\nERROR: Failed to include %s at step %d\n", include_file, *step);
                return 1;
            }
            if (process_netlist(include_fp, hash, include_tmp_files, &include_tmp_count, state, state_count, visual_mem_fp, last_output_file, step, expected_lines)) {
                fclose(include_fp);
                return 1;
            }
            fclose(include_fp);
            for (int i = 0; i < include_tmp_count; i++) {
                if (include_tmp_files[i] && (strstr(include_tmp_files[i], "tmp_xor") || strstr(include_tmp_files[i], "tmp_quantum"))) {
                    if (*tmp_count < MAX_TMP_FILES) {
                        tmp_files[*tmp_count] = strdup(include_tmp_files[i]);
                        (*tmp_count)++;
                        printf("DEBUG: Copied TMP_XOR or TMP_QUANTUM to tmp_files[%d]=%s\n", *tmp_count-1, tmp_files[*tmp_count-1]);
                    }
                }
                free(include_tmp_files[i]);
            }
        } else {
            if (strlen(line) > 100) {
                printf("ERROR: Netlist line too long at step %d: %s\n", *step, line);
                return 1;
            }
            char command[512];
            // Check if line uses quantum.+x (quantum gates)
            if (strstr(line, "./+x/quantum.+x")) {
                snprintf(command, 512, "./+x/quantum.+x \"%s\" %s %d", line, hash, *tmp_count);
            } else {
                snprintf(command, 512, "./+x/io_manager.+x \"%s\" %s %d", line, hash, *tmp_count);
            }
            printf("DEBUG: Executing: %s\n", command);
            int status = system(command);
            if (WEXITSTATUS(status) != 0) {
                printf("ERROR: Command failed at step %d: %s\n", *step, line);
                fprintf(visual_mem_fp, "\nERROR: Command failed at step %d\n", *step);
                return 1;
            }
            // Read updated tmp_files and last_output_file
            FILE* io_out = fopen("tmp/io_manager_out.txt", "r");
            if (io_out) {
                char file_path[MAX_LINE];
                while (fgets(file_path, MAX_LINE, io_out) && *tmp_count < MAX_TMP_FILES) {
                    file_path[strcspn(file_path, "\n")] = '\0';
                    if (strstr(file_path, "OUTPUT") || strstr(file_path, "TMP_XOR") || strstr(file_path, "OUTPUT_SUM") || strstr(file_path, "OUTPUT_COUT") || strstr(file_path, "TMP_QUANTUM")) {
                        strncpy(last_output_file, file_path, MAX_LINE - 1);
                        last_output_file[MAX_LINE - 1] = '\0';
                        printf("DEBUG: Set last_output_file to %s\n", last_output_file);
                    }
                    tmp_files[*tmp_count] = strdup(file_path);
                    (*tmp_count)++;
                }
                fclose(io_out);
            } else {
                printf("ERROR: Failed to read tmp/io_manager_out.txt\n");
                return 1;
            }
            // Update state from last_output_file (classical only)
            if (last_output_file[0] && !strstr(last_output_file, "TMP_QUANTUM")) {
                FILE* output_fp = fopen(last_output_file, "r");
                if (output_fp) {
                    char output_line[128];
                    if (fgets(output_line, 128, output_fp)) {
                        output_line[strcspn(output_line, "\n")] = '\0';
                        if (strlen(output_line) == 1 && (output_line[0] == '0' || output_line[0] == '1')) {
                            fprintf(visual_mem_fp, "\n%s:%s", last_output_file, output_line);
                            if (*tmp_count > 0 && *tmp_count <= MAX_STATE) {
                                state[*tmp_count - 1] = output_line[0] - '0';
                            }
                        }
                    }
                    fclose(output_fp);
                }
            }
            (*step)++;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <netlist_file> <output_file> <expected_lines>\n", argv[0]);
        return 1;
    }

    const char* netlist_file = argv[1];
    const char* output_file = argv[2];
    int expected_lines = atoi(argv[3]);
    char* tmp_files[MAX_TMP_FILES];
    unsigned char state[MAX_STATE];
    int tmp_count = 0;
    int state_count = 0;
    int step = 0;
    char last_output_file[MAX_LINE];
    last_output_file[0] = '\0';

    for (int i = 0; i < MAX_TMP_FILES; i++) tmp_files[i] = NULL;

    FILE* netlist_fp = fopen(netlist_file, "r");
    if (!netlist_fp) {
        printf("ERROR: Failed to open netlist file %s\n", netlist_file);
        return 1;
    }

    FILE* visual_mem_fp = fopen("visual_mem.txt", "w");
    if (!visual_mem_fp) {
        printf("ERROR: Failed to open visual_mem.txt\n");
        fclose(netlist_fp);
        return 1;
    }

    char hash[7];
    snprintf(hash, 7, "%d", rand() % 1000000);
    if (load_input("tmp/input.txt", hash, tmp_files, &tmp_count, state, &state_count)) {
        printf("ERROR: Failed to load inputs\n");
        fclose(netlist_fp);
        fclose(visual_mem_fp);
        return 1;
    }

    if (process_netlist(netlist_fp, hash, tmp_files, &tmp_count, state, &state_count, visual_mem_fp, last_output_file, &step, expected_lines)) {
        printf("ERROR: Failed to process netlist\n");
        fclose(netlist_fp);
        fclose(visual_mem_fp);
        for (int i = 0; i < tmp_count; i++) free(tmp_files[i]);
        return 1;
    }

    fclose(netlist_fp);
    fclose(visual_mem_fp);

    // Copy last_output_file to output_file
    if (last_output_file[0]) {
        FILE* final_output_fp = fopen(last_output_file, "r");
        if (final_output_fp) {
            FILE* output_fp = fopen(output_file, "w");
            if (output_fp) {
                char buffer[128];
                if (fgets(buffer, 128, final_output_fp)) {
                    fprintf(output_fp, "%s", buffer);
                    printf("DEBUG: Copied %s to %s\n", last_output_file, output_file);
                }
                fclose(output_fp);
            } else {
                printf("ERROR: Failed to open %s for writing\n", output_file);
            }
            fclose(final_output_fp);
        } else {
            printf("ERROR: Failed to open %s for reading\n", last_output_file);
        }
    } else {
        printf("ERROR: last_output_file is empty\n");
    }

    for (int i = 0; i < tmp_count; i++) free(tmp_files[i]);
    return 0;
}
