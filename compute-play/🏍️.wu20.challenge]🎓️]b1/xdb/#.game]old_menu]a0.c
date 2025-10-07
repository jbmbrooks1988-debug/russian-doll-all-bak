#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// Enum for player type
typedef enum {
    HUMAN,
    COMPUTER
} PlayerType;

// Basic data structures
typedef struct {
    char name[50];
    float price;
    int shares;
} Company;

typedef struct {
    char name[50];
    float cash;
    PlayerType type;
    bool ticker_on; // Added to track ticker state
    // Portfolio will be a dynamic array of company stocks
} Player;

typedef struct {
    float interest_rate;
    // News events can be a simple array of strings for now
    char news[10][100];
} Market;

// Function prototypes
void initialize_market(Market *market);
void initialize_companies(Company companies[], int num_companies);
void print_market_status(Market *market, Company companies[], int num_companies);
void save_current_state(Player players[], int num_players, int game_length, Market *market, Company companies[], int num_companies, int current_year);
void load_game_state(Player players[], int *num_players, int *game_length, Market *market, Company companies[], int num_companies, int *start_year);
int player_turn(Player *player, Player all_players[], int num_players, int game_length, Company companies[], int num_companies, Market *market, int current_year);
bool file_exists(const char *filename);

bool file_exists(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

void load_game_state(Player players[], int *num_players, int *game_length, Market *market, Company companies[], int num_companies, int *start_year) {
    FILE *fp = fopen("gamestate.txt", "r");
    if (fp == NULL) {
        printf("Error: Could not open gamestate.txt for reading.\n");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, ":");
        char *value = strtok(NULL, "\n");

        if (key && value) {
            if (strcmp(key, "year") == 0) {
                *start_year = atoi(value) - 1; // Adjust for 0-based index
            } else if (strcmp(key, "game_length") == 0) {
                *game_length = atoi(value);
            } else if (strcmp(key, "num_players") == 0) {
                *num_players = atoi(value);
            } else if (strcmp(key, "market_interest_rate") == 0) {
                market->interest_rate = atof(value);
            } else if (strncmp(key, "player_", 7) == 0) {
                int player_index;
                char player_key[50];
                sscanf(key, "player_%d_%s", &player_index, player_key);
                if (strcmp(player_key, "name") == 0) {
                    strcpy(players[player_index].name, value);
                } else if (strcmp(player_key, "cash") == 0) {
                    players[player_index].cash = atof(value);
                } else if (strcmp(player_key, "type") == 0) {
                    players[player_index].type = (PlayerType)atoi(value);
                } else if (strcmp(player_key, "ticker_on") == 0) {
                    players[player_index].ticker_on = atoi(value); // Load ticker state
                }
            } else if (strncmp(key, "company_", 8) == 0) {
                int company_index;
                char company_key[50];
                sscanf(key, "company_%d_%s", &company_index, company_key);
                if (strcmp(company_key, "name") == 0) {
                    strcpy(companies[company_index].name, value);
                } else if (strcmp(company_key, "price") == 0) {
                    companies[company_index].price = atof(value);
                } else if (strcmp(company_key, "shares") == 0) {
                    companies[company_index].shares = atoi(value);
                }
            }
        }
    }

    fclose(fp);
}

void initialize_market(Market *market) {
    market->interest_rate = 2.5;
    strcpy(market->news[0], "Tech stocks are soaring!");
    strcpy(market->news[1], "Oil prices are dropping.");
    // Add more news items
}

void initialize_companies(Company companies[], int num_companies) {
    for (int i = 0; i < num_companies; i++) {
        sprintf(companies[i].name, "Company %d", i + 1);
        companies[i].price = (rand() % 1000) / 10.0;
        companies[i].shares = 1000;
    }
}

