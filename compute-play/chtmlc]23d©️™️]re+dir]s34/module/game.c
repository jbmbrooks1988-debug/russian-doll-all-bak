#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Game module that controls a blue square in a canvas
// Receives input from input.csv and produces output to output.csv

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_csv_file> <output_csv_file>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];

    FILE* in_fp = fopen(input_file, "r");
    if (!in_fp) {
        perror("Error opening input CSV file");
        return 1;
    }

    // Read game state from input.csv: player_x,player_y,input_command,canvas_width,canvas_height
    int player_x, player_y, canvas_width, canvas_height;
    char input_command[20];
    
    if (fscanf(in_fp, "%d,%d,%19[^,],%d,%d", &player_x, &player_y, input_command, &canvas_width, &canvas_height) != 5) {
        fprintf(stderr, "Error reading game state from input CSV file\n");
        fclose(in_fp);
        return 1;
    }
    fclose(in_fp);

    // Debug: Print input command and starting position
    printf("Game module: Input command='%s', Current position=(%d, %d), Canvas=(%d, %d)\n", 
           input_command, player_x, player_y, canvas_width, canvas_height);

    // Process input command to update player position
    if (strcmp(input_command, "UP") == 0) {
        player_y += 10;  // Move up by 10 pixels
    } else if (strcmp(input_command, "DOWN") == 0) {
        player_y -= 10;  // Move down by 10 pixels
    } else if (strcmp(input_command, "LEFT") == 0) {
        player_x -= 10;  // Move left by 10 pixels
    } else if (strcmp(input_command, "RIGHT") == 0) {
        player_x += 10;  // Move right by 10 pixels
    }
    // Add boundary checking
    if (player_x < 0) player_x = 0;
    if (player_y < 0) player_y = 0;
    if (player_x >= canvas_width) player_x = canvas_width - 1;
    if (player_y >= canvas_height) player_y = canvas_height - 1;

    // Debug: Print new position
    printf("Game module: New position=(%d, %d)\n", player_x, player_y);

    // Write new position to output.csv
    FILE* out_fp = fopen(output_file, "w");
    if (!out_fp) {
        perror("Error opening output CSV file");
        return 1;
    }

    fprintf(out_fp, "%d,%d\n", player_x, player_y);
    fclose(out_fp);

    return 0;
}