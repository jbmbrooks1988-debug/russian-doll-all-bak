#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>

// --- Constants ---
#define GRID_ROWS 15
#define GRID_COLS 17
#define MAX_BOMBS 10
#define BOMB_TIMER 90 // 1.5 seconds at 60fps
#define EXPLOSION_DURATION 30
#define MAX_ENEMIES 5
#define MAX_ITEMS 20
#define MAX_QUEUE 255
#define TILE_SIZE 40

// --- Tile types ---
#define TILE_EMPTY              0
#define TILE_WALL_SOLID         1
#define TILE_WALL_DESTRUCTIBLE  2

// --- Game state ---
enum GameState { PLAYING, GAME_OVER, LEVEL_COMPLETE };
enum GameState game_state = PLAYING;
char status_message[256] = "Use arrow keys to move, Space to place bomb!";
int tiles[GRID_ROWS][GRID_COLS];
int player_row = 1, player_col = 1;
int player_max_bombs = 1;
int player_flame_power = 1;
int level = 1;
int score = 0;

// --- Bomb management ---
struct Bomb { 
    int id, row, col, timer; 
    bool active; 
} bombs[MAX_BOMBS];

struct Explosion { 
    int id, row, col, timer; 
    bool active; 
    int type; // 0=center, 1=horizontal, 2=vertical
} explosions[MAX_BOMBS * 5];

// --- Enemy management ---
struct Enemy { 
    int id, row, col, move_timer; 
    bool active; 
    int type; // 0=Basic, 1=Fast, etc.
} enemies[MAX_ENEMIES];

// --- Item management ---
struct Item { 
    int id, row, col, type; // 0=bomb upgrade, 1=fire upgrade, etc.
    bool active; 
} items[MAX_ITEMS];

// --- Global variables for session management ---
static char session_dir[256] = {0};
static int state_counter = 0;
static bool session_initialized = false;

// --- Forward declarations ---
bool enemy_on_pos(int r, int c);
bool is_valid_tile(int r, int c);
bool is_walkable(int r, int c);
void generate_world();
int count_active_bombs();
void place_bomb(int r, int c);
void add_explosion(int r, int c, int type);
void trigger_explosion(int r, int c, int power);
bool bfs_next_move(int er, int ec, int pr, int pc, int* dr, int* dc);
void update_game();
void restart_game();
void process_input_command(const char* input_command);
void write_player_state();
void write_enemy_states();
void write_bomb_states();
void write_explosion_states();
void write_map_state();
void write_game_state();
void write_powerup_states();
void write_all_game_states();
void create_state_directory();
char* get_current_state_path(const char* filename);

// --- Utility functions ---
void create_session_directory() {
    time_t now = time(0);
    struct tm *tm_info = localtime(&now);
    
    // Create session directory name with timestamp
    snprintf(session_dir, sizeof(session_dir), "session_%04d%02d%02d_%02d%02d%02d", 
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    // Create the session directory
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", session_dir);
    system(cmd);
    
    printf("Created session directory: %s\n", session_dir);
}

void create_state_directory() {
    state_counter++;
    
    // Create state subdirectory name with timestamp
    char state_dir[2048];  // Increased size to prevent overflow
    snprintf(state_dir, sizeof(state_dir), "%s/timestamp_%05d", session_dir, state_counter);
    
    // Create the state directory
    char cmd[2058];  // Increased size to prevent overflow (10 chars for "mkdir -p " + null terminator)
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", state_dir);
    system(cmd);
    
    printf("Created state directory: %s\n", state_dir);
}

char* get_current_state_path(const char* filename) {
    static char path[512];
    snprintf(path, sizeof(path), "%s/timestamp_%05d/%s", session_dir, state_counter, filename);
    return path;
}

void generate_world() {
    srand(time(NULL) + level); // Use level as seed modifier
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (r == 0 || c == 0 || r == GRID_ROWS - 1 || c == GRID_COLS - 1 || (r % 2 == 0 && c % 2 == 0)) {
                tiles[r][c] = TILE_WALL_SOLID;
            } else {
                tiles[r][c] = (rand() % 100 < 70) ? TILE_WALL_DESTRUCTIBLE : TILE_EMPTY;
            }
        }
    }
    // Clear starting positions
    tiles[1][1] = TILE_EMPTY; tiles[1][2] = TILE_EMPTY; tiles[2][1] = TILE_EMPTY;
    
    // Place enemies based on level
    int num_enemies = (level > MAX_ENEMIES) ? MAX_ENEMIES : level;
    int placed = 0;
    for (int i = 0; i < MAX_ENEMIES && placed < num_enemies; i++) {
        int attempts = 0;
        int er, ec;
        do {
            er = 1 + rand() % (GRID_ROWS - 2);
            ec = 1 + rand() % (GRID_COLS - 2);
            attempts++;
        } while ((tiles[er][ec] != TILE_EMPTY || (er == player_row && ec == player_col) || 
                 enemy_on_pos(er, ec)) && attempts < 100);
        if (attempts < 100) {
            enemies[i].id = i;
            enemies[i].row = er;
            enemies[i].col = ec;
            enemies[i].move_timer = rand() % 60;
            enemies[i].active = true;
            enemies[i].type = rand() % 2; // Random enemy type
            placed++;
        }
    }
}

