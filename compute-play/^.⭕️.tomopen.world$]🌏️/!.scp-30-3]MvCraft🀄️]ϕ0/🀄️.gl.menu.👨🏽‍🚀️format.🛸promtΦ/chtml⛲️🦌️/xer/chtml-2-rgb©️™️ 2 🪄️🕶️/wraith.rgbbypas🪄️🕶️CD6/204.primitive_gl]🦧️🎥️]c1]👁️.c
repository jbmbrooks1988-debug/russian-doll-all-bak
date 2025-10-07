
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <GL/glut.h>
#include <float.h>

#define MAX_LINE 256
#define MAX_PATH 256
#define MAX_PRIMITIVES 10000
#define MAX_TEXTS 100
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define CAMERA_DISTANCE 10.0f
#define CAMERA_HEIGHT 5.0f

// Global variables
char (*shape)[16] = NULL;
float *x = NULL, *y = NULL, *z = NULL;
float *r = NULL, *g = NULL, *b = NULL, *a = NULL;
char (*type)[16] = NULL;
int num_primitives = 0;
int primitive_capacity = 0;
char (*text_lines)[256] = NULL;
int num_texts = 0;
int text_capacity = 0;
float player_x = 0.0f, player_y = 0.0f, player_z = 0.0f;
float cam_x = 0.0f, cam_y = 5.0f, cam_z = 10.0f; // Start away from origin
float cam_yaw = 0.0f, cam_pitch = 30.0f;
int orbit_mode = 0; // 0: free, 1: orbit player
char primitive_path[MAX_PATH] = "./primitive.txt";
char log_path[MAX_PATH] = "log.txt";
time_t last_primitive_time = 0;

// Function declarations
void log_to_file(const char *message);
void read_locations();
void resize_arrays();
void resize_text_arrays();
void read_primitive();
void init_opengl();
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void animate(int value);
int check_file_changes();
void render_hud();

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
        if (strcmp(key, "primitive") == 0) {
            strncpy(primitive_path, value, MAX_PATH - 1);
            primitive_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "log") == 0) {
            strncpy(log_path, value, MAX_PATH - 1);
            log_path[MAX_PATH - 1] = '\0';
        }
    }
    fclose(fp);
    char msg[MAX_PATH * 2];
    snprintf(msg, sizeof(msg), "Loaded paths: primitive=%s, log=%s", primitive_path, log_path);
    log_to_file(msg);
}

void resize_arrays() {
    int new_capacity = primitive_capacity == 0 ? 100 : primitive_capacity * 2;
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
        log_to_file("Error: Memory allocation failed in resize_arrays");
        exit(1);
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
}

void resize_text_arrays() {
    int new_capacity = text_capacity == 0 ? 10 : text_capacity * 2;
    char (*new_texts)[256] = realloc(text_lines, new_capacity * sizeof(char[256]));
    if (!new_texts) {
        log_to_file("Error: Memory allocation failed in resize_text_arrays");
        exit(1);
    }
    text_lines = new_texts;
    text_capacity = new_capacity;
}

void read_primitive() {
    num_primitives = 0;
    num_texts = 0;

    FILE *fp = fopen(primitive_path, "r");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open primitive file %s", primitive_path);
        log_to_file(msg);
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_LINE];
        if (sscanf(line, "%s", key) != 1) continue;
        if (strcmp(key, "primitive") == 0) {
            char temp_shape[16], temp_type[16];
            float temp_x, temp_y, temp_z, temp_r, temp_g, temp_b, temp_a;
            if (sscanf(line, "primitive %15s %f %f %f %f %f %f %f type=%15s",
                       temp_shape, &temp_x, &temp_y, &temp_z, &temp_r, &temp_g, &temp_b, &temp_a, temp_type) == 9) {
                if (strcmp(temp_type, "player") == 0) {
                    player_x = temp_x;
                    player_y = temp_y;
                    player_z = temp_z;
                }
                if (num_primitives < MAX_PRIMITIVES) {
                    if (num_primitives >= primitive_capacity) {
                        resize_arrays();
                    }
                    strncpy(shape[num_primitives], temp_shape, 15);
                    shape[num_primitives][15] = '\0';
                    x[num_primitives] = temp_x;
                    y[num_primitives] = temp_y;
                    z[num_primitives] = temp_z;
                    r[num_primitives] = temp_r;
                    g[num_primitives] = temp_g;
                    b[num_primitives] = temp_b;
                    a[num_primitives] = temp_a;
                    strncpy(type[num_primitives], temp_type, 15);
                    type[num_primitives][15] = '\0';
                    num_primitives++;
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Loaded primitive: %s at (%.1f, %.1f, %.1f), color=(%.1f, %.1f, %.1f, %.1f), type=%s",
                             temp_shape, temp_x, temp_y, temp_z, temp_r, temp_g, temp_b, temp_a, temp_type);
                    log_to_file(msg);
                }
            }
        } else if (strcmp(key, "text") == 0) {
            char text_content[256];
            if (sscanf(line, "text %255[^\n]", text_content) == 1) {
                if (num_texts >= text_capacity) {
                    resize_text_arrays();
                }
                strncpy(text_lines[num_texts], text_content, 255);
                text_lines[num_texts][255] = '\0';
                num_texts++;
                char msg[256];
                snprintf(msg, sizeof(msg), "Loaded text: %s", text_content);
                log_to_file(msg);
            }
        }
    }
    fclose(fp);
    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded %d primitives and %d texts from %s", num_primitives, num_texts, primitive_path);
    log_to_file(msg);
}

