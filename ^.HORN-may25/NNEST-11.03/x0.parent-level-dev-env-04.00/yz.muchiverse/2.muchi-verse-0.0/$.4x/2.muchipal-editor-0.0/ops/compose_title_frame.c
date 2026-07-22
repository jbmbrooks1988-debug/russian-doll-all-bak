/* compose_title_frame - one verb, one binary, no shared headers.
 * Renders whichever of the editor's four screens (title/projects/
 * pieces/piece_detail) is currently active into
 * pieces/display/current_frame.txt - same bracket-cursor numbered-list
 * convention used throughout this whole project family (see
 * nav-refactor-2.txt / mutaclsym's compose_frame.c / egg-pals'
 * compose_menu.c, all cited there). The piece_detail screen is the
 * concrete proof of cross-project compatibility: it calls the SHARED
 * ops/+x/pdl_reader.+x (yz.muchiverse/2.muchi-verse/shared-ops/
 * pdl_reader.c - see that dir's own shared-ops-refactor-plan.txt)
 * with `list_methods_full`, against a REAL piece.pdl absolute path
 * belonging to whatever external project (mutaclsym, later others)
 * the user opened, and displays its actual METHOD table - not mock
 * data. This used to call a private, per-project piece_viewer.+x
 * whose parser was a third independent retyping of pdl_reader.c's own
 * parse_method_line() - retired once pdl_reader.c itself was
 * genericized to accept an absolute path and print the same
 * "name|handler" shape via list_methods_full. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_ROWS 64
#define BOX_W 60

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

static void border(FILE *out) {
    fputc('+', out);
    for (int i = 0; i < BOX_W; i++) fputc('=', out);
    fputc('+', out);
    fputc('\n', out);
}

static void line(FILE *out, const char *content) {
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    fprintf(out, "|%.*s", len, content);
    for (int i = len; i < BOX_W; i++) fputc(' ', out);
    fputc('|', out);
    fputc('\n', out);
}

static void blank(FILE *out) { line(out, ""); }

/* Event overlay (added 2026-07-21) - see ops/map_edit_input.c's own
 * event-editor header comment (above its main()) for the full
 * reasoning. This is a read-only rendering concern: load placed
 * events.txt rows (x|y|op_id) so the map view can mark them with '!',
 * same "own copy, not shared" convention as everything else in this
 * project family. */
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

/* glyph's unicode field (field 5, 0-indexed: glyph|id|name|walkable|
 * rgb_top|unicode|rgb_top_emoji) from a "pipe" registry - the ASCII
 * half of the emoji toggle (added 2026-07-21; the GL/color half is in
 * ops/compose_rgb_frame.c's own glyph_rgb_top(), see that file's header
 * for the reference investigation). Same '#'-as-glyph-vs-comment
 * collision this whole family has hit before (line[1]!='|' check, not
 * just line[0]=='#') - see compose_rgb_frame.c's own header comment
 * for the live bug this exact class of mistake caused there. */
