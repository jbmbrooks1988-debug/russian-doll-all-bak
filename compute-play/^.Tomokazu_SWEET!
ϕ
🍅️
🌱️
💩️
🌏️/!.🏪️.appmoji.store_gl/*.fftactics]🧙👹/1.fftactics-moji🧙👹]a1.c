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
#define MAX_ENEMIES 10
#define MAX_ITEMS 5
#define MAX_INVENTORY 10

// Emojis
const char* EMOJI_OCEAN = "🌊";
const char* EMOJI_LAND = "🟩";
const char* EMOJI_FOREST = "🌲";
const char* EMOJI_MOUNTAIN = "⛰️";
const char* EMOJI_RIVER = "💧";
const char* EMOJI_HERO = "🧙";
const char* EMOJI_ENEMY = "👹";
const char* EMOJI_POTION = "🧪";
const char* EMOJI_FOG = "🌫️";

// Terrain types
#define TERRAIN_OCEAN    0
#define TERRAIN_GRASS    1
#define TERRAIN_FOREST   2
#define TERRAIN_MOUNTAIN 3
#define TERRAIN_RIVER    4

// Item types
#define ITEM_POTION 1

// Game state
int level = 1;
int hero_row = -1, hero_col = -1;
int hero_hp = 20, hero_max_hp = 20;
int hero_atk = 5, hero_def = 2;
int hero_exp = 0, hero_next_exp = 10;
int hero_inventory[MAX_INVENTORY] = {0};
int hero_inventory_count = 0;

struct Enemy {
    int row, col;
    int hp, max_hp;
    int atk, def;
    int active;
};
struct Enemy enemies[MAX_ENEMIES];

struct Item {
    int row, col;
    int type;
    int active;
};
struct Item items[MAX_ITEMS];

int window_width = GRID_COLS * TILE_SIZE + 20;
int window_height = GRID_ROWS * TILE_SIZE + 100;  // Menu + UI

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.05f, 0.05f, 0.1f, 1.0f};

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
bool hero_selected = false;

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
void draw_line_rect(float x, float y, float w, float h, float color[3]);
void generate_world();
bool is_valid_tile(int r, int c);
bool is_land(int r, int c);
void reveal_around(int r, int c, int radius);
void end_turn();
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void set_status_message(const char* format, ...);
void move_hero(int row, int col);
void attack_enemy(int enemy_index);
void use_potion();
void pickup_item(int item_index);
void level_up();
void generate_new_level();
void place_enemies_and_items();
void enemy_turn();
int find_enemy_at(int row, int col);
int find_item_at(int row, int col);
bool all_enemies_defeated();

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

    // Find safe spawn for hero
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
                hero_row = r;
                hero_col = c;
                reveal_around(r, c, 3);
                cursor_row = r;
                cursor_col = c;
                set_status_message("Hero ready! Use Enter to select.");
                found = true;
            }
        }
    }

    if (!found) {
        hero_row = center_r;
        hero_col = center_c;
        terrain[hero_row][hero_col] = TERRAIN_GRASS;
        reveal_around(hero_row, hero_col, 3);
        set_status_message("Fallback spawn. Ready.");
    }

    place_enemies_and_items();
}

void place_enemies_and_items() {
    // Clear existing
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;
    for (int i = 0; i < MAX_ITEMS; i++) items[i].active = 0;

    // Place enemies
    int num_enemies = 3 + level / 2; // Increase with level
    if (num_enemies > MAX_ENEMIES) num_enemies = MAX_ENEMIES;
    for (int i = 0; i < num_enemies; i++) {
        int r, c;
        do {
            r = rand() % GRID_ROWS;
            c = rand() % GRID_COLS;
        } while (!is_land(r, c) || (abs(r - hero_row) + abs(c - hero_col) < 3));

        enemies[i].row = r;
        enemies[i].col = c;
        enemies[i].max_hp = 10 + level * 2;
        enemies[i].hp = enemies[i].max_hp;
        enemies[i].atk = 3 + level;
        enemies[i].def = 1 + level / 3;
        enemies[i].active = 1;
    }

    // Place items
    int num_items = 1 + level / 3;
    if (num_items > MAX_ITEMS) num_items = MAX_ITEMS;
    for (int i = 0; i < num_items; i++) {
        int r, c;
        do {
            r = rand() % GRID_ROWS;
            c = rand() % GRID_COLS;
        } while (!is_land(r, c) || find_enemy_at(r, c) >= 0 || (r == hero_row && c == hero_col));

        items[i].row = r;
        items[i].col = c;
        items[i].type = ITEM_POTION;
        items[i].active = 1;
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
    enemy_turn();
    reveal_around(hero_row, hero_col, 1);
    hero_selected = false;
    if (all_enemies_defeated()) {
        set_status_message("Level cleared! Press 'N' for next level.");
    } else {
        set_status_message("Turn ended. Enemies move.");
    }
    glutPostRedisplay();
}

void move_hero(int row, int col) {
    if (!is_valid_tile(row, col)) return;
    if (terrain[row][col] == TERRAIN_OCEAN || terrain[row][col] == TERRAIN_MOUNTAIN) {
        set_status_message("Can't move to that terrain!");
        return;
    }
    if (find_enemy_at(row, col) >= 0) {
        set_status_message("Enemy there! Attack instead.");
        return;
    }

    hero_row = row;
    hero_col = col;
    reveal_around(row, col, 1);

    int item_index = find_item_at(row, col);
    if (item_index >= 0) {
        pickup_item(item_index);
    }

    hero_selected = false;
    glutPostRedisplay();
}

void attack_enemy(int enemy_index) {
    if (enemy_index < 0) return;

    struct Enemy *e = &enemies[enemy_index];
    int dmg_to_enemy = hero_atk - e->def;
    if (dmg_to_enemy < 1) dmg_to_enemy = 1;
    e->hp -= dmg_to_enemy;

    if (e->hp <= 0) {
        e->active = 0;
        hero_exp += 5 + level * 2;
        if (hero_exp >= hero_next_exp) {
            level_up();
        }
        set_status_message("Enemy defeated! Gained exp.");
        // Chance to drop item
        if (rand() % 100 < 20 && hero_inventory_count < MAX_INVENTORY) {
            hero_inventory[hero_inventory_count++] = ITEM_POTION;
            set_status_message("Enemy dropped a potion!");
        }
    } else {
        int dmg_to_hero = e->atk - hero_def;
        if (dmg_to_hero < 1) dmg_to_hero = 1;
        hero_hp -= dmg_to_hero;
        set_status_message("Attacked enemy. Hero HP: %d", hero_hp);
        if (hero_hp <= 0) {
            set_status_message("Hero defeated! Game over.");
            // TODO: Game over logic
        }
    }
    hero_selected = false;
    glutPostRedisplay();
}

void use_potion() {
    for (int i = 0; i < hero_inventory_count; i++) {
        if (hero_inventory[i] == ITEM_POTION) {
            hero_hp += 10;
            if (hero_hp > hero_max_hp) hero_hp = hero_max_hp;
            // Remove from inventory
            for (int j = i; j < hero_inventory_count - 1; j++) {
                hero_inventory[j] = hero_inventory[j + 1];
            }
            hero_inventory_count--;
            set_status_message("Used potion. HP restored.");
            hero_selected = false;
            glutPostRedisplay();
            return;
        }
    }
    set_status_message("No potion in inventory.");
}

void pickup_item(int item_index) {
    if (item_index < 0 || hero_inventory_count >= MAX_INVENTORY) return;

    struct Item *it = &items[item_index];
    hero_inventory[hero_inventory_count++] = it->type;
    it->active = 0;
    set_status_message("Picked up potion.");
    glutPostRedisplay();
}

void level_up() {
    hero_exp -= hero_next_exp;
    hero_next_exp *= 2; // Exponential growth
    hero_max_hp += 5;
    hero_hp = hero_max_hp;
    hero_atk += 2;
    hero_def += 1;
    set_status_message("Level up! Stats increased.");
}

void generate_new_level() {
    level++;
    generate_world(); // Regenerate map
    hero_hp = hero_max_hp; // Full heal
    set_status_message("Entered level %d.", level);
    glutPostRedisplay();
}

void enemy_turn() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        struct Enemy *e = &enemies[i];
        int dr = hero_row - e->row;
        int dc = hero_col - e->col;
        int dist = abs(dr) + abs(dc);

        if (dist == 1) {
            // Attack hero
            int dmg = e->atk - hero_def;
            if (dmg < 1) dmg = 1;
            hero_hp -= dmg;
            set_status_message("Enemy attacked! Hero HP: %d", hero_hp);
            if (hero_hp <= 0) {
                set_status_message("Hero defeated! Game over.");
            }
        } else if (dist <= 3) { // Move towards hero if in sight
            int nr = e->row + (dr > 0 ? 1 : (dr < 0 ? -1 : 0));
            int nc = e->col + (dc > 0 ? 1 : (dc < 0 ? -1 : 0));
            if (is_land(nr, nc) && (nr != hero_row || nc != hero_col) && find_enemy_at(nr, nc) < 0) {
                e->row = nr;
                e->col = nc;
            }
        }
    }
}

int find_enemy_at(int row, int col) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].row == row && enemies[i].col == col) {
            return i;
        }
    }
    return -1;
}

int find_item_at(int row, int col) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active && items[i].row == row && items[i].col == col) {
            return i;
        }
    }
    return -1;
}

bool all_enemies_defeated() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) return false;
    }
    return true;
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

void draw_line_rect(float x, float y, float w, float h, float color[3]) {
    glLineWidth(4.0f);
    glColor3fv(color);
    glBegin(GL_LINE_LOOP);
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

            int enemy_index = find_enemy_at(r, c);
            int item_index = find_item_at(r, c);
            bool has_hero = (hero_row == r && hero_col == c);

            if (debug_fog_enabled && !explored[r][c]) {
                draw_rect(x, y, TILE_SIZE, TILE_SIZE, fog_color);
                unsigned int code;
                decode_utf8((const unsigned char*)EMOJI_FOG, &code);
                render_emoji(code, cx, cy);
            } else {
                if (debug_terrain_enabled) {
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
                    unsigned int code;
                    decode_utf8((const unsigned char*)emoji, &code);
                    render_emoji(code, cx, cy);
                } else {
                    draw_rect(x, y, TILE_SIZE, TILE_SIZE, (float[]){0.3f, 0.3f, 0.3f});
                }

                // Render item if present
                if (item_index >= 0) {
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_POTION, &code);
                    render_emoji(code, cx, cy - 10); // Slightly offset
                }

                // Render enemy or hero
                if (enemy_index >= 0) {
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_ENEMY, &code);
                    render_emoji(code, cx, cy);
                } else if (has_hero) {
                    unsigned int code;
                    decode_utf8((const unsigned char*)EMOJI_HERO, &code);
                    render_emoji(code, cx, cy);
                }
            }
        }
    }

    // Highlight attack range when hero is selected
    if (hero_selected && debug_show_cursor) {
        float attack_color[3] = {1.0f, 0.0f, 0.0f}; // Red for attack range
        int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}; // Adjacent tiles
        for (int i = 0; i < 4; i++) {
            int r = hero_row + directions[i][0];
            int c = hero_col + directions[i][1];
            if (is_valid_tile(r, c) && is_land(r, c)) {
                float x = 10 + c * TILE_SIZE;
                float y = tile_area_top - (r + 1) * TILE_SIZE;
                draw_line_rect(x, y, TILE_SIZE, TILE_SIZE, attack_color);
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
    char ui[256];
    snprintf(ui, sizeof(ui), "Level: %d | HP: %d/%d | ATK: %d | DEF: %d | EXP: %d/%d | Potions: %d", 
             level, hero_hp, hero_max_hp, hero_atk, hero_def, hero_exp, hero_next_exp, hero_inventory_count);
    render_text(ui, 10, 60);
    render_text("A: Attack | P: Use Potion | Space: End Turn | N: Next Level (when cleared)", 10, 30);
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

        if (row == hero_row && col == hero_col) {
            set_status_message("🧙 Hero selected. Move and press Enter to act.");
            hero_selected = true;
        } else if (hero_selected) {
            int dr = abs(row - hero_row);
            int dc = abs(col - hero_col);
            if (dr + dc == 1) {
                move_hero(row, col);
            } else {
                set_status_message("❌ Not adjacent!");
            }
        } else {
            set_status_message("Move cursor to hero and press Enter to select.");
        }
        glutPostRedisplay();
    }
}

// --- Input: Keyboard ---
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 13:  // Enter key
            if (!hero_selected && cursor_row == hero_row && cursor_col == hero_col) {
                hero_selected = true;
                set_status_message("✅ Hero selected. Move cursor and press Enter to move.");
            } else if (hero_selected) {
                int dr = abs(cursor_row - hero_row);
                int dc = abs(cursor_col - hero_col);
                if (dr + dc == 1 && is_land(cursor_row, cursor_col)) {
                    move_hero(cursor_row, cursor_col);
                } else if (cursor_row == hero_row && cursor_col == hero_col) {
                    set_status_message("Hero already here. Move and press Enter.");
                } else {
                    set_status_message("❌ Can't move there. Must be adjacent and valid.");
                }
            } else {
                set_status_message("Go to hero and press Enter to select.");
            }
            break;

        case 'a': case 'A':
            if (hero_selected) {
                int dr = abs(cursor_row - hero_row);
                int dc = abs(cursor_col - hero_col);
                if (dr + dc == 1) {
                    int enemy_index = find_enemy_at(cursor_row, cursor_col);
                    if (enemy_index >= 0) {
                        attack_enemy(enemy_index);
                    } else {
                        set_status_message("No enemy there.");
                    }
                } else {
                    set_status_message("Must be adjacent to attack.");
                }
            }
            break;

        case 'p': case 'P':
            if (hero_selected) {
                use_potion();
            }
            break;

        case 'n': case 'N':
            if (all_enemies_defeated()) {
                generate_new_level();
            } else {
                set_status_message("Clear all enemies first.");
            }
            break;

        case ' ':
            end_turn();
            break;

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



int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Tactics Battle: Emoji Edition");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();

    printf("Controls:\n");
    printf(" - Enter: Select hero, then move & Enter to move\n");
    printf(" - A: Attack adjacent enemy (red tiles show attack range)\n");
    printf(" - P: Use potion\n");
    printf(" - Space: End turn (enemies move/attack)\n");
    printf(" - N: Next level (after clearing enemies)\n");
    printf(" - Arrow keys: Move cursor\n");
    printf(" - G/T/K: Debug toggles\n");

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    return 0;
}
