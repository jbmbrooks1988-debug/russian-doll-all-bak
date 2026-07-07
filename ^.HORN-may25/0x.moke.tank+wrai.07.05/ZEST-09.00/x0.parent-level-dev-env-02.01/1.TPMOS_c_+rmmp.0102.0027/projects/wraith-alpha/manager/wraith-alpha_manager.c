#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#define MAX_LINE 4096
#define MAX_PATH 1024
#define MAX_WINDOWS 20
#define MAX_LAUNCHERS 32

/* SINGLE SOURCE OF TRUTH for the window chrome row's fixed icon buttons
 * (everything after the title button: o/-/x/&).
 *
 * Why this exists (2026-07-06): before this, ASCII (build_desktop_shell_markup())
 * and GL (write_semantic_projection_files()) each independently hardcoded
 * their own copy of "there are 4 chrome buttons, numbered 1-4" (or, in one
 * reverted attempt, GL tried to dodge the count entirely with a
 * trailing/computed nav slot). Two independent numbering systems for the
 * SAME visual buttons drifted out of sync three times in one session:
 * once when a new button (then "g", now "&") got nav=0 in GL while ASCII
 * auto-numbered it into the sequence anyway; once when a "trailing nav
 * slot" gave it a big GL number while ASCII's parse-order numbering gave
 * it a small one; once when a stale g_max_index made its trailing number
 * collide with the title button's own nav=1. See zest-09.00-handoff.md's
 * chrome-button appendix for the full incident history -- do not "fix"
 * chrome numbering again without reading it first.
 *
 * The fix: ONE array, consumed by BOTH renderers via the SAME loop index,
 * so nav 2..(1+CHROME_ICON_COUNT) is defined exactly once, in exactly one
 * place, for exactly one reason. There is no longer any separate
 * "launcher_start"/"chrome slot count" literal anywhere in this file that
 * could independently drift from this array -- CHROME_CONTENT_START
 * (below) is derived from its length, and every other function that used
 * to hardcode "4" or "5" reads that macro instead.
 *
 * IMPORTANT INVARIANT if you ever add a 5th icon here: every wraith
 * sub-project with its own hand-authored session/scene.objects.pdl
 * (confirmed as of this session: piececraft-wraith, wraith-browser,
 * chtmgl-wraith, wraith-ed, web-cam, chtmgl-video-isolate, fs,
 * wraith-3d-cube, screen-record -- each generates that file from a
 * hardcoded nav=N sequence inside its own ops/src/wraith_project_input.c,
 * confirmed via grep, NOT dynamically computed) MUST start its own nav
 * range at CHROME_CONTENT_START or later. These are separate C binaries
 * that cannot share a header (TPMOS convention: no shared .h files, every
 * .c is a self-contained island) or read this macro directly, so growing
 * this array again means manually re-checking and re-bumping all nine of
 * those files' own hardcoded nav=N literals -- exactly what this session
 * had to do when CHROME_ICON_COUNT went from 3 to 4. There is currently no
 * file-based shared constant those nine projects read instead of
 * hardcoding the number themselves; making that real (so growing chrome
 * again doesn't require a nine-file audit) is flagged as a worthwhile
 * follow-up, not done this session -- see WRAITH_RGB_ARCHITECTURE.md. */
typedef enum {
    CHROME_ACTION_FOCUS,            /* just selects it (o's current behavior) */
    CHROME_ACTION_MINIMIZE,
    CHROME_ACTION_CLOSE,
    CHROME_ACTION_OPEN_WINDOW_GEOM
} ChromeIconAction;

typedef struct {
    char glyph;             /* single-character label, e.g. 'o', '-', 'x', '&' */
    const char *semantic;   /* id/source_ref suffix, e.g. "chrome_open" */
    ChromeIconAction action;
} ChromeIcon;

static const ChromeIcon g_chrome_icons[] = {
    { 'o', "chrome_open",  CHROME_ACTION_FOCUS },
    { '-', "chrome_min",   CHROME_ACTION_MINIMIZE },
    { 'x', "chrome_close", CHROME_ACTION_CLOSE },
    { '&', "chrome_geom",  CHROME_ACTION_OPEN_WINDOW_GEOM },
};
#define CHROME_ICON_COUNT ((int)(sizeof(g_chrome_icons) / sizeof(g_chrome_icons[0])))
/* nav 1 is always the title button (special-cased below, not in the array
   above -- it substitutes window->title text, not a static glyph, and its
   action is always CHROME_ACTION_FOCUS). Icons occupy nav
   2..(1+CHROME_ICON_COUNT). Anything after chrome -- launcher/taskbar
   slots in GL, embedded body content's own nav range, project-owned
   scene.objects.pdl's nav range -- starts at CHROME_CONTENT_START. */
#define CHROME_CONTENT_START (2 + CHROME_ICON_COUNT)

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
    int x, y, width, height;   /* live geometry; 0 until read from project.pdl or dragged/resized */
} Window;

typedef struct {
    char dir_name[64];
    char command[128];
    char id_prefix[64];
    char title_prefix[64];
    char project_id[128];
    char entry_layout[MAX_PATH];
    char display_label[64];
} WraithLauncher;

static volatile int g_shutdown = 0;
static char g_project_root[MAX_PATH] = ".";
static Window g_windows[MAX_WINDOWS];
static int g_window_count = 0;
static int g_next_instance_no = 1;
static int g_active_window_slot = -1;
static int g_active_gui_index = 1;
/* Whether the element g_active_gui_index points at is genuinely ACTIVE
 * (a cli_io field accepting keystrokes, wraith_parser_alpha.c's
 * active_index != -1), as opposed to merely focused. Synced from
 * pieces/display/active_gui_is_typing.txt in sync_active_gui_index_from_display() --
 * see that file's own comment in wraith_parser_alpha.c's export_active_index()
 * for why this couldn't be derived from active_gui_index.txt alone
 * (2fix-july6.txt, bug 3). Used only to pick "^" vs ">" for cli_io nav
 * glyphs in emit_embedded_line_objects() -- unrelated to the existing,
 * separate "^" used for map-control/INTERACT mode elsewhere in this file. */
static int g_active_gui_is_typing = 0;
static int g_max_index = 1;
static int g_digit_accum = 0;
static int g_map_control_nav_index = 0;
static int g_mouse_x = 0;
static int g_mouse_y = 0;
static int g_mouse_btn = -1;
static int g_last_click_x = -1;
static int g_last_click_y = -1;
/* Camera-drag tracking: while the left button is held (btn 0 stream) in
   map-control mode, we forward the per-event pixel delta to the active
   project's op as a MOUSE_DRAG event (see handle_mouse). g_drag_anchored
   marks that we have a valid previous point to diff against. */
static int g_drag_anchored = 0;
static int g_drag_prev_x = 0;
static int g_drag_prev_y = 0;
static int g_presenter_ascii_mode = 0;
static char g_mouse_lock_path[MAX_PATH] = "";
static WraithLauncher g_launchers[MAX_LAUNCHERS];
static int g_launcher_count = 0;
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
static void purge_tracked_process_name(const char *name);
static void launch_default_terminal(void);
static void route_command(const char *cmd);
static void sync_active_gui_index_from_display(void);
static void bootstrap_fresh_session(void);
static void truncate_file(const char *rel_path);
static void reset_registry(void);
static void set_window_identity(Window *window, int instance_no);
static bool project_dir_for_window(const Window *window, char *out, size_t out_sz);
static int discover_launcher_projects(void);
static const WraithLauncher *find_launcher_by_command(const char *cmd);
static const WraithLauncher *find_terminal_launcher(void);
static void recompute_active_window_slot(void);
static Window *active_window(void);
static int count_launcher_methods(void);
static int count_project_nav_controls(void);
static int count_visible_windows(void);
static bool dispatch_menu_index(int menu_index);
static void open_window_geom_for_project(const char *target_project_id);
static bool dispatch_launcher_method_by_index(int launcher_idx);
static bool launch_wraith_project_command(const char *cmd);
static void launch_window_instance(const char *id_prefix, const char *title_prefix, const char *project_id);
static void set_project_map_control_in_dir(const char *project_dir, int enabled, int emit_history);
static void reset_project_view_from_default(const char *project_dir);
static void load_window_geometry_from_pdl(const char *project_dir, int *out_x, int *out_y, int *out_w, int *out_h);
static int resolve_frame_width(const Window *window, int layout_declared_width);
static void append_frame_border(char *out, size_t size, const char *label, int width);
static void resolve_window_content_origin(const Window *window, int *out_row_offset, int *out_col_offset);
static void append_with_origin_offset(char *out, size_t size, const char *raw, int row_offset, int col_offset);
static int extract_attr(const char *tag_start, const char *tag_end, const char *attr_name, char *out, size_t out_sz);
static void read_gui_state_value(const char *project_dir, const char *key, char *out, size_t out_sz);
static void emit_embedded_line_objects(FILE *objects, int *object_id, const Window *window,
                                       const char *window_chain, const char *line,
                                       int base_x, int base_y, int line_index, int *next_nav);
static void utf8_safe_truncate(const char *src, char *dst, size_t dst_sz);
static int count_scene_nav_controls(const char *project_dir);
static int count_embedded_body_nav_slots(const char *body_path);
static void reset_all_open_project_views_on_startup(void);
static void appendf(char *out, size_t size, const char *fmt, ...);
static void xml_escape_attr(const char *src, char *dst, size_t dst_sz);
static void append_markup_spaces(char *out, size_t out_sz, int count);
static void append_markup_text(char *out, size_t out_sz, const char *label, const char *fg, const char *bg);
static int debug_selector_ascii_index(void);
static int debug_selector_gl_index(void);
static void recompute_nav_bounds(void);
static void load_mouse_offset(int *offset_x, int *offset_y);
static void write_semantic_projection_files(void);
static void archive_receipt_history(void);
static void stop_project_runtime_by_rel(const char *project_rel, const char *stop_op_name);

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

static int line_kvp_value(const char *line, const char *key, char *out, size_t out_sz) {
    const char *start;
    const char *end;
    char needle[64];
    size_t len;

    if (!line || !key || !out || out_sz == 0) {
        return 0;
    }
    snprintf(needle, sizeof(needle), "%s=", key);
    start = strstr(line, needle);
    if (!start) {
        return 0;
    }
    start += strlen(needle);
    if (*start == '"') {
        start++;
        end = start;
        while (*end && *end != '"') {
            end++;
        }
    } else if (strcmp(key, "label") == 0 && *start == ' ' &&
               (start[1] == '\0' || start[1] == '\n' || start[1] == '\r')) {
        if (out_sz < 2) {
            return 0;
        }
        out[0] = ' ';
        out[1] = '\0';
        return 1;
    } else {
        end = start;
        while (*end && !isspace((unsigned char)*end)) {
            end++;
        }
    }
    len = (size_t)(end - start);
    if (len >= out_sz) {
        len = out_sz - 1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static int digits_int(int value) {
    int digits = 1;
    while (value >= 10) {
        value /= 10;
        digits++;
    }
    return digits;
}

static void sanitize_token(const char *src, char *dst, size_t dst_sz) {
    size_t out = 0;
    if (!src || !dst || dst_sz == 0) {
        return;
    }
    for (; *src && out + 1 < dst_sz; src++) {
        unsigned char ch = (unsigned char)*src;
        if (isalnum(ch)) {
            dst[out++] = (char)tolower(ch);
        } else if (ch == '-' || ch == ' ' || ch == '/' || ch == '_') {
            dst[out++] = '_';
        }
    }
    dst[out] = '\0';
}

static void titleize_token(const char *src, char *dst, size_t dst_sz) {
    size_t out = 0;
    bool new_word = true;
    if (!src || !dst || dst_sz == 0) {
        return;
    }
    for (; *src && out + 1 < dst_sz; src++) {
        unsigned char ch = (unsigned char)*src;
        if (ch == '-' || ch == '_' || ch == '/') {
            if (out > 0 && dst[out - 1] != ' ') {
                dst[out++] = ' ';
            }
            new_word = true;
            continue;
        }
        if (new_word) {
            dst[out++] = (char)toupper(ch);
            new_word = false;
        } else {
            dst[out++] = (char)tolower(ch);
        }
    }
    dst[out] = '\0';
}

static void read_pdl_value(const char *path, const char *key, char *dst, size_t dst_sz) {
    FILE *f;
    char line[MAX_LINE];

    if (!dst || dst_sz == 0) {
        return;
    }
    dst[0] = '\0';
    f = fopen(path, "r");
    if (!f) {
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, key)) {
            continue;
        }
        char *pipe = strchr(line, '|');
        if (!pipe) {
            continue;
        }
        pipe = strchr(pipe + 1, '|');
        if (!pipe) {
            continue;
        }
        char *val = trim_ws(pipe + 1);
        char *nl = strchr(val, '\n');
        if (nl) {
            *nl = '\0';
        }
        strncpy(dst, val, dst_sz - 1);
        dst[dst_sz - 1] = '\0';
        break;
    }

    fclose(f);
}

/* Reads project.pdl's WINDOW section (window_x/window_y/window_width/
   window_height keys). Any key missing from the file -- which is every
   project today, since this section doesn't exist yet anywhere -- leaves
   its output at 0. That's expected for this pass, not an error: KPI A1
   only wires up the read path, KPI A2 teaches the renderer to do
   anything with these values. */
static void load_window_geometry_from_pdl(const char *project_dir, int *out_x, int *out_y, int *out_w, int *out_h) {
    char pdl_path[MAX_PATH];
    char value[64];

    *out_x = 0;
    *out_y = 0;
    *out_w = 0;
    *out_h = 0;

    if (!project_dir || !project_dir[0]) {
        return;
    }

    snprintf(pdl_path, sizeof(pdl_path), "%s/project.pdl", project_dir);

    read_pdl_value(pdl_path, "window_x", value, sizeof(value));
    if (value[0] != '\0') {
        *out_x = atoi(value);
    }
    read_pdl_value(pdl_path, "window_y", value, sizeof(value));
    if (value[0] != '\0') {
        *out_y = atoi(value);
    }
    read_pdl_value(pdl_path, "window_width", value, sizeof(value));
    if (value[0] != '\0') {
        *out_w = atoi(value);
    }
    read_pdl_value(pdl_path, "window_height", value, sizeof(value));
    if (value[0] != '\0') {
        *out_h = atoi(value);
    }
}

/* Path A width resolution (window-geometry-render-plan-j5.md): precedence
   is project.pdl's saved WINDOW.width (always wins, it's the user's
   actual current geometry) > a layout-declared width (not wired yet --
   chtpm_parser.c's parse_attributes() is an explicit allow-list with no
   width/height attribute today; pass -1 for "none" until that lands as
   its own follow-up, since it touches the shared parser, not just this
   file) > this historical hardcoded default, which matches today's
   literal border length exactly so nothing changes for any project
   until it actually has a saved width. */
static int resolve_frame_width(const Window *window, int layout_declared_width) {
    const int historical_default = 96;
    if (window && window->width > 0) {
        return window->width;
    }
    if (layout_declared_width > 0) {
        return layout_declared_width;
    }
    return historical_default;
}

/* Builds one "+-LABEL----...----+" (or "+----...----+" when label is
   NULL/empty) border line of exactly `width` characters and appends it
   plus "<br/>" to `out`, matching the <br/>-terminated line convention
   every other appendf() call in this file already follows. Verified
   standalone against the two literal border strings this replaces
   (byte-for-byte identical at width=96) before landing here. */
static void append_frame_border(char *out, size_t size, const char *label, int width) {
    char line[256];
    int pos = 0;
    int i;
    int dash_count;

    if (width < 4) {
        width = 4;
    }
    if (width > (int)sizeof(line) - 8) {
        width = (int)sizeof(line) - 8;
    }

    line[pos++] = '+';
    if (label && label[0]) {
        pos += snprintf(line + pos, sizeof(line) - pos, "-%s", label);
    }
    dash_count = width - pos - 1;
    if (dash_count < 0) {
        dash_count = 0;
    }
    for (i = 0; i < dash_count; i++) {
        line[pos++] = '-';
    }
    line[pos++] = '+';
    line[pos] = '\0';

    appendf(out, size, "%s<br/>", line);
}

static int launcher_cmp(const void *a, const void *b) {
    const WraithLauncher *la = (const WraithLauncher *)a;
    const WraithLauncher *lb = (const WraithLauncher *)b;
    return strcmp(la->dir_name, lb->dir_name);
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
    char frame_path[MAX_PATH];
    char state_path[MAX_PATH];
    FILE *f;

    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/frame_changed.txt", g_project_root);
    f = fopen(frame_path, "a");
    if (f) {
        fprintf(f, "A\n");
        fclose(f);
    }

    /* Wraith shell uses dynamic state-backed markup such as desktop_shell_markup.
     * The parser must be nudged through its state-change seam as well so the
     * mother-terminal frame updates immediately, not only after a later input. */
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/state_changed.txt", g_project_root);
    f = fopen(state_path, "a");
    if (f) {
        fprintf(f, "WRAITH_STATE\n");
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
    stop_project_runtime_by_rel("projects/wraith-alpha/wraith-projects/web-cam", "wraith_webcam_capture.+x");
    stop_project_runtime_by_rel("projects/wraith-alpha/wraith-projects/screen-record", "wraith_screen_record.+x");
    cleanup_spawned_child(&g_gl_pid, "wraith_gl");
    cleanup_spawned_child(&g_rgb_daemon_pid, "wraith_rgb_daemon");
    remove_mouse_lock();
}

static void stop_project_runtime_by_rel(const char *project_rel, const char *stop_op_name) {
    char project_dir[MAX_PATH];
    char op_path[MAX_PATH];
    pid_t pid;
    int status;

    if (!project_rel || !project_rel[0] || !stop_op_name || !stop_op_name[0]) {
        return;
    }

    snprintf(project_dir, sizeof(project_dir), "%s/%s", g_project_root, project_rel);
    snprintf(op_path, sizeof(op_path), "%s/ops/+x/%s", project_dir, stop_op_name);
    if (access(op_path, X_OK) != 0) {
        return;
    }

    pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, "--stop", project_dir, NULL);
        _exit(127);
    }
    if (pid > 0) {
        waitpid(pid, &status, 0);
    }
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
    g_digit_accum = 0;
    recompute_active_window_slot();
    g_mouse_btn = -1;
    g_mouse_x = 0;
    g_mouse_y = 0;
    g_last_click_x = -1;
    g_last_click_y = -1;

    truncate_file("projects/wraith-alpha/session/history.txt");
    truncate_file("projects/wraith-alpha/session/history_archive.txt");
    truncate_file("projects/wraith-alpha/session/desktop_actions.txt");
    truncate_file("projects/wraith-alpha/session/input_focus.lock");
    truncate_file("projects/wraith-alpha/session/alpha_state.txt");
    truncate_file("projects/wraith-alpha/session/desktop_ui_state.txt");
    truncate_file("projects/wraith-alpha/manager/gui_state.txt");
    truncate_file("pieces/display/active_gui_index.txt");
    truncate_file("pieces/display/current_frame.txt");
    truncate_file("pieces/display/frame_changed.txt");
    truncate_file("projects/wraith-alpha/session/rgb/current_frame.rgba32");
    truncate_file("projects/wraith-alpha/session/rgb/current_frame.receipt.pdl");
    truncate_file("projects/wraith-alpha/session/rgb/gl_input.receipt.pdl");
    truncate_file("projects/wraith-alpha/session/rgb/rgb_frame_changed.txt");

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
    const WraithLauncher *launcher = find_terminal_launcher();
    if (launcher) {
        set_window_identity_custom(window, instance_no, launcher->id_prefix, launcher->title_prefix, launcher->project_id);
        return;
    }
    set_window_identity_custom(window, instance_no, "terminal", "Terminal", "wraith-alpha/wraith-projects/terminal");
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
    return discover_launcher_projects();
}

