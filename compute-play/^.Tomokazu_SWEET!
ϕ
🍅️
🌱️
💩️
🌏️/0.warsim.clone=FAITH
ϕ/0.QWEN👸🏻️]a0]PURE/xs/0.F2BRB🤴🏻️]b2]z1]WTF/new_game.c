#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Function to clear the screen (cross-platform)
void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Function to pause the program (cross-platform)
void press_any_key() {
    printf("Press any key to continue . . . ");
    getchar(); // Wait for user to press Enter
    // Consume any leftover newline character
    if (getchar() != '\n') {
        // If there was no leftover newline, we consumed the actual key press
        // If there was a leftover newline, we need to consume the actual key press
        // This is a simple way to handle it, though not perfect for all cases
    }
}

// Function to get integer input with validation
int get_int_input(const char* prompt) {
    int value;
    char buffer[100];
    
    while (1) {
        printf("%s", prompt);
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            char* endptr;
            value = strtol(buffer, &endptr, 10);
            
            // Check if the entire string was consumed and is a valid integer
            if (endptr != buffer && (*endptr == '\n' || *endptr == '\0')) {
                return value;
            } else {
                printf("Invalid input. Please enter a valid integer.\n");
            }
        } else {
            printf("Error reading input. Please try again.\n");
        }
    }
}

// Function to simulate dice roll
int roll_dice(int sides) {
    return (rand() % sides) + 1;
}

// Function to load game state from files
void load_game_state(int* year, int* gold, int* men, int* lands, int* faction_strength) {
    FILE* file;
    
    // Initialize with default values
    *year = 100;
    *gold = 5000;
    *men = 1410;
    *lands = 10;
    *faction_strength = 3;
    
    // Try to load from save files
    file = fopen("game_state.txt", "r");
    if (file != NULL) {
        fscanf(file, "%d %d %d %d %d", year, gold, men, lands, faction_strength);
        fclose(file);
    }
    
    // Load difficulty setting
    int difficulty = 2; // Default to Normal
    file = fopen("difficulty.txt", "r");
    if (file != NULL) {
        fscanf(file, "%d", &difficulty);
        fclose(file);
        
        // Adjust starting values based on difficulty
        switch (difficulty) {
            case 1: // Easy
                *gold = 5000;
                *men = 1410;
                *lands = 10;
                break;
            case 2: // Normal
                *gold = 2000;
                *men = 725;
                *lands = 5;
                break;
            case 3: // Hard
                *gold = 750;
                *men = 400;
                *lands = 3;
                break;
            case 4: // Solid
                *gold = 250;
                *men = 126;
                *lands = 1;
                break;
        }
    }
}

// Function to save game state to file
void save_game_state(int year, int gold, int men, int lands, int faction_strength) {
    FILE* file = fopen("game_state.txt", "w");
    if (file != NULL) {
        fprintf(file, "%d %d %d %d %d\n", year, gold, men, lands, faction_strength);
        fclose(file);
    }
}

// Function to display Princess Life stats
void display_stats(int year, int gold, int men, int lands, int faction_strength) {
    printf("----- Month %d-----------------ASLONA STATS---------------------------------\n", year);
    printf("Glitz %d - Charm %d - Lands %d - Faction Strength %d\n", gold, men, lands, faction_strength);
    printf("\n");
}

// Function to handle random events
void handle_random_events(int* gold, int* men, int* faction_strength) {
    int event = roll_dice(10);
    
    switch (event) {
        case 1:
        case 2:
            printf("nothing of interest has happened in the realm recently\n");
            break;
        case 3:
            printf("one of your enlisted Knights wins a tournament in a nearby Princess Life\n");
            printf("and dedicates the win to your honour.\n");
            printf("(+1 Threat)\n");
            (*faction_strength)++;
            break;
        // Add more events as needed
        default:
            printf("A mysterious traveler passes through your lands.\n");
            break;
    }
    printf("----------------------------------------------------------------------------\n");
}

