#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 256
#define MAX_ENTRIES 100
#define MAX_KEY 100
#define MAX_VALUE 256

volatile sig_atomic_t interrupted = 0;

void signal_handler(int sig) {
    interrupted = 1;
}

void trim_newline(char *str) {
    char *pos;
    if ((pos = strchr(str, '\n')) != NULL) {
        *pos = '\0';
    }
}

int read_kvp_file(const char *filename, char keys[][MAX_KEY], char values[][MAX_VALUE], int *count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return 0;
    }

    char line[MAX_LINE];
    *count = 0;
    while (*count < MAX_ENTRIES && fgets(line, MAX_LINE, file)) {
        trim_newline(line);
        char *key = strtok(line, " ");
        char *value = strtok(NULL, "");
        if (key && value) {
            strncpy(keys[*count], key, MAX_KEY - 1);
            keys[*count][MAX_KEY - 1] = '\0';
            strncpy(values[*count], value, MAX_VALUE - 1);
            values[*count][MAX_VALUE - 1] = '\0';
            (*count)++;
        }
    }

    fclose(file);
    return 1;
}

void display_menu(char keys[][MAX_KEY], int count) {
    printf("\nMenu:\n");
    for (int i = 0; i < count; i++) {
        printf("%d: %s\n", i, keys[i]);
    }
    printf("Enter choice (0-%d) or -1 to exit: ", count - 1);
}

int execute_command(const char *command) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        return 0;
    } else if (pid == 0) {
        // Child process
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        perror("Exec failed");
        exit(1);
    } else {
        // Parent process
        int status;
        interrupted = 0;
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("Failed to set signal handler");
            return 0;
        }

        waitpid(pid, &status, 0);

        // Restore default SIGINT handler
        sa.sa_handler = SIG_DFL;
        sigaction(SIGINT, &sa, NULL);

        if (interrupted) {
            printf("\nChild process interrupted, returning to menu.\n");
            return 1;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return 1;
        }
        return 0;
    }
}

int main() {
    char keys[MAX_ENTRIES][MAX_KEY];
    char values[MAX_ENTRIES][MAX_VALUE];
    int count = 0;

    if (!read_kvp_file("menu_kvp.txt", keys, values, &count)) {
        return 1;
    }

    while (1) {
        display_menu(keys, count);
        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // Clear input buffer
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        if (choice == -1) {
            printf("Exiting...\n");
            break;
        }

        if (choice >= 0 && choice < count) {
            printf("Executing: %s\n", values[choice]);
            if (!execute_command(values[choice])) {
                printf("Command execution failed.\n");
            }
        } else {
            printf("Invalid choice. Please select between 0 and %d or -1 to exit.\n", count - 1);
        }

        while (getchar() != '\n'); // Clear input buffer
    }

    return 0;
}
