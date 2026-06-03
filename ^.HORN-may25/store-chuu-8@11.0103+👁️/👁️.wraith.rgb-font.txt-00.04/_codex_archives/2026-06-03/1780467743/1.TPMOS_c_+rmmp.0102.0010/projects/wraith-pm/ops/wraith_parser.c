#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE 4096
#define MAX_BUFFER 1048576
#define MAX_VARS 256
#define MAX_VAR_NAME 96
#define MAX_VAR_VALUE 4096
#define MAX_COLS 120
#define MAX_ROWS 40
#define MAX_OBJECTS 512
#define MAX_STACK 64

typedef struct {
    int r;
    int g;
    int b;
} RGB;

typedef struct {
    char ch;
    RGB fg;
    RGB bg;
    int object_id;
    char style[24];
} Cell;

typedef struct {
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
} Var;

typedef struct {
    int id;
    char tag[32];
    char dom_id[96];
    char role[48];
    char label[256];
    char action[256];
    char src[MAX_PATH_LEN];
    int x;
    int y;
    int w;
    int h;
    RGB fg;
    RGB bg;
    RGB border;
} ObjectRecord;

typedef struct {
    int object_id;
    int x;
    int y;
    int w;
    int h;
    int next_y;
} StackFrame;

static Cell g_cells[MAX_ROWS][MAX_COLS];
static Var g_vars[MAX_VARS];
static ObjectRecord g_objects[MAX_OBJECTS];
static int g_var_count = 0;
static int g_object_count = 0;
static int g_cols = 96;
static int g_rows = 30;
static int g_cell_w = 10;
static int g_cell_h = 18;
static int g_window_w = 960;
static int g_window_h = 540;
static char g_project_root[MAX_PATH_LEN] = ".";
static char g_project_id[96] = "wraith-pm";
static char g_layout_path[MAX_PATH_LEN] = "projects/wraith-pm/layouts/desktop.chtmgl";
static char g_warnings[8192] = "";

static const RGB RGB_WHITE = {232, 241, 242};
static const RGB RGB_BLACK = {16, 24, 32};
static const RGB RGB_BORDER = {126, 223, 242};

static char *trim_ws(char *s) {
    char *end = NULL;
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static int starts_with(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}

static void append_warning(const char *msg) {
    if (!msg) return;
    if (strlen(g_warnings) + strlen(msg) + 2 >= sizeof(g_warnings)) return;
    strcat(g_warnings, msg);
    strcat(g_warnings, "\n");
}

static int root_has_anchors(const char *root) {
    char pieces_path[MAX_PATH_LEN];
    char projects_path[MAX_PATH_LEN];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

static void resolve_root(void) {
    FILE *kvp = NULL;
    if (!getcwd(g_project_root, sizeof(g_project_root))) {
        strncpy(g_project_root, ".", sizeof(g_project_root) - 1);
        g_project_root[sizeof(g_project_root) - 1] = '\0';
    }

    if (root_has_anchors(g_project_root)) return;

    kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (starts_with(line, "project_root=")) {
                char *value = trim_ws(line + 13);
                if (root_has_anchors(value)) {
                    strncpy(g_project_root, value, sizeof(g_project_root) - 1);
                    g_project_root[sizeof(g_project_root) - 1] = '\0';
                }
                break;
            }
        }
        fclose(kvp);
    }
}

static void build_path(char *dst, size_t size, const char *rel) {
    if (!dst || size == 0) return;
    if (!rel || rel[0] == '\0') {
        dst[0] = '\0';
        return;
    }
    if (rel[0] == '/') {
        strncpy(dst, rel, size - 1);
        dst[size - 1] = '\0';
        return;
    }
    snprintf(dst, size, "%s/%s", g_project_root, rel);
}

static int extract_attr(const char *line, const char *name, char *dst, size_t dst_size) {
    char needle[96];
    const char *start = NULL;
    const char *end = NULL;
    size_t len = 0;

    if (!line || !name || !dst || dst_size == 0) return 0;
    dst[0] = '\0';
    snprintf(needle, sizeof(needle), "%s=\"", name);
    start = strstr(line, needle);
    if (!start) return 0;

    start += strlen(needle);
    end = strchr(start, '"');
    if (!end) return 0;

    len = (size_t)(end - start);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
    return 1;
}

static int has_attr(const char *line, const char *name) {
    char needle[96];
    if (!line || !name) return 0;
    snprintf(needle, sizeof(needle), "%s=\"", name);
    return strstr(line, needle) != NULL;
}

