#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Player position variables
float player_x = 0.0f;
float player_y = 0.0f;
float player_z = 0.0f;

int main() {
    // Print initial state
    printf("VAR;page_status;string;Player Movement Module Loaded\n");
    printf("VAR;position_display;string;Player Position: X: %.1f, Y: %.1f, Z: %.1f\n", player_x, player_y, player_z);
    
    // Initial player shape (red cube in 3D view) - format: SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a
    printf("SHAPE;RECT;player;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n", 
           player_x * 20 + 300, player_y * 20 + 200, player_z, 20.0, 20.0, 20.0, 1.0, 0.0, 0.0, 1.0);
    
    // Initial mini map shape (yellow dot) - format: SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a
    printf("SHAPE;RECT;player_map;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n", 
           player_x + 60, player_y + 60, 0.0, 5.0, 5.0, 5.0, 1.0, 1.0, 0.0, 1.0);
    
    printf("VAR;status_display;string;Use arrow keys to move player\n");
    fflush(stdout);
    
    char input[256];
    while (fgets(input, sizeof(input), stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        // Process movement commands from arrow keys
        if (strcmp(input, "UP") == 0) {
            player_y += 1.0f;
        } else if (strcmp(input, "DOWN") == 0) {
            player_y -= 1.0f;
        } else if (strcmp(input, "LEFT") == 0) {
            player_x -= 1.0f;
        } else if (strcmp(input, "RIGHT") == 0) {
            player_x += 1.0f;
        } else if (strncmp(input, "EVENT:", 6) == 0) {
            // Handle dynamic events from the UI
            char* event_name = input + 6;
            if (strcmp(event_name, "player_move_up") == 0) {
                player_y += 1.0f;
            } else if (strcmp(event_name, "player_move_down") == 0) {
                player_y -= 1.0f;
            } else if (strcmp(event_name, "player_reset") == 0) {
                player_x = 0.0f;
                player_y = 0.0f;
                player_z = 0.0f;
            }
        }
        
        // Output updated position information
        printf("VAR;position_display;string;Player Position: X: %.1f, Y: %.1f, Z: %.1f\n", player_x, player_y, player_z);
        
        // Update 3D player shape - format: SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a
        printf("SHAPE;RECT;player;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n", 
               player_x * 20 + 300, player_y * 20 + 200, player_z, 20.0, 20.0, 20.0, 1.0, 0.0, 0.0, 1.0);
        
        // Update 2D mini map shape - format: SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a
        printf("SHAPE;RECT;player_map;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n", 
               player_x + 60, player_y + 60, 0.0, 5.0, 5.0, 5.0, 1.0, 1.0, 0.0, 1.0);
        
        printf("VAR;status_display;string;Player moved: X=%.1f, Y=%.1f\n", player_x, player_y);
        fflush(stdout);
    }
    
    return 0;
}