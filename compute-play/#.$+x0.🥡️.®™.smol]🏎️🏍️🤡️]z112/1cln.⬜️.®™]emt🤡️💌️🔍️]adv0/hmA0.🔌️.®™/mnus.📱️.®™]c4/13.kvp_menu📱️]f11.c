#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>

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

int read_kvp_file(const char *filename, char keys[][MAX_KEY], char values[][MAX_VALUE], char bg_flags[][2], int *count) {
    char full_path[MAX_VALUE];
    snprintf(full_path, sizeof(full_path), "../%s", filename); // Go up one directory

    FILE *file = fopen(full_path, "r");
    if (!file) {
        perror("Failed to open menu file");
        return 0;
    }

    char line[MAX_LINE];
    *count = 0;
    while (*count < MAX_ENTRIES && fgets(line, MAX_LINE, file)) {
        char *hash_pos = strchr(line, '#');
        if (hash_pos) {
            *hash_pos = '\0';
        }

        char *trimmed_line = trim_whitespace(line);

        if (strlen(trimmed_line) == 0) {
            continue;
        }

        char *space_pos = strchr(trimmed_line, ' ');
        if (!space_pos) continue;

        *space_pos = '\0';
        strncpy(keys[*count], trimmed_line, MAX_KEY - 1);
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

        size_t cmd_len = strlen(command);
        if (cmd_len < 3 || strcmp(command + cmd_len - 3, ".+x") != 0) {
            continue;
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
    fflush(stdout);
}

int execute_command(const char *command, int run_in_background) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        return 0;
    }

    if (pid == 0) { // Child process
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

        char original_cwd[PATH_MAX];
        getcwd(original_cwd, sizeof(original_cwd));

        char command_copy[MAX_VALUE];
        strncpy(command_copy, command, sizeof(command_copy) - 1);
        command_copy[sizeof(command_copy) - 1] = '\0';

        char* dir = dirname(command_copy);
        char* base = basename((char*)command); // Cast to char* for basename

        if (chdir(dir) != 0) {
            perror("chdir failed");
            exit(1);
        }

        char *args[2];
        args[0] = base;
        args[1] = NULL;

        execv(args[0], args);
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
        fflush(stdout);
        return 1;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char *argv[]) {
    char exe_path[PATH_MAX];
    realpath(argv[0], exe_path);
    char *exe_dir = dirname(exe_path);
    if (chdir(exe_dir) != 0) {
        perror("chdir failed");
        return 1;
    }

    char current_dir[PATH_MAX];
    if (getcwd(current_dir, sizeof(current_dir)) != NULL) {
        printf("Current working directory: %s\n", current_dir);
    } else {
        perror("getcwd failed");
    }

    char keys[MAX_ENTRIES][MAX_KEY];
    char values[MAX_ENTRIES][MAX_VALUE];
    char bg_flags[MAX_ENTRIES][2];
    int count = 0;

    char menu_file_path[MAX_VALUE] = "menu_kvp.txt";

    if (!read_kvp_file(menu_file_path, keys, values, bg_flags, &count)) {
        return 1;
    }


    if (!isatty(STDIN_FILENO)) {
        printf("Running in pipe mode\n");
        fflush(stdout);
    }

    while (1) {
        display_menu(keys, count);
        char input[MAX_LINE];
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            if (!isatty(STDIN_FILENO)) {
                printf("Pipe closed, exiting...\n");
                fflush(stdout);
                break;
            }
            printf("Input error, try again.\n");
            fflush(stdout);
            continue;
        }

        int choice;
        if (sscanf(input, "%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            fflush(stdout);
            continue;
        }

        if (choice == -1) {
            printf("Exiting...\n");
            fflush(stdout);
            break;
        }

        if (choice >= 0 && choice < count) {
            printf("Executing: %s\n", values[choice]);
            fflush(stdout);
            int run_in_background = strcmp(bg_flags[choice], "&") == 0;
            if (!execute_command(values[choice], run_in_background)) {
                printf("Command execution failed.\n");
                fflush(stdout);
            }
            continue;
        }

        printf("Invalid choice. Please select between 0 and %d or -1 to exit.\n", count - 1);
        fflush(stdout);
    }

    return 0;
}