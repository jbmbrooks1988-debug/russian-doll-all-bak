#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdarg.h>

// --- Constants ---
#define MAP_ROWS 50
#define MAP_COLS 50
#define VIEW_ROWS 12
#define VIEW_COLS 12
#define MAX_ENEMIES 50
#define MAX_ITEMS 10
#define MAX_PROJECTILES 10
#define MAX_INVENTORY 10
#define VIEW_RANGE 5
#define CAVE_ROWS 20
#define CAVE_COLS 20
#define SLASH_DURATION 30

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
const char* EMOJI_CAVE_ENTRANCE = "🕳️";
const char* EMOJI_CAVE_EXIT = "🚪";
const char* EMOJI_ARROW = "🏹";
const char* EMOJI_SLASH = "⚔️";

// Terrain types
#define TERRAIN_OCEAN    0
#define TERRAIN_GRASS    1
#define TERRAIN_FOREST   2
#define TERRAIN_MOUNTAIN 3
#define TERRAIN_RIVER    4
#define TERRAIN_CAVE_ENTRANCE 5
#define TERRAIN_CAVE_EXIT 6
#define TERRAIN_WALL 7

// Item types
#define ITEM_POTION 1

// Game state
enum GameState { OVERWORLD, CAVE };
enum GameState state = OVERWORLD;

int level = 1;
int hero_row = -1, hero_col = -1;
int hero_hp = 20, hero_max_hp = 20;
int hero_atk = 5, hero_def = 2;
int hero_exp = 0, hero_next_exp = 10;
int hero_inventory[MAX_INVENTORY] = {0};
int hero_inventory_count = 0;
int facing = 0; // 0: down, 1: up, 2: left, 3: right

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

struct Projectile {
    int row, col;
    int dr, dc;
    int active;
};
struct Projectile projectiles[MAX_PROJECTILES];

int camera_row = 0, camera_col = 0;

char status_message[256] = "Welcome! Explore the CLI roguelike world.";

// --- Map & Fog ---
int terrain_overworld[MAP_ROWS][MAP_COLS];
int terrain_cave[CAVE_ROWS][CAVE_COLS];
bool explored_overworld[MAP_ROWS][MAP_COLS] = {0};
bool explored_cave[CAVE_ROWS][CAVE_COLS] = {0};

// --- Slash Timer ---
int slash_timer = 0;
int slash_row, slash_col;

// --- Function Prototypes ---
void setup_terminal();
void restore_terminal();
char get_input();
void display();
void generate_world(int terrain[MAP_ROWS][MAP_COLS], int rows, int cols, bool is_cave);
bool is_valid_tile(int r, int c, int rows, int cols);
bool is_land(int r, int c, int terrain[][MAP_COLS], int rows, int cols);
void reveal_around(int r, int c, int radius, bool explored[][MAP_COLS], int rows, int cols);
void set_status_message(const char* format, ...);
void process_turn();
void update_enemies();
void update_projectiles();
void check_collisions();
void level_up();
void generate_new_cave();
void enter_cave();
void exit_cave();
int find_enemy_at(int r, int c);
int find_item_at(int r, int c);
void pickup_item(int item_index);
void use_potion();
void slash_attack();
void shoot_arrow();
int manhattan_dist(int r1, int c1, int r2, int c2);

// --- Terminal Setup ---
struct termios orig_termios;

void setup_terminal() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// --- Input Handling ---
char get_input() {
    char c = getchar();
    if (c == '\033') { // Escape sequence
        c = getchar();
        if (c == '[') {
            c = getchar();
            switch (c) {
                case 'A': return 'w'; // Up
                case 'B': return 's'; // Down
                case 'C': return 'd'; // Right
                case 'D': return 'a'; // Left
            }
        }
        return 0; // Invalid escape sequence
    }
    return c;
}

