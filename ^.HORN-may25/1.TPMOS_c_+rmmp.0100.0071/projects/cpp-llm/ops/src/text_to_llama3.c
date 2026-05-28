// tools/text_to_llama3.c - Converts CSV History to Llama3 Prompt (JSON Enforcement)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_unescaped(const char* s) {
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\\' && s[i+1] == 'n') { putchar('\n'); i++; }
        else if (s[i] == '\\' && s[i+1] == 'r') { putchar('\r'); i++; }
        else if (s[i] == '\\' && s[i+1] == 'c') { putchar(','); i++; }
        else putchar(s[i]);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: text_to_llama3 <history_file>\n"); return 1; }
    
    FILE* f = fopen(argv[1], "r");
    if (!f) return 1;
    
    char* line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';

        char* marker = line;
        char* first_comma = strchr(line, ',');
        if (!first_comma) continue;
        *first_comma = '\0';
        
        char* ts = first_comma + 1;
        char* second_comma = strchr(ts, ',');
        if (!second_comma) continue;
        *second_comma = '\0';
        
        char* content = second_comma + 1;

        if (strcmp(marker, "[SYSTEM]") == 0) {
            printf("<|start_header_id|>system<|end_header_id|>\n\n");
            print_unescaped(content);
            printf("<|eot_id|>");
        } else if (strcmp(marker, "[USER]") == 0) {
            printf("<|start_header_id|>user<|end_header_id|>\n\n");
            print_unescaped(content);
            printf("<|eot_id|>");
        } else if (strcmp(marker, "[ASSISTANT]") == 0) {
            printf("<|start_header_id|>assistant<|end_header_id|>\n\n");
            print_unescaped(content);
            printf("<|eot_id|>");
        } else if (strcmp(marker, "[TOOL]") == 0) {
            printf("<|start_header_id|>system<|end_header_id|>\n\nTOOL_RESULT: ");
            print_unescaped(content);
            printf("<|eot_id|>");
        }
    }
    printf("<|start_header_id|>system<|end_header_id|>\n\nRespond ONLY with JSON.<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n");
    
    free(line);
    fclose(f);
    return 0;
}
