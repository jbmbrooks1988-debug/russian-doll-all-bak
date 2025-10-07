#include <GL/glut.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

// Header strip height (same as in view_gl]3d]a0.c and model_gl.c)
#define HEADER_STRIP_HEIGHT 40

// Text input dialog structure
typedef struct {
    int x, y;           // Position of the dialog
    int width, height;  // Dimensions of the dialog
    bool visible;       // Whether the dialog is visible
    char input_buffer[512];  // Buffer for text input
    int input_cursor;   // Current cursor position
    int input_length;   // Length of the input text
    bool confirmed;     // Whether the user confirmed the input
} TextInputDialog;

// External declaration for current_project_path
extern char current_project_path[256];

// External declaration for current_map_file
extern char current_map_file[256];

// External declarations for text input dialog functions
extern TextInputDialog* text_input_dialog;
extern void handle_text_input_dialog_key(TextInputDialog* dialog, unsigned char key);
extern void handle_text_input_dialog_special(TextInputDialog* dialog, int key);
extern void save_event_text_to_file(const char* text);

// External declarations for scale functions
extern float get_scale_x(void);
extern float get_scale_y(void);
extern float get_scale_z(void);
extern void set_scale_x(float scale);
extern void set_scale_y(float scale);
extern void set_scale_z(float scale);

// External declarations for thumb picker functions
extern float emoji_thumb_y;
extern float emoji_thumb_height;
extern int emoji_thumb_dragging;
extern int emoji_start_index;
extern int emoji_visible_rows;
extern void update_emoji_index_from_thumb(int total_emojis);
extern void update_thumb_from_emoji_index(int total_emojis);
extern float mouse_y_to_emoji_thumb_y(int y);
extern int get_clicked_emoji_index(int x, int y, float sidebar_x, float base_y, int total_emojis);

// External declarations for other model functions
extern int get_view_mode(void);
extern int get_current_z_level(void);
extern int get_start_row(void);
extern int get_start_col(void);
extern int get_selector_row(void);
extern int get_selector_col(void);
extern int get_selected_emoji(void);
extern int get_selected_fg_color(void);
extern int get_selected_bg_color(void);
extern int get_canvas_rows(void);
extern int get_canvas_cols(void);
extern int get_tile_size(void);
extern int get_sidebar_width(void);
extern int get_window_height(void);
extern int get_canvas_tile(int layer, int r, int c, int field);
extern void set_canvas_tile(int layer, int r, int c, int field, int value);
extern void set_selector_row(int value);
extern void set_selector_col(int value);
extern void set_start_row(int value);
extern void set_start_col(int value);
extern void set_selected_tool(int value);
extern void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg);
extern void draw_rectangle(int layer, int r1, int c1, int r2, int c2);
extern void erase_tile(int layer, int r, int c);
extern void erase_rectangle(int layer, int r1, int c1, int r2, int c2);
extern void erase_fill(int layer, int r, int c);
extern void print_ascii_grid(void);
extern void trigger_view_update(void);
extern void set_status_message(const char* msg);

// Function declarations for build and run
void build_and_run_player(void);
int copy_directory(const char* src, const char* dst);
int create_directories(const char* path);

// Input event structure for event-driven input system
typedef enum {
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_MOUSE_CLICK,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_TERMINAL_INPUT
} InputEventType;

// Using array-based approach for InputEvent instead of struct with union
#define INPUT_EVENT_TYPE_FIELD 0
#define INPUT_EVENT_KEY_FIELD 1
#define INPUT_EVENT_KEY_X_FIELD 2
#define INPUT_EVENT_KEY_Y_FIELD 3
#define INPUT_EVENT_KEY_MOD_FIELD 4
#define INPUT_EVENT_MOUSE_BUTTON_FIELD 1
#define INPUT_EVENT_MOUSE_STATE_FIELD 2
#define INPUT_EVENT_MOUSE_X_FIELD 3
#define INPUT_EVENT_MOUSE_Y_FIELD 4
#define INPUT_EVENT_TERMINAL_CH_FIELD 1

#define INPUT_EVENT_FIELDS 5
#define INPUT_EVENT_SIZE (INPUT_EVENT_FIELDS * sizeof(int))

// Helper functions to access input event fields
InputEventType get_input_event_type(int* event) {
    return (InputEventType)event[INPUT_EVENT_TYPE_FIELD];
}

unsigned char get_input_event_key(int* event) {
    return (unsigned char)event[INPUT_EVENT_KEY_FIELD];
}

int get_input_event_key_x(int* event) {
    return event[INPUT_EVENT_KEY_X_FIELD];
}

int get_input_event_key_y(int* event) {
    return event[INPUT_EVENT_KEY_Y_FIELD];
}

int get_input_event_key_modifiers(int* event) {
    return event[INPUT_EVENT_KEY_MOD_FIELD];
}

int get_input_event_mouse_button(int* event) {
    return event[INPUT_EVENT_MOUSE_BUTTON_FIELD];
}

int get_input_event_mouse_state(int* event) {
    return event[INPUT_EVENT_MOUSE_STATE_FIELD];
}

int get_input_event_mouse_x(int* event) {
    return event[INPUT_EVENT_MOUSE_X_FIELD];
}

int get_input_event_mouse_y(int* event) {
    return event[INPUT_EVENT_MOUSE_Y_FIELD];
}

int get_input_event_terminal_ch(int* event) {
    return event[INPUT_EVENT_TERMINAL_CH_FIELD];
}

void set_input_event_type(int* event, InputEventType type) {
    event[INPUT_EVENT_TYPE_FIELD] = (int)type;
}

void set_input_event_key(int* event, unsigned char key) {
    event[INPUT_EVENT_KEY_FIELD] = (int)key;
}

void set_input_event_key_x(int* event, int x) {
    event[INPUT_EVENT_KEY_X_FIELD] = x;
}

void set_input_event_key_y(int* event, int y) {
    event[INPUT_EVENT_KEY_Y_FIELD] = y;
}

void set_input_event_key_modifiers(int* event, int modifiers) {
    event[INPUT_EVENT_KEY_MOD_FIELD] = modifiers;
}

void set_input_event_mouse_button(int* event, int button) {
    event[INPUT_EVENT_MOUSE_BUTTON_FIELD] = button;
}

void set_input_event_mouse_state(int* event, int state) {
    event[INPUT_EVENT_MOUSE_STATE_FIELD] = state;
}

void set_input_event_mouse_x(int* event, int x) {
    event[INPUT_EVENT_MOUSE_X_FIELD] = x;
}

void set_input_event_mouse_y(int* event, int y) {
    event[INPUT_EVENT_MOUSE_Y_FIELD] = y;
}

void set_input_event_terminal_ch(int* event, int ch) {
    event[INPUT_EVENT_TERMINAL_CH_FIELD] = ch;
}

#define MAX_EVENT_QUEUE_SIZE 256
static int event_queue[MAX_EVENT_QUEUE_SIZE * INPUT_EVENT_FIELDS];
static int event_queue_head = 0;
static int event_queue_tail = 0;
static int event_queue_count = 0;

// Event menu item structure - refactored to use arrays for beginner readability
#define EVENT_MENU_ITEM_LABEL_SIZE 64
#define EVENT_MENU_ITEM_FIELDS 4

// Helper functions to access event menu item fields
char* get_event_menu_item_label(void* item);
int get_event_menu_item_id(void* item);
bool get_event_menu_item_is_category(void* item);
int get_event_menu_item_page(void* item);
void set_event_menu_item_label(void* item, const char* label);
void set_event_menu_item_id(void* item, int id);
void set_event_menu_item_is_category(void* item, bool is_category);
void set_event_menu_item_page(void* item, int page);

// Event menu structure - keeping as struct since it's more complex
typedef struct {
    int x, y;           // Position of the menu
    int width, height;  // Dimensions of the menu
    int item_count;
    void* items;        // Pointer to array of menu items (using byte array instead of struct)
    int selected_item;
    bool visible;
    int parent_x, parent_y;  // Position of parent menu for positioning
    int type;  // 0 = main menu, 1 = event area, 2 = commands menu
    int current_page; // For tabbed menus
} EventMenu;

// Event menu function declarations
EventMenu* create_main_event_menu(int parent_x, int parent_y);
EventMenu* create_event_area_menu(int parent_x, int parent_y);
EventMenu* create_event_commands_menu(int parent_x, int parent_y);
EventMenu* create_contents_context_menu(int parent_x, int parent_y);
EventMenu* create_maps_context_menu(int parent_x, int parent_y);
void destroy_event_menu(EventMenu* menu);
void render_event_menu(EventMenu* menu);
void handle_event_menu_click(EventMenu* menu, int x, int y);
void show_event_menu(EventMenu* menu);
void hide_event_menu(EventMenu* menu);

// File reading functions
int read_event_commands_file(const char* filename, void* items, int max_items, int page);

// Map file functions
int scan_map_files(const char* project_path);
void free_map_files_list(void);
char** get_map_files_list(int* count);

// Global event menu variables
EventMenu* main_event_menu = NULL;
EventMenu* event_area_menu = NULL;
EventMenu* event_commands_menu = NULL;
EventMenu* contents_context_menu = NULL;
EventMenu* maps_context_menu = NULL;

// File menu variables - using arrays for beginner-friendly approach
static int file_menu_x = 10;
static int file_menu_y = 0;  // Will be set dynamically
static int file_menu_width = 60;
static int file_menu_height = 30;
static int file_menu_visible = 0;
static int file_menu_state = 0;  // 0 = hidden, 1 = main menu, 2 = new project, 3 = load project, 4 = save project
static char** project_list = NULL;
static int project_count = 0;
static int selected_project = -1;

// Input mode for getting user input
static int input_mode = 0;  // 0 = normal, 1 = getting project name
static char input_buffer[256] = "";
static int input_cursor = 0;

// Map file information structure
#define MAX_MAP_FILES 100
static char** map_files_list = NULL;
static int map_files_count = 0;
static int map_files_capacity = 0;

// Input binding structure for configurable inputs
typedef enum {
    INPUT_ACTION_NONE,
    INPUT_ACTION_PAINT,
    INPUT_ACTION_FILL,
    INPUT_ACTION_RECTANGLE,
    INPUT_ACTION_NEXT_EMOJI,
    INPUT_ACTION_NEXT_COLOR,
    INPUT_ACTION_TOGGLE_LAYERS,
    INPUT_ACTION_SAVE,
    INPUT_ACTION_LOAD,
    INPUT_ACTION_CREATE_TAB,
    INPUT_ACTION_CAMERA_FORWARD,
    INPUT_ACTION_CAMERA_BACKWARD,
    INPUT_ACTION_CAMERA_STRAFE_LEFT,
    INPUT_ACTION_CAMERA_STRAFE_RIGHT,
    INPUT_ACTION_CAMERA_ELEVATE_UP,
    INPUT_ACTION_CAMERA_ELEVATE_DOWN,
    INPUT_ACTION_DECREASE_Z_LEVEL,
    INPUT_ACTION_INCREASE_Z_LEVEL
} InputAction;

// Using array-based approach for InputBinding instead of struct
#define INPUT_BINDING_KEY_FIELD 0
#define INPUT_BINDING_MODIFIERS_FIELD 1
#define INPUT_BINDING_ACTION_FIELD 2
#define INPUT_BINDING_DESCRIPTION_PTR_FIELD 3

#define INPUT_BINDING_FIELDS 4
#define INPUT_BINDING_SIZE (INPUT_BINDING_FIELDS * sizeof(int))

// Helper functions to access input binding fields
unsigned char get_input_binding_key(int* binding) {
    return (unsigned char)binding[INPUT_BINDING_KEY_FIELD];
}

int get_input_binding_modifiers(int* binding) {
    return binding[INPUT_BINDING_MODIFIERS_FIELD];
}

InputAction get_input_binding_action(int* binding) {
    return (InputAction)binding[INPUT_BINDING_ACTION_FIELD];
}

char* get_input_binding_description(int* binding) {
    return (char*)(intptr_t)binding[INPUT_BINDING_DESCRIPTION_PTR_FIELD];
}

void set_input_binding_key(int* binding, unsigned char key) {
    binding[INPUT_BINDING_KEY_FIELD] = (int)key;
}

void set_input_binding_modifiers(int* binding, int modifiers) {
    binding[INPUT_BINDING_MODIFIERS_FIELD] = modifiers;
}

void set_input_binding_action(int* binding, InputAction action) {
    binding[INPUT_BINDING_ACTION_FIELD] = (int)action;
}

void set_input_binding_description(int* binding, const char* description) {
    // For simplicity, we'll just store the pointer
    // In a more robust implementation, we might want to copy the string
    binding[INPUT_BINDING_DESCRIPTION_PTR_FIELD] = (int)(intptr_t)description;
}

// Global list of input bindings
static int* input_bindings = NULL;
static int input_binding_count = 0;
static int input_binding_capacity = 0;

// Function to initialize default input bindings
void init_input_bindings(void);

// Function to add an input binding
void add_input_binding(unsigned char key, int modifiers, InputAction action, const char* description);

// Function to find an input binding
int* find_input_binding(unsigned char key, int modifiers);

// Function to handle an input action
void handle_input_action(InputAction action);

// Helper function to handle paint tool action
void handle_paint_tool_action(void);

// Helper function to handle fill tool action
void handle_fill_tool_action(void);

// Helper function to handle rectangle tool action
void handle_rectangle_tool_action(void);

// Helper function to handle erase tool action
void handle_erase_tool_action(void);

// Helper function to handle erase fill tool action
void handle_erase_fill_tool_action(void);

// Function to set the selected tool in the model
void set_selected_tool(int value);

// Helper function to handle space key action
void handle_space_key(void);

// Helper function to handle enter key action
void handle_enter_key(void);

// Helper function to handle tool selection keys
void handle_tool_selection_keys(unsigned char key);

// Helper function to handle emoji and color selection keys
void handle_emoji_color_selection_keys(unsigned char key);

// Helper function to handle layer and view keys
void handle_layer_view_keys(unsigned char key);

// Helper function to handle file operations keys
void handle_file_operation_keys(unsigned char key);

// Helper function to handle camera control keys
void handle_camera_control_keys(unsigned char key);

// Helper function to handle tab creation key
void handle_tab_creation_key(void);

// Function to initialize input bindings
void init_input_bindings(void);

// Event queue management functions
void enqueue_key_event(unsigned char key, int x, int y, int modifiers);
void enqueue_mouse_event(int button, int state, int x, int y);
void enqueue_terminal_event(int ch);
int dequeue_event(int* event);
int is_event_queue_empty(void);

// Model function declarations
void set_canvas_rows(int value);
void set_canvas_cols(int value);
void set_tile_size(int value);
void set_sidebar_width(int value);
void set_window_width(int value);
void set_window_height(int value);
void set_status_message(const char* msg);
void set_selected_emoji(int value);
void set_selected_fg_color(int value);
void set_selected_bg_color(int value);
void set_start_row(int value);
void set_start_col(int value);
void set_selector_row(int value);
void set_selector_col(int value);
void set_show_all_layers(bool value);
void set_canvas_tile(int layer, int r, int c, int field, int value);
void set_tab_bank(int idx, int field, int value);
void increment_tab_count(void);
void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg);
void draw_rectangle(int layer, int r1, int c1, int r2, int c2);

// File I/O function declarations
int save_game_state(const char* filename);
int load_game_state(const char* filename);
void save_canvas(void);
void load_canvas(void);
void erase_tile(int layer, int r, int c);
void erase_rectangle(int layer, int r1, int c1, int r2, int c2);
void erase_fill(int layer, int r, int c);
void rotate_camera_yaw(float degrees);
void rotate_camera_pitch(float degrees);
void reset_camera(void);
void save_canvas(void);
void load_canvas(void);
void print_ascii_grid(void);
void set_view_mode(int mode);
void set_current_z_level(int z);
void set_camera_x(float x);
void set_camera_y(float y);
void set_camera_z(float z);
void set_camera_yaw(float yaw);
void set_camera_pitch(float pitch);
void move_camera_forward(float distance);
void move_camera_backward(float distance);
void strafe_camera(float distance);
void elevate_camera(float distance);

// File menu function declarations
void show_file_menu(void);
void hide_file_menu(void);
void render_file_menu(void);
void handle_file_menu_click(int x, int y);
int create_new_project(const char* project_name);
int save_project(const char* project_path);
int load_project(const char* project_path);
char** list_projects(int* count);
void free_project_list(void);
int rename_map_file(const char* old_name, const char* new_name);

// Raycasting function declarations
void screen_to_world_ray(int screen_x, int screen_y, float* ray_origin, float* ray_direction);
int ray_intersects_box(float ray_origin[3], float ray_direction[3], 
                      float box_min[3], float box_max[3], float* distance);
