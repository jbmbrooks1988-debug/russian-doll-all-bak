#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <locale.h>

#define BOARD_SIZE 8
#define MAX_SPRITES 50
#define MAX_INVENTORY 5
#define MAX_HP 100

// Item type enum
typedef enum { ITEM_TYPE_POTION, ITEM_TYPE_WEAPON } ItemType;

// Item structure
typedef struct {
    char name[50];
    ItemType type;
    int value;
} Item;

// Game state
typedef struct {
    char* board[BOARD_SIZE][BOARD_SIZE];     // piece names
    int player_board[BOARD_SIZE][BOARD_SIZE]; // 1 or 2, or 0 if empty
    int current_turn;                         // 1 or 2
    int cursor_x, cursor_y;                   // cursor position
    int selected_x, selected_y;               // selected piece
    int in_menu;                              // showing action menu
    int menu_selection;                       // 0=Stats, 1=Move, 2=Items, 3=Attack
    int show_inventory;                       // showing inventory
    int inventory_count;
    Item inventory[MAX_INVENTORY];
    double player1_timer;
    double player2_timer;
    int game_over;
    int winner;
    char message[100];                        // temporary message
    time_t message_expire;                    // when to clear message
} GameState;

GameState game_state;

// Function declarations
void read_board_state();
char* get_emoji_for_piece(const char* piece_name);
int read_inventory(const char* piece_name, Item inventory[]);
void display();
void change_term_mode();
void restore_term_mode();
char get_key();
void handle_input();

struct termios orig_termios; // for restoring terminal

// Restore terminal on exit
void cleanup() {
    restore_term_mode();
    printf("\n");
}

// Set terminal to raw mode
void change_term_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(cleanup);

    raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Restore normal terminal mode
void restore_term_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Read a key (arrow keys return 'U','D','L','R')
char get_key() {
    char c = getchar();
    if (c == '\x1B') { // Escape
        if (getchar() == '[') {
            c = getchar();
            switch (c) {
                case 'A': return 'U';
                case 'B': return 'D';
                case 'C': return 'R';
                case 'D': return 'L';
            }
        }
        return '\x1B'; // ESC
    }
    return c;
}

// Helper: Read emoji from state.txt
char* get_emoji_for_piece(const char* piece_name) {
    static char emoji[5] = "❓"; // fallback
    char filename[100];
    sprintf(filename, "%s.dir/state.txt", piece_name);

    FILE* f = fopen(filename, "r");
    if (!f) return emoji;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "symbol :", 8) == 0) {
            char* em = line + 8;
            while (*em == ' ') em++;
            int len = 0;
            while (em[len] && em[len] != '\n' && em[len] != '\r') len++;
            if (len > 0 && len < 5) {
                strncpy(emoji, em, len);
                emoji[len] = '\0';
            }
            break;
        }
    }
    fclose(f);
    return emoji;
}

// Read inventory from .dir subfolders
int read_inventory(const char* piece_name, Item inventory[]) {
    char piece_path[100];
    sprintf(piece_path, "%s.dir", piece_name);
    DIR* d = opendir(piece_path);
    if (!d) return 0;

    struct dirent* dir;
    int count = 0;
    while ((dir = readdir(d)) != NULL && count < MAX_INVENTORY) {
        if (strstr(dir->d_name, ".dir") != NULL) {
            char* item_dir_name = strdup(dir->d_name);
            char* base_name = strtok(item_dir_name, ".");
            if (base_name) {
                char item_path[150];
                sprintf(item_path, "%s.dir/%s.dir/stats.txt", piece_name, base_name);
                FILE* f = fopen(item_path, "r");
                if (f) {
                    char type_str[20];
                    int value;
                    fscanf(f, "type:%s\n", type_str);
                    fscanf(f, "value:%d\n", &value);
                    strcpy(inventory[count].name, base_name);
                    inventory[count].type = (strcmp(type_str, "potion") == 0) ? ITEM_TYPE_POTION : ITEM_TYPE_WEAPON;
                    inventory[count].value = value;
                    count++;
                    fclose(f);
                }
            }
            free(item_dir_name);
        }
    }
    closedir(d);
    return count;
}

// Load board from board_state.txt
void read_board_state() {
    FILE* f = fopen("board_state.txt", "r");
    if (!f) {
        printf("Error: board_state.txt not found!\n");
        exit(1);
    }

    char line[256];
    int row = 0;
    while (fgets(line, sizeof(line), f) && row < BOARD_SIZE) {
        char* token = strtok(line, " \n");
        int col = 0;
        while (token && col < BOARD_SIZE) {
            if (strcmp(token, "none") != 0) {
                game_state.board[row][col] = strdup(token);
                game_state.player_board[row][col] = (row < 2) ? 1 : 2;
            } else {
                game_state.board[row][col] = NULL;
                game_state.player_board[row][col] = 0;
            }
            token = strtok(NULL, " \n");
            col++;
        }
        row++;
    }
    fclose(f);
}

// UTF-8 print helper
void print_utf8(const char* s) {
    printf("%s", s);
}

