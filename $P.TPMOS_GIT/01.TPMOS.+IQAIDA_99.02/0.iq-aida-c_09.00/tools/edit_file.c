// tools/edit_file.c - Self-contained surgical file editor
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 4) { fprintf(stderr, "Usage: edit_file <path> <search> <replace>\n"); return 1; }
    const char* path = argv[1];
    const char* search = argv[2];
    const char* replace = argv[3];

    FILE* f = fopen(path, "r");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* data = malloc(size + 1);
    fread(data, 1, size, f); data[size] = '\0';
    fclose(f);

    if (strlen(search) == 0) { fprintf(stderr, "Error: Empty search string.\n"); free(data); return 1; }

    int count = 0;
    char* pos = data;
    while ((pos = strstr(pos, search)) != NULL) {
        count++;
        pos += strlen(search);
    }

    if (count == 0) {
        fprintf(stderr, "Error: Search block not found.\n");
        free(data); return 1;
    }

    size_t new_size = size + (strlen(replace) - strlen(search)) * count;
    char* new_data = malloc(new_size + 1);
    
    char* src = data;
    char* dst = new_data;
    while ((pos = strstr(src, search)) != NULL) {
        size_t len = pos - src;
        memcpy(dst, src, len);
        dst += len;
        memcpy(dst, replace, strlen(replace));
        dst += strlen(replace);
        src = pos + strlen(search);
    }
    strcpy(dst, src);

    f = fopen(path, "w");
    if (!f) { perror("fopen write"); free(data); free(new_data); return 1; }
    fwrite(new_data, 1, new_size, f);
    fclose(f);

    printf("Successfully edited %s (%d replacement%s).\n", path, count, count == 1 ? "" : "s");
    free(data); free(new_data);
    return 0;
}
