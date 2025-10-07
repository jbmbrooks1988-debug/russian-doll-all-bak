#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOARD_SIZE 32
#define WEIGHT_COUNT 33

int board[BOARD_SIZE];
float weights_red[WEIGHT_COUNT];
float weights_black[WEIGHT_COUNT];
int game_mode;

void init_game();
void print_board();
int get_human_move(int player, int *from, int *to);
int get_ai_move(int player, int *from, int *to, float *weights);
int make_move(int from, int to, int player);
int is_valid_move(int from, int to, int player);
int can_capture(int from, int player);
int has_captures(int player);
void save_weights(int player);
void load_weights(int player, float *weights);
void update_weights(int winner, float *winner_weights, float *loser_weights);
void print_weights(float *weights, const char *player);

int main() {
    srand(time(NULL));
    printf("Select mode: 0 (Human vs AI) or 1 (AI vs AI): ");
    scanf("%d", &game_mode);
    
    load_weights(1, weights_red);
    load_weights(-1, weights_black);
    
    printf("Initial weights:\n");
    print_weights(weights_red, "Red");
    print_weights(weights_black, "Black");
    
    init_game();
    
    int current_player = 1;
    while (1) {
        print_board();
        
        int from, to, move_made = 0;
        if (game_mode == 0 && current_player == 1) {
            move_made = get_human_move(current_player, &from, &to);
            if (!move_made) {
                printf("Invalid input, try again.\n");
                continue;
            }
        } else {
            float *weights = (current_player == 1) ? weights_red : weights_black;
            move_made = get_ai_move(current_player, &from, &to, weights);
            if (move_made) printf("AI (%s) moves %d to %d\n", 
                               current_player == 1 ? "Red" : "Black", from, to);
        }
        
        if (move_made && is_valid_move(from, to, current_player)) {
            make_move(from, to, current_player);
            
            int red_pieces = 0, black_pieces = 0, red_moves = 0, black_moves = 0;
            for (int i = 0; i < BOARD_SIZE; i++) {
                if (board[i] == 1 || board[i] == 2) red_pieces++;
                if (board[i] == -1 || board[i] == -2) black_pieces++;
            }
            red_moves = has_captures(1);
            black_moves = has_captures(-1);
            
            if (red_pieces == 0 || !black_moves) {
                update_weights(-1, weights_black, weights_red);
                save_weights(-1);
                if (game_mode == 1) save_weights(1);
                printf("Game Over! Black wins!\n");
                break;
            }
            if (black_pieces == 0 || !red_moves) {
                update_weights(1, weights_red, weights_black);
                save_weights(1);
                if (game_mode == 1) save_weights(-1);
                printf("Game Over! Red wins!\n");
                break;
            }
            current_player = -current_player;
        } else if (!move_made) {
            printf("No valid moves! %s wins!\n", current_player == 1 ? "Black" : "Red");
            float *winner_weights = (current_player == 1) ? weights_black : weights_red;
            float *loser_weights = (current_player == 1) ? weights_red : weights_black;
            update_weights(-current_player, winner_weights, loser_weights);
            save_weights(-current_player);
            if (game_mode == 1) save_weights(current_player);
            break;
        } else {
            printf("Invalid move attempted: %d to %d\n", from, to);
            if (game_mode == 0 && current_player == 1) {
                printf("Try again.\n");
            } else {
                current_player = -current_player;
            }
        }
    }
    return 0;
}

void init_game() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (i < 12) board[i] = -1;
        else if (i < 20) board[i] = 0;
        else board[i] = 1;
    }
}

void print_board() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == 1) printf("r ");
        else if (board[i] == -1) printf("b ");
        else if (board[i] == 2) printf("R ");
        else if (board[i] == -2) printf("B ");
        else printf(". ");
        if ((i + 1) % 4 == 0) printf("%2d\n", i);
    }
    printf("\n");
}

int get_human_move(int player, int *from, int *to) {
    printf("Player %s - Enter piece to move (0-31): ", player == 1 ? "Red" : "Black");
    if (scanf("%d", from) != 1 || *from < 0 || *from >= BOARD_SIZE) return 0;
    printf("Enter destination (0-31): ");
    if (scanf("%d", to) != 1 || *to < 0 || *to >= BOARD_SIZE) return 0;
    return 1;
}

int get_ai_move(int player, int *from, int *to, float *weights) {
    float best_score = -1000.0;
    int best_from = -1, best_to = -1;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] != player && board[i] != player * 2) continue;
        
        int moves[4] = {i-4, i-3, i+3, i+4};
        for (int j = 0; j < 4; j++) {
            if (moves[j] >= 0 && moves[j] < BOARD_SIZE) {
                if (is_valid_move(i, moves[j], player)) {
                    float score = weights[moves[j]] + weights[WEIGHT_COUNT-1];
                    if (score > best_score) {
                        best_score = score;
                        best_from = i;
                        best_to = moves[j];
                    }
                }
            }
        }
    }
    
    if (best_from == -1) return 0;
    *from = best_from;
    *to = best_to;
    return 1;
}

int make_move(int from, int to, int player) {
    int piece = board[from];
    board[from] = 0;
    
    int diff = to - from;
    if (abs(diff) > 4) {
        int mid = from + diff/2;
        if (mid >= 0 && mid < BOARD_SIZE) board[mid] = 0;
    }
    
    if (player == 1 && to <= 3) piece = 2;
    if (player == -1 && to >= 28) piece = -2;
    board[to] = piece;
    return 1;
}

