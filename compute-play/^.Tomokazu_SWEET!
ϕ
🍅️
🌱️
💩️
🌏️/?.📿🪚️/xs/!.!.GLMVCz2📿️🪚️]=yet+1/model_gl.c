#include <GL/glut.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MAP_ROWS 50
#define MAP_COLS 50
#define CAVE_ROWS 20
#define CAVE_COLS 20
#define TILE_SIZE 40
#define VIEW_ROWS 12
#define VIEW_COLS 12
#define MAX_ENTITIES 61 // Hero + 50 enemies + 10 projectiles
#define VIEW_RANGE 5
#define SLASH_DURATION 30

#define TYPE_OCEAN 0
#define TYPE_GRASS 1
#define TYPE_FOREST 2
#define TYPE_MOUNTAIN 3
#define TYPE_RIVER 4
#define TYPE_CAVE_ENTRANCE 5
#define TYPE_CAVE_EXIT 6
#define TYPE_WALL 7
#define TYPE_HERO 8
#define TYPE_ENEMY 9
#define TYPE_POTION 10
#define TYPE_ARROW 11
#define TYPE_SLASH 12

static const char* EMOJI_OCEAN = "🌊";
static const char* EMOJI_LAND = "🟩";
static const char* EMOJI_FOREST = "🌲";
static const char* EMOJI_MOUNTAIN = "⛰️";
static const char* EMOJI_RIVER = "💧";
static const char* EMOJI_HERO = "🧙";
static const char* EMOJI_ENEMY = "👹";
static const char* EMOJI_POTION = "🧪";
static const char* EMOJI_CAVE_ENTRANCE = "🕳️";
static const char* EMOJI_CAVE_EXIT = "🚪";
static const char* EMOJI_ARROW = "🏹";
static const char* EMOJI_SLASH = "⚔️";

enum GameState { OVERWORLD, CAVE };
static enum GameState state = OVERWORLD;

static int grid[MAP_ROWS][MAP_COLS];
static int rows = MAP_ROWS, cols = MAP_COLS;
static struct {
    float x, y, z;
    int type;
    int hp, max_hp, atk, def;
    int active;
    int dr, dc; // For projectiles
} entities[MAX_ENTITIES];
static int num_entities = 0;
static int hero_index = 0;
static int hero_inventory[10] = {0};
static int hero_inventory_count = 0;
static int hero_exp = 0, hero_next_exp = 10;
static int level = 1;
static int facing = 0; // 0: down, 1: up, 2: left, 3: right
static char status_message[256] = "Welcome! Explore the roguelike world.";
static float camera_x = 0, camera_y = 0, camera_z = 0;
static float camera_pitch = 0, camera_yaw = 0, camera_roll = 0;

