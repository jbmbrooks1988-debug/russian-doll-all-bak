#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <GL/glut.h> // Required for glutPostRedisplay and glutGet

// We need the UIElement definition and the elements array from view.c
// In a real C application, you'd have a .h file for this.
#define MAX_TEXT_LINES 1000
#define MAX_LINE_LENGTH 512

#define CLIPBOARD_SIZE 10000

// Forward declarations for functions defined later in this file
void handle_ctrl_keys(unsigned char key, int element_index);
void copy_text(int element_index);
void cut_text(int element_index);
void paste_text(int element_index);
void delete_selection(int element_index);
void model_send_input(const char* input);

typedef struct {
    char type[20];
    int x, y, width, height;
    char id[50]; // Unique identifier for UI elements
    char label[50]; // Used for button label, text element value, and checkbox label
    // Multi-line text support - replace simple text_content with array of lines
    char text_content[MAX_TEXT_LINES][MAX_LINE_LENGTH];
    int num_lines; // Number of lines in the text
    int cursor_x; // X position of cursor (column)
    int cursor_y; // Y position of cursor (line number)
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
    char view_mode[10]; // View mode: "2d" or "3d"
    // Camera properties for 3D view
    float camera_pos[3]; // Camera position (x, y, z)
    float camera_target[3]; // Camera target (x, y, z)
    float camera_up[3]; // Camera up vector (x, y, z)
    float fov; // Field of view for 3D perspective
    // Event handling
    char onClick[50]; // Store the onClick handler function name
    // Menu-specific properties
    int menu_items_count; // Number of submenu items
    int is_open; // For menus - whether they are currently open
    // Text selection properties
    int selection_start_x; // X position of selection start
    int selection_start_y; // Y position of selection start
    int selection_end_x;   // X position of selection end
    int selection_end_y;   // Y position of selection end
    int has_selection;     // Whether there is an active selection
    // Clipboard for this element
    char clipboard[CLIPBOARD_SIZE];
    // Directory listing properties
    char dir_path[512]; // Directory path for directory listing elements
    char dir_entries[100][256]; // Store up to 100 directory entries
    int dir_entry_count; // Number of directory entries
    int dir_entry_selected; // Index of selected entry
} UIElement;

extern UIElement elements[];
extern int num_elements;

void run_module_handler(); // Forward declaration

int active_slider_index = -1; // Global variable to track the currently dragged slider

// Function to handle button clicks based on onClick attribute
void switch_to_2d_view() {
    // Find canvas elements and switch them to 2D mode
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "canvas") == 0) {
            strcpy(elements[i].view_mode, "2d");
            printf("Switched canvas '%s' to 2D mode\n", elements[i].id);
        }
    }
    glutPostRedisplay(); // Redraw the scene
}

void switch_to_3d_view() {
    // Find canvas elements and switch them to 3D mode
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "canvas") == 0) {
            strcpy(elements[i].view_mode, "3d");
            printf("Switched canvas '%s' to 3D mode\n", elements[i].id);
        }
    }
    glutPostRedisplay(); // Redraw the scene
}

