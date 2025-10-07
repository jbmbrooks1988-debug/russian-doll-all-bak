#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

// A primitive shape that the view knows how to render.
// The game module will communicate using these.
typedef struct {
    int id;
    char type[20]; // e.g., "SQUARE", "CIRCLE"
    float x, y, z;
    float size;
    float color[4]; // RGBA
} Shape;

// --- Forward Declarations ---
void init_model(const char* module_path);
void model_navigate_dir(const char* subdir);
char (*model_get_dir_entries(int* count))[256];
void _model_load_dir_contents();
Shape* model_get_shapes(int* count);
void model_send_input(const char* input);
int update_model();


pid_t child_pid = -1;
int parent_to_child_pipe[2];
int child_to_parent_pipe[2];

void init_model(const char* module_path) {
    _model_load_dir_contents(); // Load initial directory

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

// State for the Directory Lister widget
char dir_path[1024] = ".";
char dir_entries[200][256];
int dir_entry_count = 0;



// === Directory Listing Logic ===

// Private function to load directory contents into the model's state
void _model_load_dir_contents() {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[2048];

    dir_entry_count = 0; // Clear previous entries

    dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Error opening directory in model");
        strcpy(dir_path, "."); // Fallback to current directory
        dir = opendir(dir_path);
        if (dir == NULL) return; // Give up if even fallback fails
    }

    // Add ".." entry if not in the root
    if (strcmp(dir_path, ".") != 0 && strcmp(dir_path, "/") != 0) {
        strncpy(dir_entries[dir_entry_count], "../", sizeof(dir_entries[0]) - 1);
        dir_entries[dir_entry_count][sizeof(dir_entries[0]) - 1] = '\0'; // Ensure null termination
        dir_entry_count++;
    }

    while ((entry = readdir(dir)) != NULL && dir_entry_count < 200) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (stat(full_path, &file_stat) == 0) {
            if (S_ISDIR(file_stat.st_mode)) {
                snprintf(dir_entries[dir_entry_count], sizeof(dir_entries[0]) - 1, "%s/", entry->d_name);
                dir_entries[dir_entry_count][sizeof(dir_entries[0]) - 1] = '\0'; // Ensure null termination
                dir_entries[dir_entry_count][sizeof(dir_entries[0]) - 1] = '\0'; // Ensure null termination
            } else {
                strncpy(dir_entries[dir_entry_count], entry->d_name, sizeof(dir_entries[0]) - 1);
                dir_entries[dir_entry_count][sizeof(dir_entries[0]) - 1] = '\0'; // Ensure null termination
            }
            dir_entry_count++;
        }
    }
    closedir(dir);
}

// Navigate to a subdirectory or parent directory
void model_navigate_dir(const char* subdir) {
    if (strcmp(subdir, "../") == 0 || strcmp(subdir, "..") == 0) {
        // Navigate up
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash != NULL && last_slash != dir_path) { // Found a slash and it's not the root
            *last_slash = '\0'; // Truncate path
        } else {
            strcpy(dir_path, "."); // Go to current dir if no slash found or if it's the root slash
        }
    } else {
        // Navigate down
        char clean_subdir[256];
        strncpy(clean_subdir, subdir, sizeof(clean_subdir) - 1);
        clean_subdir[sizeof(clean_subdir) - 1] = '\0'; // Ensure null termination
        clean_subdir[strcspn(clean_subdir, "/")] = 0; // Remove trailing slash if it exists

        if (strcmp(dir_path, ".") == 0) {
             strncpy(dir_path, clean_subdir, sizeof(dir_path) - 1);
             dir_path[sizeof(dir_path) - 1] = '\0'; // Ensure null termination
        } else {
            strncat(dir_path, "/", sizeof(dir_path) - strlen(dir_path) - 1);
            strncat(dir_path, clean_subdir, sizeof(dir_path) - strlen(dir_path) - 1);
            dir_path[sizeof(dir_path) - 1] = '\0'; // Ensure null termination
        }
    }
    _model_load_dir_contents(); // Reload contents after path change
}

// Getter for the view to retrieve directory entries
char (*model_get_dir_entries(int* count))[256] {
    *count = dir_entry_count;
    return dir_entries;
}


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
