#include <GL/glut.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#define M_PI 3.14159265358979323846
#define NUM_ENEMIES 3
#define MAX_PROJECTILES 10
#define SENSITIVITY 0.1f
#define MOVE_SPEED 0.1f
#define PROJECTILE_SPEED 0.5f
#define PROJECTILE_LIFETIME 2.0f // Seconds before projectile despawns

// Camera variables
float x = 0.0f, y = 1.75f, z = 5.0f;
float yaw = -90.0f;
float pitch = 0.0f;
float front_x, front_y, front_z;

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

// Projectile structure
typedef struct {
    float x, y, z;          // Position
    float dx, dy, dz;       // Direction/velocity
    float time_alive;       // Time since spawned
    int active;             // Is projectile active?
} Projectile;

Projectile projectiles[MAX_PROJECTILES];

// Timing for updates
double last_time = 0.0;

// Function prototypes
void update_camera_direction();
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int mx, int my);
void passive_mouse_motion(int mx, int my);
void shoot();
void update_projectiles();
void check_collisions();
void idle();
void init();

void update_camera_direction() {
    front_x = cos(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
    front_y = sin(pitch * M_PI / 180.0f);
    front_z = sin(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    gluLookAt(x, y, z,
              x + front_x, y + front_y, z + front_z,
              0.0f, 1.0f, 0.0f);

    // Draw ground plane
    glColor3f(0.9f, 0.9f, 0.9f);
    glBegin(GL_QUADS);
    glVertex3f(-100.0f, 0.0f, -100.0f);
    glVertex3f(-100.0f, 0.0f, 100.0f);
    glVertex3f(100.0f, 0.0f, 100.0f);
    glVertex3f(100.0f, 0.0f, -100.0f);
    glEnd();

    // Draw enemies as red cubes
    glColor3f(1.0f, 0.0f, 0.0f);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (enemies[i].alive) {
            glPushMatrix();
            glTranslatef(enemies[i].x, enemies[i].y, enemies[i].z);
            glutSolidCube(1.0f);
            glPopMatrix();
        }
    }

    // Draw projectiles as blue spheres
    glColor3f(0.0f, 0.0f, 1.0f);
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
            glPushMatrix();
            glTranslatef(projectiles[i].x, projectiles[i].y, projectiles[i].z);
            glutSolidSphere(0.1f, 16, 16); // Small sphere for projectile
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
    glutWarpPointer(w / 2, h / 2);
}

void keyboard(unsigned char key, int mx, int my) {
    float right_x, right_y, right_z;
    float norm;

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
        case 'w':
            x += front_x * MOVE_SPEED;
            z += front_z * MOVE_SPEED;
            break;
        case 's':
            x -= front_x * MOVE_SPEED;
            z -= front_z * MOVE_SPEED;
            break;
        case 'a':
            x -= right_x * MOVE_SPEED;
            z -= right_z * MOVE_SPEED;
            break;
        case 'd':
            x += right_x * MOVE_SPEED;
            z += right_z * MOVE_SPEED;
            break;
        case 'f':
            shoot();
            break;
        case 27: // ESC
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

    int dx = mx - mouse_last_x;
    int dy = my - mouse_last_y;

    yaw += dx * SENSITIVITY;
    pitch -= dy * SENSITIVITY;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    update_camera_direction();

    glutWarpPointer(window_width / 2, window_height / 2);
    mouse_last_x = window_width / 2;
    mouse_last_y = window_height / 2;

    glutPostRedisplay();
}

void shoot() {
    // Find an inactive projectile slot
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].x = x;
            projectiles[i].y = y;
            projectiles[i].z = z;
            projectiles[i].dx = front_x * PROJECTILE_SPEED;
            projectiles[i].dy = front_y * PROJECTILE_SPEED;
            projectiles[i].dz = front_z * PROJECTILE_SPEED;
            projectiles[i].time_alive = 0.0f;
            projectiles[i].active = 1;
            break;
        }
    }
}

void update_projectiles() {
    double current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
    double delta_time = current_time - last_time;
    last_time = current_time;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
            // Update position
            projectiles[i].x += projectiles[i].dx * delta_time;
            projectiles[i].y += projectiles[i].dy * delta_time;
            projectiles[i].z += projectiles[i].dz * delta_time;
            projectiles[i].time_alive += delta_time;

            // Deactivate if lifetime exceeded
            if (projectiles[i].time_alive > PROJECTILE_LIFETIME) {
                projectiles[i].active = 0;
            }
        }
    }
}

void check_collisions() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        for (int j = 0; j < NUM_ENEMIES; j++) {
            if (!enemies[j].alive) continue;

            // Check sphere-sphere collision (projectile vs enemy)
            float dx = projectiles[i].x - enemies[j].x;
            float dy = projectiles[i].y - enemies[j].y;
            float dz = projectiles[i].z - enemies[j].z;
            float distance = sqrt(dx * dx + dy * dy + dz * dz);
            float min_distance = 0.1f + enemies[j].radius; // Projectile radius + enemy radius

            if (distance < min_distance) {
                enemies[j].alive = 0;
                projectiles[i].active = 0;
                // Optional: printf("Hit enemy %d!\n", j);
                break;
            }
        }
    }
}

void idle() {
    update_projectiles();
    check_collisions();
    glutPostRedisplay();
}

void init() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    update_camera_direction();

    // Initialize enemies
    enemies[0].x = 0.0f; enemies[0].y = 0.5f; enemies[0].z = -10.0f; enemies[0].radius = 0.7f; enemies[0].alive = 1;
    enemies[1].x = 5.0f; enemies[1].y = 0.5f; enemies[1].z = -15.0f; enemies[1].radius = 0.7f; enemies[1].alive = 1;
    enemies[2].x = -5.0f; enemies[2].y = 0.5f; enemies[2].z = -20.0f; enemies[2].radius = 0.7f; enemies[2].alive = 1;

    // Initialize projectiles
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = 0;
    }

    last_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("3D Shooter with Projectiles in Pure C");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(passive_mouse_motion);
    glutIdleFunc(idle);

    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(window_width / 2, window_height / 2);
    mouse_last_x = window_width / 2;
    mouse_last_y = window_height / 2;

    glutMainLoop();
    return 0;
}
