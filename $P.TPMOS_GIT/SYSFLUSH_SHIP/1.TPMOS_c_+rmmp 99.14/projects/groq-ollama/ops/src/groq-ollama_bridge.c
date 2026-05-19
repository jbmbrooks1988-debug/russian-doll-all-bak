#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void strip_ansi(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '\x1b') {
            src++;
            if (*src == '[') {
                src++;
                while (*src && !((*src >= '@' && *src <= '~'))) src++;
                if (*src) src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: groq-ollama_bridge <prompt>\n");
        return 1;
    }

    char *cmd = NULL;
    // Use -y for YOLO mode and text output format
    if (asprintf(&cmd, "/home/debilu/.nvm/versions/node/v22.18.0/bin/groq-ollama \"%s\" -y --output-format text --chat-recording false", argv[1]) < 0) {
        return 1;
    }

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("popen failed");
        free(cmd);
        return 1;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strip_ansi(buffer);
        printf("%s", buffer);
    }

    pclose(fp);
    free(cmd);
    return 0;
}
