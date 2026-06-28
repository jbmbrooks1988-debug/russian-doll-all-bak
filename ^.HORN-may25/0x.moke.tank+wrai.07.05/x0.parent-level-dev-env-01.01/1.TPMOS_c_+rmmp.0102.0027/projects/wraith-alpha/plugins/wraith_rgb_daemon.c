#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define GLYPH_W 8
#define GLYPH_H 16
#define COLS 128
#define ROWS 40
#define WIDTH (COLS * GLYPH_W)
#define HEIGHT (ROWS * GLYPH_H)
#define MAX_OBJECTS 256
#define MAX_LABEL 256

#define WRAITH_UI_STATE "projects/wraith-alpha/session/desktop_ui_state.txt"
#define SEMANTIC_META_PATH "pieces/display/current_frame.meta.pdl"
#define SEMANTIC_OBJECTS_PATH "pieces/display/current_frame.objects.pdl"
#define WRAITH_FRAME_SOURCE "projects/wraith-alpha/session/rgb/current_frame.rgba32"
#define RGB_RECEIPT_PATH "projects/wraith-alpha/session/rgb/current_frame.receipt.pdl"

typedef struct {
    char tag[32];
    char role[32];
    char parent_id[64];
    char container_id[64];
    char source_ref[128];
    char ancestor_chain[256];
    char clip_chain[256];
    int nav;
    int nav_selected;
    char nav_selector_glyph[8];
    int x;
    int y;
    int w;
    int h;
    int z;
    int focused;
    unsigned char fg[3];
    unsigned char bg[3];
    unsigned char border[3];
    char label[MAX_LABEL];
    char label_core[MAX_LABEL];
    char action[256];
} FrameObject;

static unsigned char glyphs[128][GLYPH_W * GLYPH_H];
static int g_presenter_ascii_mode = 0;

typedef struct {
    int valid;
    char project_id[128];
    char source_layout[256];
    char focused_object_id[128];
    char focused_object_dom_id[128];
    int mouse_x;
    int mouse_y;
    int mouse_hit_offset_x;
    int mouse_hit_offset_y;
    int mouse_cursor_visual_uses_offset;
} SemanticSourceInfo;

static void color_to_hex(const unsigned char rgb[3], char out[8]) {
    if (!out) {
        return;
    }
    snprintf(out, 8, "#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);
}

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

static long file_mtime_epoch(const char *path) {
    struct stat st;
    if (!path || stat(path, &st) != 0) {
        return -1;
    }
    return (long)st.st_mtime;
}

static void clear_buffer(unsigned char *buffer, unsigned char r, unsigned char g, unsigned char b) {
    int i;
    for (i = 0; i < WIDTH * HEIGHT * 4; i += 4) {
        buffer[i] = r;
        buffer[i + 1] = g;
        buffer[i + 2] = b;
        buffer[i + 3] = 255;
    }
}

static int parse_hex_color(const char *value, unsigned char rgb[3]) {
    unsigned int r, g, b;
    if (!value || value[0] != '#') return 0;
    if (sscanf(value + 1, "%02x%02x%02x", &r, &g, &b) != 3) return 0;
    rgb[0] = (unsigned char)r;
    rgb[1] = (unsigned char)g;
    rgb[2] = (unsigned char)b;
    return 1;
}

static void load_glyphs(void) {
    int i;
    memset(glyphs, 0, sizeof(glyphs));
    for (i = 32; i < 127; i++) {
        char path[1024];
        FILE *f;
        char line[64];
        int y = 0;
        snprintf(path, sizeof(path), "projects/wraith-alpha/assets/fonts/ascii/%d/glyph.txt", i);
        f = fopen(path, "r");
        if (!f) continue;
        while (fgets(line, sizeof(line), f) && y < GLYPH_H) {
            int x;
            for (x = 0; x < GLYPH_W && line[x] != '\0' && line[x] != '\n'; x++) {
                glyphs[i][y * GLYPH_W + x] = (line[x] == '#') ? 255 : 0;
            }
            y++;
        }
        fclose(f);
    }
}

static void blit_char(unsigned char *buffer, int col, int row, unsigned char c,
                      unsigned char r, unsigned char g, unsigned char b,
                      int cell_w, int cell_h) {
    int start_x;
    int start_y;
    int y, x;
    if (c > 127) return;
    if (cell_w <= 0) cell_w = GLYPH_W;
    if (cell_h <= 0) cell_h = GLYPH_H;
    start_x = col * cell_w;
    start_y = row * cell_h;
    for (y = 0; y < GLYPH_H; y++) {
        for (x = 0; x < GLYPH_W; x++) {
            int dx = start_x + x;
            int dy = start_y + y;
            int idx;
            if (dx >= WIDTH || dy >= HEIGHT) continue;
            if (!glyphs[c][y * GLYPH_W + x]) continue;
            idx = (dy * WIDTH + dx) * 4;
            buffer[idx] = r;
            buffer[idx + 1] = g;
            buffer[idx + 2] = b;
            buffer[idx + 3] = 255;
        }
    }
}

static void blit_text(unsigned char *buffer, int col, int row, const char *text,
                      const unsigned char rgb[3], int max_cols, int cell_w, int cell_h) {
    int i;
    if (!text) return;
    for (i = 0; text[i] != '\0'; i++) {
        if (max_cols >= 0 && i >= max_cols) break;
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) continue;
        blit_char(buffer, col + i, row, (unsigned char)text[i], rgb[0], rgb[1], rgb[2], cell_w, cell_h);
    }
}

