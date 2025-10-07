#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>

#define KVP_FILE "../../state.txt"
#define MAX_PATH 256
#define MAX_LINE 512

int main(int argc, char *argv[]) {
    char exe_path[PATH_MAX];
    realpath(argv[0], exe_path);
    char *exe_dir = dirname(exe_path);
    if (chdir(exe_dir) != 0) {
        perror("chdir failed");
        return 1;
    }
    time_t current_time;
    FILE *file;
    char time_str[20]; // Buffer for formatted time
    char watch_path[256] = "../watch_test_trash.txt";
    char line[512];
    char *key, *value;

    // Read kvp.txt to get watch.txt path
    file = fopen(KVP_FILE, "r");
    if (file != NULL) {
        while (fgets(line, sizeof(line), file)) {
            // Remove trailing newline
            line[strcspn(line, "\n")] = 0;
            // Skip comments and empty lines
            if (line[0] == '#' || line[0] == '\0') continue;
            // Split line into key and value
            key = strtok(line, " ");
            value = strtok(NULL, "\n");
            if (key && value && strcmp(key, "watch") == 0) {
                strncpy(watch_path, value, sizeof(watch_path) - 1);
                watch_path[sizeof(watch_path) - 1] = '\0'; // Ensure null-termination
                break;
            }
        }
        fclose(file);
    } else {
        printf("Warning: Could not open %s, using default watch.txt\n", KVP_FILE);
    }

    // Get current time
    current_time = time(NULL);
    
    // Format time as YYYY-MM-DDTHH:MM:SS
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", localtime(&current_time));
    
    // Open file in append mode
    file = fopen(watch_path, "a");
    if (file == NULL) {
        printf("Error opening %s\n", watch_path);
        return 1;
    }
    
    // Write formatted time to file
  //  printf("%s\n", time_str);
    fprintf(file, "%s\n", time_str);
    fclose(file);
    
    return 0;
}
