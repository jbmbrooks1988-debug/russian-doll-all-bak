#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

// External declarations
extern char current_project_path[256];
extern int scan_map_files(const char* project_path);
extern void free_map_files_list(void);

#define MAX_CANVAS_LAYERS 10
#define MAX_CANVAS_DIM 128
#define HEADER_STRIP_HEIGHT 40

// Attribute types for flexible GameObject properties
typedef enum {
    ATTR_EMOJI_IDX = 0,
    ATTR_FG_COLOR,
    ATTR_BG_COLOR,
    ATTR_COUNT  // Keep track of number of attributes
} AttributeType;

typedef struct { 
    int attributes[10];  // Flexible array for properties
    int attribute_count;
} GameObject;

// Helper functions to access GameObject attributes
int get_game_object_attribute(GameObject* obj, AttributeType attr) {
    if (attr >= 0 && attr < obj->attribute_count) {
        return obj->attributes[attr];
    }
    return -1; // Default value
}

// Minecraft-style game structures
typedef enum {
    GAME_MODE_CREATIVE,
    GAME_MODE_SURVIVAL
} GameMode;

typedef enum {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_WATER,
    BLOCK_SAND,
    BLOCK_COBBLESTONE,
    BLOCK_PLANKS,
    BLOCK_GLASS,
    BLOCK_COAL_ORE,
    BLOCK_IRON_ORE,
    BLOCK_GOLD_ORE,
    BLOCK_DIAMOND_ORE,
    BLOCK_BEDROCK,
    BLOCK_COUNT
} BlockType;

typedef struct {
    float x, y, z;          // Position
    float yaw, pitch;       // Rotation
    int health;             // Health points
    int hunger;             // Hunger points
    int selected_slot;      // Selected inventory slot (0-8)
    bool on_ground;         // Whether player is on ground
    float velocity_x, velocity_y, velocity_z;  // Movement velocity
    bool is_jumping;        // Whether player is jumping
    int view_mode;          // 0 = first person, 1 = third person, 2 = free cam
} Player;

typedef struct {
    BlockType block_type;
    int count;
} InventoryItem;

typedef struct {
    InventoryItem slots[9];
    int selected_slot;  // Current selected slot (0-8)
} Inventory;

typedef struct {
    GameMode mode;
    Player player;
    Inventory inventory;
    time_t game_start_time;     // Time when game started
    time_t last_update_time;    // Time of last game update
    int day_count;              // Number of in-game days passed
    float time_of_day;          // 0.0-1.0 representing time of day
    bool is_daytime;            // Whether it's currently daytime
    int tick_count;             // Game tick counter
    bool paused;                // Whether game is paused
    bool menu_open;             // Whether menu is open
    int view_mode;              // Current view mode (2D overhead or 3D)
} GameState;

void set_game_object_attribute(GameObject* obj, AttributeType attr, int value) {
    if (attr >= 0 && attr < 10) {
        obj->attributes[attr] = value;
        if (attr >= obj->attribute_count) {
            obj->attribute_count = attr + 1;
        }
    }
}

// Initialize a GameObject with default values
void init_game_object(GameObject* obj) {
    obj->attribute_count = ATTR_COUNT;
    for (int i = 0; i < 10; i++) {
        obj->attributes[i] = 0;
    }
    // Set default values for specific attributes
    obj->attributes[ATTR_EMOJI_IDX] = -1;  // No emoji by default
    obj->attributes[ATTR_FG_COLOR] = 0;    // Default foreground color
    obj->attributes[ATTR_BG_COLOR] = 7;    // Default background color (black)
}

// Global variables
// Forward declaration for callback function
typedef void (*ViewUpdateCallback)(void);

// Enum for integer properties
typedef enum {
    CANVAS_ROWS, CANVAS_COLS, TILE_SIZE, NUM_EMOJIS, NUM_COLORS, MAX_LAYERS,
    MAX_TABS, SIDEBAR_WIDTH, FILE_TAB_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT,
    TAB_COUNT, ALLOCATED_LAYERS, ALLOCATED_ROWS, ALLOCATED_COLS, ALLOCATED_TABS,
    SELECTED_EMOJI, SELECTED_FG_COLOR, SELECTED_BG_COLOR, SELECTED_TOOL,
    START_ROW, START_COL, SELECTOR_ROW, SELECTOR_COL, VIEW_MODE, CURRENT_Z_LEVEL,
    MOUSE_X, MOUSE_Y, INT_PROPERTY_COUNT
} IntProperty;

// Enum for float properties
typedef enum {
    EMOJI_SCALE, CAMERA_X, CAMERA_Y, CAMERA_Z, CAMERA_YAW, CAMERA_PITCH,
    SCALE_X, SCALE_Y, SCALE_Z,  // Scale properties for objects
    FLOAT_PROPERTY_COUNT
} FloatProperty;

// Enum for boolean properties
typedef enum {
    SHOW_ALL_LAYERS, MOUSE_IN_WINDOW, BOOL_PROPERTY_COUNT
} BoolProperty;

// Model context using arrays
typedef struct {
    int int_properties[INT_PROPERTY_COUNT];
    float float_properties[FLOAT_PROPERTY_COUNT];
    bool bool_properties[BOOL_PROPERTY_COUNT];

    FT_Library ft;
    FT_Face emoji_face;
    float font_color[3];
    float background_color[4];
    Display *x_display;
    Window x_window;

    GameObject ***canvas;
    GameState game_state;  // Minecraft-style game state
    GameObject *tab_bank;

    char status_message[256];
    ViewUpdateCallback view_update_callback;
} ModelContext;

// Global model context instance
static ModelContext model_context;

// Global variable for current project path
char current_project_path[256] = "";

// Global variable for currently loaded map file name
char current_map_file[256] = "";

// Global variable to track if the current map has been modified
static bool map_dirty = false;

// Helper functions for property access
int get_int_property(IntProperty prop) { return model_context.int_properties[prop]; }
void set_int_property(IntProperty prop, int value) { model_context.int_properties[prop] = value; }
float get_float_property(FloatProperty prop) { return model_context.float_properties[prop]; }
void set_float_property(FloatProperty prop, float value) { model_context.float_properties[prop] = value; }
bool get_bool_property(BoolProperty prop) { return model_context.bool_properties[prop]; }
void set_bool_property(BoolProperty prop, bool value) { model_context.bool_properties[prop] = value; }

