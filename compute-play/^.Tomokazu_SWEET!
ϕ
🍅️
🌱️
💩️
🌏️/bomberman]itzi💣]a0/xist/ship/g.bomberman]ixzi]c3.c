#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>

// --- Constants ---
#define GRID_ROWS 15
#define GRID_COLS 17
#define TILE_SIZE 40
#define MAX_BOMBS 10
#define BOMB_TIMER 90 // 1.5 seconds at 60fps
#define EXPLOSION_DURATION 30
#define MAX_ENEMIES 5
#define MAX_ITEMS 20

// --- Tile types ---
#define TILE_EMPTY              0
#define TILE_WALL_SOLID         1
#define TILE_WALL_DESTRUCTIBLE  2

// --- Game state ---
enum GameState { PLAYING, GAME_OVER };
enum GameState game_state = PLAYING;
char status_message[256] = "Use arrow keys to move, Space to place bomb!";
int tiles[GRID_ROWS][GRID_COLS];
int player_row = 1, player_col = 1;
int player_max_bombs = 1;
int player_flame_power = 1;
int level = 1;
int score = 0;

// --- Bomb management ---
struct Bomb { int row, col, timer; bool active; } bombs[MAX_BOMBS];
struct Explosion { int row, col, timer; bool active; } explosions[MAX_BOMBS * 5];

// --- Enemy management ---
struct Enemy { int row, col, move_timer; bool active; } enemies[MAX_ENEMIES];

// --- Item management ---
struct Item { int row, col, type; bool active; } items[MAX_ITEMS];

// --- Rendering & System ---
int window_width = GRID_COLS * TILE_SIZE;
int window_height = GRID_ROWS * TILE_SIZE + 100;

// --- ASCII Glyphs ---
const char GLYPH_EMPTY = '.';
const char GLYPH_WALL_SOLID = '#';
const char GLYPH_WALL_DESTRUCTIBLE = '+';
const char GLYPH_PLAYER = '@';
const char GLYPH_BOMB = 'O';
const char GLYPH_EXPLOSION = '*';
const char GLYPH_ENEMY = 'E';

// --- Function Prototypes ---
void render_ascii_glyph(char glyph, float x, float y, float color[3]);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void game_loop(int value);
void init();
void generate_world();
void place_enemies();
bool enemy_on_pos(int r, int c);
bool is_valid_tile(int r, int c);
bool is_walkable(int r, int c);
void set_status_message(const char* format, ...);
void place_bomb(int r, int c);
int count_active_bombs();
void update_game();
void add_explosion(int r, int c);
void trigger_explosion(int r, int c, int power);
void restart_game();
void draw_rect(float x, float y, float w, float h, float color[3]);
void save_game();
void load_game();

// --- World Generation ---
void generate_world() {
    srand(time(NULL));
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (r == 0 || c == 0 || r == GRID_ROWS - 1 || c == GRID_COLS - 1 || (r % 2 == 0 && c % 2 == 0)) {
                tiles[r][c] = TILE_WALL_SOLID;
            } else {
                tiles[r][c] = (rand() % 100 < 70) ? TILE_WALL_DESTRUCTIBLE : TILE_EMPTY;
            }
        }
    }
    tiles[1][1] = TILE_EMPTY; tiles[1][2] = TILE_EMPTY; tiles[2][1] = TILE_EMPTY;
    place_enemies();
}

bool enemy_on_pos(int r, int c) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].row == r && enemies[i].col == c) return true;
    }
    return false;
}

void place_enemies() {
    int num_enemies = level;
    int placed = 0;
    for (int i = 0; i < MAX_ENEMIES && placed < num_enemies; i++) {
        int attempts = 0;
        int er, ec;
        do {
            er = 1 + rand() % (GRID_ROWS - 2);
            ec = 1 + rand() % (GRID_COLS - 2);
            attempts++;
        } while ((tiles[er][ec] != TILE_EMPTY || (er == player_row && ec == player_col) || enemy_on_pos(er, ec)) && attempts < 100);
        if (attempts < 100) {
            enemies[i].row = er;
            enemies[i].col = ec;
            enemies[i].move_timer = rand() % 60;
            enemies[i].active = true;
            placed++;
        }
    }
}

bool is_valid_tile(int r, int c) { return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS; }
bool is_walkable(int r, int c) { return is_valid_tile(r, c) && tiles[r][c] == TILE_EMPTY; }

