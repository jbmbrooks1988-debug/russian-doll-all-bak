#include <GL/glut.h>
#include <math.h>
#include <float.h>
#include <stdio.h>  // For any potential debugging, optional

#define M_PI 3.14159265358979323846  // Define PI if not available
#define NUM_ENEMIES 3
#define SENSITIVITY 0.1f
#define MOVE_SPEED 0.1f

// Camera variables
float x = 0.0f, y = 1.75f, z = 5.0f;  // Camera position
float yaw = -90.0f;                   // Horizontal angle (yaw)
float pitch = 0.0f;                   // Vertical angle (pitch)
float front_x, front_y, front_z;      // Forward direction vector

// Mouse control
int mouse_last_x, mouse_last_y;
int mouse_first_entry = 1;
int window_width = 800, window_height = 600;

// Enemy structure
typedef struct {
    float x, y, z;
    float radius;
    int alive;
} Enemy;

Enemy enemies[NUM_ENEMIES];

// Function prototypes
void update_camera_direction();
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int mx, int my);
void passive_mouse_motion(int mx, int my);
void shoot();
void init();

void update_camera_direction() {
    // Compute the forward direction based on yaw and pitch
    front_x = cos(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
    front_y = sin(pitch * M_PI / 180.0f);
    front_z = sin(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Set up the camera view
    gluLookAt(x, y, z,
              x + front_x, y + front_y, z + front_z,
              0.0f, 1.0f, 0.0f);

    // Draw the ground plane
    glColor3f(0.9f, 0.9f, 0.9f);
    glBegin(GL_QUADS);
    glVertex3f(-100.0f, 0.0f, -100.0f);
    glVertex3f(-100.0f, 0.0f, 100.0f);
    glVertex3f(100.0f, 0.0f, 100.0f);
    glVertex3f(100.0f, 0.0f, -100.0f);
    glEnd();

    // Draw enemies as red cubes if alive
    glColor3f(1.0f, 0.0f, 0.0f);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (enemies[i].alive) {
            glPushMatrix();
            glTranslatef(enemies[i].x, enemies[i].y, enemies[i].z);
            glutSolidCube(1.0f);
            glPopMatrix();
        }
    }

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)w / (float)h, 0.1f, 200.0f);
    glMatrixMode(GL_MODELVIEW);
    // Warp mouse to center on reshape
    glutWarpPointer(w / 2, h / 2);
}

void keyboard(unsigned char key, int mx, int my) {
    float right_x, right_y, right_z;
    float norm;

    // Compute right vector for strafing (cross product of front and up)
    right_x = front_y * 0.0f - front_z * 1.0f;
    right_y = front_z * 0.0f - front_x * 0.0f;
    right_z = front_x * 1.0f - front_y * 0.0f;
    norm = sqrt(right_x * right_x + right_y * right_y + right_z * right_z);
    if (norm != 0.0f) {
        right_x /= norm;
        right_y /= norm;
        right_z /= norm;
    }

    switch (key) {
        case 'w':  // Move forward
            x += front_x * MOVE_SPEED;
            z += front_z * MOVE_SPEED;
            break;
        case 's':  // Move backward
            x -= front_x * MOVE_SPEED;
            z -= front_z * MOVE_SPEED;
            break;
        case 'a':  // Strafe left
            x -= right_x * MOVE_SPEED;
            z -= right_z * MOVE_SPEED;
            break;
        case 'd':  // Strafe right
            x += right_x * MOVE_SPEED;
            z += right_z * MOVE_SPEED;
            break;
        case 'f':  // Fire/shoot
            shoot();
            break;
        case 27:  // ESC to exit
            exit(0);
            break;
    }
    glutPostRedisplay();
}

void passive_mouse_motion(int mx, int my) {
    if (mouse_first_entry) {
        mouse_last_x = mx;
        mouse_last_y = my;
        mouse_first_entry = 0;
        return;
    }

    // Calculate delta
    int dx = mx - mouse_last_x;
    int dy = my - mouse_last_y;

    // Update angles
    yaw += dx * SENSITIVITY;
    pitch -= dy * SENSITIVITY;

    // Clamp pitch to avoid flipping
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    update_camera_direction();

    // Warp mouse back to center
    glutWarpPointer(window_width / 2, window_height / 2);
    mouse_last_x = window_width / 2;
    mouse_last_y = window_height / 2;

    glutPostRedisplay();
}

void shoot() {
    // Ray origin (camera position)
    float o_x = x, o_y = y, o_z = z;
    // Ray direction (front vector, assumed normalized)
    float dir_x = front_x, dir_y = front_y, dir_z = front_z;

    float min_t = FLT_MAX;
    int hit_index = -1;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (!enemies[i].alive) continue;

        // Vector from origin to enemy center
        float c_x = enemies[i].x - o_x;
        float c_y = enemies[i].y - o_y;
        float c_z = enemies[i].z - o_z;

        // Projection of center vector onto ray direction
        float tca = c_x * dir_x + c_y * dir_y + c_z * dir_z;
        if (tca < 0.0f) continue;  // Behind the camera

        // Squared distance from center to ray
        float d2 = (c_x * c_x + c_y * c_y + c_z * c_z) - (tca * tca);
        float r2 = enemies[i].radius * enemies[i].radius;
        if (d2 > r2) continue;  // No intersection

        // Half chord distance
        float thc = sqrt(r2 - d2);

        // Intersection distances
        float t0 = tca - thc;
        float t1 = tca + thc;

        if (t0 > t1) { float temp = t0; t0 = t1; t1 = temp; }
        if (t0 < 0.0f) t0 = t1;
        if (t0 < 0.0f) continue;  // No positive intersection

        // Check for closest hit
        if (t0 < min_t) {
            min_t = t0;
            hit_index = i;
        }
    }

    if (hit_index != -1) {
        enemies[hit_index].alive = 0;
        // Optional: printf("Hit enemy %d!\n", hit_index);
    }
}

void init() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Initialize camera direction
    update_camera_direction();

    // Initialize enemies (positioned in the scene)
    enemies[0].x = 0.0f; enemies[0].y = 0.5f; enemies[0].z = -10.0f; enemies[0].radius = 0.7f; enemies[0].alive = 1;
    enemies[1].x = 5.0f; enemies[1].y = 0.5f; enemies[1].z = -15.0f; enemies[1].radius = 0.7f; enemies[1].alive = 1;
    enemies[2].x = -5.0f; enemies[2].y = 0.5f; enemies[2].z = -20.0f; enemies[2].radius = 0.7f; enemies[2].alive = 1;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Simple 3D Shooter in Pure C with OpenGL and GLUT");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(passive_mouse_motion);

    // Hide cursor
    glutSetCursor(GLUT_CURSOR_NONE);

    // Initial warp to center
    glutWarpPointer(window_width / 2, window_height / 2);
    mouse_last_x = window_width / 2;
    mouse_last_y = window_height / 2;

    glutMainLoop();
    return 0;
}
