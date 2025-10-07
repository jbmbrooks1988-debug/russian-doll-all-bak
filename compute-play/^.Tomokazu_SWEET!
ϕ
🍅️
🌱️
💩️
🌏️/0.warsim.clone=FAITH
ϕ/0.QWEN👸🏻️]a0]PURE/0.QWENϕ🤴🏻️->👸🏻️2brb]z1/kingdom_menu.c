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
    printf("-------PRINCESS MENU-------------you have [%d Glitz]-----------------------\n", gold);
    printf("1)  Design New Outfits\n");
    printf("2)  Hire Stylists & Image Teams\n");
    printf("3)  Recruit Admirers & Friends\n");
    printf("4)  Arrange Social Engagements\n");
    printf("5)  Change Your Personality Traits\n");
    printf("6)  Personal Upgrades\n");
    printf("7)  Attend the Glamour Arena\n");
    printf("8)  Visit the Sparkle Bank\n");
    printf("9)  Life Management\n");
    printf("10) Explore the City\n");
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
            case 1: // Design New Outfits
                // In a real implementation, we would call the recruit_troops executable
                printf("Design New Outfits selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 2: // Hire Stylists & Image Teams
                // In a real implementation, we would call the mercenary_companies executable
                printf("Hire Stylists & Image Teams selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 3: // Recruit Admirers & Friends
                // In a real implementation, we would call the heroes_champions executable
                printf("Recruit Admirers & Friends selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 4: // Arrange Social Engagements
                // In a real implementation, we would call the diplomacy executable
                printf("Arrange Social Engagements selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 5: // Change Your Personality Traits
                // In a real implementation, we would call the laws executable
                printf("Change Your Personality Traits selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 6: // Personal Upgrades
                // In a real implementation, we would call the kingdom_upgrades executable
                printf("Personal Upgrades selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 7: // Attend the Glamour Arena
                // In a real implementation, we would call the arena executable
                printf("Attend the Glamour Arena selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 8: // Visit the Sparkle Bank
                // In a real implementation, we would call the royal_bank executable
                printf("Visit the Sparkle Bank selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 9: // Life Management
                // In a real implementation, we would call the kingdom_management executable
                printf("Life Management selected.\n");
                // For now, we'll just show a message
                press_any_key();
                break;
                
            case 10: // Explore the City
                // In a real implementation, we would call the explore_realm executable
                printf("Explore the City selected.\n");
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