static RGB parse_rgb(const char *value, RGB fallback) {
    RGB out = fallback;
    int r = 0;
    int g = 0;
    int b = 0;

    if (!value || value[0] == '\0') return out;
    if (value[0] == '#') {
        if (strlen(value) >= 7 && sscanf(value + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            out.r = r;
            out.g = g;
            out.b = b;
        }
    } else if (sscanf(value, "%d,%d,%d", &r, &g, &b) == 3) {
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        out.r = r;
        out.g = g;
        out.b = b;
    }
    return out;
}

static void rgb_hex(RGB rgb, char *dst, size_t size) {
    snprintf(dst, size, "#%02X%02X%02X", rgb.r, rgb.g, rgb.b);
}

static void set_var(const char *name, const char *value) {
    int i;
    if (!name || !value || name[0] == '\0') return;
    for (i = 0; i < g_var_count; i++) {
        if (strcmp(g_vars[i].name, name) == 0) {
            strncpy(g_vars[i].value, value, sizeof(g_vars[i].value) - 1);
            g_vars[i].value[sizeof(g_vars[i].value) - 1] = '\0';
            return;
        }
    }
    if (g_var_count >= MAX_VARS) return;
    strncpy(g_vars[g_var_count].name, name, sizeof(g_vars[g_var_count].name) - 1);
    strncpy(g_vars[g_var_count].value, value, sizeof(g_vars[g_var_count].value) - 1);
    g_vars[g_var_count].name[sizeof(g_vars[g_var_count].name) - 1] = '\0';
    g_vars[g_var_count].value[sizeof(g_vars[g_var_count].value) - 1] = '\0';
    g_var_count++;
}

static const char *get_var(const char *name) {
    int i;
    if (!name) return "";
    for (i = 0; i < g_var_count; i++) {
        if (strcmp(g_vars[i].name, name) == 0) return g_vars[i].value;
    }
    return "";
}

static void load_state_file(const char *rel_path) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    char line[MAX_LINE];

    build_path(path, sizeof(path), rel_path);
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        char *key = NULL;
        char *value = NULL;
        if (!eq) continue;
        *eq = '\0';
        key = trim_ws(line);
        value = trim_ws(eq + 1);
        value[strcspn(value, "\n\r")] = '\0';
        set_var(key, value);
    }
    fclose(f);
}

static char *read_file(const char *rel_or_abs) {
    char path[MAX_PATH_LEN];
    FILE *f = NULL;
    long size = 0;
    char *buf = NULL;

    build_path(path, sizeof(path), rel_or_abs);
    f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > MAX_BUFFER - 1) {
        fclose(f);
        return NULL;
    }

    buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void substitute_vars(const char *src, char *dst, size_t dst_size) {
    const char *p = src;
    char *out = dst;
    size_t remaining = dst_size;

    if (!src || !dst || dst_size == 0) return;
    dst[0] = '\0';

    while (*p && remaining > 1) {
        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p + 2, '}');
            if (end) {
                char key[MAX_VAR_NAME];
                size_t key_len = (size_t)(end - (p + 2));
                const char *value = NULL;
                if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
                memcpy(key, p + 2, key_len);
                key[key_len] = '\0';
                value = get_var(key);
                while (*value && remaining > 1) {
                    *out++ = *value++;
                    remaining--;
                }
                p = end + 1;
                continue;
            }
        }
        *out++ = *p++;
        remaining--;
    }
    *out = '\0';
}

static void init_cells(RGB fg, RGB bg) {
    int y;
    int x;
    for (y = 0; y < MAX_ROWS; y++) {
        for (x = 0; x < MAX_COLS; x++) {
            g_cells[y][x].ch = ' ';
            g_cells[y][x].fg = fg;
            g_cells[y][x].bg = bg;
            g_cells[y][x].object_id = 0;
            strcpy(g_cells[y][x].style, "normal");
        }
    }
}

static void set_cell(int x, int y, char ch, RGB fg, RGB bg, int object_id, const char *style) {
    if (x < 0 || y < 0 || x >= g_cols || y >= g_rows) return;
    g_cells[y][x].ch = ch;
    g_cells[y][x].fg = fg;
    g_cells[y][x].bg = bg;
    g_cells[y][x].object_id = object_id;
    if (style && style[0]) {
        strncpy(g_cells[y][x].style, style, sizeof(g_cells[y][x].style) - 1);
        g_cells[y][x].style[sizeof(g_cells[y][x].style) - 1] = '\0';
    }
}

