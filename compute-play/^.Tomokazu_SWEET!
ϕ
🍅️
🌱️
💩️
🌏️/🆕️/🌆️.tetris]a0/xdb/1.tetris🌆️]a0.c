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
#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20
#define TILE_SIZE    32
#define TICK_MS      500  // Speed (ms per step)

// Emojis for blocks
const char* EMOJI_I = "🔷";  // Blue square
const char* EMOJI_O = "🟨";  // Yellow
const char* EMOJI_T = "🟪";  // Purple
const char* EMOJI_S = "🟢";  // Green
const char* EMOJI_Z = "🟥";  // Red
const char* EMOJI_J = "🟦";  // Blue
const char* EMOJI_L = "🟧";  // Orange

// Block types
#define BLOCK_NONE 0
#define BLOCK_I    1
#define BLOCK_O    2
#define BLOCK_T    3
#define BLOCK_S    4
#define BLOCK_Z    5
#define BLOCK_J    6
#define BLOCK_L    7

// Game state
int board[BOARD_HEIGHT][BOARD_WIDTH] = {0};  // 0=empty, 1-7=block type
int current_block = 0;
int block_x = 0, block_y = 0;
int block_rot = 0;  // rotation state (0–3)
bool game_over = false;
int score = 0;
char status_message[256] = "Tetris! Use arrows to move, UP to rotate, SPACE to drop.";

// Window size
int window_width = BOARD_WIDTH * TILE_SIZE + 100;
int window_height = BOARD_HEIGHT * TILE_SIZE + 80;

// --- FreeType & Emoji Cache ---
FT_Library ft;
FT_Face emoji_face;
float emoji_scale;

struct EmojiCacheEntry {
    unsigned int codepoint;
    GLuint texture;
    int width, height;
    bool valid;
};
struct EmojiCacheEntry emoji_texture_cache[20];
int cache_size = 0;

// Timing
int last_drop_ms = 0;

// Prototypes
void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void idle();
void init_game();
void spawn_block();
bool is_valid_position(int x, int y, int block, int rot);
void place_block();
void clear_lines();
void set_status_message(const char* format, ...);
void cleanup_cache();

// UTF-8 Decoder
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) { *codepoint = str[0]; return 1; }
    if ((str[0] & 0xE0) == 0xC0 && (str[1] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F); return 2;
    }
    if ((str[0] & 0xF0) == 0xE0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F); return 3;
    }
    if ((str[0] & 0xF8) == 0xF0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F); return 4;
    }
    *codepoint = 0xFFFD; return 1;
}

// Initialize FreeType
void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType\n");
        exit(1);
    }

    const char *font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error loading %s. Install fonts-noto-color-emoji.\n", font_path);
        exit(1);
    }

    FT_Set_Pixel_Sizes(emoji_face, 0, TILE_SIZE - 6);
    int loaded_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)(TILE_SIZE * 0.9f) / (float)loaded_size;
}

// Render emoji
void render_emoji(unsigned int codepoint, float x, float y) {
    struct EmojiCacheEntry *entry = NULL;
    for (int i = 0; i < cache_size; i++) {
        if (emoji_texture_cache[i].codepoint == codepoint && emoji_texture_cache[i].valid) {
            entry = &emoji_texture_cache[i];
            break;
        }
    }

    if (!entry && cache_size < 20) {
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

    if (!entry) return;

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
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
}

// Shapes [rotation][y][x]
bool get_shape(int block, int rot, int shape[4][4]) {
    memset(shape, 0, sizeof(int)*16);
    switch (block) {
        case BLOCK_I:
            if (rot % 2 == 0) {
                for (int i = 0; i < 4; i++) shape[1][i] = 1;
            } else {
                for (int i = 0; i < 4; i++) shape[i][2] = 1;
            }
            return true;
        case BLOCK_O:
            shape[0][0] = shape[0][1] = shape[1][0] = shape[1][1] = 1;
            return true;
        case BLOCK_T:
            shape[0][1] = 1;
            shape[1][0] = shape[1][1] = shape[1][2] = 1;
            if (rot == 1) { /* right */ shape[0][1]=shape[1][1]=shape[1][2]=shape[2][1]=1; }
            if (rot == 2) { /* down */ shape[1][0]=shape[1][1]=shape[1][2]=shape[2][1]=1; }
            if (rot == 3) { /* left */ shape[0][1]=shape[1][0]=shape[1][1]=shape[2][1]=1; }
            return true;
        case BLOCK_S:
            shape[0][1] = shape[0][2] = shape[1][0] = shape[1][1] = 1;
            if (rot % 2 == 1) {
                shape[0][0] = 0; shape[1][0] = 1; shape[2][1] = 1; shape[2][0] = 0;
            }
            return true;
        case BLOCK_Z:
            shape[0][0] = shape[0][1] = shape[1][1] = shape[1][2] = 1;
            if (rot % 2 == 1) {
                shape[0][1] = 0; shape[1][1] = 1; shape[2][0] = 1; shape[2][1] = 0;
            }
            return true;
        case BLOCK_J:
            shape[0][0] = shape[1][0] = shape[1][1] = shape[1][2] = 1;
            if (rot == 1) { shape[0][2]=shape[1][2]=shape[1][1]=shape[1][0]=1; }
            if (rot == 2) { shape[1][0]=shape[2][0]=shape[1][1]=shape[1][2]=1; }
            if (rot == 3) { shape[0][0]=shape[0][1]=shape[0][2]=shape[1][0]=1; }
            return true;
        case BLOCK_L:
            shape[0][2] = shape[1][0] = shape[1][1] = shape[1][2] = 1;
            if (rot == 1) { shape[0][0]=shape[1][0]=shape[1][1]=shape[1][2]=1; }
            if (rot == 2) { shape[1][0]=shape[1][1]=shape[1][2]=shape[2][2]=1; }
            if (rot == 3) { shape[0][2]=shape[0][1]=shape[0][0]=shape[1][2]=1; }
            return true;
    }
    return false;
}

bool is_valid_position(int x, int y, int block, int rot) {
    int shape[4][4];
    get_shape(block, rot, shape);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape[r][c]) {
                int nx = x + c, ny = y + r;
                if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= BOARD_HEIGHT || (ny >= 0 && board[ny][nx])) {
                    return false;
                }
            }
        }
    }
    return true;
}

