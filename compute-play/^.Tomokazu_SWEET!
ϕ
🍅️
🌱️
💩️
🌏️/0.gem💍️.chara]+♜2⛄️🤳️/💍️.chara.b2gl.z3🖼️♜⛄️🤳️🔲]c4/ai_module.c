#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <float.h>

#define BOARD_SIZE 8
#define MOVE_RANGE 2

// Data Structures
typedef struct {
    int player;
    int hp;
    int attack;
} Piece;

typedef struct {
    char* board[BOARD_SIZE][BOARD_SIZE];
    int player_board[BOARD_SIZE][BOARD_SIZE];
} GameState;

typedef struct {
    float attack_enemy;
    float move_to_center;
    float retreat_when_low_hp;
} Weights;

typedef struct {
    float aggressive_bias;
    float defensive_bias;
} Biases;

Piece read_piece_stats(const char* piece_name) {
    char filename[100];
    sprintf(filename, "%s.dir/state.txt", piece_name);
    FILE* f = fopen(filename, "r");
    Piece piece = {0};
    if (f != NULL) {
        fscanf(f, "player:%d\nhp:%d\nattack:%d\n", &piece.player, &piece.hp, &piece.attack);
        fclose(f);
    }
    return piece;
}

void read_board_state(GameState* gs) {
    FILE* f = fopen("board_state.txt", "r");
    if (f == NULL) return;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            char piece_name[50];
            fscanf(f, "%s ", piece_name);
            if (strcmp(piece_name, "none") != 0) {
                gs->board[i][j] = strdup(piece_name);
                Piece p = read_piece_stats(piece_name);
                gs->player_board[i][j] = p.player;
            } else {
                gs->board[i][j] = NULL;
                gs->player_board[i][j] = 0;
            }
        }
    }
    fclose(f);
}

void read_weights_and_biases(const char* piece_name, Weights* w, Biases* b) {
    // Default values
    w->attack_enemy = 1.0;
    w->move_to_center = 0.5;
    w->retreat_when_low_hp = 0.8;
    b->aggressive_bias = 0.1;
    b->defensive_bias = -0.1;

    char filename[100];
    sprintf(filename, "%s.dir/weights.txt", piece_name);
    FILE* f = fopen(filename, "r");
    if (f != NULL) {
        fscanf(f, "attack_enemy:%f\n", &w->attack_enemy);
        fscanf(f, "move_to_center:%f\n", &w->move_to_center);
        fscanf(f, "retreat_when_low_hp:%f\n", &w->retreat_when_low_hp);
        fclose(f);
    }

    sprintf(filename, "%s.dir/biases.txt", piece_name);
    f = fopen(filename, "r");
    if (f != NULL) {
        fscanf(f, "aggressive_bias:%f\n", &b->aggressive_bias);
        fscanf(f, "defensive_bias:%f\n", &b->defensive_bias);
        fclose(f);
    }
}

void write_ai_move(int from_x, int from_y, int to_x, int to_y) {
    FILE* f = fopen("ai_move.txt", "w");
    if (f == NULL) return;
    fprintf(f, "%d %d %d %d", from_x, from_y, to_x, to_y);
    fclose(f);
}

int is_near_enemy(int x, int y, GameState* gs) {
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int check_x = x + j;
            int check_y = y + i;
            if (check_x >= 0 && check_x < BOARD_SIZE && check_y >= 0 && check_y < BOARD_SIZE) {
                if (gs->player_board[check_y][check_x] == 1) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

float distance_from_center(int x, int y) {
    float center_x = (float)BOARD_SIZE / 2.0;
    float center_y = (float)BOARD_SIZE / 2.0;
    return sqrt(pow(x - center_x, 2) + pow(y - center_y, 2));
}

int main() {
    srand(time(NULL));
    GameState gs;
    read_board_state(&gs);

    int best_from_x = -1, best_from_y = -1, best_to_x = -1, best_to_y = -1;
    float best_score = -FLT_MAX;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (gs.player_board[i][j] == 2) {
                const char* piece_name = gs.board[i][j];
                Weights weights;
                Biases biases;
                read_weights_and_biases(piece_name, &weights, &biases);

                for (int y = -MOVE_RANGE; y <= MOVE_RANGE; y++) {
                    for (int x = -MOVE_RANGE; x <= MOVE_RANGE; x++) {
                        int to_x = j + x;
                        int to_y = i + y;

                        if (to_x >= 0 && to_x < BOARD_SIZE && to_y >= 0 && to_y < BOARD_SIZE &&
                            gs.player_board[to_y][to_x] == 0) {
                            
                            float score = 0;
                            if (is_near_enemy(to_x, to_y, &gs)) {
                                score += weights.attack_enemy + biases.aggressive_bias;
                            }
                            score -= distance_from_center(to_x, to_y) * weights.move_to_center;

                            if (score > best_score) {
                                best_score = score;
                                best_from_x = j;
                                best_from_y = i;
                                best_to_x = to_x;
                                best_to_y = to_y;
                            }
                        }
                    }
                }
            }
        }
    }

    if (best_from_x != -1) {
        write_ai_move(best_from_x, best_from_y, best_to_x, best_to_y);
    } else {
        write_ai_move(-1, -1, -1, -1);
    }

    return 0;
}