static void generate_world(bool is_cave) {
    srand(time(NULL));
    rows = is_cave ? CAVE_ROWS : MAP_ROWS;
    cols = is_cave ? CAVE_COLS : MAP_COLS;
    memset(grid, 0, sizeof(grid));
    num_entities = 0;

    if (!is_cave) {
        float radius_sq = (rows / 3.0f) * (cols / 3.0f) * 2.5f;
        int center_r = rows / 2;
        int center_c = cols / 2;
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                float dist = (r - center_r) * (r - center_r) + (c - center_c) * (c - center_c);
                if (dist < radius_sq && rand() % 100 < 70) {
                    int roll = rand() % 100;
                    if (roll < 15) grid[r][c] = TYPE_MOUNTAIN;
                    else if (roll < 35) grid[r][c] = TYPE_FOREST;
                    else if (roll < 40) grid[r][c] = TYPE_RIVER;
                    else grid[r][c] = TYPE_GRASS;
                } else {
                    grid[r][c] = TYPE_OCEAN;
                }
            }
        }
        for (int i = 0; i < 5; i++) {
            int r = rand() % rows;
            int c = rand() % cols;
            if (grid[r][c] == TYPE_GRASS) grid[r][c] = TYPE_CAVE_ENTRANCE;
        }
        center_r = rows / 2;
        center_c = cols / 2;
        while (grid[center_r][center_c] != TYPE_GRASS) {
            center_r = rows / 2 + (rand() % 5 - 2);
            center_c = cols / 2 + (rand() % 5 - 2);
        }
        entities[num_entities].x = center_c * TILE_SIZE;
        entities[num_entities].y = center_r * TILE_SIZE;
        entities[num_entities].z = 0;
        entities[num_entities].type = TYPE_HERO;
        entities[num_entities].hp = 20;
        entities[num_entities].max_hp = 20;
        entities[num_entities].atk = 5;
        entities[num_entities].def = 2;
        entities[num_entities].active = 1;
        hero_index = num_entities++;
        grid[center_r][center_c] = TYPE_GRASS;
        camera_x = entities[hero_index].x;
        camera_y = entities[hero_index].y;
    } else {
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                grid[r][c] = (rand() % 100 < 45) ? TYPE_WALL : TYPE_GRASS;
            }
        }
        for (int iter = 0; iter < 5; iter++) {
            int temp[MAP_ROWS][MAP_COLS];
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) {
                    int walls = 0;
                    for (int dr = -1; dr <= 1; dr++) {
                        for (int dc = -1; dc <= 1; dc++) {
                            if (dr == 0 && dc == 0) continue;
                            int nr = r + dr, nc = c + dc;
                            if (nr < 0 || nr >= rows || nc < 0 || nc >= cols || grid[nr][nc] == TYPE_WALL) walls++;
                        }
                    }
                    temp[r][c] = (walls > 4) ? TYPE_WALL : TYPE_GRASS;
                }
            }
            memcpy(grid, temp, sizeof(temp));
        }
        int exit_r = rand() % rows, exit_c = rand() % cols;
        while (grid[exit_r][exit_c] == TYPE_WALL) {
            exit_r = rand() % rows;
            exit_c = rand() % cols;
        }
        grid[exit_r][exit_c] = TYPE_CAVE_EXIT;
        int hero_r = rand() % rows, hero_c = rand() % cols;
        while (grid[hero_r][hero_c] == TYPE_WALL) {
            hero_r = rand() % rows;
            hero_c = rand() % cols;
        }
        entities[num_entities].x = hero_c * TILE_SIZE;
        entities[num_entities].y = hero_r * TILE_SIZE;
        entities[num_entities].z = 0;
        entities[num_entities].type = TYPE_HERO;
        entities[num_entities].hp = entities[hero_index].hp;
        entities[num_entities].max_hp = entities[hero_index].max_hp;
        entities[num_entities].atk = entities[hero_index].atk;
        entities[num_entities].def = entities[hero_index].def;
        entities[num_entities].active = 1;
        hero_index = num_entities++;
        grid[hero_r][hero_c] = TYPE_GRASS;
        camera_x = entities[hero_index].x;
        camera_y = entities[hero_index].y;
    }

    int num_enemies = is_cave ? 10 + level : 20 + level * 2;
    if (num_enemies > 50) num_enemies = 50;
    for (int i = 0; i < num_enemies; i++) {
        int r = rand() % rows;
        int c = rand() % cols;
        while (grid[r][c] != TYPE_GRASS || abs(r - ((int)(entities[hero_index].y / TILE_SIZE))) + abs(c - ((int)(entities[hero_index].x / TILE_SIZE))) < 5) {
            r = rand() % rows;
            c = rand() % cols;
        }
        entities[num_entities].x = c * TILE_SIZE;
        entities[num_entities].y = r * TILE_SIZE;
        entities[num_entities].z = 0;
        entities[num_entities].type = TYPE_ENEMY;
        entities[num_entities].hp = 10 + level * 2;
        entities[num_entities].max_hp = entities[num_entities].hp;
        entities[num_entities].atk = 3 + level;
        entities[num_entities].def = 1 + level / 3;
        entities[num_entities].active = 1;
        grid[r][c] = TYPE_ENEMY;
        num_entities++;
    }

    int num_items = is_cave ? 3 : 5 + level / 3;
    for (int i = 0; i < num_items; i++) {
        int r = rand() % rows;
        int c = rand() % cols;
        while (grid[r][c] != TYPE_GRASS) {
            r = rand() % rows;
            c = rand() % cols;
        }
        grid[r][c] = TYPE_POTION;
    }
}

static bool is_valid_tile(float x, float y) {
    int r = (int)(y / TILE_SIZE);
    int c = (int)(x / TILE_SIZE);
    return r >= 0 && r < rows && c >= 0 && c < cols;
}

static bool is_land(float x, float y) {
    if (!is_valid_tile(x, y)) return false;
    int r = (int)(y / TILE_SIZE);
    int c = (int)(x / TILE_SIZE);
    int t = grid[r][c];
    return t != TYPE_OCEAN && t != TYPE_MOUNTAIN && t != TYPE_WALL;
}

static int find_entity_at(float x, float y, int exclude_index) {
    int r = (int)(y / TILE_SIZE);
    int c = (int)(x / TILE_SIZE);
    for (int i = 0; i < num_entities; i++) {
        if (i == exclude_index || !entities[i].active) continue;
        int er = (int)(entities[i].y / TILE_SIZE);
        int ec = (int)(entities[i].x / TILE_SIZE);
        if (er == r && ec == c) return i;
    }
    return -1;
}

