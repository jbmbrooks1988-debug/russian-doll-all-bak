#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE 4096
#define MAX_COLS 96
#define MAX_ROWS 30
#define MIN_COLS 34
#define MIN_ROWS 12
#define MAX_OBJECTS 16

typedef struct { int r, g, b; } RGB;
typedef struct { char ch; RGB fg, bg; int object_id; char style[24]; } Cell;
typedef struct {
    int id;
    char dom_id[96];
    char role[48];
    char label[256];
    char src[MAX_PATH_LEN];
    int x, y, w, h, z, focused, minimized;
    RGB fg, bg, border;
} ObjectRecord;

static Cell g_cells[MAX_ROWS][MAX_COLS];
static ObjectRecord g_objects[MAX_OBJECTS];
static int g_object_count = 0;
static char g_project_root[MAX_PATH_LEN] = ".";
static int g_mouse_x = 0;
static int g_mouse_y = 0;
static char g_taskbar_label[256] = "| [Wraith Term*] [Game Map] |";
static char g_taskbar_clock[64] = "";
static char g_current_key[64] = "None";
static int g_frame_id = 0;
static int g_frame_cols = MIN_COLS;
static int g_frame_rows = MIN_ROWS;

static const RGB RGB_WHITE = {232, 241, 242};
static const RGB RGB_BLACK = {16, 24, 32};
static const RGB RGB_BORDER = {126, 223, 242};
static const RGB RGB_YELLOW = {255, 209, 102};
static const RGB RGB_DIM = {138, 161, 177};

static void format_key_label(int key, char *out, size_t out_sz) {
    if (out_sz == 0) return;
    if (key >= 32 && key <= 126) snprintf(out, out_sz, "%c", key);
    else if (key == 1000) snprintf(out, out_sz, "LEFT");
    else if (key == 1001) snprintf(out, out_sz, "RIGHT");
    else if (key == 1002) snprintf(out, out_sz, "UP");
    else if (key == 1003) snprintf(out, out_sz, "DOWN");
    else if (key == 10 || key == 13) snprintf(out, out_sz, "ENTER");
    else snprintf(out, out_sz, "%d", key);
}

static char *trim_ws(char *s) {
    char *end = NULL;
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static int root_has_anchors(const char *root) {
    char pieces_path[MAX_PATH_LEN], projects_path[MAX_PATH_LEN];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

static void resolve_root(void) {
    FILE *kvp = NULL;
    if (!getcwd(g_project_root, sizeof(g_project_root))) strcpy(g_project_root, ".");
    if (root_has_anchors(g_project_root)) return;
    kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *candidate = trim_ws(line + 13);
                if (root_has_anchors(candidate)) {
                    strncpy(g_project_root, candidate, sizeof(g_project_root) - 1);
                    g_project_root[sizeof(g_project_root) - 1] = '\0';
                }
                break;
            }
        }
        fclose(kvp);
    }
}

static void read_session_kv(const char *rel_path, const char *key, char *out, size_t out_sz) {
    char path[MAX_PATH_LEN], line[MAX_LINE];
    out[0] = '\0';
    snprintf(path, sizeof(path), "%s/%s", g_project_root, rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(trim_ws(line), key) == 0) {
            strncpy(out, trim_ws(eq + 1), out_sz - 1);
            out[out_sz - 1] = '\0';
            break;
        }
    }
    fclose(f);
}

static void load_desktop_state(void) {
    read_session_kv("projects/wraith-pm/session/state.txt", "taskbar_windows_label", g_taskbar_label, sizeof(g_taskbar_label));
    read_session_kv("projects/wraith-pm/session/state.txt", "taskbar_clock", g_taskbar_clock, sizeof(g_taskbar_clock));
    read_session_kv("projects/wraith-pm/session/state.txt", "current_key", g_current_key, sizeof(g_current_key));
    if (!g_current_key[0]) strcpy(g_current_key, "None");
}

static void load_frame_counter(void) {
    char path[MAX_PATH_LEN], buf[64];
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/frame_counter.txt", g_project_root);
    read_session_kv("projects/wraith-pm/session/frame_counter.txt", "frame_id", buf, sizeof(buf));
    if (!buf[0]) {
        FILE *f = fopen(path, "r");
        if (f) {
            if (fgets(buf, sizeof(buf), f)) g_frame_id = atoi(buf);
            fclose(f);
        }
    } else {
        g_frame_id = atoi(buf);
    }
    if (g_frame_id < 0) g_frame_id = 0;
    g_frame_id++;
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "frame_id=%06d\n", g_frame_id);
        fclose(f);
    }
}

