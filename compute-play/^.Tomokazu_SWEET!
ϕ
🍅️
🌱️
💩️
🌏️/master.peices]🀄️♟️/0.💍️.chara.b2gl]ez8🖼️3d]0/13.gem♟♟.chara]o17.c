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

// Piece state (matches state.txt)
typedef struct {
    int player;
    int hp;
    int attack;
    int defense;
    int x, y, z;
} Piece;

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
void ensure_emoji_in_state(const char* piece_name);
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
                case 'A': return 'U'; // Up
                case 'B': return 'D'; // Down
                case 'C': return 'R'; // Right
                case 'D': return 'L'; // Left
            }
        }
        return '\x1B'; // ESC
    }
    return c;
}

// Helper: Read emoji from state.txt — only accept real UTF-8
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
            while (*em == ' ') em++; // skip spaces
            int len = 0;
            while (em[len] && em[len] != '\n' && em[len] != '\r') len++;

            // Only accept 1–4 byte UTF-8 emoji (real emoji)
            if (len >= 1 && len <= 4) {
                strncpy(emoji, em, len);
                emoji[len] = '\0';
                fclose(f);
                return emoji;
            }
            // Reject hex codes
            if (len > 5 && (strstr(em, "0x") || strstr(em, "U000") || em[0] == '\\' || em[0] == 'U')) {
                fclose(f);
                return emoji;
            }
        }
    }
    fclose(f);
    return emoji;
}

// Ensure every state.txt has a real emoji
void ensure_emoji_in_state(const char* piece_name) {
    char filename[100];
    sprintf(filename, "%s.dir/state.txt", piece_name);

    FILE* f = fopen(filename, "r");
    if (!f) return;

    char temp[1000] = {0};
    char line[256];
    int has_symbol = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "symbol :", 8) == 0) {
            has_symbol = 1;
        } else {
            strcat(temp, line);
        }
    }
    fclose(f);

    if (!has_symbol) {
        strcat(temp, "symbol : 🎯\n"); // default emoji
        f = fopen(filename, "w");
        if (f) {
            fprintf(f, "%s", temp);
            fclose(f);
        }
    }
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
                ensure_emoji_in_state(token); // ← Auto-fix missing emoji
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

// Display the board with cursor and selection
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

            // Cursor: show [ ] around cell
            if (i == game_state.cursor_y && j == game_state.cursor_x) {
                printf("[ %s ]|", emoji);
            }
            // Selected piece
            else if (i == game_state.selected_y && j == game_state.selected_x) {
                printf("  %s*|", emoji);
            }
            // Normal cell
            else {
                printf("  %s  |", emoji);
            }
        }
        printf("\n");
    }

    // Bottom border
    printf(" ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("+-----");
    }
    printf("+\n");

    // Info panel
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

    printf("\n⌨️  Arrows: move | ENTER: select/place | ESC: cancel\n");
}

// Handle input
void handle_input() {
    if (game_state.game_over) return;

    // AI Turn
    if (game_state.current_turn == 2) {
        FILE* f = fopen("board_state.txt", "w");
        if (!f) return;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                fprintf(f, "%s ", game_state.board[i][j] ? game_state.board[i][j] : "none");
            }
            fprintf(f, "\n");
        }
        fclose(f);

        system("./ai_module");

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

    // Handle cursor movement
    if (key == 'U' && game_state.cursor_y > 0) {
        game_state.cursor_y--;
    }
    else if (key == 'D' && game_state.cursor_y < BOARD_SIZE - 1) {
        game_state.cursor_y++;
    }
    else if (key == 'L' && game_state.cursor_x > 0) {
        game_state.cursor_x--;
    }
    else if (key == 'R' && game_state.cursor_x < BOARD_SIZE - 1) {
        game_state.cursor_x++;
    }
    else if (key == '\n') {
        if (game_state.selected_x == -1 && game_state.selected_y == -1) {
            if (game_state.board[game_state.cursor_y][game_state.cursor_x] != NULL &&
                game_state.player_board[game_state.cursor_y][game_state.cursor_x] == game_state.current_turn) {
                game_state.selected_x = game_state.cursor_x;
                game_state.selected_y = game_state.cursor_y;
                sprintf(game_state.message, "Selected %s", game_state.board[game_state.cursor_y][game_state.cursor_x]);
                game_state.message_expire = time(NULL) + 2;
            } else {
                sprintf(game_state.message, "Can't select that piece");
                game_state.message_expire = time(NULL) + 2;
            }
        }
        else {
            int from_x = game_state.selected_x;
            int from_y = game_state.selected_y;
            int to_x = game_state.cursor_x;
            int to_y = game_state.cursor_y;

            if (game_state.board[to_y][to_x] == NULL) {
                const char* piece_name = game_state.board[from_y][from_x];

                game_state.board[to_y][to_x] = game_state.board[from_y][from_x];
                game_state.player_board[to_y][to_x] = game_state.player_board[from_y][from_x];
                game_state.board[from_y][from_x] = NULL;
                game_state.player_board[from_y][from_x] = 0;

                char filename[100];
                sprintf(filename, "%s.dir/state.txt", piece_name);
                FILE* f = fopen(filename, "r");
                Piece piece = {0};
                if (f) {
                    fscanf(f, "player:%d\nhp:%d\nattack:%d\ndefense:%d\nx:%d\ny:%d\nz:%d\n",
                           &piece.player, &piece.hp, &piece.attack, &piece.defense, &piece.x, &piece.y, &piece.z);
                    fclose(f);
                }
                piece.x = to_x;
                piece.y = to_y;

                f = fopen(filename, "w");
                if (f) {
                    fprintf(f, "player:%d\nhp:%d\nattack:%d\ndefense:%d\nx:%d\ny:%d\nz:%d\nsymbol : %s\n",
                            piece.player, piece.hp, piece.attack, piece.defense, piece.x, piece.y, piece.z,
                            get_emoji_for_piece(piece_name));
                    fclose(f);
                }

                sprintf(game_state.message, "Moved %s to %c%d", piece_name, 'A' + to_y, to_x);
                game_state.message_expire = time(NULL) + 2;

                game_state.current_turn = 2;
                game_state.selected_x = -1;
                game_state.selected_y = -1;
            } else {
                sprintf(game_state.message, "Target occupied!");
                game_state.message_expire = time(NULL) + 2;
            }
        }
    }
    else if (key == '\x1B') {
        if (game_state.selected_x != -1 || game_state.selected_y != -1) {
            game_state.selected_x = -1;
            game_state.selected_y = -1;
            sprintf(game_state.message, "Selection cancelled");
            game_state.message_expire = time(NULL) + 1;
        }
    }
}

// Main
int main() {
    setlocale(LC_ALL, ""); // Enable UTF-8
    change_term_mode();

    memset(&game_state, 0, sizeof(game_state));
    game_state.current_turn = 1;
    game_state.cursor_x = 0;
    game_state.cursor_y = 0;
    game_state.selected_x = -1;
    game_state.selected_y = -1;
    game_state.player1_timer = 300;
    game_state.player2_timer = 300;
    game_state.message[0] = '\0';

    read_board_state();

    while (1) {
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

        usleep(100000);
    }

    return 0;
}