/* Author-declared nav controls in a project's own session/scene.objects.pdl
 * (tiles/entities, e.g. piececraft-wraith) -- these numbers are chosen
 * directly in that file (confirmed real example: piececraft-wraith's
 * controls are nav=5..22, contiguous), NOT dynamically assigned here.
 * Split out of count_project_nav_controls() (2fix.txt, 2026-07-05) so
 * write_semantic_projection_files() can also call it alone, to know
 * where the scene-declared range ends before assigning nav values to
 * embedded body-tag objects. */
static int count_scene_nav_controls(const char *project_dir) {
    char scene_path[MAX_PATH];
    char line[1024];
    char value[256];
    FILE *f;
    int count = 0;

    snprintf(scene_path, sizeof(scene_path), "%s/session/scene.objects.pdl", project_dir);
    f = fopen(scene_path, "r");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || strncmp(line, "OBJECT", 6) != 0) {
            continue;
        }
        if (!line_kvp_value(line, "tag", value, sizeof(value)) || strcmp(value, "control") != 0) {
            continue;
        }
        if (line_kvp_value(line, "nav", value, sizeof(value)) && atoi(value) > 0) {
            count++;
        }
    }
    fclose(f);
    return count;
}

/* Counts nav-worthy chunks (any embedded tag with a non-empty label=
 * attribute) across every session/wraith_body.txt line starting with
 * '<' -- mirrors emit_embedded_line_objects()'s own chunk-detection
 * criterion exactly (same extract_attr() "label" check), so this count
 * and what actually gets emitted can't drift apart. Added so
 * g_max_index / launcher_start correctly reserve slots for these now-real
 * GL objects instead of leaving them at nav=0 (2fix.txt, 2026-07-05). */
static int count_embedded_body_nav_slots(const char *body_path) {
    FILE *f;
    char line[256];
    int count = 0;

    f = fopen(body_path, "r");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        const char *p;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '<') {
            continue;
        }
        p = line;
        while (*p) {
            if (strncmp(p, "<br", 3) == 0) {
                const char *tag_end = strchr(p, '>');
                if (!tag_end) break;
                p = tag_end + 1;
                continue;
            }
            if (*p == '<') {
                char label[256];
                const char *tag_end = strchr(p, '>');
                if (!tag_end) break;
                extract_attr(p, tag_end, "label", label, sizeof(label));
                if (label[0]) {
                    count++;
                }
                p = tag_end + 1;
            } else {
                while (*p && *p != '<') {
                    p++;
                }
            }
        }
    }
    fclose(f);
    return count;
}

static int count_project_nav_controls(void) {
    Window *window = active_window();
    char project_dir[MAX_PATH];
    char body_path[MAX_PATH];

    if (!window) {
        return 0;
    }
    if (!project_dir_for_window(window, project_dir, sizeof(project_dir))) {
        return 0;
    }
    snprintf(body_path, sizeof(body_path), "%s/session/wraith_body.txt", project_dir);
    return count_scene_nav_controls(project_dir) + count_embedded_body_nav_slots(body_path);
}