void init_model_properties(void) {
    set_int_property(TILE_SIZE, 32);
    set_int_property(NUM_EMOJIS, 64);
    set_int_property(NUM_COLORS, 8);
    set_int_property(MAX_LAYERS, MAX_CANVAS_LAYERS);
    set_int_property(MAX_TABS, 10);
    set_int_property(FILE_TAB_HEIGHT, 30);
    set_int_property(TAB_COUNT, 0);
    set_int_property(ALLOCATED_LAYERS, 0);
    set_int_property(ALLOCATED_ROWS, 0);
    set_int_property(ALLOCATED_COLS, 0);
    set_int_property(ALLOCATED_TABS, 0);
    set_int_property(SELECTED_EMOJI, 0);
    set_int_property(SELECTED_FG_COLOR, 0);
    set_int_property(SELECTED_BG_COLOR, 7);
    set_int_property(SELECTED_TOOL, 0);
    set_int_property(START_ROW, -1);
    set_int_property(START_COL, -1);
    set_int_property(SELECTOR_ROW, 0);
    set_int_property(SELECTOR_COL, 0);
    set_int_property(VIEW_MODE, 0);
    set_int_property(CURRENT_Z_LEVEL, 0);
    set_int_property(MOUSE_X, 0);
    set_int_property(MOUSE_Y, 0);

    set_float_property(CAMERA_X, 0.0f);
    set_float_property(CAMERA_Y, 0.0f);
    set_float_property(CAMERA_Z, 50.0f);
    set_float_property(CAMERA_YAW, 0.0f);
    set_float_property(CAMERA_PITCH, 0.0f);
    set_float_property(SCALE_X, 1.0f);  // Default scale values
    set_float_property(SCALE_Y, 1.0f);
    set_float_property(SCALE_Z, 1.0f);

    set_bool_property(SHOW_ALL_LAYERS, true);
    set_bool_property(MOUSE_IN_WINDOW, false);

    model_context.font_color[0] = 1.0f;
    model_context.font_color[1] = 1.0f;
    model_context.font_color[2] = 1.0f;
    model_context.background_color[0] = 0.1f;
    model_context.background_color[1] = 0.1f;
    model_context.background_color[2] = 0.1f;
    model_context.background_color[3] = 1.0f;

    model_context.x_display = NULL;
    model_context.canvas = NULL;
    model_context.tab_bank = NULL;
    strncpy(model_context.status_message, "Select tool and paint!", sizeof(model_context.status_message) - 1);
    model_context.status_message[sizeof(model_context.status_message) - 1] = '\0';
}

char **emojis;

void load_emojis(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for reading\n", filename);
        return;
    }

    int count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp)) {
        count++;
    }

    // Add 1 for the blank emoji option at index 0
    set_int_property(NUM_EMOJIS, count + 1);
    emojis = (char **)malloc((count + 1) * sizeof(char *));

    // Set the first emoji as a blank option
    emojis[0] = strdup("");

    fseek(fp, 0, SEEK_SET);

    for (int i = 0; i < count; i++) {
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = 0;
        // Shift all emojis by 1 index to accommodate the blank option at index 0
        emojis[i + 1] = strdup(buffer);
    }

    fclose(fp);
}

void free_emojis(void) {
    for (int i = 0; i < get_int_property(NUM_EMOJIS); i++) {
        free(emojis[i]);
    }
    free(emojis);
}

const char *colors[8][3] = {
    {"255", "0", "0"},    // Red
    {"0", "255", "0"},    // Green
    {"0", "0", "255"},    // Blue
    {"255", "255", "0"},  // Yellow
    {"0", "255", "255"},  // Cyan
    {"255", "0", "255"},  // Magenta
    {"255", "255", "255"},// White
    {"0", "0", "0"}       // Black
};

const char *color_names[8] = {"Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White", "Black"};

// Function to mark the current map as dirty (modified)
void mark_map_dirty(void) {
    map_dirty = true;
}

// Function to clear the dirty flag (after saving)
void clear_map_dirty(void) {
    map_dirty = false;
}

// Function to check if the current map is dirty
bool is_map_dirty(void) {
    return map_dirty;
}

// Function declarations for new getters/setters
int get_view_mode(void);
void set_view_mode(int mode);
int get_current_z_level(void);
void set_current_z_level(int z);
float get_camera_x(void);
void set_camera_x(float x);
float get_camera_y(void);
void set_camera_y(float y);
float get_camera_z(void);
void set_camera_z(float z);
float get_camera_yaw(void);
void set_camera_yaw(float yaw);
float get_camera_pitch(void);
void set_camera_pitch(float pitch);
void move_camera_forward(float distance);
void move_camera_backward(float distance);
void strafe_camera(float distance);
void elevate_camera(float distance);
float get_scale_x(void);
float get_scale_y(void);
float get_scale_z(void);
void set_scale_x(float scale);
void set_scale_y(float scale);
void set_scale_z(float scale);

// Mouse position functions
int get_mouse_x(void);
int get_mouse_y(void);
bool get_mouse_in_window(void);
void set_mouse_x(int x);
void set_mouse_y(int y);
void set_mouse_in_window(bool in_window);



// Function definitions for new getters/setters
int get_view_mode(void) { return get_int_property(VIEW_MODE); }
void set_view_mode(int mode) { set_int_property(VIEW_MODE, mode); }
int get_current_z_level(void) { return get_int_property(CURRENT_Z_LEVEL); }
void set_current_z_level(int z) { if (z >= 0 && z < get_int_property(MAX_LAYERS)) set_int_property(CURRENT_Z_LEVEL, z); }
float get_camera_x(void) { return get_float_property(CAMERA_X); }
void set_camera_x(float x) { set_float_property(CAMERA_X, x); }
float get_camera_y(void) { return get_float_property(CAMERA_Y); }
void set_camera_y(float y) { set_float_property(CAMERA_Y, y); }
float get_camera_z(void) { return get_float_property(CAMERA_Z); }
void set_camera_z(float z) { set_float_property(CAMERA_Z, z); }
float get_camera_yaw(void) { return get_float_property(CAMERA_YAW); }
void set_camera_yaw(float yaw) { set_float_property(CAMERA_YAW, yaw); }
float get_camera_pitch(void) { return get_float_property(CAMERA_PITCH); }
void set_camera_pitch(float pitch) { set_float_property(CAMERA_PITCH, pitch); }

// Mouse position getters
int get_mouse_x(void) { return get_int_property(MOUSE_X); }
int get_mouse_y(void) { return get_int_property(MOUSE_Y); }
bool get_mouse_in_window(void) { return get_bool_property(MOUSE_IN_WINDOW); }

// Mouse position setters
void set_mouse_x(int x) { set_int_property(MOUSE_X, x); }
void set_mouse_y(int y) { set_int_property(MOUSE_Y, y); }
void set_mouse_in_window(bool in_window) { set_bool_property(MOUSE_IN_WINDOW, in_window); }

void move_camera_forward(float distance) {
    // Move forward in the horizontal plane (XZ plane) only
    set_float_property(CAMERA_X, get_float_property(CAMERA_X) + distance * sin(get_float_property(CAMERA_YAW) * M_PI / 180.0));
    // Keep Y constant for horizontal movement
    set_float_property(CAMERA_Z, get_float_property(CAMERA_Z) - distance * cos(get_float_property(CAMERA_YAW) * M_PI / 180.0));
}

void move_camera_backward(float distance) {
    // Move backward in the horizontal plane (XZ plane) only
    set_float_property(CAMERA_X, get_float_property(CAMERA_X) - distance * sin(get_float_property(CAMERA_YAW) * M_PI / 180.0));
    // Keep Y constant for horizontal movement
    set_float_property(CAMERA_Z, get_float_property(CAMERA_Z) + distance * cos(get_float_property(CAMERA_YAW) * M_PI / 180.0));
}

