#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 2) return 0;
    
    char *matches = argv[1];
    char *copy = strdup(matches);
    char *token = strtok(copy, "  ");
    int count = 0;
    while (token && count < 5) {
        printf("<button label=\"%s\" onClick=\"KEY:%d\" /><br/>", token, count + 2);
        token = strtok(NULL, "  ");
        count++;
    }
    free(copy);
    return 0;
}
