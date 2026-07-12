#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define WIDTH 1024
#define HEIGHT 640
#define WRAITH_KEYBOARD_HISTORY "pieces/keyboard/history.txt"
#define WRAITH_PROJECT_HISTORY "projects/wraith-alpha/session/history.txt"
#define WRAITH_SEMANTIC_META "pieces/display/current_frame.meta.pdl"
#define WRAITH_SEMANTIC_OBJECTS "pieces/display/current_frame.objects.pdl"
#define WRAITH_MOUSE_OFFSET "#.mouse-offset.txt"
#define WRAITH_FOCUS_LOCK "projects/wraith-alpha/session/input_focus.lock"
#define WRAITH_FRAME_SOURCE "projects/wraith-alpha/session/rgb/current_frame.rgba32"
#define WRAITH_FRAME_TRIGGER "projects/wraith-alpha/session/rgb/rgb_frame_changed.txt"
#define WRAITH_UI_STATE "projects/wraith-alpha/session/desktop_ui_state.txt"
#define WRAITH_GL_INPUT_RECEIPT "projects/wraith-alpha/session/rgb/gl_input.receipt.pdl"
#define WRAITH_GL_DISPLAY_RECEIPT "projects/wraith-alpha/session/rgb/gl_display.receipt.pdl"

static GLuint texture_id;
static unsigned char *frame_buffer = NULL;
static off_t last_pulse_size = 0;
static volatile sig_atomic_t g_shutdown_requested = 0;
static int g_mouse_button = -1;
static char g_pending_action[256] = "";
static int g_pending_action_set = 0;
static int g_window_w = WIDTH;
static int g_window_h = HEIGHT;
static int g_texture_view_x = 0;
static int g_texture_view_y = 0;
static int g_texture_view_w = WIDTH;
static int g_texture_view_h = HEIGHT;
static unsigned long long g_loaded_frame_checksum = 0;
static size_t g_loaded_frame_bytes = 0;
static int g_loaded_frame_partial = 0;

static unsigned long long checksum_buffer(const unsigned char *buffer, size_t len) {
    unsigned long long hash = 1469598103934665603ULL;
    size_t i;

    if (!buffer) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        hash ^= (unsigned long long)buffer[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

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
        if (f) fclose(f);
        return;
    }
    write_timestamp(f);
    fprintf(f, "COMMAND: %s\n", command);
    fclose(f);
}

static void load_mouse_offset(int *offset_x, int *offset_y) {
    FILE *f = fopen(WRAITH_MOUSE_OFFSET, "r");
    char line[128];
    int x = 0;
    int y = 0;

    if (!offset_x || !offset_y) {
        return;
    }
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "OFFSET_X=%d", &x) == 1) {
                continue;
            }
            if (sscanf(line, "OFFSET_Y=%d", &y) == 1) {
                continue;
            }
        }
        fclose(f);
    }

    *offset_x = x;
    *offset_y = y;
}

static void clear_pending_action(void) {
    g_pending_action[0] = '\0';
    g_pending_action_set = 0;
}

static void set_pending_action(const char *command) {
    if (!command || command[0] == '\0') {
        clear_pending_action();
        return;
    }
    strncpy(g_pending_action, command, sizeof(g_pending_action) - 1);
    g_pending_action[sizeof(g_pending_action) - 1] = '\0';
    g_pending_action_set = 1;
}

static void activate_pending_action(void) {
    if (!g_pending_action_set || g_pending_action[0] == '\0') {
        return;
    }
    append_project_command(g_pending_action);
    clear_pending_action();
}

