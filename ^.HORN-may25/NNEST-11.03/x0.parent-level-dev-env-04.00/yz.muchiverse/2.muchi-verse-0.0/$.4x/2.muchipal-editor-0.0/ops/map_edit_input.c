/* map_edit_input - one verb, one binary, no shared headers. Owns the
 * "map_edit" screen exclusively - self-filters (no-ops) whenever
 * editor_state.txt's screen isn't "map_edit", same convention
 * ops/project_browser.c uses in reverse (it no-ops FOR this screen).
 * Genuinely different input shape from the rest of the editor:
 * continuous cursor movement + glyph-arming + place-and-save, not
 * digit-accumulator menu navigation - real 1.TPMOS's own
 * wraith_project_input.c/piececraft-3d_manager.c draw exactly this
 * same line (a dedicated "map control" input mode, ESC to leave it),
 * so that's the precedent followed here, not invented fresh.
 *
 * Reads the CURRENT project's map + registry using whichever of the
 * two real, differently-shaped formats this family has produced so
 * far (known_projects.txt's registry_format field, set by
 * project_browser.c when a project is opened - never guessed):
 *   pipe   - mutaclsym's terrain_types.txt: one file,
 *            "glyph|id|name|walkable|rgb_top" per row.
 *   equals - piececraft-3d-pal's registry.txt: "glyph=id" per row (no
 *            per-glyph label beyond the id itself - good enough for a
 *            numbered legend).
 * This is the actual test of "compatible ops across differently-shaped
 * real content" the whole muchi-verse effort has been aiming at - one
 * editor screen, two real projects, two real tile-registry shapes.
 *
 * Controls: arrows move the cursor; digits 1-9 arm which registry row
 * to place; Enter places the armed glyph at the cursor AND SAVES
 * map.txt immediately (no separate "save" step - matches this
 * family's existing "every edit persists to disk right away" norm,
 * e.g. mutaclsym's save_game.c, keyboard_input.c's history.txt); ESC
 * (27) leaves map_edit back to project_menu.
 *
 * 'b' (added 2026-07-21, sec. 3.2 of MUCHIPAL-EDITOR-DESIGN.txt)
 * toggles arm_mode between "terrain" (the above) and "bank" - only for
 * a project whose known_projects.txt row has a real bank_rel_path
 * (mutaclsym has none and this key simply no-ops for it). In bank
 * mode, digits arm a *_bank.txt row instead of a terrain glyph, and
 * Enter shells out to that project's OWN spawn_<kind>.+x (with
 * PRISC_PROJECT_ROOT set to the TARGET project, not this editor) to
 * create a real instance directory at the cursor position - the same
 * bank-vs-instance-dir machinery every $.4x game project already uses
 * to spawn units/anglers/players/entities, just triggered from the
 * editor instead of from within that project's own running game.
 *
 * 'v' (added 2026-07-21) toggles arm_mode between "terrain" and
 * "event" - see the event editor's own header comment further down
 * (above main()) for the full "Op Selection builder" reasoning: a
 * fixed, editor-side catalog of 3 canned event types placed onto
 * tiles, author-time only (not yet read by any project's own runtime
 * dispatch code).
 *
 * Usage: map_edit_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MAP_W 128
#define MAX_MAP_H 64
#define MAX_REG_ROWS 32

#define ARROW_LEFT 1000
#define ARROW_RIGHT 1001
#define ARROW_UP 1002
#define ARROW_DOWN 1003

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

typedef struct {
    char screen[16];
    int cursor, digit_accum;
    char proj_name[64], proj_path[PATH_BUF];
    char map_rel_path[256], registry_format[16], registry_rel_path[256];
    char piece_pdl_path[PATH_BUF];
    int cursor_x, cursor_y, armed_idx;
    /* Instance placement (added 2026-07-21, sec. 3.2) - set by
     * project_browser.c from known_projects.txt's own extended schema
     * when a project is opened; empty/0 for a project with no
     * *_bank.txt (mutaclsym), which simply disables the 'b' toggle
     * below. */
    char bank_rel_path[256], instance_dir[64], spawn_op_rel_path[128];
    int has_civ_id;
    char arm_mode[8];
    int emoji_mode;
} EditorState;

