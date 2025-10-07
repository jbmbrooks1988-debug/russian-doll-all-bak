
#include <GL/glut.h>

// Forward declarations for functions in other files
void init_model();
void init_controller();
void init_view();

void display();
void reshape(int, int);
void mouse(int, int, int, int);
void keyboard(unsigned char, int, int);
void mouse_motion(int, int);

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("C-HTML Prototype");

    // Initialize MVC components
    init_model();
    init_controller();
    init_view(); // This will parse the C-HTML file

    // Register GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutMotionFunc(mouse_motion);

    glutMainLoop();

    return 0;
}