void strafe_camera(float distance) {
    set_float_property(CAMERA_X, get_float_property(CAMERA_X) + distance * cos(get_float_property(CAMERA_YAW) * M_PI / 180.0));
    set_float_property(CAMERA_Z, get_float_property(CAMERA_Z) + distance * sin(get_float_property(CAMERA_YAW) * M_PI / 180.0));
}

void elevate_camera(float distance) {
    set_float_property(CAMERA_Y, get_float_property(CAMERA_Y) + distance);
}

// Function declarations
void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg);
void draw_rectangle(int layer, int r1, int c1, int r2, int c2);

// UI element functions
int get_file_tab_count(void);
const char* get_file_tab_label(int idx);
int get_emoji_grid_rows(void);
int get_emoji_grid_cols(void);
int get_color_grid_rows(void);
int get_color_grid_cols(void);
int get_tool_count(void);
const char* get_tool_label(int idx);

// Getters
int get_canvas_rows(void) { return get_int_property(CANVAS_ROWS); }
int get_canvas_cols(void) { return get_int_property(CANVAS_COLS); }
int get_tile_size(void) { return get_int_property(TILE_SIZE); }
int get_num_emojis(void) { return get_int_property(NUM_EMOJIS); }
int get_num_colors(void) { return get_int_property(NUM_COLORS); }
int get_max_layers(void) { return get_int_property(MAX_LAYERS); }
int get_max_tabs(void) { return get_int_property(MAX_TABS); }
int get_sidebar_width(void) { return get_int_property(SIDEBAR_WIDTH); }
int get_file_tab_height(void) { return get_int_property(FILE_TAB_HEIGHT); }
int get_window_width(void) { return get_int_property(WINDOW_WIDTH); }
int get_window_height(void) { return get_int_property(WINDOW_HEIGHT); }
int get_tab_count(void) { return get_int_property(TAB_COUNT); }
const char* get_emoji(int idx) { return emojis[idx]; }
const char* get_color(int idx, int component) { return colors[idx][component]; }
const char* get_color_name(int idx) { return color_names[idx]; }
const char* get_status_message(void) { return model_context.status_message; }

// Getter and setter functions for view update callback
void set_view_update_callback(ViewUpdateCallback callback) { 
    model_context.view_update_callback = callback; 
}
ViewUpdateCallback get_view_update_callback(void) { 
    return model_context.view_update_callback; 
}

// Getter functions for FreeType variables
FT_Library get_ft(void) { return model_context.ft; }
FT_Face get_emoji_face(void) { return model_context.emoji_face; }
float get_emoji_scale(void) { return get_float_property(EMOJI_SCALE); }
float* get_font_color(void) { return model_context.font_color; }
float* get_background_color(void) { return model_context.background_color; }

// Function to trigger view updates through callback
void trigger_view_update(void) {
    if (model_context.view_update_callback) {
        model_context.view_update_callback();
    }
}

// Setter functions for FreeType variables
void set_ft(FT_Library value) { model_context.ft = value; }
void set_emoji_face(FT_Face value) { model_context.emoji_face = value; }
void set_emoji_scale(float value) { set_float_property(EMOJI_SCALE, value); }
int get_selected_emoji(void) { return get_int_property(SELECTED_EMOJI); }
int get_selected_fg_color(void) { return get_int_property(SELECTED_FG_COLOR); }
int get_selected_bg_color(void) { return get_int_property(SELECTED_BG_COLOR); }
int get_selected_tool(void) { return get_int_property(SELECTED_TOOL); }
int get_start_row(void) { return get_int_property(START_ROW); }
int get_start_col(void) { return get_int_property(START_COL); }
void set_selected_tool(int value) { set_int_property(SELECTED_TOOL, value); }
int get_selector_row(void) { return get_int_property(SELECTOR_ROW); }
int get_selector_col(void) { return get_int_property(SELECTOR_COL); }
bool get_show_all_layers(void) { return get_bool_property(SHOW_ALL_LAYERS); }
int get_canvas_tile(int layer, int r, int c, int field) {
    if (field == 0) return get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX);
    if (field == 1) return get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR);
    if (field == 2) return get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR);
    return -1;
}
int get_tab_bank(int idx, int field) {
    if (field == 0) return get_game_object_attribute(&model_context.tab_bank[idx], ATTR_EMOJI_IDX);
    if (field == 1) return get_game_object_attribute(&model_context.tab_bank[idx], ATTR_FG_COLOR);
    if (field == 2) return get_game_object_attribute(&model_context.tab_bank[idx], ATTR_BG_COLOR);
    return -1;
}

// UI element functions for customization
int get_file_tab_count(void) {
    return 6; // Save, Load, Text, Tile, 2D, 3D
}

const char* get_file_tab_label(int idx) {
    const char *labels[] = {"Save", "Load", "Text", "Tile", "2D", "3D"};
    if (idx >= 0 && idx < 6) {
        return labels[idx];
    }
    return "";
}

int get_emoji_grid_rows(void) {
    return (get_int_property(NUM_EMOJIS) + 7) / 8; // 8 columns
}

int get_emoji_grid_cols(void) {
    return 8;
}

int get_color_grid_rows(void) {
    return 1;
}

int get_color_grid_cols(void) {
    return get_int_property(NUM_COLORS);
}

int get_tool_count(void) {
    return 3; // Paint, Fill, Rect
}

const char* get_tool_label(int idx) {
    const char *labels[] = {"Paint", "Fill", "Rect"};
    if (idx >= 0 && idx < 3) {
        return labels[idx];
    }
    return "";
}

// UI Layout functions for generalized coordinate calculations
int get_grid_cell_size(void) {
    return 40; // Size of each cell in emoji/color grids
}

int get_button_width(void) {
    return 60; // Width of file tab buttons
}

int get_button_height(void) {
    return get_file_tab_height(); // Height of file tab buttons
}

int get_button_spacing(void) {
    return 10; // Spacing between buttons
}

int get_tool_button_width(void) {
    return 70; // Width of tool buttons
}

int get_tool_button_height(void) {
    return 30; // Height of tool buttons
}

int get_grid_columns(void) {
    return 8; // Number of columns in emoji/color grids
}

int get_sidebar_padding(void) {
    return 10; // Padding around sidebar elements
}

int get_element_spacing(void) {
    return 20; // Spacing between UI elements
}