static void fill_rect(int x, int y, int w, int h, char ch, RGB fg, RGB bg, int object_id) {
    int yy;
    int xx;
    for (yy = y; yy < y + h; yy++) {
        for (xx = x; xx < x + w; xx++) {
            set_cell(xx, yy, ch, fg, bg, object_id, "normal");
        }
    }
}

static void draw_text(int x, int y, const char *text, RGB fg, RGB bg, int object_id, const char *style) {
    int i;
    if (!text) return;
    for (i = 0; text[i] && x + i < g_cols; i++) {
        set_cell(x + i, y, text[i], fg, bg, object_id, style);
    }
}

static void draw_text_clipped(int x, int y, const char *text, int max_w, RGB fg, RGB bg, int object_id, const char *style) {
    int i;
    if (!text || max_w <= 0) return;
    for (i = 0; text[i] && i < max_w && x + i < g_cols; i++) {
        set_cell(x + i, y, text[i], fg, bg, object_id, style);
    }
}

static void draw_border(int x, int y, int w, int h, RGB fg, RGB bg, int object_id) {
    int i;
    if (w <= 1 || h <= 1) return;
    set_cell(x, y, '+', fg, bg, object_id, "border");
    set_cell(x + w - 1, y, '+', fg, bg, object_id, "border");
    set_cell(x, y + h - 1, '+', fg, bg, object_id, "border");
    set_cell(x + w - 1, y + h - 1, '+', fg, bg, object_id, "border");
    for (i = 1; i < w - 1; i++) {
        set_cell(x + i, y, '-', fg, bg, object_id, "border");
        set_cell(x + i, y + h - 1, '-', fg, bg, object_id, "border");
    }
    for (i = 1; i < h - 1; i++) {
        set_cell(x, y + i, '|', fg, bg, object_id, "border");
        set_cell(x + w - 1, y + i, '|', fg, bg, object_id, "border");
    }
}

static int px_to_cell_x(int px) {
    if (px <= 0) return 0;
    return px / g_cell_w;
}

static int px_to_cell_y(int px) {
    if (px <= 0) return 0;
    return px / g_cell_h;
}

static int px_len_to_cells_x(int px) {
    int v;
    if (px <= 0) return 1;
    v = (px + g_cell_w - 1) / g_cell_w;
    return v < 1 ? 1 : v;
}

static int px_len_to_cells_y(int px) {
    int v;
    if (px <= 0) return 1;
    v = (px + g_cell_h - 1) / g_cell_h;
    return v < 1 ? 1 : v;
}

static int attr_int(const char *line, const char *name, int fallback) {
    char value[64];
    if (extract_attr(line, name, value, sizeof(value))) return atoi(value);
    return fallback;
}

static RGB attr_rgb(const char *line, const char *name, RGB fallback) {
    char value[64];
    if (extract_attr(line, name, value, sizeof(value))) return parse_rgb(value, fallback);
    return fallback;
}

static void attr_string(const char *line, const char *name, char *dst, size_t size, const char *fallback) {
    if (!extract_attr(line, name, dst, size) && fallback) {
        strncpy(dst, fallback, size - 1);
        dst[size - 1] = '\0';
    }
}

static ObjectRecord *add_object(const char *tag, const char *line, const char *role, int x, int y, int w, int h, RGB fg, RGB bg, RGB border) {
    ObjectRecord *obj = NULL;
    if (g_object_count >= MAX_OBJECTS) return NULL;
    obj = &g_objects[g_object_count++];
    memset(obj, 0, sizeof(*obj));
    obj->id = g_object_count;
    strncpy(obj->tag, tag ? tag : "unknown", sizeof(obj->tag) - 1);
    strncpy(obj->role, role ? role : tag ? tag : "unknown", sizeof(obj->role) - 1);
    attr_string(line, "id", obj->dom_id, sizeof(obj->dom_id), "");
    attr_string(line, "label", obj->label, sizeof(obj->label), "");
    attr_string(line, "onClick", obj->action, sizeof(obj->action), "");
    attr_string(line, "src", obj->src, sizeof(obj->src), "");
    obj->x = x;
    obj->y = y;
    obj->w = w;
    obj->h = h;
    obj->fg = fg;
    obj->bg = bg;
    obj->border = border;
    return obj;
}

static void stack_push(StackFrame stack[], int *top, int object_id, int x, int y, int w, int h) {
    if (*top >= MAX_STACK - 1) return;
    (*top)++;
    stack[*top].object_id = object_id;
    stack[*top].x = x;
    stack[*top].y = y;
    stack[*top].w = w;
    stack[*top].h = h;
    stack[*top].next_y = 1;
}