int find_closest_intersected_block(int screen_x, int screen_y, int* out_row, int* out_col, int* out_layer);

// Mouse position functions
int get_mouse_x(void);
int get_mouse_y(void);
bool get_mouse_in_window(void);
void set_mouse_x(int x);
void set_mouse_y(int y);
void set_mouse_in_window(bool in_window);

// Model function declarations
int get_view_mode(void);
int get_selector_row(void);
int get_selector_col(void);
int get_current_z_level(void);
int get_start_row(void);
int get_start_col(void);
int get_selected_emoji(void);
int get_selected_fg_color(void);
int get_selected_bg_color(void);
int get_canvas_tile(int layer, int r, int c, int field);
void set_selector_row(int value);
void set_selector_col(int value);
void set_canvas_tile(int layer, int r, int c, int field, int value);
void trigger_view_update(void);
void set_start_row(int value);
void set_start_col(int value);
void set_status_message(const char* msg);
void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg);
void draw_rectangle(int layer, int r1, int c1, int r2, int c2);
void erase_tile(int layer, int r, int c);
void erase_rectangle(int layer, int r1, int c1, int r2, int c2);
void erase_fill(int layer, int r, int c);

// Texture cache functions
void init_emoji_texture_cache(void);
void cleanup_emoji_texture_cache(void);

// Model getter declarations
int get_canvas_rows(void);
int get_canvas_cols(void);
int get_tile_size(void);
int get_num_emojis(void);
int get_num_colors(void);
int get_max_layers(void);
int get_max_tabs(void);
int get_sidebar_width(void);
int get_file_tab_height(void);
int get_window_width(void);
int get_window_height(void);
int get_tab_count(void);
const char* get_emoji(int idx);
const char* get_color_name(int idx);
const char* get_status_message(void);
int get_selected_emoji(void);
int get_selected_fg_color(void);
int get_selected_bg_color(void);
int get_start_row(void);
int get_start_col(void);
int get_selector_row(void);
int get_selector_col(void);
bool get_show_all_layers(void);
int get_canvas_tile(int layer, int r, int c, int field);
int get_tab_bank(int idx, int field);
int get_view_mode(void);
int get_current_z_level(void);
float get_camera_x(void);
float get_camera_y(void);
float get_camera_z(void);
float get_camera_yaw(void);
float get_camera_pitch(void);

// View function declarations
void display(void);
void reshape(int w, int h);
void print_ascii_grid(void);

// External function from main.c
extern void restore_terminal(void);

// State machine for tool modes
typedef enum {
    TOOL_STATE_PAINT,
    TOOL_STATE_FILL,
    TOOL_STATE_RECTANGLE,
    TOOL_STATE_ERASE,
    TOOL_STATE_ERASE_FILL
} ToolState;

static ToolState current_tool_state = TOOL_STATE_PAINT;

void set_tool_state(ToolState state) {
    current_tool_state = state;
    // Reset any tool-specific state, like the start of a rectangle
    set_start_row(-1);
    set_start_col(-1);

    switch (state) {
        case TOOL_STATE_PAINT:
            set_status_message("Paint tool selected");
            set_selected_tool(0);
            break;
        case TOOL_STATE_FILL:
            set_status_message("Fill tool selected");
            set_selected_tool(1);
            break;
        case TOOL_STATE_RECTANGLE:
            set_status_message("Rectangle tool selected");
            set_selected_tool(2);
            break;
        case TOOL_STATE_ERASE:
            set_status_message("Erase tool selected");
            set_selected_tool(3);
            break;
        case TOOL_STATE_ERASE_FILL:
            set_status_message("Erase fill mode selected");
            set_selected_tool(4);
            break;
    }
}

// View update callback function
void view_update_callback(void) {
    glutPostRedisplay();
}

