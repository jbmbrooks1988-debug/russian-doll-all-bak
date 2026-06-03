#include <stdio.h>
#include <string.h>
#include <time.h>

void log_to_file(const char *message) {
    FILE *fp = fopen("log.txt", "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s\n", timestamp, message);
    fclose(fp);
}

void log_input(const char *type, const char *details) {
    FILE *fp = fopen("commands_gl.txt", "a");
    if (!fp) {
        log_to_file("Error: Failed to write to commands_gl.txt");
        return;
    }
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s: %s\n", timestamp, type, details);
    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        log_to_file("Error: input_logger requires type and details arguments");
        return 1;
    }
    log_input(argv[1], argv[2]);
    return 0;
}
