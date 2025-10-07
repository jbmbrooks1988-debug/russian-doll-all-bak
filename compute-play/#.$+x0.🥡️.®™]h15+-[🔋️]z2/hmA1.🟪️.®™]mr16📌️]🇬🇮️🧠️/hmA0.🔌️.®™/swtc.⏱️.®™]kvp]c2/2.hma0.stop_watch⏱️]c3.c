#include <stdio.h>
#include <string.h>
#include <time.h>

int main() {
    time_t current_time;
    FILE *file;
    char time_str[20]; // Buffer for formatted time
    char watch_path[256] = "../watch_test_trash.txt";
    char line[512];
    char *key, *value;

    // Read kvp.txt to get watch.txt path
    file = fopen("../🔐️.txt", "r");
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
        printf("Warning: Could not open ../kvp.txt, using default watch.txt\n");
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
    printf("%s\n", time_str);
    fprintf(file, "%s\n", time_str);
    fclose(file);
    
    return 0;
}