// Setters
void set_canvas_rows(int value) { set_int_property(CANVAS_ROWS, value); }
void set_canvas_cols(int value) { set_int_property(CANVAS_COLS, value); }
void set_tile_size(int value) { set_int_property(TILE_SIZE, value); }
void set_sidebar_width(int value) { set_int_property(SIDEBAR_WIDTH, value); }
void set_window_width(int value) { set_int_property(WINDOW_WIDTH, value); }
void set_window_height(int value) { set_int_property(WINDOW_HEIGHT, value); }
void set_file_tab_height(int value) { set_int_property(FILE_TAB_HEIGHT, value); }
void set_status_message(const char* msg) {
    strncpy(model_context.status_message, msg, sizeof(model_context.status_message) - 1);
    model_context.status_message[sizeof(model_context.status_message) - 1] = '\0';
}
void set_selected_emoji(int value) { set_int_property(SELECTED_EMOJI, value); }
void set_selected_fg_color(int value) { set_int_property(SELECTED_FG_COLOR, value); }
void set_selected_bg_color(int value) { set_int_property(SELECTED_BG_COLOR, value); }
void set_start_row(int value) { set_int_property(START_ROW, value); }
void set_start_col(int value) { set_int_property(START_COL, value); }
void set_selector_row(int value) { set_int_property(SELECTOR_ROW, value); }
void set_selector_col(int value) { set_int_property(SELECTOR_COL, value); }
void set_show_all_layers(bool value) { set_bool_property(SHOW_ALL_LAYERS, value); }
void set_canvas_tile(int layer, int r, int c, int field, int value) {
    if (field == 0) set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX, value);
    if (field == 1) set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR, value);
    if (field == 2) set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR, value);
    
    // Mark map as dirty when changes are made
    mark_map_dirty();
}
void set_tab_bank(int idx, int field, int value) {
    if (field == 0) set_game_object_attribute(&model_context.tab_bank[idx], ATTR_EMOJI_IDX, value);
    if (field == 1) set_game_object_attribute(&model_context.tab_bank[idx], ATTR_FG_COLOR, value);
    if (field == 2) set_game_object_attribute(&model_context.tab_bank[idx], ATTR_BG_COLOR, value);
}
void increment_tab_count(void) { set_int_property(TAB_COUNT, get_int_property(TAB_COUNT) + 1); }

// Memory management functions for dynamic arrays
void allocate_canvas_memory(int layers, int rows, int cols) {
    // Free existing memory if allocated
    if (model_context.canvas) {
        for (int i = 0; i < get_int_property(ALLOCATED_LAYERS); i++) {
            if (model_context.canvas[i]) {
                for (int j = 0; j < get_int_property(ALLOCATED_ROWS); j++) {
                    free(model_context.canvas[i][j]);
                }
                free(model_context.canvas[i]);
            }
        }
        free(model_context.canvas);
    }
    
    // Allocate new memory
    model_context.canvas = (GameObject ***)malloc(layers * sizeof(GameObject **));
    if (!model_context.canvas) return;
    
    for (int i = 0; i < layers; i++) {
        model_context.canvas[i] = (GameObject **)malloc(rows * sizeof(GameObject *));
        if (!model_context.canvas[i]) {
            // Free previously allocated memory
            for (int k = 0; k < i; k++) {
                for (int j = 0; j < rows; j++) {
                    free(model_context.canvas[k][j]);
                }
                free(model_context.canvas[k]);
            }
            free(model_context.canvas);
            return;
        }
        
        for (int j = 0; j < rows; j++) {
            model_context.canvas[i][j] = (GameObject *)malloc(cols * sizeof(GameObject));
            if (!model_context.canvas[i][j]) {
                // Free previously allocated memory
                for (int k = 0; k < i; k++) {
                    for (int l = 0; l < rows; l++) {
                        free(model_context.canvas[k][l]);
                    }
                    free(model_context.canvas[k]);
                }
                for (int k = 0; k < j; k++) {
                    free(model_context.canvas[i][k]);
                }
                free(model_context.canvas[i]);
                free(model_context.canvas);
                return;
            }
        }
    }
    
    // Update allocated dimensions
    set_int_property(ALLOCATED_LAYERS, layers);
    set_int_property(ALLOCATED_ROWS, rows);
    set_int_property(ALLOCATED_COLS, cols);
}

void allocate_tab_bank_memory(int tabs) {
    // Free existing memory if allocated
    if (model_context.tab_bank) {
        free(model_context.tab_bank);
    }
    
    // Allocate new memory
    model_context.tab_bank = (GameObject *)malloc(tabs * sizeof(GameObject));
    if (model_context.tab_bank) {
        set_int_property(ALLOCATED_TABS, tabs);
    }
}

void free_canvas_memory(void) {
    if (model_context.canvas) {
        for (int i = 0; i < get_int_property(ALLOCATED_LAYERS); i++) {
            if (model_context.canvas[i]) {
                for (int j = 0; j < get_int_property(ALLOCATED_ROWS); j++) {
                    free(model_context.canvas[i][j]);
                }
                free(model_context.canvas[i]);
            }
        }
        free(model_context.canvas);
        model_context.canvas = NULL;
        set_int_property(ALLOCATED_LAYERS, 0);
        set_int_property(ALLOCATED_ROWS, 0);
        set_int_property(ALLOCATED_COLS, 0);
    }
}

void free_tab_bank_memory(void) {
    if (model_context.tab_bank) {
        free(model_context.tab_bank);
        model_context.tab_bank = NULL;
        set_int_property(ALLOCATED_TABS, 0);
    }
}

// Game logic functions
void init_model(void) {
    srand(time(NULL));
    init_model_properties();
    load_emojis("emojis.txt");
    
    // Allocate memory for canvas and tab bank if not already allocated or if dimensions changed
    if (!model_context.canvas || get_int_property(ALLOCATED_LAYERS) != get_int_property(MAX_LAYERS) || get_int_property(ALLOCATED_ROWS) != get_int_property(CANVAS_ROWS) || get_int_property(ALLOCATED_COLS) != get_int_property(CANVAS_COLS)) {
        allocate_canvas_memory(get_int_property(MAX_LAYERS), get_int_property(CANVAS_ROWS), get_int_property(CANVAS_COLS));
    }
    
    if (!model_context.tab_bank || get_int_property(ALLOCATED_TABS) != get_int_property(MAX_TABS)) {
        allocate_tab_bank_memory(get_int_property(MAX_TABS));
    }
    
    // Initialize canvas
    for (int layer = 0; layer < get_int_property(MAX_LAYERS) && layer < get_int_property(ALLOCATED_LAYERS); layer++) {
        for (int r = 0; r < get_int_property(CANVAS_ROWS) && r < get_int_property(ALLOCATED_ROWS); r++) {
            for (int c = 0; c < get_int_property(CANVAS_COLS) && c < get_int_property(ALLOCATED_COLS); c++) {
                init_game_object(&model_context.canvas[layer][r][c]);
            }
        }
    }
    
    // Initialize tab bank
    for (int i = 0; i < get_int_property(MAX_TABS) && i < get_int_property(ALLOCATED_TABS); i++) {
        init_game_object(&model_context.tab_bank[i]);
    }
    // Initialize new 3D and Z-level variables
    set_int_property(VIEW_MODE, 0); // Start in 2D mode
    set_int_property(CURRENT_Z_LEVEL, 0); // Start at Z-level 0
    // Calculate initial camera position to view the centered canvas
    // Assuming canvas_rows, canvas_cols, max_layers, tile_size are already set
    float initial_center_x = (get_canvas_cols() * get_tile_size()) / 2.0f;
    float initial_center_y = (get_canvas_rows() * get_tile_size()) / 2.0f;
    float initial_center_z = (get_max_layers() * get_tile_size()) / 2.0f;

    set_float_property(CAMERA_X, initial_center_x);
    set_float_property(CAMERA_Y, initial_center_y);
    set_float_property(CAMERA_Z, initial_center_z + (get_max_layers() * get_tile_size() * 2.0f)); // Position camera behind the canvas
    set_float_property(CAMERA_YAW, 0.0f); // Look straight ahead
    set_float_property(CAMERA_PITCH, 0.0f); // Look straight ahead
    
    // If no project is loaded, scan for maps in the default project directory
    if (current_project_path[0] == '\0') {
        // Check if projects/default directory exists
        struct stat st = {0};
        if (stat("projects/default", &st) == -1) {
            // Create default project directory
            if (stat("projects", &st) == -1) {
                mkdir("projects", 0700);
            }
            mkdir("projects/default", 0700);
            
            // Create initial map file for default project
            FILE* fp = fopen("projects/default/map_0.txt", "w");
            if (fp) {
                fprintf(fp, "emoji_idx,fg_color_idx,bg_color_idx,layer\n");
                fclose(fp);
            }
            
            // Create empty global events file for default project
            fp = fopen("projects/default/global_events.txt", "w");
            if (fp) {
                fclose(fp);
            }
        }
        
        // Set current project path to default
        strncpy(current_project_path, "projects/default", sizeof(current_project_path) - 1);
        current_project_path[sizeof(current_project_path) - 1] = '\0';
        
        // Scan for map files in the default project directory
        scan_map_files("projects/default");
    init_world_generation();  // Generate the initial world with biomes and set player spawn
    }
}

