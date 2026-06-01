#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>  // for threading support
#include <errno.h>    // for errno

// Enhanced shape structure with more properties
typedef struct {
    int id;
    char type[32]; // Increased size for more descriptive types
    float x, y, z; // 3D coordinates
    float width, height, depth; // Separate dimensions instead of single size
    float color[4]; // RGBA
    char label[64]; // Label for identification
    int active;     // Whether this shape is currently active
} Shape;

// For storing additional variables from modules
typedef struct {
    char label[64];
    int type;  // 1=int, 2=float, 3=string
    union {
        int int_val;
        float float_val;
        char string_val[256];
    } value;
    int active;  // Whether this variable is currently active
} ModelVariable;

typedef struct {
    char label[64];
    int type;  // 1=int array, 2=float array
    int count;
    union {
        int* int_array;
        float* float_array;
    } data;
    int active;  // Whether this array is currently active
} ModelArray;

// --- Forward Declarations ---
void init_model(const char* module_path);
void model_navigate_dir(const char* subdir);
char (*model_get_dir_entries(int* count))[256];
void _model_load_dir_contents();
Shape* model_get_shapes(int* count);
void model_send_input(const char* input);
int update_model();

// New IPC parsing functions
int parse_enhanced_format(const char* line);
int parse_variable_format(const char* line);
int parse_array_format(const char* line);

// Thread-related declarations
extern pthread_t ipc_thread;  // Thread variable from main file
extern int model_updated;  // From main file
extern int ipc_thread_running;  // From main file
void* ipc_thread_func(void* arg);  // From main file

pid_t child_pid = -1;
int parent_to_child_pipe[2];
int child_to_parent_pipe[2];

// Mutex for thread-safe access to shared data
pthread_mutex_t model_mutex = PTHREAD_MUTEX_INITIALIZER;

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

        // Set the read end of the child-to-parent pipe to non-blocking for the thread
        int flags = fcntl(child_to_parent_pipe[0], F_GETFL, 0);
        fcntl(child_to_parent_pipe[0], F_SETFL, flags | O_NONBLOCK);
        
        // Start the IPC thread to handle communication with the module
        ipc_thread_running = 1;
        if (pthread_create(&ipc_thread, NULL, ipc_thread_func, NULL) != 0) {
            perror("pthread_create failed");
            ipc_thread_running = 0;
        }
    }
}

#define MAX_SHAPES 100
Shape shapes[MAX_SHAPES];
int num_shapes = 0;

// Maximum number of variables and arrays
#define MAX_VARIABLES 50
#define MAX_ARRAYS 20

ModelVariable variables[MAX_VARIABLES];
ModelArray arrays[MAX_ARRAYS];

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
                snprintf(dir_entries[dir_entry_count], sizeof(dir_entries[0]), "%.253s/", entry->d_name);
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



// Helper function to parse the old CSV format (for backward compatibility)
int parse_csv_message(const char* line) {
    if (num_shapes < MAX_SHAPES) {
        float size;  // Temp variable for the old size parameter
        // Parse old format: "TYPE,x,y,size,r,g,b,a"
        sscanf(line, "%31[^,],%f,%f,%f,%f,%f,%f,%f",
               shapes[num_shapes].type,
               &shapes[num_shapes].x,
               &shapes[num_shapes].y,
               &size,  // Read the old size parameter
               &shapes[num_shapes].color[0],
               &shapes[num_shapes].color[1],
               &shapes[num_shapes].color[2],
               &shapes[num_shapes].color[3]);
        shapes[num_shapes].z = 0.0f; // Set default Z
        shapes[num_shapes].width = size;   // Map old size to width
        shapes[num_shapes].height = size;  // Map old size to height
        shapes[num_shapes].depth = size;   // Map old size to depth
        shapes[num_shapes].id = num_shapes; // Assign a simple ID
        strcpy(shapes[num_shapes].label, ""); // Default empty label
        shapes[num_shapes].active = 1;
        num_shapes++;
        return 1;
    }
    return 0;
}

// Helper function to parse enhanced shape format: "SHAPE:type:label:x:y:z:width:height:depth:r:g:b:a"
int parse_enhanced_format(const char* line) {
    if (num_shapes >= MAX_SHAPES) return 0;
    
    char type[32], label[64];
    float x, y, z, width, height, depth, r, g, b, a;
    
    // Parse: "SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a"
    int items = sscanf(line, "SHAPE;%31[^;];%63[^;];%f;%f;%f;%f;%f;%f;%f;%f;%f;%f",
                      type, label, &x, &y, &z, &width, &height, &depth, &r, &g, &b, &a);
    
    if (items == 12) {
        strcpy(shapes[num_shapes].type, type);
        strcpy(shapes[num_shapes].label, label);
        shapes[num_shapes].x = x;
        shapes[num_shapes].y = y;
        shapes[num_shapes].z = z;
        shapes[num_shapes].width = width;
        shapes[num_shapes].height = height;
        shapes[num_shapes].depth = depth;

        shapes[num_shapes].color[0] = r;
        shapes[num_shapes].color[1] = g;
        shapes[num_shapes].color[2] = b;
        shapes[num_shapes].color[3] = a;
        shapes[num_shapes].id = num_shapes;
        shapes[num_shapes].active = 1;
        num_shapes++;
        return 1;
    }
    
    return 0;
}

