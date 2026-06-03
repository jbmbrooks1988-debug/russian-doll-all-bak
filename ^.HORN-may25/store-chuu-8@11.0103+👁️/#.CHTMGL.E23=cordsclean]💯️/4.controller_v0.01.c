#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <GL/glut.h>
#include <GL/glut.h> // Required for glutPostRedisplay and glutGet

// We need the UIElement definition and the elements array from view.c
// In a real C application, you'd have a .h file for this.
#define MAX_ELEMENTS 50
#define MAX_MODULES 10
#define MAX_TEXT_LINES 1000
#define MAX_LINE_LENGTH 512
#define MAX_PAGE_HISTORY 20  // For back/forward navigation

#define CLIPBOARD_SIZE 10000

// MAX_ATTRIBUTES added for DOM functionality
#define MAX_ATTRIBUTES 50

// Flex sizing structures
typedef enum {
    FLEX_UNIT_PIXEL,
    FLEX_UNIT_PERCENT
} FlexUnit;

typedef struct {
    float value;
    FlexUnit unit;
} FlexSize;

// UIElement structure - must match the one in view.c
typedef struct {
    char type[20];
    int x, y; // Original attributes from CHTML
    FlexSize width, height; // Updated to use FlexSize for percentage support
    int calculated_width, calculated_height; // Calculated values after flex resolution
    int calculated_x, calculated_y; // Calculated absolute positions after layout
    char id[50]; // Unique identifier for UI elements
    char label[50]; // Used for button label, text element value, and checkbox label
    // Multi-line text support - replace simple text_content with array of lines
    char text_content[MAX_TEXT_LINES][MAX_LINE_LENGTH];
    int num_lines; // Number of lines in the text
    int cursor_x; // X position of cursor (column)
    int cursor_y; // Y position of cursor (line number)
    int is_active; // For textfield active state
    int is_checked; // For checkbox checked state
    int slider_value; // For slider current value
    int slider_min; // For slider minimum value
    int slider_max; // For slider maximum value
    int slider_step; // For slider step increment
    float color[3];
    float alpha; // Alpha transparency
    int parent;
    // Canvas-specific properties
    int canvas_initialized; // Flag to track if canvas has been initialized
    void (*canvas_render_func)(int x, int y, int width, int height); // Render function for canvas
    char view_mode[10]; // View mode: "2d" or "3d"
    // Camera properties for 3D view
    float camera_pos[3]; // Camera position (x, y, z)
    float camera_target[3]; // Camera target (x, y, z)
    float camera_up[3]; // Camera up vector (x, y, z)
    float fov; // Field of view for 3D perspective
    // Event handling
    char onClick[50]; // Store the onClick handler function name
    // Menu-specific properties
    int menu_items_count; // Number of submenu items
    int is_open; // For menus - whether they are currently open
    // Text selection properties
    int selection_start_x; // X position of selection start
    int selection_start_y; // Y position of selection start
    int selection_end_x;   // X position of selection end
    int selection_end_y;   // Y position of selection end
    int has_selection;     // Whether there is an active selection
    // Clipboard for this element
    char clipboard[CLIPBOARD_SIZE];
    // Directory listing properties
    char dir_path[512]; // Directory path for directory listing elements
    char dir_entries[100][256]; // Store up to 100 directory entries
    int dir_entry_count; // Number of directory entries
    int dir_entry_selected; // Index of selected entry
    // Link properties
    char href[512]; // Store href attribute for links
    // Z-level for rendering order (0-99, default 0)
    int z_level;   // Z-level for depth ordering
    // Image-specific properties
    char image_src[512]; // Path to image file
    int texture_id;      // OpenGL texture ID
    int texture_loaded;  // Flag to indicate if texture is loaded
    float image_aspect_ratio; // Width/height ratio of the original image
    // Video-specific properties
    char video_src[512];  // Store video source file path
    int video_loaded;     // Flag to indicate if video has been loaded
    int video_playing;    // Flag to indicate if video is playing
    int video_autoplay;   // Flag for autoplay
    int video_loop;       // Flag for loop
} UIElement;

// Global variables for simplified DOM matrix
// (No complex structures needed - using the existing elements array as our "DOM")

#define CLIPBOARD_SIZE 10000

// Definitions for model variables (for UI updates)
#define MAX_VARIABLES 50
#define MAX_ARRAYS 20

// Forward declarations for functions defined later in this file
void handle_ctrl_keys(unsigned char key, int element_index);
void copy_text(int element_index);
void cut_text(int element_index);
void paste_text(int element_index);
void delete_selection(int element_index);
void model_send_input(const char* input);
void model_navigate_dir(const char* subdir);
char (*model_get_dir_entries(int* count))[256];

// Forward declarations for new model functions
typedef struct {
    char label[64];
    int type;  // 1=int, 2=float, 3=string
    union {
        int int_val;
        float float_val;
        char string_val[256];
    } value;
    int active;
} ModelVariable;

ModelVariable* model_get_variable(const char* label);
ModelVariable* model_get_variables(int* count);

// Forward declaration for execute_handler_by_name function (to avoid implicit declaration)
void execute_handler_by_name(const char* handler_name);

// Forward declaration for audio functionality
void init_audio_system();
int play_audio(const char* filepath);
int stop_audio(const char* filepath);
int pause_audio(const char* filepath);
int resume_audio(const char* filepath);
int is_audio_playing(const char* filepath);
double get_audio_time(const char* filepath);
void cleanup_audio();

// Forward declarations for video functionality
void init_video_system();
int play_video(const char* filepath, int autoplay, int loop);
int stop_video(const char* filepath);
int pause_video(const char* filepath);
int resume_video(const char* filepath);
int is_video_playing(const char* filepath);
int update_video_texture(const char* filepath, GLuint texture_id);
int get_video_dimensions(const char* filepath, int* width, int* height);
void cleanup_video();

// Forward declaration for the new createWindow function
int createWindow(const char* title, const char* chtmlContent, int width, int height);

// Extern declarations for window dimensions from view.c
extern int window_width;
extern int window_height;

extern UIElement elements[];
extern int num_elements;

void run_module_handler(); // Forward declaration

int active_slider_index = -1; // Global variable to track the currently dragged slider

// Function to handle button clicks based on onClick attribute
void switch_to_2d_view() {
    // Find canvas elements and switch them to 2D mode
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "canvas") == 0) {
            strcpy(elements[i].view_mode, "2d");
            printf("Switched canvas '%s' to 2D mode\n", elements[i].id);
        }
    }
    glutPostRedisplay(); // Redraw the scene
}

void switch_to_3d_view() {
    // Find canvas elements and switch them to 3D mode
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "canvas") == 0) {
            strcpy(elements[i].view_mode, "3d");
            printf("Switched canvas '%s' to 3D mode\n", elements[i].id);
        }
    }
    glutPostRedisplay(); // Redraw the scene
}

// Forward declaration for navigation function
extern void navigate_to_page(const char* page_path);

void handle_element_event(const char* event_handler) {
    // Use the dynamic handler execution
    execute_handler_by_name(event_handler);
}