// Display the board
void display() {
    system("clear");

    // Top header: numbers
    printf(" ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("      %d", j);
    }
    printf("\n");

    // Board
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%c ", 'A' + i); // Row label

        // Top border
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("+-----");
        }
        printf("+\n");

        // Cell content
        printf("%c|", 'A' + i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            const char* emoji = "  ";
            if (game_state.board[i][j]) {
                emoji = get_emoji_for_piece(game_state.board[i][j]);
            }

            // Cursor
            if (i == game_state.cursor_y && j == game_state.cursor_x) {
                printf("[");
                print_utf8(emoji);
                printf("] ");
            }
            // Selected
            else if (i == game_state.selected_y && j == game_state.selected_x) {
                printf(" %s* ", emoji);
            }
            else {
                printf("  %s  ", emoji);
            }
            printf("|");
        }
        printf("\n");
    }

    // Bottom border
    printf(" ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("+-----");
    }
    printf("+\n");

    // Info
    printf("Turn: Player %d | P1: %.0fs | P2: %.0fs\n",
           game_state.current_turn,
           game_state.player1_timer,
           game_state.player2_timer);

    if (game_state.message[0]) {
        time_t now = time(NULL);
        if (now >= game_state.message_expire) {
            game_state.message[0] = '\0';
        } else {
            printf("Msg: %s\n", game_state.message);
        }
    }

    if (game_state.game_over) {
        printf("🏆 GAME OVER! Player %d wins! 🏆\n", game_state.winner);
    }

    // Menu
    if (game_state.in_menu) {
        const char* opts[] = {"Stats", "Move", "Items", "Attack"};
        printf("Action: ");
        for (int i = 0; i < 4; i++) {
            if (i == game_state.menu_selection) {
                printf("[%s] ", opts[i]);
            } else {
                printf(" %s  ", opts[i]);
            }
        }
        printf("\n");
    }

    // Inventory
    if (game_state.show_inventory) {
        printf("🎒 Inventory:\n");
        for (int i = 0; i < game_state.inventory_count; i++) {
            printf("  • %s (%s %+d)\n",
                   game_state.inventory[i].name,
                   game_state.inventory[i].type == ITEM_TYPE_POTION ? "heals" : "dmg",
                   game_state.inventory[i].value);
        }
    }

    printf("\n⌨️  Arrows: move | ENTER: select | ESC: cancel\n");
}

// Handle input
void handle_input() {
    if (game_state.game_over) return;

    // AI Turn
    if (game_state.current_turn == 2) {
        // Save board, run AI, reload
        FILE* f = fopen("board_state.txt", "w");
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                fprintf(f, "%s ", game_state.board[i][j] ? game_state.board[i][j] : "none");
            }
            fprintf(f, "\n");
        }
        fclose(f);

        system("./ai_module"); // your AI

        // Reload board
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (game_state.board[i][j]) free(game_state.board[i][j]);
                game_state.board[i][j] = NULL;
            }
        }
        read_board_state();
        game_state.current_turn = 1;
        return;
    }

    char key = get_key();

    if (key == 'U' && game_state.cursor_y > 0) game_state.cursor_y--;
    else if (key == 'D' && game_state.cursor_y < BOARD_SIZE - 1) game_state.cursor_y--;
    else if (key == 'L' && game_state.cursor_x > 0) game_state.cursor_x--;
    else if (key == 'R' && game_state.cursor_x < BOARD_SIZE - 1) game_state.cursor_x++;
    else if (key == '\n') { // Enter
        if (game_state.in_menu) {
            int opt = game_state.menu_selection;
            game_state.in_menu = 0;
            if (opt == 0) {
                sprintf(game_state.message, "Viewing stats for %s", game_state.board[game_state.cursor_y][game_state.cursor_x]);
                game_state.message_expire = time(NULL) + 3;
            } else if (opt == 1) {
                game_state.selected_x = game_state.cursor_x;
                game_state.selected_y = game_state.cursor_y;
            } else if (opt == 2) {
                const char* piece = game_state.board[game_state.cursor_y][game_state.cursor_x];
                game_state.inventory_count = read_inventory(piece, game_state.inventory);
                game_state.show_inventory = 1;
            } else if (opt == 3) {
                sprintf(game_state.message, "Attack mode not implemented");
                game_state.message_expire = time(NULL) + 2;
            }
        } else if (game_state.show_inventory) {
            game_state.show_inventory = 0;
        } else if (game_state.board[game_state.cursor_y][game_state.cursor_x] &&
                   game_state.player_board[game_state.cursor_y][game_state.cursor_x] == game_state.current_turn) {
            game_state.selected_x = game_state.cursor_x;
            game_state.selected_y = game_state.cursor_y;
            game_state.in_menu = 1;
            game_state.menu_selection = 0;
        }
    } else if (key == '\x1B') { // ESC
        if (game_state.in_menu || game_state.show_inventory) {
            game_state.in_menu = 0;
            game_state.show_inventory = 0;
        } else {
            game_state.selected_x = -1;
            game_state.selected_y = -1;
        }
    }
}

// Main
int main() {
    setlocale(LC_ALL, ""); // Enable UTF-8
    change_term_mode();

    // Init game
    memset(&game_state, 0, sizeof(game_state));
    game_state.current_turn = 1;
    game_state.cursor_x = game_state.cursor_y = 0;
    game_state.selected_x = game_state.selected_y = -1;
    game_state.player1_timer = 300;
    game_state.player2_timer = 300;
    game_state.message[0] = '\0';

    read_board_state();

    // Game loop
    while (1) {
        // Update timers
        if (game_state.current_turn == 1) {
            game_state.player1_timer -= 0.1;
            if (game_state.player1_timer <= 0) {
                game_state.game_over = 1;
                game_state.winner = 2;
            }
        } else {
            game_state.player2_timer -= 0.1;
            if (game_state.player2_timer <= 0) {
                game_state.game_over = 1;
                game_state.winner = 1;
            }
        }

        display();
        handle_input();

        usleep(100000); // 100ms
    }

    return 0;
}