static void process_turn() {
    int hero_r = (int)(entities[hero_index].y / TILE_SIZE);
    int hero_c = (int)(entities[hero_index].x / TILE_SIZE);
    if (grid[hero_r][hero_c] == TYPE_CAVE_ENTRANCE && state == OVERWORLD) {
        state = CAVE;
        generate_world(true);
        return;
    }
    if (grid[hero_r][hero_c] == TYPE_CAVE_EXIT && state == CAVE) {
        state = OVERWORLD;
        generate_world(false);
        return;
    }
    if (grid[hero_r][hero_c] == TYPE_POTION) {
        if (hero_inventory_count < 10) {
            hero_inventory[hero_inventory_count++] = TYPE_POTION;
            grid[hero_r][hero_c] = TYPE_GRASS;
            snprintf(status_message, sizeof(status_message), "Picked up potion.");
        }
    }

    for (int i = 0; i < num_entities; i++) {
        if (!entities[i].active || entities[i].type != TYPE_ENEMY) continue;
        float dx = entities[hero_index].x - entities[i].x;
        float dy = entities[hero_index].y - entities[i].y;
        if (fabs(dx) + fabs(dy) <= VIEW_RANGE * TILE_SIZE) {
            float nx = entities[i].x, ny = entities[i].y;
            if (dx > 0) nx += TILE_SIZE;
            else if (dx < 0) nx -= TILE_SIZE;
            else if (dy > 0) ny += TILE_SIZE;
            else if (dy < 0) ny -= TILE_SIZE;
            int r = (int)(entities[i].y / TILE_SIZE);
            int c = (int)(entities[i].x / TILE_SIZE);
            if (is_land(nx, ny) && find_entity_at(nx, ny, i) < 0 && !(nx == entities[hero_index].x && ny == entities[hero_index].y)) {
                grid[r][c] = TYPE_GRASS;
                entities[i].x = nx;
                entities[i].y = ny;
                grid[(int)(ny / TILE_SIZE)][(int)(nx / TILE_SIZE)] = TYPE_ENEMY;
            }
        }
    }

    for (int i = 0; i < num_entities; i++) {
        if (!entities[i].active || entities[i].type != TYPE_ARROW) continue;
        float nx = entities[i].x + entities[i].dr * TILE_SIZE;
        float ny = entities[i].y + entities[i].dc * TILE_SIZE;
        int r = (int)(entities[i].y / TILE_SIZE);
        int c = (int)(entities[i].x / TILE_SIZE);
        if (!is_land(nx, ny)) {
            entities[i].active = 0;
            grid[r][c] = TYPE_GRASS;
            continue;
        }
        grid[r][c] = TYPE_GRASS;
        entities[i].x = nx;
        entities[i].y = ny;
        grid[r = (int)(ny / TILE_SIZE)][(int)(nx / TILE_SIZE)] = TYPE_ARROW;
        int enemy_index = find_entity_at(nx, ny, i);
        if (enemy_index >= 0 && entities[enemy_index].type == TYPE_ENEMY) {
            entities[enemy_index].hp -= entities[hero_index].atk;
            if (entities[enemy_index].hp <= 0) {
                entities[enemy_index].active = 0;
                grid[r][(int)(nx / TILE_SIZE)] = TYPE_GRASS;
                hero_exp += 5 + level * 2;
                if (hero_exp >= hero_next_exp) {
                    hero_exp -= hero_next_exp;
                    hero_next_exp *= 2;
                    entities[hero_index].max_hp += 5;
                    entities[hero_index].hp = entities[hero_index].max_hp;
                    entities[hero_index].atk += 2;
                    entities[hero_index].def += 1;
                    level++;
                    snprintf(status_message, sizeof(status_message), "Level up! Stats increased.");
                }
                snprintf(status_message, sizeof(status_message), "Enemy defeated by arrow!");
            }
            entities[i].active = 0;
            grid[r][(int)(nx / TILE_SIZE)] = TYPE_GRASS;
        }
    }

    int enemy_index = find_entity_at(entities[hero_index].x, entities[hero_index].y, hero_index);
    if (enemy_index >= 0 && entities[enemy_index].type == TYPE_ENEMY) {
        entities[hero_index].hp -= entities[enemy_index].atk - entities[hero_index].def;
        if (entities[hero_index].hp <= 0) {
            snprintf(status_message, sizeof(status_message), "Hero defeated! Game over.");
        }
        entities[enemy_index].hp -= entities[hero_index].atk - entities[enemy_index].def;
        if (entities[enemy_index].hp <= 0) {
            entities[enemy_index].active = 0;
            int r = (int)(entities[enemy_index].y / TILE_SIZE);
            int c = (int)(entities[hero_index].x / TILE_SIZE);
            grid[r][c] = TYPE_GRASS;
            hero_exp += 5 + level * 2;
            if (hero_exp >= hero_next_exp) {
                hero_exp -= hero_next_exp;
                hero_next_exp *= 2;
                entities[hero_index].max_hp += 5;
                entities[hero_index].hp = entities[hero_index].max_hp;
                entities[hero_index].atk += 2;
                entities[hero_index].def += 1;
                level++;
                snprintf(status_message, sizeof(status_message), "Level up! Stats increased.");
            }
            snprintf(status_message, sizeof(status_message), "Enemy defeated!");
        }
    }

    for (int i = 0; i < num_entities; i++) {
        if (entities[i].type == TYPE_SLASH && entities[i].active) {
            entities[i].hp--;
            if (entities[i].hp <= 0) {
                entities[i].active = 0;
                int r = (int)(entities[i].y / TILE_SIZE);
                int c = (int)(entities[i].x / TILE_SIZE);
                grid[r][c] = TYPE_GRASS;
            }
        }
    }
}