static void load_mouse_state(void) {
    char val[64];
    read_session_kv("projects/wraith-pm/pieces/mousehand/state.txt", "pos_x", val, sizeof(val));
    if (val[0]) g_mouse_x = atoi(val);
    read_session_kv("projects/wraith-pm/pieces/mousehand/state.txt", "pos_y", val, sizeof(val));
    if (val[0]) g_mouse_y = atoi(val);
}

static ObjectRecord *add_object(const char *dom_id, const char *role, const char *label,
                                int x, int y, int w, int h, int z, int focused,
                                const char *src, RGB fg, RGB bg, RGB border) {
    ObjectRecord *obj = NULL;
    if (g_object_count >= MAX_OBJECTS) return NULL;
    obj = &g_objects[g_object_count];
    memset(obj, 0, sizeof(*obj));
    obj->id = g_object_count + 1;
    strncpy(obj->dom_id, dom_id, sizeof(obj->dom_id) - 1);
    strncpy(obj->role, role, sizeof(obj->role) - 1);
    strncpy(obj->label, label, sizeof(obj->label) - 1);
    if (src) strncpy(obj->src, src, sizeof(obj->src) - 1);
    obj->x = x; obj->y = y; obj->w = w; obj->h = h; obj->z = z; obj->focused = focused;
    obj->fg = fg; obj->bg = bg; obj->border = border;
    g_object_count++;
    return obj;
}

static void parse_window_token(ObjectRecord *obj, char *token) {
    char *eq = strchr(token, '=');
    if (!eq) return;
    *eq = '\0';
    char *key = trim_ws(token);
    char *val = trim_ws(eq + 1);
    if (strcmp(key, "id") == 0) strncpy(obj->dom_id, val, sizeof(obj->dom_id) - 1);
    else if (strcmp(key, "title") == 0) strncpy(obj->label, val, sizeof(obj->label) - 1);
    else if (strcmp(key, "x") == 0) obj->x = atoi(val);
    else if (strcmp(key, "y") == 0) obj->y = atoi(val);
    else if (strcmp(key, "w") == 0) obj->w = atoi(val);
    else if (strcmp(key, "h") == 0) obj->h = atoi(val);
    else if (strcmp(key, "z") == 0) obj->z = atoi(val);
    else if (strcmp(key, "focused") == 0) obj->focused = (strcmp(val, "true") == 0);
    else if (strcmp(key, "minimized") == 0) obj->minimized = (strcmp(val, "true") == 0);
    else if (strcmp(key, "role") == 0) strncpy(obj->role, val, sizeof(obj->role) - 1);
    else if (strcmp(key, "src") == 0) strncpy(obj->src, val, sizeof(obj->src) - 1);
}

static void load_windows_registry(void) {
    char path[MAX_PATH_LEN], line[MAX_LINE];
    int focused_seen = 0;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/windows_state.pdl", g_project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f) && g_object_count < MAX_OBJECTS) {
        char *save = NULL;
        char *part = NULL;
        ObjectRecord obj;
        if (strncmp(line, "WINDOW", 6) != 0) continue;

        memset(&obj, 0, sizeof(obj));
        obj.fg = RGB_WHITE;
        obj.bg = RGB_BLACK;
        obj.border = RGB_BORDER;
        obj.w = 10;
        obj.h = 5;

        part = strtok_r(line, "|", &save);
        while ((part = strtok_r(NULL, "|", &save)) != NULL) {
            parse_window_token(&obj, trim_ws(part));
        }

        if (!obj.dom_id[0] || obj.minimized) continue;
        if (!obj.role[0]) strcpy(obj.role, "panel");
        if (strcmp(obj.dom_id, "game_map_window") == 0) {
            obj.bg = (RGB){24, 35, 46};
            obj.border = obj.focused ? RGB_YELLOW : RGB_DIM;
        } else {
            obj.bg = RGB_BLACK;
            obj.border = obj.focused ? RGB_BORDER : RGB_DIM;
        }

        g_objects[g_object_count] = obj;
        g_objects[g_object_count].id = g_object_count + 1;
        if (obj.focused) focused_seen = 1;
        g_object_count++;
    }
    fclose(f);

    if (!focused_seen) {
        for (int i = 0; i < g_object_count; i++) {
            if (strcmp(g_objects[i].role, "panel") == 0) {
                g_objects[i].focused = 1;
                break;
            }
        }
    }
}