static void state_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/system/editor_state.txt", project_root);
}

static void load_state(EditorState *st) {
    memset(st, 0, sizeof(*st));
    snprintf(st->screen, sizeof(st->screen), "title");
    char path[PATH_BUF];
    state_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line, *val = eq + 1;
        if (strcmp(key, "screen") == 0) snprintf(st->screen, sizeof(st->screen), "%s", val);
        else if (strcmp(key, "cursor") == 0) st->cursor = atoi(val);
        else if (strcmp(key, "digit_accum") == 0) st->digit_accum = atoi(val);
        else if (strcmp(key, "proj_name") == 0) snprintf(st->proj_name, sizeof(st->proj_name), "%s", val);
        else if (strcmp(key, "proj_path") == 0) snprintf(st->proj_path, sizeof(st->proj_path), "%s", val);
        else if (strcmp(key, "map_rel_path") == 0) snprintf(st->map_rel_path, sizeof(st->map_rel_path), "%s", val);
        else if (strcmp(key, "registry_format") == 0) snprintf(st->registry_format, sizeof(st->registry_format), "%s", val);
        else if (strcmp(key, "registry_rel_path") == 0) snprintf(st->registry_rel_path, sizeof(st->registry_rel_path), "%s", val);
        else if (strcmp(key, "piece_pdl_path") == 0) snprintf(st->piece_pdl_path, sizeof(st->piece_pdl_path), "%s", val);
        else if (strcmp(key, "cursor_x") == 0) st->cursor_x = atoi(val);
        else if (strcmp(key, "cursor_y") == 0) st->cursor_y = atoi(val);
        else if (strcmp(key, "armed_idx") == 0) st->armed_idx = atoi(val);
        else if (strcmp(key, "bank_rel_path") == 0) snprintf(st->bank_rel_path, sizeof(st->bank_rel_path), "%s", val);
        else if (strcmp(key, "instance_dir") == 0) snprintf(st->instance_dir, sizeof(st->instance_dir), "%s", val);
        else if (strcmp(key, "spawn_op_rel_path") == 0) snprintf(st->spawn_op_rel_path, sizeof(st->spawn_op_rel_path), "%s", val);
        else if (strcmp(key, "has_civ_id") == 0) st->has_civ_id = atoi(val);
        else if (strcmp(key, "arm_mode") == 0) snprintf(st->arm_mode, sizeof(st->arm_mode), "%s", val);
        else if (strcmp(key, "emoji_mode") == 0) st->emoji_mode = atoi(val);
    }
    fclose(f);
    if (!st->arm_mode[0]) snprintf(st->arm_mode, sizeof(st->arm_mode), "terrain");
}

static void save_state(const EditorState *st) {
    char path[PATH_BUF];
    state_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "screen=%s\n", st->screen);
    fprintf(f, "cursor=%d\n", st->cursor);
    fprintf(f, "digit_accum=%d\n", st->digit_accum);
    fprintf(f, "proj_name=%s\n", st->proj_name);
    fprintf(f, "proj_path=%s\n", st->proj_path);
    fprintf(f, "map_rel_path=%s\n", st->map_rel_path);
    fprintf(f, "registry_format=%s\n", st->registry_format);
    fprintf(f, "registry_rel_path=%s\n", st->registry_rel_path);
    fprintf(f, "piece_pdl_path=%s\n", st->piece_pdl_path);
    fprintf(f, "cursor_x=%d\n", st->cursor_x);
    fprintf(f, "cursor_y=%d\n", st->cursor_y);
    fprintf(f, "armed_idx=%d\n", st->armed_idx);
    fprintf(f, "bank_rel_path=%s\n", st->bank_rel_path);
    fprintf(f, "instance_dir=%s\n", st->instance_dir);
    fprintf(f, "spawn_op_rel_path=%s\n", st->spawn_op_rel_path);
    fprintf(f, "has_civ_id=%d\n", st->has_civ_id);
    fprintf(f, "arm_mode=%s\n", st->arm_mode);
    fprintf(f, "emoji_mode=%d\n", st->emoji_mode);
    fclose(f);
}

