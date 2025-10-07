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
#define GRID_ROWS 12
#define GRID_COLS 12
#define TILE_SIZE 64

// Emojis
const char* EMOJI_OCEAN = "🌊";
const char* EMOJI_LAND = "🟩";
const char* EMOJI_FOREST = "🌲";
const char* EMOJI_MOUNTAIN = "⛰️";
const char* EMOJI_RIVER = "💧";
const char* EMOJI_FARMER = "👨‍🌾";
const char* EMOJI_HOUSE = "🏠";
const char* EMOJI_PLANTED = "🌱";
const char* EMOJI_SPROUT = "🌿";
const char* EMOJI_MATURE = "🌾";
const char* EMOJI_FOG = "🌫️";

// Terrain types
#define TERRAIN_OCEAN    0
#define TERRAIN_GRASS    1
#define TERRAIN_FOREST   2
#define TERRAIN_MOUNTAIN 3
#define TERRAIN_RIVER    4

// Game state
int day = 1;
int money = 0;
int seeds = 10;
int produce = 0;

int farmer_row = -1, farmer_col = -1;
int house_row = -1, house_col = -1;

int crops[GRID_ROWS][GRID_COLS] = {0};
bool watered[GRID_ROWS][GRID_COLS] = {0};

int window_width = GRID_COLS * TILE_SIZE + 20;
int window_height = GRID_ROWS * TILE_SIZE + 100;  // Menu + UI

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.9f, 0.9f, 1.0f, 1.0f};  // Default day

char status_message[256] = "Welcome! Use Enter to select and act.";

// --- FreeType & Rendering ---
FT_Library ft;
FT_Face emoji_face;
Display *x_display = NULL;
Window x_window;

// --- Map & Fog ---
int terrain[GRID_ROWS][GRID_COLS];
bool explored[GRID_ROWS][GRID_COLS] = {0};
int cursor_row = 6, cursor_col = 6;
bool show_cursor = true;

// --- Debug Toggles ---
bool debug_fog_enabled = true;
bool debug_terrain_enabled = true;
bool debug_show_cursor = true;

// --- Selection State ---
bool farmer_selected = false;

#define MENU_HEIGHT 20
#define MENU_Y (window_height - MENU_HEIGHT)

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
void draw_rect(float x, float y, float w, float h, float color[3]);
void generate_world();
bool is_valid_tile(int r, int c);
bool is_land(int r, int c);
void reveal_around(int r, int c, int radius);
void end_day();
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void set_status_message(const char* format, ...);
void move_farmer(int row, int col);

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
    *codepoint = 0xFFFD;
    return 1;
}

// --- FreeType Init ---
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

// --- Emoji Rendering ---
void render_emoji(unsigned int codepoint, float x, float y) {
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err || !emoji_face->glyph->bitmap.buffer) return;

    FT_GlyphSlot slot = emoji_face->glyph;
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) return;

    GLboolean was_blend, was_tex;
    glGetBooleanv(GL_BLEND, &was_blend);
    glGetBooleanv(GL_TEXTURE_2D, &was_tex);
    GLfloat color[4];
    glGetFloatv(GL_CURRENT_COLOR, color);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    float w = slot->bitmap.width * emoji_scale;
    float h = slot->bitmap.rows * emoji_scale;
    float x2 = x - w / 2;
    float y2 = y - h / 2;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x2, y2);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x2, y2 + h);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
    if (!was_blend) glDisable(GL_BLEND);
    if (!was_tex) glDisable(GL_TEXTURE_2D);
    glColor4fv(color);
}

// --- Text Rendering ---
void render_text(const char* str, float x, float y) {
    GLfloat current_color[4];
    glGetFloatv(GL_CURRENT_COLOR, current_color);
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
    glColor4fv(current_color);
}

