/* compose_rgb_frame - one verb, one binary, no shared headers.
 * The GL/RGB mirror's content half: reads the exact same on-disk state
 * ops/compose_frame.c already reads (map.txt/furniture.txt/hero/items/
 * monsters, same camera-clamp formula, byte-for-byte copied from there)
 * and writes a raw RGBA32 framebuffer instead of an ASCII viewport -
 * pieces/display/rgb_frame.raw. Per
 * 2.muchi-verse/GRAND-ARCHITECTURE.md's GOVERNING CONSTRAINT: this file
 * makes ZERO GL calls of any kind, direct or indirect - it is plain,
 * portable, RISC-V-compilable-in-principle C that computes pixel color
 * values and writes them to a file, exactly the shape real 1.TPMOS
 * wraith_rgb_daemon.c already established (that file does the same job
 * for piececraft-wraith via a CPU voxel raymarch; this one is far
 * simpler - flat per-tile top-face color, no raymarch, since tempt-island
 * has no z-axis yet). system/gl_mirror.c (a separate, later file) is
 * the only thing allowed to touch GL, and its only job is to blit
 * whatever this op wrote.
 *
 * Per direct user instruction ("u said u have no eyes on the screen...
 * do the same [as wraith-gl's receipts]"), this op writes its own
 * receipt - pieces/display/rgb_frame.receipt.txt - with dimensions,
 * byte count, and an FNV-1a-64 checksum (same algorithm wraith_gl.c's
 * checksum_buffer() uses) so correctness can be verified by reading a
 * text file, not by looking at a window.
 *
 * Font/text (per direct follow-up instruction: "wraith-gl also has a
 * way to render fonts/nav that we should also have"). wraith_gl.c
 * itself has NO text-drawing code - the actual font pipeline lives in
 * wraith_rgb_daemon.c (load_glyphs()/blit_char()/blit_text()/
 * draw_text_asset(), lines ~401-560), which is exactly the right place
 * per the GOVERNING CONSTRAINT: text gets stamped into the CPU-computed
 * RGB buffer as plain pixel writes, never drawn via a GL text API. The
 * glyphs themselves are pre-generated once offline (real 1.TPMOS's
 * ops/font-gen-op.c, using FreeType, extracts each ASCII 32-126
 * character from a TTF into an 8x16 grid of #/. - see
 * pieces/registry/fonts/ascii/<code>/glyph.txt, copied verbatim from
 * wraith-alpha/assets/fonts/ascii/ rather than regenerated, same
 * "reuse the pre-extracted asset" precedent piececraft-wraith's own
 * voxel CSVs already established). load_glyphs()/blit_char()/
 * blit_text() below are a direct port of that same shape. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
/* Same viewport size as compose_frame.c's VIEWPORT_W/H - the two
 * renderers show the same window onto the same map, one in text, one
 * in pixels, matching the README.md "mirror" definition exactly: one
 * state, multiple renderers. */
#define VIEWPORT_W 40
#define VIEWPORT_H 16
#define MAX_MAP_W 256
#define MAX_MAP_H 256
/* Each map tile becomes a TILE_PX x TILE_PX solid-color block in the
 * framebuffer - flat top-face color, no raymarch (tempt-island has no
 * z-axis/voxels yet, unlike piececraft-wraith). 16 keeps the output a
 * reasonable 640x256 texture without pretending to be more detailed
 * than the source data actually is. */
#define TILE_PX 16
/* Same 8x16 monospace cell size as real 1.TPMOS's font pipeline
 * (wraith_rgb_daemon.c's GLYPH_W/GLYPH_H) - matches glyph.txt's own
 * row/column count exactly, so load_glyphs() below needs no scaling. */
#define GLYPH_W 8
#define GLYPH_H 16
/* One text row above the tile grid (map id + turn) and TWO below (HP/
 * hunger/thirst/stamina, then the action-bar choice footer) - now
 * matches compose_frame.c's own two-line HUD footer exactly (build_
 * action_footer() below is a direct copy of that file's own build_
 * choice_footer()). Was 1 footer row/no action-bar text in an earlier
 * pass ("proving the font pipeline works end-to-end... in this first
 * pass") - the real gap that left, and the reason bumping to 2 rows
 * here, not shared/reused constants: MAX_TEXT_COLS's own truncation and
 * this whole file's "no shared headers" convention (see
 * build_action_footer()'s own header comment). message log tail is
 * still NOT mirrored here - out of scope for this pass, same "one thing
 * at a time" reasoning as before. */
#define HEADER_ROWS 1
#define FOOTER_ROWS 2
#define FRAME_W (VIEWPORT_W * TILE_PX)
#define FRAME_H (HEADER_ROWS * GLYPH_H + VIEWPORT_H * TILE_PX + FOOTER_ROWS * GLYPH_H)
#define MAX_TEXT_COLS (FRAME_W / GLYPH_W)
#define MAX_RECIPES_DISPLAY 16
#define BOX_TEXT_W 48
/* Matches item_id/monster_type's own 64-byte buffers exactly (both
 * flow into this one via cell_asset[]) - gcc can't prove a shorter
 * bound is safe from their own declared sizes alone, and every real id
 * in this project's registries is well under 32 anyway, but matching
 * the source buffers' own size removes any real truncation risk margin
 * rather than just suppressing the warning. */
#define ASSET_ID_BUF 64

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

static double read_kv_double(const char *path, const char *key, double def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    double val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atof(eq + 1); break; }
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
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

static void item_registry_field(const char *item_id, int field_index, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/items/items.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(item_id) || strncmp(line, item_id, p1 - line) != 0) continue;
        char *field = p1 + 1;
        for (int i = 1; i < field_index && field; i++) {
            field = strchr(field, '|');
            if (field) field++;
        }
        if (!field) break;
        char *end = strchr(field, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", field);
        break;
    }
    fclose(f);
}

/* Direct copy of compose_frame.c's own count_in_inventory() - same
 * "this op only reads, doesn't consume" reasoning for the craft panel's
 * ready/missing status. */
static int count_in_inventory(const char *item_id) {
    char inventory_dir[PATH_BUF];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/world_tempt_island/map_start/hero/inventory", project_root);
    DIR *d = opendir(inventory_dir);
    if (!d) return 0;
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
        char id[64];
        read_kv_str(state_path, "item_id", id, sizeof(id), "?");
        if (strcmp(id, item_id) == 0) count++;
    }
    closedir(d);
    return count;
}

static void monster_registry_field(const char *monster_type, int field_index, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
        char *field = p1 + 1;
        for (int i = 1; i < field_index && field; i++) {
            field = strchr(field, '|');
            if (field) field++;
        }
        if (!field) break;
        char *end = strchr(field, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", field);
        break;
    }
    fclose(f);
}

/* Looks up rgb_top(R,G,B) for a glyph in a terrain/furniture-shaped
 * registry (glyph|id|name|walkable|rgb_top|unicode|rgb_top_emoji).
 * Returns 1 and fills r/g/b if found, 0 otherwise - caller tries
 * terrain then furniture, since the two registries' glyphs are
 * disjoint by construction (the same assumption compose_frame.c's own
 * merged grid already relies on: one glyph per cell, furniture wins
 * over terrain when present).
 *
 * emoji_mode, when set, prefers the row's rgb_top_emoji field instead
 * (falling back to plain rgb_top if that row has none). This is now
 * only the BASE/background layer under the real rasterized emoji pixels
 * blitted on top by blit_emoji_tile() below (see that function's own
 * header comment for the real pipeline) - it still matters on its own
 * for two real reasons: it shows through any transparent pixels in the
 * emoji glyph itself (most emoji don't fill their full 16x16 cell), and
 * it's the correct fallback for any tile whose asset failed to
 * generate/load. */
static int glyph_rgb_top(const char *registry_rel_path, char glyph, int emoji_mode, int *r, int *g, int *b) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        /* See ops/move_player.c's glyph_walkable() for why line[1]=='|'
         * (not just line[0]=='#') is the real comment test - '#' is
         * itself a valid glyph (t_wall). This exact check, done wrong,
         * is what first surfaced as every wall rendering magenta in
         * the debug PNG - the bug existed in move_player.c/
         * tick_monsters.c first (harmless there by coincidence), this
         * file just made it visible. */
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        char *p = strchr(line, '|'); /* -> id */
        if (!p) continue;
        p = strchr(p + 1, '|'); /* -> name */
        if (!p) continue;
        p = strchr(p + 1, '|'); /* -> walkable */
        if (!p) continue;
        p = strchr(p + 1, '|'); /* -> rgb_top */
        if (!p) continue;
        int base_found = (sscanf(p + 1, "%d,%d,%d", r, g, b) == 3);
        if (emoji_mode) {
            char *pe = strchr(p + 1, '|'); /* -> unicode */
            if (pe) pe = strchr(pe + 1, '|'); /* -> rgb_top_emoji */
            if (pe && sscanf(pe + 1, "%d,%d,%d", r, g, b) == 3) { found = 1; break; }
        }
        found = base_found;
        break;
    }
    fclose(f);
    return found;
}

