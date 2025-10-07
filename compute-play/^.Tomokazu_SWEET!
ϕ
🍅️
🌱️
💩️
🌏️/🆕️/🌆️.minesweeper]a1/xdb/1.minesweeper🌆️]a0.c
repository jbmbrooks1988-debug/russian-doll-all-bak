#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <stdarg.h>

// --- Constants ---
#define GRID_ROWS 10
#define GRID_COLS 10
#define TILE_SIZE 48
#define NUM_MINES 15

// Emojis
const char* EMOJI_HIDDEN = "🌱";
const char* EMOJI_FLAG   = "🚩";
const char* EMOJI_MINE   = "💥";
const char* EMOJI_BLANK  = "⬜";

const char* EMOJI_NUMBERS[] = {
    "0️⃣", "1️⃣", "2️⃣", "3️⃣", "4️⃣", "5️⃣", "6️⃣", "7️⃣", "8️⃣"
};

// Tile states
#define STATE_HIDDEN  0
#define STATE_REVEALED 1
#define STATE_FLAGGED  2

// Game state
int grid[GRID_ROWS][GRID_COLS];     // -1 = mine, 0-8 = neighbor count
int tile_state[GRID_ROWS][GRID_COLS];
bool game_over = false;
bool game_won = false;

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
char status_message[256] = "Left-click to reveal, Right-click to flag. Find all mines!";

// --- FreeType & Rendering ---
FT_Library ft;
FT_Face emoji_face;

struct EmojiCacheEntry {
    unsigned int codepoint;
    GLuint texture;
    int width;
    int height;
    bool valid;
};
#define NUM_EMOJI_TYPES 20
struct EmojiCacheEntry emoji_texture_cache[NUM_EMOJI_TYPES];
int cache_size = 0;

// Window size
int window_width = GRID_COLS * TILE_SIZE + 40;
int window_height = GRID_ROWS * TILE_SIZE + 100;

// --- Function Prototypes ---
void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int x, int y);
void init_game();
void place_mines(int first_r, int first_c);
int count_adjacent_mines(int r, int c);
void reveal_tile(int r, int c);
void check_win();
void set_status_message(const char* format, ...);
void cleanup_cache();

// UTF-8 Decoder
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

// Init FreeType
void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType\n");
        exit(1);
    }

    const char *font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error loading %s. Try: sudo apt install fonts-noto-color-emoji\n", font_path);
        exit(1);
    }

    FT_Set_Pixel_Sizes(emoji_face, 0, TILE_SIZE - 10);

    int loaded_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)(TILE_SIZE * 0.8f) / (float)loaded_size;
    fprintf(stderr, "Emoji font loaded. Size: %d, Scale: %.2f\n", loaded_size, emoji_scale);
}

// Render emoji with caching
void render_emoji(unsigned int codepoint, float x, float y) {
    struct EmojiCacheEntry *entry = NULL;
    for (int i = 0; i < cache_size; i++) {
        if (emoji_texture_cache[i].codepoint == codepoint && emoji_texture_cache[i].valid) {
            entry = &emoji_texture_cache[i];
            break;
        }
    }

    if (!entry) {
        if (cache_size >= NUM_EMOJI_TYPES) return;

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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        entry = &emoji_texture_cache[cache_size++];
        entry->codepoint = codepoint;
        entry->texture = texture;
        entry->width = slot->bitmap.width;
        entry->height = slot->bitmap.rows;
        entry->valid = true;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, entry->texture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    float w = entry->width * emoji_scale;
    float h = entry->height * emoji_scale;
    float x2 = x - w / 2;
    float y2 = y - h / 2;

    glBegin(GL_QUADS);
    glTexCoord2f(0,1); glVertex2f(x2,   y2);
    glTexCoord2f(1,1); glVertex2f(x2+w, y2);
    glTexCoord2f(1,0); glVertex2f(x2+w, y2+h);
    glTexCoord2f(0,0); glVertex2f(x2,   y2+h);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

// Text rendering
void render_text(const char* str, float x, float y) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
}

// Initialize game
void init_game() {
    memset(grid, 0, sizeof(grid));
    memset(tile_state, STATE_HIDDEN, sizeof(tile_state));
    game_over = false;
    game_won = false;
    set_status_message("Game started! Left-click to begin.");
}

// Place mines (avoid first click)
void place_mines(int first_r, int first_c) {
    int placed = 0;
    while (placed < NUM_MINES) {
        int r = rand() % GRID_ROWS;
        int c = rand() % GRID_COLS;
        if (grid[r][c] == -1) continue;
        if (r == first_r && c == first_c) continue;  // Safe first click
        grid[r][c] = -1;
        placed++;
    }

    // Set numbers
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (grid[r][c] != -1) {
                grid[r][c] = count_adjacent_mines(r, c);
            }
        }
    }
}

int count_adjacent_mines(int r, int c) {
    int count = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int nr = r + dr, nc = c + dc;
            if (nr >= 0 && nr < GRID_ROWS && nc >= 0 && nc < GRID_COLS) {
                if (grid[nr][nc] == -1) count++;
            }
        }
    }
    return count;
}

