
#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// For this prototype, we'll define a simple structure for UI elements
// and a hardcoded array to represent the parsed C-HTML file.

#define MAX_ELEMENTS 50

// Forward declaration for canvas render function
void canvas_render_sample(int x, int y, int width, int height);

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

UIElement elements[MAX_ELEMENTS];
int num_elements = 0;

// Simple parser for our C-HTML format
void parse_chtml(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening CHTML file");
        return;
    }

    char line[512]; // Increased buffer size for longer lines
    num_elements = 0;
    int parent_stack[MAX_ELEMENTS];
    int stack_top = -1;

    while (fgets(line, sizeof(line), file)) {
        // Trim leading/trailing whitespace
        char* trimmed_line = line;
        while (*trimmed_line == ' ' || *trimmed_line == '\t') trimmed_line++;
        size_t len = strlen(trimmed_line);
        while (len > 0 && (trimmed_line[len-1] == '\n' || trimmed_line[len-1] == '\r' || trimmed_line[len-1] == ' ' || trimmed_line[len-1] == '\t')) {
            trimmed_line[--len] = '\0';
        }

        if (len == 0) continue; // Skip empty lines

        // Check for closing tag
        if (trimmed_line[0] == '<' && trimmed_line[1] == '/') {
            if (stack_top > -1) {
                stack_top--;
            }
            continue;
        }

        // Check for opening tag
        if (trimmed_line[0] != '<') continue;

        // Extract tag name
        char* tag_start = trimmed_line + 1;
        char* tag_end = strchr(tag_start, ' ');
        if (!tag_end) tag_end = strchr(tag_start, '>');
        if (!tag_end) continue; // Malformed tag

        char tag_name[50];
        strncpy(tag_name, tag_start, tag_end - tag_start);
        tag_name[tag_end - tag_start] = '\0';

        if (num_elements >= MAX_ELEMENTS) {
            fprintf(stderr, "Error: Max elements reached!\n");
            break;
        }

        elements[num_elements].parent = (stack_top > -1) ? parent_stack[stack_top] : -1;
        strcpy(elements[num_elements].type, tag_name);
        elements[num_elements].id[0] = '\0'; // Initialize ID to empty string
        elements[num_elements].is_active = 0;
        elements[num_elements].cursor_pos = 0;
        elements[num_elements].text_content[0] = '\0';
        elements[num_elements].is_checked = 0;
        elements[num_elements].slider_value = 0;
        elements[num_elements].slider_min = 0;
        elements[num_elements].slider_max = 100;
        elements[num_elements].slider_step = 1;
        elements[num_elements].canvas_initialized = 0;
        elements[num_elements].canvas_render_func = NULL;
        elements[num_elements].onClick[0] = '\0'; // Initialize onClick to empty string
        elements[num_elements].menu_items_count = 0;
        elements[num_elements].is_open = 0;

        // Parse attributes
        char* attr_start = tag_end;
        while (attr_start && (attr_start = strstr(attr_start, " ")) != NULL) {
            attr_start++; // Move past space
            char* eq_sign = strchr(attr_start, '=');
            if (!eq_sign) break; // No more attributes

            char attr_name[50];
            strncpy(attr_name, attr_start, eq_sign - attr_start);
            attr_name[eq_sign - attr_start] = '\0';

            char* value_start = eq_sign + 1;
            if (*value_start != '\"') break; // Value not quoted
            value_start++; // Move past opening quote

            char* value_end = strchr(value_start, '\"');
            if (!value_end) break; // No closing quote

            char attr_value[256];
            strncpy(attr_value, value_start, value_end - value_start);
            attr_value[value_end - value_start] = '\0';

            if (strcmp(attr_name, "x") == 0) elements[num_elements].x = atoi(attr_value);
            else if (strcmp(attr_name, "y") == 0) elements[num_elements].y = atoi(attr_value);
            else if (strcmp(attr_name, "width") == 0) elements[num_elements].width = atoi(attr_value);
            else if (strcmp(attr_name, "height") == 0) elements[num_elements].height = atoi(attr_value);
            else if (strcmp(attr_name, "id") == 0) strncpy(elements[num_elements].id, attr_value, 49);
            else if (strcmp(attr_name, "label") == 0) strncpy(elements[num_elements].label, attr_value, 49);
            else if (strcmp(attr_name, "value") == 0) {
                if (strcmp(elements[num_elements].type, "textfield") == 0) {
                    strncpy(elements[num_elements].text_content, attr_value, 255);
                    elements[num_elements].cursor_pos = strlen(elements[num_elements].text_content);
                } else if (strcmp(elements[num_elements].type, "slider") == 0) {
                    elements[num_elements].slider_value = atoi(attr_value);
                } else if (strcmp(elements[num_elements].type, "text") == 0) {
                    // For text elements, store the value in the label field
                    strncpy(elements[num_elements].label, attr_value, 49);
                }
            }
            else if (strcmp(attr_name, "checked") == 0) {
                if (strcmp(attr_value, "true") == 0) {
                    elements[num_elements].is_checked = 1;
                }
            }
            else if (strcmp(attr_name, "min") == 0) elements[num_elements].slider_min = atoi(attr_value);
            else if (strcmp(attr_name, "max") == 0) elements[num_elements].slider_max = atoi(attr_value);
            else if (strcmp(attr_name, "step") == 0) elements[num_elements].slider_step = atoi(attr_value);
            else if (strcmp(attr_name, "color") == 0) {
                long color = strtol(attr_value + 1, NULL, 16);
                elements[num_elements].color[0] = ((color >> 16) & 0xFF) / 255.0f;
                elements[num_elements].color[1] = ((color >> 8) & 0xFF) / 255.0f;
                elements[num_elements].color[2] = (color & 0xFF) / 255.0f;
            }
            else if (strcmp(attr_name, "onClick") == 0) {
                strncpy(elements[num_elements].onClick, attr_value, 49);
                elements[num_elements].onClick[49] = '\0'; // Ensure null termination
            }
            attr_start = value_end + 1;
        }

        // Ensure slider_value is within min/max bounds after parsing all attributes
        if (strcmp(elements[num_elements].type, "slider") == 0) {
            if (elements[num_elements].slider_value < elements[num_elements].slider_min) {
                elements[num_elements].slider_value = elements[num_elements].slider_min;
            }
            if (elements[num_elements].slider_value > elements[num_elements].slider_max) {
                elements[num_elements].slider_value = elements[num_elements].slider_max;
            }
        }

        if (strcmp(tag_name, "panel") == 0) {
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "canvas") == 0) {
            // Assign a default render function to canvas elements
            elements[num_elements].canvas_render_func = canvas_render_sample;
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "menu") == 0) {
            elements[num_elements].menu_items_count = 0; // Initialize to 0, will be updated during parsing
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "menuitem") == 0) {
            // menuitem elements don't change the stack (they don't contain other elements)
            // but might need to increment the parent's menu_items_count
        }

        num_elements++;
    }

    fclose(file);
}

