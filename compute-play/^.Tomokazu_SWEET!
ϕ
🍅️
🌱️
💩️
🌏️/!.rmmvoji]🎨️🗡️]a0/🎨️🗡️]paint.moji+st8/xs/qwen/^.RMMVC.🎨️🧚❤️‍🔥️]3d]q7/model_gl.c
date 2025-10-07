#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define MAX_CANVAS_LAYERS 10
#define MAX_CANVAS_DIM 128

typedef struct { 
    int emoji_idx; 
    int fg_color; 
    int bg_color; 
} Tile;

// Global variables
int canvas_rows;
int canvas_cols;
int tile_size = 32;
int num_emojis = 64;
int num_colors = 8;
int max_layers = MAX_CANVAS_LAYERS;
int max_tabs = 10;
int sidebar_width;
int file_tab_height = 30;
void initFreeType(void);
int window_width;
int window_height;

FT_Library ft;
FT_Face emoji_face;
float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};
Display *x_display = NULL;
Window x_window;

Tile canvas[MAX_CANVAS_LAYERS][MAX_CANVAS_DIM][MAX_CANVAS_DIM];
Tile tab_bank[10];
int tab_count = 0;

const char *emojis[64] = {
    "🍒", "🍋", "🍊", "💎", "🔔", "💩", "🎨", "🗡️",
    "🍏", "🍐", "🍑", "🍓", "🥝", "🍅", "🥥", "🥑",
    "🥔", "🥕", "🌽", "🌶", "🥒", "🥬", "🥦", "🧄",
    "🧅", "🍄", "🥜", "🌰", "🍞", "🥐", "🥖", "🥨",
    "🥯", "🥞", "🧇", "🧀", "🍖", "🍗", "🥩", "🥓",
    "🍔", "🍟", "🍕", "🌭", "🥪", "🌮", "🌯", "🥙",
    "🧆", "🥚", "🍳", "🥘", "🍲", "🥣", "🥗", "🍿",
    "🧈", "🧂", "🥫", "🍱", "🍘", "🍙", "🍚", "🍛"
};

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

char status_message[256] = "Select tool and paint!";

int selected_emoji = 0;
int selected_fg_color = 0;
int selected_bg_color = 7; // Black default
int selected_tool = 0; // 0: Paint, 1: Fill, 2: Rectangle
int start_row = -1, start_col = -1; // For rectangle tool
int selector_row = 0, selector_col = 0; // Tile selector position
bool show_all_layers = true; // Terminal layer toggle

// New global variables for 3D view and Z-level
static int view_mode = 0; // 0: 2D, 1: 3D
static int current_z_level = 0; // Current Z-level (0-9)
static float camera_x = 0.0f;
static float camera_y = 0.0f;
static float camera_z = 50.0f; // Initial camera Z position
static float camera_yaw = 0.0f; // Y-axis rotation
static float camera_pitch = 0.0f; // X-axis rotation

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

// Function definitions for new getters/setters
int get_view_mode(void) {
    return view_mode;
}

void set_view_mode(int mode) {
    view_mode = mode;
}

int get_current_z_level(void) {
    return current_z_level;
}

void set_current_z_level(int z) {
    if (z >= 0 && z < MAX_CANVAS_LAYERS) {
        current_z_level = z;
    }
}

float get_camera_x(void) {
    return camera_x;
}

void set_camera_x(float x) {
    camera_x = x;
}

float get_camera_y(void) {
    return camera_y;
}

void set_camera_y(float y) {
    camera_y = y;
}

float get_camera_z(void) {
    return camera_z;
}

void set_camera_z(float z) {
    camera_z = z;
}

float get_camera_yaw(void) {
    return camera_yaw;
}

void set_camera_yaw(float yaw) {
    camera_yaw = yaw;
}

float get_camera_pitch(void) {
    return camera_pitch;
}

void set_camera_pitch(float pitch) {
    camera_pitch = pitch;
}

// Function declarations
void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg);
void draw_rectangle(int layer, int r1, int c1, int r2, int c2);
void print_ascii_grid();

