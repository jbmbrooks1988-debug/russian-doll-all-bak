#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <GL/glut.h>

#define MAX_LINE 256
#define MAX_PATH 256
#define GRID_WIDTH 640
#define GRID_HEIGHT 480

// Pixel structure
typedef struct {
    unsigned char r, g, b; // 0-255
} Pixel;

// Global variables
Pixel pixel_buffer[GRID_HEIGHT][GRID_WIDTH];
GLuint texture_id = 0;
char rgb_path[MAX_PATH] = "./rgb.txt";
char log_path[MAX_PATH] = "log.txt";
time_t last_rgb_time = 0;
int window_width = GRID_WIDTH;
int window_height = GRID_HEIGHT;

// Function declarations
void log_to_file(const char *message);
void read_locations();
void read_rgb_file();
void init_opengl();
void display();
void animate(int value);
int check_file_changes();

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

void read_rgb_file() {
    // Clear pixel buffer
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            pixel_buffer[y][x].r = 0;
            pixel_buffer[y][x].g = 0;
            pixel_buffer[y][x].b = 0;
        }
    }

    FILE *fp = fopen(rgb_path, "r");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open rgb file %s", rgb_path);
        log_to_file(msg);
        return;
    }

    char line[MAX_LINE];
    int pixel_count = 0;
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        int x, y, r, g, b;
        if (sscanf(line, "pixel %d %d %d %d %d", &x, &y, &r, &g, &b) == 5) {
            if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
                pixel_buffer[y][x].r = (unsigned char)r;
                pixel_buffer[y][x].g = (unsigned char)g;
                pixel_buffer[y][x].b = (unsigned char)b;
                pixel_count++;
            }
        }
    }
    fclose(fp);

    // Update texture
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GRID_WIDTH, GRID_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel_buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded %d pixels from %s", pixel_count, rgb_path);
    log_to_file(msg);
}

void init_opengl() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Initialize texture with black
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            pixel_buffer[y][x].r = 0;
            pixel_buffer[y][x].g = 0;
            pixel_buffer[y][x].b = 0;
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GRID_WIDTH, GRID_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel_buffer);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Check for file changes
    if (check_file_changes()) {
        read_rgb_file();
    }

    // Render texture
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, window_height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(window_width, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(window_width, window_height);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, window_height);
    glEnd();

/* Point Cloud Mode (alternative, uncomment to use)
    glPointSize(1.0f);
    glBegin(GL_POINTS);
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (pixel_buffer[y][x].r != 0 || pixel_buffer[y][x].g != 0 || pixel_buffer[y][x].b != 0) {
                glColor3ub(pixel_buffer[y][x].r, pixel_buffer[y][x].g, pixel_buffer[y][x].b);
                glVertex2f(x, y);
            }
        }
    }
    glEnd();
*/

    glutSwapBuffers();
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
}

void animate(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, animate, 0); // ~60 FPS
}

int check_file_changes() {
    struct stat stat_buf;
    if (stat(rgb_path, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_rgb_time) {
            last_rgb_time = stat_buf.st_mtime;
            log_to_file("RGB file changed, reloading");
            return 1;
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat rgb file %s", rgb_path);
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
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(GRID_WIDTH, GRID_HEIGHT);
    glutCreateWindow("RGB Display");
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutTimerFunc(0, animate, 0);

    // Initialize OpenGL and read initial data
    read_locations();
    init_opengl();
    read_rgb_file();

    // Start main loop
    glutMainLoop();

    // Cleanup (unreachable due to glutMainLoop)
    glDeleteTextures(1, &texture_id);
    return 0;
}