// --- World Generation ---
void generate_world() {
    srand(time(NULL));
    int center_r = GRID_ROWS / 2;
    int center_c = GRID_COLS / 2;
    float radius_sq = (GRID_ROWS / 3.0f) * (GRID_COLS / 3.0f) * 2.5f;

    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float dist = (r - center_r) * (r - center_r) + (c - center_c) * (c - center_c);
            if (dist < radius_sq && rand() % 100 < 70) {
                int roll = rand() % 100;
                if (roll < 15) terrain[r][c] = TERRAIN_MOUNTAIN;
                else if (roll < 35) terrain[r][c] = TERRAIN_FOREST;
                else if (roll < 40) terrain[r][c] = TERRAIN_RIVER;
                else terrain[r][c] = TERRAIN_GRASS;
            } else {
                terrain[r][c] = TERRAIN_OCEAN;
            }
            explored[r][c] = false;
        }
    }

    // Find safe spawn
    bool found = false;
    for (int dr = -2; dr <= 2 && !found; dr++) {
        for (int dc = -2; dc <= 2 && !found; dc++) {
            int r = center_r + dr;
            int c = center_c + dc;
            if (!is_valid_tile(r, c)) continue;
            if (terrain[r][c] != TERRAIN_GRASS) continue;

            int free_neighbors = 0;
            if (is_valid_tile(r-1,c) && terrain[r-1][c] != TERRAIN_MOUNTAIN) free_neighbors++;
            if (is_valid_tile(r+1,c) && terrain[r+1][c] != TERRAIN_MOUNTAIN) free_neighbors++;
            if (is_valid_tile(r,c-1) && terrain[r][c-1] != TERRAIN_MOUNTAIN) free_neighbors++;
            if (is_valid_tile(r,c+1) && terrain[r][c+1] != TERRAIN_MOUNTAIN) free_neighbors++;

            if (free_neighbors >= 2) {
                farmer_row = r;
                farmer_col = c;
                house_row = r;
                house_col = c + 1;
                if (!is_valid_tile(house_row, house_col) || terrain[house_row][house_col] != TERRAIN_GRASS) {
                    house_col = c - 1;
                }
                if (!is_valid_tile(house_row, house_col) || terrain[house_row][house_col] != TERRAIN_GRASS) {
                    house_row = r + 1;
                    house_col = c;
                }
                terrain[house_row][house_col] = TERRAIN_GRASS;
                reveal_around(r, c, 5);
                cursor_row = r;
                cursor_col = c;
                set_status_message("Farmer ready! Use Enter to select.");
                found = true;
            }
        }
    }

    if (!found) {
        farmer_row = center_r;
        farmer_col = center_c;
        house_row = center_r;
        house_col = center_c + 1;
        terrain[farmer_row][farmer_col] = TERRAIN_GRASS;
        terrain[house_row][house_col] = TERRAIN_GRASS;
        reveal_around(farmer_row, farmer_col, 5);
        set_status_message("Fallback spawn. Ready.");
    }
}

void reveal_around(int r, int c, int radius) {
    for (int dr = -radius; dr <= radius; dr++) {
        for (int dc = -radius; dc <= radius; dc++) {
            int nr = r + dr, nc = c + dc;
            if (is_valid_tile(nr, nc)) {
                explored[nr][nc] = true;
            }
        }
    }
}

bool is_valid_tile(int r, int c) {
    return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS;
}
bool is_land(int r, int c) {
    return is_valid_tile(r, c) && terrain[r][c] != TERRAIN_OCEAN;
}

// --- Game Logic ---
void end_day() {
    day++;
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (crops[r][c] > 0 && crops[r][c] < 3 && watered[r][c]) {
                crops[r][c]++;
            }
            watered[r][c] = false;
        }
    }
    farmer_selected = false;
    set_status_message("Turn %d. Money: %d. Seeds: %d. Produce: %d", day, money, seeds, produce);
    glutPostRedisplay();
}

void move_farmer(int row, int col) {
    if (!is_valid_tile(row, col)) return;
    if (terrain[row][col] == TERRAIN_OCEAN || terrain[row][col] == TERRAIN_MOUNTAIN) {
        set_status_message("Can't move to that terrain!");
        return;
    }

    farmer_row = row;
    farmer_col = col;
    reveal_around(row, col, 1);
    set_status_message("✅ Farmer moved to (%d,%d).", row, col);
    farmer_selected = false;  // Reset after move
    glutPostRedisplay();
}

// --- Input: Mouse ---
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Menu clicks
        if (y >= MENU_Y && y < window_height) {
            if (x >= 10 && x <= 60) {
                set_status_message("File > Save (not implemented)");
            } else if (x >= 80 && x <= 140) {
                set_status_message("Debug menu open.");
            } else if (x >= 160 && x <= 220) {
                debug_fog_enabled = !debug_fog_enabled;
                set_status_message("Fog: %s", debug_fog_enabled ? "ON" : "OFF");
            } else if (x >= 240 && x <= 320) {
                debug_terrain_enabled = !debug_terrain_enabled;
                set_status_message("Terrain: %s", debug_terrain_enabled ? "ON" : "OFF");
            } else if (x >= 340 && x <= 440) {
                debug_show_cursor = !debug_show_cursor;
                set_status_message("Cursor: %s", debug_show_cursor ? "SHOWING" : "HIDDEN");
            }
            glutPostRedisplay();
            return;
        }

        // Grid click
        int col = (x - 10) / TILE_SIZE;
        float grid_y = window_height - y;
        float tile_area_top = MENU_Y - 80;

        if (grid_y < 80 || grid_y > tile_area_top) return;

        int row = (tile_area_top - grid_y) / TILE_SIZE;

        if (!is_valid_tile(row, col)) return;

        cursor_row = row;
        cursor_col = col;

        if (row == farmer_row && col == farmer_col) {
            set_status_message("👨‍🌾 Farmer selected. Move and press Enter to act.");
            farmer_selected = true;
        } else if (farmer_selected) {
            int dr = abs(row - farmer_row);
            int dc = abs(col - farmer_col);
            if (dr + dc == 1) {
                move_farmer(row, col);
            } else {
                set_status_message("❌ Not adjacent!");
            }
        } else {
            set_status_message("Move cursor to farmer and press Enter to select.");
        }
        glutPostRedisplay();
    }
}

