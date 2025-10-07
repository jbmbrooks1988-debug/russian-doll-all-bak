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

// Function to run a menu executable
void run_menu(const char* menu_name) {
    char command[256];
    snprintf(command, sizeof(command), "./+x/%s.+x", menu_name);
    int result = system(command);
    if (result != 0) {
        printf("Error: Failed to run menu '%s'\n", menu_name);
        press_any_key();
    }
}

// Main game loop
int main() {
    int choice;
    
    // Initial setup
   
    
    // Main menu loop
    while (1) {
        clear_screen();
        printf("[Welcome to F.A.I.T.H.: Guardians of The ϕ (Full Release 1.0.0) ]\n");
        printf("1) Start a New Game\n");
        printf("2) Load a Savegame\n");
        printf("3) Quickstart (Instant new game)\n");
        printf("4) Options and Settings\n");
        printf("5) Start a new Challenge Mode Game\n");
        printf("6) Extras and Generators\n");
        printf("7) Report a bug/Suggest a feature\n");
        printf("8) Community Links\n");
        printf("9) View your scores\n");
        printf("10) Help the text is too small on the screen\n");
        printf("11) Quit to Desktop\n");
        printf("______________________________________________________\n");
        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            // Clear the input buffer if scanf fails
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            printf("Invalid input. Please enter a number.\n");
            press_any_key();
            continue;
        }
        
        // Consume the newline character left in the buffer
        getchar();
        
        switch (choice) {
            case 1:
           //  run_menu("interactive_setup");
                run_menu("new_game");
                
                break;
            case 2:
                run_menu("load_savegame");
                break;
            case 3:
                run_menu("quickstart");
                break;
            case 4:
                run_menu("options_settings");
                break;
            case 5:
                run_menu("challenge_mode");
                break;
            case 6:
                run_menu("extras_generators");
                break;
            case 7:
                run_menu("report_bug");
                break;
            case 8:
                run_menu("community_links");
                break;
            case 9:
                run_menu("view_scores");
                break;
            case 10:
                run_menu("text_size_help");
                break;
            case 11:
                printf("Quitting to desktop...\n");
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
                press_any_key();
        }
    }
    
    return 0;
}
