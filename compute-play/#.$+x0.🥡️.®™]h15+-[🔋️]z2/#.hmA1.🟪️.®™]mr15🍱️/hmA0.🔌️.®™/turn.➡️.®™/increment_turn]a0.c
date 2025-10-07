#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256
#define MAX_LINES 100

int main() {
    FILE *file = fopen("../state.txt", "r");
    if (!file) {
        file = fopen("../state.txt", "w");
        if (!file) {
            perror("Cannot open state.txt");
            return 1;
        }
        fprintf(file, "turn 0\n");
        fclose(file);
        return 0;
    }

    char lines[MAX_LINES][MAX_LINE];
    int line_count = 0;
    int turn_found = 0;
    int turn_value = 0;

    while (line_count < MAX_LINES && fgets(lines[line_count], MAX_LINE, file)) {
        if (strncmp(lines[line_count], "turn ", 5) == 0) {
            turn_found = 1;
            turn_value = atoi(lines[line_count] + 5);
        }
        line_count++;
    }
    fclose(file);

    if (!turn_found && line_count < MAX_LINES) {
        strcpy(lines[line_count], "turn 0\n");
        line_count++;
    }

    file = fopen("../state.txt", "w");
    if (!file) {
        perror("Cannot open state.txt for writing");
        return 1;
    }

    for (int i = 0; i < line_count; i++) {
        if (strncmp(lines[i], "turn ", 5) == 0) {
            fprintf(file, "turn %d\n", turn_value + 1);
        } else {
            fputs(lines[i], file);
        }
    }

    fclose(file);
    return 0;
}
