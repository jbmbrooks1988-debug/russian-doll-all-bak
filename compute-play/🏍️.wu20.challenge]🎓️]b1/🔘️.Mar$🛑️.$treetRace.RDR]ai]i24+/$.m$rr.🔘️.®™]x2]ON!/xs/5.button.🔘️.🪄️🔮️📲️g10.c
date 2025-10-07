#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

#define MAX_THREADS 10
#define MAX_LINE 256

volatile sig_atomic_t keep_running = 1; // Flag to control thread termination

// Signal handler for main thread
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0; // Signal threads to stop
    }
}

// Thread function that executes the system command
void* thread_func(void* data) {
    // Block SIGINT in this thread
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    char* command = (char*)data;
    system(command);
    free(command);  // Free the duplicated string
    pthread_exit(NULL);
}

int main() {
    // Block SIGINT in main thread initially
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Set up signal handler for SIGTERM, ignore SIGINT
    signal(SIGTERM, signal_handler);
    signal(SIGINT, SIG_IGN);

    printf("test1\n");

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

    if (chdir("../") != 0) {
        perror("chdir failed");
        return 1;
    }

    char line[MAX_LINE];
    char* commands[MAX_THREADS];
    int num_threads = 0;

    // Read commands from file
    while (fgets(line, MAX_LINE, file) && num_threads < MAX_THREADS) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0 && line[0] != '#') {
            char* command_part = strtok(line, " \t");
            char* redirect_flag = strtok(NULL, " \t");
            const char* redirect;
            if (redirect_flag && strcmp(redirect_flag, "&") == 0) {
                redirect = " > /dev/null 2>&1";
            } else {
                redirect = " 2>&1 | tee -a gl_cli_out.txt";
            }

            size_t len = (redirect_flag && strcmp(redirect_flag, "&") == 0 ? 0 : strlen("stdbuf -oL ")) 
                       + strlen(command_part) + strlen(redirect) + 1;
            commands[num_threads] = malloc(len);
            if (!commands[num_threads]) {
                perror("Failed to allocate memory for command string");
                fclose(file);
                return 1;
            }
            if (redirect_flag && strcmp(redirect_flag, "&") == 0) {
                snprintf(commands[num_threads], len, "%s%s", command_part, redirect);
            } else {
                snprintf(commands[num_threads], len, "stdbuf -oL %s%s", command_part, redirect);
            }
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
        if (pthread_create(&threads[i], NULL, thread_func, commands[i])) {
            perror("Failed to create thread");
            free(commands[i]);
            return 1;
        }
    }

    // Unblock SIGINT in main thread
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    // Wait for threads to complete or for SIGINT/SIGTERM
    while (keep_running) {
        int all_done = 1;
        for (int i = 0; i < num_threads; i++) {
            void* status;
            if (pthread_tryjoin_np(threads[i], &status) == 0) {
                continue;
            }
            all_done = 0;
        }
        if (all_done) break;
        usleep(100000);
    }

    // Clean up: join any remaining threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Free command memory
    for (int i = 0; i < num_threads; i++) {
        free(commands[i]);
    }

    return 0;
}
