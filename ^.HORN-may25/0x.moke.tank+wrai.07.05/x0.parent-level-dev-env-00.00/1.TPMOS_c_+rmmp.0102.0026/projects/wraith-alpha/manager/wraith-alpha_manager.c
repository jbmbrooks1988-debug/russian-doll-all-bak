#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#define MAX_LINE 4096
#define MAX_PATH 1024
#define MAX_WINDOWS 20
#define WRAITH_TERMINAL_PROJECT_ID "wraith/wraith-projects/terminal"
#define WRAITH_BLANK_PROJECT_ID "wraith/wraith-projects/blank-project"

typedef enum {
    WSTATE_CLOSED = 0,
    WSTATE_OPEN = 1,
    WSTATE_MINIMIZED = 2
} WindowState;

typedef struct {
    char id[32];
    char title[64];
    char project_id[64];
    int instance_no;
    WindowState state;
} Window;

typedef struct {
    const char *command;
    const char *id_prefix;
    const char *title_prefix;
    const char *project_id;
} WraithLaunchSpec;

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
#ifndef _WIN32
static pid_t g_rgb_daemon_pid = -1;
static pid_t g_gl_pid = -1;
#else
static intptr_t g_rgb_daemon_pid = -1;
static intptr_t g_gl_pid = -1;
#endif

static void update_state(int last_key);
static void route_input(int key);
static void handle_mouse(int btn, int x, int y);
static void enable_mouse_mode(void);
static void remove_mouse_lock(void);
static void cleanup_runtime(void);
static void log_alpha(const char *fmt, ...);
static void log_pid(int pid, const char* name);
static void route_command(const char *cmd);
static void sync_active_gui_index_from_display(void);
static void bootstrap_fresh_session(void);
static void truncate_file(const char *rel_path);
static void reset_registry(void);
static void set_window_identity(Window *window, int instance_no);
static void recompute_active_window_slot(void);
static int count_launcher_methods(void);
static int count_visible_windows(void);
static bool dispatch_menu_index(int menu_index);
static bool dispatch_launcher_method_by_index(int launcher_idx);
static bool launch_wraith_project_command(const char *cmd);
static void launch_window_instance(const char *id_prefix, const char *title_prefix, const char *project_id);
static void appendf(char *out, size_t size, const char *fmt, ...);

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

static void request_layout_change(const char *layout_path) {
    char path[MAX_PATH];

    if (!layout_path || layout_path[0] == '\0') {
        return;
    }

    snprintf(path, sizeof(path), "%s/pieces/display/layout_changed.txt", g_project_root);
    FILE *f = fopen(path, "a");
    if (!f) {
        log_alpha("ERROR: Failed to request layout change: %s", path);
        return;
    }

    fprintf(f, "%s\n", layout_path);
    fclose(f);
    log_alpha("Layout change requested: %s", layout_path);
}

static void remove_mouse_lock(void) {
    if (g_mouse_lock_path[0] != '\0') {
        remove(g_mouse_lock_path);
    }
}

static void cleanup_spawned_child(void *pid_ptr, const char *name) {
#ifndef _WIN32
    pid_t pid = *(pid_t *)pid_ptr;
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        log_alpha("Stopped %s (pid=%d)", name, (int)pid);
        *(pid_t *)pid_ptr = -1;
    }
#else
    intptr_t pid = *(intptr_t *)pid_ptr;
    if (pid > 0) {
        TerminateProcess((HANDLE)pid, 0);
        CloseHandle((HANDLE)pid);
        log_alpha("Stopped %s (handle=%ld)", name, (long)pid);
        *(intptr_t *)pid_ptr = -1;
    }
#endif
}

static void cleanup_runtime(void) {
    cleanup_spawned_child(&g_gl_pid, "wraith_gl");
    cleanup_spawned_child(&g_rgb_daemon_pid, "wraith_rgb_daemon");
    remove_mouse_lock();
}

