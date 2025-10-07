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
#include <stdarg.h>

// --- Constants ---
#define GRID_ROWS 20
#define GRID_COLS 20
#define TILE_SIZE 32

// Emojis
const char* EMOJI_EMPTY = "🟩";
const char* EMOJI_RESIDENTIAL_LOW = "🏡";
const char* EMOJI_RESIDENTIAL_HIGH = "🏘️";
const char* EMOJI_COMMERCIAL = "🏪";
const char* EMOJI_INDUSTRIAL = "🏭";
const char* EMOJI_ROAD = "🛣️";
const char* EMOJI_POWER_PLANT = "⚡";
const char* EMOJI_POLICE = "👮";
const char* EMOJI_FIRE = "🚒";
const char* EMOJI_PARK = "🌳";
const char* EMOJI_WATER = "🌊";

// Tile types
#define TILE_EMPTY       0
#define TILE_RES_LOW     1
#define TILE_RES_HIGH    2
#define TILE_COMMERCIAL  3
#define TILE_INDUSTRIAL  4
#define TILE_ROAD        5
#define TILE_POWER_PLANT 6
#define TILE_POLICE      7
#define TILE_FIRE        8
#define TILE_PARK        9
#define TILE_WATER       10
#define NUM_EMOJIS       11

// Game state
int year = 1900;
int month = 1;
int budget = 10000;
int population = 0;
int tax_rate = 7;  // Percent

int tiles[GRID_ROWS][GRID_COLS];
bool powered[GRID_ROWS][GRID_COLS] = {0};

int cursor_row = 10, cursor_col = 10;
int current_tool = TILE_ROAD;  // Default tool: Road

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.05f, 0.05f, 0.1f, 1.0f};

char status_message[256] = "Welcome to SimCity Emoji! Use arrows to move, 1-9 for tools, Enter to place.";

// --- FreeType & Rendering ---
FT_Library ft;
FT_Face emoji_face;
Display *x_display = NULL;
Window x_window;

// --- Cache for emoji textures ---
struct EmojiCacheEntry {
    unsigned int codepoint;
    GLuint texture;
    int width;
    int height;
    bool valid;
};
struct EmojiCacheEntry emoji_texture_cache[NUM_EMOJIS];
int cache_size = 0;

// --- Menu ---
#define MENU_HEIGHT 20
int window_width = GRID_COLS * TILE_SIZE + 20;
int window_height = GRID_ROWS * TILE_SIZE + 100;  // Menu + UI
#define MENU_Y (window_height - MENU_HEIGHT)

// --- Tools Menu ---
bool show_tools_menu = false;
const char* tool_options[10] = {
    "Bulldoze (0)",
    "Residential (1)",
    "Commercial (2)",
    "Industrial (3)",
    "Road (4)",
    "Power Plant (5)",
    "Police (6)",
    "Fire (7)",
    "Park (8)",
    "Query (9)"
};

// --- Debug Toggles ---
bool debug_power_enabled = true;
bool debug_show_cursor = true;

// --- Function Prototypes ---
void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int xx, int yy);
void special(int key, int x, int y);
void init();
void idle();
void draw_rect(float x, float y, float w, float h, float color[3], float alpha);
void generate_world();
bool is_valid_tile(int r, int c);
void advance_month();
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void set_status_message(const char* format, ...);
void place_tile(int row, int col, int type);
void propagate_power();
void simulate_growth();
void open_tools_menu();
void select_tool(int choice);
void query_tile(int row, int col);
void cleanup_cache();

// --- UTF-8 Decoder ---
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0 && (str[1] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    }
    if ((str[0] & 0xF0) == 0xE0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    }
    if ((str[0] & 0xF8) == 0xF0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }
    *codepoint = 0xFFFD; // Replacement character
    return 1;
}