static int object_right_edge(const ObjectRecord *obj) {
    return obj->x + obj->w;
}

static int object_bottom_edge(const ObjectRecord *obj) {
    return obj->y + obj->h;
}

static void compute_frame_bounds(void) {
    int max_right = MIN_COLS;
    int max_bottom = 0;
    int taskbar_y = 0;
    int clock_len = (int)strlen(g_taskbar_clock[0] ? g_taskbar_clock : "--:-- --- -- ----");
    int label_len = (int)strlen(g_taskbar_label);
    int key_len = (int)strlen(g_current_key) + 6;
    int taskbar_right = 0;

    for (int i = 0; i < g_object_count; i++) {
        if (strcmp(g_objects[i].role, "panel") != 0) continue;
        if (object_right_edge(&g_objects[i]) > max_right) max_right = object_right_edge(&g_objects[i]);
        if (object_bottom_edge(&g_objects[i]) > max_bottom) max_bottom = object_bottom_edge(&g_objects[i]);
    }

    taskbar_y = max_bottom + 1;
    g_frame_rows = taskbar_y + 2;
    if (g_frame_rows < MIN_ROWS) g_frame_rows = MIN_ROWS;
    if (g_frame_rows > MAX_ROWS) g_frame_rows = MAX_ROWS;

    taskbar_right = 10 + label_len + 1 + key_len + 1 + clock_len;
    if (taskbar_right > max_right) max_right = taskbar_right;

    g_frame_cols = max_right + 2;
    if (g_frame_cols < MIN_COLS) g_frame_cols = MIN_COLS;
    if (g_frame_cols > MAX_COLS) g_frame_cols = MAX_COLS;
}

static void add_builtin_objects(void) {
    int taskbar_y = g_frame_rows - 2;
    int clock_x = g_frame_cols - 19;
    int label_x = 10;
    int label_w = (int)strlen(g_taskbar_label);
    int key_x = 0;
    int key_w = 0;
    int clock_visible = 1;
    int key_visible = 1;
    char key_label[96];
    snprintf(key_label, sizeof(key_label), "K:%s", g_current_key[0] ? g_current_key : "None");
    key_w = (int)strlen(key_label);
    key_x = label_x + label_w + 1;
    if (clock_x < 0) clock_x = 0;
    if (clock_x <= key_x + key_w) clock_visible = 0;
    if (!clock_visible && g_frame_cols <= key_x + key_w) key_visible = 0;
    add_object("wraith_root", "window", "Wraith Desktop", 0, 0, g_frame_cols, g_frame_rows, 1, 0, "", RGB_WHITE, RGB_BLACK, RGB_BORDER);
    add_object("desktop_banner", "banner", "WRAITH", 0, 0, g_frame_cols, 1, 2, 0, "", RGB_WHITE, (RGB){20, 20, 60}, (RGB){20, 20, 60});
    add_object("taskbar", "banner", "taskbar", 0, taskbar_y, g_frame_cols, 2, 5, 0, "", RGB_WHITE, (RGB){11, 15, 20}, RGB_DIM);
    add_object("taskbar_start", "text", "[Start]", 1, taskbar_y + 1, 8, 1, 6, 0, "", RGB_YELLOW, (RGB){20, 20, 60}, RGB_YELLOW);
    add_object("taskbar_windows", "text", g_taskbar_label, label_x, taskbar_y + 1, label_w, 1, 7, 0, "", RGB_WHITE, (RGB){20, 20, 60}, RGB_WHITE);
    if (key_visible) {
        add_object("taskbar_key", "text", key_label, key_x, taskbar_y + 1, key_w, 1, 8, 0, "", RGB_YELLOW, (RGB){20, 20, 60}, RGB_YELLOW);
    }
    if (clock_visible) {
        add_object("taskbar_clock", "text", g_taskbar_clock[0] ? g_taskbar_clock : "--:-- --- -- ----", clock_x, taskbar_y + 1, 18, 1, 9, 0, "", RGB_YELLOW, (RGB){20, 20, 60}, RGB_YELLOW);
    }
}

