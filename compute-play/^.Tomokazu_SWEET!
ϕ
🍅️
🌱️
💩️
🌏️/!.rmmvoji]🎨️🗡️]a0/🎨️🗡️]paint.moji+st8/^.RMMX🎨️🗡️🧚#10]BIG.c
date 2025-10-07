#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

// --- Globals ---

FT_Library ft;
FT_Face emoji_face;

Display *x_display = NULL;
Window x_window;

int canvas_rows;
int canvas_cols;
int tile_size = 32;
int num_emojis = 64;
int num_colors = 8;
int max_layers = 2;
int max_tabs = 10;
int sidebar_width;
int file_tab_height = 30;

#define MAX_CANVAS_DIM 128

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

typedef struct { 
    int emoji_idx; 
    int fg_color; 
    int bg_color; 
} Tile;

Tile canvas[2][MAX_CANVAS_DIM][MAX_CANVAS_DIM]; // MAX_LAYERS is 2
Tile tab_bank[10];
int tab_count = 0;

int window_width;
int window_height;

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};
char status_message[256] = "Select tool and paint!";

int selected_emoji = 0;
int selected_fg_color = 0;
int selected_bg_color = 7; // Black default
int selected_tool = 0; // 0: Paint, 1: Fill, 2: Rectangle
int start_row = -1, start_col = -1; // For rectangle tool
int selector_row = 0, selector_col = 0; // Tile selector position
bool show_all_layers = true; // Terminal layer toggle

// --- Function Prototypes ---

void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y, float fg[3], float bg[3]);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void init();
void idle();
void draw_rect(float x, float y, float w, float h, float color[3]);
void draw_border(float x, float y, float w, float h, float color[3]);
void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg);
void draw_rectangle(int layer, int r1, int c1, int r2, int c2);
void save_canvas();
void load_canvas();
void print_ascii_grid();
int check_terminal_input();

// --- UTF8 & Font Helpers ---

int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0) {
        if ((str[1] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            return 2;
        }
    }
    if ((str[0] & 0xF0) == 0xE0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            return 3;
        }
    }
    if ((str[0] & 0xF8) == 0xF0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            return 4;
        }
    }
    *codepoint = '?';
    return 1;
}

void render_emoji(unsigned int codepoint, float x, float y, float fg[3], float bg[3]) {
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        fprintf(stderr, "Error: Could not load glyph for codepoint U+%04X, error code: %d\n", codepoint, err);
        return;
    }

    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer || slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        fprintf(stderr, "Error: Invalid bitmap for glyph U+%04X\n", codepoint);
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw background first
    draw_rect(x - tile_size/2, y - tile_size/2, tile_size, tile_size, bg);

    // Then draw emoji
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glColor3fv(fg);

    float scale_factor = emoji_scale;
    float w = slot->bitmap.width * scale_factor;
    float h = slot->bitmap.rows * scale_factor;
    float x2 = x - w / 2;
    float y2 = y - h / 2;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex2f(x2, y2);
    glTexCoord2f(1.0, 1.0); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0, 0.0); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0, 0.0); glVertex2f(x2, y2 + h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void render_text(const char* str, float x, float y) {
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
    glColor3f(1.0f, 1.0f, 1.0f);
}

void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        exit(1);
    }

    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error: Could not load emoji font at %s, error code: %d\n", emoji_font_path, err);
        emoji_face = NULL;
        exit(1);
    }
    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, tile_size - 10);
        if (err) {
            fprintf(stderr, "Error: Could not set pixel size for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            exit(1);
        }
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
        if (err) {
            fprintf(stderr, "Error: Could not select size for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            exit(1);
        }
    } else {
        fprintf(stderr, "Error: No fixed sizes available in emoji font\n");
        FT_Done_Face(emoji_face);
        emoji_face = NULL;
        exit(1);
    }

    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)(tile_size * 0.8f) / (float)loaded_emoji_size;
    fprintf(stderr, "Emoji font loaded, loaded size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
}

void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
}

void draw_border(float x, float y, float w, float h, float color[3]) {
    glLineWidth(2.0f);
    glColor3fv(color);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
}

void set_status_message(const char* msg) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

// --- Game Logic ---

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

