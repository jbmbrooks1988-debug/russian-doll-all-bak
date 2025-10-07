#include <GL/glut.h>
#include <stdio.h>

void init(void);
void view_init(void);
void display(void);
void get_viewport_size(int *width, int *height);

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    int width, height;
    get_viewport_size(&width, &height);
    glutInitWindowSize(width, height);
    glutCreateWindow("Zelda-like Roguelike");
    
    view_init(); // Initialize FreeType and view keyboard
    init();      // Initialize OpenGL and game world
    
    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}
