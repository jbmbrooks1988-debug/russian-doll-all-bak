#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>  // for usleep

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

// Define human-readable names for the colors
const char* color_names[27] = {
    "Red", "Green", "Blue", "Yellow", "Magenta", "Cyan", 
    "Orange", "Purple", "White", "Gray", "Black", "Gold",
    "Dark Green", "Dark Red", "Dark Blue", "Olive", "Teal", 
    "Maroon", "Vermillion", "Chartreuse", "Sky Blue", "Violet",
    "Rose", "Spring Green", "Turquoise", "Pink", "Light Coral"
};

// Global variables to track state
int slider_value = 0;  // Initial slider value (0-2 for 3 sets of 9 colors)
int last_clicked_color_index = -1;
int canvas_click_x = 0;
int canvas_click_y = 0;
int canvas_clicked = 0;

// Function to send current state to the main program
void send_current_state() {
    // Clear previous shapes
    printf("CLEAR_SHAPES\n");
    
    // Draw the 9 visible color squares based on slider position
    int start_index = slider_value * 9;  // Each slider value shows a different panel of 9 colors (0-8, 9-17, 18-26)
    int square_size = 40;
    
    for (int i = 0; i < 9; i++) {
        int color_index = start_index + i;
        if (color_index >= 27) break;  // Don't exceed available colors
        
        int grid_x = i % 3;  // 0, 1, 2
        int grid_y = i / 3;  // 0, 0, 0, 1, 1, 1, 2, 2, 2
        
        int x_pos = grid_x * square_size + square_size/2;
        int y_pos = grid_y * square_size + square_size/2;
        
        // Print shape in the enhanced format: SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a
        printf("SHAPE;RECT;square_%d;%d;%d;0;%d;%d;5;%.2f;%.2f;%.2f;1.0\n", 
               color_index, x_pos, y_pos, 
               square_size, square_size, 
               colors[color_index][0], colors[color_index][1], colors[color_index][2]);
    }
    
    // If a canvas click was registered, highlight the clicked square
    if (canvas_clicked) {
        // Calculate which square was clicked based on canvas coordinates
        // The canvas is 120x120, and we have 3x3 grid of 40x40 squares
        int clicked_square_x = canvas_click_x / square_size;  // 0, 1, or 2
        int clicked_square_y = canvas_click_y / square_size;  // 0, 1, or 2
        
        if (clicked_square_x >= 0 && clicked_square_x < 3 && 
            clicked_square_y >= 0 && clicked_square_y < 3) {
            // Calculate the index of the clicked color based on current panel
            int clicked_color_index = start_index + clicked_square_y * 3 + clicked_square_x;
            
            if (clicked_color_index < 27) {
                // Store the clicked color index for variable updates
                last_clicked_color_index = clicked_color_index;
                
                // Print a highlighted square where the click occurred
                int x_pos = clicked_square_x * square_size + square_size/2;
                int y_pos = clicked_square_y * square_size + square_size/2;
                
                printf("SHAPE;RECT;highlight_%d;%d;%d;0;%d;%d;6;1.0;1.0;0.0;0.8\n", 
                       clicked_color_index, x_pos, y_pos, 
                       square_size-4, square_size-4);
            }
        }
        canvas_clicked = 0;  // Reset click after processing
    }
    
    // Send back the color information as variables
    if (last_clicked_color_index >= 0) {
        printf("VAR;clicked_color_index;int;%d\n", last_clicked_color_index);
        printf("VAR;clicked_color_name;string;%s\n", color_names[last_clicked_color_index]);
        
        // Calculate RGB values as a string
        char rgb_str[100];
        snprintf(rgb_str, sizeof(rgb_str), "RGB(%.2f,%.2f,%.2f)", 
                 colors[last_clicked_color_index][0], 
                 colors[last_clicked_color_index][1], 
                 colors[last_clicked_color_index][2]);
        printf("VAR;clicked_color_rgb;string;%s\n", rgb_str);
    } else {
        // Send default values when no color has been clicked yet
        printf("VAR;clicked_color_index;int;-1\n");
        printf("VAR;clicked_color_name;string;None\n");
        printf("VAR;clicked_color_rgb;string;None\n");
    }
    
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    char input_line[256];
    
    // Initialize with default values
    send_current_state();
    
    while (1) {  // Continuous loop like in the working example
        // Check if there's input from stdin (non-blocking approach)
        if (fgets(input_line, sizeof(input_line), stdin) != NULL) {
            // Remove newline character
            input_line[strcspn(input_line, "\n")] = 0;
            
            // Check if input is a slider command - handle both formats
            if (strncmp(input_line, "SLIDER:", 7) == 0) {
                int new_slider_value;
                // Try parsing SLIDER:id:value format
                if (sscanf(input_line, "SLIDER:%*[^:]:%d", &new_slider_value) == 1) {
                    // Ensure slider value is within bounds for 27 colors, 9 at a time
                    // We have 27 colors total with 9 visible at once, so 3 panels (0,1,2)
                    if (new_slider_value > 2) new_slider_value = 2;  // Only 3 panels: 0-2
                    if (new_slider_value < 0) new_slider_value = 0;
                    slider_value = new_slider_value;
                }
                // Try parsing SLIDER:value format
                else if (sscanf(input_line, "SLIDER:%d", &new_slider_value) == 1) {
                    if (new_slider_value > 2) new_slider_value = 2;
                    if (new_slider_value < 0) new_slider_value = 0;
                    slider_value = new_slider_value;
                }
            }
            // Check if input is a canvas click
            else if (strncmp(input_line, "CANVAS:", 7) == 0) {
                int x, y;
                if (sscanf(input_line, "CANVAS:%d,%d", &x, &y) == 2) {
                    canvas_click_x = x;
                    canvas_click_y = y;
                    canvas_clicked = 1;
                }
            }
        }
        
        // Send the current state (this needs to happen regularly)
        send_current_state();
        
        // Small delay to avoid busy waiting
        usleep(16000); // ~60 FPS
    }
    
    return 0;
}