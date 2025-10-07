/*
 * view.c - View Component for CSV UI Framework
 * 
 * Handles rendering of UI elements using OpenGL
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <sys/types.h>

// Include our model header information (extern declarations)
extern int window_width;
extern int window_height;

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

// Global variables for 3D player position (extern declaration to link with controller.c)
extern float player_x;
extern float player_z;
extern float player_angle;

// Function declarations
void render_debug_info();

// Function to draw a rectangle
void draw_rectangle(int x, int y, int width, int height, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + width, y);
    glVertex2i(x + width, y + height);
    glVertex2i(x, y + height);
    glEnd();
}

// Function to draw text (simplified - in real implementation would use proper font rendering)
void draw_text(int x, int y, const char* text) {
    glRasterPos2i(x, y + 15); // Adjust for text baseline
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
}

// Function to render a button element
void render_button(ui_element_t* elem) {
    if (!elem->visible) return;
    
    // Set button color based on state
    float r = 0.3f, g = 0.5f, b = 0.8f, a = 1.0f;  // Default blue
    
    if (elem->clicked) {
        r = 0.2f; g = 0.4f; b = 0.7f;  // Darker when clicked
    } else if (elem->hovered) {
        r = 0.4f; g = 0.6f; b = 0.9f;  // Lighter when hovered
    }
    
    // Draw button background
    draw_rectangle(elem->x, elem->y, elem->width, elem->height, r, g, b, a);
    
    // Draw button border
    glColor3f(0.0f, 0.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(elem->x, elem->y);
    glVertex2i(elem->x + elem->width, elem->y);
    glVertex2i(elem->x + elem->width, elem->y + elem->height);
    glVertex2i(elem->x, elem->y + elem->height);
    glEnd();
    
    // Draw button text centered
    if (elem->text[0] != '\0') {
        int text_width = strlen(elem->text) * 8; // Rough approximation
        int center_x = elem->x + (elem->width - text_width) / 2;
        int center_y = elem->y + (elem->height + 12) / 2; // Adjust for text height
        
        glColor3f(1.0f, 1.0f, 1.0f); // White text
        draw_text(center_x, center_y, elem->text);
    }
}

// Function to render a slider element
void render_slider(ui_element_t* elem) {
    if (!elem->visible) return;
    
    // Draw slider track
    draw_rectangle(elem->x, elem->y + elem->height/2 - 3, elem->width, 6, 0.4f, 0.4f, 0.4f, 1.0f);
    
    // Calculate slider thumb position (simplified)
    float min_val = 0.0f, max_val = 100.0f, current_val = 50.0f; // Default values
    // In a real implementation, these would come from properties
    
    // Parse properties to get min, max, value
    char* props = strdup(elem->properties);
    char* token = strtok(props, ";");
    
    while (token) {
        if (strncmp(token, "min=", 4) == 0) {
            min_val = atof(token + 4);
        } else if (strncmp(token, "max=", 4) == 0) {
            max_val = atof(token + 4);
        } else if (strncmp(token, "value=", 6) == 0) {
            current_val = atof(token + 6);
        }
        token = strtok(NULL, ";");
    }
    
    free(props);
    
    float value_ratio = (current_val - min_val) / (max_val - min_val);
    int thumb_x = elem->x + (int)(value_ratio * (elem->width - 20)); // 20 is thumb width
    
    // Draw slider thumb
    float r = 0.6f, g = 0.8f, b = 1.0f;
    if (elem->hovered) {
        r = 0.8f; g = 0.9f; b = 1.0f;
    }
    
    draw_rectangle(thumb_x, elem->y + elem->height/2 - 10, 20, 20, r, g, b, 1.0f);
    
    // Draw border for thumb
    glColor3f(0.0f, 0.0f, 0.0f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(thumb_x, elem->y + elem->height/2 - 10);
    glVertex2i(thumb_x + 20, elem->y + elem->height/2 - 10);
    glVertex2i(thumb_x + 20, elem->y + elem->height/2 + 10);
    glVertex2i(thumb_x, elem->y + elem->height/2 + 10);
    glEnd();
}

// Function to render a label element
void render_label(ui_element_t* elem) {
    if (!elem->visible) return;
    
    // Draw label text
    if (elem->text[0] != '\0') {
        glColor3f(1.0f, 1.0f, 1.0f); // White text by default
        draw_text(elem->x, elem->y + elem->height/2, elem->text);
    }
}

// Function to render a canvas element (background area)
void render_canvas(ui_element_t* elem) {
    if (!elem->visible) return;
    
    // Save current OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    
    // Draw canvas background
    draw_rectangle(elem->x, elem->y, elem->width, elem->height, 0.1f, 0.1f, 0.1f, 1.0f);
    
    // Draw canvas border - different colors based on render mode
    if (strstr(elem->properties, "render_mode=3d") != NULL) {
        glColor3f(1.0f, 0.0f, 0.0f); // Red for 3D canvas
    } else if (strstr(elem->properties, "render_mode=2d") != NULL) {
        glColor3f(0.0f, 1.0f, 0.0f); // Green for 2D canvas
    } else {
        glColor3f(0.5f, 0.5f, 0.5f); // Gray for default canvas
    }
    
    glLineWidth(3.0f); // Thicker border for better visibility
    glBegin(GL_LINE_LOOP);
    glVertex2i(elem->x, elem->y);
    glVertex2i(elem->x + elem->width, elem->y);
    glVertex2i(elem->x + elem->width, elem->y + elem->height);
    glVertex2i(elem->x, elem->y + elem->height);
    glEnd();
    
    // Check if this canvas needs 3D rendering
    if (strstr(elem->properties, "render_mode=3d") != NULL) {
        // Draw a large colored rectangle to indicate 3D canvas
        glColor3f(0.8f, 0.2f, 0.2f); // Red to indicate 3D canvas
        glBegin(GL_QUADS);
        glVertex2i(elem->x + 10, elem->y + 10);
        glVertex2i(elem->x + elem->width - 10, elem->y + 10);
        glVertex2i(elem->x + elem->width - 10, elem->y + elem->height - 10);
        glVertex2i(elem->x + 10, elem->y + elem->height - 10);
        glEnd();
        
        // Add text to indicate 3D mode
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2i(elem->x + elem->width/2 - 30, elem->y + elem->height/2);
        for(const char* c = "3D GAME"; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
        
        // Draw a representation of the player in the 3D world view
        float player_screen_x = elem->x + elem->width/2 + (player_x * 5); // Scale player x position
        float player_screen_y = elem->y + elem->height/2 - (player_z * 5); // Scale player z position (inverted Y)
        
        // Ensure player is within canvas bounds
        if (player_screen_x < elem->x + 15) player_screen_x = elem->x + 15;
        if (player_screen_x > elem->x + elem->width - 15) player_screen_x = elem->x + elem->width - 15;
        if (player_screen_y < elem->y + 15) player_screen_y = elem->y + 15;
        if (player_screen_y > elem->y + elem->height - 15) player_screen_y = elem->y + elem->height - 15;
        
        // Draw a more visible player marker
        glColor3f(1.0f, 1.0f, 0.0f); // Yellow player
        glRectf(player_screen_x - 10, player_screen_y - 10, player_screen_x + 10, player_screen_y + 10);
        
        // Add a border to make the player more visible
        glColor3f(0.0f, 0.0f, 0.0f); // Black border
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(player_screen_x - 10, player_screen_y - 10);
        glVertex2f(player_screen_x + 10, player_screen_y - 10);
        glVertex2f(player_screen_x + 10, player_screen_y + 10);
        glVertex2f(player_screen_x - 10, player_screen_y + 10);
        glEnd();
    } 
    // Check if this canvas needs 2D rendering
    else if (strstr(elem->properties, "render_mode=2d") != NULL) {
        // Draw mini map grid
        glColor3f(0.0f, 0.5f, 0.0f); // Green grid lines
        for (int i = 10; i <= elem->width - 10; i += 20) {
            if (i % 40 == 0) { // Make every 4th line darker
                glColor3f(0.0f, 0.3f, 0.0f);
            } else {
                glColor3f(0.0f, 0.2f, 0.0f);
            }
            glBegin(GL_LINES);
            glVertex2i(elem->x + i, elem->y + 10);
            glVertex2i(elem->x + i, elem->y + elem->height - 10);
            glEnd();
        }
        
        for (int i = 10; i <= elem->height - 10; i += 20) {
            if (i % 40 == 0) { // Make every 4th line darker
                glColor3f(0.0f, 0.3f, 0.0f);
            } else {
                glColor3f(0.0f, 0.2f, 0.0f);
            }
            glBegin(GL_LINES);
            glVertex2i(elem->x + 10, elem->y + i);
            glVertex2i(elem->x + elem->width - 10, elem->y + i);
            glEnd();
        }
        
        // Draw player position (scaled from 3D world coordinates)
        float map_x = elem->x + 15 + (player_x + 5.0f) * (elem->width - 30) / 10.0f; // -5 to 5 world -> 0 to width
        float map_y = elem->y + 15 + (player_z + 5.0f) * (elem->height - 30) / 10.0f; // -5 to 5 world -> 0 to height
        
        // Clamp player position to map bounds
        if (map_x < elem->x + 15) map_x = elem->x + 15;
        if (map_x > elem->x + elem->width - 15) map_x = elem->x + elem->width - 15;
        if (map_y < elem->y + 15) map_y = elem->y + 15;
        if (map_y > elem->y + elem->height - 15) map_y = elem->y + elem->height - 15;
        
        // Draw a more visible player marker on mini map
        glColor3f(1.0f, 0.0f, 0.0f); // Red for player on mini map
        glRectf(map_x - 6, map_y - 6, map_x + 6, map_y + 6);
        
        // Add a border to make the player more visible
        glColor3f(1.0f, 1.0f, 1.0f); // White border
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(map_x - 6, map_y - 6);
        glVertex2f(map_x + 6, map_y - 6);
        glVertex2f(map_x + 6, map_y + 6);
        glVertex2f(map_x - 6, map_y + 6);
        glEnd();
        
        // Draw obstacles on mini map with better visibility
        float obs1_x = elem->x + 15 + (2.0f + 5.0f) * (elem->width - 30) / 10.0f;
        float obs1_y = elem->y + 15 + (-2.0f + 5.0f) * (elem->height - 30) / 10.0f;
        glColor3f(0.0f, 0.0f, 1.0f); // Blue obstacle
        glRectf(obs1_x - 4, obs1_y - 4, obs1_x + 4, obs1_y + 4);
        
        float obs2_x = elem->x + 15 + (-2.0f + 5.0f) * (elem->width - 30) / 10.0f;
        float obs2_y = elem->y + 15 + (2.0f + 5.0f) * (elem->height - 30) / 10.0f;
        glColor3f(1.0f, 1.0f, 0.0f); // Yellow obstacle
        glRectf(obs2_x - 4, obs2_y - 4, obs2_x + 4, obs2_y + 4);
        
        // Add text to indicate 2D mode
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2i(elem->x + elem->width/2 - 25, elem->y + elem->height - 15);
        for(const char* c = "2D MAP"; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
    }
    
    // Restore previous OpenGL state
    glPopMatrix();
    glPopAttrib();
}

// Function to render a container element
void render_container(ui_element_t* elem) {
    if (!elem->visible) return;
    
    // Draw container background
    draw_rectangle(elem->x, elem->y, elem->width, elem->height, 0.2f, 0.2f, 0.2f, 0.5f);
    
    // Draw container border
    glColor3f(0.7f, 0.7f, 0.7f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(elem->x, elem->y);
    glVertex2i(elem->x + elem->width, elem->y);
    glVertex2i(elem->x + elem->width, elem->y + elem->height);
    glVertex2i(elem->x, elem->y + elem->height);
    glEnd();
}

// Main render function
void render_view() {
    // Set up 2D rendering mode
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Render all UI elements
    for (int i = 0; i < element_count; i++) {
        ui_element_t* elem = &ui_elements[i];
        
        switch (elem->type) {
            case ELEMENT_BUTTON:
                render_button(elem);
                break;
            case ELEMENT_SLIDER:
                render_slider(elem);
                break;
            case ELEMENT_LABEL:
                render_label(elem);
                break;
            case ELEMENT_CANVAS:
                render_canvas(elem);
                break;
            case ELEMENT_CONTAINER:
                render_container(elem);
                break;
            case ELEMENT_IMAGE:
                // For now, render as a colored rectangle
                draw_rectangle(elem->x, elem->y, elem->width, elem->height, 0.5f, 0.3f, 0.7f, 1.0f);
                break;
            default:
                printf("Unknown element type for rendering: %d\n", elem->type);
                break;
        }
    }
    
    glPopMatrix();
    
    // Render debug info
    render_debug_info();
}

// Function to render debug info
void render_debug_info() {
    char info[256];
    sprintf(info, "Elements: %d | Window: %dx%d", element_count, window_width, window_height);
    
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text(10, 20, info);
}