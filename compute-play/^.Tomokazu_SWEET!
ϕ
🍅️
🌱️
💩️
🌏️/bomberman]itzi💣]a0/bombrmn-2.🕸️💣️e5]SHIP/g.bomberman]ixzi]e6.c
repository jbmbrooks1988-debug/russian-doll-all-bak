#include <GL/glut.h>
#include <ft2build.h>
#include FT_FREETYPE_H
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
#define MAX_QUEUE 255
#define UI_HEIGHT 150

// --- Tile types ---
#define TILE_EMPTY              0
#define TILE_WALL_SOLID         1
#define TILE_WALL_DESTRUCTIBLE  2

// --- Game state ---
enum GameState { PLAYING, GAME_OVER };
enum GameState game_state = PLAYING;
char status_message[256] = "使用箭头键移动，空格键放置炸弹！";
char english_status[256] = "(Use arrow keys to move, Space to place bomb!)";
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
int window_height = GRID_ROWS * TILE_SIZE + UI_HEIGHT;

// --- FreeType & Glyph Texture Globals ---
FT_Library ft;
FT_Face glyph_face;
FT_Face text_face;
#define GLYPH_COUNT 7
GLuint g_glyph_textures[GLYPH_COUNT];
const char* g_glyph_strings[GLYPH_COUNT] = {
    " ", "墙", "砖", 
    "人", "炸", "爆", "敌"
};

// --- Function Prototypes ---
void render_glyph_texture(GLuint texture_id, float x, float y);
void render_string(float x, float y, const char* text, float color[3]);
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
void set_status_message(const char* chn_format, const char* eng_format, ...);
void place_bomb(int r, int c);
int count_active_bombs();
void update_game();
void add_explosion(int r, int c);
void trigger_explosion(int r, int c, int power);
void restart_game();
void draw_rect(float x, float y, float w, float h, float color[3]);
bool init_glyphs();
void cleanup_glyphs();
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void save_game();
void load_game();
bool bfs_next_move(int er, int ec, int pr, int pc, int* dr, int* dc);
void create_menu();
void menu_callback(int option);

// --- UTF-8 Decoder ---
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) { *codepoint = str[0]; return 1; }
    if ((str[0] & 0xE0) == 0xC0) { *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F); return 2; }
    if ((str[0] & 0xF0) == 0xE0) { *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F); return 3; }
    if ((str[0] & 0xF8) == 0xF0) { *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F); return 4; }
    *codepoint = 0xFFFD; return 1;
}

// --- Glyph Loader ---
bool load_glyph_texture(const char* glyph_char, GLuint* texture_id) {
    unsigned int codepoint;
    decode_utf8((const unsigned char*)glyph_char, &codepoint);

    if (FT_Load_Char(glyph_face, codepoint, FT_LOAD_RENDER)) return false;
    FT_GlyphSlot slot = glyph_face->glyph;
    if (!slot->bitmap.buffer || slot->bitmap.width == 0 || slot->bitmap.rows == 0) return false;
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) return false;

    glGenTextures(1, texture_id);
    glBindTexture(GL_TEXTURE_2D, *texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, slot->bitmap.width, slot->bitmap.rows, 0, GL_ALPHA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool init_glyphs() {
    if (FT_Init_FreeType(&ft)) return false;
    const char* font_path = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc";
    if (FT_New_Face(ft, font_path, 0, &glyph_face)) {
        return false;
    }
    FT_Set_Pixel_Sizes(glyph_face, 0, 48);
    if (FT_New_Face(ft, font_path, 0, &text_face)) {
        FT_Done_Face(glyph_face);
        return false;
    }
    FT_Set_Pixel_Sizes(text_face, 0, 18);

    for (int i = 1; i < GLYPH_COUNT; i++) {
        if (!load_glyph_texture(g_glyph_strings[i], &g_glyph_textures[i])) {
            cleanup_glyphs();
            return false;
        }
    }
    g_glyph_textures[0] = 0;
    return true;
}

void cleanup_glyphs() {
    glDeleteTextures(GLYPH_COUNT, g_glyph_textures);
    if (text_face) FT_Done_Face(text_face);
    if (glyph_face) FT_Done_Face(glyph_face);
    FT_Done_FreeType(ft);
}

// --- Glyph & String Rendering ---
void render_glyph_texture(GLuint texture_id, float x, float y) {
    if (texture_id == 0) return;
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x + TILE_SIZE, y);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x + TILE_SIZE, y + TILE_SIZE);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y + TILE_SIZE);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void render_string(float x, float y, const char* text, float color[3]) {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(color[0], color[1], color[2]);
    glRasterPos2f(x, y);
    for (int i = 0; text[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, text[i]);
    }
}