/* Glyphs not covered by either registry (hero, ground items, monsters -
 * none of which have an rgb_top field yet, a known v0 limitation
 * documented in GRAND-ARCHITECTURE.md) get an obvious fallback color
 * rather than silently defaulting to black/floor - magenta reads as
 * "unmapped" at a glance the same way missing-texture magenta/black
 * checkerboards do in real engines. Hero '@' and the xlector cursor 'X'
 * (real active-target pattern, see ops/choice.c's own header comment)
 * both get their own fixed colors since they're always-present,
 * always-interesting special cases, not genuinely "unmapped" content -
 * cyan for the cursor, distinct from both the hero's yellow and the
 * generic magenta fallback. */
static void glyph_fallback_rgb(char glyph, int *r, int *g, int *b) {
    if (glyph == '@') { *r = 255; *g = 255; *b = 0; return; }
    if (glyph == 'X') { *r = 0; *g = 255; *b = 255; return; }
    if (glyph == ' ' || glyph == '\0') { *r = 0; *g = 0; *b = 0; return; }
    *r = 255; *g = 0; *b = 255;
}

static void glyph_to_rgb(char glyph, int emoji_mode, int *r, int *g, int *b) {
    if (glyph_rgb_top("pieces/registry/terrain/terrain_types.txt", glyph, emoji_mode, r, g, b)) return;
    if (glyph_rgb_top("pieces/registry/furniture/furniture_types.txt", glyph, emoji_mode, r, g, b)) return;
    glyph_fallback_rgb(glyph, r, g, b);
}

/* Same registry-scan shape as glyph_rgb_top() above, but stops after
 * the FIRST pipe to extract the row's own `id` field (glyph|id|...)
 * instead of walking further for rgb_top - this is the asset-directory
 * name blit_emoji_tile() looks up on disk, e.g. '#' -> "t_wall" ->
 * pieces/registry/emoji_assets/t_wall/voxels_16.csv. Terrain then
 * furniture, same disjoint-glyph-sets reasoning as glyph_rgb_top(). */
static int glyph_asset_id(const char *registry_rel_path, char glyph, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        char *p1 = strchr(line, '|'); /* -> id */
        if (!p1) continue;
        char *end = strchr(p1 + 1, '|');
        if (end) *end = '\0';
        else p1[strcspn(p1, "\n")] = '\0';
        snprintf(out, out_sz, "%s", p1 + 1);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

static void terrain_or_furniture_asset_id(char glyph, char *out, size_t out_sz) {
    if (glyph_asset_id("pieces/registry/terrain/terrain_types.txt", glyph, out, out_sz)) return;
    if (glyph_asset_id("pieces/registry/furniture/furniture_types.txt", glyph, out, out_sz)) return;
    out[0] = '\0';
}

/* The real ASCII<->emoji GL pipeline, ported from mutaclsym+2's own
 * working mechanism (itself ported from real 1.TPMOS wraith-alpha):
 * assets pre-generated ONCE into
 * pieces/registry/emoji_assets/<asset_id>/voxels_16.csv, at N=16 -
 * chosen to match TILE_PX exactly, so no runtime scaling is needed.
 * Keyed by this project's own registry `id` (see glyph_asset_id()
 * above), same as items/monsters keyed by their own item_id/
 * monster_type - every registry row already has its own unique id, so
 * reusing it avoids a whole extra codepoint-decoding step. */
static int load_emoji_voxels(const char *asset_id, unsigned char voxels[TILE_PX][TILE_PX][4]) {
    /* Single-entry cache - adjacent tiles in the same frame are very
     * often the same asset_id (a run of floor tiles, a wall row), and
     * this is popen/fopen-per-call registry-lookup-adjacent code
     * already, matching this project's existing "correctness over
     * micro-optimization, small file scale" convention rather than
     * building real caching infrastructure for it. */
    static char cached_id[ASSET_ID_BUF] = "";
    static unsigned char cached_voxels[TILE_PX][TILE_PX][4];
    static int cache_valid = 0;

    if (cache_valid && strcmp(cached_id, asset_id) == 0) {
        memcpy(voxels, cached_voxels, sizeof(cached_voxels));
        return 1;
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv", project_root, asset_id);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[128];
    int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (strncmp(line, "r,g,b,a", 7) == 0) continue;
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        if (idx >= TILE_PX * TILE_PX) break;
        int vy = idx / TILE_PX, vx = idx % TILE_PX;
        voxels[vy][vx][0] = (unsigned char)r;
        voxels[vy][vx][1] = (unsigned char)g;
        voxels[vy][vx][2] = (unsigned char)b;
        voxels[vy][vx][3] = (unsigned char)a;
        idx++;
    }
    fclose(f);
    if (idx != TILE_PX * TILE_PX) return 0;

    snprintf(cached_id, sizeof(cached_id), "%s", asset_id);
    memcpy(cached_voxels, voxels, sizeof(cached_voxels));
    cache_valid = 1;
    return 1;
}

/* Alpha-composites one tile's real rasterized emoji pixels on top of
 * whatever's already in the framebuffer at that tile (the flat
 * rgb_top/rgb_top_emoji color, so transparent emoji pixels show the
 * themed background color through, not black) - same "blit with alpha
 * testing" shape as mutaclsym+2's own blit_emoji_tile(), just per-tile
 * instead of per-glyph-cell. */
static void blit_emoji_tile(unsigned char fb[FRAME_H][FRAME_W][4], int tile_x0, int tile_y0,
                             unsigned char voxels[TILE_PX][TILE_PX][4]) {
    for (int y = 0; y < TILE_PX; y++) {
        int fy = tile_y0 + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < TILE_PX; x++) {
            int fx = tile_x0 + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            unsigned char a = voxels[y][x][3];
            if (a == 0) continue;
            if (a == 255) {
                fb[fy][fx][0] = voxels[y][x][0];
                fb[fy][fx][1] = voxels[y][x][1];
                fb[fy][fx][2] = voxels[y][x][2];
            } else {
                fb[fy][fx][0] = (unsigned char)((voxels[y][x][0] * a + fb[fy][fx][0] * (255 - a)) / 255);
                fb[fy][fx][1] = (unsigned char)((voxels[y][x][1] * a + fb[fy][fx][1] * (255 - a)) / 255);
                fb[fy][fx][2] = (unsigned char)((voxels[y][x][2] * a + fb[fy][fx][2] * (255 - a)) / 255);
            }
            fb[fy][fx][3] = 255;
        }
    }
}

/* glyphs[c] is an 8x16 on/off mask (1=foreground) for ASCII char c,
 * 32 <= c < 127 - loaded once from pieces/registry/fonts/ascii/<c>/
 * glyph.txt (a plain #/. text grid, ported unmodified from real
 * 1.TPMOS's wraith_rgb_daemon.c load_glyphs()/font-gen-op.c). Missing
 * files (shouldn't happen - all 95 printable-ASCII codes were copied)
 * leave that slot all-zero, same silent-skip behavior the original
 * has. */
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

/* Stamps one character's foreground pixels directly into the RGBA
 * framebuffer at pixel origin (px,py) - plain CPU pixel writes, same
 * as wraith_rgb_daemon.c's blit_char(), no GL text API of any kind
 * (there is no such thing anywhere in this codebase, by the GOVERNING
 * CONSTRAINT). Background pixels are left untouched (transparent over
 * whatever's already there), matching the original's behavior. */
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
            fb[fy][fx][0] = r;
            fb[fy][fx][1] = g;
            fb[fy][fx][2] = b;
            fb[fy][fx][3] = 255;
        }
    }
}

/* One text row, left-aligned at pixel origin (px,py), clipped to
 * MAX_TEXT_COLS - plain ASCII only (no UTF-8 decoding; nothing this op
 * prints today needs it, unlike wraith_rgb_daemon.c's blit_text()
 * which also handles emoji codepoints via blit_codepoint() - out of
 * scope for tempt-island's HUD text specifically, tracked separately from
 * the emoji/ascii tile parity already documented in
 * GRAND-ARCHITECTURE.md's §0a). */
static void blit_text(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, const char *text,
                       unsigned char r, unsigned char g, unsigned char b) {
    int col = 0;
    for (const char *p = text; *p && col < MAX_TEXT_COLS; p++, col++) {
        blit_char(fb, px + col * GLYPH_W, py, (unsigned char)*p, r, g, b);
    }
}

/* Direct copy of compose_frame.c's own build_choice_footer() - same
 * pdl_reader.+x hero list_methods call, same "row 0/1 (move/end_turn)
 * skipped, numbering starts at 2" convention, same one-shared-source-of-
 * truth reasoning (see that file's own header comment for the full
 * citation of the real 1.TPMOS bug class - get_piece_methods_op.c vs
 * pdl_reader.c/route_input() disagreeing - this avoids by construction).
 * This was the ACTUAL gap behind "I don't see the ASCII action-bar text
 * in the GL window" - both tempt-island's dispatch (ops/choice.c) and the
 * ASCII footer (compose_frame.c) already read hero/piece.pdl's own
 * METHOD table dynamically; this GL-side renderer simply never got the
 * matching row. Duplicated here (not shared via a header) per this
 * project's own "one op = one binary, no shared headers" convention -
 * see MAX_TEXT_COLS's own clipping in blit_text() for what happens if
 * this line runs long: clipped at the frame edge, not wrapped, matching
 * the same bounded-buffer behavior the ASCII version already has. */
