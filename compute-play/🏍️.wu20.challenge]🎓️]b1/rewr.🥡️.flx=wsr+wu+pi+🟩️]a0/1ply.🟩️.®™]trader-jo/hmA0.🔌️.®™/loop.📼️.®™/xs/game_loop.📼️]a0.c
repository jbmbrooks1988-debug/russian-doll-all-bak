#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define WATCH_FILE "../wtch.⌚️.®™/watch.txt"
#define LOCATIONS_FILE "locations.txt"
#define LOG_MESSAGE "test1\n"
#define MAX_PATH 256
#define MAX_LINE 512

void log_message() {
    printf("%s", LOG_MESSAGE);
    fflush(stdout);
}

void read_and_execute_locations() {
    FILE *file = fopen(LOCATIONS_FILE, "r");
    if (!file) {
        perror("Failed to open locations.txt");
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            char command[MAX_LINE];
            snprintf(command, MAX_LINE, "%s", line);
            int result = system(command);
            if (result == -1) {
                perror("Failed to execute command");
            }
        }
    }

    fclose(file);
}

long get_file_mod_time(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_mtime;
    }
    return -1;
}

int main() {
    long last_mod_time = get_file_mod_time(WATCH_FILE);
    if (last_mod_time == -1) {
        fprintf(stderr, "Cannot access %s\n", WATCH_FILE);
        return 1;
    }

    while (1) {
        long current_mod_time = get_file_mod_time(WATCH_FILE);
        if (current_mod_time == -1) {
            fprintf(stderr, "Error checking %s\n", WATCH_FILE);
            sleep(1);
            continue;
        }

        if (current_mod_time > last_mod_time) {
            log_message();
            read_and_execute_locations();
            last_mod_time = current_mod_time;
        }

        sleep(1);
    }

    return 0;
}