void init_opengl() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    float light_pos[] = {0.0f, 0.0f, 10.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}

void render_hud() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < num_texts; i++) {
        glRasterPos2i(10, 20 + i * 20);
        for (char *c = text_lines[i]; *c; c++) {
            glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
        }
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Check for file changes
    if (check_file_changes()) {
        read_primitive();
    }

    // Set up perspective camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Camera position
    float look_x = player_x, look_y = player_y, look_z = player_z;
    if (orbit_mode) {
        float cam_rad = CAMERA_DISTANCE;
        float cam_y_offset = cam_rad * sin(cam_pitch * 3.14159f / 180.0f);
        float cam_horiz = cam_rad * cos(cam_pitch * 3.14159f / 180.0f);
        cam_x = player_x + cam_horiz * cos(cam_yaw * 3.14159f / 180.0f);
        cam_z = player_z + cam_horiz * sin(cam_yaw * 3.14159f / 180.0f);
        cam_y = player_y + cam_y_offset;
    }
    gluLookAt(cam_x, cam_y, cam_z, look_x, look_y, look_z, 0.0f, 1.0f, 0.0f);

    // Log camera position
    char msg[256];
    snprintf(msg, sizeof(msg), "Camera: pos=(%.1f, %.1f, %.1f), yaw=%.1f, pitch=%.1f, mode=%s",
             cam_x, cam_y, cam_z, cam_yaw, cam_pitch, orbit_mode ? "orbit" : "free");
    log_to_file(msg);

    // Render primitives in order: blocks, pieces, player, cursor
    for (int render_pass = 0; render_pass < 4; render_pass++) {
        const char *target_type;
        switch (render_pass) {
            case 0: target_type = "block"; break;
            case 1: target_type = "piece"; break;
            case 2: target_type = "player"; break;
            case 3: target_type = "cursor"; break;
            default: continue;
        }

        for (int i = 0; i < num_primitives; i++) {
            if (strcmp(type[i], target_type) != 0) continue;

            glPushMatrix();
            glTranslatef(x[i], y[i], z[i]);
            glColor3f(r[i], g[i], b[i]);
            float mat_diffuse[] = {r[i], g[i], b[i], a[i]};
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
            if (strcmp(shape[i], "cube") == 0) {
                glutSolidCube(1.0f);
            } else if (strcmp(shape[i], "sphere") == 0) {
                glutSolidSphere(0.5f, 16, 16);
            }
            glPopMatrix();
        }
    }

    // Render HUD (text)
    render_hud();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

void keyboard(unsigned char key, int x, int y) {
    float yaw_rad = cam_yaw * 3.14159f / 180.0f;
    float speed = 0.5f;
    float angle_speed = 5.0f;
    switch (key) {
        case 'w': // Move camera forward
            cam_x += speed * cos(yaw_rad);
            cam_z += speed * sin(yaw_rad);
            break;
        case 's': // Move camera backward
            cam_x -= speed * cos(yaw_rad);
            cam_z -= speed * sin(yaw_rad);
            break;
        case 'a': // Strafe left
            cam_x += speed * sin(yaw_rad);
            cam_z -= speed * cos(yaw_rad);
            break;
        case 'd': // Strafe right
            cam_x -= speed * sin(yaw_rad);
            cam_z += speed * cos(yaw_rad);
            break;
        case 'q': // Move up
            cam_y += speed;
            break;
        case 'e': // Move down
            cam_y -= speed;
            break;
        case 'j': // Rotate left
            cam_yaw -= angle_speed;
            break;
        case 'l': // Rotate right
            cam_yaw += angle_speed;
            break;
        case 'i': // Pitch up
            cam_pitch = fmin(cam_pitch + angle_speed, 89.0f);
            break;
        case 'k': // Pitch down
            cam_pitch = fmax(cam_pitch - angle_speed, -89.0f);
            break;
        case 'o': // Toggle orbit/free mode
            orbit_mode = !orbit_mode;
            break;
        case 27: // Escape to exit
            exit(0);
    }

    // Log camera update
    char msg[256];
    snprintf(msg, sizeof(msg), "Key %c: Camera pos=(%.1f, %.1f, %.1f), yaw=%.1f, pitch=%.1f, mode=%s",
             key, cam_x, cam_y, cam_z, cam_yaw, cam_pitch, orbit_mode ? "orbit" : "free");
    log_to_file(msg);

    glutPostRedisplay(); // Ensure redraw after key press
}

void animate(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, animate, 0); // ~60 FPS
}

int check_file_changes() {
    struct stat stat_buf;
    if (stat(primitive_path, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_primitive_time) {
            last_primitive_time = stat_buf.st_mtime;
            log_to_file("Primitive file changed, reloading");
            return 1;
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat primitive file %s", primitive_path);
        log_to_file(msg);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        log_to_file("Error: No arguments allowed");
        return 1;
    }

    // Initialize arrays
    primitive_capacity = 0;
    text_capacity = 0;
    resize_arrays();
    resize_text_arrays();

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("Primitive GL Display");
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, animate, 0);

    // Initialize OpenGL and read initial data
    read_locations();
    init_opengl();
    read_primitive();

    // Start main loop
    glutMainLoop();

    // Cleanup (unreachable due to glutMainLoop)
    free(shape);
    free(x);
    free(y);
    free(z);
    free(r);
    free(g);
    free(b);
    free(a);
    free(type);
    free(text_lines);
    return 0;
}