void restart_game() {
    player_row = 1; player_col = 1;
    player_max_bombs = 1;
    player_flame_power = 1;
    level = 1;
    score = 0;
    game_state = PLAYING;
    generate_world();
    memset(bombs, 0, sizeof(bombs));
    memset(explosions, 0, sizeof(explosions));
    memset(items, 0, sizeof(items));
    memset(enemies, 0, sizeof(enemies));
    set_status_message("Use arrow keys to move, Space to place bomb!");
}

int count_active_bombs() {
    int cnt = 0;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) cnt++;
    }
    return cnt;
}

void place_bomb(int r, int c) {
    if (count_active_bombs() >= player_max_bombs) return;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) {
            bombs[i] = (struct Bomb){r, c, BOMB_TIMER, true};
            return;
        }
    }
}

void add_explosion(int r, int c) {
    int idx = 0;
    while (idx < MAX_BOMBS * 5 && explosions[idx].active) idx++;
    if (idx < MAX_BOMBS * 5) {
        explosions[idx] = (struct Explosion){r, c, EXPLOSION_DURATION, true};
    }
}

void trigger_explosion(int r, int c, int power) {
    int dirs[4][2] = {{0,1}, {1,0}, {0,-1}, {-1,0}};
    add_explosion(r, c);
    for (int d = 0; d < 4; d++) {
        int dr = dirs[d][0], dc = dirs[d][1];
        for (int dist = 1; dist <= power; dist++) {
            int nr = r + dist * dr, nc = c + dist * dc;
            if (!is_valid_tile(nr, nc)) break;
            add_explosion(nr, nc);
            if (tiles[nr][nc] == TILE_WALL_SOLID) break;
            if (tiles[nr][nc] == TILE_WALL_DESTRUCTIBLE) {
                tiles[nr][nc] = TILE_EMPTY;
                int iidx = 0;
                while (iidx < MAX_ITEMS && items[iidx].active) iidx++;
                if (iidx < MAX_ITEMS && rand() % 5 == 0) {
                    items[iidx] = (struct Item){nr, nc, rand() % 2, true};
                }
                break;
            }
        }
    }
}

void update_game() {
    if (game_state != PLAYING) return;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active && --bombs[i].timer <= 0) {
            bombs[i].active = false;
            trigger_explosion(bombs[i].row, bombs[i].col, player_flame_power);
        }
    }
    // Update enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        enemies[i].move_timer--;
        if (enemies[i].move_timer <= 0) {
            enemies[i].move_timer = 30 + rand() % 30;
            int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};
            int d = rand() % 4;
            int nr = enemies[i].row + dirs[d][0];
            int nc = enemies[i].col + dirs[d][1];
            if (is_valid_tile(nr, nc) && tiles[nr][nc] == TILE_EMPTY && !enemy_on_pos(nr, nc)) {
                enemies[i].row = nr;
                enemies[i].col = nc;
                if (nr == player_row && nc == player_col) {
                    game_state = GAME_OVER;
                    set_status_message("GAME OVER! Eaten by enemy. Press 'r' to restart.");
                }
            }
        }
        // Check if hit by explosion
        bool hit = false;
        for (int j = 0; j < MAX_BOMBS * 5; j++) {
            if (explosions[j].active && explosions[j].row == enemies[i].row && explosions[j].col == enemies[i].col) {
                hit = true;
                break;
            }
        }
        if (hit) {
            enemies[i].active = false;
            score += 100;
        }
    }
    // Check if all enemies dead
    int active_count = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) active_count++;
    }
    if (active_count == 0 && level < 10) {
        level++;
        place_enemies();
        char msg[256];
        sprintf(msg, "Congratulations! Entering level %d. Continue to defeat enemies!", level);
        set_status_message("%s", msg);
    }
    // Update explosions
    bool player_hit = false;
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            if (explosions[i].row == player_row && explosions[i].col == player_col) player_hit = true;
            if (--explosions[i].timer <= 0) explosions[i].active = false;
        }
    }
    if (player_hit) {
        game_state = GAME_OVER;
        set_status_message("GAME OVER! Hit by explosion. Press 'r' to restart.");
    }
}