/* Loads glyph+label rows from either real registry format - see this
 * file's header. reg_labels rows are "glyph name" (pipe format's real
 * name field) or "glyph id" (equals format has no separate label). */
static int load_registry_rows(const char *proj_path, const char *registry_rel_path,
                               const char *format, char glyphs[MAX_REG_ROWS], char labels[MAX_REG_ROWS][64]) {
    char path[PATH_BUF + 256];
    snprintf(path, sizeof(path), "%s/%s", proj_path, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAX_LINE];
    int is_pipe = (strcmp(format, "pipe") == 0);
    while (n < MAX_REG_ROWS && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        char sep = is_pipe ? '|' : '=';
        /* A real comment line here always has a SPACE right after '#'
         * (both registries' own header rows are "# free text"); a real
         * data row has the separator character immediately after the
         * one-char glyph - checking for that, not just line[0]=='#',
         * is what correctly keeps '#' itself usable as a glyph (both
         * mutaclsym's t_wall and piececraft's wall row do exactly
         * this) - see GRAND-ARCHITECTURE.md §0b/§0c for the two prior
         * times this exact bug was found and fixed elsewhere. */
        if (line[0] == '#' && line[1] != sep) continue;
        if (line[1] != sep) continue;
        glyphs[n] = line[0];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(labels[n], 64, "%s", line + 2);
#pragma GCC diagnostic pop
        n++;
    }
    fclose(f);
    return n;
}

static int load_map(const char *proj_path, const char *map_rel_path,
                     char grid[MAX_MAP_H][MAX_MAP_W + 1], int *width) {
    char path[PATH_BUF + 256];
    snprintf(path, sizeof(path), "%s/%s", proj_path, map_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int rows = 0;
    *width = 0;
    char line[MAX_MAP_W + 4];
    while (rows < MAX_MAP_H && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(grid[rows], sizeof(grid[0]), "%s", line);
#pragma GCC diagnostic pop
        int len = (int)strlen(grid[rows]);
        if (len > *width) *width = len;
        rows++;
    }
    fclose(f);
    return rows;
}

static void save_map(const char *proj_path, const char *map_rel_path,
                      char grid[MAX_MAP_H][MAX_MAP_W + 1], int rows) {
    char path[PATH_BUF + 256], tmp[PATH_BUF + 264];
    snprintf(path, sizeof(path), "%s/%s", proj_path, map_rel_path);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int r = 0; r < rows; r++) fprintf(f, "%s\n", grid[r]);
    fclose(f);
    rename(tmp, path);
}

/* bank row: id|name|glyph|... (see e.g. unit_bank.txt/angler_bank.txt/
 * player_bank.txt/entity_bank.txt's own header comments - all 4 share
 * this same "id|name|glyph|color|category|..." shape). Only id/name/
 * glyph are needed here - spawn_<kind>.+x re-reads the full row itself
 * from its OWN hardcoded bank path once given the id. */
static int load_bank_rows(const char *proj_path, const char *bank_rel_path,
                           char ids[][64], char labels[][64], char glyphs[][8], int max_rows) {
    if (!bank_rel_path[0]) return 0;
    char path[PATH_BUF + 256];
    snprintf(path, sizeof(path), "%s/%s", proj_path, bank_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAX_LINE];
    while (n < max_rows && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0';
        char buf[MAX_LINE];
        snprintf(buf, sizeof(buf), "%s", line);
        char *fields[10] = {0};
        int nf = 0;
        char *tok = strtok(buf, "|");
        while (tok && nf < 10) { fields[nf++] = tok; tok = strtok(NULL, "|"); }
        if (nf < 3) continue;
        snprintf(ids[n], 64, "%s", fields[0]);
        snprintf(labels[n], 64, "%s", fields[1]);
        snprintf(glyphs[n], 8, "%s", fields[2]);
        n++;
    }
    fclose(f);
    return n;
}

/* "pieces/world_NAME/map_start/map.txt" -> "world_NAME" - the world
 * directory component spawn_<kind>.+x's own <world_dir> argument
 * expects (confirmed against all 4 $.4x game projects' real spawn ops -
 * they build "pieces/<world_dir>/map_start/<instance_dir>/<id>" from
 * it directly). */