// Helper function to parse variable format: "VAR;label;type;value"
int parse_variable_format(const char* line) {
    char label[64], type_str[16], value_str[256];
    if (sscanf(line, "VAR;%63[^;];%15[^;];%255[^\r\n]", label, type_str, value_str) == 3) {
        // Find an available slot or update existing variable
        for (int i = 0; i < MAX_VARIABLES; i++) {
            if (strcmp(variables[i].label, label) == 0 || !variables[i].active) {
                strcpy(variables[i].label, label);
                
                if (strcmp(type_str, "int") == 0) {
                    variables[i].type = 1;
                    variables[i].value.int_val = atoi(value_str);
                } else if (strcmp(type_str, "float") == 0) {
                    variables[i].type = 2;
                    variables[i].value.float_val = atof(value_str);
                } else if (strcmp(type_str, "string") == 0) {
                    variables[i].type = 3;
                    strcpy(variables[i].value.string_val, value_str);
                }
                
                variables[i].active = 1;
                return 1;
            }
        }
    }
    return 0;
}

// Helper function to parse array format: "ARRAY;label;type;count;val1,val2,val3..."
int parse_array_format(const char* line) {
    char label[64], type_str[16];
    int count;
    if (sscanf(line, "ARRAY;%63[^;];%15[^;];%d;", label, type_str, &count) == 3) {
        // Find the start of the values part (after the third semicolon)
        const char* values_start = strstr(line, ";");
        if (values_start) values_start = strstr(values_start + 1, ";");
        if (values_start) values_start = strstr(values_start + 1, ";");
        if (values_start) values_start = strstr(values_start + 1, ";");
        if (values_start) values_start++; // Move past the last ';'
        else return 0;
        
        // Find an available slot or update existing array
        for (int i = 0; i < MAX_ARRAYS; i++) {
            if (strcmp(arrays[i].label, label) == 0 || !arrays[i].active) {
                // Clean up any existing array data
                if (arrays[i].data.int_array) {
                    free(arrays[i].data.int_array);
                    arrays[i].data.int_array = NULL;
                }
                
                strcpy(arrays[i].label, label);
                if (strcmp(type_str, "int") == 0) {
                    arrays[i].type = 1;
                    arrays[i].data.int_array = (int*)malloc(count * sizeof(int));
                    if (arrays[i].data.int_array) {
                        // Parse the integer values
                        char *values_copy = strdup(values_start);
                        if (values_copy) {
                            char *token = strtok(values_copy, ",");
                            int parsed_count = 0;
                            while (token && parsed_count < count) {
                                arrays[i].data.int_array[parsed_count] = atoi(token);
                                token = strtok(NULL, ",");
                                parsed_count++;
                            }
                            arrays[i].count = parsed_count;
                            free(values_copy);
                        }
                    }
                } else if (strcmp(type_str, "float") == 0) {
                    arrays[i].type = 2;
                    arrays[i].data.float_array = (float*)malloc(count * sizeof(float));
                    if (arrays[i].data.float_array) {
                        // Parse the float values
                        char *values_copy = strdup(values_start);
                        if (values_copy) {
                            char *token = strtok(values_copy, ",");
                            int parsed_count = 0;
                            while (token && parsed_count < count) {
                                arrays[i].data.float_array[parsed_count] = atof(token);
                                token = strtok(NULL, ",");
                                parsed_count++;
                            }
                            arrays[i].count = parsed_count;
                            free(values_copy);
                        }
                    }
                }
                
                arrays[i].active = 1;
                return 1;
            }
        }
    }
    return 0;
}



// Get a specific variable by label
ModelVariable* model_get_variable(const char* label) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i].active && strcmp(variables[i].label, label) == 0) {
            return &variables[i];
        }
    }
    return NULL;
}

// Get a specific array by label  
ModelArray* model_get_array(const char* label) {
    for (int i = 0; i < MAX_ARRAYS; i++) {
        if (arrays[i].active && strcmp(arrays[i].label, label) == 0) {
            return &arrays[i];
        }
    }
    return NULL;
}