// Global variables to access main app functions
extern void init_view(const char* filename);

// Global variable to store current file path
char current_file_path[512] = "";

// External functions for loading new view
extern void init_view(const char* filename);
extern void display();

// External functions for content management (from view.c)
extern const char* get_current_page_path();
extern int is_page_loaded(const char* filename);
extern void clear_page_cache();
extern int load_page(const char* filename);
extern int activate_page(const char* filename);

// Global variable to track when navigation occurs (defined in main)
extern int navigation_requested;
extern char navigation_target[512];

// Variables for reload system (defined in main)
extern int reload_requested;
extern char next_chtml_file[512];

// Forward declaration for content manager functions (from view.c)
extern int activate_page(const char* filename);

// Forward declarations for simplified DOM functions
int get_element_index_by_id(const char* id);
int get_element_index_by_tag_name(const char* tag_name);
int get_elements_by_tag_name(const char* tag_name, int* results, int max_results);
void output_dom_to_csv();

#include <unistd.h>
#include <sys/wait.h>

// Function to handle navigation by writing to a coordination file
void navigate_to_page(const char* page_path) {
    printf("Navigation requested to: %s\n", page_path);
    
    // Verify the file exists before navigating
    FILE* test_file = fopen(page_path, "r");
    if (!test_file) {
        printf("Navigation target file does not exist: %s\n", page_path);
        return;
    }
    fclose(test_file);
    
    // Write the navigation target to a coordination file
    FILE* nav_file = fopen("nav_request.tmp", "w");
    if (nav_file) {
        fprintf(nav_file, "%s", page_path);
        fclose(nav_file);
        printf("Navigation signal sent to: %s\n", page_path);
    } else {
        printf("Failed to create navigation request file\n");
    }
}

void init_controller() {
    // Nothing to initialize for now
}

// Coordinate transformation helper
int convert_y_to_opengl_coords(int y) {
    extern int window_height; // Reference to global window_height from view.c
    return window_height - y;
}