// --- World Generation ---
void generate_world(int terrain[][MAP_COLS], int rows, int cols, bool is_cave) {
    srand(time(NULL));
    if (!is_cave) {
        float radius_sq = (rows / 3.0f) * (cols / 3.0f) * 2.5f;
        int center_r = rows / 2;
        int center_c = cols / 2;

        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
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
            }
        }

        for (int i = 0; i < 5; i++) {
            int r = rand() % rows;
            int c = rand() % cols;
            if (terrain[r][c] == TERRAIN_GRASS) terrain[r][c] = TERRAIN_CAVE_ENTRANCE;
        }

        hero_row = center_r;
        hero_col = center_c;
        terrain[hero_row][hero_col] = TERRAIN_GRASS;
        reveal_around(hero_row, hero_col, 3, explored_overworld, MAP_ROWS, MAP_COLS);
    } else {
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                terrain[r][c] = (rand() % 100 < 45) ? TERRAIN_WALL : TERRAIN_GRASS;
            }
        }

        for (int iter = 0; iter < 5; iter++) {
            int temp[CAVE_ROWS][CAVE_COLS];
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) {
                    int walls = 0;
                    for (int dr = -1; dr <= 1; dr++) {
                        for (int dc = -1; dc <= 1; dc++) {
                            if (dr == 0 && dc == 0) continue;
                            int nr = r + dr, nc = c + dc;
                            if (!is_valid_tile(nr, nc, rows, cols) || terrain[nr][nc] == TERRAIN_WALL) walls++;
                        }
                    }
                    temp[r][c] = (walls > 4) ? TERRAIN_WALL : TERRAIN_GRASS;
                }
            }
            memcpy(terrain, temp, sizeof(temp));
        }

        int exit_r = rand() % rows, exit_c = rand() % cols;
        while (terrain[exit_r][exit_c] == TERRAIN_WALL) {
            exit_r = rand() % rows;
            exit_c = rand() % cols;
        }
        terrain[exit_r][exit_c] = TERRAIN_CAVE_EXIT;

        hero_row = rand() % rows;
        hero_col = rand() % cols;
        while (terrain[hero_row][hero_col] == TERRAIN_WALL) {
            hero_row = rand() % rows;
            hero_col = rand() % cols;
        }
        reveal_around(hero_row, hero_col, 3, explored_cave, CAVE_ROWS, CAVE_COLS);
    }

    int num_enemies = is_cave ? 10 + level : 20 + level * 2;
    if (num_enemies > MAX_ENEMIES) num_enemies = MAX_ENEMIES;
    for (int i = 0; i < num_enemies; i++) {
        int r = rand() % rows;
        int c = rand() % cols;
        while (!is_land(r, c, terrain, rows, cols) || manhattan_dist(r, c, hero_row, hero_col) < 5) {
            r = rand() % rows;
            c = rand() % cols;
        }
        enemies[i].row = r;
        enemies[i].col = c;
        enemies[i].max_hp = 10 + level * 2;
        enemies[i].hp = enemies[i].max_hp;
        enemies[i].atk = 3 + level;
        enemies[i].def = 1 + level / 3;
        enemies[i].active = 1;
    }

    int num_items = is_cave ? 3 : 5 + level / 3;
    if (num_items > MAX_ITEMS) num_items = MAX_ITEMS;
    for (int i = 0; i < num_items; i++) {
        int r = rand() % rows;
        int c = rand() % cols;
        while (!is_land(r, c, terrain, rows, cols) || find_enemy_at(r, c) >= 0 || (r == hero_row && c == hero_col)) {
            r = rand() % rows;
            c = rand() % cols;
        }
        items[i].row = r;
        items[i].col = c;
        items[i].type = ITEM_POTION;
        items[i].active = 1;
    }
}

// --- Game Logic Helpers ---
bool is_valid_tile(int r, int c, int rows, int cols) {
    return r >= 0 && r < rows && c >= 0 && c < cols;
}

bool is_land(int r, int c, int terrain[][MAP_COLS], int rows, int cols) {
    if (!is_valid_tile(r, c, rows, cols)) return false;
    int t = terrain[r][c];
    return t != TERRAIN_OCEAN && t != TERRAIN_MOUNTAIN && t != TERRAIN_WALL;
}

void reveal_around(int r, int c, int radius, bool explored[][MAP_COLS], int rows, int cols) {
    for (int dr = -radius; dr <= radius; dr++) {
        for (int dc = -radius; dc <= radius; dc++) {
            int nr = r + dr, nc = c + dc;
            if (is_valid_tile(nr, nc, rows, cols)) {
                explored[nr][nc] = true;
            }
        }
    }
}

