#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE 4096
#define MAX_VARS 256
#define MAX_VAR_NAME 96
#define MAX_VAR_VALUE 4096
#define MAX_COLS 96
#define MAX_ROWS 30
#define MAX_OBJECTS 64

typedef struct { int r, g, b; } RGB;
typedef struct { char ch; RGB fg, bg; int object_id; char style[24]; } Cell;
typedef struct { char name[MAX_VAR_NAME]; char value[MAX_VAR_VALUE]; } Var;
typedef struct {
    int id; char dom_id[96], role[48], label[256], src[MAX_PATH_LEN];
    int x, y, w, h, z, focused, minimized; RGB fg, bg, border;
} ObjectRecord;

static Cell g_cells[MAX_ROWS][MAX_COLS];
static Var g_vars[MAX_VARS];
static ObjectRecord g_objects[MAX_OBJECTS];
static int g_var_count = 0, g_object_count = 0, g_cols = 96, g_rows = 30;
static char g_project_root[MAX_PATH_LEN] = ".";
static int g_mouse_x = 0, g_mouse_y = 0;

static const RGB RGB_WHITE = {232, 241, 242}, RGB_BLACK = {16, 24, 32}, RGB_BORDER = {126, 223, 242}, RGB_YELLOW = {255, 255, 0};

static char *trim_ws(char *s) {
    char *end = NULL; if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static void resolve_root(void) {
    if (!getcwd(g_project_root, sizeof(g_project_root))) strcpy(g_project_root, ".");
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) { strncpy(g_project_root, trim_ws(line + 13), MAX_PATH_LEN - 1); break; }
        }
        fclose(kvp);
    }
}

static void set_var(const char *name, const char *value) {
    for (int i = 0; i < g_var_count; i++) { if (strcmp(g_vars[i].name, name) == 0) { strncpy(g_vars[i].value, value, MAX_VAR_VALUE - 1); return; } }
    if (g_var_count < MAX_VARS) { strncpy(g_vars[g_var_count].name, name, MAX_VAR_NAME - 1); strncpy(g_vars[g_var_count].value, value, MAX_VAR_VALUE - 1); g_var_count++; }
}

static const char *get_var(const char *name) {
    for (int i = 0; i < g_var_count; i++) { if (strcmp(g_vars[i].name, name) == 0) return g_vars[i].value; }
    return "";
}

static void load_state_file(const char *rel_path) {
    char path[MAX_PATH_LEN]; snprintf(path, sizeof(path), "%s/%s", g_project_root, rel_path);
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) { *eq = '\0'; set_var(trim_ws(line), trim_ws(eq + 1)); }
    }
    fclose(f);
}

static void init_cells(RGB fg, RGB bg) {
    for (int y = 0; y < MAX_ROWS; y++) {
        for (int x = 0; x < MAX_COLS; x++) { g_cells[y][x].ch = ' '; g_cells[y][x].fg = fg; g_cells[y][x].bg = bg; g_cells[y][x].object_id = 0; strcpy(g_cells[y][x].style, "normal"); }
    }
}

static void set_cell(int x, int y, char ch, RGB fg, RGB bg, int object_id, const char *style) {
    if (x < 0 || y < 0 || x >= g_cols || y >= g_rows) return;
    g_cells[y][x].ch = ch; g_cells[y][x].fg = fg; g_cells[y][x].bg = bg; g_cells[y][x].object_id = object_id;
    if (style) strncpy(g_cells[y][x].style, style, 23);
}

static void fill_rect(int x, int y, int w, int h, char ch, RGB fg, RGB bg, int object_id) {
    for (int yy = y; yy < y + h; yy++) { for (int xx = x; xx < x + w; xx++) set_cell(xx, yy, ch, fg, bg, object_id, "normal"); }
}

static void draw_text(int x, int y, const char *text, RGB fg, RGB bg, int object_id, const char *style) {
    if (!text) return;
    for (int i = 0; text[i] && x + i < g_cols; i++) set_cell(x + i, y, text[i], fg, bg, object_id, style);
}