int check_terminal_input() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    fd_set set;
    struct timeval timeout;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int ready = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);
    int ch = -1;
    if (ready > 0) {
        ch = getchar();
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

// Input binding management functions
void init_input_bindings(void) {
    // Initialize with default capacity
    input_binding_capacity = 50;
    input_bindings = (int*)malloc(input_binding_capacity * INPUT_BINDING_SIZE);
    input_binding_count = 0;
    
    if (!input_bindings) {
        fprintf(stderr, "Error: Failed to allocate memory for input bindings\n");
        input_binding_capacity = 0;
        return;
    }
    
    // Add default input bindings
    int* binding0 = &input_bindings[0 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding0, ' ');
    set_input_binding_modifiers(binding0, 0);
    set_input_binding_action(binding0, INPUT_ACTION_PAINT);
    set_input_binding_description(binding0, "Paint tile");
    
    int* binding1 = &input_bindings[1 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding1, '\r');
    set_input_binding_modifiers(binding1, 0);
    set_input_binding_action(binding1, INPUT_ACTION_PAINT);
    set_input_binding_description(binding1, "Paint tile");
    
    int* binding2 = &input_bindings[2 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding2, 'f');
    set_input_binding_modifiers(binding2, 0);
    set_input_binding_action(binding2, INPUT_ACTION_FILL);
    set_input_binding_description(binding2, "Fill tool");
    
    int* binding3 = &input_bindings[3 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding3, '4');
    set_input_binding_modifiers(binding3, 0);
    set_input_binding_action(binding3, INPUT_ACTION_RECTANGLE);
    set_input_binding_description(binding3, "Rectangle tool");
    
    int* binding4 = &input_bindings[4 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding4, '0');
    set_input_binding_modifiers(binding4, 0);
    set_input_binding_action(binding4, INPUT_ACTION_NONE);
    set_input_binding_description(binding4, "Erase tool"); // Use 0 for erase tool
    
    int* binding5 = &input_bindings[5 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding5, '1');
    set_input_binding_modifiers(binding5, 0);
    set_input_binding_action(binding5, INPUT_ACTION_NEXT_COLOR);
    set_input_binding_description(binding5, "Next color");
    
    int* binding6 = &input_bindings[6 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding6, '2');
    set_input_binding_modifiers(binding6, 0);
    set_input_binding_action(binding6, INPUT_ACTION_TOGGLE_LAYERS);
    set_input_binding_description(binding6, "Toggle layers");
    
    int* binding7 = &input_bindings[7 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding7, '3');
    set_input_binding_modifiers(binding7, 0);
    set_input_binding_action(binding7, INPUT_ACTION_NEXT_COLOR);
    set_input_binding_description(binding7, "Next color");
    
    int* binding8 = &input_bindings[8 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding8, 's');
    set_input_binding_modifiers(binding8, GLUT_ACTIVE_CTRL);
    set_input_binding_action(binding8, INPUT_ACTION_SAVE);
    set_input_binding_description(binding8, "Save canvas");
    
    int* binding9 = &input_bindings[9 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding9, 'l');
    set_input_binding_modifiers(binding9, 0);
    set_input_binding_action(binding9, INPUT_ACTION_LOAD);
    set_input_binding_description(binding9, "Load canvas");
    
    int* binding10 = &input_bindings[10 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding10, '5');
    set_input_binding_modifiers(binding10, 0);
    set_input_binding_action(binding10, INPUT_ACTION_CREATE_TAB);
    set_input_binding_description(binding10, "Create tab");
    
    int* binding11 = &input_bindings[11 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding11, 'w');
    set_input_binding_modifiers(binding11, 0);
    set_input_binding_action(binding11, INPUT_ACTION_CAMERA_FORWARD);
    set_input_binding_description(binding11, "Camera forward");
    
    int* binding12 = &input_bindings[12 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding12, 's');
    set_input_binding_modifiers(binding12, 0);
    set_input_binding_action(binding12, INPUT_ACTION_CAMERA_BACKWARD);
    set_input_binding_description(binding12, "Camera backward");
    
    int* binding13 = &input_bindings[13 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding13, 'a');
    set_input_binding_modifiers(binding13, 0);
    set_input_binding_action(binding13, INPUT_ACTION_CAMERA_STRAFE_LEFT);
    set_input_binding_description(binding13, "Camera strafe left");
    
    int* binding14 = &input_bindings[14 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding14, 'd');
    set_input_binding_modifiers(binding14, 0);
    set_input_binding_action(binding14, INPUT_ACTION_CAMERA_STRAFE_RIGHT);
    set_input_binding_description(binding14, "Camera strafe right");
    
    int* binding15 = &input_bindings[15 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding15, 'z');
    set_input_binding_modifiers(binding15, 0);
    set_input_binding_action(binding15, INPUT_ACTION_CAMERA_ELEVATE_UP);
    set_input_binding_description(binding15, "Camera elevate up");
    
    int* binding16 = &input_bindings[16 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding16, 'x');
    set_input_binding_modifiers(binding16, 0);
    set_input_binding_action(binding16, INPUT_ACTION_CAMERA_ELEVATE_DOWN);
    set_input_binding_description(binding16, "Camera elevate down");
    
    int* binding17 = &input_bindings[17 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding17, 'c');
    set_input_binding_modifiers(binding17, 0);
    set_input_binding_action(binding17, INPUT_ACTION_DECREASE_Z_LEVEL);
    set_input_binding_description(binding17, "Decrease Z-level");
    
    int* binding18 = &input_bindings[18 * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding18, 'v');
    set_input_binding_modifiers(binding18, 0);
    set_input_binding_action(binding18, INPUT_ACTION_INCREASE_Z_LEVEL);
    set_input_binding_description(binding18, "Increase Z-level");
    
    input_binding_count = 19;
}

void add_input_binding(unsigned char key, int modifiers, InputAction action, const char* description) {
    // Check if we need to expand the array
    if (input_binding_count >= input_binding_capacity) {
        int new_capacity = input_binding_capacity * 2;
        int* new_bindings = (int*)realloc(input_bindings, new_capacity * INPUT_BINDING_SIZE);
        if (new_bindings) {
            input_bindings = new_bindings;
            input_binding_capacity = new_capacity;
        } else {
            fprintf(stderr, "Error: Failed to expand input bindings array\n");
            return;
        }
    }
    
    // Add the new binding
    int* binding = &input_bindings[input_binding_count * INPUT_BINDING_FIELDS];
    set_input_binding_key(binding, key);
    set_input_binding_modifiers(binding, modifiers);
    set_input_binding_action(binding, action);
    set_input_binding_description(binding, description);
    
    input_binding_count++;
}

int* find_input_binding(unsigned char key, int modifiers) {
    for (int i = 0; i < input_binding_count; i++) {
        int* binding = &input_bindings[i * INPUT_BINDING_FIELDS];
        if (get_input_binding_key(binding) == key && get_input_binding_modifiers(binding) == modifiers) {
            return binding;
        }
    }
    return NULL;
}

// Event queue management functions
void enqueue_key_event(unsigned char key, int x, int y, int modifiers) {
    if (event_queue_count < MAX_EVENT_QUEUE_SIZE) {
        int* event = &event_queue[event_queue_tail * INPUT_EVENT_FIELDS];
        set_input_event_type(event, INPUT_EVENT_KEY_PRESS);
        set_input_event_key(event, key);
        set_input_event_key_x(event, x);
        set_input_event_key_y(event, y);
        set_input_event_key_modifiers(event, modifiers);
        
        event_queue_tail = (event_queue_tail + 1) % MAX_EVENT_QUEUE_SIZE;
        event_queue_count++;
    }
}

void enqueue_mouse_event(int button, int state, int x, int y) {
    if (event_queue_count < MAX_EVENT_QUEUE_SIZE) {
        int* event = &event_queue[event_queue_tail * INPUT_EVENT_FIELDS];
        set_input_event_type(event, INPUT_EVENT_MOUSE_CLICK);
        set_input_event_mouse_button(event, button);
        set_input_event_mouse_state(event, state);
        set_input_event_mouse_x(event, x);
        set_input_event_mouse_y(event, y);
        
        event_queue_tail = (event_queue_tail + 1) % MAX_EVENT_QUEUE_SIZE;
        event_queue_count++;
    }
}

void enqueue_terminal_event(int ch) {
    if (event_queue_count < MAX_EVENT_QUEUE_SIZE) {
        int* event = &event_queue[event_queue_tail * INPUT_EVENT_FIELDS];
        set_input_event_type(event, INPUT_EVENT_TERMINAL_INPUT);
        set_input_event_terminal_ch(event, ch);
        
        event_queue_tail = (event_queue_tail + 1) % MAX_EVENT_QUEUE_SIZE;
        event_queue_count++;
    }
}

int dequeue_event(int* event) {
    if (event_queue_count > 0) {
        int* src_event = &event_queue[event_queue_head * INPUT_EVENT_FIELDS];
        // Copy the event data
        for (int i = 0; i < INPUT_EVENT_FIELDS; i++) {
            event[i] = src_event[i];
        }
        event_queue_head = (event_queue_head + 1) % MAX_EVENT_QUEUE_SIZE;
        event_queue_count--;
        return 1; // Success
    }
    return 0; // Queue empty
}

int is_event_queue_empty(void) {
    return event_queue_count == 0;
}

// Helper function to handle paint tool action
void handle_paint_tool_action(void) {
    // In 3D mode, invert the Y coordinate for placement to match 2D coordinate system
    int placement_row = (get_view_mode() == 1) ? (get_canvas_rows() - 1 - get_selector_row()) : get_selector_row();
    set_canvas_tile(get_current_z_level(), placement_row, get_selector_col(), 0, get_selected_emoji());
    set_canvas_tile(get_current_z_level(), placement_row, get_selector_col(), 1, get_selected_fg_color());
    set_canvas_tile(get_current_z_level(), placement_row, get_selector_col(), 2, get_selected_bg_color());
    char coord_msg[100];
    snprintf(coord_msg, sizeof(coord_msg), "Tile painted at (%d, %d, %d)", 
             get_selector_col(), placement_row, get_current_z_level());
    set_status_message(coord_msg);
}

// Helper function to handle fill tool action
void handle_fill_tool_action(void) {
    // In 3D mode, invert the Y coordinate for placement to match 2D coordinate system
    int placement_row = (get_view_mode() == 1) ? (get_canvas_rows() - 1 - get_selector_row()) : get_selector_row();
    flood_fill(get_current_z_level(), placement_row, get_selector_col(),
               get_canvas_tile(get_current_z_level(), placement_row, get_selector_col(), 0),
               get_canvas_tile(get_current_z_level(), placement_row, get_selector_col(), 1),
               get_canvas_tile(get_current_z_level(), placement_row, get_selector_col(), 2));
    char coord_msg[100];
    snprintf(coord_msg, sizeof(coord_msg), "Area filled at (%d, %d, %d)", 
             get_selector_col(), placement_row, get_current_z_level());
    set_status_message(coord_msg);
}

// Helper function to handle rectangle tool action
void handle_rectangle_tool_action(void) {
    if (get_start_row() == -1) {
        // In 3D mode, invert the Y coordinate for placement to match 2D coordinate system
        int placement_row = (get_view_mode() == 1) ? (get_canvas_rows() - 1 - get_selector_row()) : get_selector_row();
        set_start_row(placement_row);
        set_start_col(get_selector_col());
        char coord_msg[100];
        snprintf(coord_msg, sizeof(coord_msg), "Rectangle start at (%d, %d, %d)", 
                 get_selector_col(), placement_row, get_current_z_level());
        set_status_message(coord_msg);
    } else {
        // In 3D mode, invert the Y coordinate for placement to match 2D coordinate system
        int placement_row = (get_view_mode() == 1) ? (get_canvas_rows() - 1 - get_selector_row()) : get_selector_row();
        draw_rectangle(get_current_z_level(), get_start_row(), get_start_col(), placement_row, get_selector_col());
        set_start_row(-1);
        set_start_col(-1);
        char coord_msg[100];
        snprintf(coord_msg, sizeof(coord_msg), "Rectangle drawn from (%d, %d) to (%d, %d) at Z=%d", 
                 get_start_col(), get_start_row(), get_selector_col(), placement_row, get_current_z_level());
        set_status_message(coord_msg);
    }
}

// Helper function to handle erase tool action
void handle_erase_tool_action(void) {
    if (get_start_row() == -1) {
        // In 3D mode, invert the Y coordinate for placement to match 2D coordinate system
        int placement_row = (get_view_mode() == 1) ? (get_canvas_rows() - 1 - get_selector_row()) : get_selector_row();
        erase_tile(get_current_z_level(), placement_row, get_selector_col());
        char coord_msg[100];
        snprintf(coord_msg, sizeof(coord_msg), "Tile erased at (%d, %d, %d)", 
                    get_selector_col(), placement_row, get_current_z_level());
        set_status_message(coord_msg);
    } else {
        // In 3D mode, invert the Y coordinate for placement to match 2D coordinate system
        int placement_row = (get_view_mode() == 1) ? (get_canvas_rows() - 1 - get_selector_row()) : get_selector_row();
        erase_rectangle(get_current_z_level(), get_start_row(), get_start_col(), placement_row, get_selector_col());
        set_start_row(-1);
        set_start_col(-1);
        char coord_msg[100];
        snprintf(coord_msg, sizeof(coord_msg), "Rectangle erased from (%d, %d) to (%d, %d) at Z=%d", 
                    get_start_col(), get_start_row(), get_selector_col(), placement_row, get_current_z_level());
        set_status_message(coord_msg);
    }
}

// Helper function to handle erase fill tool action
void handle_erase_fill_tool_action(void) {
    // In 3D mode, invert the Y coordinate for placement to match 2D coordinate system
    int placement_row = (get_view_mode() == 1) ? (get_canvas_rows() - 1 - get_selector_row()) : get_selector_row();
    erase_fill(get_current_z_level(), placement_row, get_selector_col());
    char coord_msg[100];
    snprintf(coord_msg, sizeof(coord_msg), "Area erased at (%d, %d, %d)", 
                get_selector_col(), placement_row, get_current_z_level());
    set_status_message(coord_msg);
}

void handle_input_action(InputAction action) {
    switch (action) {
        case INPUT_ACTION_PAINT:
            if (current_tool_state == TOOL_STATE_PAINT) {
                handle_paint_tool_action();
            } else if (current_tool_state == TOOL_STATE_FILL) {
                handle_fill_tool_action();
            } else if (current_tool_state == TOOL_STATE_RECTANGLE) {
                handle_rectangle_tool_action();
            } else if (current_tool_state == TOOL_STATE_ERASE) {
                handle_erase_tool_action();
            } else if (current_tool_state == TOOL_STATE_ERASE_FILL) {
                handle_erase_fill_tool_action();
            }
            print_ascii_grid();
            trigger_view_update();
            break;
        default:
            break;
    }
}

// Helper function to handle 3D mode click with raycasting
void handle_3d_mode_click(int x, int y) {
    int row, col, layer;
    if (find_closest_intersected_block(x, y, &row, &col, &layer)) {
        set_selector_row(row);
        set_selector_col(col);
        char coord_msg[100];
        // Invert the Y coordinate for display to match 2D coordinate system
        int display_row = get_canvas_rows() - 1 - row;
        if (current_tool_state == TOOL_STATE_PAINT) { // Paint
            // Invert the Y coordinate for actual placement to match 2D coordinate system
            int placement_row = get_canvas_rows() - 1 - row;
            set_canvas_tile(get_current_z_level(), placement_row, col, 0, get_selected_emoji());
            set_canvas_tile(get_current_z_level(), placement_row, col, 1, get_selected_fg_color());
            set_canvas_tile(get_current_z_level(), placement_row, col, 2, get_selected_bg_color());
            snprintf(coord_msg, sizeof(coord_msg), "Block painted at (%d, %d, %d) in 3D", col, display_row, get_current_z_level());
            set_status_message(coord_msg);
        } else if (current_tool_state == TOOL_STATE_FILL) { // Fill
            // Invert the Y coordinate for actual placement to match 2D coordinate system
            int placement_row = get_canvas_rows() - 1 - row;
            flood_fill(get_current_z_level(), placement_row, col,
                       get_canvas_tile(get_current_z_level(), placement_row, col, 0),
                       get_canvas_tile(get_current_z_level(), placement_row, col, 1),
                       get_canvas_tile(get_current_z_level(), placement_row, col, 2));
            snprintf(coord_msg, sizeof(coord_msg), "Area filled at (%d, %d, %d) in 3D", col, display_row, get_current_z_level());
            set_status_message(coord_msg);
        } else if (current_tool_state == TOOL_STATE_RECTANGLE) { // Rectangle
            if (get_start_row() == -1) {
                // Invert the Y coordinate for actual placement to match 2D coordinate system
                int placement_row = get_canvas_rows() - 1 - row;
                set_start_row(placement_row);
                set_start_col(col);
                snprintf(coord_msg, sizeof(coord_msg), "Rectangle start at (%d, %d, %d) in 3D", col, display_row, get_current_z_level());
                set_status_message(coord_msg);
            } else {
                // Invert the Y coordinate for actual placement to match 2D coordinate system
                int placement_row = get_canvas_rows() - 1 - row;
                int placement_start_row = get_canvas_rows() - 1 - get_start_row();
                draw_rectangle(get_current_z_level(), placement_start_row, get_start_col(), placement_row, col);
                set_start_row(-1);
                set_start_col(-1);
                // Invert the Y coordinate for display to match 2D coordinate system
                int display_start_row = get_canvas_rows() - 1 - get_start_row();
                snprintf(coord_msg, sizeof(coord_msg), "Rectangle drawn from (%d, %d) to (%d, %d) at Z=%d in 3D", 
                         get_start_col(), display_start_row, col, display_row, get_current_z_level());
                set_status_message(coord_msg);
            }
        } else if (current_tool_state == TOOL_STATE_ERASE) { // Erase
            if (get_start_row() == -1) {
                // Invert the Y coordinate for actual placement to match 2D coordinate system
                int placement_row = get_canvas_rows() - 1 - row;
                erase_tile(get_current_z_level(), placement_row, col);
                snprintf(coord_msg, sizeof(coord_msg), "Block erased at (%d, %d, %d) in 3D", col, display_row, get_current_z_level());
                set_status_message(coord_msg);
            } else {
                // Invert the Y coordinate for actual placement to match 2D coordinate system
                int placement_row = get_canvas_rows() - 1 - row;
                int placement_start_row = get_canvas_rows() - 1 - get_start_row();
                erase_rectangle(get_current_z_level(), placement_start_row, get_start_col(), placement_row, col);
                set_start_row(-1);
                set_start_col(-1);
                // Invert the Y coordinate for display to match 2D coordinate system
                int display_start_row = get_canvas_rows() - 1 - get_start_row();
                snprintf(coord_msg, sizeof(coord_msg), "Rectangle erased from (%d, %d) to (%d, %d) at Z=%d in 3D", 
                         get_start_col(), display_start_row, col, display_row, get_current_z_level());
                set_status_message(coord_msg);
            }
        } else if (current_tool_state == TOOL_STATE_ERASE_FILL) {
            // Invert the Y coordinate for actual placement to match 2D coordinate system
            int placement_row = get_canvas_rows() - 1 - row;
            erase_fill(get_current_z_level(), placement_row, col);
            snprintf(coord_msg, sizeof(coord_msg), "Area erased at (%d, %d, %d) in 3D", col, display_row, get_current_z_level());
            set_status_message(coord_msg);
        }
        print_ascii_grid();
        trigger_view_update();
    } else {
        set_status_message("Clicked empty space in 3D");
    }
}

// Helper function to handle canvas click in 2D mode
void handle_canvas_click(int x, int y) {
    float gl_y = get_window_height() - y;
    // Account for scale when calculating grid coordinates
    float scale_x = get_scale_x();
    float scale_y = get_scale_y();
    int col = (int)((x - get_sidebar_width() - 10) / (get_tile_size() * scale_x));
    // Calculate row based on the same coordinate system used in rendering
    int row = (int)((get_window_height() - HEADER_STRIP_HEIGHT - 10 - gl_y) / (get_tile_size() * scale_y));
    
    if (col >= 0 && col < get_canvas_cols() && row >= 0 && row < get_canvas_rows()) {
        set_selector_row(row);
        set_selector_col(col);
        char coord_msg[100];
        if (current_tool_state == TOOL_STATE_PAINT) { // Paint
            set_canvas_tile(get_current_z_level(), row, col, 0, get_selected_emoji());
            set_canvas_tile(get_current_z_level(), row, col, 1, get_selected_fg_color());
            set_canvas_tile(get_current_z_level(), row, col, 2, get_selected_bg_color());
            snprintf(coord_msg, sizeof(coord_msg), "Tile painted at (%d, %d, %d)", col, row, get_current_z_level());
            set_status_message(coord_msg);
        } else if (current_tool_state == TOOL_STATE_FILL) { // Fill
            flood_fill(get_current_z_level(), row, col,
                       get_canvas_tile(get_current_z_level(), row, col, 0),
                       get_canvas_tile(get_current_z_level(), row, col, 1),
                       get_canvas_tile(get_current_z_level(), row, col, 2));
            snprintf(coord_msg, sizeof(coord_msg), "Area filled at (%d, %d, %d)", col, row, get_current_z_level());
            set_status_message(coord_msg);
        } else if (current_tool_state == TOOL_STATE_RECTANGLE) { // Rectangle
            if (get_start_row() == -1) {
                set_start_row(row);
                set_start_col(col);
                snprintf(coord_msg, sizeof(coord_msg), "Rectangle start at (%d, %d, %d)", col, row, get_current_z_level());
                set_status_message(coord_msg);
            } else {
                draw_rectangle(get_current_z_level(), get_start_row(), get_start_col(), row, col);
                set_start_row(-1);
                set_start_col(-1);
                snprintf(coord_msg, sizeof(coord_msg), "Rectangle drawn from (%d, %d) to (%d, %d) at Z=%d", 
                         get_start_col(), get_start_row(), col, row, get_current_z_level());
                set_status_message(coord_msg);
            }
        } else if (current_tool_state == TOOL_STATE_ERASE) { // Erase
            if (get_start_row() == -1) {
                erase_tile(get_current_z_level(), row, col);
                snprintf(coord_msg, sizeof(coord_msg), "Tile erased at (%d, %d, %d)", col, row, get_current_z_level());
                set_status_message(coord_msg);
            } else {
                erase_rectangle(get_current_z_level(), get_start_row(), get_start_col(), row, col);
                set_start_row(-1);
                set_start_col(-1);
                snprintf(coord_msg, sizeof(coord_msg), "Rectangle erased from (%d, %d) to (%d, %d) at Z=%d", 
                         get_start_col(), get_start_row(), col, row, get_current_z_level());
                set_status_message(coord_msg);
            }
        } else if (current_tool_state == TOOL_STATE_ERASE_FILL) { // Erase Fill
            erase_fill(get_current_z_level(), row, col);
            snprintf(coord_msg, sizeof(coord_msg), "Area erased at (%d, %d, %d)", col, row, get_current_z_level());
            set_status_message(coord_msg);
        }
        print_ascii_grid();
        trigger_view_update();
    }
}

// Helper function to handle file tab clicks
void handle_file_tab_click(int x, int y, float gl_y) {
    float sidebar_x = 10;
    
    // Only handle file tab clicks if they are below the header strip
    if (gl_y <= HEADER_STRIP_HEIGHT) {
        return; // Click is in header strip, handled elsewhere
    }
    
    // File menu button (replaces Save and Load buttons)
    if (x >= sidebar_x && x < sidebar_x + 60 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
        handle_file_menu_click(x, gl_y);
    }
    // Text button (moved left to fill gap from removed Save/Load buttons)
    else if (x >= sidebar_x + 70 && x < sidebar_x + 130 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
        set_status_message("Text button clicked (no functionality)");
    }
    // Tile button
    else if (x >= sidebar_x + 140 && x < sidebar_x + 200 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
        set_status_message("Tile button clicked (no functionality)");
    }
    // 2D view button
    else if (x >= sidebar_x + 210 && x < sidebar_x + 270 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
        set_view_mode(0);
        set_status_message("Switched to 2D view");
    }
    // 3D view button
    else if (x >= sidebar_x + 280 && x < sidebar_x + 340 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
        set_view_mode(1);
        // Reset camera for 3D view of current Z-level
        float center_x = (get_canvas_cols() * get_tile_size()) / 2.0f;
        float center_y = (get_canvas_rows() * get_tile_size()) / 2.0f;
        float z_level = get_current_z_level() * get_tile_size();
        set_camera_x(center_x);
        set_camera_y(center_y);
        set_camera_z(z_level + get_tile_size() * 5.0f); // Position camera above the Z-level
        set_camera_yaw(0.0f); // Look straight ahead
        set_camera_pitch(-90.0f); // Look straight down
        set_status_message("Switched to 3D view, centered on current Z-level");
    }
}

// Helper function to handle sidebar emoji clicks
void handle_sidebar_emoji_click(int x, int y, float gl_y) {
    float sidebar_x = 10;
    float sidebar_y_offset = get_file_tab_height() + 50;
    float base_y = get_window_height() - sidebar_y_offset - 20;
    int total_emojis = get_num_emojis();
    int emoji_cols = 8;
    int emoji_rows = (total_emojis + emoji_cols - 1) / emoji_cols;
    int visible_emojis = emoji_visible_rows * emoji_cols;
    float emoji_section_top = base_y + 40;
    
    // Check if we have a thumb picker (more than 64 emojis)
    if (total_emojis > 64 || total_emojis > visible_emojis) {
        float emoji_section_bottom = base_y - (emoji_visible_rows - 1) * 40;
        
        // Check if click is on an emoji (but not on the thumb picker area)
        float thumb_x = sidebar_x + 10 + emoji_cols * 40 + 5;
        if (x >= sidebar_x + 10 && x < thumb_x &&  // Make sure click is not on thumb area
            gl_y >= emoji_section_bottom && gl_y <= emoji_section_top) {
            float local_y_from_top = emoji_section_top - gl_y;
            int relative_row = (int)(local_y_from_top / 40);
            int col = (int)((x - (sidebar_x + 10)) / 40);
            if (relative_row >= 0 && relative_row < emoji_visible_rows && col >= 0 && col < emoji_cols) {
                int idx = emoji_start_index + relative_row * emoji_cols + col;
                if (idx < total_emojis) {
                    set_selected_emoji(idx);
                    set_status_message("Emoji selected");
                }
            }
        }
    } else {
        // Original logic for small emoji sets
        float emoji_section_bottom = base_y - (emoji_rows - 1) * 40;
        
        if (x >= sidebar_x + 10 && x < sidebar_x + 10 + emoji_cols * 40 &&
            gl_y >= emoji_section_bottom && gl_y <= emoji_section_top) {
            float local_y_from_top = emoji_section_top - gl_y;
            int row = (int)(local_y_from_top / 40);
            int col = (int)((x - (sidebar_x + 10)) / 40);
            if (row >= 0 && row < emoji_rows && col >= 0 && col < emoji_cols) {
                int idx = row * emoji_cols + col;
                if (idx < total_emojis) {
                    set_selected_emoji(idx);
                    set_status_message("Emoji selected");
                }
            }
        }
    }
}

// Helper function to handle sidebar color clicks
void handle_sidebar_color_click(int x, int y, float gl_y) {
    float sidebar_x = 10;
    float sidebar_y_offset = get_file_tab_height() + 50;
    float base_y = get_window_height() - sidebar_y_offset - 20;
    int emoji_cols = 8;
    int total_emojis = get_num_emojis();
    int emoji_rows = (total_emojis + emoji_cols - 1) / emoji_cols;
    int visible_emojis = emoji_visible_rows * emoji_cols;
    float emoji_section_top = base_y + 40;
    float emoji_section_bottom;
    
    // Calculate emoji section bottom based on whether we're using thumb picker or not
    if (total_emojis > 64 || total_emojis > visible_emojis) {
        emoji_section_bottom = base_y - (emoji_visible_rows - 1) * 40;
    } else {
        emoji_section_bottom = base_y - (emoji_rows - 1) * 40;
    }
    
    float color_section_top = emoji_section_bottom - 20;
    float color_section_bottom = color_section_top - 40;
    
    if (x >= sidebar_x + 10 && x < sidebar_x + 10 + get_num_colors() * 40 &&
        gl_y >= color_section_bottom && gl_y <= color_section_top) {
        int idx = (int)((x - (sidebar_x + 10)) / 40);
        if (idx >= 0 && idx < get_num_colors()) {
            set_selected_fg_color(idx);
            set_status_message("Color selected");
        }
    }
}

// Helper function to handle sidebar tab clicks
void handle_sidebar_tab_click(int x, int y, float gl_y) {
    float sidebar_x = 10;
    float sidebar_y_offset = get_file_tab_height() + 50;
    float base_y = get_window_height() - sidebar_y_offset - 20;
    int emoji_cols = 8;
    int total_emojis = get_num_emojis();
    int emoji_rows = (total_emojis + emoji_cols - 1) / emoji_cols;
    int visible_emojis = emoji_visible_rows * emoji_cols;
    float emoji_section_top = base_y + 40;
    float emoji_section_bottom;
    
    // Calculate emoji section bottom based on whether we're using thumb picker or not
    if (total_emojis > 64 || total_emojis > visible_emojis) {
        emoji_section_bottom = base_y - (emoji_visible_rows - 1) * 40;
    } else {
        emoji_section_bottom = base_y - (emoji_rows - 1) * 40;
    }
    
    float color_section_top = emoji_section_bottom - 20;
    float color_section_bottom = color_section_top - 40;
    float tabs_section_top = color_section_bottom - 20;
    float tabs_section_bottom = tabs_section_top - get_tab_count() * 40;
    float tools_section_top = tabs_section_bottom - 20;
    float tools_section_bottom = tools_section_top - 30;
    
    if (x >= sidebar_x + 10 && x < sidebar_x + 50 &&
        gl_y >= tabs_section_bottom && gl_y <= tabs_section_top) {
        float local_y_from_top = tabs_section_top - gl_y;
        int idx = (int)(local_y_from_top / 40);
        if (idx >= 0 && idx < get_tab_count()) {
            set_selected_emoji(get_tab_bank(idx, 0));
            set_selected_fg_color(get_tab_bank(idx, 1));
            set_selected_bg_color(get_tab_bank(idx, 2));
            set_status_message("Tab selected");
        }
    }
}

// Helper function to handle sidebar tool clicks
void handle_sidebar_tool_click(int x, int y, float gl_y) {
    float sidebar_x = 10;
    float sidebar_y_offset = get_file_tab_height() + 50;
    float base_y = get_window_height() - sidebar_y_offset - 20;
    int emoji_cols = 8;
    int total_emojis = get_num_emojis();
    int emoji_rows = (total_emojis + emoji_cols - 1) / emoji_cols;
    int visible_emojis = emoji_visible_rows * emoji_cols;
    float emoji_section_top = base_y + 40;
    float emoji_section_bottom;
    
    // Calculate emoji section bottom based on whether we're using thumb picker or not
    if (total_emojis > 64 || total_emojis > visible_emojis) {
        emoji_section_bottom = base_y - (emoji_visible_rows - 1) * 40;
    } else {
        emoji_section_bottom = base_y - (emoji_rows - 1) * 40;
    }
    
    float color_section_top = emoji_section_bottom - 20;
    float color_section_bottom = color_section_top - 40;
    float tabs_section_top = color_section_bottom - 20;
    float tabs_section_bottom = tabs_section_top - get_tab_count() * 40;
    float tools_section_top = tabs_section_bottom - 20;
    float tools_section_bottom = tools_section_top - 30;
    
    if (x >= sidebar_x + 10 && x < sidebar_x + 10 + 4 * 70 &&
        gl_y >= tools_section_bottom && gl_y <= tools_section_top) {
        int idx = (int)((x - (sidebar_x + 10)) / 70);
        if (idx >= 0 && idx < 4) {
            // Map the UI index to the correct tool state
            switch (idx) {
                case 0:
                    set_tool_state(TOOL_STATE_PAINT);
                    break;
                case 1:
                    set_tool_state(TOOL_STATE_FILL);
                    break;
                case 2:
                    set_tool_state(TOOL_STATE_RECTANGLE);
                    break;
                case 3:
                    set_tool_state(TOOL_STATE_ERASE);
                    break;
            }
        }
    }
}

// Helper function to handle sidebar map clicks
int rename_map_file(const char* old_name, const char* new_name) {
    // Make sure we have a proper project path
    if (strlen(current_project_path) == 0) {
        fprintf(stderr, "Error: Current project path is empty\n");
        return -1;
    }
    
    // Create full paths for old and new files
    char old_map_path[512];
    char new_map_path[512];
    
    snprintf(old_map_path, sizeof(old_map_path), "%s/%s", current_project_path, old_name);
    snprintf(new_map_path, sizeof(new_map_path), "%s/%s", current_project_path, new_name);
    
    // Rename the map file
    if (rename(old_map_path, new_map_path) != 0) {
        fprintf(stderr, "Error: Could not rename map file from %s to %s\n", old_name, new_name);
        return -1;
    }
    
    // Also rename the corresponding events file if it exists
    // Extract the base name of the map file (without extension)
    char* old_base_name = strdup(old_name);
    char* new_base_name = strdup(new_name);
    
    if (!old_base_name || !new_base_name) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(old_base_name);
        free(new_base_name);
        return -1;
    }
    
    // Remove the .txt extension if present
    char* old_dot = strrchr(old_base_name, '.');
    if (old_dot && strcmp(old_dot, ".txt") == 0) {
        *old_dot = '\0';
    }
    
    char* new_dot = strrchr(new_base_name, '.');
    if (new_dot && strcmp(new_dot, ".txt") == 0) {
        *new_dot = '\0';
    }
    
    // Create the events file names using dynamic allocation
    size_t old_events_len = strlen(old_base_name) + strlen("_events.txt") + 1;
    size_t new_events_len = strlen(new_base_name) + strlen("_events.txt") + 1;
    
    char* old_events_name = malloc(old_events_len);
    char* new_events_name = malloc(new_events_len);
    
    if (!old_events_name || !new_events_name) {
        fprintf(stderr, "Error: Memory allocation failed for event names\n");
        free(old_base_name);
        free(new_base_name);
        free(old_events_name);
        free(new_events_name);
        return -1;
    }
    
    snprintf(old_events_name, old_events_len, "%s_events.txt", old_base_name);
    snprintf(new_events_name, new_events_len, "%s_events.txt", new_base_name);
    
    // Create full paths for old and new events files
    size_t old_events_path_len = strlen(current_project_path) + 1 + old_events_len + 1;
    size_t new_events_path_len = strlen(current_project_path) + 1 + new_events_len + 1;
    
    char* old_events_path = malloc(old_events_path_len);
    char* new_events_path = malloc(new_events_path_len);
    
    if (!old_events_path || !new_events_path) {
        fprintf(stderr, "Error: Memory allocation failed for event paths\n");
        free(old_base_name);
        free(new_base_name);
        free(old_events_name);
        free(new_events_name);
        free(old_events_path);
        free(new_events_path);
        return -1;
    }
    
    snprintf(old_events_path, old_events_path_len, "%s/%s", current_project_path, old_events_name);
    snprintf(new_events_path, new_events_path_len, "%s/%s", current_project_path, new_events_name);
    
    // Check if the old events file exists
    if (access(old_events_path, F_OK) == 0) {
        // Rename the events file
        if (rename(old_events_path, new_events_path) != 0) {
            fprintf(stderr, "Warning: Could not rename events file from %s to %s\n", old_events_name, new_events_name);
            // This is just a warning, we don't return an error since the map was renamed successfully
        }
    }
    
    // If the renamed map was the currently loaded map, update the current_map_file variable
    if (strcmp(current_map_file, old_name) == 0) {
        strncpy(current_map_file, new_name, sizeof(current_map_file) - 1);
        current_map_file[sizeof(current_map_file) - 1] = '\0';
    }
    
    // Rescan the map files list to update the UI
    scan_map_files(current_project_path);
    
    // Clean up dynamically allocated memory
    free(old_base_name);
    free(new_base_name);
    free(old_events_name);
    free(new_events_name);
    free(old_events_path);
    free(new_events_path);
    
    return 0;
}

