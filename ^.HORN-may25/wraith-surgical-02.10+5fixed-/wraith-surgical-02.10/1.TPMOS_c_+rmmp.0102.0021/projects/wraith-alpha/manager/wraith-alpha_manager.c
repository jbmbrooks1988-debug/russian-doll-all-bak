#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 4096
#define MAX_PATH 1024
#define MAX_WINDOWS 20

typedef enum {
    WSTATE_CLOSED = 0,
    WSTATE_OPEN = 1,
    WSTATE_MINIMIZED = 2
} WindowState;

typedef struct {
    char id[32];
    char title[64];
    int instance_no;
    WindowState state;
} Window;

static volatile int g_shutdown = 0;
static char g_project_root[MAX_PATH] = ".";
static Window g_windows[MAX_WINDOWS];
static int g_window_count = 0;
static int g_next_instance_no = 1;
static int g_active_window_slot = -1;
static int g_active_gui_index = 1;
static int g_max_index = 1;
static int g_mouse_x = 0;
static int g_mouse_y = 0;
static int g_mouse_btn = -1;
static int g_last_click_x = -1;
static int g_last_click_y = -1;
static char g_mouse_lock_path[MAX_PATH] = "";

static void update_state(int last_key);
static void route_input(int key);
static void handle_mouse(int btn, int x, int y);
static void enable_mouse_mode(void);
static void remove_mouse_lock(void);

static void handle_signal(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static char *trim_ws(char *str) {
    char *end;

    if (!str) {
        return NULL;
    }
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    return str;
}

static void resolve_root(void) {
    if (getcwd(g_project_root, sizeof(g_project_root)) == NULL) {
        strcpy(g_project_root, ".");
    }

    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) {
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *value = trim_ws(line + 13);
            if (value[0] != '\0') {
                strncpy(g_project_root, value, MAX_PATH - 1);
                g_project_root[MAX_PATH - 1] = '\0';
            }
            break;
        }
    }
    fclose(kvp);
}

static void log_alpha(const char *fmt, ...) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/wraith-alpha/manager/alpha_manager.log", g_project_root);

    FILE *f = fopen(path, "a");
    if (!f) {
        return;
    }

    time_t now = time(NULL);
    char *ts = ctime(&now);
    if (ts && strlen(ts) > 0) {
        ts[strlen(ts) - 1] = '\0';
    }
    fprintf(f, "[%s] ", ts ? ts : "time");

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fputc('\n', f);
    fclose(f);
}

static void trigger_render(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", g_project_root);

    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "A\n");
        fclose(f);
    }
}

static void remove_mouse_lock(void) {
    if (g_mouse_lock_path[0] != '\0') {
        remove(g_mouse_lock_path);
    }
}

static void enable_mouse_mode(void) {
    char mouse_dir[MAX_PATH];
    FILE *lock;

    snprintf(mouse_dir, sizeof(mouse_dir), "%s/pieces/mouse", g_project_root);
    mkdir(mouse_dir, 0777);

    snprintf(g_mouse_lock_path, sizeof(g_mouse_lock_path), "%s/pieces/mouse/mouse_enabled.lock", g_project_root);
    lock = fopen(g_mouse_lock_path, "w");
    if (!lock) {
        log_alpha("ERROR: Failed to create mouse lock: %s", g_mouse_lock_path);
        return;
    }

    fprintf(lock, "wraith-alpha\n");
    fclose(lock);
    log_alpha("Mouse mode requested via %s", g_mouse_lock_path);
}

static const char *window_state_name(WindowState state) {
    if (state == WSTATE_OPEN) {
        return "open";
    }
    if (state == WSTATE_MINIMIZED) {
        return "minimized";
    }
    return "closed";
}

static WindowState window_state_from_name(const char *name) {
    if (strcmp(name, "open") == 0) {
        return WSTATE_OPEN;
    }
    if (strcmp(name, "minimized") == 0) {
        return WSTATE_MINIMIZED;
    }
    return WSTATE_CLOSED;
}

