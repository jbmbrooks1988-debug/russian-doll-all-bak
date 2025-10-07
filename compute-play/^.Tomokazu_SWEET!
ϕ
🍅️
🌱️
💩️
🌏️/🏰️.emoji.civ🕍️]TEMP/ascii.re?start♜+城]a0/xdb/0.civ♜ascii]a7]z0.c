#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>

// --- Constants ---
#define GRID_ROWS 12
#define GRID_COLS 12

// Emojis (or ASCII fallbacks for terminals)
const char* EMOJI_OCEAN = "🌊";
const char* EMOJI_LAND = "🟩";
const char* EMOJI_FOREST = "🌲";
const char* EMOJI_MOUNTAIN = "⛰️";
const char* EMOJI_RIVER = "💧";
const char* EMOJI_SETTLER = "👤";
const char* EMOJI_CITY = "🏙️";
const char* EMOJI_FOG = "🌫️";

// Terrain types
#define TERRAIN_OCEAN    0
#define TERRAIN_GRASS    1
#define TERRAIN_FOREST   2
#define TERRAIN_MOUNTAIN 3
#define TERRAIN_RIVER    4

// Game state
int turn = 1;
int gold = 0;
int city_count = 0;
int settler_row = -1, settler_col = -1;
bool has_settler = true;
bool city_map[GRID_ROWS][GRID_COLS] = {0};
int terrain[GRID_ROWS][GRID_COLS];
bool explored[GRID_ROWS][GRID_COLS] = {0};
int cursor_row = 6, cursor_col = 6;
bool debug_fog_enabled = true;
bool debug_terrain_enabled = true;
bool debug_show_cursor = true;
bool settler_selected = false;
bool show_production_menu = false;
int production_choice = 0;
const char* production_options[3] = {"Granary (+food)", "Walls (+defense)", "Temple (+happiness)"};
char status_message[256] = "Welcome! Use Enter to select and act.";

// --- Function Prototypes ---
void generate_world();
bool is_valid_tile(int r, int c);
bool is_land(int r, int c);
void reveal_around(int r, int c, int radius);
void end_turn();
void move_settler(int row, int col);
void open_production_menu();
void finalize_city_founding(int row, int col);
void set_status_message(const char* format, ...);
void render_grid();
void clear_screen();
void process_input();

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
                settler_row = r;
                settler_col = c;
                has_settler = true;
                reveal_around(r, c, 3);
                cursor_row = r;
                cursor_col = c;
                set_status_message("Settler landed safely! Use Enter to select.");
                found = true;
            }
        }
    }

    if (!found) {
        settler_row = center_r;
        settler_col = center_c;
        terrain[settler_row][settler_col] = TERRAIN_GRASS;
        has_settler = true;
        reveal_around(settler_row, settler_col, 3);
        set_status_message("Fallback spawn. Ready.");
    }
}

bool is_valid_tile(int r, int c) {
    return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS;
}

