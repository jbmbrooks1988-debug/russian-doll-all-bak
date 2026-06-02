#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>  // For waitpid when handling processes
#include <sys/shm.h>   // For shared memory
#include <sys/sem.h>   // For semaphores
#include <sys/ipc.h>   // For IPC constants
#include <errno.h>     // For errno
#include <time.h>      // For time functions
#include <GL/gl.h>     // For OpenGL types like GLuint

#define MAX_MODULES 10

// Flex sizing structures
typedef enum {
    FLEX_UNIT_PIXEL,
    FLEX_UNIT_PERCENT
} FlexUnit;

typedef struct {
    float value;
    FlexUnit unit;
} FlexSize;

// Enhanced shape structure with more properties
typedef struct {
    int id;
    char type[32]; // Increased size for more descriptive types
    float x, y, z; // 3D coordinates
    float width, height, depth; // Separate dimensions instead of single size
    float color[3]; // RGB
    float alpha; // Alpha transparency
    char label[64]; // Label for identification
    int active;     // Whether this shape is currently active
    float rotation_x, rotation_y, rotation_z; // Rotation angles in degrees
    float scale_x, scale_y, scale_z; // Scale factors
    // Texture fields for module shapes
    char texture_src[256];    // path from module SHAPE command
    GLuint texture_id;        // OpenGL texture ID
    int texture_loaded;       // flag to indicate if texture has been loaded
    int texture_loading;      // flag to prevent multiple loading attempts
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

// Function declaration for texture source extraction
int extract_texture_source_from_line(const char* line, char* texture_path, size_t path_size);

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

// Shared memory structures and constants
#define SHMEM_MODULE_TYPE_PIPE 0
#define SHMEM_MODULE_TYPE_SHARED 1

// Shared memory block for module communication
typedef struct {
    int module_id;
    int active;                    // Whether this module is active
    int command_ready;             // Flag: 1 if command is ready for module
    int response_ready;            // Flag: 1 if response is ready for main
    char command_buffer[4096];     // Command from main to module
    char response_buffer[4096];    // Response from module to main
    int last_command_timestamp;    // For detecting stale commands
    int last_response_timestamp;   // For detecting stale responses
    int sem_id;                    // Semaphore ID for synchronization
    // New fields for CHTML content via shared memory
    int chtml_ready;               // Flag: 1 if CHTML content is ready for main
    int chtml_content_size;        // Size of CHTML content
    char chtml_content[4096];      // CHTML content from module to main
} ModuleSharedMem;

// Structure to store module information (extending the existing ModuleProcess structure)
typedef struct {
    int module_type;               // 0 = pipe, 1 = shared memory
    pid_t pid;
    int parent_to_child_pipe[2];   // For pipe-based modules
    int child_to_parent_pipe[2];   // For pipe-based modules
    int shmem_id;                  // For shared memory modules
    ModuleSharedMem* shmem_ptr;    // Shared memory pointer
    int active;
} ModuleInfo;

// --- Forward Declarations ---
void init_model(const char* module_path);
void model_navigate_dir(const char* subdir);
char (*model_get_dir_entries(int* count))[256];
void _model_load_dir_contents();
Shape* model_get_shapes(int* count);
void model_send_input(const char* input);
void model_send_input_to_module(const char* input, int module_idx);
int update_model();
void init_multiple_modules_extended(const char** module_paths, const int* protocols, int count);
extern int add_chtml_to_dom(const char* chtml_content);
extern int view_needs_refresh();
extern int load_chtml_file_to_window(const char* filename, int width, int height);

// Shared memory functions
int init_shared_memory_module(const char* module_path, int module_idx);
int send_to_shared_module(const char* input, int module_idx);
int read_from_shared_module(char* buffer, int buffer_size, int module_idx);
int cleanup_shared_module(int module_idx);
int generate_shm_key(int module_idx);
int is_shared_memory_module(const char* module_path);

// CHTML content functions via shared memory
int send_chtml_to_main(int module_idx, const char* chtml_content);
int receive_chtml_from_module(char* buffer, int buffer_size, int module_idx);
int is_chtml_ready(int module_idx);

// New IPC parsing functions
int parse_enhanced_format(const char* line);
int parse_variable_format(const char* line);
int parse_array_format(const char* line);

pid_t child_pid = -1;
int parent_to_child_pipe[2];
int child_to_parent_pipe[2];

// For multiple modules - using the new ModuleInfo structure
ModuleInfo modules[MAX_MODULES];
int num_modules = 0;

void init_model(const char* module_path) {
    _model_load_dir_contents(); // Load initial directory

    // Initialize modules array
    for (int i = 0; i < MAX_MODULES; i++) {
        modules[i].module_type = 0;  // Default to pipe-based for backward compatibility
        modules[i].pid = -1;
        modules[i].active = 0;
        modules[i].shmem_id = -1;
        modules[i].shmem_ptr = NULL;
    }
    num_modules = 0;

    if (module_path == NULL) {
        printf("No module specified. Running without game module.\n");
        return;
    }

    // Check if the module is a special shared memory module by looking for a prefix
    // For now, defaulting to pipe-based for backward compatibility
    if (module_path != NULL && strlen(module_path) > 0) {
        // For backward compatibility, initialize as pipe-based module
        modules[0].module_type = 0;  // pipe-based
        modules[0].active = 1;
        
        // Create pipes for two-way communication
        if (pipe(modules[0].parent_to_child_pipe) == -1 || pipe(modules[0].child_to_parent_pipe) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        modules[0].pid = fork();

        if (modules[0].pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (modules[0].pid == 0) {
            // --- Child Process ---

            // Redirect stdin to read from the parent-to-child pipe
            dup2(modules[0].parent_to_child_pipe[0], STDIN_FILENO);

            // Redirect stdout to write to the child-to-parent pipe
            dup2(modules[0].child_to_parent_pipe[1], STDOUT_FILENO);

            // Close unused pipe ends in the child
            close(modules[0].parent_to_child_pipe[0]);
            close(modules[0].parent_to_child_pipe[1]);
            close(modules[0].child_to_parent_pipe[0]);
            close(modules[0].child_to_parent_pipe[1]);

            // Execute the game module
            char *argv[] = {(char*)module_path, NULL};
            execv(argv[0], argv);

            // execv only returns if an error occurred
            perror("execv");
            exit(EXIT_FAILURE);

        } else {
            // --- Parent Process ---
            printf("Initializing model... Spawning game module with PID: %d\n", modules[0].pid);

            // Set the read end of the child-to-parent pipe to non-blocking
            int flags = fcntl(modules[0].child_to_parent_pipe[0], F_GETFL, 0);
            fcntl(modules[0].child_to_parent_pipe[0], F_SETFL, flags | O_NONBLOCK);
            
            num_modules = 1;
        }
    }
}

// Helper function to determine if a module should use shared memory
int is_shared_memory_module(const char* module_path) {
    // For now, we can use a naming convention or file extension to determine
    // In the future, this could be based on CHTML attributes
    if (strstr(module_path, ".shmem") != NULL) {
        return 1;  // Use shared memory for .shmem modules
    }
    return 0;  // Default to pipe-based
}

// Initialize multiple modules from path array with protocol information
void init_multiple_modules_extended(const char** module_paths, const int* protocols, int count) {
    _model_load_dir_contents(); // Load initial directory

    // Initialize modules array
    for (int i = 0; i < MAX_MODULES; i++) {
        modules[i].module_type = 0;  // Default to pipe-based for backward compatibility
        modules[i].pid = -1;
        modules[i].active = 0;
        modules[i].shmem_id = -1;
        modules[i].shmem_ptr = NULL;
    }
    num_modules = 0;

    if (module_paths == NULL || count == 0) {
        printf("No modules specified. Running without game modules.\n");
        return;
    }

    // Initialize each module
    for (int i = 0; i < count && i < MAX_MODULES; i++) {
        if (module_paths[i] == NULL || strlen(module_paths[i]) == 0) continue;
        
        // Use the protocol information passed in
        int use_shared_memory = (protocols != NULL) ? protocols[i] : 0;
        
        if (use_shared_memory) {
            // Initialize as shared memory module
            modules[i].module_type = 1;  // shared memory
            modules[i].active = 1;
            
            if (init_shared_memory_module(module_paths[i], i) == 0) {
                printf("Successfully initialized shared memory module %d: %s\n", i, module_paths[i]);
                
                // Fork and execute the shared memory module
                // The module will connect to the existing shared memory segments
                modules[i].pid = fork();
                
                if (modules[i].pid == -1) {
                    perror("fork for shared memory module");
                    modules[i].active = 0;  // Mark as inactive on failure
                    continue; // Skip this module and try the next one
                }
                
                if (modules[i].pid == 0) {
                    // Child process: execute the module with module index as argument
                    char module_idx_str[16];
                    snprintf(module_idx_str, sizeof(module_idx_str), "%d", i);
                    char *argv[] = {(char*)module_paths[i], module_idx_str, NULL};
                    execv(argv[0], argv);
                    
                    // execv only returns if an error occurred
                    perror("execv for shared memory module");
                    exit(EXIT_FAILURE);
                } else {
                    // Parent process: continue
                    printf("Spawning shared memory module %d with PID: %d, path: %s\n", 
                           i, modules[i].pid, module_paths[i]);
                    num_modules++;
                }
            } else {
                printf("Failed to initialize shared memory module %d: %s\n", i, module_paths[i]);
                modules[i].active = 0;  // Mark as inactive on failure
            }
        } else {
            // Initialize as pipe-based module (backward compatibility)
            modules[i].module_type = 0;  // pipe-based
            modules[i].active = 1;
            
            // Create pipes for two-way communication
            if (pipe(modules[i].parent_to_child_pipe) == -1 || pipe(modules[i].child_to_parent_pipe) == -1) {
                perror("pipe");
                modules[i].active = 0;  // Mark as inactive on failure
                continue; // Skip this module and try the next one
            }

            modules[i].pid = fork();

            if (modules[i].pid == -1) {
                perror("fork");
                modules[i].active = 0;  // Mark as inactive on failure
                continue; // Skip this module and try the next one
            }

            if (modules[i].pid == 0) {
                // --- Child Process ---

                // Redirect stdin to read from the parent-to-child pipe
                dup2(modules[i].parent_to_child_pipe[0], STDIN_FILENO);

                // Redirect stdout to write to the child-to-parent pipe
                dup2(modules[i].child_to_parent_pipe[1], STDOUT_FILENO);

                // Close unused pipe ends in the child
                close(modules[i].parent_to_child_pipe[0]);
                close(modules[i].parent_to_child_pipe[1]);
                close(modules[i].child_to_parent_pipe[0]);
                close(modules[i].child_to_parent_pipe[1]);

                // Execute the game module
                char *argv[] = {(char*)module_paths[i], NULL};
                execv(argv[0], argv);

                // execv only returns if an error occurred
                perror("execv");
                exit(EXIT_FAILURE);

            } else {
                // --- Parent Process ---
                printf("Initializing model... Spawning game module %d with PID: %d, path: %s\n", 
                       i, modules[i].pid, module_paths[i]);

                // Set the read end of the child-to-parent pipe to non-blocking
                int flags = fcntl(modules[i].child_to_parent_pipe[0], F_GETFL, 0);
                fcntl(modules[i].child_to_parent_pipe[0], F_SETFL, flags | O_NONBLOCK);
                
                num_modules++;
            }
        }
    }
}

// Initialize multiple modules from path array
void init_multiple_modules(const char** module_paths, int count) {
    _model_load_dir_contents(); // Load initial directory

    // Initialize modules array
    for (int i = 0; i < MAX_MODULES; i++) {
        modules[i].module_type = 0;  // Default to pipe-based for backward compatibility
        modules[i].pid = -1;
        modules[i].active = 0;
        modules[i].shmem_id = -1;
        modules[i].shmem_ptr = NULL;
    }
    num_modules = 0;

    if (module_paths == NULL || count == 0) {
        printf("No modules specified. Running without game modules.\n");
        return;
    }

    // Initialize each module
    for (int i = 0; i < count && i < MAX_MODULES; i++) {
        if (module_paths[i] == NULL || strlen(module_paths[i]) == 0) continue;
        
        if (is_shared_memory_module(module_paths[i])) {
            // Initialize as shared memory module
            modules[i].module_type = 1;  // shared memory
            modules[i].active = 1;
            
            if (init_shared_memory_module(module_paths[i], i) == 0) {
                printf("Successfully initialized shared memory module %d: %s\n", i, module_paths[i]);
                num_modules++;
            } else {
                printf("Failed to initialize shared memory module %d: %s\n", i, module_paths[i]);
                modules[i].active = 0;  // Mark as inactive on failure
            }
        } else {
            // Initialize as pipe-based module (backward compatibility)
            modules[i].module_type = 0;  // pipe-based
            modules[i].active = 1;
            
            // Create pipes for two-way communication
            if (pipe(modules[i].parent_to_child_pipe) == -1 || pipe(modules[i].child_to_parent_pipe) == -1) {
                perror("pipe");
                modules[i].active = 0;  // Mark as inactive on failure
                continue; // Skip this module and try the next one
            }

            modules[i].pid = fork();

            if (modules[i].pid == -1) {
                perror("fork");
                modules[i].active = 0;  // Mark as inactive on failure
                continue; // Skip this module and try the next one
            }

            if (modules[i].pid == 0) {
                // --- Child Process ---

                // Redirect stdin to read from the parent-to-child pipe
                dup2(modules[i].parent_to_child_pipe[0], STDIN_FILENO);

                // Redirect stdout to write to the child-to-parent pipe
                dup2(modules[i].child_to_parent_pipe[1], STDOUT_FILENO);

                // Close unused pipe ends in the child
                close(modules[i].parent_to_child_pipe[0]);
                close(modules[i].parent_to_child_pipe[1]);
                close(modules[i].child_to_parent_pipe[0]);
                close(modules[i].child_to_parent_pipe[1]);

                // Execute the game module
                char *argv[] = {(char*)module_paths[i], NULL};
                execv(argv[0], argv);

                // execv only returns if an error occurred
                perror("execv");
                exit(EXIT_FAILURE);

            } else {
                // --- Parent Process ---
                printf("Initializing model... Spawning game module %d with PID: %d, path: %s\n", 
                       i, modules[i].pid, module_paths[i]);

                // Set the read end of the child-to-parent pipe to non-blocking
                int flags = fcntl(modules[i].child_to_parent_pipe[0], F_GETFL, 0);
                fcntl(modules[i].child_to_parent_pipe[0], F_SETFL, flags | O_NONBLOCK);
                
                num_modules++;
            }
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

// Function for the controller to send input to the game module (all active modules).
void model_send_input(const char* input) {
    for (int i = 0; i < MAX_MODULES; i++) {
        if (modules[i].active) {
            if (modules[i].module_type == 0) {
                // Pipe-based module
                if (modules[i].pid > 0) {
                    write(modules[i].parent_to_child_pipe[1], input, strlen(input));
                    write(modules[i].parent_to_child_pipe[1], "\n", 1); // Add newline to signal end of command
                }
            } else if (modules[i].module_type == 1) {
                // Shared memory module
                send_to_shared_module(input, i);
            }
        }
    }
}

// Function for the controller to send input to a specific module.
void model_send_input_to_module(const char* input, int module_idx) {
    if (module_idx >= 0 && module_idx < MAX_MODULES && modules[module_idx].active) {
        if (modules[module_idx].module_type == 0) {
            // Pipe-based module
            if (modules[module_idx].pid > 0) {
                write(modules[module_idx].parent_to_child_pipe[1], input, strlen(input));
                write(modules[module_idx].parent_to_child_pipe[1], "\n", 1); // Add newline to signal end of command
            }
        } else if (modules[module_idx].module_type == 1) {
            // Shared memory module
            send_to_shared_module(input, module_idx);
        }
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
               &shapes[num_shapes].alpha);
        shapes[num_shapes].z = 0.0f; // Set default Z
        shapes[num_shapes].width = size;   // Map old size to width
        shapes[num_shapes].height = size;  // Map old size to height
        shapes[num_shapes].depth = size;   // Map old size to depth
        shapes[num_shapes].id = num_shapes; // Assign a simple ID
        strcpy(shapes[num_shapes].label, ""); // Default empty label
        shapes[num_shapes].active = 1;
        // Set default values for rotation and scale (backward compatibility)
        shapes[num_shapes].rotation_x = 0.0f;
        shapes[num_shapes].rotation_y = 0.0f;
        shapes[num_shapes].rotation_z = 0.0f;
        shapes[num_shapes].scale_x = 1.0f;
        shapes[num_shapes].scale_y = 1.0f;
        shapes[num_shapes].scale_z = 1.0f;
        num_shapes++;
        return 1;
    }
    return 0;
}

// Helper function to parse enhanced shape format: "SHAPE:type:label:x:y:z:width:height:depth:r:g:b;a"
// Supports both old format (12 fields) and new format (18 fields) with rotation and scale
int parse_enhanced_format(const char* line) {
    if (num_shapes >= MAX_SHAPES) return 0;
    
    // Create a copy of the line to parse
    char line_copy[1024];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    
    // Tokenize the line by semicolons
    char* tokens[50]; // Array to hold all tokens
    int token_count = 0;
    
    char* token = strtok(line_copy, ";");
    while (token != NULL && token_count < 50) {
        tokens[token_count] = malloc(strlen(token) + 1);
        strcpy(tokens[token_count], token);
        token_count++;
        token = strtok(NULL, ";");
    }
    
    // Check if we have minimum required tokens (at least 13: SHAPE + 12 params: type, label, x, y, z, width, height, depth, r, g, b, a)
    if (token_count < 13) { // Need at least SHAPE + 12 params
        // Free allocated memory
        for (int i = 0; i < token_count; i++) {
            free(tokens[i]);
        }
        return 0;
    }
    
    // Parse the basic parameters (skip "SHAPE")
    strcpy(shapes[num_shapes].type, tokens[1]);
    strcpy(shapes[num_shapes].label, tokens[2]);
    shapes[num_shapes].x = atof(tokens[3]);
    shapes[num_shapes].y = atof(tokens[4]);
    shapes[num_shapes].z = atof(tokens[5]);
    shapes[num_shapes].width = atof(tokens[6]);
    shapes[num_shapes].height = atof(tokens[7]);
    shapes[num_shapes].depth = atof(tokens[8]);
    shapes[num_shapes].color[0] = atof(tokens[9]);
    shapes[num_shapes].color[1] = atof(tokens[10]);
    shapes[num_shapes].color[2] = atof(tokens[11]);
    shapes[num_shapes].alpha = atof(tokens[12]);
    
    // Set default values first
    shapes[num_shapes].rotation_x = 0.0f;
    shapes[num_shapes].rotation_y = 0.0f;
    shapes[num_shapes].rotation_z = 0.0f;
    shapes[num_shapes].scale_x = 1.0f;
    shapes[num_shapes].scale_y = 1.0f;
    shapes[num_shapes].scale_z = 1.0f;
    
    // Initialize texture fields
    shapes[num_shapes].texture_src[0] = '\0';    // Empty string (no texture by default)
    shapes[num_shapes].texture_id = 0;          // No texture ID
    shapes[num_shapes].texture_loaded = 0;      // Not loaded
    shapes[num_shapes].texture_loading = 0;     // Not loading
    
    // Parse optional rotation and scale parameters if they exist
    int param_idx = 13; // Start from 13th token (after basic 13 params)
    
    if (param_idx < token_count) shapes[num_shapes].rotation_x = atof(tokens[param_idx++]);
    if (param_idx < token_count) shapes[num_shapes].rotation_y = atof(tokens[param_idx++]);
    if (param_idx < token_count) shapes[num_shapes].rotation_z = atof(tokens[param_idx++]);
    if (param_idx < token_count) shapes[num_shapes].scale_x = atof(tokens[param_idx++]);
    if (param_idx < token_count) shapes[num_shapes].scale_y = atof(tokens[param_idx++]);
    if (param_idx < token_count) shapes[num_shapes].scale_z = atof(tokens[param_idx++]);
    
    // Now check for optional parameters like texture_src
    for (int i = param_idx; i < token_count; i++) {
        char* current_token = tokens[i];
        
        // Check if this token contains "texture_src="
        if (strstr(current_token, "texture_src=") == current_token) {
            // Extract the texture source path
            char* src_path = current_token + strlen("texture_src=");
            if (strlen(src_path) > 0) {
                strncpy(shapes[num_shapes].texture_src, src_path, sizeof(shapes[num_shapes].texture_src) - 1);
                shapes[num_shapes].texture_src[sizeof(shapes[num_shapes].texture_src) - 1] = '\0';
            }
        }
        // Add more optional parameters here in the future
    }
    
    // Set shape properties
    shapes[num_shapes].id = num_shapes;
    shapes[num_shapes].active = 1;
    num_shapes++;
    
    // Free allocated memory
    for (int i = 0; i < token_count; i++) {
        free(tokens[i]);
    }
    
    return 1;
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

// Function to be called continuously to update the model state from the game module(s).
// Returns 1 if any model was updated, 0 otherwise.
int update_model() {
    int any_updated = 0; // Flag to track if any module updated the model
    
    // Process data from all active modules
    for (int mod_idx = 0; mod_idx < MAX_MODULES; mod_idx++) {
        if (!modules[mod_idx].active) {
            continue; // Skip inactive modules
        }
        
        if (modules[mod_idx].module_type == 0) {
            // Handle pipe-based module
            if (modules[mod_idx].pid <= 0) {
                continue; // Skip if no valid PID for pipe-based module
            }
            
            char buffer[1024];
            int bytes_read = read(modules[mod_idx].child_to_parent_pipe[0], buffer, sizeof(buffer) - 1);

            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';

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
                        
                        // First, try to parse the old format (12 fields): "SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a"
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
                                    shapes[i].alpha = a;
                                    // Set default values for rotation and scale (backward compatibility)
                                    shapes[i].rotation_x = 0.0f;
                                    shapes[i].rotation_y = 0.0f;
                                    shapes[i].rotation_z = 0.0f;
                                    shapes[i].scale_x = 1.0f;
                                    shapes[i].scale_y = 1.0f;
                                    shapes[i].scale_z = 1.0f;
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
                                        shapes[i].alpha = a;
                                        // Set default values for rotation and scale (backward compatibility)
                                        shapes[i].rotation_x = 0.0f;
                                        shapes[i].rotation_y = 0.0f;
                                        shapes[i].rotation_z = 0.0f;
                                        shapes[i].scale_x = 1.0f;
                                        shapes[i].scale_y = 1.0f;
                                        shapes[i].scale_z = 1.0f;
                                        
                                        // Initialize texture fields
                                        shapes[i].texture_src[0] = '\0';    // Empty string (no texture by default)
                                        shapes[i].texture_id = 0;           // No texture ID
                                        shapes[i].texture_loaded = 0;       // Not loaded
                                        shapes[i].texture_loading = 0;      // Not loading
                                        
                                        // Try to extract texture source from the line if present
                                        char texture_path[256];
                                        if (extract_texture_source_from_line(line, texture_path, sizeof(texture_path))) {
                                            strncpy(shapes[i].texture_src, texture_path, sizeof(shapes[i].texture_src) - 1);
                                            shapes[i].texture_src[sizeof(shapes[i].texture_src) - 1] = '\0';
                                        }
                                        
                                        shapes[i].id = i;
                                        shapes[i].active = 1;
                                        break;
                                    }
                                }
                            }
                        } else {
                            // Try to parse the new format (18 fields): "SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a;rot_x;rot_y;rot_z;scale_x;scale_y;scale_z"
                            float rot_x, rot_y, rot_z, scale_x, scale_y, scale_z;
                            items = sscanf(line, "SHAPE;%31[^;];%63[^;];%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f",
                                          type, label, &x, &y, &z, &width, &height, &depth, &r, &g, &b, &a,
                                          &rot_x, &rot_y, &rot_z, &scale_x, &scale_y, &scale_z);
                            
                            if (items == 18) {
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
                                        shapes[i].alpha = a;
                                        // Use the provided rotation and scale values
                                        shapes[i].rotation_x = rot_x;
                                        shapes[i].rotation_y = rot_y;
                                        shapes[i].rotation_z = rot_z;
                                        shapes[i].scale_x = scale_x;
                                        shapes[i].scale_y = scale_y;
                                        shapes[i].scale_z = scale_z;
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
                                            shapes[i].alpha = a;
                                            // Use the provided rotation and scale values
                                            shapes[i].rotation_x = rot_x;
                                            shapes[i].rotation_y = rot_y;
                                            shapes[i].rotation_z = rot_z;
                                            shapes[i].scale_x = scale_x;
                                            shapes[i].scale_y = scale_y;
                                            shapes[i].scale_z = scale_z;
                                            
                                            // Initialize texture fields
                                            shapes[i].texture_src[0] = '\0';    // Empty string (no texture by default)
                                            shapes[i].texture_id = 0;           // No texture ID
                                            shapes[i].texture_loaded = 0;       // Not loaded
                                            shapes[i].texture_loading = 0;      // Not loading
                                            
                                            shapes[i].id = i;
                                            shapes[i].active = 1;
                                            break;
                                        }
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
                
                any_updated = 1; // Mark that at least one module updated the model
            }
        } else if (modules[mod_idx].module_type == 1) {
            // Handle shared memory module
            
            // First, check if there's CHTML content available from this module
            char chtml_buffer[4096];
            int chtml_len = receive_chtml_from_module(chtml_buffer, sizeof(chtml_buffer), mod_idx);
            if (chtml_len > 0) {
                printf("Received CHTML content from shared memory module %d:\n%s\n", mod_idx, chtml_buffer);
                
                // Check if this is a special command (e.g., LOAD_FILE:filename:width:height)
                if (strncmp(chtml_buffer, "LOAD_FILE:", 10) == 0) {
                    // Parse the command: LOAD_FILE:filename:width:height
                    char filename[256];
                    int width, height;
                    if (sscanf(chtml_buffer, "LOAD_FILE:%255[^:]:%d:%d", filename, &width, &height) == 3) {
                        printf("Module requesting to load CHTML file: %s with dimensions %dx%d\n", filename, width, height);
                        
                        // Call the view function to load the external CHTML file
                        int window_id = load_chtml_file_to_window(filename, width, height);
                        printf("Created window (ID: %d) with external CHTML from: %s\n", window_id, filename);
                        any_updated = 1; // Mark that content was updated
                    } else {
                        printf("Error: Could not parse LOAD_FILE command: %s\n", chtml_buffer);
                    }
                } else {
                    // Regular CHTML content - add to DOM
                    int elements_added = add_chtml_to_dom(chtml_buffer);
                    printf("Added %d elements to DOM from module %d\n", elements_added, mod_idx);
                    any_updated = 1; // Mark that content was updated
                }
            }
            
            // Then, check for regular response content
            char buffer[4096];
            int response_len = read_from_shared_module(buffer, sizeof(buffer), mod_idx);
            
            if (response_len > 0) {
                // Process the response similar to pipe-based modules
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
                        
                        // First, try to parse the old format (12 fields): "SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a"
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
                                    shapes[i].alpha = a;
                                    // Set default values for rotation and scale (backward compatibility)
                                    shapes[i].rotation_x = 0.0f;
                                    shapes[i].rotation_y = 0.0f;
                                    shapes[i].rotation_z = 0.0f;
                                    shapes[i].scale_x = 1.0f;
                                    shapes[i].scale_y = 1.0f;
                                    shapes[i].scale_z = 1.0f;
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
                                        shapes[i].alpha = a;
                                        // Set default values for rotation and scale (backward compatibility)
                                        shapes[i].rotation_x = 0.0f;
                                        shapes[i].rotation_y = 0.0f;
                                        shapes[i].rotation_z = 0.0f;
                                        shapes[i].scale_x = 1.0f;
                                        shapes[i].scale_y = 1.0f;
                                        shapes[i].scale_z = 1.0f;
                                        
                                        // Initialize texture fields
                                        shapes[i].texture_src[0] = '\0';    // Empty string (no texture by default)
                                        shapes[i].texture_id = 0;           // No texture ID
                                        shapes[i].texture_loaded = 0;       // Not loaded
                                        shapes[i].texture_loading = 0;      // Not loading
                                        
                                        // Try to extract texture source from the line if present
                                        char texture_path[256];
                                        if (extract_texture_source_from_line(line, texture_path, sizeof(texture_path))) {
                                            strncpy(shapes[i].texture_src, texture_path, sizeof(shapes[i].texture_src) - 1);
                                            shapes[i].texture_src[sizeof(shapes[i].texture_src) - 1] = '\0';
                                        }
                                        
                                        shapes[i].id = i;
                                        shapes[i].active = 1;
                                        break;
                                    }
                                }
                            }
                        } else {
                            // Try to parse the new format (18 fields): "SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a;rot_x;rot_y;rot_z;scale_x;scale_y;scale_z"
                            float rot_x, rot_y, rot_z, scale_x, scale_y, scale_z;
                            items = sscanf(line, "SHAPE;%31[^;];%63[^;];%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f",
                                          type, label, &x, &y, &z, &width, &height, &depth, &r, &g, &b, &a,
                                          &rot_x, &rot_y, &rot_z, &scale_x, &scale_y, &scale_z);
                            
                            if (items == 18) {
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
                                        shapes[i].alpha = a;
                                        // Use the provided rotation and scale values
                                        shapes[i].rotation_x = rot_x;
                                        shapes[i].rotation_y = rot_y;
                                        shapes[i].rotation_z = rot_z;
                                        shapes[i].scale_x = scale_x;
                                        shapes[i].scale_y = scale_y;
                                        shapes[i].scale_z = scale_z;
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
                                            shapes[i].alpha = a;
                                            // Use the provided rotation and scale values
                                            shapes[i].rotation_x = rot_x;
                                            shapes[i].rotation_y = rot_y;
                                            shapes[i].rotation_z = rot_z;
                                            shapes[i].scale_x = scale_x;
                                            shapes[i].scale_y = scale_y;
                                            shapes[i].scale_z = scale_z;
                                            
                                            // Initialize texture fields
                                            shapes[i].texture_src[0] = '\0';    // Empty string (no texture by default)
                                            shapes[i].texture_id = 0;           // No texture ID
                                            shapes[i].texture_loaded = 0;       // Not loaded
                                            shapes[i].texture_loading = 0;      // Not loading
                                            
                                            shapes[i].id = i;
                                            shapes[i].active = 1;
                                            break;
                                        }
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
                
                any_updated = 1; // Mark that at least one module updated the model
            }
        }
    }
    
    return any_updated; // Return 1 if any module updated, 0 otherwise
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