// --- BFS for enemy pathfinding ---
bool bfs_next_move(int er, int ec, int pr, int pc, int* dr, int* dc) {
    if (er == pr && ec == pc) return false;
    bool visited[GRID_ROWS][GRID_COLS] = {0};
    int parent[GRID_ROWS][GRID_COLS][2] = {{{-1}}}; // parent row, col
    int queue[MAX_QUEUE];
    int front = 0, rear = 0;
    int dirs[4][2] = {{0,1},{1,0},{0,-1},{-1,0}};

    queue[rear++] = er * GRID_COLS + ec;
    visited[er][ec] = true;

    bool found = false;
    while (front < rear) {
        int idx = queue[front++];
        int cr = idx / GRID_COLS, cc = idx % GRID_COLS;
        if (cr == pr && cc == pc) {
            found = true;
            break;
        }
        for (int d = 0; d < 4; d++) {
            int nr = cr + dirs[d][0], nc = cc + dirs[d][1];
            if (is_valid_tile(nr, nc) && !visited[nr][nc] && tiles[nr][nc] == TILE_EMPTY && !enemy_on_pos(nr, nc)) {
                visited[nr][nc] = true;
                parent[nr][nc][0] = cr;
                parent[nr][nc][1] = cc;
                queue[rear++] = nr * GRID_COLS + nc;
                if (nr == pr && nc == pc) {
                    found = true;
                    break;
                }
            }
        }
        if (found) break;
    }

    if (!found) return false;

    // Backtrack to find first move
    int curr_r = pr, curr_c = pc;
    while (parent[curr_r][curr_c][0] != er || parent[curr_r][curr_c][1] != ec) {
        int prev_r = parent[curr_r][curr_c][0];
        int prev_c = parent[curr_r][curr_c][1];
        curr_r = prev_r;
        curr_c = prev_c;
    }
    *dr = curr_r - er;
    *dc = curr_c - ec;
    return true;
}

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
    set_status_message("使用箭头键移动，空格键放置炸弹！", "(Use arrow keys to move, Space to place bomb!)");
}

int count_active_bombs() {
    int cnt = 0;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) cnt++;
    }
    return cnt;
}

void place_bomb(int r, int c) {
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
            int dr = 0, dc = 0;
            if (bfs_next_move(enemies[i].row, enemies[i].col, player_row, player_col, &dr, &dc)) {
                int nr = enemies[i].row + dr, nc = enemies[i].col + dc;
                if (is_walkable(nr, nc) && !enemy_on_pos(nr, nc)) {
                    enemies[i].row = nr;
                    enemies[i].col = nc;
                }
            } else {
                // Random move if no path
                int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};
                int d = rand() % 4;
                int nr = enemies[i].row + dirs[d][0];
                int nc = enemies[i].col + dirs[d][1];
                if (is_walkable(nr, nc) && !enemy_on_pos(nr, nc)) {
                    enemies[i].row = nr;
                    enemies[i].col = nc;
                }
            }
            // Check collision with player after move
            if (enemies[i].row == player_row && enemies[i].col == player_col) {
                game_state = GAME_OVER;
                set_status_message("游戏结束！被敌人吃掉。按 'r' 重启。", "GAME OVER! Eaten by enemy. Press 'r' to restart.");
            }
            // Drop bomb occasionally if close to player
            int dist = abs(enemies[i].row - player_row) + abs(enemies[i].col - player_col);
            if (dist <= 2 && rand() % 10 == 0 && count_active_bombs() < 5) { // Limit enemy bombs
                place_bomb(enemies[i].row, enemies[i].col);
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
        char chn[256], eng[256];
        sprintf(chn, "恭喜！进入关卡 %d。继续消灭敌人！", level);
        sprintf(eng, "(Congratulations! Entering level %d. Continue to defeat enemies!)", level);
        set_status_message(chn, eng);
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
        set_status_message("游戏结束！被爆炸波及。按 'r' 重启。", "GAME OVER! Hit by explosion. Press 'r' to restart.");
    }
}