static int discover_launcher_projects(void) {
    char projects_dir[MAX_PATH];
    DIR *dir;
    struct dirent *entry;
    static char last_signature[512] = "";
    char signature[512];
    size_t sig_used = 0;
    int i;

    g_launcher_count = 0;
    snprintf(projects_dir, sizeof(projects_dir), "%s/projects/wraith-alpha/wraith-projects", g_project_root);
    dir = opendir(projects_dir);
    if (!dir) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL && g_launcher_count < MAX_LAUNCHERS) {
        char project_dir[MAX_PATH];
        char pdl_path[MAX_PATH];
        struct stat st;
        WraithLauncher *launcher;
        char project_id[128];
        char title[64];

        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(project_dir, sizeof(project_dir), "%s/%s", projects_dir, entry->d_name);
        if (stat(project_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }

        snprintf(pdl_path, sizeof(pdl_path), "%s/project.pdl", project_dir);
        if (stat(pdl_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        launcher = &g_launchers[g_launcher_count];
        memset(launcher, 0, sizeof(*launcher));

        strncpy(launcher->dir_name, entry->d_name, sizeof(launcher->dir_name) - 1);
        sanitize_token(entry->d_name, launcher->id_prefix, sizeof(launcher->id_prefix));
        titleize_token(entry->d_name, launcher->title_prefix, sizeof(launcher->title_prefix));
        strncpy(launcher->display_label, entry->d_name, sizeof(launcher->display_label) - 1);
        snprintf(launcher->command, sizeof(launcher->command), "DESKTOP_ACTION:launch_%s", launcher->id_prefix);

        read_pdl_value(pdl_path, "project_id", project_id, sizeof(project_id));
        read_pdl_value(pdl_path, "entry_layout", launcher->entry_layout, sizeof(launcher->entry_layout));
        read_pdl_value(pdl_path, "title", title, sizeof(title));

        if (project_id[0] != '\0') {
            strncpy(launcher->project_id, project_id, sizeof(launcher->project_id) - 1);
        } else {
            snprintf(launcher->project_id, sizeof(launcher->project_id), "wraith-alpha/wraith-projects/%s", entry->d_name);
        }
        if (title[0] != '\0') {
            strncpy(launcher->title_prefix, title, sizeof(launcher->title_prefix) - 1);
        }

        g_launcher_count++;
    }

    closedir(dir);
    qsort(g_launchers, g_launcher_count, sizeof(WraithLauncher), launcher_cmp);

    signature[0] = '\0';
    for (i = 0; i < g_launcher_count; i++) {
        int wrote = snprintf(signature + sig_used,
            sizeof(signature) - sig_used,
            "%s%s",
            i == 0 ? "" : ",",
            g_launchers[i].dir_name);
        if (wrote < 0 || (size_t)wrote >= sizeof(signature) - sig_used) {
            sig_used = sizeof(signature) - 1;
            break;
        }
        sig_used += (size_t)wrote;
    }

    if (strcmp(signature, last_signature) != 0) {
        log_alpha("Discovered %d Wraith launcher projects: %s",
            g_launcher_count,
            signature[0] != '\0' ? signature : "(none)");
        strncpy(last_signature, signature, sizeof(last_signature) - 1);
        last_signature[sizeof(last_signature) - 1] = '\0';
    }

    return g_launcher_count;
}

static const WraithLauncher *find_launcher_by_command(const char *cmd) {
    char normalized[256];
    int i;
    discover_launcher_projects();
    for (i = 0; i < g_launcher_count; i++) {
        if (strcmp(g_launchers[i].command, cmd) == 0) {
            return &g_launchers[i];
        }
    }
    sanitize_token(cmd, normalized, sizeof(normalized));
    for (i = 0; i < g_launcher_count; i++) {
        char launcher_normalized[256];
        sanitize_token(g_launchers[i].command, launcher_normalized, sizeof(launcher_normalized));
        if (strcmp(launcher_normalized, normalized) == 0) {
            return &g_launchers[i];
        }
    }
    return NULL;
}

static const WraithLauncher *find_terminal_launcher(void) {
    int i;
    discover_launcher_projects();
    for (i = 0; i < g_launcher_count; i++) {
        if (strcmp(g_launchers[i].dir_name, "terminal") == 0) {
            return &g_launchers[i];
        }
    }
    return NULL;
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

static pid_t launch_project_manager(const char *root, const char *project_id) {
    char pdl_path[MAX_PATH], manager_path[MAX_PATH], line[MAX_LINE];
    FILE *f;

    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", root, project_id);
    f = fopen(pdl_path, "r");
    if (!f) return -1;

    manager_path[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        char *pipe;
        if (strstr(line, "module_path") || strstr(line, "manager")) {
            pipe = strrchr(line, '|');
            if (pipe) {
                char *val = trim_ws(pipe + 1);
                if (val && val[0]) {
                    if (val[0] == '/') {
                        strncpy(manager_path, val, MAX_PATH - 1);
                    } else {
                        snprintf(manager_path, MAX_PATH, "%s/%s", root, val);
                    }
                    break;
                }
            }
        }
    }
    fclose(f);

    if (manager_path[0] == '\0') return -1;
    if (access(manager_path, X_OK) != 0) return -1;

    pid_t child = fork();
    if (child == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(manager_path, manager_path, (char *)NULL);
        _exit(127);
    }
    if (child > 0) {
        log_alpha("Launched manager for %s (pid=%d)", project_id, (int)child);
        /* Same tracking convention already used for wraith_gl/wraith_rgb_daemon
           below, and the SAME shared pieces/os/proc_list.txt the orchestrator
           (pieces/chtpm/plugins/orchestrator.c) uses for the other ~40
           chtpm-parser-launched projects -- confirmed same file, same
           "%d %s\n" format. Without this, every per-project manager this
           function ever launches (settings, window-geom, piececraft-wraith,
           literally all of them) was invisible to any tracked-process
           cleanup: log_pid() previously only got called for wraith_gl and
           wraith_rgb_daemon, so quitting wraith-alpha_manager.c left every
           spawned project manager running as an orphan. See
           kill_all_tracked_processes() and its call site for the other half
           of this fix. */
        log_pid((int)child, project_id);
    }
    return child;
}

static void ensure_project_manager(const char *root, const char *project_id) {
    char lockfile[MAX_PATH], session_dir[MAX_PATH];
    FILE *lf;
    pid_t locked_pid = 0;
    bool alive = false;

    snprintf(session_dir, sizeof(session_dir), "%s/projects/%s/session", root, project_id);
    snprintf(lockfile, sizeof(lockfile), "%s/.manager.lock", session_dir);

    /* A lockfile merely existing doesn't mean its manager is still running --
       the process could have died (crash, kill_all.sh, manual test run) any
       time after the lock was written. Checking the stored pid with kill(pid,0)
       is what actually tells us whether to relaunch, vs. trusting a lock that
       could be stale from a prior session. */
    lf = fopen(lockfile, "r");
    if (lf) {
        if (fscanf(lf, "%d", &locked_pid) == 1 && locked_pid > 0) {
            if (kill(locked_pid, 0) == 0 || errno == EPERM) {
                alive = true;
            }
        }
        fclose(lf);
    }

    if (!alive) {
        pid_t child_pid = launch_project_manager(root, project_id);
        usleep(100000);
        if (child_pid > 0) {
            lf = fopen(lockfile, "w");
            if (lf) {
                fprintf(lf, "%d\n", (int)child_pid);
                fclose(lf);
            }
        }
    }
}

static void launch_window_instance(const char *id_prefix, const char *title_prefix, const char *project_id) {
    Window *window;
    char project_dir[MAX_PATH];
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
    if (project_dir_for_window(window, project_dir, sizeof(project_dir))) {
        set_project_map_control_in_dir(project_dir, 0, 0);
        /* Reset camera/view to the project's declared default on every open,
           so it never reopens at the last session's orbited angle. */
        reset_project_view_from_default(project_dir);
        /* KPI A1: load this project's persisted window geometry, if any.
           Almost every project.pdl has no WINDOW section yet, so this is
           expected to yield 0/0/0/0 for now -- read-path only, nothing
           renders from these fields until KPI A2. */
        load_window_geometry_from_pdl(project_dir, &window->x, &window->y, &window->width, &window->height);
    }
    g_map_control_nav_index = 0;
    window->state = WSTATE_OPEN;
    normalize_registry();
    g_active_gui_index = 1;

    /* Reset settings' embedded-page state to the top menu on every window open.
       state_changed.txt is append-only (correct marker-file pattern), but
       write_wraith_body() reads the last line as "current page". Without this,
       a fresh window open re-reads whatever the last test session left as the
       final line, skipping the menu and picker entirely. Only reset for the
       settings project itself; other projects have no multi-page state. */
    if (project_id && strcmp(project_id, "wraith-alpha/wraith-projects/settings") == 0) {
        char state_changed_path[MAX_PATH];
        FILE *f;
        snprintf(state_changed_path, sizeof(state_changed_path),
                 "%s/projects/wraith-alpha/wraith-projects/settings/session/state_changed.txt", g_project_root);
        f = fopen(state_changed_path, "a");
        if (f) {
            fprintf(f, "settings\n");
            fclose(f);
        }
    }

    ensure_project_manager(g_project_root, project_id);

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

static int debug_selector_ascii_index(void) {
    return g_max_index - 1;
}

static int debug_selector_gl_index(void) {
    return g_max_index;
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

static void xml_escape_attr(const char *src, char *dst, size_t dst_sz) {
    size_t out = 0;
    if (!dst || dst_sz == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }
    while (*src && out + 1 < dst_sz) {
        const char *rep = NULL;
        switch (*src) {
            case '&': rep = "&amp;"; break;
            case '"': rep = "&quot;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            default: break;
        }
        if (rep) {
            size_t len = strlen(rep);
            if (out + len >= dst_sz) {
                break;
            }
            memcpy(dst + out, rep, len);
            out += len;
        } else {
            dst[out++] = *src;
        }
        src++;
    }
    dst[out] = '\0';
}

static void append_markup_spaces(char *out, size_t out_sz, int count) {
    char spaces[128];
    if (!out || out_sz == 0 || count <= 0) {
        return;
    }
    if (count >= (int)sizeof(spaces)) {
        count = (int)sizeof(spaces) - 1;
    }
    memset(spaces, ' ', (size_t)count);
    spaces[count] = '\0';
    appendf(out, out_sz, "<text label=\"%s\" />", spaces);
}

static void append_markup_text(char *out, size_t out_sz, const char *label, const char *fg, const char *bg) {
    char escaped[768];
    if (!label || !label[0]) {
        return;
    }
    xml_escape_attr(label, escaped, sizeof(escaped));
    if ((fg && fg[0]) || (bg && bg[0])) {
        appendf(out, out_sz, "<text fg=\"%s\" bg=\"%s\" label=\"%s\" />", fg && fg[0] ? fg : "default", bg && bg[0] ? bg : "default", escaped);
    } else {
        appendf(out, out_sz, "<text label=\"%s\" />", escaped);
    }
}

static bool active_window_is_terminal(const Window *window) {
    return window && strcmp(window->project_id, "wraith-alpha/wraith-projects/terminal") == 0;
}

static bool project_dir_for_window(const Window *window, char *out, size_t out_sz) {
    const char *prefix = "wraith-alpha/wraith-projects/";
    const char *name;

    if (!window || !out || out_sz == 0) {
        return false;
    }
    out[0] = '\0';
    if (strncmp(window->project_id, prefix, strlen(prefix)) != 0) {
        return false;
    }
    name = window->project_id + strlen(prefix);
    /* No longer rejects a nested project_id (e.g.
       "settings/window-geom") -- that restriction assumed every wraith
       sub-project lives one level deep, which broke opening
       window-geom as its own standalone Window (its project_id is
       nested under settings/ purely for on-disk organization, per
       settings-hub-window-geom-design-j5.md). ".." traversal is the
       actual security boundary and is still rejected; a plain "/" is
       just directory nesting and is fine. */
    if (!name[0] || strstr(name, "..")) {
        return false;
    }
    snprintf(out, out_sz, "%s/projects/wraith-alpha/wraith-projects/%s", g_project_root, name);
    return true;
}

/* Found 2026-07-05, reported live by the user (piececraft-wraith's
 * box-drawing header showed a corrupted replacement character instead
 * of its border). Root cause: printf's ".83s" precision (used
 * just below to fit body text into an 83-wide column) truncates by
 * BYTES, not by displayed characters. Box-drawing glyphs like the
 * "╔══...══╗" border are 3-byte UTF-8 sequences -- confirmed the actual
 * line is 46 visible characters but 138 UTF-8 bytes, so an 83-byte cut
 * lands mid-character, producing an invalid, truncated byte sequence
 * (which terminals render as the replacement character). Pre-existing,
 * not introduced today -- this format string predates this session.
 * This helper truncates to at most `dst_sz - 1` BYTES same as before,
 * but backs up to the last complete UTF-8 character boundary first, so
 * a too-long multi-byte line gets cut CLEANLY (fewer glyphs shown) not
 * CORRUPTED (a broken glyph shown). It does not make long UTF-8 lines
 * fit -- that's still a real content-length constraint -- it only
 * removes the corruption when they don't fit. */
static void utf8_safe_truncate(const char *src, char *dst, size_t dst_sz) {
    size_t max_bytes = dst_sz - 1;
    size_t src_len = strlen(src);
    size_t copy_len = src_len < max_bytes ? src_len : max_bytes;

    if (copy_len < src_len) {
        while (copy_len > 0 && ((unsigned char)src[copy_len] & 0xC0) == 0x80) {
            copy_len--;
        }
    }
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

static int append_project_probe_body(char *out, size_t size, const Window *window) {
    char project_dir[MAX_PATH];
    char body_path[MAX_PATH];
    char line[256];
    FILE *body;
    int count = 0;

    if (!window) {
        return 0;
    }

    if (project_dir_for_window(window, project_dir, sizeof(project_dir))) {
        snprintf(body_path, sizeof(body_path), "%s/session/wraith_body.txt", project_dir);
        body = fopen(body_path, "r");
        if (body) {
            /* Was capped at 12, which silently truncated piececraft-wraith's
               21-line body (header + full 10-row map grid + separators +
               Position + CONTROLS) after the 8th map row in every mode —
               this is what made 3D look "more broken" than 2D: 2D's
               truncated-but-mostly-visible grid still read as a map, 3D's
               (at the time) grid-less body was entirely eaten by this cap.
               appendf() is itself bounds-safe against `out`/`size` (checks
               remaining capacity before each append), so raising this is
               just "don't discard real content the project actually
               wrote" — no overflow risk. */
            while (fgets(line, sizeof(line), body) && count < 40) {
                line[strcspn(line, "\r\n")] = '\0';
                if (line[0] == '<') {
                    appendf(out, size, "%s", line);
                } else {
                    /* 84 = the .83s precision below + 1 for the null
                       terminator -- pre-truncating here to the SAME
                       83-byte limit, but UTF-8-safely, means the
                       ".83s" precision that follows never has to cut
                       anything itself (the string is already <= 83
                       bytes), so it can't land mid-character anymore. */
                    char safe_line[84];
                    utf8_safe_truncate(line, safe_line, sizeof(safe_line));
                    appendf(out, size, "| |  %-83.83s |<br visibility=\"${desktop_active_window_body_visible}\" />", safe_line);
                }
                count++;
            }
            fclose(body);
            if (count > 0) {
                return count;
            }
        }
    }
    return 0;
}

static int append_project_probe_scene_markup(char *out, size_t size, const Window *window, int max_lines) {
    typedef struct {
        char tag[32];
        char label[256];
        char action[128];
        char fg[16];
        char bg[16];
        int nav;
        int x;
        int y;
        int w;
    } SceneShellItem;

    char project_dir[MAX_PATH];
    char scene_path[MAX_PATH];
    char line[1024];
    SceneShellItem items[2048];
    char row_buf[8192];
    FILE *scene;
    int item_count = 0;
    int count = 0;
    const int row_origin_x = 3;
    const int row_width = 83;
    int min_y = 0;
    int max_y = 0;
    int y;

    int cmp_scene_shell_item(const void *a, const void *b) {
        const SceneShellItem *ia = (const SceneShellItem *)a;
        const SceneShellItem *ib = (const SceneShellItem *)b;
        if (ia->y != ib->y) return ia->y - ib->y;
        if (ia->x != ib->x) return ia->x - ib->x;
        return ia->nav - ib->nav;
    }

    if (!out || !window || max_lines <= 0) {
        return 0;
    }
    if (!project_dir_for_window(window, project_dir, sizeof(project_dir))) {
        return 0;
    }

    snprintf(scene_path, sizeof(scene_path), "%s/session/scene.objects.pdl", project_dir);
    scene = fopen(scene_path, "r");
    if (!scene) {
        return 0;
    }

    while (fgets(line, sizeof(line), scene) && item_count < (int)(sizeof(items) / sizeof(items[0]))) {
        SceneShellItem *item = &items[item_count];
        char value[256];

        memset(item, 0, sizeof(*item));

        if (line[0] == '#' || strncmp(line, "OBJECT", 6) != 0) {
            continue;
        }
        if (line_kvp_value(line, "tag", value, sizeof(value))) snprintf(item->tag, sizeof(item->tag), "%s", value);
        if (line_kvp_value(line, "label", value, sizeof(value))) snprintf(item->label, sizeof(item->label), "%s", value);
        if (line_kvp_value(line, "action", value, sizeof(value))) snprintf(item->action, sizeof(item->action), "%s", value);
        if (line_kvp_value(line, "fg", value, sizeof(value))) snprintf(item->fg, sizeof(item->fg), "%s", value);
        if (line_kvp_value(line, "bg", value, sizeof(value))) snprintf(item->bg, sizeof(item->bg), "%s", value);
        if (line_kvp_value(line, "nav", value, sizeof(value))) item->nav = atoi(value);
        if (line_kvp_value(line, "y", value, sizeof(value))) item->y = atoi(value);
        if (line_kvp_value(line, "x", value, sizeof(value))) item->x = atoi(value);
        if (line_kvp_value(line, "w", value, sizeof(value))) item->w = atoi(value);

        if (!item->label[0]) {
            continue;
        }
        if (strcmp(item->tag, "text") != 0 && strcmp(item->tag, "control") != 0) {
            continue;
        }
        item_count++;
    }

    fclose(scene);
    if (item_count <= 0) {
        return 0;
    }

    qsort(items, item_count, sizeof(items[0]), cmp_scene_shell_item);
    min_y = items[0].y;
    max_y = items[item_count - 1].y;

    for (y = min_y; y <= max_y && count < max_lines; y++) {
        int i;
        int current_x = 0;
        int row_has_content = 0;

        row_buf[0] = '\0';
        for (i = 0; i < item_count; i++) {
            SceneShellItem *item = &items[i];
            int rel_x;
            int label_len;
            int visible_width;

            if (item->y != y) {
                continue;
            }
            rel_x = item->x - row_origin_x;
            if (rel_x < 0) {
                rel_x = current_x;
            }
            if (rel_x > row_width) {
                continue;
            }
            if (rel_x > current_x) {
                append_markup_spaces(row_buf, sizeof(row_buf), rel_x - current_x);
                current_x = rel_x;
            }

            label_len = (int)strlen(item->label);
            if (strcmp(item->tag, "control") == 0 && item->nav > 0 && item->action[0] && strcmp(item->action, "-") != 0) {
                char escaped_label[512];
                xml_escape_attr(item->label, escaped_label, sizeof(escaped_label));
                appendf(row_buf, sizeof(row_buf), "<button compact=\"true\" fg=\"%s\" bg=\"%s\" label=\"%s\" onClick=\"%s\" visibility=\"${desktop_active_window_body_visible}\" />",
                    item->fg[0] ? item->fg : "default",
                    item->bg[0] ? item->bg : "default",
                    escaped_label,
                    item->action);
                visible_width = 8 + digits_int(item->nav) + label_len;
            } else {
                append_markup_text(row_buf, sizeof(row_buf), item->label, item->fg, item->bg);
                visible_width = label_len;
                if (item->w > visible_width) {
                    visible_width = item->w;
                }
            }
            row_has_content = 1;
            if (visible_width > 0) {
                current_x = rel_x + visible_width;
            }
        }

        if (!row_has_content) {
            continue;
        }

        appendf(out, size, "| |  %s |<br visibility=\"${desktop_active_window_body_visible}\" />", row_buf);
        count++;
    }

    return count;
}

static int project_probe_body_lines(const Window *window, const char **lines, int max_lines) {
    /* Was char[16][160] -- silently truncated any line past 159 chars.
     * Confirmed live (2026-07-05 review): settings' own embedded body
     * line (231 chars, <text label="..." /><button label="..." href="..." /><text .../><br/>)
     * got cut mid-attribute on the trailing border <text> tag. Landed
     * harmlessly there only by luck (the actual button chunk fit inside
     * the old 159-char limit; emit_embedded_line_objects()'s malformed-tag
     * guard silently dropped the cut-off trailing tag rather than
     * misrendering). A longer href path or project name would have cut
     * INTO the button itself. Widened to match the 256-byte read buffer
     * immediately below, so this storage step adds no truncation beyond
     * what fgets() already imposes -- not a full fix for arbitrarily long
     * body lines (that ceiling is fgets()'s own 256-byte read buffer,
     * shared with ASCII mode's append_project_probe_body(), out of scope
     * for tonight), just removes an unnecessary SECOND, smaller ceiling
     * stacked on top of it. */
    /* 16 -> 40: matches ASCII mode's append_project_probe_body(), which
       was raised to 40 for the exact same reason (see that function's own
       comment, ~line 1557) -- a body longer than the cap gets silently
       cut off in GL only, with no error, while ASCII shows all of it.
       Confirmed live this session: window-geom's standalone editor body
       is 20 lines (header/cli_io/blank + the "Or use buttons" text at
       line 12, with the actual [-]/[+] buttons starting at line 13) --
       the call site passing max_lines=12 cut the body off in GL one line
       before the buttons even started, so they were never read into
       project_lines[] at all, let alone rendered. This wasn't a rendering
       bug or a nav bug; the button objects were never generated because
       their source lines were never read. */
    static char body_lines[40][256];
    char project_dir[MAX_PATH];
    char body_path[MAX_PATH];
    FILE *body;
    char line[256];
    int count = 0;

    if (!window || !lines || max_lines <= 0) {
        return 0;
    }
    if (max_lines > 40) {
        max_lines = 40;
    }
    if (project_dir_for_window(window, project_dir, sizeof(project_dir))) {
        snprintf(body_path, sizeof(body_path), "%s/session/wraith_body.txt", project_dir);
        body = fopen(body_path, "r");
        if (body) {
            while (fgets(line, sizeof(line), body) && count < max_lines) {
                line[strcspn(line, "\r\n")] = '\0';
                snprintf(body_lines[count], sizeof(body_lines[count]), "%.255s", line);
                lines[count] = body_lines[count];
                count++;
            }
            fclose(body);
        }
    }
    if (count > 0) {
        return count;
    }
    snprintf(body_lines[0], sizeof(body_lines[0]), "Project: %.120s", window->project_id);
    snprintf(body_lines[1], sizeof(body_lines[1]), "Missing project body file: session/wraith_body.txt");
    lines[0] = body_lines[0];
    if (max_lines > 1) {
        lines[1] = body_lines[1];
        return 2;
    }
    return 1;
}

/* 2fix.txt, 2026-07-05: GL-mode gap. project_probe_body_lines() feeds
 * whole wraith_body.txt lines verbatim into an OBJECT's label= field.
 * Several projects (chtmgl-video-isolate's Play/Pause/Resume/Stop
 * buttons, the new settings project's menu) deliberately embed raw
 * <text>/<button> tags in a body line so the ASCII path
 * (append_project_probe_body() -> re-parsed by wraith_parser_alpha.c)
 * renders them as real elements. GL mode never re-parses that label=
 * text at all, so those lines showed the literal tag syntax instead of
 * a real, clickable button. extract_attr()/emit_embedded_line_objects()
 * below give GL-mode parity: any project_body_text line starting with
 * '<' is now parsed into one real OBJECT per tag/text-run, each with
 * its own x/y/w/label/action, instead of one OBJECT holding the whole
 * raw line as text.
 *
 * UPDATE (2026-07-05, same day, follow-up): actionable chunks (a real
 * onClick/href, i.e. real buttons -- not plain <text> border/padding
 * pieces) now DO get a real nav= value, not nav=0. See
 * count_scene_nav_controls()/count_embedded_body_nav_slots() and the
 * next_body_nav counter threaded in from write_semantic_projection_files()
 * below -- this turned out not to need matching wraith_parser_alpha.c's
 * own numbering at all (that fragility concern was real for the outer
 * chrome buttons, per window-geometry-render-plan-j5.md's Path B
 * write-up, but g_max_index/launcher_start are entirely
 * wraith-alpha_manager.c's own self-contained arithmetic, so this was
 * safely fixable without touching the parser side). */
static int extract_attr(const char *tag_start, const char *tag_end, const char *attr_name, char *out, size_t out_sz) {
    char pattern[64];
    void *found;
    const char *val_start;
    const char *val_end;
    size_t pattern_len;

    out[0] = '\0';
    /* Leading space anchors the match to an actual attribute boundary --
     * every real attribute in this codebase's generated tags is
     * space-preceded ("<button label=\"...\" onClick=\"...\" />"), so
     * without this a search for e.g. "label=\"" could false-positive
     * match inside a hypothetical differently-named attribute ending in
     * the same substring (not a live bug against current markup, but a
     * latent one caught during 2026-07-05 review, cheap to close). */
    snprintf(pattern, sizeof(pattern), " %s=\"", attr_name);
    pattern_len = strlen(pattern);
    if ((size_t)(tag_end - tag_start) < pattern_len) {
        return 0;
    }
    found = memmem(tag_start, (size_t)(tag_end - tag_start), pattern, pattern_len);
    if (!found) {
        return 0;
    }
    val_start = (const char *)found + pattern_len;
    val_end = strchr(val_start, '"');
    if (!val_end || val_end > tag_end) {
        return 0;
    }
    {
        size_t len = (size_t)(val_end - val_start);
        if (len >= out_sz) {
            len = out_sz - 1;
        }
        memcpy(out, val_start, len);
        out[len] = '\0';
    }
    return 1;
}

/* Reads a single key=value pair back out of <project_dir>/manager/gui_state.txt
 * -- the SAME file wraith_parser_alpha.c's save_to_gui_state()/get_var()
 * already read and write for cli_io state on the ASCII side (confirmed:
 * identical path shape -- project_root_path/projects/<project_id>/manager/gui_state.txt
 * there, project_dir/manager/gui_state.txt here, where project_dir already
 * IS project_root_path + "/projects/" + project_id -- and identical
 * "key=value\n" line format). Scoped correctly by construction: callers
 * pass the SPECIFIC embedded window's own project_dir (from
 * project_dir_for_window()), not this manager's own project_id, avoiding
 * the "5a. gui_state scoping" mistake documented in
 * 真.how-2-fix-clio-chtpm.txt (reading the desktop shell's own gui_state
 * instead of the embedded project's). Did not exist anywhere in this file
 * before -- see 2fix-july6.txt, bug 2, for why GL's embedded cli_io fields
 * could never show typed content without it. Keeps scanning to the last
 * match (gui_state.txt is fully rewritten on every save, so there's
 * normally exactly one, but this matches the same "last line wins"
 * convention already used elsewhere in this codebase, e.g. the input-ops'
 * own read_active_page()). */
static void read_gui_state_value(const char *project_dir, const char *key, char *out, size_t out_sz) {
    char path[MAX_PATH];
    FILE *f;
    char line[512];
    size_t key_len;

    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!project_dir || !key || !key[0]) return;
    key_len = strlen(key);

    snprintf(path, sizeof(path), "%s/manager/gui_state.txt", project_dir);
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *val = line + key_len + 1;
            size_t len;
            val[strcspn(val, "\r\n")] = '\0';
            len = strlen(val);
            if (len >= out_sz) len = out_sz - 1;
            memcpy(out, val, len);
            out[len] = '\0';
        }
    }
    fclose(f);
}

static void emit_embedded_line_objects(FILE *objects, int *object_id, const Window *window,
                                        const char *window_chain, const char *line,
                                        int base_x, int base_y, int line_index,
                                        int *next_nav) {
    const char *p = line;
    int cursor_x = base_x;
    int chunk_index = 0;

    while (*p) {
        if (strncmp(p, "<br", 3) == 0) {
            const char *tag_end = strchr(p, '>');
            if (!tag_end) break;
            p = tag_end + 1;
            continue;
        }
        if (*p == '<') {
            char label[256];
            char action[256];
            const char *tag_end = strchr(p, '>');
            int rendered_len;
            int nav = 0;

            if (!tag_end) break; /* malformed tag, stop rather than loop forever */

            extract_attr(p, tag_end, "label", label, sizeof(label));
            if (!extract_attr(p, tag_end, "onClick", action, sizeof(action))) {
                extract_attr(p, tag_end, "href", action, sizeof(action));
            }

            /* Mirrors wraith_parser_alpha.c's own cli_io rendering exactly:
               that file's display_val is "input_buffer if non-empty, else
               the placeholder label" -- ASCII's input_buffer is populated
               by reading this SAME target_id-keyed gui_state.txt value
               back on reparse. GL had no equivalent at all before this:
               emit_embedded_line_objects() only ever echoed the tag's
               static label attribute, so even with focus+typing fully
               working on the ASCII/backend side, GL's cli_io boxes could
               never show it (see 2fix-july6.txt, bug 2). Buttons are
               untouched -- this only fires for <cli_io> tags, and only
               when a live, non-empty value actually exists for its own
               target_id. */
            int is_cli_io = (strncmp(p, "<cli_io", 7) == 0);
            if (is_cli_io) {
                char target_id[128];
                if (extract_attr(p, tag_end, "target_id", target_id, sizeof(target_id)) && target_id[0]) {
                    char project_dir[MAX_PATH];
                    if (project_dir_for_window(window, project_dir, sizeof(project_dir))) {
                        char live_value[256];
                        read_gui_state_value(project_dir, target_id, live_value, sizeof(live_value));
                        if (live_value[0]) {
                            strncpy(label, live_value, sizeof(label) - 1);
                            label[sizeof(label) - 1] = '\0';
                        }
                    }
                }
            }

            if (label[0]) {
                /* Only real buttons (a non-empty action) OR a <cli_io>
                   field consume a nav slot -- plain <text> border/padding
                   chunks stay nav=0, visible but not Tab-focusable,
                   matching count_embedded_body_nav_slots()'s job of
                   reserving the right number of slots (that counter
                   counts ALL labeled chunks, cli_io included, so this
                   already fits inside its reserved upper bound -- no
                   renumbering needed elsewhere).
                   cli_io has no onClick/href (action[0] stays empty), so
                   without the explicit check here it would stay nav=0
                   forever and never become clickable in GL -- that was
                   Bug B (window-geom's cli_io fields showing as inert
                   text in GL, zest-09.00-handoff.md). Giving it a nav
                   slot with no explicit action is enough:
                   hit_test_semantic_action() in wraith_gl.c already
                   synthesizes SET_ACTIVE:<nav> for any nav>0 OBJECT with
                   no action of its own -- the same generic click-to-focus
                   path every other nav-only element already uses. */
                if ((action[0] || is_cli_io) && next_nav) {
                    nav = (*next_nav)++;
                }
                rendered_len = (int)strlen(label);
                {
                    /* wraith_rgb_daemon.c's build_display_label() wraps
                       ANY nav>0 object's text as "[%s] %d. [%s]" before
                       drawing it -- 7 literal characters ([, ], space,
                       ., space, [, ]) plus a 1-char focus glyph plus
                       nav's own digit count, all on top of the bare
                       label. w= (and cursor_x's advance below) MUST
                       reserve room for that wrapped length, not the bare
                       label -- this was the root cause of chrome icons'
                       glyphs, cli_io fields, and [-]/[+] buttons all
                       showing garbled/clipped/missing in GL: their w=
                       was sized to the bare label only, so
                       wraith_rgb_daemon.c's blit_text() clipped the
                       wrapped prefix/suffix (or the label itself) to fit
                       a box that was always too narrow once nav>0
                       triggered the wrap. See
                       zest-09.00-handoff.md/WRAITH_RGB_ARCHITECTURE.md
                       for the fuller trace. */
                    int display_len = rendered_len;
                    int is_selected = (nav > 0 && g_active_gui_index == nav);
                    /* "^" only for a cli_io field that's genuinely ACTIVE
                       (accepting keystrokes), not just focused -- mirrors
                       ASCII's own is_active-vs-is_focused distinction
                       exactly (wraith_parser_alpha.c's render_element():
                       is_active uses "[^]", is_focused-only uses "[>]").
                       g_active_gui_is_typing is the ONLY signal that
                       distinguishes them; without it (before this fix)
                       this could only ever say ">" (see 2fix-july6.txt,
                       bug 3). Deliberately scoped to is_cli_io -- this
                       must not affect the UNRELATED existing "^" used for
                       map-control/INTERACT elements elsewhere in this
                       file. */
                    const char *glyph = " ";
                    if (is_selected) {
                        glyph = (is_cli_io && g_active_gui_is_typing) ? "^" : ">";
                    }
                    if (nav > 0) {
                        int nav_digits = 1;
                        int tmp = nav;
                        while (tmp >= 10) { nav_digits++; tmp /= 10; }
                        display_len = rendered_len + 8 + nav_digits;
                    }
                    fprintf(objects, "OBJECT | %04d | tag=text id=%s_body_%02d_%02d role=project_body_embedded x=%d y=%d w=%d h=1 z=14 focused=%s parent_id=%s container_id=%s source_ref=semantic:project_body_embedded ancestor_chain=%s>%s_body_%02d_%02d clip_chain=%s nav=%d nav_selected=%s nav_selector_glyph=%s label_core= fg=#E8F1F2 bg=#162534 border=#E8F1F2 label=%s action=%s src=\n",
                        (*object_id)++,
                        window->id, line_index, chunk_index,
                        cursor_x, base_y, display_len > 0 ? display_len : 1,
                        is_selected ? "true" : "false",
                        window->id, window->id,
                        window_chain, window->id, line_index, chunk_index,
                        window_chain,
                        nav,
                        is_selected ? "true" : "false",
                        glyph,
                        label,
                        action[0] ? action : "");
                    cursor_x += display_len;
                }
                chunk_index++;
            }

            p = tag_end + 1;
        } else {
            const char *run_start = p;
            while (*p && *p != '<') {
                p++;
            }
            {
                int run_len = (int)(p - run_start);
                if (run_len > 0) {
                    char buf[256];
                    int copy_len = run_len < (int)sizeof(buf) - 1 ? run_len : (int)sizeof(buf) - 1;
                    memcpy(buf, run_start, copy_len);
                    buf[copy_len] = '\0';
                    fprintf(objects, "OBJECT | %04d | tag=text id=%s_body_%02d_%02d role=project_body_embedded x=%d y=%d w=%d h=1 z=14 focused=false parent_id=%s container_id=%s source_ref=semantic:project_body_embedded ancestor_chain=%s>%s_body_%02d_%02d clip_chain=%s nav=0 nav_selected=false nav_selector_glyph=  label_core= fg=#E8F1F2 bg=#162534 border=#E8F1F2 label=%s action= src=\n",
                        (*object_id)++,
                        window->id, line_index, chunk_index,
                        cursor_x, base_y, run_len,
                        window->id, window->id,
                        window_chain, window->id, line_index, chunk_index,
                        window_chain,
                        buf);
                    cursor_x += run_len;
                    chunk_index++;
                }
            }
        }
    }
}

static void append_project_scene_objects(FILE *objects, int *object_id, const Window *window, const char *window_chain, int win_row_offset, int win_col_offset) {
    char project_dir[MAX_PATH];
    char scene_path[MAX_PATH];
    char line[1024];
    FILE *scene;
    int is_map_control = 0;

    if (!objects || !object_id || !window || !window_chain) {
        return;
    }
    if (!project_dir_for_window(window, project_dir, sizeof(project_dir))) {
        return;
    }
    {
        char state_path[MAX_PATH];
        char state_line[256];
        FILE *state;
        snprintf(state_path, sizeof(state_path), "%s/session/state.txt", project_dir);
        state = fopen(state_path, "r");
        if (state) {
            while (fgets(state_line, sizeof(state_line), state)) {
                if (strncmp(state_line, "is_map_control=", 15) == 0) {
                    is_map_control = atoi(state_line + 15);
                    break;
                }
            }
            fclose(state);
        }
    }
    snprintf(scene_path, sizeof(scene_path), "%s/session/scene.objects.pdl", project_dir);
    scene = fopen(scene_path, "r");
    if (!scene) {
        return;
    }
    while (fgets(line, sizeof(line), scene)) {
        char tag[32] = "model";
        char id[64] = "project_object";
        char role[64] = "project_object";
        char source_ref[256] = "project_scene";
        char target_surface[80] = "";
        char label[256] = "";
        char action[128] = "";
        char src[256] = "";
        char fg[16] = "#E8F1F2";
        char bg[16] = "#0B1118";
        char border[16] = "#7EDFF2";
        int x = 3;
        int y = 4;
        int w = 20;
        int h = 1;
        int z = 22;
        int nav = 0;
        int nav_selected = 0;
        char nav_selector_glyph[8] = " ";
        char value[256];

        if (line[0] == '#' || strncmp(line, "OBJECT", 6) != 0) {
            continue;
        }
        if (line_kvp_value(line, "tag", value, sizeof(value))) snprintf(tag, sizeof(tag), "%s", value);
        if (line_kvp_value(line, "id", value, sizeof(value))) snprintf(id, sizeof(id), "%s", value);
        if (line_kvp_value(line, "role", value, sizeof(value))) snprintf(role, sizeof(role), "%s", value);
        if (line_kvp_value(line, "x", value, sizeof(value))) x = atoi(value);
        if (line_kvp_value(line, "y", value, sizeof(value))) y = atoi(value);
        if (line_kvp_value(line, "w", value, sizeof(value))) w = atoi(value);
        if (line_kvp_value(line, "h", value, sizeof(value))) h = atoi(value);
        if (line_kvp_value(line, "z", value, sizeof(value))) z = atoi(value);
        if (line_kvp_value(line, "source_ref", value, sizeof(value))) {
            snprintf(source_ref, sizeof(source_ref), "%s", value);
        } else if (line_kvp_value(line, "source", value, sizeof(value))) {
            snprintf(source_ref, sizeof(source_ref), "%s", value);
        }
        if (line_kvp_value(line, "target_surface", value, sizeof(value))) snprintf(target_surface, sizeof(target_surface), "%s", value);
        if (line_kvp_value(line, "fg", value, sizeof(value))) snprintf(fg, sizeof(fg), "%s", value);
        if (line_kvp_value(line, "bg", value, sizeof(value))) snprintf(bg, sizeof(bg), "%s", value);
        if (line_kvp_value(line, "border", value, sizeof(value))) snprintf(border, sizeof(border), "%s", value);
        if (line_kvp_value(line, "action", value, sizeof(value))) snprintf(action, sizeof(action), "%s", value);
        if (line_kvp_value(line, "label", value, sizeof(value))) snprintf(label, sizeof(label), "%s", value);
        if (line_kvp_value(line, "nav", value, sizeof(value))) {
            nav = atoi(value);
        }
        if (nav > 0) {
            nav_selected = (g_active_gui_index == nav);
            if (is_map_control && strcmp(action, "INTERACT") == 0) {
                if (g_map_control_nav_index <= 0 || g_map_control_nav_index == nav) {
                    nav_selected = 1;
                }
            }
            if (nav_selected) {
                snprintf(nav_selector_glyph, sizeof(nav_selector_glyph), "%s", (is_map_control && strcmp(action, "INTERACT") == 0) ? "^" : ">");
            }
        }
        snprintf(src, sizeof(src), "%.255s", source_ref);
        fprintf(objects, "OBJECT | %04d | tag=%s id=%s_%s role=%s x=%d y=%d w=%d h=%d z=%d focused=%s parent_id=%s container_id=%s source_ref=%s target_surface=%s ancestor_chain=%s>%s clip_chain=%s nav=%d nav_selected=%s nav_selector_glyph=%s label_core= fg=%s bg=%s border=%s label=%s action=%s src=%s\n",
            (*object_id)++,
            tag,
            window->id,
            id,
            role,
            x + win_col_offset,
            y + win_row_offset,
            w,
            h,
            z,
            nav_selected ? "true" : "false",
            window->id,
            window->id,
            source_ref,
            target_surface[0] ? target_surface : "-",
            window_chain,
            id,
            window_chain,
            nav,
            nav_selected ? "true" : "false",
            nav_selector_glyph,
            fg,
            bg,
            border,
            label,
            strcmp(action, "-") == 0 ? "" : action,
            src);
    }
    fclose(scene);
}

static void load_mouse_offset(int *offset_x, int *offset_y) {
    FILE *f = fopen("#.mouse-offset.txt", "r");
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

/* Path B (window-geometry-render-plan-j5.md): resolves a window's
   content origin as row/col offsets. Called from BOTH the visual-
   emission path (append_with_origin_offset(), below -- leading-space /
   blank-line padding, confirmed by a full trace of
   wraith_parser_alpha.c's render_element() to be the only positioning
   mechanism available, since it has no row/column cursor at all) and
   the semantic/click-emission path (write_semantic_projection_files()'s
   OBJECT x=/y= values), so the two can't drift out of sync by
   construction -- one function, two call sites, not two independently
   hand-written copies of the same arithmetic. Default (window->x == 0
   && window->y == 0, every project today, since no project.pdl has a
   WINDOW section yet) resolves to {0, 0}, preserving today's exact
   behavior. */
static void resolve_window_content_origin(const Window *window, int *out_row_offset, int *out_col_offset) {
    *out_row_offset = 0;
    *out_col_offset = 0;
    if (!window) {
        return;
    }
    if (window->y > 0) {
        *out_row_offset = window->y;
    }
    if (window->x > 0) {
        *out_col_offset = window->x;
    }
}

/* Applies a resolved row/col offset to an already-composed markup
   block: emits `row_offset` blank lines first, then walks `raw`
   appending `col_offset` spaces immediately after every "<br...>" tag
   (and once before the first line) -- deliberately keyed on any tag
   starting with "<br" (not just the literal "<br/>"), since this
   codebase also uses "<br visibility=\"...\" />" as a line break, and a
   naive exact-string split would silently fail to offset those lines. */
static void append_with_origin_offset(char *out, size_t size, const char *raw, int row_offset, int col_offset) {
    const char *p = raw;
    int i;

    for (i = 0; i < row_offset; i++) {
        appendf(out, size, "<br/>");
    }
    for (i = 0; i < col_offset; i++) {
        appendf(out, size, " ");
    }

    while (*p) {
        if (strncmp(p, "<br", 3) == 0) {
            const char *tag_end = strchr(p, '>');
            size_t tag_len = tag_end ? (size_t)(tag_end - p + 1) : strlen(p);
            appendf(out, size, "%.*s", (int)tag_len, p);
            p += tag_len;
            for (i = 0; i < col_offset; i++) {
                appendf(out, size, " ");
            }
        } else {
            const char *run_start = p;
            while (*p && strncmp(p, "<br", 3) != 0) {
                p++;
            }
            appendf(out, size, "%.*s", (int)(p - run_start), run_start);
        }
    }
}

static void build_desktop_shell_markup(char *out, size_t size, Window *window) {
    int launcher_index = 0;
    int taskbar_index = 0;
    int launcher_count = 0;
    int launcher_start = 0;
    int taskbar_start = 0;
    int i;
    /* Path B: the window's own title bar + content (NOT the outer frame
       border and NOT the taskbar, which stay fixed -- matches the same
       split already used in write_semantic_projection_files()) is
       composed here first, then injected into `out` with its resolved
       row/col offset applied in one place. */
    char raw[4096];
    int row_offset = 0;
    int col_offset = 0;

    out[0] = '\0';

    if (!window) {
        const WraithLauncher *terminal_launcher = find_terminal_launcher();
        appendf(out, size, "+-WRAITH DESKTOP GUI---------------------------------------------------------------------------+<br/>");
        appendf(out, size, "<br/><br/><br/>");
        appendf(out, size, "| [ TASKBAR ] <button compact=\"true\" label=\"Terminal\" onClick=\"%s\" />",
            terminal_launcher ? terminal_launcher->command : "DESKTOP_ACTION:launch_terminal");
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
    /* CHROME_CONTENT_START, not a literal -- derived from g_chrome_icons[]'s
       own length (see that array's comment for why this must never again
       be a separately-maintained number). */
    launcher_start = CHROME_CONTENT_START;
    taskbar_start = window ? (launcher_start + launcher_count) : 1;

    append_frame_border(out, size, "WRAITH DESKTOP GUI", resolve_frame_width(window, -1));

    raw[0] = '\0';
    appendf(raw, sizeof(raw), "| +-");
    appendf(raw, sizeof(raw), "<button label=\" %s \" onClick=\"SET_ACTIVE:1\" />", window->title);
    appendf(raw, sizeof(raw), "------------------------------------------");
    /* Chrome icon row: iterates g_chrome_icons[] -- the single source of
       truth -- instead of one hardcoded <button> line per icon. nav 2 is
       the first icon, matching CHROME_CONTENT_START's own "title is 1,
       icons follow" assumption. This loop and the matching GL OBJECT loop
       in write_semantic_projection_files() are the ONLY two places that
       read this array; keep them structurally identical (same icon ->
       same nav number in both) or the ASCII/GL divergence this was built
       to prevent comes back. */
    {
        int ci;
        for (ci = 0; ci < CHROME_ICON_COUNT; ci++) {
            appendf(raw, sizeof(raw), "<button compact=\"true\" label=\"%c\" onClick=\"SET_ACTIVE:%d\" />",
                    g_chrome_icons[ci].glyph, 2 + ci);
        }
    }
    appendf(raw, sizeof(raw), "-+               |<br/>");

    if (window && !active_window_is_terminal(window)) {
        int project_line_count = append_project_probe_body(raw, sizeof(raw), window);
        if (project_line_count == 0) {
            project_line_count = append_project_probe_scene_markup(raw, sizeof(raw), window, 14);
        }
        if (project_line_count == 0) {
            appendf(raw, sizeof(raw), "| |  Project: %-70.70s |<br visibility=\"${desktop_active_window_body_visible}\" />", window->project_id);
            appendf(raw, sizeof(raw), "| |  Missing project body file: session/wraith_body.txt                               |<br visibility=\"${desktop_active_window_body_visible}\" />");
            appendf(raw, sizeof(raw), "| |                                                                                   |<br visibility=\"${desktop_active_window_body_visible}\" />");
            appendf(raw, sizeof(raw), "| |                                                                                   |<br visibility=\"${desktop_active_window_body_visible}\" />");
            project_line_count = 4;
        }
        launcher_index = project_line_count;
    } else if (launcher_count > 0) {
        int li;
        discover_launcher_projects();
        for (li = 0; li < g_launcher_count; li++) {
            launcher_index++;
            appendf(raw, sizeof(raw), "| |  ");
            appendf(raw, sizeof(raw),
                "<button compact=\"true\" label=\"%-12.12s\" onClick=\"%s\" visibility=\"${desktop_active_window_body_visible}\" />",
                g_launchers[li].display_label,
                g_launchers[li].command);
            appendf(raw, sizeof(raw), "                                                                          |<br visibility=\"${desktop_active_window_body_visible}\" />");
        }
    }

    if (launcher_index == 0) {
        appendf(raw, sizeof(raw), "| |  %-85s |<br visibility=\"${desktop_active_window_body_visible}\" />", "[ No Projects Found ]");
    }

    while (launcher_index < 4) {
        appendf(raw, sizeof(raw), "| |                                                                                              |<br visibility=\"${desktop_active_window_body_visible}\" />");
        launcher_index++;
    }

    appendf(raw, sizeof(raw), "| +----------------------------------------------------------------------------------------------+               |<br visibility=\"${desktop_active_window_body_visible}\" />");

    resolve_window_content_origin(window, &row_offset, &col_offset);
    append_with_origin_offset(out, size, raw, row_offset, col_offset);

    {
        const WraithLauncher *terminal_launcher = find_terminal_launcher();
        appendf(out, size, "| [ TASKBAR ] <button compact=\"true\" label=\"Terminal\" onClick=\"%s\" />",
            terminal_launcher ? terminal_launcher->command : "DESKTOP_ACTION:launch_terminal");
    }

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
    append_frame_border(out, size, NULL, resolve_frame_width(window, -1));
}

static void write_semantic_projection_files(void) {
    char meta_path[MAX_PATH];
    char meta_tmp[MAX_PATH];
    char objects_path[MAX_PATH];
    char objects_tmp[MAX_PATH];
    char audit_path[MAX_PATH];
    char audit_tmp[MAX_PATH];
    char desktop_path[MAX_PATH];
    char desktop_tmp[MAX_PATH];
    char focus_path[MAX_PATH];
    char focus_tmp[MAX_PATH];
    FILE *meta;
    FILE *objects;
    FILE *audit;
    FILE *desktop;
    FILE *focus;
    Window *window = active_window();
    const char *focused_object_dom_id = window ? window->id : "wraith_root";
    const char *focused_object_label = window ? window->title : "WRAITH DESKTOP GUI";
    const char *focused_object_role = window ? "panel" : "window";
    int focused_object_id = window ? 3 : 1;
    int mouse_offset_x = 0;
    int mouse_offset_y = 0;
    int object_id = 1;
    int taskbar_index = 0;
    int launcher_count = count_launcher_methods();
    /* CHROME_CONTENT_START, not a literal -- see g_chrome_icons[]'s comment. */
    int taskbar_start = window ? (CHROME_CONTENT_START + launcher_count) : 1;
    int li;
    int i;
    /* Path B: same resolve_window_content_origin() call as
       build_desktop_shell_markup() -- these OBJECT x=/y= values must
       agree with that function's visual padding, not be computed
       independently, or clicks would land where the window used to be. */
    int win_row_offset = 0;
    int win_col_offset = 0;

    resolve_window_content_origin(window, &win_row_offset, &win_col_offset);

    snprintf(meta_path, sizeof(meta_path), "%s/pieces/display/current_frame.meta.pdl", g_project_root);
    snprintf(meta_tmp, sizeof(meta_tmp), "%s.tmp", meta_path);
    snprintf(objects_path, sizeof(objects_path), "%s/pieces/display/current_frame.objects.pdl", g_project_root);
    snprintf(objects_tmp, sizeof(objects_tmp), "%s.tmp", objects_path);
    snprintf(audit_path, sizeof(audit_path), "%s/pieces/display/current_frame.audit.txt", g_project_root);
    snprintf(audit_tmp, sizeof(audit_tmp), "%s.tmp", audit_path);
    snprintf(desktop_path, sizeof(desktop_path), "%s/pieces/display/current_frame.desktop_state.pdl", g_project_root);
    snprintf(desktop_tmp, sizeof(desktop_tmp), "%s.tmp", desktop_path);
    snprintf(focus_path, sizeof(focus_path), "%s/pieces/display/current_frame.focus_state.pdl", g_project_root);
    snprintf(focus_tmp, sizeof(focus_tmp), "%s.tmp", focus_path);

    meta = fopen(meta_tmp, "w");
    objects = fopen(objects_tmp, "w");
    audit = fopen(audit_tmp, "w");
    desktop = fopen(desktop_tmp, "w");
    focus = fopen(focus_tmp, "w");
    if (!meta || !objects || !audit || !desktop || !focus) {
        if (meta) fclose(meta);
        if (objects) fclose(objects);
        if (audit) fclose(audit);
        if (desktop) fclose(desktop);
        if (focus) fclose(focus);
        return;
    }

    load_mouse_offset(&mouse_offset_x, &mouse_offset_y);
    {
        char root_chain[64];
        char window_chain[128];
        char taskbar_shell_chain[128];
        char taskbar_row_chain[128];
        char debug_chain[64];
        char summary_chain[64];
        const int desktop_body_top = 2;
        const int desktop_body_height = 23;
        const int desktop_body_bottom = desktop_body_top + desktop_body_height;
        const int footer_top = desktop_body_bottom + 1;
        const int footer_band_height = 4;
        const int taskbar_y = footer_top;
        const int summary_y = footer_top + 1;
        const int debug_y = footer_top + 2;

        snprintf(root_chain, sizeof(root_chain), "wraith_root");
        snprintf(window_chain, sizeof(window_chain), "wraith_root>%s", window ? window->id : "wraith_root");
        snprintf(taskbar_shell_chain, sizeof(taskbar_shell_chain), "wraith_root>taskbar");
        snprintf(taskbar_row_chain, sizeof(taskbar_row_chain), "wraith_root>taskbar>taskbar_row");
        snprintf(debug_chain, sizeof(debug_chain), "wraith_root>debug_row");
        snprintf(summary_chain, sizeof(summary_chain), "wraith_root>summary_row");

    if (g_active_gui_index == debug_selector_ascii_index()) {
        focused_object_id = debug_selector_ascii_index();
        focused_object_dom_id = "debug_ascii";
        focused_object_label = "ASCII*";
        focused_object_role = "debug_selector";
    } else if (g_active_gui_index == debug_selector_gl_index()) {
        focused_object_id = debug_selector_gl_index();
        focused_object_dom_id = "debug_gl";
        focused_object_label = "GL";
        focused_object_role = "debug_selector";
    }

    fprintf(meta, "SECTION | KEY | VALUE\n");
    fprintf(meta, "FRAME | frame_id | %06d\n", g_next_instance_no);
    fprintf(meta, "FRAME | project_id | wraith-alpha\n");
    fprintf(meta, "FRAME | source_layout | projects/wraith-alpha/layouts/alpha-shell.chtpm\n");
    fprintf(meta, "FRAME | cols | 96\n");
    fprintf(meta, "FRAME | rows | 30\n");
    fprintf(meta, "FRAME | viewport_width_px | 960\n");
    fprintf(meta, "FRAME | viewport_height_px | 540\n");
    fprintf(meta, "FRAME | cell_width_px | 10\n");
    fprintf(meta, "FRAME | cell_height_px | 18\n");
    fprintf(meta, "FRAME | rgb_width_px | 960\n");
    fprintf(meta, "FRAME | rgb_height_px | 540\n");
    fprintf(meta, "FRAME | focused_object_id | %d\n", focused_object_id);
    fprintf(meta, "FRAME | focused_object_dom_id | %s\n", focused_object_dom_id);
    fprintf(meta, "FRAME | focused_object_label | %s\n", focused_object_label);
    fprintf(meta, "FRAME | focused_object_role | %s\n", focused_object_role);
    fprintf(meta, "FRAME | mouse_x | %d\n", g_mouse_x);
    fprintf(meta, "FRAME | mouse_y | %d\n", g_mouse_y);
    fprintf(meta, "FRAME | mouse_cursor_visual_uses_offset | true\n");
    fprintf(meta, "FRAME | mouse_hit_offset_x | %d\n", mouse_offset_x);
    fprintf(meta, "FRAME | mouse_hit_offset_y | %d\n", mouse_offset_y);
    fprintf(meta, "FRAME | drag_active | false\n");
    fprintf(meta, "RASTER | font_policy | converter_owns_glyphs\n");
    fprintf(meta, "RASTER | gl_policy | gl_presents_rgb_only\n");
    fprintf(meta, "WARNINGS | count | none\n");

    fprintf(objects, "SECTION | KEY | VALUE\n");
    fprintf(objects, "FRAME | frame_id | %06d\n", g_next_instance_no);
    fprintf(objects, "OBJECT | %04d | tag=window id=wraith_root role=window x=0 y=0 w=96 h=30 z=1 focused=%s parent_id=none container_id=none source_ref=semantic:desktop_shell ancestor_chain=%s clip_chain=%s fg=#E8F1F2 bg=#0B1118 border=#7EDFF2 label=WRAITH DESKTOP GUI action= src=\n",
        object_id++, window ? "false" : "true", root_chain, root_chain);
    fprintf(objects, "OBJECT | %04d | tag=header id=desktop_banner role=banner x=0 y=0 w=96 h=2 z=2 focused=false parent_id=wraith_root container_id=wraith_root source_ref=semantic:desktop_banner ancestor_chain=%s>desktop_banner clip_chain=%s fg=#DDE7F0 bg=#0B1118 border=#0B1118 label=WRAITH DESKTOP GUI action= src=\n",
        object_id++, root_chain, root_chain);

    if (window) {
        fprintf(objects, "OBJECT | %04d | tag=panel id=window_chrome_row role=window_chrome_row x=%d y=%d w=94 h=1 z=9 focused=false parent_id=%s container_id=%s source_ref=semantic:window_chrome_row ancestor_chain=%s>window_chrome_row clip_chain=%s fg=#E8F1F2 bg=#162534 border=#162534 label= action= src=\n",
            object_id++,
            1 + win_col_offset,
            2 + win_row_offset,
            window->id,
            window->id,
            window_chain,
            window_chain);
        fprintf(objects, "OBJECT | %04d | tag=text id=%s_title role=window_title x=%d y=%d w=58 h=1 z=20 focused=%s parent_id=window_chrome_row container_id=window_chrome_row source_ref=semantic:window_title ancestor_chain=%s>window_chrome_row>%s_title clip_chain=%s nav=%d nav_selected=%s nav_selector_glyph=%s label_core= fg=#E8F1F2 bg=#162534 border=#162534 label=%s action=SET_ACTIVE:1 src=\n",
            object_id++,
            window->id,
            3 + win_col_offset,
            2 + win_row_offset,
            g_active_gui_index == 1 ? "true" : "false",
            window_chain,
            window->id,
            window_chain,
            1,
            g_active_gui_index == 1 ? "true" : "false",
            g_active_gui_index == 1 ? ">" : " ",
            window->title);
        fprintf(objects, "OBJECT | %04d | tag=panel id=%s role=panel x=%d y=%d w=94 h=22 z=10 focused=true parent_id=wraith_root container_id=wraith_root source_ref=%s ancestor_chain=%s clip_chain=%s nav=0 nav_selected=false nav_selector_glyph=  label_core= fg=#E8F1F2 bg=#162534 border=#FFD166 label= action=SET_ACTIVE:1 src=%s\n",
            object_id++,
            window->id,
            1 + win_col_offset,
            3 + win_row_offset,
            strcmp(window->project_id, "wraith-alpha/wraith-projects/terminal") == 0 ? "semantic:terminal_panel" : "semantic:project_panel",
            window_chain,
            root_chain,
            strcmp(window->project_id, "wraith-alpha/wraith-projects/terminal") == 0 ? "terminal_window.view.txt" : "wraith_window.view.txt");
        /* Chrome icon row: iterates g_chrome_icons[] -- the single source
           of truth (see its own comment) -- instead of one hardcoded
           OBJECT per icon. nav = 2+ci matches the ASCII loop in
           build_desktop_shell_markup() exactly, term for term, so the two
           renderers cannot independently drift on numbering again.
           Width/spacing MUST fit wraith_rgb_daemon.c's build_display_label()
           wrap ("[%s] %d. [%s]" -- 7 literal chars + 1 glyph char +
           nav's digit count, on top of the 1-char icon glyph itself):
           for nav 2-5 that's 1+9=10 characters. A prior version of this
           loop used w=7/delta=8 (sized to just the bare 1-char glyph,
           forgetting the wrap entirely) -- confirmed live this is what
           made every chrome icon's glyph render clipped/missing in GL,
           not just the newly-added one. See
           WRAITH_RGB_ARCHITECTURE.md's nav-numbering-invariant section
           and zest-09.00-handoff.md for the fuller trace. */
        {
            int ci;
            for (ci = 0; ci < CHROME_ICON_COUNT; ci++) {
                int nav = 2 + ci;
                int nav_digits = 1;
                int tmp = nav;
                int display_len;
                int x;
                while (tmp >= 10) { nav_digits++; tmp /= 10; }
                display_len = 1 + 8 + nav_digits; /* 1 = the bare glyph */
                x = 63 + ci * display_len;
                fprintf(objects, "OBJECT | %04d | tag=text id=%s_%s role=chrome_button x=%d y=%d w=%d h=1 z=21 focused=%s parent_id=%s container_id=%s source_ref=semantic:%s ancestor_chain=%s>%s>%s clip_chain=%s nav=%d nav_selected=%s nav_selector_glyph=%s label_core=%c fg=#E8F1F2 bg=#162534 border=#E8F1F2 label=%c action=SET_ACTIVE:%d src=\n",
                    object_id++, window->id, g_chrome_icons[ci].semantic,
                    x + win_col_offset, 2 + win_row_offset, display_len,
                    g_active_gui_index == nav ? "true" : "false",
                    window->id, window->id,
                    g_chrome_icons[ci].semantic,
                    window_chain, window->id, g_chrome_icons[ci].semantic,
                    window_chain,
                    nav,
                    g_active_gui_index == nav ? "true" : "false",
                    g_active_gui_index == nav ? ">" : " ",
                    g_chrome_icons[ci].glyph, g_chrome_icons[ci].glyph, nav);
            }
        }
        if (!active_window_is_terminal(window)) {
            /* 12 -> 40: was silently truncating any embedded body longer
               than 12 lines in GL only (see project_probe_body_lines()'s
               own comment for the confirmed live case -- window-geom's
               editor body's [-]/[+] buttons never got read at all). */
            const char *project_lines[40];
            int project_line_count = project_probe_body_lines(window, project_lines, 40);
            int pl;
            /* CHROME_CONTENT_START, not a literal 5 or 6 -- embedded
               body-tag objects get real nav= values continuing right
               after whatever this project's OWN scene.objects.pdl already
               declared (author-chosen, contiguous from CHROME_CONTENT_START
               by convention -- every project with its own
               session/scene.objects.pdl generator was updated this
               session to start there instead of a hardcoded 5, see
               zest-09.00-handoff.md) so the two ranges never collide. If
               this project has no scene.objects.pdl at all,
               scene_nav_count is 0 and body objects correctly start at
               CHROME_CONTENT_START. (2fix.txt, 2026-07-05; base updated
               2026-07-06 when chrome grew from 4 fixed slots to
               CHROME_ICON_COUNT+1.) */
            int next_body_nav = CHROME_CONTENT_START;
            {
                char nav_project_dir[MAX_PATH];
                if (project_dir_for_window(window, nav_project_dir, sizeof(nav_project_dir))) {
                    next_body_nav = CHROME_CONTENT_START + count_scene_nav_controls(nav_project_dir);
                }
            }
            for (pl = 0; pl < project_line_count; pl++) {
                if (project_lines[pl][0] == '<') {
                    /* Embedded markup line (see extract_attr()/
                       emit_embedded_line_objects() above, 2fix.txt
                       2026-07-05) -- parse it into real per-tag OBJECTs
                       instead of dumping the raw line as one text label. */
                    emit_embedded_line_objects(objects, &object_id, window, window_chain,
                                                project_lines[pl],
                                                3 + win_col_offset, 4 + pl + win_row_offset, pl,
                                                &next_body_nav);
                } else {
                    fprintf(objects, "OBJECT | %04d | tag=text id=%s_body_%02d role=project_body_text x=%d y=%d w=86 h=1 z=14 focused=false parent_id=%s container_id=%s source_ref=semantic:project_body ancestor_chain=%s>%s_body_%02d clip_chain=%s label_core= fg=#E8F1F2 bg=#162534 border=#E8F1F2 label=%s action= src=\n",
                        object_id++,
                        window->id,
                        pl + 1,
                        3 + win_col_offset,
                        4 + pl + win_row_offset,
                        window->id,
                        window->id,
                        window_chain,
                        window->id,
                        pl + 1,
                        window_chain,
                        project_lines[pl]);
                }
            }
            append_project_scene_objects(objects, &object_id, window, window_chain, win_row_offset, win_col_offset);
        } else {
            discover_launcher_projects();
            for (li = 0; li < g_launcher_count; li++) {
                int nav_idx = 5 + li;
                fprintf(objects, "OBJECT | %04d | tag=panel id=launcher_row_%s role=launcher_row x=%d y=%d w=30 h=1 z=18 focused=%s parent_id=%s container_id=%s source_ref=semantic:launcher_row ancestor_chain=%s>launcher_row_%s clip_chain=%s fg=#E8F1F2 bg=#162534 border=#162534 label= action= src=\n",
                    object_id++,
                    g_launchers[li].id_prefix,
                    2 + win_col_offset,
                    4 + li + win_row_offset,
                    g_active_gui_index == nav_idx ? "true" : "false",
                    window->id,
                    window->id,
                    window_chain,
                    g_launchers[li].id_prefix,
                    window->id);
                fprintf(objects, "OBJECT | %04d | tag=text id=launcher_%s role=launcher_item x=%d y=%d w=24 h=1 z=20 focused=%s parent_id=launcher_row_%s container_id=%s source_ref=semantic:launcher_item ancestor_chain=%s>launcher_row_%s>launcher_%s clip_chain=%s nav=%d nav_selected=%s nav_selector_glyph=%s label_core=%s fg=#E8F1F2 bg=#162534 border=#E8F1F2 label=%s action=%s src=\n",
                    object_id++,
                    g_launchers[li].id_prefix,
                    3 + win_col_offset,
                    4 + li + win_row_offset,
                    g_active_gui_index == nav_idx ? "true" : "false",
                    g_launchers[li].id_prefix,
                    window->id,
                    window_chain,
                    g_launchers[li].id_prefix,
                    g_launchers[li].id_prefix,
                    window_chain,
                    nav_idx,
                    g_active_gui_index == nav_idx ? "true" : "false",
                    g_active_gui_index == nav_idx ? ">" : " ",
                    g_launchers[li].display_label,
                    g_launchers[li].display_label,
                    g_launchers[li].command);
            }
        }
    }

    fprintf(objects, "OBJECT | %04d | tag=panel id=footer_band role=footer_band x=0 y=%d w=96 h=%d z=39 focused=false parent_id=wraith_root container_id=wraith_root source_ref=semantic:footer_band ancestor_chain=%s>footer_band clip_chain=wraith_root fg=#F7FAFC bg=#7A633A border=#D2B16E label= action= src=\n",
        object_id++, footer_top, footer_band_height, root_chain);
    fprintf(objects, "OBJECT | %04d | tag=header id=taskbar role=banner x=0 y=%d w=96 h=2 z=40 focused=false parent_id=wraith_root container_id=wraith_root source_ref=semantic:taskbar_banner ancestor_chain=%s>taskbar clip_chain=%s fg=#F7FAFC bg=#5B4728 border=#D2B16E label= action= src=\n",
        object_id++, taskbar_y, root_chain, taskbar_shell_chain);
    fprintf(objects, "OBJECT | %04d | tag=panel id=taskbar_row role=taskbar_row x=0 y=%d w=96 h=1 z=40 focused=false parent_id=taskbar container_id=taskbar source_ref=semantic:taskbar_row ancestor_chain=%s clip_chain=%s fg=#F7FAFC bg=#5B4728 border=#5B4728 label= action= src=\n",
        object_id++, taskbar_y, taskbar_row_chain, taskbar_shell_chain);
    fprintf(objects, "OBJECT | %04d | tag=text id=taskbar_prefix role=frame_text x=0 y=%d w=14 h=1 z=41 focused=false parent_id=taskbar_row container_id=taskbar_row source_ref=semantic:taskbar_prefix ancestor_chain=%s>taskbar_prefix clip_chain=%s label_core=| [ TASKBAR ] fg=#F7FAFC bg=#5B4728 border=#F7FAFC label=| [ TASKBAR ] action= src=\n",
        object_id++, taskbar_y, taskbar_row_chain, taskbar_row_chain);
    fprintf(objects, "OBJECT | %04d | tag=text id=taskbar_start role=taskbar_item x=14 y=%d w=18 h=1 z=41 focused=%s parent_id=taskbar_row container_id=taskbar_row source_ref=semantic:taskbar_item ancestor_chain=%s>taskbar_start clip_chain=%s nav=%d nav_selected=%s nav_selector_glyph=%s label_core=Terminal fg=#F7FAFC bg=#5B4728 border=#F7FAFC label=Terminal action=DESKTOP_ACTION:launch_terminal src=\n",
        object_id++,
        taskbar_y,
        g_active_gui_index == taskbar_start ? "true" : "false",
        taskbar_row_chain,
        taskbar_row_chain,
        taskbar_start,
        g_active_gui_index == taskbar_start ? "true" : "false",
        g_active_gui_index == taskbar_start ? ">" : " ");
    for (i = 0; i < g_window_count; i++) {
        Window *taskbar_window = &g_windows[i];
        int nav_idx;
        if (taskbar_window->state == WSTATE_CLOSED) {
            continue;
        }
        nav_idx = taskbar_start + taskbar_index + 1;
        fprintf(objects, "OBJECT | %04d | tag=text id=taskbar_window_%d role=taskbar_item x=%d y=%d w=24 h=1 z=42 focused=%s parent_id=taskbar_row container_id=taskbar_row source_ref=semantic:taskbar_item ancestor_chain=%s>%s clip_chain=%s nav=%d nav_selected=%s nav_selector_glyph=%s label_core= fg=#F7FAFC bg=#5B4728 border=#F7FAFC label=%s action=SET_ACTIVE:%d src=\n",
            object_id++,
            i + 1,
            34 + (taskbar_index * 24),
            taskbar_y,
            g_active_gui_index == nav_idx ? "true" : "false",
            taskbar_row_chain,
            taskbar_window->id,
            taskbar_row_chain,
            nav_idx,
            g_active_gui_index == nav_idx ? "true" : "false",
            g_active_gui_index == nav_idx ? ">" : " ",
            taskbar_window->title,
            nav_idx);
        taskbar_index++;
    }
    fprintf(objects, "OBJECT | %04d | tag=panel id=debug_row role=debug_row x=0 y=%d w=44 h=1 z=48 focused=false parent_id=wraith_root container_id=wraith_root source_ref=semantic:debug_row ancestor_chain=%s clip_chain=wraith_root fg=#F7FAFC bg=#7A633A border=#D2B16E label= action= src=\n",
        object_id++, debug_y, debug_chain);
    fprintf(objects, "OBJECT | %04d | tag=text id=desktop_debug_header role=frame_text x=0 y=%d w=20 h=1 z=49 focused=false parent_id=debug_row container_id=debug_row source_ref=semantic:debug_header ancestor_chain=%s>desktop_debug_header clip_chain=debug_row label_core=[ DESKTOP DEBUG ] fg=#F7FAFC bg=#7A633A border=#F7FAFC label=[ DESKTOP DEBUG ] action= src=\n",
        object_id++, debug_y, debug_chain);
    fprintf(objects, "OBJECT | %04d | tag=text id=debug_view_prefix role=frame_text x=0 y=%d w=6 h=1 z=49 focused=false parent_id=debug_row container_id=debug_row source_ref=semantic:debug_view_prefix ancestor_chain=%s>debug_view_prefix clip_chain=debug_row label_core=View: fg=#F7FAFC bg=#7A633A border=#F7FAFC label=View: action= src=\n",
        object_id++, debug_y, debug_chain);
    fprintf(objects, "OBJECT | %04d | tag=text id=debug_ascii role=debug_selector x=6 y=%d w=18 h=1 z=50 focused=%s parent_id=debug_row container_id=debug_row source_ref=semantic:debug_ascii ancestor_chain=%s>debug_ascii clip_chain=debug_row nav=%d nav_selected=%s nav_selector_glyph=%s label_core=ASCII* fg=#7EDFF2 bg=#7A633A border=#7EDFF2 label=ASCII* action=DESKTOP_ACTION:view_ascii src=\n",
        object_id++,
        debug_y,
        g_active_gui_index == debug_selector_ascii_index() ? "true" : "false",
        debug_chain,
        debug_selector_ascii_index(),
        g_active_gui_index == debug_selector_ascii_index() ? "true" : "false",
        g_active_gui_index == debug_selector_ascii_index() ? ">" : " ");
    fprintf(objects, "OBJECT | %04d | tag=text id=debug_gl role=debug_selector x=24 y=%d w=14 h=1 z=50 focused=%s parent_id=debug_row container_id=debug_row source_ref=semantic:debug_gl ancestor_chain=%s>debug_gl clip_chain=debug_row nav=%d nav_selected=%s nav_selector_glyph=%s label_core=GL fg=#FFD166 bg=#7A633A border=#FFD166 label=GL action=DESKTOP_ACTION:view_gl src=\n",
        object_id++,
        debug_y,
        g_active_gui_index == debug_selector_gl_index() ? "true" : "false",
        debug_chain,
        debug_selector_gl_index(),
        g_active_gui_index == debug_selector_gl_index() ? "true" : "false",
        g_active_gui_index == debug_selector_gl_index() ? ">" : " ");
    fprintf(objects, "OBJECT | %04d | tag=panel id=summary_row role=summary_row x=0 y=%d w=96 h=1 z=48 focused=false parent_id=wraith_root container_id=wraith_root source_ref=semantic:summary_row ancestor_chain=%s clip_chain=wraith_root fg=#F7FAFC bg=#7A633A border=#D2B16E label= action= src=\n",
        object_id++, summary_y, summary_chain);
    fprintf(objects, "OBJECT | %04d | tag=text id=desktop_summary role=frame_text x=0 y=%d w=80 h=1 z=49 focused=false parent_id=summary_row container_id=summary_row source_ref=semantic:desktop_summary ancestor_chain=%s>desktop_summary clip_chain=summary_row label_core=Desktop: %s | Open: %d | Focus: %s | Mode: half fg=#F7FAFC bg=#7A633A border=#F7FAFC label=Desktop: %s | Open: %d | Focus: %s | Mode: half action= src=\n",
        object_id++,
        summary_y,
        summary_chain,
        "desktop",
        g_window_count,
        window ? window->title : "Desktop",
        "desktop",
        g_window_count,
        window ? window->title : "Desktop");

    fprintf(objects, "OBJECT | %04d | tag=text id=mouse_cursor role=mouse_cursor x=%d y=%d w=1 h=1 z=99 focused=false parent_id=wraith_root container_id=wraith_root source_ref=semantic:mouse_cursor ancestor_chain=%s>mouse_cursor clip_chain=wraith_root fg=#FFFFFF bg=#0B1118 border=#FFFFFF label_core=< label=< action= src=\n",
        object_id++,
        (g_mouse_x / 10) + mouse_offset_x,
        (g_mouse_y / 18) + mouse_offset_y,
        root_chain);
    }

    fprintf(audit, "WRAITH FRAME AUDIT\n");
    fprintf(audit, "frame_id=%06d\n", g_next_instance_no);
    fprintf(audit, "project_id=wraith-alpha\n");
    fprintf(audit, "source_layout=projects/wraith-alpha/layouts/alpha-shell.chtpm\n");
    fprintf(audit, "generated_at_epoch=%ld\n", (long)time(NULL));
    fprintf(audit, "cols=96\n");
    fprintf(audit, "rows=30\n");
    fprintf(audit, "cell_width_px=10\n");
    fprintf(audit, "cell_height_px=18\n");
    fprintf(audit, "object_count=%d\n", object_id - 1);
    fprintf(audit, "outputs=current_frame.txt,current_frame.ansi.txt,current_frame.cells.pdl,current_frame.meta.pdl,current_frame.objects.pdl,current_frame.hitmap.pdl,current_frame.desktop_state.pdl,current_frame.window_stack.pdl,current_frame.focus_state.pdl,current_frame.mouse_state.pdl\n");
    fprintf(audit, "warnings=none\n");

    fprintf(desktop, "SECTION | KEY | VALUE\n");
    fprintf(desktop, "FRAME | frame_id | %06d\n", g_next_instance_no);
    fprintf(desktop, "DESKTOP | project_id | wraith-alpha\n");
    fprintf(desktop, "DESKTOP | title | Wraith Alpha\n");
    fprintf(desktop, "DESKTOP | cols | 96\n");
    fprintf(desktop, "DESKTOP | rows | 30\n");
    fprintf(desktop, "DESKTOP | object_count | %d\n", object_id - 1);
    fprintf(desktop, "DESKTOP | focused_object_id | %d\n", focused_object_id);
    fprintf(desktop, "WINDOW | id=wraith_root | title=WRAITH DESKTOP GUI x=0 y=0 w=96 h=30 z=1 focused=%s role=window\n",
        window ? "false" : "true");
    fprintf(desktop, "WINDOW | id=desktop_banner | title=WRAITH DESKTOP GUI x=0 y=0 w=96 h=2 z=2 focused=false role=banner\n");
    if (window) {
        fprintf(desktop, "WINDOW | id=%s | title=%s x=1 y=2 w=94 h=23 z=10 focused=true role=panel\n",
            window->id, window->title);
    }
    fprintf(desktop, "WINDOW | id=taskbar | title=taskbar x=0 y=28 w=96 h=2 z=40 focused=false role=banner\n");

    fprintf(focus, "SECTION | KEY | VALUE\n");
    fprintf(focus, "FRAME | frame_id | %06d\n", g_next_instance_no);
    fprintf(focus, "FOCUS | active_object_id | %d\n", focused_object_id);
    fprintf(focus, "FOCUS | active_object_dom_id | %s\n", focused_object_dom_id);
    fprintf(focus, "FOCUS | active_object_label | %s\n", focused_object_label);
    fprintf(focus, "FOCUS | active_object_role | %s\n", focused_object_role);

    fclose(meta);
    fclose(objects);
    fclose(audit);
    fclose(desktop);
    fclose(focus);
    rename(meta_tmp, meta_path);
    rename(objects_tmp, objects_path);
    rename(audit_tmp, audit_path);
    rename(desktop_tmp, desktop_path);
    rename(focus_tmp, focus_path);
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
    /* Additive only (settings-hub-window-geom-design-j5.md): no existing
       line touched. window-geom needs the active window's real
       project_id to know which project.pdl to read -- nothing else
       exposed this anywhere. */
    fprintf(f, "desktop_focused_window_project_id=%s\n", window ? window->project_id : "");
    fprintf(f, "desktop_focused_window_title=%s\n", focused_title);
    fprintf(f, "desktop_presenter_mode=%s\n", g_presenter_ascii_mode ? "ascii" : "gl");
    fprintf(f, "desktop_presenter_ascii_selected=*\n");
    fprintf(f, "desktop_presenter_gl_selected=\n");
    fprintf(f, "desktop_gl_presenter_mode=%s\n", g_presenter_ascii_mode ? "ascii" : "gl");
    fprintf(f, "desktop_gl_presenter_ascii_selected=%s\n", g_presenter_ascii_mode ? "*" : " ");
    fprintf(f, "desktop_gl_presenter_gl_selected=%s\n", g_presenter_ascii_mode ? " " : "*");
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
    /* g_max_index MUST be fresh before write_semantic_projection_files()
       (GL) and the ASCII raw-buffer builder run below, or any nav value
       derived as an offset from it (debug_selector_ascii_index(),
       debug_selector_gl_index()) reads whatever g_max_index was left over
       from a DIFFERENT context. recompute_nav_bounds() used to only run
       from route_input() (keyboard) or once at startup -- meaning any
       window opened via a MOUSE click (dispatch_menu_index(),
       launch_wraith_project_command(), open_window_geom_for_project(),
       etc., all of which call update_state() but never
       recompute_nav_bounds() themselves) could render at least once with
       a stale, too-small g_max_index left over from a "no window" or
       differently-sized state. Confirmed live during this feature's
       development: a since-reverted trailing-nav-slot design for the
       window-geom chrome button collided with the window's own title
       button (both got nav=1) for exactly this reason -- g_max_index was
       still 3 (the "no window" formula) at the moment that window's first
       render happened. Chrome no longer uses a trailing slot (see
       g_chrome_icons[]), so this exact symptom can't recur for it, but
       the underlying staleness gap this fixes is general -- keeping the
       fix here, unconditionally, closes it for every caller at once
       instead of hunting down each site that opens a window without
       going through route_input() first. */
    recompute_nav_bounds();

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

    write_semantic_projection_files();
    archive_receipt_history();

    log_alpha("State updated: idx=%d, windows=%d, minimized=%d", g_active_gui_index, g_window_count, count_minimized_windows());
}

static void append_history_file(FILE *out, const char *label, const char *path) {
    FILE *src;
    char line[1024];
    int wrote_any = 0;

    if (!out || !label || !path) {
        return;
    }

    fprintf(out, "[%s]\n", label);
    src = fopen(path, "r");
    if (!src) {
        fprintf(out, "missing=%s\n\n", path);
        return;
    }

    while (fgets(line, sizeof(line), src)) {
        fputs(line, out);
        wrote_any = 1;
    }
    if (wrote_any) {
        fputc('\n', out);
    }
    fputc('\n', out);
    fclose(src);
}

/* Receipt-history policy, read from pieces/config/receipts.conf (plain
   key=value). Defaults are used if the file or a key is missing. Keeps the
   rolling receipt-history file from ballooning into the hundreds of MB it
   reached when every state write appended a full ~50KB snapshot. */
static void read_receipt_config(int *max_entries, int *only_on_change, int *max_log_lines) {
    char path[MAX_PATH];
    char line[256];
    FILE *f;

    if (max_entries) *max_entries = 50;      /* default snapshot cap */
    if (only_on_change) *only_on_change = 1; /* default: skip identical re-renders */
    if (max_log_lines) *max_log_lines = 500; /* default line-based log cap */

    snprintf(path, sizeof(path), "%s/pieces/config/receipts.conf", g_project_root);
    f = fopen(path, "r");
    if (!f) {
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
        if (max_entries && strncmp(p, "max_entries=", 12) == 0) {
            int v = atoi(p + 12);
            if (v > 0) *max_entries = v;
        } else if (only_on_change && strncmp(p, "only_on_change=", 15) == 0) {
            *only_on_change = atoi(p + 15) ? 1 : 0;
        } else if (max_log_lines && strncmp(p, "max_log_lines=", 14) == 0) {
            int v = atoi(p + 14);
            if (v > 0) *max_log_lines = v;
        }
    }
    fclose(f);
}

/* Trims a plain line-based audit log to its last max_lines lines. Cheap
   size fast-path (skip if already small enough that trimming can't help),
   then two-pass streaming (count lines, copy from the keep-point) with an
   atomic temp+rename. Only for pure append-only audit files with NO
   byte-cursor reader — never call on history.txt / keyboard history, whose
   readers track byte offsets that a trim would desync. */
static void cap_log_file_lines(const char *path, int max_lines) {
    char tmp_path[MAX_PATH];
    char line[2048];
    struct stat st;
    FILE *in;
    FILE *out;
    long total = 0;
    long seen = 0;
    long skip;

    if (max_lines <= 0) {
        return;
    }
    /* Fast path: if the file is smaller than max_lines could ever be at a
       generous ~16 bytes/line, it definitely holds <= max_lines lines. */
    if (stat(path, &st) != 0 || st.st_size <= (off_t)max_lines * 16) {
        return;
    }
    in = fopen(path, "r");
    if (!in) {
        return;
    }
    while (fgets(line, sizeof(line), in)) {
        total++;
    }
    if (total <= max_lines) {
        fclose(in);
        return;
    }
    skip = total - max_lines;

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    out = fopen(tmp_path, "w");
    if (!out) {
        fclose(in);
        return;
    }
    rewind(in);
    while (fgets(line, sizeof(line), in)) {
        seen++;
        if (seen > skip) {
            fputs(line, out);
        }
    }
    fclose(in);
    fclose(out);
    rename(tmp_path, path);
}

/* Periodic janitor: trims the append-only audit logs/ledgers to the
   configured line cap. Called on a slow tick from the manager main loop
   (not every frame). Covers files written by other binaries (the display
   renderers) too, since it just trims by path — no need to touch every
   writer. Deliberately excludes byte-cursor files (history/keyboard). */
static void sweep_audit_logs(void) {
    int max_log_lines;
    char path[MAX_PATH];
    const char *rel_files[] = {
        "pieces/display/ledger.txt",
        "pieces/master_ledger/master_ledger.txt",
        "projects/wraith-alpha/manager/alpha_manager.log",
        "debug.txt", /* wraith_parser_alpha.c's append-only click/launch
                        log at the project root — found growing
                        unbounded (1.6MB) during a disk-usage survey,
                        same category as the other three above. */
    };
    size_t i;

    read_receipt_config(NULL, NULL, &max_log_lines);
    for (i = 0; i < sizeof(rel_files) / sizeof(rel_files[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s", g_project_root, rel_files[i]);
        cap_log_file_lines(path, max_log_lines);
    }
}

/* Reads the RGB render checksum ("render_checksum_fnv1a64=0x...") the
   daemon stamps into current_frame.receipt.pdl. Returns the raw string
   (empty on failure) so callers can cheaply compare "did the screen
   actually change since the last snapshot". */
static void read_current_render_checksum(char *out, size_t out_sz) {
    char path[MAX_PATH];
    char line[256];
    FILE *f;

    out[0] = '\0';
    snprintf(path, sizeof(path), "%s/projects/wraith-alpha/session/rgb/current_frame.receipt.pdl", g_project_root);
    f = fopen(path, "r");
    if (!f) {
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "render_checksum_fnv1a64=", 24) == 0) {
            char *v = line + 24;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

/* Trims history_path so it retains only the last max_entries snapshot
   blocks (each delimited by a line starting with "=== RECEIPT SNAPSHOT").
   Two-pass streaming (count markers, then copy from the keep-point) so it
   never loads the whole file into memory. Atomic temp+rename. */
static void cap_receipt_snapshots(const char *history_path, int max_entries) {
    char tmp_path[MAX_PATH];
    char line[1024];
    FILE *in;
    FILE *out;
    int total = 0;
    int seen = 0;
    int skip;

    if (max_entries <= 0) {
        return;
    }
    in = fopen(history_path, "r");
    if (!in) {
        return;
    }
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "=== RECEIPT SNAPSHOT", 20) == 0) {
            total++;
        }
    }
    if (total <= max_entries) {
        fclose(in);
        return;
    }
    skip = total - max_entries; /* number of oldest snapshots to drop */

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", history_path);
    out = fopen(tmp_path, "w");
    if (!out) {
        fclose(in);
        return;
    }
    rewind(in);
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "=== RECEIPT SNAPSHOT", 20) == 0) {
            seen++;
        }
        if (seen > skip) {
            fputs(line, out);
        }
    }
    fclose(in);
    fclose(out);
    rename(tmp_path, history_path);
}

static void archive_receipt_history(void) {
    static char last_checksum[64] = "";
    char history_path[MAX_PATH];
    char rgb_receipt[MAX_PATH];
    char gl_input_receipt[MAX_PATH];
    char gl_display_receipt[MAX_PATH];
    char meta_path[MAX_PATH];
    char checksum[64];
    int max_entries;
    int only_on_change;
    FILE *out;
    time_t now;
    char *ts;

    read_receipt_config(&max_entries, &only_on_change, NULL);

    /* Change-gate: only snapshot when the rendered frame actually changed.
       This is what stops the mouse-move / idle-re-render flood (each of
       which used to append ~50KB) from ever reaching disk. */
    read_current_render_checksum(checksum, sizeof(checksum));
    if (only_on_change && checksum[0] && strcmp(checksum, last_checksum) == 0) {
        return;
    }

    snprintf(history_path, sizeof(history_path), "%s/pieces/debug/frames/session_receipt_history.txt", g_project_root);
    snprintf(rgb_receipt, sizeof(rgb_receipt), "%s/projects/wraith-alpha/session/rgb/current_frame.receipt.pdl", g_project_root);
    snprintf(gl_input_receipt, sizeof(gl_input_receipt), "%s/projects/wraith-alpha/session/rgb/gl_input.receipt.pdl", g_project_root);
    snprintf(gl_display_receipt, sizeof(gl_display_receipt), "%s/projects/wraith-alpha/session/rgb/gl_display.receipt.pdl", g_project_root);
    snprintf(meta_path, sizeof(meta_path), "%s/pieces/display/current_frame.meta.pdl", g_project_root);

    out = fopen(history_path, "a");
    if (!out) {
        return;
    }

    now = time(NULL);
    ts = ctime(&now);
    if (ts && strlen(ts) > 0) {
        ts[strlen(ts) - 1] = '\0';
    }

    fprintf(out, "=== RECEIPT SNAPSHOT ts=%s frame_hint=%06d presenter=%s focus=%s windows=%d ===\n",
        ts ? ts : "time",
        g_next_instance_no,
        g_presenter_ascii_mode ? "ascii" : "gl",
        active_window() ? active_window()->title : "Desktop",
        g_window_count);

    append_history_file(out, "current_frame.meta.pdl", meta_path);
    append_history_file(out, "current_frame.receipt.pdl", rgb_receipt);
    append_history_file(out, "gl_input.receipt.pdl", gl_input_receipt);
    append_history_file(out, "gl_display.receipt.pdl", gl_display_receipt);
    fclose(out);

    snprintf(last_checksum, sizeof(last_checksum), "%s", checksum);

    /* Rolling cap: keep only the last max_entries snapshots on disk. */
    cap_receipt_snapshots(history_path, max_entries);
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

/* Chrome-button targeted invocation (settings-hub-window-geom-design-j5.md
 * "Mode 2: Standalone"): pre-writes window-geom's own session/wg_target.txt
 * with the target project's id, then opens window-geom as its own
 * standalone Window via the same launch_window_instance() every other
 * wraith sub-project uses. window-geom_manager.c's write_wraith_body()
 * reads wg_target.txt on every render, so this is the entire handoff -- no
 * other shared state needed. Shared by both the named
 * DESKTOP_ACTION:open_window_geom: command and the chrome "&" icon's
 * SET_ACTIVE:<N> -> dispatch_menu_index() path (via g_chrome_icons[]'s
 * CHROME_ACTION_OPEN_WINDOW_GEOM case), so there's exactly one place
 * that does this, not two copies that could drift. */
static void open_window_geom_for_project(const char *target_project_id) {
    char wg_target_path[MAX_PATH];
    char regenerate_marker_path[MAX_PATH];
    FILE *f;

    if (!target_project_id || !target_project_id[0]) {
        return;
    }
    snprintf(wg_target_path, sizeof(wg_target_path),
             "%s/projects/wraith-alpha/wraith-projects/settings/window-geom/session/wg_target.txt",
             g_project_root);
    f = fopen(wg_target_path, "w");
    if (f) {
        fprintf(f, "%s\n", target_project_id);
        fclose(f);
    }
    /* wg_target.txt is a whole-file overwrite (not append), so its own
       size can't be trusted as a growth signal if window-geom's manager
       is already alive from an earlier open. Bump the SAME
       regenerate_marker.txt its hot-path loop already watches
       (append-only growth, the real marker-file convention) so a
       retarget while already running is picked up exactly like a
       KEY:5-13 edit is. */
    snprintf(regenerate_marker_path, sizeof(regenerate_marker_path),
             "%s/projects/wraith-alpha/wraith-projects/settings/window-geom/session/regenerate_marker.txt",
             g_project_root);
    f = fopen(regenerate_marker_path, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }
    launch_window_instance("window-geom", "WINDOW GEOM",
                            "wraith-alpha/wraith-projects/settings/window-geom");
}

static bool dispatch_menu_index(int menu_index) {
    Window *window = active_window();
    int launcher_count = count_launcher_methods();
    int taskbar_count = count_visible_windows();
    int project_nav_count = window ? count_project_nav_controls() : 0;
    int extra_project_slots = project_nav_count > 1 ? (project_nav_count - 1) : 0;
    /* CHROME_CONTENT_START, not a literal -- see g_chrome_icons[]'s comment. */
    int launcher_start = CHROME_CONTENT_START + extra_project_slots;
    int taskbar_start;
    int ascii_idx = debug_selector_ascii_index();
    int gl_idx = debug_selector_gl_index();

    taskbar_start = window ? (launcher_start + launcher_count) : 1;

    if (menu_index < 1) {
        return false;
    }

    if (menu_index == ascii_idx) {
        g_presenter_ascii_mode = 1;
        update_state(0);
        trigger_render();
        return true;
    }
    if (menu_index == gl_idx) {
        g_presenter_ascii_mode = 0;
        update_state(0);
        trigger_render();
        return true;
    }

    if (!window) {
        if (menu_index == 1) {
            launch_default_terminal();
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
    /* Chrome icon row: one generic branch instead of a separate
       if-block per icon, driven by the SAME g_chrome_icons[] array both
       renderers' emission code reads -- nav 2+ci resolves to
       g_chrome_icons[ci]'s own action, whatever that array currently
       contains. Adding a 5th icon means adding one array entry, not a
       5th copy-pasted if-block here. */
    {
        int ci;
        for (ci = 0; ci < CHROME_ICON_COUNT; ci++) {
            if (menu_index == 2 + ci) {
                if (!window) {
                    return false;
                }
                switch (g_chrome_icons[ci].action) {
                    case CHROME_ACTION_FOCUS:
                        g_active_gui_index = menu_index;
                        break;
                    case CHROME_ACTION_MINIMIZE:
                        minimize_active_window();
                        break;
                    case CHROME_ACTION_CLOSE:
                        close_active_window();
                        break;
                    case CHROME_ACTION_OPEN_WINDOW_GEOM:
                        open_window_geom_for_project(window->project_id);
                        break;
                }
                update_state(0);
                trigger_render();
                return true;
            }
        }
    }
    /* Generic "just focus it" fallback for embedded project-body content
       (cli_io fields, scene.objects.pdl controls) -- everything between
       chrome (1..CHROME_CONTENT_START-1) and launcher_start. This is the
       exact gap that made cli_io fields unclickable in GL: a <cli_io>
       tag has no onClick/href, so hit_test_semantic_action() in
       wraith_gl.c can only synthesize SET_ACTIVE:<nav> for it (never a
       real action string) -- and until this branch existed, THAT number
       matched no case here at all, so dispatch_menu_index() fell through
       to `return false` and g_active_gui_index never moved. Buttons in
       the same body (KEY:5, KEY:13, etc.) never needed this: they carry
       a real action, so their click bypasses dispatch_menu_index()
       entirely via route_command()'s own "KEY:" branch. Mirrors exactly
       what menu_index==1 (title) and CHROME_ACTION_FOCUS ('o') already
       do -- just move focus, nothing project-specific. */
    if (menu_index >= CHROME_CONTENT_START && menu_index < launcher_start) {
        g_active_gui_index = menu_index;
        update_state(0);
        trigger_render();
        return true;
    }
    if (menu_index >= launcher_start && menu_index < launcher_start + launcher_count) {
        if (dispatch_launcher_method_by_index(menu_index - launcher_start + 1)) {
            return true;
        }
        return false;
    }
    if (menu_index == taskbar_start) {
        launch_default_terminal();
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

static bool active_project_dir(char *out, size_t out_sz) {
    return project_dir_for_window(active_window(), out, out_sz);
}

static int read_project_map_control(void) {
    char project_dir[MAX_PATH];
    char state_path[MAX_PATH];
    char line[256];
    FILE *f;

    if (!active_project_dir(project_dir, sizeof(project_dir))) {
        return 0;
    }
    snprintf(state_path, sizeof(state_path), "%s/session/state.txt", project_dir);
    f = fopen(state_path, "r");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "is_map_control=", 15) == 0) {
            int value = atoi(line + 15);
            fclose(f);
            return value;
        }
    }
    fclose(f);
    return 0;
}

static void append_project_history(const char *kind, const char *value) {
    char project_dir[MAX_PATH];
    char history_path[MAX_PATH];
    FILE *f;
    time_t now = time(NULL);
    char *ts = ctime(&now);

    if (!kind || !value || !active_project_dir(project_dir, sizeof(project_dir))) {
        return;
    }
    if (ts && strlen(ts) > 0) {
        ts[strlen(ts) - 1] = '\0';
    }
    snprintf(history_path, sizeof(history_path), "%s/session/history.txt", project_dir);
    f = fopen(history_path, "a");
    if (!f) {
        return;
    }
    fprintf(f, "[%s] %s: %s\n", ts ? ts : "time", kind, value);
    fclose(f);
}

static void run_active_project_input_op(void) {
    char project_dir[MAX_PATH];
    char op_path[MAX_PATH];
    pid_t pid;
    int status;

    if (!active_project_dir(project_dir, sizeof(project_dir))) {
        return;
    }
    snprintf(op_path, sizeof(op_path), "%s/ops/+x/wraith_project_input.+x", project_dir);
    if (access(op_path, X_OK) != 0) {
        return;
    }
    pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, project_dir, NULL);
        _exit(127);
    }
    if (pid > 0) {
        waitpid(pid, &status, 0);
    }
}

static void run_active_project_input_op_mode(const char *mode) {
    char project_dir[MAX_PATH];
    char op_path[MAX_PATH];
    pid_t pid;
    int status;

    if (!active_project_dir(project_dir, sizeof(project_dir))) {
        return;
    }
    snprintf(op_path, sizeof(op_path), "%s/ops/+x/wraith_project_input.+x", project_dir);
    if (access(op_path, X_OK) != 0) {
        return;
    }
    pid = fork();
    if (pid == 0) {
        if (mode && mode[0]) {
            execl(op_path, op_path, project_dir, mode, NULL);
        } else {
            execl(op_path, op_path, project_dir, NULL);
        }
        _exit(127);
    }
    if (pid > 0) {
        waitpid(pid, &status, 0);
    }
}

static void process_active_project_marker(long *last_pos, char *last_project_dir, size_t dir_sz) {
    char project_dir[MAX_PATH];
    char marker_path[MAX_PATH];
    struct stat st;

    if (!last_pos || !last_project_dir || dir_sz == 0) {
        return;
    }
    if (!active_project_dir(project_dir, sizeof(project_dir))) {
        last_project_dir[0] = '\0';
        *last_pos = 0;
        return;
    }
    if (strcmp(last_project_dir, project_dir) != 0) {
        snprintf(last_project_dir, dir_sz, "%s", project_dir);
        *last_pos = 0;
    }

    snprintf(marker_path, sizeof(marker_path), "%s/session/fs_watch.marker", project_dir);
    if (stat(marker_path, &st) != 0) {
        return;
    }
    if (st.st_size < *last_pos) {
        *last_pos = 0;
    }
    if (st.st_size <= *last_pos) {
        return;
    }

    *last_pos = st.st_size;
    run_active_project_input_op_mode("marker_tick");
    update_state(0);
    trigger_render();
}

static int nav_index_for_action(const char *wanted_action) {
    char objects_path[MAX_PATH];
    char line[2048];
    char value[256];
    FILE *f;

    if (!wanted_action || !wanted_action[0]) {
        return 0;
    }
    snprintf(objects_path, sizeof(objects_path), "%s/pieces/display/current_frame.objects.pdl", g_project_root);
    f = fopen(objects_path, "r");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        int nav = 0;
        char action[256] = "";
        if (strncmp(line, "OBJECT", 6) != 0) {
            continue;
        }
        if (line_kvp_value(line, "action", value, sizeof(value))) {
            snprintf(action, sizeof(action), "%s", value);
        }
        if (strcmp(action, wanted_action) != 0) {
            continue;
        }
        if (line_kvp_value(line, "nav", value, sizeof(value))) {
            nav = atoi(value);
        }
        if (nav > 0) {
            fclose(f);
            return nav;
        }
    }
    fclose(f);
    return 0;
}

static void set_project_map_control_in_dir(const char *project_dir, int enabled, int emit_history) {
    char state_path[MAX_PATH];
    char tmp_path[MAX_PATH];
    char lines[128][512];
    int count = 0;
    int found = 0;
    FILE *in;
    FILE *out;
    int i;

    if (!project_dir || !project_dir[0]) {
        return;
    }
    snprintf(state_path, sizeof(state_path), "%s/session/state.txt", project_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", state_path);
    in = fopen(state_path, "r");
    if (in) {
        while (count < 128 && fgets(lines[count], sizeof(lines[count]), in)) {
            if (strncmp(lines[count], "is_map_control=", 15) == 0) {
                snprintf(lines[count], sizeof(lines[count]), "is_map_control=%d\n", enabled ? 1 : 0);
                found = 1;
            }
            count++;
        }
        fclose(in);
    }
    if (!found && count < 128) {
        snprintf(lines[count++], sizeof(lines[0]), "is_map_control=%d\n", enabled ? 1 : 0);
    }
    out = fopen(tmp_path, "w");
    if (!out) {
        return;
    }
    for (i = 0; i < count; i++) {
        fputs(lines[i], out);
    }
    fclose(out);
    rename(tmp_path, state_path);
    if (enabled) {
        int interact_nav = nav_index_for_action("INTERACT");
        g_map_control_nav_index = interact_nav > 0 ? interact_nav : g_active_gui_index;
        if (g_map_control_nav_index > 0) {
            g_active_gui_index = g_map_control_nav_index;
        }
    } else {
        g_map_control_nav_index = 0;
    }
    if (emit_history) {
        append_project_history("COMMAND", enabled ? "INTERACT" : "ESC");
        run_active_project_input_op();
        update_state(0);
        trigger_render();
    }
}

/* Binary debug toggle for the camera-reset-on-open behavior below,
   read from pieces/config/wraith_debug.conf's `camera_reset_on_open=`
   key (default 1 = on, matching normal/shipped behavior). Set it to 0
   while actively debugging camera code — the last session's orbited/
   panned angle then persists across both a fresh window launch AND a
   full manager/Wraith restart, instead of snapping back to
   camera_default.txt every time, which otherwise makes it hard to tell
   "did my fix actually stick" from "the reset just papered over it".
   Flip back to 1 for normal use. Re-read on every call rather than
   cached, so toggling the file takes effect on the very next open —
   this only runs once per window (re)open, not per-frame, so the extra
   file read is free. */
static int camera_reset_on_open_enabled(void) {
    char path[MAX_PATH];
    char line[128];
    FILE *f;
    int enabled = 1;

    snprintf(path, sizeof(path), "%s/pieces/config/wraith_debug.conf", g_project_root);
    f = fopen(path, "r");
    if (!f) {
        return enabled;
    }
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
        if (strncmp(p, "camera_reset_on_open=", 21) == 0) {
            enabled = atoi(p + 21) ? 1 : 0;
        }
    }
    fclose(f);
    return enabled;
}

/* On every window (re)open, if the project ships assets/camera_default.txt,
   overwrite the matching key=value lines in its session/state.txt with the
   file's values so the map/view always starts from a known default instead
   of the last session's mouse-orbited/panned angle. Generic Wraith
   mechanism — any project with that file opts in; projects without it are
   untouched (early return). Only the keys the file lists are reset, so a
   camera-only default file leaves xelector position etc. alone. Modeled on
   set_project_map_control_in_dir()'s state.txt read-replace-append-atomic
   rewrite. See x0.piececrafts/py3d-inspo.md.

   Gated by camera_reset_on_open_enabled() (see above) — the debug toggle.
   Also now called from two places, not one: launch_window_instance() (a
   genuinely new window open) AND main() at manager startup for every
   already-open window restored from disk (see sync_registry_from_disk())
   — the latter was the actual gap being reported: restarting the whole
   Wraith/manager process resumes previously-open windows straight from
   their persisted registry, never going through launch_window_instance(),
   so the reset never used to fire for that path at all. */
static void reset_project_view_from_default(const char *project_dir) {
    if (!camera_reset_on_open_enabled()) {
        return;
    }
    char default_path[MAX_PATH];
    char state_path[MAX_PATH];
    char tmp_path[MAX_PATH];
    char dkeys[32][64];
    char dlines[32][256];
    int dused[32];
    int dcount = 0;
    char lines[128][512];
    int count = 0;
    char line[256];
    FILE *f;
    FILE *out;
    int i, j;

    if (!project_dir || !project_dir[0]) {
        return;
    }
    snprintf(default_path, sizeof(default_path), "%s/assets/camera_default.txt", project_dir);
    f = fopen(default_path, "r");
    if (!f) {
        return;
    }
    while (dcount < 32 && fgets(line, sizeof(line), f)) {
        char *p = line;
        char *eq;
        char *nl;
        size_t klen;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') {
            continue;
        }
        eq = strchr(p, '=');
        if (!eq) {
            continue;
        }
        klen = (size_t)(eq - p);
        if (klen == 0 || klen >= sizeof(dkeys[0])) {
            continue;
        }
        memcpy(dkeys[dcount], p, klen);
        dkeys[dcount][klen] = '\0';
        nl = strpbrk(p, "\r\n");
        if (nl) {
            *nl = '\0';
        }
        snprintf(dlines[dcount], sizeof(dlines[dcount]), "%s\n", p);
        dused[dcount] = 0;
        dcount++;
    }
    fclose(f);
    if (dcount == 0) {
        return;
    }

    snprintf(state_path, sizeof(state_path), "%s/session/state.txt", project_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", state_path);
    f = fopen(state_path, "r");
    if (f) {
        while (count < 128 && fgets(lines[count], sizeof(lines[count]), f)) {
            char *eq = strchr(lines[count], '=');
            if (eq) {
                size_t klen = (size_t)(eq - lines[count]);
                for (j = 0; j < dcount; j++) {
                    if (strlen(dkeys[j]) == klen &&
                        strncmp(lines[count], dkeys[j], klen) == 0) {
                        snprintf(lines[count], sizeof(lines[count]), "%s", dlines[j]);
                        dused[j] = 1;
                        break;
                    }
                }
            }
            count++;
        }
        fclose(f);
    }
    for (j = 0; j < dcount && count < 128; j++) {
        if (!dused[j]) {
            snprintf(lines[count++], sizeof(lines[0]), "%s", dlines[j]);
        }
    }

    out = fopen(tmp_path, "w");
    if (!out) {
        return;
    }
    for (i = 0; i < count; i++) {
        fputs(lines[i], out);
    }
    fclose(out);
    rename(tmp_path, state_path);
}

/* Applies reset_project_view_from_default() to every currently-open
   window, called once at manager startup right after
   sync_registry_from_disk() restores the persisted window registry. This
   is the actual fix for "3D view isn't resetting when a new session
   starts": launch_window_instance()'s reset only ever fired for a
   genuinely new window open (clicking a project in the launcher).
   Restarting the whole manager/Wraith process instead resumes previously
   -open windows straight from disk via sync_registry_from_disk(), which
   never called launch_window_instance() (or anything else) for them —
   so a window that was already open before a restart kept whatever
   camera angle it had, forever. This closes that gap without duplicating
   logic: same reset_project_view_from_default() call, same
   camera_reset_on_open_enabled() debug gate, just a second call site. */
static void reset_all_open_project_views_on_startup(void) {
    int i;
    for (i = 0; i < g_window_count; i++) {
        char project_dir[MAX_PATH];
        if (g_windows[i].state == WSTATE_CLOSED) {
            continue;
        }
        if (project_dir_for_window(&g_windows[i], project_dir, sizeof(project_dir))) {
            reset_project_view_from_default(project_dir);
        }
    }
}

static void set_project_map_control(int enabled) {
    char project_dir[MAX_PATH];

    if (!active_project_dir(project_dir, sizeof(project_dir))) {
        return;
    }
    set_project_map_control_in_dir(project_dir, enabled, 1);
}

static bool action_for_nav_index(int nav_index, char *out, size_t out_sz) {
    char objects_path[MAX_PATH];
    char line[2048];
    char value[256];
    FILE *f;

    if (!out || out_sz == 0 || nav_index <= 0) {
        return false;
    }
    out[0] = '\0';
    snprintf(objects_path, sizeof(objects_path), "%s/pieces/display/current_frame.objects.pdl", g_project_root);
    f = fopen(objects_path, "r");
    if (!f) {
        return false;
    }
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "OBJECT |", 8) != 0) {
            continue;
        }
        if (!line_kvp_value(line, "nav", value, sizeof(value)) || atoi(value) != nav_index) {
            continue;
        }
        if (line_kvp_value(line, "action", value, sizeof(value)) && value[0]) {
            snprintf(out, out_sz, "%s", value);
            fclose(f);
            return true;
        }
    }
    fclose(f);
    return false;
}