static int compare_z(const void *a, const void *b) {
    const ObjectRecord *oa = (const ObjectRecord *)a;
    const ObjectRecord *ob = (const ObjectRecord *)b;
    return oa->z - ob->z;
}

static void init_cells(void) {
    for (int y = 0; y < g_frame_rows; y++) {
        for (int x = 0; x < g_frame_cols; x++) {
            g_cells[y][x].ch = ' ';
            g_cells[y][x].fg = RGB_WHITE;
            g_cells[y][x].bg = RGB_BLACK;
            g_cells[y][x].object_id = 0;
            strcpy(g_cells[y][x].style, "normal");
        }
    }
}

static void set_cell(int x, int y, char ch, RGB fg, RGB bg, int object_id, const char *style) {
    if (x < 0 || y < 0 || x >= g_frame_cols || y >= g_frame_rows) return;
    g_cells[y][x].ch = ch;
    g_cells[y][x].fg = fg;
    g_cells[y][x].bg = bg;
    g_cells[y][x].object_id = object_id;
    if (style) {
        strncpy(g_cells[y][x].style, style, sizeof(g_cells[y][x].style) - 1);
        g_cells[y][x].style[sizeof(g_cells[y][x].style) - 1] = '\0';
    }
}

static void fill_rect(int x, int y, int w, int h, char ch, RGB fg, RGB bg, int object_id, const char *style) {
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) set_cell(xx, yy, ch, fg, bg, object_id, style);
    }
}

static void draw_text(int x, int y, const char *text, RGB fg, RGB bg, int object_id, const char *style) {
    if (!text) return;
    for (int i = 0; text[i] && x + i < g_frame_cols; i++) set_cell(x + i, y, text[i], fg, bg, object_id, style);
}

static void draw_terrain(void) {
    for (int y = 0; y < g_frame_rows; y++) {
        for (int x = 0; x < g_frame_cols; x++) {
            if ((x + y) % 10 == 0) set_cell(x, y, '.', (RGB){40, 40, 80}, RGB_BLACK, 1, "bg");
        }
    }
}

static void draw_builtin_object(const ObjectRecord *obj) {
    if (strcmp(obj->dom_id, "desktop_banner") == 0) {
        fill_rect(obj->x, obj->y, obj->w, obj->h, ' ', obj->fg, obj->bg, obj->id, "banner");
        draw_text(1, 0, obj->label, obj->fg, obj->bg, obj->id, "banner");
    } else if (strcmp(obj->dom_id, "taskbar") == 0) {
        fill_rect(obj->x, obj->y, obj->w, obj->h, ' ', obj->fg, obj->bg, obj->id, "taskbar");
    } else if (strcmp(obj->dom_id, "taskbar_start") == 0 || strcmp(obj->dom_id, "taskbar_windows") == 0 || strcmp(obj->dom_id, "taskbar_key") == 0 || strcmp(obj->dom_id, "taskbar_clock") == 0) {
        draw_text(obj->x, obj->y, obj->label, obj->fg, obj->bg, obj->id, "taskbar_text");
    }
}

static void draw_window(const ObjectRecord *obj) {
    RGB chrome = obj->focused ? obj->border : RGB_DIM;
    fill_rect(obj->x, obj->y, obj->w, obj->h, ' ', obj->fg, obj->bg, obj->id, "window");
    for (int x = obj->x; x < obj->x + obj->w; x++) {
        set_cell(x, obj->y, (x == obj->x || x == obj->x + obj->w - 1) ? '+' : '-', chrome, obj->bg, obj->id, "titlebar");
        set_cell(x, obj->y + obj->h - 1, (x == obj->x || x == obj->x + obj->w - 1) ? '+' : '-', chrome, obj->bg, obj->id, "border");
    }
    for (int y = obj->y + 1; y < obj->y + obj->h - 1; y++) {
        set_cell(obj->x, y, '|', chrome, obj->bg, obj->id, "border");
        set_cell(obj->x + obj->w - 1, y, '|', chrome, obj->bg, obj->id, "border");
    }
    {
        char title[256];
        snprintf(title, sizeof(title), " %s ", obj->label[0] ? obj->label : obj->dom_id);
        draw_text(obj->x + 2, obj->y, title, chrome, obj->bg, obj->id, "title");
    }
    draw_text(obj->x + obj->w - 10, obj->y, "[o][-][x]", chrome, obj->bg, obj->id, "controls");
}

