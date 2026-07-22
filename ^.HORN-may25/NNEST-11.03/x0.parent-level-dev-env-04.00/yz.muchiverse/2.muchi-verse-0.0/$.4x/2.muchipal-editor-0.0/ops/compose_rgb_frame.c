/* compose_rgb_frame - one verb, one binary, no shared headers.
 * muchipal-editor's GL/RGB mirror content half - added 2026-07-21,
 * porting the same "mirror" pattern already proven in all four $.4x
 * game projects (civ-pal/angler-empires/pal-craft/muchimon-pal each
 * have their own compose_rgb_frame.c + system/gl_mirror.c pair; this
 * editor previously had NEITHER). Per those projects' own GOVERNING
 * CONSTRAINT (2.muchi-verse/GRAND-ARCHITECTURE.md): this file makes
 * ZERO GL calls, direct or indirect - plain C that computes pixel
 * colors and writes pieces/display/rgb_frame.raw; system/gl_mirror.c
 * (copied from muchimon-pal, project-agnostic infrastructure) is the
 * only thing that touches GL, blitting whatever this op wrote.
 *
 * Deliberately much leaner than the 4 game projects' own copies of
 * this file (which inherited full 3D voxel-raymarch machinery from
 * pal-craft's own needs) - this editor only ever needs a flat top-down
 * tile grid, so the raymarch/voxel/emoji code from those files is not
 * ported here at all, just the flat-tile-blit + font-blit core.
 *
 * Reads pieces/system/editor_state.txt (the SAME state ops/
 * compose_title_frame.c's ASCII renderer already reads) and mirrors
 * whichever screen is active:
 *   - non-map_edit screens: header/footer text only, no tile grid
 *     (nothing to show - these are menu screens, not a map viewport).
 *   - map_edit: the same camera-clamped viewport compose_title_frame.c
 *     now computes (see that file's own 2026-07-21 camera-fix comment
 *     for why this was a real, live bug, not just a missing feature -
 *     pal-craft's own map is already wider than the ASCII box), each
 *     tile's glyph resolved to an RGB color via the current project's
 *     own registry (pipe format's rgb_top field, "R,G,B"; equals
 *     format falls back to a flat gray - piececraft-3d-pal's own
 *     per-tile-file color isn't ported here yet, a real, named gap).
 *     The cursor tile is drawn with a bright highlight border instead
 *     of ASCII's bracket characters, since there's no character grid
 *     to bracket in the pixel version. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define VIEWPORT_W 40
#define VIEWPORT_H 16
#define MAX_MAP_W 256
#define MAX_MAP_H 256
#define TILE_PX 16
#define GLYPH_W 8
#define GLYPH_H 16
#define HEADER_ROWS 1
#define FOOTER_ROWS 2
#define FRAME_W (VIEWPORT_W * TILE_PX)
#define FRAME_H (HEADER_ROWS * GLYPH_H + VIEWPORT_H * TILE_PX + FOOTER_ROWS * GLYPH_H)
#define MAX_TEXT_COLS (FRAME_W / GLYPH_W)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

/* glyphs[c] is an 8x16 on/off mask, loaded from pieces/registry/fonts/
 * ascii/<c>/glyph.txt - copied verbatim from muchimon-pal's own asset
 * set (font bitmaps are project-agnostic, not regenerated here). */
static unsigned char glyphs[127][GLYPH_H][GLYPH_W];

static void load_glyphs(void) {
    memset(glyphs, 0, sizeof(glyphs));
    for (int c = 32; c < 127; c++) {
        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/pieces/registry/fonts/ascii/%d/glyph.txt", project_root, c);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[64];
        int y = 0;
        while (y < GLYPH_H && fgets(line, sizeof(line), f)) {
            for (int x = 0; x < GLYPH_W && line[x] != '\0' && line[x] != '\n'; x++) {
                glyphs[c][y][x] = (line[x] == '#') ? 1 : 0;
            }
            y++;
        }
        fclose(f);
    }
}

static void blit_char(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, unsigned char c,
                       unsigned char r, unsigned char g, unsigned char b) {
    if (c < 32 || c > 126) return;
    for (int y = 0; y < GLYPH_H; y++) {
        int fy = py + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < GLYPH_W; x++) {
            int fx = px + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            if (!glyphs[c][y][x]) continue;
            fb[fy][fx][0] = r; fb[fy][fx][1] = g; fb[fy][fx][2] = b; fb[fy][fx][3] = 255;
        }
    }
}

