#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_THREADS 10

// Thread function that executes the system command
void* thread_func(void* data) {
    char* command = (char*)data;
    system(command);
    free(command);  // Free the duplicated string
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    // Check if we have enough arguments
    if (argc < 2) {
        printf("Usage: %s <command1> [command2] [command3] ...\n", argv[0]);
        printf("Up to %d commands can be specified\n", MAX_THREADS);
        return 1;
    }

    // Calculate number of threads (limited to MAX_THREADS)
    int num_threads = argc - 1;  // Subtract 1 for program name
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
        printf("Warning: Limited to %d threads. Additional arguments ignored.\n", MAX_THREADS);
    }

    // Array to store thread IDs and command strings
    pthread_t threads[MAX_THREADS];
    char* thread_commands[MAX_THREADS];

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        // Duplicate the command string to avoid issues with argv memory
        thread_commands[i] = strdup(argv[i + 1]);
        if (!thread_commands[i]) {
            perror("Failed to duplicate command string");
            exit(1);
        }

        // Create thread
        if (pthread_create(&threads[i], NULL, thread_func, thread_commands[i])) {
            perror("Failed to create thread");
            free(thread_commands[i]);
            exit(1);
        }
    }

    // Wait for all threads to complete (optional)
    // Uncomment if you want main to wait for threads
    /*
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    */

    pthread_exit(NULL);
    return 0;
}