static int kvp_value(const char *line, const char *key, char *out, size_t out_sz) {
    const char *start, *end;
    size_t len;
    char needle[64];
    if (!line || !key || !out || out_sz == 0) return 0;
    snprintf(needle, sizeof(needle), "%s=", key);
    start = strstr(line, needle);
    if (!start) return 0;
    start += strlen(needle);
    end = start;
    while (*end != '\0' && !isspace((unsigned char)*end)) end++;
    len = (size_t)(end - start);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static int hit_test_semantic_action(int px, int py, char *out, size_t out_sz) {
    FILE *meta = fopen(WRAITH_SEMANTIC_META, "r");
    FILE *objects = fopen(WRAITH_SEMANTIC_OBJECTS, "r");
    char line[2048];
    int cell_w = 10;
    int cell_h = 18;
    int offset_x = 0;
    int offset_y = 0;
    int cell_x;
    int cell_y;
    int best_z = -1000000;
    int found = 0;

    if (out && out_sz > 0) {
        out[0] = '\0';
    }

    if (meta) {
        while (fgets(line, sizeof(line), meta)) {
            if (strstr(line, "FRAME | cell_width_px |")) {
                cell_w = atoi(strrchr(line, '|') + 1);
            } else if (strstr(line, "FRAME | cell_height_px |")) {
                cell_h = atoi(strrchr(line, '|') + 1);
            }
        }
        fclose(meta);
    }

    if (!objects) {
        return 0;
    }

    load_mouse_offset(&offset_x, &offset_y);
    cell_x = (px / (cell_w > 0 ? cell_w : 10)) + offset_x;
    cell_y = (py / (cell_h > 0 ? cell_h : 18)) + offset_y;

    while (fgets(line, sizeof(line), objects)) {
        char value[256];
        int x, y, w, h, z;
        char action[256] = "";

        if (strncmp(line, "OBJECT |", 8) != 0) {
            continue;
        }
        if (!kvp_value(line, "x", value, sizeof(value))) continue;
        x = atoi(value);
        if (!kvp_value(line, "y", value, sizeof(value))) continue;
        y = atoi(value);
        if (!kvp_value(line, "w", value, sizeof(value))) continue;
        w = atoi(value);
        if (!kvp_value(line, "h", value, sizeof(value))) continue;
        h = atoi(value);
        if (kvp_value(line, "z", value, sizeof(value))) {
            z = atoi(value);
        } else {
            z = 0;
        }
        if (kvp_value(line, "action", action, sizeof(action)) && action[0] != '\0') {
            /* already populated */
        } else if (kvp_value(line, "nav", value, sizeof(value)) && atoi(value) > 0) {
            snprintf(action, sizeof(action), "SET_ACTIVE:%d", atoi(value));
        }

        if (action[0] == '\0') {
            continue;
        }
        if (cell_x < x || cell_y < y || cell_x >= x + w || cell_y >= y + h) {
            continue;
        }
        if (z >= best_z) {
            best_z = z;
            if (out && out_sz > 0) {
                strncpy(out, action, out_sz - 1);
                out[out_sz - 1] = '\0';
            }
            found = 1;
        }
    }

    fclose(objects);
    return found;
}

static void update_texture_viewport(int win_w, int win_h) {
    double texture_aspect = (double)WIDTH / (double)HEIGHT;
    double window_aspect;

    if (win_w <= 0) win_w = WIDTH;
    if (win_h <= 0) win_h = HEIGHT;

    g_window_w = win_w;
    g_window_h = win_h;
    window_aspect = (double)win_w / (double)win_h;

    if (window_aspect > texture_aspect) {
        g_texture_view_h = win_h;
        g_texture_view_w = (int)((double)win_h * texture_aspect + 0.5);
        g_texture_view_x = (win_w - g_texture_view_w) / 2;
        g_texture_view_y = 0;
    } else {
        g_texture_view_w = win_w;
        g_texture_view_h = (int)((double)win_w / texture_aspect + 0.5);
        g_texture_view_x = 0;
        g_texture_view_y = (win_h - g_texture_view_h) / 2;
    }
}

static int map_window_mouse_to_texture(int x, int y, int *texture_x, int *texture_y) {
    int local_x = x - g_texture_view_x;
    int local_y = y - g_texture_view_y;

    if (g_texture_view_w <= 0 || g_texture_view_h <= 0) {
        return 0;
    }
    if (local_x < 0 || local_y < 0 || local_x >= g_texture_view_w || local_y >= g_texture_view_h) {
        return 0;
    }

    if (texture_x) {
        *texture_x = (local_x * WIDTH) / g_texture_view_w;
    }
    if (texture_y) {
        *texture_y = (local_y * HEIGHT) / g_texture_view_h;
    }
    return 1;
}

static void write_gl_input_receipt(const char *event, int button, int raw_x, int raw_y,
                                   int inside_texture, int texture_x, int texture_y) {
    FILE *f = fopen(WRAITH_GL_INPUT_RECEIPT, "w");
    time_t now = time(NULL);
    struct tm *tm_now;
    char stamp[64];

    if (!f) {
        return;
    }
    tm_now = gmtime(&now);
    if (tm_now) {
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", tm_now);
    } else {
        snprintf(stamp, sizeof(stamp), "unknown");
    }

    fprintf(f, "receipt_type=gl_input_transform\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "generated_at_iso_utc=%s\n", stamp);
    fprintf(f, "event=%s\n", event ? event : "unknown");
    fprintf(f, "button=%d\n", button);
    fprintf(f, "window_w=%d\n", g_window_w);
    fprintf(f, "window_h=%d\n", g_window_h);
    fprintf(f, "texture_width_px=%d\n", WIDTH);
    fprintf(f, "texture_height_px=%d\n", HEIGHT);
    fprintf(f, "texture_view_x=%d\n", g_texture_view_x);
    fprintf(f, "texture_view_y=%d\n", g_texture_view_y);
    fprintf(f, "texture_view_w=%d\n", g_texture_view_w);
    fprintf(f, "texture_view_h=%d\n", g_texture_view_h);
    fprintf(f, "raw_mouse_x=%d\n", raw_x);
    fprintf(f, "raw_mouse_y=%d\n", raw_y);
    fprintf(f, "inside_texture=%d\n", inside_texture);
    fprintf(f, "texture_mouse_x=%d\n", texture_x);
    fprintf(f, "texture_mouse_y=%d\n", texture_y);
    fclose(f);
}

static void write_gl_display_receipt(const char *event) {
    FILE *f = fopen(WRAITH_GL_DISPLAY_RECEIPT, "w");
    time_t now = time(NULL);
    struct tm *tm_now;
    char stamp[64];
    double scale_x = 0.0;
    double scale_y = 0.0;

    if (!f) {
        return;
    }
    tm_now = gmtime(&now);
    if (tm_now) {
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", tm_now);
    } else {
        snprintf(stamp, sizeof(stamp), "unknown");
    }
    if (WIDTH > 0) {
        scale_x = (double)g_texture_view_w / (double)WIDTH;
    }
    if (HEIGHT > 0) {
        scale_y = (double)g_texture_view_h / (double)HEIGHT;
    }

    fprintf(f, "receipt_type=gl_display_upload\n");
    fprintf(f, "generated_by=wraith_gl\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "generated_at_iso_utc=%s\n", stamp);
    fprintf(f, "event=%s\n", event ? event : "unknown");
    fprintf(f, "source_rgba32=%s\n", WRAITH_FRAME_SOURCE);
    fprintf(f, "texture_width_px=%d\n", WIDTH);
    fprintf(f, "texture_height_px=%d\n", HEIGHT);
    fprintf(f, "expected_rgba_bytes=%d\n", WIDTH * HEIGHT * 4);
    fprintf(f, "loaded_rgba_bytes=%lu\n", (unsigned long)g_loaded_frame_bytes);
    fprintf(f, "loaded_frame_partial=%d\n", g_loaded_frame_partial);
    fprintf(f, "loaded_rgba_checksum_fnv1a64=0x%016llX\n", g_loaded_frame_checksum);
    fprintf(f, "window_w=%d\n", g_window_w);
    fprintf(f, "window_h=%d\n", g_window_h);
    fprintf(f, "texture_view_x=%d\n", g_texture_view_x);
    fprintf(f, "texture_view_y=%d\n", g_texture_view_y);
    fprintf(f, "texture_view_w=%d\n", g_texture_view_w);
    fprintf(f, "texture_view_h=%d\n", g_texture_view_h);
    fprintf(f, "display_scale_x=%.6f\n", scale_x);
    fprintf(f, "display_scale_y=%.6f\n", scale_y);
    fprintf(f, "render_origin=top_left_texture_to_gl_viewport\n");
    fclose(f);
}

static int map_special_key(int key) {
    if (key == GLUT_KEY_LEFT) return 1000;
    if (key == GLUT_KEY_RIGHT) return 1001;
    if (key == GLUT_KEY_UP) return 1002;
    if (key == GLUT_KEY_DOWN) return 1003;
    return 0;
}

static void emit_mouse_event(int button, int x, int y) {
    char command[64];
    snprintf(command, sizeof(command), "MOUSE_MOVE %d %d %d", button, x, y);
    append_project_command(command);
}

static void load_texture(void) {
    FILE *f = fopen(WRAITH_FRAME_SOURCE, "rb");

    if (!frame_buffer) frame_buffer = malloc(WIDTH * HEIGHT * 4);
    if (!frame_buffer) {
        if (f) fclose(f);
        return;
    }

    if (!f) {
        memset(frame_buffer, 0, WIDTH * HEIGHT * 4);
        g_loaded_frame_bytes = 0;
        g_loaded_frame_partial = 1;
    } else {
        size_t bytes_read = fread(frame_buffer, 1, WIDTH * HEIGHT * 4, f);
        fclose(f);
        g_loaded_frame_bytes = bytes_read;
        g_loaded_frame_partial = (bytes_read < WIDTH * HEIGHT * 4);
        if (bytes_read < WIDTH * HEIGHT * 4) {
            memset(frame_buffer + bytes_read, 0, (WIDTH * HEIGHT * 4) - bytes_read);
        }
    }
    g_loaded_frame_checksum = checksum_buffer(frame_buffer, WIDTH * HEIGHT * 4);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);
    write_gl_display_receipt("texture_upload");
}

static void display(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(g_texture_view_x, g_window_h - g_texture_view_y - g_texture_view_h,
               g_texture_view_w, g_texture_view_h);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();

    glutSwapBuffers();
    write_gl_display_receipt("display_swap");
}

static void reshape(int width, int height) {
    update_texture_viewport(width, height);
    write_gl_display_receipt("reshape");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glutPostRedisplay();
}

static void timer(int value) {
    struct stat st;
    (void)value;

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

static void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;

    if (key == 3) {
        g_shutdown_requested = 1;
        return;
    }
    if (key == 10 || key == 13) {
        append_keyboard_event(13);
        activate_pending_action();
        return;
    }
    if (key == 8 || key == 127) {
        append_keyboard_event(key);
        return;
    }

    append_keyboard_event((int)key);
}

static void special_keyboard(int key, int x, int y) {
    int mapped;
    (void)x;
    (void)y;

    mapped = map_special_key(key);
    if (mapped > 0) {
        append_keyboard_event(mapped);
    }
}

static void mouse(int button, int state, int x, int y) {
    int texture_x = -1;
    int texture_y = -1;
    int inside_texture = map_window_mouse_to_texture(x, y, &texture_x, &texture_y);

    if (state == GLUT_DOWN) {
        write_gl_input_receipt("mouse_down", button, x, y, inside_texture, texture_x, texture_y);
        g_mouse_button = button;
        if (!inside_texture) {
            clear_pending_action();
            return;
        }
        emit_mouse_event(button, texture_x, texture_y);
        {
            char command[256];
            if (hit_test_semantic_action(texture_x, texture_y, command, sizeof(command))) {
                if (g_pending_action_set && strcmp(g_pending_action, command) == 0) {
                    activate_pending_action();
                } else {
                    set_pending_action(command);
                }
            }
        }
        return;
    }

    if (state == GLUT_UP) {
        write_gl_input_receipt("mouse_up", button, x, y, inside_texture, texture_x, texture_y);
        if (inside_texture) {
            emit_mouse_event(button, texture_x, texture_y);
        }
        if (g_mouse_button == button) {
            g_mouse_button = -1;
        }
    }
}

static void motion(int x, int y) {
    int texture_x = -1;
    int texture_y = -1;
    int inside_texture = map_window_mouse_to_texture(x, y, &texture_x, &texture_y);
    write_gl_input_receipt("mouse_motion", g_mouse_button, x, y, inside_texture, texture_x, texture_y);
    if (inside_texture) {
        emit_mouse_event(g_mouse_button, texture_x, texture_y);
    }
}

int main(int argc, char **argv) {
    struct stat st;

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
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutTimerFunc(16, timer, 0);
    /* 2026-07-12: without this, X11's auto-repeat mechanism generates
       synthetic KeyRelease+KeyPress pairs for held keys (and, at
       certain timing boundaries, even for a quick tap), and freeglut
       delivers each as a distinct call to keyboard()/special_keyboard()
       -- neither of which has any repeat-suppression or dedup of its
       own. Symptom, confirmed live: arrow keys sometimes registering
       twice for one tap, or appearing to skip a step (the same
       repeat-boundary ambiguity that causes the double-fire also
       explains occasional drops in freeglut's internal repeat-vs-new-
       press heuristic). ASCII's input path never hits this -- it reads
       keys from the terminal, not GLUT/X11 callbacks -- which is why
       this was never seen there. glutIgnoreKeyRepeat(1) is GLUT's own,
       standard API for filtering OS-generated auto-repeat events so
       only genuine distinct key-down transitions reach the callbacks.
       This is the ONLY GL renderer file in the repo (every embedded
       Wraith project's window runs through this same process), so the
       fix applies to all of them at once, not just this project. */
    glutIgnoreKeyRepeat(1);

    update_focus_lock();

    if (stat(WRAITH_FRAME_TRIGGER, &st) == 0) {
        last_pulse_size = st.st_size;
    }

    load_texture();

    glutMainLoop();
    return 0;
}