static void world_dir_from_map_rel(const char *map_rel_path, char *out, size_t out_sz) {
    out[0] = '\0';
    const char *p = strstr(map_rel_path, "pieces/");
    if (!p) return;
    p += 7;
    const char *slash = strchr(p, '/');
    if (!slash) return;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out, out_sz, "%.*s", (int)(slash - p), p);
#pragma GCC diagnostic pop
}

/* Singular prefix from "ops/+x/spawn_unit.+x" -> "unit" - used to
 * generate the next free instance_id (unit_01, unit_02, ... matching
 * the naming convention already live in every real spawned instance
 * this session created, e.g. civ-pal's unit_01/unit_02/unit_03). */
static void singular_from_spawn_op(const char *spawn_op_rel_path, char *out, size_t out_sz) {
    out[0] = '\0';
    const char *p = strstr(spawn_op_rel_path, "spawn_");
    if (!p) return;
    p += 6;
    const char *dot = strstr(p, ".+x");
    if (!dot) return;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out, out_sz, "%.*s", (int)(dot - p), p);
#pragma GCC diagnostic pop
}

static void next_instance_id(const char *proj_path, const char *world_dir, const char *instance_dir,
                              const char *singular, char *out, size_t out_sz) {
    for (int i = 1; i < 1000; i++) {
        char candidate[128], path[PATH_BUF + 256];
        snprintf(candidate, sizeof(candidate), "%s_%02d", singular, i);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(path, sizeof(path), "%s/pieces/%s/map_start/%s/%s", proj_path, world_dir, instance_dir, candidate);
#pragma GCC diagnostic pop
        /* Linux fopen("r") on a directory path succeeds (reads would
         * fail with EISDIR, but the open itself doesn't) - exactly
         * what's needed here: treat an existing instance directory as
         * "taken" without a separate stat()/opendir() call. */
        FILE *f = fopen(path, "r");
        if (f) { fclose(f); continue; }
        snprintf(out, out_sz, "%s", candidate);
        return;
    }
    snprintf(out, out_sz, "%s_%02d", singular, 999);
}

/* Event editor (added 2026-07-21) - the "Op Selection builder" half of
 * op-ed's own real roadmap (README.md's Phase A: "an Op Selection
 * builder... rather than the author hand-writing PRISC assembly"),
 * scaled down to fit what this engine can actually do today. This
 * whole prisc+x/pal stack has NO free-text keyboard entry anywhere (no
 * project in this family has ever built one) - so instead of pretending
 * to support arbitrary event scripts, this is a small FIXED catalog of
 * canned op types, chosen by digit, same shape as choosing a terrain
 * glyph or a bank row. Author-time only: this writes a real events.txt
 * into the TARGET project's own world dir (never a lie - "if it's not
 * in a file, it's a lie"), but NO game project's own choice.c/
 * move_player.c currently reads it back and triggers anything at
 * runtime - that wiring is a real, separate, larger follow-up (would
 * need touching all 4 $.4x game projects' own dispatch code), not done
 * this pass. This proves the placement+storage+rendering pipeline
 * end-to-end; runtime triggering is the next real step, not a lie by
 * omission - explicitly named here and in the design doc. */
#define EVENT_CATALOG_COUNT 3
static const char *EVENT_CATALOG_IDS[EVENT_CATALOG_COUNT] = { "message", "teleport", "spawn" };
/* Labels for this same catalog are duplicated (not shared, per this
 * project's own "no shared headers" convention) in
 * ops/compose_title_frame.c and ops/compose_rgb_frame.c, whichever
 * actually renders the legend - this op only needs the ids. */

static void events_path_for(const char *proj_path, const char *world_dir, char *out, size_t out_sz) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out, out_sz, "%s/pieces/%s/map_start/events.txt", proj_path, world_dir);
#pragma GCC diagnostic pop
}

/* One row per placed event: x|y|op_id - loaded fresh each keypress
 * (same "everything lives in a file, not memory" convention as every
 * other op in this family, since each keypress is a fresh short-lived
 * process). */
