#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>

int main() {
    float rotation_x = 0.0f;
    float rotation_y = 0.0f;
    float rotation_z = 0.0f;
    float z_position = -3.0f;  // For zoom effect (moving cube in/out)
    float zoom_step = 0.2f;
    
    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    while(1) {
        // Check for input from the controller
        char input[256];
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // Process input commands
            if (strncmp(input, "UP", 2) == 0) {
                // Zoom in - move cube closer (less negative z)
                z_position += zoom_step;
                if (z_position > -1.0f) z_position = -1.0f;  // Limit zoom in
            } else if (strncmp(input, "DOWN", 4) == 0) {
                // Zoom out - move cube farther (more negative z) 
                z_position -= zoom_step;
                if (z_position < -10.0f) z_position = -10.0f; // Limit zoom out
            } else if (strncmp(input, "LEFT", 4) == 0) {
                // Rotate left (around Y axis)
                rotation_y += 10.0f;
            } else if (strncmp(input, "RIGHT", 5) == 0) {
                // Rotate right (around Y axis)
                rotation_y -= 10.0f;
            }
        }
        
        // Increment rotation values for animation (slower rotation)
        rotation_x += 0.5f;
        rotation_y += 1.0f;
        rotation_z += 0.2f;
        
        // Keep rotation values within 0-360 range
        if (rotation_x >= 360.0f) rotation_x -= 360.0f;
        if (rotation_y >= 360.0f) rotation_y -= 360.0f;
        if (rotation_z >= 360.0f) rotation_z -= 360.0f;
        
        // Output the rotating cube shape with the new 18-field format
        // Format: SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a;rot_x;rot_y;rot_z;scale_x;scale_y;scale_z
        printf("SHAPE;RECT;rotating_cube;0;0;%.2f;100;100;100;1;0;0;1;%.2f;%.2f;%.2f;1.00;1.00;1.00\n", 
               z_position, rotation_x, rotation_y, rotation_z);
        
        fflush(stdout);
        usleep(50000); // Sleep for 50ms for ~20 FPS
    }
    
    return 0;
}