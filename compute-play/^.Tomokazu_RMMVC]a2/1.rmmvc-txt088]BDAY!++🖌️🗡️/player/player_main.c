/*
 * player_main.c - Main orchestrator for the player program
 * This file is based on the RMMV architecture pattern
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>

// Debug flags
#define DEBUG_MODE 1
#define DEBUG_KEYS 1
#define DEBUG_MAP_LOADING 1

// Forward declaration for player movement function
// void move_player(int dx, int dy); // This is now defined in scenes.c

// Forward declarations for all modules
void init_core(void);
void init_managers(void);
void init_objects(void);
void init_scenes(void);
void init_sprites(void);
void init_windows(void);
void init_plugins(void);

void update_core(void);
void update_managers(void);
void update_objects(void);
void update_scenes(void);
void update_sprites(void);
void update_windows(void);
void update_plugins(void);

void render_core(void);
void render_managers(void);
void render_objects(void);
void render_scenes(void);
void render_sprites(void);
void render_windows(void);
void render_plugins(void);

void cleanup_core(void);
void cleanup_managers(void);
void cleanup_objects(void);
void cleanup_scenes(void);
void cleanup_sprites(void);
void cleanup_windows(void);
void cleanup_plugins(void);

// External function declarations
typedef enum {
    SCENE_TITLE,
    SCENE_MAP,
    SCENE_MENU,
    SCENE_BATTLE,
    SCENE_GAMEOVER,
    SCENE_NUM_TYPES
} SceneType;

typedef struct Scene {
    SceneType type;
    bool active;
    int fade_sign;      // 0=none, 1=fadeIn, -1=fadeOut
    int fade_duration;
} Scene;

Scene* get_current_scene(void);
void goto_scene(int scene_type);

// Game state
static bool game_running = true;
static int window_width = 800;
static int window_height = 600;

#if DEBUG_MODE
static bool debug_mode = true;
static char debug_message[256] = "";
#endif

// Display function
void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Render all modules
    render_core();
    render_managers();
    render_objects();
    render_scenes();
    render_sprites();
    render_windows();
    render_plugins();
    
#if DEBUG_MODE
    // Render debug information at the bottom of the screen
    if (debug_mode) {
        // Switch to 2D orthographic projection for UI rendering
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, window_width, 0, window_height);
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        // Disable depth testing for 2D UI
        glDisable(GL_DEPTH_TEST);
        
        // Render debug text at the bottom of the screen
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(10, 20); // Position at bottom left
        for (int i = 0; debug_message[i] != '\0'; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, debug_message[i]);
        }
        
        // Restore 3D perspective projection
        glPopMatrix(); // MODELVIEW
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        
        // Re-enable depth testing
        glEnable(GL_DEPTH_TEST);
    }
#endif
    
    glutSwapBuffers();
}

// Reshape function
void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    
    // Update window dimensions in other modules
    // (We would need to implement setters for this in a full implementation)
    
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);  // Set up orthographic projection for 2D rendering
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// Keyboard function
void keyboard(unsigned char key, int x, int y) {
#if DEBUG_KEYS
    if (debug_mode) {
        snprintf(debug_message, sizeof(debug_message), "Key pressed: %d ('%c')", key, key);
        printf("DEBUG: %s\n", debug_message); // Also output to CLI
    }
#endif
    
    switch (key) {
        case 27: // ESC key
            game_running = false;
            printf("DEBUG: ESC key pressed, exiting...\n");
            exit(0); // Just exit the program
            break;
        case 'd': // Toggle debug mode
        case 'D':
#if DEBUG_MODE
            debug_mode = !debug_mode;
            if (debug_mode) {
                strcpy(debug_message, "Debug mode enabled");
            } else {
                strcpy(debug_message, "Debug mode disabled");
            }
            printf("DEBUG: %s\n", debug_message); // Output to CLI
#endif
            break;
        case 13: // Enter key
        case 32: // Space key
            // Get current scene to determine how to handle input
            struct Scene* current_scene = get_current_scene();
            if (current_scene) {
                // If in title scene, go to map scene
                if (current_scene->type == 0) { // SCENE_TITLE (0)
                    goto_scene(1); // SCENE_MAP (1)
                }
            }
            break;
        case 'w': // Move player up
        case 'W':
            printf("DEBUG: Move player up\n");
            // Move player logic would go here
            break;
        case 's': // Move player down
        case 'S':
            printf("DEBUG: Move player down\n");
            // Move player logic would go here
            break;
        case 'a': // Move player left
        case 'A':
            printf("DEBUG: Move player left\n");
            // Move player logic would go here
            break;
        case 'x': // Move player right (avoiding conflict with debug)
        case 'X':
            printf("DEBUG: Move player right\n");
            // Move player logic would go here
            break;
        default:
            break;
    }
}

// Forward declaration for player movement
void move_player(int dx, int dy);

// Special keyboard function
void special(int key, int x, int y) {
#if DEBUG_KEYS
    if (debug_mode) {
        switch (key) {
            case GLUT_KEY_UP:
                strcpy(debug_message, "Arrow Up pressed");
                break;
            case GLUT_KEY_DOWN:
                strcpy(debug_message, "Arrow Down pressed");
                break;
            case GLUT_KEY_LEFT:
                strcpy(debug_message, "Arrow Left pressed");
                break;
            case GLUT_KEY_RIGHT:
                strcpy(debug_message, "Arrow Right pressed");
                break;
            default:
                snprintf(debug_message, sizeof(debug_message), "Special key pressed: %d", key);
                break;
        }
        printf("DEBUG: %s\n", debug_message); // Also output to CLI
    }
#endif

    // Get current scene to determine how to handle input
    struct Scene* current_scene = get_current_scene();
    if (current_scene) {
        // Handle different scenes
        switch (current_scene->type) {
            case 0: // SCENE_TITLE (0)
                switch (key) {
                    case GLUT_KEY_UP:
                        // Move selection up in title menu
                        // This would need to be implemented in scenes.c
                        break;
                    case GLUT_KEY_DOWN:
                        // Move selection down in title menu
                        // This would need to be implemented in scenes.c
                        break;
                    case 13: // Enter key
                    case 32: // Space key
                        // Select current menu option
                        // For now, just go to map scene
                        goto_scene(1); // SCENE_MAP (1)
                        break;
                }
                break;
                
            case 1: // SCENE_MAP (1)
                switch (key) {
                    case GLUT_KEY_UP:
                        move_player(0, -1); // Move up
                        break;
                    case GLUT_KEY_DOWN:
                        move_player(0, 1); // Move down
                        break;
                    case GLUT_KEY_LEFT:
                        move_player(-1, 0); // Move left
                        break;
                    case GLUT_KEY_RIGHT:
                        move_player(1, 0); // Move right
                        break;
                }
                break;
                
            default:
                // For other scenes, just handle player movement
                switch (key) {
                    case GLUT_KEY_UP:
                        move_player(0, -1); // Move up
                        break;
                    case GLUT_KEY_DOWN:
                        move_player(0, 1); // Move down
                        break;
                    case GLUT_KEY_LEFT:
                        move_player(-1, 0); // Move left
                        break;
                    case GLUT_KEY_RIGHT:
                        move_player(1, 0); // Move right
                        break;
                }
                break;
        }
    }
}

// Idle function
void idle(void) {
    if (game_running) {
        // Update all modules
        update_core();
        update_managers();
        update_objects();
        update_scenes();
        update_sprites();
        update_windows();
        update_plugins();
        
        // Trigger redraw
        glutPostRedisplay();
    }
}

// Initialize all modules
void init_player(void) {
    printf("Initializing player modules...\n");
    
    init_core();
    init_managers();
    init_objects();
    init_scenes();
    init_sprites();
    init_windows();
    init_plugins();
    
    printf("Player modules initialized.\n");
}

// Forward declaration for getting the current scene
Scene* get_current_scene(void);

// Cleanup all modules
void cleanup_player(void) {
    printf("Cleaning up player modules...\n");
    
    cleanup_plugins();
    cleanup_windows();
    cleanup_sprites();
    cleanup_scenes();
    cleanup_objects();
    cleanup_managers();
    cleanup_core();
    
    printf("Player modules cleaned up.\n");
}

// Main function
int main(int argc, char** argv) {
    printf("Starting player program\n");
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("RMMV-Inspired Player");
    
    // Setup callbacks
    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);
    
    // Initialize player
    init_player();
    
#if DEBUG_MODE
    strcpy(debug_message, "Player started - Press 'D' to toggle debug");
#endif
    
    // Enter main loop
    printf("Entering main loop\n");
    glutMainLoop();
    
    // Cleanup
    cleanup_player();
    
    return 0;
}