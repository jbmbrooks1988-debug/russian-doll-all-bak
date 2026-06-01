#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// This is the new game module designed to work with the pipe-based IPC system.
int main() {
    int player_x = 50;
    int player_y = 50;
    char command[256];

    // The game runs in an infinite loop.
    // It waits for input on stdin, updates state, and prints the new state to stdout.
    while (1) {
        // Read a command from stdin (which is piped from the main app).
        if (fgets(command, sizeof(command), stdin) != NULL) {
            // Remove newline character from the command
            command[strcspn(command, "\n")] = 0;

            // Update player position based on the command.
            if (strcmp(command, "UP") == 0) {
                player_y += 10;
            } else if (strcmp(command, "DOWN") == 0) {
                player_y -= 10;
            } else if (strcmp(command, "LEFT") == 0) {
                player_x -= 10;
            } else if (strcmp(command, "RIGHT") == 0) {
                player_x += 10;
            }

            // Write the new game state to stdout (which is piped to the main app).
            // Format: TYPE,x,y,size,r,g,b,a
            printf("SQUARE,%d,%d,20,0.0,0.0,1.0,1.0\n", player_x, player_y);

            // Flush stdout to ensure the parent process receives the message immediately.
            fflush(stdout);
        }
    }

    return 0;
}