// --- Input Handlers ---
void keyboard(unsigned char key, int x, int y) {
    if (game_state == PLAYING && key == ' ') {
        place_bomb(player_row, player_col);
    }
    if (key == 'r' || key == 'R') restart_game();
    if (key == 's' || key == 'S') save_game();
    if (key == 'l' || key == 'L') load_game();
    if (key == 27) exit(0);
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    if (game_state != PLAYING) return;
    int nr = player_row, nc = player_col;
    switch (key) {
        case GLUT_KEY_UP: nr--; break;
        case GLUT_KEY_DOWN: nr++; break;
        case GLUT_KEY_LEFT: nc--; break;
        case GLUT_KEY_RIGHT: nc++; break;
    }
    if (is_walkable(nr, nc)) {
        player_row = nr;
        player_col = nc;
        // Pick up items
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (items[i].active && items[i].row == nr && items[i].col == nc) {
                char msg[256];
                if (items[i].type == 0) {
                    player_max_bombs++;
                    sprintf(msg, "Got extra bomb! Max bombs: %d", player_max_bombs);
                } else {
                    player_flame_power++;
                    sprintf(msg, "Got flame upgrade! Flame range: %d", player_flame_power);
                }
                set_status_message("%s", msg);
                items[i].active = false;
            }
        }
        // Check enemy collision
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active && enemies[i].row == nr && enemies[i].col == nc) {
                game_state = GAME_OVER;
                set_status_message("GAME OVER! Collided with enemy. Press 'r' to restart.");
                return;
            }
        }
    }
    glutPostRedisplay();
}

// --- Drawing ---
void draw_rect(float x, float y, float w, float h, float color[3]) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void render_ascii_glyph(char glyph, float x, float y, float color[3]) {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3fv(color);
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

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    float white[] = {1.0f, 1.0f, 1.0f}, black[] = {0.0f, 0.0f, 0.0f};

    // === GAME GRID ===
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = c * TILE_SIZE, y = window_height - (r + 1) * TILE_SIZE;
            float bg_color[3]; char glyph = ' ';
            switch (tiles[r][c]) {
                case TILE_WALL_SOLID: 
                    bg_color[0] = 0.5f; bg_color[1] = 0.5f; bg_color[2] = 0.5f; 
                    glyph = GLYPH_WALL_SOLID; 
                    break;
                case TILE_WALL_DESTRUCTIBLE: 
                    bg_color[0] = 0.7f; bg_color[1] = 0.4f; bg_color[2] = 0.2f; 
                    glyph = GLYPH_WALL_DESTRUCTIBLE; 
                    break;
                default: 
                    bg_color[0] = 0.2f; bg_color[1] = 0.2f; bg_color[2] = 0.2f; 
                    glyph = GLYPH_EMPTY; 
                    break;
            }
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, bg_color);
            if (glyph != ' ') render_ascii_glyph(glyph, x, y, white);
        }
    }

    // === DYNAMIC OBJECTS ===
    float px = player_col * TILE_SIZE, py = window_height - (player_row + 1) * TILE_SIZE;
    if (game_state == PLAYING) {
        float cyan[] = {0.0f, 1.0f, 1.0f};
        draw_rect(px, py, TILE_SIZE, TILE_SIZE, cyan);
        render_ascii_glyph(GLYPH_PLAYER, px, py, white);
    }
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            float bx = bombs[i].col * TILE_SIZE, by = window_height - (bombs[i].row + 1) * TILE_SIZE;
            float magenta[] = {1.0f, 0.0f, 1.0f};
            draw_rect(bx, by, TILE_SIZE, TILE_SIZE, magenta);
            render_ascii_glyph(GLYPH_BOMB, bx, by, black);
        }
    }
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            float ex = explosions[i].col * TILE_SIZE, ey = window_height - (explosions[i].row + 1) * TILE_SIZE;
            float yellow[] = {1.0f, 1.0f, 0.0f};
            draw_rect(ex, ey, TILE_SIZE, TILE_SIZE, yellow);
            render_ascii_glyph(GLYPH_EXPLOSION, ex, ey, black);
        }
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            float ex = enemies[i].col * TILE_SIZE, ey = window_height - (enemies[i].row + 1) * TILE_SIZE;
            float red[] = {1.0f, 0.0f, 0.0f};
            draw_rect(ex, ey, TILE_SIZE, TILE_SIZE, red);
            render_ascii_glyph(GLYPH_ENEMY, ex, ey, white);
        }
    }
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            float ix = items[i].col * TILE_SIZE + 8, iy = window_height - (items[i].row + 1) * TILE_SIZE + 8;
            float icol[3];
            if (items[i].type == 0) {
                icol[0] = 0.0f; icol[1] = 0.0f; icol[2] = 1.0f; // Blue for bomb
            } else {
                icol[0] = 1.0f; icol[1] = 0.5f; icol[2] = 0.0f; // Orange for flame
            }
            draw_rect(ix, iy, TILE_SIZE - 16, TILE_SIZE - 16, icol);
        }
    }

    // === UI PANEL ===
    float panel_color[] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, 100, panel_color);
    render_text("Bomberman", 10, 70);
    render_text(status_message, 10, 50);
    char stats_buf[100];
    sprintf(stats_buf, "Level: %d  Score: %d  Bombs: %d  Flame: %d", level, score, player_max_bombs, player_flame_power);
    render_text(stats_buf, 10, 30);
    glutSwapBuffers();
}

