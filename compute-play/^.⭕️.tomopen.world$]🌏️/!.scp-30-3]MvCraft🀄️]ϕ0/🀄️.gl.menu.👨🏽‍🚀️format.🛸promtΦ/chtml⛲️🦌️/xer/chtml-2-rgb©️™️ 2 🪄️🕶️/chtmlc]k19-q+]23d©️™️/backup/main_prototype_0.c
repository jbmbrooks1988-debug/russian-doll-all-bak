
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations for functions in other files
void init_model();
void init_controller();
void init_view(const char* filename);

void display();
void reshape(int, int);
void mouse(int, int, int, int);
void keyboard(unsigned char, int, int);
void special_keys(int, int, int);
void mouse_motion(int, int);

char* game_executable = NULL;  // Global variable to store the game executable path

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        printf("Usage: %s <markup_file.chtml> [game_executable]\n", argv[0]);
        exit(1);
    }
    
    if (argc == 3) {
        game_executable = argv[2];  // Store the game executable path
    }
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("C-HTML Prototype");

    // Initialize MVC components
    init_model();
    init_controller();
    init_view(argv[1]); // Parse the C-HTML file provided as argument

    // Register GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keys);
    glutMotionFunc(mouse_motion);

    glutMainLoop();

    return 0;
}