void mouse(int button, int state, int x, int y) {
    int ry = convert_y_to_opengl_coords(y); // Convert from window coordinates (y=0 at top) to OpenGL coordinates (y=0 at bottom)

    if (button == 0) { // Left mouse button
        if (state == 0) { // Mouse button down
            int clicked_textfield_index = -1;

            for (int i = 0; i < num_elements; i++) {
                int parent_x = 0;
                int parent_y = 0;
                int current_parent = elements[i].parent;
                while (current_parent != -1) {
                    parent_x += elements[current_parent].x;
                    parent_y += elements[current_parent].y;
                    current_parent = elements[current_parent].parent;
                }

                int abs_x = parent_x + elements[i].calculated_x;
                int abs_y = window_height - (parent_y + elements[i].calculated_y + elements[i].calculated_height); // Convert to OpenGL coordinates to match view.c
                
                // Special handling for menuitems when their parent menu is open
                if (strcmp(elements[i].type, "menuitem") == 0 && elements[i].parent != -1 && 
                    strcmp(elements[elements[i].parent].type, "menu") == 0 && 
                    elements[elements[i].parent].is_open) {
                    // Calculate submenu position (under the parent menu)
                    int parent_menu = elements[i].parent;
                    int parent_menu_abs_x = 0;
                    int parent_menu_abs_y = 0;
                    int parent_menu_parent = elements[parent_menu].parent;
                    while (parent_menu_parent != -1) {
                        parent_menu_abs_x += elements[parent_menu_parent].calculated_x;
                        parent_menu_abs_y += elements[parent_menu_parent].calculated_y;
                        parent_menu_parent = elements[parent_menu_parent].parent;
                    }
                    int submenu_x = parent_menu_abs_x + elements[parent_menu].calculated_x;
                    int submenu_y = parent_menu_abs_y + elements[parent_menu].calculated_y + elements[parent_menu].calculated_height;
                    
                    // Calculate position of this item in the submenu
                    int item_position = 0;
                    for (int j = 0; j < num_elements; j++) {
                        if (elements[j].parent == parent_menu && 
                            strcmp(elements[j].type, "menuitem") == 0) {
                            if (j == i) { // If this is our element
                                break;
                            }
                            item_position++;
                        }
                    }
                    
                    // Update absolute position to submenu coordinates
                    abs_x = submenu_x;
                    abs_y = submenu_y + (item_position * 20); // 20px height per menu item
                }

                if (x >= abs_x && x <= abs_x + elements[i].calculated_width &&
                    ry >= abs_y && ry <= abs_y + elements[i].calculated_height) {
                    if (strcmp(elements[i].type, "button") == 0) {
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                    } else if (strcmp(elements[i].type, "textfield") == 0 || strcmp(elements[i].type, "textarea") == 0) {
                        clicked_textfield_index = i;
                    } else if (strcmp(elements[i].type, "checkbox") == 0) {
                        elements[i].is_checked = !elements[i].is_checked;
                        printf("Checkbox '%s' toggled to %s!\n", elements[i].label, elements[i].is_checked ? "true" : "false");
                    } else if (strcmp(elements[i].type, "slider") == 0) {
                        active_slider_index = i;
                        // Check if this is a vertical slider by looking at the aspect ratio
                        int is_vertical = (elements[i].calculated_height > elements[i].calculated_width);  // If height is greater than width, consider it vertical
                        
                        if (is_vertical) {
                            // Calculate initial slider value based on vertical click position
                            float normalized_y = (float)(ry - abs_y) / elements[i].calculated_height;
                            // Keep the normalized value as is (no inversion needed)
                            elements[i].slider_value = elements[i].slider_min + normalized_y * (elements[i].slider_max - elements[i].slider_min);
                        } else {
                            // Original horizontal slider implementation
                            float normalized_x = (float)(x - abs_x) / elements[i].calculated_width;
                            elements[i].slider_value = elements[i].slider_min + normalized_x * (elements[i].slider_max - elements[i].slider_min);
                        }
                        // Snap to step
                        elements[i].slider_value = (elements[i].slider_value / elements[i].slider_step) * elements[i].slider_step;
                        if (elements[i].slider_value < elements[i].slider_min) elements[i].slider_value = elements[i].slider_min;
                        if (elements[i].slider_value > elements[i].slider_max) elements[i].slider_value = elements[i].slider_max;
                        printf("Slider '%s' value: %d\n", elements[i].label, elements[i].slider_value);
                        
                        // Send slider value to the module using the model_send_input function
                        char slider_input[100];
                        snprintf(slider_input, sizeof(slider_input), "SLIDER:%s:%d", elements[i].id, elements[i].slider_value);
                        model_send_input(slider_input);
                    } else if (strcmp(elements[i].type, "canvas") == 0) {
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        } else {
                            printf("Canvas '%s' clicked at (%d, %d)\n", elements[i].label, x, ry);
                        }
                        
                        // Calculate relative coordinates within the canvas for the module
                        int canvas_abs_x = parent_x + elements[i].calculated_x;
                        int canvas_abs_y = window_height - (parent_y + elements[i].calculated_y + elements[i].calculated_height); // Convert to OpenGL coords using calculated values
                        
                        // Calculate relative coordinates within canvas
                        int rel_x = x - canvas_abs_x;
                        int rel_y = ry - canvas_abs_y;
                        
                        // Make sure the click is within canvas bounds before sending
                        if (rel_x >= 0 && rel_x <= elements[i].calculated_width && rel_y >= 0 && rel_y <= elements[i].calculated_height) {
                            printf("Canvas '%s' clicked at relative (%d, %d)\\n", elements[i].label, rel_x, rel_y);
                            
                            // Send canvas click information to the module using relative coordinates
                            char canvas_input[100];
                            snprintf(canvas_input, sizeof(canvas_input), "CANVAS:%d,%d", rel_x, rel_y);
                            model_send_input(canvas_input);
                        }
                    } else if (strcmp(elements[i].type, "dirlist") == 0) {
                        // Calculate which entry was clicked based on mouse position
                        int line_height = 20;
                        int header_height = 25; // Account for the "dir element list:" header
                        int relative_y = ry - (abs_y + header_height);
                        int entry_index = relative_y / line_height;

                        // Get the current entries from the model to find the clicked one
                        int current_entry_count = 0;
                        char (*current_entries)[256] = model_get_dir_entries(&current_entry_count);

                        if (entry_index >= 0 && entry_index < current_entry_count) {
                            model_navigate_dir(current_entries[entry_index]);
                            glutPostRedisplay(); // Request a redraw to show the new directory
                        }
                    } else if (strcmp(elements[i].type, "menu") == 0) {
                        // Toggle menu open/close state
                        elements[i].is_open = !elements[i].is_open;
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                        printf("Menu '%s' %s\n", elements[i].label, elements[i].is_open ? "opened" : "closed");
                    } else if (strcmp(elements[i].type, "menuitem") == 0) {
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                        printf("Menu item '%s' clicked\n", elements[i].label);
                    } else if (strcmp(elements[i].type, "link") == 0) {
                        // Handle link clicks - if href is present, navigate to that page
                        if (strlen(elements[i].href) > 0) {
                            printf("Link clicked: %s -> %s\n", elements[i].label, elements[i].href);
                            
                            // Check for relative paths and resolve them relative to current page
                            char resolved_path[512];
                            if (elements[i].href[0] == '/') {
                                // Absolute path
                                strncpy(resolved_path, elements[i].href, sizeof(resolved_path) - 1);
                                resolved_path[sizeof(resolved_path) - 1] = '\0';
                            } else {
                                // Relative path - resolve relative to current file
                                const char* current_path = get_current_page_path();
                                if (current_path && strlen(current_path) > 0) {
                                    // Extract directory from current path
                                    strcpy(resolved_path, current_path);
                                    char* last_slash = strrchr(resolved_path, '/');
                                    if (last_slash) {
                                        *(last_slash + 1) = '\0'; // Include the slash
                                        strncat(resolved_path, elements[i].href, sizeof(resolved_path) - strlen(resolved_path) - 1);
                                    } else {
                                        // If no slash, current file is in current directory
                                        strncpy(resolved_path, elements[i].href, sizeof(resolved_path) - 1);
                                        resolved_path[sizeof(resolved_path) - 1] = '\0';
                                    }
                                } else {
                                    // Fallback to using href as-is
                                    strncpy(resolved_path, elements[i].href, sizeof(resolved_path) - 1);
                                    resolved_path[sizeof(resolved_path) - 1] = '\0';
                                }
                            }
                            
                            // Navigate to the resolved path
                            navigate_to_page(resolved_path);
                        }
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                        printf("Link '%s' clicked\n", elements[i].label);
                    } else if (strcmp(elements[i].type, "audio") == 0) {
                        // Handle audio element clicks, specifically for play/stop buttons
                        // Calculate button positions
                        int button_width = 40;
                        int button_height = 20;
                        int button_y = abs_y + elements[i].calculated_height - button_height - 5;
                        int play_button_x = abs_x + 10;
                        int stop_button_x = abs_x + 10 + button_width + 10;
                        
                        // Check if clicked on play button
                        if (x >= play_button_x && x <= play_button_x + button_width &&
                            ry >= button_y && ry <= button_y + button_height) {
                            printf("Play button clicked for audio: %s\n", elements[i].image_src);
                            if (strlen(elements[i].image_src) > 0) {
                                play_audio(elements[i].image_src);
                            }
                        }
                        // Check if clicked on stop button
                        else if (x >= stop_button_x && x <= stop_button_x + button_width &&
                                 ry >= button_y && ry <= button_y + button_height) {
                            printf("Stop button clicked for audio: %s\n", elements[i].image_src);
                            if (strlen(elements[i].image_src) > 0) {
                                stop_audio(elements[i].image_src);
                            }
                        }
                    }
                    else if (strcmp(elements[i].type, "video") == 0) {
                        // Handle video element clicks, specifically for play/stop buttons
                        // Calculate button positions (similar to audio controls)
                        int button_width = 40;
                        int button_height = 20;
                        int button_y = abs_y + elements[i].calculated_height - button_height - 5;
                        int play_button_x = abs_x + 10;
                        int stop_button_x = abs_x + 10 + button_width + 10;
                        int pause_button_x = abs_x + 10 + (button_width * 2) + 20; // Add pause button
                        
                        // Check if clicked on play button
                        if (x >= play_button_x && x <= play_button_x + button_width &&
                            ry >= button_y && ry <= button_y + button_height) {
                            printf("Play button clicked for video: %s\n", elements[i].video_src);
                            if (strlen(elements[i].video_src) > 0) {
                                play_video(elements[i].video_src, 0, elements[i].video_loop); // Don't autoplay, just play on click
                            }
                        }
                        // Check if clicked on stop button
                        else if (x >= stop_button_x && x <= stop_button_x + button_width &&
                                 ry >= button_y && ry <= button_y + button_height) {
                            printf("Stop button clicked for video: %s\n", elements[i].video_src);
                            if (strlen(elements[i].video_src) > 0) {
                                stop_video(elements[i].video_src);
                            }
                        }
                        // Check if clicked on pause button
                        else if (x >= pause_button_x && x <= pause_button_x + button_width &&
                                 ry >= button_y && ry <= button_y + button_height) {
                            printf("Pause button clicked for video: %s\n", elements[i].video_src);
                            if (strlen(elements[i].video_src) > 0) {
                                if (is_video_playing(elements[i].video_src)) {
                                    pause_video(elements[i].video_src);
                                } else {
                                    resume_video(elements[i].video_src);
                                }
                            }
                        }
                    }
                }
            }

            // Deactivate all textfields first
            for (int i = 0; i < num_elements; i++) {
                if (strcmp(elements[i].type, "textfield") == 0) {
                    elements[i].is_active = 0;
                }
            }

            // Activate the clicked textfield
            if (clicked_textfield_index != -1) {
                elements[clicked_textfield_index].is_active = 1;
            }
            
            // Check if click was outside any menu element to close open menus
            int clicked_on_menu = 0;
            for (int i = 0; i < num_elements; i++) {
                int parent_x = 0;
                int parent_y = 0;
                int current_parent = elements[i].parent;
                // Validate indices to prevent segmentation fault
                while (current_parent != -1 && current_parent < num_elements) {
                    parent_x += elements[current_parent].x;
                    parent_y += elements[current_parent].y;
                    current_parent = elements[current_parent].parent;
                }

                int abs_x = parent_x + elements[i].calculated_x;
                int abs_y = window_height - (parent_y + elements[i].calculated_y + elements[i].calculated_height); // Convert to OpenGL coordinates
                
                // Special handling for menuitems when their parent menu is open (they're drawn in submenu)
                if (strcmp(elements[i].type, "menuitem") == 0 && elements[i].parent != -1 && 
                    elements[i].parent < num_elements && // Validate parent index
                    strcmp(elements[elements[i].parent].type, "menu") == 0 && 
                    elements[elements[i].parent].is_open) {
                    // Calculate submenu position (under the parent menu) - matching view drawing logic
                    int parent_menu = elements[i].parent;
                    int parent_menu_abs_x = 0;
                    int parent_menu_abs_y = 0;
                    int parent_menu_parent = elements[parent_menu].parent;
                    // Validate indices to prevent segmentation fault
                    while (parent_menu_parent != -1 && parent_menu_parent < num_elements) {
                        parent_menu_abs_x += elements[parent_menu_parent].calculated_x;
                        parent_menu_abs_y += elements[parent_menu_parent].calculated_y;
                        parent_menu_parent = elements[parent_menu_parent].parent;
                    }
                    int submenu_x = parent_menu_abs_x + elements[parent_menu].calculated_x;
                    int submenu_y = parent_menu_abs_y + elements[parent_menu].calculated_y + elements[parent_menu].calculated_height;
                    
                    // Calculate position of this item in the submenu
                    int item_position = 0;
                    for (int j = 0; j < num_elements; j++) {
                        if (j < num_elements && elements[j].parent == parent_menu && 
                            strcmp(elements[j].type, "menuitem") == 0) {
                            if (j == i) { // If this is our element
                                break;
                            }
                            item_position++;
                        }
                    }
                    
                    // Update absolute position to submenu coordinates - matching view drawing
                    abs_x = submenu_x;
                    abs_y = submenu_y + (item_position * 20); // 20px height per menu item, matching view drawing
                }

                if (x >= abs_x && x <= abs_x + elements[i].calculated_width &&
                    ry >= abs_y && ry <= abs_y + (strcmp(elements[i].type, "menuitem") == 0 ? 20 : elements[i].calculated_height)) { // Fixed height of 20 for submenu items to match view drawing
                    if (strcmp(elements[i].type, "menu") == 0 || strcmp(elements[i].type, "menuitem") == 0) {
                        clicked_on_menu = 1;  // Clicked on a menu or menu item, don't close menus
                        break;
                    }
                }
            }
            
            // If click was outside any menu, close all open menus
            if (!clicked_on_menu) {
                int menus_closed = 0;
                for (int i = 0; i < num_elements; i++) {
                    if (strcmp(elements[i].type, "menu") == 0 && elements[i].is_open) {
                        // If this is the generic context menu, reset its position to a default/invisible location
                        if (strstr(elements[i].label, "context_menu") != NULL) {
                            // Reset to off-screen position to ensure it doesn't linger where the user clicked
                            elements[i].x = -1000;  // Move off-screen
                            elements[i].y = -1000;  // Move off-screen
                        }
                        elements[i].is_open = 0;
                        printf("Closed menu '%s' as click was outside menu area\\n", elements[i].label);
                        menus_closed = 1;
                    }
                }
                if (menus_closed) {
                    glutPostRedisplay(); // Redraw to hide closed menus
                }
            }
        } else { // Mouse button up
            active_slider_index = -1; // Stop dragging slider
        }
        glutPostRedisplay(); // Redraw to show active textfield/cursor or checkbox state or slider value
    }
    else if (button == 2) { // Right mouse button
        if (state == 0) { // Mouse button down
            // Close any currently open menus first
            for (int j = 0; j < num_elements; j++) {
                if (strcmp(elements[j].type, "menu") == 0) {
                    elements[j].is_open = 0;
                }
            }
            
            // Find a generic context menu in the UI (if it exists)
            int generic_context_menu_idx = -1;
            for (int i = 0; i < num_elements; i++) {
                if (strcmp(elements[i].type, "menu") == 0 && 
                    strstr(elements[i].label, "context_menu") != NULL) {
                    generic_context_menu_idx = i;
                    break;
                }
            }
            // Removed the loop that checks if you clicked on a menu to open it as a context menu
            // Only use the generic context menu
            
            if (generic_context_menu_idx != -1) {
                // For context menu to appear at exact mouse location, we need to compensate for parent offsets
                // During rendering: final_x = parent_x_total + element_x
                // So: element_x = desired_x - parent_x_total = mouse_x - parent_x_total
                int parent_x_total = 0;
                int parent_y_total = 0;
                int current_parent = elements[generic_context_menu_idx].parent;
                while (current_parent != -1) {
                    parent_x_total += elements[current_parent].x;
                    parent_y_total += elements[current_parent].y;
                    current_parent = elements[current_parent].parent;
                }
                
                elements[generic_context_menu_idx].x = x - parent_x_total;
                elements[generic_context_menu_idx].y = y - parent_y_total;
                
                // Boundary checks for context menu - only limit if it would go outside window when rendered
                int final_x_pos = parent_x_total + elements[generic_context_menu_idx].x;
                int final_y_pos = parent_y_total + elements[generic_context_menu_idx].y;
                int total_menu_height = elements[generic_context_menu_idx].calculated_height + (20 * elements[generic_context_menu_idx].menu_items_count);
                
                // Adjust x position to stay within window bounds
                if (final_x_pos < 0) {
                    elements[generic_context_menu_idx].x = -parent_x_total; // position at left edge of window
                } else if (final_x_pos + elements[generic_context_menu_idx].calculated_width > window_width) {
                    elements[generic_context_menu_idx].x = window_width - parent_x_total - elements[generic_context_menu_idx].calculated_width; // position at right edge
                }
                
                // Adjust y position to stay within window bounds  
                if (final_y_pos + total_menu_height > window_height) {
                    // If menu would go below window, position it at the bottom
                    elements[generic_context_menu_idx].y = window_height - parent_y_total - total_menu_height;
                } else if (final_y_pos < 0) {
                    // If menu would go above window, position it at the top
                    elements[generic_context_menu_idx].y = -parent_y_total;
                }
                
                // Make sure the menu is within window bounds
                if (elements[generic_context_menu_idx].x + elements[generic_context_menu_idx].calculated_width > window_width) {
                    elements[generic_context_menu_idx].x = window_width - elements[generic_context_menu_idx].calculated_width;
                }
                if (elements[generic_context_menu_idx].y + elements[generic_context_menu_idx].calculated_height + 
                    20 * elements[generic_context_menu_idx].menu_items_count > window_height) {
                    elements[generic_context_menu_idx].y = window_height - elements[generic_context_menu_idx].calculated_height - 
                        20 * elements[generic_context_menu_idx].menu_items_count;
                }
                
                if (elements[generic_context_menu_idx].x < 0) elements[generic_context_menu_idx].x = 0;
                if (elements[generic_context_menu_idx].y < 0) elements[generic_context_menu_idx].y = 0;
                
                elements[generic_context_menu_idx].is_open = 1;
                printf("Generic context menu opened at position (%d, %d)\\n", x, y);
                glutPostRedisplay();
            } else {
                printf("Right-clicked at position (%d, %d) - no context_menu defined\\n", x, y);
            }
            
            }
        }
    }