// Reveal tile (with flood fill for blanks)
void reveal_tile(int r, int c) {
    if (!is_valid_tile(r, c) || tile_state[r][c] != STATE_HIDDEN || game_over) return;

    tile_state[r][c] = STATE_REVEALED;

    if (grid[r][c] == -1) {
        game_over = true;
        set_status_message("BOOM! Game Over. Press 'R' to restart.");
        return;
    }

    if (grid[r][c] == 0) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                reveal_tile(r + dr, c + dc);
            }
        }
    }

    check_win();
}

void check_win() {
    bool won = true;
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (grid[r][c] != -1 && tile_state[r][c] != STATE_REVEALED) {
                won = false;
                break;
            }
        }
    }
    if (won) {
        game_won = true;
        game_over = true;
        set_status_message("You Win! 🎉 Press 'R' to play again.");
    }
}

// Mouse input
void mouse(int button, int state, int x, int y) {
    if (state != GLUT_DOWN || game_over) return;

    // UI panel offset
    if (y < 80) return;

    int col = (x - 20) / TILE_SIZE;
    int row = (y - 80) / TILE_SIZE;
    row = GRID_ROWS - 1 - row;

    if (!is_valid_tile(row, col)) return;

    if (button == GLUT_LEFT_BUTTON) {
        if (tile_state[row][col] == STATE_FLAGGED) return;

        if (grid[row][col] == 0 && tile_state[row][col] == STATE_HIDDEN) {
            place_mines(row, col);  // First safe click
        }
        reveal_tile(row, col);

    } else if (button == GLUT_RIGHT_BUTTON) {
        if (tile_state[row][col] == STATE_REVEALED) return;

        if (tile_state[row][col] == STATE_FLAGGED) {
            tile_state[row][col] = STATE_HIDDEN;
        } else {
            tile_state[row][col] = STATE_FLAGGED;
        }
        set_status_message("Flagged (%d,%d)", row, col);
    }
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 'r' || key == 'R') {
        init_game();
        glutPostRedisplay();
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.05f, 0.05f, 0.15f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    float board_top = window_height - 80;

    // Draw grid
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = 20 + c * TILE_SIZE;
            float y = board_top - (r + 1) * TILE_SIZE;
            float cx = x + TILE_SIZE / 2;
            float cy = y + TILE_SIZE / 2;

            float color[3] = {0.2f, 0.2f, 0.4f};
            const char* emoji = NULL;

            if (tile_state[r][c] == STATE_REVEALED) {
                if (grid[r][c] == -1) {
                    emoji = EMOJI_MINE;
                    color[0] = 0.8f; color[1] = 0.0f; color[2] = 0.0f;
                } else if (grid[r][c] == 0) {
                    emoji = EMOJI_BLANK;
                    color[0] = 0.3f; color[1] = 0.3f; color[2] = 0.3f;
                } else {
                    emoji = EMOJI_NUMBERS[grid[r][c]];
                    color[0] = 0.1f; color[1] = 0.4f; color[2] = 0.8f;
                }
            } else if (tile_state[r][c] == STATE_FLAGGED) {
                emoji = EMOJI_FLAG;
                color[0] = 0.6f; color[1] = 0.2f; color[2] = 0.2f;
            } else {
                emoji = EMOJI_HIDDEN;
                color[0] = 0.3f; color[1] = 0.5f; color[2] = 0.3f;
            }

            // Draw tile background
            glBegin(GL_QUADS);
            glColor3fv(color);
            glVertex2f(x, y);
            glVertex2f(x + TILE_SIZE, y);
            glVertex2f(x + TILE_SIZE, y + TILE_SIZE);
            glVertex2f(x, y + TILE_SIZE);
            glEnd();

            // Draw emoji
            if (emoji) {
                unsigned int code;
                decode_utf8((const unsigned char*)emoji, &code);
                render_emoji(code, cx, cy);
            }
        }
    }

    // UI Panel
    float panel[3] = {0.1f, 0.1f, 0.2f};
    glBegin(GL_QUADS);
    glColor3fv(panel);
    glVertex2f(0, 0);
    glVertex2f(window_width, 0);
    glVertex2f(window_width, 80);
    glVertex2f(0, 80);
    glEnd();

    render_text("Minesweeper: Emoji Edition", 10, 60);
    render_text("Left: Reveal | Right: Flag | R: Restart", 10, 35);
    render_text(status_message, 10, 10);

    glutSwapBuffers();
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
    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);
}

int is_valid_tile(int r, int c) {
    return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    window_width = GRID_COLS * TILE_SIZE + 40;
    window_height = GRID_ROWS * TILE_SIZE + 100;
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Minesweeper: Emoji Edition");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(idle);

    srand(time(NULL));
    initFreeType();
    init_game();

    atexit(cleanup_cache);
    printf("Controls:\n");
    printf(" - Left-click: Reveal tile (safe first click)\n");
    printf(" - Right-click: Toggle flag\n");
    printf(" - R: Restart game\n");

    glutMainLoop();
    return 0;
}
