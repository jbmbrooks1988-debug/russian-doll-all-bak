#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH 4096

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <project_root> <target_path>\n", argv[0]);
        return 1;
    }

    const char *project_root = argv[1];
    const char *target_path = argv[2];
    
    char doc_path[MAX_PATH];
    snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);

    printf("Op: Saving %s -> %s\n", doc_path, target_path);

    FILE *src = fopen(doc_path, "r");
    if (!src) {
        perror("fopen src");
        return 1;
    }

    FILE *dst = fopen(target_path, "w");
    if (!dst) {
        fclose(src);
        perror("fopen dst");
        return 1;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);

    return 0;
}