// Semaphore operations union (needed for semctl)
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Shared memory module implementation
// Generate a unique key for shared memory based on module index
int generate_shm_key(int module_idx) {
    // Create a unique key using ftok and module index
    char pathname[256];
    snprintf(pathname, sizeof(pathname), "/tmp/chtml_shmem_%d", module_idx);
    
    // Create the file if it doesn't exist
    FILE* temp_file = fopen(pathname, "w");
    if (temp_file) {
        fprintf(temp_file, "Shared memory for chtml module %d", module_idx);
        fclose(temp_file);
    }
    
    return ftok(pathname, 'R' + module_idx); // 'R' is arbitrary project identifier
}

// Initialize a shared memory module
int init_shared_memory_module(const char* module_path, int module_idx) {
    if (module_idx >= MAX_MODULES) {
        printf("Error: Module index %d exceeds maximum modules (%d)\n", module_idx, MAX_MODULES);
        return -1;
    }
    
    // Generate unique key for shared memory segment
    key_t shm_key = generate_shm_key(module_idx);
    if (shm_key == -1) {
        perror("ftok for shared memory");
        return -1;
    }
    
    // Create shared memory segment
    int shmem_id = shmget(shm_key, sizeof(ModuleSharedMem), IPC_CREAT | 0666);
    if (shmem_id == -1) {
        perror("shmget");
        return -1;
    }
    
    // Attach to shared memory
    ModuleSharedMem* shmem_ptr = (ModuleSharedMem*)shmat(shmem_id, NULL, 0);
    if (shmem_ptr == (ModuleSharedMem*)-1) {
        perror("shmat");
        return -1;
    }
    
    // Initialize the shared memory structure
    memset(shmem_ptr, 0, sizeof(ModuleSharedMem));
    shmem_ptr->module_id = module_idx;
    shmem_ptr->active = 1;
    shmem_ptr->command_ready = 0;
    shmem_ptr->response_ready = 0;
    shmem_ptr->last_command_timestamp = 0;
    shmem_ptr->last_response_timestamp = 0;
    
    // Create semaphore for synchronization
    key_t sem_key = ftok("/tmp", 'S' + module_idx); // Different identifier for semaphores
    if (sem_key == -1) {
        perror("ftok for semaphore");
        shmdt(shmem_ptr);
        return -1;
    }
    
    int sem_id = semget(sem_key, 2, IPC_CREAT | 0666); // Two semaphores: command and response
    if (sem_id == -1) {
        perror("semget");
        shmdt(shmem_ptr);
        return -1;
    }
    
    // Initialize semaphores: command semaphore = 0 (no command initially), response semaphore = 0
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg;
    arg.val = 0;
    if (semctl(sem_id, 0, SETVAL, arg) == -1) { // command semaphore
        perror("semctl SETVAL command");
        shmdt(shmem_ptr);
        return -1;
    }
    if (semctl(sem_id, 1, SETVAL, arg) == -1) { // response semaphore
        perror("semctl SETVAL response");
        shmdt(shmem_ptr);
        return -1;
    }
    
    shmem_ptr->sem_id = sem_id;
    
    // Set up the module info structure
    modules[module_idx].module_type = 1; // shared memory type
    modules[module_idx].pid = -1;        // No separate process for shared memory modules (they're linked as libraries later)
    modules[module_idx].shmem_id = shmem_id;
    modules[module_idx].shmem_ptr = shmem_ptr;
    modules[module_idx].active = 1;
    
    // Initialize pipe arrays to invalid values (for safety)
    modules[module_idx].parent_to_child_pipe[0] = -1;
    modules[module_idx].parent_to_child_pipe[1] = -1;
    modules[module_idx].child_to_parent_pipe[0] = -1;
    modules[module_idx].child_to_parent_pipe[1] = -1;
    
    num_modules++;
    
    printf("Initialized shared memory module %d at key %d, shmem_id %d\n", module_idx, shm_key, shmem_id);
    
    return 0;
}

