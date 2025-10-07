#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <GL/glut.h> // Required for glutPostRedisplay and glutGet

// We need the UIElement definition and the elements array from view.c
// In a real C application, you'd have a .h file for this.
typedef struct {
    char type[20];
    int x, y, width, height;
    char id[50]; // Unique identifier for UI elements
    char label[50]; // Used for button label, text element value, and checkbox label
    char text_content[256]; // For textfield content
    int cursor_pos; // For textfield cursor position
    int is_active; // For textfield active state
    int is_checked; // For checkbox checked state
    int slider_value; // For slider current value
    int slider_min; // For slider minimum value
    int slider_max; // For slider maximum value
    int slider_step; // For slider step increment
    float color[3];
    int parent;
    // Canvas-specific properties
    int canvas_initialized; // Flag to track if canvas has been initialized
    void (*canvas_render_func)(int x, int y, int width, int height); // Render function for canvas
    // Event handling
    char onClick[50]; // Store the onClick handler function name
    // Menu-specific properties
    int menu_items_count; // Number of submenu items
    int is_open; // For menus - whether they are currently open
} UIElement;

extern UIElement elements[];
extern int num_elements;

void run_module_handler(); // Forward declaration

int active_slider_index = -1; // Global variable to track the currently dragged slider

// Function to handle button clicks based on onClick attribute
void handle_element_event(const char* event_handler) {
    if (strcmp(event_handler, "button_click_handler") == 0) {
        printf("Button click handler executed!\n");
        // Add specific button click logic here
    } else if (strcmp(event_handler, "run_module_handler") == 0) {
        run_module_handler();
    } else if (strcmp(event_handler, "canvas_click_handler") == 0) {
        printf("Canvas click handler executed!\n");
        // Add specific canvas click logic here
    } else if (strcmp(event_handler, "menu_click_handler") == 0) {
        printf("Menu click handler executed!\n");
        // Add specific menu click logic here
    } else {
        printf("Unknown event handler: %s\n", event_handler);
    }
}

void init_controller() {
    // Nothing to initialize for now
}

void mouse(int button, int state, int x, int y) {
    int ry = 600 - y; // 600 is the window height

    if (button == 0) { // Left mouse button
        if (state == 0) { // Mouse button down
            int clicked_textfield_index = -1;

            for (int i = 0; i < num_elements; i++) {
                int parent_x = 0;
                int parent_y = 0;
                int current_parent = elements[i].parent;
                while (current_parent != -1) {
                    parent_x += elements[current_parent].x;
                    parent_y += elements[current_parent].y;
                    current_parent = elements[current_parent].parent;
                }

                int abs_x = parent_x + elements[i].x;
                int abs_y = parent_y + elements[i].y;

                if (x >= abs_x && x <= abs_x + elements[i].width &&
                    ry >= abs_y && ry <= abs_y + elements[i].height) {
                    if (strcmp(elements[i].type, "button") == 0) {
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                    } else if (strcmp(elements[i].type, "textfield") == 0) {
                        clicked_textfield_index = i;
                    } else if (strcmp(elements[i].type, "checkbox") == 0) {
                        elements[i].is_checked = !elements[i].is_checked;
                        printf("Checkbox '%s' toggled to %s!\n", elements[i].label, elements[i].is_checked ? "true" : "false");
                    } else if (strcmp(elements[i].type, "slider") == 0) {
                        active_slider_index = i;
                        // Calculate initial slider value based on click position
                        float normalized_x = (float)(x - abs_x) / elements[i].width;
                        elements[i].slider_value = elements[i].slider_min + normalized_x * (elements[i].slider_max - elements[i].slider_min);
                        // Snap to step
                        elements[i].slider_value = (elements[i].slider_value / elements[i].slider_step) * elements[i].slider_step;
                        if (elements[i].slider_value < elements[i].slider_min) elements[i].slider_value = elements[i].slider_min;
                        if (elements[i].slider_value > elements[i].slider_max) elements[i].slider_value = elements[i].slider_max;
                        printf("Slider '%s' value: %d\n", elements[i].label, elements[i].slider_value);
                    } else if (strcmp(elements[i].type, "canvas") == 0) {
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        } else {
                            printf("Canvas '%s' clicked at (%d, %d)\n", elements[i].label, x, ry);
                        }
                    } else if (strcmp(elements[i].type, "menu") == 0) {
                        // Toggle menu open/close state
                        elements[i].is_open = !elements[i].is_open;
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                        printf("Menu '%s' %s\n", elements[i].label, elements[i].is_open ? "opened" : "closed");
                    } else if (strcmp(elements[i].type, "menuitem") == 0) {
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                        printf("Menu item '%s' clicked\n", elements[i].label);
                    }
                }
            }

            // Deactivate all textfields first
            for (int i = 0; i < num_elements; i++) {
                if (strcmp(elements[i].type, "textfield") == 0) {
                    elements[i].is_active = 0;
                }
            }

            // Activate the clicked textfield
            if (clicked_textfield_index != -1) {
                elements[clicked_textfield_index].is_active = 1;
            }
        } else { // Mouse button up
            active_slider_index = -1; // Stop dragging slider
        }
        glutPostRedisplay(); // Redraw to show active textfield/cursor or checkbox state or slider value
    }
}