// Helper function to handle sidebar map clicks
void handle_sidebar_map_click(int x, int y, float gl_y) {
    float sidebar_x = 10;
    float sidebar_y_offset = get_file_tab_height() + 50;
    float base_y = get_window_height() - sidebar_y_offset - 20;
    int emoji_cols = 8;
    int total_emojis = get_num_emojis();
    int emoji_rows = (total_emojis + emoji_cols - 1) / emoji_cols;
    int visible_emojis = emoji_visible_rows * emoji_cols;
    float emoji_section_top = base_y + 40;
    float emoji_section_bottom;
    
    // Calculate emoji section bottom based on whether we're using thumb picker or not
    if (total_emojis > 64 || total_emojis > visible_emojis) {
        emoji_section_bottom = base_y - (emoji_visible_rows - 1) * 40;
    } else {
        emoji_section_bottom = base_y - (emoji_rows - 1) * 40;
    }
    
    float color_section_top = emoji_section_bottom - 20;
    float color_section_bottom = color_section_top - 40;
    float tabs_section_top = color_section_bottom - 20;
    float tabs_section_bottom = tabs_section_top - get_tab_count() * 40;
    float tools_section_top = tabs_section_bottom - 20;
    float tools_section_bottom = tools_section_top - 30;
    float map_window_y = tools_section_bottom - 210;
    
    if (x >= sidebar_x && x < sidebar_x + get_sidebar_width() - 20 &&
        gl_y >= map_window_y && gl_y <= map_window_y + 200) {
        // Check if click is on a map name (but not on the "Maps" label)
        int map_files_count = 0;
        char** map_files = get_map_files_list(&map_files_count);
        if (map_files && map_files_count > 0) {
            // Calculate which map name was clicked
            float click_y = gl_y - map_window_y;
            int max_display = (map_files_count < 8) ? map_files_count : 8;
            
            // Map names are displayed from top to bottom starting at y=160
            // The "Maps" label is at y=180, so we need to skip it
            for (int i = 0; i < max_display; i++) {
                float text_y = 160 - (i * 20);
                // Check if click is within the text area (approximate)
                if (click_y >= text_y - 10 && click_y <= text_y + 10) {
                    // Load the selected map file
                    char full_path[512];
                    if (strncmp(current_project_path, "projects/", 9) == 0) {
                        // Already has projects/ prefix
                        snprintf(full_path, sizeof(full_path), "%s/%s", current_project_path, map_files[i]);
                    } else {
                        // Add projects/ prefix
                        snprintf(full_path, sizeof(full_path), "projects/%s/%s", current_project_path, map_files[i]);
                    }
                    fprintf(stderr, "Attempting to load map: %s\n", full_path);
                    if (load_game_state(full_path) == 0) {
                        // Set the current map file name
                        strncpy(current_map_file, map_files[i], sizeof(current_map_file) - 1);
                        current_map_file[sizeof(current_map_file) - 1] = '\0';
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Loaded map: %s", map_files[i]);
                        set_status_message(msg);
                        fprintf(stderr, "Successfully loaded map: %s\n", map_files[i]);
                    } else {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Failed to load map: %s", map_files[i]);
                        set_status_message(msg);
                        fprintf(stderr, "Failed to load map: %s\n", map_files[i]);
                    }
                    break;
                }
            }
        }
    }
}

// Header strip height (defined in view_gl]3d]a0.c)
#define HEADER_STRIP_HEIGHT 40

