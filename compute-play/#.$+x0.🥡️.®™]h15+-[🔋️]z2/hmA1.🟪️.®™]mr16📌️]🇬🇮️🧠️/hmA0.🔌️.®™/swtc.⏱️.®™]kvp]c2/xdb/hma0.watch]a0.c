#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main() {
    int watch_time_series = 2; // seconds between increments
    time_t current_time;
    FILE *file;
    char time_str[20]; // Buffer for formatted time

    while (1) {
        // Get current time
        current_time = time(NULL);
        
        // Format time as YYYY-MM-DDTHH:MM:SS
        strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", localtime(&current_time));
        
        // Open file in append mode
        file = fopen("watch.txt", "a");
        if (file == NULL) {
            printf("Error opening watch.txt\n");
            return 1;
        }
        
        // Write formatted time to file
        printf("%s\n", time_str);
        fprintf(file, "%s\n", time_str);
        fclose(file);
        
        // Wait for watch_time_series seconds
        sleep(watch_time_series);
    }

    return 0;
}