void place_block() {
    int shape[4][4];
    get_shape(current_block, block_rot, shape);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape[r][c]) {
                int gy = block_y + r;
                int gx = block_x + c;
                if (gy >= 0 && gy < BOARD_HEIGHT && gx >= 0 && gx < BOARD_WIDTH) {
                    board[gy][gx] = current_block;
                }
            }
        }
    }
    clear_lines();
    spawn_block();
}

void clear_lines() {
    int lines_cleared = 0;
    for (int r = BOARD_HEIGHT - 1; r >= 0; r--) {
        bool full = true;
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (board[r][c] == 0) {
                full = false;
                break;
            }
        }
        if (full) {
            lines_cleared++;
            for (int rr = r; rr > 0; rr--) {
                memmove(board[rr], board[rr-1], sizeof(int) * BOARD_WIDTH);
            }
            memset(board[0], 0, sizeof(int) * BOARD_WIDTH);
            r++;  // recheck this row
        }
    }
    if (lines_cleared > 0) {
        score += (lines_cleared == 1 ? 100 : lines_cleared == 2 ? 300 : lines_cleared == 3 ? 500 : 800);
        set_status_message("Cleared %d lines! Score: %d", lines_cleared, score);
    }
}

void spawn_block() {
    current_block = rand() % 7 + 1;
    block_x = BOARD_WIDTH / 2 - 2;
    block_y = BOARD_HEIGHT;
    block_rot = 0;

    if (!is_valid_position(block_x, block_y - 4, current_block, block_rot)) {
        game_over = true;
        set_status_message("Game Over! Score: %d. Press R to restart.", score);
    }
}

void hard_drop() {
    while (is_valid_position(block_x, block_y - 1, current_block, block_rot)) {
        block_y--;
    }
    place_block();
}

void init_game() {
    memset(board, 0, sizeof(board));
    score = 0;
    game_over = false;
    spawn_block();
    last_drop_ms = glutGet(GLUT_ELAPSED_TIME);
    set_status_message("New game started!");
}

// Input
void keyboard(unsigned char key, int x, int y) {
    if (game_over) {
        if (key == 'r' || key == 'R') init_game();
        return;
    }

    switch (key) {
        case ' ':  // Hard drop
            hard_drop();
            break;
        case 'r': case 'R':
            init_game();
            break;
    }
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    if (game_over) return;

    switch (key) {
        case GLUT_KEY_LEFT:
            if (is_valid_position(block_x - 1, block_y, current_block, block_rot))
                block_x--;
            break;
        case GLUT_KEY_RIGHT:
            if (is_valid_position(block_x + 1, block_y, current_block, block_rot))
                block_x++;
            break;
        case GLUT_KEY_DOWN:
            if (is_valid_position(block_x, block_y - 1, current_block, block_rot))
                block_y--;
            break;
        case GLUT_KEY_UP:
            if (is_valid_position(block_x, block_y, current_block, (block_rot + 1) % 4))
                block_rot = (block_rot + 1) % 4;
            break;
    }
    glutPostRedisplay();
}