// Getters
int get_canvas_rows(void) { return canvas_rows; }
int get_canvas_cols(void) { return canvas_cols; }
int get_tile_size(void) { return tile_size; }
int get_num_emojis(void) { return num_emojis; }
int get_num_colors(void) { return num_colors; }
int get_max_layers(void) { return max_layers; }
int get_max_tabs(void) { return max_tabs; }
int get_sidebar_width(void) { return sidebar_width; }
int get_file_tab_height(void) { return file_tab_height; }
int get_window_width(void) { return window_width; }
int get_window_height(void) { return window_height; }
int get_tab_count(void) { return tab_count; }
const char* get_emoji(int idx) { return emojis[idx]; }
const char* get_color(int idx, int component) { return colors[idx][component]; }
const char* get_color_name(int idx) { return color_names[idx]; }
const char* get_status_message(void) { return status_message; }
int get_selected_emoji(void) { return selected_emoji; }
int get_selected_fg_color(void) { return selected_fg_color; }
int get_selected_bg_color(void) { return selected_bg_color; }
int get_selected_tool(void) { return selected_tool; }
int get_start_row(void) { return start_row; }
int get_start_col(void) { return start_col; }
int get_selector_row(void) { return selector_row; }
int get_selector_col(void) { return selector_col; }
bool get_show_all_layers(void) { return show_all_layers; }
int get_canvas_tile(int layer, int r, int c, int field) {
    if (field == 0) return canvas[layer][r][c].emoji_idx;
    if (field == 1) return canvas[layer][r][c].fg_color;
    if (field == 2) return canvas[layer][r][c].bg_color;
    return -1;
}
int get_tab_bank(int idx, int field) {
    if (field == 0) return tab_bank[idx].emoji_idx;
    if (field == 1) return tab_bank[idx].fg_color;
    if (field == 2) return tab_bank[idx].bg_color;
    return -1;
}

// Setters
void set_canvas_rows(int value) { canvas_rows = value; }
void set_canvas_cols(int value) { canvas_cols = value; }
void set_tile_size(int value) { tile_size = value; }
void set_sidebar_width(int value) { sidebar_width = value; }
void set_window_width(int value) { window_width = value; }
void set_window_height(int value) { window_height = value; }
void set_status_message(const char* msg) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}
void set_selected_emoji(int value) { selected_emoji = value; }
void set_selected_fg_color(int value) { selected_fg_color = value; }
void set_selected_bg_color(int value) { selected_bg_color = value; }
void set_selected_tool(int value) { selected_tool = value; }
void set_start_row(int value) { start_row = value; }
void set_start_col(int value) { start_col = value; }
void set_selector_row(int value) { selector_row = value; }
void set_selector_col(int value) { selector_col = value; }
void set_show_all_layers(bool value) { show_all_layers = value; }
void set_canvas_tile(int layer, int r, int c, int field, int value) {
    if (field == 0) canvas[layer][r][c].emoji_idx = value;
    if (field == 1) canvas[layer][r][c].fg_color = value;
    if (field == 2) canvas[layer][r][c].bg_color = value;
}
void set_tab_bank(int idx, int field, int value) {
    if (field == 0) tab_bank[idx].emoji_idx = value;
    if (field == 1) tab_bank[idx].fg_color = value;
    if (field == 2) tab_bank[idx].bg_color = value;
}
void increment_tab_count(void) { tab_count++; }

// Game logic functions
void init_model(void) {
    srand(time(NULL));
    for (int layer = 0; layer < max_layers; layer++) {
        for (int r = 0; r < canvas_rows; r++) {
            for (int c = 0; c < canvas_cols; c++) {
                canvas[layer][r][c].emoji_idx = -1;
                canvas[layer][r][c].fg_color = 0;
                canvas[layer][r][c].bg_color = 7;
            }
        }
    }
    // Initialize new 3D and Z-level variables
    view_mode = 0; // Start in 2D mode
    current_z_level = 0; // Start at Z-level 0
    // Calculate initial camera position to view the centered canvas
    // Assuming canvas_rows, canvas_cols, max_layers, tile_size are already set
    float initial_center_x = (get_canvas_cols() * get_tile_size()) / 2.0f;
    float initial_center_y = (get_canvas_rows() * get_tile_size()) / 2.0f;
    float initial_center_z = (get_max_layers() * get_tile_size()) / 2.0f;

    camera_x = initial_center_x;
    camera_y = initial_center_y;
    camera_z = initial_center_z + (get_max_layers() * get_tile_size() * 2.0f); // Position camera behind the canvas
    camera_yaw = 0.0f; // Look straight ahead
    camera_pitch = 0.0f; // Look straight ahead
}