static int glyph_unicode_lookup(const char *reg_path, char glyph, char *out, size_t out_sz) {
    FILE *f = fopen(reg_path, "r");
    if (!f) return 0;
    char line_buf[MAX_LINE];
    int found = 0;
    while (fgets(line_buf, sizeof(line_buf), f)) {
        if (line_buf[0] == '\n') continue;
        if (line_buf[0] == '#' && line_buf[1] != '|') continue;
        if (line_buf[0] != glyph || line_buf[1] != '|') continue;
        char *p = line_buf;
        for (int field = 0; field < 5 && p; field++) p = strchr(p + 1, '|');
        if (!p) continue;
        p++;
        char *end = strchr(p, '|');
        if (end) *end = '\0';
        else p[strcspn(p, "\r\n")] = '\0';
        snprintf(out, out_sz, "%s", p);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

static void title(FILE *out, const char *text) {
    char spaced[BOX_W + 1] = "";
    int p = 0;
    for (const char *c = text; *c && p < BOX_W - 1; c++) {
        spaced[p++] = *c;
        if (c[1]) spaced[p++] = ' ';
    }
    spaced[p] = '\0';
    int pad = (BOX_W - (int)strlen(spaced)) / 2;
    char padded[BOX_W + 1] = "";
    for (int i = 0; i < pad && i < BOX_W; i++) padded[i] = ' ';
    snprintf(padded + (pad > 0 ? pad : 0), sizeof(padded) - (pad > 0 ? pad : 0), "%s", spaced);
    line(out, padded);
}

static void option(FILE *out, int index, int cursor, const char *label) {
    char buf[BOX_W + 1];
    const char *cur = index == cursor ? "[>]" : "[ ]";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(buf, sizeof(buf), "  %s %d. [%s]", cur, index, label);
#pragma GCC diagnostic pop
    line(out, buf);
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], out_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/editor_state.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/current_frame.txt", project_root);

    char screen[16], proj_name[64], proj_path[PATH_BUF], piece_pdl_path[PATH_BUF];
    read_kv_str(state_path, "screen", screen, sizeof(screen), "title");
    int cursor = read_kv_int(state_path, "cursor", 1);
    read_kv_str(state_path, "proj_name", proj_name, sizeof(proj_name), "");
    read_kv_str(state_path, "proj_path", proj_path, sizeof(proj_path), "");
    read_kv_str(state_path, "piece_pdl_path", piece_pdl_path, sizeof(piece_pdl_path), "");

    FILE *out = fopen(out_path, "w");
    if (!out) return 1;

    border(out);
    blank(out);
    title(out, "MUCHIPAL-EDITOR");
    blank(out);

    if (strcmp(screen, "title") == 0) {
        option(out, 1, cursor, "Open Project");
    } else if (strcmp(screen, "projects") == 0) {
        title(out, "PROJECTS");
        blank(out);
        char reg_path[PATH_BUF];
        snprintf(reg_path, sizeof(reg_path), "%s/pieces/registry/known_projects.txt", project_root);
        FILE *rf = fopen(reg_path, "r");
        int row = 0;
        if (rf) {
            char rline[MAX_LINE];
            while (fgets(rline, sizeof(rline), rf)) {
                if (rline[0] == '#' || rline[0] == '\n') continue;
                rline[strcspn(rline, "\r\n")] = '\0';
                char *p1 = strchr(rline, '|');
                if (!p1) continue;
                char *p2 = strchr(p1 + 1, '|');
                if (!p2) continue;
                *p2 = '\0';
                option(out, row + 1, cursor, p1 + 1);
                row++;
            }
            fclose(rf);
        }
        if (row == 0) line(out, "  (no known projects - edit pieces/registry/known_projects.txt)");
    } else if (strcmp(screen, "project_menu") == 0) {
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "PROJECT: %s", proj_name);
        title(out, hdr);
        blank(out);
        option(out, 1, cursor, "Browse Pieces");
        option(out, 2, cursor, "Edit Map");
        option(out, 3, cursor, "Play Project (runs its own button.sh run)");
    } else if (strcmp(screen, "map_edit") == 0) {
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "MAP EDIT: %s", proj_name);
        title(out, hdr);
        blank(out);

        char map_rel_path[256], registry_format[16], registry_rel_path[256];
        read_kv_str(state_path, "map_rel_path", map_rel_path, sizeof(map_rel_path), "");
        read_kv_str(state_path, "registry_format", registry_format, sizeof(registry_format), "pipe");
        read_kv_str(state_path, "registry_rel_path", registry_rel_path, sizeof(registry_rel_path), "");
        int cursor_x = read_kv_int(state_path, "cursor_x", 0);
        int cursor_y = read_kv_int(state_path, "cursor_y", 0);
        int armed_idx = read_kv_int(state_path, "armed_idx", 0);

        char map_path[PATH_BUF + 256];
        snprintf(map_path, sizeof(map_path), "%s/%s", proj_path, map_rel_path);

        int event_xs[MAX_EVENT_MARKERS], event_ys[MAX_EVENT_MARKERS];
        int event_count = load_event_coords(proj_path, map_rel_path, event_xs, event_ys);

        /* Camera fix (2026-07-21): this used to always print rows/cols
         * starting at 0,0 with a hard 24-row cutoff and no column clamp
         * at all - the exact same class of bug found and fixed
         * family-wide this session in the 4 $.4x game projects' own
         * compose_frame.c (see handoff-possession-4x.txt sec. 5 /
         * camera-fix.txt): a viewport that never scrolls, silently
         * clipping any map wider/taller than the box instead of
         * following the cursor. pal-craft's own map is 65 cols wide -
         * already bigger than this box's 60-col BOX_W, so this was a
         * real, live bug here too, not theoretical. Same clamp formula
         * as compose_frame.c's cam_x/cam_y (centered on the moving
         * anchor - here the edit cursor rather than xlector/a possessed
         * piece, since editing has no possession concept - clamped to
         * the map's own real bounds read from the file itself). */
        int map_w = 0, map_h = 0;
        {
            FILE *dimf = fopen(map_path, "r");
            if (dimf) {
                char dline[4096];
                while (fgets(dline, sizeof(dline), dimf)) {
                    dline[strcspn(dline, "\r\n")] = '\0';
                    int len = (int)strlen(dline);
                    if (len > map_w) map_w = len;
                    map_h++;
                }
                fclose(dimf);
            }
        }
        int view_w = BOX_W;
        int view_h = 16;
        int cam_x = cursor_x - view_w / 2;
        int cam_x_max = map_w - view_w;
        if (cam_x_max < 0) cam_x_max = 0;
        if (cam_x < 0) cam_x = 0;
        if (cam_x > cam_x_max) cam_x = cam_x_max;
        int cam_y = cursor_y - view_h / 2;
        int cam_y_max = map_h - view_h;
        if (cam_y_max < 0) cam_y_max = 0;
        if (cam_y < 0) cam_y = 0;
        if (cam_y > cam_y_max) cam_y = cam_y_max;

        int emoji_mode = read_kv_int(state_path, "emoji_mode", 0);
        char terrain_reg_path[PATH_BUF + 256] = "";
        if (emoji_mode) snprintf(terrain_reg_path, sizeof(terrain_reg_path), "%s/%s", proj_path, registry_rel_path);

        FILE *mf = fopen(map_path, "r");
        int row = 0;
        int shown = 0;
        if (mf) {
            char mline[BOX_W + 8];
            while (shown < view_h && fgets(mline, sizeof(mline), mf)) {
                mline[strcspn(mline, "\r\n")] = '\0';
                if (row < cam_y) { row++; continue; }
                int len = (int)strlen(mline);
                if (emoji_mode) {
                    /* Variable-width emoji row - NOT run through the
                     * fixed-width line()/border() helpers (a real
                     * unicode glyph is multiple bytes but one terminal
                     * column, so per-column byte accounting the way
                     * line() does it would misalign or overflow the
                     * box). Same "don't force strict column alignment
                     * in emoji mode" choice muchimon-pal's own
                     * compose_frame.c already made (its viewport_emoji
                     * cells are fputs'd raw, no width enforcement
                     * either) - not this editor's own invention. */
                    char rowbuf[(BOX_W * 4) + 32] = "|";
                    for (int i = cam_x; i < cam_x + view_w; i++) {
                        char ch = (i >= 0 && i < len) ? mline[i] : ' ';
                        char cell[16];
                        if (has_event_at(event_xs, event_ys, event_count, i, row))
                            snprintf(cell, sizeof(cell), "!");
                        else if (ch == ' ' || !glyph_unicode_lookup(terrain_reg_path, ch, cell, sizeof(cell)))
                            snprintf(cell, sizeof(cell), "%c", ch);
                        if (row == cursor_y && i == cursor_x) strncat(rowbuf, "[", sizeof(rowbuf) - strlen(rowbuf) - 1);
                        strncat(rowbuf, cell, sizeof(rowbuf) - strlen(rowbuf) - 1);
                        if (row == cursor_y && i == cursor_x) strncat(rowbuf, "]", sizeof(rowbuf) - strlen(rowbuf) - 1);
                    }
                    strncat(rowbuf, "|", sizeof(rowbuf) - strlen(rowbuf) - 1);
                    fprintf(out, "%s\n", rowbuf);
                    row++;
                    shown++;
                    continue;
                }
                /* Highlight the cursor cell with brackets - only cheap
                 * way to show position in a plain fixed-width text
                 * grid without touching the character underneath. */
                char marked[BOX_W + 8];
                int mi = 0;
                for (int i = cam_x; i < cam_x + view_w && mi < BOX_W - 2; i++) {
                    char ch = (i >= 0 && i < len) ? mline[i] : ' ';
                    if (has_event_at(event_xs, event_ys, event_count, i, row)) ch = '!';
                    if (row == cursor_y && i == cursor_x) {
                        marked[mi++] = '['; marked[mi++] = ch; marked[mi++] = ']';
                    } else {
                        marked[mi++] = ch;
                    }
                }
                marked[mi] = '\0';
                line(out, marked);
                row++;
                shown++;
            }
            fclose(mf);
        }
        if (shown == 0) line(out, "  (map file not found or empty)");

        blank(out);
        char arm_mode[8], bank_rel_path[256];
        read_kv_str(state_path, "arm_mode", arm_mode, sizeof(arm_mode), "terrain");
        read_kv_str(state_path, "bank_rel_path", bank_rel_path, sizeof(bank_rel_path), "");
        int in_bank_mode = (strcmp(arm_mode, "bank") == 0) && bank_rel_path[0];
        int in_event_mode = (strcmp(arm_mode, "event") == 0);

        if (in_event_mode) {
            /* Event editor legend (added 2026-07-21) - fixed 3-row
             * catalog, same "labels duplicated per op" convention as
             * ops/map_edit_input.c's own copy of the id list; see that
             * file's header comment above main() for the full
             * reasoning (op-ed's own "Op Selection builder" scaled to
             * what this engine can actually do - no free-text entry). */
            static const char *labels[3] = {
                "Message: \"Welcome!\"",
                "Teleport to (0,0)",
                "Spawn bank row #1 here on trigger"
            };
            line(out, "  [EVENT MODE - press 'v' to return to terrain]");
            for (int i = 0; i < 3; i++) {
                char legend[BOX_W + 1];
                const char *cur = (i == armed_idx) ? "[ARMED]" : "       ";
                snprintf(legend, sizeof(legend), "  %s %d. %s", cur, i + 1, labels[i]);
                line(out, legend);
            }
        } else if (in_bank_mode) {
            /* Bank/instance placement mode (added 2026-07-21, sec. 3.2)
             * - a real *_bank.txt row list (id|name|glyph|...) instead
             * of the terrain registry, so the legend always matches
             * whatever Enter is actually about to do (same "never let
             * the visible UI lie about what input will do" principle
             * as the game projects' own dispatch_id footer). */
            char bank_path[PATH_BUF + 256];
            snprintf(bank_path, sizeof(bank_path), "%s/%s", proj_path, bank_rel_path);
            FILE *bf = fopen(bank_path, "r");
            int bank_row = 0;
            line(out, "  [BANK MODE - press 'b' to return to terrain]");
            if (bf) {
                char bline[MAX_LINE];
                while (bank_row < 8 && fgets(bline, sizeof(bline), bf)) {
                    bline[strcspn(bline, "\r\n")] = '\0';
                    if (!bline[0] || bline[0] == '#') continue;
                    char buf[MAX_LINE];
                    snprintf(buf, sizeof(buf), "%s", bline);
                    char *fields[10] = {0};
                    int nf = 0;
                    char *tok = strtok(buf, "|");
                    while (tok && nf < 10) { fields[nf++] = tok; tok = strtok(NULL, "|"); }
                    if (nf < 3) continue;
                    char legend[BOX_W + 1];
                    const char *cur = (bank_row == armed_idx) ? "[ARMED]" : "       ";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                    snprintf(legend, sizeof(legend), "  %s %d. '%s' %s", cur, bank_row + 1, fields[2], fields[1]);
#pragma GCC diagnostic pop
                    line(out, legend);
                    bank_row++;
                }
                fclose(bf);
            }
            if (bank_row == 0) line(out, "  (bank registry not found or empty)");
        } else {
        char reg_path[PATH_BUF + 256];
        snprintf(reg_path, sizeof(reg_path), "%s/%s", proj_path, registry_rel_path);
        FILE *rf = fopen(reg_path, "r");
        int reg_row = 0;
        int is_pipe = (strcmp(registry_format, "pipe") == 0);
        char sep = is_pipe ? '|' : '=';
        if (rf) {
            char rline[MAX_LINE];
            while (reg_row < 9 && fgets(rline, sizeof(rline), rf)) {
                rline[strcspn(rline, "\r\n")] = '\0';
                if (!rline[0]) continue;
                if (rline[0] == '#' && rline[1] != sep) continue;
                if (rline[1] != sep) continue;
                char legend[BOX_W + 1];
                const char *cur = (reg_row == armed_idx) ? "[ARMED]" : "       ";
                /* pipe format's remainder is "id|name|walkable|rgb_top" -
                 * take just the name (2nd field) for a readable legend
                 * instead of the whole row; equals format's remainder
                 * is already just the id, used as-is. */
                char label[64];
                if (is_pipe) {
                    char *name_start = strchr(rline + 2, '|');
                    if (name_start) {
                        name_start++;
                        char *name_end = strchr(name_start, '|');
                        int len = name_end ? (int)(name_end - name_start) : (int)strlen(name_start);
                        if (len > 63) len = 63;
                        memcpy(label, name_start, (size_t)len);
                        label[len] = '\0';
                    } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                        snprintf(label, sizeof(label), "%s", rline + 2);
#pragma GCC diagnostic pop
                    }
                } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                    snprintf(label, sizeof(label), "%s", rline + 2);
#pragma GCC diagnostic pop
                }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(legend, sizeof(legend), "  %s %d. '%c' %s", cur, reg_row + 1, rline[0], label);
#pragma GCC diagnostic pop
                line(out, legend);
                reg_row++;
            }
            fclose(rf);
        }
        if (reg_row == 0) line(out, "  (registry not found or empty)");
        }
    } else if (strcmp(screen, "pieces") == 0) {
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "PIECES IN: %s", proj_name);
        title(out, hdr);
        blank(out);
        char cmd[PATH_BUF + 64];
        snprintf(cmd, sizeof(cmd), "find '%s/pieces' -name piece.pdl 2>/dev/null", proj_path);
        FILE *pf = popen(cmd, "r");
        int row = 0;
        if (pf) {
            char pline[PATH_BUF];
            while (fgets(pline, sizeof(pline), pf)) {
                pline[strcspn(pline, "\r\n")] = '\0';
                if (!pline[0]) continue;
                char *slash = strrchr(pline, '/');
                if (slash) *slash = '\0';
                char *id_slash = strrchr(pline, '/');
                option(out, row + 1, cursor, id_slash ? id_slash + 1 : pline);
                row++;
            }
            pclose(pf);
        }
        if (row == 0) line(out, "  (no piece.pdl files found under this project)");
    } else if (strcmp(screen, "piece_detail") == 0) {
        char *slash = strrchr(piece_pdl_path, '/');
        char piece_id[64];
        if (slash) {
            char tmp[PATH_BUF];
            snprintf(tmp, sizeof(tmp), "%s", piece_pdl_path);
            tmp[slash - piece_pdl_path] = '\0';
            char *id_slash = strrchr(tmp, '/');
            /* piece_id is genuinely short (a directory basename)
             * despite gcc only being able to prove it no longer than
             * tmp's own PATH_BUF - same class of warning suppressed
             * narrowly elsewhere in this project family. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(piece_id, sizeof(piece_id), "%s", id_slash ? id_slash + 1 : tmp);
#pragma GCC diagnostic pop
        } else {
            snprintf(piece_id, sizeof(piece_id), "%s", "?");
        }
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "PIECE: %s", piece_id);
        title(out, hdr);
        blank(out);
        line(out, "  METHOD table:");
        blank(out);

        char cmd[(PATH_BUF * 2) + 32];
        snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' '%s' list_methods_full", project_root, piece_pdl_path);
        FILE *pf = popen(cmd, "r");
        int row = 0;
        if (pf) {
            char pline[MAX_LINE];
            while (fgets(pline, sizeof(pline), pf)) {
                pline[strcspn(pline, "\r\n")] = '\0';
                char *bar = strchr(pline, '|');
                if (!bar) continue;
                *bar = '\0';
                char rowbuf[BOX_W + 1];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(rowbuf, sizeof(rowbuf), "  %d. %s -> %s", row, pline, bar + 1);
#pragma GCC diagnostic pop
                line(out, rowbuf);
                row++;
            }
            pclose(pf);
        }
        if (row == 0) line(out, "  (no METHOD rows found)");
        blank(out);
        option(out, 1, cursor, "Back");
    }

    blank(out);
    border(out);
    /* [NAV]/[ACTIVE] marker - same bracket convention the 4 $.4x game
     * projects use for interact_mode (sec 28/30/31/32 of !.pal-
     * standards.txt: "[>]" free-nav vs "[^]" active-control), applied
     * here to the editor's own already-real NAV(browser)/ACTIVE(map
     * edit) split (screen=title/projects/project_menu/pieces/
     * piece_detail vs screen=map_edit) - see MUCHIPAL-EDITOR-DESIGN.txt
     * sec. 3.4 for why the FULL possession/xlector machinery isn't
     * ported here (the editor's own arm-and-place loop already is its
     * own correct analogue); this is just the visual label made
     * consistent with the rest of the family, not new behavior. */
    if (strcmp(screen, "map_edit") == 0) {
        int emoji_mode = read_kv_int(state_path, "emoji_mode", 0);
        fprintf(out, "[ACTIVE] emoji=%s [arrows]move [1-9]arm [enter]place [b]bank [v]event [e]emoji [esc]back\n",
                emoji_mode ? "ON" : "OFF");
    } else {
        fprintf(out, "[NAV] [0-9] jump  [enter] select  [q] quit\n");
    }

    fclose(out);
    return 0;
}
