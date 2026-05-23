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
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <pdl_file> <section> <key> <value>\n", argv[0]);
        return 1;
    }
    const char *path = argv[1];
    const char *target_sec = argv[2];
    const char *target_key = argv[3];
    const char *new_val = argv[4];

    FILE *f = fopen(path, "r");
    char **lines = NULL;
    int line_count = 0, found = 0;
    char line_buf[MAX_LINE];
    
    if (f) {
        while (fgets(line_buf, sizeof(line_buf), f) && line_count < 200) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            char line_copy[MAX_LINE];
            strcpy(line_copy, line_buf);
            
            char *p1 = strchr(line_copy, '|');
            char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
            
            if (p2) {
                *p2 = '\0';
                *p1 = '\0';
                char *sec = trim_str(line_copy);
                char *key = trim_str(p1 + 1);
                
                if (strcmp(sec, target_sec) == 0 && strcmp(key, target_key) == 0) {
                    asprintf(&lines[line_count], "%-12s | %-18s | %s\n", target_sec, target_key, new_val);
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

    if (!found && line_count < 200) {
        lines = realloc(lines, sizeof(char*) * (line_count + 1));
        asprintf(&lines[line_count++], "%-12s | %-18s | %s\n", target_sec, target_key, new_val);
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