// --- FreeType Init (Original Path) ---
void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        exit(1);
    }

    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error: Could not load emoji font at %s, error code: %d\n", emoji_font_path, err);
        exit(1);
    }

    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, TILE_SIZE - 10);
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
    } else {
        fprintf(stderr, "Error: No usable size in emoji font\n");
        exit(1);
    }

    if (err) {
        fprintf(stderr, "Error: Could not set emoji font size, error code: %d\n", err);
        exit(1);
    }

    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)(TILE_SIZE * 0.8f) / (float)loaded_emoji_size;
    fprintf(stderr, "Emoji font loaded. Size: %d, Scale: %.2f\n", loaded_emoji_size, emoji_scale);
}

// --- Emoji Rendering with Caching ---
void render_emoji(unsigned int codepoint, float x, float y) {
    struct EmojiCacheEntry *entry = NULL;
    for (int i = 0; i < cache_size; i++) {
        if (emoji_texture_cache[i].codepoint == codepoint && emoji_texture_cache[i].valid) {
            entry = &emoji_texture_cache[i];
            break;
        }
    }

    if (!entry) {
        if (cache_size >= NUM_EMOJIS) {
            fprintf(stderr, "Emoji cache full for codepoint %u\n", codepoint);
            return;
        }

        FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
        if (err) {
            fprintf(stderr, "Failed to load glyph for codepoint %u, error: %d\n", codepoint, err);
            return;
        }

        FT_GlyphSlot slot = emoji_face->glyph;
        if (!slot->bitmap.buffer || slot->bitmap.width == 0 || slot->bitmap.rows == 0) {
            fprintf(stderr, "No valid bitmap for codepoint %u\n", codepoint);
            return;
        }

        if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
            fprintf(stderr, "Unsupported pixel mode %d for codepoint %u\n", slot->bitmap.pixel_mode, codepoint);
            return;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glBindTexture(GL_TEXTURE_2D, 0);

        entry = &emoji_texture_cache[cache_size++];
        entry->codepoint = codepoint;
        entry->texture = texture;
        entry->width = slot->bitmap.width;
        entry->height = slot->bitmap.rows;
        entry->valid = true;
    }

    // Clean state
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, entry->texture);

    float w = entry->width * emoji_scale;
    float h = entry->height * emoji_scale;
    float x2 = x - w / 2;
    float y2 = y - h / 2;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x2, y2);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x2, y2 + h);
    glEnd();

    // Cleanup
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

// --- Text Rendering ---
void render_text(const char* str, float x, float y) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
}

// --- World Generation ---
void generate_world() {
    srand(time(NULL));
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (rand() % 100 < 10) {
                tiles[r][c] = TILE_WATER;
            } else {
                tiles[r][c] = TILE_EMPTY;
            }
            powered[r][c] = false;
        }
    }
    set_status_message("City founded in %d. Budget: %d. Population: %d", year, budget, population);
}

// --- Validation ---
bool is_valid_tile(int r, int c) {
    return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS;
}

// --- Simulation ---
void advance_month() {
    month++;
    if (month > 12) {
        month = 1;
        year++;
    }

    int income = (population * tax_rate) / 100;
    budget += income;
    budget -= 100;  // Maintenance

    if (budget < 0) {
        set_status_message("Bankrupt! Game over.");
        return;
    }

    propagate_power();
    simulate_growth();
    set_status_message("%d-%02d | Budget: %d | Pop: %d", year, month, budget, population);
    glutPostRedisplay();
}

void propagate_power() {
    memset(powered, 0, sizeof(powered));
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (tiles[r][c] == TILE_POWER_PLANT) {
                for (int dr = -5; dr <= 5; dr++) {
                    for (int dc = -5; dc <= 5; dc++) {
                        int nr = r + dr, nc = c + dc;
                        if (is_valid_tile(nr, nc) && abs(dr) + abs(dc) <= 5) {
                            powered[nr][nc] = true;
                        }
                    }
                }
            }
        }
    }
}