void handle_element_event(const char* event_handler) {
    if (strcmp(event_handler, "button_click_handler") == 0) {
        printf("Button click handler executed!\n");
        // Add specific button click logic here
    } else if (strcmp(event_handler, "canvas_click_handler") == 0) {
        printf("Canvas click handler executed!\n");
        // Add specific canvas click logic here
    } else if (strcmp(event_handler, "menu_click_handler") == 0) {
        printf("Menu click handler executed!\n");
        // Add specific menu click logic here
    } else if (strcmp(event_handler, "switch_to_2d_handler") == 0) {
        switch_to_2d_view();
    } else if (strcmp(event_handler, "switch_to_3d_handler") == 0) {
        switch_to_3d_view();
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
                
                // Special handling for menuitems when their parent menu is open
                if (strcmp(elements[i].type, "menuitem") == 0 && elements[i].parent != -1 && 
                    strcmp(elements[elements[i].parent].type, "menu") == 0 && 
                    elements[elements[i].parent].is_open) {
                    // Calculate submenu position (under the parent menu)
                    int parent_menu = elements[i].parent;
                    int parent_menu_abs_x = 0;
                    int parent_menu_abs_y = 0;
                    int parent_menu_parent = elements[parent_menu].parent;
                    while (parent_menu_parent != -1) {
                        parent_menu_abs_x += elements[parent_menu_parent].x;
                        parent_menu_abs_y += elements[parent_menu_parent].y;
                        parent_menu_parent = elements[parent_menu_parent].parent;
                    }
                    int submenu_x = parent_menu_abs_x + elements[parent_menu].x;
                    int submenu_y = parent_menu_abs_y + elements[parent_menu].y + elements[parent_menu].height;
                    
                    // Calculate position of this item in the submenu
                    int item_position = 0;
                    for (int j = 0; j < num_elements; j++) {
                        if (elements[j].parent == parent_menu && 
                            strcmp(elements[j].type, "menuitem") == 0) {
                            if (j == i) { // If this is our element
                                break;
                            }
                            item_position++;
                        }
                    }
                    
                    // Update absolute position to submenu coordinates
                    abs_x = submenu_x;
                    abs_y = submenu_y + (item_position * 20); // 20px height per menu item
                }

                if (x >= abs_x && x <= abs_x + elements[i].width &&
                    ry >= abs_y && ry <= abs_y + elements[i].height) {
                    if (strcmp(elements[i].type, "button") == 0) {
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                    } else if (strcmp(elements[i].type, "textfield") == 0 || strcmp(elements[i].type, "textarea") == 0) {
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
                    } else if (strcmp(elements[i].type, "dirlist") == 0) {
                        // Handle clicks on directory entries
                        // Calculate which entry was clicked based on mouse position
                        int line_height = 20;
                        int header_height = 25; // Account for the "dir element list:" header
                        int relative_y = ry - (parent_y + elements[i].y + header_height);
                        int entry_index = relative_y / line_height;

                        if (entry_index >= 0 && entry_index < elements[i].dir_entry_count) {
                            // Check if the clicked entry is a directory (ends with "/")
                            const char* clicked_entry = elements[i].dir_entries[entry_index];
                            int entry_len = strlen(clicked_entry);
                            if (entry_len > 0 && clicked_entry[entry_len-1] == '/') {
                                // This is a directory - remove the "/" to get the actual directory name
                                char dir_name[256];
                                strncpy(dir_name, clicked_entry, entry_len-1);
                                dir_name[entry_len-1] = '\0';

                                // Construct the new path
                                char new_path[1024];
                                if (strcmp(elements[i].dir_path, ".") == 0 || strlen(elements[i].dir_path) == 0) {
                                    strcpy(new_path, dir_name);
                                } else {
                                    snprintf(new_path, sizeof(new_path), "%s/%s", elements[i].dir_path, dir_name);
                                }

                                // Update the element's directory path and reload contents
                                strncpy(elements[i].dir_path, new_path, 511);
                                elements[i].dir_path[511] = '\0';
                                elements[i].dir_entry_count = 0; // Force reload on next render

                                printf("Navigating to directory: %s (full path: %s)\n", dir_name, new_path);

                                // Force UI refresh after navigation
                                glutPostRedisplay();

                                // Also handle directory selection event if there is an onClick handler
                                if (strlen(elements[i].onClick) > 0) {
                                    handle_element_event(elements[i].onClick);
                                }
                            } else {
                                // This is a file - could implement file selection here in the future
                                printf("File selected: %s\n", clicked_entry);
                            }
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
    else if (button == 2) { // Right mouse button
        if (state == 0) { // Mouse button down
            int menu_clicked = 0;
            // Check for right-click on a menu to open it as a context menu
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
                    if (strcmp(elements[i].type, "menu") == 0) {
                        // Open the menu (close any other open menus first)
                        // First close all other menus
                        for (int j = 0; j < num_elements; j++) {
                            if (strcmp(elements[j].type, "menu") == 0) {
                                elements[j].is_open = 0;
                            }
                        }
                        
                        // Then open this menu
                        elements[i].is_open = 1;
                        if (strlen(elements[i].onClick) > 0) {
                            handle_element_event(elements[i].onClick);
                        }
                        printf("Right-clicked menu '%s' opened as context menu\\n", elements[i].label);
                        glutPostRedisplay();
                        menu_clicked = 1;
                        break;
                    }
                }
            }
            
            // If no menu was right-clicked, open a generic context menu at the click position
            if (!menu_clicked) {
                // Close any currently open menus first
                for (int j = 0; j < num_elements; j++) {
                    if (strcmp(elements[j].type, "menu") == 0) {
                        elements[j].is_open = 0;
                    }
                }
                
                // Find a generic context menu in the UI (if it exists)
                int generic_context_menu_idx = -1;
                for (int i = 0; i < num_elements; i++) {
                    if (strcmp(elements[i].type, "menu") == 0 && 
                        strcmp(elements[i].label, "Generic Context Menu") == 0) {
                        generic_context_menu_idx = i;
                        break;
                    }
                }
                
                if (generic_context_menu_idx != -1) {
                    // Position the menu at the click location
                    elements[generic_context_menu_idx].x = x;
                    elements[generic_context_menu_idx].y = ry;  // Use ry since it's already converted
                    
                    // Make sure the menu is within window bounds
                    if (elements[generic_context_menu_idx].x + elements[generic_context_menu_idx].width > 800) {
                        elements[generic_context_menu_idx].x = 800 - elements[generic_context_menu_idx].width;
                    }
                    if (elements[generic_context_menu_idx].y + elements[generic_context_menu_idx].height + 
                        20 * elements[generic_context_menu_idx].menu_items_count > 600) {
                        elements[generic_context_menu_idx].y = 600 - elements[generic_context_menu_idx].height - 
                            20 * elements[generic_context_menu_idx].menu_items_count;
                    }
                    
                    if (elements[generic_context_menu_idx].x < 0) elements[generic_context_menu_idx].x = 0;
                    if (elements[generic_context_menu_idx].y < 0) elements[generic_context_menu_idx].y = 0;
                    
                    elements[generic_context_menu_idx].is_open = 1;
                    printf("Generic context menu opened at position (%d, %d)\\n", x, ry);
                    glutPostRedisplay();
                } else {
                    printf("Right-clicked at position (%d, %d) - no generic context menu defined\\n", x, ry);
                }
            }
        }
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
            snprintf(elements[i].text_content[0], MAX_LINE_LENGTH, "%d", processed_value);
            elements[i].cursor_x = strlen(elements[i].text_content[0]);
            elements[i].cursor_y = 0;
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
    // Check for active textfield or textarea
    for (int i = 0; i < num_elements; i++) {
        if ((strcmp(elements[i].type, "textfield") == 0 || strcmp(elements[i].type, "textarea") == 0) && elements[i].is_active) {
            // Handle control keys (Ctrl+C, Ctrl+V, etc.)
            if (key < 32) {  // Control keys are below 32
                handle_ctrl_keys(key, i);
                glutPostRedisplay();
                return;
            }
            
            if (key == 8) { // Backspace
                if (elements[i].cursor_x > 0) {
                    // Move characters to the left to remove the character
                    for (int j = elements[i].cursor_x; j < (int)strlen(elements[i].text_content[elements[i].cursor_y]); j++) {
                        elements[i].text_content[elements[i].cursor_y][j-1] = elements[i].text_content[elements[i].cursor_y][j];
                    }
                    elements[i].text_content[elements[i].cursor_y][strlen(elements[i].text_content[elements[i].cursor_y]) - 1] = '\0';
                    elements[i].cursor_x--;
                } else if (elements[i].cursor_y > 0) {
                    // Join current line with the previous line
                    int prev_line_len = strlen(elements[i].text_content[elements[i].cursor_y - 1]);
                    int curr_line_len = strlen(elements[i].text_content[elements[i].cursor_y]);
                    if (prev_line_len + curr_line_len < MAX_LINE_LENGTH) {
                        strcat(elements[i].text_content[elements[i].cursor_y - 1], elements[i].text_content[elements[i].cursor_y]);
                        
                        // Shift all lines up by one position starting from current line
                        for (int j = elements[i].cursor_y; j < elements[i].num_lines - 1; j++) {
                            strcpy(elements[i].text_content[j], elements[i].text_content[j+1]);
                        }
                        strcpy(elements[i].text_content[elements[i].num_lines - 1], ""); // Clear last line
                        elements[i].num_lines--;
                        elements[i].cursor_y--;
                        elements[i].cursor_x = prev_line_len;
                    }
                }
            } else if (key == 13) { // Enter - create new line
                if (elements[i].num_lines < MAX_TEXT_LINES - 1) {
                    // Shift lines down from the current position
                    for (int j = elements[i].num_lines; j > elements[i].cursor_y; j--) {
                        strcpy(elements[i].text_content[j], elements[i].text_content[j-1]);
                    }
                    elements[i].num_lines++;
                    
                    // Move the text after cursor to new line
                    char temp_line[MAX_LINE_LENGTH];
                    strcpy(temp_line, elements[i].text_content[elements[i].cursor_y] + elements[i].cursor_x);
                    elements[i].text_content[elements[i].cursor_y][elements[i].cursor_x] = '\0';
                    
                    strcpy(elements[i].text_content[elements[i].cursor_y + 1], temp_line);
                    
                    elements[i].cursor_y++;
                    elements[i].cursor_x = 0;
                }
            } else if (key >= 32 && key <= 126) { // Printable characters
                // Insert character at cursor position
                int line_len = strlen(elements[i].text_content[elements[i].cursor_y]);
                if (line_len < MAX_LINE_LENGTH - 1 && elements[i].cursor_x <= line_len) {
                    // Shift characters to the right to make space
                    for (int j = line_len; j > elements[i].cursor_x; j--) {
                        elements[i].text_content[elements[i].cursor_y][j] = elements[i].text_content[elements[i].cursor_y][j-1];
                    }
                    elements[i].text_content[elements[i].cursor_y][elements[i].cursor_x] = key;
                    elements[i].text_content[elements[i].cursor_y][line_len + 1] = '\0';
                    elements[i].cursor_x++;
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

void special_keys(int key, int x, int y) {
    // Check for active textfield or textarea
    for (int i = 0; i < num_elements; i++) {
        if ((strcmp(elements[i].type, "textfield") == 0 || strcmp(elements[i].type, "textarea") == 0) && elements[i].is_active) {
            switch(key) {
                case GLUT_KEY_UP:
                    if (elements[i].cursor_y > 0) {
                        elements[i].cursor_y--;
                        if (elements[i].cursor_x > (int)strlen(elements[i].text_content[elements[i].cursor_y])) {
                            elements[i].cursor_x = strlen(elements[i].text_content[elements[i].cursor_y]);
                        }
                    }
                    break;
                case GLUT_KEY_DOWN:
                    if (elements[i].cursor_y < elements[i].num_lines - 1) {
                        elements[i].cursor_y++;
                        if (elements[i].cursor_x > (int)strlen(elements[i].text_content[elements[i].cursor_y])) {
                            elements[i].cursor_x = strlen(elements[i].text_content[elements[i].cursor_y]);
                        }
                    }
                    break;
                case GLUT_KEY_LEFT:
                    if (elements[i].cursor_x > 0) {
                        elements[i].cursor_x--;
                    } else if (elements[i].cursor_y > 0) {
                        elements[i].cursor_y--;
                        elements[i].cursor_x = strlen(elements[i].text_content[elements[i].cursor_y]);
                    }
                    break;
                case GLUT_KEY_RIGHT:
                    if (elements[i].cursor_x < (int)strlen(elements[i].text_content[elements[i].cursor_y])) {
                        elements[i].cursor_x++;
                    } else if (elements[i].cursor_y < elements[i].num_lines - 1) {
                        elements[i].cursor_y++;
                        elements[i].cursor_x = 0;
                    }
                    break;
            }
            glutPostRedisplay();
            return;
        }
    }
    

    // Handle arrow keys for the game module
    switch(key) {
        case GLUT_KEY_UP:
            model_send_input("UP");
            break;
        case GLUT_KEY_DOWN:
            model_send_input("DOWN");
            break;
        case GLUT_KEY_LEFT:
            model_send_input("LEFT");
            break;
        case GLUT_KEY_RIGHT:
            model_send_input("RIGHT");
            break;
    }
}
// Function to copy selected text to clipboard
void copy_text(int element_index) {
    UIElement* el = &elements[element_index];
    if (!el->has_selection) {
        // If no selection, copy the entire line
        strcpy(el->clipboard, el->text_content[el->cursor_y]);
        return;
    }

    // Calculate start and end positions correctly
    int start_y = el->selection_start_y;
    int end_y = el->selection_end_y;
    int start_x = el->selection_start_x;
    int end_x = el->selection_end_x;
    
    // Ensure start is before end
    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int temp = start_y;
        start_y = end_y;
        end_y = temp;
        temp = start_x;
        start_x = end_x;
        end_x = temp;
    }
    
    if (start_y == end_y) {
        // Same line selection
        int length = end_x - start_x;
        if (length > 0 && length < CLIPBOARD_SIZE) {
            strncpy(el->clipboard, el->text_content[start_y] + start_x, length);
            el->clipboard[length] = '\0';
        }
    } else {
        // Multi-line selection
        int pos = 0;
        for (int line = start_y; line <= end_y; line++) {
            int start_col = (line == start_y) ? start_x : 0;
            int end_col = (line == end_y) ? end_x : strlen(el->text_content[line]);
            
            for (int col = start_col; col < end_col && pos < CLIPBOARD_SIZE - 1; col++) {
                el->clipboard[pos++] = el->text_content[line][col];
            }
            
            // Add newline between lines (except for the last line)
            if (line < end_y && pos < CLIPBOARD_SIZE - 1) {
                el->clipboard[pos++] = '\n';
            }
        }
        el->clipboard[pos] = '\0';
    }
}

// Function to cut selected text
void cut_text(int element_index) {
    copy_text(element_index);
    delete_selection(element_index);
}

// Function to delete selected text
void delete_selection(int element_index) {
    UIElement* el = &elements[element_index];
    if (!el->has_selection) {
        return;
    }

    // Calculate start and end positions correctly
    int start_y = el->selection_start_y;
    int end_y = el->selection_end_y;
    int start_x = el->selection_start_x;
    int end_x = el->selection_end_x;
    
    // Ensure start is before end
    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int temp = start_y;
        start_y = end_y;
        end_y = temp;
        temp = start_x;
        start_x = end_x;
        end_x = temp;
    }
    
    if (start_y == end_y) {
        // Same line deletion
        int line_len = strlen(el->text_content[start_y]);
        for (int i = end_x; i <= line_len; i++) {
            el->text_content[start_y][start_x + i - end_x] = el->text_content[start_y][i];
        }
        el->cursor_x = start_x;
    } else {
        // Multi-line deletion
        int remaining_len = strlen(el->text_content[end_y]) - end_x;
        // Append end line content after end_x to the start line after start_x
        for (int i = 0; i <= remaining_len; i++) {
            el->text_content[start_y][start_x + i] = el->text_content[end_y][end_x + i];
        }
        
        // Shift lines up to remove the deleted lines
        for (int line = start_y + 1; line <= end_y; line++) {
            strcpy(el->text_content[line - (end_y - start_y)], el->text_content[line]);
        }
        
        // Update line count
        el->num_lines -= (end_y - start_y);
        
        // Update cursor position
        el->cursor_y = start_y;
        el->cursor_x = start_x;
    }
    
    // Clear selection
    el->has_selection = 0;
}

// Function to paste text from clipboard
void paste_text(int element_index) {
    UIElement* el = &elements[element_index];
    
    // If there's a selection, delete it first
    if (el->has_selection) {
        delete_selection(element_index);
    }
    
    int clipboard_len = strlen(el->clipboard);
    if (clipboard_len == 0) {
        return; // Nothing to paste
    }
    
    // Check if clipboard has newlines
    char* newline_ptr = strchr(el->clipboard, '\n');
    if (newline_ptr == NULL) {
        // Simple paste without newlines
        int line_len = strlen(el->text_content[el->cursor_y]);
        int paste_len = strlen(el->clipboard);
        
        // Make sure we don't exceed line length
        if (line_len + paste_len < MAX_LINE_LENGTH) {
            // Shift existing text to the right
            for (int i = line_len; i >= el->cursor_x; i--) {
                el->text_content[el->cursor_y][i + paste_len] = el->text_content[el->cursor_y][i];
            }
            
            // Insert clipboard text
            for (int i = 0; i < paste_len; i++) {
                el->text_content[el->cursor_y][el->cursor_x + i] = el->clipboard[i];
            }
            
            el->cursor_x += paste_len;
        }
    } else {
        // Pasting multi-line text - need to split and insert
        char *clipboard_copy = malloc(clipboard_len + 1);
        strcpy(clipboard_copy, el->clipboard);
        
        char *line = strtok(clipboard_copy, "\n");
        int first_line = 1;
        
        while (line != NULL) {
            if (first_line) {
                // Insert into current line at cursor position
                int line_len = strlen(el->text_content[el->cursor_y]);
                int insert_len = strlen(line);
                
                if (line_len + insert_len < MAX_LINE_LENGTH) {
                    // Shift existing text to the right
                    for (int i = line_len; i >= el->cursor_x; i--) {
                        el->text_content[el->cursor_y][i + insert_len] = el->text_content[el->cursor_y][i];
                    }
                    
                    // Insert the line text
                    for (int i = 0; i < insert_len; i++) {
                        el->text_content[el->cursor_y][el->cursor_x + i] = line[i];
                    }
                    
                    el->cursor_x += insert_len;
                }
                first_line = 0;
            } else {
                // Create new line after current line
                if (el->num_lines < MAX_TEXT_LINES - 1) {
                    // Shift lines down
                    for (int j = el->num_lines; j > el->cursor_y + 1; j--) {
                        strcpy(el->text_content[j], el->text_content[j-1]);
                    }
                    el->num_lines++;
                    
                    // Insert the new line
                    strcpy(el->text_content[el->cursor_y + 1], line);
                    
                    el->cursor_y++;
                    el->cursor_x = strlen(line);
                }
            }
            line = strtok(NULL, "\n");
        }
        
        free(clipboard_copy);
    }
}

// Function to handle special control keys for text editing
void handle_ctrl_keys(unsigned char key, int element_index) {
    if (key == 3) {  // Ctrl+C
        copy_text(element_index);
    } else if (key == 24) {  // Ctrl+X
        cut_text(element_index);
    } else if (key == 22) {  // Ctrl+V
        paste_text(element_index);
    }
}