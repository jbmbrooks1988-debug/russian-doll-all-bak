#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <source_path> <target_buffer_path> <pdl_to_update> <project_id> <entry_layout>\n", argv[0]);
        return 1;
    }

    const char *source_path = argv[1];
    const char *target_path = argv[2];
    const char *pdl_path = argv[3];
    const char *project_id = argv[4];
    const char *entry_layout = argv[5];
    
    printf("Op: Loading %s -> %s for project %s\n", source_path, target_path, project_id);

    FILE *src = fopen(source_path, "r");
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

    // Marker signal
    char marker_path[MAX_PATH];
    snprintf(marker_path, sizeof(marker_path), "%s/projects/agy-text-editor/manager/load_complete.txt", getenv("PROJECT_ROOT") ? getenv("PROJECT_ROOT") : ".");
    FILE *mf = fopen(marker_path, "a");
    if (mf) {
        fprintf(mf, "D\n");
        fclose(mf);
    }

    // Update active_file in PDL
    if (strlen(pdl_path) > 0 && strcmp(pdl_path, "none") != 0) {
        FILE *pf = fopen(pdl_path, "w");
        if (pf) {
            fprintf(pf, "SECTION      | KEY                | VALUE\n");
            fprintf(pf, "----------------------------------------\n");
            fprintf(pf, "META         | project_id         | %s\n", project_id);
            fprintf(pf, "META         | version            | 1.0\n");
            fprintf(pf, "META         | entry_layout       | %s\n\n", entry_layout);
            
            fprintf(pf, "STATE        | active_file        | %s\n", source_path);
            fprintf(pf, "STATE        | cursor_x           | 0\n");
            fprintf(pf, "STATE        | cursor_y           | 0\n");
            
            fprintf(pf, "RESPONSE     | default            | Loaded via Global Load Op.\n");
            fclose(pf);
        }
    }

    return 0;
}
