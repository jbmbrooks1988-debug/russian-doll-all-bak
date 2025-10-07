#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

// --- Globals ---

#define CANVAS_ROWS 16
#define CANVAS_COLS 16
#define NUM_EMOJIS 8
#define NUM_COLORS 8
#define MAX_LAYERS 2
#define MAX_TABS 10

#define KEY_UP 1000
#define KEY_DOWN 1001
#define KEY_LEFT 1002
#define KEY_RIGHT 1003

const char *emojis[NUM_EMOJIS] = {"🍒", "🍋", "🍊", "💎", "🔔", "💩", "🎨", "🗡"};
const char *colors[NUM_COLORS][3] = {
    {"255", "0", "0"},    // Red
    {"0", "255", "0"},    // Green
    {"0", "0", "255"},    // Blue
    {"255", "255", "0"},  // Yellow
    {"0", "255", "255"},  // Cyan
    {"255", "0", "255"},  // Magenta
    {"255", "255", "255"},// White
    {"0", "0", "0"}       // Black
};
const char *color_names[NUM_COLORS] = {"Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White", "Black"};
typedef struct { int emoji_idx; int fg_color; int bg_color; } Tile;
Tile canvas[MAX_LAYERS][CANVAS_ROWS][CANVAS_COLS];
Tile tab_bank[MAX_TABS];
int tab_count = 0;

char status_message[256] = "Select tool and paint!";

int selected_emoji = 0;
int selected_fg_color = 0;
int selected_bg_color = 7; // Black default
int selected_tool = 0; // 0: Paint, 1: Fill, 2: Rectangle
int start_row = -1, start_col = -1; // For rectangle tool
int selector_row = 0, selector_col = 0; // Tile selector position
bool show_all_layers = true; // Terminal layer toggle

// --- Function Prototypes ---

void set_status_message(const char* msg);
void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg);
void draw_rectangle(int layer, int r1, int c1, int r2, int c2);
void save_canvas();
void load_canvas();
void print_ascii_grid();
int check_terminal_input();
void process_input(int ch);
void init();

// --- Game Logic ---

void set_status_message(const char* msg) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg) {
    if (r < 0 || r >= CANVAS_ROWS || c < 0 || c >= CANVAS_COLS) return;
    if (canvas[layer][r][c].emoji_idx != old_emoji || canvas[layer][r][c].fg_color != old_fg || canvas[layer][r][c].bg_color != old_bg) return;
    canvas[layer][r][c].emoji_idx = selected_emoji;
    canvas[layer][r][c].fg_color = selected_fg_color;
    canvas[layer][r][c].bg_color = selected_bg_color;
    flood_fill(layer, r+1, c, old_emoji, old_fg, old_bg);
    flood_fill(layer, r-1, c, old_emoji, old_fg, old_bg);
    flood_fill(layer, r, c+1, old_emoji, old_fg, old_bg);
    flood_fill(layer, r, c-1, old_emoji, old_fg, old_bg);
}

void draw_rectangle(int layer, int r1, int c1, int r2, int c2) {
    int r_min = r1 < r2 ? r1 : r2;
    int r_max = r1 > r2 ? r1 : r2;
    int c_min = c1 < c2 ? c1 : c2;
    int c_max = c1 > c2 ? c1 : c2;
    for (int r = r_min; r <= r_max; r++) {
        for (int c = c_min; c <= c_max; c++) {
            if (r >= 0 && r < CANVAS_ROWS && c >= 0 && c < CANVAS_COLS) {
                if (selected_tool == 2) { // Outline
                    if (r == r_min || r == r_max || c == c_min || c == c_max) {
                        canvas[layer][r][c].emoji_idx = selected_emoji;
                        canvas[layer][r][c].fg_color = selected_fg_color;
                        canvas[layer][r][c].bg_color = selected_bg_color;
                    }
                } else { // Filled
                    canvas[layer][r][c].emoji_idx = selected_emoji;
                    canvas[layer][r][c].fg_color = selected_fg_color;
                    canvas[layer][r][c].bg_color = selected_bg_color;
                }
            }
        }
    }
}