void init(void) {
    glEnable(GL_DEPTH_TEST);
    generate_world(false);
}

void get_viewport_size(int *width, int *height) {
    *width = VIEW_COLS * TILE_SIZE + 20;
    *height = VIEW_ROWS * TILE_SIZE + 100;
}

void get_camera_position(float *x, float *y, float *z) {
    *x = camera_x;
    *y = camera_y;
    *z = camera_z;
}

void get_camera_orientation(float *pitch, float *yaw, float *roll) {
    *pitch = camera_pitch;
    *yaw = camera_yaw;
    *roll = camera_roll;
}

int get_num_entities(void) {
    return num_entities;
}

void get_entity_position(int index, float *x, float *y, float *z) {
    if (index < 0 || index >= num_entities) {
        *x = *y = *z = 0;
        return;
    }
    *x = entities[index].x;
    *y = entities[index].y;
    *z = entities[index].z;
}

int get_entity_type(int index) {
    if (index < 0 || index >= num_entities) return -1;
    return entities[index].type;
}

const char* get_entity_emoji(int type) {
    switch (type) {
        case TYPE_OCEAN: return EMOJI_OCEAN;
        case TYPE_GRASS: return EMOJI_LAND;
        case TYPE_FOREST: return EMOJI_FOREST;
        case TYPE_MOUNTAIN: return EMOJI_MOUNTAIN;
        case TYPE_RIVER: return EMOJI_RIVER;
        case TYPE_CAVE_ENTRANCE: return EMOJI_CAVE_ENTRANCE;
        case TYPE_CAVE_EXIT: return EMOJI_CAVE_EXIT;
        case TYPE_WALL: return "🪨";
        case TYPE_HERO: return EMOJI_HERO;
        case TYPE_ENEMY: return EMOJI_ENEMY;
        case TYPE_POTION: return EMOJI_POTION;
        case TYPE_ARROW: return EMOJI_ARROW;
        case TYPE_SLASH: return EMOJI_SLASH;
        default: return EMOJI_LAND;
    }
}

int get_world_value(float x, float y, float z) {
    if (!is_valid_tile(x, y) || z != 0) return -1;
    int r = (int)(y / TILE_SIZE);
    int c = (int)(x / TILE_SIZE);
    return grid[r][c];
}

const char* get_status_message(void) {
    return status_message;
}

int get_score(void) {
    return hero_exp;
}

bool get_is_animating(void) {
    return false;
}

void update_entities(void) {
    // Not used in turn-based game
}

