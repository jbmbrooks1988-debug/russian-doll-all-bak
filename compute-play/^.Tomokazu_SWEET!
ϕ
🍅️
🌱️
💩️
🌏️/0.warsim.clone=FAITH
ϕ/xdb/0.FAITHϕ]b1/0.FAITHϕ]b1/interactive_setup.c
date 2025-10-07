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

// Function to get string input
void get_string_input(const char* prompt, char* buffer, size_t buffer_size) {
    printf("%s", prompt);
    if (fgets(buffer, buffer_size, stdin) != NULL) {
        // Remove the trailing newline character if present
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
    }
}

int main() {
    FILE* file;
    int num_players, game_length;
    char player_name[100];
    int player_type;
    int difficulty;
    
    clear_screen();
    
    // Check if starting_data.txt exists
    file = fopen("starting_data.txt", "r");
    if (file != NULL) {
        // File exists, read data from it
        fclose(file);
        printf("Found starting_data.txt, loading settings...\n");
        // In a real implementation, we would parse the file here
        // For now, we'll just run the interactive setup
    } else {
        printf("No starting_data.txt found, running interactive setup.\n");
    }
    
    printf("--- Game Setup ---\n");
    
    // Get number of players
    num_players = get_int_input("Enter number of players (1-5): ");
    while (num_players < 1 || num_players > 5) {
        printf("Invalid number of players. Please enter a number between 1 and 5.\n");
        num_players = get_int_input("Enter number of players (1-5): ");
    }
    
    // Get game length
    game_length = get_int_input("Enter game length (in years): ");
    while (game_length <= 0) {
        printf("Invalid game length. Please enter a positive number.\n");
        game_length = get_int_input("Enter game length (in years): ");
    }
    
    // Get player details
    for (int i = 1; i <= num_players; i++) {
        printf("\n--- Player %d ---\n", i);
        
        // Get player name
        get_string_input("Enter name: ", player_name, sizeof(player_name));
        
        // Get player type
        player_type = get_int_input("Enter player type (0 for Human, 1 for Computer): ");
        while (player_type != 0 && player_type != 1) {
            printf("Invalid player type. Please enter 0 for Human or 1 for Computer.\n");
            player_type = get_int_input("Enter player type (0 for Human, 1 for Computer): ");
        }
        
        // Save player data to a file
        char filename[100];
        snprintf(filename, sizeof(filename), "player_%d.txt", i);
        file = fopen(filename, "w");
        if (file != NULL) {
            fprintf(file, "Name: %s\n", player_name);
            fprintf(file, "Type: %s\n", player_type == 0 ? "Human" : "Computer");
            fclose(file);
        } else {
            printf("Error: Could not save player data to %s\n", filename);
        }
    }
    
    // Choose difficulty
    clear_screen();
    printf("---------------------------choose your difficulty---------------------------\n");
    printf("1) Easy   (10 lands) (1000 peasants) (400 soldiers) (10 knights) (5000gold)\n");
    printf("2) Normal (5 lands) (500 peasants) (200 soldiers) (5 knights) (2000gold)\n");
    printf("3) Hard   (3 lands) (300 peasants) (75 soldiers) (3 knights) (750gold)\n");
    printf("4) Solid  (1 lands) (100 peasants) (25 soldiers) (1 knights) (250gold)\n");
    printf("____________________________________________________________________________\n");
    
    difficulty = get_int_input("Enter your choice: ");
    while (difficulty < 1 || difficulty > 4) {
        printf("Invalid choice. Please enter a number between 1 and 4.\n");
        difficulty = get_int_input("Enter your choice: ");
    }
    
    // Save difficulty to a file
    file = fopen("difficulty.txt", "w");
    if (file != NULL) {
        fprintf(file, "%d\n", difficulty);
        fclose(file);
    } else {
        printf("Error: Could not save difficulty setting\n");
    }
    
    printf("Setup complete. Press any key to continue...\n");
    press_any_key();
    
    return 0;
}