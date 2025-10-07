// orchestrator.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LEN 50
#define MAX_GAMES 10

// Game entry
typedef struct {
    char name[MAX_NAME_LEN];
    char command[100];
} Game;

Game games[MAX_GAMES];
int game_count = 0;

// Add a game to the menu
void add_game(const char* name, const char* cmd) {
    if (game_count < MAX_GAMES) {
        strncpy(games[game_count].name, name, MAX_NAME_LEN - 1);
        strncpy(games[game_count].command, cmd, 99);
        games[game_count].name[MAX_NAME_LEN - 1] = '\0';
        games[game_count].command[99] = '\0';
        game_count++;
    }
}

// Clear screen (works on Linux/macOS/Windows Terminal)
void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

// Show menu
void show_menu() {
    clear_screen();
    printf("\n");
    printf("🎮 === GAME ORCHESTRATOR === 🎮\n");
    printf("💖 Welcome, superstar! Choose a game:\n\n");
    for (int i = 0; i < game_count; i++) {
        printf("  %d. %s\n", i + 1, games[i].name);
    }
    printf("  0. Exit\n");
    printf("\n👉 Enter choice: ");
}

// Run selected game
void launch_game(int choice) {
    if (choice < 1 || choice > game_count) return;

    clear_screen();
    printf("\n🚀 Launching: %s...\n", games[choice - 1].name);
    printf("💡 Press any key to return after game closes.\n");
    printf("------------------------------------------------\n");

    int result = system(games[choice - 1].command);
    if (result != 0) {
        printf("⚠️ Game exited with error code: %d\n", result);
    }

    printf("\n👋 Back to menu. Press Enter to continue...");
    getchar(); // Catch leftover newline
    getchar(); // Wait for enter
}

// Main
int main() {
    // Register your games here
    add_game("CLI Jeopardy", "./jeopardy");
    add_game("Tappy Turd (OpenGL Poop Game)", "./tappy_turd_gl");

    int choice = -1;

    while (1) {
        show_menu();
        if (scanf("%d", &choice) != 1) {
            // Clear invalid input
            while (getchar() != '\n');
            printf("❌ Invalid input! Press Enter...");
            getchar();
            continue;
        }

        if (choice == 0) {
            printf("\n👋 Goodbye, legend! Keep coding, keep flushing. 💖\n");
            break;
        } else if (choice > 0 && choice <= game_count) {
            launch_game(choice);
        } else {
            printf("❌ Invalid choice! Press Enter...");
            getchar();
        }
    }

    return 0;
}