static void build_display_label(const FrameObject *obj, char *out, size_t out_sz) {
    const char *core;
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!obj) return;
    core = obj->label_core[0] != '\0' ? obj->label_core : obj->label;
    if (obj->nav > 0) {
        const char *glyph = obj->nav_selector_glyph[0] != '\0' ? obj->nav_selector_glyph : " ";
        snprintf(out, out_sz, "[%s] %d. [%s]", glyph, obj->nav, core);
        return;
    }
    snprintf(out, out_sz, "%s", core);
}

static void fill_rect_px(unsigned char *buffer, int x0, int y0, int x1, int y1,
                         const unsigned char rgb[3]) {
    int x, y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > WIDTH) x1 = WIDTH;
    if (y1 > HEIGHT) y1 = HEIGHT;
    if (x0 >= x1 || y0 >= y1) return;
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            int idx = (y * WIDTH + x) * 4;
            buffer[idx] = rgb[0];
            buffer[idx + 1] = rgb[1];
            buffer[idx + 2] = rgb[2];
            buffer[idx + 3] = 255;
        }
    }
}

static void draw_border_px(unsigned char *buffer, int x0, int y0, int x1, int y1,
                           const unsigned char rgb[3], int thickness) {
    fill_rect_px(buffer, x0, y0, x1, y0 + thickness, rgb);
    fill_rect_px(buffer, x0, y1 - thickness, x1, y1, rgb);
    fill_rect_px(buffer, x0, y0, x0 + thickness, y1, rgb);
    fill_rect_px(buffer, x1 - thickness, y0, x1, y1, rgb);
}

static int footer_row_y_for_role(const FrameObject *obj, int cell_h) {
    int visible_rows;
    int footer_top;
    const char *chain;

    if (!obj || !obj->role[0]) {
        return -1;
    }
    if (cell_h <= 0) {
        return -1;
    }
    visible_rows = HEIGHT / cell_h;
    footer_top = visible_rows - 4;
    if (footer_top < 0) {
        footer_top = 0;
    }
    chain = obj->ancestor_chain[0] ? obj->ancestor_chain : obj->container_id;
    if (strcmp(obj->source_ref, "semantic:taskbar_banner") == 0 ||
        (chain && strcmp(chain, "wraith_root>taskbar") == 0)) {
        return footer_top;
    }
    if (strcmp(obj->role, "footer_band") == 0 || strcmp(obj->role, "taskbar_row") == 0 ||
        (chain && strstr(chain, "taskbar_row")) || (chain && strstr(chain, "footer_band"))) {
        return footer_top;
    }
    if (strcmp(obj->role, "summary_row") == 0 || (chain && strstr(chain, "summary_row"))) {
        return footer_top + 1;
    }
    if (strcmp(obj->role, "debug_row") == 0 || (chain && strstr(chain, "debug_row"))) {
        return footer_top + 2;
    }
    return -1;
}

static int focused_outline_is_red(const FrameObject *obj) {
    if (!obj || !obj->focused) {
        return 0;
    }
    if (strcmp(obj->role, "window_title") == 0) {
        return 0;
    }
    if (strcmp(obj->tag, "text") == 0) {
        return 1;
    }
    if (strcmp(obj->role, "launcher_row") == 0) {
        return 1;
    }
    return 0;
}

static void shade_rgb(const unsigned char in[3], unsigned char out[3], int pct) {
    int i;
    for (i = 0; i < 3; i++) {
        int v = ((int)in[i] * pct) / 100;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        out[i] = (unsigned char)v;
    }
}

