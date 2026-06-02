
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // for usleep (no longer used but some systems might have it referenced)
#include <sys/wait.h>  // for waitpid
#include <sys/stat.h>  // for file operations
#include <string.h>  // for string operations

#define MAX_MODULES 10

// Forward declarations for functions in other files
void init_controller();
void init_view(const char* filename);
void init_model(const char* module_path);
int update_model();
int update_camera_frame(void);  // Added for camera updates
void get_window_properties(int* x, int* y, int* width, int* height, char* title, int title_size);

void display();
void reshape(int, int);
void mouse(int, int, int, int);
void keyboard(unsigned char, int, int);
void special_keys(int, int, int);
void mouse_motion(int, int);

// Forward declarations for audio functionality
void init_audio_system();
void cleanup_audio();

// Forward declarations for video functionality
void init_video_system();
void cleanup_video();
int are_any_videos_playing();
int update_video_texture(const char* filepath, GLuint texture_id);

char* module_executable = NULL;

// Forward declaration for controller function to update UI with model variables
void update_ui_with_model_variables();

// Forward declaration for timer callback function
void timer_callback(int value);

// Forward declaration for getting modules from view
typedef struct {
    char path[512];
    int active;
    int protocol;  // 0 = pipe (default), 1 = shared memory
} ModulePath;

ModulePath* get_modules(int* count);

// Function declarations for features
void check_navigation_requests();
void reload_view_with_file(const char* filename);
void init_multiple_modules(const char** module_paths, int count);
void init_multiple_modules_extended(const char** module_paths, const int* protocols, int count);

// Additional globals for the navigation system
char current_chtml_file[512];
int reload_requested = 0;
char next_chtml_file[512];

// Forward declaration for updating inline elements
void update_inline_elements();
extern int view_needs_refresh();

void timer_callback(int value) {
    int updated = update_model();
    int camera_updated = update_camera_frame();  // Update camera frame if available
    
    if (updated || camera_updated) {
        update_ui_with_model_variables();  // Update UI with new model data
        glutPostRedisplay();
    }
    
    // Update inline elements (check for new CHTML content from modules)
    update_inline_elements();
    
    // Check if view needs refresh after DOM updates
    if (view_needs_refresh()) {
        glutPostRedisplay();
    }
    
    // Check if any videos are playing and may need refresh
    if (are_any_videos_playing()) {
        glutPostRedisplay(); // Force a redraw if videos are playing
    }
    
    // Check for navigation requests
    check_navigation_requests();
    
    // Schedule the next timer callback for consistent ~60 FPS (16ms interval)
    glutTimerFunc(16, timer_callback, 0);
}

// Forward declarations for functions in view.c that need to be accessible
extern void cleanup_view(); // We'll implement this to reset the view state

// Function to reload the view with a new CHTML file
void reload_view_with_file(const char* filename) {
    printf("Reloading view with file: %s\n", filename);
    
    // Cleanup current view state
    cleanup_view();
    
    // Initialize MVC components - init_view first to parse modules
    init_view(filename);
    
    // Get modules from the parsed CHTML file
    int num_modules = 0;
    ModulePath* modules = get_modules(&num_modules);
    
    if (num_modules > 0) {
        printf("Found %d module(s) in CHTML file:\n", num_modules);
        
        // Create array of module paths
        const char* module_paths[MAX_MODULES];
        for (int i = 0; i < num_modules && i < MAX_MODULES; i++) {
            module_paths[i] = modules[i].path;
            printf("  Module %d: %s\n", i+1, modules[i].path);
        }
        
        // Create array of protocol information
        int module_protocols[MAX_MODULES];
        for (int i = 0; i < num_modules && i < MAX_MODULES; i++) {
            module_protocols[i] = modules[i].protocol;  // Use protocol from parsed modules
            printf("Module %d protocol: %s\n", i+1, (modules[i].protocol == 1) ? "shared" : "pipe");
        }
        
        // Initialize model with multiple modules using extended function
        init_multiple_modules_extended(module_paths, module_protocols, num_modules);
    } else {
        // Initialize model without module if none specified in CHTML
        init_model(NULL);
    }
    
    init_controller();
    
    // Redraw the scene
    glutPostRedisplay();
}

