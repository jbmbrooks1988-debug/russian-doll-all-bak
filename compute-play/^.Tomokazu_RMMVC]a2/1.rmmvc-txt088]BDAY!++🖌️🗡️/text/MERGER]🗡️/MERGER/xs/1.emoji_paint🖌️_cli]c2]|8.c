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
#define NUM_EMOJIS 100  // Increased to accommodate all emojis from file
#define NUM_COLORS 8
#define MAX_LAYERS 2
#define MAX_TABS 10

#define KEY_UP 1000
#define KEY_DOWN 1001
#define KEY_LEFT 1002
#define KEY_RIGHT 1003

char emojis[NUM_EMOJIS][5];  // Store emojis as strings (up to 4 chars + null terminator)
int emoji_count = 0;  // Actual number of emojis loaded
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
const char *tool_names[3] = {"Paint", "Fill", "Rectangle"};
typedef struct { int emoji_idx; int fg_color; int bg_color; } Tile;
Tile canvas[MAX_LAYERS][CANVAS_ROWS][CANVAS_COLS];
Tile tab_bank[MAX_TABS];
int tab_count = 0;

char status_message[256] = "Select tool and paint!";

int selected_emoji = 1;  // 1 = first actual emoji (cherry)
int selected_fg_color = 0;
int selected_bg_color = 7; // Black default
int selected_tool = 0; // 0: Paint, 1: Fill, 2: Rectangle
int start_row = -1, start_col = -1; // For rectangle tool
int selector_row = 0, selector_col = 0; // Tile selector position
bool show_all_layers = true; // Terminal layer toggle
int menu_selection = 0; // 0: Canvas, 1: Emoji Palette, 2: Color Palette, 3: Tool

// --- Function Prototypes ---

void load_emojis_from_file();
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

// Function to load emojis from file
void load_emojis_from_file() {
    FILE *fp = fopen("emojis.txt", "r");
    if (!fp) {
        // Fallback to hardcoded emojis if file not found
        strcpy(emojis[0], " ");   // Blank space
        strcpy(emojis[1], "🍒");
        strcpy(emojis[2], "🍋");
        strcpy(emojis[3], "🍊");
        strcpy(emojis[4], "💎");
        strcpy(emojis[5], "🔔");
        strcpy(emojis[6], "💩");
        strcpy(emojis[7], "🎨");
        emoji_count = 8;
        return;
    }
    
    char line[10];
    emoji_count = 0;
    // First emoji should be blank
    strcpy(emojis[0], " ");
    emoji_count = 1;
    
    while (fgets(line, sizeof(line), fp) && emoji_count < NUM_EMOJIS) {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            strcpy(emojis[emoji_count], line);
            emoji_count++;
        }
    }
    fclose(fp);
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
    
    // Write header in the new format
    fprintf(fp, "x,y,z,emoji_idx,fg_color_idx,bg_color_idx,scale,rel_x,rel_y,rel_z\n");
    
    // Write scale info line
    fprintf(fp, "scale_info,0,0,0,0,0,1.90,0,0,0\n");
    
    // Write tile data for ALL tiles (including empty ones)
    for (int layer = 0; layer < MAX_LAYERS; layer++) {
        for (int r = 0; r < CANVAS_ROWS; r++) {
            for (int c = 0; c < CANVAS_COLS; c++) {
                // Save emoji indices directly (0 = blank, 1 = cherry, 2 = lemon, etc.)
                int emoji_idx = canvas[layer][r][c].emoji_idx;
                
                // Calculate relative coordinates
                float rel_x = (float)c * 0.1f;
                float rel_y = (float)r * 0.1f;
                float rel_z = (float)layer * 0.1f;
                
                fprintf(fp, "%d,%d,%d,%d,%d,%d,1.90,%.2f,%.2f,%.2f\n",
                        c, r, layer,
                        emoji_idx,
                        canvas[layer][r][c].fg_color,
                        canvas[layer][r][c].bg_color,
                        rel_x, rel_y, rel_z);
            }
        }
    }
    
    fclose(fp);
    set_status_message("Saved to debug.csv in new format");
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
                canvas[layer][r][c].emoji_idx = 0;  // 0 = blank
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

    // Skip scale info line
    if (!fgets(line, sizeof(line), fp)) {
        set_status_message("Error: Missing scale info in debug.csv");
        fclose(fp);
        return;
    }

    // Load tile data
    while (fgets(line, sizeof(line), fp)) {
        int x, y, z, emoji_idx, fg_color_idx, bg_color_idx;
        float scale, rel_x, rel_y, rel_z;
        
        // Try to parse the line with the new format
        int parsed = sscanf(line, "%d,%d,%d,%d,%d,%d,%f,%f,%f,%f", 
                           &x, &y, &z, &emoji_idx, &fg_color_idx, &bg_color_idx,
                           &scale, &rel_x, &rel_y, &rel_z);
        
        // If parsing failed, try the old format
        if (parsed != 10) {
            parsed = sscanf(line, "%d,%d,%d,%d", &emoji_idx, &fg_color_idx, &bg_color_idx, &z);
            if (parsed == 4) {
                // Old format - need to determine x,y from line position
                // This is a simplified approach - in practice, you might want a more robust solution
                static int loaded_entries = 0;
                x = loaded_entries % CANVAS_COLS;
                y = (loaded_entries / CANVAS_COLS) % CANVAS_ROWS;
                loaded_entries++;
            } else {
                fprintf(stderr, "Warning: Invalid line in debug.csv: %s", line);
                continue;
            }
        }
        
        // Validate indices
        if (z < 0 || z >= MAX_LAYERS ||
            emoji_idx < 0 || emoji_idx >= NUM_EMOJIS ||  // Changed from -1 to 0
            fg_color_idx < 0 || fg_color_idx >= NUM_COLORS ||
            bg_color_idx < 0 || bg_color_idx >= NUM_COLORS) {
            fprintf(stderr, "Warning: Invalid data in debug.csv: emoji=%d, fg=%d, bg=%d, layer=%d\n",
                    emoji_idx, fg_color_idx, bg_color_idx, z);
            continue;
        }
        
        // Validate coordinates
        if (x >= 0 && x < CANVAS_COLS && y >= 0 && y < CANVAS_ROWS) {
            canvas[z][y][x].emoji_idx = emoji_idx;
            canvas[z][y][x].fg_color = fg_color_idx;
            canvas[z][y][x].bg_color = bg_color_idx;
        }
    }

    fclose(fp);
    set_status_message("Loaded from debug.csv");
    print_ascii_grid();
}

