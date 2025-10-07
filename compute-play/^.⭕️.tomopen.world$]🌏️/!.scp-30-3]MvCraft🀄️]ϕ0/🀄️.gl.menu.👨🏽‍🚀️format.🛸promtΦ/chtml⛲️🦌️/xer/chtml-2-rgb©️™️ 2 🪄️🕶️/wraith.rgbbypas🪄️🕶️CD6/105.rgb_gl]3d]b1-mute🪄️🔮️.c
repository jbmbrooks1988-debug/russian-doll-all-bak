
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define MAX_LINE 256
#define MAX_PATH 256
#define MAX_PIXELS 307200 // 640x480
#define MAX_PRIMITIVES 10000
#define FILE_CHECK_INTERVAL 100000 // 100ms in microseconds
#define TARGET_FPS 24

// Global variables
char rgb_path[MAX_PATH] = "./rgb.txt";
char log_path[MAX_PATH] = "log.txt";
int pixel_x[MAX_PIXELS], pixel_y[MAX_PIXELS];
unsigned char pixel_r[MAX_PIXELS], pixel_g[MAX_PIXELS], pixel_b[MAX_PIXELS];
int num_pixels = 0;
float cam_x = 0.5f, cam_y = 2.0f, cam_z = 8.5f;
float player_x = 0.5f, player_y = 0.5f, player_z = 6.5f;
float cursor_x = 3.5f, cursor_y = 1.5f, cursor_z = 5.5f;
int orbit_mode = 1;
float orbit_angle = 0.0f;
time_t last_rgb_time = 0;
struct timespec last_check_time = {0, 0};

// Primitives reconstructed from rgb.txt
char (*shape)[16] = NULL;
float *x = NULL, *y = NULL, *z = NULL;
float *r = NULL, *g = NULL, *b = NULL, *a = NULL;
char (*type)[16] = NULL;
int num_primitives = 0;
int primitive_capacity = 0;

void log_to_file(const char *message) {
    FILE *fp = fopen(log_path, "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s\n", timestamp, message);
    fclose(fp);
}

void read_locations() {
    FILE *fp = fopen("locations.txt", "r");
    if (!fp) {
        log_to_file("Warning: Failed to open locations.txt, using default paths");
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_PATH], value[MAX_PATH];
        if (sscanf(line, "%s %s", key, value) != 2) continue;
        if (strcmp(key, "rgb") == 0) {
            strncpy(rgb_path, value, MAX_PATH - 1);
            rgb_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "log") == 0) {
            strncpy(log_path, value, MAX_PATH - 1);
            log_path[MAX_PATH - 1] = '\0';
        }
    }
    fclose(fp);
    char msg[MAX_PATH * 2];
    snprintf(msg, sizeof(msg), "Loaded paths: rgb=%s, log=%s", rgb_path, log_path);
    log_to_file(msg);
}

void init_primitives() {
    primitive_capacity = 1000;
    shape = malloc(primitive_capacity * sizeof(char[16]));
    x = malloc(primitive_capacity * sizeof(float));
    y = malloc(primitive_capacity * sizeof(float));
    z = malloc(primitive_capacity * sizeof(float));
    r = malloc(primitive_capacity * sizeof(float));
    g = malloc(primitive_capacity * sizeof(float));
    b = malloc(primitive_capacity * sizeof(float));
    a = malloc(primitive_capacity * sizeof(float));
    type = malloc(primitive_capacity * sizeof(char[16]));
    if (!shape || !x || !y || !z || !r || !g || !b || !a || !type) {
        log_to_file("Error: Initial memory allocation failed in init_primitives");
        exit(1);
    }
    memset(shape, 0, primitive_capacity * sizeof(char[16]));
    memset(x, 0, primitive_capacity * sizeof(float));
    memset(y, 0, primitive_capacity * sizeof(float));
    memset(z, 0, primitive_capacity * sizeof(float));
    memset(r, 0, primitive_capacity * sizeof(float));
    memset(g, 0, primitive_capacity * sizeof(float));
    memset(b, 0, primitive_capacity * sizeof(float));
    memset(a, 0, primitive_capacity * sizeof(float));
    memset(type, 0, primitive_capacity * sizeof(char[16]));
    log_to_file("Initialized primitive arrays with capacity 1000");
}