// --- Input Handlers ---
void keyboard(unsigned char key, int x, int y) {
    if (game_state == PLAYING && key == ' ') {
        if (count_active_bombs() < player_max_bombs) {
            place_bomb(player_row, player_col);
        }
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
                char chn[256], eng[256];
                if (items[i].type == 0) {
                    player_max_bombs++;
                    sprintf(chn, "获得额外炸弹！最大炸弹数：%d (Bomb - 炸弹)", player_max_bombs);
                    sprintf(eng, "(Got extra bomb! Max bombs: %d)", player_max_bombs);
                } else {
                    player_flame_power++;
                    sprintf(chn, "获得火焰升级！火焰范围：%d (Flame - 火焰)", player_flame_power);
                    sprintf(eng, "(Got flame upgrade! Flame range: %d)", player_flame_power);
                }
                set_status_message(chn, eng);
                items[i].active = false;
            }
        }
        // Check enemy collision
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active && enemies[i].row == nr && enemies[i].col == nc) {
                game_state = GAME_OVER;
                set_status_message("游戏结束！碰到敌人。按 'r' 重启。", "GAME OVER! Collided with enemy. Press 'r' to restart.");
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

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float white[3] = {1.0f, 1.0f, 1.0f};
    float black[3] = {0.0f, 0.0f, 0.0f};

    // === GAME GRID ===
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = c * TILE_SIZE;
            float y = UI_HEIGHT + (GRID_ROWS - r - 1) * TILE_SIZE;
            float bg_color[3];
            switch (tiles[r][c]) {
                case TILE_WALL_SOLID:
                    bg_color[0] = 0.5f; bg_color[1] = 0.5f; bg_color[2] = 0.5f;
                    break;
                case TILE_WALL_DESTRUCTIBLE:
                    bg_color[0] = 0.7f; bg_color[1] = 0.4f; bg_color[2] = 0.2f;
                    break;
                default:
                    bg_color[0] = 0.2f; bg_color[1] = 0.2f; bg_color[2] = 0.2f;
                    break;
            }
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, bg_color);
            if (tiles[r][c] != TILE_EMPTY) {
                render_glyph_texture(g_glyph_textures[tiles[r][c]], x, y);
            }
        }
    }

    // === DYNAMIC OBJECTS ===
    float px = player_col * TILE_SIZE;
    float py = UI_HEIGHT + (GRID_ROWS - player_row - 1) * TILE_SIZE;
    if (game_state == PLAYING) {
        float cyan[3] = {0.0f, 1.0f, 1.0f};
        draw_rect(px, py, TILE_SIZE, TILE_SIZE, cyan);
        render_glyph_texture(g_glyph_textures[3], px, py);
    }
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            float bx = bombs[i].col * TILE_SIZE;
            float by = UI_HEIGHT + (GRID_ROWS - bombs[i].row - 1) * TILE_SIZE;
            float magenta[3] = {1.0f, 0.0f, 1.0f};
            draw_rect(bx, by, TILE_SIZE, TILE_SIZE, magenta);
            render_glyph_texture(g_glyph_textures[4], bx, by);
        }
    }
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            float ex = explosions[i].col * TILE_SIZE;
            float ey = UI_HEIGHT + (GRID_ROWS - explosions[i].row - 1) * TILE_SIZE;
            float yellow[3] = {1.0f, 1.0f, 0.0f};
            draw_rect(ex, ey, TILE_SIZE, TILE_SIZE, yellow);
            render_glyph_texture(g_glyph_textures[5], ex, ey);
        }
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            float ex = enemies[i].col * TILE_SIZE;
            float ey = UI_HEIGHT + (GRID_ROWS - enemies[i].row - 1) * TILE_SIZE;
            float red[3] = {1.0f, 0.0f, 0.0f};
            draw_rect(ex, ey, TILE_SIZE, TILE_SIZE, red);
            render_glyph_texture(g_glyph_textures[6], ex, ey);
        }
    }
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            float ix = items[i].col * TILE_SIZE + 8;
            float iy = UI_HEIGHT + (GRID_ROWS - items[i].row - 1) * TILE_SIZE + 8;
            float icol[3];
            if (items[i].type == 0) {
                icol[0] = 0.0f; icol[1] = 0.0f; icol[2] = 1.0f;
            } else {
                icol[0] = 1.0f; icol[1] = 0.5f; icol[2] = 0.0f;
            }
            draw_rect(ix, iy, TILE_SIZE - 16, TILE_SIZE - 16, icol);
        }
    }

    // === UI PANEL ===
    float panel_color[3] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, UI_HEIGHT, panel_color);
    // Use brighter colors for better contrast against dark background
    float bright_white[3] = {0.9f, 0.9f, 0.9f};
    render_string(10, UI_HEIGHT - 20, "炸弹人 (Bomberman)", bright_white);
    render_string(10, UI_HEIGHT - 50, status_message, bright_white);
    render_string(10, UI_HEIGHT - 70, english_status, bright_white);
    char chn_stats[100], eng_stats[100];
    sprintf(chn_stats, "关卡: %d 分数: %d 炸弹: %d 火焰: %d", level, score, player_max_bombs, player_flame_power);
    sprintf(eng_stats, "(Level: %d Score: %d Bombs: %d Flame: %d)", level, score, player_max_bombs, player_flame_power);
    render_string(10, UI_HEIGHT - 100, chn_stats, bright_white);
    render_string(10, UI_HEIGHT - 120, eng_stats, bright_white);
    glutSwapBuffers();
}