static void stack_pop(StackFrame stack[], int *top) {
    (void)stack;
    if (*top >= 0) (*top)--;
}

static StackFrame current_parent(StackFrame stack[], int top) {
    StackFrame root;
    if (top >= 0) return stack[top];
    root.object_id = 0;
    root.x = 0;
    root.y = 0;
    root.w = g_cols;
    root.h = g_rows;
    root.next_y = 1;
    return root;
}

static void compute_box(const char *line, StackFrame *parent, int *x, int *y, int *w, int *h) {
    int px = attr_int(line, "x", -1);
    int py = attr_int(line, "y", -1);
    int pw = attr_int(line, "width", attr_int(line, "w", 180));
    int ph = attr_int(line, "height", attr_int(line, "h", 36));

    if (px >= 0) *x = parent->x + px_to_cell_x(px);
    else *x = parent->x + 1;

    if (py >= 0) *y = parent->y + px_to_cell_y(py);
    else *y = parent->y + parent->next_y;

    *w = px_len_to_cells_x(pw);
    *h = px_len_to_cells_y(ph);
    if (*w < 2) *w = 2;
    if (*h < 1) *h = 1;
    if (*x + *w > g_cols) *w = g_cols - *x;
    if (*y + *h > g_rows) *h = g_rows - *y;
}

static void draw_button(ObjectRecord *obj) {
    char text[320];
    int text_x;
    int max_label;
    fill_rect(obj->x, obj->y, obj->w, obj->h, ' ', obj->fg, obj->bg, obj->id);
    draw_border(obj->x, obj->y, obj->w, obj->h, obj->border, obj->bg, obj->id);
    max_label = obj->w - 4;
    if (max_label < 1) max_label = 1;
    snprintf(text, sizeof(text), "[%.*s]", max_label, obj->label[0] ? obj->label : "button");
    text_x = obj->x + (obj->w - (int)strlen(text)) / 2;
    if (text_x < obj->x + 1) text_x = obj->x + 1;
    draw_text(text_x, obj->y + obj->h / 2, text, obj->fg, obj->bg, obj->id, "button");
}

static void draw_checkbox(ObjectRecord *obj, const char *line) {
    char checked[16];
    char text[320];
    attr_string(line, "checked", checked, sizeof(checked), "false");
    snprintf(text, sizeof(text), "[%c] %s", strcmp(checked, "true") == 0 ? 'x' : ' ', obj->label[0] ? obj->label : obj->dom_id);
    draw_text(obj->x, obj->y, text, obj->fg, obj->bg, obj->id, "checkbox");
}

static void draw_slider(ObjectRecord *obj, const char *line) {
    int min_v = attr_int(line, "min", 0);
    int max_v = attr_int(line, "max", 100);
    int value = attr_int(line, "value", min_v);
    int bar_w = obj->w;
    int filled = 0;
    char label[320];
    char bar[160];
    const char *raw_label = obj->label[0] ? obj->label : "slider";
    int label_w = 0;
    int i;

    label_w = (int)strlen(raw_label);
    if (obj->w >= 18) {
        bar_w = obj->w - label_w - 1;
        if (bar_w < 6) bar_w = 6;
    } else {
        label_w = 0;
        bar_w = obj->w;
    }

    if (bar_w > (int)sizeof(bar) - 3) bar_w = (int)sizeof(bar) - 3;
    if (bar_w < 6) bar_w = 6;
    if (bar_w > obj->w) bar_w = obj->w;
    if (max_v <= min_v) max_v = min_v + 1;
    if (value < min_v) value = min_v;
    if (value > max_v) value = max_v;
    filled = ((value - min_v) * (bar_w - 2)) / (max_v - min_v);

    bar[0] = '[';
    for (i = 0; i < bar_w - 2; i++) bar[i + 1] = i <= filled ? '=' : '-';
    bar[bar_w - 1] = ']';
    bar[bar_w] = '\0';

    if (label_w > 0) snprintf(label, sizeof(label), "%s %s", raw_label, bar);
    else snprintf(label, sizeof(label), "%s", bar);
    draw_text_clipped(obj->x, obj->y, label, obj->w, obj->fg, obj->bg, obj->id, "slider");
}