void mouse(int button, int state, int x, int y) {
    // If the text input dialog is visible, it should consume all mouse events.
    if (text_input_dialog && text_input_dialog->visible) {
        // Convert Y coordinate to match OpenGL coordinate system
        int gl_y = get_window_height() - y;
        
        printf("Text input dialog is visible, handling mouse event\n");
        if (state == GLUT_DOWN) {
            printf("Mouse click at (%d, %d) -> OpenGL coords (%d, %d)\n", x, y, x, gl_y);
            printf("Dialog bounds: (%d, %d) to (%d, %d)\n", 
                   text_input_dialog->x, text_input_dialog->y,
                   text_input_dialog->x + text_input_dialog->width,
                   text_input_dialog->y + text_input_dialog->height);
            
            // Check if the click is within the dialog bounds
            if (x >= text_input_dialog->x && x <= text_input_dialog->x + text_input_dialog->width &&
                gl_y >= text_input_dialog->y && gl_y <= text_input_dialog->y + text_input_dialog->height) {
                printf("Click is inside dialog bounds\n");

                // Check for OK button click
                float ok_x = text_input_dialog->x + text_input_dialog->width - 160;
                float ok_y = text_input_dialog->y + 20;
                float ok_width = 60;
                float ok_height = 30;
                printf("OK button bounds: (%.0f, %.0f) to (%.0f, %.0f)\n", 
                       ok_x, ok_y, ok_x + ok_width, ok_y + ok_height);
                if (x >= ok_x && x <= ok_x + ok_width && gl_y >= ok_y && gl_y <= ok_y + ok_height) {
                    printf("OK button clicked\n");
                    text_input_dialog->confirmed = true;
                    text_input_dialog->visible = false;
                    save_event_text_to_file(text_input_dialog->input_buffer);
                    glutPostRedisplay();
                    return;
                }

                // Check for Cancel button click
                float cancel_x = text_input_dialog->x + text_input_dialog->width - 80;
                float cancel_y = text_input_dialog->y + 20;
                float cancel_width = 60;
                float cancel_height = 30;
                printf("Cancel button bounds: (%.0f, %.0f) to (%.0f, %.0f)\n", 
                       cancel_x, cancel_y, cancel_x + cancel_width, cancel_y + cancel_height);
                if (x >= cancel_x && x <= cancel_x + cancel_width && gl_y >= cancel_y && gl_y <= cancel_y + cancel_height) {
                    printf("Cancel button clicked\n");
                    text_input_dialog->confirmed = false;
                    text_input_dialog->visible = false;
                    glutPostRedisplay();
                    return;
                }
                // Any click inside the dialog (but not on buttons) should not close it
                // Just redisplay to handle any potential changes
                printf("Click inside dialog but not on buttons, keeping dialog open\n");
                glutPostRedisplay();
                return;
            } else {
                // Click is outside the dialog, so close it without confirmation.
                printf("Click is outside dialog, closing\n");
                text_input_dialog->confirmed = false;
                text_input_dialog->visible = false;
            }
        }
        // After handling the click (or ignoring it if it's not GLUT_DOWN),
        // we redisplay and consume the event by returning.
        printf("Redisplaying after handling text input dialog\n");
        glutPostRedisplay();
        return;
    }

    // Handle thumb dragging for emoji picker
    if (state == GLUT_DOWN) {
        // Check if click is on thumb picker when we have more than 64 emojis
        int total_emojis = get_num_emojis();
        int emoji_cols = 8;
        int emoji_rows = (total_emojis + emoji_cols - 1) / emoji_cols;
        int visible_emojis = emoji_visible_rows * emoji_cols;
        
        if (total_emojis > 64 || total_emojis > visible_emojis) {
            float sidebar_x = 10;
            float sidebar_y_offset = get_file_tab_height() + 50;
            float base_y = get_window_height() - sidebar_y_offset - 20;
            float thumb_x = sidebar_x + 10 + emoji_cols * 40 + 5;
            float thumb_top_screen_y = emoji_thumb_y;
            float thumb_bottom_screen_y = emoji_thumb_y + emoji_thumb_height;
            float visible_height = emoji_visible_rows * 40;
            
            if (x >= thumb_x && x <= thumb_x + 10 &&
                y >= thumb_top_screen_y && y <= thumb_bottom_screen_y) {
                // Start dragging the thumb
                emoji_thumb_dragging = 1;
                return;
            }
        }
        // Check if click is in header strip
        if (y <= HEADER_STRIP_HEIGHT) {
            // Handle header strip button clicks
            int button_width = 60;
            int button_height = 30;
            int button_y = 5; // 5 pixels from top of header strip
            
            // Check File button (10, 5)
            if (x >= 10 && x <= 10 + button_width && y >= button_y && y <= button_y + button_height) {
                // Toggle file menu
                if (file_menu_visible) {
                    hide_file_menu();
                } else {
                    show_file_menu();
                }
                trigger_view_update();
                return;
            }
            // Check Text button (80, 5)
            else if (x >= 80 && x <= 80 + button_width && y >= button_y && y <= button_y + button_height) {
                set_status_message("Text button clicked (no functionality)");
                trigger_view_update();
                return;
            }
            // Check Tile button (150, 5)
            else if (x >= 150 && x <= 150 + button_width && y >= button_y && y <= button_y + button_height) {
                set_status_message("Tile button clicked (no functionality)");
                trigger_view_update();
                return;
            }
            // Check 2D button (220, 5)
            else if (x >= 220 && x <= 220 + button_width && y >= button_y && y <= button_y + button_height) {
                set_view_mode(0);
                set_status_message("Switched to 2D view");
                trigger_view_update();
                return;
            }
            // Check 3D button (290, 5)
            else if (x >= 290 && x <= 290 + button_width && y >= button_y && y <= button_y + button_height) {
                set_view_mode(1);
                // Reset camera for 3D view of current Z-level
                float center_x = (get_canvas_rows() * get_tile_size()) / 2.0f;
                float center_y = (get_canvas_rows() * get_tile_size()) / 2.0f;
                float z_level = get_current_z_level() * get_tile_size();
                set_camera_x(center_x);
                set_camera_y(center_y);
                set_camera_z(z_level + get_tile_size() * 5.0f); // Position camera above the Z-level
                set_camera_yaw(0.0f); // Look straight ahead
                set_camera_pitch(-90.0f); // Look straight down
                set_status_message("Switched to 3D view, centered on current Z-level");
                trigger_view_update();
                return;
            }
            // Check + button (middle of header)
            else {
                int middle_x = get_window_width() * 0.5;
                // Check - button (middle_x - 105, 5)
                if (x >= middle_x - 105 && x <= middle_x - 105 + button_width && y >= button_y && y <= button_y + button_height) {
                    // Decrease scale values
                    set_scale_x(get_scale_x() - 0.1f);
                    set_scale_y(get_scale_y() - 0.1f);
                    set_scale_z(get_scale_z() - 0.1f);
                    set_status_message("Scale decreased");
                    trigger_view_update();
                    return;
                }
                // Check + button (middle_x - 35, 5)
                else if (x >= middle_x - 35 && x <= middle_x - 35 + button_width && y >= button_y && y <= button_y + button_height) {
                    // Increase scale values
                    set_scale_x(get_scale_x() + 0.1f);
                    set_scale_y(get_scale_y() + 0.1f);
                    set_scale_z(get_scale_z() + 0.1f);
                    set_status_message("Scale increased");
                    trigger_view_update();
                    return;
                }
                // Check 1:1 button (middle_x + 35, 5)
                else if (x >= middle_x + 35 && x <= middle_x + 35 + button_width && y >= button_y && y <= button_y + button_height) {
                    // Reset scale values to 1.0
                    set_scale_x(1.0f);
                    set_scale_y(1.0f);
                    set_scale_z(1.0f);
                    set_status_message("Scale reset to 1:1");
                    trigger_view_update();
                    return;
                }
                // Check coordinate field for input (middle_x + 105, 5)
                else if (x >= middle_x + 105 && x <= middle_x + 105 + 100 && y >= button_y + 2 && y <= button_y + 2 + 25) {
                    set_status_message("Coordinate field clicked - scale editing not implemented yet");
                    trigger_view_update();
                    return;
                }
                // Check scale field for input (middle_x + 215, 5)
                else if (x >= middle_x + 215 && x <= middle_x + 215 + 60 && y >= button_y + 2 && y <= button_y + 2 + 25) {
                    set_status_message("Scale field clicked - scale editing not implemented yet");
                    trigger_view_update();
                    return;
                }
                // Check plug button (get_window_width() - 280, 5)
                else if (x >= get_window_width() - 280 && x <= get_window_width() - 280 + button_width && y >= button_y && y <= button_y + button_height) {
                    set_status_message("plug button clicked");
                    trigger_view_update();
                    return;
                }
                // Check db button (get_window_width() - 210, 5)
                else if (x >= get_window_width() - 210 && x <= get_window_width() - 210 + button_width && y >= button_y && y <= button_y + button_height) {
                    set_status_message("db button clicked");
                    trigger_view_update();
                    return;
                }
                // Check play button (get_window_width() - 140, 5)
                else if (x >= get_window_width() - 140 && x <= get_window_width() - 140 + button_width && y >= button_y && y <= button_y + button_height) {
                    // Build and run the player
                    build_and_run_player();
                    trigger_view_update();
                    return;
                }
            }
        }
        
        // Check if click is in any event menu, with proper priority
        // Submenus have higher priority than parent menus
        bool handled = false;
        
        // Check maps context menu (high priority)
        if (maps_context_menu && maps_context_menu->visible) {
            // Check if click is within the menu bounds
            if (x >= maps_context_menu->x && x <= maps_context_menu->x + maps_context_menu->width &&
                get_window_height() - y >= maps_context_menu->y && get_window_height() - y <= maps_context_menu->y + maps_context_menu->height) {
                handle_event_menu_click(maps_context_menu, x, get_window_height() - y);
                handled = true;
            } else {
                // Click is outside the maps context menu, hide it
                hide_event_menu(maps_context_menu);
            }
        }
        
        // Check commands menu first (highest priority)
        if (!handled && event_commands_menu && event_commands_menu->visible) {
            // Check if click is within the menu bounds
            if (x >= event_commands_menu->x && x <= event_commands_menu->x + event_commands_menu->width &&
                get_window_height() - y >= event_commands_menu->y && get_window_height() - y <= event_commands_menu->y + event_commands_menu->height) {
                handle_event_menu_click(event_commands_menu, x, get_window_height() - y);
                handled = true;
            } else {
                // Click is outside the commands menu, hide it
                hide_event_menu(event_commands_menu);
            }
        }
        
        // Check contents context menu (high priority)
        if (!handled && contents_context_menu && contents_context_menu->visible) {
            // Check if click is within the menu bounds
            if (x >= contents_context_menu->x && x <= contents_context_menu->x + contents_context_menu->width &&
                get_window_height() - y >= contents_context_menu->y && get_window_height() - y <= contents_context_menu->y + contents_context_menu->height) {
                handle_event_menu_click(contents_context_menu, x, get_window_height() - y);
                handled = true;
            } else {
                // Click is outside the contents context menu, hide it
                hide_event_menu(contents_context_menu);
            }
        }
        
        // Check event area menu (medium priority)
        if (!handled && event_area_menu && event_area_menu->visible) {
            // Check if click is within the menu bounds
            if (x >= event_area_menu->x && x <= event_area_menu->x + event_area_menu->width &&
                get_window_height() - y >= event_area_menu->y && get_window_height() - y <= event_area_menu->y + event_area_menu->height) {
                // Check if click is in the contents area (right side)
                int left_empty_width = event_area_menu->width / 3;
                int content_x = event_area_menu->x + left_empty_width;
                if (x >= content_x) {
                    // Click is in contents area, show contents context menu
                    if (!contents_context_menu) {
                        contents_context_menu = create_contents_context_menu(x, get_window_height() - y);
                    }
                    if (contents_context_menu) {
                        contents_context_menu->x = x;
                        contents_context_menu->y = get_window_height() - y - contents_context_menu->height;
                        // Make sure the menu is within the window bounds
                        if (contents_context_menu->x + contents_context_menu->width > get_window_width()) {
                            contents_context_menu->x = get_window_width() - contents_context_menu->width;
                        }
                        if (contents_context_menu->y < 0) {
                            contents_context_menu->y = 0;
                        }
                        show_event_menu(contents_context_menu);
                        handled = true;
                    }
                } else {
                    // Click is in the left side, handle normally
                    handle_event_menu_click(event_area_menu, x, get_window_height() - y);
                    handled = true;
                }
            } else {
                // Click is outside the event area menu, hide it
                hide_event_menu(event_area_menu);
            }
        }
        
        // Check main event menu (lowest priority among menus)
        if (!handled && main_event_menu && main_event_menu->visible) {
            // Check if click is within the menu bounds
            if (x >= main_event_menu->x && x <= main_event_menu->x + main_event_menu->width &&
                get_window_height() - y >= main_event_menu->y && get_window_height() - y <= main_event_menu->y + main_event_menu->height) {
                handle_event_menu_click(main_event_menu, x, get_window_height() - y);
                handled = true;
            } else {
                // Click is outside the main event menu, hide it
                hide_event_menu(main_event_menu);
            }
        }
        
        // If click was in a menu area, don't process it further
        if (handled) {
            trigger_view_update();
            return;
        }
        
        // If we get here, the click was not in any menu
        // Hide all menus first
        if (main_event_menu) hide_event_menu(main_event_menu);
        if (event_area_menu) hide_event_menu(event_area_menu);
        if (event_commands_menu) hide_event_menu(event_commands_menu);
        if (contents_context_menu) hide_event_menu(contents_context_menu);
        
        // Now handle other mouse events
        if (button == GLUT_LEFT_BUTTON) {
            float gl_y = get_window_height() - y;
            
            // Check if file menu is open and give it priority
            if (file_menu_visible) {
                // Handle file menu click first when it's open
                handle_file_menu_click(x, gl_y);
                return; // Return early since we handled the click
            }
            
            // Handle 3D mode click with raycasting - ONLY if click is on canvas area
            if (get_view_mode() == 1 && x >= get_sidebar_width() + 10) {
                handle_3d_mode_click(x, y);
                return; // Return early since we handled the click
            }
            
            // Canvas click (2D mode only)
            if (x >= get_sidebar_width() + 10 && get_view_mode() == 0) {
                handle_canvas_click(x, y);
                return; // Return early since we handled the click
            }
            
            // Only handle sidebar clicks if we're not dragging the thumb
            if (!emoji_thumb_dragging) {
                // Sidebar click - Emojis
                handle_sidebar_emoji_click(x, y, gl_y);
                
                // Sidebar click - Colors
                handle_sidebar_color_click(x, y, gl_y);
                
                // Sidebar click - Tabs
                handle_sidebar_tab_click(x, y, gl_y);
                
                // Sidebar click - Tools
                handle_sidebar_tool_click(x, y, gl_y);
                
                // Sidebar click - Maps
                handle_sidebar_map_click(x, y, gl_y);
            }
            
            print_ascii_grid();
            trigger_view_update();
        } else if (button == GLUT_RIGHT_BUTTON) {
            fprintf(stderr, "Right-click detected at (%d, %d)\n", x, y);
            // Right-click: Show context menu
            float gl_y = get_window_height() - y;
            
            // Check if right-click is in the maps window area
            float sidebar_x = 10;
            float sidebar_y_offset = get_file_tab_height() + 50;
            float base_y = get_window_height() - sidebar_y_offset - 20;
            int emoji_cols = 8;
            int emoji_rows = (get_num_emojis() + emoji_cols - 1) / emoji_cols;
            float emoji_section_top = base_y + 40;
            float emoji_section_bottom = base_y - (emoji_rows - 1) * 40;
            float color_section_top = emoji_section_bottom - 20;
            float color_section_bottom = color_section_top - 40;
            float tabs_section_top = color_section_bottom - 20;
            float tabs_section_bottom = tabs_section_top - get_tab_count() * 40;
            float tools_section_top = tabs_section_bottom - 20;
            float tools_section_bottom = tools_section_top - 30;
            float map_window_y = tools_section_bottom - 210;
            
            bool in_map_window = (x >= sidebar_x && x < sidebar_x + get_sidebar_width() - 20 && 
                                 gl_y >= map_window_y && gl_y <= map_window_y + 200);
            
            fprintf(stderr, "Checking if right-click is in maps window:\n");
            fprintf(stderr, "  Click position: (%d, %d)\n", x, y);
            fprintf(stderr, "  Gl_y: %f\n", gl_y);
            fprintf(stderr, "  Sidebar x range: %f to %f\n", sidebar_x, sidebar_x + get_sidebar_width() - 20);
            fprintf(stderr, "  Map window y range: %f to %f\n", map_window_y, map_window_y + 200);
            fprintf(stderr, "  In map window: %s\n", in_map_window ? "yes" : "no");
            
            if (in_map_window) {
                // Show maps context menu
                fprintf(stderr, "Right-click in maps window detected at (%d, %d)\n", x, y);
                EventMenu* context_menu = create_maps_context_menu(x, gl_y);
                if (context_menu) {
                    fprintf(stderr, "Maps context menu created successfully\n");
                    // Hide other menus
                    if (main_event_menu) hide_event_menu(main_event_menu);
                    if (event_area_menu) hide_event_menu(event_area_menu);
                    if (event_commands_menu) hide_event_menu(event_commands_menu);
                    if (contents_context_menu) hide_event_menu(contents_context_menu);
                    
                    // Show the maps context menu at the click position
                    context_menu->x = x;
                    context_menu->y = gl_y - context_menu->height; // Position above the click
                    // Make sure the menu is within the window bounds
                    if (context_menu->x + context_menu->width > get_window_width()) {
                        context_menu->x = get_window_width() - context_menu->width;
                    }
                    if (context_menu->y < 0) {
                        context_menu->y = 0;
                    }
                    show_event_menu(context_menu);
                    fprintf(stderr, "Maps context menu positioned at (%d, %d)\n", context_menu->x, context_menu->y);
                    
                    // Store the context menu in maps_context_menu for handling
                    if (maps_context_menu) {
                        destroy_event_menu(maps_context_menu);
                    }
                    maps_context_menu = context_menu;
                } else {
                    fprintf(stderr, "Failed to create maps context menu\n");
                }
            } else {
                // Create and show context menu with options
                // "new, edit, cut, copy, paste, delete, find, quick event, set starting position"
                EventMenu* context_menu = create_main_event_menu(x, gl_y);
                if (context_menu) {
                    // Update menu items to match the context menu options
                    context_menu->item_count = 9;
                    
                    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
                    void* item0 = (char*)context_menu->items + 0 * item_size;
                    set_event_menu_item_label(item0, "New");
                    set_event_menu_item_id(item0, 0);
                    set_event_menu_item_is_category(item0, 0);
                    set_event_menu_item_page(item0, 0);
                    
                    void* item1 = (char*)context_menu->items + 1 * item_size;
                    set_event_menu_item_label(item1, "Edit");
                    set_event_menu_item_id(item1, 1);
                    set_event_menu_item_is_category(item1, 0);
                    set_event_menu_item_page(item1, 0);
                    
                    void* item2 = (char*)context_menu->items + 2 * item_size;
                    set_event_menu_item_label(item2, "Cut");
                    set_event_menu_item_id(item2, 2);
                    set_event_menu_item_is_category(item2, 0);
                    set_event_menu_item_page(item2, 0);
                    
                    void* item3 = (char*)context_menu->items + 3 * item_size;
                    set_event_menu_item_label(item3, "Copy");
                    set_event_menu_item_id(item3, 3);
                    set_event_menu_item_is_category(item3, 0);
                    set_event_menu_item_page(item3, 0);
                    
                    void* item4 = (char*)context_menu->items + 4 * item_size;
                    set_event_menu_item_label(item4, "Paste");
                    set_event_menu_item_id(item4, 4);
                    set_event_menu_item_is_category(item4, 0);
                    set_event_menu_item_page(item4, 0);
                    
                    void* item5 = (char*)context_menu->items + 5 * item_size;
                    set_event_menu_item_label(item5, "Delete");
                    set_event_menu_item_id(item5, 5);
                    set_event_menu_item_is_category(item5, 0);
                    set_event_menu_item_page(item5, 0);
                    
                    void* item6 = (char*)context_menu->items + 6 * item_size;
                    set_event_menu_item_label(item6, "Find");
                    set_event_menu_item_id(item6, 6);
                    set_event_menu_item_is_category(item6, 0);
                    set_event_menu_item_page(item6, 0);
                    
                    void* item7 = (char*)context_menu->items + 7 * item_size;
                    set_event_menu_item_label(item7, "Quick Event");
                    set_event_menu_item_id(item7, 7);
                    set_event_menu_item_is_category(item7, 0);
                    set_event_menu_item_page(item7, 0);
                    
                    void* item8 = (char*)context_menu->items + 8 * item_size;
                    set_event_menu_item_label(item8, "Set Starting Position");
                    set_event_menu_item_id(item8, 8);
                    set_event_menu_item_is_category(item8, 0);
                    set_event_menu_item_page(item8, 0);
                    
                    // Hide other menus
                    if (main_event_menu) hide_event_menu(main_event_menu);
                    if (event_area_menu) hide_event_menu(event_area_menu);
                    if (event_commands_menu) hide_event_menu(event_commands_menu);
                    
                    // Show the context menu at the click position
                    context_menu->x = x;
                    context_menu->y = gl_y - context_menu->height; // Position above the click
                    // Make sure the menu is within the window bounds
                    if (context_menu->x + context_menu->width > get_window_width()) {
                        context_menu->x = get_window_width() - context_menu->width;
                    }
                    if (context_menu->y < 0) {
                        context_menu->y = 0;
                    }
                    show_event_menu(context_menu);
                    
                    // Store the context menu in main_event_menu for handling
                    if (main_event_menu) {
                        destroy_event_menu(main_event_menu);
                    }
                    main_event_menu = context_menu;
                }
            }
            
            trigger_view_update();
        }
    }
}

// Helper function to handle space key action
void handle_space_key(void) {
    // Space bar: Move selector to mouse position in 3D mode, paint in 2D mode
    if (get_view_mode() == 1 && get_mouse_in_window()) {
        // 3D mode - move selector to mouse position
        int mouse_x = get_mouse_x();
        int mouse_y = get_mouse_y();
        int row, col, layer;
        // Use raycasting to determine where the mouse is pointing
        if (find_closest_intersected_block(mouse_x, mouse_y, &row, &col, &layer)) {
            set_selector_row(row);
            set_selector_col(col);
            char coord_msg[100];
            // Invert the Y coordinate for display to match 2D coordinate system
            int display_row = get_canvas_rows() - 1 - row;
            snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", col, display_row, layer);
            set_status_message(coord_msg);
        }
    } else {
        // 2D mode or no mouse - paint at current selector position
        char coord_msg[100];
        if (current_tool_state == TOOL_STATE_PAINT) {
            handle_paint_tool_action();
        } else if (current_tool_state == TOOL_STATE_FILL) {
            handle_fill_tool_action();
        } else if (current_tool_state == TOOL_STATE_RECTANGLE) {
            handle_rectangle_tool_action();
        } else if (current_tool_state == TOOL_STATE_ERASE) { // Erase
            handle_erase_tool_action();
        } else if (current_tool_state == TOOL_STATE_ERASE_FILL) {
            handle_erase_fill_tool_action();
        }
    }
}