bool enemy_on_pos(int r, int c) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].row == r && enemies[i].col == c) return true;
    }
    return false;
}

bool is_valid_tile(int r, int c) { 
    return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS; 
}

bool is_walkable(int r, int c) { 
    return is_valid_tile(r, c) && tiles[r][c] == TILE_EMPTY && !enemy_on_pos(r, c); 
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
            bombs[i].id = i;
            bombs[i].row = r;
            bombs[i].col = c;
            bombs[i].timer = BOMB_TIMER;
            bombs[i].active = true;
            return;
        }
    }
}

void add_explosion(int r, int c, int type) {
    int idx = 0;
    while (idx < MAX_BOMBS * 5 && explosions[idx].active) idx++;
    if (idx < MAX_BOMBS * 5) {
        explosions[idx].id = idx;
        explosions[idx].row = r;
        explosions[idx].col = c;
        explosions[idx].timer = EXPLOSION_DURATION;
        explosions[idx].active = true;
        explosions[idx].type = type;
    }
}

void trigger_explosion(int r, int c, int power) {
    int dirs[4][2] = {{0,1}, {1,0}, {0,-1}, {-1,0}};
    add_explosion(r, c, 0); // Center explosion
    
    for (int d = 0; d < 4; d++) {
        int dr = dirs[d][0], dc = dirs[d][1];
        for (int dist = 1; dist <= power; dist++) {
            int nr = r + dist * dr, nc = c + dist * dc;
            if (!is_valid_tile(nr, nc)) break;
            
            add_explosion(nr, nc, d+1); // Directional explosion
            
            if (tiles[nr][nc] == TILE_WALL_SOLID) break;
            if (tiles[nr][nc] == TILE_WALL_DESTRUCTIBLE) {
                tiles[nr][nc] = TILE_EMPTY;
                // Randomly spawn power-up
                if (rand() % 5 == 0) {
                    int iidx = 0;
                    while (iidx < MAX_ITEMS && items[iidx].active) iidx++;
                    if (iidx < MAX_ITEMS) {
                        items[iidx].id = iidx;
                        items[iidx].row = nr;
                        items[iidx].col = nc;
                        items[iidx].type = rand() % 2;
                        items[iidx].active = true;
                    }
                }
                break;
            }
        }
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
            if (is_valid_tile(nr, nc) && !visited[nr][nc] && 
                (tiles[nr][nc] == TILE_EMPTY || (nr == pr && nc == pc)) && !enemy_on_pos(nr, nc)) {
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

void update_game() {
    if (game_state != PLAYING) return;
    
    // Update bombs
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
            enemies[i].move_timer = (enemies[i].type == 0) ? 30 : 15; // Fast enemies move faster
            int dr = 0, dc = 0;
            if (bfs_next_move(enemies[i].row, enemies[i].col, player_row, player_col, &dr, &dc)) {
                int nr = enemies[i].row + dr, nc = enemies[i].col + dc;
                if (is_walkable(nr, nc)) {
                    enemies[i].row = nr;
                    enemies[i].col = nc;
                }
            } else {
                // Random move if no path
                int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};
                int d = rand() % 4;
                int nr = enemies[i].row + dirs[d][0];
                int nc = enemies[i].col + dirs[d][1];
                if (is_walkable(nr, nc)) {
                    enemies[i].row = nr;
                    enemies[i].col = nc;
                }
            }
            
            // Check collision with player after move
            if (enemies[i].row == player_row && enemies[i].col == player_col) {
                game_state = GAME_OVER;
                strcpy(status_message, "GAME OVER! Eaten by enemy. Press 'R' to restart.");
            }
        }
    }
    
    // Update explosions and check for enemy/player hits
    bool player_hit = false;
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            // Check if explosion hits player
            if (explosions[i].row == player_row && explosions[i].col == player_col) {
                player_hit = true;
            }
            
            // Check if explosion hits enemies
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (enemies[j].active && 
                    explosions[i].row == enemies[j].row && 
                    explosions[i].col == enemies[j].col) {
                    enemies[j].active = false;
                    score += 100 * level;
                }
            }
            
            if (--explosions[i].timer <= 0) {
                explosions[i].active = false;
            }
        }
    }
    
    if (player_hit) {
        game_state = GAME_OVER;
        strcpy(status_message, "GAME OVER! Hit by explosion. Press 'R' to restart.");
    }
    
    // Check if all enemies are dead
    int active_count = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) active_count++;
    }
    if (active_count == 0 && level < 10) {
        level++;
        strcpy(status_message, "Level Complete! Next level starting...");
        game_state = LEVEL_COMPLETE;
        // Set up next level after a delay would happen in the controller
    }
}