void set_entity_action(int entity_index, int action_id, float value) {
    if (entity_index != hero_index) return;
    bool turn_taken = false;
    float dx = 0, dy = 0;
    if (action_id == 'w' || action_id == 'W' || action_id == GLUT_KEY_UP + 256) {
        dy = -TILE_SIZE; facing = 1; turn_taken = true; // Up: decrease Y (move up in screen)
        snprintf(status_message, sizeof(status_message), "Moving up");
    } else if (action_id == 's' || action_id == 'S' || action_id == GLUT_KEY_DOWN + 256) {
        dy = TILE_SIZE; facing = 0; turn_taken = true;  // Down: increase Y (move down in screen)
        snprintf(status_message, sizeof(status_message), "Moving down");
    } else if (action_id == 'a' || action_id == 'A' || action_id == GLUT_KEY_LEFT + 256) {
        dx = -TILE_SIZE; facing = 2; turn_taken = true;
        snprintf(status_message, sizeof(status_message), "Moving left");
    } else if (action_id == 'd' || action_id == 'D' || action_id == GLUT_KEY_RIGHT + 256) {
        dx = TILE_SIZE; facing = 3; turn_taken = true;
        snprintf(status_message, sizeof(status_message), "Moving right");
    } else if (action_id == 'p' || action_id == 'P') {
        for (int i = 0; i < hero_inventory_count; i++) {
            if (hero_inventory[i] == TYPE_POTION) {
                entities[hero_index].hp += 10;
                if (entities[hero_index].hp > entities[hero_index].max_hp) entities[hero_index].hp = entities[hero_index].max_hp;
                for (int j = i; j < hero_inventory_count - 1; j++) {
                    hero_inventory[j] = hero_inventory[j + 1];
                }
                hero_inventory_count--;
                snprintf(status_message, sizeof(status_message), "Used potion. HP restored.");
                turn_taken = true;
                break;
            }
        }
        if (!turn_taken) snprintf(status_message, sizeof(status_message), "No potion in inventory.");
    } else if (action_id == ' ') {
        float tx = entities[hero_index].x, ty = entities[hero_index].y;
        switch (facing) {
            case 0: ty += TILE_SIZE; break; // Down
            case 1: ty -= TILE_SIZE; break; // Up
            case 2: tx -= TILE_SIZE; break; // Left
            case 3: tx += TILE_SIZE; break; // Right
        }
        if (is_valid_tile(tx, ty)) {
            entities[num_entities].x = tx;
            entities[num_entities].y = ty;
            entities[num_entities].z = 0;
            entities[num_entities].type = TYPE_SLASH;
            entities[num_entities].hp = SLASH_DURATION;
            entities[num_entities].active = 1;
            int r = (int)(ty / TILE_SIZE);
            int c = (int)(tx / TILE_SIZE);
            grid[r][c] = TYPE_SLASH;
            num_entities++;
            int enemy_index = find_entity_at(tx, ty, hero_index);
            if (enemy_index >= 0 && entities[enemy_index].type == TYPE_ENEMY) {
                entities[enemy_index].hp -= entities[hero_index].atk - entities[enemy_index].def;
                if (entities[enemy_index].hp <= 0) {
                    entities[enemy_index].active = 0;
                    grid[r][c] = TYPE_GRASS;
                    hero_exp += 5 + level * 2;
                    if (hero_exp >= hero_next_exp) {
                        hero_exp -= hero_next_exp;
                        hero_next_exp *= 2;
                        entities[hero_index].max_hp += 5;
                        entities[hero_index].hp = entities[hero_index].max_hp;
                        entities[hero_index].atk += 2;
                        entities[hero_index].def += 1;
                        level++;
                        snprintf(status_message, sizeof(status_message), "Level up! Stats increased.");
                    }
                    snprintf(status_message, sizeof(status_message), "Enemy slashed!");
                }
            }
            turn_taken = true;
        }
    } else if (action_id == 13) { // Enter for arrow
        if (num_entities < MAX_ENTITIES) {
            entities[num_entities].x = entities[hero_index].x;
            entities[num_entities].y = entities[hero_index].y;
            entities[num_entities].z = 0;
            entities[num_entities].type = TYPE_ARROW;
            entities[num_entities].active = 1;
            entities[num_entities].dr = 0;
            entities[num_entities].dc = 0;
            switch (facing) {
                case 0: entities[num_entities].dr = 1; break;  // Down
                case 1: entities[num_entities].dr = -1; break; // Up
                case 2: entities[num_entities].dc = -1; break; // Left
                case 3: entities[num_entities].dc = 1; break;  // Right
            }
            num_entities++;
            snprintf(status_message, sizeof(status_message), "Arrow shot!");
            turn_taken = true;
        }
    }

    if (turn_taken && (dx != 0 || dy != 0)) {
        float nx = entities[hero_index].x + dx;
        float ny = entities[hero_index].y + dy;
        if (is_land(nx, ny)) {
            int r = (int)(entities[hero_index].y / TILE_SIZE);
            int c = (int)(entities[hero_index].x / TILE_SIZE);
            grid[r][c] = TYPE_GRASS;
            entities[hero_index].x = nx;
            entities[hero_index].y = ny;
            r = (int)(ny / TILE_SIZE);
            c = (int)(nx / TILE_SIZE);
            if (grid[r][c] <= TYPE_CAVE_EXIT) grid[r][c] = TYPE_GRASS;
            camera_x = nx;
            camera_y = ny;
        }
    }

    if (turn_taken) process_turn();
}