void resize_primitives() {
    int new_capacity = primitive_capacity == 0 ? 1000 : primitive_capacity * 2;
    if (new_capacity > MAX_PRIMITIVES) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Reached MAX_PRIMITIVES (%d), cannot resize", MAX_PRIMITIVES);
        log_to_file(msg);
        return;
    }
    char (*new_shape)[16] = realloc(shape, new_capacity * sizeof(char[16]));
    float *new_x = realloc(x, new_capacity * sizeof(float));
    float *new_y = realloc(y, new_capacity * sizeof(float));
    float *new_z = realloc(z, new_capacity * sizeof(float));
    float *new_r = realloc(r, new_capacity * sizeof(float));
    float *new_g = realloc(g, new_capacity * sizeof(float));
    float *new_b = realloc(b, new_capacity * sizeof(float));
    float *new_a = realloc(a, new_capacity * sizeof(float));
    char (*new_type)[16] = realloc(type, new_capacity * sizeof(char[16]));

    if (!new_shape || !new_x || !new_y || !new_z || !new_r || !new_g || !new_b || !new_a || !new_type) {
        log_to_file("Error: Memory allocation failed in resize_primitives");
        exit(1);
    }

    // Initialize new memory
    if (new_capacity > primitive_capacity) {
        memset(new_shape + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(char[16]));
        memset(new_x + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(float));
        memset(new_y + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(float));
        memset(new_z + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(float));
        memset(new_r + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(float));
        memset(new_g + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(float));
        memset(new_b + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(float));
        memset(new_a + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(float));
        memset(new_type + primitive_capacity, 0, (new_capacity - primitive_capacity) * sizeof(char[16]));
    }

    shape = new_shape;
    x = new_x;
    y = new_y;
    z = new_z;
    r = new_r;
    g = new_g;
    b = new_b;
    a = new_a;
    type = new_type;
    primitive_capacity = new_capacity;
    char msg[256];
    snprintf(msg, sizeof(msg), "Resized primitive arrays to capacity %d", new_capacity);
    log_to_file(msg);
}

void read_rgb() {
    num_pixels = 0;
    num_primitives = 0;

    FILE *fp = fopen(rgb_path, "r");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open rgb file %s", rgb_path);
        log_to_file(msg);
        return;
    }

    char line[MAX_LINE];
    float scale = 64.0f; // Reverse read_primitive.c's projection
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_LINE];
        if (sscanf(line, "%s", key) != 1) continue;
        if (strcmp(key, "pixel") == 0) {
            int px, py;
            unsigned char pr, pg, pb;
            if (sscanf(line, "pixel %d %d %hhu %hhu %hhu", &px, &py, &pr, &pg, &pb) == 5) {
                if (num_pixels < MAX_PIXELS) {
                    pixel_x[num_pixels] = px;
                    pixel_y[num_pixels] = py;
                    pixel_r[num_pixels] = pr;
                    pixel_g[num_pixels] = pg;
                    pixel_b[num_pixels] = pb;
                    num_pixels++;
                }
                // Reconstruct primitive (group by color and position)
                if (num_primitives >= primitive_capacity) {
                    resize_primitives();
                    if (num_primitives >= primitive_capacity) continue; // Skip if max reached
                }
                float wx = (px - 320.0f) / scale + player_x;
                float wy = (py - 240.0f) / scale + player_y;
                float wz = 0.0f;
                char *wshape = "cube";
                char *wtype = "block";
                float wa = 1.0f;
                if (pr == 0 && pg == 0 && pb == 255) { // Player (blue)
                    wshape = "sphere";
                    wtype = "player";
                    wx = player_x; // Snap to known player position
                    wy = player_y;
                    wz = player_z;
                } else if (pr == 255 && pg == 255 && pb == 0) { // Cursor (yellow)
                    wtype = "cursor";
                    wx = cursor_x;
                    wy = cursor_y;
                    wz = cursor_z;
                    wa = 0.3f;
                } else if (pr == 128 && pg == 128 && pb == 128) { // Block (gray)
                    wz = 0.5f;
                } else if (pr == 255 && pg == 255 && pb == 255) { // Text pixels
                    continue; // Skip text pixels for 3D rendering
                } else { // Piece (e.g., green)
                    wshape = "sphere";
                    wtype = "piece";
                    wz = 0.5f;
                }
                // Check for duplicate primitive
                int duplicate = 0;
                for (int i = 0; i < num_primitives; i++) {
                    if (strcmp(type[i], wtype) == 0 && fabs(x[i] - wx) < 0.1 && fabs(y[i] - wy) < 0.1) {
                        duplicate = 1;
                        break;
                    }
                }
                if (!duplicate) {
                    strncpy(shape[num_primitives], wshape, 15);
                    x[num_primitives] = wx;
                    y[num_primitives] = wy;
                    z[num_primitives] = wz;
                    r[num_primitives] = pr / 255.0f;
                    g[num_primitives] = pg / 255.0f;
                    b[num_primitives] = pb / 255.0f;
                    a[num_primitives] = wa;
                    strncpy(type[num_primitives], wtype, 15);
                    num_primitives++;
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Reconstructed primitive: %s at (%.1f, %.1f, %.1f), type=%s", wshape, wx, wy, wz, wtype);
                    log_to_file(msg);
                }
            }
        }
        // Skip text lines
    }
    fclose(fp);

    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded %d pixels, reconstructed %d primitives from %s", num_pixels, num_primitives, rgb_path);
    log_to_file(msg);
}