void save_canvas() {
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

void load_canvas() {
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
    glutPostRedisplay();
}

// --- ASCII Rendering ---

void print_ascii_grid() {
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
                    if (!show_all_layers && layer != max_layers - 1) continue;
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

// --- Terminal Input ---

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

// --- Display ---

void display() {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float sidebar_x = 10; // Define sidebar_x here

    // Draw canvas
    float grid_color[3] = {0.4f, 0.4f, 0.4f};
    for (int r = 0; r < canvas_rows; r++) {
        for (int c = 0; c < canvas_cols; c++) {
            float x = sidebar_width + 10 + c * tile_size;
            float y = window_height - file_tab_height - 10 - (r + 1) * tile_size;
            draw_rect(x, y, tile_size, tile_size, grid_color);
        }
    }

    for (int layer = 0; layer < max_layers; layer++) {
        for (int r = 0; r < canvas_rows; r++) {
            for (int c = 0; c < canvas_cols; c++) {
                if (canvas[layer][r][c].emoji_idx == -1) continue;
                float x = sidebar_width + 10 + c * tile_size + tile_size / 2;
                float y = window_height - file_tab_height - 10 - (r + 1) * tile_size + tile_size / 2;
                const char *emoji = emojis[canvas[layer][r][c].emoji_idx];
                unsigned int codepoint;
                decode_utf8((const unsigned char*)emoji, &codepoint);
                float fg[3] = {
                    atof(colors[canvas[layer][r][c].fg_color][0]) / 255.0f,
                    atof(colors[canvas[layer][r][c].fg_color][1]) / 255.0f,
                    atof(colors[canvas[layer][r][c].fg_color][2]) / 255.0f
                };
                float bg[3] = {
                    atof(colors[canvas[layer][r][c].bg_color][0]) / 255.0f,
                    atof(colors[canvas[layer][r][c].bg_color][1]) / 255.0f,
                    atof(colors[canvas[layer][r][c].bg_color][2]) / 255.0f
                };
                render_emoji(codepoint, x, y, fg, bg);
            }
        }
    }

    // Draw tile selector
    float sel_color[3] = {1.0f, 1.0f, 0.0f}; // Yellow
    if (selected_tool == 2 && start_row != -1) {
        // Highlight rectangle region
        int r_min = start_row < selector_row ? start_row : selector_row;
        int r_max = start_row > selector_row ? start_row : selector_row;
        int c_min = start_col < selector_col ? start_col : selector_col;
        int c_max = start_col > selector_col ? start_col : selector_col;
        float x = sidebar_width + 10 + c_min * tile_size;
        float y = window_height - file_tab_height - 10 - (r_max + 1) * tile_size;
        float w = (c_max - c_min + 1) * tile_size;
        float h = (r_max - r_min + 1) * tile_size;
        draw_border(x, y, w, h, sel_color);
    } else {
        // Highlight single tile
        float x = sidebar_width + 10 + selector_col * tile_size;
        float y = window_height - file_tab_height - 10 - (selector_row + 1) * tile_size;
        draw_border(x, y, tile_size, tile_size, sel_color);
    }

    // Draw file tab with additional buttons
    float tab_color[3] = {0.5f, 0.5f, 0.5f};
    draw_rect(sidebar_x, window_height - file_tab_height - 10, 60, file_tab_height, tab_color);
    draw_rect(sidebar_x + 70, window_height - file_tab_height - 10, 60, file_tab_height, tab_color);
    draw_rect(sidebar_x + 140, window_height - file_tab_height - 10, 60, file_tab_height, tab_color);
    draw_rect(sidebar_x + 210, window_height - file_tab_height - 10, 60, file_tab_height, tab_color);
    draw_rect(sidebar_x + 280, window_height - file_tab_height - 10, 60, file_tab_height, tab_color);
    draw_rect(sidebar_x + 350, window_height - file_tab_height - 10, 60, file_tab_height, tab_color);
    render_text("Save", sidebar_x + 10, window_height - file_tab_height + 5);
    render_text("Load", sidebar_x + 80, window_height - file_tab_height + 5);
    render_text("Text", sidebar_x + 150, window_height - file_tab_height + 5);
    render_text("Tile", sidebar_x + 220, window_height - file_tab_height + 5);
    render_text("2D", sidebar_x + 290, window_height - file_tab_height + 5);
    render_text("3D", sidebar_x + 360, window_height - file_tab_height + 5);

    // Draw sidebar (left side, below file tab)
    float sidebar_y_offset = file_tab_height + 50; // Increased offset to avoid overlap
    float base_y = window_height - sidebar_y_offset - 20;

    // Emoji grid (8x8)
    int emoji_cols = 8;
    int emoji_rows = (num_emojis + emoji_cols - 1) / emoji_cols;
    for (int i = 0; i < num_emojis; i++) {
        int row = i / emoji_cols;
        int col = i % emoji_cols;
        float x = sidebar_x + 10 + col * 40;
        float y = base_y - row * 40;
        unsigned int codepoint;
        decode_utf8((const unsigned char*)emojis[i], &codepoint);
        float fg[3] = {1.0f, 1.0f, 1.0f};
        float bg[3] = {0.0f, 0.0f, 0.0f};
        render_emoji(codepoint, x + 20, y + 20, fg, bg);
        if (i == selected_emoji) {
            draw_border(x, y, 40, 40, sel_color);
        }
    }

    // Colors (horizontal row below emojis)
    float colors_y_start = base_y - (emoji_rows * 40) - 20;
    int colors_cols = 8;
    int colors_rows = 1;
    for (int i = 0; i < num_colors; i++) {
        int row = i / colors_cols;
        int col = i % colors_cols;
        float x = sidebar_x + 10 + col * 40;
        float y = colors_y_start - row * 40;
        float color[3] = {
            atof(colors[i][0]) / 255.0f,
            atof(colors[i][1]) / 255.0f,
            atof(colors[i][2]) / 255.0f
        };
        draw_rect(x, y, 40, 40, color);
        if (i == selected_fg_color) {
            draw_border(x, y, 40, 40, sel_color);
        }
    }

    // Tab bank (vertical below colors)
    float tabs_y_start = colors_y_start - (colors_rows * 40) - 20;
    for (int i = 0; i < tab_count; i++) {
        float x = sidebar_x + 10;
        float y = tabs_y_start - i * 40;
        unsigned int codepoint;
        decode_utf8((const unsigned char*)emojis[tab_bank[i].emoji_idx], &codepoint);
        float fg[3] = {
            atof(colors[tab_bank[i].fg_color][0]) / 255.0f,
            atof(colors[tab_bank[i].fg_color][1]) / 255.0f,
            atof(colors[tab_bank[i].fg_color][2]) / 255.0f
        };
        float bg[3] = {
            atof(colors[tab_bank[i].bg_color][0]) / 255.0f,
            atof(colors[tab_bank[i].bg_color][1]) / 255.0f,
            atof(colors[tab_bank[i].bg_color][2]) / 255.0f
        };
        render_emoji(codepoint, x + 20, y + 20, fg, bg);
    }

    // Tools (horizontal below tabs)
    float tabs_section_height = tab_count * 40;
    float tools_y_start = tabs_y_start - tabs_section_height - 20;
    const char *tools[] = {"Paint", "Fill", "Rect"};
    for (int i = 0; i < 3; i++) {
        float x = sidebar_x + 10 + i * 70;
        float y = tools_y_start;
        draw_rect(x, y, 60, 30, tab_color);
        render_text(tools[i], x + 10, y + 10);
        if (i == selected_tool) {
            draw_border(x, y, 60, 30, sel_color);
        }
    }

    render_text(status_message, sidebar_width + 10, 10);
    glutSwapBuffers();
}

// --- GLUT Callbacks ---

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        float gl_y = window_height - y;
        float sidebar_x = 10; // Define sidebar_x here
        // Canvas click
        int col = (x - sidebar_width - 10) / tile_size;
        int row = canvas_rows - 1 - ((gl_y - file_tab_height - 10) / tile_size); // Flip row
        if (col >= 0 && col < canvas_cols && row >= 0 && row < canvas_rows) {
            selector_row = row;
            selector_col = col;
            if (selected_tool == 0) { // Paint
                canvas[max_layers-1][row][col].emoji_idx = selected_emoji;
                canvas[max_layers-1][row][col].fg_color = selected_fg_color;
                canvas[max_layers-1][row][col].bg_color = selected_bg_color;
            } else if (selected_tool == 1) { // Fill
                flood_fill(max_layers-1, row, col,
                           canvas[max_layers-1][row][col].emoji_idx,
                           canvas[max_layers-1][row][col].fg_color,
                           canvas[max_layers-1][row][col].bg_color);
            } else if (selected_tool == 2) { // Rectangle
                if (start_row == -1) {
                    start_row = row;
                    start_col = col;
                    set_status_message("Select second corner for rectangle");
                } else {
                    draw_rectangle(max_layers-1, start_row, start_col, row, col);
                    start_row = -1;
                    start_col = -1;
                    set_status_message("Rectangle drawn");
                }
            }
        }
        // File tab click
        else if (x >= sidebar_x && x < sidebar_x + 60 && gl_y >= window_height - file_tab_height - 10 && gl_y < window_height - 10) {
            save_canvas();
        } else if (x >= sidebar_x + 70 && x < sidebar_x + 130 && gl_y >= window_height - file_tab_height - 10 && gl_y < window_height - 10) {
            load_canvas();
        } else if (x >= sidebar_x + 140 && x < sidebar_x + 200 && gl_y >= window_height - file_tab_height - 10 && gl_y < window_height - 10) {
            set_status_message("Text button clicked (no functionality)");
        } else if (x >= sidebar_x + 210 && x < sidebar_x + 270 && gl_y >= window_height - file_tab_height - 10 && gl_y < window_height - 10) {
            set_status_message("Tile button clicked (no functionality)");
        } else if (x >= sidebar_x + 280 && x < sidebar_x + 340 && gl_y >= window_height - file_tab_height - 10 && gl_y < window_height - 10) {
            set_status_message("2D button clicked (no functionality)");
        } else if (x >= sidebar_x + 350 && x < sidebar_x + 410 && gl_y >= window_height - file_tab_height - 10 && gl_y < window_height - 10) {
            set_status_message("3D button clicked (no functionality)");
        }
        // Sidebar click - Emojis
        float sidebar_y_offset = file_tab_height + 50; // Match display offset
        float base_y = window_height - sidebar_y_offset - 20;
        int emoji_cols = 8;
        int emoji_rows = (num_emojis + emoji_cols - 1) / emoji_cols;
        float emoji_section_top = base_y + 40;
        float emoji_section_bottom = base_y - (emoji_rows - 1) * 40;
        if (x >= sidebar_x + 10 && x < sidebar_x + 10 + emoji_cols * 40 &&
            gl_y >= emoji_section_bottom && gl_y <= emoji_section_top) {
            float local_y_from_top = emoji_section_top - gl_y;
            int row = (int)(local_y_from_top / 40);
            int col = (int)((x - (sidebar_x + 10)) / 40);
            if (row >= 0 && row < emoji_rows && col >= 0 && col < emoji_cols) {
                int idx = row * emoji_cols + col;
                if (idx < num_emojis) {
                    selected_emoji = idx;
                    set_status_message("Emoji selected");
                }
            }
        }
        // Colors
        float colors_y_start = base_y - (emoji_rows * 40) - 20;
        int colors_cols = 8;
        int colors_rows = 1;
        float colors_section_top = colors_y_start + 40;
        float colors_section_bottom = colors_y_start - (colors_rows - 1) * 40;
        if (x >= sidebar_x + 10 && x < sidebar_x + 10 + colors_cols * 40 &&
            gl_y >= colors_section_bottom && gl_y <= colors_section_top) {
            float local_y_from_top = colors_section_top - gl_y;
            int row = (int)(local_y_from_top / 40);
            int col = (int)((x - (sidebar_x + 10)) / 40);
            if (row >= 0 && row < colors_rows && col >= 0 && col < colors_cols) {
                int idx = row * colors_cols + col;
                if (idx < num_colors) {
                    selected_fg_color = idx;
                    set_status_message("Color selected");
                }
            }
        }
        // Tabs
        float tabs_y_start = colors_y_start - (colors_rows * 40) - 20;
        float tabs_section_top = tabs_y_start + 40;
        float tabs_section_bottom = tabs_y_start - (tab_count - 1) * 40;
        if (x >= sidebar_x + 10 && x < sidebar_x + 50 &&
            gl_y >= tabs_section_bottom && gl_y <= tabs_section_top) {
            float local_y_from_top = tabs_section_top - gl_y;
            int idx = (int)(local_y_from_top / 40);
            if (idx >= 0 && idx < tab_count) {
                selected_emoji = tab_bank[idx].emoji_idx;
                selected_fg_color = tab_bank[idx].fg_color;
                selected_bg_color = tab_bank[idx].bg_color;
                set_status_message("Tab selected");
            }
        }
        // Tools
        float tabs_section_height = tab_count * 40;
        float tools_y_start = tabs_y_start - tabs_section_height - 20;
        float tools_section_top = tools_y_start + 30;
        float tools_section_bottom = tools_y_start;
        if (x >= sidebar_x + 10 && x < sidebar_x + 10 + 3 * 70 &&
            gl_y >= tools_section_bottom && gl_y <= tools_section_top) {
            int idx = (int)((x - (sidebar_x + 10)) / 70);
            if (idx >= 0 && idx < 3) {
                selected_tool = idx;
                start_row = -1;
                start_col = -1;
                set_status_message("Tool selected");
            }
        }
        print_ascii_grid();
        glutPostRedisplay();
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (key == ' ') {
        // Mouse-driven paint, but allow space as alternative to Enter
        if (selected_tool == 0) {
            canvas[max_layers-1][selector_row][selector_col].emoji_idx = selected_emoji;
            canvas[max_layers-1][selector_row][selector_col].fg_color = selected_fg_color;
            canvas[max_layers-1][selector_row][selector_col].bg_color = selected_bg_color;
            set_status_message("Tile painted");
        } else if (selected_tool == 1) {
            flood_fill(max_layers-1, selector_row, selector_col,
                       canvas[max_layers-1][selector_row][selector_col].emoji_idx,
                       canvas[max_layers-1][selector_row][selector_col].fg_color,
                       canvas[max_layers-1][selector_row][selector_col].bg_color);
            set_status_message("Area filled");
        } else if (selected_tool == 2) {
            if (start_row == -1) {
                start_row = selector_row;
                start_col = selector_col;
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(max_layers-1, start_row, start_col, selector_row, selector_col);
                start_row = -1;
                start_col = -1;
                set_status_message("Rectangle drawn");
            }
        }
    } else if (key == '\r') { // Enter key
        if (selected_tool == 0) {
            canvas[max_layers-1][selector_row][selector_col].emoji_idx = selected_emoji;
            canvas[max_layers-1][selector_row][selector_col].fg_color = selected_fg_color;
            canvas[max_layers-1][selector_row][selector_col].bg_color = selected_bg_color;
            set_status_message("Tile painted");
        } else if (selected_tool == 1) {
            flood_fill(max_layers-1, selector_row, selector_col,
                       canvas[max_layers-1][selector_row][selector_col].emoji_idx,
                       canvas[max_layers-1][selector_row][selector_col].fg_color,
                       canvas[max_layers-1][selector_row][selector_col].bg_color);
            set_status_message("Area filled");
        } else if (selected_tool == 2) {
            if (start_row == -1) {
                start_row = selector_row;
                start_col = selector_col;
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(max_layers-1, start_row, start_col, selector_row, selector_col);
                start_row = -1;
                start_col = -1;
                set_status_message("Rectangle drawn");
            }
        }
    } else if (key == 'f') {
        selected_tool = 1;
        start_row = -1;
        start_col = -1;
        set_status_message("Fill tool selected");
    } else if (key == 'r') {
        selected_tool = 2;
        start_row = -1;
        start_col = -1;
        set_status_message("Rectangle tool selected");
    } else if (key == '1') {
        selected_emoji = (selected_emoji + 1) % num_emojis;
        set_status_message("Emoji selected");
    } else if (key == 'c') {
        selected_fg_color = (selected_fg_color + 1) % num_colors;
        set_status_message("Color selected");
    } else if (key == '2') {
        show_all_layers = !show_all_layers;
        set_status_message(show_all_layers ? "Showing all layers" : "Showing top layer");
    } else if (key == 's') {
        save_canvas();
    } else if (key == 'l') {
        load_canvas();
    } else if (key == 't' && tab_count < max_tabs) {
        tab_bank[tab_count].emoji_idx = selected_emoji;
        tab_bank[tab_count].fg_color = selected_fg_color;
        tab_bank[tab_count].bg_color = selected_bg_color;
        tab_count++;
        set_status_message("Tab created");
    } else if (key == 'q' || key == 'Q') {
        if (emoji_face) FT_Done_Face(emoji_face);
        FT_Done_FreeType(ft);
        exit(0);
    }
    print_ascii_grid();
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_UP:
            if (selector_row > 0) selector_row--;
            break;
        case GLUT_KEY_DOWN:
            if (selector_row < canvas_rows - 1) selector_row++;
            break;
        case GLUT_KEY_LEFT:
            if (selector_col > 0) selector_col--;
            break;
        case GLUT_KEY_RIGHT:
            if (selector_col < canvas_cols - 1) selector_col++;
            break;
    }
    set_status_message("Selector moved");
    print_ascii_grid();
    glutPostRedisplay();
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle() {
    int ch = check_terminal_input();
    if (ch == ' ') {
        if (selected_tool == 0) {
            canvas[max_layers-1][selector_row][selector_col].emoji_idx = selected_emoji;
            canvas[max_layers-1][selector_row][selector_col].fg_color = selected_fg_color;
            canvas[max_layers-1][selector_row][selector_col].bg_color = selected_bg_color;
            set_status_message("Tile painted");
        } else if (selected_tool == 1) {
            flood_fill(max_layers-1, selector_row, selector_col,
                       canvas[max_layers-1][selector_row][selector_col].emoji_idx,
                       canvas[max_layers-1][selector_row][selector_col].fg_color,
                       canvas[max_layers-1][selector_row][selector_col].bg_color);
            set_status_message("Area filled");
        } else if (selected_tool == 2) {
            if (start_row == -1) {
                start_row = selector_row;
                start_col = selector_col;
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(max_layers-1, start_row, start_col, selector_row, selector_col);
                start_row = -1;
                start_col = -1;
                set_status_message("Rectangle drawn");
            }
        }
    } else if (ch == '\r') { // Enter key
        if (selected_tool == 0) {
            canvas[max_layers-1][selector_row][selector_col].emoji_idx = selected_emoji;
            canvas[max_layers-1][selector_row][selector_col].fg_color = selected_fg_color;
            canvas[max_layers-1][selector_row][selector_col].bg_color = selected_bg_color;
            set_status_message("Tile painted");
        } else if (selected_tool == 1) {
            flood_fill(max_layers-1, selector_row, selector_col,
                       canvas[max_layers-1][selector_row][selector_col].emoji_idx,
                       canvas[max_layers-1][selector_row][selector_col].fg_color,
                       canvas[max_layers-1][selector_row][selector_col].bg_color);
            set_status_message("Area filled");
        } else if (selected_tool == 2) {
            if (start_row == -1) {
                start_row = selector_row;
                start_col = selector_col;
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(max_layers-1, start_row, start_col, selector_row, selector_col);
                start_row = -1;
                start_col = -1;
                set_status_message("Rectangle drawn");
            }
        }
    } else if (ch == 'f') {
        selected_tool = 1;
        start_row = -1;
        start_col = -1;
        set_status_message("Fill tool selected");
    } else if (ch == 'r') {
        selected_tool = 2;
        start_row = -1;
        start_col = -1;
        set_status_message("Rectangle tool selected");
    } else if (ch == '1') {
        selected_emoji = (selected_emoji + 1) % num_emojis;
        set_status_message("Emoji selected");
    } else if (ch == 'c') {
        selected_fg_color = (selected_fg_color + 1) % num_colors;
        set_status_message("Color selected");
    } else if (ch == '2') {
        show_all_layers = !show_all_layers;
        set_status_message(show_all_layers ? "Showing all layers" : "Showing top layer");
    } else if (ch == 's') {
        save_canvas();
    } else if (ch == 'l') {
        load_canvas();
    } else if (ch == 't' && tab_count < max_tabs) {
        tab_bank[tab_count].emoji_idx = selected_emoji;
        tab_bank[tab_count].fg_color = selected_fg_color;
        tab_bank[tab_count].bg_color = selected_bg_color;
        tab_count++;
        set_status_message("Tab created");
    } else if (ch == 'q' || ch == 'Q') {
        if (emoji_face) FT_Done_Face(emoji_face);
        FT_Done_FreeType(ft);
        exit(0);
    }
    if (ch != -1) {
        print_ascii_grid();
        glutPostRedisplay();
    }
}

// --- Main ---

void init() {
    initFreeType();
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
    print_ascii_grid();
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
}

int main(int argc, char** argv) {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    // Get screen size for near full-screen window
    Display *disp = XOpenDisplay(NULL);
    if (disp) {
        int scr = DefaultScreen(disp);
        int sw = DisplayWidth(disp, scr);
        int sh = DisplayHeight(disp, scr);
        XCloseDisplay(disp);
        window_width = sw - 100;
        window_height = sh - 100;
    } else {
        // Fallback
        window_width = 682;
        window_height = 562;
    }

    // Set sidebar wider for emoji grid
    sidebar_width = 420; // Increased to accommodate new buttons

    // Compute canvas size
    canvas_cols = (window_width - sidebar_width - 20) / tile_size;
    canvas_rows = (window_height - file_tab_height - 20) / tile_size;
    if (canvas_cols > MAX_CANVAS_DIM) canvas_cols = MAX_CANVAS_DIM;
    if (canvas_rows > MAX_CANVAS_DIM) canvas_rows = MAX_CANVAS_DIM;

    // Adjust window size to fit exactly
    window_width = canvas_cols * tile_size + sidebar_width + 20;
    window_height = canvas_rows * tile_size + file_tab_height + 20;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Emoji Paint");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);
    return 0;
}