void init(void) {
    init_model();
    model_context.x_display = glXGetCurrentDisplay();
    model_context.x_window = glXGetCurrentDrawable();
    atexit(free_emojis);
}

void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg) {
    if (r < 0 || r >= get_int_property(CANVAS_ROWS) || c < 0 || c >= get_int_property(CANVAS_COLS)) return;
    if (get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX) != old_emoji || 
        get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR) != old_fg || 
        get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR) != old_bg) return;
    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX, get_int_property(SELECTED_EMOJI));
    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR, get_int_property(SELECTED_FG_COLOR));
    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR, get_int_property(SELECTED_BG_COLOR));
    flood_fill(layer, r+1, c, old_emoji, old_fg, old_bg);
    flood_fill(layer, r-1, c, old_emoji, old_fg, old_bg);
    flood_fill(layer, r, c+1, old_emoji, old_fg, old_bg);
    flood_fill(layer, r, c-1, old_emoji, old_fg, old_bg);
    
    // Mark map as dirty when changes are made
    mark_map_dirty();
}

void draw_rectangle(int layer, int r1, int c1, int r2, int c2) {
    int r_min = r1 < r2 ? r1 : r2;
    int r_max = r1 > r2 ? r1 : r2;
    int c_min = c1 < c2 ? c1 : c2;
    int c_max = c1 > c2 ? c1 : c2;
    for (int r = r_min; r <= r_max; r++) {
        for (int c = c_min; c <= c_max; c++) {
            if (r >= 0 && r < get_int_property(CANVAS_ROWS) && c >= 0 && c < get_int_property(CANVAS_COLS)) {
                // Use the current tool state to determine if we should draw outline or filled
                if (get_selected_tool() == 2) { // Outline mode (rectangle tool)
                    if (r == r_min || r == r_max || c == c_min || c == c_max) {
                        set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX, get_int_property(SELECTED_EMOJI));
                        set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR, get_int_property(SELECTED_FG_COLOR));
                        set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR, get_int_property(SELECTED_BG_COLOR));
                    }
                } else { // Filled mode
                    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX, get_int_property(SELECTED_EMOJI));
                    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR, get_int_property(SELECTED_FG_COLOR));
                    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR, get_int_property(SELECTED_BG_COLOR));
                }
            }
        }
    }
    
    // Mark map as dirty when changes are made
    mark_map_dirty();
}

// Erase functions
void erase_tile(int layer, int r, int c) {
    if (r >= 0 && r < get_int_property(CANVAS_ROWS) && c >= 0 && c < get_int_property(CANVAS_COLS)) {
        set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX, -1);
        set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR, 0);
        set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR, 7);
        
        // Mark map as dirty when changes are made
        mark_map_dirty();
    }
}

void erase_rectangle(int layer, int r1, int c1, int r2, int c2) {
    int r_min = r1 < r2 ? r1 : r2;
    int r_max = r1 > r2 ? r1 : r2;
    int c_min = c1 < c2 ? c1 : c2;
    int c_max = c1 > c2 ? c1 : c2;
    for (int r = r_min; r <= r_max; r++) {
        for (int c = c_min; c <= c_max; c++) {
            erase_tile(layer, r, c);
        }
    }
    
    // Mark map as dirty when changes are made
    mark_map_dirty();
}

void erase_fill(int layer, int r, int c) {
    // Add bounds checking for layer as well
    if (layer < 0 || layer >= get_int_property(MAX_LAYERS)) return;
    if (r < 0 || r >= get_int_property(CANVAS_ROWS) || c < 0 || c >= get_int_property(CANVAS_COLS)) return;
    // For erase fill, we'll just clear the entire connected area of non-empty tiles
    if (get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX) == -1) return; // Already empty
    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX, -1);
    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR, 0);
    set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR, 7);
    // Recursively erase adjacent cells with bounds checking
    if (r+1 < get_int_property(CANVAS_ROWS)) erase_fill(layer, r+1, c);
    if (r-1 >= 0) erase_fill(layer, r-1, c);
    if (c+1 < get_int_property(CANVAS_COLS)) erase_fill(layer, r, c+1);
    if (c-1 >= 0) erase_fill(layer, r, c-1);
    
    // Mark map as dirty when changes are made
    mark_map_dirty();
}

// Camera rotation functions
void rotate_camera_yaw(float degrees) {
    set_float_property(CAMERA_YAW, get_float_property(CAMERA_YAW) + degrees);
    // Keep yaw within 0-360 range
    if (get_float_property(CAMERA_YAW) >= 360.0f) set_float_property(CAMERA_YAW, get_float_property(CAMERA_YAW) - 360.0f);
    if (get_float_property(CAMERA_YAW) < 0.0f) set_float_property(CAMERA_YAW, get_float_property(CAMERA_YAW) + 360.0f);
}

void rotate_camera_pitch(float degrees) {
    set_float_property(CAMERA_PITCH, get_float_property(CAMERA_PITCH) + degrees);
    // Limit pitch to avoid gimbal lock
    if (get_float_property(CAMERA_PITCH) > 89.0f) set_float_property(CAMERA_PITCH, 89.0f);
    if (get_float_property(CAMERA_PITCH) < -89.0f) set_float_property(CAMERA_PITCH, -89.0f);
}

