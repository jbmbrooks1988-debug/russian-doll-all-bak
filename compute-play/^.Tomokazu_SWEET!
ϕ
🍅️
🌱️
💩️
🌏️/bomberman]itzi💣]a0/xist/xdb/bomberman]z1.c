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
#include <math.h>

// --- Constants ---
#define GRID_ROWS 15
#define GRID_COLS 17
#define TILE_SIZE 40

// Emojis
const char* EMOJI_EMPTY = "⬛️";
const char* EMOJI_WALL_SOLID = "🧱";
const char* EMOJI_WALL_DESTRUCTIBLE = "📦";
const char* EMOJI_PLAYER = "🏃";
const char* EMOJI_BOMB = "💣";
const char* EMOJI_EXPLOSION = "💥";
const char* EMOJI_ENEMY = "👾";

// Tile types
#define TILE_EMPTY              0
#define TILE_WALL_SOLID         1
#define TILE_WALL_DESTRUCTIBLE  2
#define NUM_TILE_EMOJIS         3 // Emojis that are part of the grid array

// Game Object Emojis (drawn on top of the grid)
#define EMOJI_INDEX_PLAYER      3
#define EMOJI_INDEX_BOMB        4
#define EMOJI_INDEX_EXPLOSION   5
#define EMOJI_INDEX_ENEMY       6
#define NUM_TOTAL_EMOJIS        7

// Game state
enum GameState { PLAYING, GAME_OVER };
enum GameState game_state = PLAYING;
char status_message[256] = "Use arrows to move, Space to drop a bomb!";

int tiles[GRID_ROWS][GRID_COLS];
int player_row = 1, player_col = 1;

// Bomb management
#define MAX_BOMBS 10
#define BOMB_TIMER 180 // 3 seconds at 60fps
#define EXPLOSION_DURATION 30 // 0.5 seconds
struct Bomb {
    int row, col;
    int timer;
    bool active;
} bombs[MAX_BOMBS];

struct Explosion {
    int row, col;
    int timer;
    bool active;
} explosions[MAX_BOMBS * 5]; // Max 5 explosion tiles per bomb

// --- Rendering & System ---
FT_Library ft;
FT_Face emoji_face;
int window_width = GRID_COLS * TILE_SIZE;
int window_height = GRID_ROWS * TILE_SIZE + 80;
float emoji_scale;

struct EmojiCacheEntry {
    unsigned int codepoint;
    GLuint texture;
    int width;
    int height;
    bool valid;
} emoji_texture_cache[NUM_TOTAL_EMOJIS];
int cache_size = 0;

// --- Function Prototypes ---
void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void game_loop(int value);
void init();
void generate_world();
bool is_valid_tile(int r, int c);
bool is_walkable(int r, int c);
void set_status_message(const char* format, ...);
void place_bomb(int r, int c);
void update_game();
void trigger_explosion(int r, int c);
void restart_game();
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void cleanup_cache();

// --- UTF-8 Decoder ---
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) { *codepoint = str[0]; return 1; }
    if ((str[0] & 0xE0) == 0xC0) { *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F); return 2; }
    if ((str[0] & 0xF0) == 0xE0) { *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F); return 3; }
    if ((str[0] & 0xF8) == 0xF0) { *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F); return 4; }
    *codepoint = 0xFFFD; return 1;
}

// --- FreeType Init ---
void initFreeType() {
    if (FT_Init_FreeType(&ft)) { fprintf(stderr, "Could not init FreeType\n"); exit(1); }
    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    if (FT_New_Face(ft, emoji_font_path, 0, &emoji_face)) { fprintf(stderr, "Could not load emoji font\n"); exit(1); }
    FT_Set_Pixel_Sizes(emoji_face, 0, TILE_SIZE);
    emoji_scale = 0.8f;
}