void init(void) {
    init_model();
    initFreeType();
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
}

void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg) {
    if (r < 0 || r >= canvas_rows || c < 0 || c >= canvas_cols) return;
    if (canvas[layer][r][c].emoji_idx != old_emoji || canvas[layer][r][c].fg_color != old_fg || canvas[layer][r][c].bg_color != old_bg) return;
    canvas[layer][r][c].emoji_idx = selected_emoji;
    canvas[layer][r][c].fg_color = selected_fg_color;
    canvas[layer][r][c].bg_color = selected_bg_color;
    flood_fill(layer, r+1, c, old_emoji, old_fg, old_bg);
    flood_fill(layer, r-1, c, old_emoji, old_fg, old_bg);
    flood_fill(layer, r, c+1, old_emoji, old_fg, old_bg);
    flood_fill(layer, r, c-1, old_emoji, old_fg, old_bg);
}

void draw_rectangle(int layer, int r1, int c1, int r2, int c2) {
    int r_min = r1 < r2 ? r1 : r2;
    int r_max = r1 > r2 ? r1 : r2;
    int c_min = c1 < c2 ? c1 : c2;
    int c_max = c1 > c2 ? c1 : c2;
    for (int r = r_min; r <= r_max; r++) {
        for (int c = c_min; c <= c_max; c++) {
            if (r >= 0 && r < canvas_rows && c >= 0 && c < canvas_cols) {
                if (selected_tool == 2) { // Outline
                    if (r == r_min || r == r_max || c == c_min || c == c_max) {
                        canvas[layer][r][c].emoji_idx = selected_emoji;
                        canvas[layer][r][c].fg_color = selected_fg_color;
                        canvas[layer][r][c].bg_color = selected_bg_color;
                    }
                } else { // Filled
                    canvas[layer][r][c].emoji_idx = selected_emoji;
                    canvas[layer][r][c].fg_color = selected_fg_color;
                    canvas[layer][r][c].bg_color = selected_bg_color;
                }
            }
        }
    }
}

void save_canvas(void) {
    FILE *fp = fopen("debug.csv", "w");
    if (!fp) {
        set_status_message("Error: Could not save to debug.csv");
        fprintf(stderr, "Error: Could not open debug.csv for writing\n");
        return;
    }
    fprintf(fp, "emoji_idx,fg_color_idx,bg_color_idx,layer\n");
    for (int layer = 0; layer < max_layers; layer++) {
        for (int r = 0; r < canvas_rows; r++) {
            for (int c = 0; c < canvas_cols; c++) {
                fprintf(fp, "%d,%d,%d,%d\n",
                        canvas[layer][r][c].emoji_idx,
                        canvas[layer][r][c].fg_color,
                        canvas[layer][r][c].bg_color,
                        layer);
            }
        }
    }
    fclose(fp);
    set_status_message("Saved to debug.csv");
}

void load_canvas(void) {
    FILE *fp = fopen("debug.csv", "r");
    if (!fp) {
        set_status_message("Error: Could not load debug.csv");
        fprintf(stderr, "Error: Could not open debug.csv for reading\n");
        return;
    }

    // Clear canvas before loading
    for (int layer = 0; layer < max_layers; layer++) {
        for (int r = 0; r < canvas_rows; r++) {
            for (int c = 0; c < canvas_cols; c++) {
                canvas[layer][r][c].emoji_idx = -1;
                canvas[layer][r][c].fg_color = 0;
                canvas[layer][r][c].bg_color = 7;
            }
        }
    }

    char line[256];
    // Skip header
    if (!fgets(line, sizeof(line), fp)) {
        set_status_message("Error: Empty or invalid debug.csv");
        fclose(fp);
        return;
    }

    int expected_entries = max_layers * canvas_rows * canvas_cols;
    int loaded_entries = 0;

    while (fgets(line, sizeof(line), fp) && loaded_entries < expected_entries) {
        int emoji_idx, fg_color_idx, bg_color_idx, layer;
        int parsed = sscanf(line, "%d,%d,%d,%d", &emoji_idx, &fg_color_idx, &bg_color_idx, &layer);
        if (parsed != 4) {
            fprintf(stderr, "Warning: Invalid line in debug.csv: %s", line);
            continue;
        }
        // Validate indices
        if (layer < 0 || layer >= max_layers ||
            emoji_idx < -1 || emoji_idx >= num_emojis ||
            fg_color_idx < 0 || fg_color_idx >= num_colors ||
            bg_color_idx < 0 || bg_color_idx >= num_colors) {
            fprintf(stderr, "Warning: Invalid data in debug.csv: emoji=%d, fg=%d, bg=%d, layer=%d\n",
                    emoji_idx, fg_color_idx, bg_color_idx, layer);
            continue;
        }
        int idx = loaded_entries % (canvas_rows * canvas_cols);
        int l = layer;
        int r = idx / canvas_cols;
        int c = idx % canvas_cols;
        if (l >= 0 && l < max_layers && r >= 0 && r < canvas_rows && c >= 0 && c < canvas_cols) {
            canvas[l][r][c].emoji_idx = emoji_idx;
            canvas[l][r][c].fg_color = fg_color_idx;
            canvas[l][r][c].bg_color = bg_color_idx;
            loaded_entries++;
        }
    }

    fclose(fp);
    if (loaded_entries < expected_entries) {
        fprintf(stderr, "Warning: Loaded %d entries, expected %d\n", loaded_entries, expected_entries);
        set_status_message("Warning: Incomplete load from debug.csv");
    } else {
        set_status_message("Loaded from debug.csv");
    }
    print_ascii_grid();
}