void print_market_status(Market *market, Company companies[], int num_companies) {
    printf("\n--- Market Status ---\n");
    printf("Interest Rate: %.2f%%\n", market->interest_rate);
    printf("News: %s\n", market->news[rand() % 2]); // Display a random news item
    printf("\n--- Companies ---\n");
    for (int i = 0; i < num_companies; i++) {
        printf("%s: $%.2f (%d shares)\n", companies[i].name, companies[i].price, companies[i].shares);
    }
    printf("---------------------\n");
}

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void display_main_menu(Player *player, Market *market) {
    clear_screen();
    printf("1. File | 2. Game Options | 3. Settings | 4. Help\n");
    printf("-----------------------------------------------------\
");
    printf("Active Entity Selected: %s\n", player->name);
    printf("----------------------------------------\n");
    printf("-Research Menus and Tools:-\
");
    printf("5. Select Player\t\t6. My News\n");
    printf("7. Entity Info\t\t8. Select Corp.\n");
    printf("9. Select Last\t\t10. General\n");
    printf("----------------------------------------\n");
    printf("-Transactions:-\
");
    printf("11. Buy Stock\t\t12. Buy/Sell\n");
    printf("13. Financing\t\t14. Sell Stock\n");
    printf("15. Management\t\t16. Other Trans\n");
    printf("----------------------------------------\n");
    printf("Stock Symbol.... N/A Go\t\tStock Price.......Start Game!\n");
    printf("Controlled By.....Start Game!\t\tActing Now.......%s\n", player->name);
    printf("---------------------------\n");
    printf("Date: January 1, 2025\t\tNext Earnings Release Date: N/A\n");
    printf("Player: %s\n", player->name);
    printf("-Other:-\
");
    printf("----------------------------------\n");
    printf("17. End Turn\t\t18. Misc. Menu\n");
    printf("19. Chart\t\t20. Cheat\n");
    printf("21. Add/Del. Select Fill Clear\n");
    printf("22. TICKER %s\n", player->ticker_on ? "[ON]" : "[OFF]");
    printf("----------------------------------\n");
    printf("My Balance Sheet: (Millions)\n");
    printf("Cash (DD's)....\t\t%.2f\n", player->cash);
    printf("Other Assets....\t\t0.00\n");
    printf("Total Assets.....\t\t%.2f\n", player->cash);
    printf("Less Debt....\t\t-0.00\n");
    printf("Net Worth........\t\t%.2f\n", player->cash);
    printf("----------------------------------------------\n");
    printf("FINANCIAL NEWS HEADLINES\n");
    printf("\n");
    printf("-Quick Search Functions:-\
");
    printf("23. Research Report\t\t24. List Portfolio Holdings\n");
    printf("25. List Futures Contracts\t\t26. Financial Profile\n");
    printf("27. List Put and Call Options\t\t28. Recall DB Search List\n");
    printf("----------------------------------\n");
    printf("Commodity Prices/Indexes/Indicators:\n");
    printf("Stock Index: Market closed\t\tPrime Rate: %.2f%%\n", market->interest_rate);
    printf("Long Bond: 7.25%%\t\tShort Bond: 6.50%%\n");
    printf("Spot Crude: $100.00\t\tSilver: $20.00\n");
    printf("Spot Gold: $1,200.00\t\tSpot Wheat: $5.00\n");
    printf("GDP Growth: 2.5%%\t\tSpot Corn: $5.00\n");
    printf("------------------------------------------\n");
}

void save_current_state(Player players[], int num_players, int game_length, Market *market, Company companies[], int num_companies, int current_year) {
    FILE *fp = fopen("gamestate.txt", "w");
    if (fp == NULL) {
        printf("Error: Could not open gamestate.txt for writing.\n");
        return;
    }

    fprintf(fp, "year:%d\n", current_year);
    fprintf(fp, "game_length:%d\n", game_length);
    fprintf(fp, "num_players:%d\n", num_players);

    // Save market data
    fprintf(fp, "market_interest_rate:%.2f\n", market->interest_rate);

    // Save player data
    for (int i = 0; i < num_players; i++) {
        fprintf(fp, "player_%d_name:%s\n", i, players[i].name);
        fprintf(fp, "player_%d_cash:%.2f\n", i, players[i].cash);
        fprintf(fp, "player_%d_type:%d\n", i, players[i].type);
        fprintf(fp, "player_%d_ticker_on:%d\n", i, players[i].ticker_on); // Save ticker state
    }

    // Save company data
    for (int i = 0; i < num_companies; i++) {
        fprintf(fp, "company_%d_name:%s\n", i, companies[i].name);
        fprintf(fp, "company_%d_price:%.2f\n", i, companies[i].price);
        fprintf(fp, "company_%d_shares:%d\n", i, companies[i].shares);
    }

    fclose(fp);
}

