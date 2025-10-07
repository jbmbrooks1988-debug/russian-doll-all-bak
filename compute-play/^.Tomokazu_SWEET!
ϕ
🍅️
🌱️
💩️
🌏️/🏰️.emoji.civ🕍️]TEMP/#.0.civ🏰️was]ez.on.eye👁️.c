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

// --- Constants ---
#define GRID_ROWS 12
#define GRID_COLS 12
#define TILE_SIZE 64
#define MATCH_MIN 3

const int MAP[GRID_ROWS][GRID_COLS]; // Forward declare for now

// Emoji glyphs
const char* EMOJI_OCEAN = "🌊";
const char* EMOJI_LAND = "🟩";
const char* EMOJI_SETTLER = "👤";
const char* EMOJI_CITY = "🏙️";
const char* EMOJI_GOLD = "💰";

// Game state
int turn = 1;
int gold = 0;
int city_count = 0;

int settler_row = -1, settler_col = -1;
bool has_settler = true;

bool city_map[GRID_ROWS][GRID_COLS] = {0};

int window_width = GRID_COLS * TILE_SIZE + 20;
int window_height = GRID_ROWS * TILE_SIZE + 80;  // Extra for UI

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.05f, 0.05f, 0.1f, 1.0f};

char status_message[256] = "Select a land tile to move or found a city.";

// --- FreeType & Rendering Globals ---
FT_Library ft;
FT_Face emoji_face;
Display *x_display = NULL;
Window x_window;

// --- Function Prototypes ---
void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int xx, int yy);
void init();
void idle();
void draw_rect(float x, float y, float w, float h, float color[3]);
void generate_island_map();
bool is_valid_tile(int r, int c);
bool is_ocean(int r, int c);
bool is_land(int r, int c);
void end_turn();
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void set_status_message(const char* msg);

// --- Map Data (generated at runtime) ---
int map[GRID_ROWS][GRID_COLS] = {0}; // 0 = ocean, 1 = land

// --- UTF-8 Decoder ---
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

// --- FreeType Init & Emoji Render ---
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

void render_emoji(unsigned int codepoint, float x, float y) {
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err || !emoji_face->glyph->bitmap.buffer) return;

    FT_GlyphSlot slot = emoji_face->glyph;
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) return;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = slot->bitmap.width * emoji_scale;
    float h = slot->bitmap.rows * emoji_scale;
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
}

void render_text(const char* str, float x, float y) {
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
}

// --- Map Generation ---
void generate_island_map() {
    srand(time(NULL));

    // Center of map
    int center_r = GRID_ROWS / 2;
    int center_c = GRID_COLS / 2;

    // Flood-fill style island generation
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float dist = (r - center_r) * (r - center_r) + (c - center_c) * (c - center_c);
            float radius_sq = (GRID_ROWS / 3) * (GRID_COLS / 3) * 1.8f;
            if (dist < radius_sq && rand() % 100 < 85) {
                map[r][c] = 1; // land
            } else {
                map[r][c] = 0; // ocean
            }
        }
    }

    // Ensure center is land
    map[center_r][center_c] = 1;

    // Place settler
    settler_row = center_r;
    settler_col = center_c;
    has_settler = true;

    set_status_message("Game started! Move or found city.");
}

bool is_valid_tile(int r, int c) {
    return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS;
}
bool is_ocean(int r, int c) { return is_valid_tile(r, c) && map[r][c] == 0; }
bool is_land(int r, int c) { return is_valid_tile(r, c) && map[r][c] == 1; }

// --- Game Logic ---
void end_turn() {
    turn++;

    // Earn gold: 1 per city
    gold += city_count;

    // Reset settler action (if needed)
    sprintf(status_message, "Turn %d. Gold: %d. Cities: %d", turn, gold, city_count);
}