// Helper function to handle enter key action
void handle_enter_key(void) {
    char coord_msg[100];
    if (current_tool_state == TOOL_STATE_PAINT) {
        handle_paint_tool_action();
    } else if (current_tool_state == TOOL_STATE_FILL) {
        handle_fill_tool_action();
    } else if (current_tool_state == TOOL_STATE_RECTANGLE) {
        handle_rectangle_tool_action();
    } else if (current_tool_state == TOOL_STATE_ERASE) { // Erase
        if (get_start_row() == -1) {
            handle_erase_tool_action();
        } else {
            erase_rectangle(get_current_z_level(), get_start_row(), get_start_col(), get_selector_row(), get_selector_col());
            set_start_row(-1);
            set_start_col(-1);
            char coord_msg[100];
            snprintf(coord_msg, sizeof(coord_msg), "Rectangle erased from (%d, %d) to (%d, %d) at Z=%d", 
                     get_start_col(), get_start_row(), get_selector_col(), get_selector_row(), get_current_z_level());
            set_status_message(coord_msg);
        }
    } else if (current_tool_state == TOOL_STATE_ERASE_FILL) {
        handle_erase_fill_tool_action();
    }
    print_ascii_grid();
    trigger_view_update();
}

// Helper function to handle tool selection keys
void handle_tool_selection_keys(unsigned char key) {
    switch (key) {
        case 'f':
            set_tool_state(TOOL_STATE_FILL);
            break;
        case '4':
            set_tool_state(TOOL_STATE_RECTANGLE);
            break;
        case '0':
            set_tool_state(TOOL_STATE_ERASE);
            break;
    }
}

// Helper function to handle emoji and color selection keys
void handle_emoji_color_selection_keys(unsigned char key) {
    switch (key) {
        case '1':
            set_selected_emoji((get_selected_emoji() + 1) % get_num_emojis());
            set_status_message("Emoji selected");
            break;
        case '3':
            set_selected_fg_color((get_selected_fg_color() + 1) % get_num_colors());
            set_status_message("Color selected");
            break;
    }
}

// Helper function to handle layer and view keys
void handle_layer_view_keys(unsigned char key) {
    switch (key) {
        case '2':
            set_show_all_layers(!get_show_all_layers());
            set_status_message(get_show_all_layers() ? "Showing all layers" : "Showing top layer");
            break;
        case 'c':
            if (get_current_z_level() > 0) {
                set_current_z_level(get_current_z_level() - 1);
                set_status_message("Z-level Decreased");
            }
            break;
        case 'v':
            if (get_current_z_level() < get_max_layers() - 1) {
                set_current_z_level(get_current_z_level() + 1);
                set_status_message("Z-level Increased");
            }
            break;
    }
}

// Helper function to handle file operations keys
void handle_file_operation_keys(unsigned char key) {
    switch (key) {
        case 'l':
            load_canvas();
            set_status_message("Canvas loaded");
            break;
        case 's':
            if (get_view_mode() == 1) {
                move_camera_backward(10.0f);
                set_status_message("Camera Backward");
            } else {
                save_canvas();
                set_status_message("Canvas saved");
                glutPostRedisplay();
            }
            break;
    }
}

// Helper function to handle camera control keys
void handle_camera_control_keys(unsigned char key) {
    switch (key) {
        case 'w':
            if (get_view_mode() == 1) {
                move_camera_forward(10.0f);
                set_status_message("Camera Forward");
            }
            break;
        case 'a':
            if (get_view_mode() == 1) {
                strafe_camera(-10.0f);
                set_status_message("Camera Strafe Left");
            }
            break;
        case 's':
            if (get_view_mode() == 1) {
                move_camera_backward(10.0f);
                set_status_message("Camera Backward");
            }
            break;
        case 'd':
            if (get_view_mode() == 1) {
                strafe_camera(10.0f);
                set_status_message("Camera Strafe Right");
            }
            break;
        case 'z':
            if (get_view_mode() == 1) {
                elevate_camera(10.0f);
                set_status_message("Camera Elevate Up");
            } else if (get_current_z_level() > 0) {
                set_current_z_level(get_current_z_level() - 1);
                set_status_message("Z-level Decreased");
            }
            break;
        case 'x':
            if (get_view_mode() == 1) {
                elevate_camera(-10.0f);
                set_status_message("Camera Elevate Down");
            } else if (get_current_z_level() < get_max_layers() - 1) {
                set_current_z_level(get_current_z_level() + 1);
                set_status_message("Z-level Increased");
            }
            break;
        case 'q':
            if (get_view_mode() == 1) {
                rotate_camera_yaw(-90.0f);
                set_status_message("Camera Rotated Left 90°");
            }
            break;
        case 'e':
            if (get_view_mode() == 1) {
                rotate_camera_yaw(90.0f);
                set_status_message("Camera Rotated Right 90°");
            }
            break;
        case 'r':
            if (get_view_mode() == 1) {
                rotate_camera_pitch(90.0f);
                set_status_message("Camera Rotated Up 90°");
            }
            break;
        case 't':
            if (get_view_mode() == 1) {
                rotate_camera_pitch(-90.0f);
                set_status_message("Camera Rotated Down 90°");
            }
            break;
        case 'y':
            if (get_view_mode() == 1) {
                reset_camera();
                set_status_message("Camera Reset to Default Position");
            }
            break;
    }
}

// Helper function to handle tab creation key
void handle_tab_creation_key(void) {
    if (get_tab_count() < get_max_tabs()) {
        set_tab_bank(get_tab_count(), 0, get_selected_emoji());
        set_tab_bank(get_tab_count(), 1, get_selected_fg_color());
        set_tab_bank(get_tab_count(), 2, get_selected_bg_color());
        increment_tab_count();
        set_status_message("Tab created");
    }
}

void keyboard(unsigned char key, int x, int y) {
    // Handle text input dialog first
    if (text_input_dialog && text_input_dialog->visible) {
        handle_text_input_dialog_key(text_input_dialog, key);
        glutPostRedisplay();
        return;
    }
    // Handle input mode first
    if (input_mode == 1) {  // Getting project name
        if (key == '\r' || key == '\n') {  // Enter key
            // Process the input
            if (strlen(input_buffer) > 0) {
                if (file_menu_state == 2) {  // New project
                    if (create_new_project(input_buffer) == 0) {
                        strncpy(current_project_path, input_buffer, sizeof(current_project_path) - 1);
                        current_project_path[sizeof(current_project_path) - 1] = '\0';
                        set_status_message("New project created");
                    } else {
                        set_status_message("Failed to create new project");
                    }
                }
                // Reset input mode
                input_mode = 0;
                input_cursor = 0;
                input_buffer[0] = '\0';
                hide_file_menu();
            }
            return;
        } else if (key == '\b' || key == 127) {  // Backspace or Delete
            // Handle backspace
            if (input_cursor > 0) {
                input_cursor--;
                input_buffer[input_cursor] = '\0';
                print_ascii_grid();
                trigger_view_update();
            }
            return;
        } else if (key >= 32 && key <= 126) {  // Printable characters
            // Add character to input buffer
            if (input_cursor < sizeof(input_buffer) - 1) {
                input_buffer[input_cursor] = key;
                input_cursor++;
                input_buffer[input_cursor] = '\0';
                print_ascii_grid();
                trigger_view_update();
            }
            return;
        }
    }
    
    // Handle Ctrl+C to exit
    if (key == 'c' && (glutGetModifiers() & GLUT_ACTIVE_CTRL)) {
        restore_terminal();
        exit(0);
    }

    // Handle Ctrl+S to save
    if (key == 's' && (glutGetModifiers() & GLUT_ACTIVE_CTRL)) {
        if (current_project_path[0] != '\0') {
            save_project(current_project_path);
            set_status_message("Project saved");
        } else {
            // Enter input mode for project name
            show_file_menu();
            file_menu_state = 2;  // New project input mode
            input_mode = 1;  // Getting project name
            input_cursor = 0;
            input_buffer[0] = '\0';
            set_status_message("Enter project name:");
        }
        glutPostRedisplay();
        return;
    }

    // Handle Ctrl+0 for erase fill
    if (key == '0' && (glutGetModifiers() & GLUT_ACTIVE_CTRL)) {
        set_tool_state(TOOL_STATE_ERASE_FILL);
        return;
    }

    // Handle camera controls directly (like in the working version)
    if (key == 'w') {
        if (get_view_mode() == 1) {
            move_camera_forward(10.0f);
            set_status_message("Camera Forward");
        }
    } else if (key == 's') {
        if (get_view_mode() == 1) {
            move_camera_backward(10.0f);
            set_status_message("Camera Backward");
        }
    } else if (key == 'a') {
        if (get_view_mode() == 1) {
            strafe_camera(-10.0f);
            set_status_message("Camera Strafe Left");
        }
    } else if (key == 'd') {
        if (get_view_mode() == 1) {
            strafe_camera(10.0f);
            set_status_message("Camera Strafe Right");
        }
    } else if (key == ' ') {
        handle_space_key();
    } else if (key == '\r') { // Enter key - always paint at current selector position
        handle_enter_key();
        print_ascii_grid();
        trigger_view_update();
        return; // Return early since we handled the click
    } else if (key == 'f') {
        set_tool_state(TOOL_STATE_FILL);
    } else if (key == '4') {
        set_tool_state(TOOL_STATE_RECTANGLE);
    } else if (key == '1') {
        set_selected_emoji((get_selected_emoji() + 1) % get_num_emojis());
        set_status_message("Emoji selected");
    } else if (key == '3') {
        set_selected_fg_color((get_selected_fg_color() + 1) % get_num_colors());
        set_status_message("Color selected");
    } else if (key == '2') {
        set_show_all_layers(!get_show_all_layers());
        set_status_message(get_show_all_layers() ? "Showing all layers" : "Showing top layer");
    } else if (key == 'l') {
        load_canvas();
        set_status_message("Canvas loaded");
    } else if (key == '5' && get_tab_count() < get_max_tabs()) {
        set_tab_bank(get_tab_count(), 0, get_selected_emoji());
        set_tab_bank(get_tab_count(), 1, get_selected_fg_color());
        set_tab_bank(get_tab_count(), 2, get_selected_bg_color());
        increment_tab_count();
        set_status_message("Tab created");
    } else if (key == 'z') {
        if (get_view_mode() == 1) {
            elevate_camera(10.0f);
            set_status_message("Camera Elevate Up");
        } else if (get_current_z_level() > 0) {
            set_current_z_level(get_current_z_level() - 1);
            set_status_message("Z-level Decreased");
        }
    } else if (key == '0') {
        set_tool_state(TOOL_STATE_ERASE);
    } else if (key == 'x') {
        if (get_view_mode() == 1) {
            elevate_camera(-10.0f);
            set_status_message("Camera Elevate Down");
        } else if (get_current_z_level() < get_max_layers() - 1) {
            set_current_z_level(get_current_z_level() + 1);
            set_status_message("Z-level Increased");
        }
    } else if (key == 'c') {
        if (get_current_z_level() > 0) {
            set_current_z_level(get_current_z_level() - 1);
            set_status_message("Z-level Decreased");
        }
    } else if (key == 'v') {
        if (get_current_z_level() < get_max_layers() - 1) {
            set_current_z_level(get_current_z_level() + 1);
            set_status_message("Z-level Increased");
        }
    } else if (key == 'q') {
        if (get_view_mode() == 1) {
            rotate_camera_yaw(-90.0f);
            set_status_message("Camera Rotated Left 90°");
        }
    } else if (key == 'e') {
        if (get_view_mode() == 1) {
            rotate_camera_yaw(90.0f);
            set_status_message("Camera Rotated Right 90°");
        }
    } else if (key == 'r') {
        if (get_view_mode() == 1) {
            rotate_camera_pitch(90.0f);
            set_status_message("Camera Rotated Up 90°");
        }
    } else if (key == 't') {
        if (get_view_mode() == 1) {
            rotate_camera_pitch(-90.0f);
            set_status_message("Camera Rotated Down 90°");
        }
    } else if (key == 'y') {
        if (get_view_mode() == 1) {
            reset_camera();
            set_status_message("Camera Reset to Default Position");
        }
    }

    print_ascii_grid();
    trigger_view_update();
}

void special(int key, int x, int y) {
    // Handle text input dialog special keys
    if (text_input_dialog && text_input_dialog->visible) {
        handle_text_input_dialog_special(text_input_dialog, key);
        glutPostRedisplay();
        return;
    }
    switch (key) {
        case GLUT_KEY_UP:
            if (get_view_mode() == 0) {
                // 2D mode: move selector up (decrease row)
                if (get_selector_row() > 0) {
                    set_selector_row(get_selector_row() - 1);
                    char coord_msg[100];
                    snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", 
                             get_selector_col(), get_selector_row(), get_current_z_level());
                    set_status_message(coord_msg);
                }
            } else if (get_view_mode() == 1) {
                // 3D mode: move selector down (increase row) - swapped behavior
                if (get_selector_row() < get_canvas_rows() - 1) {
                    set_selector_row(get_selector_row() + 1);
                    char coord_msg[100];
                    // Invert the Y coordinate for display to match 2D coordinate system
                    int display_row = get_canvas_rows() - 1 - get_selector_row();
                    snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", 
                             get_selector_col(), display_row, get_current_z_level());
                    set_status_message(coord_msg);
                }
            }
            break;
        case GLUT_KEY_DOWN:
            if (get_view_mode() == 0) {
                // 2D mode: move selector down (increase row)
                if (get_selector_row() < get_canvas_rows() - 1) {
                    set_selector_row(get_selector_row() + 1);
                    char coord_msg[100];
                    snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", 
                             get_selector_col(), get_selector_row(), get_current_z_level());
                    set_status_message(coord_msg);
                }
            } else if (get_view_mode() == 1) {
                // 3D mode: move selector up (decrease row) - swapped behavior
                if (get_selector_row() > 0) {
                    set_selector_row(get_selector_row() - 1);
                    char coord_msg[100];
                    // Invert the Y coordinate for display to match 2D coordinate system
                    int display_row = get_canvas_rows() - 1 - get_selector_row();
                    snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", 
                             get_selector_col(), display_row, get_current_z_level());
                    set_status_message(coord_msg);
                }
            }
            break;
        case GLUT_KEY_LEFT:
            if (get_view_mode() == 0 || get_view_mode() == 1) {
                if (get_selector_col() > 0) {
                    set_selector_col(get_selector_col() - 1);
                    char coord_msg[100];
                    if (get_view_mode() == 0) {
                        snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", 
                                 get_selector_col(), get_selector_row(), get_current_z_level());
                    } else {
                        // Invert the Y coordinate for display to match 2D coordinate system
                        int display_row = get_canvas_rows() - 1 - get_selector_row();
                        snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", 
                                 get_selector_col(), display_row, get_current_z_level());
                    }
                    set_status_message(coord_msg);
                }
            }
            break;
        case GLUT_KEY_RIGHT:
            if (get_view_mode() == 0 || get_view_mode() == 1) {
                if (get_selector_col() < get_canvas_cols() - 1) {
                    set_selector_col(get_selector_col() + 1);
                    char coord_msg[100];
                    if (get_view_mode() == 0) {
                        snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", 
                                 get_selector_col(), get_selector_row(), get_current_z_level());
                    } else {
                        // Invert the Y coordinate for display to match 2D coordinate system
                        int display_row = get_canvas_rows() - 1 - get_selector_row();
                        snprintf(coord_msg, sizeof(coord_msg), "Selector moved to (%d, %d, %d)", 
                                 get_selector_col(), display_row, get_current_z_level());
                    }
                    set_status_message(coord_msg);
                }
            }
            break;
    }
    print_ascii_grid();
    trigger_view_update();
}

