#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define BOARD_SIZE 8

// Structure to hold piece info
typedef struct {
    int x, y;
    char emoji[5]; // UTF-8 emoji (1–4 bytes)
} Piece;

// Board: each cell holds an emoji (or blank)
char board[BOARD_SIZE][BOARD_SIZE][5]; // [y][x][emoji]

// Helper: Read emoji from state.txt
void get_emoji_from_state(const char* dir_name, char* emoji_out) {
    char filename[150];
    sprintf(filename, "%s/state.txt", dir_name);

    FILE* f = fopen(filename, "r");
    if (!f) {
        strcpy(emoji_out, "?");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "symbol :", 8) == 0) {
            char* em = line + 8;
            while (*em == ' ') em++;
            int len = 0;
            while (em[len] && em[len] != '\n' && em[len] != '\r' && len < 4) len++;
            if (len >= 1 && len <= 4) {
                strncpy(emoji_out, em, len);
                emoji_out[len] = '\0';
                fclose(f);
                return;
            }
        }
    }
    fclose(f);
    strcpy(emoji_out, "?");
}

// Clear board
void init_board() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            strcpy(board[i][j], "  "); // default empty
        }
    }
}

// Set piece at x,y
void place_piece(int x, int y, const char* emoji) {
    if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) {
        strcpy(board[y][x], emoji);
    }
}

// Read x,y from state.txt
int read_position(const char* dir_name, int* x, int* y) {
    char filename[150];
    sprintf(filename, "%s/state.txt", dir_name);

    FILE* f = fopen(filename, "r");
    if (!f) return 0;

    char line[256];
    int has_x = 0, has_y = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "x:%d", x) == 1) has_x = 1;
        if (sscanf(line, "y:%d", y) == 1) has_y = 1;
    }
    fclose(f);

    return has_x && has_y;
}

// Render board to file
void render_to_file() {
    FILE* f = fopen("1rst_render_debug.txt", "w");
    if (!f) {
        printf("Error: Cannot create 1rst_render_debug.txt\n");
        return;
    }

    // Header: numbers
    fprintf(f, "       ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        fprintf(f, "      %d", j);
    }
    fprintf(f, "\n");

    // Board
    for (int i = 0; i < BOARD_SIZE; i++) {
        fprintf(f, "%c ", 'A' + i); // Row label

        // Top border
        for (int j = 0; j < BOARD_SIZE; j++) {
            fprintf(f, "+-----");
        }
        fprintf(f, "+\n");

        // Cell content
        fprintf(f, "%c|", 'A' + i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            fprintf(f, "  %s  |", board[i][j]);
        }
        fprintf(f, "\n");
    }

    // Bottom border
    fprintf(f, " ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        fprintf(f, "+-----");
    }
    fprintf(f, "+\n");

    fclose(f);
    printf("Board rendered to 1rst_render_debug.txt\n");
}

int main() {
    DIR* d = opendir(".");
    if (!d) {
        printf("Error: Cannot open current directory.\n");
        return 1;
    }

    struct dirent* dir;
    init_board(); // clear board

    while ((dir = readdir(d)) != NULL) {
        if (strstr(dir->d_name, ".dir") != NULL) {
            char base_name[100];
            strncpy(base_name, dir->d_name, strlen(dir->d_name) - 4);
            base_name[strlen(dir->d_name) - 4] = '\0';

            char full_dir[150];
            sprintf(full_dir, "%s.dir", base_name);

            int x = -1, y = -1;
            if (read_position(full_dir, &x, &y)) {
                char emoji[5] = "❓";
                get_emoji_from_state(full_dir, emoji);
                place_piece(x, y, emoji);
            }
        }
    }

    closedir(d);
    render_to_file();
    return 0;
}
