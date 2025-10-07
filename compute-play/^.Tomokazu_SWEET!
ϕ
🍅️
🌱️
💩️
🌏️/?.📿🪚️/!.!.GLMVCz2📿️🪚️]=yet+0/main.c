#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

void init(void);
void initFreeType(void);
void display(void);
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void idle(void);

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(512, 552);
    glutCreateWindow("Generic Game");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();
    initFreeType();

    glutMainLoop();
    return 0;
}