void run_module_handler() {
    printf("Running module via IPC...\n");

    // 1. Read previous result to increment from, or use default value
    int start_value = 10;  // Default starting value
    FILE* prev_out_fp = fopen("output.csv", "r");
    if (prev_out_fp) {
        int prev_value = 0;
        if (fscanf(prev_out_fp, "%d", &prev_value) == 1) {
            start_value = prev_value;  // Start from previous result if exists
        }
        fclose(prev_out_fp);
    }

    // 2. Send the starting value to the module via IPC pipe
    char input_buffer[64];
    snprintf(input_buffer, sizeof(input_buffer), "%d", start_value);
    
    // Use the model's IPC mechanism instead of CSV files
    model_send_input(input_buffer);

    // Note: In a real implementation, we would wait for the response via update_model()
    // For now, we'll simulate the behavior by assuming the module responds immediately
    
    printf("Module command sent: %s\n", input_buffer);
    
    // Update UI to show that command was sent - find the result display textfield and update it by ID
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "textfield") == 0 && 
            strcmp(elements[i].id, "result_display") == 0) {
            snprintf(elements[i].text_content[0], MAX_LINE_LENGTH, "Processing...");
            elements[i].cursor_x = strlen(elements[i].text_content[0]);
            elements[i].cursor_y = 0;
            break; // Found and updated the result display
        }
    }
    
    // Also update the text element that displays the result by ID
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "text") == 0 && 
            strcmp(elements[i].id, "result_text") == 0) {
            snprintf(elements[i].label, sizeof(elements[i].label), "Result: Processing...");
            break;
        }
    }
    
    glutPostRedisplay(); // Redraw to show updated UI
    
    // The actual result will come through update_model() which is called by the idle function
}