static void blit_text(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, const char *text,
                       unsigned char r, unsigned char g, unsigned char b) {
    int col = 0;
    for (const char *p = text; *p && col < MAX_TEXT_COLS; p++, col++) {
        blit_char(fb, px + col * GLYPH_W, py, (unsigned char)*p, r, g, b);
    }
}

static void fill_tile(unsigned char fb[FRAME_H][FRAME_W][4], int tile_x0, int tile_y0,
                      unsigned char r, unsigned char g, unsigned char b) {
    for (int y = 0; y < TILE_PX; y++) {
        int fy = tile_y0 + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < TILE_PX; x++) {
            int fx = tile_x0 + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            fb[fy][fx][0] = r; fb[fy][fx][1] = g; fb[fy][fx][2] = b; fb[fy][fx][3] = 255;
        }
    }
}

static void outline_tile(unsigned char fb[FRAME_H][FRAME_W][4], int tile_x0, int tile_y0,
                          unsigned char r, unsigned char g, unsigned char b) {
    for (int x = 0; x < TILE_PX; x++) {
        int fx = tile_x0 + x;
        if (fx < 0 || fx >= FRAME_W) continue;
        if (tile_y0 >= 0 && tile_y0 < FRAME_H) { fb[tile_y0][fx][0] = r; fb[tile_y0][fx][1] = g; fb[tile_y0][fx][2] = b; fb[tile_y0][fx][3] = 255; }
        int by = tile_y0 + TILE_PX - 1;
        if (by >= 0 && by < FRAME_H) { fb[by][fx][0] = r; fb[by][fx][1] = g; fb[by][fx][2] = b; fb[by][fx][3] = 255; }
    }
    for (int y = 0; y < TILE_PX; y++) {
        int fy = tile_y0 + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        if (tile_x0 >= 0 && tile_x0 < FRAME_W) { fb[fy][tile_x0][0] = r; fb[fy][tile_x0][1] = g; fb[fy][tile_x0][2] = b; fb[fy][tile_x0][3] = 255; }
        int bx = tile_x0 + TILE_PX - 1;
        if (bx >= 0 && bx < FRAME_W) { fb[fy][bx][0] = r; fb[fy][bx][1] = g; fb[fy][bx][2] = b; fb[fy][bx][3] = 255; }
    }
}

/* Looks up glyph's rgb_top (or rgb_top_emoji, field 6, when emoji_mode
 * is set - added 2026-07-21, matching mutaclsym/muchimon-pal's own
 * emoji_mode field, see this op's own header comment for the reference
 * investigation) in a "pipe" registry (glyph|id|name|walkable|rgb_top|
 * unicode|rgb_top_emoji, both color fields "R,G,B"). Returns 0
 * (fallback gray applied by caller) if not found or registry_format
 * isn't "pipe" - "equals" format (piececraft-3d-pal's per-id
 * *.tile.txt files) isn't wired up here yet, a real, named gap matching
 * the ASCII renderer's own equals-format handling (legend text only,
 * no per-tile color). Deliberately color-only, not a glyph swap - the
 * ASCII renderer's fixed-width char grid would need a parallel per-cell
 * string buffer (same shape as muchimon-pal's own cell_emoji[][
 * EMOJI_BUF]) to safely show a multi-byte unicode glyph, a real,
 * separate, harder follow-up not done this pass. */