static void draw_border(int x, int y, int w, int h, RGB fg, RGB bg, int object_id) {
    if (w <= 1 || h <= 1) return;
    set_cell(x, y, '+', fg, bg, object_id, "border"); set_cell(x + w - 1, y, '+', fg, bg, object_id, "border");
    set_cell(x, y + h - 1, '+', fg, bg, object_id, "border"); set_cell(x + w - 1, y + h - 1, '+', fg, bg, object_id, "border");
    for (int i = 1; i < w - 1; i++) { set_cell(x + i, y, '-', fg, bg, object_id, "border"); set_cell(x + i, y + h - 1, '-', fg, bg, object_id, "border"); }
    for (int i = 1; i < h - 1; i++) { set_cell(x, y + i, '|', fg, bg, object_id, "border"); set_cell(x + w - 1, y + i, '|', fg, bg, object_id, "border"); }
}

static void draw_window_chrome(ObjectRecord *obj) {
    RGB chrome = obj->focused ? obj->border : obj->fg;
    fill_rect(obj->x, obj->y, obj->w, obj->h, ' ', obj->fg, obj->bg, obj->id);
    draw_border(obj->x, obj->y, obj->w, obj->h, chrome, obj->bg, obj->id);
    for (int i = 1; i < obj->w - 1; i++) set_cell(obj->x + i, obj->y, obj->focused ? '=' : '-', chrome, obj->bg, obj->id, "titlebar");
    char title[256]; snprintf(title, sizeof(title), " %s ", obj->label[0] ? obj->label : obj->dom_id);
    draw_text(obj->x + 2, obj->y, title, chrome, obj->bg, obj->id, "title");
    const char *ctrls = "[o][-][x]"; int cx = obj->x + obj->w - 11;
    if (cx > obj->x + (int)strlen(title) + 2) draw_text(cx, obj->y, ctrls, chrome, obj->bg, obj->id, "titlebar");
}

static void merge_view(ObjectRecord *obj) {
    if (obj->minimized || !obj->src[0] || strcmp(obj->src, "none") == 0) return;
    char path[MAX_PATH_LEN]; if (obj->src[0] == '/') strcpy(path, obj->src);
    else snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/%s", g_project_root, obj->src);
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[MAX_LINE]; int ly = 0;
    while (fgets(line, sizeof(line), f) && ly < obj->h - 2) {
        line[strcspn(line, "\n\r")] = 0;
        for (int lx = 0; line[lx] && lx < obj->w - 2; lx++) set_cell(obj->x + 1 + lx, obj->y + 1 + ly, line[lx], obj->fg, obj->bg, obj->id, "content");
        ly++;
    }
    fclose(f);
}

static int compare_z(const void *a, const void *b) { return ((ObjectRecord*)a)->z - ((ObjectRecord*)b)->z; }

static char* extract_val(const char* line, const char* key, char* dst, size_t sz) {
    char needle[64]; snprintf(needle, 64, "%s=", key);
    char *p = strstr(line, needle); if (!p) return NULL;
    p += strlen(needle);
    size_t i = 0; 
    while (*p && i < sz - 1) { 
        if (*p == '|') break;
        // Logic fix: look ahead for next key= pattern if space encountered
        if (*p == ' ' && (strstr(p, " x=") || strstr(p, " y=") || strstr(p, " w=") || strstr(p, " h=") || strstr(p, " z=") || strstr(p, " focused=") || strstr(p, " minimized=") || strstr(p, " role=") || strstr(p, " src="))) break;
        dst[i++] = *p++; 
    }
    dst[i] = '\0'; return trim_ws(dst);
}

static void load_windows_pdl(void) {
    char path[MAX_PATH_LEN]; snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/windows_state.pdl", g_project_root);
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && g_object_count < MAX_OBJECTS) {
        if (strncmp(line, "WINDOW |", 8) == 0) {
            ObjectRecord *obj = &g_objects[g_object_count++]; memset(obj, 0, sizeof(*obj));
            obj->id = g_object_count; strcpy(obj->role, "window"); obj->fg = RGB_WHITE; obj->bg = RGB_BLACK; obj->border = RGB_BORDER;
            extract_val(line, "id", obj->dom_id, 96); extract_val(line, "title", obj->label, 256);
            char tmp[256];
            if (extract_val(line, "x", tmp, 256)) obj->x = atoi(tmp) / 10;
            if (extract_val(line, "y", tmp, 256)) obj->y = atoi(tmp) / 18;
            if (extract_val(line, "w", tmp, 256)) obj->w = atoi(tmp) / 10;
            if (extract_val(line, "h", tmp, 256)) obj->h = atoi(tmp) / 18;
            if (extract_val(line, "z", tmp, 256)) obj->z = atoi(tmp);
            if (extract_val(line, "focused", tmp, 256)) obj->focused = (strcmp(tmp, "true") == 0);
            if (extract_val(line, "minimized", tmp, 256)) obj->minimized = (strcmp(tmp, "true") == 0);
            extract_val(line, "src", obj->src, MAX_PATH_LEN);
            if (obj->w < 4) obj->w = 4; if (obj->h < 2) obj->h = 2;
        }
    }
    fclose(f); qsort(g_objects, g_object_count, sizeof(ObjectRecord), compare_z);
}