static void draw_media(ObjectRecord *obj, const char *kind) {
    int yy;
    fill_rect(obj->x, obj->y, obj->w, obj->h, '.', obj->fg, obj->bg, obj->id);
    draw_border(obj->x, obj->y, obj->w, obj->h, obj->border, obj->bg, obj->id);
    for (yy = obj->y + 1; yy < obj->y + obj->h - 1; yy++) {
        int xx;
        for (xx = obj->x + 1; xx < obj->x + obj->w - 1; xx++) {
            char ch = ((xx + yy) % 3 == 0) ? '#' : ((xx + yy) % 3 == 1) ? ':' : '.';
            set_cell(xx, yy, ch, obj->fg, obj->bg, obj->id, "media");
        }
    }
    if (obj->src[0]) {
        char label[320];
        snprintf(label, sizeof(label), "%s:%s", kind, obj->src);
        draw_text_clipped(obj->x + 1, obj->y + obj->h / 2, label, obj->w - 2, obj->fg, obj->bg, obj->id, "media_label");
    }
}

static void parse_layout(const char *layout) {
    char *copy = strdup(layout);
    char *saveptr = NULL;
    char *line = NULL;
    StackFrame stack[MAX_STACK];
    int top = -1;

    if (!copy) return;
    line = strtok_r(copy, "\n", &saveptr);
    while (line) {
        char *t = trim_ws(line);
        StackFrame parent = current_parent(stack, top);

        if (t[0] == '\0' || starts_with(t, "<!--")) {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        if (starts_with(t, "</")) {
            stack_pop(stack, &top);
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        if (starts_with(t, "<window") || starts_with(t, "<panel") || starts_with(t, "<canvas") ||
            starts_with(t, "<header") || starts_with(t, "<menu") || starts_with(t, "<div")) {
            const char *tag = "panel";
            const char *role = "panel";
            ObjectRecord *obj = NULL;
            RGB fg = attr_rgb(t, "fg", attr_rgb(t, "color_fg", RGB_WHITE));
            RGB bg = attr_rgb(t, "color", attr_rgb(t, "bg", RGB_BLACK));
            RGB border = attr_rgb(t, "border", RGB_BORDER);
            int x = 0;
            int y = 0;
            int w = 1;
            int h = 1;
            int self_closing = strstr(t, "/>") != NULL;

            if (starts_with(t, "<window")) {
                char title[256];
                tag = "window";
                role = "window";
                attr_string(t, "title", title, sizeof(title), "Wraith PM");
                g_window_w = attr_int(t, "width", g_window_w);
                g_window_h = attr_int(t, "height", g_window_h);
                g_cols = px_len_to_cells_x(g_window_w);
                g_rows = px_len_to_cells_y(g_window_h);
                if (g_cols > MAX_COLS) g_cols = MAX_COLS;
                if (g_rows > MAX_ROWS) g_rows = MAX_ROWS;
                init_cells(fg, bg);
                x = 0;
                y = 0;
                w = g_cols;
                h = g_rows;
                obj = add_object(tag, t, role, x, y, w, h, fg, bg, border);
                if (obj && obj->label[0] == '\0') strncpy(obj->label, title, sizeof(obj->label) - 1);
                if (obj) {
                    fill_rect(x, y, w, h, ' ', fg, bg, obj->id);
                    draw_border(x, y, w, h, border, bg, obj->id);
                    draw_text(x + 2, y, title, fg, bg, obj->id, "title");
                    if (!self_closing) stack_push(stack, &top, obj->id, x, y, w, h);
                }
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }

            if (starts_with(t, "<canvas")) {
                tag = "canvas";
                role = "canvas";
            } else if (starts_with(t, "<header")) {
                tag = "header";
                role = "banner";
            } else if (starts_with(t, "<menu")) {
                tag = "menu";
                role = "menu";
            } else if (starts_with(t, "<div")) {
                tag = "div";
                role = "group";
            }

            compute_box(t, &parent, &x, &y, &w, &h);
            obj = add_object(tag, t, role, x, y, w, h, fg, bg, border);
            if (obj) {
                fill_rect(x, y, w, h, ' ', fg, bg, obj->id);
                draw_border(x, y, w, h, border, bg, obj->id);
                if (obj->label[0]) draw_text(x + 1, y + (strcmp(tag, "header") == 0 ? 0 : 1), obj->label, fg, bg, obj->id, "label");
                if (!self_closing) stack_push(stack, &top, obj->id, x, y, w, h);
            }
        } else if (starts_with(t, "<text") || starts_with(t, "<textfield") || starts_with(t, "<scroller")) {
            ObjectRecord *obj = NULL;
            RGB fg = attr_rgb(t, "color", attr_rgb(t, "fg", RGB_WHITE));
            RGB bg = parent.object_id > 0 ? g_objects[parent.object_id - 1].bg : RGB_BLACK;
            int x = 0;
            int y = 0;
            int w = 30;
            int h = 1;
            compute_box(t, &parent, &x, &y, &w, &h);
            if (!has_attr(t, "width") && !has_attr(t, "w")) {
                w = parent.x + parent.w - x - 1;
                if (w < 1) w = 1;
            }
            obj = add_object(starts_with(t, "<textfield") ? "textfield" : starts_with(t, "<scroller") ? "scroller" : "text",
                             t, "text", x, y, w, h, fg, bg, fg);
            if (obj) draw_text_clipped(x, y, obj->label[0] ? obj->label : obj->dom_id, obj->w, fg, bg, obj->id, "text");
        } else if (starts_with(t, "<button") || starts_with(t, "<menuitem")) {
            ObjectRecord *obj = NULL;
            RGB fg = attr_rgb(t, "fg", RGB_WHITE);
            RGB bg = attr_rgb(t, "color", (RGB){45, 106, 79});
            RGB border = attr_rgb(t, "border", fg);
            int x = 0;
            int y = 0;
            int w = 18;
            int h = 2;
            compute_box(t, &parent, &x, &y, &w, &h);
            if (starts_with(t, "<menuitem")) {
                x = parent.x + 1;
                y = parent.y + parent.next_y;
                w = parent.w - 2;
                h = 1;
                if (top >= 0) stack[top].next_y++;
            }
            obj = add_object(starts_with(t, "<menuitem") ? "menuitem" : "button", t, "button", x, y, w, h, fg, bg, border);
            if (obj) draw_button(obj);
        } else if (starts_with(t, "<checkbox")) {
            ObjectRecord *obj = NULL;
            RGB fg = attr_rgb(t, "fg", RGB_WHITE);
            RGB bg = attr_rgb(t, "color", parent.object_id > 0 ? g_objects[parent.object_id - 1].bg : RGB_BLACK);
            int x = 0;
            int y = 0;
            int w = 24;
            int h = 1;
            compute_box(t, &parent, &x, &y, &w, &h);
            obj = add_object("checkbox", t, "checkbox", x, y, w, h, fg, bg, fg);
            if (obj) draw_checkbox(obj, t);
        } else if (starts_with(t, "<slider")) {
            ObjectRecord *obj = NULL;
            RGB fg = attr_rgb(t, "fg", RGB_WHITE);
            RGB bg = attr_rgb(t, "color", parent.object_id > 0 ? g_objects[parent.object_id - 1].bg : RGB_BLACK);
            int x = 0;
            int y = 0;
            int w = 22;
            int h = 1;
            compute_box(t, &parent, &x, &y, &w, &h);
            obj = add_object("slider", t, "slider", x, y, w, h, fg, bg, fg);
            if (obj) draw_slider(obj, t);
        } else if (starts_with(t, "<img") || starts_with(t, "<video")) {
            ObjectRecord *obj = NULL;
            const char *tag = starts_with(t, "<img") ? "img" : "video";
            RGB fg = attr_rgb(t, "fg", RGB_WHITE);
            RGB bg = attr_rgb(t, "color", starts_with(t, "<img") ? (RGB){51, 65, 85} : (RGB){61, 44, 84});
            RGB border = attr_rgb(t, "border", fg);
            int x = 0;
            int y = 0;
            int w = 20;
            int h = 6;
            compute_box(t, &parent, &x, &y, &w, &h);
            obj = add_object(tag, t, starts_with(t, "<img") ? "image" : "video", x, y, w, h, fg, bg, border);
            if (obj) draw_media(obj, starts_with(t, "<img") ? "IMG" : "VID");
        } else if (t[0] == '<') {
            char warning[512];
            snprintf(warning, sizeof(warning), "Skipped unknown CHTMGL tag: %.180s", t);
            append_warning(warning);
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(copy);
}

static int read_frame_counter(const char *path) {
    FILE *f = fopen(path, "r");
    int value = 0;
    if (!f) return 0;
    if (fscanf(f, "%d", &value) != 1) value = 0;
    fclose(f);
    return value;
}

static void write_frame_counter(const char *path, int value) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d\n", value);
    fclose(f);
}

static void write_plain_frame(const char *path) {
    FILE *f = fopen(path, "w");
    int y;
    int x;
    if (!f) return;
    for (y = 0; y < g_rows; y++) {
        for (x = 0; x < g_cols; x++) fputc(g_cells[y][x].ch, f);
        fputc('\n', f);
    }
    fclose(f);
}

static void write_ansi_frame(const char *path) {
    FILE *f = fopen(path, "w");
    int y;
    int x;
    if (!f) return;
    for (y = 0; y < g_rows; y++) {
        for (x = 0; x < g_cols; x++) {
            Cell *c = &g_cells[y][x];
            fprintf(f, "\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm%c",
                    c->fg.r, c->fg.g, c->fg.b, c->bg.r, c->bg.g, c->bg.b, c->ch);
        }
        fprintf(f, "\033[0m\n");
    }
    fclose(f);
}

static void write_cells_pdl(const char *path, int frame_id) {
    FILE *f = fopen(path, "w");
    int y;
    int x;
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", frame_id);
    fprintf(f, "FRAME | cols | %d\n", g_cols);
    fprintf(f, "FRAME | rows | %d\n", g_rows);
    fprintf(f, "FRAME | cell_width_px | %d\n", g_cell_w);
    fprintf(f, "FRAME | cell_height_px | %d\n", g_cell_h);
    for (y = 0; y < g_rows; y++) {
        for (x = 0; x < g_cols; x++) {
            Cell *c = &g_cells[y][x];
            char fg[16];
            char bg[16];
            rgb_hex(c->fg, fg, sizeof(fg));
            rgb_hex(c->bg, bg, sizeof(bg));
            fprintf(f, "CELL | x=%d,y=%d | ch=%02X fg=%s bg=%s object=%d style=%s\n",
                    x, y, (unsigned char)c->ch, fg, bg, c->object_id, c->style);
        }
    }
    fclose(f);
}

static void write_meta_pdl(const char *path, int frame_id) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", frame_id);
    fprintf(f, "FRAME | project_id | %s\n", g_project_id);
    fprintf(f, "FRAME | source_layout | %s\n", g_layout_path);
    fprintf(f, "FRAME | cols | %d\n", g_cols);
    fprintf(f, "FRAME | rows | %d\n", g_rows);
    fprintf(f, "FRAME | viewport_width_px | %d\n", g_window_w);
    fprintf(f, "FRAME | viewport_height_px | %d\n", g_window_h);
    fprintf(f, "FRAME | cell_width_px | %d\n", g_cell_w);
    fprintf(f, "FRAME | cell_height_px | %d\n", g_cell_h);
    fprintf(f, "FRAME | rgb_width_px | %d\n", g_cols * g_cell_w);
    fprintf(f, "FRAME | rgb_height_px | %d\n", g_rows * g_cell_h);
    fprintf(f, "RASTER | font_policy | converter_owns_glyphs\n");
    fprintf(f, "RASTER | gl_policy | gl_presents_rgb_only\n");
    fprintf(f, "WARNINGS | count | %s\n", g_warnings[0] ? "present" : "none");
    fclose(f);
}

static void write_objects_pdl(const char *path, int frame_id) {
    FILE *f = fopen(path, "w");
    int i;
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", frame_id);
    for (i = 0; i < g_object_count; i++) {
        ObjectRecord *o = &g_objects[i];
        char fg[16];
        char bg[16];
        char border[16];
        rgb_hex(o->fg, fg, sizeof(fg));
        rgb_hex(o->bg, bg, sizeof(bg));
        rgb_hex(o->border, border, sizeof(border));
        fprintf(f,
                "OBJECT | %04d | tag=%s id=%s role=%s x=%d y=%d w=%d h=%d fg=%s bg=%s border=%s label=%s action=%s src=%s\n",
                o->id, o->tag, o->dom_id, o->role, o->x, o->y, o->w, o->h, fg, bg, border,
                o->label, o->action, o->src);
    }
    fclose(f);
}

static void write_hitmap_pdl(const char *path, int frame_id) {
    FILE *f = fopen(path, "w");
    int y;
    int x;
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "FRAME | frame_id | %06d\n", frame_id);
    for (y = 0; y < g_rows; y++) {
        for (x = 0; x < g_cols; x++) {
            if (g_cells[y][x].object_id > 0) {
                fprintf(f, "HIT | x=%d,y=%d | object=%d\n", x, y, g_cells[y][x].object_id);
            }
        }
    }
    fclose(f);
}

static void write_audit(const char *path, int frame_id) {
    FILE *f = fopen(path, "w");
    time_t now = time(NULL);
    if (!f) return;
    fprintf(f, "WRAITH FRAME AUDIT\n");
    fprintf(f, "frame_id=%06d\n", frame_id);
    fprintf(f, "project_id=%s\n", g_project_id);
    fprintf(f, "source_layout=%s\n", g_layout_path);
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "cols=%d\n", g_cols);
    fprintf(f, "rows=%d\n", g_rows);
    fprintf(f, "cell_width_px=%d\n", g_cell_w);
    fprintf(f, "cell_height_px=%d\n", g_cell_h);
    fprintf(f, "object_count=%d\n", g_object_count);
    fprintf(f, "outputs=current_frame.txt,current_frame.ansi.txt,current_frame.cells.pdl,current_frame.meta.pdl,current_frame.objects.pdl,current_frame.hitmap.pdl\n");
    fprintf(f, "warnings=%s\n", g_warnings[0] ? "present" : "none");
    if (g_warnings[0]) {
        fprintf(f, "\nWARNINGS\n%s", g_warnings);
    }
    fclose(f);
}