static void build_action_footer(const char *project_root_, char *out, size_t out_sz, int action_cursor, int interact_mode, int emoji_mode) {
    char buf[512];

    /* interact_mode replaces the whole footer with a cursor-mode hint,
     * same reasoning as compose_frame.c's own copy: digits are genuine
     * no-ops while controlling the xlector cursor (see ops/choice.c),
     * so showing the numbered action list here would be misleading. */
    if (interact_mode) {
        snprintf(out, out_sz, "[wasd/arrows] Look  [enter] Examine  [esc] Back");
        return;
    }

    snprintf(buf, sizeof(buf), "[wasd/arrows] Move");
    size_t len = strlen(buf);

    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' hero list_methods", project_root_);
    FILE *pf = popen(cmd, "r");
    int idx = 0;
    if (pf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), pf)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (idx >= 2 && line[0]) {
                char label[MAX_LINE];
                snprintf(label, sizeof(label), "%s", line);
                label[0] = (char)toupper((unsigned char)label[0]);
                const char *cur = (idx == action_cursor) ? "[>]" : "[ ]";
                int n = snprintf(buf + len, sizeof(buf) - len, "  %s %d. [%s]", cur, idx, label);
                if (n > 0 && (size_t)n < sizeof(buf) - len) len += (size_t)n;
            }
            idx++;
        }
        pclose(pf);
    }
    const char *emoji_status = emoji_mode ? "ON" : "OFF";
    int n = snprintf(buf + len, sizeof(buf) - len, "  [e] Emoji:%s  [i] Interact  [q] Quit", emoji_status);
    if (n > 0 && (size_t)n < sizeof(buf) - len) len += (size_t)n;

    snprintf(out, out_sz, "%s", buf);
}

/* GL-side equivalent of compose_frame.c's own draw_panel_box() - was the
 * real, concrete gap behind "id like to see menu in gl" (the flat
 * action-bar footer already got this treatment - overlay panels hadn't).
 * Same overlay shape (a titled, bordered box glued to the tile
 * viewport's top-left corner, drawn OVER the already-rasterized map,
 * screen-space not map-space so it stays put regardless of camera
 * scroll), same content (title + numbered/bracket-cursor rows + a
 * trailing Cancel/Close row) - just pixels instead of characters.
 * ASCII blanks the map underneath by overwriting grid characters with
 * spaces first; this fills a solid background rectangle first instead,
 * for the same reason (so old tile colors don't show through the box's
 * own "empty" areas). box_row/box_col below are TILE-grid coordinates
 * (matching compose_frame.c's own box_row=1/box_col=2 exactly, since a
 * viewport row/col here is one TILE_PX/GLYPH_H-ish text line - both
 * happen to be 16px, no separate conversion needed), converted to real
 * pixel offsets at the two call sites below. */
static void draw_panel_box_gl(unsigned char fb[FRAME_H][FRAME_W][4], int tile_y0,
                               const char *title, char panel_rows[][BOX_TEXT_W + 1],
                               int panel_row_count, const char *cancel_row) {
    int box_row = 1, box_col = 2;
    int box_h = panel_row_count + 3; /* border + rows + Cancel + border */
    if (box_row + box_h > VIEWPORT_H) box_h = VIEWPORT_H - box_row;

    int box_x = box_col * TILE_PX;
    int box_y = tile_y0 + box_row * TILE_PX;
    int box_w_px = BOX_TEXT_W * GLYPH_W;
    if (box_x + box_w_px > FRAME_W) box_w_px = FRAME_W - box_x;
    int box_h_px = box_h * GLYPH_H;

    /* Solid background first, so old tile colors underneath don't show
     * through - same reason ASCII pads short rows with spaces before
     * overwriting the grid. */
    for (int y = 0; y < box_h_px; y++) {
        int fy = box_y + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < box_w_px; x++) {
            int fx = box_x + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            fb[fy][fx][0] = 20; fb[fy][fx][1] = 20; fb[fy][fx][2] = 30; fb[fy][fx][3] = 255;
        }
    }

    char top[BOX_TEXT_W + 12];
    snprintf(top, sizeof(top), "+--- %s ------------------+", title);
    for (int r = 0; r < box_h; r++) {
        const char *line;
        if (r == 0) line = top;
        else if (r <= panel_row_count) line = panel_rows[r - 1];
        else if (r == panel_row_count + 1) line = cancel_row;
        else line = "+-----------------------------+";
        blit_text(fb, box_x, box_y + r * GLYPH_H, line, 255, 255, 0);
    }
}

/* Same FNV-1a-64 algorithm real 1.TPMOS wraith_gl.c's checksum_buffer()
 * uses, so a checksum computed here and one computed there (once
 * system/gl_mirror.c exists and reads this same file) are directly
 * comparable - the whole point of the receipt pattern the user asked
 * for: verify the pipeline via matching numbers in two text files,
 * never by looking at a window. */
