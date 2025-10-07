
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NUM_SPACES 40
#define MAX_PLAYERS 5
#define SPACES_PER_ROW 20
#define SPACE_WIDTH 6

// ANSI color codes
#define ANSI_RESET "\033[0m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN "\033[36m"
#define ANSI_BROWN "\033[38;5;130m"
#define ANSI_DARKGREEN "\033[38;5;28m"
#define ANSI_DARKBLUE "\033[38;5;18m"

// Space type enum
typedef enum {
    TYPE_GO, TYPE_PROPERTY, TYPE_UTILITY, TYPE_RAILROAD,
    TYPE_CHANCE, TYPE_CHEST, TYPE_TAX, TYPE_JAIL,
    TYPE_FREE_PARKING, TYPE_GO_TO_JAIL
} SpaceType;

// Player structure
typedef struct {
    char name[50];
    int type; // 0 human, 1 computer
    char emoji[5];
    char color[10]; // ANSI color for player
    int position;
    int money;
    int in_jail;
} Player;

// Space structure
typedef struct {
    char name[6]; // Abbreviated name for ASCII
    char color[10]; // ANSI color for group
    SpaceType type;
    int price;
    int mortgage;
    int house_cost;
    int rent[6];
    int color_group; // -1 if not property
    int owner; // -1 none, else player index
    int houses; // 0-5
} Space;

// Game state
typedef struct {
    int current_turn;
    int state; // 0: normal, 1: buying
    char message[100];
    time_t message_expire;
    time_t last_action_time;
    int num_players;
    Player players[MAX_PLAYERS];
    Space board[NUM_SPACES];
    int game_over;
} GameState;

GameState game_state;

// Function declarations
void init_board(Space* board);
void display();
void handle_roll(int is_human);
void set_status_message(const char* msg, int duration_seconds);
void keyboard(char key);