static void pulse_frame_changed(const char *path, int frame_id) {
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%06d\n", frame_id);
    fclose(f);
}

static int render_frame(void) {
    char *layout_raw = NULL;
    char *layout_sub = NULL;
    char session_dir[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    int frame_id = 0;

    snprintf(session_dir, sizeof(session_dir), "projects/%s/session", g_project_id);
    load_state_file("projects/wraith-pm/session/state.txt");
    load_state_file("projects/wraith-pm/manager/gui_state.txt");

    if (strlen(get_var("cell_width_px")) > 0) g_cell_w = atoi(get_var("cell_width_px"));
    if (strlen(get_var("cell_height_px")) > 0) g_cell_h = atoi(get_var("cell_height_px"));
    if (g_cell_w <= 0) g_cell_w = 10;
    if (g_cell_h <= 0) g_cell_h = 18;

    layout_raw = read_file(g_layout_path);
    if (!layout_raw) {
        fprintf(stderr, "wraith_parser: failed to read layout %s\n", g_layout_path);
        return 1;
    }

    layout_sub = (char *)malloc(MAX_BUFFER);
    if (!layout_sub) {
        free(layout_raw);
        return 1;
    }
    substitute_vars(layout_raw, layout_sub, MAX_BUFFER);
    free(layout_raw);

    init_cells(RGB_WHITE, RGB_BLACK);
    parse_layout(layout_sub);
    free(layout_sub);

    snprintf(path, sizeof(path), "%s/%s/frame_counter.txt", g_project_root, session_dir);
    frame_id = read_frame_counter(path) + 1;
    write_frame_counter(path, frame_id);

    snprintf(path, sizeof(path), "%s/%s/current_frame.txt", g_project_root, session_dir);
    write_plain_frame(path);
    snprintf(path, sizeof(path), "%s/%s/current_frame.ansi.txt", g_project_root, session_dir);
    write_ansi_frame(path);
    snprintf(path, sizeof(path), "%s/%s/current_frame.cells.pdl", g_project_root, session_dir);
    write_cells_pdl(path, frame_id);
    snprintf(path, sizeof(path), "%s/%s/current_frame.meta.pdl", g_project_root, session_dir);
    write_meta_pdl(path, frame_id);
    snprintf(path, sizeof(path), "%s/%s/current_frame.objects.pdl", g_project_root, session_dir);
    write_objects_pdl(path, frame_id);
    snprintf(path, sizeof(path), "%s/%s/current_frame.hitmap.pdl", g_project_root, session_dir);
    write_hitmap_pdl(path, frame_id);
    snprintf(path, sizeof(path), "%s/%s/current_frame.audit.txt", g_project_root, session_dir);
    write_audit(path, frame_id);
    snprintf(path, sizeof(path), "%s/%s/frame_changed.txt", g_project_root, session_dir);
    pulse_frame_changed(path, frame_id);

    printf("wraith_parser: frame %06d rendered for %s\n", frame_id, g_project_id);
    return 0;
}

int main(int argc, char **argv) {
    resolve_root();
    if (argc > 1 && argv[1][0] != '\0') {
        strncpy(g_layout_path, argv[1], sizeof(g_layout_path) - 1);
        g_layout_path[sizeof(g_layout_path) - 1] = '\0';
    }
    if (argc > 2 && argv[2][0] != '\0') {
        strncpy(g_project_id, argv[2], sizeof(g_project_id) - 1);
        g_project_id[sizeof(g_project_id) - 1] = '\0';
    }
    set_var("project_id", g_project_id);
    return render_frame();
}