void reset_camera(void) {
    // Calculate initial camera position to view the centered canvas
    float initial_center_x = (get_canvas_cols() * get_tile_size()) / 2.0f;
    float initial_center_y = (get_canvas_rows() * get_tile_size()) / 2.0f;
    float initial_center_z = (get_max_layers() * get_tile_size()) / 2.0f;

    set_float_property(CAMERA_X, initial_center_x);
    set_float_property(CAMERA_Y, initial_center_y);
    set_float_property(CAMERA_Z, initial_center_z + (get_max_layers() * get_tile_size() * 2.0f)); // Position camera behind the canvas
    set_float_property(CAMERA_YAW, 0.0f); // Look straight ahead
    set_float_property(CAMERA_PITCH, 0.0f); // Look straight ahead
}
// Generic game state functions
void update_game_state(void) {
    // For now, this function can be used to update any game state logic
    // In a more complex game, this might update physics, AI, animations, etc.
    // For this emoji paint app, it might not need to do much, but we'll keep it for consistency
}

// External declaration for current_project_path
// (already declared at the top of the file)

// Function to get the next available map index
int get_next_map_index(const char* project_path) {
    // Scan project directory for existing map files to determine the next index
    DIR* dir = opendir(project_path);
    if (!dir) {
        return 0;
    }
    
    struct dirent* entry;
    int max_index = -1;
    
    // Scan for files matching pattern "map_*.txt" but not "map_events_*.txt"
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            // Check if file matches pattern "map_*.txt"
            if (strncmp(entry->d_name, "map_", 4) == 0) {
                // Exclude map_events_*.txt files
                if (strncmp(entry->d_name, "map_events_", 11) != 0) {
                    char* ext = strrchr(entry->d_name, '.');
                    if (ext && strcmp(ext, ".txt") == 0) {
                        // Extract the index from the filename
                        int index = -1;
                        sscanf(entry->d_name, "map_%d.txt", &index);
                        if (index > max_index) {
                            max_index = index;
                        }
                    }
                }
            }
        }
    }
    
    closedir(dir);
    return max_index + 1;
}

// Function to update the map legend file
void update_map_legend(const char* project_path, int map_index, const char* map_name) {
    char legend_path[512];
    snprintf(legend_path, sizeof(legend_path), "%s/map_legend.txt", project_path);
    
    // Open the legend file in append mode
    FILE *legend_fp = fopen(legend_path, "a");
    if (!legend_fp) {
        // If file doesn't exist, create it
        legend_fp = fopen(legend_path, "w");
        if (!legend_fp) {
            fprintf(stderr, "Error: Could not create map legend file %s\n", legend_path);
            return;
        }
        // Write header
        fprintf(legend_fp, "index,filename,map_name,created_timestamp\n");
    }
    
    // Get current timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    // Write map entry to legend
    fprintf(legend_fp, "%d,map_%d.txt,%s,%s\n", map_index, map_index, map_name ? map_name : "Untitled", timestamp);
    
    fclose(legend_fp);
}

int save_game_state(const char* filename) {
    // Generic function to save game state
    // Currently saves to CSV format but could be extended to other formats
    
    const char* local_filename = filename;
    char* full_path = NULL;
    int map_index = -1;
    
    // If no filename provided, use current project path or default
    if (!filename) {
        // If we have a current map file loaded, save to that file
        if (current_map_file[0] != '\0') {
            // Use the current map file
            size_t len = strlen(current_project_path) + 1 + strlen(current_map_file) + 1;
            full_path = malloc(len);
            if (!full_path) {
                fprintf(stderr, "Error: Could not allocate memory for full path\n");
                return -1;
            }
            snprintf(full_path, len, "%s/%s", current_project_path, current_map_file);
            local_filename = full_path;
            
            // Extract map index from current_map_file if it follows the pattern map_*.txt
            if (strncmp(current_map_file, "map_", 4) == 0) {
                char* ext = strrchr(current_map_file, '.');
                if (ext && strcmp(ext, ".txt") == 0) {
                    sscanf(current_map_file, "map_%d.txt", &map_index);
                }
            }
        } else if (current_project_path[0] != '\0') {
            // Generate a unique filename for each save
            map_index = get_next_map_index(current_project_path);
            size_t len = strlen(current_project_path) + 32; // Extra space for filename
            full_path = malloc(len);
            if (!full_path) {
                fprintf(stderr, "Error: Could not allocate memory for full path\n");
                return -1;
            }
            snprintf(full_path, len, "%s/map_%d.txt", current_project_path, map_index);
            local_filename = full_path;
            
            // Update map legend
            update_map_legend(current_project_path, map_index, "Untitled Map");
        } else {
            local_filename = "debug.csv";
        }
    } else if (strchr(filename, '/') == NULL && current_project_path[0] != '\0') {
        // If filename doesn't contain path and we have a project path, save to project directory
        size_t len = strlen(current_project_path) + 1 + strlen(filename) + 1;
        full_path = malloc(len);
        if (!full_path) {
            fprintf(stderr, "Error: Could not allocate memory for full path\n");
            return -1;
        }
        snprintf(full_path, len, "%s/%s", current_project_path, filename);
        local_filename = full_path;
        
        // Extract map index from filename if it follows the pattern map_*.txt
        if (strncmp(filename, "map_", 4) == 0) {
            char* ext = strrchr(filename, '.');
            if (ext && strcmp(ext, ".txt") == 0) {
                sscanf(filename, "map_%d.txt", &map_index);
            }
        }
    } else if (filename[0] != '/' && strncmp(filename, "projects/", 9) != 0) {
        // If filename is not absolute and doesn't start with projects/, add projects/ prefix
        size_t len = strlen("projects/") + strlen(filename) + 1;
        full_path = malloc(len);
        if (!full_path) {
            fprintf(stderr, "Error: Could not allocate memory for full path\n");
            return -1;
        }
        snprintf(full_path, len, "projects/%s", filename);
        local_filename = full_path;
    }
    
    // Create temp directory if it doesn't exist
    char temp_dir[512];
    if (current_project_path[0] != '\0') {
        snprintf(temp_dir, sizeof(temp_dir), "%s/temp", current_project_path);
        struct stat st = {0};
        if (stat(temp_dir, &st) == -1) {
            mkdir(temp_dir, 0700);
        }
    }
    
    // Save to temp directory first
    char temp_path[512];
    if (current_project_path[0] != '\0') {
        // Extract just the filename part from local_filename
        const char* filename_part = strrchr(local_filename, '/');
        if (filename_part) {
            filename_part++; // Skip the '/'
        } else {
            filename_part = local_filename;
        }
        
        snprintf(temp_path, sizeof(temp_path), "%s/temp/%s", current_project_path, filename_part);
        
        FILE *fp = fopen(temp_path, "w");
        if (!fp) {
            char msg[1024];
            snprintf(msg, sizeof(msg), "Error: Could not save game state to %s", temp_path);
            set_status_message(msg);
            fprintf(stderr, "Error: Could not open %s for writing\n", temp_path);
            if (full_path) free(full_path);
            return -1;
        }
        
        // New format with x, y, z coordinates, scale, and relative coordinates
        fprintf(fp, "x,y,z,emoji_idx,fg_color_idx,bg_color_idx,scale,rel_x,rel_y,rel_z\n");
        fprintf(fp, "scale_info,0,0,0,0,0,%.2f,0,0,0\n", get_scale_x());
        for (int layer = 0; layer < get_max_layers(); layer++) {
            for (int r = 0; r < get_canvas_rows(); r++) {
                for (int c = 0; c < get_canvas_cols(); c++) {
                    int emoji_idx = get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX);
                    // Only save positions with actual data (not empty spaces)
                    if (emoji_idx != -1) {
                        // Calculate relative coordinates based on scale
                        float rel_x = (get_scale_x() != 1.0f) ? fmod(c, get_scale_x()) : 0.0f;
                        float rel_y = (get_scale_y() != 1.0f) ? fmod(r, get_scale_y()) : 0.0f;
                        float rel_z = (get_scale_z() != 1.0f) ? fmod(layer, get_scale_z()) : 0.0f;
                        
                        fprintf(fp, "%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f\n",
                                c, r, layer,
                                emoji_idx,
                                get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR),
                                get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR),
                                get_scale_x(), rel_x, rel_y, rel_z);
                    }
                }
            }
        }
        
        fclose(fp);
        
        // Now move from temp to final location
        if (rename(temp_path, local_filename) != 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Error: Could not move temp file to %s", local_filename);
            set_status_message(msg);
            fprintf(stderr, "Error: Could not rename %s to %s\n", temp_path, local_filename);
            if (full_path) free(full_path);
            return -1;
        }
    } else {
        // If no project path, save directly
        FILE *fp = fopen(local_filename, "w");
        if (!fp) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Error: Could not save game state to %s", local_filename);
            set_status_message(msg);
            fprintf(stderr, "Error: Could not open %s for writing\n", local_filename);
            if (full_path) free(full_path);
            return -1;
        }
        
        // New format with x, y, z coordinates, scale, and relative coordinates
        fprintf(fp, "x,y,z,emoji_idx,fg_color_idx,bg_color_idx,scale,rel_x,rel_y,rel_z\n");
        fprintf(fp, "scale_info,0,0,0,0,0,%.2f,0,0,0\n", get_scale_x());
        for (int layer = 0; layer < get_max_layers(); layer++) {
            for (int r = 0; r < get_canvas_rows(); r++) {
                for (int c = 0; c < get_canvas_cols(); c++) {
                    int emoji_idx = get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX);
                    // Only save positions with actual data (not empty spaces)
                    if (emoji_idx != -1) {
                        // Calculate relative coordinates based on scale
                        float rel_x = (get_scale_x() != 1.0f) ? fmod(c, get_scale_x()) : 0.0f;
                        float rel_y = (get_scale_y() != 1.0f) ? fmod(r, get_scale_y()) : 0.0f;
                        float rel_z = (get_scale_z() != 1.0f) ? fmod(layer, get_scale_z()) : 0.0f;
                        
                        fprintf(fp, "%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f\n",
                                c, r, layer,
                                emoji_idx,
                                get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR),
                                get_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR),
                                get_scale_x(), rel_x, rel_y, rel_z);
                    }
                }
            }
        }
        
        fclose(fp);
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Saved game state to %s", local_filename);
    set_status_message(msg);
    
    // Clear the dirty flag after successful save
    clear_map_dirty();
    
    // Free dynamically allocated memory
    if (full_path) {
        free(full_path);
    }
    
    return 0;
}

