#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX_LINE 256

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s /+x/kvp_menu.+x\n", argv[0]);
        return 1;
    }

    int parent_to_child[2];
    int child_to_parent[2];
    if (pipe(parent_to_child) == -1 || pipe(child_to_parent) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child: redirect pipes and execute kvp_menu.+x
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);
        close(parent_to_child[0]);
        close(child_to_parent[1]);
        execl(argv[1], "kvp_menu.+x", (char *)NULL);
        perror("Exec failed");
        exit(1);
    }

    // Parent: close unused pipe ends
    close(parent_to_child[0]);
    close(child_to_parent[1]);

    char input[MAX_LINE];
    char output[MAX_LINE];
    FILE *child_out = fdopen(child_to_parent[0], "r");
    FILE *child_in = fdopen(parent_to_child[1], "w");

    while (1) {
        // Read menu or messages from child
        while (fgets(output, MAX_LINE, child_out)) {
            printf("%s", output);
            if (strstr(output, "Enter choice") || strstr(output, "Invalid") || strstr(output, "Exiting")) {
                break;
            }
        }

        // Check for exit message
        if (strstr(output, "Exiting")) {
            break;
        }

        // Get input from keyboard
        if (!fgets(input, MAX_LINE, stdin)) {
            fprintf(child_in, "-1\n");
            fflush(child_in);
            break;
        }

        // Send input to child
        fprintf(child_in, "%s", input);
        fflush(child_in);
    }

    fclose(child_in);
    fclose(child_out);
    close(parent_to_child[1]);
    close(child_to_parent[0]);
    wait(NULL);
    return 0;
}
