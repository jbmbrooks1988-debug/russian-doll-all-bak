#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

int run_list_dir(FILE* out, int argc, char* const argv[]) {
    const char* path = (argc > 1) ? argv[1] : ".";
    DIR* d = opendir(path);
    if (!d) { fprintf(stderr, "opendir error\n"); return 1; }
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char* full_path;
        asprintf(&full_path, "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) fprintf(out, "%s/\n", entry->d_name);
            else fprintf(out, "%s\n", entry->d_name);
        } else {
            fprintf(out, "%s\n", entry->d_name);
        }
        free(full_path);
    }
    closedir(d);
    return 0;
}

static int get_sandbox_depth() {
    int depth = 1;
    FILE* f = fopen("config/context.txt", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "sandbox_depth=", 14) == 0) {
                depth = atoi(line + 14);
                break;
            }
        }
        fclose(f);
    }
    return depth;
}

static int is_safe(const char* path) {
    int max_depth = get_sandbox_depth();
    int count = 0;
    const char* p = path;
    while ((p = strstr(p, "../"))) {
        count++;
        p += 3;
    }
    return count <= max_depth;
}

int run_file_ops(FILE* out, int argc, char* const argv[]) {
    if (argc < 3) { fprintf(stderr, "Usage: file_ops [read|write] <file> [content]\n"); return 1; }
    const char* action = argv[1];
    const char* file = argv[2];
    if (!is_safe(file)) {
        fprintf(stderr, "Error: Path '%s' exceeds sandbox depth.\n", file);
        return 1;
    }
    if (strcmp(action, "read") == 0) {
        FILE* f = fopen(file, "r");
        if (!f) { fprintf(stderr, "Error: Cannot open %s\n", file); return 1; }
        char buf[8192]; size_t n = fread(buf, 1, sizeof(buf)-1, f); buf[n] = '\0';
        fclose(f);
        fprintf(out, "%s", buf);
        return 0;
    }
    if (strcmp(action, "write") == 0 && argc >= 4) {
        FILE* f = fopen(file, "w");
        if (!f) { fprintf(stderr, "fopen error\n"); return 1; }
        fputs(argv[3], f);
        fclose(f);
        fprintf(out, "Written %lu bytes to %s\n", strlen(argv[3]), file);
        return 0;
    }
    fprintf(stderr, "Invalid action or missing arguments\n");
    return 1;
}