static void reset_registry(void) {
    memset(g_windows, 0, sizeof(g_windows));
    g_window_count = 0;
    g_active_window_slot = -1;
}

static void set_window_identity(Window *window, int instance_no) {
    snprintf(window->id, sizeof(window->id), "terminal_%d", instance_no);
    snprintf(window->title, sizeof(window->title), "Terminal #%d", instance_no);
    window->instance_no = instance_no;
}

static int count_minimized_windows(void) {
    int count = 0;
    int i;

    for (i = 0; i < g_window_count; i++) {
        if (g_windows[i].state == WSTATE_MINIMIZED) {
            count++;
        }
    }
    return count;
}

static int count_open_windows(void) {
    int count = 0;
    int i;

    for (i = 0; i < g_window_count; i++) {
        if (g_windows[i].state == WSTATE_OPEN) {
            count++;
        }
    }
    return count;
}

static void recompute_active_window_slot(void) {
    int i;

    g_active_window_slot = -1;
    for (i = 0; i < g_window_count; i++) {
        if (g_windows[i].state == WSTATE_OPEN) {
            g_active_window_slot = i;
            break;
        }
    }
}

static void normalize_registry(void) {
    int i;
    int open_seen = 0;
    int max_instance_no = 0;

    for (i = 0; i < g_window_count; i++) {
        if (g_windows[i].instance_no <= 0) {
            g_windows[i].instance_no = i + 1;
        }
        if (g_windows[i].id[0] == '\0' || g_windows[i].title[0] == '\0') {
            set_window_identity(&g_windows[i], g_windows[i].instance_no);
        }
        if (g_windows[i].instance_no > max_instance_no) {
            max_instance_no = g_windows[i].instance_no;
        }
        if (g_windows[i].state == WSTATE_OPEN) {
            if (open_seen) {
                g_windows[i].state = WSTATE_MINIMIZED;
            } else {
                open_seen = 1;
            }
        }
    }

    if (g_next_instance_no <= max_instance_no) {
        g_next_instance_no = max_instance_no + 1;
    }
    recompute_active_window_slot();
}

static void compact_registry(void) {
    int dst = 0;
    int src;

    for (src = 0; src < g_window_count; src++) {
        if (g_windows[src].state == WSTATE_CLOSED) {
            continue;
        }
        if (dst != src) {
            g_windows[dst] = g_windows[src];
        }
        dst++;
    }
    while (dst < g_window_count) {
        memset(&g_windows[dst], 0, sizeof(Window));
        dst++;
    }
    g_window_count = dst;
    normalize_registry();
}

static void minimize_all_open_windows_except(int keep_slot) {
    int i;

    for (i = 0; i < g_window_count; i++) {
        if (i == keep_slot) {
            continue;
        }
        if (g_windows[i].state == WSTATE_OPEN) {
            g_windows[i].state = WSTATE_MINIMIZED;
        }
    }
    normalize_registry();
}

static Window *active_window(void) {
    if (g_active_window_slot < 0 || g_active_window_slot >= g_window_count) {
        return NULL;
    }
    if (g_windows[g_active_window_slot].state != WSTATE_OPEN) {
        return NULL;
    }
    return &g_windows[g_active_window_slot];
}