// --- Emoji Rendering with Caching ---
void render_emoji_from_cache(int cache_index, float x, float y) {
    if (cache_index < 0 || cache_index >= cache_size || !emoji_texture_cache[cache_index].valid) return;
    struct EmojiCacheEntry *entry = &emoji_texture_cache[cache_index];

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

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void cache_emoji(const char* emoji_char, int cache_index) {
    unsigned int codepoint;
    decode_utf8((const unsigned char*)emoji_char, &codepoint);

    if (FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR)) {
        fprintf(stderr, "Failed to load glyph for codepoint %u\n", codepoint);
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
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    emoji_texture_cache[cache_index] = (struct EmojiCacheEntry){codepoint, texture, slot->bitmap.width, slot->bitmap.rows, true};
    if (cache_index >= cache_size) cache_size = cache_index + 1;
}

// --- Text Rendering ---
void render_text(const char* str, float x, float y) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor3f(1.0f, 1.0f, 1.0f);
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
            if (r == 0 || c == 0 || r == GRID_ROWS - 1 || c == GRID_COLS - 1 || (r % 2 == 0 && c % 2 == 0)) {
                tiles[r][c] = TILE_WALL_SOLID;
            } else {
                if (rand() % 100 < 70) {
                    tiles[r][c] = TILE_WALL_DESTRUCTIBLE;
                } else {
                    tiles[r][c] = TILE_EMPTY;
                }
            }
        }
    }
    // Clear starting area for player
    tiles[1][1] = TILE_EMPTY;
    tiles[1][2] = TILE_EMPTY;
    tiles[2][1] = TILE_EMPTY;
}

// --- Game Logic ---
bool is_valid_tile(int r, int c) {
    return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS;
}

bool is_walkable(int r, int c) {
    if (!is_valid_tile(r, c)) return false;
    return tiles[r][c] == TILE_EMPTY;
}

void restart_game() {
    player_row = 1;
    player_col = 1;
    game_state = PLAYING;
    generate_world();
    memset(bombs, 0, sizeof(bombs));
    memset(explosions, 0, sizeof(explosions));
    set_status_message("Use arrows to move, Space to drop a bomb!");
}

void place_bomb(int r, int c) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) {
            bombs[i] = (struct Bomb){r, c, BOMB_TIMER, true};
            return;
        }
    }
}

void trigger_explosion(int r, int c) {
    int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};
    
    // Center explosion
    int explosion_idx = 0;
    while(explosion_idx < MAX_BOMBS * 5 && explosions[explosion_idx].active) explosion_idx++;
    if(explosion_idx < MAX_BOMBS * 5) explosions[explosion_idx] = (struct Explosion){r, c, EXPLOSION_DURATION, true};

    for(int i = 0; i < 4; i++) {
        int nr = r + dirs[i][0];
        int nc = c + dirs[i][1];
        if(is_valid_tile(nr, nc)) {
            if(tiles[nr][nc] == TILE_WALL_DESTRUCTIBLE) {
                tiles[nr][nc] = TILE_EMPTY;
            }
            if(tiles[nr][nc] != TILE_WALL_SOLID) {
                 explosion_idx = 0;
                 while(explosion_idx < MAX_BOMBS * 5 && explosions[explosion_idx].active) explosion_idx++;
                 if(explosion_idx < MAX_BOMBS * 5) explosions[explosion_idx] = (struct Explosion){nr, nc, EXPLOSION_DURATION, true};
            }
        }
    }
}

void update_game() {
    if (game_state != PLAYING) return;

    // Update bombs
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            bombs[i].timer--;
            if (bombs[i].timer <= 0) {
                bombs[i].active = false;
                trigger_explosion(bombs[i].row, bombs[i].col);
            }
        }
    }

    // Update explosions
    bool player_hit = false;
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            explosions[i].timer--;
            if (explosions[i].row == player_row && explosions[i].col == player_col) {
                player_hit = true;
            }
            if (explosions[i].timer <= 0) {
                explosions[i].active = false;
            }
        }
    }

    if (player_hit) {
        game_state = GAME_OVER;
        set_status_message("GAME OVER! Press 'r' to restart.");
    }
}

