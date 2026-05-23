#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <state_txt_path> <key> <value>\n", argv[0]);
        return 1;
    }

    const char *state_path = argv[1];
    const char *key = argv[2];
    const char *value = argv[3];
    
    FILE *f = fopen(state_path, "r");
    char **lines = NULL;
    int line_count = 0;
    int found = 0;
    char line_buf[MAX_LINE];
    
    if (f) {
        while (fgets(line_buf, sizeof(line_buf), f) && line_count < 99) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            char *eq = strchr(line_buf, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line_buf);
                if (strcmp(k, key) == 0) {
                    asprintf(&lines[line_count], "%s=%s\n", key, value);
                    found = 1;
                } else {
                    *eq = '=';
                    asprintf(&lines[line_count], "%s", line_buf);
                }
            } else {
                asprintf(&lines[line_count], "%s", line_buf);
            }
            line_count++;
        }
        fclose(f);
    }
    
    if (!found && line_count < 100) {
        lines = realloc(lines, sizeof(char*) * (line_count + 1));
        asprintf(&lines[line_count++], "%s=%s\n", key, value);
    }
    
    f = fopen(state_path, "w");
    if (f) {
        for (int i = 0; i < line_count; i++) {
            fputs(lines[i], f);
            free(lines[i]);
        }
        fclose(f);
    }
    free(lines);
    
    printf("Op: Synced %s=%s in %s\n", key, value, state_path);
    return 0;
}