void save_canvas() {
    FILE *fp = fopen("debug.csv", "w");
    if (!fp) {
        set_status_message("Error: Could not save to debug.csv");
        fprintf(stderr, "Error: Could not open debug.csv for writing\n");
        return;
    }
    fprintf(fp, "emoji_idx,fg_color_idx,bg_color_idx,layer\n");
    for (int layer = 0; layer < MAX_LAYERS; layer++) {
        for (int r = 0; r < CANVAS_ROWS; r++) {
            for (int c = 0; c < CANVAS_COLS; c++) {
                fprintf(fp, "%d,%d,%d,%d",
                        canvas[layer][r][c].emoji_idx,
                        canvas[layer][r][c].fg_color,
                        canvas[layer][r][c].bg_color,
                        layer);
                if (c < CANVAS_COLS - 1 || r < CANVAS_ROWS - 1 || layer < MAX_LAYERS - 1) {
                    fprintf(fp, "\n");
                }
            }
        }
    }
    fclose(fp);
    set_status_message("Saved to debug.csv");
}

void load_canvas() {
    FILE *fp = fopen("debug.csv", "r");
    if (!fp) {
        set_status_message("Error: Could not load debug.csv");
        fprintf(stderr, "Error: Could not open debug.csv for reading\n");
        return;
    }

    // Clear canvas before loading
    for (int layer = 0; layer < MAX_LAYERS; layer++) {
        for (int r = 0; r < CANVAS_ROWS; r++) {
            for (int c = 0; c < CANVAS_COLS; c++) {
                canvas[layer][r][c].emoji_idx = -1;
                canvas[layer][r][c].fg_color = 0;
                canvas[layer][r][c].bg_color = 7;
            }
        }
    }

    char line[256];
    // Skip header
    if (!fgets(line, sizeof(line), fp)) {
        set_status_message("Error: Empty or invalid debug.csv");
        fclose(fp);
        return;
    }

    int expected_entries = MAX_LAYERS * CANVAS_ROWS * CANVAS_COLS;
    int loaded_entries = 0;

    while (fgets(line, sizeof(line), fp) && loaded_entries < expected_entries) {
        int emoji_idx, fg_color_idx, bg_color_idx, layer;
        int parsed = sscanf(line, "%d,%d,%d,%d", &emoji_idx, &fg_color_idx, &bg_color_idx, &layer);
        if (parsed != 4) {
            fprintf(stderr, "Warning: Invalid line in debug.csv: %s", line);
            continue;
        }
        // Validate indices
        if (layer < 0 || layer >= MAX_LAYERS ||
            emoji_idx < -1 || emoji_idx >= NUM_EMOJIS ||
            fg_color_idx < 0 || fg_color_idx >= NUM_COLORS ||
            bg_color_idx < 0 || bg_color_idx >= NUM_COLORS) {
            fprintf(stderr, "Warning: Invalid data in debug.csv: emoji=%d, fg=%d, bg=%d, layer=%d\n",
                    emoji_idx, fg_color_idx, bg_color_idx, layer);
            continue;
        }
        int idx = loaded_entries;
        int l = layer;
        int r = (idx / CANVAS_COLS) % CANVAS_ROWS;
        int c = idx % CANVAS_COLS;
        if (l >= 0 && l < MAX_LAYERS && r >= 0 && r < CANVAS_ROWS && c >= 0 && c < CANVAS_COLS) {
            canvas[l][r][c].emoji_idx = emoji_idx;
            canvas[l][r][c].fg_color = fg_color_idx;
            canvas[l][r][c].bg_color = bg_color_idx;
            loaded_entries++;
        }
    }

    fclose(fp);
    if (loaded_entries < expected_entries) {
        fprintf(stderr, "Warning: Loaded %d entries, expected %d\n", loaded_entries, expected_entries);
        set_status_message("Warning: Incomplete load from debug.csv");
    } else {
        set_status_message("Loaded from debug.csv");
    }
    print_ascii_grid();
}

// --- ASCII Rendering ---

