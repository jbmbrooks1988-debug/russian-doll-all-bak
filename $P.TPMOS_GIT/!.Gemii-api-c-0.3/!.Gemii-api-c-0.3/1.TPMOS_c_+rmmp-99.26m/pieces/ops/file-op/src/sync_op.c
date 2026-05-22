#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_LINE 1024

char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <state_file> <key> <value>\n", argv[0]);
        return 1;
    }
    const char *path = argv[1];
    const char *key = argv[2];
    const char *val = argv[3];

    FILE *f = fopen(path, "r");
    char **lines = NULL;
    int line_count = 0, found = 0;
    char line_buf[MAX_LINE];
    if (f) {
        while (fgets(line_buf, sizeof(line_buf), f) && line_count < 100) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            char *eq = strchr(line_buf, '=');
            if (eq) {
                char temp[MAX_LINE];
                strncpy(temp, line_buf, sizeof(temp));
                temp[eq - line_buf] = '\0';
                if (strcmp(trim_str(temp), key) == 0) {
                    asprintf(&lines[line_count], "%s=%s\n", key, val);
                    found = 1;
                } else {
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
        asprintf(&lines[line_count++], "%s=%s\n", key, val);
    }
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < line_count; i++) {
            fputs(lines[i], f);
            free(lines[i]);
        }
        fclose(f);
    }
    free(lines);
    return 0;
}