void idle(void) {
    // Process terminal input
    int ch = check_terminal_input();
    if (ch != -1) {
        // Handle input mode first
        if (input_mode == 1) {  // Getting project name
            if (ch == '\r' || ch == '\n') {  // Enter key
                // Process the input
                if (strlen(input_buffer) > 0) {
                    if (file_menu_state == 2) {  // New project
                        if (create_new_project(input_buffer) == 0) {
                            strncpy(current_project_path, input_buffer, sizeof(current_project_path) - 1);
                            current_project_path[sizeof(current_project_path) - 1] = '\0';
                            set_status_message("New project created");
                        } else {
                            set_status_message("Failed to create new project");
                        }
                    }
                    // Reset input mode
                    input_mode = 0;
                    input_cursor = 0;
                    input_buffer[0] = '\0';
                    hide_file_menu();
                }
                print_ascii_grid();
                trigger_view_update();
                return;
            } else if (ch == '\b' || ch == 127) {  // Backspace or Delete
                // Handle backspace
                if (input_cursor > 0) {
                    input_cursor--;
                    input_buffer[input_cursor] = '\0';
                    print_ascii_grid();
                    trigger_view_update();
                }
                return;
            } else if (ch >= 32 && ch <= 126) {  // Printable characters
                // Add character to input buffer
                if (input_cursor < sizeof(input_buffer) - 1) {
                    input_buffer[input_cursor] = ch;
                    input_cursor++;
                    input_buffer[input_cursor] = '\0';
                    print_ascii_grid();
                    trigger_view_update();
                }
                return;
            }
        }
        
        enqueue_terminal_event(ch);
    }
    
    // Process events from the queue
    int event[INPUT_EVENT_FIELDS];
    while (dequeue_event(event)) {
        // For now, we'll just handle terminal input events
        // In a full implementation, we would handle all event types
        if (get_input_event_type(event) == INPUT_EVENT_TERMINAL_INPUT) {
            ch = get_input_event_terminal_ch(event);
            
            // Handle input mode first
            if (input_mode == 1) {  // Getting project name
                if (ch == '\r' || ch == '\n') {  // Enter key
                    // Process the input
                    if (strlen(input_buffer) > 0) {
                        if (file_menu_state == 2) {  // New project
                            if (create_new_project(input_buffer) == 0) {
                                strncpy(current_project_path, input_buffer, sizeof(current_project_path) - 1);
                                current_project_path[sizeof(current_project_path) - 1] = '\0';
                                set_status_message("New project created");
                            } else {
                                set_status_message("Failed to create new project");
                            }
                        }
                        // Reset input mode
                        input_mode = 0;
                        input_cursor = 0;
                        input_buffer[0] = '\0';
                        hide_file_menu();
                    }
                    print_ascii_grid();
                    trigger_view_update();
                    continue;
                } else if (ch == '\b' || ch == 127) {  // Backspace or Delete
                    // Handle backspace
                    if (input_cursor > 0) {
                        input_cursor--;
                        input_buffer[input_cursor] = '\0';
                        print_ascii_grid();
                        trigger_view_update();
                    }
                    continue;
                } else if (ch >= 32 && ch <= 126) {  // Printable characters
                    // Add character to input buffer
                    if (input_cursor < sizeof(input_buffer) - 1) {
                        input_buffer[input_cursor] = ch;
                        input_cursor++;
                        input_buffer[input_cursor] = '\0';
                        print_ascii_grid();
                        trigger_view_update();
                    }
                    continue;
                }
            }
            
            // Handle Ctrl+C to exit
            if (ch == 'c' && (glutGetModifiers() & GLUT_ACTIVE_CTRL)) {
                restore_terminal();
                exit(0);
            }

            // Handle Ctrl+S to save
            if (ch == 's' && (glutGetModifiers() & GLUT_ACTIVE_CTRL)) {
                if (current_project_path[0] != '\0') {
                    save_project(current_project_path);
                    set_status_message("Project saved");
                } else {
                    // Enter input mode for project name
                    show_file_menu();
                    file_menu_state = 2;  // New project input mode
                    input_mode = 1;  // Getting project name
                    input_cursor = 0;
                    input_buffer[0] = '\0';
                    set_status_message("Enter project name:");
                }
                print_ascii_grid();
                trigger_view_update();
                continue;
            }

            // Handle Ctrl+0 for erase fill
            if (ch == '0' && (glutGetModifiers() & GLUT_ACTIVE_CTRL)) {
                set_tool_state(TOOL_STATE_ERASE_FILL);
                print_ascii_grid();
                trigger_view_update();
                continue;
            }

            // Handle camera controls directly (like in the working version)
            if (ch == 'w') {
                if (get_view_mode() == 1) {
                    move_camera_forward(10.0f);
                    set_status_message("Camera Forward");
                }
            } else if (ch == 's') {
                if (get_view_mode() == 1) {
                    move_camera_backward(10.0f);
                    set_status_message("Camera Backward");
                }
            } else if (ch == 'a') {
                if (get_view_mode() == 1) {
                    strafe_camera(-10.0f);
                    set_status_message("Camera Strafe Left");
                }
            } else if (ch == 'd') {
                if (get_view_mode() == 1) {
                    strafe_camera(10.0f);
                    set_status_message("Camera Strafe Right");
                }
            } else if (ch == ' ') {
                handle_space_key();
            } else if (ch == '\r') {
                handle_enter_key();
                print_ascii_grid();
                trigger_view_update();
                continue; // Return early since we handled the click
            } else if (ch == 'f') {
                set_tool_state(TOOL_STATE_FILL);
            } else if (ch == '4') {
                set_tool_state(TOOL_STATE_RECTANGLE);
            } else if (ch == '1') {
                set_selected_emoji((get_selected_emoji() + 1) % get_num_emojis());
                set_status_message("Emoji selected");
            } else if (ch == '3') {
                set_selected_fg_color((get_selected_fg_color() + 1) % get_num_colors());
                set_status_message("Color selected");
            } else if (ch == '2') {
                set_show_all_layers(!get_show_all_layers());
                set_status_message(get_show_all_layers() ? "Showing all layers" : "Showing top layer");
            } else if (ch == 'l') {
                load_canvas();
                set_status_message("Canvas loaded");
            } else if (ch == '5' && get_tab_count() < get_max_tabs()) {
                set_tab_bank(get_tab_count(), 0, get_selected_emoji());
                set_tab_bank(get_tab_count(), 1, get_selected_fg_color());
                set_tab_bank(get_tab_count(), 2, get_selected_bg_color());
                increment_tab_count();
                set_status_message("Tab created");
            } else if (ch == 'z') {
                if (get_view_mode() == 1) {
                    elevate_camera(10.0f);
                    set_status_message("Camera Elevate Up");
                } else if (get_current_z_level() > 0) {
                    set_current_z_level(get_current_z_level() - 1);
                    set_status_message("Z-level Decreased");
                }
            } else if (ch == '0') {
                set_tool_state(TOOL_STATE_ERASE);
            } else if (ch == 'x') {
                if (get_view_mode() == 1) {
                    elevate_camera(-10.0f);
                    set_status_message("Camera Elevate Down");
                } else if (get_current_z_level() < get_max_layers() - 1) {
                    set_current_z_level(get_current_z_level() + 1);
                    set_status_message("Z-level Increased");
                }
            } else if (ch == 'c') {
                if (get_current_z_level() > 0) {
                    set_current_z_level(get_current_z_level() - 1);
                    set_status_message("Z-level Decreased");
                }
            } else if (ch == 'v') {
                if (get_current_z_level() < get_max_layers() - 1) {
                    set_current_z_level(get_current_z_level() + 1);
                    set_status_message("Z-level Increased");
                }
            } else if (ch == 'q') {
                if (get_view_mode() == 1) {
                    rotate_camera_yaw(-90.0f);
                    set_status_message("Camera Rotated Left 90°");
                }
            } else if (ch == 'e') {
                if (get_view_mode() == 1) {
                    rotate_camera_yaw(90.0f);
                    set_status_message("Camera Rotated Right 90°");
                }
            } else if (ch == 'r') {
                if (get_view_mode() == 1) {
                    rotate_camera_pitch(90.0f);
                    set_status_message("Camera Rotated Up 90°");
                }
            } else if (ch == 't') {
                if (get_view_mode() == 1) {
                    rotate_camera_pitch(-90.0f);
                    set_status_message("Camera Rotated Down 90°");
                }
            } else if (ch == 'y') {
                if (get_view_mode() == 1) {
                    reset_camera();
                    set_status_message("Camera Reset to Default Position");
                }
            }

            print_ascii_grid();
            trigger_view_update();
        }
    }
    
    if (ch != -1 || !is_event_queue_empty()) {
        print_ascii_grid();
        trigger_view_update();
    }
}


// File menu functions
void show_file_menu(void) {
    fprintf(stderr, "show_file_menu called\n");
    file_menu_visible = 1;
    file_menu_state = 1;  // Main menu
}

void hide_file_menu(void) {
    fprintf(stderr, "hide_file_menu called\n");
    file_menu_visible = 0;
    file_menu_state = 0;  // Hidden
    // Free project list when hiding menu
    free_project_list();
}

// External declarations for drawing functions
extern void draw_rect(float x, float y, float w, float h, float color[3]);
extern void draw_border(float x, float y, float w, float h, float color[3]);
extern void render_text(const char* str, float x, float y);

// External function declarations
extern char** list_projects(int* count);
extern void free_project_list(void);

void render_file_menu(void) {
    // If menu is not visible, nothing to render
    if (!file_menu_visible || file_menu_state == 0) {
        return;
    }
    
    // Set file menu position
    file_menu_y = get_window_height() - HEADER_STRIP_HEIGHT - 10;
    
    float tab_color[3] = {0.5f, 0.5f, 0.5f};
    float sel_color[3] = {1.0f, 1.0f, 0.0f};
    
    int menu_width = 120;
    int menu_height = 90;
    int item_height = 30;
    int menu_x = 10; // Same as file button x position
    int menu_y = get_window_height() - HEADER_STRIP_HEIGHT - menu_height - 5; // Above the header strip
    
    // Draw menu background
    draw_rect(menu_x, menu_y, menu_width, menu_height, tab_color);
    draw_border(menu_x, menu_y, menu_width, menu_height, sel_color);
    
    // Draw menu items based on state
    switch (file_menu_state) {
        case 1: // Main menu
            render_text("New", menu_x + 10, menu_y + menu_height - 20);
            render_text("Load", menu_x + 10, menu_y + menu_height - 40);
            render_text("Save", menu_x + 10, menu_y + menu_height - 60);
            break;
        case 2: // New project input mode
            if (input_mode == 1) {
                // Display input buffer
                char display_text[280];
                snprintf(display_text, sizeof(display_text), "Name: %s_", input_buffer);
                render_text(display_text, menu_x + 10, menu_y + menu_height - 20);
            } else {
                render_text("Enter project name", menu_x + 10, menu_y + menu_height - 20);
            }
            break;
        case 3: // Load project
            render_text("Load Project", menu_x + 10, menu_y + menu_height - 20);
            // List projects
            if (!project_list) {
                project_list = list_projects(&project_count);
            }
            if (project_list && project_count > 0) {
                for (int i = 0; i < project_count && i < 3; i++) {
                    if (project_list[i]) {  // Check if the project name is not NULL
                        float item_y = menu_y + menu_height - 50 - i * 20;
                        render_text(project_list[i], menu_x + 10, item_y);
                    }
                }
            } else {
                render_text("No projects found", menu_x + 10, menu_y + menu_height - 50);
            }
            break;
    }
}

void handle_file_menu_click(int x, int y) {
    // Add debug output
    fprintf(stderr, "handle_file_menu_click: x=%d, y=%d, file_menu_x=%d, file_menu_y=%d, file_menu_visible=%d\n", 
            x, y, file_menu_x, file_menu_y, file_menu_visible);
    
    // Check if click is on the file menu button
    if (x >= file_menu_x && x < file_menu_x + file_menu_width && 
        y >= file_menu_y && y < file_menu_y + file_menu_height) {
        fprintf(stderr, "Click on file menu button\n");
        // Toggle menu visibility
        if (file_menu_visible) {
            fprintf(stderr, "Hiding file menu\n");
            hide_file_menu();
        } else {
            fprintf(stderr, "Showing file menu\n");
            show_file_menu();
        }
        return;
    }
    
    // If menu is not visible, nothing more to do
    if (!file_menu_visible) {
        fprintf(stderr, "File menu not visible, returning\n");
        return;
    }
    
    // Check if click is within the menu
    int menu_width = 120;
    int menu_height = 90;
    if (x >= file_menu_x && x < file_menu_x + menu_width && 
        y >= file_menu_y - menu_height && y < file_menu_y) {
        fprintf(stderr, "Click within menu area\n");
        
        // Handle menu item clicks based on state
        switch (file_menu_state) {
            case 1: // Main menu
                fprintf(stderr, "Handling main menu click\n");
                if (y >= file_menu_y - 20 && y < file_menu_y) {
                    // New
                    fprintf(stderr, "New menu item clicked\n");
                    // Enter input mode for project name
                    file_menu_state = 2;  // New project input mode
                    input_mode = 1;  // Getting project name
                    input_cursor = 0;
                    input_buffer[0] = '\0';
                    set_status_message("Enter project name:");
                } else if (y >= file_menu_y - 40 && y < file_menu_y - 20) {
                    // Load
                    fprintf(stderr, "Load menu item clicked\n");
                    file_menu_state = 3;  // Load project
                } else if (y >= file_menu_y - 60 && y < file_menu_y - 40) {
                    // Save
                    fprintf(stderr, "Save menu item clicked\n");
                    if (current_project_path[0] != '\0') {
                        save_project(current_project_path);
                        set_status_message("Project saved");
                        hide_file_menu();
                    } else {
                        // Enter input mode for project name
                        file_menu_state = 2;  // New project input mode
                        input_mode = 1;  // Getting project name
                        input_cursor = 0;
                        input_buffer[0] = '\0';
                        set_status_message("Enter project name:");
                    }
                }
                break;
            case 3: // Load project
                fprintf(stderr, "Handling load project click\n");
                // Handle project selection
                if (!project_list) {
                    fprintf(stderr, "Project list is NULL, calling list_projects\n");
                    project_list = list_projects(&project_count);
                    fprintf(stderr, "list_projects returned: project_list=%p, project_count=%d\n", project_list, project_count);
                }
                if (project_list && project_count > 0) {
                    fprintf(stderr, "Processing %d projects\n", project_count);
                    for (int i = 0; i < project_count && i < 3; i++) {
                        float item_y = file_menu_y - 50 - i * 20;
                        if (y >= item_y && y < item_y + 20) {
                            fprintf(stderr, "Project %d clicked: %s\n", i, project_list[i] ? project_list[i] : "NULL");
                            if (project_list[i]) {
                                // Copy the project name before freeing the list
                                char project_name[256];
                                strncpy(project_name, project_list[i], sizeof(project_name) - 1);
                                project_name[sizeof(project_name) - 1] = '\0';
                                
                                if (load_project(project_list[i]) == 0) {
                                    // current_project_path is already set by load_project
                                    hide_file_menu();
                                    char msg[256];
                                    // Truncate project_name if necessary to prevent buffer overflow
                                    char truncated_name[240]; // Leave room for "Loaded project: " prefix
                                    strncpy(truncated_name, project_name, sizeof(truncated_name) - 1);
                                    truncated_name[sizeof(truncated_name) - 1] = '\0';
                                    snprintf(msg, sizeof(msg), "Loaded project: %s", truncated_name);
                                    set_status_message(msg);
                                } else {
                                    fprintf(stderr, "Failed to load project: %s\n", project_list[i]);
                                    hide_file_menu();
                                    set_status_message("Failed to load project");
                                }
                            } else {
                                fprintf(stderr, "Project name at index %d is NULL\n", i);
                            }
                            break;
                        }
                    }
                } else {
                    fprintf(stderr, "No projects found or project list is NULL\n");
                    set_status_message("No projects found");
                }
                break;
        }
    } else {
        // Click outside menu, hide it
        fprintf(stderr, "Click outside menu, hiding\n");
        hide_file_menu();
    }
}

// Helper function to read a line from stdin
int read_line(char* buffer, int size) {
    if (fgets(buffer, size, stdin) == NULL) {
        return -1; // Error or EOF
    }
    
    // Remove newline character if present
    int len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    return 0; // Success
}