static void draw_zslice_piece_preview(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    int w = x1 - x0;
    int h = y1 - y0;
    int cx = x0 + w / 2;
    int cy = y0 + h / 2;
    int size = (w < h ? w : h) / 2;
    int depth = size / 3;
    unsigned char top[3], side[3], front[3], outline[3] = {255, 209, 102};

    if (size < 24) size = 24;
    shade_rgb(obj->fg, top, 120);
    shade_rgb(obj->fg, side, 75);
    shade_rgb(obj->fg, front, 95);

    fill_rect_px(buffer, cx - size / 2 + depth, cy - size / 2 - depth, cx + size / 2 + depth, cy + size / 2 - depth, top);
    fill_rect_px(buffer, cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2, front);
    fill_rect_px(buffer, cx + size / 2, cy - size / 2, cx + size / 2 + depth, cy + size / 2 - depth, side);
    draw_border_px(buffer, cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2, outline, 2);
    draw_border_px(buffer, cx - size / 2 + depth, cy - size / 2 - depth, cx + size / 2 + depth, cy + size / 2 - depth, outline, 2);
    draw_border_px(buffer, cx + size / 2, cy - size / 2, cx + size / 2 + depth, cy + size / 2 - depth, outline, 2);
}

static void draw_tile_zmap_preview(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    FILE *f = fopen(obj->source_ref, "r");
    char rows[32][128];
    char line[256];
    int row_count = 0;
    int col_count = 0;
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    int tile_w, tile_h;
    int y, x;
    unsigned char grass[3] = {34, 139, 34};
    unsigned char wall[3] = {95, 112, 142};
    unsigned char tree[3] = {22, 130, 29};
    unsigned char stone[3] = {108, 131, 170};
    unsigned char gap[3] = {24, 35, 52};

    if (!f) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
        draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
        return;
    }
    while (row_count < 32 && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        snprintf(rows[row_count], sizeof(rows[row_count]), "%s", line);
        if ((int)strlen(line) > col_count) col_count = (int)strlen(line);
        row_count++;
    }
    fclose(f);
    if (row_count <= 0 || col_count <= 0) return;
    tile_w = (x1 - x0) / col_count;
    tile_h = (y1 - y0) / row_count;
    if (tile_w < 2) tile_w = 2;
    if (tile_h < 2) tile_h = 2;
    fill_rect_px(buffer, x0, y0, x1, y1, gap);
    for (y = 0; y < row_count; y++) {
        for (x = 0; rows[y][x] && x < col_count; x++) {
            unsigned char *rgb = grass;
            int px = x0 + x * tile_w;
            int py = y0 + y * tile_h;
            if (rows[y][x] == '#') rgb = wall;
            else if (rows[y][x] == 'T') rgb = tree;
            else if (rows[y][x] == 'R') rgb = stone;
            fill_rect_px(buffer, px + 1, py + 1, px + tile_w - 1, py + tile_h - 1, rgb);
        }
    }
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
}

static void draw_rgba_extrusion_preview(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    FILE *f = fopen(obj->source_ref, "r");
    char line[256];
    int values[4096][4];
    int count = 0;
    int resolution = 0;
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    int cell;
    int i;
    unsigned char bg[3] = {10, 16, 24};

    if (!f) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
        draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
        return;
    }
    while (fgets(line, sizeof(line), f) && count < 4096) {
        int r, g, b, a;
        if (line[0] == '#') {
            if (sscanf(line, "# resolution=%d", &resolution) == 1) {}
            continue;
        }
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            values[count][0] = r;
            values[count][1] = g;
            values[count][2] = b;
            values[count][3] = a;
            count++;
        }
    }
    fclose(f);
    if (resolution <= 0) {
        for (resolution = 1; resolution * resolution < count; resolution++) {}
    }
    if (resolution <= 0) return;
    cell = (x1 - x0) / resolution;
    if ((y1 - y0) / resolution < cell) cell = (y1 - y0) / resolution;
    if (cell < 2) cell = 2;
    fill_rect_px(buffer, x0, y0, x1, y1, bg);
    for (i = 0; i < count && i < resolution * resolution; i++) {
        int px_i = i % resolution;
        int py_i = i / resolution;
        int alpha = values[i][3];
        int height = alpha > 0 ? 3 + (alpha / 64) : 0;
        unsigned char rgb[3];
        int px = x0 + px_i * cell;
        int py = y0 + py_i * cell;
        if (alpha <= 0) continue;
        rgb[0] = (unsigned char)values[i][0];
        rgb[1] = (unsigned char)values[i][1];
        rgb[2] = (unsigned char)values[i][2];
        fill_rect_px(buffer, px + height, py - height, px + cell + height, py + cell - height, rgb);
        draw_border_px(buffer, px + height, py - height, px + cell + height, py + cell - height, obj->border, 1);
    }
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
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

static void sync_presenter_mode(void) {
    FILE *f = fopen(WRAITH_UI_STATE, "r");
    char line[512];
    g_presenter_ascii_mode = 0;
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        char *value;
        if (!eq) continue;
        *eq = '\0';
        value = eq + 1;
        while (*value && isspace((unsigned char)*value)) value++;
        value[strcspn(value, "\r\n")] = '\0';
        if (strcmp(line, "desktop_presenter_mode") == 0) {
            g_presenter_ascii_mode = (strcmp(value, "gl") != 0);
            break;
        }
    }
    fclose(f);
}