// Wait for semaphore with timeout
int wait_for_semaphore(int sem_id, int sem_num, int timeout_seconds) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = -1;  // Wait (decrement)
    sb.sem_flg = IPC_NOWAIT; // Use non-blocking to allow timeout checking
    
    time_t start_time = time(NULL);
    while (1) {
        if (semop(sem_id, &sb, 1) == 0) {
            return 0; // Success
        }
        
        if (errno != EAGAIN) {
            perror("semop wait");
            return -1;
        }
        
        // Check if timeout has been reached
        if (time(NULL) - start_time > timeout_seconds) {
            return -1; // Timeout
        }
        
        usleep(1000); // Sleep 1ms before retrying
    }
}

// Signal semaphore
int signal_semaphore(int sem_id, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = 1;   // Signal (increment)
    sb.sem_flg = 0;
    
    return semop(sem_id, &sb, 1);
}

// Send command to shared memory module
int send_to_shared_module(const char* input, int module_idx) {
    if (module_idx >= MAX_MODULES || !modules[module_idx].active || 
        modules[module_idx].module_type != 1 || !modules[module_idx].shmem_ptr) {
        printf("Error: Invalid shared memory module index %d\n", module_idx);
        return -1;
    }
    
    ModuleSharedMem* shm = modules[module_idx].shmem_ptr;
    
    // Wait if previous command is still being processed
    if (shm->command_ready) {
        printf("Warning: Previous command still pending for module %d\n", module_idx);
        // In a real implementation, we might want to wait or return an error
    }
    
    // Copy command to shared buffer
    int input_len = strlen(input);
    if (input_len >= sizeof(shm->command_buffer)) {
        printf("Error: Command too long for module %d\n", module_idx);
        return -1;
    }
    
    strcpy(shm->command_buffer, input);
    shm->command_ready = 1;
    shm->last_command_timestamp = time(NULL);
    
    // Signal the module that a command is ready (semaphore 0)
    if (signal_semaphore(shm->sem_id, 0) == -1) {
        perror("signal_semaphore command");
        shm->command_ready = 0;
        return -1;
    }
    
    return 0;
}