int load_game_state(const char* filename) {
    // Generic function to load game state
    // Currently loads from CSV format but could be extended to other formats
    
    const char* local_filename = filename;
    char* full_path = NULL;
    
    // If no filename provided, use default
    if (!filename) {
        local_filename = "debug.csv";
    } else if (strchr(filename, '/') == NULL && strncmp(filename, "projects/", 9) != 0) {
        // If filename doesn't contain path and doesn't start with projects/, add projects/ prefix
        size_t len = strlen("projects/") + strlen(filename) + 1;
        full_path = malloc(len);
        if (!full_path) {
            fprintf(stderr, "Error: Could not allocate memory for full path\n");
            return -1;
        }
        snprintf(full_path, len, "projects/%s", filename);
        local_filename = full_path;
    }
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Error: Could not load game state from %s", filename);
        set_status_message(msg);
        fprintf(stderr, "Error: Could not open %s for reading\n", filename);
        return -1;
    }
    
    // Clear canvas before loading
    for (int layer = 0; layer < get_max_layers(); layer++) {
        for (int r = 0; r < get_canvas_rows(); r++) {
            for (int c = 0; c < get_canvas_cols(); c++) {
                set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_EMOJI_IDX, -1);
                set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_FG_COLOR, 0);
                set_game_object_attribute(&model_context.canvas[layer][r][c], ATTR_BG_COLOR, 7);
            }
        }
    }
    
    char line[256];
    // Skip header
    if (!fgets(line, sizeof(line), fp)) {
        set_status_message("Error: Empty or invalid game state file");
        fclose(fp);
        return -1;
    }
    
    int loaded_entries = 0;
    
    // Parse header to determine format
    bool is_new_format = (strstr(line, "x,y,z,") != NULL);
    bool has_scale_info = (strstr(line, "scale,rel_x,rel_y,rel_z") != NULL);
    
    while (fgets(line, sizeof(line), fp)) {
        int x, y, z, emoji_idx, fg_color_idx, bg_color_idx;
        float scale = 1.0f, rel_x = 0.0f, rel_y = 0.0f, rel_z = 0.0f;
        int parsed;
        
        if (is_new_format && has_scale_info) {
            // New format with scale info: x,y,z,emoji_idx,fg_color_idx,bg_color_idx,scale,rel_x,rel_y,rel_z
            // Check if this is the scale info line
            if (strncmp(line, "scale_info,", 11) == 0) {
                parsed = sscanf(line, "scale_info,%*d,%*d,%*d,%*d,%*d,%f,%*f,%*f,%*f", &scale);
                if (parsed == 1) {
                    set_scale_x(scale);
                    set_scale_y(scale);
                    set_scale_z(scale);
                }
                continue; // Skip to next line
            }
            
            parsed = sscanf(line, "%d,%d,%d,%d,%d,%d,%f,%f,%f,%f", &x, &y, &z, &emoji_idx, &fg_color_idx, &bg_color_idx, &scale, &rel_x, &rel_y, &rel_z);
            if (parsed != 10) {
                fprintf(stderr, "Warning: Invalid line in %s: %s", filename, line);
                continue;
            }
        } else if (is_new_format) {
            // New format without scale info: x,y,z,emoji_idx,fg_color_idx,bg_color_idx
            parsed = sscanf(line, "%d,%d,%d,%d,%d,%d", &x, &y, &z, &emoji_idx, &fg_color_idx, &bg_color_idx);
            if (parsed != 6) {
                fprintf(stderr, "Warning: Invalid line in %s: %s", filename, line);
                continue;
            }
        } else {
            // Old format: emoji_idx,fg_color_idx,bg_color_idx,layer
            int layer;
            parsed = sscanf(line, "%d,%d,%d,%d", &emoji_idx, &fg_color_idx, &bg_color_idx, &layer);
            if (parsed != 4) {
                fprintf(stderr, "Warning: Invalid line in %s: %s", filename, line);
                continue;
            }
            
            // Calculate x, y from loaded_entries (old method)
            int idx = loaded_entries % (get_canvas_rows() * get_canvas_cols());
            z = layer;
            y = idx / get_canvas_cols();
            x = idx % get_canvas_cols();
        }
        
        // Validate indices
        if (z < 0 || z >= get_max_layers() ||
            y < 0 || y >= get_canvas_rows() ||
            x < 0 || x >= get_canvas_cols() ||
            emoji_idx < -1 || emoji_idx >= get_num_emojis() ||
            fg_color_idx < 0 || fg_color_idx >= get_num_colors() ||
            bg_color_idx < 0 || bg_color_idx >= get_num_colors()) {
            fprintf(stderr, "Warning: Invalid data in %s: x=%d, y=%d, z=%d, emoji=%d, fg=%d, bg=%d\n",
                    filename, x, y, z, emoji_idx, fg_color_idx, bg_color_idx);
            continue;
        }
        
        if (z >= 0 && z < get_max_layers() && y >= 0 && y < get_canvas_rows() && x >= 0 && x < get_canvas_cols()) {
            set_game_object_attribute(&model_context.canvas[z][y][x], ATTR_EMOJI_IDX, emoji_idx);
            set_game_object_attribute(&model_context.canvas[z][y][x], ATTR_FG_COLOR, fg_color_idx);
            set_game_object_attribute(&model_context.canvas[z][y][x], ATTR_BG_COLOR, bg_color_idx);
            loaded_entries++;
        }
    }
    
    fclose(fp);
    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded game state from %s", local_filename);
    set_status_message(msg);
    
    // Free dynamically allocated memory
    if (full_path) {
        free(full_path);
    }
    
    return 0;
}

