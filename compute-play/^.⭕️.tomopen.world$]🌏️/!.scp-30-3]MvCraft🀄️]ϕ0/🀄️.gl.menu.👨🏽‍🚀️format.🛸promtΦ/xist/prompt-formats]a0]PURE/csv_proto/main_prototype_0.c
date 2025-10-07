/*
 * main_prototype_0.c - OpenGL Context Manager
 * 
 * This file manages the OpenGL/Glut context and integrates
 * the MVC components for the CSV-based UI framework.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

// Forward declarations for MVC functions (will be provided by modules)
extern void init_model();
extern void render_view();
extern void handle_input(unsigned char key, int x, int y);
extern void update_controller();
extern void load_ui_from_csv(const char* filename);
extern void cleanup();
extern void execute_callbacks();
extern void update_element_positions(int window_width, int window_height);
extern void reset_element_click_states();
extern void handle_special_keys(int key, int x, int y);

// Global state
int window_width = 1024;
int window_height = 768;
int mouse_x = 0, mouse_y = 0;
int left_mouse_down = 0;

// Function prototypes for callbacks
void display_callback();
void reshape_callback(int width, int height);
void keyboard_callback(unsigned char key, int x, int y);
void mouse_callback(int button, int state, int x, int y);
void motion_callback(int x, int y);
void idle_callback();

int main(int argc, char** argv) {
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("CSV UI Framework Prototype");
    
    // Initialize OpenGL settings based on the working example
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);  // Slightly brighter background
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set up initial projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)window_width / (double)window_height, 0.1, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    
    // Initialize MVC components
    init_model();
    
    // Load default UI from CSV
    load_ui_from_csv("ui_definitions/game_menu.csv");
    
    // Set GLUT callbacks
    glutDisplayFunc(display_callback);
    glutReshapeFunc(reshape_callback);
    glutKeyboardFunc(keyboard_callback);
    glutSpecialFunc(handle_special_keys); // Add special keys callback for arrow keys
    glutMouseFunc(mouse_callback);
    glutMotionFunc(motion_callback);
    glutIdleFunc(idle_callback);
    
    // Start the GLUT main loop
    printf("CSV UI Framework initialized. Window size: %dx%d\n", window_width, window_height);
    printf("Loading UI from CSV files...\n");
    glutMainLoop();
    
    return 0;
}

void display_callback() {
    // Clear the screen and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Update controller logic
    update_controller();
    
    // Update element positions based on window size
    update_element_positions(window_width, window_height);
    
    // Set up orthographic projection for 2D UI rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, window_width, 0, window_height);
    
    // Set up modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Render the view
    render_view();
    
    glutSwapBuffers();
}

void reshape_callback(int width, int height) {
    window_width = width;
    window_height = height;
    
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);  // Keep Y-axis in standard screen coordinates (0 at bottom)
    glMatrixMode(GL_MODELVIEW);
}

void keyboard_callback(unsigned char key, int x, int y) {
    // Handle escape to exit
    if (key == 27) { // ESC key
        cleanup();
        exit(0);
    }
    
    // Pass keyboard input to controller
    handle_input(key, x, y);
}

void mouse_callback(int button, int state, int x, int y) {
    mouse_x = x;
    mouse_y = y;
    
    if (button == GLUT_LEFT_BUTTON) {
        left_mouse_down = (state == GLUT_DOWN) ? 1 : 0;
    }
    
    // Update controller with mouse state
    update_controller();
    
    // Execute callbacks if needed
    if (state == GLUT_UP) {
        execute_callbacks();
        reset_element_click_states();
    }
}

void motion_callback(int x, int y) {
    mouse_x = x;
    mouse_y = y;
    
    // Update controller with mouse movement
    update_controller();
}

void idle_callback() {
    // Perform any background tasks or updates
    // This is called when GLUT is not handling events
    glutPostRedisplay();
}

// External implementations will be provided by model.c, view.c, controller.c
// These stubs will be replaced by the actual implementations when linked