// Initialize board with abbreviated names and colors
void init_board(Space* board) {
    int i;
    for (i = 0; i < NUM_SPACES; i++) {
        board[i].owner = -1;
        board[i].houses = 0;
        board[i].color_group = -1;
        strcpy(board[i].color, ANSI_RESET);
    }

    // 0: Go
    strcpy(board[0].name, "GO");
    board[0].type = TYPE_GO;

    // 1: Mediterranean Avenue
    strcpy(board[1].name, "MedAv");
    board[1].type = TYPE_PROPERTY;
    board[1].price = 60;
    board[1].mortgage = 30;
    board[1].house_cost = 50;
    board[1].rent[0] = 2;
    board[1].rent[1] = 10;
    board[1].rent[2] = 30;
    board[1].rent[3] = 90;
    board[1].rent[4] = 160;
    board[1].rent[5] = 250;
    board[1].color_group = 0;
    strcpy(board[1].color, ANSI_BROWN);

    // 2: Community Chest
    strcpy(board[2].name, "Chest");
    board[2].type = TYPE_CHEST;

    // 3: Baltic Avenue
    strcpy(board[3].name, "BalAv");
    board[3].type = TYPE_PROPERTY;
    board[3].price = 60;
    board[3].mortgage = 30;
    board[3].house_cost = 50;
    board[3].rent[0] = 4;
    board[3].rent[1] = 20;
    board[3].rent[2] = 60;
    board[3].rent[3] = 180;
    board[3].rent[4] = 320;
    board[3].rent[5] = 450;
    board[3].color_group = 0;
    strcpy(board[3].color, ANSI_BROWN);

    // 4: Income Tax
    strcpy(board[4].name, "Tax");
    board[4].type = TYPE_TAX;
    board[4].price = 200;

    // 5: Reading Railroad
    strcpy(board[5].name, "ReadR");
    board[5].type = TYPE_RAILROAD;
    board[5].price = 200;
    board[5].mortgage = 100;
    board[5].rent[0] = 25;
    board[5].rent[1] = 50;
    board[5].rent[2] = 100;
    board[5].rent[3] = 200;

    // 6: Oriental Avenue
    strcpy(board[6].name, "OriAv");
    board[6].type = TYPE_PROPERTY;
    board[6].price = 100;
    board[6].mortgage = 50;
    board[6].house_cost = 50;
    board[6].rent[0] = 6;
    board[6].rent[1] = 30;
    board[6].rent[2] = 90;
    board[6].rent[3] = 270;
    board[6].rent[4] = 400;
    board[6].rent[5] = 550;
    board[6].color_group = 1;
    strcpy(board[6].color, ANSI_CYAN);

    // 7: Chance
    strcpy(board[7].name, "Chnce");
    board[7].type = TYPE_CHANCE;

    // 8: Vermont Avenue
    strcpy(board[8].name, "VerAv");
    board[8].type = TYPE_PROPERTY;
    board[8].price = 100;
    board[8].mortgage = 50;
    board[8].house_cost = 50;
    board[8].rent[0] = 6;
    board[8].rent[1] = 30;
    board[8].rent[2] = 90;
    board[8].rent[3] = 270;
    board[8].rent[4] = 400;
    board[8].rent[5] = 550;
    board[8].color_group = 1;
    strcpy(board[8].color, ANSI_CYAN);

    // 9: Connecticut Avenue
    strcpy(board[9].name, "ConAv");
    board[9].type = TYPE_PROPERTY;
    board[9].price = 120;
    board[9].mortgage = 60;
    board[9].house_cost = 50;
    board[9].rent[0] = 8;
    board[9].rent[1] = 40;
    board[9].rent[2] = 100;
    board[9].rent[3] = 300;
    board[9].rent[4] = 450;
    board[9].rent[5] = 600;
    board[9].color_group = 1;
    strcpy(board[9].color, ANSI_CYAN);

    // 10: Jail
    strcpy(board[10].name, "Jail");
    board[10].type = TYPE_JAIL;

    // 11: St. Charles Place
    strcpy(board[11].name, "StCPl");
    board[11].type = TYPE_PROPERTY;
    board[11].price = 140;
    board[11].mortgage = 70;
    board[11].house_cost = 100;
    board[11].rent[0] = 10;
    board[11].rent[1] = 50;
    board[11].rent[2] = 150;
    board[11].rent[3] = 450;
    board[11].rent[4] = 625;
    board[11].rent[5] = 750;
    board[11].color_group = 2;
    strcpy(board[11].color, ANSI_MAGENTA);

    // 12: Electric Company
    strcpy(board[12].name, "ElecC");
    board[12].type = TYPE_UTILITY;
    board[12].price = 150;
    board[12].mortgage = 75;

    // 13: States Avenue
    strcpy(board[13].name, "StaAv");
    board[13].type = TYPE_PROPERTY;
    board[13].price = 140;
    board[13].mortgage = 70;
    board[13].house_cost = 100;
    board[13].rent[0] = 10;
    board[13].rent[1] = 50;
    board[13].rent[2] = 150;
    board[13].rent[3] = 450;
    board[13].rent[4] = 625;
    board[13].rent[5] = 750;
    board[13].color_group = 2;
    strcpy(board[13].color, ANSI_MAGENTA);

    // 14: Virginia Avenue
    strcpy(board[14].name, "VirAv");
    board[14].type = TYPE_PROPERTY;
    board[14].price = 160;
    board[14].mortgage = 80;
    board[14].house_cost = 100;
    board[14].rent[0] = 12;
    board[14].rent[1] = 60;
    board[14].rent[2] = 180;
    board[14].rent[3] = 500;
    board[14].rent[4] = 700;
    board[14].rent[5] = 900;
    board[14].color_group = 2;
    strcpy(board[14].color, ANSI_MAGENTA);

    // 15: Pennsylvania Railroad
    strcpy(board[15].name, "PennR");
    board[15].type = TYPE_RAILROAD;
    board[15].price = 200;
    board[15].mortgage = 100;
    board[15].rent[0] = 25;
    board[15].rent[1] = 50;
    board[15].rent[2] = 100;
    board[15].rent[3] = 200;

    // 16: St. James Place
    strcpy(board[16].name, "StJPl");
    board[16].type = TYPE_PROPERTY;
    board[16].price = 180;
    board[16].mortgage = 90;
    board[16].house_cost = 100;
    board[16].rent[0] = 14;
    board[16].rent[1] = 70;
    board[16].rent[2] = 200;
    board[16].rent[3] = 550;
    board[16].rent[4] = 750;
    board[16].rent[5] = 950;
    board[16].color_group = 3;
    strcpy(board[16].color, ANSI_RED);

    // 17: Community Chest
    strcpy(board[17].name, "Chest");
    board[17].type = TYPE_CHEST;

    // 18: Tennessee Avenue
    strcpy(board[18].name, "TenAv");
    board[18].type = TYPE_PROPERTY;
    board[18].price = 180;
    board[18].mortgage = 90;
    board[18].house_cost = 100;
    board[18].rent[0] = 14;
    board[18].rent[1] = 70;
    board[18].rent[2] = 200;
    board[18].rent[3] = 550;
    board[18].rent[4] = 750;
    board[18].rent[5] = 950;
    board[18].color_group = 3;
    strcpy(board[18].color, ANSI_RED);

    // 19: New York Avenue
    strcpy(board[19].name, "NYAv");
    board[19].type = TYPE_PROPERTY;
    board[19].price = 200;
    board[19].mortgage = 100;
    board[19].house_cost = 100;
    board[19].rent[0] = 16;
    board[19].rent[1] = 80;
    board[19].rent[2] = 220;
    board[19].rent[3] = 600;
    board[19].rent[4] = 800;
    board[19].rent[5] = 1000;
    board[19].color_group = 3;
    strcpy(board[19].color, ANSI_RED);

    // 20: Free Parking
    strcpy(board[20].name, "Park");
    board[20].type = TYPE_FREE_PARKING;

    // 21: Kentucky Avenue
    strcpy(board[21].name, "KenAv");
    board[21].type = TYPE_PROPERTY;
    board[21].price = 220;
    board[21].mortgage = 110;
    board[21].house_cost = 150;
    board[21].rent[0] = 18;
    board[21].rent[1] = 90;
    board[21].rent[2] = 250;
    board[21].rent[3] = 700;
    board[21].rent[4] = 875;
    board[21].rent[5] = 1050;
    board[21].color_group = 4;
    strcpy(board[21].color, ANSI_YELLOW);

    // 22: Chance
    strcpy(board[22].name, "Chnce");
    board[22].type = TYPE_CHANCE;

    // 23: Indiana Avenue
    strcpy(board[23].name, "IndAv");
    board[23].type = TYPE_PROPERTY;
    board[23].price = 220;
    board[23].mortgage = 110;
    board[23].house_cost = 150;
    board[23].rent[0] = 18;
    board[23].rent[1] = 90;
    board[23].rent[2] = 250;
    board[23].rent[3] = 700;
    board[23].rent[4] = 875;
    board[23].rent[5] = 1050;
    board[23].color_group = 4;
    strcpy(board[23].color, ANSI_YELLOW);

    // 24: Illinois Avenue
    strcpy(board[24].name, "IllAv");
    board[24].type = TYPE_PROPERTY;
    board[24].price = 240;
    board[24].mortgage = 120;
    board[24].house_cost = 150;
    board[24].rent[0] = 20;
    board[24].rent[1] = 100;
    board[24].rent[2] = 300;
    board[24].rent[3] = 750;
    board[24].rent[4] = 925;
    board[24].rent[5] = 1100;
    board[24].color_group = 4;
    strcpy(board[24].color, ANSI_YELLOW);

    // 25: B & O Railroad
    strcpy(board[25].name, "B&OR");
    board[25].type = TYPE_RAILROAD;
    board[25].price = 200;
    board[25].mortgage = 100;
    board[25].rent[0] = 25;
    board[25].rent[1] = 50;
    board[25].rent[2] = 100;
    board[25].rent[3] = 200;

    // 26: Atlantic Avenue
    strcpy(board[26].name, "AtlAv");
    board[26].type = TYPE_PROPERTY;
    board[26].price = 260;
    board[26].mortgage = 130;
    board[26].house_cost = 150;
    board[26].rent[0] = 22;
    board[26].rent[1] = 110;
    board[26].rent[2] = 330;
    board[26].rent[3] = 800;
    board[26].rent[4] = 975;
    board[26].rent[5] = 1150;
    board[26].color_group = 5;
    strcpy(board[26].color, ANSI_GREEN);

    // 27: Ventnor Avenue
    strcpy(board[27].name, "VenAv");
    board[27].type = TYPE_PROPERTY;
    board[27].price = 260;
    board[27].mortgage = 130;
    board[27].house_cost = 150;
    board[27].rent[0] = 22;
    board[27].rent[1] = 110;
    board[27].rent[2] = 330;
    board[27].rent[3] = 800;
    board[27].rent[4] = 975;
    board[27].rent[5] = 1150;
    board[27].color_group = 5;
    strcpy(board[27].color, ANSI_GREEN);

    // 28: Water Works
    strcpy(board[28].name, "WatWk");
    board[28].type = TYPE_UTILITY;
    board[28].price = 150;
    board[28].mortgage = 75;

    // 29: Marvin Gardens
    strcpy(board[29].name, "MarGd");
    board[29].type = TYPE_PROPERTY;
    board[29].price = 280;
    board[29].mortgage = 140;
    board[29].house_cost = 150;
    board[29].rent[0] = 24;
    board[29].rent[1] = 120;
    board[29].rent[2] = 360;
    board[29].rent[3] = 850;
    board[29].rent[4] = 1025;
    board[29].rent[5] = 1200;
    board[29].color_group = 5;
    strcpy(board[29].color, ANSI_GREEN);

    // 30: Go to Jail
    strcpy(board[30].name, "ToJai");
    board[30].type = TYPE_GO_TO_JAIL;

    // 31: Pacific Avenue
    strcpy(board[31].name, "PacAv");
    board[31].type = TYPE_PROPERTY;
    board[31].price = 300;
    board[31].mortgage = 150;
    board[31].house_cost = 200;
    board[31].rent[0] = 26;
    board[31].rent[1] = 130;
    board[31].rent[2] = 390;
    board[31].rent[3] = 900;
    board[31].rent[4] = 1100;
    board[31].rent[5] = 1275;
    board[31].color_group = 6;
    strcpy(board[31].color, ANSI_DARKGREEN);

    // 32: North Carolina Avenue
    strcpy(board[32].name, "NCAv");
    board[32].type = TYPE_PROPERTY;
    board[32].price = 300;
    board[32].mortgage = 150;
    board[32].house_cost = 200;
    board[32].rent[0] = 26;
    board[32].rent[1] = 130;
    board[32].rent[2] = 390;
    board[32].rent[3] = 900;
    board[32].rent[4] = 1100;
    board[32].rent[5] = 1275;
    board[32].color_group = 6;
    strcpy(board[32].color, ANSI_DARKGREEN);

    // 33: Community Chest
    strcpy(board[33].name, "Chest");
    board[33].type = TYPE_CHEST;

    // 34: Pennsylvania Avenue
    strcpy(board[34].name, "PenAv");
    board[34].type = TYPE_PROPERTY;
    board[34].price = 320;
    board[34].mortgage = 160;
    board[34].house_cost = 200;
    board[34].rent[0] = 28;
    board[34].rent[1] = 150;
    board[34].rent[2] = 450;
    board[34].rent[3] = 1000;
    board[34].rent[4] = 1200;
    board[34].rent[5] = 1400;
    board[34].color_group = 6;
    strcpy(board[34].color, ANSI_DARKGREEN);

    // 35: Short Line
    strcpy(board[35].name, "ShtLn");
    board[35].type = TYPE_RAILROAD;
    board[35].price = 200;
    board[35].mortgage = 100;
    board[35].rent[0] = 25;
    board[35].rent[1] = 50;
    board[35].rent[2] = 100;
    board[35].rent[3] = 200;

    // 36: Chance
    strcpy(board[36].name, "Chnce");
    board[36].type = TYPE_CHANCE;

    // 37: Park Place
    strcpy(board[37].name, "PrkPl");
    board[37].type = TYPE_PROPERTY;
    board[37].price = 350;
    board[37].mortgage = 175;
    board[37].house_cost = 200;
    board[37].rent[0] = 35;
    board[37].rent[1] = 175;
    board[37].rent[2] = 500;
    board[37].rent[3] = 1100;
    board[37].rent[4] = 1300;
    board[37].rent[5] = 1500;
    board[37].color_group = 7;
    strcpy(board[37].color, ANSI_DARKBLUE);

    // 38: Luxury Tax
    strcpy(board[38].name, "LuxTx");
    board[38].type = TYPE_TAX;
    board[38].price = 100;

    // 39: Boardwalk
    strcpy(board[39].name, "Brdwk");
    board[39].type = TYPE_PROPERTY;
    board[39].price = 400;
    board[39].mortgage = 200;
    board[39].house_cost = 200;
    board[39].rent[0] = 50;
    board[39].rent[1] = 200;
    board[39].rent[2] = 600;
    board[39].rent[3] = 1400;
    board[39].rent[4] = 1700;
    board[39].rent[5] = 2000;
    board[39].color_group = 7;
    strcpy(board[39].color, ANSI_DARKBLUE);
}