void mouse_motion(int x, int y) {
    if (active_slider_index != -1) {
        UIElement* slider = &elements[active_slider_index];

        int parent_x = 0;
        int parent_y = 0;
        int current_parent = slider->parent;
        while (current_parent != -1) {
            parent_x += elements[current_parent].x;
            parent_y += elements[current_parent].y;
            current_parent = elements[current_parent].parent;
        }

        int abs_x = parent_x + slider->calculated_x;
        int abs_y = window_height - (parent_y + slider->calculated_y + slider->calculated_height); // Convert to OpenGL coordinates to match hit detection logic

        int new_value;
        // Check if this is a vertical slider by looking at the aspect ratio
        int is_vertical = (slider->calculated_height > slider->calculated_width);  // Use calculated dimensions, if height is greater than width, consider it vertical
        
        if (is_vertical) {
            // Calculate new slider value based on mouse y position for vertical slider
            float normalized_y = (float)(y - abs_y) / slider->calculated_height;
            // Invert the Y normalization since y=0 is at bottom in OpenGL coordinates
            normalized_y = 1.0f - normalized_y;
            if (normalized_y < 0.0f) normalized_y = 0.0f;
            if (normalized_y > 1.0f) normalized_y = 1.0f;

            new_value = slider->slider_min + normalized_y * (slider->slider_max - slider->slider_min);
        } else {
            // Calculate new slider value based on mouse x position for horizontal slider
            float normalized_x = (float)(x - abs_x) / slider->calculated_width;
            if (normalized_x < 0.0f) normalized_x = 0.0f;
            if (normalized_x > 1.0f) normalized_x = 1.0f;

            new_value = slider->slider_min + normalized_x * (slider->slider_max - slider->slider_min);
        }
        
        // Snap to step
        new_value = (new_value / slider->slider_step) * slider->slider_step;

        if (new_value < slider->slider_min) new_value = slider->slider_min;
        if (new_value > slider->slider_max) new_value = slider->slider_max;

        if (new_value != slider->slider_value) {
            slider->slider_value = new_value;
            printf("Slider '%s' value: %d\n", slider->label, slider->slider_value);
            
            // Send slider value to the module using the model_send_input function
            char slider_input[100];
            snprintf(slider_input, sizeof(slider_input), "SLIDER:%s:%d", slider->id, slider->slider_value);
            model_send_input(slider_input);
            
            glutPostRedisplay();
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    // Check for active textfield or textarea
    for (int i = 0; i < num_elements; i++) {
        if ((strcmp(elements[i].type, "textfield") == 0 || strcmp(elements[i].type, "textarea") == 0) && elements[i].is_active) {
            // Handle control keys (Ctrl+C, Ctrl+V, etc.)
            if (key < 32) {  // Control keys are below 32
                handle_ctrl_keys(key, i);
                glutPostRedisplay();
                return;
            }
            
            if (key == 8) { // Backspace
                if (elements[i].cursor_x > 0) {
                    // Move characters to the left to remove the character
                    for (int j = elements[i].cursor_x; j < (int)strlen(elements[i].text_content[elements[i].cursor_y]); j++) {
                        elements[i].text_content[elements[i].cursor_y][j-1] = elements[i].text_content[elements[i].cursor_y][j];
                    }
                    elements[i].text_content[elements[i].cursor_y][strlen(elements[i].text_content[elements[i].cursor_y]) - 1] = '\0';
                    elements[i].cursor_x--;
                } else if (elements[i].cursor_y > 0) {
                    // Join current line with the previous line
                    int prev_line_len = strlen(elements[i].text_content[elements[i].cursor_y - 1]);
                    int curr_line_len = strlen(elements[i].text_content[elements[i].cursor_y]);
                    if (prev_line_len + curr_line_len < MAX_LINE_LENGTH) {
                        strcat(elements[i].text_content[elements[i].cursor_y - 1], elements[i].text_content[elements[i].cursor_y]);
                        
                        // Shift all lines up by one position starting from current line
                        for (int j = elements[i].cursor_y; j < elements[i].num_lines - 1; j++) {
                            strcpy(elements[i].text_content[j], elements[i].text_content[j+1]);
                        }
                        strcpy(elements[i].text_content[elements[i].num_lines - 1], ""); // Clear last line
                        elements[i].num_lines--;
                        elements[i].cursor_y--;
                        elements[i].cursor_x = prev_line_len;
                    }
                }
            } else if (key == 13) { // Enter - create new line
                if (elements[i].num_lines < MAX_TEXT_LINES - 1) {
                    // Shift lines down from the current position
                    for (int j = elements[i].num_lines; j > elements[i].cursor_y; j--) {
                        strcpy(elements[i].text_content[j], elements[i].text_content[j-1]);
                    }
                    elements[i].num_lines++;
                    
                    // Move the text after cursor to new line
                    char temp_line[MAX_LINE_LENGTH];
                    strcpy(temp_line, elements[i].text_content[elements[i].cursor_y] + elements[i].cursor_x);
                    elements[i].text_content[elements[i].cursor_y][elements[i].cursor_x] = '\0';
                    
                    strcpy(elements[i].text_content[elements[i].cursor_y + 1], temp_line);
                    
                    elements[i].cursor_y++;
                    elements[i].cursor_x = 0;
                }
            } else if (key >= 32 && key <= 126) { // Printable characters
                // Insert character at cursor position
                int line_len = strlen(elements[i].text_content[elements[i].cursor_y]);
                if (line_len < MAX_LINE_LENGTH - 1 && elements[i].cursor_x <= line_len) {
                    // Shift characters to the right to make space
                    for (int j = line_len; j > elements[i].cursor_x; j--) {
                        elements[i].text_content[elements[i].cursor_y][j] = elements[i].text_content[elements[i].cursor_y][j-1];
                    }
                    elements[i].text_content[elements[i].cursor_y][elements[i].cursor_x] = key;
                    elements[i].text_content[elements[i].cursor_y][line_len + 1] = '\0';
                    elements[i].cursor_x++;
                }
            }
            glutPostRedisplay();
            return;
        }
    }

    // If no textfield is active or key is not handled by textfield, check for global keys
    if (key == 27) { // ESC
        exit(0);
    }
}

void special_keys(int key, int x, int y) {
    // Check for active textfield or textarea
    for (int i = 0; i < num_elements; i++) {
        if ((strcmp(elements[i].type, "textfield") == 0 || strcmp(elements[i].type, "textarea") == 0) && elements[i].is_active) {
            switch(key) {
                case GLUT_KEY_UP:
                    if (elements[i].cursor_y > 0) {
                        elements[i].cursor_y--;
                        if (elements[i].cursor_x > (int)strlen(elements[i].text_content[elements[i].cursor_y])) {
                            elements[i].cursor_x = strlen(elements[i].text_content[elements[i].cursor_y]);
                        }
                    }
                    break;
                case GLUT_KEY_DOWN:
                    if (elements[i].cursor_y < elements[i].num_lines - 1) {
                        elements[i].cursor_y++;
                        if (elements[i].cursor_x > (int)strlen(elements[i].text_content[elements[i].cursor_y])) {
                            elements[i].cursor_x = strlen(elements[i].text_content[elements[i].cursor_y]);
                        }
                    }
                    break;
                case GLUT_KEY_LEFT:
                    if (elements[i].cursor_x > 0) {
                        elements[i].cursor_x--;
                    } else if (elements[i].cursor_y > 0) {
                        elements[i].cursor_y--;
                        elements[i].cursor_x = strlen(elements[i].text_content[elements[i].cursor_y]);
                    }
                    break;
                case GLUT_KEY_RIGHT:
                    if (elements[i].cursor_x < (int)strlen(elements[i].text_content[elements[i].cursor_y])) {
                        elements[i].cursor_x++;
                    } else if (elements[i].cursor_y < elements[i].num_lines - 1) {
                        elements[i].cursor_y++;
                        elements[i].cursor_x = 0;
                    }
                    break;
            }
            glutPostRedisplay();
            return;
        }
    }
    

    // Handle arrow keys for the game module
    switch(key) {
        case GLUT_KEY_UP:
            model_send_input("UP");
            break;
        case GLUT_KEY_DOWN:
            model_send_input("DOWN");
            break;
        case GLUT_KEY_LEFT:
            model_send_input("LEFT");
            break;
        case GLUT_KEY_RIGHT:
            model_send_input("RIGHT");
            break;
    }
}
// Function to copy selected text to clipboard
void copy_text(int element_index) {
    UIElement* el = &elements[element_index];
    if (!el->has_selection) {
        // If no selection, copy the entire line
        strcpy(el->clipboard, el->text_content[el->cursor_y]);
        return;
    }

    // Calculate start and end positions correctly
    int start_y = el->selection_start_y;
    int end_y = el->selection_end_y;
    int start_x = el->selection_start_x;
    int end_x = el->selection_end_x;
    
    // Ensure start is before end
    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int temp = start_y;
        start_y = end_y;
        end_y = temp;
        temp = start_x;
        start_x = end_x;
        end_x = temp;
    }
    
    if (start_y == end_y) {
        // Same line selection
        int length = end_x - start_x;
        if (length > 0 && length < CLIPBOARD_SIZE) {
            strncpy(el->clipboard, el->text_content[start_y] + start_x, length);
            el->clipboard[length] = '\0';
        }
    } else {
        // Multi-line selection
        int pos = 0;
        for (int line = start_y; line <= end_y; line++) {
            int start_col = (line == start_y) ? start_x : 0;
            int end_col = (line == end_y) ? end_x : strlen(el->text_content[line]);
            
            for (int col = start_col; col < end_col && pos < CLIPBOARD_SIZE - 1; col++) {
                el->clipboard[pos++] = el->text_content[line][col];
            }
            
            // Add newline between lines (except for the last line)
            if (line < end_y && pos < CLIPBOARD_SIZE - 1) {
                el->clipboard[pos++] = '\n';
            }
        }
        el->clipboard[pos] = '\0';
    }
}

// Function to cut selected text
void cut_text(int element_index) {
    copy_text(element_index);
    delete_selection(element_index);
}

// Function to delete selected text
void delete_selection(int element_index) {
    UIElement* el = &elements[element_index];
    if (!el->has_selection) {
        return;
    }

    // Calculate start and end positions correctly
    int start_y = el->selection_start_y;
    int end_y = el->selection_end_y;
    int start_x = el->selection_start_x;
    int end_x = el->selection_end_x;
    
    // Ensure start is before end
    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int temp = start_y;
        start_y = end_y;
        end_y = temp;
        temp = start_x;
        start_x = end_x;
        end_x = temp;
    }
    
    if (start_y == end_y) {
        // Same line deletion
        int line_len = strlen(el->text_content[start_y]);
        for (int i = end_x; i <= line_len; i++) {
            el->text_content[start_y][start_x + i - end_x] = el->text_content[start_y][i];
        }
        el->cursor_x = start_x;
    } else {
        // Multi-line deletion
        int remaining_len = strlen(el->text_content[end_y]) - end_x;
        // Append end line content after end_x to the start line after start_x
        for (int i = 0; i <= remaining_len; i++) {
            el->text_content[start_y][start_x + i] = el->text_content[end_y][end_x + i];
        }
        
        // Shift lines up to remove the deleted lines
        for (int line = start_y + 1; line <= end_y; line++) {
            strcpy(el->text_content[line - (end_y - start_y)], el->text_content[line]);
        }
        
        // Update line count
        el->num_lines -= (end_y - start_y);
        
        // Update cursor position
        el->cursor_y = start_y;
        el->cursor_x = start_x;
    }
    
    // Clear selection
    el->has_selection = 0;
}

