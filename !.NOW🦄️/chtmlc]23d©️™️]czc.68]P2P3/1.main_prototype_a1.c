
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // for usleep

// Forward declarations for functions in other files
void init_controller();
void init_view(const char* filename);
void init_model(const char* module_path);
int update_model();

void display();
void reshape(int, int);
void mouse(int, int, int, int);
void keyboard(unsigned char, int, int);
void special_keys(int, int, int);
void mouse_motion(int, int);

char* module_executable = NULL;

// Forward declaration for controller function to update UI with model variables
void update_ui_with_model_variables();

void idle_func() {
    int updated = update_model();
    
    if (updated) {
        update_ui_with_model_variables();  // Update UI with new model data
        glutPostRedisplay();
    }
    
    // Limit to ~60 FPS by sleeping about 16ms between cycles
    usleep(16000); // Sleep for approximately 16 milliseconds
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        printf("Usage: %s <markup_file.chtml> [module_executable]\n", argv[0]);
        exit(1);
    }
    
    if (argc == 3) {
        module_executable = argv[2];
    }
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("C-HTML Prototype");

    // Initialize MVC components
    init_model(module_executable);
    init_controller();
    init_view(argv[1]); // Parse the C-HTML file provided as argument

    // Register GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keys);
    glutMotionFunc(mouse_motion);

    // Set the idle function for optimized rendering
    glutIdleFunc(idle_func);

    glutMainLoop();

    return 0;
}