// Read response from shared memory module
int read_from_shared_module(char* buffer, int buffer_size, int module_idx) {
    if (module_idx >= MAX_MODULES || !modules[module_idx].active || 
        modules[module_idx].module_type != 1 || !modules[module_idx].shmem_ptr) {
        printf("Error: Invalid shared memory module index %d\n", module_idx);
        return -1;
    }
    
    ModuleSharedMem* shm = modules[module_idx].shmem_ptr;
    
    // Check if response is ready without blocking
    if (!shm->response_ready) {
        // In a real implementation, we might want to use semaphores to wait efficiently
        return 0; // No response ready yet
    }
    
    // Copy response from shared buffer
    int response_len = strlen(shm->response_buffer);
    if (response_len >= buffer_size) {
        printf("Error: Response too long for buffer in module %d\n", module_idx);
        return -1;
    }
    
    strcpy(buffer, shm->response_buffer);
    shm->response_ready = 0;
    
    // Signal that response has been read (for synchronization purposes)
    // In this implementation, we just clear the response flag
    // The module will set response_ready back to 1 when it has new data
    
    return response_len;
}

// Cleanup shared memory module
int cleanup_shared_module(int module_idx) {
    if (module_idx >= MAX_MODULES || !modules[module_idx].active || 
        modules[module_idx].module_type != 1 || !modules[module_idx].shmem_ptr) {
        return -1;
    }
    
    ModuleSharedMem* shmem_ptr = modules[module_idx].shmem_ptr;
    
    if (shmem_ptr) {
        // Detach from shared memory
        shmdt(shmem_ptr);
        
        // Remove the shared memory segment
        shmctl(modules[module_idx].shmem_id, IPC_RMID, NULL);
        
        // Remove semaphores if needed
        if (shmem_ptr->sem_id != -1) {
            semctl(shmem_ptr->sem_id, 0, IPC_RMID);
        }
    }
    
    // Reset module info
    modules[module_idx].active = 0;
    modules[module_idx].shmem_ptr = NULL;
    modules[module_idx].shmem_id = -1;
    
    num_modules--;
    
    return 0;
}