void init_view() {
    parse_chtml("demo.chtml");
}

void draw_element(UIElement* el) {
    int parent_x = 0;
    int parent_y = 0;

    if (el->parent != -1) {
        parent_x = elements[el->parent].x;
        parent_y = elements[el->parent].y;
    }

    int abs_x = parent_x + el->x;
    int abs_y = parent_y + el->y;

    if (strcmp(el->type, "canvas") == 0) {
        // Draw a border for the canvas
        glColor3f(0.5f, 0.5f, 0.5f); // Gray border
        glBegin(GL_LINE_LOOP);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();
        
        // Draw canvas content without changing global OpenGL state
        if (el->canvas_render_func != NULL) {
            // Save the current modelview matrix
            glPushMatrix();
            
            // Apply transformation to move to canvas position
            glTranslatef(abs_x, abs_y, 0.0f);
            
            // Call the canvas render function, adjusting the coordinates it receives
            // The function will draw in the local coordinate system
            el->canvas_render_func(0, 0, el->width, el->height);
            
            // Restore the modelview matrix
            glPopMatrix();
        }
    } else if (strcmp(el->type, "menu") == 0) {
        // Draw menu bar background
        glColor3fv(el->color);
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();
        
        // Draw menu text
        glColor3f(1.0f, 1.0f, 1.0f);
        int text_offset_x = 5;
        int text_offset_y = 15;  // Adjust based on menu height
        glRasterPos2i(abs_x + text_offset_x, abs_y + text_offset_y);
        for (char* c = el->label; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
        
        // If menu is open, draw submenu items (simplified for now)
        if (el->is_open) {
            glColor3f(0.8f, 0.8f, 0.8f); // Light gray for submenu background
            glBegin(GL_QUADS);
            glVertex2i(abs_x, abs_y + el->height); // Below the menu bar
            glVertex2i(abs_x + el->width, abs_y + el->height);
            glVertex2i(abs_x + el->width, abs_y + el->height + 20 * el->menu_items_count); // Each item is ~20px high
            glVertex2i(abs_x, abs_y + el->height + 20 * el->menu_items_count);
            glEnd();
        }
    } else if (strcmp(el->type, "menuitem") == 0) {
        // Draw menu item (only when part of an open menu)
        // This would be drawn in the submenu context, which we'll handle in a more complete implementation
        glColor3fv(el->color);
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();
        
        // Draw menu item text
        glColor3f(0.0f, 0.0f, 0.0f); // Black text for menu items
        glRasterPos2i(abs_x + 5, abs_y + 15);
        for (char* c = el->label; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
    } else if (strcmp(el->type, "header") == 0 || strcmp(el->type, "button") == 0 || strcmp(el->type, "panel") == 0 || strcmp(el->type, "textfield") == 0 || strcmp(el->type, "checkbox") == 0 || strcmp(el->type, "slider") == 0) {
        if (strcmp(el->type, "textfield") == 0) {
            glColor3f(0.1f, 0.1f, 0.1f); // Dark background for textfield
        } else if (strcmp(el->type, "checkbox") == 0) {
            glColor3f(0.8f, 0.8f, 0.8f); // Light background for checkbox square
        } else if (strcmp(el->type, "slider") == 0) {
            glColor3f(0.3f, 0.3f, 0.3f); // Dark background for slider track
        } else {
            glColor3fv(el->color);
        }
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();

        if (strcmp(el->type, "checkbox") == 0 && el->is_checked) {
            // Draw a simple checkmark (upright V shape, adjusted for inverted Y)
            glColor3f(0.0f, 0.0f, 0.0f); // Black checkmark
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            // First segment: bottom-left to top-middle
            glVertex2i(abs_x + el->width * 0.2, abs_y + el->height * 0.8); // Visually lower
            glVertex2i(abs_x + el->width * 0.4, abs_y + el->height * 0.2); // Visually higher
            // Second segment: top-middle to bottom-right
            glVertex2i(abs_x + el->width * 0.4, abs_y + el->height * 0.2); // Visually higher
            glVertex2i(abs_x + el->width * 0.8, abs_y + el->height * 0.8); // Visually lower
            glEnd();
            glLineWidth(1.0f);
        } else if (strcmp(el->type, "slider") == 0) {
            // Draw slider thumb
            float thumb_pos_x = abs_x + (float)(el->slider_value - el->slider_min) / (el->slider_max - el->slider_min) * (el->width - 10); // 10 is thumb width
            glColor3f(0.8f, 0.8f, 0.8f); // Light grey for thumb
            glBegin(GL_QUADS);
            glVertex2i(thumb_pos_x, abs_y);
            glVertex2i(thumb_pos_x + 10, abs_y);
            glVertex2i(thumb_pos_x + 10, abs_y + el->height);
            glVertex2i(thumb_pos_x, abs_y + el->height);
            glEnd();

            // Draw slider value as text
            char value_str[10];
            sprintf(value_str, "%d", el->slider_value);
            glColor3f(1.0f, 1.0f, 1.0f);
            glRasterPos2i(abs_x + el->width + 5, abs_y + el->height / 2 - 5);
            for (char* c = value_str; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
            }
        }
    }

    if (strcmp(el->type, "text") == 0 || strcmp(el->type, "button") == 0 || strcmp(el->type, "checkbox") == 0) {
        glColor3f(1.0f, 1.0f, 1.0f);
        int text_offset_x = 5;
        int text_offset_y = 15;
        if (strcmp(el->type, "checkbox") == 0) {
            text_offset_x = el->width + 5; // Label next to checkbox
            text_offset_y = el->height / 2 - 5; // Center label vertically
        }
        glRasterPos2i(abs_x + text_offset_x, abs_y + text_offset_y);
        for (char* c = el->label; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
    } else if (strcmp(el->type, "textfield") == 0) {
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2i(abs_x + 5, abs_y + 15);
        for (char* c = el->text_content; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }

        // Draw cursor if active
        if (el->is_active) {
            // Simple blinking cursor (toggle every 500ms)
            if ((glutGet(GLUT_ELAPSED_TIME) / 500) % 2) {
                int text_width = 0;
                for (int i = 0; i < el->cursor_pos; i++) {
                    text_width += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, el->text_content[i]);
                }
                glColor3f(1.0f, 1.0f, 1.0f);
                glBegin(GL_QUADS);
                glVertex2i(abs_x + 5 + text_width, abs_y + 10);
                glVertex2i(abs_x + 5 + text_width + 2, abs_y + 10);
                glVertex2i(abs_x + 5 + text_width + 2, abs_y + 20);
                glVertex2i(abs_x + 5 + text_width, abs_y + 20);
                glEnd();
            }
        }
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    for (int i = 0; i < num_elements; i++) {
        draw_element(&elements[i]);
    }

    glutSwapBuffers();
}

// Sample canvas rendering function
void canvas_render_sample(int x, int y, int width, int height) {
    // Draw a simple rectangle (do not clear the color buffer as that would affect the whole screen)
    glColor3f(0.8f, 0.9f, 1.0f); // Light blue background for canvas content
    glBegin(GL_QUADS);
    glVertex2i(0, 0);  // Start at local origin (0,0) which is translated to canvas position
    glVertex2i(width, 0);
    glVertex2i(width, height);
    glVertex2i(0, height);
    glEnd();
    
    // Draw a simple rectangle
    glColor3f(1.0f, 0.0f, 0.0f); // Red
    glBegin(GL_QUADS);
    glVertex2i(width * 0.2, height * 0.2);
    glVertex2i(width * 0.8, height * 0.2);
    glVertex2i(width * 0.8, height * 0.8);
    glVertex2i(width * 0.2, height * 0.8);
    glEnd();
    
    // Draw a triangle
    glColor3f(0.0f, 1.0f, 0.0f); // Green
    glBegin(GL_TRIANGLES);
    glVertex2i(width * 0.5, height * 0.7);
    glVertex2i(width * 0.3, height * 0.3);
    glVertex2i(width * 0.7, height * 0.3);
    glEnd();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
