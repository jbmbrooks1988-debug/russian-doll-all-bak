/*
 * windows.c - UI window system
 * Based on RMMV's rpg_windows.js functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <GL/glut.h>

// Window structure
typedef struct GameWindow {
    float x, y;
    float width, height;
    bool visible;
    bool active;
    int opacity;
    int back_opacity;
    int contents_opacity;
    int padding;
    int margin;
    char** lines;
    int line_count;
    int max_lines;
    int cursor_index;
    struct GameWindow* next;
    struct GameWindow* prev;
} GameWindow;

// Windows function declarations
void init_windows(void);
void update_windows(void);
void render_windows(void);
void cleanup_windows(void);

// Window management functions
GameWindow* create_window(float x, float y, float width, float height);
void destroy_window(GameWindow* window);

// Window property functions
void set_window_position(GameWindow* window, float x, float y);
void set_window_size(GameWindow* window, float width, float height);
void set_window_visible(GameWindow* window, bool visible);
void set_window_active(GameWindow* window, bool active);
void set_window_opacity(GameWindow* window, int opacity);

// Window content functions
void add_text_to_window(GameWindow* window, const char* text);
void clear_window_text(GameWindow* window);
void set_window_cursor_index(GameWindow* window, int index);

// Getters
GameWindow* get_first_window(void);
GameWindow* get_last_window(void);

// Window manager state
static GameWindow* first_window = NULL;
static GameWindow* last_window = NULL;
static bool windows_initialized = false;
static int window_width = 800;
static int window_height = 600;

// Create a new window
GameWindow* create_window(float x, float y, float width, float height) {
    GameWindow* window = malloc(sizeof(GameWindow));
    if (window) {
        memset(window, 0, sizeof(GameWindow));
        window->x = x;
        window->y = y;
        window->width = width;
        window->height = height;
        window->visible = true;
        window->active = false;
        window->opacity = 255;
        window->back_opacity = 192;
        window->contents_opacity = 255;
        window->padding = 12;
        window->margin = 4;
        window->line_count = 0;
        window->max_lines = 10;
        window->cursor_index = 0;
        window->lines = malloc(window->max_lines * sizeof(char*));
        if (!window->lines) {
            free(window);
            return NULL;
        }
        
        // Add to window list
        if (last_window) {
            last_window->next = window;
            window->prev = last_window;
        } else {
            first_window = window;
        }
        last_window = window;
    }
    return window;
}

// Destroy a window
void destroy_window(GameWindow* window) {
    if (!window) return;
    
    // Remove from window list
    if (window->prev) {
        window->prev->next = window->next;
    } else {
        first_window = window->next;
    }
    
    if (window->next) {
        window->next->prev = window->prev;
    } else {
        last_window = window->prev;
    }
    
    // Free lines
    for (int i = 0; i < window->line_count; i++) {
        if (window->lines[i]) {
            free(window->lines[i]);
        }
    }
    free(window->lines);
    
    free(window);
}

// Set window position
void set_window_position(GameWindow* window, float x, float y) {
    if (window) {
        window->x = x;
        window->y = y;
    }
}

// Set window size
void set_window_size(GameWindow* window, float width, float height) {
    if (window) {
        window->width = width;
        window->height = height;
    }
}

// Set window visibility
void set_window_visible(GameWindow* window, bool visible) {
    if (window) {
        window->visible = visible;
    }
}

// Set window active state
void set_window_active(GameWindow* window, bool active) {
    if (window) {
        window->active = active;
    }
}

// Set window opacity
void set_window_opacity(GameWindow* window, int opacity) {
    if (window) {
        window->opacity = opacity;
    }
}

// Add text to window
void add_text_to_window(GameWindow* window, const char* text) {
    if (!window || !text) return;
    
    // Expand lines array if needed
    if (window->line_count >= window->max_lines) {
        int new_max_lines = window->max_lines * 2;
        char** new_lines = realloc(window->lines, new_max_lines * sizeof(char*));
        if (new_lines) {
            window->lines = new_lines;
            window->max_lines = new_max_lines;
        } else {
            return; // Failed to expand
        }
    }
    
    // Add new line
    window->lines[window->line_count] = malloc(strlen(text) + 1);
    if (window->lines[window->line_count]) {
        strcpy(window->lines[window->line_count], text);
        window->line_count++;
    }
}

// Clear window text
void clear_window_text(GameWindow* window) {
    if (!window) return;
    
    for (int i = 0; i < window->line_count; i++) {
        if (window->lines[i]) {
            free(window->lines[i]);
            window->lines[i] = NULL;
        }
    }
    window->line_count = 0;
    window->cursor_index = 0;
}

// Set cursor index
void set_window_cursor_index(GameWindow* window, int index) {
    if (window) {
        window->cursor_index = index;
        if (window->cursor_index < 0) window->cursor_index = 0;
        if (window->cursor_index >= window->line_count) window->cursor_index = window->line_count - 1;
    }
}

// Render a window
void render_window(GameWindow* window) {
    if (!window || !window->visible) return;
    
    // Save current matrix and attributes
    glPushMatrix();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Apply window position
    glTranslatef(window->x, window->y, 0);
    
    // Render window background
    glColor4f(0.0f, 0.0f, 0.0f, window->back_opacity / 255.0f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(window->width, 0);
    glVertex2f(window->width, window->height);
    glVertex2f(0, window->height);
    glEnd();
    
    // Render window frame
    glColor4f(1.0f, 1.0f, 1.0f, window->opacity / 255.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(0, 0);
    glVertex2f(window->width, 0);
    glVertex2f(window->width, window->height);
    glVertex2f(0, window->height);
    glEnd();
    
    // Render contents
    if (window->line_count > 0) {
        float content_x = window->padding;
        float content_y = window->height - window->padding;
        float line_height = 20.0f;
        
        for (int i = 0; i < window->line_count; i++) {
            if (window->lines[i]) {
                // Highlight cursor line if active
                if (window->active && i == window->cursor_index) {
                    glColor4f(0.2f, 0.4f, 1.0f, window->contents_opacity / 255.0f);
                    glBegin(GL_QUADS);
                    glVertex2f(content_x - 2, content_y - line_height + 2);
                    glVertex2f(window->width - window->padding, content_y - line_height + 2);
                    glVertex2f(window->width - window->padding, content_y + 2);
                    glVertex2f(content_x - 2, content_y + 2);
                    glEnd();
                }
                
                // Render text
                glColor4f(1.0f, 1.0f, 1.0f, window->contents_opacity / 255.0f);
                glRasterPos2f(content_x, content_y);
                for (int j = 0; window->lines[i][j] != '\0'; j++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, window->lines[i][j]);
                }
            }
            content_y -= line_height;
        }
    }
    
    // Restore matrix and attributes
    glPopAttrib();
    glPopMatrix();
}

// Windows initialization
void init_windows(void) {
    if (windows_initialized) return;
    
    printf("Initializing windows...\n");
    
    windows_initialized = true;
    printf("Windows initialized.\n");
}

// Windows update
void update_windows(void) {
    // Windows update logic
}

// Windows rendering
void render_windows(void) {
    if (!windows_initialized) return;
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render all windows
    GameWindow* window = first_window;
    while (window) {
        render_window(window);
        window = window->next;
    }
    
    // Disable blending
    glDisable(GL_BLEND);
}

// Windows cleanup
void cleanup_windows(void) {
    if (!windows_initialized) return;
    
    printf("Cleaning up windows...\n");
    
    // Destroy all windows
    while (first_window) {
        GameWindow* next = first_window->next;
        destroy_window(first_window);
        first_window = next;
    }
    last_window = NULL;
    
    windows_initialized = false;
    printf("Windows cleaned up.\n");
}

// Getters
GameWindow* get_first_window(void) {
    return first_window;
}

GameWindow* get_last_window(void) {
    return last_window;
}