// Function to check if a response is available without blocking
int is_shared_module_response_ready(int module_idx) {
    if (module_idx >= MAX_MODULES || !modules[module_idx].active || 
        modules[module_idx].module_type != 1 || !modules[module_idx].shmem_ptr) {
        return 0;
    }
    
    return modules[module_idx].shmem_ptr->response_ready;
}

// Send CHTML content to main process via shared memory
int send_chtml_to_main(int module_idx, const char* chtml_content) {
    if (module_idx >= MAX_MODULES || !modules[module_idx].active || 
        modules[module_idx].module_type != 1 || !modules[module_idx].shmem_ptr) {
        printf("Error: Invalid shared memory module index %d in send_chtml_to_main\n", module_idx);
        return -1;
    }
    
    ModuleSharedMem* shm = modules[module_idx].shmem_ptr;
    
    // Check if previous CHTML content is still being processed
    if (shm->chtml_ready) {
        printf("Warning: Previous CHTML content still pending for module %d\n", module_idx);
        // In a real implementation, we might want to wait or return an error
    }
    
    // Copy CHTML content to shared buffer
    int content_len = strlen(chtml_content);
    if (content_len >= sizeof(shm->chtml_content)) {
        printf("Error: CHTML content too long for module %d\n", module_idx);
        return -1;
    }
    
    strcpy(shm->chtml_content, chtml_content);
    shm->chtml_content_size = content_len;
    shm->chtml_ready = 1;
    
    printf("CHTML content sent to main from module %d (size: %d)\n", module_idx, content_len);
    
    return 0;
}