void simulate_growth() {
    int new_pop = 0;
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (tiles[r][c] == TILE_RES_LOW && powered[r][c]) {
                bool has_road = false;
                if (is_valid_tile(r-1, c) && tiles[r-1][c] == TILE_ROAD) has_road = true;
                if (is_valid_tile(r+1, c) && tiles[r+1][c] == TILE_ROAD) has_road = true;
                if (is_valid_tile(r, c-1) && tiles[r][c-1] == TILE_ROAD) has_road = true;
                if (is_valid_tile(r, c+1) && tiles[r][c+1] == TILE_ROAD) has_road = true;

                if (has_road && rand() % 100 < 20) {
                    tiles[r][c] = TILE_RES_HIGH;
                    new_pop += 10;
                } else if (has_road) {
                    new_pop += 5;
                }
            } else if (tiles[r][c] == TILE_RES_HIGH) {
                new_pop += 20;
            } else if (tiles[r][c] == TILE_COMMERCIAL && powered[r][c]) {
                new_pop += 5;
            } else if (tiles[r][c] == TILE_INDUSTRIAL && powered[r][c]) {
                new_pop += 10;
            }
        }
    }
    population += new_pop / 10;
}

// --- Placement ---
void place_tile(int row, int col, int type) {
    if (!is_valid_tile(row, col)) return;
    if (tiles[row][col] == TILE_WATER && type != 0) {
        set_status_message("Can't build on water!");
        return;
    }

    int cost = 0;
    switch (type) {
        case 0: cost = 10; break;
        case TILE_RES_LOW: cost = 100; break;
        case TILE_COMMERCIAL: cost = 100; break;
        case TILE_INDUSTRIAL: cost = 100; break;
        case TILE_ROAD: cost = 50; break;
        case TILE_POWER_PLANT: cost = 3000; break;
        case TILE_POLICE: cost = 500; break;
        case TILE_FIRE: cost = 500; break;
        case TILE_PARK: cost = 200; break;
    }

    if (budget < cost) {
        set_status_message("Not enough budget! Need %d", cost);
        return;
    }

    budget -= cost;

    if (type == 0) {
        if (tiles[row][col] != TILE_EMPTY && tiles[row][col] != TILE_WATER) {
            tiles[row][col] = TILE_EMPTY;
            set_status_message("Bulldozed tile (%d,%d)", row, col);
        }
    } else {
        tiles[row][col] = type;
        set_status_message("Placed %s at (%d,%d)", tool_options[type], row, col);
    }

    propagate_power();
    glutPostRedisplay();
}

// --- Query ---
void query_tile(int row, int col) {
    if (!is_valid_tile(row, col)) return;
    const char* type_str = tool_options[tiles[row][col]];
    bool is_powered = powered[row][col];
    set_status_message("Tile (%d,%d): %s | Powered: %s", row, col, type_str, is_powered ? "Yes" : "No");
}
// --- Mouse Input (Fixed Y-Axis) ---
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (y >= MENU_Y && y < window_height) {
            if (x >= 10 && x <= 60) {
                set_status_message("File > Save (not implemented)");
            } else if (x >= 80 && x <= 140) {
                open_tools_menu();
            }
            glutPostRedisplay();
            return;
        }

        // ✅ Corrected Y mapping
        int col = (x - 10) / TILE_SIZE;
        int row = (y - 80) / TILE_SIZE;           // Subtract UI panel height
        row = GRID_ROWS - 1 - row;                // Flip Y: top of screen = row 0

        if (!is_valid_tile(row, col)) return;

        cursor_row = row;
        cursor_col = col;

        if (current_tool == 9) {
            query_tile(row, col);
        } else {
            place_tile(row, col, current_tool);
        }
        glutPostRedisplay();
    }
}