static uint64_t checksum_buffer(const unsigned char *buf, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= buf[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void write_receipt(const char *path, size_t byte_count, uint64_t checksum,
                           int map_w, int map_h, int cam_x, int cam_y) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    time_t now = time(NULL);
    fprintf(f, "op=compose_rgb_frame\n");
    fprintf(f, "frame_w=%d\n", FRAME_W);
    fprintf(f, "frame_h=%d\n", FRAME_H);
    fprintf(f, "tile_px=%d\n", TILE_PX);
    fprintf(f, "viewport_w=%d\n", VIEWPORT_W);
    fprintf(f, "viewport_h=%d\n", VIEWPORT_H);
    fprintf(f, "bytes_per_pixel=4\n");
    fprintf(f, "byte_count=%zu\n", byte_count);
    fprintf(f, "expected_byte_count=%d\n", FRAME_W * FRAME_H * 4);
    fprintf(f, "checksum_fnv1a64=%016llx\n", (unsigned long long)checksum);
    fprintf(f, "map_w=%d\n", map_w);
    fprintf(f, "map_h=%d\n", map_h);
    fprintf(f, "cam_x=%d\n", cam_x);
    fprintf(f, "cam_y=%d\n", cam_y);
    fprintf(f, "written_at=%ld\n", (long)now);
    fclose(f);
}

/* ========================================================================
 * 3D VIEW - ported from mutaclsym's own compose_rgb_frame.c (ray-marched
 * DDA wall renderer, same code, adapted to tempt-island's own PATH_BUF/
 * MAX_LINE/MAX_MAP_W/MAX_MAP_H/ASSET_ID_BUF constants and glyph_rgb_top()/
 * glyph_fallback_rgb()/terrain_or_furniture_asset_id() helpers - those
 * already have identical signatures/behavior in this file, no adaptation
 * needed there). GL-only: ops/compose_frame.c (ASCII) never reads
 * render_mode at all, matching mutaclsym's own convention that '0' only
 * changes what the GL/RGB view shows.
 *
 * NAMED SIMPLIFICATIONS vs. a "real" AAA renderer (matching mutaclsym's
 * own scope, not a corner cut specific to this port):
 *   - Floor is painter's-algorithm (back-to-front row order), walls are a
 *     real per-pixel DDA ray march with a full AABB slab test per cell -
 *     correct occlusion and correct per-camera-angle face visibility for
 *     the walls; the floor is a flat continuous plane so draw order
 *     there can never mis-order.
 *   - Faces behind the near plane are REJECTED, not clipped (a wall very
 *     close to the camera can visibly vanish rather than clip smoothly).
 *   - Optional per-face emoji texture (voxels_8.csv under
 *     pieces/registry/emoji_assets/<id>/) - if tempt-island has no such
 *     assets yet, sample_voxel8_pixel() simply fails to load and every
 *     wall face falls back to its flat, darkened registry color; no
 *     crash, no missing-texture visual either.
 */
#define VIEW_3D_RADIUS 12 /* tiles around the hero actually rendered - real perf/scope bound, not the whole map */
#define NEAR_PLANE_3D 0.15

/* Simple perspective projection: world (wx,wy,wz) -> screen (out_x,out_y),
 * real camera-space transform (translate by cam pos, rotate by pitch),
 * then perspective divide. wy is height (up), wz is depth (forward from
 * the camera's own unrotated axis - yaw is applied by the CALLER before
 * this, by rotating wx/wz around the yaw pivot; see render_3d_view()). */
static void project_3d(double wx, double wy, double wz,
                        double cam_x, double cam_y, double cam_z, double pitch_deg,
                        double focal, int screen_cx, int screen_cy, double scale,
                        int *out_x, int *out_y, double *out_z2) {
    double rx = wx - cam_x;
    double ry = wy - cam_y;
    double rz = wz - cam_z;
    double ax = pitch_deg * M_PI / 180.0;
    double cpitch = cos(ax), spitch = sin(ax);
    double y2 = ry * cpitch - rz * spitch;
    double z2 = ry * spitch + rz * cpitch;
    int rejected = (z2 <= NEAR_PLANE_3D);
    if (rejected) z2 = NEAR_PLANE_3D; /* clamp so callers doing simple math don't divide-by-near-zero; drawing code below still checks out_z2 itself to reject */
    double persp = focal / z2;
    /* REAL BUG FIX, GL-only (live-reported: "the whole map seems
     * inverted right and left"): world X (col) increases with
     * yaw_rotate()'s own standard-math-convention rotation, but this
     * project's world uses row-increases-DOWNWARD as its Z axis (a
     * left-handed 2D plane, not the right-handed one that rotation
     * formula assumes) - screen_cx + rx put east/west content on the
     * opposite screen side from where the ASCII/2D view (and every
     * other renderer) shows it. Negating rx here un-mirrors the X axis;
     * raymarch_walls_3d()'s own dx_cam computation is this formula's
     * algebraic inverse and must flip the same way for the wall pass to
     * stay aligned with this (floor) pass - see that function's own
     * matching comment. */
    *out_x = screen_cx - (int)(rx * scale * persp);
    *out_y = screen_cy - (int)(y2 * scale * persp);
    if (out_z2) *out_z2 = z2;
}

/* Yaw-rotates a world (x,z) pair around (pivot_x,pivot_z) by yaw_deg -
 * applied before project_3d() so "forward" always means "into the
 * screen" regardless of which absolute map direction the hero (or the
 * free camera) currently faces. */
static void yaw_rotate(double x, double z, double pivot_x, double pivot_z, double yaw_deg,
                        double *out_x, double *out_z) {
    double rad = yaw_deg * M_PI / 180.0;
    double cy = cos(rad), sy = sin(rad);
    double dx = x - pivot_x, dz = z - pivot_z;
    *out_x = pivot_x + dx * cy - dz * sy;
    *out_z = pivot_z + dx * sy + dz * cy;
}

static double facing_to_yaw(int facing) {
    /* ARROW_* sentinel values (1000-1003, same convention every file in
     * this project uses) map to compass-style yaw degrees.
     *
     * REAL BUG FIX: project_3d() structurally treats +Z (rz = wz -
     * cam_z > 0) as "in front of the camera" (that's what
     * NEAR_PLANE_3D rejects against) - that's baked into its math, not
     * a convention this function gets to redefine by itself. At yaw=0
     * (no rotation), a point north of the hero (smaller row) has
     * negative rz and is wrongly treated as BEHIND the camera, while a
     * point south (larger row, genuinely behind a hero facing up) has
     * positive rz and wrongly renders as "in front" - i.e. ARROW_UP
     * showed what's behind the hero, not ahead of it (live-reported:
     * "camera facing player's back"). ARROW_RIGHT/LEFT's own 90°/270°
     * were independently verified correct by the same rz>0-after-
     * rotation reasoning (east/west points already land in front for a
     * hero facing right/left) - only UP/DOWN needed swapping, which is
     * exactly what "invert the arrow keys up and down" (live feedback)
     * describes: ARROW_UP now yaws the world a full 180° so a point
     * actually ahead (north, smaller row) rotates to positive rz, and
     * ARROW_DOWN goes to the old ARROW_UP's 0° for the same reason in
     * reverse (south, larger row, is already positive rz unrotated).
     *
     * REAL, LIVE-CONFIRMED FOLLOW-UP FIX: ARROW_RIGHT/LEFT had the same
     * class of inversion as UP/DOWN above (live-tested and reported
     * after the UP/DOWN fix landed) - swapped the same way, 90°<->270°. */
    switch (facing) {
        case 1002: return 180.0; /* ARROW_UP */
        case 1003: return 0.0;   /* ARROW_DOWN */
        case 1001: return 270.0; /* ARROW_RIGHT */
        case 1000: return 90.0;  /* ARROW_LEFT */
        default:   return 180.0; /* unset - matches this project's own default facing=1002 (ARROW_UP) in hero/state.txt */
    }
}

/* Simple scanline fill of a convex quad (4 screen points, in order).
 * fb is a flat RGBA8888 buffer, fb_w*fb_h*4 bytes - explicit width/
 * height (not the fixed FRAME_W/FRAME_H) so this same code could later
 * serve a differently-sized target buffer too. */
static void fill_quad_3d(unsigned char *fb, int fb_w, int fb_h,
                          const int xs[4], const int ys[4],
                          int r, int g, int b) {
    int ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < 4; i++) {
        if (ys[i] < ymin) ymin = ys[i];
        if (ys[i] > ymax) ymax = ys[i];
    }
    if (ymin < 0) ymin = 0;
    if (ymax >= fb_h) ymax = fb_h - 1;
    for (int y = ymin; y <= ymax; y++) {
        double xleft = 1e18, xright = -1e18;
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            int ay = ys[i], by = ys[j];
            if (ay == by) continue;
            if ((y >= ay && y < by) || (y >= by && y < ay)) {
                double t = (double)(y - ay) / (double)(by - ay);
                double x = xs[i] + t * (xs[j] - xs[i]);
                if (x < xleft) xleft = x;
                if (x > xright) xright = x;
            }
        }
        if (xleft > xright) continue;
        int xi0 = (int)xleft, xi1 = (int)xright;
        if (xi0 < 0) xi0 = 0;
        if (xi1 >= fb_w) xi1 = fb_w - 1;
        for (int x = xi0; x <= xi1; x++) {
            unsigned char *px = &fb[(y * fb_w + x) * 4];
            px[0] = (unsigned char)r;
            px[1] = (unsigned char)g;
            px[2] = (unsigned char)b;
            px[3] = 255;
        }
    }
}

/* Projects a quad's 4 world-space corners and fills it, UNLESS any
 * corner falls behind the near plane (reject-not-clip). */
static void draw_quad_3d(unsigned char *fb, int fb_w, int fb_h,
                          double wx0, double wy0, double wz0,
                          double wx1, double wy1, double wz1,
                          double wx2, double wy2, double wz2,
                          double wx3, double wy3, double wz3,
                          double cam_x, double cam_y, double cam_z, double pitch, double focal,
                          int screen_cx, int screen_cy, double scale,
                          int r, int g, int b) {
    int xs[4], ys[4];
    double z2[4];
    project_3d(wx0, wy0, wz0, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &xs[0], &ys[0], &z2[0]);
    project_3d(wx1, wy1, wz1, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &xs[1], &ys[1], &z2[1]);
    project_3d(wx2, wy2, wz2, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &xs[2], &ys[2], &z2[2]);
    project_3d(wx3, wy3, wz3, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &xs[3], &ys[3], &z2[3]);
    for (int i = 0; i < 4; i++) if (z2[i] <= NEAR_PLANE_3D) return;
    fill_quad_3d(fb, fb_w, fb_h, xs, ys, r, g, b);
}

/* Registry `walkable` field (glyph|id|name|walkable|rgb_top|unicode|
 * rgb_top_emoji) - determines whether a tile is drawn as an extruded
 * wall (ray-marched below) or a flat floor quad. Duplicated narrowly
 * (matching this project's own "no shared headers" convention, same
 * shape as glyph_rgb_top() above) rather than shared. */
static int glyph_walkable_3d(const char *registry_rel_path, char glyph) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[MAX_LINE];
    int walkable = 1;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        line[strcspn(line, "\n")] = '\0';
        char *p = strchr(line, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        walkable = atoi(p + 1);
        break;
    }
    fclose(f);
    return walkable;
}

/* REAL RAY-MARCHED WALL RENDERING - a DDA walk over the map's real
 * (col,row) grid, testing a full 3D AABB per occupied cell in near-to-
 * far order, gives correct occlusion AND correct per-camera-angle face
 * visibility for free (the first hit found along each pixel's ray is
 * guaranteed nearest). Ported from mutaclsym's own port of real
 * piececraft-wraith's raymarch_tile_grid()/ray_aabb_hit()/
 * sample_voxel_pixel().
 *
 * Frame convention: mutaclsym/tempt-island have no mouse-orbit - world
 * geometry is yaw-rotated by the hero's own `facing` before projecting
 * (see yaw_rotate()/facing_to_yaw() above), keeping the camera itself
 * always at its natural map-space position. That means cam_x/cam_y/
 * cam_z are already in the map's real, unrotated frame the DDA needs -
 * only the per-pixel RAY DIRECTION needs un-yawing (rotating by -yaw
 * instead of +yaw), done below with pivot (0,0) since rotating a
 * direction vector needs no pivot. */

/* Ray/AABB slab test. Returns the nearest entry t>=0 and which face was
 * hit: 0/1 = -X/+X, 2/3 = -Y/+Y (3 = top), 4/5 = -Z/+Z. */
