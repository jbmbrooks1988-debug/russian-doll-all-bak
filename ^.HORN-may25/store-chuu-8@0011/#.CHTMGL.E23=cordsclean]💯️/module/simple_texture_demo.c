#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

// Simple texture demo module
int main() {
    float player_x = 350.0f;  // Start in middle of canvas
    float player_y = 150.0f;
    float player_z = 0.0f;
    char command[256];
    
    // Game loop
    while (1) {
        // Read command from stdin
        if (fgets(command, sizeof(command), stdin) != NULL) {
            // Remove newline
            command[strcspn(command, "\n")] = 0;
            
            // Handle movement commands
            if (strcmp(command, "UP") == 0) {
                player_y -= 5.0f;
            } else if (strcmp(command, "DOWN") == 0) {
                player_y += 5.0f;
            } else if (strcmp(command, "LEFT") == 0) {
                player_x -= 5.0f;
            } else if (strcmp(command, "RIGHT") == 0) {
                player_x += 5.0f;
            }
        }
        
        // Keep player on screen
        if (player_x < 20.0f) player_x = 20.0f;
        if (player_x > 680.0f) player_x = 680.0f;
        if (player_y < 20.0f) player_y = 20.0f;
        if (player_y > 280.0f) player_y = 280.0f;
        
        // Send textured player square (blue square with texture)
        printf("SHAPE;SQUARE;player;%.2f;%.2f;%.2f;30.0;30.0;5.0;0.0;0.0;1.0;1.0;0;0;0;1;1;1;texture_src=missingno.png\n", 
               player_x, player_y, player_z);
        
        // Send a static textured square (red square with same texture)
        printf("SHAPE;SQUARE;static_red;200.0;100.0;0.0;30.0;30.0;5.0;1.0;0.0;0.0;1.0;0;0;0;1;1;1;texture_src=missingno.png\n");
        
        // Send a textured circle (green circle with texture)
        printf("SHAPE;CIRCLE;green_circle;500.0;100.0;0.0;25.0;25.0;5.0;0.0;1.0;0.0;1.0;0;0;0;1;1;1;texture_src=missingno.png\n");
        
        // Send textured triangle (orange triangle with texture)
        printf("SHAPE;TRIANGLE;triangle;300.0;200.0;0.0;20.0;20.0;5.0;1.0;0.5;0.0;1.0;0;0;0;1;1;1;texture_src=missingno.png\n");
        
        fflush(stdout);
        
        // Brief pause to control update rate
        usleep(16000); // About 60 FPS
    }
    
    return 0;
}