void restart_game() {
    player_row = 1; 
    player_col = 1;
    player_max_bombs = 1;
    player_flame_power = 1;
    level = 1;
    score = 0;
    game_state = PLAYING;
    generate_world();
    
    // Reset game objects
    for (int i = 0; i < MAX_BOMBS; i++) {
        bombs[i].active = false;
    }
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        explosions[i].active = false;
    }
    for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].active = false;
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = false;
    }
    
    strcpy(status_message, "Use arrow keys to move, Space to place bomb!");
}

void process_input_command(const char* input_command) {
    if (game_state != PLAYING) {
        if (strcmp(input_command, "R") == 0 || strcmp(input_command, "r") == 0) {
            restart_game();
        }
        return;
    }
    
    // Player movement
    int new_row = player_row, new_col = player_col;
    
    if (strcmp(input_command, "UP") == 0) {
        new_row--;
    } else if (strcmp(input_command, "DOWN") == 0) {
        new_row++;
    } else if (strcmp(input_command, "LEFT") == 0) {
        new_col--;
    } else if (strcmp(input_command, "RIGHT") == 0) {
        new_col++;
    } else if (strcmp(input_command, "BOMB") == 0 || strcmp(input_command, "SPACE") == 0) {
        if (count_active_bombs() < player_max_bombs) {
            place_bomb(player_row, player_col);
        }
        return; // Don't move when placing bomb
    }
    
    // Check if new position is valid
    if (is_walkable(new_row, new_col)) {
        player_row = new_row;
        player_col = new_col;
        
        // Check for item collection
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (items[i].active && items[i].row == player_row && items[i].col == player_col) {
                if (items[i].type == 0) { // Bomb upgrade
                    player_max_bombs++;
                    sprintf(status_message, "Got extra bomb! Max bombs: %d", player_max_bombs);
                } else if (items[i].type == 1) { // Fire upgrade
                    player_flame_power++;
                    sprintf(status_message, "Got flame upgrade! Flame range: %d", player_flame_power);
                }
                items[i].active = false;
            }
        }
        
        // Check for enemy collision after movement
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active && enemies[i].row == player_row && enemies[i].col == player_col) {
                game_state = GAME_OVER;
                strcpy(status_message, "GAME OVER! Collided with enemy. Press 'R' to restart.");
            }
        }
    }
}

// Function to write player state to file
void write_player_state() {
    char* path = get_current_state_path("player.csv");
    FILE* fp = fopen(path, "w");
    if (!fp) {
        perror("Error opening player.csv");
        return;
    }
    
    // Convert grid position to pixel coordinates
    int player_x = player_col * TILE_SIZE;
    int player_y = (GRID_ROWS - 1 - player_row) * TILE_SIZE;
    
    fprintf(fp, "x,y,bombs,power\n");
    fprintf(fp, "%d,%d,%d,%d\n", player_x, player_y, player_max_bombs, player_flame_power);
    fclose(fp);
}

// Function to write enemy states to file
void write_enemy_states() {
    char* path = get_current_state_path("enemies.csv");
    FILE* fp = fopen(path, "w");
    if (!fp) {
        perror("Error opening enemies.csv");
        return;
    }
    
    fprintf(fp, "id,x,y,type\n");
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            int x = enemies[i].col * TILE_SIZE;
            int y = (GRID_ROWS - 1 - enemies[i].row) * TILE_SIZE;
            fprintf(fp, "%d,%d,%d,%d\n", enemies[i].id, x, y, enemies[i].type);
        }
    }
    fclose(fp);
}