// Status message
void set_status_message(const char* msg, int duration_seconds) {
    strncpy(game_state.message, msg, sizeof(game_state.message) - 1);
    game_state.message[sizeof(game_state.message) - 1] = '\0';
    game_state.message_expire = time(NULL) + duration_seconds;
}

// Display function
void display() {
    system("clear"); // Clear terminal

    // Draw board in rows
    for (int row = 0; row < NUM_SPACES / SPACES_PER_ROW; row++) {
        // Draw separator
        printf("+");
        for (int i = 0; i < SPACES_PER_ROW; i++) {
            printf("------+");
        }
        printf("\n");

        // Draw space numbers
        printf("|");
        for (int i = 0; i < SPACES_PER_ROW; i++) {
            int p = row * SPACES_PER_ROW + i;
            if (p < NUM_SPACES) {
                printf("%2d   |", p);
            } else {
                printf("%*s|", SPACE_WIDTH, "");
            }
        }
        printf("\n");

        // Draw space content (name or tokens/price)
        printf("|");
        for (int i = 0; i < SPACES_PER_ROW; i++) {
            int p = row * SPACES_PER_ROW + i;
            if (p >= NUM_SPACES) {
                printf("%*s|", SPACE_WIDTH, "");
                continue;
            }
            char content[10] = "";
            int token_count = 0;
            for (int pl = 0; pl < game_state.num_players; pl++) {
                if (game_state.players[pl].position == p && token_count < 2) {
                    strcat(content, game_state.players[pl].emoji);
                    token_count++;
                }
            }
            if (!content[0] && (game_state.board[p].type == TYPE_PROPERTY ||
                               game_state.board[p].type == TYPE_RAILROAD ||
                               game_state.board[p].type == TYPE_UTILITY ||
                               game_state.board[p].type == TYPE_TAX)) {
                snprintf(content, sizeof(content), "$%d", game_state.board[p].price);
            } else if (!content[0]) {
                strncpy(content, game_state.board[p].name, sizeof(content));
            }
            printf("%s%*s%s|", game_state.board[p].color, SPACE_WIDTH, content, ANSI_RESET);
        }
        printf("\n");

        // Draw separator
        printf("+");
        for (int i = 0; i < SPACES_PER_ROW; i++) {
            printf("------+");
        }
        printf("\n");
    }

    // Draw player info
    printf("\nPlayers:\n");
    for (int i = 0; i < game_state.num_players; i++) {
        if (game_state.current_turn == i) {
            printf("%s%s %s $%d%s\n", game_state.players[i].color, game_state.players[i].name, game_state.players[i].emoji, game_state.players[i].money, ANSI_RESET);
        } else {
            printf("%s %s $%d\n", game_state.players[i].name, game_state.players[i].emoji, game_state.players[i].money);
        }
    }

    // Draw message
    if (game_state.message[0] && time(NULL) < game_state.message_expire) {
        printf("\n%s\n", game_state.message);
    }

    // Draw instructions
    printf("\nPress 'r' to roll dice (humans only), 'q' to quit\n");
}

