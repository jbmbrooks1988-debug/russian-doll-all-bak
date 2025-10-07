#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

#define MAX_LINE 256
#define COMMAND_BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    printf("test1\n");

    FILE* file = fopen("locations.txt", "r");
    if (!file) {
        perror("Failed to open locations.txt");
        return 1;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0 && line[0] != '#') {
            char* command_path = strtok(line, " ");
            if(command_path == NULL) continue;

            printf("Executing command: %s\n", command_path);

            char* command_copy = strdup(command_path);
            char* dir = dirname(command_copy);
            char* base = basename(command_path);

            char original_cwd[COMMAND_BUFFER_SIZE];
            getcwd(original_cwd, sizeof(original_cwd));

            printf("Changing to dir: %s\n", dir);
            if (chdir(dir) != 0) {
                perror("chdir failed");
                free(command_copy);
                continue;
            }

            char system_command[COMMAND_BUFFER_SIZE];
            snprintf(system_command, sizeof(system_command), "./%s", base);
            
            char* redirect_flag = strtok(NULL, " ");
            if (redirect_flag && strcmp(redirect_flag, "&") == 0) {
                strcat(system_command, " &");
            }

            printf("Running system command: %s\n", system_command);
            system(system_command);

            chdir(original_cwd);

            free(command_copy);
        }
    }
    fclose(file);

    return 0;
}