// Function to paste text from clipboard
void paste_text(int element_index) {
    UIElement* el = &elements[element_index];
    
    // If there's a selection, delete it first
    if (el->has_selection) {
        delete_selection(element_index);
    }
    
    int clipboard_len = strlen(el->clipboard);
    if (clipboard_len == 0) {
        return; // Nothing to paste
    }
    
    // Check if clipboard has newlines
    char* newline_ptr = strchr(el->clipboard, '\n');
    if (newline_ptr == NULL) {
        // Simple paste without newlines
        int line_len = strlen(el->text_content[el->cursor_y]);
        int paste_len = strlen(el->clipboard);
        
        // Make sure we don't exceed line length
        if (line_len + paste_len < MAX_LINE_LENGTH) {
            // Shift existing text to the right
            for (int i = line_len; i >= el->cursor_x; i--) {
                el->text_content[el->cursor_y][i + paste_len] = el->text_content[el->cursor_y][i];
            }
            
            // Insert clipboard text
            for (int i = 0; i < paste_len; i++) {
                el->text_content[el->cursor_y][el->cursor_x + i] = el->clipboard[i];
            }
            
            el->cursor_x += paste_len;
        }
    } else {
        // Pasting multi-line text - need to split and insert
        char *clipboard_copy = malloc(clipboard_len + 1);
        strcpy(clipboard_copy, el->clipboard);
        
        char *line = strtok(clipboard_copy, "\n");
        int first_line = 1;
        
        while (line != NULL) {
            if (first_line) {
                // Insert into current line at cursor position
                int line_len = strlen(el->text_content[el->cursor_y]);
                int insert_len = strlen(line);
                
                if (line_len + insert_len < MAX_LINE_LENGTH) {
                    // Shift existing text to the right
                    for (int i = line_len; i >= el->cursor_x; i--) {
                        el->text_content[el->cursor_y][i + insert_len] = el->text_content[el->cursor_y][i];
                    }
                    
                    // Insert the line text
                    for (int i = 0; i < insert_len; i++) {
                        el->text_content[el->cursor_y][el->cursor_x + i] = line[i];
                    }
                    
                    el->cursor_x += insert_len;
                }
                first_line = 0;
            } else {
                // Create new line after current line
                if (el->num_lines < MAX_TEXT_LINES - 1) {
                    // Shift lines down
                    for (int j = el->num_lines; j > el->cursor_y + 1; j--) {
                        strcpy(el->text_content[j], el->text_content[j-1]);
                    }
                    el->num_lines++;
                    
                    // Insert the new line
                    strcpy(el->text_content[el->cursor_y + 1], line);
                    
                    el->cursor_y++;
                    el->cursor_x = strlen(line);
                }
            }
            line = strtok(NULL, "\n");
        }
        
        free(clipboard_copy);
    }
}