// Handle roll and space logic
void handle_roll(int is_human) {
    int roll = (rand() % 6 + 1) + (rand() % 6 + 1);
    Player *pl = &game_state.players[game_state.current_turn];
    int old_pos = pl->position;
    pl->position = (pl->position + roll) % NUM_SPACES;
    if (pl->position < old_pos) {
        pl->money += 200;
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "%s rolled %d", pl->name, roll);
    set_status_message(msg, 2);

    Space *s = &game_state.board[pl->position];

    if (s->type == TYPE_GO_TO_JAIL) {
        pl->position = 10;
        pl->in_jail = 1;
        set_status_message("Go to Jail!", 2);
    } else if (s->type == TYPE_PROPERTY && s->owner == -1) {
        if (is_human) {
            snprintf(msg, sizeof(msg), "Press 'b' to buy %s for $%d, 's' to skip", s->name, s->price);
            set_status_message(msg, 5);
            game_state.state = 1;
            return; // Wait for input
        } else if (pl->money >= s->price) {
            pl->money -= s->price;
            s->owner = game_state.current_turn;
            snprintf(msg, sizeof(msg), "%s bought %s", pl->name, s->name);
            set_status_message(msg, 2);
        }
    } else if ((s->type == TYPE_PROPERTY || s->type == TYPE_RAILROAD || s->type == TYPE_UTILITY) && s->owner != -1 && s->owner != game_state.current_turn) {
        int rent = s->rent[0]; // Basic rent
        pl->money -= rent;
        game_state.players[s->owner].money += rent;
        snprintf(msg, sizeof(msg), "Paid $%d rent to %s", rent, game_state.players[s->owner].name);
        set_status_message(msg, 2);
    } else if (s->type == TYPE_TAX) {
        pl->money -= s->price;
        snprintf(msg, sizeof(msg), "Paid $%d tax", s->price);
        set_status_message(msg, 2);
    } else if (s->type == TYPE_CHANCE || s->type == TYPE_CHEST) {
        set_status_message("Draw card (not implemented)", 2);
    }

    // Next turn
    game_state.current_turn = (game_state.current_turn + 1) % game_state.num_players;
    game_state.state = 0;
}