void try_move_or_found(int row, int col) {
    if (!is_land(row, col)) {
        set_status_message("Can only move to land!");
        return;
    }

    if (settler_row == row && settler_col == col) {
        set_status_message("Already here!");
        return;
    }

    // Check adjacency (1 tile move)
    int dr = abs(settler_row - row);
    int dc = abs(settler_col - col);
    if (dr + dc != 1) {
        set_status_message("Can only move one tile orthogonally!");
        return;
    }

    // If target has city already?
    if (city_map[row][col]) {
        set_status_message("Tile already has a city!");
        return;
    }

    // Option: Found city?
    printf("Prompt: Move (M) or Found City (F)?\n");
    // For now: Auto prompt via console (later: GUI popup)
    char choice = 'f'; // Simulate "found"
    if (choice == 'f' || choice == 'F') {
        // Found city!
        city_map[row][col] = true;
        city_count++;
        has_settler = false;
        set_status_message("City founded! Settler is gone.");
    } else {
        // Just move
        settler_row = row;
        settler_col = col;
        set_status_message("Settler moved.");
    }

    glutPostRedisplay();
}

// --- Input ---
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        int col = (x - 10) / TILE_SIZE;
        float gl_y = window_height - y;
        int row = (int)((window_height - 80 - gl_y) / TILE_SIZE);  // Adjust for UI height

        if (!is_valid_tile(row, col)) return;

        if (has_settler && settler_row == row && settler_col == col) {
            set_status_message("Selected settler. Click adjacent land to move or found.");
            return;
        }

        if (has_settler && is_land(row, col)) {
            int dr = abs(settler_row - row);
            int dc = abs(settler_col - col);
            if (dr + dc == 1) {
                try_move_or_found(row, col);
            } else {
                set_status_message("Not adjacent! Click adjacent tile.");
            }
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (key == ' ' || key == '\r') {
        end_turn();
        glutPostRedisplay();
    }
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

    float land_color[3] = {0.2f, 0.6f, 0.2f};
    float ocean_color[3] = {0.0f, 0.2f, 0.8f};

    // Draw tiles
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = 10 + c * TILE_SIZE;
            float y = window_height - 80 - (r + 1) * TILE_SIZE;
            if (map[r][c] == 1) {
                draw_rect(x, y, TILE_SIZE, TILE_SIZE, land_color);
            } else {
                draw_rect(x, y, TILE_SIZE, TILE_SIZE, ocean_color);
            }

            float emoji_x = x + TILE_SIZE / 2;
            float emoji_y = y + TILE_SIZE / 2;

            if (city_map[r][c]) {
                unsigned int code;
                decode_utf8((const unsigned char*)EMOJI_CITY, &code);
                render_emoji(code, emoji_x, emoji_y);
            } else if (has_settler && settler_row == r && settler_col == c) {
                unsigned int code;
                decode_utf8((const unsigned char*)EMOJI_SETTLER, &code);
                render_emoji(code, emoji_x, emoji_y);
            } else {
                const char* emoji = map[r][c] ? EMOJI_LAND : EMOJI_OCEAN;
                unsigned int code;
                decode_utf8((const unsigned char*)emoji, &code);
                render_emoji(code, emoji_x, emoji_y);
            }
        }
    }

    // UI Panel
    float panel_color[3] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, 80, panel_color);

    // Stats
    char ui_text[100];
    snprintf(ui_text, sizeof(ui_text), "Turn: %d | Gold: %d | Cities: %d", turn, gold, city_count);
    render_text(ui_text, 10, 60);

    snprintf(ui_text, sizeof(ui_text), "Press SPACE to end turn");
    render_text(ui_text, 10, 30);

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
    // Optional: animation later
}

void set_status_message(const char* msg) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

void init() {
    initFreeType();
    generate_island_map();
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
    glutIdleFunc(idle);

    init();

    printf("Controls:\n");
    printf(" - Click adjacent land to move or found city\n");
    printf(" - Press SPACE to end turn\n");

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    return 0;
}
