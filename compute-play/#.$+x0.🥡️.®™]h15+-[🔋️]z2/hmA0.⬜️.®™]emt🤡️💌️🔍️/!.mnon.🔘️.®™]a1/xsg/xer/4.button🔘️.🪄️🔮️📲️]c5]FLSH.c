#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

#define MAX_THREADS 10
#define MAX_LINE 256

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

// Signal handler to kill child processes
void signal_handler(int sig) {
    // Sending SIGTERM to process group (includes children)
    kill(0, SIGTERM);
    exit(0);
}

int main() {
    printf("test1\n");

    // Set up signal handler for SIGINT and SIGTERM
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Truncate or create the output file
    FILE* fp = fopen("gl_cli_out.txt", "w");
    if (fp) {
        fclose(fp);
    } else {
        perror("Failed to open gl_cli_out.txt");
        return 1;
    }

    // Open and read locations.txt
    FILE* file = fopen("locations.txt", "r");
    if (!file) {
        perror("Failed to open locations.txt");
        return 1;
    }

    char line[MAX_LINE];
    char* commands[MAX_THREADS];
    int num_threads = 0;

    // Read commands from file
    while (fgets(line, MAX_LINE, file) && num_threads < MAX_THREADS) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        // Skip empty lines and comments (lines starting with #)
        if (strlen(line) > 0 && line[0] != '#') {
            // Create command string with stdbuf and redirection using tee -a
            const char* redirect = " 2>&1 | tee -a gl_cli_out.txt";
            size_t len = strlen("stdbuf -oL ") + strlen(line) + strlen(redirect) + 1;
            commands[num_threads] = malloc(len);
            if (!commands[num_threads]) {
                perror("Failed to allocate memory for command string");
                fclose(file);
                return 1;
            }
            snprintf(commands[num_threads], len, "stdbuf -oL %s%s", line, redirect);
            num_threads++;
        }
    }
    fclose(file);

    if (num_threads == 0) {
        printf("No valid commands found in locations.txt\n");
        return 1;
    }

    // Array to store thread IDs
    pthread_t threads[MAX_THREADS];

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        // Create thread
        if (pthread_create(&threads[i], NULL, thread_func, commands[i])) {
            perror("Failed to create thread");
            free(commands[i]);
            return 1;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
