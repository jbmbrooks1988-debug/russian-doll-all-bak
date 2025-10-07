#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define GRID_ROWS 8
#define GRID_COLS 8
#define NUM_CANDY_TYPES 6
#define TILE_SIZE 64
#define MATCH_MIN 3

static int grid[GRID_ROWS][GRID_COLS];
static bool is_animating = false;
static int score = 0;
static int selected_row = -1;
static int selected_col = -1;
static int window_width = GRID_COLS * TILE_SIZE + 20;
static int window_height = GRID_ROWS * TILE_SIZE + 60;
static char status_message[256] = "";
static const char *candy_emojis[NUM_CANDY_TYPES] = {"❤️", "🧡", "💛", "💚", "💙", "💜"};

// Forward declarations
bool find_matches(void);
void remove_matches(void);
void drop_candies(void);

int get_window_width(void) { return window_width; }
int get_window_height(void) { return window_height; }
int get_grid_rows(void) { return GRID_ROWS; }
int get_grid_cols(void) { return GRID_COLS; }
int get_tile_size(void) { return TILE_SIZE; }
int get_grid_value(int r, int c) { return grid[r][c]; }
int get_selected_row(void) { return selected_row; }
int get_selected_col(void) { return selected_col; }
int get_score(void) { return score; }
const char* get_candy_emoji(int type) { return candy_emojis[type]; }
const char* get_status_message(void) { return status_message; }

void set_status_message(const char* msg) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

void fill_grid(void) {
    srand(time(NULL));
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            grid[r][c] = rand() % NUM_CANDY_TYPES;
        }
    }
    while (find_matches()) {
        remove_matches();
        drop_candies();
    }
}

void init(void) {
    fill_grid();
}

bool is_adjacent(int r1, int c1, int r2, int c2) {
    return (abs(r1 - r2) + abs(c1 - c2) == 1);
}

void swap_candies(int r1, int c1, int r2, int c2) {
    int temp = grid[r1][c1];
    grid[r1][c1] = grid[r2][c2];
    grid[r2][c2] = temp;
}

bool find_matches(void) {
    bool has_match = false;
    bool visited[GRID_ROWS][GRID_COLS] = {false};
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (visited[r][c]) continue;
            int type = grid[r][c];
            if (type == -1) continue;
            int count = 1;
            for (int cc = c + 1; cc < GRID_COLS && grid[r][cc] == type; cc++) {
                count++;
            }
            if (count >= MATCH_MIN) {
                has_match = true;
                for (int cc = c; cc < c + count; cc++) {
                    visited[r][cc] = true;
                }
                score += count * 10;
            }
       

 }
    }
    for (int c = 0; c < GRID_COLS; c++) {
        for (int r = 0; r < GRID_ROWS; r++) {
            if (visited[r][c]) continue;
            int type = grid[r][c];
            if (type == -1) continue;
            int count = 1;
            for (int rr = r + 1; rr < GRID_ROWS && grid[rr][c] == type; rr++) {
                count++;
            }
            if (count >= MATCH_MIN) {
                has_match = true;
                for (int rr = r; rr < r + count; rr++) {
                    visited[rr][c] = true;
                }
                score += count * 10;
            }
        }
    }
    return has_match;
}

void remove_matches(void) {
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            int h_count = 1;
            int type = grid[r][c];
            if (type == -1) continue;
            for (int cc = c + 1; cc < GRID_COLS && grid[r][cc] == type; cc++) h_count++;
            if (h_count >= MATCH_MIN) {
                for (int cc = c; cc < c + h_count; cc++) grid[r][cc] = -1;
            }
            int v_count = 1;
            type = grid[r][c];
            for (int rr = r + 1; rr < GRID_ROWS && grid[rr][c] == type; rr++) v_count++;
            if (v_count >= MATCH_MIN) {
                for (int rr = r; rr < r + v_count; rr++) grid[rr][c] = -1;
            }
        }
    }
}

void drop_candies(void) {
    for (int c = 0; c < GRID_COLS; c++) {
        int write_r = GRID_ROWS - 1;
        for (int r = GRID_ROWS - 1; r >= 0; r--) {
            if (grid[r][c] != -1) {
                grid[write_r][c] = grid[r][c];
                if (write_r != r) grid[r][c] = -1;
                write_r--;
            }
        }
        for (int r = write_r; r >= 0; r--) {
            grid[r][c] = rand() % NUM_CANDY_TYPES;
        }
    }
}

bool get_is_animating(void) { return is_animating; }
void set_is_animating(bool value) { is_animating = value; }
void set_selected(int row, int col) { selected_row = row; selected_col = col; }