// Function to check for navigation requests
void check_navigation_requests() {
    if (reload_requested) {
        reload_requested = 0;
        printf("Processing navigation request to: %s\n", next_chtml_file);
        reload_view_with_file(next_chtml_file);
        
        // Update current file path to reflect new page
        strncpy(current_chtml_file, next_chtml_file, sizeof(current_chtml_file) - 1);
        current_chtml_file[sizeof(current_chtml_file) - 1] = '\0';
    }
    
    // Check if there's a navigation request file
    FILE* nav_file = fopen("nav_request.tmp", "r");
    if (nav_file) {
        char new_file[512];
        if (fgets(new_file, sizeof(new_file), nav_file)) {
            // Remove newline if present
            new_file[strcspn(new_file, "\n")] = 0;
            printf("Navigation requested to: %s\n", new_file);
            
            // Verify the file exists
            struct stat buffer;
            if (stat(new_file, &buffer) == 0) {
                strncpy(next_chtml_file, new_file, sizeof(next_chtml_file) - 1);
                next_chtml_file[sizeof(next_chtml_file) - 1] = '\0';
                reload_requested = 1;
            } else {
                printf("Navigation target file does not exist: %s\n", new_file);
            }
        }
        fclose(nav_file);
        // Remove the navigation request file
        remove("nav_request.tmp");
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <markup_file.chtml>\n", argv[0]);
        exit(1);
    }
    
    strncpy(current_chtml_file, argv[1], sizeof(current_chtml_file) - 1);
    current_chtml_file[sizeof(current_chtml_file) - 1] = '\0';
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    
    // Initialize MVC components - init_view first to parse modules
    init_view(current_chtml_file); // Parse the CHTML file provided as argument
    
    // Get window properties from the parsed CHTML file
    int window_x, window_y, window_width, window_height;
    char window_title[256];
    get_window_properties(&window_x, &window_y, &window_width, &window_height, window_title, sizeof(window_title));
    
    glutInitWindowSize(window_width, window_height);
    glutInitWindowPosition(window_x, window_y);
    glutCreateWindow(window_title);

    // Get modules from the parsed CHTML file
    int num_modules = 0;
    ModulePath* modules = get_modules(&num_modules);
    
    if (num_modules > 0) {
        printf("Found %d module(s) in CHTML file:\n", num_modules);
        
        // Create array of module paths
        const char* module_paths[MAX_MODULES];
        for (int i = 0; i < num_modules && i < MAX_MODULES; i++) {
            module_paths[i] = modules[i].path;
            printf("  Module %d: %s\n", i+1, modules[i].path);
        }
        
        // Create array of protocol information
        int module_protocols[MAX_MODULES];
        for (int i = 0; i < num_modules && i < MAX_MODULES; i++) {
            module_protocols[i] = modules[i].protocol;  // Use protocol from parsed modules
            printf("Module %d protocol: %s\n", i+1, (modules[i].protocol == 1) ? "shared" : "pipe");
        }
        
        // Initialize model with multiple modules using extended function
        init_multiple_modules_extended(module_paths, module_protocols, num_modules);
    } else {
        // Initialize model without module if none specified in CHTML
        init_model(NULL);
    }
    
    init_controller();
    
    // Initialize audio and video systems
    init_audio_system();
    init_video_system();

    // Register GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keys);
    glutMotionFunc(mouse_motion);

    // Set up timer callback for consistent ~60 FPS instead of busy-wait idle loop
    glutTimerFunc(16, timer_callback, 0);

    glutMainLoop();
    
    // Cleanup audio and video systems on exit
    cleanup_audio();
    cleanup_video();

    return 0;
}