static void merge_view(const ObjectRecord *obj) {
    char path[MAX_PATH_LEN], line[MAX_LINE];
    int ly = 0;
    FILE *f = NULL;
    if (!obj->src[0]) return;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/%s", g_project_root, obj->src);
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f) && ly < obj->h - 2) {
        line[strcspn(line, "\r\n")] = '\0';
        for (int lx = 0; line[lx] && lx < obj->w - 2; lx++) {
            set_cell(obj->x + 1 + lx, obj->y + 1 + ly, line[lx], obj->fg, obj->bg, obj->id, "content");
        }
        ly++;
    }
    fclose(f);
}

static void draw_mousehand(void) {
    if (g_mouse_x >= 0 && g_mouse_x < g_frame_cols && g_mouse_y >= 0 && g_mouse_y < g_frame_rows) {
        set_cell(g_mouse_x, g_mouse_y, '+', RGB_YELLOW, g_cells[g_mouse_y][g_mouse_x].bg, 0, "mousehand");
    }
}

static int find_focused_object_index(void) {
    for (int i = 0; i < g_object_count; i++) if (g_objects[i].focused) return i;
    return -1;
}

static void write_frame_text_files(void) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    const char *targets[] = {
        "projects/wraith-pm/manager/view.txt",
        "projects/wraith-pm/session/current_frame.txt",
        "projects/wraith-pm/session/current_frame.ansi.txt"
    };
    for (int t = 0; t < 3; t++) {
        snprintf(path, sizeof(path), "%s/%s", g_project_root, targets[t]);
        f = fopen(path, "w");
        if (!f) continue;
        for (int y = 0; y < g_frame_rows; y++) {
            for (int x = 0; x < g_frame_cols; x++) fputc(g_cells[y][x].ch, f);
            fputc('\n', f);
        }
        fclose(f);
    }
}

static void write_objects_file(void) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.objects.pdl", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", g_frame_id);
    for (int i = 0; i < g_object_count; i++) {
        fprintf(f, "OBJECT | %04d | tag=%s id=%s role=%s x=%d y=%d w=%d h=%d z=%d focused=%s label=%s src=%s\n",
                g_objects[i].id, g_objects[i].role, g_objects[i].dom_id, g_objects[i].role,
                g_objects[i].x, g_objects[i].y, g_objects[i].w, g_objects[i].h, g_objects[i].z,
                g_objects[i].focused ? "true" : "false", g_objects[i].label, g_objects[i].src);
    }
    fclose(f);
}

static void write_desktop_state_file(void) {
    char path[MAX_PATH_LEN];
    int focused_idx = find_focused_object_index();
    FILE *f = NULL;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.desktop_state.pdl", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", g_frame_id);
    fprintf(f, "DESKTOP | project_id | wraith-pm\n");
    fprintf(f, "DESKTOP | title | Wraith PM\n");
    fprintf(f, "DESKTOP | cols | %d\n", g_frame_cols);
    fprintf(f, "DESKTOP | rows | %d\n", g_frame_rows);
    fprintf(f, "DESKTOP | object_count | %d\n", g_object_count);
    fprintf(f, "DESKTOP | focused_object_id | %d\n", focused_idx >= 0 ? g_objects[focused_idx].id : 0);
    for (int i = 0; i < g_object_count; i++) {
        fprintf(f, "WINDOW | id=%s | title=%s | x=%d | y=%d | w=%d | h=%d | z=%d | focused=%s | role=%s\n",
                g_objects[i].dom_id, g_objects[i].label, g_objects[i].x, g_objects[i].y,
                g_objects[i].w, g_objects[i].h, g_objects[i].z,
                g_objects[i].focused ? "true" : "false", g_objects[i].role);
    }
    fclose(f);
}

static void write_focus_state_file(void) {
    char path[MAX_PATH_LEN];
    int focused_idx = find_focused_object_index();
    FILE *f = NULL;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.focus_state.pdl", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", g_frame_id);
    if (focused_idx >= 0) {
        fprintf(f, "FOCUS | active_object_id | %d\n", g_objects[focused_idx].id);
        fprintf(f, "FOCUS | active_object_dom_id | %s\n", g_objects[focused_idx].dom_id);
        fprintf(f, "FOCUS | active_object_label | %s\n", g_objects[focused_idx].label);
        fprintf(f, "FOCUS | active_object_role | %s\n", g_objects[focused_idx].role);
    }
    fclose(f);
}

