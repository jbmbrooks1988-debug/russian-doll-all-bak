#ifndef MODEL_GL_H
#define MODEL_GL_H

#include <stdbool.h>

#define GRID_ROWS 8
#define GRID_COLS 8
#define NUM_CANDY_TYPES 6
#define TILE_SIZE 64
#define MATCH_MIN 3

extern const char *candy_emojis[NUM_CANDY_TYPES];
extern int grid[GRID_ROWS][GRID_COLS];
extern bool is_animating;
extern int score;
extern int window_width;
extern int window_height;
extern int selected_row;
extern int selected_col;
extern char status_message[256];

void fill_grid();
void swap_candies(int r1, int c1, int r2, int c2);
bool find_matches();
void remove_matches();
void drop_candies();
bool is_adjacent(int r1, int c1, int r2, int c2);
void set_status_message(const char* msg);

#endif
