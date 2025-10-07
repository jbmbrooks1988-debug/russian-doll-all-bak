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
void load_game_state(int* month, int* glitz, int* charm, int* outfits, int* popularity) {
    FILE* file;
    
    // Initialize with default values
    *month = 1;
    *glitz = 5000;
    *charm = 1410;
    *outfits = 10;
    *popularity = 3;
    
    // Try to load from save files
    file = fopen("game_state.txt", "r");
    if (file != NULL) {
        fscanf(file, "%d %d %d %d %d", month, glitz, charm, outfits, popularity);
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
                *glitz = 5000;
                *charm = 1410;
                *outfits = 10;
                break;
            case 2: // Normal
                *glitz = 2000;
                *charm = 725;
                *outfits = 5;
                break;
            case 3: // Hard
                *glitz = 750;
                *charm = 400;
                *outfits = 3;
                break;
            case 4: // Solid
                *glitz = 250;
                *charm = 126;
                *outfits = 1;
                break;
        }
    }
}

// Function to save game state to file
void save_game_state(int month, int glitz, int charm, int outfits, int popularity) {
    FILE* file = fopen("game_state.txt", "w");
    if (file != NULL) {
        fprintf(file, "%d %d %d %d %d\n", month, glitz, charm, outfits, popularity);
        fclose(file);
    }
}

// Function to display life stats
void display_stats(int month, int glitz, int charm, int outfits, int popularity) {
    printf("----- Month %d-----------------ASLONA'S LIFE STATS---------------------------------\n", month);
    printf("Glitz %d - Charm %d - Outfits %d - Popularity %d\n", glitz, charm, outfits, popularity);
    printf("\n");
}

// Function to handle random events
void handle_random_events(int* glitz, int* charm, int* popularity) {
    int event = roll_dice(10);
    
    switch (event) {
        case 1:
        case 2:
            printf("Nothing exciting happened this month... or so the gossip says.\n");
            break;
        case 3:
            printf("One of your classmates wins the school fashion contest\n");
            printf("and dedicates her win to your style inspiration.\n");
            printf("(+1 Gossip Wave)\n");
            (*popularity)++;
            break;
        // Add more events as needed
        default:
            printf("A mysterious traveler passes through your lands.\n");
            break;
    }
    printf("----------------------------------------------------------------------------\n");
}

// Function to handle income and expenses
void handle_economy(int* glitz, int* charm, int outfits) {
    int outfit_income = outfits * 20; // Income from outfits being seen
    int follower_gain = outfits * 4;   // New followers from social media
    int admirer_charm = 50;            // Charm from admirers
    int modeling_income = 500;         // Modeling gig income
    int rival_loss = 3000;             // Loss from rivals
    int rumor_loss = 200;              // Loss from rumors
    int upkeep_cost = 500;             // Makeup and accessory upkeep
    int gift_cost = 30;                // Admirer gift maintenance
    
    printf("----END OF MONTH REPORT------------------------------------------------------\n");
    printf("You earned %d glitz from your %d outfits being seen around town\n", outfit_income, outfits);
    printf("You gained %d new followers on StarNet (social media)\n", follower_gain);
    printf("Your admirers trained %d charm points through secret admiration\n", admirer_charm);
    printf("You earned %d glitz from this month's part-time modeling gig\n", modeling_income);
    printf("Band of Rivals stole %d glitz from your spotlight with a surprise drop\n", rival_loss);
    printf("Other Gossip Circles spread %d rumors reducing your charm\n", rumor_loss);
    printf("You paid %d glitz in makeup and accessory upkeep\n", upkeep_cost);
    printf("You paid %d glitz in admirer gift maintenance\n", gift_cost);
    
    // Update glitz and charm
    *glitz += outfit_income + modeling_income - rival_loss - rumor_loss - upkeep_cost - gift_cost;
    *charm += admirer_charm - rumor_loss;
    
    // Ensure glitz doesn't go negative
    if (*glitz < 0) {
        *glitz = 0;
    }
}

// Function to handle gossip reports
void handle_gossip_reports() {
    printf("\n----Gossip Network REPORTS-------------------------------------------------------------\n");
    printf("The Drama Club gained 3 new members from their 6 events\n");
    printf("The Trendsetters hosted 21 new parties and 5 new influencer meetups\n");
    printf("The K'rut Market launched 15 new accessories, 12 bold looks, and 6 trend waves\n");
    printf("The E'rak Art Café released 14 new art collabs, 16 fashion sketches, and 8 collab dates\n");
    printf("\n");
}

// Function to display activity options
int get_activity_choice() {
    printf("Mama_Marie: My dear, what would you like to do today?\n");
    printf("1) Attend School Drama Club [In Progress]\n");
    printf("2) Hang out with the Band of Trendsetters\n");
    printf("3) Visit the K'rut Fashion Market\n");
    printf("4) Join the E'rak Art Café\n");
    printf("5) Tea Party with the Murky Gossip Circle\n");
    printf("6) Sleepover at the Green-Bannered Dorm\n");
    printf("7) Photo Shoot with the Marked Dusk Crew\n");
    printf("8) Ice Skating with the Purple Frost Squad\n");
    printf("9) Movie Night with the Warborn Marsh Crew\n");
    printf("10) Date at the Purple Dead Café (Casual)\n");
    printf("11) Brunch at the Royal Free Free Lounge (Public)\n");
    printf("12) Fashion Show at The Faceless Runway\n");
    printf("13) Talent Showcase at Monstrous Hill Stage\n");
    printf("14) Royal Reining Ball (Invite Only)\n");
    printf("0) Stay home and rest\n");
    printf("______________________________________________________\n");
    
    return get_int_input("Enter your choice: ");
}

int main() {
    int month, glitz, charm, outfits, popularity;
    int choice;
    
    // Seed the random number generator
    srand(time(NULL));
    
    // Load game state
    load_game_state(&month, &glitz, &charm, &outfits, &popularity);
    
    // Game loop
    while (1) {
        clear_screen();
        
        // Display stats
        display_stats(month, glitz, charm, outfits, popularity);
        
        // Handle random events
        handle_random_events(&glitz, &charm, &popularity);
        
        // Get activity choice
        choice = get_activity_choice();
        
        // Validate choice
        if (choice < 0 || choice > 14) {
            printf("Invalid choice. Please try again.\n");
            press_any_key();
            continue;
        }
        
        if (choice == 0) {
            system("./+x/kingdom_menu.+x");
        }
        
        // Handle activity (simplified)
        if (choice > 0) {
            printf("Engaging in activity %d...\n", choice);
            // In a real implementation, this would involve various calculations
            // For now, we'll just simulate a simple outcome
            int outcome = roll_dice(3);
            switch (outcome) {
                case 1:
                    printf("Your activity was successful!\n");
                    glitz += 1000; // Reward for successful activity
                    break;
                case 2:
                    printf("Your activity was inconclusive.\n");
                    break;
                case 3:
                    printf("Your activity failed and you lost some resources.\n");
                    glitz -= 50; // Loss for failed activity
                    break;
            }
            press_any_key();
        }
        
        // Handle economy
        handle_economy(&glitz, &charm, outfits);
        
        // Handle gossip reports
        handle_gossip_reports();
        
        // Save game state
        save_game_state(month, glitz, charm, outfits, popularity);
        
        // Wait for user input before next turn
        press_any_key();
        
        // Advance to next month
        month++;
    }
    
    return 0;
}