// Keyboard input
void keyboard(char key) {
    if (game_state.game_over) return;

    if (key == 'q') {
        exit(0);
    }

    if (game_state.state == 0 && key == 'r' && game_state.players[game_state.current_turn].type == 0) {
        handle_roll(1);
    } else if (game_state.state == 1 && game_state.players[game_state.current_turn].type == 0) {
        Player *pl = &game_state.players[game_state.current_turn];
        Space *s = &game_state.board[pl->position];
        if (key == 'b' && pl->money >= s->price) {
            pl->money -= s->price;
            s->owner = game_state.current_turn;
            set_status_message("Bought property", 2);
            game_state.current_turn = (game_state.current_turn + 1) % game_state.num_players;
            game_state.state = 0;
        } else if (key == 's') {
            set_status_message("Skipped buy", 2);
            game_state.current_turn = (game_state.current_turn + 1) % game_state.num_players;
            game_state.state = 0;
        }
    }
}

// Main
int main() {
    srand(time(NULL));

    // Game setup
    printf("Enter number of players (1-5): ");
    scanf("%d", &game_state.num_players);
    if (game_state.num_players < 1 || game_state.num_players > 5) {
        fprintf(stderr, "Invalid number of players\n");
        exit(1);
    }

    const char* emoji_options[5] = {"🚗", "🐶", "🎩", "🚢", "🐱"};
    const char* color_options[5] = {ANSI_RED, ANSI_BLUE, ANSI_GREEN, ANSI_YELLOW, ANSI_MAGENTA};
    for (int i = 0; i < game_state.num_players; i++) {
        printf("--- Player %d ---\n", i + 1);
        printf("Enter name: ");
        scanf("%s", game_state.players[i].name);
        printf("Enter player type (0 for Human, 1 for Computer): ");
        scanf("%d", &game_state.players[i].type);
        printf("Choose emoji:\n");
        for (int j = 0; j < 5; j++) {
            printf("%d: %s\n", j, emoji_options[j]);
        }
        int choice;
        scanf("%d", &choice);
        if (choice < 0 || choice >= 5) choice = 0;
        strcpy(game_state.players[i].emoji, emoji_options[choice]);
        strcpy(game_state.players[i].color, color_options[choice]);
        game_state.players[i].position = 0;
        game_state.players[i].money = 1500;
        game_state.players[i].in_jail = 0;
    }

    game_state.current_turn = 0;
    game_state.state = 0;
    game_state.game_over = 0;
    game_state.message[0] = '\0';
    game_state.last_action_time = time(NULL);

    init_board(game_state.board);

    // Main loop
    while (1) {
        display();
        if (game_state.players[game_state.current_turn].type == 1 && game_state.state == 0 && time(NULL) > game_state.last_action_time) {
            handle_roll(0);
            game_state.last_action_time = time(NULL) + 2;
        } else {
            char input[10];
            if (fgets(input, sizeof(input), stdin)) {
                keyboard(input[0]);
            }
        }
        usleep(100000);
    }

    return 0;
}