#define MAX_EVENTS 256
typedef struct { int x, y; char op_id[32]; } EventRow;

static int load_events(const char *path, EventRow rows[MAX_EVENTS]) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAX_LINE];
    while (n < MAX_EVENTS && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#') continue;
        int x = 0, y = 0;
        char op_id[32];
        if (sscanf(line, "%d|%d|%31[^|\n]", &x, &y, op_id) == 3) {
            rows[n].x = x; rows[n].y = y;
            snprintf(rows[n].op_id, sizeof(rows[n].op_id), "%s", op_id);
            n++;
        }
    }
    fclose(f);
    return n;
}

/* Places (or replaces, if one already exists at that tile) one event
 * row - mkdir_p not needed, the world dir already exists (the map
 * itself lives there). */
static void save_event(const char *path, int cursor_x, int cursor_y, const char *op_id) {
    EventRow rows[MAX_EVENTS];
    int n = load_events(path, rows);
    int replaced = 0;
    for (int i = 0; i < n; i++) {
        if (rows[i].x == cursor_x && rows[i].y == cursor_y) {
            snprintf(rows[i].op_id, sizeof(rows[i].op_id), "%s", op_id);
            replaced = 1;
            break;
        }
    }
    if (!replaced && n < MAX_EVENTS) {
        rows[n].x = cursor_x; rows[n].y = cursor_y;
        snprintf(rows[n].op_id, sizeof(rows[n].op_id), "%s", op_id);
        n++;
    }
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# events placed via muchipal-editor - x|y|op_id (see map_edit_input.c's\n");
    fprintf(f, "# own header comment: author-time only, not yet read by this project's\n");
    fprintf(f, "# own runtime dispatch code)\n");
    for (int i = 0; i < n; i++) fprintf(f, "%d|%d|%s\n", rows[i].x, rows[i].y, rows[i].op_id);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    EditorState st;
    load_state(&st);
    if (strcmp(st.screen, "map_edit") != 0) return 0; /* owned by project_browser.c */

    char grid[MAX_MAP_H][MAX_MAP_W + 1];
    int width = 0;
    int rows = load_map(st.proj_path, st.map_rel_path, grid, &width);
    if (rows < 1) rows = 1;
    if (width < 1) width = 1;

    char reg_glyphs[MAX_REG_ROWS];
    char reg_labels[MAX_REG_ROWS][64];
    int reg_count = load_registry_rows(st.proj_path, st.registry_rel_path, st.registry_format, reg_glyphs, reg_labels);

    char bank_ids[MAX_REG_ROWS][64], bank_labels[MAX_REG_ROWS][64], bank_glyphs[MAX_REG_ROWS][8];
    int bank_count = load_bank_rows(st.proj_path, st.bank_rel_path, bank_ids, bank_labels, bank_glyphs, MAX_REG_ROWS);
    int in_bank_mode = (strcmp(st.arm_mode, "bank") == 0) && bank_count > 0;
    int in_event_mode = (strcmp(st.arm_mode, "event") == 0);

    if (key == ARROW_LEFT) { if (st.cursor_x > 0) st.cursor_x--; }
    else if (key == ARROW_RIGHT) { if (st.cursor_x < width - 1) st.cursor_x++; }
    else if (key == ARROW_UP) { if (st.cursor_y > 0) st.cursor_y--; }
    else if (key == ARROW_DOWN) { if (st.cursor_y < rows - 1) st.cursor_y++; }
    else if (key == 'b' && st.bank_rel_path[0]) {
        /* Toggle terrain/bank arm mode (added 2026-07-21, sec. 3.2) -
         * only available for a project with a real *_bank.txt; no-ops
         * otherwise (checked above via st.bank_rel_path[0]). */
        if (strcmp(st.arm_mode, "bank") == 0) snprintf(st.arm_mode, sizeof(st.arm_mode), "terrain");
        else snprintf(st.arm_mode, sizeof(st.arm_mode), "bank");
        st.armed_idx = 0;
    } else if (key == 'v') {
        /* Toggle terrain/event arm mode - see the event editor's own
         * header comment above main() for the full "Op Selection
         * builder" reasoning. Available for every project (the event
         * catalog is fixed/editor-side, unlike bank rows which need a
         * real project *_bank.txt). */
        if (strcmp(st.arm_mode, "event") == 0) snprintf(st.arm_mode, sizeof(st.arm_mode), "terrain");
        else snprintf(st.arm_mode, sizeof(st.arm_mode), "event");
        st.armed_idx = 0;
    } else if (key == 'e') {
        /* ASCII<->emoji color toggle (added 2026-07-21) - matches
         * mutaclsym/muchimon-pal's own emoji_mode preference, ported
         * for the GL/RGB mirror only (color field, terrain_types.txt's
         * own rgb_top_emoji column) - see ops/compose_rgb_frame.c's
         * own header comment for why the ASCII glyph-swap half isn't
         * done this pass (needs a parallel per-cell string buffer, not
         * a safe fixed-width char substitution). */
        st.emoji_mode = !st.emoji_mode;
    } else if (key >= '1' && key <= '9') {
        int idx = key - '1';
        int count = in_event_mode ? EVENT_CATALOG_COUNT : (in_bank_mode ? bank_count : reg_count);
        if (idx < count) st.armed_idx = idx;
    } else if (key == 10 || key == 13) {
        if (in_event_mode) {
            if (st.armed_idx < EVENT_CATALOG_COUNT) {
                char world_dir[64], ev_path[PATH_BUF + 256];
                world_dir_from_map_rel(st.map_rel_path, world_dir, sizeof(world_dir));
                events_path_for(st.proj_path, world_dir, ev_path, sizeof(ev_path));
                save_event(ev_path, st.cursor_x, st.cursor_y, EVENT_CATALOG_IDS[st.armed_idx]);
            }
        } else if (in_bank_mode) {
            if (bank_count > 0 && st.armed_idx < bank_count) {
                char world_dir[64], singular[64], instance_id[128];
                world_dir_from_map_rel(st.map_rel_path, world_dir, sizeof(world_dir));
                singular_from_spawn_op(st.spawn_op_rel_path, singular, sizeof(singular));
                next_instance_id(st.proj_path, world_dir, st.instance_dir, singular, instance_id, sizeof(instance_id));

                /* civ_id defaults to a fixed "editor" placeholder for
                 * projects whose spawn op takes one (civ-pal/pal-craft/
                 * muchimon-pal) - a real, named simplification: this
                 * editor has no concept of "which civ/player owns this"
                 * beyond a single fixed value; a future "Choose Civ"
                 * sub-screen is a real, separate follow-up, not built
                 * here. angler-empires' spawn_angler.c takes no civ_id
                 * argument at all (st.has_civ_id==0), so it's omitted. */
                char cmd[(PATH_BUF * 2) + 512];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                if (st.has_civ_id) {
                    snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/%s' '%s' '%s' '%s' editor %d %d",
                             st.proj_path, st.proj_path, st.spawn_op_rel_path, bank_ids[st.armed_idx],
                             instance_id, world_dir, st.cursor_x, st.cursor_y);
                } else {
                    snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/%s' '%s' '%s' '%s' %d %d",
                             st.proj_path, st.proj_path, st.spawn_op_rel_path, bank_ids[st.armed_idx],
                             instance_id, world_dir, st.cursor_x, st.cursor_y);
                }
#pragma GCC diagnostic pop
                if (system(cmd) != 0) { /* spawn op's own stderr already reports failure reasons */ }
            }
        } else if (reg_count > 0 && st.armed_idx < reg_count &&
            st.cursor_y >= 0 && st.cursor_y < rows &&
            st.cursor_x >= 0 && st.cursor_x < (int)strlen(grid[st.cursor_y])) {
            grid[st.cursor_y][st.cursor_x] = reg_glyphs[st.armed_idx];
            save_map(st.proj_path, st.map_rel_path, grid, rows);
        }
    } else if (key == 27) {
        snprintf(st.screen, sizeof(st.screen), "project_menu");
        st.cursor = 1;
        st.digit_accum = 0;
    }

    save_state(&st);
    return 0;
}