static int ray_aabb_hit_3d(double ox, double oy, double oz, double dx, double dy, double dz,
                            double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                            double *out_t, int *out_face) {
    double tmin = -1e18, tmax = 1e18;
    int face = -1;

    if (fabs(dx) < 1e-12) {
        if (ox < bx0 || ox > bx1) return 0;
    } else {
        double t0 = (bx0 - ox) / dx, t1 = (bx1 - ox) / dx;
        int f0 = 0;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 1; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dy) < 1e-12) {
        if (oy < by0 || oy > by1) return 0;
    } else {
        double t0 = (by0 - oy) / dy, t1 = (by1 - oy) / dy;
        int f0 = 2;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 3; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dz) < 1e-12) {
        if (oz < bz0 || oz > bz1) return 0;
    } else {
        double t0 = (bz0 - oz) / dz, t1 = (bz1 - oz) / dz;
        int f0 = 4;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 5; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (tmax < 0.0) return 0;
    if (tmin < 0.0) { tmin = 0.0; face = -1; }
    *out_t = tmin;
    if (out_face) *out_face = face;
    return 1;
}

/* Small persistent cache of loaded voxels_8.csv assets (resolution-8,
 * per-face emoji texture) - optional per-glyph decoration on wall side
 * faces. Absent assets simply fail to load (see get_voxel8_cached()'s
 * own "cached as not loaded" comment) and callers fall back to the
 * tile's own flat, darkened rgb_top - no crash, no build-time
 * dependency on these assets existing in tempt-island's own registry. */
#define MAX_VOXEL8_CACHE 16
typedef struct {
    char path[PATH_BUF];
    int resolution;
    int count;
    unsigned char pixels[64][4];
    int loaded;
} Voxel8Cache;
static Voxel8Cache g_voxel8_cache[MAX_VOXEL8_CACHE];
static int g_voxel8_cache_count = 0;

static Voxel8Cache *get_voxel8_cached(const char *path) {
    for (int i = 0; i < g_voxel8_cache_count; i++) {
        if (strcmp(g_voxel8_cache[i].path, path) == 0) return &g_voxel8_cache[i];
    }
    if (g_voxel8_cache_count >= MAX_VOXEL8_CACHE) return NULL;
    Voxel8Cache *c = &g_voxel8_cache[g_voxel8_cache_count++];
    snprintf(c->path, sizeof(c->path), "%s", path);
    c->count = 0; c->resolution = 0; c->loaded = 0;
    FILE *f = fopen(path, "r");
    if (!f) return c; /* cached as "not loaded" so we don't retry every hit */
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && c->count < 64) {
        int r, g, b, a;
        if (line[0] == '#') { sscanf(line, "# resolution=%d", &c->resolution); continue; }
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            c->pixels[c->count][0] = (unsigned char)r;
            c->pixels[c->count][1] = (unsigned char)g;
            c->pixels[c->count][2] = (unsigned char)b;
            c->pixels[c->count][3] = (unsigned char)a;
            c->count++;
        }
    }
    fclose(f);
    if (c->resolution <= 0) for (c->resolution = 1; c->resolution * c->resolution < c->count; c->resolution++) {}
    c->loaded = (c->resolution > 0 && c->count > 0);
    return c;
}

/* Samples a voxels_8.csv at normalized (u,v) in [0,1). Returns 1 and
 * fills out_rgb if that pixel is occupied (alpha>10); 0 if transparent
 * or the CSV couldn't be loaded (caller falls back to the tile's own
 * flat, darkened rgb_top). */
static int sample_voxel8_pixel(const char *path, double u, double v, unsigned char out_rgb[3]) {
    Voxel8Cache *c = get_voxel8_cached(path);
    if (!c || !c->loaded) return 0;
    if (u < 0.0) u = 0.0;
    if (u > 0.999999) u = 0.999999;
    if (v < 0.0) v = 0.0;
    if (v > 0.999999) v = 0.999999;
    int col = (int)(u * c->resolution), row = (int)(v * c->resolution);
    int idx = row * c->resolution + col;
    if (idx < 0 || idx >= c->count) return 0;
    if (c->pixels[idx][3] <= 10) return 0;
    out_rgb[0] = c->pixels[idx][0];
    out_rgb[1] = c->pixels[idx][1];
    out_rgb[2] = c->pixels[idx][2];
    return 1;
}

/* Per-glyph metadata, cached ONCE per raymarch_walls_3d() call rather
 * than re-reading the registry file from disk on every DDA step of
 * every pixel. */
typedef struct {
    char glyph;
    int walkable;
    int r, g, b;
    char voxel_path[PATH_BUF];
    int has_voxel;
} WallGlyphMeta;

/* Per-pixel DDA ray marcher over the non-walkable ("wall") cells of the
 * map grid. Draws directly into fb - called AFTER the floor pass in
 * render_3d_view() below, so a wall hit always correctly occludes
 * whatever floor color was drawn at the same pixel. */
static void raymarch_walls_3d(unsigned char *fb, int fb_w, int fb_h,
                               char grid[MAX_MAP_H][MAX_MAP_W + 1], int rows, int map_w,
                               double cam_x, double cam_y, double cam_z, double pitch, double yaw,
                               double focal, int screen_cx, int screen_cy, double scale,
                               int col_lo, int col_hi, int row_lo, int row_hi) {
    (void)map_w;
    (void)rows;
    WallGlyphMeta meta_cache[16];
    int meta_count = 0;
    static int row_len[MAX_MAP_H];
    for (int row = row_lo; row <= row_hi; row++) row_len[row] = (int)strlen(grid[row]);

    double cpitch = cos(pitch * M_PI / 180.0), spitch = sin(pitch * M_PI / 180.0);

    for (int sy = 0; sy < fb_h; sy++) {
        for (int sx = 0; sx < fb_w; sx++) {
            /* Algebraic inverse of project_3d()'s own negated out_x
             * (screen_cx - rx*scale*persp) - see that function's own
             * matching comment. Must flip the same way this pass's
             * rays are cast, or the ray-marched walls would un-mirror
             * independently of the floor pass above and the two would
             * no longer align. */
            double dx_cam = (screen_cx - sx) / (scale * focal);
            double dy_cam = (screen_cy - sy) / (scale * focal);
            double dy_p = dy_cam * cpitch + spitch;
            double dz_p = -dy_cam * spitch + cpitch;
            double dir_x, dir_z;
            yaw_rotate(dx_cam, dz_p, 0.0, 0.0, -yaw, &dir_x, &dir_z);
            double dir_y = dy_p;
            double ox = cam_x, oy = cam_y, oz = cam_z;

            int col = (int)floor(ox), row = (int)floor(oz);
            int step_col = (dir_x > 0.0) ? 1 : (dir_x < 0.0 ? -1 : 0);
            int step_row = (dir_z > 0.0) ? 1 : (dir_z < 0.0 ? -1 : 0);
            double t_delta_x = (dir_x != 0.0) ? fabs(1.0 / dir_x) : 1e18;
            double t_delta_z = (dir_z != 0.0) ? fabs(1.0 / dir_z) : 1e18;
            double t_max_x = (dir_x > 0.0) ? ((col + 1) - ox) / dir_x : (dir_x < 0.0 ? (col - ox) / dir_x : 1e18);
            double t_max_z = (dir_z > 0.0) ? ((row + 1) - oz) / dir_z : (dir_z < 0.0 ? (row - oz) / dir_z : 1e18);

            int hit = 0, hit_face = -1, hit_col = -1, hit_row = -1;
            double hit_t = 0.0;
            int max_steps = (col_hi - col_lo + row_hi - row_lo) * 2 + 8;

            for (int steps = 0; steps < max_steps; steps++) {
                if (col >= col_lo && col <= col_hi && row >= row_lo && row <= row_hi) {
                    int len = row_len[row];
                    char glyph = (col < len) ? grid[row][col] : ' ';
                    if (glyph != ' ' && glyph != '\0') {
                        WallGlyphMeta *m = NULL;
                        for (int mi = 0; mi < meta_count; mi++) {
                            if (meta_cache[mi].glyph == glyph) { m = &meta_cache[mi]; break; }
                        }
                        WallGlyphMeta overflow_meta;
                        if (!m) {
                            m = (meta_count < 16) ? &meta_cache[meta_count++] : &overflow_meta;
                            m->glyph = glyph;
                            m->walkable = glyph_walkable_3d("pieces/registry/terrain/terrain_types.txt", glyph);
                            if (!glyph_rgb_top("pieces/registry/terrain/terrain_types.txt", glyph, 0, &m->r, &m->g, &m->b) &&
                                !glyph_rgb_top("pieces/registry/furniture/furniture_types.txt", glyph, 0, &m->r, &m->g, &m->b)) {
                                glyph_fallback_rgb(glyph, &m->r, &m->g, &m->b);
                            }
                            char asset_id[64];
                            terrain_or_furniture_asset_id(glyph, asset_id, sizeof(asset_id));
                            if (asset_id[0]) {
                                snprintf(m->voxel_path, sizeof(m->voxel_path), "%s/pieces/registry/emoji_assets/%s/voxels_8.csv", project_root, asset_id);
                                m->has_voxel = 1;
                            } else {
                                m->has_voxel = 0;
                            }
                        }
                        if (!m->walkable) {
                            double bx0 = (double)col, bx1 = bx0 + 1.0;
                            double bz0 = (double)row, bz1 = bz0 + 1.0;
                            double t; int face;
                            if (ray_aabb_hit_3d(ox, oy, oz, dir_x, dir_y, dir_z, bx0, bx1, 0.0, 1.0, bz0, bz1, &t, &face)) {
                                hit = 1; hit_t = t; hit_face = face; hit_col = col; hit_row = row;
                                break;
                            }
                        }
                    }
                }
                if (t_max_x < t_max_z) { col += step_col; t_max_x += t_delta_x; }
                else { row += step_row; t_max_z += t_delta_z; }
                if (col < col_lo - 1 || col > col_hi + 1 || row < row_lo - 1 || row > row_hi + 1) break;
            }

            if (hit) {
                double wx = ox + hit_t * dir_x;
                double wy = oy + hit_t * dir_y;
                double wz = oz + hit_t * dir_z;
                WallGlyphMeta *m = NULL;
                for (int mi = 0; mi < meta_count; mi++) {
                    if (meta_cache[mi].glyph == grid[hit_row][hit_col]) { m = &meta_cache[mi]; break; }
                }
                if (!m) continue;
                int r = m->r, g = m->g, b = m->b;
                if (hit_face != 3) {
                    /* Side face: darken as the flat fallback, then try a
                     * real emoji texture on top of it if this glyph has
                     * a voxels_8.csv asset. */
                    r = r * 3 / 4; g = g * 3 / 4; b = b * 3 / 4;
                    if (m->has_voxel) {
                        double bx0 = (double)hit_col, bz0 = (double)hit_row;
                        double u = (hit_face == 4 || hit_face == 5) ? (wx - bx0) : (wz - bz0);
                        double v = 1.0 - wy; /* extrude is always 1.0 for now */
                        unsigned char sampled[3];
                        if (sample_voxel8_pixel(m->voxel_path, u, v, sampled)) {
                            r = sampled[0]; g = sampled[1]; b = sampled[2];
                        }
                    }
                }
                unsigned char *px = &fb[(sy * fb_w + sx) * 4];
                px[0] = (unsigned char)r; px[1] = (unsigned char)g; px[2] = (unsigned char)b; px[3] = 255;
            }
        }
    }
}