// --- Save/Load ---
void save_game() {
    FILE* f = fopen("save.csv", "w");
    if (!f) return;
    // Map
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            fprintf(f, "%d", tiles[r][c]);
            if (c < GRID_COLS - 1) fprintf(f, ",");
        }
        fprintf(f, "\n");
    }
    // Player
    fprintf(f, "PLAYER,%d,%d\n", player_row, player_col);
    // Enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) fprintf(f, "ENEMY,%d,%d\n", enemies[i].row, enemies[i].col);
    }
    // Items
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) fprintf(f, "ITEM,%d,%d,%d\n", items[i].row, items[i].col, items[i].type);
    }
    // Game state
    fprintf(f, "LEVEL,%d\n", level);
    fprintf(f, "SCORE,%d\n", score);
    fprintf(f, "MAX_BOMBS,%d\n", player_max_bombs);
    fprintf(f, "FLAME_POWER,%d\n", player_flame_power);
    fclose(f);
    set_status_message("Game saved to save.csv");
}

void load_game() {
    FILE* f = fopen("save.csv", "r");
    if (!f) {
        set_status_message("No save file found.");
        return;
    }
    // Read map
    for (int r = 0; r < GRID_ROWS; r++) {
        char line[512];
        if (fgets(line, sizeof(line), f)) {
            char* tok = strtok(line, ",");
            int c = 0;
            while (tok && c < GRID_COLS) {
                tiles[r][c++] = atoi(tok);
                tok = strtok(NULL, ",");
            }
        }
    }
    // Read other data
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PLAYER,", 7) == 0) {
            sscanf(line + 7, "%d,%d", &player_row, &player_col);
        } else if (strncmp(line, "ENEMY,", 6) == 0) {
            int er, ec, idx = 0;
            sscanf(line + 6, "%d,%d", &er, &ec);
            while (idx < MAX_ENEMIES && enemies[idx].active) idx++;
            if (idx < MAX_ENEMIES) {
                enemies[idx].row = er;
                enemies[idx].col = ec;
                enemies[idx].move_timer = 0;
                enemies[idx].active = true;
            }
        } else if (strncmp(line, "ITEM,", 5) == 0) {
            int ir, ic, it, idx = 0;
            sscanf(line + 5, "%d,%d,%d", &ir, &ic, &it);
            while (idx < MAX_ITEMS && items[idx].active) idx++;
            if (idx < MAX_ITEMS) {
                items[idx].row = ir;
                items[idx].col = ic;
                items[idx].type = it;
                items[idx].active = true;
            }
        } else if (strncmp(line, "LEVEL,", 6) == 0) {
            sscanf(line + 6, "%d", &level);
        } else if (strncmp(line, "SCORE,", 6) == 0) {
            sscanf(line + 6, "%d", &score);
        } else if (strncmp(line, "MAX_BOMBS,", 10) == 0) {
            sscanf(line + 10, "%d", &player_max_bombs);
        } else if (strncmp(line, "FLAME_POWER,", 12) == 0) {
            sscanf(line + 12, "%d", &player_flame_power);
        }
    }
    fclose(f);
    memset(bombs, 0, sizeof(bombs));
    memset(explosions, 0, sizeof(explosions));
    game_state = PLAYING;
    set_status_message("Game loaded from save.csv");
}

// --- Main & Init ---
void reshape(int w, int h) { glViewport(0, 0, w, h); window_width = w; window_height = h; }
void set_status_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(status_message, sizeof(status_message), format, args);
    va_end(args);
}
void game_loop(int value) { update_game(); glutPostRedisplay(); glutTimerFunc(16, game_loop, 0); }

void init() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    restart_game();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Bomberman");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    
    init();
    glutTimerFunc(0, game_loop, 0);

    printf("Controls:\n - Arrow keys: Move player\n - Space: Place bomb\n - R: Restart game\n - S: Save game\n - L: Load game\n - ESC: Exit\n");
    glutMainLoop();
    return 0;
}