static Window *restore_slot_window(int restore_slot) {
    int seen = 0;
    int i;

    for (i = 0; i < g_window_count; i++) {
        if (g_windows[i].state != WSTATE_MINIMIZED) {
            continue;
        }
        seen++;
        if (seen == restore_slot) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static void launch_terminal_instance(void) {
    Window *window;

    if (g_window_count >= MAX_WINDOWS) {
        log_alpha("Launch ignored: registry full (%d windows)", g_window_count);
        return;
    }

    minimize_all_open_windows_except(-1);

    window = &g_windows[g_window_count++];
    memset(window, 0, sizeof(*window));
    set_window_identity(window, g_next_instance_no++);
    window->state = WSTATE_OPEN;
    normalize_registry();

    log_alpha("Launch requested: opening %s", window->title);
}

static void minimize_active_window(void) {
    Window *window = active_window();

    if (!window) {
        return;
    }

    window->state = WSTATE_MINIMIZED;
    normalize_registry();
    g_active_gui_index = 1;
    log_alpha("Minimize requested: moving %s to taskbar", window->title);
}

static void close_active_window(void) {
    Window *window = active_window();

    if (!window) {
        return;
    }

    log_alpha("Close requested: quitting %s", window->title);
    window->state = WSTATE_CLOSED;
    compact_registry();
    g_active_gui_index = 1;
}

static void restore_minimized_slot(int restore_slot) {
    Window *window = restore_slot_window(restore_slot);

    if (!window) {
        return;
    }

    minimize_all_open_windows_except(-1);
    window->state = WSTATE_OPEN;
    normalize_registry();
    g_active_gui_index = 1;
    log_alpha("Restore requested: reopening %s from taskbar slot %d", window->title, restore_slot);
}

static void sync_registry_from_disk(void) {
    char path[MAX_PATH];
    FILE *f;
    char line[MAX_LINE];
    int registry_count = -1;
    int legacy_window_count = 0;
    int legacy_open = 0;
    int legacy_minimized = 0;

    snprintf(path, sizeof(path), "%s/projects/wraith-alpha/session/desktop_ui_state.txt", g_project_root);
    f = fopen(path, "r");
    if (!f) {
        reset_registry();
        g_next_instance_no = 1;
        return;
    }

    reset_registry();
    g_next_instance_no = 1;

    while (fgets(line, sizeof(line), f)) {
        char *value;
        int slot;

        line[strcspn(line, "\r\n")] = '\0';
        value = strchr(line, '=');
        if (!value) {
            continue;
        }
        *value++ = '\0';
        value = trim_ws(value);

        if (strcmp(line, "desktop_registry_count") == 0) {
            registry_count = atoi(value);
            if (registry_count < 0) {
                registry_count = 0;
            }
            if (registry_count > MAX_WINDOWS) {
                registry_count = MAX_WINDOWS;
            }
            g_window_count = registry_count;
            continue;
        }
        if (strcmp(line, "desktop_next_instance_no") == 0) {
            g_next_instance_no = atoi(value);
            if (g_next_instance_no < 1) {
                g_next_instance_no = 1;
            }
            continue;
        }
        if (strcmp(line, "active_gui_index") == 0) {
            g_active_gui_index = atoi(value);
            if (g_active_gui_index < 1) {
                g_active_gui_index = 1;
            }
            continue;
        }
        if (strcmp(line, "desktop_window_count") == 0) {
            legacy_window_count = atoi(value);
            continue;
        }
        if (strcmp(line, "desktop_window_1_open") == 0) {
            legacy_open = (strcmp(value, "true") == 0);
            continue;
        }
        if (strcmp(line, "desktop_window_1_minimized") == 0) {
            legacy_minimized = (strcmp(value, "true") == 0);
            continue;
        }
        if (sscanf(line, "desktop_registry_%d_instance_no", &slot) == 1) {
            if (slot >= 1 && slot <= MAX_WINDOWS) {
                g_windows[slot - 1].instance_no = atoi(value);
            }
            continue;
        }
        if (sscanf(line, "desktop_registry_%d_id", &slot) == 1) {
            if (slot >= 1 && slot <= MAX_WINDOWS) {
                strncpy(g_windows[slot - 1].id, value, sizeof(g_windows[slot - 1].id) - 1);
            }
            continue;
        }
        if (sscanf(line, "desktop_registry_%d_title", &slot) == 1) {
            if (slot >= 1 && slot <= MAX_WINDOWS) {
                strncpy(g_windows[slot - 1].title, value, sizeof(g_windows[slot - 1].title) - 1);
            }
            continue;
        }
        if (sscanf(line, "desktop_registry_%d_state", &slot) == 1) {
            if (slot >= 1 && slot <= MAX_WINDOWS) {
                g_windows[slot - 1].state = window_state_from_name(value);
            }
            continue;
        }
    }
    fclose(f);

    if (g_window_count == 0 && legacy_window_count > 0) {
        g_window_count = 1;
        set_window_identity(&g_windows[0], 1);
        if (legacy_minimized) {
            g_windows[0].state = WSTATE_MINIMIZED;
        } else if (legacy_open) {
            g_windows[0].state = WSTATE_OPEN;
        } else {
            g_windows[0].state = WSTATE_CLOSED;
        }
    }

    compact_registry();
}

static void format_key_label(int key, char *out, size_t size) {
    if (key >= 32 && key <= 126) {
        snprintf(out, size, "%c", key);
    } else if (key == 1002) {
        snprintf(out, size, "UP");
    } else if (key == 1003) {
        snprintf(out, size, "DOWN");
    } else if (key == 1000) {
        snprintf(out, size, "LEFT");
    } else if (key == 1001) {
        snprintf(out, size, "RIGHT");
    } else if (key == 10 || key == 13) {
        snprintf(out, size, "ENTER");
    } else {
        snprintf(out, size, "%d", key);
    }
}

static void build_projection_signature(char *out, size_t size) {
    int i;

    snprintf(out, size, "count=%d;active=%d;next=%d", g_window_count, g_active_window_slot, g_next_instance_no);
    for (i = 0; i < g_window_count; i++) {
        char part[160];
        snprintf(
            part,
            sizeof(part),
            "|%s:%s:%s",
            g_windows[i].id,
            g_windows[i].title,
            window_state_name(g_windows[i].state)
        );
        if (strlen(out) + strlen(part) < size - 1) {
            strcat(out, part);
        }
    }
}

static void write_projection(FILE *f, int last_key) {
    Window *window = active_window();
    char key_label[64];
    char active_title[64];
    char focused_title[64];
    char projection_signature[2048];
    int restore_count = 0;
    int slot;

    format_key_label(last_key, key_label, sizeof(key_label));
    snprintf(active_title, sizeof(active_title), "%s", window ? window->title : "Terminal");
    snprintf(focused_title, sizeof(focused_title), "%s", window ? window->title : "Desktop");
    build_projection_signature(projection_signature, sizeof(projection_signature));

    fprintf(f, "project_id=wraith-alpha\n");
    fprintf(f, "desktop_mode=desktop\n");
    fprintf(f, "desktop_window_count=%d\n", g_window_count);
    fprintf(f, "desktop_open_window_count=%d\n", count_open_windows());
    fprintf(f, "desktop_minimized_window_count=%d\n", count_minimized_windows());
    fprintf(f, "desktop_default_window_id=terminal\n");
    fprintf(f, "desktop_default_window_title=Terminal\n");
    fprintf(f, "desktop_focused_window_id=%s\n", window ? window->id : "desktop");
    fprintf(f, "desktop_focused_window_title=%s\n", focused_title);
    fprintf(f, "desktop_launcher_count=1\n");
    fprintf(f, "desktop_launcher_1_id=terminal_launcher\n");
    fprintf(f, "desktop_launcher_1_title=Terminal\n");
    fprintf(f, "desktop_launcher_1_kind=launcher\n");
    fprintf(f, "desktop_next_instance_no=%d\n", g_next_instance_no);
    fprintf(f, "desktop_projection_signature=%s\n", projection_signature);
    fprintf(f, "desktop_toolbar_markup=\n");

    fprintf(f, "desktop_active_window_visible=%s\n", window ? "true" : "false");
    fprintf(f, "desktop_active_window_id=%s\n", window ? window->id : "");
    fprintf(f, "desktop_active_window_title=%s\n", active_title);
    fprintf(f, "desktop_active_window_body_visible=%s\n", window ? "true" : "false");
    fprintf(f, "desktop_active_window_hidden=%s\n", window ? "false" : "true");
    fprintf(f, "desktop_active_window_mode=half\n");
    fprintf(f, "desktop_active_window_collapse_glyph=-\n");

    fprintf(f, "desktop_window_1_id=%s\n", window ? window->id : "terminal");
    fprintf(f, "desktop_window_1_title=%s\n", active_title);
    fprintf(f, "desktop_window_1_kind=terminal\n");
    fprintf(f, "desktop_window_1_open=%s\n", window ? "true" : "false");
    fprintf(f, "desktop_window_1_collapsed=false\n");
    fprintf(f, "desktop_window_1_minimized=false\n");
    fprintf(f, "desktop_window_1_minimized_visible=false\n");
    fprintf(f, "desktop_window_1_restore_title=\n");
    fprintf(f, "desktop_window_1_body_visible=%s\n", window ? "true" : "false");
    fprintf(f, "desktop_window_1_hidden=%s\n", window ? "false" : "true");
    fprintf(f, "desktop_window_1_collapse_glyph=-\n");
    fprintf(f, "desktop_window_1_fullscreen=false\n");
    fprintf(f, "desktop_window_1_mode=half\n");
    fprintf(f, "desktop_window_1_halfscreen_visible=true\n");

    fprintf(f, "desktop_registry_count=%d\n", g_window_count);
    for (slot = 0; slot < g_window_count; slot++) {
        fprintf(f, "desktop_registry_%d_id=%s\n", slot + 1, g_windows[slot].id);
        fprintf(f, "desktop_registry_%d_title=%s\n", slot + 1, g_windows[slot].title);
        fprintf(f, "desktop_registry_%d_instance_no=%d\n", slot + 1, g_windows[slot].instance_no);
        fprintf(f, "desktop_registry_%d_state=%s\n", slot + 1, window_state_name(g_windows[slot].state));
        fprintf(f, "desktop_registry_%d_open=%s\n", slot + 1, g_windows[slot].state == WSTATE_OPEN ? "true" : "false");
        fprintf(f, "desktop_registry_%d_minimized=%s\n", slot + 1, g_windows[slot].state == WSTATE_MINIMIZED ? "true" : "false");
        fprintf(f, "desktop_registry_%d_visible=%s\n", slot + 1, g_windows[slot].state != WSTATE_CLOSED ? "true" : "false");
    }

    for (slot = 1; slot <= MAX_WINDOWS; slot++) {
        Window *restore_window = restore_slot_window(slot);
        if (restore_window) {
            restore_count++;
            fprintf(f, "desktop_restore_slot_%d_visible=true\n", slot);
            fprintf(f, "desktop_restore_slot_%d_title=%s\n", slot, restore_window->title);
            fprintf(f, "desktop_restore_slot_%d_id=%s\n", slot, restore_window->id);
        } else {
            fprintf(f, "desktop_restore_slot_%d_visible=false\n", slot);
            fprintf(f, "desktop_restore_slot_%d_title=\n", slot);
            fprintf(f, "desktop_restore_slot_%d_id=\n", slot);
        }
    }
    fprintf(f, "desktop_restore_count=%d\n", restore_count);
    fprintf(f, "active_gui_index=%d\n", g_active_gui_index);
    fprintf(f, "current_key_label=%s\n", key_label);
    fprintf(f, "current_key_raw=%d\n", last_key);
    fprintf(f, "mouse_x=%d\n", g_mouse_x);
    fprintf(f, "mouse_y=%d\n", g_mouse_y);
    fprintf(f, "mouse_btn=%d\n", g_mouse_btn);
    fprintf(f, "last_click_x=%d\n", g_last_click_x);
    fprintf(f, "last_click_y=%d\n", g_last_click_y);
}

static void update_state(int last_key) {
    char alpha_path[MAX_PATH];
    char alpha_tmp[MAX_PATH];
    char ui_path[MAX_PATH];
    char ui_tmp[MAX_PATH];
    FILE *alpha;
    FILE *ui;

    normalize_registry();

    snprintf(alpha_path, sizeof(alpha_path), "%s/projects/wraith-alpha/session/alpha_state.txt", g_project_root);
    snprintf(alpha_tmp, sizeof(alpha_tmp), "%s.tmp", alpha_path);
    snprintf(ui_path, sizeof(ui_path), "%s/projects/wraith-alpha/session/desktop_ui_state.txt", g_project_root);
    snprintf(ui_tmp, sizeof(ui_tmp), "%s.tmp", ui_path);

    alpha = fopen(alpha_tmp, "w");
    if (!alpha) {
        log_alpha("ERROR: Failed to open alpha state tmp for writing: %s", alpha_tmp);
        return;
    }
    write_projection(alpha, last_key);
    fclose(alpha);
    rename(alpha_tmp, alpha_path);

    ui = fopen(ui_tmp, "w");
    if (!ui) {
        log_alpha("ERROR: Failed to open desktop UI state tmp for writing: %s", ui_tmp);
        return;
    }
    write_projection(ui, last_key);
    fclose(ui);
    rename(ui_tmp, ui_path);

    log_alpha("State updated: idx=%d, windows=%d, minimized=%d", g_active_gui_index, g_window_count, count_minimized_windows());
}

static void archive_input(int key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/wraith-alpha/session/history_archive.txt", g_project_root);

    FILE *f = fopen(path, "a");
    if (!f) {
        return;
    }

    time_t now = time(NULL);
    char *ts = ctime(&now);
    if (ts && strlen(ts) > 0) {
        ts[strlen(ts) - 1] = '\0';
    }
    fprintf(f, "[%s] KEY: %d\n", ts ? ts : "time", key);
    fclose(f);
}

static void execute_action(int action_idx) {
    int changed = 0;

    if (action_idx == 1) {
        g_active_gui_index = 1;
        update_state(0);
        trigger_render();
        return;
    }
    if (action_idx == 2) {
        g_active_gui_index = 2;
        update_state(0);
        trigger_render();
        return;
    }
    if (action_idx == 3) {
        if (active_window()) {
            minimize_active_window();
            changed = 1;
        }
    } else if (action_idx == 4) {
        if (active_window()) {
            close_active_window();
            changed = 1;
        }
    } else if (action_idx == 5) {
        launch_terminal_instance();
        changed = 1;
    } else if (action_idx >= 6 && action_idx < 6 + MAX_WINDOWS) {
        int restore_slot = action_idx - 5;
        if (restore_slot_window(restore_slot)) {
            restore_minimized_slot(restore_slot);
            changed = 1;
        }
    }

    if (changed) {
        update_state(0);
        trigger_render();
    }
}

static void route_command(const char *cmd) {
    log_alpha("Command received: %s", cmd);

    if (strncmp(cmd, "MOUSE_MOVE ", 11) == 0) {
        int btn, x, y;
        if (sscanf(cmd + 11, "%d %d %d", &btn, &x, &y) == 3) {
            handle_mouse(btn, x, y);
        }
        return;
    }
    if (strncmp(cmd, "SET_ACTIVE:", 11) == 0) {
        execute_action(atoi(cmd + 11));
        return;
    }
    if (strcmp(cmd, "DESKTOP_ACTION:launch_terminal") == 0) {
        execute_action(5);
        return;
    }
    if (strcmp(cmd, "DESKTOP_ACTION:minimize_terminal") == 0) {
        execute_action(3);
        return;
    }
    if (strcmp(cmd, "DESKTOP_ACTION:close_terminal") == 0) {
        execute_action(4);
        return;
    }
    if (strcmp(cmd, "DESKTOP_ACTION:restore_terminal") == 0) {
        execute_action(6);
        return;
    }
}

static void handle_mouse(int btn, int x, int y) {
    g_mouse_btn = btn;
    g_mouse_x = x;
    g_mouse_y = y;

    /* SGR button codes: 0=Left, 1=Middle, 2=Right, 3=Release.
     * Motion codes are 32+. */
    if (btn == 0) {
        g_last_click_x = x;
        g_last_click_y = y;
    }

    log_alpha("Mouse updated: btn=%d x=%d y=%d (last_click: %d,%d)", btn, x, y, g_last_click_x, g_last_click_y);
    update_state(0);

    if (btn < 4) {
        trigger_render();
    }
}

static void process_history_file(const char *path, long *last_pos, const char *label) {
    struct stat st;
    FILE *f;
    char line[MAX_LINE];

    if (stat(path, &st) != 0) {
        return;
    }
    if (st.st_size < *last_pos) {
        *last_pos = 0;
        log_alpha("%s history truncation detected.", label);
    }
    if (st.st_size <= *last_pos) {
        return;
    }

    f = fopen(path, "r");
    if (!f) {
        return;
    }

    fseek(f, *last_pos, SEEK_SET);
    while (fgets(line, sizeof(line), f)) {
        char *key_pos = strstr(line, "KEY_PRESSED: ");
        char *cmd_pos = strstr(line, "COMMAND: ");

        if (key_pos) {
            route_input(atoi(key_pos + 12));
        }
        if (cmd_pos) {
            route_command(trim_ws(cmd_pos + 9));
        }
    }
    *last_pos = ftell(f);
    fclose(f);
}

static void recompute_nav_bounds(void) {
    int open_chrome = active_window() ? 4 : 0;
    g_max_index = open_chrome + 1 + count_minimized_windows();
    if (g_max_index < 1) {
        g_max_index = 1;
    }
    if (g_active_gui_index > g_max_index) {
        g_active_gui_index = g_max_index;
    }
    if (g_active_gui_index < 1) {
        g_active_gui_index = 1;
    }
}

static void route_input(int key) {
    int changed = 0;

    archive_input(key);
    normalize_registry();
    recompute_nav_bounds();
    log_alpha("Input received: %d", key);

    if (key >= '1' && key <= '9') {
        int digit = key - '0';
        if (digit <= g_max_index) {
            g_active_gui_index = digit;
            changed = 1;
        }
    } else if (key == 1002) {
        if (g_active_gui_index > 1) {
            g_active_gui_index--;
            changed = 1;
        }
    } else if (key == 1003) {
        if (g_active_gui_index < g_max_index) {
            g_active_gui_index++;
            changed = 1;
        }
    } else if (key == 10 || key == 13) {
        return;
    }

    if (changed) {
        update_state(key);
        trigger_render();
    }
}

int main(void) {
    struct stat st;
    long last_keyboard_pos = 0;
    long last_project_pos = 0;
    char keyboard_hist_path[MAX_PATH];
    char project_hist_path[MAX_PATH];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    resolve_root();
    atexit(remove_mouse_lock);
    enable_mouse_mode();

    snprintf(keyboard_hist_path, sizeof(keyboard_hist_path), "%s/pieces/keyboard/history.txt", g_project_root);
    snprintf(project_hist_path, sizeof(project_hist_path), "%s/projects/wraith-alpha/session/history.txt", g_project_root);

    if (stat(keyboard_hist_path, &st) == 0) {
        last_keyboard_pos = st.st_size;
    }
    if (stat(project_hist_path, &st) == 0) {
        last_project_pos = st.st_size;
    }

    sync_registry_from_disk();
    recompute_nav_bounds();
    update_state(0);
    trigger_render();

    log_alpha("Wraith-Alpha Manager starting in %s", g_project_root);

    while (!g_shutdown) {
        process_history_file(keyboard_hist_path, &last_keyboard_pos, "Keyboard");
        process_history_file(project_hist_path, &last_project_pos, "Project");
        usleep(16667);
    }

    log_alpha("Wraith-Alpha Manager shutting down.");
    return 0;
}