void print_ascii_grid(void) {
    printf("\033[H\033[J"); // Clear terminal
    printf("Emoji Paint (%s Layers)\n", show_all_layers ? "All" : "Top");
    printf("----------------------------------------\n");
    for (int r = 0; r < canvas_rows; r++) {
        for (int c = 0; c < canvas_cols; c++) {
            bool drawn = false;
            if (selected_tool != 2 && r == selector_row && c == selector_col) {
                printf("\033[1;33m[]\033[0m "); // Yellow brackets for selector
                drawn = true;
            } else {
                for (int layer = max_layers - 1; layer >= 0; layer--) {
                    if (!show_all_layers && layer != get_current_z_level()) continue;
                    if (canvas[layer][r][c].emoji_idx != -1) {
                        printf("\033[38;2;%s;%s;%sm%s\033[0m ",
                               colors[canvas[layer][r][c].fg_color][0],
                               colors[canvas[layer][r][c].fg_color][1],
                               colors[canvas[layer][r][c].fg_color][2],
                               emojis[canvas[layer][r][c].emoji_idx]);
                        drawn = true;
                        break;
                    }
                }
            }
            if (!drawn && !(r == selector_row && c == selector_col)) printf("  ");
        }
        printf("|\n");
    }
    if (selected_tool == 2 && start_row != -1) {
        printf("Rectangle Start: (%d, %d)\n", start_row, start_col);
    }
    printf("----------------------------------------\n");
    printf("Emoji Palette: ");
    for (int i = 0; i < num_emojis; i++) {
        if (i == selected_emoji) printf("[");
        printf("%s", emojis[i]);
        if (i == selected_emoji) printf("]");
        printf(" ");
    }
    printf("\nColor Palette: ");
    for (int i = 0; i < num_colors; i++) {
        if (i == selected_fg_color) printf("[");
        printf("%s", color_names[i]);
        if (i == selected_fg_color) printf("]");
        printf(" ");
    }
    printf("\nTab Bank: ");
    for (int i = 0; i < tab_count; i++) {
        printf("\033[38;2;%s;%s;%sm%s\033[0m ",
               colors[tab_bank[i].fg_color][0],
               colors[tab_bank[i].fg_color][1],
               colors[tab_bank[i].fg_color][2],
               emojis[tab_bank[i].emoji_idx]);
    }
    printf("\nTool: %s  Emoji: %s  FG: %s  BG: %s  Selector: (%d, %d)\n",
           selected_tool == 0 ? "Paint" : (selected_tool == 1 ? "Fill" : "Rectangle"),
           emojis[selected_emoji], color_names[selected_fg_color], color_names[selected_bg_color],
           selector_row, selector_col);
    printf("%s\n", status_message);
    printf("SPACE: Paint  F: Fill  R: Rect  1: Emoji  C: Color  2: Layer  S: Save  L: Load  T: Tab  Q: Quit\n");
    printf("Arrows: Move Selector  Enter: Place Tile\n");
}