static int glyph_rgb_top(const char *reg_path, char glyph, int emoji_mode, int *r, int *g, int *b) {
    FILE *f = fopen(reg_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    int target_field = emoji_mode ? 6 : 4;
    while (fgets(line, sizeof(line), f)) {
        /* A real comment line always has a SPACE right after '#' (this
         * registry's own header rows are "# free text"); a real data
         * row has '|' immediately after a one-char glyph. Checking
         * line[1]!='|' first (not just line[0]=='#') is what keeps '#'
         * itself usable as a glyph - mutaclsym's own t_wall row is
         * exactly that. This is the SAME bug class documented in
         * ops/map_edit_input.c's own load_registry_rows() (see that
         * file's header comment - found and fixed there already this
         * session); this file had an independent, unfixed copy of it
         * until caught live: emoji-color toggle testing showed BOTH
         * modes silently falling back to the same gray default because
         * the wall row was being skipped as a "comment" the whole
         * time, not because the emoji lookup itself was wrong. */
        if (line[0] == '\n') continue;
        if (line[0] == '#' && line[1] != '|') continue;
        if (line[0] != glyph || line[1] != '|') continue;
        char *p = line;
        for (int field = 0; field < target_field && p; field++) p = strchr(p + 1, '|');
        if (!p) continue;
        p++;
        int rr = 0, gg = 0, bb = 0;
        if (sscanf(p, "%d,%d,%d", &rr, &gg, &bb) == 3) {
            *r = rr; *g = gg; *b = bb; found = 1;
        }
        break;
    }
    fclose(f);
    return found;
}

/* Event overlay (added 2026-07-21) - own copy of ops/compose_title_
 * frame.c's load_event_coords(), same "no shared headers" convention;
 * see ops/map_edit_input.c's own event-editor header comment for the
 * full reasoning. */
#define MAX_EVENT_MARKERS 256
static int load_event_coords(const char *proj_path, const char *map_rel_path, int xs[MAX_EVENT_MARKERS], int ys[MAX_EVENT_MARKERS]) {
    const char *p = strstr(map_rel_path, "pieces/");
    if (!p) return 0;
    p += 7;
    const char *slash = strchr(p, '/');
    if (!slash) return 0;
    char world_dir[64];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(world_dir, sizeof(world_dir), "%.*s", (int)(slash - p), p);
#pragma GCC diagnostic pop
    char ev_path[PATH_BUF + 256];
    snprintf(ev_path, sizeof(ev_path), "%s/pieces/%s/map_start/events.txt", proj_path, world_dir);
    FILE *f = fopen(ev_path, "r");
    if (!f) return 0;
    int n = 0;
    char line_buf[MAX_LINE];
    while (n < MAX_EVENT_MARKERS && fgets(line_buf, sizeof(line_buf), f)) {
        line_buf[strcspn(line_buf, "\r\n")] = '\0';
        if (!line_buf[0] || line_buf[0] == '#') continue;
        int x = 0, y = 0;
        char op_id[32];
        if (sscanf(line_buf, "%d|%d|%31[^|\n]", &x, &y, op_id) == 3) { xs[n] = x; ys[n] = y; n++; }
    }
    fclose(f);
    return n;
}

static int has_event_at(int xs[], int ys[], int n, int x, int y) {
    for (int i = 0; i < n; i++) if (xs[i] == x && ys[i] == y) return 1;
    return 0;
}

static uint64_t checksum_buffer(const unsigned char *buf, size_t len) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; i++) { h ^= buf[i]; h *= 1099511628211ULL; }
    return h;
}

