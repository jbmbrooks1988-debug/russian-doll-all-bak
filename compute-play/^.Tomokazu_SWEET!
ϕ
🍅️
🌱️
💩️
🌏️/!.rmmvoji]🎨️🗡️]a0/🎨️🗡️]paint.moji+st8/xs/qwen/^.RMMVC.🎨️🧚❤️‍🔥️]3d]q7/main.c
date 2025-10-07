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

// Forward declarations
void init(void);
void initFreeType(void);
void display(void);
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void idle(void);

extern int canvas_rows;
extern int canvas_cols;
extern int tile_size;
extern int num_emojis;
extern int num_colors;
extern int max_layers;
extern int max_tabs;
extern int sidebar_width;
extern int file_tab_height;

#define MAX_CANVAS_DIM 128

extern int window_width;
extern int window_height;

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

    // Get screen size for near full-screen window
    Display *disp = XOpenDisplay(NULL);
    if (disp) {
        int scr = DefaultScreen(disp);
        int sw = DisplayWidth(disp, scr);
        int sh = DisplayHeight(disp, scr);
        XCloseDisplay(disp);
        window_width = sw - 100;
        window_height = sh - 100;
    } else {
        // Fallback
        window_width = 682;
        window_height = 562;
    }

    // Set sidebar wider for emoji grid
    sidebar_width = 420;

    // Compute canvas size
    canvas_cols = (window_width - sidebar_width - 20) / tile_size;
    canvas_rows = (window_height - file_tab_height - 20) / tile_size;
    if (canvas_cols > MAX_CANVAS_DIM) canvas_cols = MAX_CANVAS_DIM;
    if (canvas_rows > MAX_CANVAS_DIM) canvas_rows = MAX_CANVAS_DIM;

    // Adjust window size to fit exactly
    window_width = canvas_cols * tile_size + sidebar_width + 20;
    window_height = canvas_rows * tile_size + file_tab_height + 20;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Emoji Paint");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();

    glutMainLoop();

    return 0;
}