static void draw_terrain(void) {
    for (int y = 0; y < g_rows; y++) { for (int x = 0; x < g_cols; x++) { if ((x + y) % 10 == 0) set_cell(x, y, '.', (RGB){40, 40, 80}, RGB_BLACK, 0, "bg"); } }
    fill_rect(0, 0, g_cols, 1, ' ', RGB_WHITE, (RGB){20, 20, 60}, 1);
    draw_text(2, 0, "WRAITH DESKTOP GUI", RGB_WHITE, (RGB){20, 20, 60}, 1, "banner");
    char coords[32]; snprintf(coords, sizeof(coords), "MOUSE: %d, %d", g_mouse_x, g_mouse_y);
    draw_text(g_cols - 20, 0, coords, RGB_YELLOW, (RGB){20, 20, 60}, 1, "debug");
    fill_rect(0, g_rows - 1, g_cols, 1, ' ', RGB_WHITE, (RGB){20, 20, 60}, 2);
    draw_text(1, g_rows - 1, "[Start]", RGB_YELLOW, (RGB){20, 20, 60}, 2, "start_btn");
    draw_text(10, g_rows - 1, get_var("taskbar_windows_label"), RGB_WHITE, (RGB){20, 20, 60}, 2, "taskbar");
    draw_text(g_cols - 20, g_rows - 1, get_var("taskbar_clock"), RGB_WHITE, (RGB){20, 20, 60}, 2, "clock");
}

static void draw_mousehand(void) {
    int cx = g_mouse_x / 10, cy = g_mouse_y / 18;
    if (cx >= 0 && cx < g_cols && cy >= 0 && cy < g_rows) set_cell(cx, cy, '+', RGB_YELLOW, g_cells[cy][cx].bg, 0, "mousehand");
}

static void write_outputs(void) {
    char path[MAX_PATH_LEN]; snprintf(path, sizeof(path), "%s/projects/wraith-pm/manager/view.txt", g_project_root);
    FILE *f = fopen(path, "w"); if (f) { for (int y = 0; y < g_rows; y++) { for (int x = 0; x < g_cols; x++) fputc(g_cells[y][x].ch, f); fputc('\n', f); } fclose(f); }
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.hitmap.pdl", g_project_root);
    f = fopen(path, "w"); if (f) {
        fprintf(f, "SECTION | KEY | VALUE\n");
        for (int y = 0; y < g_rows; y++) {
            for (int x = 0; x < g_cols; x++) {
                if (g_cells[y][x].object_id > 0) {
                    ObjectRecord *obj = NULL; for(int i=0; i<g_object_count; i++) if(g_objects[i].id == g_cells[y][x].object_id) { obj = &g_objects[i]; break; }
                    const char *role = "body"; if (obj && y == obj->y) { int ex = obj->x + obj->w; if (x == ex - 2) role = "close_btn"; else if (x == ex - 5) role = "min_btn"; else role = "titlebar"; }
                    fprintf(f, "HIT | x=%d,y=%d | object=%d role=%s id=%s\n", x, y, g_cells[y][x].object_id, role, obj ? obj->dom_id : "none");
                }
            }
        }
        fclose(f);
    }
}

int main(void) {
    resolve_root(); load_state_file("projects/wraith-pm/session/state.txt"); load_state_file("projects/wraith-pm/session/nav_state.txt");
    g_mouse_x = atoi(get_var("mouse_x")); g_mouse_y = atoi(get_var("mouse_y"));
    init_cells(RGB_WHITE, RGB_BLACK); draw_terrain(); load_windows_pdl();
    for (int i = 0; i < g_object_count; i++) { if (!g_objects[i].minimized) { draw_window_chrome(&g_objects[i]); merge_view(&g_objects[i]); } }
    draw_mousehand(); write_outputs(); return 0;
}