static void write_window_stack_file(void) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    int order = 0;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.window_stack.pdl", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", g_frame_id);
    for (int i = g_object_count - 1; i >= 0; i--) {
        fprintf(f, "STACK | %02d | object=%d title=%s role=%s z=%d focused=%s\n",
                order++, g_objects[i].id, g_objects[i].label, g_objects[i].role,
                g_objects[i].z, g_objects[i].focused ? "true" : "false");
    }
    fclose(f);
}

static void write_mouse_state_file(void) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.mouse_state.pdl", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", g_frame_id);
    fprintf(f, "MOUSE | x | %d\n", g_mouse_x);
    fprintf(f, "MOUSE | y | %d\n", g_mouse_y);
    fprintf(f, "DRAG | active | false\n");
    fclose(f);
}

static void write_meta_file(void) {
    char path[MAX_PATH_LEN];
    int focused_idx = find_focused_object_index();
    FILE *f = NULL;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.meta.pdl", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", g_frame_id);
    fprintf(f, "FRAME | project_id | wraith-pm\n");
    fprintf(f, "FRAME | cols | %d\n", g_frame_cols);
    fprintf(f, "FRAME | rows | %d\n", g_frame_rows);
    fprintf(f, "FRAME | object_count | %d\n", g_object_count);
    if (focused_idx >= 0) fprintf(f, "FRAME | focused_object_dom_id | %s\n", g_objects[focused_idx].dom_id);
    fclose(f);
}

static void write_cells_file(void) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.cells.pdl", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", g_frame_id);
    fprintf(f, "FRAME | cols | %d\n", g_frame_cols);
    fprintf(f, "FRAME | rows | %d\n", g_frame_rows);
    fclose(f);
}

static void write_hitmap_file(void) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.hitmap.pdl", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", g_frame_id);
    for (int y = 0; y < g_frame_rows; y++) {
        for (int x = 0; x < g_frame_cols; x++) {
            if (g_cells[y][x].object_id <= 0) continue;
            {
                ObjectRecord *obj = &g_objects[g_cells[y][x].object_id - 1];
                const char *role = g_cells[y][x].style;
                fprintf(f, "HIT | x=%d,y=%d | id=%s | role=%s | object=%d\n",
                        x, y, obj->dom_id, role, obj->id);
            }
        }
    }
    fclose(f);
}

static void write_audit_file(void) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.audit.txt", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "WRAITH FRAME AUDIT\n");
    fprintf(f, "frame_id=%06d\n", g_frame_id);
    fprintf(f, "project_id=wraith-pm\n");
    fprintf(f, "source_layout=projects/wraith-pm/layouts/desktop.chtpm\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)time(NULL));
    fprintf(f, "cols=%d\n", g_frame_cols);
    fprintf(f, "rows=%d\n", g_frame_rows);
    fprintf(f, "object_count=%d\n", g_object_count);
    fprintf(f, "outputs=current_frame.txt,current_frame.ansi.txt,current_frame.cells.pdl,current_frame.meta.pdl,current_frame.objects.pdl,current_frame.hitmap.pdl,current_frame.desktop_state.pdl,current_frame.window_stack.pdl,current_frame.focus_state.pdl,current_frame.mouse_state.pdl\n");
    fprintf(f, "warnings=none\n");
    fclose(f);
}

static void write_outputs(void) {
    write_frame_text_files();
    write_cells_file();
    write_meta_file();
    write_objects_file();
    write_hitmap_file();
    write_desktop_state_file();
    write_window_stack_file();
    write_focus_state_file();
    write_mouse_state_file();
    write_audit_file();
}

int main(void) {
    resolve_root();
    load_frame_counter();
    load_mouse_state();
    load_desktop_state();
    load_windows_registry();
    compute_frame_bounds();
    init_cells();
    add_builtin_objects();
    qsort(g_objects, g_object_count, sizeof(ObjectRecord), compare_z);
    draw_terrain();
    for (int i = 0; i < g_object_count; i++) {
        if (strcmp(g_objects[i].role, "panel") == 0) {
            draw_window(&g_objects[i]);
            merge_view(&g_objects[i]);
        } else {
            draw_builtin_object(&g_objects[i]);
        }
    }
    draw_mousehand();
    write_outputs();
    return 0;
}