int main(void) {
    resolve_root();
    load_glyphs();

    char state_path[PATH_BUF], out_path[PATH_BUF], receipt_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/editor_state.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);

    char screen[16], proj_name[64], proj_path[PATH_BUF];
    read_kv_str(state_path, "screen", screen, sizeof(screen), "title");
    read_kv_str(state_path, "proj_name", proj_name, sizeof(proj_name), "");
    read_kv_str(state_path, "proj_path", proj_path, sizeof(proj_path), "");

    static unsigned char fb[FRAME_H][FRAME_W][4];
    memset(fb, 0, sizeof(fb));

    char hdr[96];
    snprintf(hdr, sizeof(hdr), "MUCHIPAL-EDITOR  screen=%s", screen);
    blit_text(fb, 4, 0, hdr, 255, 255, 0);

    if (strcmp(screen, "map_edit") == 0) {
        char map_rel_path[256], registry_format[16], registry_rel_path[256];
        read_kv_str(state_path, "map_rel_path", map_rel_path, sizeof(map_rel_path), "");
        read_kv_str(state_path, "registry_format", registry_format, sizeof(registry_format), "pipe");
        read_kv_str(state_path, "registry_rel_path", registry_rel_path, sizeof(registry_rel_path), "");
        int cursor_x = read_kv_int(state_path, "cursor_x", 0);
        int cursor_y = read_kv_int(state_path, "cursor_y", 0);
        int emoji_mode = read_kv_int(state_path, "emoji_mode", 0);

        char map_path[PATH_BUF + 256], reg_path[PATH_BUF + 256];
        snprintf(map_path, sizeof(map_path), "%s/%s", proj_path, map_rel_path);
        snprintf(reg_path, sizeof(reg_path), "%s/%s", proj_path, registry_rel_path);
        int is_pipe = (strcmp(registry_format, "pipe") == 0);

        char grid[MAX_MAP_H][MAX_MAP_W + 1];
        int map_w = 0, map_h = 0;
        FILE *mf = fopen(map_path, "r");
        if (mf) {
            while (map_h < MAX_MAP_H && fgets(grid[map_h], sizeof(grid[map_h]), mf)) {
                grid[map_h][strcspn(grid[map_h], "\r\n")] = '\0';
                int len = (int)strlen(grid[map_h]);
                if (len > map_w) map_w = len;
                map_h++;
            }
            fclose(mf);
        }

        int cam_x = cursor_x - VIEWPORT_W / 2;
        int cam_x_max = map_w - VIEWPORT_W;
        if (cam_x_max < 0) cam_x_max = 0;
        if (cam_x < 0) cam_x = 0;
        if (cam_x > cam_x_max) cam_x = cam_x_max;
        int cam_y = cursor_y - VIEWPORT_H / 2;
        int cam_y_max = map_h - VIEWPORT_H;
        if (cam_y_max < 0) cam_y_max = 0;
        if (cam_y < 0) cam_y = 0;
        if (cam_y > cam_y_max) cam_y = cam_y_max;

        int event_xs[MAX_EVENT_MARKERS], event_ys[MAX_EVENT_MARKERS];
        int event_count = load_event_coords(proj_path, map_rel_path, event_xs, event_ys);

        for (int row = 0; row < VIEWPORT_H; row++) {
            int src_row = cam_y + row;
            for (int col = 0; col < VIEWPORT_W; col++) {
                int src_col = cam_x + col;
                char glyph = (src_row >= 0 && src_row < map_h && src_col >= 0 && src_col < (int)strlen(grid[src_row]))
                             ? grid[src_row][src_col] : ' ';
                int r = 90, g = 90, b = 90;
                if (is_pipe && glyph != ' ') glyph_rgb_top(reg_path, glyph, emoji_mode, &r, &g, &b);
                int tx = col * TILE_PX, ty = HEADER_ROWS * GLYPH_H + row * TILE_PX;
                fill_tile(fb, tx, ty, (unsigned char)r, (unsigned char)g, (unsigned char)b);
                /* Event marker overlay - a small inset magenta square,
                 * distinct from any real terrain color, so a placed
                 * event is never mistaken for ordinary ground (and the
                 * underlying terrain color is still partly visible). */
                if (has_event_at(event_xs, event_ys, event_count, src_col, src_row)) {
                    for (int ey = 4; ey < TILE_PX - 4; ey++) {
                        int fy = ty + ey;
                        if (fy < 0 || fy >= FRAME_H) continue;
                        for (int ex = 4; ex < TILE_PX - 4; ex++) {
                            int fx = tx + ex;
                            if (fx < 0 || fx >= FRAME_W) continue;
                            fb[fy][fx][0] = 255; fb[fy][fx][1] = 0; fb[fy][fx][2] = 255; fb[fy][fx][3] = 255;
                        }
                    }
                }
                if (src_row == cursor_y && src_col == cursor_x)
                    outline_tile(fb, tx, ty, 255, 255, 255);
            }
        }
    } else {
        blit_text(fb, 4, HEADER_ROWS * GLYPH_H + GLYPH_H, "(no map viewport on this screen)", 180, 180, 180);
    }

    char footer[MAX_TEXT_COLS + 32];
    if (strcmp(screen, "map_edit") == 0) {
        int armed_idx = read_kv_int(state_path, "armed_idx", 0);
        int emoji_mode = read_kv_int(state_path, "emoji_mode", 0);
        char arm_mode[8];
        read_kv_str(state_path, "arm_mode", arm_mode, sizeof(arm_mode), "terrain");
        snprintf(footer, sizeof(footer), "[ACTIVE:%.7s] %.12s armed=%d emoji=%s [b]bank [v]event [e]emoji",
                 arm_mode, proj_name, armed_idx + 1, emoji_mode ? "ON" : "OFF");
    } else {
        snprintf(footer, sizeof(footer), "[NAV] %.20s  [0-9]jump [enter]select [q]quit", proj_name);
    }
    blit_text(fb, 4, FRAME_H - GLYPH_H, footer, 200, 200, 255);

    FILE *out = fopen(out_path, "wb");
    if (!out) return 1;
    fwrite(fb, 1, sizeof(fb), out);
    fclose(out);

    uint64_t sum = checksum_buffer((unsigned char *)fb, sizeof(fb));
    FILE *rf = fopen(receipt_path, "w");
    if (rf) {
        fprintf(rf, "frame_w=%d\n", FRAME_W);
        fprintf(rf, "frame_h=%d\n", FRAME_H);
        fprintf(rf, "byte_count=%zu\n", sizeof(fb));
        fprintf(rf, "checksum_fnv1a64=%llu\n", (unsigned long long)sum);
        fclose(rf);
    }
    return 0;
}
