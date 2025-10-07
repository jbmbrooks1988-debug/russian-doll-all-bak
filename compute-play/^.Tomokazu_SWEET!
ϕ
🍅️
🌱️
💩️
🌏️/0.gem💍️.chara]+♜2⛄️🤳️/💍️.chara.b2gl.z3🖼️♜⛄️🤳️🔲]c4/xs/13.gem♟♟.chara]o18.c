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
#define MAX_HP 100

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
    double player1_timer;
    double player2_timer;
    int game_over;
    int winner;
    char message[100];
    time_t message_expire;
} GameState;

GameState game_state;

// Function declarations
void read_board_state();
char* get_emoji_for_piece(const char* piece_name);
void display();
void change_term_mode();
void restore_term_mode();
char get_key();
void handle_input();

struct termios orig_termios;

void cleanup() {
    restore_term_mode();
    printf("\n");
}

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

void restore_term_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

char get_key() {
    char c = getchar();
    if (c == '\x1B') {
        if (getchar() == '[') {
            c = getchar();
            switch (c) {
                case 'A': return 'U';
                case 'B': return 'D';
                case 'C': return 'R';
                case 'D': return 'L';
            }
        }
        return '\x1B';
    }
    return c;
}

char* get_emoji_for_piece(const char* piece_name) {
    static char emoji[5] = "❓";
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
            if (len >= 1 && len <= 4) {
                strncpy(emoji, em, len);
                emoji[len] = '\0';
                fclose(f);
                return emoji;
            }
        }
    }
    fclose(f);
    return emoji;
}

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
                if (game_state.board[row][col] && strcmp(game_state.board[row][col], token) != 0) {
                    free(game_state.board[row][col]);
                } else if (!game_state.board[row][col]) {
                    game_state.board[row][col] = strdup(token);
                }
                game_state.player_board[row][col] = (row < 2) ? 1 : 2;
            } else {
                if (game_state.board[row][col]) {
                    free(game_state.board[row][col]);
                    game_state.board[row][col] = NULL;
                }
                game_state.player_board[row][col] = 0;
            }
            token = strtok(NULL, " \n");
            col++;
        }
        row++;
    }
    fclose(f);
}

void display() {
    system("clear");

    printf(" ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("      %d", j);
    }
    printf("\n");

    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%c ", 'A' + i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("+-----");
        }
        printf("+\n");

        printf("%c|", 'A' + i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            const char* emoji = "  ";
            if (game_state.board[i][j]) {
                emoji = get_emoji_for_piece(game_state.board[i][j]);
            }

            if (i == game_state.cursor_y && j == game_state.cursor_x) {
                printf("[ %s ]|", emoji);
            } else if (i == game_state.selected_y && j == game_state.selected_x) {
                printf("  %s*|", emoji);
            } else {
                printf("  %s  |", emoji);
            }
        }
        printf("\n");
    }

    printf(" ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("+-----");
    }
    printf("+\n");

    printf("Turn: Player %d | P1: %.0fs | P2: %.0fs\n",
           game_state.current_turn,
           game_state.player1_timer,
           game_state.player2_timer);

    if (game_state.message[0] && time(NULL) < game_state.message_expire) {
        printf("Msg: %s\n", game_state.message);
    }

    if (game_state.game_over) {
        printf("🏆 GAME OVER! Player %d wins! 🏆\n", game_state.winner);
    }

    printf("\n⌨️  Arrows: move | ENTER: select/place | ESC: cancel\n");
}

void handle_input() {
    if (game_state.game_over) return;

    if (game_state.current_turn == 2) {
        FILE* f = fopen("board_state.txt", "w");
        if (f) {
            for (int i = 0; i < BOARD_SIZE; i++) {
                for (int j = 0; j < BOARD_SIZE; j++) {
                    fprintf(f, "%s ", game_state.board[i][j] ? game_state.board[i][j] : "none");
                }
                fprintf(f, "\n");
            }
            fclose(f);
        }
        system("./ai_module");
        read_board_state();  // Safe now — handles memory
        game_state.current_turn = 1;
        return;
    }

    char key = get_key();

    if (key == 'U' && game_state.cursor_y > 0) game_state.cursor_y--;
    else if (key == 'D' && game_state.cursor_y < BOARD_SIZE - 1) game_state.cursor_y++;
    else if (key == 'L' && game_state.cursor_x > 0) game_state.cursor_x--;
    else if (key == 'R' && game_state.cursor_x < BOARD_SIZE - 1) game_state.cursor_x++;
    else if (key == '\n') {
        if (game_state.selected_x == -1) {
            const char* piece = game_state.board[game_state.cursor_y][game_state.cursor_y];
            if (piece && game_state.player_board[game_state.cursor_y][game_state.cursor_x] == 1) {
                game_state.selected_x = game_state.cursor_x;
                game_state.selected_y = game_state.cursor_y;
                sprintf(game_state.message, "Selected %s", piece);
                game_state.message_expire = time(NULL) + 2;
            } else {
                sprintf(game_state.message, "Can't select that piece");
                game_state.message_expire = time(NULL) + 2;
            }
        } else {
            int to_x = game_state.cursor_x;
            int to_y = game_state.cursor_y;
            if (game_state.board[to_y][to_x] == NULL) {
                const char* name = game_state.board[game_state.selected_y][game_state.selected_x];
                game_state.board[to_y][to_x] = game_state.board[game_state.selected_y][game_state.selected_x];
                game_state.player_board[to_y][to_x] = 1;
                game_state.board[game_state.selected_y][game_state.selected_x] = NULL;
                game_state.player_board[game_state.selected_y][game_state.selected_x] = 0;

                char filename[100];
                sprintf(filename, "%s.dir/state.txt", name);
                FILE* f = fopen(filename, "w");
                if (f) {
                    fprintf(f, "player:1\nhp:100\nattack:10\ndefense:0\nx:%d\ny:%d\nz:0\nsymbol : %s\n",
                            to_x, to_y, get_emoji_for_piece(name));
                    fclose(f);
                }

                sprintf(game_state.message, "Moved %s to %c%d", name, 'A' + to_y, to_x);
                game_state.message_expire = time(NULL) + 2;
                game_state.current_turn = 2;
                game_state.selected_x = game_state.selected_y = -1;
            } else {
                sprintf(game_state.message, "Occupied!");
                game_state.message_expire = time(NULL) + 2;
            }
        }
    } else if (key == '\x1B') {
        game_state.selected_x = game_state.selected_y = -1;
        sprintf(game_state.message, "Cancelled");
        game_state.message_expire = time(NULL) + 1;
    }
}

int main() {
    setlocale(LC_ALL, "");
    change_term_mode();

    memset(&game_state, 0, sizeof(game_state));
    game_state.current_turn = 1;
    game_state.cursor_x = game_state.cursor_y = 0;
    game_state.selected_x = game_state.selected_y = -1;
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