static bool launch_wraith_project_command(const char *cmd) {
    const WraithLauncher *launcher;

    if (!cmd || cmd[0] == '\0') {
        return false;
    }

    launcher = find_launcher_by_command(cmd);
    if (!launcher) {
        return false;
    }

    launch_window_instance(
        launcher->id_prefix,
        launcher->title_prefix,
        launcher->project_id
    );
    update_state(0);
    trigger_render();
    return true;

}

static bool dispatch_launcher_method_by_index(int launcher_idx) {
    const WraithLauncher *launcher;

    if (launcher_idx < 1) {
        return false;
    }
    if (discover_launcher_projects() <= 0) {
        log_alpha("Launcher dispatch failed: no discovered nested Wraith projects");
        return false;
    }
    if (launcher_idx > g_launcher_count) {
        log_alpha("Launcher dispatch failed: index %d out of range (count=%d)", launcher_idx, g_launcher_count);
        return false;
    }

    launcher = &g_launchers[launcher_idx - 1];
    log_alpha("Launcher dispatch resolved: %s -> %s", launcher->display_label, launcher->command);
    route_command(launcher->command);
    return true;
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
    if (strcmp(cmd, "INTERACT") == 0) {
        set_project_map_control(1);
        return;
    }
    if (strcmp(cmd, "ESC") == 0) {
        set_project_map_control(0);
        return;
    }
    if (strncmp(cmd, "KEY:", 4) == 0) {
        int key_code = atoi(cmd + 4);
        char key_buf[32];
        snprintf(key_buf, sizeof(key_buf), "%d", key_code);
        append_project_history("KEY_PRESSED", key_buf);
        run_active_project_input_op();
        update_state(key_code);
        trigger_render();
        return;
    }
    if (strncmp(cmd, "PROJECT_ACTION:", 15) == 0) {
        append_project_history("COMMAND", cmd + 15);
        run_active_project_input_op();
        update_state(0);
        trigger_render();
        return;
    }
    if (strncmp(cmd, "SETTINGS_PAGE:", 14) == 0) {
        char page_name[256];
        char page_file[MAX_PATH];
        FILE *f;
        strncpy(page_name, cmd + 14, sizeof(page_name) - 1);
        page_name[sizeof(page_name) - 1] = '\0';
        snprintf(page_file, sizeof(page_file), "%s/projects/wraith-alpha/wraith-projects/settings/session/state_changed.txt", g_project_root);
        f = fopen(page_file, "a");
        if (f) {
            fprintf(f, "%s\n", page_name);
            fclose(f);
        }
        trigger_render();
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
    if (strcmp(cmd, "DESKTOP_ACTION:view_ascii") == 0) {
        g_presenter_ascii_mode = 1;
        g_active_gui_index = 1;
        update_state(0);
        trigger_render();
        return;
    }
    if (strcmp(cmd, "DESKTOP_ACTION:view_gl") == 0) {
        g_presenter_ascii_mode = 0;
        g_active_gui_index = 1;
        update_state(0);
        trigger_render();
        return;
    }

    /* Chrome-button targeted invocation, named-command form. Kept for any
     * explicit/future caller that wants to name a project directly; the
     * chrome "&" icon itself now goes through SET_ACTIVE:<N> ->
     * dispatch_menu_index()'s g_chrome_icons[] loop instead (see
     * CHROME_ACTION_OPEN_WINDOW_GEOM there), so both routes funnel into
     * the same open_window_geom_for_project() helper. */
    if (strncmp(cmd, "DESKTOP_ACTION:open_window_geom:", 33) == 0) {
        open_window_geom_for_project(cmd + 33);
        return;
    }

    /* Found during 2026-07-05 code review, not something introduced today
     * on purpose: in ASCII mode, an embedded <button href="..."> is
     * clicked directly by wraith_parser_alpha.c's own element tree, which
     * owns href navigation entirely in-process (strncpy into its own
     * current_layout global) -- route_command() is never even involved.
     * But GL-mode clicks/keyboard-Enter go through a completely different
     * path (wraith_gl.c's hit_test_semantic_action() / action_for_nav_index()
     * -> here), which flattens everything down to a plain action= string
     * with no memory of "this came from an href". A raw layout path
     * reaching here (e.g. "projects/.../window-geom.chtpm", the action
     * emit_embedded_line_objects() extracts from href=) matched none of
     * the prefixes above and was silently dropped -- the button looked
     * right and was nav-indexed correctly, but activating it in GL mode
     * did nothing. request_layout_change() already existed for exactly
     * this (writes to pieces/display/layout_changed.txt, which
     * wraith_parser_alpha.c already polls and applies -- confirmed by
     * reading its own poll loop) but was completely unused (the compiler
     * already flagged it as such). This is the missing wire, not new
     * infrastructure. */
    if (cmd[0] && strstr(cmd, ".chtpm")) {
        request_layout_change(cmd);
        trigger_render();
        return;
    }
}

static void launch_default_terminal(void) {
    const WraithLauncher *terminal_launcher = find_terminal_launcher();
    if (terminal_launcher) {
        launch_wraith_project_command(terminal_launcher->command);
    } else {
        launch_wraith_project_command("DESKTOP_ACTION:launch_terminal");
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

    /* Camera drag: while in map-control (interact) mode, a held-left drag
       over the game surface orbits the camera — same gate as keyboard
       (interact mode = the game owns input), matching plugy3d/gl-os mouse
       camera control. The left-button drag arrives as a stream of btn=0
       events; we anchor on the first and forward each subsequent event's
       pixel delta to the active project's op as a MOUSE_DRAG history
       entry (the exact same forward path as KEY_PRESSED). Outside
       map-control mode this whole block is skipped, so mouse still just
       moves the desktop cursor as before. */
    if (btn == 0 && read_project_map_control()) {
        if (!g_drag_anchored) {
            g_drag_prev_x = x;
            g_drag_prev_y = y;
            g_drag_anchored = 1;
        } else {
            int dx = x - g_drag_prev_x;
            int dy = y - g_drag_prev_y;
            if (dx * dx + dy * dy >= 4) { /* ~2px deadzone, skip jitter */
                char buf[64];
                snprintf(buf, sizeof(buf), "%d %d", dx, dy);
                append_project_history("MOUSE_DRAG", buf);
                run_active_project_input_op();
                g_drag_prev_x = x;
                g_drag_prev_y = y;
                update_state(0);
                trigger_render();
                return;
            }
        }
    } else {
        /* release, non-left button, or not in map-control: end any drag */
        g_drag_anchored = 0;
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
    int project_nav_count = window ? count_project_nav_controls() : 0;
    int extra_project_slots = project_nav_count > 1 ? (project_nav_count - 1) : 0;

    if (window) {
        /* (CHROME_CONTENT_START - 1), not a literal -- title (1) plus
           CHROME_ICON_COUNT fixed icon slots, derived from
           g_chrome_icons[]'s own length. Trailing "+2" is unrelated to
           chrome; it reserves the two debug selectors
           (debug_selector_ascii_index()/gl_index() = max-1/max). */
        g_max_index = (CHROME_CONTENT_START - 1) + launcher_count + 1 + extra_project_slots + taskbar_count + 2;
    } else {
        g_max_index = 1 + taskbar_count + 2;
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

static void purge_tracked_process_name(const char *name) {
    char path[MAX_PATH];
    char tmp_path[MAX_PATH];
    char line[256];
    FILE *in;
    FILE *out;

    if (!name || !name[0]) {
        return;
    }

    snprintf(path, sizeof(path), "%s/pieces/os/proc_list.txt", g_project_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/pieces/os/proc_list.txt.tmp", g_project_root);
    in = fopen(path, "r");
    if (!in) {
        return;
    }
    out = fopen(tmp_path, "w");
    if (!out) {
        fclose(in);
        return;
    }

    while (fgets(line, sizeof(line), in)) {
        int pid = -1;
        char proc_name[128] = "";
        if (sscanf(line, "%d %127s", &pid, proc_name) == 2 && strcmp(proc_name, name) == 0) {
            if (pid > 1) {
                kill(pid, SIGTERM);
                usleep(200000);
                kill(pid, SIGKILL);
                log_alpha("Purged stale tracked process %s (pid=%d)", name, pid);
            }
            continue;
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    rename(tmp_path, path);
}

/* Shutdown-time cleanup, mirroring pieces/chtpm/plugins/orchestrator.c's
   own kill_all_tracked_processes() (the "HOLY Pattern - File-Backed"
   process tracking already used for the ~40 other, non-wraith-alpha
   projects orchestrator.c launches) term-for-term: two phases (SIGTERM to
   every tracked pid's own process group AND the pid directly, wait 200ms
   for graceful exit, then SIGKILL the same way to any survivors), reading
   the SAME shared pieces/os/proc_list.txt file both this manager and the
   orchestrator already write to via log_pid(). This did not exist here
   before -- wraith-alpha_manager.c's own shutdown path (see
   handle_signal()'s call site, end of main()) previously just logged
   "shutting down" and returned, leaving every forked child (project
   managers, wraith_gl, wraith_rgb_daemon) running as an orphan. Not a new
   invention: replicated the existing, working pattern rather than
   designing a new one, per this codebase's own "use existing patterns"
   standard. */
static void kill_all_tracked_processes(void) {
    char path[MAX_PATH];
    FILE *f;
    char line[256];

    snprintf(path, sizeof(path), "%s/pieces/os/proc_list.txt", g_project_root);

    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            int pid;
            char name[128];
            if (sscanf(line, "%d %127s", &pid, name) == 2 && pid > 1) {
                kill(-pid, SIGTERM);
                kill(pid, SIGTERM);
            }
        }
        fclose(f);
    }

    usleep(200000);

    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            int pid;
            char name[128];
            if (sscanf(line, "%d %127s", &pid, name) == 2 && pid > 1) {
                kill(-pid, SIGKILL);
                kill(pid, SIGKILL);
                waitpid(pid, NULL, WNOHANG);
            }
        }
        fclose(f);
    }

    /* Clear the file for next run, same as orchestrator.c does. */
    f = fopen(path, "w");
    if (f) fclose(f);
}

/* Final sweep: exec kill_all.sh directly (fork/exec, not system() -- the
   orchestrator's own equivalent step uses system("bash kill_all.sh ..."),
   which is the one part of that pattern NOT replicated verbatim here,
   since this codebase's own CPU-safety standard is explicit that system()
   must never be used for child process management (see the TPMOS Bible,
   section 3, "The Fuzzpet Pattern"). Same practical effect -- kill_all.sh
   runs and does its own broad pattern-based pkill sweep as a last-resort
   safety net -- via the mandated fork()/exec()/waitpid() shape instead. */
static void run_final_kill_sweep(void) {
    char script_path[MAX_PATH];
    pid_t pid;
    int status;

    snprintf(script_path, sizeof(script_path), "%s/pieces/os/kill_all.sh", g_project_root);
    if (access(script_path, X_OK) != 0 && access(script_path, F_OK) != 0) {
        return;
    }

    pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        if (chdir(g_project_root) != 0) {
            _exit(127);
        }
        execl("/bin/bash", "bash", "pieces/os/kill_all.sh", (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        waitpid(pid, &status, 0);
    }
}

static void sync_active_gui_index_from_display(void) {
    char path[MAX_PATH];
    FILE *f = NULL;
    char line[64];
    int idx;

    if (read_project_map_control()) {
        if (g_map_control_nav_index > 0) {
            g_active_gui_index = g_map_control_nav_index;
        }
        return;
    }

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

    /* Companion read for g_active_gui_is_typing -- see that global's own
     * comment and export_active_index() in wraith_parser_alpha.c
     * (2fix-july6.txt, bug 3). Defaults to 0 (not typing) if the file is
     * missing/unreadable, which is the safe fallback (shows ">" not "^"). */
    g_active_gui_is_typing = 0;
    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_is_typing.txt", g_project_root);
    f = fopen(path, "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) {
            g_active_gui_is_typing = (atoi(line) != 0);
        }
        fclose(f);
    }
}

static void launch_rgb_pipeline(void) {
    char daemon_path[MAX_PATH];
    char gl_path[MAX_PATH];

    snprintf(daemon_path, sizeof(daemon_path), "%s/projects/wraith-alpha/plugins/+x/wraith_rgb_daemon.+x", g_project_root);
    snprintf(gl_path, sizeof(gl_path), "%s/projects/wraith-alpha/ops/+x/wraith_gl.+x", g_project_root);

    log_alpha("Launching RGB Pipeline...");
    purge_tracked_process_name("wraith_rgb_daemon");
    purge_tracked_process_name("wraith_gl");

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

static void log_runtime_identity(void) {
    struct stat st;
    char self_path[MAX_PATH];

    snprintf(self_path, sizeof(self_path), "%s/projects/wraith-alpha/manager/+x/wraith-alpha_manager.+x", g_project_root);
    if (stat(self_path, &st) == 0) {
        log_alpha("Runtime identity: root=%s binary=%s mtime=%lld build=%s %s",
            g_project_root,
            self_path,
            (long long)st.st_mtime,
            __DATE__,
            __TIME__);
    } else {
        log_alpha("Runtime identity: root=%s binary=%s build=%s %s (stat failed)",
            g_project_root,
            self_path,
            __DATE__,
            __TIME__);
    }

    discover_launcher_projects();
}

static void route_input(int key) {
    int changed = 0;
    char action[256];

    archive_input(key);
    normalize_registry();
    recompute_nav_bounds();
    if (!read_project_map_control()) {
        sync_active_gui_index_from_display();
    } else if (g_map_control_nav_index > 0) {
        g_active_gui_index = g_map_control_nav_index;
    }
    log_alpha("Input received: %d", key);

    if (read_project_map_control()) {
        char key_buf[32];
        if (key == 27) {
            set_project_map_control(0);
            return;
        }
        snprintf(key_buf, sizeof(key_buf), "%d", key);
        append_project_history("KEY_PRESSED", key_buf);
        run_active_project_input_op();
        update_state(key);
        trigger_render();
        return;
    }

    if (key >= '0' && key <= '9') {
        int digit = key - '0';
        int candidate = (g_digit_accum * 10) + digit;
        if (candidate > 0 && candidate <= g_max_index) {
            g_digit_accum = candidate;
            g_active_gui_index = candidate;
            changed = 1;
        } else if (digit > 0 && digit <= g_max_index) {
            g_digit_accum = digit;
            g_active_gui_index = digit;
            changed = 1;
        } else {
            g_digit_accum = 0;
        }
    } else if (key == 1002) {
        g_digit_accum = 0;
        if (g_active_gui_index > 1) {
            g_active_gui_index--;
            changed = 1;
        }
    } else if (key == 1003) {
        g_digit_accum = 0;
        if (g_active_gui_index < g_max_index) {
            g_active_gui_index++;
            changed = 1;
        }
    } else if (key == 10 || key == 13) {
        sync_active_gui_index_from_display();
        if (g_digit_accum > 0 && g_digit_accum <= g_max_index) {
            g_active_gui_index = g_digit_accum;
        }
        log_alpha("Enter received: gui_index=%d max_index=%d", g_active_gui_index, g_max_index);
        g_digit_accum = 0;
        if (action_for_nav_index(g_active_gui_index, action, sizeof(action))) {
            route_command(action);
            return;
        }
        if (dispatch_menu_index(g_active_gui_index)) {
            return;
        }
        return;
    } else {
        g_digit_accum = 0;
        /* Any other key -- overwhelmingly, in practice, the printable
           characters cli_io typing sends. This process has zero
           interpretation of them; that entire state machine (focus_index
           vs. active_index, el->input_buffer, save_to_gui_state()) is
           wraith_parser_alpha.c's own, private, internal state. But
           GL-content-mode rendering depends on THIS process re-running
           write_semantic_projection_files() (which reads gui_state.txt's
           live per-keystroke value for whichever cli_io field is
           currently active -- see read_gui_state_value(),
           2fix-july6.txt bug 3) -- and until this line, "changed" was
           never set here, so update_state()/trigger_render() never ran
           for these keys. objects.pdl went stale the instant focus moved
           to a cli_io field and stayed stale through every character
           subsequently typed, even though current_frame.txt (written by
           the OTHER process) re-rendered correctly on every single key
           via its own unconditional "NAV MARKER: for ALL layouts"
           convention. Confirmed live: current_frame.txt showed
           "[^] 6. [hi hitest_]" (correct) while the objects.pdl generated
           at the same moment still showed nav_selector_glyph=">" and the
           static placeholder label (stale from focus time) -- this is
           why ASCII always reflected typed text/the >/^ glyph correctly
           while GL content mode never did. Matches the SAME "any key =>
           re-render, let whichever process actually understands it
           decide what to do" precedent the read_project_map_control()
           branch above already uses unconditionally, for a different
           mode. */
        changed = 1;
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
    long last_active_project_marker_pos = 0;
    char keyboard_hist_path[MAX_PATH];
    char project_hist_path[MAX_PATH];
    char action_queue_path[MAX_PATH];
    char last_active_project_dir[MAX_PATH] = "";

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
    reset_all_open_project_views_on_startup();
    recompute_nav_bounds();
    update_state(0);
    trigger_render();

    launch_rgb_pipeline();
    log_runtime_identity();

    log_alpha("Wraith-Alpha Manager starting in %s", g_project_root);

    sweep_audit_logs(); /* trim any oversized audit files left from before */

    {
        long janitor_tick = 0;
        /* ~60Hz loop; sweep the audit logs every ~600 ticks (~10s) so they
           stay trimmed without paying a file rewrite every frame. The cap
           helper size-fast-paths, so most sweeps are just a few stat()s. */
        const long janitor_interval = 600;
        while (!g_shutdown) {
            sync_active_gui_index_from_display();
            process_history_file(keyboard_hist_path, &last_keyboard_pos, "Keyboard");
            process_history_file(project_hist_path, &last_project_pos, "Project");
            process_history_file(action_queue_path, &last_action_pos, "Desktop Action");
            process_active_project_marker(&last_active_project_marker_pos, last_active_project_dir, sizeof(last_active_project_dir));
            if (++janitor_tick >= janitor_interval) {
                janitor_tick = 0;
                sweep_audit_logs();
            }
            usleep(16667);
        }
    }

    log_alpha("Wraith-Alpha Manager shutting down.");

    /* Mirrors pieces/chtpm/plugins/orchestrator.c's handle_sigint(): kill
       this process's own group first (fast path -- reaches every child
       that was fork()'d without its own setpgid(), which is every child
       this file spawns today), then the file-backed tracked-process sweep
       (catches anything that escaped the group), then the same kill_all.sh
       final sweep the orchestrator uses as its own last resort. Previously
       none of this existed here at all -- see kill_all_tracked_processes()'s
       own comment for why that mattered (every project manager this
       process ever launched was left running as an orphan on quit). */
    kill(0, SIGTERM);
    usleep(100000);
    kill_all_tracked_processes();
    run_final_kill_sweep();
    return 0;
}