// Function to handle special control keys for text editing
void handle_ctrl_keys(unsigned char key, int element_index) {
    if (key == 3) {  // Ctrl+C
        copy_text(element_index);
    } else if (key == 24) {  // Ctrl+X
        cut_text(element_index);
    } else if (key == 22) {  // Ctrl+V
        paste_text(element_index);
    }
}

// Function to update UI elements with current model variables
void update_ui_with_model_variables() {
    int var_count;
    ModelVariable* vars = model_get_variables(&var_count);
    
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (vars[i].active) {
            // Look for UI elements with IDs that match variable labels
            for (int j = 0; j < num_elements; j++) {
                // Check if this UI element's ID matches the variable label
                if (strcmp(elements[j].id, vars[i].label) == 0) {
                    // Update the appropriate field based on the element type
                    if (strcmp(elements[j].type, "text") == 0) {
                        // Update text element's value based on the variable content
                        if (vars[i].type == 1) { // int
                            snprintf(elements[j].label, sizeof(elements[j].label), "%d", vars[i].value.int_val);
                        } else if (vars[i].type == 2) { // float
                            snprintf(elements[j].label, sizeof(elements[j].label), "%.2f", vars[i].value.float_val);
                        } else if (vars[i].type == 3) { // string
                            strncpy(elements[j].label, vars[i].value.string_val, sizeof(elements[j].label) - 1);
                            elements[j].label[sizeof(elements[j].label) - 1] = '\0'; // Ensure null termination
                        }
                    } else if (strcmp(elements[j].type, "textfield") == 0 || 
                               strcmp(elements[j].type, "textarea") == 0) {
                        // Update textfield's first content line
                        if (vars[i].type == 1) { // int
                            snprintf(elements[j].text_content[0], sizeof(elements[j].text_content[0]), "%d", vars[i].value.int_val);
                        } else if (vars[i].type == 2) { // float
                            snprintf(elements[j].text_content[0], sizeof(elements[j].text_content[0]), "%.2f", vars[i].value.float_val);
                        } else if (vars[i].type == 3) { // string
                            strncpy(elements[j].text_content[0], vars[i].value.string_val, sizeof(elements[j].text_content[0]) - 1);
                            elements[j].text_content[0][sizeof(elements[j].text_content[0]) - 1] = '\0';
                        }
                        elements[j].cursor_x = strlen(elements[j].text_content[0]);
                        elements[j].cursor_y = 0;
                    }
                }
            }
        }
    }
}

// Simplified DOM Matrix - Array of UI elements with dynamic event handling
// Each element is represented as [index, type, id, onClick, parent_index, ...]

