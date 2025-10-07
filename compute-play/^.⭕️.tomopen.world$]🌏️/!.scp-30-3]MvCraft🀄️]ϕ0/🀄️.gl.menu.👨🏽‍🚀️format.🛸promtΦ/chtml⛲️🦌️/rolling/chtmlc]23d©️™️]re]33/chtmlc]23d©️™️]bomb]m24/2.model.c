#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

// A primitive shape that the view knows how to render.
// The game module will communicate using these.
typedef struct {
    int id;
    char type[20]; // e.g., "SQUARE", "CIRCLE"
    float x, y, z;
    float size;
    float color[4]; // RGBA
} Shape;

void init_model();

pid_t child_pid = -1;
int parent_to_child_pipe[2];
int child_to_parent_pipe[2];

void init_model(const char* module_path) {
    if (module_path == NULL) {
        printf("No module specified. Running without game module.\n");
        return;
    }

    // Create pipes for two-way communication
    if (pipe(parent_to_child_pipe) == -1 || pipe(child_to_parent_pipe) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    child_pid = fork();

    if (child_pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        // --- Child Process ---

        // Redirect stdin to read from the parent-to-child pipe
        dup2(parent_to_child_pipe[0], STDIN_FILENO);

        // Redirect stdout to write to the child-to-parent pipe
        dup2(child_to_parent_pipe[1], STDOUT_FILENO);

        // Close unused pipe ends in the child
        close(parent_to_child_pipe[0]);
        close(parent_to_child_pipe[1]);
        close(child_to_parent_pipe[0]);
        close(child_to_parent_pipe[1]);

        // Execute the game module
        char *argv[] = {(char*)module_path, NULL};
        execv(argv[0], argv);

        // execv only returns if an error occurred
        perror("execv");
        exit(EXIT_FAILURE);

    } else {
        // --- Parent Process ---
        printf("Initializing model... Spawning game module with PID: %d\n", child_pid);

        // Set the read end of the child-to-parent pipe to non-blocking
        int flags = fcntl(child_to_parent_pipe[0], F_GETFL, 0);
        fcntl(child_to_parent_pipe[0], F_SETFL, flags | O_NONBLOCK);
    }
}

#define MAX_SHAPES 100
Shape shapes[MAX_SHAPES];
int num_shapes = 0;

// Function for the view to get the current list of shapes to render.
Shape* model_get_shapes(int* count) {
    *count = num_shapes;
    return shapes;
}

// Function for the controller to send input to the game module.
void model_send_input(const char* input) {
    if (child_pid > 0) {
        write(parent_to_child_pipe[1], input, strlen(input));
        write(parent_to_child_pipe[1], "\n", 1); // Add newline to signal end of command
    }
}

// Function to be called continuously to update the model state from the game module.
// Returns 1 if the model was updated, 0 otherwise.
int update_model() {
    char buffer[1024];
    // TODO: This should be a non-blocking read.
    int bytes_read = read(child_to_parent_pipe[0], buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';

        // For now, we expect one shape description per line.
        // Example: "SQUARE,x,y,size,r,g,b,a"
        num_shapes = 0; // Reset shapes on each update
        char* line = strtok(buffer, "\n");
        while (line != NULL && num_shapes < MAX_SHAPES) {
            sscanf(line, "%19[^,],%f,%f,%f,%f,%f,%f,%f",
                   shapes[num_shapes].type,
                   &shapes[num_shapes].x,
                   &shapes[num_shapes].y,
                   &shapes[num_shapes].size,
                   &shapes[num_shapes].color[0],
                   &shapes[num_shapes].color[1],
                   &shapes[num_shapes].color[2],
                   &shapes[num_shapes].color[3]);
            shapes[num_shapes].id = num_shapes; // Assign a simple ID
            num_shapes++;
            line = strtok(NULL, "\n");
        }
        return 1; // Model was updated
    }
    return 0; // No update
}