void print_ascii_grid() {
    printf("\033[H\033[J"); // Clear terminal
    printf("Emoji Paint (%s Layers)\n", show_all_layers ? "All" : "Top");
    printf("----------------------------------------\n");
    for (int r = 0; r < CANVAS_ROWS; r++) {
        for (int c = 0; c < CANVAS_COLS; c++) {
            bool drawn = false;
            if (selected_tool != 2 && r == selector_row && c == selector_col) {
                printf("\033[1;33m[]\033[0m "); // Yellow brackets for selector
                drawn = true;
            } else {
                for (int layer = MAX_LAYERS - 1; layer >= 0; layer--) {
                    if (!show_all_layers && layer != MAX_LAYERS - 1) continue;
                    if (canvas[layer][r][c].emoji_idx != -1) {
                        printf("\033[38;2;%s;%s;%sm%s\033[0m ",
                               colors[canvas[layer][r][c].fg_color][0],
                               colors[canvas[layer][r][c].fg_color][1],
                               colors[canvas[layer][r][c].fg_color][2],
                               emojis[canvas[layer][r][c].emoji_idx]);
                        drawn = true;
                        break;
                    }
                }
            }
            if (!drawn && !(r == selector_row && c == selector_col)) printf("  ");
        }
        printf("|\n");
    }
    if (selected_tool == 2 && start_row != -1) {
        printf("Rectangle Start: (%d, %d)\n", start_row, start_col);
    }
    printf("----------------------------------------\n");
    printf("Emoji Palette: ");
    for (int i = 0; i < NUM_EMOJIS; i++) {
        if (i == selected_emoji) printf("[");
        printf("%s", emojis[i]);
        if (i == selected_emoji) printf("]");
        printf(" ");
    }
    printf("\nColor Palette: ");
    for (int i = 0; i < NUM_COLORS; i++) {
        if (i == selected_fg_color) printf("[");
        printf("%s", color_names[i]);
        if (i == selected_fg_color) printf("]");
        printf(" ");
    }
    printf("\nTab Bank: ");
    for (int i = 0; i < tab_count; i++) {
        printf("\033[38;2;%s;%s;%sm%s\033[0m ",
               colors[tab_bank[i].fg_color][0],
               colors[tab_bank[i].fg_color][1],
               colors[tab_bank[i].fg_color][2],
               emojis[tab_bank[i].emoji_idx]);
    }
    printf("\nTool: %s  Emoji: %s  FG: %s  BG: %s  Selector: (%d, %d)\n",
           selected_tool == 0 ? "Paint" : (selected_tool == 1 ? "Fill" : "Rectangle"),
           emojis[selected_emoji], color_names[selected_fg_color], color_names[selected_bg_color],
           selector_row, selector_col);
    printf("%s\n", status_message);
    printf("SPACE: Paint  F: Fill  R: Rect  1: Emoji  C: Color  2: Layer  S: Save  L: Load  T: Tab  Q: Quit\n");
    printf("Arrows: Move Selector  Enter: Place Tile\n");
}

// --- Terminal Input ---