// Read CHTML content from shared memory module
int receive_chtml_from_module(char* buffer, int buffer_size, int module_idx) {
    if (module_idx >= MAX_MODULES || !modules[module_idx].active || 
        modules[module_idx].module_type != 1 || !modules[module_idx].shmem_ptr) {
        printf("Error: Invalid shared memory module index %d in receive_chtml_from_module\n", module_idx);
        return -1;
    }
    
    ModuleSharedMem* shm = modules[module_idx].shmem_ptr;
    
    // Check if CHTML content is ready
    if (!shm->chtml_ready) {
        return 0; // No CHTML content ready yet
    }
    
    // Copy CHTML content from shared buffer
    int content_len = shm->chtml_content_size;
    if (content_len >= buffer_size) {
        printf("Error: CHTML content too long for buffer in module %d\n", module_idx);
        return -1;
    }
    
    strcpy(buffer, shm->chtml_content);
    shm->chtml_ready = 0; // Mark as consumed
    
    return content_len;
}

// Check if CHTML content is ready without blocking
int is_chtml_ready(int module_idx) {
    if (module_idx >= MAX_MODULES || !modules[module_idx].active || 
        modules[module_idx].module_type != 1 || !modules[module_idx].shmem_ptr) {
        return 0;
    }
    
    return modules[module_idx].shmem_ptr->chtml_ready;
}

