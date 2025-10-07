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

// Function to display recruitment menu
void display_recruitment_menu(int gold) {
    printf("------RECRUITMENT MENU-----------you have [%d Gold]-----------------------\n", gold);
    printf("1) Hire Peasants from the local militia (20 gold)\n");
    printf("2) Hire Soldiers from the local Guilds and Forts (50 gold)\n");
    printf("3) Hire Knight from the noble houses (500 gold)\n");
    printf("x) There are no bandits currently in prison to be hired\n");
    printf("5) Hire Tribal Goblin from the local goblin slaver (5 gold)\n");
    printf("\n");
    printf("0) exit\n");
    printf("____________________________________________________________________________\n");
}

// Function to handle recruitment
void handle_recruitment(int* gold, int* men) {
    int choice;
    int quantity;
    int cost;
    
    while (1) {
        clear_screen();
        display_recruitment_menu(*gold);
        choice = get_int_input("Enter your choice: ");
        
        // Validate choice
        if (choice < 0 || (choice > 5 && choice != 'x')) {
            printf("Invalid choice. Please try again.\n");
            press_any_key();
            continue;
        }
        
        // Exit
        if (choice == 0) {
            break;
        }
        
        // Handle recruitment options
        switch (choice) {
            case 1: // Hire Peasants
                cost = 20;
                quantity = get_int_input("How many Peasants would you like to hire? ");
                if (quantity * cost > *gold) {
                    printf("You don't have enough gold for that.\n");
                } else {
                    *gold -= quantity * cost;
                    *men += quantity;
                    printf("You hired %d Peasants.\n", quantity);
                }
                break;
                
            case 2: // Hire Soldiers
                cost = 50;
                quantity = get_int_input("How many Soldiers would you like to hire? ");
                if (quantity * cost > *gold) {
                    printf("You don't have enough gold for that.\n");
                } else {
                    *gold -= quantity * cost;
                    *men += quantity;
                    printf("You hired %d Soldiers.\n", quantity);
                }
                break;
                
            case 3: // Hire Knight
                cost = 500;
                if (cost > *gold) {
                    printf("You don't have enough gold for that.\n");
                } else {
                    *gold -= cost;
                    *men += 1; // Simplified: 1 knight
                    printf("You hired 1 Knight.\n");
                }
                break;
                
            case 5: // Hire Tribal Goblin
                cost = 5;
                quantity = get_int_input("How many Tribal Goblins would you like to hire? ");
                if (quantity * cost > *gold) {
                    printf("You don't have enough gold for that.\n");
                } else {
                    *gold -= quantity * cost;
                    *men += quantity;
                    printf("You hired %d Tribal Goblins.\n", quantity);
                }
                break;
                
            default:
                printf("Invalid choice or not implemented yet.\n");
                break;
        }
        
        press_any_key();
    }
}

int main() {
    // In a real implementation, we would load the current gold and men from game state files
    // For now, we'll use default values
    int gold = 66;
    int men = 1449;
    
    handle_recruitment(&gold, &men);
    
    // In a real implementation, we would save the updated gold and men to game state files
    printf("Final resources: %d Gold, %d Men\n", gold, men);
    
    return 0;
}