void idle() {
    if (game_over) {
        glutPostRedisplay();
        return;
    }

    int now = glutGet(GLUT_ELAPSED_TIME);
    if (now - last_drop_ms > TICK_MS) {
        if (is_valid_position(block_x, block_y - 1, current_block, block_rot)) {
            block_y--;
        } else {
            place_block();
        }
        last_drop_ms = now;
        glutPostRedisplay();
    }
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

    float board_left = (window_width - BOARD_WIDTH * TILE_SIZE) / 2;
    float board_top = window_height - 40;

    // Draw board grid
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            float x = board_left + c * TILE_SIZE;
            float y = board_top - (r + 1) * TILE_SIZE;
            float cx = x + TILE_SIZE / 2;
            float cy = y + TILE_SIZE / 2;

            if (board[r][c]) {
                float color[3] = {0.7f, 0.7f, 0.7f};
                const char* emoji = NULL;
                switch (board[r][c]) {
                    case BLOCK_I: emoji = EMOJI_I; color[0]=0.2f;color[1]=0.6f;color[2]=0.8f; break;
                    case BLOCK_O: emoji = EMOJI_O; color[0]=0.8f;color[1]=0.8f;color[2]=0.2f; break;
                    case BLOCK_T: emoji = EMOJI_T; color[0]=0.6f;color[1]=0.4f;color[2]=0.8f; break;
                    case BLOCK_S: emoji = EMOJI_S; color[0]=0.2f;color[1]=0.8f;color[2]=0.2f; break;
                    case BLOCK_Z: emoji = EMOJI_Z; color[0]=0.8f;color[1]=0.2f;color[2]=0.2f; break;
                    case BLOCK_J: emoji = EMOJI_J; color[0]=0.2f;color[1]=0.4f;color[2]=0.8f; break;
                    case BLOCK_L: emoji = EMOJI_L; color[0]=0.8f;color[1]=0.5f;color[2]=0.1f; break;
                }
                glBegin(GL_QUADS);
                glColor3fv(color);
                glVertex2f(x, y);
                glVertex2f(x+TILE_SIZE, y);
                glVertex2f(x+TILE_SIZE, y+TILE_SIZE);
                glVertex2f(x, y+TILE_SIZE);
                glEnd();

                unsigned int code;
                decode_utf8((const unsigned char*)emoji, &code);
                render_emoji(code, cx, cy);
            } else {
                glColor3f(0.1f, 0.1f, 0.2f);
                glBegin(GL_LINE_LOOP);
                glVertex2f(x, y);
                glVertex2f(x+TILE_SIZE, y);
                glVertex2f(x+TILE_SIZE, y+TILE_SIZE);
                glVertex2f(x, y+TILE_SIZE);
                glEnd();
            }
        }
    }

    // Draw current block
    if (!game_over && current_block) {
        int shape[4][4];
        get_shape(current_block, block_rot, shape);
        const char* emoji = NULL;
        switch (current_block) {
            case BLOCK_I: emoji = EMOJI_I; break;
            case BLOCK_O: emoji = EMOJI_O; break;
            case BLOCK_T: emoji = EMOJI_T; break;
            case BLOCK_S: emoji = EMOJI_S; break;
            case BLOCK_Z: emoji = EMOJI_Z; break;
            case BLOCK_J: emoji = EMOJI_J; break;
            case BLOCK_L: emoji = EMOJI_L; break;
        }
        unsigned int code;
        decode_utf8((const unsigned char*)emoji, &code);
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (shape[r][c]) {
                    int gr = block_y + r;
                    int gc = block_x + c;
                    if (gr >= 0) {
                        float x = board_left + gc * TILE_SIZE + TILE_SIZE/2;
                        float y = board_top - (gr + 1) * TILE_SIZE + TILE_SIZE/2;
                        render_emoji(code, x, y);
                    }
                }
            }
        }
    }

    // UI Panel
    glColor3f(0.1f, 0.1f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(window_width, 0);
    glVertex2f(window_width, 40);
    glVertex2f(0, 40);
    glEnd();

    char ui[100];
    snprintf(ui, sizeof(ui), "Score: %d | Next: ? | R: Restart", score);
    render_text(ui, 10, 25);
    render_text(status_message, 10, 5);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

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

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    window_width = 500;
    window_height = 720;
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Tetris: Emoji Edition");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    srand(time(NULL));
    initFreeType();
    init_game();

    atexit(cleanup_cache);
    printf("Controls:\n");
    printf(" - Arrow Keys: Move/Rotate\n");
    printf(" - Space: Hard Drop\n");
    printf(" - R: Restart\n");

    glutMainLoop();
    return 0;
}
