#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH 4096

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <load|save> <source_path> <target_path>\n", argv[0]);
        return 1;
    }
    const char *cmd = argv[1];
    const char *src_path = argv[2];
    const char *dst_path = argv[3];

    FILE *src = fopen(src_path, "r");
    if (!src) { perror("fopen src"); return 1; }
    FILE *dst = fopen(dst_path, "w");
    if (!dst) { fclose(src); perror("fopen dst"); return 1; }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    fclose(src);
    fclose(dst);
    return 0;
}
