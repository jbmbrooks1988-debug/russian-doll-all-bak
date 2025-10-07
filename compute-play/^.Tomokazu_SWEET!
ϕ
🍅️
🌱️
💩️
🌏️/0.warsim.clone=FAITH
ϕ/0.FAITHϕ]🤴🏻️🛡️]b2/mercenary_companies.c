#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Function to display mercenaries menu
void display_mercenaries_menu(int gold) {
    printf("------------MERCENARY GROUPS------------------------------------------------\n");
    printf("1) Purple-Bannered Soul Angels (cost 3681 per year)\n");
    printf("2) Blue Granite Razors (cost 10233 per year)\n");
    printf("3) Golden Dawn Rangers (cost 5918 per year)\n");
    printf("4) Ultimate Soldiers (cost 4911 per year)\n");
    printf("5) Great Wind Shields (cost 6683 per year)\n");
    printf("\n");
    printf("0) Exit\n");
    printf("____________________________________________________________________________\n");
}

// Function to handle hiring mercenaries
void handle_mercenaries(int* gold) {
    int choice;
    int cost;
    char* mercenary_names[] = {
        "Purple-Bannered Soul Angels",
        "Blue Granite Razors",
        "Golden Dawn Rangers",
        "Ultimate Soldiers",
        "Great Wind Shields"
    };
    
    while (1) {
        clear_screen();
        display_mercenaries_menu(*gold);
        choice = get_int_input("Enter your choice: ");
        
        // Validate choice
        if (choice < 0 || choice > 5) {
            printf("Invalid choice. Please try again.\n");
            press_any_key();
            continue;
        }
        
        // Exit
        if (choice == 0) {
            break;
        }
        
        // Get cost for selected mercenary group
        int costs[] = {3681, 10233, 5918, 4911, 6683};
        cost = costs[choice - 1];
        
        // Check if player can afford
        if (cost > *gold) {
            printf("You don't have enough gold to hire the %s.\n", mercenary_names[choice - 1]);
        } else {
            *gold -= cost;
            printf("You have hired the %s for %d gold per year.\n", mercenary_names[choice - 1], cost);
            // In a real implementation, we would save this information to a file
        }
        
        press_any_key();
    }
}

int main() {
    // In a real implementation, we would load the current gold from game state files
    // For now, we'll use a default value
    int gold = 66;
    
    handle_mercenaries(&gold);
    
    // In a real implementation, we would save the updated gold to game state files
    printf("Remaining gold: %d\n", gold);
    
    return 0;
}