int create_new_project(const char* project_name) {
    // Create a directory for the new project in the projects/ directory
    struct stat st = {0};
    
    // Create projects directory if it doesn't exist
    if (stat("projects", &st) == -1) {
        if (mkdir("projects", 0700) == -1) {
            fprintf(stderr, "Error: Could not create projects directory\n");
            return -1;
        }
    }
    
    // Use "default" as project name if none is provided
    const char* actual_project_name = project_name ? project_name : "default";
    
    // Create full path for the new project - dynamically allocated
    size_t full_project_path_len = strlen("projects/") + strlen(actual_project_name) + 1;
    char* full_project_path = malloc(full_project_path_len);
    if (!full_project_path) {
        fprintf(stderr, "Error: Could not allocate memory for project path\n");
        return -1;
    }
    snprintf(full_project_path, full_project_path_len, "projects/%s", actual_project_name);
    
    // Check if directory already exists
    if (stat(full_project_path, &st) == -1) {
        // Create directory
        if (mkdir(full_project_path, 0700) == -1) {
            fprintf(stderr, "Error: Could not create directory %s\n", full_project_path);
            free(full_project_path);
            return -1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory\n", full_project_path);
        free(full_project_path);
        return -1;
    }
    
    // Set current project path
    strncpy(current_project_path, full_project_path, sizeof(current_project_path) - 1);
    current_project_path[sizeof(current_project_path) - 1] = '\0';
    
    // Create initial map file - dynamically allocated
    size_t filename_len = strlen(full_project_path) + strlen("/map_0.txt") + 1;
    char* filename = malloc(filename_len);
    if (!filename) {
        fprintf(stderr, "Error: Could not allocate memory for filename\n");
        free(full_project_path);
        return -1;
    }
    snprintf(filename, filename_len, "%s/map_0.txt", full_project_path);
    
    // Check if map file exists, if not create it
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        // Create new map file with header
        fp = fopen(filename, "w");
        if (fp) {
            fprintf(fp, "emoji_idx,fg_color_idx,bg_color_idx,layer\n");
            fclose(fp);
        }
    } else {
        fclose(fp);
    }
    
    // Create empty global events file - reuse the filename buffer
    snprintf(filename, filename_len, "%s/global_events.txt", full_project_path);
    fp = fopen(filename, "r");
    if (!fp) {
        fp = fopen(filename, "w");
        if (fp) {
            fclose(fp);
        }
    } else {
        fclose(fp);
    }
    
    // Free dynamically allocated memory
    free(filename);
    free(full_project_path);
    
    // Scan for map files in the new project directory
    scan_map_files(full_project_path);
    
    return 0;
}

int save_project(const char* project_path) {
    // Save to the currently loaded map file instead of always saving to map_0.txt
    char* full_project_path;
    if (strncmp(project_path, "projects/", 9) == 0) {
        // Already has projects/ prefix
        size_t len = strlen(project_path) + 1;
        full_project_path = malloc(len);
        if (!full_project_path) {
            fprintf(stderr, "Error: Could not allocate memory for project path\n");
            return -1;
        }
        strncpy(full_project_path, project_path, len - 1);
        full_project_path[len - 1] = '\0';
    } else {
        // Add projects/ prefix
        size_t len = strlen("projects/") + strlen(project_path) + 1;
        full_project_path = malloc(len);
        if (!full_project_path) {
            fprintf(stderr, "Error: Could not allocate memory for project path\n");
            return -1;
        }
        snprintf(full_project_path, len, "projects/%s", project_path);
    }
    
    // Determine which file to save to
    char* filename;
    if (current_map_file[0] != '\0') {
        // Use the currently loaded map file
        size_t filename_len = strlen(full_project_path) + 1 + strlen(current_map_file) + 1;
        filename = malloc(filename_len);
        if (!filename) {
            fprintf(stderr, "Error: Could not allocate memory for filename\n");
            free(full_project_path);
            return -1;
        }
        snprintf(filename, filename_len, "%s/%s", full_project_path, current_map_file);
        fprintf(stderr, "Saving to current map file: %s\n", filename);
    } else {
        // Fall back to map_0.txt if no current map file is set
        size_t filename_len = strlen(full_project_path) + strlen("/map_0.txt") + 1;
        filename = malloc(filename_len);
        if (!filename) {
            fprintf(stderr, "Error: Could not allocate memory for filename\n");
            free(full_project_path);
            return -1;
        }
        snprintf(filename, filename_len, "%s/map_0.txt", full_project_path);
        fprintf(stderr, "Saving to default map file (no current map): %s\n", filename);
    }
    
    int result = save_game_state(filename);
    
    // Provide user feedback about where the file was saved
    if (result == 0) {
        char msg[512];  // Increased buffer size to prevent truncation
        if (current_map_file[0] != '\0') {
            snprintf(msg, sizeof(msg), "Saved to: %s", current_map_file);
        } else {
            snprintf(msg, sizeof(msg), "Saved to: map_0.txt");
        }
        set_status_message(msg);
    }
    
    // Free dynamically allocated memory
    free(filename);
    free(full_project_path);
    
    return result;
}

int load_project(const char* project_path) {
    // Create full path for the project in the projects/ directory
    char* full_project_path;
    if (strncmp(project_path, "projects/", 9) == 0) {
        // Already has projects/ prefix
        size_t len = strlen(project_path) + 1;
        full_project_path = malloc(len);
        if (!full_project_path) {
            fprintf(stderr, "Error: Could not allocate memory for project path\n");
            return -1;
        }
        strncpy(full_project_path, project_path, len - 1);
        full_project_path[len - 1] = '\0';
    } else {
        // Add projects/ prefix
        size_t len = strlen("projects/") + strlen(project_path) + 1;
        full_project_path = malloc(len);
        if (!full_project_path) {
            fprintf(stderr, "Error: Could not allocate memory for project path\n");
            return -1;
        }
        snprintf(full_project_path, len, "projects/%s", project_path);
    }
    
    // Store the current project path
    strncpy(current_project_path, full_project_path, sizeof(current_project_path) - 1);
    current_project_path[sizeof(current_project_path) - 1] = '\0';
    
    // Scan for map files in the project directory
    scan_map_files(full_project_path);
    
    // Load the first map file if available
    int map_count = 0;
    char** map_files = get_map_files_list(&map_count);
    
    int result = 0;
    if (map_files && map_count > 0) {
        // Construct the full path to the first map file - dynamically allocated
        size_t full_path_len = strlen(full_project_path) + 1 + strlen(map_files[0]) + 1;
        char* full_path = malloc(full_path_len);
        if (!full_path) {
            fprintf(stderr, "Error: Could not allocate memory for full path\n");
            free(full_project_path);
            return -1;
        }
        snprintf(full_path, full_path_len, "%s/%s", full_project_path, map_files[0]);
        
        // Load the first map file
        fprintf(stderr, "Loading first map file: %s\n", full_path);
        result = load_game_state(full_path);
        if (result == 0) {
            // Set the current map file name
            strncpy(current_map_file, map_files[0], sizeof(current_map_file) - 1);
            current_map_file[sizeof(current_map_file) - 1] = '\0';
            fprintf(stderr, "Successfully loaded first map file: %s\n", map_files[0]);
        } else {
            fprintf(stderr, "Failed to load first map file: %s, falling back to debug.csv\n", map_files[0]);
            result = load_game_state("debug.csv");
            // Clear the current map file since we're falling back to debug.csv
            current_map_file[0] = '\0';
        }
        
        // Free dynamically allocated memory
        free(full_path);
    } else {
        // No map files found, load from debug.csv
        fprintf(stderr, "No map files found, loading from debug.csv\n");
        result = load_game_state("debug.csv");
        // Clear the current map file since we're loading from debug.csv
        current_map_file[0] = '\0';
    }
    
    // Free dynamically allocated memory
    free(full_project_path);
    
    return result;
}

char** list_projects(int* count) {
    // Free existing project list if it exists
    free_project_list();
    
    // Scan projects directory for project directories
    DIR* dir = opendir("projects");
    if (!dir) {
        *count = 0;
        return NULL;
    }
    
    struct dirent* entry;
    char** projects = NULL;
    int project_count = 0;
    int project_capacity = 10;
    
    // Allocate initial array for project names
    projects = (char**)malloc(project_capacity * sizeof(char*));
    if (!projects) {
        closedir(dir);
        *count = 0;
        return NULL;
    }
    
    // Scan for directories that contain required project files
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && 
            strcmp(entry->d_name, ".") != 0 && 
            strcmp(entry->d_name, "..") != 0) {
            // Check if it's a project directory by looking for required files
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "projects/%s/map_0.txt", entry->d_name);
            FILE* fp = fopen(filepath, "r");
            if (fp) {
                fclose(fp);
                // This is a valid project directory
                if (project_count >= project_capacity) {
                    // Expand array if needed
                    project_capacity *= 2;
                    char** temp = (char**)realloc(projects, project_capacity * sizeof(char*));
                    if (!temp) {
                        // Failed to expand, clean up and return what we have
                        break;
                    }
                    projects = temp;
                }
                
                // Add project name to list
                projects[project_count] = strdup(entry->d_name);
                if (projects[project_count]) {
                    project_count++;
                }
            }
        }
    }
    
    closedir(dir);
    
    // Resize array to actual size
    if (project_count < project_capacity) {
        char** temp = (char**)realloc(projects, project_count * sizeof(char*));
        if (temp || project_count == 0) {
            projects = temp;
        }
    }
    
    *count = project_count;
    return projects;
}

void free_project_list(void) {
    if (project_list) {
        for (int i = 0; i < project_count; i++) {
            free(project_list[i]);
        }
        free(project_list);
        project_list = NULL;
        project_count = 0;
    }
}


void mouse_motion(int x, int y) {
    // Handle thumb dragging for emoji picker
    if (emoji_thumb_dragging) {
        // Check if mouse is still within the thumb area horizontally
        float sidebar_x = 10;
        int emoji_cols = 8;
        float thumb_x = sidebar_x + 10 + emoji_cols * 40 + 5;
        
        // If mouse moves too far from the thumb area horizontally, stop dragging
        if (x < thumb_x - 20 || x > thumb_x + 30) {
            emoji_thumb_dragging = 0;
        } else {
            // Update thumbY based on mouse Y (from top)
            emoji_thumb_y = mouse_y_to_emoji_thumb_y(y);
            
            // Constrain within track
            int total_emojis = get_num_emojis();
            int emoji_cols = 8;
            int emoji_rows = (total_emojis + emoji_cols - 1) / emoji_cols;
            int visible_emojis = emoji_visible_rows * emoji_cols;
            int visible_height = emoji_visible_rows * 40;
            float maxY = visible_height - emoji_thumb_height;
            
            if (emoji_thumb_y < 0) emoji_thumb_y = 0;
            if (emoji_thumb_y > maxY) emoji_thumb_y = maxY;
            
            update_emoji_index_from_thumb(total_emojis);
            glutPostRedisplay();
            return;
        }
    }
    
    // Update mouse position
    set_mouse_x(x);
    set_mouse_y(y);
    set_mouse_in_window(true);
    glutPostRedisplay();
}

void reshape(int w, int h) {
    set_window_width(w);
    set_window_height(h);
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

// Function to scan for map files in a project directory
int scan_map_files(const char* project_path) {
    // Free existing map files list if it exists
    free_map_files_list();
    
    // Create full path for the project in the projects/ directory if needed
    char full_project_path[512];
    if (strncmp(project_path, "projects/", 9) == 0) {
        // Already has projects/ prefix
        strncpy(full_project_path, project_path, sizeof(full_project_path) - 1);
        full_project_path[sizeof(full_project_path) - 1] = '\0';
    } else if (strncmp(project_path, "../", 3) == 0 || project_path[0] == '/') {
        // Absolute path or relative path going up, use as is
        strncpy(full_project_path, project_path, sizeof(full_project_path) - 1);
        full_project_path[sizeof(full_project_path) - 1] = '\0';
    } else {
        // Add projects/ prefix
        snprintf(full_project_path, sizeof(full_project_path), "projects/%s", project_path);
    }
    
    // Initialize map files list
    map_files_capacity = MAX_MAP_FILES;
    map_files_list = (char**)malloc(map_files_capacity * sizeof(char*));
    if (!map_files_list) {
        map_files_count = 0;
        map_files_capacity = 0;
        return -1;
    }
    
    // Scan project directory for map files
    DIR* dir = opendir(full_project_path);
    if (!dir) {
        free(map_files_list);
        map_files_list = NULL;
        map_files_count = 0;
        map_files_capacity = 0;
        return -1;
    }
    
    struct dirent* entry;
    map_files_count = 0;
    
    // Scan for files matching pattern "map_*.txt" but not "map_events_*.txt"
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            // Check if file matches pattern "map_*.txt"
            if (strncmp(entry->d_name, "map_", 4) == 0) {
                // Exclude map_events_*.txt files
                if (strncmp(entry->d_name, "map_events_", 11) != 0) {
                    char* ext = strrchr(entry->d_name, '.');
                    if (ext && strcmp(ext, ".txt") == 0) {
                        // This is a map file, add it to our list
                        if (map_files_count >= map_files_capacity) {
                            // Expand array if needed
                            map_files_capacity *= 2;
                            char** temp = (char**)realloc(map_files_list, map_files_capacity * sizeof(char*));
                            if (!temp) {
                                // Failed to expand, stop scanning
                                break;
                            }
                            map_files_list = temp;
                        }
                        
                        // Add map file to list
                        map_files_list[map_files_count] = strdup(entry->d_name);
                        if (map_files_list[map_files_count]) {
                            map_files_count++;
                            // Debug output
                            fprintf(stderr, "Found map file: %s\n", entry->d_name);
                        }
                    }
                }
            }
        }
    }
    
    closedir(dir);
    
    // Sort map files alphabetically for consistent ordering
    if (map_files_count > 1) {
        for (int i = 0; i < map_files_count - 1; i++) {
            for (int j = i + 1; j < map_files_count; j++) {
                if (strcmp(map_files_list[i], map_files_list[j]) > 0) {
                    char* temp = map_files_list[i];
                    map_files_list[i] = map_files_list[j];
                    map_files_list[j] = temp;
                }
            }
        }
    }
    
    return 0;
}

// Function to free the map files list
void free_map_files_list(void) {
    if (map_files_list) {
        for (int i = 0; i < map_files_count; i++) {
            free(map_files_list[i]);
        }
        free(map_files_list);
        map_files_list = NULL;
        map_files_count = 0;
        map_files_capacity = 0;
    }
}

// Function to get the map files list
char** get_map_files_list(int* count) {
    *count = map_files_count;
    return map_files_list;
}

// Helper function to create directories recursively
int create_directories(const char* path) {
    char tmp[512];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

// Helper function to copy a file
int copy_file(const char* src, const char* dst) {
    FILE* src_file = fopen(src, "rb");
    if (!src_file) {
        return -1;
    }
    
    FILE* dst_file = fopen(dst, "wb");
    if (!dst_file) {
        fclose(src_file);
        return -1;
    }
    
    char buffer[4096];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        fwrite(buffer, 1, bytes, dst_file);
    }
    
    fclose(src_file);
    fclose(dst_file);
    return 0;
}

// Helper function to copy a directory recursively
int copy_directory(const char* src, const char* dst) {
    DIR* dir = opendir(src);
    if (!dir) {
        return -1;
    }
    
    // Create destination directory
    create_directories(dst);
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char src_path[512];
        char dst_path[512];
        
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);
        
        struct stat statbuf;
        if (stat(src_path, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                // Recursively copy subdirectory
                copy_directory(src_path, dst_path);
            } else {
                // Copy file
                copy_file(src_path, dst_path);
            }
        }
    }
    
    closedir(dir);
    return 0;
}

// Function to build and run the player
void build_and_run_player(void) {
    // Check if we have a current project
    if (current_project_path[0] == '\0') {
        set_status_message("No project loaded");
        return;
    }
    
    // Get project name from path
    char project_name[256];
    char* last_slash = strrchr(current_project_path, '/');
    if (last_slash) {
        strncpy(project_name, last_slash + 1, sizeof(project_name) - 1);
        project_name[sizeof(project_name) - 1] = '\0';
    } else {
        strncpy(project_name, current_project_path, sizeof(project_name) - 1);
        project_name[sizeof(project_name) - 1] = '\0';
    }
    
    // Create build directory
    char build_dir[512];
    snprintf(build_dir, sizeof(build_dir), "player/build/%s", project_name);
    create_directories(build_dir);
    
    // Create data directory
    char data_dir[512];
    snprintf(data_dir, sizeof(data_dir), "player/data/%s", project_name);
    create_directories(data_dir);
    
    // Copy project data to player data directory (overwrite existing)
    copy_directory(current_project_path, data_dir);
    
    // Compile the player program using the existing compilation script
    int compile_result = system("cd 1.rmmvc_gl]q67/player && ./build.sh");
    if (compile_result != 0) {
        set_status_message("Failed to compile player");
        return;
    }
    
    // Run the player using system() call
    char command[1024];
    snprintf(command, sizeof(command), "cd 1.rmmvc_gl]q67/player && ./player.+x %s &", project_name);
    
    int result = system(command);
    if (result == 0) {
        // Allocate enough memory for the status message
        size_t status_msg_len = strlen("Player started for project: ") + strlen(project_name) + 1;
        char* status_msg = (char*)malloc(status_msg_len);
        if (status_msg) {
            snprintf(status_msg, status_msg_len, "Player started for project: %s", project_name);
            set_status_message(status_msg);
            free(status_msg);
        } else {
            set_status_message("Player started");
        }
    } else {
        set_status_message("Failed to start player");
    }
}