int manhattan_dist(int r1, int c1, int r2, int c2) {
    return abs(r1 - r2) + abs(c1 - c2);
}

int find_enemy_at(int r, int c) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].row == r && enemies[i].col == c) {
            return i;
        }
    }
    return -1;
}

int find_item_at(int r, int c) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active && items[i].row == r && items[i].col == c) {
            return i;
        }
    }
    return -1;
}

void pickup_item(int item_index) {
    if (item_index < 0 || hero_inventory_count >= MAX_INVENTORY) return;
    hero_inventory[hero_inventory_count++] = items[item_index].type;
    items[item_index].active = 0;
    set_status_message("Picked up potion.");
}

void use_potion() {
    for (int i = 0; i < hero_inventory_count; i++) {
        if (hero_inventory[i] == ITEM_POTION) {
            hero_hp += 10;
            if (hero_hp > hero_max_hp) hero_hp = hero_max_hp;
            for (int j = i; j < hero_inventory_count - 1; j++) {
                hero_inventory[j] = hero_inventory[j + 1];
            }
            hero_inventory_count--;
            set_status_message("Used potion. HP restored.");
            return;
        }
    }
    set_status_message("No potion in inventory.");
}

void level_up() {
    hero_exp -= hero_next_exp;
    hero_next_exp *= 2;
    hero_max_hp += 5;
    hero_hp = hero_max_hp;
    hero_atk += 2;
    hero_def += 1;
    set_status_message("Level up! Stats increased.");
}

void generate_new_cave() {
    generate_world(terrain_cave, CAVE_ROWS, CAVE_COLS, true);
}

void enter_cave() {
    state = CAVE;
    generate_new_cave();
    set_status_message("Entered cave.");
}

void exit_cave() {
    state = OVERWORLD;
    set_status_message("Exited cave.");
}

// --- Game Logic ---
void process_turn() {
    int rows = (state == OVERWORLD) ? MAP_ROWS : CAVE_ROWS;
    int cols = (state == OVERWORLD) ? MAP_COLS : CAVE_COLS;
    int (*terrain)[MAP_COLS] = (state == OVERWORLD) ? terrain_overworld : terrain_cave;
    bool (*explored)[MAP_COLS] = (state == OVERWORLD) ? explored_overworld : explored_cave;

    reveal_around(hero_row, hero_col, 3, explored, rows, cols);
    int tr = hero_row, tc = hero_col;
    if (terrain[tr][tc] == TERRAIN_CAVE_ENTRANCE && state == OVERWORLD) enter_cave();
    if (terrain[tr][tc] == TERRAIN_CAVE_EXIT && state == CAVE) exit_cave();
    int item_index = find_item_at(hero_row, hero_col);
    if (item_index >= 0) pickup_item(item_index);

    camera_row = hero_row - VIEW_ROWS / 2;
    camera_col = hero_col - VIEW_COLS / 2;
    if (camera_row < 0) camera_row = 0;
    if (camera_col < 0) camera_col = 0;
    if (camera_row > rows - VIEW_ROWS) camera_row = rows - VIEW_ROWS;
    if (camera_col > cols - VIEW_COLS) camera_col = cols - VIEW_COLS;

    update_enemies();
    update_projectiles();
    check_collisions();
    if (slash_timer > 0) slash_timer--;
}

void update_enemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        int d = manhattan_dist(enemies[i].row, enemies[i].col, hero_row, hero_col);
        if (d <= VIEW_RANGE) {
            int dr = 0, dc = 0;
            if (enemies[i].row < hero_row) dr = 1;
            else if (enemies[i].row > hero_row) dr = -1;
            else if (enemies[i].col < hero_col) dc = 1;
            else if (enemies[i].col > hero_col) dc = -1;

            int rows = (state == OVERWORLD) ? MAP_ROWS : CAVE_ROWS;
            int cols = (state == OVERWORLD) ? MAP_COLS : CAVE_COLS;
            int (*terrain)[MAP_COLS] = (state == OVERWORLD) ? terrain_overworld : terrain_cave;
            int nr = enemies[i].row + dr, nc = enemies[i].col + dc;
            if (is_land(nr, nc, terrain, rows, cols) && find_enemy_at(nr, nc) < 0 && !(nr == hero_row && nc == hero_col)) {
                enemies[i].row = nr;
                enemies[i].col = nc;
            }
        }
    }
}