static int semantic_source_is_allowed(const SemanticSourceInfo *info) {
    if (!info || !info->valid) return 0;
    if (info->project_id[0] == '\0') return 0;
    if (strcmp(info->project_id, "wraith-pm") == 0) return 0;
    if (strcmp(info->project_id, "wraith-alpha") == 0) return 1;
    if (strncmp(info->project_id, "wraith/", 7) == 0) return 1;
    return 0;
}

static void write_rgb_receipt(
    const char *mode,
    const char *semantic_status,
    const SemanticSourceInfo *info,
    const FrameObject *objects,
    int object_count,
    int cell_w,
    int cell_h,
    const unsigned char bg[3],
    unsigned long long render_checksum
) {
    int i;
    FILE *f = fopen(RGB_RECEIPT_PATH, "w");
    if (!f) return;
    time_t now = time(NULL);
    struct tm tm_utc;
    char iso_time[32];
    long frame_mtime = file_mtime_epoch(SEMANTIC_META_PATH);
    long objects_mtime = file_mtime_epoch(SEMANTIC_OBJECTS_PATH);
    long receipt_mtime = file_mtime_epoch(RGB_RECEIPT_PATH);
    iso_time[0] = '\0';
    if (gmtime_r(&now, &tm_utc) != NULL) {
        strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    }
    fprintf(f, "receipt_type=rgb_presenter_audit\n");
    fprintf(f, "generated_by=wraith_rgb_daemon\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "generated_at_iso_utc=%s\n", iso_time[0] ? iso_time : "unknown");
    fprintf(f, "receipt_generation_key=%s@%ld\n", info && info->project_id[0] ? info->project_id : "unknown", (long)now);
    fprintf(f, "mode=%s\n", mode ? mode : "unknown");
    fprintf(f, "semantic_status=%s\n", semantic_status ? semantic_status : "unknown");
    fprintf(f, "source_frame_txt=pieces/display/current_frame.txt\n");
    fprintf(f, "source_objects_pdl=%s\n", SEMANTIC_OBJECTS_PATH);
    fprintf(f, "source_meta_pdl=%s\n", SEMANTIC_META_PATH);
    fprintf(f, "source_meta_mtime_epoch=%ld\n", frame_mtime);
    fprintf(f, "source_objects_mtime_epoch=%ld\n", objects_mtime);
    fprintf(f, "previous_receipt_mtime_epoch=%ld\n", receipt_mtime);
    fprintf(f, "output_rgba32=%s\n", WRAITH_FRAME_SOURCE);
    fprintf(f, "viewport_width_px=%d\n", WIDTH);
    fprintf(f, "viewport_height_px=%d\n", HEIGHT);
    fprintf(f, "cell_width_px=%d\n", cell_w);
    fprintf(f, "cell_height_px=%d\n", cell_h);
    fprintf(f, "glyph_width_px=%d\n", GLYPH_W);
    fprintf(f, "glyph_height_px=%d\n", GLYPH_H);
    fprintf(f, "canvas_origin_x=0\n");
    fprintf(f, "canvas_origin_y=0\n");
    fprintf(f, "render_origin=top_left\n");
    fprintf(f, "render_y_axis=down\n");
    fprintf(f, "text_anchor=cell_top_left\n");
    fprintf(f, "text_clipping=on\n");
    fprintf(f, "sort_order=ascending_z\n");
    fprintf(f, "background_rgb=#%02X%02X%02X\n", bg[0], bg[1], bg[2]);
    fprintf(f, "render_checksum_fnv1a64=0x%016llX\n", render_checksum);
    fprintf(f, "object_count=%d\n", object_count);
    if (info) {
        int mouse_visual_cell_x = (info->mouse_x / cell_w) + info->mouse_hit_offset_x;
        int mouse_visual_cell_y = (info->mouse_y / cell_h) + info->mouse_hit_offset_y;
        fprintf(f, "source_project_id=%s\n", info->project_id);
        fprintf(f, "source_layout=%s\n", info->source_layout);
        fprintf(f, "focused_object_id=%s\n", info->focused_object_id);
        fprintf(f, "focused_object_dom_id=%s\n", info->focused_object_dom_id);
        fprintf(f, "mouse_x=%d\n", info->mouse_x);
        fprintf(f, "mouse_y=%d\n", info->mouse_y);
        fprintf(f, "mouse_hit_offset_x=%d\n", info->mouse_hit_offset_x);
        fprintf(f, "mouse_hit_offset_y=%d\n", info->mouse_hit_offset_y);
        fprintf(f, "mouse_cursor_visual_uses_offset=%d\n", info->mouse_cursor_visual_uses_offset);
        fprintf(f, "mouse_visual_cell_x=%d\n", mouse_visual_cell_x);
        fprintf(f, "mouse_visual_cell_y=%d\n", mouse_visual_cell_y);
    }
    fprintf(f, "SECTION | OBJECTS | DERIVED_PIXEL_BOUNDS\n");
    for (i = 0; i < object_count; i++) {
        const FrameObject *obj = &objects[i];
        char fg_hex[8];
        char bg_hex[8];
        char border_hex[8];
        int x0 = obj->x * cell_w;
        int render_y = obj->y;
        int override_y = footer_row_y_for_role(obj, cell_h);
        int y0;
        int x1 = (obj->x + obj->w) * cell_w;
        int y1;
        int clip_x0 = x0 < 0 ? 0 : x0;
        int clip_y0;
        int clip_x1 = x1 > WIDTH ? WIDTH : x1;
        int clip_y1;
        int visible;
        const char *clip_reason = "none";
        char display_label[MAX_LABEL];
        int text_col;
        int text_row;
        int max_cols;
        int rendered_chars;
        int text_px_x0 = -1;
        int text_px_y0 = -1;
        int text_px_x1 = -1;
        int text_px_y1 = -1;
        int focus_px_x0 = -1;
        int focus_px_y0 = -1;
        int focus_px_x1 = -1;
        int focus_px_y1 = -1;
        const char *focus_rect_source = "none";

        if (override_y >= 0) {
            render_y = override_y;
        }
        y0 = render_y * cell_h;
        y1 = (render_y + obj->h) * cell_h;
        clip_y0 = y0 < 0 ? 0 : y0;
        clip_y1 = y1 > HEIGHT ? HEIGHT : y1;
        visible = (clip_x0 < clip_x1 && clip_y0 < clip_y1);

        if (x1 <= 0 || y1 <= 0 || x0 >= WIDTH || y0 >= HEIGHT) {
            clip_reason = "offscreen";
        } else if (clip_x0 != x0 || clip_y0 != y0 || clip_x1 != x1 || clip_y1 != y1) {
            clip_reason = "clipped_to_viewport";
        }

        color_to_hex(obj->fg, fg_hex);
        color_to_hex(obj->bg, bg_hex);
        color_to_hex(obj->border, border_hex);
        build_display_label(obj, display_label, sizeof(display_label));
        text_col = obj->x + ((strcmp(obj->tag, "text") == 0) ? 0 : 1);
        text_row = render_y;
        max_cols = obj->w - ((strcmp(obj->tag, "text") == 0) ? 0 : 2);
        if (max_cols < 0) max_cols = 0;
        rendered_chars = (int)strlen(display_label);
        if (max_cols >= 0 && rendered_chars > max_cols) {
            rendered_chars = max_cols;
        }
        if (display_label[0] != '\0' && rendered_chars > 0) {
            text_px_x0 = text_col * cell_w;
            text_px_y0 = text_row * cell_h;
            text_px_x1 = text_px_x0 + (rendered_chars * cell_w);
            text_px_y1 = text_px_y0 + GLYPH_H;
        }
        if (focused_outline_is_red(obj)) {
            focus_px_x0 = x0;
            focus_px_y0 = y0;
            focus_px_x1 = x1;
            focus_px_y1 = y1;
            focus_rect_source = "hit_rect";
        }
        fprintf(f, "OBJECT | %04d | tag=%s role=%s draw_index=%d z=%d focused=%d nav=%d nav_selected=%d nav_selector_glyph=%s parent_id=%s container_id=%s source_ref=%s ancestor_chain=%s clip_chain=%s x=%d y=%d render_y=%d w=%d h=%d px_x0=%d px_y0=%d px_x1=%d px_y1=%d clip_x0=%d clip_y0=%d clip_x1=%d clip_y1=%d visible=%d clip_reason=%s text_col=%d text_row=%d text_px_x0=%d text_px_y0=%d text_px_x1=%d text_px_y1=%d rendered_chars=%d hit_px_x0=%d hit_px_y0=%d hit_px_x1=%d hit_px_y1=%d focus_rect_source=%s focus_px_x0=%d focus_px_y0=%d focus_px_x1=%d focus_px_y1=%d fg=%s bg=%s border=%s label_core=%s label=%s action=%s\n",
            i + 1,
            obj->tag,
            obj->role,
            i,
            obj->z,
            obj->focused,
            obj->nav,
            obj->nav_selected,
            obj->nav_selector_glyph[0] ? obj->nav_selector_glyph : " ",
            obj->parent_id[0] ? obj->parent_id : "none",
            obj->container_id[0] ? obj->container_id : "none",
            obj->source_ref[0] ? obj->source_ref : "none",
            obj->ancestor_chain[0] ? obj->ancestor_chain : "none",
            obj->clip_chain[0] ? obj->clip_chain : "none",
            obj->x,
            obj->y,
            render_y,
            obj->w,
            obj->h,
            x0,
            y0,
            x1,
            y1,
            clip_x0,
            clip_y0,
            clip_x1,
            clip_y1,
            visible,
            clip_reason,
            text_col,
            text_row,
            text_px_x0,
            text_px_y0,
            text_px_x1,
            text_px_y1,
            rendered_chars,
            x0,
            y0,
            x1,
            y1,
            focus_rect_source,
            focus_px_x0,
            focus_px_y0,
            focus_px_x1,
            focus_px_y1,
            fg_hex,
            bg_hex,
            border_hex,
            obj->label_core,
            obj->label,
            obj->action);
    }
    fclose(f);
}

static int parse_frame_meta(unsigned char bg[3], int *cell_w, int *cell_h, SemanticSourceInfo *info) {
    FILE *f = fopen(SEMANTIC_META_PATH, "r");
    char line[1024];
    int loaded = 0;
    bg[0] = 15; bg[1] = 23; bg[2] = 32;
    *cell_w = 10;
    *cell_h = 18;
    if (info) {
        memset(info, 0, sizeof(*info));
    }
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "FRAME | cell_width_px |")) {
            *cell_w = atoi(strrchr(line, '|') + 1);
            loaded = 1;
        } else if (strstr(line, "FRAME | cell_height_px |")) {
            *cell_h = atoi(strrchr(line, '|') + 1);
            loaded = 1;
        } else if (info && strstr(line, "FRAME | project_id |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                strncpy(info->project_id, value, sizeof(info->project_id) - 1);
                info->project_id[strcspn(info->project_id, "\r\n")] = '\0';
                info->valid = 1;
            }
        } else if (info && strstr(line, "FRAME | source_layout |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                strncpy(info->source_layout, value, sizeof(info->source_layout) - 1);
                info->source_layout[strcspn(info->source_layout, "\r\n")] = '\0';
            }
        } else if (info && strstr(line, "FRAME | focused_object_id |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                strncpy(info->focused_object_id, value, sizeof(info->focused_object_id) - 1);
                info->focused_object_id[strcspn(info->focused_object_id, "\r\n")] = '\0';
            }
        } else if (info && strstr(line, "FRAME | focused_object_dom_id |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                strncpy(info->focused_object_dom_id, value, sizeof(info->focused_object_dom_id) - 1);
                info->focused_object_dom_id[strcspn(info->focused_object_dom_id, "\r\n")] = '\0';
            }
        } else if (info && strstr(line, "FRAME | mouse_x |")) {
            info->mouse_x = atoi(strrchr(line, '|') + 1);
        } else if (info && strstr(line, "FRAME | mouse_y |")) {
            info->mouse_y = atoi(strrchr(line, '|') + 1);
        } else if (info && strstr(line, "FRAME | mouse_hit_offset_x |")) {
            info->mouse_hit_offset_x = atoi(strrchr(line, '|') + 1);
        } else if (info && strstr(line, "FRAME | mouse_hit_offset_y |")) {
            info->mouse_hit_offset_y = atoi(strrchr(line, '|') + 1);
        } else if (info && strstr(line, "FRAME | mouse_cursor_visual_uses_offset |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                info->mouse_cursor_visual_uses_offset = (strncmp(value, "true", 4) == 0 || atoi(value) != 0);
            }
        }
    }
    fclose(f);
    return loaded;
}

