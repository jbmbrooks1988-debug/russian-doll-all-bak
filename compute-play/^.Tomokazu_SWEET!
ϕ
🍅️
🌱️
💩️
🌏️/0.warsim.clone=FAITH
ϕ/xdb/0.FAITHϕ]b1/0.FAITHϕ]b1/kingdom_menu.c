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

// Function to display kingdom menu
void display_kingdom_menu(int gold) {
    printf("-------KINGDOM MENU-------------you have [%d Gold]-----------------------\n", gold);
    printf("1)  Recruit Troops\n");
    printf("2)  Hire Mercenary companies\n");
    printf("3)  Hire Heroes and Champions\n");
    printf("4)  Arrange Diplomacy\n");
    printf("5)  Change Laws of the Land\n");
    printf("6)  Kingdom Upgrades\n");
    printf("7)  Visit the Arena\n");
    printf("8)  Visit the Royal Bank\n");
    printf("9)  Kingdom Management\n");
    printf("10) Explore the Realm\n");
    printf("\n");
    printf("0) exit\n");
    printf("____________________________________________________________________________\n");
}

int main() {
    // In a real implementation, we would load the current gold from game state files
    // For now, we'll use a default value
    int gold = 66;
    int choice;
    
    while (1) {
        clear_screen();
        display_kingdom_menu(gold);
        choice = get_int_input("Enter your choice: ");
        
        // Validate choice
        if (choice < 0 || choice > 10) {
            printf("Invalid choice. Please try again.\n");
            press_any_key();
            continue;
        }
        
        // Exit
        if (choice == 0) {
            break;
        }
        
        // Handle menu options
        switch (choice) {
            case 1: // Recruit Troops
                // In a real implementation, we would call the recruit_troops executable
                printf("Recruit Troops selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 2: // Hire Mercenary companies
                // In a real implementation, we would call the mercenary_companies executable
                printf("Hire Mercenary companies selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 3: // Hire Heroes and Champions
                // In a real implementation, we would call the heroes_champions executable
                printf("Hire Heroes and Champions selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 4: // Arrange Diplomacy
                // In a real implementation, we would call the diplomacy executable
                printf("Arrange Diplomacy selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 5: // Change Laws of the Land
                // In a real implementation, we would call the laws executable
                printf("Change Laws of the Land selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 6: // Kingdom Upgrades
                // In a real implementation, we would call the kingdom_upgrades executable
                printf("Kingdom Upgrades selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 7: // Visit the Arena
                // In a real implementation, we would call the arena executable
                printf("Visit the Arena selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 8: // Visit the Royal Bank
                // In a real implementation, we would call the royal_bank executable
                printf("Visit the Royal Bank selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 9: // Kingdom Management
                // In a real implementation, we would call the kingdom_management executable
                printf("Kingdom Management selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 10: // Explore the Realm
                // In a real implementation, we would call the explore_realm executable
                printf("Explore the Realm selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            default:
                printf("Invalid choice.\n");
                press_any_key();
                break;
        }
    }
    
    return 0;
}