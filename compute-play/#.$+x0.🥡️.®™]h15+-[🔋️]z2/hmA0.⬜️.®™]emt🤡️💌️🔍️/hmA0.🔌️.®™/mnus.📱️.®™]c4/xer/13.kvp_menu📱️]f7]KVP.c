#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_LINE 256
#define MAX_ENTRIES 100
#define MAX_KEY 100
#define MAX_VALUE 256

volatile sig_atomic_t interrupted = 0;

void signal_handler(int sig) {
    interrupted = 1;
}

char *trim_whitespace(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t') str++;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) end--;
    *(end + 1) = '\0';
    return str;
}

// New function to read the menu file path from ../kvp.txt
int get_menu_file_path(char *menu_file_path, size_t max_len) {
    FILE *file = fopen("../kvp.txt", "r");
    if (!file) {
        perror("Failed to open ../kvp.txt, using fallback");
        return 0;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file)) {
        char *space_pos = strchr(line, ' ');
        if (!space_pos) continue;

        *space_pos = '\0';
        char *key = trim_whitespace(line);
        char *value = trim_whitespace(space_pos + 1);

        // Check for the 📱️ key (UTF-8 encoded emoji)
        if (strcmp(key, "📱️") == 0) {
            strncpy(menu_file_path, value, max_len - 1);
            menu_file_path[max_len - 1] = '\0';
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0; // Key not found
}

int read_kvp_file(const char *filename, char keys[][MAX_KEY], char values[][MAX_VALUE], char bg_flags[][2], int *count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open menu file");
        return 0;
    }

    char line[MAX_LINE];
    *count = 0;
    while (*count < MAX_ENTRIES && fgets(line, MAX_LINE, file)) {
        char *space_pos = strchr(line, ' ');
        if (!space_pos) continue;

        *space_pos = '\0';
        strncpy(keys[*count], line, MAX_KEY - 1);
        keys[*count][MAX_KEY - 1] = '\0';
        char *command = space_pos + 1;
        command = trim_whitespace(command);
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '&') {
            strncpy(bg_flags[*count], "&", 2);
            command[len - 1] = '\0';
            command = trim_whitespace(command);
        } else {
            strncpy(bg_flags[*count], "", 2);
        }
        strncpy(values[*count], command, MAX_VALUE - 1);
        values[*count][MAX_VALUE - 1] = '\0';
        (*count)++;
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

int execute_command(const char *command, int run_in_background) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        return 0;
    }

    if (pid == 0) {
        if (run_in_background) {
            int dev_null = open("/dev/null", O_RDWR);
            if (dev_null == -1) {
                perror("Failed to open /dev/null");
                exit(1);
            }
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        perror("Exec failed");
        exit(1);
    }

    if (run_in_background) return 1;

    interrupted = 0;
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set signal handler");
        return 0;
    }

    int status;
    waitpid(pid, &status, 0);

    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to restore signal handler");
    }

    if (interrupted) {
        printf("\nChild process interrupted, returning to menu.\n");
        return 1;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main() {
    char keys[MAX_ENTRIES][MAX_KEY];
    char values[MAX_ENTRIES][MAX_VALUE];
    char bg_flags[MAX_ENTRIES][2];
    int count = 0;

    // Buffer for the menu file path
    char menu_file_path[MAX_VALUE] = "menu_kvp.txt"; // Fallback
    if (!get_menu_file_path(menu_file_path, MAX_VALUE)) {
        printf("Using fallback menu file: %s\n", menu_file_path);
    }

    if (!read_kvp_file(menu_file_path, keys, values, bg_flags, &count)) {
        return 1;
    }

    while (1) {
        display_menu(keys, count);
        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        if (choice == -1) {
            printf("Exiting...\n");
            break;
        }

        if (choice >= 0 && choice < count) {
            printf("Executing: %s\n", values[choice]);
            int run_in_background = strcmp(bg_flags[choice], "&") == 0;
            if (!execute_command(values[choice], run_in_background)) {
                printf("Command execution failed.\n");
            }
            while (getchar() != '\n');
            continue;
        }

        printf("Invalid choice. Please select between 0 and %d or -1 to exit.\n", count - 1);
        while (getchar() != '\n');
    }

    return 0;
}