// --- Keyboard Input (Fixed Space + Escape/T Toggle) ---
void keyboard(unsigned char key, int xx, int yy) {
    // Handle global keys first
    switch (key) {
        case ' ':  // Advance month anytime
            advance_month();
            return;

        case 't':
        case 'T':
        case 27:  // Escape
            show_tools_menu = !show_tools_menu;
            set_status_message(show_tools_menu ? "Select tool 0-9." : "Tools menu closed.");
            glutPostRedisplay();
            return;
    }

    // If menu is open, only allow digit input
    if (show_tools_menu) {
        if (key >= '0' && key <= '9') {
            select_tool(key - '0');
        }
        return;
    }

    // Normal gameplay
    switch (key) {
        case 13:  // Enter
            if (current_tool == 9) {
                query_tile(cursor_row, cursor_col);
            } else {
                place_tile(cursor_row, cursor_col, current_tool);
            }
            break;

        case 'p': case 'P':
            debug_power_enabled = !debug_power_enabled;
            set_status_message("Power overlay: %s", debug_power_enabled ? "ON" : "OFF");
            break;

        case 'c': case 'C':
            debug_show_cursor = !debug_show_cursor;
            set_status_message("Cursor: %s", debug_show_cursor ? "ON" : "OFF");
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            select_tool(key - '0');
            break;
    }
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_UP:    if (cursor_row > 0) cursor_row--; break;
        case GLUT_KEY_DOWN:  if (cursor_row < GRID_ROWS - 1) cursor_row++; break;
        case GLUT_KEY_LEFT:  if (cursor_col > 0) cursor_col--; break;
        case GLUT_KEY_RIGHT: if (cursor_col < GRID_COLS - 1) cursor_col++; break;
    }
    glutPostRedisplay();
}

void open_tools_menu() {
    show_tools_menu = true;
    set_status_message("Select tool 0-9.");
    glutPostRedisplay();
}

void select_tool(int choice) {
    if (choice >= 0 && choice <= 9) {
        current_tool = (choice == 0) ? 0 : choice;
        show_tools_menu = false;
        set_status_message("Tool selected: %s", tool_options[choice]);
    }
    glutPostRedisplay();
}

