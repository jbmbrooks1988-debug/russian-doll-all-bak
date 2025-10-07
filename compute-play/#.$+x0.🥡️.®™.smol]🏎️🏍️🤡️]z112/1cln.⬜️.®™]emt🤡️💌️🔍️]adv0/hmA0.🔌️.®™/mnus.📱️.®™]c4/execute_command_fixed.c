#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>

// This is the fixed execute_command function
int execute_command_fixed(const char *command, int run_in_background) {
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

        char command_copy[MAX_VALUE]; // Assuming MAX_VALUE is defined in calling file
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

    // Parent process
    if (run_in_background) return 1;

    // Signal handling for foreground processes
    volatile sig_atomic_t interrupted = 0; // This should be defined in the calling file
    struct sigaction sa;
    sa.sa_handler = SIG_DFL; // Temporarily set to default to avoid conflicts
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set signal handler");
        return 0;
    }

    int status;
    waitpid(pid, &status, 0);

    // Restore original signal handler (assuming it's handled in calling file)
    // sa.sa_handler = signal_handler; // This should be set by the calling file
    // if (sigaction(SIGINT, &sa, NULL) == -1) {
    //     perror("Failed to restore signal handler");
    // }

    if (interrupted) { // This variable should be managed by the calling file
        // printf("\nChild process interrupted, returning to menu.\n");
        // fflush(stdout);
        return 1;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