// --- Save/Load ---
void set_status_message(const char* chn_format, const char* eng_format, ...) {
    va_list args;
    va_start(args, eng_format);
    vsnprintf(status_message, sizeof(status_message), chn_format, args);
    vsnprintf(english_status, sizeof(english_status), eng_format, args);
    va_end(args);
}

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
    set_status_message("游戏已保存到 save.csv", "(Game saved to save.csv)");
}

void load_game() {
    FILE* f = fopen("save.csv", "r");
    if (!f) {
        set_status_message("无保存文件。", "(No save file found.)");
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
    set_status_message("游戏已从 save.csv 加载", "(Game loaded from save.csv)");
}

// --- Main & Init ---
void reshape(int w, int h) { glViewport(0, 0, w, h); window_width = w; window_height = h; }
void game_loop(int value) { update_game(); glutPostRedisplay(); glutTimerFunc(16, game_loop, 0); }

void init() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    if (!init_glyphs()) {
        printf("Failed to load Chinese font. Please install fonts-noto-cjk.\n");
        exit(1);
    }
    printf("Chinese font loaded successfully.\n");
    restart_game();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("炸弹人 (Bomberman)");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    
    init();
    create_menu(); // Create the right-click menu
    glutTimerFunc(0, game_loop, 0);

    printf("Controls:\n - Arrow keys: Move player\n - Space: Place bomb\n - R: Restart game\n - S: Save game\n - L: Load game\n - ESC: Exit\n");
    atexit(cleanup_glyphs);

    glutMainLoop();
    return 0;
}


// --- Menu System ---
int file_menu_id, main_menu_id;

void menu_callback(int option) {
    switch(option) {
        case 1: // Save game
            save_game();
            break;
        case 2: // Load game
            load_game();
            break;
        case 3: // Restart game
            restart_game();
            break;
        case 4: // Exit
            exit(0);
            break;
    }
    glutPostRedisplay();
}

void create_menu() {
    file_menu_id = glutCreateMenu(menu_callback);
    glutAddMenuEntry("保存游戏 (S)", 1);
    glutAddMenuEntry("加载游戏 (L)", 2);
    
    main_menu_id = glutCreateMenu(menu_callback);
    glutAddSubMenu("文件 (File)", file_menu_id);
    glutAddMenuEntry("重新开始 (R)", 3);
    glutAddMenuEntry("退出 (ESC)", 4);
    
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}
