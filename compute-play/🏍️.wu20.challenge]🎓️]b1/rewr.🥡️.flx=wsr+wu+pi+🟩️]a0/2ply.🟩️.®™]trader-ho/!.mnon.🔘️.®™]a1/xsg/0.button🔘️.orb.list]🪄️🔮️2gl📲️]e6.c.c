#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_THREADS 10
#define MAX_LINE_LENGTH 1024

struct termios original_term; // Store original terminal settings

void save_term() {
    tcgetattr(0, &original_term); // Save terminal settings
}

void restore_term() {
    tcsetattr(0, TCSANOW, &original_term); // Restore terminal settings
}

// Thread function that executes the system command
void* thread_func(void* data) {
    char* command = (char*)data;
    save_term(); // Save terminal settings before running command
    system(command);
    restore_term(); // Restore terminal settings after command
    free(command);  // Free the duplicated string
    pthread_exit(NULL);
}

int main() {
    // Open and truncate the output file
    FILE* fp = fopen("gl_cli_out.txt", "w");
    if (fp) {
        fclose(fp);
    } else {
        perror("Failed to open gl_cli_out.txt");
        exit(1);
    }

    // Open locations.txt
    FILE* loc_file = fopen("locations.txt", "r");
    if (!loc_file) {
        perror("Failed to open locations.txt");
        exit(1);
    }

    // Array to store thread IDs and command strings
    pthread_t threads[MAX_THREADS];
    char* thread_commands[MAX_THREADS];
    int num_threads = 0;

    // Read commands from locations.txt
    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, loc_file) && num_threads < MAX_THREADS) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;

        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }

        // Create command string with redirection using tee -a
        const char* redirect = " 2>&1 | tee -a gl_cli_out.txt";
        size_t len = strlen(line) + strlen(redirect) + 1;
        thread_commands[num_threads] = malloc(len);
        if (!thread_commands[num_threads]) {
            perror("Failed to allocate memory for command string");
            fclose(loc_file);
            exit(1);
        }
        snprintf(thread_commands[num_threads], len, "%s%s", line, redirect);

        // Create thread
        if (pthread_create(&threads[num_threads], NULL, thread_func, thread_commands[num_threads])) {
            perror("Failed to create thread");
            free(thread_commands[num_threads]);
            fclose(loc_file);
            exit(1);
        }
        num_threads++;
    }

    fclose(loc_file);

    if (num_threads == 0) {
        printf("No valid commands found in locations.txt\n");
        return 1;
    }

    if (num_threads == MAX_THREADS && fgets(line, MAX_LINE_LENGTH, loc_file)) {
        printf("Warning: Limited to %d threads. Additional commands ignored.\n", MAX_THREADS);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