void update_projectiles() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;
        int nr = projectiles[i].row + projectiles[i].dr;
        int nc = projectiles[i].col + projectiles[i].dc;
        int rows = (state == OVERWORLD) ? MAP_ROWS : CAVE_ROWS;
        int cols = (state == OVERWORLD) ? MAP_COLS : CAVE_COLS;
        int (*terrain)[MAP_COLS] = (state == OVERWORLD) ? terrain_overworld : terrain_cave;
        if (!is_land(nr, nc, terrain, rows, cols)) {
            projectiles[i].active = 0;
            continue;
        }
        projectiles[i].row = nr;
        projectiles[i].col = nc;
        int enemy_index = find_enemy_at(nr, nc);
        if (enemy_index >= 0) {
            enemies[enemy_index].hp -= hero_atk;
            if (enemies[enemy_index].hp <= 0) {
                enemies[enemy_index].active = 0;
                hero_exp += 5 + level * 2;
                if (hero_exp >= hero_next_exp) level_up();
                set_status_message("Enemy defeated by arrow!");
            }
            projectiles[i].active = 0;
        }
    }
}

void check_collisions() {
    int enemy_index = find_enemy_at(hero_row, hero_col);
    if (enemy_index >= 0) {
        hero_hp -= enemies[enemy_index].atk - hero_def;
        if (hero_hp <= 0) {
            set_status_message("Hero defeated! Game over.");
        }
        enemies[enemy_index].hp -= hero_atk - enemies[enemy_index].def;
        if (enemies[enemy_index].hp <= 0) {
            enemies[enemy_index].active = 0;
            hero_exp += 5 + level * 2;
            if (hero_exp >= hero_next_exp) level_up();
            set_status_message("Enemy defeated!");
        }
    }
}

void slash_attack() {
    if (slash_timer > 0) return;
    slash_timer = SLASH_DURATION;
    int dr = 0, dc = 0;
    switch (facing) {
        case 0: dr = 1; break;  // down
        case 1: dr = -1; break; // up
        case 2: dc = -1; break; // left
        case 3: dc = 1; break;  // right
    }
    slash_row = hero_row + dr;
    slash_col = hero_col + dc;
    int enemy_index = find_enemy_at(slash_row, slash_col);
    if (enemy_index >= 0) {
        enemies[enemy_index].hp -= hero_atk - enemies[enemy_index].def;
        if (enemies[enemy_index].hp <= 0) {
            enemies[enemy_index].active = 0;
            hero_exp += 5 + level * 2;
            if (hero_exp >= hero_next_exp) level_up();
            set_status_message("Enemy slashed!");
        }
    }
}

void shoot_arrow() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].row = hero_row;
            projectiles[i].col = hero_col;
            projectiles[i].dr = 0;
            projectiles[i].dc = 0;
            switch (facing) {
                case 0: projectiles[i].dr = 1; break;
                case 1: projectiles[i].dr = -1; break;
                case 2: projectiles[i].dc = -1; break;
                case 3: projectiles[i].dc = 1; break;
            }
            projectiles[i].active = 1;
            break;
        }
    }
}

