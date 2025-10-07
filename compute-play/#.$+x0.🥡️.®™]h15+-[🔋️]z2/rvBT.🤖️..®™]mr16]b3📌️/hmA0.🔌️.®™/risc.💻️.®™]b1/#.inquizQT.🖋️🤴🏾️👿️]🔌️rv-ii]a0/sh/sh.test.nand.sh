#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LINE 256
#define MAX_CYCLES 1000
#define MAX_STATE 256

// Global state
unsigned char state[MAX_STATE];
unsigned char clk_signal = 0;
int state_count = 0;

// Generate a 6-digit random hash
void generate_hash(char* hash) {
    sprintf(hash, "%06d", rand() % 1000000);
}

// Load input file into state
int load_input(const char* hash, unsigned char* state, int* state_count) {
    FILE* src = fopen("tmp/input.txt", "r");
    if (!src) {
        printf("ERROR: Failed to open tmp/input.txt\n");
        return 1;
    }
    char hashed_input[MAX_LINE];
    sprintf(hashed_input, "tmp/input.%s.txt", hash);
    FILE* dst = fopen(hashed_input, "w");
    if (!dst) {
        printf("ERROR: Failed to open %s\n", hashed_input);
        fclose(src);
        return 1;
    }
    char buffer[10];
    int lines = 0;
    *state_count = 0;
    while (fgets(buffer, 10, src) && *state_count < MAX_STATE && lines < 2) {
        buffer[strcspn(buffer, "\n")] = '\0';
        int val = (strcmp(buffer, "0") == 0 || strcmp(buffer, "1") == 0) ? atoi(buffer) : 0;
        state[*state_count] = val;
        fprintf(dst, "%d\n", val);
        (*state_count)++;
        lines++;
    }
    // Ensure at least two lines
    while (lines < 2 && *state_count < MAX_STATE) {
        state[*state_count] = 0;
        fprintf(dst, "0\n");
        (*state_count)++;
        lines++;
    }
    fclose(src);
    fclose(dst);
    printf("DEBUG: Copied tmp/input.txt to %s, loaded %d state entries\n", hashed_input, *state_count);
    return 0;
}

// Create an intermediate input file combining two files
int create_intermediate_file(const char* file1, const char* file2, const char* output, unsigned char* state, int* state_count) {
    FILE* f1 = fopen(file1, "r");
    if (!f1) {
        printf("ERROR: Failed to open %s\n", file1);
        return 1;
    }
    FILE* f2 = fopen(file2, "r");
    if (!f2) {
        printf("ERROR: Failed to open %s\n", file2);
        fclose(f1);
        return 1;
    }
    FILE* out = fopen(output, "w");
    if (!out) {
        printf("ERROR: Failed to open %s\n", output);
        fclose(f1);
        fclose(f2);
        return 1;
    }
    char buffer[10];
    int val1 = 0, val2 = 0;
    if (fgets(buffer, 10, f1)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        val1 = (strcmp(buffer, "0") == 0 || strcmp(buffer, "1") == 0) ? atoi(buffer) : 0;
    }
    fprintf(out, "%d\n", val1);
    if (*state_count < MAX_STATE) {
        state[*state_count] = val1;
        (*state_count)++;
    }
    if (fgets(buffer, 10, f2)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        val2 = (strcmp(buffer, "0") == 0 || strcmp(buffer, "1") == 0) ? atoi(buffer) : 0;
    }
    fprintf(out, "%d\n", val2);
    if (*state_count < MAX_STATE) {
        state[*state_count] = val2;
        (*state_count)++;
    }
    fclose(f1);
    fclose(f2);
    fclose(out);
    printf("DEBUG: Created %s with %d,%d from %s and %s\n", output, val1, val2, file1, file2);
    return 0;
}