static int parse_frame_objects(FrameObject objects[], int max_objects) {
    FILE *f = fopen(SEMANTIC_OBJECTS_PATH, "r");
    char line[2048];
    int count = 0;
    if (!f) return 0;
    while (fgets(line, sizeof(line), f) && count < max_objects) {
        FrameObject *obj;
        char value[256];
        char *label_start;
        char *action_start;
        if (strncmp(line, "OBJECT |", 8) != 0) continue;
        obj = &objects[count];
        memset(obj, 0, sizeof(*obj));
        strcpy(obj->tag, "text");
        strcpy(obj->role, "text");
        obj->fg[0] = 232; obj->fg[1] = 241; obj->fg[2] = 242;
        obj->bg[0] = 15; obj->bg[1] = 23; obj->bg[2] = 32;
        obj->border[0] = 126; obj->border[1] = 223; obj->border[2] = 242;
        if (kvp_value(line, "tag", value, sizeof(value))) strncpy(obj->tag, value, sizeof(obj->tag) - 1);
        if (kvp_value(line, "role", value, sizeof(value))) strncpy(obj->role, value, sizeof(obj->role) - 1);
        if (kvp_value(line, "nav", value, sizeof(value))) obj->nav = atoi(value);
        if (kvp_value(line, "nav_selected", value, sizeof(value))) obj->nav_selected = (strcmp(value, "true") == 0);
        if (kvp_value(line, "nav_selector_glyph", value, sizeof(value))) strncpy(obj->nav_selector_glyph, value, sizeof(obj->nav_selector_glyph) - 1);
        if (kvp_value(line, "parent_id", value, sizeof(value))) strncpy(obj->parent_id, value, sizeof(obj->parent_id) - 1);
        if (kvp_value(line, "container_id", value, sizeof(value))) strncpy(obj->container_id, value, sizeof(obj->container_id) - 1);
        if (kvp_value(line, "source_ref", value, sizeof(value))) strncpy(obj->source_ref, value, sizeof(obj->source_ref) - 1);
        if (kvp_value(line, "ancestor_chain", value, sizeof(value))) strncpy(obj->ancestor_chain, value, sizeof(obj->ancestor_chain) - 1);
        if (kvp_value(line, "clip_chain", value, sizeof(value))) strncpy(obj->clip_chain, value, sizeof(obj->clip_chain) - 1);
        if (kvp_value(line, "x", value, sizeof(value))) obj->x = atoi(value);
        if (kvp_value(line, "y", value, sizeof(value))) obj->y = atoi(value);
        if (kvp_value(line, "w", value, sizeof(value))) obj->w = atoi(value);
        if (kvp_value(line, "h", value, sizeof(value))) obj->h = atoi(value);
        if (kvp_value(line, "z", value, sizeof(value))) obj->z = atoi(value);
        if (kvp_value(line, "focused", value, sizeof(value))) obj->focused = (strcmp(value, "true") == 0);
        if (kvp_value(line, "fg", value, sizeof(value))) parse_hex_color(value, obj->fg);
        if (kvp_value(line, "bg", value, sizeof(value))) parse_hex_color(value, obj->bg);
        if (kvp_value(line, "border", value, sizeof(value))) parse_hex_color(value, obj->border);
        if (kvp_value(line, "label_core", value, sizeof(value))) strncpy(obj->label_core, value, sizeof(obj->label_core) - 1);
        label_start = strstr(line, "label=");
        if (label_start) {
            label_start += 6;
            action_start = strstr(label_start, " action=");
            if (!action_start) action_start = line + strlen(line);
            {
                size_t len = (size_t)(action_start - label_start);
                if (len >= sizeof(obj->label)) len = sizeof(obj->label) - 1;
                memcpy(obj->label, label_start, len);
                obj->label[len] = '\0';
            }
        }
        action_start = strstr(line, "action=");
        if (action_start) {
            char *src_start;
            action_start += 7;
            src_start = strstr(action_start, " src=");
            if (!src_start) src_start = line + strlen(line);
            {
                size_t len = (size_t)(src_start - action_start);
                if (len >= sizeof(obj->action)) len = sizeof(obj->action) - 1;
                memcpy(obj->action, action_start, len);
                obj->action[len] = '\0';
            }
        }
        count++;
    }
    fclose(f);
    return count;
}