// --- Display ---
void display() {
    printf("\033[H\033[J"); // Clear screen

    int rows = (state == OVERWORLD) ? MAP_ROWS : CAVE_ROWS;
    int cols = (state == OVERWORLD) ? MAP_COLS : CAVE_COLS;
    int (*terrain)[MAP_COLS] = (state == OVERWORLD) ? terrain_overworld : terrain_cave;
    bool (*explored)[MAP_COLS] = (state == OVERWORLD) ? explored_overworld : explored_cave;

    for (int r = camera_row; r < camera_row + VIEW_ROWS && r < rows; r++) {
        for (int c = camera_col; c < camera_col + VIEW_COLS && c < cols; c++) {
            const char* symbol = EMOJI_FOG;
            if (explored[r][c]) {
                int enemy_index = find_enemy_at(r, c);
                int item_index = find_item_at(r, c);
                if (r == hero_row && c == hero_col) {
                    symbol = EMOJI_HERO;
                } else if (slash_timer > 0 && r == slash_row && c == slash_col) {
                    symbol = EMOJI_SLASH;
                } else if (enemy_index >= 0) {
                    symbol = EMOJI_ENEMY;
                } else if (item_index >= 0) {
                    symbol = EMOJI_POTION;
                } else {
                    switch (terrain[r][c]) {
                        case TERRAIN_GRASS: symbol = EMOJI_LAND; break;
                        case TERRAIN_FOREST: symbol = EMOJI_FOREST; break;
                        case TERRAIN_MOUNTAIN: symbol = EMOJI_MOUNTAIN; break;
                        case TERRAIN_RIVER: symbol = EMOJI_RIVER; break;
                        case TERRAIN_OCEAN: symbol = EMOJI_OCEAN; break;
                        case TERRAIN_CAVE_ENTRANCE: symbol = EMOJI_CAVE_ENTRANCE; break;
                        case TERRAIN_CAVE_EXIT: symbol = EMOJI_CAVE_EXIT; break;
                        case TERRAIN_WALL: symbol = "🪨"; break;
                    }
                }
            }
            printf("%s ", symbol);
        }
        printf("\n");
    }

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
            int pr = projectiles[i].row - camera_row;
            int pc = projectiles[i].col - camera_col;
            if (pr >= 0 && pr < VIEW_ROWS && pc >= 0 && pc < VIEW_COLS) {
                printf("\033[%d;%dH%s", pr + 1, pc * 2 + 1, EMOJI_ARROW);
            }
        }
    }

    printf("\n");
    printf("Level: %d | HP: %d/%d | ATK: %d | DEF: %d | EXP: %d/%d | Potions: %d\n",
           level, hero_hp, hero_max_hp, hero_atk, hero_def, hero_exp, hero_next_exp, hero_inventory_count);
    printf("WASD/Arrows: Move | Space: Slash | B: Shoot Arrow | P: Potion\n");
    printf("%s\n", status_message);
}

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
    generate_world(terrain_overworld, MAP_ROWS, MAP_COLS, false);
    memset(projectiles, 0, sizeof(projectiles));
}

int main() {
    setup_terminal();
    init();

    printf("Controls:\n");
    printf(" - WASD or Arrow Keys: Move one tile\n");
    printf(" - Space: Sword slash\n");
    printf(" - B: Shoot arrow\n");
    printf(" - P: Use potion\n");
    printf("Press any key to start...\n");
    getchar();

    while (1) {
        display();
        char input = get_input();
        bool turn_taken = false;
        int dr = 0, dc = 0;
        switch (input) {
            case 'w': case 'W':
                dr = -1; facing = 1; turn_taken = true;
                set_status_message("Moving up");
                break;
            case 's': case 'S':
                dr = 1; facing = 0; turn_taken = true;
                set_status_message("Moving down");
                break;
            case 'a': case 'A':
                dc = -1; facing = 2; turn_taken = true;
                set_status_message("Moving left");
                break;
            case 'd': case 'D':
                dc = 1; facing = 3; turn_taken = true;
                set_status_message("Moving right");
                break;
            case 'p': case 'P':
                use_potion();
                turn_taken = true;
                break;
            case ' ':
                slash_attack();
                turn_taken = true;
                break;
            case 'b': case 'B':
                shoot_arrow();
                turn_taken = true;
                break;
            case 'q': case 'Q':
                restore_terminal();
                printf("\033[H\033[J");
                printf("Game exited.\n");
                return 0;
        }
        if (turn_taken) {
            int rows = (state == OVERWORLD) ? MAP_ROWS : CAVE_ROWS;
            int cols = (state == OVERWORLD) ? MAP_COLS : CAVE_COLS;
            int (*terrain)[MAP_COLS] = (state == OVERWORLD) ? terrain_overworld : terrain_cave;
            int nr = hero_row + dr, nc = hero_col + dc;
            if (is_land(nr, nc, terrain, rows, cols)) {
                hero_row = nr;
                hero_col = nc;
            }
            process_turn();
        }
        if (hero_hp <= 0) {
            display();
            restore_terminal();
            printf("\033[H\033[J");
            printf("Game over! Hero defeated.\n");
            return 0;
        }
    }

    restore_terminal();
    return 0;
}