// --- Input: Keyboard ---
void keyboard(unsigned char key, int x, int y) {
    if (farmer_selected) {
        int dr = abs(cursor_row - farmer_row);
        int dc = abs(cursor_col - farmer_col);
        bool adjacent = (dr + dc <= 1);

        switch (key) {
            case 13:  // Enter key
                if (adjacent && is_land(cursor_row, cursor_col)) {
                    move_farmer(cursor_row, cursor_col);
                } else if (cursor_row == farmer_row && cursor_col == farmer_col) {
                    set_status_message("Farmer already here. Move and press Enter.");
                } else {
                    set_status_message("❌ Can't move there. Must be adjacent and valid.");
                }
                break;

            case 'p': case 'P':
                if (adjacent && terrain[cursor_row][cursor_col] == TERRAIN_GRASS && 
                    crops[cursor_row][cursor_col] == 0 && seeds > 0 &&
                    !(cursor_row == house_row && cursor_col == house_col)) {
                    crops[cursor_row][cursor_col] = 1;
                    seeds--;
                    set_status_message("🌱 Planted a seed!");
                } else {
                    set_status_message("Can't plant here or no seeds.");
                }
                break;

            case 'w': case 'W':
                if (adjacent && crops[cursor_row][cursor_col] > 0 && !watered[cursor_row][cursor_col]) {
                    watered[cursor_row][cursor_col] = true;
                    set_status_message("💧 Watered the crop!");
                } else {
                    set_status_message("Can't water here.");
                }
                break;

            case 'h': case 'H':
                if (adjacent && crops[cursor_row][cursor_col] >= 3) {
                    produce++;
                    money += 10;
                    crops[cursor_row][cursor_col] = 0;
                    watered[cursor_row][cursor_col] = false;
                    set_status_message("🌾 Harvested! +1 produce, +10 money.");
                } else {
                    set_status_message("Not ready to harvest.");
                }
                break;

            case 's': case 'S':
                if (farmer_row == house_row && farmer_col == house_col) {
                    end_day();
                } else {
                    set_status_message("Must be at house to sleep.");
                }
                break;
        }
    } else {
        switch (key) {
            case 13:  // Enter key
                if (cursor_row == farmer_row && cursor_col == farmer_col) {
                    farmer_selected = true;
                    set_status_message("✅ Farmer selected. Move cursor and press Enter to move, or P/W/H/S to act.");
                } else {
                    set_status_message("Go to farmer and press Enter to select.");
                }
                break;
        }
    }

    switch (key) {
        case 'g':
            debug_fog_enabled = !debug_fog_enabled;
            set_status_message("Fog: %s", debug_fog_enabled ? "ON" : "OFF");
            break;
        case 't':
            debug_terrain_enabled = !debug_terrain_enabled;
            set_status_message("Terrain: %s", debug_terrain_enabled ? "ON" : "OFF");
            break;
        case 'k':
            debug_show_cursor = !debug_show_cursor;
            set_status_message("Cursor: %s", debug_show_cursor ? "ON" : "OFF");
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

// --- Drawing ---
void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void display() {
    // Day-night cycle: even days = day, odd = night
    if (day % 2 == 0) {
        background_color[0] = 0.9f; background_color[1] = 0.9f; background_color[2] = 1.0f;
    } else {
        background_color[0] = 0.1f; background_color[1] = 0.1f; background_color[2] = 0.2f;
    }
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float fog_color[3] = {0.05f, 0.05f, 0.15f};
    float tile_area_top = MENU_Y - 80;

    // === TOP MENU BAR ===
    float menu_bg[3] = {0.2f, 0.2f, 0.4f};
    draw_rect(0, MENU_Y, window_width, MENU_HEIGHT, menu_bg);

    render_text(" File ", 10, MENU_Y + 4);
    render_text(" Debug ", 80, MENU_Y + 4);

    char fog_text[20];
    snprintf(fog_text, sizeof(fog_text), "Fog: %s", debug_fog_enabled ? "ON" : "OFF");
    render_text(fog_text, 160, MENU_Y + 4);

    char terrain_text[20];
    snprintf(terrain_text, sizeof(terrain_text), "Terrain: %s", debug_terrain_enabled ? "ON" : "OFF");
    render_text(terrain_text, 240, MENU_Y + 4);

    char cursor_text[20];
    snprintf(cursor_text, sizeof(cursor_text), "Cursor: %s", debug_show_cursor ? "ON" : "OFF");
    render_text(cursor_text, 340, MENU_Y + 4);

    // === GAME GRID ===
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = 10 + c * TILE_SIZE;
            float y = tile_area_top - (r + 1) * TILE_SIZE;
            float cx = x + TILE_SIZE / 2;
            float cy = y + TILE_SIZE / 2;

            bool has_farmer = (farmer_row == r && farmer_col == c);
            bool has_house = (house_row == r && house_col == c);

            if (debug_fog_enabled && !explored[r][c]) {
                draw_rect(x, y, TILE_SIZE, TILE_SIZE, fog_color);
                if (has_farmer || has_house) {
                    const char* emoji = has_house ? EMOJI_HOUSE : EMOJI_FARMER;
                    unsigned int code;
                    decode_utf8((const unsigned char*)emoji, &code);
                    render_emoji(code, cx, cy);
                } else {
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_FOG, &code);
                    render_emoji(code, cx, cy);
                }
            } else {
                if (debug_terrain_enabled) {
                    float color[3];
                    const char* base_emoji = EMOJI_LAND;
                    switch (terrain[r][c]) {
                        case TERRAIN_GRASS:   color[0]=0.2f; color[1]=0.6f; color[2]=0.2f; base_emoji = EMOJI_LAND; break;
                        case TERRAIN_FOREST:  color[0]=0.1f; color[1]=0.5f; color[2]=0.1f; base_emoji = EMOJI_FOREST; break;
                        case TERRAIN_MOUNTAIN:color[0]=0.5f; color[1]=0.5f; color[2]=0.5f; base_emoji = EMOJI_MOUNTAIN; break;
                        case TERRAIN_RIVER:   color[0]=0.2f; color[1]=0.4f; color[2]=0.8f; base_emoji = EMOJI_RIVER; break;
                        case TERRAIN_OCEAN:   color[0]=0.0f; color[1]=0.3f; color[2]=0.7f; base_emoji = EMOJI_OCEAN; break;
                    }
                    draw_rect(x, y, TILE_SIZE, TILE_SIZE, color);
                    unsigned int code;
                    decode_utf8((const unsigned char*)base_emoji, &code);
                    render_emoji(code, cx, cy);
                } else {
                    draw_rect(x, y, TILE_SIZE, TILE_SIZE, (float[]){0.3f, 0.3f, 0.3f});
                }

                // Render crop if present
                if (crops[r][c] > 0) {
                    const char* crop_emoji;
                    if (crops[r][c] == 1) crop_emoji = EMOJI_PLANTED;
                    else if (crops[r][c] == 2) crop_emoji = EMOJI_SPROUT;
                    else crop_emoji = EMOJI_MATURE;
                    unsigned int code;
                    decode_utf8((const unsigned char*)crop_emoji, &code);
                    render_emoji(code, cx, cy);
                }

                // Render house or farmer on top
                if (has_house) {
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_HOUSE, &code);
                    render_emoji(code, cx, cy);
                } else if (has_farmer) {
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_FARMER, &code);
                    render_emoji(code, cx, cy);
                }
            }
        }
    }

    // Cursor
    if (debug_show_cursor && is_valid_tile(cursor_row, cursor_col)) {
        float x = 10 + cursor_col * TILE_SIZE;
        float y = tile_area_top - (cursor_row + 1) * TILE_SIZE;
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

    // UI Panel (bottom)
    float panel[3] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, 80, panel);
    char ui[100];
    snprintf(ui, sizeof(ui), "Day: %d | Money: %d | Seeds: %d | Produce: %d", day, money, seeds, produce);
    render_text(ui, 10, 60);
    render_text("P: Plant | W: Water | H: Harvest | S: Sleep (at house)", 10, 30);
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
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    strncpy(status_message, buffer, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

void init() {
    initFreeType();
    generate_world();
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Farming: Emoji Edition");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();

    printf("Controls:\n");
    printf(" - Enter: Select farmer, then move & Enter to move\n");
    printf(" - P: Plant (adjacent grass) | W: Water (adjacent crop) | H: Harvest (mature crop)\n");
    printf(" - S: Sleep (at house to advance day)\n");
    printf(" - Arrow keys: Move cursor\n");
    printf(" - G/T/K: Debug toggles\n");

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    return 0;
}