// Backward compatibility functions
void save_canvas(void) {
    save_game_state("debug.csv");
}

void load_canvas(void) {
    load_game_state("debug.csv");
}

// Function definitions for scale getters/setters
float get_scale_x(void) { return get_float_property(SCALE_X); }
float get_scale_y(void) { return get_float_property(SCALE_Y); }
float get_scale_z(void) { return get_float_property(SCALE_Z); }
void set_scale_x(float scale) { set_float_property(SCALE_X, scale); }
void set_scale_y(float scale) { set_float_property(SCALE_Y, scale); }
void set_scale_z(float scale) { set_float_property(SCALE_Z, scale); }
// World generation functions
void generate_terrain(int width, int depth, int height) {
    // Initialize the canvas with basic terrain
    // Using the existing canvas structure as the world
    
    // Generate a basic terrain with different biomes based on position
    for (int layer = 0; layer < height && layer < get_max_layers(); layer++) {
        for (int r = 0; r < get_canvas_rows() && r < width; r++) {
            for (int c = 0; c < get_canvas_cols() && c < depth; c++) {
                // Calculate distance from center for biome determination
                int center_x = get_canvas_cols() / 2;
                int center_y = get_canvas_rows() / 2;
                float dist_x = abs(c - center_x);
                float dist_y = abs(r - center_y);
                
                // Calculate biome type based on distance and noise-like function
                float biome_factor = (dist_x + dist_y) / (float)(center_x + center_y);
                
                BlockType block_type = BLOCK_AIR; // Default air above ground
                
                // Generate ground layer based on biome
                if (layer == 0) {
                    // Bedrock layer (bottom)
                    block_type = BLOCK_BEDROCK;
                } else if (layer < 3) {
                    // Underground stone layers
                    block_type = BLOCK_STONE;
                } else if (layer == 3) {
                    // Surface layer - different biomes
                    if (biome_factor < 0.2) {
                        // Center area - grass biome
                        block_type = BLOCK_GRASS;
                    } else if (biome_factor < 0.4) {
                        // Forest biome
                        block_type = BLOCK_DIRT;
                    } else if (biome_factor < 0.7) {
                        // More arid biome
                        block_type = BLOCK_SAND;
                    } else {
                        // Mountain biome
                        block_type = BLOCK_STONE;
                    }
                } else if (layer == 4 && (c == center_x && r == center_y) && height >= 4) {
                    // Place a tree in the center
                    block_type = BLOCK_WOOD; // Just trunk at the center
                }
                
                // Set the block in the canvas
                set_canvas_tile(layer, r, c, 0, block_type);
                set_canvas_tile(layer, r, c, 1, 7); // Default white foreground
                set_canvas_tile(layer, r, c, 2, 0); // Default black background
            }
        }
    }
}

void set_player_spawn() {
    // Set player initial position in the center of the world at ground level
    int center_x = get_canvas_cols() / 2;
    int center_y = get_canvas_rows() / 2;
    
    // Find the surface level at the center by checking from top down
    int surface_layer = 0;
    for (int layer = get_max_layers() - 1; layer >= 0; layer--) {
        if (get_canvas_tile(layer, center_y, center_x, 0) != -1 && 
            get_canvas_tile(layer, center_y, center_x, 0) != BLOCK_AIR) {
            surface_layer = layer + 1; // Place player above the surface
            break;
        }
    }
    
    // Set player position to spawn point
    model_context.game_state.player.x = center_x * get_tile_size();
    model_context.game_state.player.y = surface_layer * get_tile_size();
    model_context.game_state.player.z = center_y * get_tile_size();
    
    // Set initial rotation facing outward
    model_context.game_state.player.yaw = 0.0f;
    model_context.game_state.player.pitch = 0.0f;
    
    // Player starts on ground
    model_context.game_state.player.on_ground = true;
    model_context.game_state.player.is_jumping = false;
}

void init_world_generation() {
    // Generate the basic world terrain
    generate_terrain(get_canvas_cols(), get_canvas_rows(), get_max_layers());
    
    // Set the player spawn point
    set_player_spawn();
    
    // Initialize player inventory with basic blocks
    for (int i = 0; i < 9; i++) {
        model_context.game_state.inventory.slots[i].block_type = BLOCK_AIR;
        model_context.game_state.inventory.slots[i].count = 0;
    }
    
    // Give player some initial blocks
    model_context.game_state.inventory.slots[0].block_type = BLOCK_DIRT;
    model_context.game_state.inventory.slots[0].count = 10;
    model_context.game_state.inventory.slots[1].block_type = BLOCK_WOOD;
    model_context.game_state.inventory.slots[1].count = 10;
    model_context.game_state.inventory.slots[2].block_type = BLOCK_STONE;
    model_context.game_state.inventory.slots[2].count = 10;
    model_context.game_state.inventory.selected_slot = 0;
}