// --- Input Handlers ---
void keyboard(unsigned char key, int x, int y) {
    if (game_state == PLAYING) {
        if (key == ' ') { // Spacebar
            place_bomb(player_row, player_col);
        }
    }
    if (key == 'r' || key == 'R') {
        restart_game();
    }
    if (key == 27) { // Escape
        exit(0);
    }
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    if (game_state != PLAYING) return;
    int next_r = player_row, next_c = player_col;
    switch (key) {
        case GLUT_KEY_UP:    next_r--; break;
        case GLUT_KEY_DOWN:  next_r++; break;
        case GLUT_KEY_LEFT:  next_c--; break;
        case GLUT_KEY_RIGHT: next_c++; break;
    }
    if (is_walkable(next_r, next_c)) {
        player_row = next_r;
        player_col = next_c;
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // === GAME GRID ===
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = c * TILE_SIZE;
            float y = window_height - (r + 1) * TILE_SIZE;
            float cx = x + TILE_SIZE / 2;
            float cy = y + TILE_SIZE / 2;

            // Draw a colored rectangle for the tile background for debugging
            float debug_color[3];
            switch(tiles[r][c]) {
                case TILE_WALL_SOLID:
                    debug_color[0] = 0.5f; debug_color[1] = 0.5f; debug_color[2] = 0.5f; // Gray
                    break;
                case TILE_WALL_DESTRUCTIBLE:
                    debug_color[0] = 0.7f; debug_color[1] = 0.4f; debug_color[2] = 0.2f; // Brown
                    break;
                default: // TILE_EMPTY
                    debug_color[0] = 0.2f; debug_color[1] = 0.2f; debug_color[2] = 0.2f; // Dark Gray
                    break;
            }
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, debug_color);

            // Render emoji on top
            int emoji_idx = tiles[r][c];
            render_emoji_from_cache(emoji_idx, cx, cy);
        }
    }

    // === BOMBS ===
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            float x = bombs[i].col * TILE_SIZE;
            float y = window_height - (bombs[i].row + 1) * TILE_SIZE;
            float magenta[] = {1.0f, 0.0f, 1.0f};
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, magenta);
            render_emoji_from_cache(EMOJI_INDEX_BOMB, x + TILE_SIZE / 2, y + TILE_SIZE / 2);
        }
    }
    
    // === EXPLOSIONS ===
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            float x = explosions[i].col * TILE_SIZE;
            float y = window_height - (explosions[i].row + 1) * TILE_SIZE;
            float yellow[] = {1.0f, 1.0f, 0.0f};
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, yellow);
            render_emoji_from_cache(EMOJI_INDEX_EXPLOSION, x + TILE_SIZE / 2, y + TILE_SIZE / 2);
        }
    }

    // === PLAYER ===
    if (game_state == PLAYING) {
        float px = player_col * TILE_SIZE;
        float py = window_height - (player_row + 1) * TILE_SIZE;
        float cyan[] = {0.0f, 1.0f, 1.0f};
        draw_rect(px, py, TILE_SIZE, TILE_SIZE, cyan);
        render_emoji_from_cache(EMOJI_INDEX_PLAYER, px + TILE_SIZE / 2, py + TILE_SIZE / 2);
    }

    // === UI PANEL ===
    float panel_color[3] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, 80, panel_color);
    render_text("Emoji Bomberman", 10, 50);
    render_text(status_message, 10, 20);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    window_width = w;
    window_height = h;
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
}

void game_loop(int value) {
    update_game();
    glutPostRedisplay();
    glutTimerFunc(16, game_loop, 0); // ~60 FPS
}

void init() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    initFreeType();
    
    // Pre-cache all emojis
    cache_emoji(EMOJI_EMPTY, TILE_EMPTY);
    cache_emoji(EMOJI_WALL_SOLID, TILE_WALL_SOLID);
    cache_emoji(EMOJI_WALL_DESTRUCTIBLE, TILE_WALL_DESTRUCTIBLE);
    cache_emoji(EMOJI_PLAYER, EMOJI_INDEX_PLAYER);
    cache_emoji(EMOJI_BOMB, EMOJI_INDEX_BOMB);
    cache_emoji(EMOJI_EXPLOSION, EMOJI_INDEX_EXPLOSION);
    cache_emoji(EMOJI_ENEMY, EMOJI_INDEX_ENEMY);

    restart_game();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Emoji Bomberman");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    
    init();
    
    glutTimerFunc(0, game_loop, 0);

    printf("Controls:\n");
    printf(" - Arrows: Move Player\n");
    printf(" - Space: Place Bomb\n");
    printf(" - R: Restart Game\n");
    printf(" - ESC: Exit\n");

    atexit(cleanup_cache);
    glutMainLoop();

    return 0;
}
