#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LINE 256
#define MAX_TMP_FILES 50
#define MAX_STATE 256
#define HASH_SIZE 100

// Arrays for signal table
char* signal_names[HASH_SIZE];
char* signal_files[HASH_SIZE];
int signal_indices[HASH_SIZE];
int signal_count = 0;

// Hash function for signal names
unsigned int hash_string(const char* str) {
    unsigned int h = 0;
    while (*str) h = h * 31 + *str++;
    return h % HASH_SIZE;
}

// Find or add signal to table
int find_or_add_signal(const char* name, const char* hash, char* tmp_files[], int* tmp_count) {
    unsigned int h = hash_string(name);
    for (int i = 0; i < signal_count; i++) {
        if (strcmp(signal_names[i], name) == 0) {
            return signal_indices[i];
        }
    }
    if (signal_count >= HASH_SIZE || *tmp_count >= MAX_TMP_FILES) {
        printf("ERROR: Signal table or tmp_files full\n");
        return -1;
    }
    signal_names[signal_count] = strdup(name);
    signal_files[signal_count] = malloc(128);
    snprintf(signal_files[signal_count], 128, "tmp/%s.%s.txt", name, hash);
    signal_indices[signal_count] = *tmp_count;
    tmp_files[*tmp_count] = strdup(signal_files[signal_count]);
    (*tmp_count)++;
    printf("DEBUG: Added signal %s -> %s, tmp_index=%d\n", name, signal_files[signal_count], signal_indices[signal_count]);
    signal_count++;
    return signal_indices[signal_count - 1];
}

// Create intermediate input file
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
        val1 = atoi(buffer);
    }
    fprintf(out, "%d\n", val1);
    if (*state_count < MAX_STATE) {
        state[*state_count] = val1;
        (*state_count)++;
    }
    if (fgets(buffer, 10, f2)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        val2 = atoi(buffer);
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

// Parse and execute netlist line
int execute_line(const char* line, const char* hash, char* tmp_files[], int* tmp_count, unsigned char* state, int* state_count, char* last_output_file) {
    char trimmed_line[MAX_LINE];
    strncpy(trimmed_line, line, MAX_LINE);
    trimmed_line[MAX_LINE - 1] = '\0';
    char* comment = strstr(trimmed_line, "#");
    if (comment) *comment = '\0';
    char* ptr = trimmed_line;
    while (*ptr == ' ') ptr++;
    if (strlen(ptr) == 0) {
        printf("DEBUG: Skipping comment-only line\n");
        return 0;
    }

    // Parse tokens
    char* token_copy = strdup(ptr);
    char* tokens[4];
    int token_count = 0;
    char* token = strtok(token_copy, " ");
    while (token && token_count < 4) {
        tokens[token_count++] = strdup(token);
        token = strtok(NULL, " ");
    }
    if (token_count != 4 || strcmp(tokens[0], "./+x/nand.+x") != 0) {
        printf("ERROR: Invalid command format: %s\n", ptr);
        for (int i = 0; i < token_count; i++) free(tokens[i]);
        free(token_copy);
        return 1;
    }

    // Get signals
    int in1_idx = find_or_add_signal(tokens[1], hash, tmp_files, tmp_count);
    int in2_idx = find_or_add_signal(tokens[2], hash, tmp_files, tmp_count);
    int out_idx = find_or_add_signal(tokens[3], hash, tmp_files, tmp_count);
    if (in1_idx < 0 || in2_idx < 0 || out_idx < 0) {
        printf("ERROR: Failed to process signals\n");
        for (int i = 0; i < token_count; i++) free(tokens[i]);
        free(token_copy);
        return 1;
    }

    // Create intermediate input file
    char intermediate_file[128];
    char in1_clean[64], in2_clean[64];
    strncpy(in1_clean, tokens[1], 63); in1_clean[63] = '\0';
    strncpy(in2_clean, tokens[2], 63); in2_clean[63] = '\0';
    snprintf(intermediate_file, 128, "tmp/%s_%s.%s.txt", in1_clean, in2_clean, hash);
    if (create_intermediate_file(tmp_files[in1_idx], tmp_files[in2_idx], intermediate_file, state, state_count)) {
        printf("ERROR: Failed to create intermediate file\n");
        for (int i = 0; i < token_count; i++) free(tokens[i]);
        free(token_copy);
        return 1;
    }

    // Execute NAND
    char command[256];
    snprintf(command, 256, "./+x/nand.+x %s %s", intermediate_file, tmp_files[out_idx]);
    printf("DEBUG: Executing: %s\n", command);
    int status = system(command);
    if (WEXITSTATUS(status) != 0) {
        printf("ERROR: NAND execution failed\n");
        for (int i = 0; i < token_count; i++) free(tokens[i]);
        free(token_copy);
        return 1;
    }

    // Update last_output_file
    strncpy(last_output_file, tmp_files[out_idx], MAX_LINE - 1);
    last_output_file[MAX_LINE - 1] = '\0';
    printf("DEBUG: Updated last_output_file to %s\n", last_output_file);

    // Write updated tmp_files to output
    FILE* io_out = fopen("tmp/io_manager_out.txt", "w");
    if (io_out) {
        for (int i = 0; i < *tmp_count; i++) {
            if (tmp_files[i]) {
                fprintf(io_out, "%s\n", tmp_files[i]);
            }
        }
        fclose(io_out);
        printf("DEBUG: Wrote %d tmp_files to tmp/io_manager_out.txt\n", *tmp_count);
    } else {
        printf("ERROR: Failed to write tmp/io_manager_out.txt\n");
    }

    // Clean up tokens
    for (int i = 0; i < token_count; i++) free(tokens[i]);
    free(token_copy);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <netlist_line> <hash> <tmp_count>\n", argv[0]);
        return 1;
    }

    const char* line = argv[1];
    const char* hash = argv[2];
    int tmp_count = atoi(argv[3]);
    char* tmp_files[MAX_TMP_FILES];
    unsigned char state[MAX_STATE];
    int state_count = 0;
    char last_output_file[MAX_LINE];
    last_output_file[0] = '\0';

    for (int i = 0; i < MAX_TMP_FILES; i++) tmp_files[i] = NULL;

    if (execute_line(line, hash, tmp_files, &tmp_count, state, &state_count, last_output_file)) {
        printf("ERROR: Failed to execute line: %s\n", line);
        for (int i = 0; i < tmp_count; i++) free(tmp_files[i]);
        return 1;
    }

    for (int i = 0; i < tmp_count; i++) free(tmp_files[i]);
    for (int i = 0; i < signal_count; i++) {
        free(signal_names[i]);
        free(signal_files[i]);
    }
    return 0;
}