static void sort_objects(FrameObject objects[], int count) {
    int i, j;
    for (i = 0; i < count - 1; i++) {
        for (j = i + 1; j < count; j++) {
            if (objects[j].z < objects[i].z) {
                FrameObject tmp = objects[i];
                objects[i] = objects[j];
                objects[j] = tmp;
            }
        }
    }
}

static void draw_object(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    int render_y = obj->y;
    int y_override = footer_row_y_for_role(obj, cell_h);
    int x0 = obj->x * cell_w;
    int y0;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1;
    unsigned char border_rgb[3];
    char display_label[MAX_LABEL];
    memcpy(border_rgb, obj->border, sizeof(border_rgb));
    if (focused_outline_is_red(obj)) {
        border_rgb[0] = 255; border_rgb[1] = 64; border_rgb[2] = 64;
    }

    if (y_override >= 0) {
        render_y = y_override;
    }
    y0 = render_y * cell_h;
    y1 = (render_y + obj->h) * cell_h;

    if (strcmp(obj->tag, "window") == 0 || strcmp(obj->tag, "panel") == 0 || strcmp(obj->tag, "header") == 0) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
        draw_border_px(buffer, x0, y0, x1, y1, border_rgb, 2);
    }
    if (strcmp(obj->tag, "surface") == 0) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
        draw_border_px(buffer, x0, y0, x1, y1, border_rgb, 2);
    }
    if (strcmp(obj->role, "zslice_piece") == 0) {
        draw_zslice_piece_preview(buffer, obj, cell_w, cell_h);
        return;
    }
    if (strcmp(obj->role, "tile_zmap") == 0) {
        draw_tile_zmap_preview(buffer, obj, cell_w, cell_h);
        return;
    }
    if (strcmp(obj->role, "rgba_extrusion") == 0) {
        draw_rgba_extrusion_preview(buffer, obj, cell_w, cell_h);
        return;
    }

    build_display_label(obj, display_label, sizeof(display_label));
    if (display_label[0] != '\0') {
        int text_col = obj->x + ((strcmp(obj->tag, "text") == 0) ? 0 : 1);
        int text_row = render_y;
        int max_cols = obj->w - ((strcmp(obj->tag, "text") == 0) ? 0 : 2);
        if (max_cols < 0) max_cols = 0;
        blit_text(buffer, text_col, text_row, display_label, obj->fg, max_cols, cell_w, cell_h);
    }

    if (focused_outline_is_red(obj)) {
        unsigned char focus_rgb[3] = {255, 64, 64};
        draw_border_px(buffer, x0, y0, x1, y1, focus_rgb, 1);
    }
}