/* The 3D view itself. World space: col->X, row->Z (depth), extrude
 * height->Y (up). */
static void render_3d_view(unsigned char *fb, int fb_w, int fb_h,
                            char grid[MAX_MAP_H][MAX_MAP_W + 1], int rows, int map_w,
                            int px, int py, int camera_mode, int facing,
                            double cam_pan_x, double cam_pan_y, double cam_pan_z) {
    double focal = 1.0;
    double scale = 210.0;
    int screen_cx = fb_w / 2;
    int screen_cy = fb_h / 2;
    double pitch, cam_y_off, cam_z_off;
    double yaw = facing_to_yaw(facing);

    /* Camera presets - values chosen for tempt-island's own tile scale
     * (same numbers mutaclsym itself tuned to and re-verified, since
     * both projects share the same TILE_PX=16 flat-viewport convention
     * this 3D path's screen_cx/screen_cy/scale are built against).
     * pitch/cam_y_off/scale are coupled (steeper pitch or higher cam_y
     * needs a smaller `scale` to stay on-screen) - if these ever need
     * retuning, verify with dump_rgb_png-style output before trusting
     * new numbers, don't guess. */
    switch (camera_mode) {
        case 2: pitch = 10.0; cam_y_off = 1.6; cam_z_off = 2.5; break;  /* 3rd person: pulled back/higher - "behind" is +Z since forward is -Z */
        case 3: pitch = 6.0; cam_y_off = 0.9; cam_z_off = 0.0; break;   /* free camera: starts at the same neutral spot as 1st person */
        default: pitch = 6.0; cam_y_off = 0.9; cam_z_off = 0.0; break;  /* 1st person: at hero eye height */
    }

    double base_x = (double)px;
    double base_z = (double)py;
    double off_x, off_z;
    yaw_rotate(0.0, cam_z_off, 0.0, 0.0, yaw, &off_x, &off_z);
    double cam_x = base_x + off_x;
    double cam_z = base_z + off_z;
    double cam_y = cam_y_off;
    if (camera_mode == 3) {
        cam_x += cam_pan_x;
        cam_y += cam_pan_y;
        cam_z += cam_pan_z;
    }

    int col_lo = px - VIEW_3D_RADIUS, col_hi = px + VIEW_3D_RADIUS;
    int row_lo = py - VIEW_3D_RADIUS, row_hi = py + VIEW_3D_RADIUS;
    if (col_lo < 0) col_lo = 0;
    if (row_lo < 0) row_lo = 0;
    if (col_hi >= map_w) col_hi = map_w - 1;
    if (row_hi >= rows) row_hi = rows - 1;

    /* Pass 1: the floor. Walkable tiles only, drawn as flat y=0 quads.
     * Non-walkable ("wall") tiles are fully handled by the ray marcher
     * below instead, not drawn here at all. */
    for (int row = row_hi; row >= row_lo; row--) {
        int len = (int)strlen(grid[row]);
        for (int col = col_lo; col <= col_hi; col++) {
            char glyph = (col < len) ? grid[row][col] : ' ';
            if (glyph == ' ' || glyph == '\0') continue;
            if (!glyph_walkable_3d("pieces/registry/terrain/terrain_types.txt", glyph)) continue;

            int r, g, b;
            if (!glyph_rgb_top("pieces/registry/terrain/terrain_types.txt", glyph, 0, &r, &g, &b) &&
                !glyph_rgb_top("pieces/registry/furniture/furniture_types.txt", glyph, 0, &r, &g, &b)) {
                glyph_fallback_rgb(glyph, &r, &g, &b);
            }

            double wx0 = (double)col, wx1 = (double)col + 1.0;
            double wz0 = (double)row, wz1 = (double)row + 1.0;
            double rx0, rz0, rx1, rz1, rx2, rz2, rx3, rz3;
            yaw_rotate(wx0, wz0, base_x, base_z, yaw, &rx0, &rz0);
            yaw_rotate(wx1, wz0, base_x, base_z, yaw, &rx1, &rz1);
            yaw_rotate(wx1, wz1, base_x, base_z, yaw, &rx2, &rz2);
            yaw_rotate(wx0, wz1, base_x, base_z, yaw, &rx3, &rz3);

            draw_quad_3d(fb, fb_w, fb_h, rx0, 0.0, rz0, rx1, 0.0, rz1, rx2, 0.0, rz2, rx3, 0.0, rz3,
                         cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, r, g, b);
        }
    }

    /* Pass 2: walls, ray-marched. */
    raymarch_walls_3d(fb, fb_w, fb_h, grid, rows, map_w, cam_x, cam_y, cam_z, pitch, yaw,
                       focal, screen_cx, screen_cy, scale, col_lo, col_hi, row_lo, row_hi);
}

