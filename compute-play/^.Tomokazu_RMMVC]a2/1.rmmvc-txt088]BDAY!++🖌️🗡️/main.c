#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>

#define MAX_CANVAS_DIM 128
#define HEADER_STRIP_HEIGHT 40

// Forward declarations
void init(void);
void initFreeType(void);
void init_emoji_texture_cache(void);
void cleanup_emoji_texture_cache(void);
void init_input_bindings(void);
void display(void);
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void mouse_motion(int x, int y);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void idle(void);

// Model function declarations
void set_canvas_rows(int value);
void set_canvas_cols(int value);
void set_tile_size(int value);
void set_sidebar_width(int value);
void set_window_width(int value);
void set_window_height(int value);
void set_file_tab_height(int value);
int get_canvas_rows(void);
int get_canvas_cols(void);
int get_tile_size(void);
int get_sidebar_width(void);
int get_file_tab_height(void);
int get_window_width(void);
int get_window_height(void);
void trigger_view_update(void);
void set_view_update_callback(void (*callback)(void));

// Global variable to store original terminal settings
struct termios original_term;

void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
}

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    restore_terminal();
    printf("Caught Ctrl+C, exiting...\n");
    exit(0);
}

int main(int argc, char** argv) {
    printf("Starting main function\n");
    // Set up SIGINT handler for Ctrl+C
    signal(SIGINT, handle_sigint);

    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &original_term);
    // Register cleanup function
    atexit(restore_terminal);

    // Modify terminal settings
    struct termios term = original_term;
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    printf("Getting screen size\n");
    // Get screen size for near full-screen window
    Display *disp = XOpenDisplay(NULL);
    if (disp) {
        int scr = DefaultScreen(disp);
        int sw = DisplayWidth(disp, scr);
        int sh = DisplayHeight(disp, scr);
        XCloseDisplay(disp);
        set_window_width(sw - 100);
        set_window_height(sh - 100);
    } else {
        // Fallback
        set_window_width(682);
        set_window_height(562);
    }

    printf("Setting canvas parameters\n");
    // Set sidebar wider for emoji grid
    set_sidebar_width(420);
    set_file_tab_height(30);
    set_tile_size(32);

    // Compute canvas size
    int cols = (get_window_width() - get_sidebar_width() - 20) / get_tile_size();
    int rows = (get_window_height() - get_file_tab_height() - 20 - HEADER_STRIP_HEIGHT) / get_tile_size();
    if (cols > MAX_CANVAS_DIM) cols = MAX_CANVAS_DIM;
    if (rows > MAX_CANVAS_DIM) rows = MAX_CANVAS_DIM;

    set_canvas_cols(cols);
    set_canvas_rows(rows);

    // Adjust window size to fit exactly
    set_window_width(get_canvas_cols() * get_tile_size() + get_sidebar_width() + 20);
    set_window_height(get_canvas_rows() * get_tile_size() + get_file_tab_height() + 20 + HEADER_STRIP_HEIGHT);

    printf("Initializing GLUT\n");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(get_window_width(), get_window_height());
    glutCreateWindow("Emoji Paint");

    printf("Setting up GLUT callbacks\n");
    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutMotionFunc(mouse_motion);
    glutPassiveMotionFunc(mouse_motion);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    printf("Initializing components\n");
    init();
    initFreeType();
    init_emoji_texture_cache();  // Initialize emoji texture cache
    init_input_bindings();
    
    // Register view update callback
    set_view_update_callback(glutPostRedisplay);

    printf("Entering main loop\n");
    glutMainLoop();

    // Cleanup
    cleanup_emoji_texture_cache();  // Cleanup emoji texture cache

    return 0;
}