static void truncate_file(const char *rel_path) {
    char path[MAX_PATH];
    FILE *f;

    if (!rel_path || rel_path[0] == '\0') {
        return;
    }

    snprintf(path, sizeof(path), "%s/%s", g_project_root, rel_path);
    f = fopen(path, "w");
    if (f) {
        fclose(f);
    }
}

static void bootstrap_fresh_session(void) {
    reset_registry();
    memset(g_windows, 0, sizeof(g_windows));
    g_window_count = 0;
    g_next_instance_no = 1;
    g_active_gui_index = 1;
    recompute_active_window_slot();
    g_mouse_btn = -1;
    g_mouse_x = 0;
    g_mouse_y = 0;
    g_last_click_x = -1;
    g_last_click_y = -1;

    truncate_file("projects/wraith-alpha/session/history.txt");
    truncate_file("projects/wraith-alpha/session/history_archive.txt");
    truncate_file("projects/wraith-alpha/session/desktop_actions.txt");
    truncate_file("projects/wraith-alpha/session/alpha_state.txt");
    truncate_file("projects/wraith-alpha/session/desktop_ui_state.txt");
    truncate_file("projects/wraith-alpha/manager/gui_state.txt");
    truncate_file("pieces/display/active_gui_index.txt");
    truncate_file("pieces/display/current_frame.txt");
    truncate_file("pieces/display/frame_changed.txt");

    update_state(0);
    trigger_render();
    log_alpha("Bootstrapped fresh Wraith Alpha session state.");
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

static void set_window_identity_custom(
    Window *window,
    int instance_no,
    const char *id_prefix,
    const char *title_prefix,
    const char *project_id
) {
    snprintf(window->id, sizeof(window->id), "%s_%d", id_prefix, instance_no);
    snprintf(window->title, sizeof(window->title), "%s #%d", title_prefix, instance_no);
    strncpy(window->project_id, project_id, sizeof(window->project_id) - 1);
    window->project_id[sizeof(window->project_id) - 1] = '\0';
    window->instance_no = instance_no;
}

static void set_window_identity(Window *window, int instance_no) {
    set_window_identity_custom(window, instance_no, "terminal", "Terminal", WRAITH_TERMINAL_PROJECT_ID);
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

static int count_visible_windows(void) {
    int count = 0;
    int i;

    for (i = 0; i < g_window_count; i++) {
        if (g_windows[i].state != WSTATE_CLOSED) {
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

static int count_launcher_methods(void) {
    char pdl_path[MAX_PATH];
    FILE *f;
    char line[MAX_LINE];
    int count = 0;

    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/wraith-alpha/wraith-projects/launcher/piece.pdl", g_project_root);
    f = fopen(pdl_path, "r");
    if (!f) {
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) == 0) {
            count++;
        }
    }

    fclose(f);
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

static Window *taskbar_slot_window(int slot) {
    int seen = 0;
    int i;

    for (i = 0; i < g_window_count; i++) {
        if (g_windows[i].state == WSTATE_CLOSED) {
            continue;
        }
        seen++;
        if (seen == slot) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static void launch_window_instance(const char *id_prefix, const char *title_prefix, const char *project_id) {
    Window *window;
    int slot = -1;
    int i;

    compact_registry();

    for (i = 0; i < g_window_count; i++) {
        if (g_windows[i].state == WSTATE_CLOSED) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        if (g_window_count >= MAX_WINDOWS) {
            log_alpha("Launch ignored: registry full (%d windows)", g_window_count);
            return;
        }
        slot = g_window_count++;
    }

    minimize_all_open_windows_except(-1);

    window = &g_windows[slot];
    memset(window, 0, sizeof(*window));
    set_window_identity_custom(window, g_next_instance_no++, id_prefix, title_prefix, project_id);
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

static void activate_taskbar_slot(int slot) {
    Window *window = taskbar_slot_window(slot);

    if (!window) {
        return;
    }

    minimize_all_open_windows_except(-1);
    window->state = WSTATE_OPEN;
    normalize_registry();
    g_active_gui_index = 1;
    log_alpha("Toolbar requested: focusing %s from slot %d", window->title, slot);
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
        if (sscanf(line, "desktop_registry_%d_project_id", &slot) == 1) {
            if (slot >= 1 && slot <= MAX_WINDOWS) {
                strncpy(g_windows[slot - 1].project_id, value, sizeof(g_windows[slot - 1].project_id) - 1);
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

static void appendf(char *out, size_t size, const char *fmt, ...) {
    char buf[1024];
    size_t used;
    va_list args;

    if (!out || size == 0 || !fmt) {
        return;
    }

    used = strlen(out);
    if (used >= size - 1) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    strncat(out, buf, size - used - 1);
}

static void build_desktop_shell_markup(char *out, size_t size, Window *window) {
    char pdl_path[MAX_PATH];
    char line[MAX_LINE];
    int launcher_index = 0;
    int taskbar_index = 0;
    int launcher_count = 0;
    int launcher_start = 0;
    int taskbar_start = 0;
    int i;

    out[0] = '\0';

    if (!window) {
        appendf(out, size, "+-WRAITH DESKTOP GUI---------------------------------------------------------------------------+<br/>");
        appendf(out, size, "<br/><br/><br/>");
        appendf(out, size, "| [ TASKBAR ] <button compact=\"true\" label=\"Terminal\" onClick=\"DESKTOP_ACTION:launch_terminal\" />");
        for (i = 0; i < g_window_count; i++) {
            Window *taskbar_window = &g_windows[i];
            char action[32];

            if (taskbar_window->state == WSTATE_CLOSED) {
                continue;
            }

            taskbar_index++;
            snprintf(action, sizeof(action), "SET_ACTIVE:%d", taskbar_index + 1);
            appendf(out, size, "<button compact=\"true\" label=\"%s\" onClick=\"%s\" />", taskbar_window->title, action);
        }
        appendf(out, size, " |<br/>");
        appendf(out, size, "+----------------------------------------------------------------------------------------------+<br/>");
        return;
    }

    launcher_count = count_launcher_methods();
    launcher_start = 5;
    taskbar_start = window ? (launcher_start + launcher_count) : 1;

    appendf(out, size, "+-WRAITH DESKTOP GUI---------------------------------------------------------------------------+<br/>");

    appendf(out, size, "| +-");
    appendf(out, size, "<button label=\" %s \" onClick=\"SET_ACTIVE:1\" />", window->title);
    appendf(out, size, "------------------------------------------");
    appendf(out, size, "<button compact=\"true\" label=\"o\" onClick=\"SET_ACTIVE:2\" />");
    appendf(out, size, "<button compact=\"true\" label=\"-\" onClick=\"SET_ACTIVE:3\" />");
    appendf(out, size, "<button compact=\"true\" label=\"x\" onClick=\"SET_ACTIVE:4\" />");
    appendf(out, size, "-+               |<br/>");

    if (launcher_count > 0) {
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/wraith-alpha/wraith-projects/launcher/piece.pdl", g_project_root);
        FILE *f = fopen(pdl_path, "r");
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                char *key_start;
                char *val_start;
                char *cmd_val;
                char *method_name;

                if (strncmp(line, "METHOD", 6) != 0) {
                    continue;
                }

                key_start = strchr(line, '|');
                if (!key_start) {
                    continue;
                }
                key_start++;

                val_start = strchr(key_start, '|');
                if (!val_start) {
                    continue;
                }
                *val_start = '\0';

                method_name = trim_ws(key_start);
                cmd_val = trim_ws(val_start + 1);
                if (method_name[0] == '\0' || cmd_val[0] == '\0') {
                    continue;
                }

                launcher_index++;
                appendf(out, size, "| |  ");
                appendf(out, size, "<button compact=\"true\" label=\"%-12s\" onClick=\"%s\" visibility=\"${desktop_active_window_body_visible}\" />", method_name, cmd_val);
                appendf(out, size, "                                                                          |<br visibility=\"${desktop_active_window_body_visible}\" />");
            }
            fclose(f);
        }
    }

    if (launcher_index == 0) {
        appendf(out, size, "| |  %-85s |<br visibility=\"${desktop_active_window_body_visible}\" />", "[ No Projects Found ]");
    }

    while (launcher_index < 4) {
        appendf(out, size, "| |                                                                                              |<br visibility=\"${desktop_active_window_body_visible}\" />");
        launcher_index++;
    }

    appendf(out, size, "| +----------------------------------------------------------------------------------------------+               |<br visibility=\"${desktop_active_window_body_visible}\" />");
    appendf(out, size, "| [ TASKBAR ] <button compact=\"true\" label=\"Terminal\" onClick=\"DESKTOP_ACTION:launch_terminal\" />");

    for (i = 0; i < g_window_count; i++) {
        Window *taskbar_window = &g_windows[i];
        char action[32];

        if (taskbar_window->state == WSTATE_CLOSED) {
            continue;
        }

        taskbar_index++;
        snprintf(action, sizeof(action), "SET_ACTIVE:%d", taskbar_start + taskbar_index);
        appendf(out, size, "<button compact=\"true\" label=\"%s\" onClick=\"%s\" />", taskbar_window->title, action);
    }

    appendf(out, size, " |<br/>");
    appendf(out, size, "+----------------------------------------------------------------------------------------------+<br/>");
}

static void write_projection(FILE *f, int last_key) {
    Window *window = active_window();
    char key_label[64];
    char active_title[64];
    char focused_title[64];
    char projection_signature[2048];
    char body_content[8192];
    int launcher_count = 0;
    int visible_count = 0;
    int slot;

    format_key_label(last_key, key_label, sizeof(key_label));
    snprintf(active_title, sizeof(active_title), "%s", window ? window->title : "Terminal");
    snprintf(focused_title, sizeof(focused_title), "%s", window ? window->title : "Desktop");
    build_projection_signature(projection_signature, sizeof(projection_signature));
    build_desktop_shell_markup(body_content, sizeof(body_content), window);
    launcher_count = count_launcher_methods();
    visible_count = count_visible_windows();

    fprintf(f, "project_id=wraith-alpha\n");
    fprintf(f, "desktop_mode=desktop\n");
    fprintf(f, "desktop_window_count=%d\n", g_window_count);
    fprintf(f, "desktop_open_window_count=%d\n", count_open_windows());
    fprintf(f, "desktop_minimized_window_count=%d\n", count_minimized_windows());
    fprintf(f, "desktop_default_window_id=terminal\n");
    fprintf(f, "desktop_default_window_title=Terminal\n");
    fprintf(f, "desktop_focused_window_id=%s\n", window ? window->id : "desktop");
    fprintf(f, "desktop_focused_window_title=%s\n", focused_title);
    fprintf(f, "desktop_launcher_count=%d\n", launcher_count);
    fprintf(f, "desktop_restore_count=%d\n", visible_count + 1);
    fprintf(f, "desktop_next_instance_no=%d\n", g_next_instance_no);
    fprintf(f, "desktop_projection_signature=%s\n", projection_signature);
    fprintf(f, "desktop_shell_markup=%s\n", body_content);
    fprintf(f, "desktop_toolbar_markup=%s\n", body_content);

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
        fprintf(f, "desktop_registry_%d_project_id=%s\n", slot + 1, g_windows[slot].project_id);
        fprintf(f, "desktop_registry_%d_instance_no=%d\n", slot + 1, g_windows[slot].instance_no);
        fprintf(f, "desktop_registry_%d_state=%s\n", slot + 1, window_state_name(g_windows[slot].state));
        fprintf(f, "desktop_registry_%d_open=%s\n", slot + 1, g_windows[slot].state == WSTATE_OPEN ? "true" : "false");
        fprintf(f, "desktop_registry_%d_minimized=%s\n", slot + 1, g_windows[slot].state == WSTATE_MINIMIZED ? "true" : "false");
        fprintf(f, "desktop_registry_%d_visible=%s\n", slot + 1, g_windows[slot].state != WSTATE_CLOSED ? "true" : "false");
    }

    for (slot = 1; slot <= MAX_WINDOWS; slot++) {
        Window *taskbar_window = taskbar_slot_window(slot);
        if (taskbar_window) {
            fprintf(f, "desktop_restore_slot_%d_visible=true\n", slot);
            fprintf(f, "desktop_restore_slot_%d_title=%s\n", slot, taskbar_window->title);
            fprintf(f, "desktop_restore_slot_%d_id=%s\n", slot, taskbar_window->id);
        } else {
            fprintf(f, "desktop_restore_slot_%d_visible=false\n", slot);
            fprintf(f, "desktop_restore_slot_%d_title=\n", slot);
            fprintf(f, "desktop_restore_slot_%d_id=\n", slot);
        }
    }
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

static bool dispatch_menu_index(int menu_index) {
    Window *window = active_window();
    int launcher_count = count_launcher_methods();
    int taskbar_count = count_visible_windows();
    int taskbar_start = window ? (5 + launcher_count) : 1;

    if (menu_index < 1) {
        return false;
    }

    if (!window) {
        if (menu_index == 1) {
            launch_wraith_project_command("DESKTOP_ACTION:launch_terminal");
            return true;
        }
        if (menu_index >= 2 && menu_index <= taskbar_count + 1) {
            activate_taskbar_slot(menu_index - 1);
            update_state(0);
            trigger_render();
            return true;
        }
        return false;
    }

    if (menu_index == 1) {
        g_active_gui_index = 1;
        update_state(0);
        trigger_render();
        return true;
    }
    if (menu_index == 2) {
        g_active_gui_index = 2;
        update_state(0);
        trigger_render();
        return true;
    }
    if (menu_index == 3) {
        if (window) {
            minimize_active_window();
            update_state(0);
            trigger_render();
            return true;
        }
        return false;
    }
    if (menu_index == 4) {
        if (window) {
            close_active_window();
            update_state(0);
            trigger_render();
            return true;
        }
        return false;
    }
    if (menu_index >= 5 && menu_index < 5 + launcher_count) {
        if (dispatch_launcher_method_by_index(menu_index - 4)) {
            return true;
        }
        return false;
    }
    if (menu_index == taskbar_start) {
        launch_wraith_project_command("DESKTOP_ACTION:launch_terminal");
        return true;
    }
    if (menu_index > taskbar_start && menu_index <= taskbar_start + taskbar_count) {
        int slot = menu_index - taskbar_start;
        if (taskbar_slot_window(slot)) {
            activate_taskbar_slot(slot);
            update_state(0);
            trigger_render();
            return true;
        }
    }

    return false;
}

static void execute_action(int action_idx) {
    (void)dispatch_menu_index(action_idx);
}

static bool launch_wraith_project_command(const char *cmd) {
    static const WraithLaunchSpec launch_specs[] = {
        { "DESKTOP_ACTION:launch_terminal", "terminal", "Terminal", WRAITH_TERMINAL_PROJECT_ID },
        { "DESKTOP_ACTION:launch_blank_project", "blank_project", "Blank Project", WRAITH_BLANK_PROJECT_ID }
    };
    size_t i;

    if (!cmd || cmd[0] == '\0') {
        return false;
    }

    for (i = 0; i < sizeof(launch_specs) / sizeof(launch_specs[0]); i++) {
        if (strcmp(cmd, launch_specs[i].command) == 0) {
            launch_window_instance(
                launch_specs[i].id_prefix,
                launch_specs[i].title_prefix,
                launch_specs[i].project_id
            );
            update_state(0);
            trigger_render();
            return true;
        }
    }

    return false;
}

static bool dispatch_launcher_method_by_index(int launcher_idx) {
    char pdl_path[MAX_PATH];
    FILE *f = NULL;
    char line[MAX_LINE];
    int method_idx = 1;

    if (launcher_idx < 1) {
        return false;
    }

    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/wraith-alpha/wraith-projects/launcher/piece.pdl", g_project_root);
    f = fopen(pdl_path, "r");
    if (!f) {
        log_alpha("Launcher dispatch failed: missing PDL %s", pdl_path);
        return false;
    }

    log_alpha("Launcher dispatch lookup: gui_index=%d launcher_idx=%d pdl=%s", g_active_gui_index, launcher_idx, pdl_path);

    while (fgets(line, sizeof(line), f)) {
        char *key_start;
        char *val_start;
        char *cmd_val;
        char *method_name;

        if (strncmp(line, "METHOD", 6) != 0) {
            continue;
        }

        key_start = strchr(line, '|');
        if (!key_start) {
            continue;
        }
        key_start++;

        val_start = strchr(key_start, '|');
        if (!val_start) {
            continue;
        }
        *val_start = '\0';

        method_name = trim_ws(key_start);
        cmd_val = trim_ws(val_start + 1);

        if (method_name[0] == '\0' || cmd_val[0] == '\0') {
            continue;
        }

        if (method_idx == launcher_idx) {
            log_alpha("Launcher dispatch resolved: %s -> %s", method_name, cmd_val);
            route_command(cmd_val);
            fclose(f);
            return true;
        }

        method_idx++;
    }

    fclose(f);
    log_alpha("Launcher dispatch failed: index %d out of range", launcher_idx);
    return false;
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
        dispatch_menu_index(atoi(cmd + 11));
        return;
    }
    if (launch_wraith_project_command(cmd)) {
        return;
    }
    if (strcmp(cmd, "DESKTOP_ACTION:minimize_terminal") == 0) {
        dispatch_menu_index(3);
        return;
    }
    if (strcmp(cmd, "DESKTOP_ACTION:close_terminal") == 0) {
        dispatch_menu_index(4);
        return;
    }
    if (strcmp(cmd, "DESKTOP_ACTION:restore_terminal") == 0) {
        dispatch_menu_index(active_window() ? (5 + count_launcher_methods()) : 1);
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
        char *raw_action_pos = strstr(line, "DESKTOP_ACTION:");
        char *raw_set_active_pos = strstr(line, "SET_ACTIVE:");

        if (key_pos) {
            route_input(atoi(key_pos + 12));
        }
        if (cmd_pos) {
            route_command(trim_ws(cmd_pos + 9));
        } else if (raw_action_pos) {
            route_command(trim_ws(raw_action_pos));
        } else if (raw_set_active_pos) {
            route_command(trim_ws(raw_set_active_pos));
        }
    }
    *last_pos = ftell(f);
    fclose(f);
}

static void recompute_nav_bounds(void) {
    Window *window = active_window();
    int launcher_count = count_launcher_methods();
    int taskbar_count = count_visible_windows();

    if (window) {
        g_max_index = 4 + launcher_count + 1 + taskbar_count;
    } else {
        g_max_index = 1 + taskbar_count;
    }
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

static void log_pid(int pid, const char* name) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/os/proc_list.txt", g_project_root);
    FILE* f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%d %s\n", pid, name);
    fclose(f);
}

static void sync_active_gui_index_from_display(void) {
    char path[MAX_PATH];
    FILE *f = NULL;
    char line[64];
    int idx;

    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_index.txt", g_project_root);
    f = fopen(path, "r");
    if (!f) {
        return;
    }

    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return;
    }
    fclose(f);

    idx = atoi(line);
    if (idx > 0 && idx <= g_max_index) {
        if (g_active_gui_index != idx) {
            log_alpha("Synced active_gui_index from display: %d -> %d", g_active_gui_index, idx);
        }
        g_active_gui_index = idx;
    } else if (idx > 0) {
        /* Launcher rows can live outside the window-chrome bound.
         * Keep the manager max at least as large as the live parser focus
         * so Enter can dispatch the same selection the layout exported. */
        if (g_max_index < idx) {
            log_alpha("Raised max index for launcher sync: %d -> %d", g_max_index, idx);
            g_max_index = idx;
        }
        if (g_active_gui_index != idx) {
            log_alpha("Synced active_gui_index from display beyond max: %d -> %d", g_active_gui_index, idx);
        }
        g_active_gui_index = idx;
    }
}

static void launch_rgb_pipeline(void) {
    char daemon_path[MAX_PATH];
    char gl_path[MAX_PATH];

    snprintf(daemon_path, sizeof(daemon_path), "%s/projects/wraith-alpha/plugins/+x/wraith_rgb_daemon.+x", g_project_root);
    snprintf(gl_path, sizeof(gl_path), "%s/projects/wraith-alpha/ops/+x/wraith_gl.+x", g_project_root);

    log_alpha("Launching RGB Pipeline...");

    /* Launch Middle Fork Daemon */
#ifndef _WIN32
    pid_t d_pid = fork();
    if (d_pid == 0) {
        execl(daemon_path, daemon_path, NULL);
        _exit(127);
    } else if (d_pid > 0) {
        g_rgb_daemon_pid = d_pid;
        log_pid(d_pid, "wraith_rgb_daemon");
    }
#else
    int d_pid = _spawnl(_P_DETACH, daemon_path, daemon_path, NULL);
    if (d_pid > 0) {
        g_rgb_daemon_pid = d_pid;
        log_pid(d_pid, "wraith_rgb_daemon");
    }
#endif

    /* Launch GL Presenter */
#ifndef _WIN32
    pid_t g_pid = fork();
    if (g_pid == 0) {
        execl(gl_path, gl_path, NULL);
        _exit(127);
    } else if (g_pid > 0) {
        g_gl_pid = g_pid;
        log_pid(g_pid, "wraith_gl");
    }
#else
    int g_pid = _spawnl(_P_DETACH, gl_path, gl_path, NULL);
    if (g_pid > 0) {
        g_gl_pid = g_pid;
        log_pid(g_pid, "wraith_gl");
    }
#endif
}

static void route_input(int key) {
    int changed = 0;

    archive_input(key);
    normalize_registry();
    recompute_nav_bounds();
    sync_active_gui_index_from_display();
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
        sync_active_gui_index_from_display();
        log_alpha("Enter received: gui_index=%d max_index=%d", g_active_gui_index, g_max_index);
        if (dispatch_menu_index(g_active_gui_index)) {
            return;
        }
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
    long last_action_pos = 0;
    char keyboard_hist_path[MAX_PATH];
    char project_hist_path[MAX_PATH];
    char action_queue_path[MAX_PATH];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    resolve_root();
    atexit(cleanup_runtime);
    enable_mouse_mode();
    bootstrap_fresh_session();

    snprintf(keyboard_hist_path, sizeof(keyboard_hist_path), "%s/pieces/keyboard/history.txt", g_project_root);
    snprintf(project_hist_path, sizeof(project_hist_path), "%s/projects/wraith-alpha/session/history.txt", g_project_root);
    snprintf(action_queue_path, sizeof(action_queue_path), "%s/projects/wraith-alpha/session/desktop_actions.txt", g_project_root);

    if (stat(keyboard_hist_path, &st) == 0) {
        last_keyboard_pos = st.st_size;
    }
    if (stat(project_hist_path, &st) == 0) {
        last_project_pos = st.st_size;
    }
    if (stat(action_queue_path, &st) == 0) {
        last_action_pos = st.st_size;
    }

    sync_registry_from_disk();
    recompute_nav_bounds();
    update_state(0);
    trigger_render();

    launch_rgb_pipeline();

    log_alpha("Wraith-Alpha Manager starting in %s", g_project_root);

    while (!g_shutdown) {
        sync_active_gui_index_from_display();
        process_history_file(keyboard_hist_path, &last_keyboard_pos, "Keyboard");
        process_history_file(project_hist_path, &last_project_pos, "Project");
        process_history_file(action_queue_path, &last_action_pos, "Desktop Action");
        usleep(16667);
    }

    log_alpha("Wraith-Alpha Manager shutting down.");
    return 0;
}