// --- ASCII Rendering ---

void print_ascii_grid() {
    printf("\033[H\033[J"); // Clear terminal
    printf("Emoji Paint (%s Layers)\n", show_all_layers ? "All" : "Top");
    printf("----------------------------------------\n");
    printf("0. Canvas: ");
    if (menu_selection == 0) printf("\033[1;33m[Active]\033[0m\n");
    else printf("Active\n");
    for (int r = 0; r < CANVAS_ROWS; r++) {
        for (int c = 0; c < CANVAS_COLS; c++) {
            bool drawn = false;
            if (selected_tool != 2 && r == selector_row && c == selector_col && menu_selection == 0) {
                printf("\033[1;33m[]\033[0m "); // Yellow brackets for selector
                drawn = true;
            } else {
                for (int layer = MAX_LAYERS - 1; layer >= 0; layer--) {
                    if (!show_all_layers && layer != MAX_LAYERS - 1) continue;
                    if (canvas[layer][r][c].emoji_idx != 0) {  // 0 = blank, so only render non-blank emojis
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
            if (!drawn && !(r == selector_row && c == selector_col && menu_selection == 0)) printf("  ");
        }
        printf("|\n");
    }
    if (selected_tool == 2 && start_row != -1) {
        printf("Rectangle Start: (%d, %d)\n", start_row, start_col);
    }
    printf("----------------------------------------\n");
    printf("1. Emoji Palette: ");
    // Show only 8 emojis at a time with navigation
    int start_idx = (selected_emoji / 8) * 8;  // Start of current page
    int end_idx = start_idx + 8;
    if (end_idx > emoji_count) end_idx = emoji_count;
    
    // Show left arrow if there are more emojis to the left
    if (start_idx > 0) {
        printf("<- ");
    }
    
    for (int i = start_idx; i < end_idx; i++) {
        if (menu_selection == 1 && i == selected_emoji) {
            printf("\033[1;33m[%d:%s]\033[0m ", i, emojis[i]);
        } else if (i == selected_emoji) {
            printf("[%d:%s] ", i, emojis[i]);
        } else {
            printf("%d:%s ", i, emojis[i]);
        }
    }
    
    // Show right arrow if there are more emojis to the right
    if (end_idx < emoji_count) {
        printf("->");
    }
    printf("\n2. Color Palette: ");
    for (int i = 0; i < NUM_COLORS; i++) {
        if (menu_selection == 2 && i == selected_fg_color) printf("\033[1;33m[");
        else if (i == selected_fg_color) printf("[");
        printf("%d:%s", i + 1, color_names[i]);
        if (menu_selection == 2 && i == selected_fg_color) printf("]\033[0m");
        else if (i == selected_fg_color) printf("]");
        printf(" ");
    }
    printf("\n3. Tool: ");
    for (int i = 0; i < 3; i++) {
        if (menu_selection == 3 && i == selected_tool) printf("\033[1;33m[");
        else if (i == selected_tool) printf("[");
        printf("%d:%s", i + 1, tool_names[i]);
        if (menu_selection == 3 && i == selected_tool) printf("]\033[0m");
        else if (i == selected_tool) printf("]");
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
           tool_names[selected_tool],
           emojis[selected_emoji], color_names[selected_fg_color], color_names[selected_bg_color],
           selector_row, selector_col);
    printf("%s\n", status_message);
    printf("0-3: Select Menu  SPACE: Paint  F: Fill  R: Rect  S: Save  L: Load  T: Tab  Q: Quit\n");
    printf("Arrows: Move Selector (or navigate menu)  Enter: Place Tile or Confirm Selection\n");
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
    if (ch == '0') {
        menu_selection = 0;
        set_status_message("Canvas selected (use arrows to move, Enter to paint)");
    } else if (ch == '1') {
        menu_selection = 1;
        set_status_message("Emoji menu selected (use arrows, Enter to confirm)");
    } else if (ch == '2') {
        menu_selection = 2;
        set_status_message("Color menu selected (use arrows, Enter to confirm)");
    } else if (ch == '3') {
        menu_selection = 3;
        set_status_message("Tool menu selected (use arrows, Enter to confirm)");
    } else if (menu_selection > 0 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == '\r' || ch == '\n')) {
        if (menu_selection == 1) { // Emoji Palette
            if (ch == KEY_LEFT && selected_emoji > 0) {
                selected_emoji--;
                set_status_message("Emoji selected");
            } else if (ch == KEY_RIGHT && selected_emoji < emoji_count - 1) {
                selected_emoji++;
                set_status_message("Emoji selected");
            } else if (ch == '\r' || ch == '\n') {
                menu_selection = 0;
                set_status_message("Emoji confirmed, canvas selected");
            }
        } else if (menu_selection == 2) { // Color Palette
            if (ch == KEY_LEFT && selected_fg_color > 0) {
                selected_fg_color--;
                set_status_message("Color selected");
            } else if (ch == KEY_RIGHT && selected_fg_color < NUM_COLORS - 1) {
                selected_fg_color++;
                set_status_message("Color selected");
            } else if (ch == '\r' || ch == '\n') {
                menu_selection = 0;
                set_status_message("Color confirmed, canvas selected");
            }
        } else if (menu_selection == 3) { // Tool
            if (ch == KEY_LEFT && selected_tool > 0) {
                selected_tool--;
                start_row = -1;
                start_col = -1;
                set_status_message("Tool selected");
            } else if (ch == KEY_RIGHT && selected_tool < 2) {
                selected_tool++;
                start_row = -1;
                start_col = -1;
                set_status_message("Tool selected");
            } else if (ch == '\r' || ch == '\n') {
                menu_selection = 0;
                set_status_message("Tool confirmed, canvas selected");
            }
        }
    } else if (menu_selection == 0 && (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT)) {
        if (ch == KEY_UP && selector_row > 0) {
            selector_row--;
            set_status_message("Selector moved");
        } else if (ch == KEY_DOWN && selector_row < CANVAS_ROWS - 1) {
            selector_row++;
            set_status_message("Selector moved");
        } else if (ch == KEY_LEFT && selector_col > 0) {
            selector_col--;
            set_status_message("Selector moved");
        } else if (ch == KEY_RIGHT && selector_col < CANVAS_COLS - 1) {
            selector_col++;
            set_status_message("Selector moved");
        }
    } else if (ch == ' ' || (ch == '\r' || ch == '\n') && menu_selection == 0) {
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
    } else if (ch == 'c' || ch == 'C') {
        selected_fg_color = (selected_fg_color + 1) % NUM_COLORS;
        set_status_message("Color selected");
    } else if (ch == '2' && menu_selection == 0) {
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
    }
}

// --- Main ---

void init() {
    srand(time(NULL));
    
    // Load emojis from file
    load_emojis_from_file();
    
    for (int layer = 0; layer < MAX_LAYERS; layer++) {
        for (int r = 0; r < CANVAS_ROWS; r++) {
            for (int c = 0; c < CANVAS_COLS; c++) {
                canvas[layer][r][c].emoji_idx = 0;  // 0 = blank
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