// Function to write bomb states to file
void write_bomb_states() {
    char* path = get_current_state_path("bombs.csv");
    FILE* fp = fopen(path, "w");
    if (!fp) {
        perror("Error opening bombs.csv");
        return;
    }
    
    fprintf(fp, "id,x,y,timer\n");
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            int x = bombs[i].col * TILE_SIZE;
            int y = (GRID_ROWS - 1 - bombs[i].row) * TILE_SIZE;
            fprintf(fp, "%d,%d,%d,%d\n", bombs[i].id, x, y, bombs[i].timer);
        }
    }
    fclose(fp);
}

// Function to write explosion states to file
void write_explosion_states() {
    char* path = get_current_state_path("explosions.csv");
    FILE* fp = fopen(path, "w");
    if (!fp) {
        perror("Error opening explosions.csv");
        return;
    }
    
    fprintf(fp, "id,x,y,type\n");
    for (int i = 0; i < MAX_BOMBS * 5; i++) {
        if (explosions[i].active) {
            int x = explosions[i].col * TILE_SIZE;
            int y = (GRID_ROWS - 1 - explosions[i].row) * TILE_SIZE;
            fprintf(fp, "%d,%d,%d,%d\n", explosions[i].id, x, y, explosions[i].type);
        }
    }
    fclose(fp);
}

// Function to write map state to file
void write_map_state() {
    char* path = get_current_state_path("map.csv");
    FILE* fp = fopen(path, "w");
    if (!fp) {
        perror("Error opening map.csv");
        return;
    }
    
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            fprintf(fp, "%d", tiles[r][c]);
            if (c < GRID_COLS - 1) fprintf(fp, ",");
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}

// Function to write game state to file
void write_game_state() {
    char* path = get_current_state_path("game_state.csv");
    FILE* fp = fopen(path, "w");
    if (!fp) {
        perror("Error opening game_state.csv");
        return;
    }
    
    fprintf(fp, "score,level,lives,status,message\n");
    fprintf(fp, "%d,%d,%d,%s,%s\n", score, level, 3, game_state == PLAYING ? "Playing" : "GameOver", status_message);
    fclose(fp);
}

// Function to write power-up states to file
void write_powerup_states() {
    char* path = get_current_state_path("powerups.csv");
    FILE* fp = fopen(path, "w");
    if (!fp) {
        perror("Error opening powerups.csv");
        return;
    }
    
    fprintf(fp, "id,x,y,type\n");
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            int x = items[i].col * TILE_SIZE;
            int y = (GRID_ROWS - 1 - items[i].row) * TILE_SIZE;
            fprintf(fp, "%d,%d,%d,%d\n", items[i].id, x, y, items[i].type);
        }
    }
    fclose(fp);
}

// Function to write a simple output.csv for compatibility with CHTML canvas rendering
void write_simple_output() {
    FILE* out_fp = fopen("output.csv", "w");
    if (!out_fp) {
        perror("Error opening output.csv");
        return;
    }
    
    // Write current player position in pixel coordinates
    int player_x = player_col * TILE_SIZE;
    int player_y = (GRID_ROWS - 1 - player_row) * TILE_SIZE;
    
    fprintf(out_fp, "%d,%d\n", player_x, player_y);
    fclose(out_fp);
}

// Function to write all game states to timestamped CSV files
void write_all_game_states() {
    create_state_directory();
    
    write_player_state();
    write_enemy_states();
    write_bomb_states();
    write_explosion_states();
    write_map_state();
    write_game_state();
    write_powerup_states();
    
    // Write simple output.csv for CHTML canvas compatibility
    write_simple_output();
}

int main(int argc, char* argv[]) {
    // Read from hardcoded input file
    const char* input_file = "input.csv";

    FILE* in_fp = fopen(input_file, "r");
    if (!in_fp) {
        perror("Error opening input CSV file");
        return 1;
    }

    // Read input: player_x,player_y,input_command,canvas_width,canvas_height
    int player_x, player_y, canvas_width, canvas_height;
    char input_command[20];
    
    if (fscanf(in_fp, "%d,%d,%19[^,],%d,%d", &player_x, &player_y, input_command, &canvas_width, &canvas_height) != 5) {
        fprintf(stderr, "Error reading game state from input CSV file\n");
        fclose(in_fp);
        return 1;
    }
    fclose(in_fp);

    // Initialize session on first run
    if (!session_initialized) {
        create_session_directory();
        srand(time(NULL));
        generate_world();
        session_initialized = true;
    }

    // Process input command
    process_input_command(input_command);
    
    // Update game state
    update_game();
    
    // Write all game states to timestamped CSV files
    write_all_game_states();

    return 0;
}