// Helper function to extract texture source from a SHAPE command line
int extract_texture_source_from_line(const char* line, char* texture_path, size_t path_size) {
    // Create a copy of the line to tokenize
    char line_copy[1024];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    
    // Look for the texture_src= parameter in the whole line
    const char* texture_param = strstr(line_copy, "texture_src=");
    if (texture_param) {
        texture_param += strlen("texture_src="); // Move past the equals sign
        // Copy the texture path up to the end or next delimiter
        const char* end = texture_param;
        while (*end && *end != ';' && *end != '\n' && *end != '\r' && *end != '\0') {
            end++;
        }
        size_t len = end - texture_param;
        if (len >= path_size) len = path_size - 1;
        strncpy(texture_path, texture_param, len);
        texture_path[len] = '\0';
        return 1; // Successfully extracted
    }
    
    // Try alternative parameter name
    const char* alt_texture_param = strstr(line_copy, "texture=");
    if (alt_texture_param) {
        alt_texture_param += strlen("texture=");
        const char* end = alt_texture_param;
        while (*end && *end != ';' && *end != '\n' && *end != '\r' && *end != '\0') {
            end++;
        }
        size_t len = end - alt_texture_param;
        if (len >= path_size) len = path_size - 1;
        strncpy(texture_path, alt_texture_param, len);
        texture_path[len] = '\0';
        return 1; // Successfully extracted
    }
    
    return 0; // No texture parameter found
}
