#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define WIDTH 1024
#define HEIGHT 640

#define WRAITH_KEYBOARD_HISTORY "pieces/keyboard/history.txt"
#define WRAITH_PROJECT_HISTORY "projects/wraith-alpha/session/history.txt"
#define WRAITH_FOCUS_LOCK "projects/wraith-alpha/session/input_focus.lock"
#define WRAITH_FRAME_SOURCE "projects/wraith-alpha/session/rgb/current_frame.rgba32"
#define WRAITH_FRAME_TRIGGER "projects/wraith-alpha/session/rgb/rgb_frame_changed.txt"

GLuint texture_id;
unsigned char *frame_buffer = NULL;
off_t last_pulse_size = 0;
static volatile sig_atomic_t g_shutdown_requested = 0;
static int g_mouse_button = -1;

static void write_timestamp(FILE *f) {
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char stamp[64];

    if (!f) {
        return;
    }

    if (tm_now) {
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm_now);
    } else {
        snprintf(stamp, sizeof(stamp), "time");
    }

    fprintf(f, "[%s] ", stamp);
}

static void update_focus_lock(void) {
    FILE *f = fopen(WRAITH_FOCUS_LOCK, "w");
    if (!f) {
        return;
    }
    fprintf(f, "owner=wraith_gl\n");
    fprintf(f, "session=wraith-alpha\n");
    fclose(f);
}

static void remove_focus_lock(void) {
    remove(WRAITH_FOCUS_LOCK);
}

static void cleanup_runtime(void) {
    remove_focus_lock();
}

static void handle_signal(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

static void append_keyboard_event(int code) {
    FILE *f = fopen(WRAITH_KEYBOARD_HISTORY, "a");
    if (!f) {
        return;
    }
    write_timestamp(f);
    fprintf(f, "KEY_PRESSED: %d\n", code);
    fclose(f);
}

static void append_project_command(const char *command) {
    FILE *f = fopen(WRAITH_PROJECT_HISTORY, "a");
    if (!f || !command) {
        if (f) {
            fclose(f);
        }
        return;
    }
    write_timestamp(f);
    fprintf(f, "COMMAND: %s\n", command);
    fclose(f);
}

static int map_special_key(int key) {
    if (key == GLUT_KEY_LEFT) {
        return 1000;
    }
    if (key == GLUT_KEY_RIGHT) {
        return 1001;
    }
    if (key == GLUT_KEY_UP) {
        return 1002;
    }
    if (key == GLUT_KEY_DOWN) {
        return 1003;
    }
    return 0;
}

static void emit_mouse_event(int button, int x, int y) {
    char command[64];
    snprintf(command, sizeof(command), "MOUSE_MOVE %d %d %d", button, x, y);
    append_project_command(command);
}

void load_texture() {
    FILE *f = fopen(WRAITH_FRAME_SOURCE, "rb");
    if (!f) return;

    if (!frame_buffer) frame_buffer = malloc(WIDTH * HEIGHT * 4);
    size_t read = fread(frame_buffer, 1, WIDTH * HEIGHT * 4, f);
    (void)read;
    fclose(f);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();

    glutSwapBuffers();
}

void timer(int value) {
    struct stat st;
    if (g_shutdown_requested) {
        cleanup_runtime();
        exit(0);
    }
    if (stat(WRAITH_FRAME_TRIGGER, &st) == 0) {
        if (st.st_size != last_pulse_size) {
            last_pulse_size = st.st_size;
            load_texture();
            glutPostRedisplay();
        }
    }
    glutTimerFunc(16, timer, 0);
}

void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;

    if (key == 3) {
        g_shutdown_requested = 1;
        return;
    }

    if (key == 10 || key == 13) {
        append_keyboard_event(13);
        return;
    }

    if (key == 8 || key == 127) {
        append_keyboard_event(key);
        return;
    }

    append_keyboard_event((int)key);
}

void special_keyboard(int key, int x, int y) {
    (void)x;
    (void)y;

    int mapped = map_special_key(key);
    if (mapped > 0) {
        append_keyboard_event(mapped);
    }
}

void mouse(int button, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        g_mouse_button = button;
        emit_mouse_event(button, x, y);
        return;
    }

    if (state == GLUT_UP) {
        emit_mouse_event(button, x, y);
        if (g_mouse_button == button) {
            g_mouse_button = -1;
        }
    }
}

void motion(int x, int y) {
    emit_mouse_event(g_mouse_button, x, y);
}

int main(int argc, char** argv) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(cleanup_runtime);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Wraith Alpha RGB Mirror");

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutTimerFunc(16, timer, 0);

    update_focus_lock();

    struct stat st;
    if (stat(WRAITH_FRAME_TRIGGER, &st) == 0)
        last_pulse_size = st.st_size;

    load_texture();
    glutMainLoop();
    return 0;
}
