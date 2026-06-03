
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
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define CAMERA_DISTANCE 10.0f
#define CAMERA_HEIGHT 5.0f

// Primitive structure
typedef struct {
    char shape[16]; // cube or sphere
    float x, y, z;  // World coordinates
    float r, g, b, a; // Color (0.0-1.0)
    char type[16];  // block, player, piece, cursor
} Primitive;

// Text structure for HUD
typedef struct {
    char text[256];
} Text;

// Global variables
Primitive *primitives = NULL;
int num_primitives = 0;
int primitive_capacity = 0;
Text *texts = NULL;
int num_texts = 0;
int text_capacity = 0;
float player_x = 0.0f, player_y = 0.0f, player_z = 0.0f;
float cam_x = 0.0f, cam_y = 0.0f, cam_z = 0.0f;
float cam_yaw = 0.0f, cam_pitch = 30.0f;
char primitive_path[MAX_PATH] = "./primitive.txt";
char log_path[MAX_PATH] = "log.txt";
time_t last_primitive_time = 0;

// Function declarations
void log_to_file(const char *message);
void read_locations();
void resize_primitives();
void resize_texts();
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

void resize_primitives() {
    if (num_primitives >= primitive_capacity) {
        int new_capacity = primitive_capacity == 0 ? 100 : primitive_capacity * 2;
        Primitive *new_primitives = realloc(primitives, new_capacity * sizeof(Primitive));
        if (!new_primitives) {
            log_to_file("Error: Memory allocation failed in resize_primitives");
            exit(1);
        }
        primitives = new_primitives;
        primitive_capacity = new_capacity;
    }
}

void resize_texts() {
    if (num_texts >= text_capacity) {
        int new_capacity = text_capacity == 0 ? 10 : text_capacity * 2;
        Text *new_texts = realloc(texts, new_capacity * sizeof(Text));
        if (!new_texts) {
            log_to_file("Error: Memory allocation failed in resize_texts");
            exit(1);
        }
        texts = new_texts;
        text_capacity = new_capacity;
    }
}

void read_primitive() {
    // Clear previous primitives and texts
    free(primitives);
    free(texts);
    primitives = NULL;
    texts = NULL;
    num_primitives = 0;
    primitive_capacity = 0;
    num_texts = 0;
    text_capacity = 0;

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
            char shape[16], type[16];
            float x, y, z, r, g, b, a;
            if (sscanf(line, "primitive %15s %f %f %f %f %f %f %f type=%15s",
                       shape, &x, &y, &z, &r, &g, &b, &a, type) == 9) {
                if (strcmp(type, "player") == 0) {
                    player_x = x;
                    player_y = y;
                    player_z = z;
                }
                if (num_primitives < MAX_PRIMITIVES) {
                    resize_primitives();
                    Primitive *p = &primitives[num_primitives];
                    strncpy(p->shape, shape, 15);
                    p->shape[15] = '\0';
                    p->x = x;
                    p->y = y;
                    p->z = z;
                    p->r = r;
                    p->g = g;
                    p->b = b;
                    p->a = a;
                    strncpy(p->type, type, 15);
                    p->type[15] = '\0';
                    num_primitives++;
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Loaded primitive: %s at (%.1f, %.1f, %.1f), color=(%.1f, %.1f, %.1f, %.1f), type=%s",
                             shape, x, y, z, r, g, b, a, type);
                    log_to_file(msg);
                }
            }
        } else if (strcmp(key, "text") == 0) {
            char text_content[256];
            if (sscanf(line, "text %255[^\n]", text_content) == 1) {
                resize_texts();
                strncpy(texts[num_texts].text, text_content, 255);
                texts[num_texts].text[255] = '\0';
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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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
        for (char *c = texts[i].text; *c; c++) {
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

    // Orbit camera around player
    float cam_rad = CAMERA_DISTANCE;
    float cam_y = cam_rad * sin(cam_pitch * 3.14159f / 180.0f);
    float cam_horiz = cam_rad * cos(cam_pitch * 3.14159f / 180.0f);
    cam_x = player_x + cam_horiz * cos(cam_yaw * 3.14159f / 180.0f);
    cam_z = player_z + cam_horiz * sin(cam_yaw * 3.14159f / 180.0f);
    cam_y = player_y + cam_y;
    gluLookAt(cam_x, cam_y, cam_z, player_x, player_y, player_z, 0.0f, 1.0f, 0.0f);

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
            Primitive *p = &primitives[i];
            if (strcmp(p->type, target_type) != 0) continue;

            glPushMatrix();
            glTranslatef(p->x, p->y, p->z);
            glColor3f(p->r, p->g, p->b);
            float mat_diffuse[] = {p->r, p->g, p->b, p->a};
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
            if (strcmp(p->shape, "cube") == 0) {
                glutSolidCube(1.0f);
            } else if (strcmp(p->shape, "sphere") == 0) {
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
        case 27: // Escape to exit
            exit(0);
    }
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
    free(primitives);
    free(texts);
    return 0;
}

