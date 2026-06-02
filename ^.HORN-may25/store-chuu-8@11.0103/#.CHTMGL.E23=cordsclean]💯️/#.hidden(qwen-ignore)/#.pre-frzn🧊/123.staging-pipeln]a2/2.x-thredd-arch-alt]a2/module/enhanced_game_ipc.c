#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

// This is an example game module demonstrating the new enhanced IPC system
int main() {
    float player_x = 50.0f;
    float player_y = 50.0f;
    float player_z = 0.0f;
    int score = 0;
    int health = 100;
    char status_text[256] = "Game in progress";
    
    // Additional shapes with their own properties
    float enemy_x = 150.0f;
    float enemy_y = 100.0f;
    float enemy_z = 0.0f;
    
    float enemy2_x = 200.0f;
    float enemy2_y = 200.0f;
    float enemy2_z = 0.0f;
    
    float powerup_x = 300.0f;
    float powerup_y = 150.0f;
    float powerup_z = 0.0f;
    
    // Moving background elements
    float bg_element_x = 100.0f;
    float bg_element_y = 50.0f;
    float bg_element_z = 0.0f;
    float bg_element_size = 10.0f;
    
    float positions[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};  // Example array of positions
    int num_positions = 5;
    
    char command[256];

    // The game runs in an infinite loop.
    // It waits for input on stdin, updates state, and prints the new state to stdout.
    while (1) {
        // Read a command from stdin (which is piped from the main app).
        if (fgets(command, sizeof(command), stdin) != NULL) {
            // Remove newline character from the command
            command[strcspn(command, "\n")] = 0;

            // Update game state based on the command.
            if (strcmp(command, "UP") == 0) {
                player_y += 5.0f;
                score += 10;
            } else if (strcmp(command, "DOWN") == 0) {
                player_y -= 5.0f;
                score += 5;
            } else if (strcmp(command, "LEFT") == 0) {
                player_x -= 5.0f;
                score += 5;
            } else if (strcmp(command, "RIGHT") == 0) {
                player_x += 5.0f;
                score += 10;
            } else if (strcmp(command, "ATTACK") == 0) {
                health -= 10;
                if (health < 0) health = 0;
                strcpy(status_text, "Attacking!");
            } else if (strcmp(command, "HEAL") == 0) {
                health += 20;
                if (health > 100) health = 100;
                strcpy(status_text, "Healing...");
            }

            // Update animation/movement for non-controlled elements
            enemy_x += 1.0f;
            if (enemy_x > 700.0f) enemy_x = 0.0f;
            
            enemy2_y -= 0.8f;
            if (enemy2_y < 0.0f) enemy2_y = 600.0f;
            
            powerup_x -= 0.5f;
            if (powerup_x < 0.0f) powerup_x = 800.0f;
            
            // Create a pulsing effect for the background element
            bg_element_size = 10.0f + 5.0f * sin((float)(score) / 10.0f);

            // Write multiple types of messages to stdout (which is piped to the main app).
            
            // 1. Send player shape (blue moving square)
            printf("SHAPE;SQUARE;player;%.2f;%.2f;%.2f;20.0;20.0;5.0;0.0;0.0;1.0;1.0\n", 
                   player_x, player_y, player_z);
            
            // 2. Send enemy shape (red square)
            printf("SHAPE;RECT;enemy;%.2f;%.2f;%.2f;15.0;15.0;5.0;1.0;0.0;0.0;1.0\n", 
                   enemy_x, enemy_y, enemy_z);
            
            // 3. Send second enemy shape (orange square)
            printf("SHAPE;SQUARE;enemy2;%.2f;%.2f;%.2f;12.0;12.0;5.0;1.0;0.5;0.0;1.0\n", 
                   enemy2_x, enemy2_y, enemy2_z);
            
            // 4. Send powerup shape (green circle)
            printf("SHAPE;CIRCLE;powerup;%.2f;%.2f;%.2f;8.0;8.0;5.0;0.0;1.0;0.0;1.0\n", 
                   powerup_x, powerup_y, powerup_z);
            
            // 5. Send background element (purple pulsing square)
            printf("SHAPE;SQUARE;bg_element;%.2f;%.2f;%.2f;%.2f;%.2f;5.0;0.5;0.0;0.5;0.7\n", 
                   bg_element_x, bg_element_y, bg_element_z, bg_element_size, bg_element_size);

            // 6. Send animated shapes (rotating)
            float time = (float)score / 10.0f;
            float rot_x = 400.0f + 100.0f * cos(time);
            float rot_y = 300.0f + 100.0f * sin(time);
            printf("SHAPE;TRIANGLE;rotating_shape;%.2f;%.2f;%.2f;15.0;15.0;5.0;1.0;1.0;0.0;1.0\n", 
                   rot_x, rot_y, 0.0f);

            // 7. Send formatted text for position display (matches the text element ID)
            char position_text[256];
            snprintf(position_text, sizeof(position_text), "Blue Square: X=%.2f, Y=%.2f, Z=%.2f", player_x, player_y, player_z);
            printf("VAR;position_text;string;%s\n", position_text);
            
            // 8. Send other variables
            printf("VAR;score;int;%d\n", score);
            printf("VAR;health;int;%d\n", health);
            printf("VAR;status;string;%s\n", status_text);
            printf("VAR;player_x;float;%.2f\n", player_x);
            printf("VAR;player_y;float;%.2f\n", player_y);
            printf("VAR;player_z;float;%.2f\n", player_z);
            
            // 9. Send array data (example positions of multiple game objects)
            printf("ARRAY;positions;float;%d;%.2f,%.2f,%.2f,%.2f,%.2f\n", 
                   num_positions, positions[0], positions[1], positions[2], positions[3], positions[4]);
            
            // 10. Send more arrays (example: health values of enemies)
            int enemy_healths[] = {80, 65, 90, 75};
            int num_enemies = 4;
            printf("ARRAY;enemy_healths;int;%d;%d,%d,%d,%d\n", 
                   num_enemies, enemy_healths[0], enemy_healths[1], enemy_healths[2], enemy_healths[3]);

            // Flush stdout to ensure the parent process receives the messages immediately.
            fflush(stdout);
            
            // Increment score for animation
            score++;
        } else {
            // If no command received, still send the current state to maintain the shapes
            // 1. Send player shape (blue moving square)
            printf("SHAPE;SQUARE;player;%.2f;%.2f;%.2f;20.0;20.0;5.0;0.0;0.0;1.0;1.0\n", 
                   player_x, player_y, player_z);
            
            // 2. Send enemy shape (red square)
            printf("SHAPE;RECT;enemy;%.2f;%.2f;%.2f;15.0;15.0;5.0;1.0;0.0;0.0;1.0\n", 
                   enemy_x, enemy_y, enemy_z);
            
            // 3. Send second enemy shape (orange square)
            printf("SHAPE;SQUARE;enemy2;%.2f;%.2f;%.2f;12.0;12.0;5.0;1.0;0.5;0.0;1.0\n", 
                   enemy2_x, enemy2_y, enemy2_z);
            
            // 4. Send powerup shape (green circle)
            printf("SHAPE;CIRCLE;powerup;%.2f;%.2f;%.2f;8.0;8.0;5.0;0.0;1.0;0.0;1.0\n", 
                   powerup_x, powerup_y, powerup_z);
            
            // 5. Send background element (purple pulsing square)
            printf("SHAPE;SQUARE;bg_element;%.2f;%.2f;%.2f;%.2f;%.2f;5.0;0.5;0.0;0.5;0.7\n", 
                   bg_element_x, bg_element_y, bg_element_z, bg_element_size, bg_element_size);

            // 6. Send animated shapes (rotating)
            float time = (float)score / 10.0f;
            float rot_x = 400.0f + 100.0f * cos(time);
            float rot_y = 300.0f + 100.0f * sin(time);
            printf("SHAPE;TRIANGLE;rotating_shape;%.2f;%.2f;%.2f;15.0;15.0;5.0;1.0;1.0;0.0;1.0\n", 
                   rot_x, rot_y, 0.0f);

            // 7. Send formatted text for position display (matches the text element ID)
            char position_text[256];
            snprintf(position_text, sizeof(position_text), "Blue Square: X=%.2f, Y=%.2f, Z=%.2f", player_x, player_y, player_z);
            printf("VAR;position_text;string;%s\n", position_text);
            
            // 8. Send other variables
            printf("VAR;score;int;%d\n", score);
            printf("VAR;health;int;%d\n", health);
            printf("VAR;status;string;%s\n", status_text);
            printf("VAR;player_x;float;%.2f\n", player_x);
            printf("VAR;player_y;float;%.2f\n", player_y);
            printf("VAR;player_z;float;%.2f\n", player_z);
            
            // 9. Send array data (example positions of multiple game objects)
            printf("ARRAY;positions;float;%d;%.2f,%.2f,%.2f,%.2f,%.2f\n", 
                   num_positions, positions[0], positions[1], positions[2], positions[3], positions[4]);
            
            // 10. Send more arrays (example: health values of enemies)
            int enemy_healths[] = {80, 65, 90, 75};
            int num_enemies = 4;
            printf("ARRAY;enemy_healths;int;%d;%d,%d,%d,%d\n", 
                   num_enemies, enemy_healths[0], enemy_healths[1], enemy_healths[2], enemy_healths[3]);

            // Flush stdout to ensure the parent process receives the messages immediately.
            fflush(stdout);
            
            // Increment score for animation
            score++;
            
            // Sleep briefly to avoid busy waiting
            usleep(16000); // ~60 FPS
        }
    }

    return 0;
}