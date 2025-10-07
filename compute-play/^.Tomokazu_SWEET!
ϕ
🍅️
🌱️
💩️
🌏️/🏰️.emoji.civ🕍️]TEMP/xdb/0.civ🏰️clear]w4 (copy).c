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
#include <stdarg.h>  // 👈 Required for va_list in set_status_message

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
const char* EMOJI_SETTLER = "👤";
const char* EMOJI_CITY = "🏙️";
const char* EMOJI_FOG = "🌫️";

// Terrain types
#define TERRAIN_OCEAN    0
#define TERRAIN_GRASS    1
#define TERRAIN_FOREST   2
#define TERRAIN_MOUNTAIN 3
#define TERRAIN_RIVER    4

// Game state
int turn = 1;
int gold = 0;
int city_count = 0;

int settler_row = -1, settler_col = -1;
bool has_settler = true;

bool city_map[GRID_ROWS][GRID_COLS] = {0};

int window_width = GRID_COLS * TILE_SIZE + 20;
int window_height = GRID_ROWS * TILE_SIZE + 80;

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.05f, 0.05f, 0.1f, 1.0f};

char status_message[256] = "Use arrow keys. 'F' to found city. 'Space' to end turn.";

// --- FreeType & Rendering ---
FT_Library ft;
FT_Face emoji_face;
Display *x_display = NULL;
Window x_window;

// --- Map & Fog ---
int terrain[GRID_ROWS][GRID_COLS];           // terrain type
bool explored[GRID_ROWS][GRID_COLS] = {0};   // fog of war
bool show_cursor = true;
int cursor_row = 6, cursor_col = 6;

// --- Production Menu ---
bool show_production_menu = false;
int production_choice = 0;
const char* production_options[3] = {"Granary (+food)", "Walls (+defense)", "Temple (+happiness)"};

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
void end_turn();
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void set_status_message(const char* format, ...);  // ✅ Fixed: variadic
void move_settler(int row, int col);
void open_production_menu();
void finalize_city_founding(int row, int col);

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