void run_module_handler() {
    printf("Running module...\n");

    // 1. Read previous result to increment from, or use default value
    int start_value = 10;  // Default starting value
    FILE* prev_out_fp = fopen("output.csv", "r");
    if (prev_out_fp) {
        int prev_value = 0;
        if (fscanf(prev_out_fp, "%d", &prev_value) == 1) {
            start_value = prev_value;  // Start from previous result if exists
        }
        fclose(prev_out_fp);
    }

    // 2. Write the starting value to input.csv
    FILE* in_fp = fopen("input.csv", "w");
    if (!in_fp) {
        perror("Error creating input.csv");
        return;
    }
    fprintf(in_fp, "%d\n", start_value); // Use previous result or default
    fclose(in_fp);

    // 3. Execute the module
    // Using system() for simplicity. For more robust IPC, use fork()/exec() and pipes.
    int result = system("./module/module_example input.csv output.csv");
    if (result == -1) {
        perror("Error executing module");
        return;
    } else if (result != 0) {
        fprintf(stderr, "Module execution failed with exit code: %d\n", result);
        return;
    }

    // 4. Read result from output.csv
    FILE* out_fp = fopen("output.csv", "r");
    if (!out_fp) {
        perror("Error opening output.csv");
        return;
    }
    int processed_value = 0;
    if (fscanf(out_fp, "%d", &processed_value) != 1) {
        fprintf(stderr, "Error reading processed value from output.csv\n");
        fclose(out_fp);
        return;
    }
    fclose(out_fp);

    printf("Module processed value: %d\n", processed_value);
    
    // Update UI to show the result - find the result display textfield and update it by ID
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "textfield") == 0 && 
            strcmp(elements[i].id, "result_display") == 0) {
            snprintf(elements[i].text_content, sizeof(elements[i].text_content), "%d", processed_value);
            elements[i].cursor_pos = strlen(elements[i].text_content);
            break; // Found and updated the result display
        }
    }
    
    // Also update the text element that displays the result by ID
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "text") == 0 && 
            strcmp(elements[i].id, "result_text") == 0) {
            snprintf(elements[i].label, sizeof(elements[i].label), "Result: %d", processed_value);
            break;
        }
    }
    
    glutPostRedisplay(); // Redraw to show updated UI
}

void mouse_motion(int x, int y) {
    if (active_slider_index != -1) {
        UIElement* slider = &elements[active_slider_index];

        int parent_x = 0;
        int parent_y = 0;
        int current_parent = slider->parent;
        while (current_parent != -1) {
            parent_x += elements[current_parent].x;
            parent_y += elements[current_parent].y;
            current_parent = elements[current_parent].parent;
        }

        int abs_x = parent_x + slider->x;
        // int abs_y = parent_y + slider->y; // Not needed for horizontal slider

        // Calculate new slider value based on mouse x position
        float normalized_x = (float)(x - abs_x) / slider->width;
        if (normalized_x < 0.0f) normalized_x = 0.0f;
        if (normalized_x > 1.0f) normalized_x = 1.0f;

        int new_value = slider->slider_min + normalized_x * (slider->slider_max - slider->slider_min);
        // Snap to step
        new_value = (new_value / slider->slider_step) * slider->slider_step;

        if (new_value < slider->slider_min) new_value = slider->slider_min;
        if (new_value > slider->slider_max) new_value = slider->slider_max;

        if (new_value != slider->slider_value) {
            slider->slider_value = new_value;
            printf("Slider '%s' value: %d\n", slider->label, slider->slider_value);
            glutPostRedisplay();
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    // Check for active textfield
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "textfield") == 0 && elements[i].is_active) {
            if (key == 8) { // Backspace
                if (elements[i].cursor_pos > 0) {
                    elements[i].cursor_pos--;
                    elements[i].text_content[elements[i].cursor_pos] = '\0';
                }
            } else if (key == 13) { // Enter
                printf("Textfield content: %s\n", elements[i].text_content);
            } else if (key >= 32 && key <= 126) { // Printable characters
                if (elements[i].cursor_pos < sizeof(elements[i].text_content) - 1) {
                    elements[i].text_content[elements[i].cursor_pos] = key;
                    elements[i].cursor_pos++;
                    elements[i].text_content[elements[i].cursor_pos] = '\0';
                }
            }
            glutPostRedisplay();
            return;
        }
    }

    // If no textfield is active or key is not handled by textfield, check for global keys
    if (key == 27) { // ESC
        exit(0);
    }
}