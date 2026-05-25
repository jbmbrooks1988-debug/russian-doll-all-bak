#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    char *content = read_file_to_string(file_path);

    if (content == NULL) {
        fprintf(stderr, "Error: Could not read file '%s'.\n", file_path);
        return 1;
    }

    printf("%s", content);
    free(content);

    return 0;
}
