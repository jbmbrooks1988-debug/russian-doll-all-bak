#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

// --- Constants ---
#define GRID_ROWS 15
#define GRID_COLS 17
#define TILE_SIZE 40

// ASCII Gylphs
const char GLYPH_EMPTY = '.';
const char GLYPH_WALL_SOLID = '#';
const char GLYPH_WALL_DESTRUCTIBLE = '+';
const char GLYPH_PLAYER = '@';
const char GLYPH_BOMB = 'O';
const char GLYPH_EXPLOSION = '*';
const char GLYPH_ENEMY = 'X';

// Tile types
#define TILE_EMPTY              0
#define TILE_WALL_SOLID         1
#define TILE_WALL_DESTRUCTIBLE  2

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
} explosions[MAX_BOMBS * 5];

// --- Rendering & System ---
int window_width = GRID_COLS * TILE_SIZE;
int window_height = GRID_ROWS * TILE_SIZE + 80;

// --- Function Prototypes ---
void render_glyph(char glyph, float x, float y, float color[3]);
void render_text(const char* str, float x, float y);
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
void draw_rect(float x, float y, float w, float h, float color[3]);

// --- Glyph & Text Rendering ---
void render_glyph(char glyph, float x, float y, float color[3]) {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3fv(color);
    // Position the character in the center of the tile
    // GLUT bitmap characters are drawn from the bottom-left, so we adjust.
    glRasterPos2f(x + (TILE_SIZE / 2.0f) - 5.0f, y + (TILE_SIZE / 2.0f) - 8.0f);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, glyph);
}

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
    tiles[1][1] = TILE_EMPTY;
    tiles[1][2] = TILE_EMPTY;
    tiles[2][1] = TILE_EMPTY;
}

// --- Game Logic ---
bool is_valid_tile(int r, int c) { return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS; }
bool is_walkable(int r, int c) { return is_valid_tile(r, c) && tiles[r][c] == TILE_EMPTY; }

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
            bombs[i] = (struct Bomb){r, c, BOMB_TIMER, true}; return;
        }
    }
}

void trigger_explosion(int r, int c) {
    int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};
    int explosion_idx = 0;
    while(explosion_idx < MAX_BOMBS * 5 && explosions[explosion_idx].active) explosion_idx++;
    if(explosion_idx < MAX_BOMBS * 5) explosions[explosion_idx] = (struct Explosion){r, c, EXPLOSION_DURATION, true};

    for(int i = 0; i < 4; i++) {
        int nr = r + dirs[i][0], nc = c + dirs[i][1];
        if(is_valid_tile(nr, nc)) {
            if(tiles[nr][nc] == TILE_WALL_DESTRUCTIBLE) tiles[nr][nc] = TILE_EMPTY;
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
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active && --bombs[i].timer <= 0) {
            bombs[i].active = false;
            trigger_explosion(bombs[i].row, bombs[i].col);
        }
    }
    bool player_hit = false;
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            if (explosions[i].row == player_row && explosions[i].col == player_col) player_hit = true;
            if (--explosions[i].timer <= 0) explosions[i].active = false;
        }
    }
    if (player_hit) {
        game_state = GAME_OVER;
        set_status_message("GAME OVER! Press 'r' to restart.");
    }
}

// --- Input Handlers ---
void keyboard(unsigned char key, int x, int y) {
    if (game_state == PLAYING && key == ' ') place_bomb(player_row, player_col);
    if (key == 'r' || key == 'R') restart_game();
    if (key == 27) exit(0);
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
    if (is_walkable(next_r, next_c)) { player_row = next_r; player_col = next_c; }
    glutPostRedisplay();
}

// --- Drawing ---
void draw_rect(float x, float y, float w, float h, float color[3]) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor3fv(color);
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    float white[] = {1.0f, 1.0f, 1.0f};
    float black[] = {0.0f, 0.0f, 0.0f};

    // === GAME GRID ===
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = c * TILE_SIZE;
            float y = window_height - (r + 1) * TILE_SIZE;
            float bg_color[3];
            char glyph = ' ';
            switch(tiles[r][c]) {
                case TILE_WALL_SOLID:
                    bg_color[0] = 0.5f; bg_color[1] = 0.5f; bg_color[2] = 0.5f; glyph = GLYPH_WALL_SOLID; break;
                case TILE_WALL_DESTRUCTIBLE:
                    bg_color[0] = 0.7f; bg_color[1] = 0.4f; bg_color[2] = 0.2f; glyph = GLYPH_WALL_DESTRUCTIBLE; break;
                default: // TILE_EMPTY
                    bg_color[0] = 0.2f; bg_color[1] = 0.2f; bg_color[2] = 0.2f; glyph = GLYPH_EMPTY; break;
            }
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, bg_color);
            if(glyph != ' ') render_glyph(glyph, x, y, white);
        }
    }

    // === BOMBS ===
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            float x = bombs[i].col * TILE_SIZE, y = window_height - (bombs[i].row + 1) * TILE_SIZE;
            float magenta[] = {1.0f, 0.0f, 1.0f};
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, magenta);
            render_glyph(GLYPH_BOMB, x, y, black);
        }
    }
    
    // === EXPLOSIONS ===
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            float x = explosions[i].col * TILE_SIZE, y = window_height - (explosions[i].row + 1) * TILE_SIZE;
            float yellow[] = {1.0f, 1.0f, 0.0f};
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, yellow);
            render_glyph(GLYPH_EXPLOSION, x, y, black);
        }
    }

    // === PLAYER ===
    if (game_state == PLAYING) {
        float px = player_col * TILE_SIZE, py = window_height - (player_row + 1) * TILE_SIZE;
        float cyan[] = {0.0f, 1.0f, 1.0f};
        draw_rect(px, py, TILE_SIZE, TILE_SIZE, cyan);
        render_glyph(GLYPH_PLAYER, px, py, white);
    }

    // === UI PANEL ===
    float panel_color[] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, 80, panel_color);
    render_text("ASCII Bomberman", 10, 50);
    render_text(status_message, 10, 20);

    glutSwapBuffers();
}

void reshape(int w, int h) { glViewport(0, 0, w, h); window_width = w; window_height = h; }
void set_status_message(const char* format, ...) { va_list args; va_start(args, format); vsnprintf(status_message, sizeof(status_message), format, args); va_end(args); }
void game_loop(int value) { update_game(); glutPostRedisplay(); glutTimerFunc(16, game_loop, 0); }

void init() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    restart_game();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("ASCII Bomberman");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    
    init();
    glutTimerFunc(0, game_loop, 0);

    printf("Controls:\n - Arrows: Move Player\n - Space: Place Bomb\n - R: Restart Game\n - ESC: Exit\n");

    glutMainLoop();
    return 0;
}