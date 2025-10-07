/*
 * model.c - Model Component for CSV UI Framework
 * 
 * Handles data structures, state management, and CSV parsing
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MAX_ELEMENTS 256
#define MAX_ID_LENGTH 64
#define MAX_TEXT_LENGTH 256
#define MAX_CALLBACK_LENGTH 128
#define MAX_PROPERTIES_LENGTH 512

// Element types
typedef enum {
    ELEMENT_BUTTON,
    ELEMENT_SLIDER,
    ELEMENT_CANVAS,
    ELEMENT_LABEL,
    ELEMENT_CONTAINER,
    ELEMENT_IMAGE
} element_type_t;

// UI Element structure
typedef struct {
    element_type_t type;
    char id[MAX_ID_LENGTH];
    char parent_id[MAX_ID_LENGTH];
    int x, y;                 // Absolute position
    int width, height;        // Size
    float rel_x, rel_y;       // Relative position (0-1 scale)
    char text[MAX_TEXT_LENGTH];
    char onclick_callback[MAX_CALLBACK_LENGTH];
    char properties[MAX_PROPERTIES_LENGTH];
    int visible;
    int enabled;
    int hovered;
    int clicked;
    void* data;               // For custom data per element type
} ui_element_t;

// Global UI elements array
ui_element_t ui_elements[MAX_ELEMENTS];
int element_count = 0;

// Function to initialize the model
void init_model() {
    element_count = 0;
    memset(ui_elements, 0, sizeof(ui_elements));
    printf("Model initialized with capacity for %d elements\n", MAX_ELEMENTS);
}

// Function to create a new UI element
int create_element(element_type_t type, const char* id, const char* parent_id, 
                   int x, int y, int width, int height,
                   float rel_x, float rel_y, const char* text,
                   const char* onclick_callback, const char* properties) {
    if (element_count >= MAX_ELEMENTS) {
        printf("Error: Maximum element count reached\n");
        return -1;
    }
    
    ui_element_t* elem = &ui_elements[element_count];
    elem->type = type;
    
    strncpy(elem->id, id, MAX_ID_LENGTH - 1);
    elem->id[MAX_ID_LENGTH - 1] = '\0';
    
    strncpy(elem->parent_id, parent_id ? parent_id : "root", MAX_ID_LENGTH - 1);
    elem->parent_id[MAX_ID_LENGTH - 1] = '\0';
    
    elem->x = x;
    elem->y = y;
    elem->width = width;
    elem->height = height;
    elem->rel_x = rel_x;
    elem->rel_y = rel_y;
    
    if (text) {
        strncpy(elem->text, text, MAX_TEXT_LENGTH - 1);
        elem->text[MAX_TEXT_LENGTH - 1] = '\0';
    } else {
        elem->text[0] = '\0';
    }
    
    if (onclick_callback) {
        strncpy(elem->onclick_callback, onclick_callback, MAX_CALLBACK_LENGTH - 1);
        elem->onclick_callback[MAX_CALLBACK_LENGTH - 1] = '\0';
    } else {
        elem->onclick_callback[0] = '\0';
    }
    
    if (properties) {
        strncpy(elem->properties, properties, MAX_PROPERTIES_LENGTH - 1);
        elem->properties[MAX_PROPERTIES_LENGTH - 1] = '\0';
    } else {
        elem->properties[0] = '\0';
    }
    
    elem->visible = 1;
    elem->enabled = 1;
    elem->hovered = 0;
    elem->clicked = 0;
    elem->data = NULL;
    
    return element_count++;
}

// Function to find an element by ID
ui_element_t* find_element_by_id(const char* id) {
    for (int i = 0; i < element_count; i++) {
        if (strcmp(ui_elements[i].id, id) == 0) {
            return &ui_elements[i];
        }
    }
    return NULL;
}

// Function to update element position based on relative coordinates
void update_element_position(ui_element_t* elem, int container_width, int container_height) {
    if (elem->rel_x >= 0.0f && elem->rel_x <= 1.0f) {
        elem->x = (int)(elem->rel_x * container_width) - (elem->width / 2);
    }
    if (elem->rel_y >= 0.0f && elem->rel_y <= 1.0f) {
        elem->y = (int)(elem->rel_y * container_height) - (elem->height / 2);
    }
    
    // Ensure canvas elements are not positioned outside the window bounds
    if (elem->type == ELEMENT_CANVAS) {
        if (elem->x < 0) elem->x = 0;
        if (elem->y < 0) elem->y = 0;
        if (elem->x + elem->width > container_width) elem->x = container_width - elem->width;
        if (elem->y + elem->height > container_height) elem->y = container_height - elem->height;
    }
}

// Function to update all element positions based on relative coordinates
void update_all_positions(int window_width, int window_height) {
    for (int i = 0; i < element_count; i++) {
        update_element_position(&ui_elements[i], window_width, window_height);
    }
}

// Function to get element type from string
element_type_t get_element_type_from_string(const char* type_str) {
    if (strcmp(type_str, "button") == 0) return ELEMENT_BUTTON;
    if (strcmp(type_str, "slider") == 0) return ELEMENT_SLIDER;
    if (strcmp(type_str, "canvas") == 0) return ELEMENT_CANVAS;
    if (strcmp(type_str, "label") == 0) return ELEMENT_LABEL;
    if (strcmp(type_str, "container") == 0) return ELEMENT_CONTAINER;
    if (strcmp(type_str, "image") == 0) return ELEMENT_IMAGE;
    return ELEMENT_BUTTON; // default
}

// Simple CSV parser
int parse_csv_line(char* line, char* fields[], int max_fields) {
    int field_count = 0;
    char* token;
    char* rest = line;
    
    while ((token = strtok_r(field_count == 0 ? line : NULL, ",", &rest)) && field_count < max_fields) {
        // Remove leading/trailing whitespace and quotes
        while (*token == ' ' || *token == '"') token++;
        int len = strlen(token);
        while (len > 0 && (token[len-1] == ' ' || token[len-1] == '"')) {
            token[len-1] = '\0';
            len--;
        }
        
        fields[field_count++] = token;
    }
    
    return field_count;
}

// Load UI from CSV file
void load_ui_from_csv(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open CSV file: %s\n", filename);
        return;
    }
    
    char line[1024];
    int line_num = 0;
    
    // Read header line
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Error: Empty CSV file: %s\n", filename);
        fclose(file);
        return;
    }
    
    // Process each line
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) continue;
        
        // Parse CSV line
        char* fields[11]; // We expect 11 fields based on blueprint
        int field_count = parse_csv_line(line, fields, 11);
        
        if (field_count < 9) {
            printf("Warning: Line %d has %d fields, expected at least 9\n", line_num, field_count);
            continue;
        }
        
        // Create element from CSV fields
        element_type_t type = get_element_type_from_string(fields[0]);
        const char* id = fields[1];
        const char* parent_id = fields[2];
        int x = atoi(fields[3]);
        int y = atoi(fields[4]);
        int width = atoi(fields[5]);
        int height = atoi(fields[6]);
        float rel_x = atof(fields[7]);
        float rel_y = atof(fields[8]);
        const char* text = (field_count > 9) ? fields[9] : "";
        const char* onclick = (field_count > 10) ? fields[10] : "";
        const char* properties = (field_count > 11) ? fields[11] : "";
        
        create_element(type, id, parent_id, x, y, width, height, 
                      rel_x, rel_y, text, onclick, properties);
    }
    
    fclose(file);
    printf("Loaded %d UI elements from %s\n", element_count, filename);
}

// Update element state based on mouse position
void update_element_state(int mouse_x, int mouse_y, int mouse_down) {
    for (int i = 0; i < element_count; i++) {
        ui_element_t* elem = &ui_elements[i];
        
        // Skip invisible or disabled elements
        if (!elem->visible || !elem->enabled) continue;
        
        // Check if mouse is over element
        int is_over = (mouse_x >= elem->x && mouse_x <= elem->x + elem->width &&
                       mouse_y >= elem->y && mouse_y <= elem->y + elem->height);
        
        elem->hovered = is_over;
        
        if (is_over && mouse_down) {
            elem->clicked = 1;
        } else {
            elem->clicked = 0;
        }
    }
}

// Reset click states after processing
void reset_click_states() {
    for (int i = 0; i < element_count; i++) {
        ui_elements[i].clicked = 0;
    }
}

// NOTE: execute_callbacks() is also defined in controller.c, 
// so we'll remove this duplicate definition

// Cleanup function
void cleanup_model() {
    // Free any dynamically allocated data
    for (int i = 0; i < element_count; i++) {
        if (ui_elements[i].data) {
            free(ui_elements[i].data);
            ui_elements[i].data = NULL;
        }
    }
    element_count = 0;
}