bool is_land(int r, int c) {
    return is_valid_tile(r, c) && terrain[r][c] != TERRAIN_OCEAN;
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

// --- Game Logic ---
void end_turn() {
    turn++;
    gold += city_count;
    if (has_settler && is_valid_tile(settler_row, settler_col)) {
        reveal_around(settler_row, settler_col, 1);
    }
    settler_selected = false;
    set_status_message("Turn %d. Gold: %d. Cities: %d", turn, gold, city_count);
}

void move_settler(int row, int col) {
    if (!is_valid_tile(row, col)) {
        set_status_message("Invalid tile!");
        return;
    }
    if (terrain[row][col] == TERRAIN_OCEAN || terrain[row][col] == TERRAIN_MOUNTAIN) {
        set_status_message("Can't move to that terrain!");
        return;
    }
    if (city_map[row][col]) {
        set_status_message("A city is already there!");
        return;
    }

    settler_row = row;
    settler_col = col;
    reveal_around(row, col, 1);
    set_status_message("✅ Settler moved to (%d,%d). Press 'f' to found city.", row, col);
    settler_selected = false;
}

void open_production_menu() {
    show_production_menu = true;
    production_choice = 0;
    set_status_message("Choose city improvement: Use w/s, Enter to confirm.");
}

void finalize_city_founding(int row, int col) {
    city_map[row][col] = true;
    city_count++;
    has_settler = false;
    show_production_menu = false;
    set_status_message("🎉 City founded! Built: %s", production_options[production_choice]);
}

// --- Status Message ---
void set_status_message(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    strncpy(status_message, buffer, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

// --- Rendering ---
void render_grid() {
    clear_screen();
    // Print column headers (0 to GRID_COLS-1)
    printf("   ");
    for (int c = 0; c < GRID_COLS; c++) {
        printf("  %2d   ", c);
    }
    printf("\n");

    // Print top border
    printf("  +");
    for (int c = 0; c < GRID_COLS; c++) {
        printf("------+");
    }
    printf("\n");

    // Print grid
    for (int r = 0; r < GRID_ROWS; r++) {
        // Row label (A, B, C, ...)
        printf("%c |", 'A' + r);
        for (int c = 0; c < GRID_COLS; c++) {
            bool is_cursor = debug_show_cursor && r == cursor_row && c == cursor_col;
            const char* symbol = "     "; // Default empty

            if (debug_fog_enabled && !explored[r][c]) {
                symbol = EMOJI_FOG;
            } else if (debug_terrain_enabled) {
                if (city_map[r][c]) {
                    symbol = EMOJI_CITY;
                } else if (has_settler && r == settler_row && c == settler_col) {
                    symbol = EMOJI_SETTLER;
                } else {
                    switch (terrain[r][c]) {
                        case TERRAIN_GRASS:   symbol = EMOJI_LAND; break;
                        case TERRAIN_FOREST:  symbol = EMOJI_FOREST; break;
                        case TERRAIN_MOUNTAIN:symbol = EMOJI_MOUNTAIN; break;
                        case TERRAIN_RIVER:   symbol = EMOJI_RIVER; break;
                        case TERRAIN_OCEAN:   symbol = EMOJI_OCEAN; break;
                    }
                }
            } else {
                if (city_map[r][c]) {
                    symbol = EMOJI_CITY;
                } else if (has_settler && r == settler_row && c == settler_col) {
                    symbol = EMOJI_SETTLER;
                }
            }

            if (is_cursor) {
                printf("[%4s ]", symbol);
            } else {
                printf(" %4s |", symbol);
            }
        }
        printf("\n");

        // Print row separator
        printf("  +");
        for (int c = 0; c < GRID_COLS; c++) {
            printf("------+");
        }
        printf("\n");
    }

    // Print UI
    printf("\nTurn: %d | Gold: %d | Cities: %d\n", turn, gold, city_count);
    printf("Controls: Arrow keys (w/a/s/d), Enter to select, f to found city, Space to end turn\n");
    printf("Debug: g (fog), t (terrain), k (cursor)\n");
    printf("Status: %s\n", status_message);

    // Production menu
    if (show_production_menu) {
        printf("\nChoose Improvement:\n");
        for (int i = 0; i < 3; i++) {
            printf("%s %s\n", i == production_choice ? ">" : " ", production_options[i]);
        }
    }
}

// --- Clear Screen (Cross-Platform) ---
void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// --- Input Handling ---
void process_input() {
    char input;
    printf("Enter command: ");
    input = getchar();
    while (getchar() != '\n'); // Clear input buffer

    if (show_production_menu) {
        if (input == '\n' || input == '\r') {
            finalize_city_founding(settler_row, settler_col);
            return;
        } else if (input == 'w' && production_choice > 0) {
            production_choice--;
        } else if (input == 's' && production_choice < 2) {
            production_choice++;
        }
        return;
    }

    switch (input) {
        case 'w': if (cursor_row > 0) cursor_row--; break;
        case 's': if (cursor_row < GRID_ROWS - 1) cursor_row++; break;
        case 'a': if (cursor_col > 0) cursor_col--; break;
        case 'd': if (cursor_col < GRID_COLS - 1) cursor_col++; break;
        case '\n': case '\r': // Enter key
            if (!has_settler) {
                set_status_message("No settler to control.");
            } else if (!settler_selected && cursor_row == settler_row && cursor_col == settler_col) {
                settler_selected = true;
                set_status_message("✅ Settler selected. Move cursor and press Enter to move.");
            } else if (settler_selected) {
                int dr = abs(cursor_row - settler_row);
                int dc = abs(cursor_col - settler_col);
                if (dr + dc == 1 && is_land(cursor_row, cursor_col) && !city_map[cursor_row][cursor_col]) {
                    move_settler(cursor_row, cursor_col);
                } else if (cursor_row == settler_row && cursor_col == settler_col) {
                    set_status_message("Settler already here. Move and press Enter.");
                } else {
                    set_status_message("❌ Can't move there. Must be adjacent and valid.");
                }
            } else {
                set_status_message("Go to settler and press Enter to select.");
            }
            break;
        case 'f': case 'F':
            if (has_settler && cursor_row == settler_row && cursor_col == settler_col) {
                open_production_menu();
            } else if (has_settler && is_land(cursor_row, cursor_col) && !city_map[cursor_row][cursor_col]) {
                move_settler(cursor_row, cursor_col);
            } else {
                set_status_message("Can't found city here.");
            }
            break;
        case ' ': // Space to end turn
            end_turn();
            break;
        case 'g': case 'G':
            debug_fog_enabled = !debug_fog_enabled;
            set_status_message("Fog: %s", debug_fog_enabled ? "ON" : "OFF");
            break;
        case 't': case 'T':
            debug_terrain_enabled = !debug_terrain_enabled;
            set_status_message("Terrain: %s", debug_terrain_enabled ? "ON" : "OFF");
            break;
        case 'k': case 'K':
            debug_show_cursor = !debug_show_cursor;
            set_status_message("Cursor: %s", debug_show_cursor ? "ON" : "OFF");
            break;
        case 'q': case 'Q':
            exit(0);
            break;
    }
}

int main() {
    generate_world();
    printf("Controls:\n");
    printf(" - w/a/s/d: Move cursor\n");
    printf(" - Enter: Select settler, then move & Enter to act\n");
    printf(" - f: Found city\n");
    printf(" - Space: End turn\n");
    printf(" - g/t/k: Toggle fog/terrain/cursor\n");
    printf(" - q: Quit\n");

    while (true) {
        render_grid();
        process_input();
    }

    return 0;
}