// Get all active variables
ModelVariable* model_get_variables(int* count) {
    *count = 0;
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i].active) {
            (*count)++;
        }
    }
    return variables;
}

// Get all active arrays
ModelArray* model_get_arrays(int* count) {
    *count = 0;
    for (int i = 0; i < MAX_ARRAYS; i++) {
        if (arrays[i].active) {
            (*count)++;
        }
    }
    return arrays;
}

// Thread function for handling IPC communication
void* ipc_thread_func(void* arg) {
    char buffer[1024];
    int bytes_read;
    
    while (ipc_thread_running) {
        // Try to read from the pipe
        bytes_read = read(child_to_parent_pipe[0], buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';

            // Lock the mutex before modifying shared data
            pthread_mutex_lock(&model_mutex);

            char* line = strtok(buffer, "\n");
            while (line != NULL) {
                // Determine the format based on the prefix
                if (strncmp(line, "CLEAR_SHAPES", 12) == 0) {
                    // Special command to clear all shapes
                    for (int i = 0; i < MAX_SHAPES; i++) {
                        shapes[i].active = 0;
                    }
                    num_shapes = 0;
                } else if (strncmp(line, "SHAPE;", 6) == 0) {
                    // Look for the shape by label to update existing shape or create new one
                    char type[32], label[64];
                    float x, y, z, width, height, depth, r, g, b, a;
                    
                    int items = sscanf(line, "SHAPE;%31[^;];%63[^;];%f;%f;%f;%f;%f;%f;%f;%f;%f;%f",
                                      type, label, &x, &y, &z, &width, &height, &depth, &r, &g, &b, &a);
                    
                    if (items == 12) {
                        // Look for an existing shape with the same label to update it
                        int found = 0;
                        for (int i = 0; i < MAX_SHAPES; i++) {
                            if (shapes[i].active && strcmp(shapes[i].label, label) == 0) {
                                // Update existing shape
                                strcpy(shapes[i].type, type);
                                shapes[i].x = x;
                                shapes[i].y = y;
                                shapes[i].z = z;
                                shapes[i].width = width;
                                shapes[i].height = height;
                                shapes[i].depth = depth;
                                shapes[i].color[0] = r;
                                shapes[i].color[1] = g;
                                shapes[i].color[2] = b;
                                shapes[i].color[3] = a;
                                found = 1;
                                break;
                            }
                        }
                        
                        // If we didn't find an existing shape with this label, create a new one
                        if (!found) {
                            // Find an empty slot to create a new shape
                            for (int i = 0; i < MAX_SHAPES; i++) {
                                if (!shapes[i].active) {
                                    // Use this slot
                                    strcpy(shapes[i].type, type);
                                    strcpy(shapes[i].label, label);
                                    shapes[i].x = x;
                                    shapes[i].y = y;
                                    shapes[i].z = z;
                                    shapes[i].width = width;
                                    shapes[i].height = height;
                                    shapes[i].depth = depth;
                                    shapes[i].color[0] = r;
                                    shapes[i].color[1] = g;
                                    shapes[i].color[2] = b;
                                    shapes[i].color[3] = a;
                                    shapes[i].id = i;
                                    shapes[i].active = 1;
                                    break;
                                }
                            }
                        }
                    }
                } else if (strncmp(line, "VAR;", 4) == 0) {
                    parse_variable_format(line);
                } else if (strncmp(line, "ARRAY;", 6) == 0) {
                    parse_array_format(line);
                } else {
                    // Treat as old format for backward compatibility
                    parse_csv_message(line);
                }
                line = strtok(NULL, "\n");
            }
            
            // Count only active shapes
            num_shapes = 0;
            for (int i = 0; i < MAX_SHAPES; i++) {
                if (shapes[i].active) {
                    num_shapes++;
                }
            }
            
            // Unlock the mutex
            pthread_mutex_unlock(&model_mutex);
            
            // Signal that model was updated for the main thread
            model_updated = 1;
        } 
        else if (bytes_read == 0) {
            // EOF - child process closed the pipe
            // This might mean the child process terminated, so we should exit the thread
            printf("Child process terminated, exiting IPC thread\n");
            break;
        }
        else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Real error occurred (not just "no data available")
            perror("read error from child process in thread");
            break;
        }
        
        // Small sleep to prevent busy waiting
        usleep(1000); // Sleep for 1 millisecond
    }
    
    return NULL;
}

// Function to be called continuously to update the model state (this is now a no-op in multi-threaded version)
// Since updates happen in the separate thread, this just returns if there was an update
int update_model() {
    int was_updated = model_updated;
    if (was_updated) {
        model_updated = 0;  // Reset the flag after checking
    }
    return was_updated;
}