// --- Drawing ---
void draw_rect(float x, float y, float w, float h, float color[3], float alpha) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor4f(color[0], color[1], color[2], alpha);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    float tile_area_top = window_height - MENU_HEIGHT - 80;

    // === TOP MENU BAR ===
    float menu_bg[3] = {0.2f, 0.2f, 0.4f};
    draw_rect(0, window_height - MENU_HEIGHT, window_width, MENU_HEIGHT, menu_bg, 1.0f);
    render_text(" File ", 10, window_height - MENU_HEIGHT + 4);
    render_text(" Tools (T) ", 80, window_height - MENU_HEIGHT + 4);

    char power_text[20];
    snprintf(power_text, sizeof(power_text), "Power: %s", debug_power_enabled ? "ON" : "OFF");
    render_text(power_text, 160, window_height - MENU_HEIGHT + 4);

    char cursor_text[20];
    snprintf(cursor_text, sizeof(cursor_text), "Cursor: %s", debug_show_cursor ? "ON" : "OFF");
    render_text(cursor_text, 240, window_height - MENU_HEIGHT + 4);

    // === GAME GRID ===
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = 10 + c * TILE_SIZE;
            float y = tile_area_top - r * TILE_SIZE;
            float cx = x + TILE_SIZE / 2;
            float cy = y + TILE_SIZE / 2;

            const char* emoji;
            float color[3] = {0.3f, 0.3f, 0.3f};
            switch (tiles[r][c]) {
                case TILE_EMPTY:      emoji = EMOJI_EMPTY; color[0]=0.2f; color[1]=0.6f; color[2]=0.2f; break;
                case TILE_RES_LOW:    emoji = EMOJI_RESIDENTIAL_LOW; color[0]=0.4f; color[1]=0.8f; color[2]=0.4f; break;
                case TILE_RES_HIGH:   emoji = EMOJI_RESIDENTIAL_HIGH; color[0]=0.6f; color[1]=0.9f; color[2]=0.6f; break;
                case TILE_COMMERCIAL: emoji = EMOJI_COMMERCIAL; color[0]=0.4f; color[1]=0.4f; color[2]=0.8f; break;
                case TILE_INDUSTRIAL: emoji = EMOJI_INDUSTRIAL; color[0]=0.8f; color[1]=0.4f; color[2]=0.4f; break;
                case TILE_ROAD:       emoji = EMOJI_ROAD; color[0]=0.5f; color[1]=0.5f; color[2]=0.5f; break;
                case TILE_POWER_PLANT:emoji = EMOJI_POWER_PLANT; color[0]=1.0f; color[1]=1.0f; color[2]=0.0f; break;
                case TILE_POLICE:     emoji = EMOJI_POLICE; color[0]=0.0f; color[1]=0.0f; color[2]=1.0f; break;
                case TILE_FIRE:       emoji = EMOJI_FIRE; color[0]=1.0f; color[1]=0.0f; color[2]=0.0f; break;
                case TILE_PARK:       emoji = EMOJI_PARK; color[0]=0.0f; color[1]=0.8f; color[2]=0.0f; break;
                case TILE_WATER:      emoji = EMOJI_WATER; color[0]=0.0f; color[1]=0.3f; color[2]=0.7f; break;
                default: emoji = EMOJI_EMPTY;
            }

            draw_rect(x, y, TILE_SIZE, TILE_SIZE, color, 0.8f);

            if (debug_power_enabled && !powered[r][c] && tiles[r][c] != TILE_EMPTY && tiles[r][c] != TILE_WATER) {
                draw_rect(x, y, TILE_SIZE, TILE_SIZE, (float[]){0.0f, 0.0f, 0.0f}, 0.5f);
            }

            unsigned int code;
            decode_utf8((const unsigned char*)emoji, &code);
            render_emoji(code, cx, cy);
        }
    }

    // Cursor
    if (debug_show_cursor && is_valid_tile(cursor_row, cursor_col)) {
        float x = 10 + cursor_col * TILE_SIZE;
        float y = tile_area_top - cursor_row * TILE_SIZE;
        float sel[3] = {1.0f, 1.0f, 0.0f};
        glLineWidth(4.0f);
        glColor3fv(sel);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + TILE_SIZE, y);
        glVertex2f(x + TILE_SIZE, y + TILE_SIZE);
        glVertex2f(x, y + TILE_SIZE);
        glEnd();
    }

    // Tools Menu
    if (show_tools_menu) {
        float mx = window_width / 2 - 150;
        float my = window_height / 2;
        draw_rect(mx, my, 300, 200, (float[]){0.1f, 0.1f, 0.3f}, 0.9f);
        render_text("Select Tool (0-9):", mx + 10, my + 170);
        for (int i = 0; i < 10; i++) {
            render_text(tool_options[i], mx + 10, my + 140 - i * 20);
        }
    }

    // UI Panel (bottom)
    float panel[3] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, 80, panel, 1.0f);
    char ui[100];
    snprintf(ui, sizeof(ui), "Year: %d Month: %d | Budget: %d | Pop: %d | Tax: %d%%", year, month, budget, population, tax_rate);
    render_text(ui, 10, 60);
    render_text("Enter: Place | Space: Advance Month | T/ESC: Close Menu", 10, 30);
    render_text(status_message, 10, 10);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle() {}

void set_status_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(status_message, sizeof(status_message), format, args);
    va_end(args);
}

void cleanup_cache() {
    for (int i = 0; i < cache_size; i++) {
        if (emoji_texture_cache[i].valid) {
            glDeleteTextures(1, &emoji_texture_cache[i].texture);
        }
    }
    cache_size = 0;
}

void init() {
    initFreeType();
    generate_world();
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
    memset(emoji_texture_cache, 0, sizeof(emoji_texture_cache));
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("SimCity: Emoji Edition");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();

    printf("Controls:\n");
    printf(" - Arrows: Move cursor\n");
    printf(" - 0-9: Select tool\n");
    printf(" - Enter: Place or Query\n");
    printf(" - Space: Advance month\n");
    printf(" - T or ESC: Open/Close Tools Menu\n");
    printf(" - P/C: Toggle overlays\n");

    atexit(cleanup_cache);
    glutMainLoop();

    return 0;
}
