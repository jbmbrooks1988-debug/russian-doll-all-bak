/*
 * controller.c - Controller Component for CSV UI Framework
 * 
 * Handles input processing, event management, and state updates
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>

// Include our model and view elements
extern int mouse_x, mouse_y;
extern int left_mouse_down;

// Include relevant model structures
typedef enum {
    ELEMENT_BUTTON,
    ELEMENT_SLIDER,
    ELEMENT_CANVAS,
    ELEMENT_LABEL,
    ELEMENT_CONTAINER,
    ELEMENT_IMAGE
} element_type_t;

typedef struct {
    element_type_t type;
    char id[64];
    char parent_id[64];
    int x, y;
    int width, height;
    float rel_x, rel_y;
    char text[256];
    char onclick_callback[128];
    char properties[512];
    int visible;
    int enabled;
    int hovered;
    int clicked;
    void* data;
} ui_element_t;

extern ui_element_t ui_elements[];
extern int element_count;

// FSM states for UI interaction
typedef enum {
    STATE_NORMAL,
    STATE_HOVER,
    STATE_ACTIVE,
    STATE_DRAGGING
} ui_state_t;

// Global variables for 3D player position (definitions)
float player_x = 0.0f;
float player_z = 0.0f;
float player_angle = 0.0f;

// Global state for controller
ui_state_t controller_state = STATE_NORMAL;
int dragging_element_index = -1;

// Function to initialize the controller
void init_controller() {
    controller_state = STATE_NORMAL;
    dragging_element_index = -1;
    printf("Controller initialized\n");
}

// Handle keyboard input
void handle_input(unsigned char key, int x, int y) {
    // Process global keyboard shortcuts here
    switch(key) {
        case 'r': // Reload UI from CSV
        case 'R':
            printf("Reloading UI from CSV...\n");
            // In a real implementation, this would reload the UI
            break;
        case 'd': // Toggle debug mode
        case 'D':
            printf("Debug mode toggled\n");
            // In a real implementation, this would toggle debug rendering
            break;
        default:
            // Pass other keys to focused element if needed
            break;
    }
    
    printf("Key pressed: %c at (%d, %d)\n", key, x, y);
}

// Update controller logic
void update_controller() {
    // Update element states based on current mouse position
    for (int i = 0; i < element_count; i++) {
        ui_element_t* elem = &ui_elements[i];
        
        // Skip invisible or disabled elements
        if (!elem->visible || !elem->enabled) continue;
        
        // Check if mouse is over element
        int is_over = (mouse_x >= elem->x && mouse_x <= elem->x + elem->width &&
                       mouse_y >= elem->y && mouse_y <= elem->y + elem->height);
        
        elem->hovered = is_over;
        
        // Handle click detection
        if (is_over && left_mouse_down) {
            elem->clicked = 1;
        } else {
            elem->clicked = 0;
        }
    }
    
    // Handle dragging state
    if (controller_state == STATE_DRAGGING && !left_mouse_down) {
        controller_state = STATE_NORMAL;
        dragging_element_index = -1;
    }
    
    // Process callbacks for clicked elements
    for (int i = 0; i < element_count; i++) {
        if (ui_elements[i].clicked && strlen(ui_elements[i].onclick_callback) > 0) {
            printf("Executing callback: %s for element: %s\n", 
                   ui_elements[i].onclick_callback, ui_elements[i].id);
            // In a real implementation, this would call the actual callback function
        }
    }
}

// Function to handle mouse button events
void handle_mouse_button(int button, int state, int x, int y) {
    mouse_x = x;
    mouse_y = y;
    
    if (button == GLUT_LEFT_BUTTON) {
        left_mouse_down = (state == GLUT_DOWN) ? 1 : 0;
        
        // Check if we're clicking on a draggable element
        if (state == GLUT_DOWN) {
            for (int i = 0; i < element_count; i++) {
                ui_element_t* elem = &ui_elements[i];
                
                if (elem->visible && elem->enabled && 
                    x >= elem->x && x <= elem->x + elem->width &&
                    y >= elem->y && y <= elem->y + elem->height) {
                    
                    // If this is a slider, start dragging
                    if (elem->type == ELEMENT_SLIDER) {
                        controller_state = STATE_DRAGGING;
                        dragging_element_index = i;
                        break;
                    }
                }
            }
        }
    }
}

// Function to handle mouse motion
void handle_mouse_motion(int x, int y) {
    mouse_x = x;
    mouse_y = y;
    
    // Handle element dragging if applicable
    if (controller_state == STATE_DRAGGING && dragging_element_index >= 0) {
        ui_element_t* elem = &ui_elements[dragging_element_index];
        
        // For sliders, update the value based on mouse position
        if (elem->type == ELEMENT_SLIDER) {
            float relative_pos = (float)(x - elem->x) / (float)elem->width;
            
            // Clamp to 0-1 range
            if (relative_pos < 0.0f) relative_pos = 0.0f;
            if (relative_pos > 1.0f) relative_pos = 1.0f;
            
            // Parse min/max from properties to calculate actual value
            float min_val = 0.0f, max_val = 100.0f; // defaults
            char* props = strdup(elem->properties);
            char* token = strtok(props, ";");
            
            while (token) {
                if (strncmp(token, "min=", 4) == 0) {
                    min_val = atof(token + 4);
                } else if (strncmp(token, "max=", 4) == 0) {
                    max_val = atof(token + 4);
                }
                token = strtok(NULL, ";");
            }
            
            free(props);
            
            // Update the slider value
            float new_value = min_val + (relative_pos * (max_val - min_val));
            
            // Update properties string to reflect new value
            char new_props[512];
            sprintf(new_props, "min=%.2f;max=%.2f;value=%.2f", min_val, max_val, new_value);
            strncpy(elem->properties, new_props, 511);
            elem->properties[511] = '\0';
        }
    }
}

// Function to execute callbacks
void execute_callbacks() {
    for (int i = 0; i < element_count; i++) {
        if (ui_elements[i].clicked && strlen(ui_elements[i].onclick_callback) > 0) {
            printf("Executing callback: %s for element: %s\n", 
                   ui_elements[i].onclick_callback, ui_elements[i].id);
            // In a real implementation, this would execute the callback
        }
    }
}

// Function to handle special keys (arrow keys, function keys, etc.)
void handle_special_keys(int key, int x, int y) {
    switch(key) {
        case GLUT_KEY_UP:
            player_z -= 0.2f; // Move forward
            printf("Player moved forward. New position: (%.2f, %.2f)\n", player_x, player_z);
            break;
        case GLUT_KEY_DOWN:
            player_z += 0.2f; // Move backward
            printf("Player moved backward. New position: (%.2f, %.2f)\n", player_x, player_z);
            break;
        case GLUT_KEY_LEFT:
            player_x -= 0.2f; // Move left
            printf("Player moved left. New position: (%.2f, %.2f)\n", player_x, player_z);
            break;
        case GLUT_KEY_RIGHT:
            player_x += 0.2f; // Move right
            printf("Player moved right. New position: (%.2f, %.2f)\n", player_x, player_z);
            break;
        default:
            printf("Special key %d pressed at (%d, %d)\n", key, x, y);
            break;
    }
}

// Function to handle keyboard up events
void handle_key_release(unsigned char key, int x, int y) {
    // Handle key release events if needed
    printf("Key released: %c at (%d, %d)\n", key, x, y);
}

// Function to update element positions based on relative coordinates
extern void update_all_positions(int window_width, int window_height);
void update_element_positions(int window_width, int window_height) {
    update_all_positions(window_width, window_height);
}

// Function to reset click states after processing
extern void reset_click_states();
void reset_element_click_states() {
    reset_click_states();
}

// Function to cleanup controller resources
void cleanup_controller() {
    controller_state = STATE_NORMAL;
    dragging_element_index = -1;
    printf("Controller cleaned up\n");
}

// Global cleanup function
void cleanup() {
    printf("Cleaning up resources...\n");
    cleanup_controller();
}