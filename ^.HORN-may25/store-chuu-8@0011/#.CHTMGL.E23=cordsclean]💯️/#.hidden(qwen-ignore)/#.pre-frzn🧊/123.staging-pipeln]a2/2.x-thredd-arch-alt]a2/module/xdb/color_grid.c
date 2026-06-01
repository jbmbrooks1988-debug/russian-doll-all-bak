#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Define the 27 colors (in RGB format)
float colors[27][3] = {
    {1.0, 0.0, 0.0},   // Red
    {0.0, 1.0, 0.0},   // Green
    {0.0, 0.0, 1.0},   // Blue
    {1.0, 1.0, 0.0},   // Yellow
    {1.0, 0.0, 1.0},   // Magenta
    {0.0, 1.0, 1.0},   // Cyan
    {1.0, 0.5, 0.0},   // Orange
    {0.5, 0.0, 1.0},   // Purple
    {1.0, 1.0, 1.0},   // White
    {0.5, 0.5, 0.5},   // Gray
    {0.0, 0.0, 0.0},   // Black
    {1.0, 0.75, 0.0},  // Gold
    {0.0, 0.5, 0.0},   // Dark Green
    {0.5, 0.0, 0.0},   // Dark Red
    {0.0, 0.0, 0.5},   // Dark Blue
    {0.5, 0.5, 0.0},   // Olive
    {0.0, 0.5, 0.5},   // Teal
    {0.5, 0.0, 0.5},   // Maroon
    {1.0, 0.25, 0.0},  // Vermillion
    {0.25, 1.0, 0.0},  // Chartreuse
    {0.0, 0.25, 1.0},  // Sky Blue
    {0.25, 0.0, 1.0},  // Violet
    {1.0, 0.0, 0.25},  // Rose
    {0.0, 1.0, 0.25},  // Spring Green
    {0.25, 1.0, 1.0},  // Turquoise
    {1.0, 0.25, 1.0},  // Pink
    {1.0, 0.5, 0.5}    // Light Coral
};

// Structure to hold canvas click information
typedef struct {
    int x, y;
    int active;
} CanvasClick;

CanvasClick last_click = {0, 0, 0};

int main(int argc, char *argv[]) {
    char input_line[256];
    int slider_value = 0;  // Initial slider value (0-18 for sets of 9)
    
    while (fgets(input_line, sizeof(input_line), stdin)) {
        // Remove newline character
        input_line[strcspn(input_line, "\n")] = 0;
        
        // Check if input is a slider command
        if (strncmp(input_line, "SLIDER:", 7) == 0) {
            slider_value = atoi(input_line + 7);
            // Ensure slider value is within bounds for 27 colors (showing 9 at a time)
            if (slider_value > 18) slider_value = 18;  // Max 18 to show up to 26 (9 per set)
            if (slider_value < 0) slider_value = 0;
        }
        // Check if input is a canvas click
        else if (strncmp(input_line, "CANVAS:", 7) == 0) {
            int x, y;
            if (sscanf(input_line, "CANVAS:%d,%d", &x, &y) == 2) {
                last_click.x = x;
                last_click.y = y;
                last_click.active = 1;
            }
        }
        
        // Clear previous shapes
        printf("CLEAR_SHAPES\n");
        
        // Draw the 9 visible color squares based on slider position
        int start_index = slider_value;  // Each slider value shows a different set
        int square_size = 40;
        int grid_size = 120;  // 3x40
        
        for (int i = 0; i < 9; i++) {
            int color_index = start_index + i;
            if (color_index >= 27) break;  // Don't exceed available colors
            
            int grid_x = i % 3;  // 0, 1, 2
            int grid_y = i / 3;  // 0, 0, 0, 1, 1, 1, 2, 2, 2
            
            int x_pos = grid_x * square_size;
            int y_pos = grid_y * square_size;
            
            // Print shape in the enhanced format: SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a
            printf("SHAPE;RECT;square_%d;%d;%d;0;%d;%d;5;%.2f;%.2f;%.2f;1.0\n", 
                   color_index, x_pos + square_size/2, y_pos + square_size/2, 
                   square_size, square_size, 
                   colors[color_index][0], colors[color_index][1], colors[color_index][2]);
        }
        
        // If there was a recent click, find which square was clicked
        if (last_click.active) {
            // Calculate which square was clicked based on canvas coordinates
            int clicked_square_x = last_click.x / square_size;
            int clicked_square_y = last_click.y / square_size;
            
            if (clicked_square_x >= 0 && clicked_square_x < 3 && 
                clicked_square_y >= 0 && clicked_square_y < 3) {
                // Calculate the index of the clicked color
                int clicked_color_index = start_index + clicked_square_y * 3 + clicked_square_x;
                
                if (clicked_color_index < 27) {
                    // Print a highlighed square where the click occurred
                    int x_pos = clicked_square_x * square_size + square_size/2;
                    int y_pos = clicked_square_y * square_size + square_size/2;
                    
                    printf("SHAPE;RECT;highlight_%d;%d;%d;0;%d;%d;6;1.0;1.0;0.0;0.8\n", 
                           clicked_color_index, x_pos, y_pos, 
                           square_size-4, square_size-4);
                           
                    // Send back the color information as a variable
                    printf("VAR;clicked_color_index;int;%d\n", clicked_color_index);
                    printf("VAR;clicked_color_name;string;Color_%d\n", clicked_color_index);
                    
                    // Calculate RGB values as a string
                    char rgb_str[100];
                    snprintf(rgb_str, sizeof(rgb_str), "RGB(%.2f,%.2f,%.2f)", 
                             colors[clicked_color_index][0], 
                             colors[clicked_color_index][1], 
                             colors[clicked_color_index][2]);
                    printf("VAR;clicked_color_rgb;string;%s\n", rgb_str);
                }
            }
            last_click.active = 0;  // Reset click after processing
        }
        
        fflush(stdout);
    }
    
    return 0;
}