int player_turn(Player *player, Player all_players[], int num_players, int game_length, Company companies[], int num_companies, Market *market, int current_year) {
    if (player->type == HUMAN) {
        save_current_state(all_players, num_players, game_length, market, companies, num_companies, current_year);
        int choice;
        while (1) {
            display_main_menu(player, market);
            printf("\nEnter your choice (1-28): "); // Updated to 28 options
            scanf("%d", &choice);

            switch (choice) {
                case 1: // File
                    system("./+x/file_submenu.+x");
                    if (file_exists(".load_successful")) {
                        remove(".load_successful");
                        return 2; // Signal to load
                    }
                    break;
                case 3: // Settings
                    system("./+x/settings_submenu.+x");
                    break;
                case 17: // End Turn
                    printf("Passing turn...\n");
                    return 1; // End turn
                case 22: // TICKER ON/OFF
                    player->ticker_on = !player->ticker_on; // Toggle ticker state
                    printf("Ticker is now %s\n", player->ticker_on ? "ON" : "OFF");
                    printf("Press Enter to continue...");
                    while (getchar() != '\n'); // Consume leftover newline
                    getchar(); // Wait for user input
                    break;
                case 23: // Research Report
                    system("./+x/research_report.+x");
                    break;
                case 28: // Recall DB Search List
                    system("./+x/db_search.+x");
                    printf("Press Enter to return to main menu...");
                    while (getchar() != '\n'); // Consume leftover newline
                    getchar(); // Wait for user to press Enter
                    break;
                default:
                    if (choice > 1 && choice < 29) { // Updated to 28 options
                        printf("Not implemented yet.\n");
                    } else {
                        printf("Invalid choice. Please try again.\n");
                    }
                    break;
            }
        }
    } else {
        printf("\n--- Player Turn: %s (Computer) ---\n", player->name);
        printf("Computer player is thinking...\n");
        // Implement basic AI for computer player
        return 1; // End turn
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: ./game num_players game_length starting_cash [player_name player_type]...\n");
        return 1;
    }

    srand(time(NULL));
    system("./+x/setup_corporations.+x");
    system("./+x/setup_governments.+x governments/gov-list.txt");

    Player players[5];
    int num_players = atoi(argv[1]);
    int game_length = atoi(argv[2]);
    float starting_cash = atof(argv[3]);
    Market market;
    Company companies[5];

    int arg_index = 4;
    for (int i = 0; i < num_players; i++) {
        strcpy(players[i].name, argv[arg_index++]);
        players[i].type = (PlayerType)atoi(argv[arg_index++]);
        players[i].cash = starting_cash;
        players[i].ticker_on = false;
    }

    initialize_market(&market);
    initialize_companies(companies, 5);

    int i = 0;
    while (i < game_length) {
        printf("\n--- Year %d ---\n", i + 1);
        for (int j = 0; j < num_players; j++) {
            print_market_status(&market, companies, 5);
            int turn_result = player_turn(&players[j], players, num_players, game_length, companies, 5, &market, i + 1);

            if (turn_result == 2) { // Load game
                load_game_state(players, &num_players, &game_length, &market, companies, 5, &i);
                goto loop_restart;
            }
        }
        i++;
    loop_restart:;
    }

    printf("\n--- Game Over ---\n");

    return 0;
}
