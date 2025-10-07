#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

void init(void);
void initFreeType(void);
void display(void);
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void idle(void);

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(512, 552);  // 8*64 + 20, 8*64 + 60
    glutCreateWindow("Candy Crush Clone");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutIdleFunc(idle);

    init();
    initFreeType();

    glutMainLoop();

    return 0;
}