// Substitute generic I/O names in a netlist line
void substitute_io_names(const char* line, char* result, const char* hash, char* tmp_files[], int* tmp_count, const char* subhash_a, const char* subhash_b, const char* subhash_2_3, int* tmp2_index, int* tmp3_index) {
    char cwd[MAX_LINE];
    getcwd(cwd, MAX_LINE);
    const char* pos = line;
    char* dest = result;
    printf("DEBUG: Entering substitute_io_names, tmp_count=%d\n", *tmp_count);
    while (*pos) {
        if (strncmp(pos, "INPUT", 5) == 0) {
            sprintf(dest, "tmp/input.%s.txt", hash);
            dest += strlen(dest);
            pos += 5;
            printf("DEBUG: Substituted INPUT with tmp/input.%s.txt\n", hash);
        } else if (strncmp(pos, "OUTPUT", 6) == 0) {
            sprintf(dest, "tmp/output.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 6;
            printf("DEBUG: Substituted OUTPUT with tmp/output.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "OUTPUT_SUM", 10) == 0) {
            sprintf(dest, "tmp/output_sum.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 10;
            printf("DEBUG: Substituted OUTPUT_SUM with tmp/output_sum.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "CLOCK", 5) == 0) {
            sprintf(dest, "tmp/clock.%s.txt", hash);
            dest += strlen(dest);
            pos += 5;
            printf("DEBUG: Substituted CLOCK with tmp/clock.%s.txt\n", hash);
        } else if (strncmp(pos, "TMP1", 4) == 0) {
            sprintf(dest, "tmp/tmp1.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP1 with tmp/tmp1.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "TMP2", 4) == 0 && strncmp(pos, "TMP2_3", 6) != 0) {
            sprintf(dest, "tmp/tmp2.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            *tmp2_index = *tmp_count;
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP2 with tmp/tmp2.%s.txt, tmp_count=%d, tmp2_index=%d\n", hash, *tmp_count, *tmp2_index);
        } else if (strncmp(pos, "TMP3", 4) == 0) {
            sprintf(dest, "tmp/tmp3.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            *tmp3_index = *tmp_count;
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP3 with tmp/tmp3.%s.txt, tmp_count=%d, tmp3_index=%d\n", hash, *tmp_count, *tmp3_index);
        } else if (strncmp(pos, "TMP4", 4) == 0) {
            sprintf(dest, "tmp/tmp4.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP4 with tmp/tmp4.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "TMP5", 4) == 0) {
            sprintf(dest, "tmp/tmp5.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP5 with tmp/tmp5.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "TMP6", 4) == 0) {
            sprintf(dest, "tmp/tmp6.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP6 with tmp/tmp6.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "TMP7", 4) == 0) {
            sprintf(dest, "tmp/tmp7.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP7 with tmp/tmp7.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "TMP8", 4) == 0) {
            sprintf(dest, "tmp/tmp8.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP8 with tmp/tmp8.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "TMP9", 4) == 0) {
            sprintf(dest, "tmp/tmp9.%s.txt", hash);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 4;
            printf("DEBUG: Substituted TMP9 with tmp/tmp9.%s.txt, tmp_count=%d\n", hash, *tmp_count);
        } else if (strncmp(pos, "TMP_A_TMP1", 10) == 0) {
            sprintf(dest, "tmp/tmp_a_tmp1.%s.%s.txt", hash, subhash_a);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 10;
            printf("DEBUG: Substituted TMP_A_TMP1 with tmp/tmp_a_tmp1.%s.%s.txt, tmp_count=%d\n", hash, subhash_a, *tmp_count);
        } else if (strncmp(pos, "TMP_B_TMP1", 10) == 0) {
            sprintf(dest, "tmp/tmp_b_tmp1.%s.%s.txt", hash, subhash_b);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 10;
            printf("DEBUG: Substituted TMP_B_TMP1 with tmp/tmp_b_tmp1.%s.%s.txt, tmp_count=%d\n", hash, subhash_b, *tmp_count);
        } else if (strncmp(pos, "TMP2_3", 6) == 0) {
            sprintf(dest, "tmp/tmp2_3.%s.%s.txt", hash, subhash_2_3);
            tmp_files[*tmp_count] = strdup(dest);
            (*tmp_count)++;
            dest += strlen(dest);
            pos += 6;
            printf("DEBUG: Substituted TMP2_3 with tmp/tmp2_3.%s.%s.txt, tmp_count=%d\n", hash, subhash_2_3, *tmp_count);
        } else if (strncmp(pos, "./+x/nand.+x", 12) == 0) {
            sprintf(dest, "%s/+x/nand.+x", cwd);
            dest += strlen(dest);
            pos += 12;
            printf("DEBUG: Substituted ./+x/nand.+x with %s/+x/nand.+x\n", cwd);
        } else {
            *dest++ = *pos++;
        }
    }
    *dest = '\0';
}

// Sync state to output files
void sync_state_to_files(const char* hash, char* tmp_files[], int tmp_count, unsigned char* state) {
    for (int i = 0; i < tmp_count; i++) {
        if (tmp_files[i]) {
            FILE* fp = fopen(tmp_files[i], "w");
            if (fp) {
                fprintf(fp, "%d\n", state[i]);
                fclose(fp);
                printf("DEBUG: Synced state[%d]=%d to %s\n", i, state[i], tmp_files[i]);
            }
        }
    }
    char clock_file[MAX_LINE];
    sprintf(clock_file, "tmp/clock.%s.txt", hash);
    FILE* clock_fp = fopen(clock_file, "w");
    if (clock_fp) {
        fprintf(clock_fp, "%d\n", clk_signal);
        fclose(clock_fp);
        printf("DEBUG: Synced clk_signal=%d to %s\n", clk_signal, clock_file);
    }
}

// Execute a single netlist step
int execute_step(const char* line, const char* hash, char* tmp_files[], int* tmp_count, unsigned char* state, int step, FILE* visual_mem_fp, char* last_output_file, int* tmp2_index, int* tmp3_index) {
    char trimmed_line[MAX_LINE];
    strncpy(trimmed_line, line, MAX_LINE);
    trimmed_line[MAX_LINE - 1] = '\0';
    char* comment = strstr(trimmed_line, "#");
    if (comment) *comment = '\0';
    char* ptr = trimmed_line;
    while (*ptr == ' ') ptr++;
    if (strlen(ptr) == 0) {
        printf("DEBUG: Skipping comment-only line at step %d\n", step);
        return 0;
    }

    printf("DEBUG: Step %d, raw command: '%s'\n", step, ptr);

    char intermediate_file[MAX_LINE] = "";
    char subhash_a[7] = "000000";
    char subhash_b[7] = "000000";
    char subhash_2_3[7] = "000000";
    char* input_file1 = NULL;
    char* input_file2 = NULL;
    if (strstr(ptr, "TMP_A_TMP1") && *tmp_count >= 1) {
        generate_hash(subhash_a);
        snprintf(intermediate_file, MAX_LINE, "tmp/tmp_a_tmp1.%s.%s.txt", hash, subhash_a);
        input_file1 = "tmp/input.txt";
        input_file2 = tmp_files[0];
        if (!input_file2) {
            printf("ERROR: tmp_files[0] is NULL for TMP_A_TMP1\n");
            return 1;
        }
        if (create_intermediate_file(input_file1, input_file2, intermediate_file, state, tmp_count)) {
            printf("ERROR: Failed to create %s\n", intermediate_file);
            return 1;
        }
        tmp_files[*tmp_count] = strdup(intermediate_file);
        (*tmp_count)++;
    } else if (strstr(ptr, "TMP_B_TMP1") && *tmp_count >= 1) {
        generate_hash(subhash_b);
        snprintf(intermediate_file, MAX_LINE, "tmp/tmp_b_tmp1.%s.%s.txt", hash, subhash_b);
        input_file1 = "tmp/input.txt";
        input_file2 = tmp_files[0];
        if (!input_file2) {
            printf("ERROR: tmp_files[0] is NULL for TMP_B_TMP1\n");
            return 1;
        }
        if (create_intermediate_file(input_file1, input_file2, intermediate_file, state, tmp_count)) {
            printf("ERROR: Failed to create %s\n", intermediate_file);
            return 1;
        }
        tmp_files[*tmp_count] = strdup(intermediate_file);
        (*tmp_count)++;
    } else if (strstr(ptr, "TMP2_3") && *tmp2_index >= 0 && *tmp3_index >= 0) {
        generate_hash(subhash_2_3);
        snprintf(intermediate_file, MAX_LINE, "tmp/tmp2_3.%s.%s.txt", hash, subhash_2_3);
        input_file1 = tmp_files[*tmp2_index];
        input_file2 = tmp_files[*tmp3_index];
        if (!input_file1 || !input_file2) {
            printf("ERROR: tmp_files[tmp2_index=%d]='%s' or tmp_files[tmp3_index=%d]='%s' is NULL for TMP2_3\n",
                   *tmp2_index, input_file1 ? input_file1 : "NULL", *tmp3_index, input_file2 ? input_file2 : "NULL");
            return 1;
        }
        if (create_intermediate_file(input_file1, input_file2, intermediate_file, state, tmp_count)) {
            printf("ERROR: Failed to create %s\n", intermediate_file);
            return 1;
        }
        tmp_files[*tmp_count] = strdup(intermediate_file);
        (*tmp_count)++;
    } else if (strstr(ptr, "TMP2_3")) {
        printf("ERROR: TMP2_3 used before TMP2 or TMP3 defined (tmp2_index=%d, tmp3_index=%d)\n", *tmp2_index, *tmp3_index);
        return 1;
    } else {
        printf("DEBUG: No intermediate file creation needed for step %d\n", step);
    }

    char command[MAX_LINE];
    substitute_io_names(ptr, command, hash, tmp_files, tmp_count, subhash_a, subhash_b, subhash_2_3, tmp2_index, tmp3_index);
    printf("DEBUG: Executing step %d: %s\n", step, command);

    // Debug: Check input file contents before executing
    char* tokens = strtok(strdup(command), " ");
    char* prog = tokens;
    char* input_file = strtok(NULL, " ");
    char* output_file = strtok(NULL, " ");
    if (input_file && output_file && strstr(input_file, ".txt")) {
        FILE* fp = fopen(input_file, "r");
        if (fp) {
            printf("DEBUG: Contents of %s before NAND:\n", input_file);
            char buf[10];
            while (fgets(buf, 10, fp)) printf("  %s", buf);
            fclose(fp);
        } else {
            printf("DEBUG: Cannot read %s\n", input_file);
        }
    }
    free(tokens);

    // Execute command and capture exit status
    printf("DEBUG: Raw system command: '%s'\n", command);
    int status = system(command);
    int exit_code = WEXITSTATUS(status);
    printf("DEBUG: system('%s') returned status=%d, exit_code=%d\n", command, status, exit_code);
    if (status != 0) {
        printf("ERROR: Failed to execute: %s (exit code %d)\n", command, exit_code);
        fprintf(visual_mem_fp, "\nERROR: Command failed at step %d\n", step);
        return 1;
    }

    tokens = strtok(strdup(command), " ");
    char* last_token = NULL;
    while (tokens) {
        last_token = tokens;
        tokens = strtok(NULL, " ");
    }
    if (last_token && strstr(last_token, ".txt")) {
        strncpy(last_output_file, last_token, MAX_LINE - 1);
        last_output_file[MAX_LINE - 1] = '\0';
        printf("DEBUG: Last output file set to %s\n", last_output_file);

        FILE* output_fp = fopen(last_output_file, "r");
        if (output_fp) {
            char output_line[10];
            if (fgets(output_line, 10, output_fp)) {
                output_line[strcspn(output_line, "\n")] = '\0';
                if (strlen(output_line) == 1 && (output_line[0] == '0' || output_line[0] == '1')) {
                    fprintf(visual_mem_fp, "\n%s:%s", last_output_file, output_line);
                    if (*tmp_count > 0 && *tmp_count <= MAX_STATE) {
                        state[*tmp_count - 1] = output_line[0] - '0';
                        printf("DEBUG: Updated state[%d]=%d\n", *tmp_count - 1, state[*tmp_count - 1]);
                    }
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
        printf("DEBUG: No valid output file found at step %d\n", step);
    }
    free(tokens);
    return 0;
}

// Process a netlist file, including INCLUDE directives
int process_netlist(FILE* netlist_fp, const char* hash, char* tmp_files[], int* tmp_count, unsigned char* state, int* state_count, FILE* visual_mem_fp, char* last_output_file, int* tmp2_index, int* tmp3_index, int* step) {
    char line[MAX_LINE];
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
            if (process_netlist(include_fp, hash, tmp_files, tmp_count, state, state_count, visual_mem_fp, last_output_file, tmp2_index, tmp3_index, step)) {
                fclose(include_fp);
                return 1;
            }
            fclose(include_fp);
        } else {
            if (execute_step(line, hash, tmp_files, tmp_count, state, *step, visual_mem_fp, last_output_file, tmp2_index, tmp3_index)) {
                return 1;
            }
            (*step)++;
        }
    }
    return 0;
}

// Read netlist and execute one cycle
int execute_cycle(FILE* netlist_fp, char* last_output_file, long cycle, unsigned char* state, int* state_count) {
    fseek(netlist_fp, 0, SEEK_SET);
    int step = 0;
    last_output_file[0] = '\0';
    FILE* visual_mem_fp = fopen("visual_mem.txt", "a");
    if (!visual_mem_fp) {
        printf("ERROR: Failed to open visual_mem.txt for appending\n");
        return 1;
    }
    fprintf(visual_mem_fp, "Cycle %ld NAND Outputs: ", cycle);

    char hash[7];
    generate_hash(hash);
    printf("DEBUG: Using hash %s for cycle %ld\n", hash, cycle);

    if (load_input(hash, state, state_count)) {
        fclose(visual_mem_fp);
        return 1;
    }

    char* tmp_files[20];
    int tmp_count = 0;
    int tmp2_index = -1;
    int tmp3_index = -1;
    for (int i = 0; i < 20; i++) tmp_files[i] = NULL;

    if (process_netlist(netlist_fp, hash, tmp_files, &tmp_count, state, state_count, visual_mem_fp, last_output_file, &tmp2_index, &tmp3_index, &step)) {
        for (int i = 0; i < tmp_count; i++) free(tmp_files[i]);
        fclose(visual_mem_fp);
        return 1;
    }

    sync_state_to_files(hash, tmp_files, tmp_count, state);
    clk_signal = 1 - clk_signal;
    fprintf(visual_mem_fp, "\n\n");
    for (int i = 0; i < tmp_count; i++) free(tmp_files[i]);
    fclose(visual_mem_fp);
    printf("DEBUG: Cycle completed, final last_output_file: %s\n", last_output_file);
    return 0;
}

// Display tape contents
void display_tape() {
    FILE* tape_fp = fopen("visual_mem.txt", "r");
    if (tape_fp) {
        char buf[MAX_LINE];
        printf("Tape contents (visual_mem.txt):\n");
        while (fgets(buf, MAX_LINE, tape_fp)) {
            printf("%s", buf);
        }
        fclose(tape_fp);
    } else {
        printf("Tape empty (visual_mem.txt not found)\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <netlist_file> <output_file> <cycles>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    const char* netlist_file = argv[1];
    const char* output_file = argv[2];
    long max_cycles = atol(argv[3]);
    if (max_cycles < -1 || max_cycles > MAX_CYCLES) {
        printf("Cycles must be -1 or between 0 and %d\n", MAX_CYCLES);
        return 1;
    }
    int infinite = (max_cycles == -1);

    FILE* netlist_fp = fopen(netlist_file, "r");
    if (!netlist_fp) {
        printf("ERROR: Failed to open netlist file %s\n", netlist_file);
        return 1;
    }

    FILE* visual_mem_fp = fopen("visual_mem.txt", "w");
    if (!visual_mem_fp) {
        printf("ERROR: Failed to open visual_mem.txt for writing\n");
        fclose(netlist_fp);
        return 1;
    }
    fclose(visual_mem_fp);

    long cycles = 0;
    int running = (max_cycles > 0 || infinite);
    char last_output_file[MAX_LINE];
    last_output_file[0] = '\0';

    if (max_cycles > 0 || infinite) {
        printf("Initial run:\n");
        while (running && (infinite || cycles < max_cycles)) {
            printf("Cycle %ld\n", cycles);
            if (execute_cycle(netlist_fp, last_output_file, cycles, state, &state_count)) {
                printf("Cycle %ld failed\n", cycles);
                fclose(netlist_fp);
                return 1;
            }
            display_tape();
            cycles++;
        }
    }

    while (1) {
        printf("\nClock cycle %ld, clk_signal = %d\n", cycles, clk_signal);
        display_tape();
        printf("Options: 's' step, 'r' run, 'q' quit: ");
        char choice;
        scanf(" %c", &choice);

        if (choice == 'q') break;
        else if (choice == 's') {
            running = 0;
            printf("Cycle %ld\n", cycles);
            if (execute_cycle(netlist_fp, last_output_file, cycles, state, &state_count)) {
                printf("Cycle %ld failed\n", cycles);
                fclose(netlist_fp);
                return 1;
            }
            cycles++;
        } else if (choice == 'r') {
            running = 1;
        }

        if (running) {
            printf("Cycle %ld\n", cycles);
            if (execute_cycle(netlist_fp, last_output_file, cycles, state, &state_count)) {
                printf("Cycle %ld failed\n", cycles);
                fclose(netlist_fp);
                return 1;
            }
            cycles++;
            printf("\033[2J\033[1;1H");
        }
    }

    fclose(netlist_fp);

    if (last_output_file[0]) {
        printf("DEBUG: Copying from %s to %s\n", last_output_file, output_file);
        FILE* src = fopen(last_output_file, "r");
        FILE* dst = fopen(output_file, "w");
        if (src && dst) {
            char line[10];
            if (fgets(line, 10, src)) {
                fprintf(dst, "%s", line);
                printf("DEBUG: Copied line: %s\n", line);
            }
            fclose(src);
            fclose(dst);
        } else {
            printf("DEBUG: Failed to open files (src=%p, dst=%p)\n", src, dst);
        }
    }

    printf("DEBUG: Contents of %s:\n", output_file);
    char command[MAX_LINE];
    snprintf(command, MAX_LINE, "cat %s", output_file);
    system(command);
    printf("Total cycles executed: %ld\n", cycles);
    return 0;
}