int check_file_changes() {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    long elapsed_ns = (current_time.tv_sec - last_check_time.tv_sec) * 1000000000L +
                      (current_time.tv_nsec - last_check_time.tv_nsec);
    if (elapsed_ns < FILE_CHECK_INTERVAL * 1000) {
        return 0;
    }
    last_check_time = current_time;

    struct stat stat_buf;
    if (stat(rgb_path, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_rgb_time) {
            last_rgb_time = stat_buf.st_mtime;
            log_to_file("RGB file changed, reloading");
            return 1;
        }
    }
    return 0;
}

void init() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat light_pos[] = {1.0f, 1.0f, 1.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glEnable(GL_COLOR_MATERIAL);
}

void draw_mini_map() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-5.0, 5.0, -5.0, 5.0, -10.0, 10.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glTranslatef(-cursor_x, -cursor_y, 0.0f);

    for (int i = 0; i < num_primitives; i++) {
        if (strcmp(type[i], "text") == 0) continue;
        glColor4f(r[i], g[i], b[i], a[i]);
        glBegin(GL_QUADS);
        float size = (strcmp(shape[i], "cube") == 0) ? 0.5f : 0.25f;
        glVertex2f(x[i] - size, y[i] - size);
        glVertex2f(x[i] + size, y[i] - size);
        glVertex2f(x[i] + size, y[i] + size);
        glVertex2f(x[i] - size, y[i] + size);
        glEnd();
    }

    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(player_x - 0.25f, player_y - 0.25f);
    glVertex2f(player_x + 0.25f, player_y - 0.25f);
    glVertex2f(player_x + 0.25f, player_y + 0.25f);
    glVertex2f(player_x - 0.25f, player_y + 0.25f);
    glEnd();

    glColor4f(1.0f, 1.0f, 0.0f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(cursor_x - 0.5f, cursor_y - 0.5f);
    glVertex2f(cursor_x + 0.5f, cursor_y - 0.5f);
    glVertex2f(cursor_x + 0.5f, cursor_y + 0.5f);
    glVertex2f(cursor_x - 0.5f, cursor_y + 0.5f);
    glEnd();

    glEnable(GL_LIGHTING);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 4.0 / 3.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (orbit_mode) {
        float orbit_radius = 8.0f;
        cam_x = player_x + orbit_radius * cos(orbit_angle);
        cam_z = player_z + orbit_radius * sin(orbit_angle);
        cam_y = player_y + 2.0f;
    }
    gluLookAt(cam_x, cam_y, cam_z, player_x, player_y, player_z, 0.0, 1.0, 0.0);

    // Render primitives
    for (int i = 0; i < num_primitives; i++) {
        if (strcmp(type[i], "text") == 0) continue;
        glPushMatrix();
        glTranslatef(x[i], y[i], z[i]);
        glColor4f(r[i], g[i], b[i], a[i]);
        if (strcmp(shape[i], "sphere") == 0) {
            glutSolidSphere(0.5, 16, 16);
        } else {
            glutSolidCube(1.0);
        }
        glPopMatrix();
    }

    // Mini-map
    glPushMatrix();
    glTranslatef(2.0f, 2.0f, -5.0f);
    glScalef(0.2f, 0.2f, 0.2f);
    draw_mini_map();
    glPopMatrix();

    glutSwapBuffers();
}

void animate(int value) {
    if (orbit_mode) {
        orbit_angle += 0.05f;
        if (orbit_angle > 2 * 3.14159f) orbit_angle -= 2 * 3.14159f;
    }
    if (check_file_changes()) {
        read_rgb();
    }
    glutPostRedisplay();
    glutTimerFunc(1000 / TARGET_FPS, animate, 0);
}

void keyboard(unsigned char key, int x, int y) {
    float move_speed = 0.1f;
    float rot_speed = 0.05f;
    if (key == 'w') cam_z -= move_speed;
    else if (key == 's') cam_z += move_speed;
    else if (key == 'a') cam_x -= move_speed;
    else if (key == 'd') cam_x += move_speed;
    else if (key == 'q') cam_y += move_speed;
    else if (key == 'e') cam_y -= move_speed;
    else if (key == 'i') cam_y += rot_speed;
    else if (key == 'k') cam_y -= rot_speed;
    else if (key == 'j') cam_x -= rot_speed;
    else if (key == 'l') cam_x += rot_speed;
    else if (key == 'o') orbit_mode = !orbit_mode;

    char msg[256];
    snprintf(msg, sizeof(msg), "Key pressed: %c, Camera: (%.1f, %.1f, %.1f)", key, cam_x, cam_y, cam_z);
    log_to_file(msg);
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(640, 480);
    glutCreateWindow("Wraith Racer");
    init();
    read_locations();
    init_primitives(); // Initialize arrays
    read_rgb();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutReshapeFunc(reshape);
    glutTimerFunc(0, animate, 0);
    glutMainLoop();

    free(shape);
    free(x);
    free(y);
    free(z);
    free(r);
    free(g);
    free(b);
    free(a);
    free(type);
    return 0;
}

