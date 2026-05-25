#include "file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for ask_for_confirmation, will be properly included from cli_utils.h later
extern int ask_for_confirmation(const char* question, int auto_confirm);

// Helper function to read a file into a string buffer
char* read_file_to_string(const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char*)malloc(length + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    return buffer;
}

// Function to write content to a file
int write_string_to_file(const char* filename, const char* content, int auto_confirm) {
    char question[512];
    snprintf(question, sizeof(question), "Write content to file '%s'?", filename);

    if (!ask_for_confirmation(question, auto_confirm)) {
        fprintf(stderr, "Skipping file write to '%s' due to user denial.\n", filename);
        return -1; // User denied
    }

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file '%s' for writing.\n", filename);
        return -1; // Failed to open file
    }

    if (fputs(content, file) == EOF) {
        fprintf(stderr, "Error: Failed to write content to file '%s'.\n", filename);
        fclose(file);
        return -1; // Failed to write content
    }

    fclose(file);
    return 0; // Success
}
