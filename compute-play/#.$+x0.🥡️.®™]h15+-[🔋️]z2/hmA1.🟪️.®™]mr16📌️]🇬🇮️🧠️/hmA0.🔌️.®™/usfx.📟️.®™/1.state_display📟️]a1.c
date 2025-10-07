#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256

char *trim_whitespace(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t') str++;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) end--;
    *(end + 1) = '\0';
    return str;
}

int main() {
    FILE *file = fopen("../state.txt", "r");
    if (!file) {
        printf("Failed to open ../state.txt, trying ../../state.txt\n");
        file = fopen("../../state.txt", "r");
        if (!file) {
            perror("Failed to open ../../state.txt");
            return 1;
        }
    }

    char line[MAX_LINE];
    printf("\nContents of state.txt:\n");
    while (fgets(line, MAX_LINE, file)) {
        printf("%s", trim_whitespace(line));
        printf("\n"); // Ensure each line ends with a newline
    }

    fclose(file);
    return 0;
}