// --- Emoji Rendering (Safe State) ---
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
            if (dist < radius_sq * 0.7f) {
                terrain[r][c] = TERRAIN_MOUNTAIN;
            } else if (dist < radius_sq && rand() % 100 < 70) {
                int roll = rand() % 100;
                if (roll < 10) terrain[r][c] = TERRAIN_MOUNTAIN;
                else if (roll < 30) terrain[r][c] = TERRAIN_FOREST;
                else if (rand() % 100 < 5) terrain[r][c] = TERRAIN_RIVER;
                else terrain[r][c] = TERRAIN_GRASS;
            } else {
                terrain[r][c] = TERRAIN_OCEAN;
            }
            explored[r][c] = false;
        }
    }

    // Place settler on grassland near center
    for (int dr = 0; dr < 3; dr++) {
        for (int dc = 0; dc < 3; dc++) {
            int r = center_r + dr - 1;
            int c = center_c + dc - 1;
            if (is_valid_tile(r, c) && terrain[r][c] == TERRAIN_GRASS) {
                settler_row = r;
                settler_col = c;
                has_settler = true;
                reveal_around(r, c, 2);
                cursor_row = r;
                cursor_col = c;
                set_status_message("Settler landed! Use arrow keys to move.");
                return;
            }
        }
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
void end_turn() {
    turn++;
    gold += city_count;
    if (has_settler && is_valid_tile(settler_row, settler_col)) {
        reveal_around(settler_row, settler_col, 1);
    }
    set_status_message("Turn %d. Gold: %d. Cities: %d", turn, gold, city_count);
    glutPostRedisplay();
}

void move_settler(int row, int col) {
    if (!is_valid_tile(row, col)) return;
    if (terrain[row][col] == TERRAIN_OCEAN) {
        set_status_message("Can't move to ocean!");
        return;
    }
    if (city_map[row][col]) {
        set_status_message("City already there!");
        return;
    }

    settler_row = row;
    settler_col = col;
    reveal_around(row, col, 1);
    set_status_message("Settler moved to (%d,%d). Press 'F' to found city.", row, col);
    glutPostRedisplay();
}

void open_production_menu() {
    show_production_menu = true;
    production_choice = 0;
    set_status_message("Choose city improvement: Use UP/DOWN, Enter to confirm.");
    glutPostRedisplay();
}

void finalize_city_founding(int row, int col) {
    city_map[row][col] = true;
    city_count++;
    has_settler = false;
    show_production_menu = false;
    set_status_message("City founded! Built: %s", production_options[production_choice]);
    glutPostRedisplay();
}

// --- Input ---
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        int col = (x - 10) / TILE_SIZE;
        float gl_y = window_height - y;
        int row = (int)((window_height - 80 - gl_y) / TILE_SIZE);

        if (is_valid_tile(row, col)) {
            cursor_row = row;
            cursor_col = col;
            glutPostRedisplay();
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (show_production_menu) {
        if (key == 13) { // Enter
            finalize_city_founding(settler_row, settler_col);
            return;
        }
        return;
    }

    switch (key) {
        case 'f':
        case 'F':
            if (has_settler && cursor_row == settler_row && cursor_col == settler_col) {
                open_production_menu();
            } else if (has_settler && is_land(cursor_row, cursor_col) && !city_map[cursor_row][cursor_col]) {
                move_settler(cursor_row, cursor_col);
            } else {
                set_status_message("Cannot found city here.");
            }
            break;
        case ' ':
            end_turn();
            break;
    }
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    if (show_production_menu) {
        if (key == GLUT_KEY_UP && production_choice > 0) production_choice--;
        if (key == GLUT_KEY_DOWN && production_choice < 2) production_choice++;
        glutPostRedisplay();
        return;
    }

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
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float fog_color[3] = {0.05f, 0.05f, 0.15f};  // Subtle dark blue

    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = 10 + c * TILE_SIZE;
            float y = window_height - 80 - (r + 1) * TILE_SIZE;
            float cx = x + TILE_SIZE / 2;
            float cy = y + TILE_SIZE / 2;

            bool has_unit_to_show = false;
            if (has_settler && settler_row == r && settler_col == c) has_unit_to_show = true;
            if (city_map[r][c]) has_unit_to_show = true;

            if (!explored[r][c]) {
                // Fog: dim tile
                draw_rect(x, y, TILE_SIZE, TILE_SIZE, fog_color);

                // But still show unit if present
                if (has_unit_to_show) {
                    const char* emoji = city_map[r][c] ? EMOJI_CITY : EMOJI_SETTLER;
                    unsigned int code;
                    decode_utf8((const unsigned char*)emoji, &code);
                    render_emoji(code, cx, cy);
                } else {
                    // Show fog emoji on empty hidden tiles
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_FOG, &code);
                    render_emoji(code, cx, cy);
                }
            } else {
                // Explored: draw terrain
                float color[3];
                const char* emoji = EMOJI_LAND;
                switch (terrain[r][c]) {
                    case TERRAIN_GRASS:   color[0]=0.2f; color[1]=0.6f; color[2]=0.2f; emoji = EMOJI_LAND; break;
                    case TERRAIN_FOREST:  color[0]=0.1f; color[1]=0.5f; color[2]=0.1f; emoji = EMOJI_FOREST; break;
                    case TERRAIN_MOUNTAIN:color[0]=0.5f; color[1]=0.5f; color[2]=0.5f; emoji = EMOJI_MOUNTAIN; break;
                    case TERRAIN_RIVER:   color[0]=0.2f; color[1]=0.4f; color[2]=0.8f; emoji = EMOJI_RIVER; break;
                    case TERRAIN_OCEAN:   color[0]=0.0f; color[1]=0.3f; color[2]=0.7f; emoji = EMOJI_OCEAN; break;
                }
                draw_rect(x, y, TILE_SIZE, TILE_SIZE, color);

                if (city_map[r][c]) {
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_CITY, &code);
                    render_emoji(code, cx, cy);
                } else if (has_settler && settler_row == r && settler_col == c) {
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_SETTLER, &code);
                    render_emoji(code, cx, cy);
                } else {
                    unsigned int code;
                    decode_utf8((const unsigned char*)emoji, &code);
                    render_emoji(code, cx, cy);
                }
            }
        }
    }

    // Cursor (only if explored)
    if (show_cursor && is_valid_tile(cursor_row, cursor_col) && explored[cursor_row][cursor_col]) {
        float x = 10 + cursor_col * TILE_SIZE;
        float y = window_height - 80 - (cursor_row + 1) * TILE_SIZE;
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

    // Production Menu
    if (show_production_menu) {
        float mx = window_width / 2 - 150;
        float my = window_height / 2;
        draw_rect(mx, my, 300, 100, (float[]){0.1f, 0.1f, 0.3f});
        render_text("Choose Improvement:", mx + 10, my + 70);
        for (int i = 0; i < 3; i++) {
            const char* prefix = (i == production_choice) ? "> " : "  ";
            char line[60];
            snprintf(line, 60, "%s%s", prefix, production_options[i]);
            render_text(line, mx + 10, my + 40 - i * 20);
        }
    }

    // UI Panel
    float panel[3] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, 80, panel);
    char ui[100];
    snprintf(ui, sizeof(ui), "Turn: %d | Gold: %d | Cities: %d", turn, gold, city_count);
    render_text(ui, 10, 60);
    render_text("F: Found City | Space: End Turn", 10, 30);
    render_text(status_message, 10, 10);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle() {
    // For animations later
}

// ✅ Correct variadic function definition
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
    glutCreateWindow("Civilization: Emoji Edition");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();

    printf("Controls:\n");
    printf(" - Arrow keys: Move cursor\n");
    printf(" - F: Found city (after moving)\n");
    printf(" - SPACE: End turn\n");
    printf(" - Mouse: Click to move cursor\n");

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    return 0;
}
