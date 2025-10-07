#include <GL/glut.h>
#include <stdbool.h>

void set_entity_action(int entity_index, int action_id, float value);
void set_status_message(const char* msg);
bool get_is_animating(void);
void update_entities(void);

void keyboard(unsigned char key, int x, int y) {
    set_entity_action(0, key, 1.0f); // Entity 0 (e.g., player) with key as action ID
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    set_entity_action(0, key + 256, 1.0f); // Special keys offset by 256
    glutPostRedisplay();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle(void) {
    if (get_is_animating()) {
        update_entities();
        glutPostRedisplay();
    }
}