int is_valid_move(int from, int to, int player) {
    if (from < 0 || from >= BOARD_SIZE || to < 0 || to >= BOARD_SIZE) return 0;
    if (board[from] != player && board[from] != player * 2) return 0;
    if (board[to] != 0) return 0;
    
    int is_king = abs(board[from]) == 2;
    int diff = to - from;
    int row_diff = (to / 4) - (from / 4);
    
    if (has_captures(player) && abs(diff) <= 4) return 0;
    
    if (abs(diff) == 3 || abs(diff) == 4) {
        if (player == 1 && !is_king && row_diff > 0) return 0;
        if (player == -1 && !is_king && row_diff < 0) return 0;
        return 1;
    }
    
    if (abs(diff) == 7 || abs(diff) == 9) {
        int mid = from + (to - from)/2;
        if (mid >= 0 && mid < BOARD_SIZE && 
            (board[mid] == -player || board[mid] == -player * 2)) {
            if (player == 1 && !is_king && row_diff > 0) return 0;
            if (player == -1 && !is_king && row_diff < 0) return 0;
            return 1;
        }
    }
    return 0;
}

int can_capture(int from, int player) {
    int is_king = abs(board[from]) == 2;
    int moves[4] = {from-9, from-7, from+7, from+9};
    
    for (int j = 0; j < 4; j++) {
        if (moves[j] >= 0 && moves[j] < BOARD_SIZE && board[moves[j]] == 0) {
            int mid = from + (moves[j] - from)/2;
            if (mid >= 0 && mid < BOARD_SIZE && 
                (board[mid] == -player || board[mid] == -player * 2)) {
                int row_diff = (moves[j] / 4) - (from / 4);
                if (player == 1 && !is_king && row_diff > 0) continue;
                if (player == -1 && !is_king && row_diff < 0) continue;
                return 1;
            }
        }
    }
    return 0;
}

int has_captures(int player) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == player || board[i] == player * 2) {
            if (can_capture(i, player)) return 1;
        }
    }
    return 0;
}

void save_weights(int player) {
    char filename[20];
    sprintf(filename, "weights_%s.txt", player == 1 ? "red" : "black");
    FILE *fp = fopen(filename, "w");
    if (fp) {
        float *weights = (player == 1) ? weights_red : weights_black;
        fprintf(fp, "[");
        for (int i = 0; i < BOARD_SIZE; i++) {
            fprintf(fp, "%.2f", weights[i]);
            if (i < BOARD_SIZE - 1) fprintf(fp, ", ");
            if ((i + 1) % 8 == 0) fprintf(fp, "\n ");
        }
        fprintf(fp, "]\nBias: %.2f\n", weights[WEIGHT_COUNT-1]);
        fclose(fp);
    }
}

void load_weights(int player, float *weights) {
    char filename[20];
    sprintf(filename, "weights_%s.txt", player == 1 ? "red" : "black");
    FILE *fp = fopen(filename, "r");
    if (fp) {
        char buffer[1024];
        int i = 0;
        fgets(buffer, sizeof(buffer), fp);
        while (i < BOARD_SIZE && fscanf(fp, "%f", &weights[i]) == 1) {
            i++;
            fgetc(fp);
        }
        fscanf(fp, "Bias: %f", &weights[WEIGHT_COUNT-1]);
        fclose(fp);
    } else {
        for (int i = 0; i < WEIGHT_COUNT; i++) {
            weights[i] = (float)(rand() % 100) / 100.0;
        }
    }
}

void update_weights(int winner, float *winner_weights, float *loser_weights) {
    float learning_rate = 0.1;
    
    printf("\nWeight updates:\n");
    printf("Winner (%s):\n", winner == 1 ? "Red" : "Black");
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == winner || board[i] == winner * 2) {
            float old_weight = winner_weights[i];
            winner_weights[i] += learning_rate;
            printf("  Position %d: %.2f -> %.2f\n", i, old_weight, winner_weights[i]);
        }
    }
    float old_bias_w = winner_weights[WEIGHT_COUNT-1];
    winner_weights[WEIGHT_COUNT-1] += learning_rate;
    printf("  Bias: %.2f -> %.2f\n", old_bias_w, winner_weights[WEIGHT_COUNT-1]);
    
    printf("Loser (%s):\n", winner == 1 ? "Black" : "Red");
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == -winner || board[i] == -winner * 2) {
            float old_weight = loser_weights[i];
            loser_weights[i] -= learning_rate;
            printf("  Position %d: %.2f -> %.2f\n", i, old_weight, loser_weights[i]);
        }
    }
    float old_bias_l = loser_weights[WEIGHT_COUNT-1];
    loser_weights[WEIGHT_COUNT-1] -= learning_rate;
    printf("  Bias: %.2f -> %.2f\n", old_bias_l, loser_weights[WEIGHT_COUNT-1]);
}

void print_weights(float *weights, const char *player) {
    printf("%s weights: ", player);
    for (int i = 0; i < 5; i++) {
        printf("%.2f ", weights[i]);
    }
    printf("... bias: %.2f\n", weights[WEIGHT_COUNT-1]);
}