static int render_semantic_frame(unsigned char *buffer) {
    FrameObject objects[MAX_OBJECTS];
    unsigned char bg[3];
    int cell_w, cell_h;
    int count, i;
    SemanticSourceInfo info;
    unsigned long long checksum;
    parse_frame_meta(bg, &cell_w, &cell_h, &info);
    if (!semantic_source_is_allowed(&info)) {
        write_rgb_receipt("gl", "rejected_non_wraith_semantic_source", &info, NULL, 0, cell_w, cell_h, bg, 0);
        return 0;
    }
    count = parse_frame_objects(objects, MAX_OBJECTS);
    if (count <= 0) {
        write_rgb_receipt("gl", "rejected_empty_semantic_scene", &info, NULL, 0, cell_w, cell_h, bg, 0);
        return 0;
    }
    clear_buffer(buffer, bg[0], bg[1], bg[2]);
    sort_objects(objects, count);
    for (i = 0; i < count; i++) {
        draw_object(buffer, &objects[i], cell_w, cell_h);
    }
    checksum = checksum_buffer(buffer, WIDTH * HEIGHT * 4);
    write_rgb_receipt("gl", "accepted_semantic_scene", &info, objects, count, cell_w, cell_h, bg, checksum);
    return 1;
}

static void render_ascii_frame(const char *frame_path, unsigned char *buffer) {
    FILE *f;
    char line[1024];
    int row = 0;
    clear_buffer(buffer, 0, 0, 68);
    f = fopen(frame_path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f) && row < ROWS) {
        int col;
        unsigned char r = 200, g = 200, b = 200;
        if (strstr(line, "[>]")) {
            r = 0; g = 255; b = 255;
        }
        for (col = 0; col < (int)strlen(line) && col < COLS; col++) {
            unsigned char c = (unsigned char)line[col];
            if (c == '\n' || c == '\r') break;
            blit_char(buffer, col, row, c, r, g, b, GLYPH_W, GLYPH_H);
        }
        row++;
    }
    fclose(f);
}

