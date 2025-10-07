#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define KVP_FILE "../🔐️.txt"
#define DEFAULT_WATCH_FILE "../wtch.⌚️.®™/watch.txt"
#define DEFAULT_LOCATIONS_FILE "locations.txt"
#define LOG_MESSAGE "test1"
#define MAX_PATH 256
#define MAX_LINE 512

char watch_file[MAX_PATH];
char locations_file[MAX_PATH];

int read_kvp_file() {
    strncpy(watch_file, DEFAULT_WATCH_FILE, MAX_PATH - 1);
    watch_file[MAX_PATH - 1] = '\0';
    strncpy(locations_file, DEFAULT_LOCATIONS_FILE, MAX_PATH - 1);
    locations_file[MAX_PATH - 1] = '\0';

    FILE *file = fopen(KVP_FILE, "r");
    if (!file) {
        fprintf(stderr, "Warning: %s not found, using default paths\n", KVP_FILE);
        return 1;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0 || line[0] == '#') continue;

        char *key = strtok(line, " ");
        char *value = strtok(NULL, "");
        if (!key || !value) continue;

        if (strcmp(key, "watch") == 0) {
            strncpy(watch_file, value, MAX_PATH - 1);
            watch_file[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "loop") == 0) {
            strncpy(locations_file, value, MAX_PATH - 1);
            locations_file[MAX_PATH - 1] = '\0';
        }
    }

    fclose(file);
    return (watch_file[0] != '\0' && locations_file[0] != '\0');
}

void log_message(int counter) {
    printf("%s %d\n", LOG_MESSAGE, counter);
    fflush(stdout);
}

void read_and_execute_locations() {
    FILE *file = fopen(locations_file, "r");
    if (!file) {
        perror("Failed to open locations file");
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            struct stat st;
            if (stat(line, &st) == -1) {
                fprintf(stderr, "Alert: Executable not found: %s\n", line);
                continue;
            }
            if (!(st.st_mode & S_IXUSR)) {
                fprintf(stderr, "Alert: File is not executable: %s\n", line);
                continue;
            }

            char command[MAX_LINE];
            snprintf(command, MAX_LINE, "%s", line);
            int result = system(command);
            if (result == -1) {
                perror("Failed to execute command");
            } else if (result != 0) {
                fprintf(stderr, "Alert: Command failed with exit code %d: %s\n", result, line);
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
    if (!read_kvp_file()) {
        fprintf(stderr, "Failed to read valid paths, using defaults\n");
    }

 if (chdir("../") != 0) {
        perror("chdir failed");
        return 1;
    }

    long last_mod_time = get_file_mod_time(watch_file);
    if (last_mod_time == -1) {
        fprintf(stderr, "Cannot access %s\n", watch_file);
        return 1;
    }

    static int loop_counter = 0;

    while (1) {
        long current_mod_time = get_file_mod_time(watch_file);
        if (current_mod_time == -1) {
            fprintf(stderr, "Error checking %s\n", watch_file);
            sleep(1);
            continue;
        }

        if (current_mod_time > last_mod_time) {
            loop_counter++;
            log_message(loop_counter);
            read_and_execute_locations();
            last_mod_time = current_mod_time;
        }

        sleep(1);
    }

    return 0;
}