int main(void) {
    resolve_root();
    load_glyphs();

    char hero_path[PATH_BUF], out_path[PATH_BUF], receipt_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_tempt_island/map_start/hero/state.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);

    int px = read_kv_int(hero_path, "pos_x", 0);
    int py = read_kv_int(hero_path, "pos_y", 0);
    int hp = read_kv_int(hero_path, "hp", 100);
    int hunger = read_kv_int(hero_path, "hunger", 0);
    int thirst = read_kv_int(hero_path, "thirst", 0);
    int stamina = read_kv_int(hero_path, "stamina", 100);
    int action_cursor = read_kv_int(hero_path, "action_cursor", -1);
    int interact_mode = read_kv_int(hero_path, "interact_mode", 0);
    int emoji_mode = read_kv_int(hero_path, "emoji_mode", 0);
    int xlector_x = read_kv_int(hero_path, "xlector_pos_x", px);
    int xlector_y = read_kv_int(hero_path, "xlector_pos_y", py);
    int render_mode = read_kv_int(hero_path, "render_mode", 0);
    int camera_mode = read_kv_int(hero_path, "camera_mode", 1);
    int facing = read_kv_int(hero_path, "facing", 1002);
    double cam_pan_x = read_kv_double(hero_path, "cam_pan_x", 0.0);
    double cam_pan_y = read_kv_double(hero_path, "cam_pan_y", 0.0);
    double cam_pan_z = read_kv_double(hero_path, "cam_pan_z", 0.0);
    char map_id[64];
    read_kv_str(hero_path, "map_id", map_id, sizeof(map_id), "map_start");

    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_tempt_island/%s", project_root, map_id);
    char map_path[PATH_BUF + 32], furniture_path[PATH_BUF + 32], turn_path[PATH_BUF + 32];
    snprintf(map_path, sizeof(map_path), "%s/map.txt", map_dir);
    snprintf(furniture_path, sizeof(furniture_path), "%s/furniture.txt", map_dir);
    snprintf(turn_path, sizeof(turn_path), "%s/state.txt", map_dir);

    int turn = read_kv_int(turn_path, "turn", 0);
    int map_w = read_kv_int(turn_path, "width", VIEWPORT_W);
    int map_h = read_kv_int(turn_path, "height", VIEWPORT_H);
    if (map_w > MAX_MAP_W) map_w = MAX_MAP_W;
    if (map_h > MAX_MAP_H) map_h = MAX_MAP_H;
    /* Same "one state, two renderers" mirror principle as
     * compose_frame.c - z is the same purely-informational multi-Z
     * floor field, kept consistent across both renderers. */
    int z = read_kv_int(turn_path, "z", 0);

    /* Grid-building (terrain -> furniture -> items -> monsters -> hero)
     * copied verbatim in shape from compose_frame.c - same absolute
     * map-space buffer, same layering order, so the two renderers can
     * never show a different world, only a different encoding of it. */
    char grid[MAX_MAP_H][MAX_MAP_W + 1];
    /* Parallel per-cell asset-id map, identity-resolved at the exact
     * point each layer is drawn (not re-derived from grid's flat char
     * afterward) - same reasoning as compose_frame.c's own cell_emoji
     * buffer: this project's registries have real glyph collisions
     * between terrain and items, so a naive shared-char lookup would
     * pick the wrong asset for whichever one is actually at that tile.
     * static, matching this file's own framebuf convention for large
     * buffers. Only populated/read when emoji_mode=1. */
    static char cell_asset[MAX_MAP_H][MAX_MAP_W][ASSET_ID_BUF];
    int rows = 0;
    FILE *mf = fopen(map_path, "r");
    FILE *ff = fopen(furniture_path, "r");
    if (mf) {
        char terrain_line[MAX_MAP_W + 4], furniture_line[MAX_MAP_W + 4];
        while (rows < map_h && fgets(terrain_line, sizeof(terrain_line), mf)) {
            terrain_line[strcspn(terrain_line, "\n")] = '\0';
            furniture_line[0] = '\0';
            if (ff) {
                if (!fgets(furniture_line, sizeof(furniture_line), ff)) furniture_line[0] = '\0';
                furniture_line[strcspn(furniture_line, "\n")] = '\0';
            }
            int len = (int)strlen(terrain_line);
            if (len > map_w) len = map_w;
            for (int col = 0; col < len; col++) {
                char fg = (col < (int)strlen(furniture_line)) ? furniture_line[col] : ' ';
                grid[rows][col] = (fg != ' ') ? fg : terrain_line[col];
                if (emoji_mode) terrain_or_furniture_asset_id(grid[rows][col], cell_asset[rows][col], ASSET_ID_BUF);
            }
            grid[rows][len] = '\0';
            rows++;
        }
        fclose(mf);
    }
    if (ff) fclose(ff);

    char items_dir[PATH_BUF + 32];
    snprintf(items_dir, sizeof(items_dir), "%s/items", map_dir);
    DIR *d = opendir(items_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 320];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", items_dir, entry->d_name);
            int ix = read_kv_int(state_path, "pos_x", -1);
            int iy = read_kv_int(state_path, "pos_y", -1);
            if (ix < 0 || iy < 0 || ix >= map_w || iy >= rows) continue;
            char item_id[64];
            read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");
            char glyph[4];
            item_registry_field(item_id, 3, glyph, sizeof(glyph), "?");
            if (glyph[0]) grid[iy][ix] = glyph[0];
            /* Resolved by item_id directly (its own asset dir name),
             * not re-derived from glyph[0] - see cell_asset's own
             * declaration comment for the real terrain/item collisions
             * this avoids. */
            if (emoji_mode && glyph[0]) snprintf(cell_asset[iy][ix], ASSET_ID_BUF, "%s", item_id);
        }
        closedir(d);
    }

    char monsters_dir[PATH_BUF + 32];
    snprintf(monsters_dir, sizeof(monsters_dir), "%s/monsters", map_dir);
    d = opendir(monsters_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 320];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", monsters_dir, entry->d_name);
            int mx = read_kv_int(state_path, "pos_x", -1);
            int my = read_kv_int(state_path, "pos_y", -1);
            if (mx < 0 || my < 0 || mx >= map_w || my >= rows) continue;
            char monster_type[64];
            read_kv_str(state_path, "monster_type", monster_type, sizeof(monster_type), "?");
            char glyph[4];
            monster_registry_field(monster_type, 2, glyph, sizeof(glyph), "?");
            if (glyph[0]) grid[my][mx] = glyph[0];
            /* Resolved by monster_type directly - same reasoning as the
             * item loop above. */
            if (emoji_mode && glyph[0]) snprintf(cell_asset[my][mx], ASSET_ID_BUF, "%s", monster_type);
        }
        closedir(d);
    }

    if (py >= 0 && py < rows) {
        int rowlen = (int)strlen(grid[py]);
        while (rowlen <= px && rowlen < map_w) {
            grid[py][rowlen] = ' ';
            if (emoji_mode) cell_asset[py][rowlen][0] = '\0';
            rowlen++;
            grid[py][rowlen] = '\0';
        }
        if (px >= 0 && px < map_w) {
            grid[py][px] = '@';
            /* Hero/xlector are hardcoded chars, not registry-driven -
             * "hero"/"xlector" are the fixed asset ids generated for
             * them, same as mutaclsym+2's own convention. */
            if (emoji_mode) snprintf(cell_asset[py][px], ASSET_ID_BUF, "hero");
        }
    }

    /* Xlector cursor - same real xlector active-target pattern as
     * compose_frame.c's own copy (see that file's header comment and
     * ops/choice.c's own writeup / dox/04-chtpm-parser-research-and-
     * interact-mode.txt for the full research). Drawn last, on top of
     * even the hero. */
    if (interact_mode == 1 && xlector_y >= 0 && xlector_y < rows) {
        int rowlen = (int)strlen(grid[xlector_y]);
        while (rowlen <= xlector_x && rowlen < map_w) {
            grid[xlector_y][rowlen] = ' ';
            if (emoji_mode) cell_asset[xlector_y][rowlen][0] = '\0';
            rowlen++;
            grid[xlector_y][rowlen] = '\0';
        }
        if (xlector_x >= 0 && xlector_x < map_w) {
            grid[xlector_y][xlector_x] = 'X';
            if (emoji_mode) snprintf(cell_asset[xlector_y][xlector_x], ASSET_ID_BUF, "xlector");
        }
    }

    /* Camera - byte-for-byte the same clamp formula as compose_frame.c,
     * so the GL viewport and the ASCII viewport always show the exact
     * same rectangle of the map. */
    int cam_x = px - VIEWPORT_W / 2;
    int cam_x_max = map_w - VIEWPORT_W;
    if (cam_x_max < 0) cam_x_max = 0;
    if (cam_x < 0) cam_x = 0;
    if (cam_x > cam_x_max) cam_x = cam_x_max;
    int cam_y = py - VIEWPORT_H / 2;
    int cam_y_max = map_h - VIEWPORT_H;
    if (cam_y_max < 0) cam_y_max = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_y > cam_y_max) cam_y = cam_y_max;

    /* Rasterize: one glyph -> one TILE_PX x TILE_PX solid block, written
     * top-to-bottom, left-to-right, RGBA8888 - the exact flat layout
     * wraith_gl.c's load_texture()/glTexImage2D expects from its own
     * WRAITH_FRAME_SOURCE file, so system/gl_mirror.c's port of that
     * function can read this file with zero format translation. Tile
     * rows start HEADER_ROWS*GLYPH_H pixels down, leaving the top text
     * row untouched (stays black, matching blit_char()'s
     * leave-background-alone behavior) for the header text blitted
     * below. */
    static unsigned char framebuf[FRAME_H][FRAME_W][4];
    /* Alpha defined as fully opaque everywhere up front - the tile loop
     * below and blit_char() both only ever set alpha=255 on pixels they
     * actually touch, which would otherwise leave the header/footer
     * text rows' background at the static zero-init's alpha=0 (RGB 0
     * too, so visually identical black either way since neither
     * gl_mirror.c nor real wraith_gl.c enables GL_BLEND - this is about
     * this file being a well-defined RGBA32 buffer on its own terms,
     * not a rendering bug). */
    for (int fy = 0; fy < FRAME_H; fy++)
        for (int fx = 0; fx < FRAME_W; fx++)
            framebuf[fy][fx][3] = 255;
    int tile_y0 = HEADER_ROWS * GLYPH_H;
    if (render_mode == 1) {
        /* Real 3D perspective view - see render_3d_view()'s own header
         * comment above main() for the full writeup and named
         * simplifications. emoji_mode is irrelevant here (real terrain
         * colors either way, no flat-vs-emoji distinction in 3D) - and
         * this whole branch is a no-op in ops/compose_frame.c (ASCII),
         * matching mutaclsym's own convention that '0' only changes
         * what the GL view shows. */
        render_3d_view((unsigned char *)framebuf, FRAME_W, FRAME_H, grid, rows, map_w, px, py, camera_mode, facing, cam_pan_x, cam_pan_y, cam_pan_z);
    } else {
        for (int vr = 0; vr < VIEWPORT_H; vr++) {
            int src_row = cam_y + vr;
            int src_len = (src_row >= 0 && src_row < rows) ? (int)strlen(grid[src_row]) : 0;
            for (int vc = 0; vc < VIEWPORT_W; vc++) {
                int src_col = cam_x + vc;
                char glyph = (src_row >= 0 && src_row < rows && src_col < src_len) ? grid[src_row][src_col] : ' ';
                int r, g, b;
                glyph_to_rgb(glyph, emoji_mode, &r, &g, &b);
                int tile_x0 = vc * TILE_PX;
                int tile_y0_px = tile_y0 + vr * TILE_PX;
                for (int py2 = 0; py2 < TILE_PX; py2++) {
                    int fy = tile_y0_px + py2;
                    for (int px2 = 0; px2 < TILE_PX; px2++) {
                        int fx = tile_x0 + px2;
                        framebuf[fy][fx][0] = (unsigned char)r;
                        framebuf[fy][fx][1] = (unsigned char)g;
                        framebuf[fy][fx][2] = (unsigned char)b;
                        framebuf[fy][fx][3] = 255;
                    }
                }
                /* Real rasterized emoji pixels on top of the flat base
                 * color - see blit_emoji_tile()'s own header comment for
                 * the full pipeline. */
                if (emoji_mode && src_row >= 0 && src_row < rows && src_col < src_len && cell_asset[src_row][src_col][0]) {
                    unsigned char voxels[TILE_PX][TILE_PX][4];
                    if (load_emoji_voxels(cell_asset[src_row][src_col], voxels)) {
                        blit_emoji_tile(framebuf, tile_x0, tile_y0_px, voxels);
                    }
                }
            }
        }
    }

    /* Overlay panel (craft/inventory) - see draw_panel_box_gl()'s own
     * header comment. Content-building logic (recipe/inventory row
     * formatting) is a direct copy of compose_frame.c's own main() -
     * same registry files, same "ready"/"missing" check, same trailing
     * Cancel/Close row convention - only the final draw call differs
     * (pixels instead of characters). */
    char active_panel[32];
    read_kv_str(hero_path, "active_panel", active_panel, sizeof(active_panel), "none");
    if (strcmp(active_panel, "craft") == 0) {
        int panel_cursor = read_kv_int(hero_path, "panel_cursor", 1);
        char recipes_path[PATH_BUF];
        snprintf(recipes_path, sizeof(recipes_path), "%s/pieces/registry/recipes/recipes.txt", project_root);
        FILE *rf = fopen(recipes_path, "r");
        char panel_rows[MAX_RECIPES_DISPLAY][BOX_TEXT_W + 1];
        int panel_row_count = 0;
        if (rf) {
            char rline[MAX_LINE];
            while (panel_row_count < MAX_RECIPES_DISPLAY && fgets(rline, sizeof(rline), rf)) {
                if (rline[0] == '#' || rline[0] == '\n') continue;
                rline[strcspn(rline, "\n")] = '\0';
                char *p1 = strchr(rline, '|');
                if (!p1) continue;
                *p1 = '\0';
                char *rname = p1 + 1;
                char *p2 = strchr(rname, '|');
                if (!p2) continue;
                *p2 = '\0';
                char *presult = p2 + 1;
                char *p3 = strchr(presult, '|');
                if (!p3) continue;
                *p3 = '\0';
                char *reqs_str = p3 + 1;

                int satisfied = 1;
                char reqs_copy[MAX_LINE];
                snprintf(reqs_copy, sizeof(reqs_copy), "%s", reqs_str);
                char *tok = strtok(reqs_copy, ",");
                while (tok) {
                    char *colon = strchr(tok, ':');
                    if (colon) {
                        *colon = '\0';
                        if (count_in_inventory(tok) < atoi(colon + 1)) satisfied = 0;
                    }
                    tok = strtok(NULL, ",");
                }

                int idx = panel_row_count + 1;
                const char *cur = (idx == panel_cursor) ? "[>]" : "[ ]";
                snprintf(panel_rows[panel_row_count], sizeof(panel_rows[0]), "%s %d. [%s]%s",
                         cur, idx, rname, satisfied ? " (ready)" : " (missing)");
                panel_row_count++;
            }
            fclose(rf);
        }
        int cancel_idx = panel_row_count + 1;
        const char *cancel_cur = (cancel_idx == panel_cursor) ? "[>]" : "[ ]";
        char cancel_row[BOX_TEXT_W + 1];
        snprintf(cancel_row, sizeof(cancel_row), "%s %d. [Cancel]", cancel_cur, cancel_idx);

        draw_panel_box_gl(framebuf, tile_y0, "CRAFT", panel_rows, panel_row_count, cancel_row);
    } else if (strcmp(active_panel, "inventory") == 0) {
        int panel_cursor = read_kv_int(hero_path, "panel_cursor", 1);
        char inventory_dir_panel[PATH_BUF];
        snprintf(inventory_dir_panel, sizeof(inventory_dir_panel), "%s/pieces/world_tempt_island/map_start/hero/inventory", project_root);
        char panel_rows[MAX_RECIPES_DISPLAY][BOX_TEXT_W + 1];
        int panel_row_count = 0;
        DIR *pd = opendir(inventory_dir_panel);
        if (pd) {
            struct dirent *entry;
            while (panel_row_count < MAX_RECIPES_DISPLAY && (entry = readdir(pd)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                char state_path[PATH_BUF + 384];
                snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir_panel, entry->d_name);
                char item_id[64];
                read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");

                char iname[64], category[16], power_str[16];
                item_registry_field(item_id, 1, iname, sizeof(iname), item_id);
                item_registry_field(item_id, 2, category, sizeof(category), "?");
                item_registry_field(item_id, 5, power_str, sizeof(power_str), "0");
                int power = atoi(power_str);

                char detail[24] = "";
                if (strcmp(category, "weapon") == 0) snprintf(detail, sizeof(detail), " dmg+%d", power);
                else if (strcmp(category, "food") == 0) snprintf(detail, sizeof(detail), " hunger-%d", power);
                else if (strcmp(category, "drink") == 0) snprintf(detail, sizeof(detail), " thirst-%d", power);
                else if (strcmp(category, "armor") == 0) snprintf(detail, sizeof(detail), " def+%d", power);

                int idx = panel_row_count + 1;
                const char *cur = (idx == panel_cursor) ? "[>]" : "[ ]";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(panel_rows[panel_row_count], sizeof(panel_rows[0]), "%s %d. [%s] (%s)%s",
                         cur, idx, iname, category, detail);
#pragma GCC diagnostic pop
                panel_row_count++;
            }
            closedir(pd);
        }
        int cancel_idx = panel_row_count + 1;
        const char *cancel_cur = (cancel_idx == panel_cursor) ? "[>]" : "[ ]";
        char cancel_row[BOX_TEXT_W + 1];
        snprintf(cancel_row, sizeof(cancel_row), "%s %d. [Close]", cancel_cur, cancel_idx);
        if (panel_row_count == 0) {
            snprintf(panel_rows[0], sizeof(panel_rows[0]), "[ ] (nothing carried)");
            panel_row_count = 1;
        }

        draw_panel_box_gl(framebuf, tile_y0, "INVENTORY", panel_rows, panel_row_count, cancel_row);
    }

    /* Nav/HUD text - the font pipeline's whole point. White on the
     * framebuffer's default-black background, same as compose_frame.c's
     * ASCII header/stat lines carry the same information. */
    char header_text[160];
    if (z != 0) {
        snprintf(header_text, sizeof(header_text), "TEMPT ISLAND  map: %-10s turn: %d  Floor: %d", map_id, turn, z);
    } else {
        snprintf(header_text, sizeof(header_text), "TEMPT ISLAND  map: %-10s turn: %d", map_id, turn);
    }
    blit_text(framebuf, 0, 0, header_text, 255, 255, 255);

    char footer_text[96];
    snprintf(footer_text, sizeof(footer_text), "HP:%d Hunger:%d Thirst:%d Stamina:%d",
             hp, hunger, thirst, stamina);
    blit_text(framebuf, 0, tile_y0 + VIEWPORT_H * TILE_PX, footer_text, 255, 255, 255);

    char action_footer[512];
    build_action_footer(project_root, action_footer, sizeof(action_footer), action_cursor, interact_mode, emoji_mode);
    blit_text(framebuf, 0, tile_y0 + VIEWPORT_H * TILE_PX + GLYPH_H, action_footer, 255, 255, 255);

    size_t byte_count = (size_t)FRAME_W * FRAME_H * 4;
    FILE *out = fopen(out_path, "wb");
    if (!out) return 1;
    size_t written = fwrite(framebuf, 1, byte_count, out);
    fclose(out);
    if (written != byte_count) return 1;

    uint64_t checksum = checksum_buffer((const unsigned char *)framebuf, byte_count);
    write_receipt(receipt_path, byte_count, checksum, map_w, map_h, cam_x, cam_y);

    return 0;
}