int check_terminal_input() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    fd_set set;
    struct timeval timeout;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int ready = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);
    int ch = -1;
    if (ready > 0) {
        ch = getchar();
        // Check for arrow keys (escape sequences)
        if (ch == 27) { // ESC
            ch = getchar();
            if (ch == 91) { // [
                ch = getchar();
                switch (ch) {
                    case 65: ch = KEY_UP; break; // Up arrow
                    case 66: ch = KEY_DOWN; break; // Down arrow
                    case 67: ch = KEY_RIGHT; break; // Right arrow
                    case 68: ch = KEY_LEFT; break; // Left arrow
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

void process_input(int ch) {
    if (ch == ' ') {
        if (selected_tool == 0) {
            canvas[MAX_LAYERS-1][selector_row][selector_col].emoji_idx = selected_emoji;
            canvas[MAX_LAYERS-1][selector_row][selector_col].fg_color = selected_fg_color;
            canvas[MAX_LAYERS-1][selector_row][selector_col].bg_color = selected_bg_color;
            set_status_message("Tile painted");
        } else if (selected_tool == 1) {
            flood_fill(MAX_LAYERS-1, selector_row, selector_col,
                       canvas[MAX_LAYERS-1][selector_row][selector_col].emoji_idx,
                       canvas[MAX_LAYERS-1][selector_row][selector_col].fg_color,
                       canvas[MAX_LAYERS-1][selector_row][selector_col].bg_color);
            set_status_message("Area filled");
        } else if (selected_tool == 2) {
            if (start_row == -1) {
                start_row = selector_row;
                start_col = selector_col;
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(MAX_LAYERS-1, start_row, start_col, selector_row, selector_col);
                start_row = -1;
                start_col = -1;
                set_status_message("Rectangle drawn");
            }
        }
    } else if (ch == '\r' || ch == '\n') { // Enter key
        if (selected_tool == 0) {
            canvas[MAX_LAYERS-1][selector_row][selector_col].emoji_idx = selected_emoji;
            canvas[MAX_LAYERS-1][selector_row][selector_col].fg_color = selected_fg_color;
            canvas[MAX_LAYERS-1][selector_row][selector_col].bg_color = selected_bg_color;
            set_status_message("Tile painted");
        } else if (selected_tool == 1) {
            flood_fill(MAX_LAYERS-1, selector_row, selector_col,
                       canvas[MAX_LAYERS-1][selector_row][selector_col].emoji_idx,
                       canvas[MAX_LAYERS-1][selector_row][selector_col].fg_color,
                       canvas[MAX_LAYERS-1][selector_row][selector_col].bg_color);
            set_status_message("Area filled");
        } else if (selected_tool == 2) {
            if (start_row == -1) {
                start_row = selector_row;
                start_col = selector_col;
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(MAX_LAYERS-1, start_row, start_col, selector_row, selector_col);
                start_row = -1;
                start_col = -1;
                set_status_message("Rectangle drawn");
            }
        }
    } else if (ch == 'f' || ch == 'F') {
        selected_tool = 1;
        start_row = -1;
        start_col = -1;
        set_status_message("Fill tool selected");
    } else if (ch == 'r' || ch == 'R') {
        selected_tool = 2;
        start_row = -1;
        start_col = -1;
        set_status_message("Rectangle tool selected");
    } else if (ch == '1') {
        selected_emoji = (selected_emoji + 1) % NUM_EMOJIS;
        set_status_message("Emoji selected");
    } else if (ch == 'c' || ch == 'C') {
        selected_fg_color = (selected_fg_color + 1) % NUM_COLORS;
        set_status_message("Color selected");
    } else if (ch == '2') {
        show_all_layers = !show_all_layers;
        set_status_message(show_all_layers ? "Showing all layers" : "Showing top layer");
    } else if (ch == 's' || ch == 'S') {
        save_canvas();
    } else if (ch == 'l' || ch == 'L') {
        load_canvas();
    } else if ((ch == 't' || ch == 'T') && tab_count < MAX_TABS) {
        tab_bank[tab_count].emoji_idx = selected_emoji;
        tab_bank[tab_count].fg_color = selected_fg_color;
        tab_bank[tab_count].bg_color = selected_bg_color;
        tab_count++;
        set_status_message("Tab created");
    } else if (ch == 'q' || ch == 'Q') {
        printf("\033[H\033[J"); // Clear terminal
        printf("Thanks for using Emoji Paint!\n");
        exit(0);
    } else if (ch == KEY_UP) { // Up arrow
        if (selector_row > 0) selector_row--;
        set_status_message("Selector moved");
    } else if (ch == KEY_DOWN) { // Down arrow
        if (selector_row < CANVAS_ROWS - 1) selector_row++;
        set_status_message("Selector moved");
    } else if (ch == KEY_LEFT) { // Left arrow
        if (selector_col > 0) selector_col--;
        set_status_message("Selector moved");
    } else if (ch == KEY_RIGHT) { // Right arrow
        if (selector_col < CANVAS_COLS - 1) selector_col++;
        set_status_message("Selector moved");
    }
}

// --- Main ---

void init() {
    srand(time(NULL));
    for (int layer = 0; layer < MAX_LAYERS; layer++) {
        for (int r = 0; r < CANVAS_ROWS; r++) {
            for (int c = 0; c < CANVAS_COLS; c++) {
                canvas[layer][r][c].emoji_idx = -1;
                canvas[layer][r][c].fg_color = 0;
                canvas[layer][r][c].bg_color = 7;
            }
        }
    }
    print_ascii_grid();
}

int main() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    init();

    // Main loop
    while (1) {
        int ch = check_terminal_input();
        if (ch != -1) {
            process_input(ch);
            print_ascii_grid();
        }
        usleep(10000); // Small delay to reduce CPU usage
    }

    return 0;
}