// Function to get element by ID from our simplified DOM matrix
int get_element_index_by_id(const char* id) {
    if (!id) return -1;
    
    for (int i = 0; i < num_elements; i++) {
        if (elements[i].id[0] != '\0' && strcmp(elements[i].id, id) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

// Function to get element by tag name (first match)
int get_element_index_by_tag_name(const char* tag_name) {
    if (!tag_name) return -1;
    
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, tag_name) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

// Function to get all elements by tag name
int get_elements_by_tag_name(const char* tag_name, int* results, int max_results) {
    if (!tag_name || !results) return 0;
    
    int count = 0;
    for (int i = 0; i < num_elements && count < max_results; i++) {
        if (strcmp(elements[i].type, tag_name) == 0) {
            results[count] = i;
            count++;
        }
    }
    return count;
}

// Function to get elements by class name (using label as class for now)
int get_elements_by_class_name(const char* class_name, int* results, int max_results) {
    if (!class_name || !results) return 0;
    
    int count = 0;
    for (int i = 0; i < num_elements && count < max_results; i++) {
        if (strcmp(elements[i].label, class_name) == 0) {  // Using label as class for now
            results[count] = i;
            count++;
        }
    }
    return count;
}

// Function to find elements using a simple selector (id, tag, or basic class)
int query_selector_all(const char* selector, int* results, int max_results) {
    if (!selector || !results) return 0;
    
    int count = 0;
    
    // Check if selector is an ID selector (#id)
    if (selector[0] == '#') {
        int index = get_element_index_by_id(selector + 1);  // Skip '#'
        if (index != -1 && count < max_results) {
            results[count] = index;
            count++;
        }
    }
    // Check if selector is a class selector (.class)
    else if (selector[0] == '.') {
        count = get_elements_by_class_name(selector + 1, results, max_results);  // Skip '.'
    }
    // Otherwise treat as tag name selector
    else {
        count = get_elements_by_tag_name(selector, results, max_results);
    }
    
    return count;
}

// Function to find the first element matching a selector
int query_selector(const char* selector) {
    int results[1];
    int count = query_selector_all(selector, results, 1);
    return (count > 0) ? results[0] : -1;
}

// Function to dynamically execute handler by name
void execute_handler_by_name(const char* handler_name) {
    if (!handler_name) return;
    
    // Check if this is a createWindow call by looking for the pattern
    if (strncmp(handler_name, "createWindow(", 13) == 0) {
        // More robust parsing for createWindow('title', 'chtmlFile', width, height)
        // Extract the content between parentheses
        const char* start = strchr(handler_name, '(');
        if (start != NULL) {
            start++; // Move past the '('
            const char* end = strrchr(handler_name, ')');
            if (end != NULL) {
                char params_str[512];
                int len = end - start;
                if (len >= sizeof(params_str)) len = sizeof(params_str) - 1;
                strncpy(params_str, start, len);
                params_str[len] = '\0';
                
                // Manually parse the parameters (handling quoted strings properly)
                char title[256] = {0};
                char chtmlPath[256] = {0};
                int width = 400, height = 300; // Default values
                
                // Find the first quoted string (title)
                const char* title_start = strchr(params_str, '\'');
                if (title_start != NULL) {
                    title_start++; // Move past first quote
                    const char* title_end = strchr(title_start, '\'');
                    if (title_end != NULL) {
                        int title_len = title_end - title_start;
                        if (title_len >= sizeof(title)) title_len = sizeof(title) - 1;
                        strncpy(title, title_start, title_len);
                        title[title_len] = '\0';
                        
                        // Find the second quoted string (chtmlPath)
                        const char* path_start = strchr(title_end + 1, '\'');
                        if (path_start != NULL) {
                            path_start++; // Move past quote
                            const char* path_end = strchr(path_start, '\'');
                            if (path_end != NULL) {
                                int path_len = path_end - path_start;
                                if (path_len >= sizeof(chtmlPath)) path_len = sizeof(chtmlPath) - 1;
                                strncpy(chtmlPath, path_start, path_len);
                                chtmlPath[path_len] = '\0';
                                
                                // Find the width and height after the closing quotes
                                const char* after_path = path_end + 1;
                                // Skip comma and whitespace
                                while (*after_path && (*after_path == ',' || *after_path == ' ')) after_path++;
                                
                                // Now parse the numbers
                                int parsed = sscanf(after_path, "%d, %d", &width, &height);
                                if (parsed == 2) {
                                    printf("Creating window with params: '%s', '%s', %d, %d\n", title, chtmlPath, width, height);
                                    createWindow(title, chtmlPath, width, height);
                                    return; // Successfully handled
                                }
                            }
                        }
                    }
                }
                
                printf("Error: Could not parse createWindow parameters: %s\n", handler_name);
            }
        }
        return;
    }
    
    // Log that a handler was called - this shows it's being processed dynamically
    printf("Event handler called: %s\n", handler_name);
    
    // Pass the handler name to the module for processing, like JavaScript events
    // This is the key change - instead of hardcoded logic, we pass all events to the module
    char module_input[256];
    snprintf(module_input, sizeof(module_input), "EVENT:%s", handler_name);
    model_send_input(module_input);
    
    // For specific UI operations that affect the view (like view mode changes), handle them
    if (strcmp(handler_name, "switch_to_2d_handler") == 0) {
        switch_to_2d_view();
    } else if (strcmp(handler_name, "switch_to_3d_handler") == 0) {
        switch_to_3d_view();
    }
    // Handle video-specific events
    else if (strcmp(handler_name, "play_video_clicked") == 0) {
        // Find the video element and start playing
        for (int i = 0; i < num_elements; i++) {
            if (strcmp(elements[i].type, "video") == 0 && strlen(elements[i].video_src) > 0) {
                play_video(elements[i].video_src, 0, elements[i].video_loop);
                
                // Update status text if there's an element with id "video_status"
                for (int j = 0; j < num_elements; j++) {
                    if (strcmp(elements[j].type, "text") == 0 && strcmp(elements[j].id, "video_status") == 0) {
                        strcpy(elements[j].label, "Video Status: Playing");
                        break;
                    }
                }
                break;
            }
        }
    }
    else if (strcmp(handler_name, "pause_video_clicked") == 0) {
        // Find the video element and pause playback
        for (int i = 0; i < num_elements; i++) {
            if (strcmp(elements[i].type, "video") == 0 && strlen(elements[i].video_src) > 0) {
                if (is_video_playing(elements[i].video_src)) {
                    pause_video(elements[i].video_src);
                    
                    // Update status text if there's an element with id "video_status"
                    for (int j = 0; j < num_elements; j++) {
                        if (strcmp(elements[j].type, "text") == 0 && strcmp(elements[j].id, "video_status") == 0) {
                            strcpy(elements[j].label, "Video Status: Paused");
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    else if (strcmp(handler_name, "stop_video_clicked") == 0) {
        // Find the video element and stop playback
        for (int i = 0; i < num_elements; i++) {
            if (strcmp(elements[i].type, "video") == 0 && strlen(elements[i].video_src) > 0) {
                stop_video(elements[i].video_src);
                
                // Update status text if there's an element with id "video_status"
                for (int j = 0; j < num_elements; j++) {
                    if (strcmp(elements[j].type, "text") == 0 && strcmp(elements[j].id, "video_status") == 0) {
                        strcpy(elements[j].label, "Video Status: Stopped");
                        break;
                    }
                }
                break;
            }
        }
    }
    else if (strcmp(handler_name, "exit_app") == 0) {
        exit(0);
    }
    // All other event handling is delegated to the module
}

// Function to output our simplified DOM matrix to CSV for debugging
void output_dom_to_csv() {
    FILE* csv_file = fopen("dom_matrix.csv", "w");
    if (!csv_file) {
        printf("Could not create DOM CSV file for debugging.\n");
        return;
    }
    
    fprintf(csv_file, "index,type,id,onClick,parent_index,x,y,width_value,width_unit,height_value,height_unit\n");
    for (int i = 0; i < num_elements; i++) {
        fprintf(csv_file, "%d,%s,%s,%s,%d,%d,%d,%f,%d,%f,%d\n", 
                i, 
                elements[i].type, 
                elements[i].id, 
                elements[i].onClick, 
                elements[i].parent,
                elements[i].x,
                elements[i].y,
                elements[i].width.value,
                elements[i].width.unit,
                elements[i].height.value,
                elements[i].height.unit);
    }
    
    fclose(csv_file);
    printf("DOM matrix output to dom_matrix.csv for debugging.\n");
}