static void pulse_rgb(void) {
    FILE *f = fopen("projects/wraith-alpha/session/rgb/rgb_frame_changed.txt", "a");
    if (f) {
        fprintf(f, "P\n");
        fclose(f);
    }
}

int main(void) {
    struct stat st;
    off_t last_size = 0;
    const char *trigger = "pieces/display/frame_changed.txt";
    const char *frame_src = "pieces/display/current_frame.txt";
    const char *output = "projects/wraith-alpha/session/rgb/current_frame.rgba32";
    unsigned char *buffer;

    printf("[RGB-DAEMON] Starting Wraith RGB converter...\n");
    load_glyphs();
    buffer = malloc(WIDTH * HEIGHT * 4);
    if (!buffer) return 1;

    if (stat(trigger, &st) == 0) last_size = st.st_size;

    while (1) {
        static int rendered_initial_frame = 0;
        int dirty = !rendered_initial_frame;
        FILE *f;
        if (stat(trigger, &st) == 0 && st.st_size != last_size) {
            last_size = st.st_size;
            dirty = 1;
        }
        if (!dirty) {
            usleep(16667);
            continue;
        }

        sync_presenter_mode();
        if (g_presenter_ascii_mode) {
            render_ascii_frame(frame_src, buffer);
            {
                unsigned char ascii_bg[3] = {0, 0, 68};
                unsigned long long checksum = checksum_buffer(buffer, WIDTH * HEIGHT * 4);
                write_rgb_receipt("ascii", "ascii_frame_rendered", NULL, NULL, 0, 10, 18, ascii_bg, checksum);
            }
        } else if (!render_semantic_frame(buffer)) {
            render_ascii_frame(frame_src, buffer);
            {
                unsigned char fallback_bg[3] = {0, 0, 68};
                unsigned long long checksum = checksum_buffer(buffer, WIDTH * HEIGHT * 4);
                write_rgb_receipt("gl", "fallback_to_ascii_frame", NULL, NULL, 0, 10, 18, fallback_bg, checksum);
            }
        }

        f = fopen(output, "wb");
        if (f) {
            fwrite(buffer, 1, WIDTH * HEIGHT * 4, f);
            fclose(f);
            pulse_rgb();
        }
        rendered_initial_frame = 1;
        usleep(16667);
    }

    free(buffer);
    return 0;
}