// Function to handle income and expenses
void handle_economy(int* gold, int* men, int lands) {
    int rent_income = lands * 20; // Simplified rent calculation
    int enlistment = lands * 4;   // Simplified enlistment calculation
    int training = 50;            // Fixed training amount
    int harvest_income = 500;     // Simplified harvest income
    int bandit_loss = 3000;       // Simplified bandit loss
    int other_bandit_loss = 200;  // Simplified other bandit loss
    int soldier_wages = 500;      // Simplified soldier wages
    int knight_wages = 30;        // Simplified knight wages
    
    printf("----END OF TURN REPORT------------------------------------------------------\n");
    printf("you receive %d gold in rents from your %d Lands\n", rent_income, lands);
    printf("you enlist %d soldiers from your %d Lands\n", enlistment, lands);
    printf("your Knights train %d Peasants into Soldiers\n", training);
    printf("your peasants earn you %d gold from this seasons harvest\n", harvest_income);
    printf("Bandits pillage %d gold from you this season\n", bandit_loss);
    printf("Other Bandit Groups pillage %d gold from you this season\n", other_bandit_loss);
    printf("you pay %d in wages to your soldiers\n", soldier_wages);
    printf("you pay %d in wages to your Knights\n", knight_wages);
    
    // Update gold and men
    *gold += rent_income + harvest_income - bandit_loss - other_bandit_loss - soldier_wages - knight_wages;
    *men += enlistment + training;
    
    // Ensure gold doesn't go negative
    if (*gold < 0) {
        *gold = 0;
    }
}

// Function to handle spy reports
void handle_spy_reports() {
    printf("\n----SPY REPORTS-------------------------------------------------------------\n");
    printf("the rebels enlist 3 new soldiers from their 6 Lands\n");
    printf("the bandits enlist 21 new bandits and 5 new warlords from their 5 Lands\n");
    printf("the K'rut enlist 15 tribals, 12 Berserkers and  6 Warlords from their 6 Lands\n");
    printf("the E'rak enlist 14 tribals, 16 Berserkers and 8 Warlords from their 8 Lands\n");
    printf("\n");
}

// Function to display attack options
int get_attack_choice() {
    printf("General Ghorin: My King, what are your orders?\n");
    printf("1) We can attack the Rebels [At War]\n");
    printf("2) We can attack the Bandits\n");
    printf("3) We can attack the K'rut\n");
    printf("4) We can attack the e'rak\n");
    printf("5) We can attack the Murky Knaves (Bandits)\n");
    printf("6) We can attack the The Green-Bannered Dusk Hounds (Bandits)\n");
    printf("7) We can attack the The Marked Dusk Gatekeepers (Bandits)\n");
    printf("8) We can attack the The Purple Frost Razors (Bandits)\n");
    printf("9) We can attack the Warborn Dark Marsh Raiders (Bandits)\n");
    printf("10) We can attack the Purple Dead Dutchy (Independent)\n");
    printf("11) We can attack the Royal Free Free City (Independent)\n");
    printf("12) We can attack the The Faceless Dutchy (Independent)\n");
    printf("13) We can attack the Monstrous Hill Free City (Independent)\n");
    printf("14) We can attack the The Reining Land (Independent)\n");
    printf("0) Launch no attacks\n");
    printf("____________________________________________________________________________\n");
    
    return get_int_input("Enter your choice: ");
}

int main() {
    int year, gold, men, lands, faction_strength;
    int choice;
    
    // Seed the random number generator
    srand(time(NULL));
    
    // Load game state
    load_game_state(&year, &gold, &men, &lands, &faction_strength);
    
    // Game loop
    while (1) {
        clear_screen();
        
        // Display stats
        display_stats(year, gold, men, lands, faction_strength);
        
        // Handle random events
        handle_random_events(&gold, &men, &faction_strength);
        
        // Get attack choice
        choice = get_attack_choice();
        
        // Validate choice
        if (choice < 0 || choice > 14) {
            printf("Invalid choice. Please try again.\n");
            press_any_key();
            continue;
        }
        
        if (choice == 0) {
        system("./+x/Princess Life_menu.+x");
        }
        
        // Handle attack (simplified)
        if (choice > 0) {
            printf("Launching attack on option %d...\n", choice);
            // In a real implementation, this would involve combat calculations
            // For now, we'll just simulate a simple outcome
            int outcome = roll_dice(3);
            switch (outcome) {
                case 1:
                    printf("Your attack was successful!\n");
                    gold += 1000; // Reward for successful attack
                    break;
                case 2:
                    printf("Your attack was inconclusive.\n");
                    break;
                case 3:
                    printf("Your attack failed and you lost some men.\n");
                    men -= 50; // Loss for failed attack
                    break;
            }
            press_any_key();
        }
        
        // Handle economy
        handle_economy(&gold, &men, lands);
        
        // Handle spy reports
        handle_spy_reports();
        
        // Save game state
        save_game_state(year, gold, men, lands, faction_strength);
        
        // Wait for user input before next turn
        press_any_key();
        
        // Advance to next year
        year++;
    }
    
    return 0;
}
