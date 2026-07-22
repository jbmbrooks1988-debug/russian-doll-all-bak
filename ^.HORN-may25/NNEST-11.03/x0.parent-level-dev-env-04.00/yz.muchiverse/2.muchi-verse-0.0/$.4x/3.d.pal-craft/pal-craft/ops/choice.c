/* choice - one verb, one binary, no shared headers.
 * The number-key dispatcher: prisc+x's pal scripts only have `beq`
 * (exact equality), no range comparison, so digit/Enter handling has to
 * live in a C op, not pal/main_loop.pal itself - see nav-refactor-2.txt
 * §3 for why. Runs UNCONDITIONALLY every tick, alongside move_player
 * (self-filters like it does), because it now owns persistent
 * accumulator state that must be reset on ANY key it doesn't recognize
 * as a digit or Enter, not just ignored.
 *
 * Ported faithfully from real 1.TPMOS's chtmp_parser.c (the actual live
 * renderer behind its numbered-panel menus - confirmed via its compiled
 * +x binary; the sibling chtmp_player.c has no equivalent digit
 * handling and no +x artifact, i.e. dead code): each digit keystroke
 * accumulates into a persisted digit_accum field (persisted in
 * hero/state.txt since every keypress here is a fresh short-lived
 * process, unlike chtmp_parser.c's one long-lived process with the
 * accumulator in memory), bounds-checked on every keystroke. A digit
 * only PREVIEWS a selection - it does NOT execute anything. Only Enter
 * commits. Matches real 1.TPMOS's do_jump()/process_key() split exactly
 * (digit keys only ever move focus_index; activation is Enter-only).
 *
 * Two independent accumulator "layers" share this one op, matching
 * real CDDA's own convention of a menu drawn ON TOP of the still-
 * visible map (researched via gl_desktop.c's project_mirror window
 * type - draws the map, then draws a menu list directly over it in the
 * same pass, no screen swap) rather than a full-screen mode switch:
 *
 *   - The outer action bar (action_cursor/digit_accum, valid range
 *     [2, total_methods) - piece.pdl rows 0/1 = move/end_turn are
 *     reserved, matching real 1.TPMOS's fuzz-op_manager.c route_input()
 *     convention of starting at 2) - active whenever hero/state.txt's
 *     active_panel field is "none".
 *   - An overlay PANEL (active_panel/panel_cursor/panel_digit_accum),
 *     active once active_panel != "none". Two panel types exist:
 *     "craft" (opened when Enter commits the outer bar's craft choice
 *     instead of executing it directly - lets the player pick WHICH
 *     satisfiable recipe to make instead of always getting the first
 *     satisfiable one; committing execs craft.+x with the chosen
 *     recipe_id) and "inventory" (opened the same way from the outer
 *     bar's examine choice - a read-only browse of hero/inventory/,
 *     Enter never execs anything on any row, only closes the panel).
 *     compose_frame.c reads active_panel the same way and draws each
 *     panel's own bracket-numbered list over a sub-rectangle of the
 *     already-rendered map grid. Panel list numbering is 1-based (not
 *     the piece.pdl-index-based 2-based scheme the outer bar uses)
 *     with a trailing "Cancel" row at item_count+1, closing the panel
 *     without acting - same "Back always gets the next free slot"
 *     convention already used in egg-pals. While a panel is open,
 *     digits/Enter/arrows/Escape all do something (see below); any
 *     OTHER key is a pure no-op that does not close the panel - closing
 *     requires an explicit Cancel selection or Escape, unlike the outer
 *     bar's "any other key abandons the sequence" rule, since an
 *     accidental keypress silently dropping an open menu would be bad
 *     UX. move_player.c independently suspends movement while
 *     active_panel != "none", so wasd genuinely does nothing while a
 *     panel is open, not just "doesn't reach here."
 *
 * "Interact mode" (interact_mode field, 0/1) - the REAL xlector active-
 * target pattern from real 1.TPMOS (`projects/fuzz-op/manager/
 * fuzz-op_manager.c`, read in full, not excerpted - see dox/
 * 04-chtpm-parser-research-and-interact-mode.txt for the complete
 * research writeup), adapted for muchi-civ's own shape (a single game,
 * not a multi-project desktop host; monsters/items have no piece.pdl
 * METHOD table of their own yet, so there's nothing to redirect further
 * VERBS to - v1 scope is a real EXAMINE, not full verb-redirection).
 * SUPERSEDES an earlier, narrower build in this same file (a toggle
 * that moved the ACTION BAR's own cursor with arrows) - that wasn't the
 * real feature; this is a genuine cursor you walk around the MAP.
 *
 * 'i'/'I' toggles interact_mode outside a panel (meaningless inside one
 * - a panel is already its own menu-mode context), resetting the
 * xlector cursor's position (hero/state.txt's xlector_pos_x/
 * xlector_pos_y) to the hero's current position every time it's
 * entered. While interact_mode=1: wasd/arrows move the CURSOR, not the
 * hero - that's ops/move_player.c's own responsibility (this file does
 * NOT handle arrows while interact_mode=1; see that file's own header
 * comment), not this one's. Enter examines whatever's at the cursor's
 * current position (examine_at(), below) - the real v1 "select."
 * Digits are a pure no-op in this mode - there's no action-bar menu to
 * jump around while controlling the cursor. Escape (27) exits
 * interact_mode back to plain hero movement (or closes an open panel
 * first, if one happens to be open) - matches real chtpm_parser.c's own
 * confirmed ESC_KEY=27 "always wins, checked first" convention.
 * ARROW_UP/DOWN inside a panel still move its own cursor (wraparound,
 * unaffected by any of this - a panel is its own menu-mode context,
 * independent of interact_mode's value, same as it always was).
 *
 * Quit stays the separate, fixed, non-numbered 'q' key in
 * pal/main_loop.pal - confirmed real 1.TPMOS keeps Quit as a literal
 * 'q'/'Q' check outside this numbered/accumulator dispatch entirely
 * (only a genuine "Back to X" row is a real navigable numbered item).
 * Note 'q' never actually reaches this op at all: pal/main_loop.pal's
 * beq x2,x9,quit runs BEFORE `choice x2` and jumps straight to halt, so
 * any pending state here simply goes stale, harmless since the process
 * is halting anyway (this means 'q' also quits the whole game even with
 * a panel open - there is no dedicated "close panel" key by design,
 * only the panel's own Cancel row).
 *
 * Usage: choice.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_PANEL_ITEMS 32

/* Same ARROW_* sentinel values keyboard_input.c/gl_mirror.c/move_player.c
 * already use everywhere else in this project. */
#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Reads a piece's own piece.pdl method NAMES into a small in-memory list
 * (not just a single lookup) since this op needs both the total count
 * (for bounds-checking the accumulator) and, on commit, the name at a
 * specific index - one popen covers both instead of two like an
 * earlier version needed.
 *
 * CORRECTED 2026-07-21 (possession control model, see this file's own
 * header comment below for the full fuzz-op citation): piece_id is now
 * a real parameter, not a hardcoded "hero" literal - once a unit is
 * POSSESSED, the action bar must dispatch against THAT unit's own
 * piece.pdl (feed/play/sleep-equivalent methods), not the permanently-
 * inert hero/xlector's. Every call site passes either "hero" (xlector
 * mode, unpossessed) or the current possessed_id. */
static int load_method_names(const char *piece_id, char names[][MAX_LINE], int max_names) {
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' '%s' list_methods", project_root, piece_id);
    FILE *pf = popen(cmd, "r");
    if (!pf) return 0;
    int n = 0;
    char line[MAX_LINE];
    while (n < max_names && fgets(line, sizeof(line), pf)) {
        line[strcspn(line, "\n")] = '\0';
        snprintf(names[n], MAX_LINE, "%s", line);
        n++;
    }
    pclose(pf);
    return n;
}

/* Plain top-to-bottom scan of recipes.txt, ids only - deliberately the
 * same simple, unfiltered order compose_frame.c's own panel renderer
 * uses, so the position a recipe is drawn at and the position this op
 * resolves a digit to can never drift (the exact class of bug real
 * 1.TPMOS's fuzz-op subsystem had between its renderer and its
 * dispatcher's independently-written skip-lists). */
static int load_recipe_ids(char ids[][MAX_LINE], int max_ids) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/recipes/recipes.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAX_LINE];
    while (n < max_ids && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *bar = strchr(line, '|');
        if (!bar) continue;
        snprintf(ids[n], MAX_LINE, "%.*s", (int)(bar - line), line);
        n++;
    }
    fclose(f);
    return n;
}

/* Directory-order scan of hero/inventory/ - deliberately the same
 * order compose_frame.c's inventory panel renderer uses (readdir()
 * order is stable within a single process run against an unchanged
 * directory), same anti-drift reasoning as load_recipe_ids(). Only
 * needs item_ids since the inventory panel never execs anything on
 * commit - see this file's header comment. */
static int load_inventory_ids(char ids[][MAX_LINE], int max_ids) {
    char inventory_dir[PATH_BUF];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/world_pal_craft_home/map_start/hero/inventory", project_root);
    DIR *d = opendir(inventory_dir);
    if (!d) return 0;
    struct dirent *entry;
    int n = 0;
    while (n < max_ids && (entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
        FILE *f = fopen(state_path, "r");
        if (!f) continue;
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            if (strncmp(line, "item_id=", 8) == 0) { snprintf(ids[n], MAX_LINE, "%s", line + 8); break; }
        }
        fclose(f);
        n++;
    }
    closedir(d);
    return n;
}

/* Single-field registry name lookups, matching the same pipe-delimited
 * shape compose_frame.c's/compose_rgb_frame.c's own item_registry_field()/
 * monster_registry_field() already read - duplicated narrowly here (just
 * the name column, field index 2) rather than shared, per this project's
 * own "no shared headers" convention. */
static void item_name(const char *item_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", item_id);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/items/items.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(item_id) || strncmp(line, item_id, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *end = strchr(name, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", name);
        break;
    }
    fclose(f);
}

static void monster_name(const char *monster_type, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", monster_type);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *end = strchr(name, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", name);
        break;
    }
    fclose(f);
}

/* CORRECTED 2026-07-21 (direct user catch: "its still not using
 * xelector mode, did u know that?"): the field below WAS still an
 * examine-only op - it could log "you see a Settler here" but never
 * actually SELECTED anything a later order-issuing op could act on.
 * This is Civilization/Dwarf-Fortress-style play (design doc's own
 * correction, same session: "this is how civilization's own control
 * system works too") - there is no directly-moved hero, the cursor
 * (interact_mode/xlector_pos_x/xlector_pos_y, already wired in
 * move_player.c) is the ONLY way the player ever touches a unit, so
 * "examine" must also really SELECT: on a units/ match, this now
 * writes the matched instance_id out through selected_id_out so
 * main() can persist it into hero/state.txt as possessed_id -
 * the field every future order-issuing op (move-order, found-city,
 * attack, ...) is meant to read. selected_id_out[0] is left '\0' (and
 * hero/state.txt's own possessed_id gets written as "none")
 * when nothing is found here, so a stale selection never lingers past
 * an examine of empty ground - matches this project's own general
 * "don't leave a pending accumulator/selection stale" discipline
 * (see digit_accum's own reset-on-any-other-key rule above in main()). */
static void examine_at(const char *map_id, int x, int y, char *selected_id_out, size_t selected_id_sz) {
    char items_dir[PATH_BUF + 32], monsters_dir[PATH_BUF + 32], players_dir[PATH_BUF + 32];
    snprintf(items_dir, sizeof(items_dir), "%s/pieces/world_pal_craft_home/%s/items", project_root, map_id);
    snprintf(monsters_dir, sizeof(monsters_dir), "%s/pieces/world_pal_craft_home/%s/monsters", project_root, map_id);
    snprintf(players_dir, sizeof(players_dir), "%s/pieces/world_pal_craft_home/%s/players", project_root, map_id);

    if (selected_id_out && selected_id_sz) selected_id_out[0] = '\0';
    char msg[160] = "";

    /* Units checked FIRST - a unit standing on an item/monster tile
     * (not possible today, but not structurally prevented either)
     * should win the selection, matching "the thing you can actually
     * command" taking priority over flavor-text examine targets. */
    DIR *ud = opendir(players_dir);
    if (ud) {
        struct dirent *entry;
        while ((entry = readdir(ud)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 384];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", players_dir, entry->d_name);
            FILE *sf = fopen(state_path, "r");
            if (!sf) continue;
            char line[MAX_LINE], name[64] = "?", civ_id[64] = "?";
            int ux = -1, uy = -1;
            while (fgets(line, sizeof(line), sf)) {
                line[strcspn(line, "\n")] = '\0';
                if (strncmp(line, "name=", 5) == 0) snprintf(name, sizeof(name), "%s", line + 5);
                else if (strncmp(line, "civ_id=", 7) == 0) snprintf(civ_id, sizeof(civ_id), "%s", line + 7);
                else if (strncmp(line, "pos_x=", 6) == 0) ux = atoi(line + 6);
                else if (strncmp(line, "pos_y=", 6) == 0) uy = atoi(line + 6);
            }
            fclose(sf);
            if (ux == x && uy == y) {
                snprintf(msg, sizeof(msg), "Selected %s (%s).", name, civ_id);
                if (selected_id_out && selected_id_sz) snprintf(selected_id_out, selected_id_sz, "%s", entry->d_name);
                break;
            }
        }
        closedir(ud);
    }

    DIR *d = !msg[0] ? opendir(items_dir) : NULL;
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 384];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", items_dir, entry->d_name);
            FILE *sf = fopen(state_path, "r");
            if (!sf) continue;
            char line[MAX_LINE], item_id[64] = "?";
            int ix = -1, iy = -1;
            while (fgets(line, sizeof(line), sf)) {
                line[strcspn(line, "\n")] = '\0';
                /* gcc can't prove line+8 fits in item_id's 64 bytes from
                 * static sizes alone - same class of warning narrowly
                 * suppressed elsewhere in this project (tick_monsters.c,
                 * prisc+x.c) rather than widened indefinitely. */
                if (strncmp(line, "item_id=", 8) == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                    snprintf(item_id, sizeof(item_id), "%s", line + 8);
#pragma GCC diagnostic pop
                }
                else if (strncmp(line, "pos_x=", 6) == 0) ix = atoi(line + 6);
                else if (strncmp(line, "pos_y=", 6) == 0) iy = atoi(line + 6);
            }
            fclose(sf);
            if (ix == x && iy == y) {
                char name[64];
                item_name(item_id, name, sizeof(name));
                snprintf(msg, sizeof(msg), "You see a %s here.", name);
                break;
            }
        }
        closedir(d);
    }

    if (!msg[0]) {
        d = opendir(monsters_dir);
        if (d) {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                char state_path[PATH_BUF + 384];
                snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", monsters_dir, entry->d_name);
                FILE *sf = fopen(state_path, "r");
                if (!sf) continue;
                char line[MAX_LINE], monster_type[64] = "?";
                int mx = -1, my = -1, hp = 0;
                while (fgets(line, sizeof(line), sf)) {
                    line[strcspn(line, "\n")] = '\0';
                    if (strncmp(line, "monster_type=", 13) == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                        snprintf(monster_type, sizeof(monster_type), "%s", line + 13);
#pragma GCC diagnostic pop
                    }
                    else if (strncmp(line, "pos_x=", 6) == 0) mx = atoi(line + 6);
                    else if (strncmp(line, "pos_y=", 6) == 0) my = atoi(line + 6);
                    else if (strncmp(line, "hp=", 3) == 0) hp = atoi(line + 3);
                }
                fclose(sf);
                if (mx == x && my == y) {
                    char name[64];
                    monster_name(monster_type, name, sizeof(name));
                    snprintf(msg, sizeof(msg), "You see a %s (hp %d) here.", name, hp);
                    break;
                }
            }
            closedir(d);
        }
    }

    if (!msg[0]) snprintf(msg, sizeof(msg), "You see nothing here.");

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *lf = fopen(log_path, "a");
    if (lf) { fprintf(lf, "%s\n", msg); fclose(lf); }
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_pal_craft_home/map_start/hero/state.txt", project_root);
    FILE *f = fopen(hero_path, "r");
    if (!f) return 1;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int action_cursor = -1, digit_accum = 0, panel_cursor = 0, panel_digit_accum = 0;
    int interact_mode = 0, emoji_mode = 0;
    int render_mode = 0; /* 0=2D, 1=3D - '0' toggles, GL-only (no-op visually in ASCII) */
    int camera_mode = 1; /* 1=1st person, 2=3rd person, 3=free camera */
    int hero_x = 0, hero_y = 0;
    int xlector_x = -1, xlector_y = -1; /* -1 = absent, filled from hero_x/y below */
    char map_id[64] = "map_start";
    char active_panel[32] = "none";
    char possessed_id[64] = "none"; /* the real cursor "select" target -
        see examine_at()'s own header comment for the 2026-07-21 correction
        that made this field real (was examine-only before). */
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "action_cursor") == 0) action_cursor = atoi(eq + 1);
            else if (strcmp(lines[nlines], "digit_accum") == 0) digit_accum = atoi(eq + 1);
            else if (strcmp(lines[nlines], "panel_cursor") == 0) panel_cursor = atoi(eq + 1);
            else if (strcmp(lines[nlines], "panel_digit_accum") == 0) panel_digit_accum = atoi(eq + 1);
            else if (strcmp(lines[nlines], "interact_mode") == 0) interact_mode = atoi(eq + 1);
            else if (strcmp(lines[nlines], "emoji_mode") == 0) emoji_mode = atoi(eq + 1);
            else if (strcmp(lines[nlines], "render_mode") == 0) render_mode = atoi(eq + 1);
            else if (strcmp(lines[nlines], "camera_mode") == 0) camera_mode = atoi(eq + 1);
            else if (strcmp(lines[nlines], "pos_x") == 0) hero_x = atoi(eq + 1);
            else if (strcmp(lines[nlines], "pos_y") == 0) hero_y = atoi(eq + 1);
            else if (strcmp(lines[nlines], "xlector_pos_x") == 0) xlector_x = atoi(eq + 1);
            else if (strcmp(lines[nlines], "xlector_pos_y") == 0) xlector_y = atoi(eq + 1);
            else if (strcmp(lines[nlines], "map_id") == 0) {
                /* Copy into a separate buffer before stripping the
                 * newline - `eq + 1` aliases directly into lines[nlines],
                 * so stripping in place would destroy that line's own
                 * trailing '\n' for the rest of this op's lifetime,
                 * corrupting the later passthrough fputs() write-back
                 * (map_id has no dedicated found/replace branch there,
                 * so it always goes through that path) by merging it
                 * with whatever field follows it in the file. Hit this
                 * for real: produced a `map_id=map_starthp=100` glued
                 * line. See active_panel's own matching fix and its
                 * comment on the self-heal this exact bug class already
                 * needed once before. */
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s", eq + 1);
                tmp[strcspn(tmp, "\n")] = '\0';
                snprintf(map_id, sizeof(map_id), "%s", tmp);
            } else if (strcmp(lines[nlines], "active_panel") == 0) {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "%s", eq + 1);
                tmp[strcspn(tmp, "\n")] = '\0';
                snprintf(active_panel, sizeof(active_panel), "%s", tmp);
            } else if (strcmp(lines[nlines], "possessed_id") == 0) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s", eq + 1);
                tmp[strcspn(tmp, "\n")] = '\0';
                snprintf(possessed_id, sizeof(possessed_id), "%s", tmp);
            }
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);
    if (xlector_x < 0) xlector_x = hero_x;
    if (xlector_y < 0) xlector_y = hero_y;

    /* !.pal-standards.txt sec. 31.4: "fixed to one character" is a
     * CONFIG-DRIVEN mode, not a stripped-down code path - the full
     * free-possession machinery stays wired in every project, this is
     * a one-line self-healing clamp applied BEFORE any dispatch logic
     * runs. mode=free (this project's own default) makes this a
     * complete no-op. */
    {
        char cfg_path[PATH_BUF];
        snprintf(cfg_path, sizeof(cfg_path), "%s/pieces/system/possession_config.txt", project_root);
        char mode[16] = "free";
        char fixed_target[64] = "none";
        FILE *cf = fopen(cfg_path, "r");
        if (cf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), cf)) {
                line[strcspn(line, "\r\n")] = '\0';
                if (strncmp(line, "mode=", 5) == 0) snprintf(mode, sizeof(mode), "%s", line + 5);
                else if (strncmp(line, "fixed_target_id=", 16) == 0) snprintf(fixed_target, sizeof(fixed_target), "%s", line + 16);
            }
            fclose(cf);
        }
        if (strcmp(mode, "fixed") == 0 && strcmp(fixed_target, "none") != 0) {
            snprintf(possessed_id, sizeof(possessed_id), "%s", fixed_target);
        }
    }

    int is_digit = (key >= '0' && key <= '9');
    int is_enter = (key == 10 || key == 13);
    /* "Interact mode" (per real 1.TPMOS's is_map_control, researched and
     * adapted - see dox/04-chtpm-parser-research-and-interact-mode.txt
     * for the full writeup, not copied verbatim since muchi-civ is a
     * single game, not a multi-project desktop host). Lets wasd/arrows
     * drive the action-bar cursor instead of the hero, for input
     * sources with no digit keys (a joystick's d-pad, deferred to a
     * later pass - this groundwork is what that will build on). Digits
     * still directly jump the action bar regardless of this flag - it's
     * a pure addition, never required for keyboard play. */
    int is_up = (key == ARROW_UP);
    int is_down = (key == ARROW_DOWN);
    /* CORRECTED 2026-07-21 (real bug found live: "possession doesn't
     * work" - root cause was THIS FILE never defined/checked
     * ARROW_LEFT/ARROW_RIGHT at all, only up/down. Any LEFT/RIGHT
     * keypress while interact_mode==1 fell through every real branch
     * straight to the final catch-all, which reset action_cursor to
     * -1 - silently breaking xlector's own "scan" dispatch (and any
     * possessed unit's own method dispatch) the moment the player
     * moved horizontally, which is required to reach most units on a
     * real map). Needed alongside is_up/is_down everywhere arrows must
     * be excluded from resetting nav state - sec. 28.4's "arrows
     * belong to move_player.c while ACTIVE" rule was never actually
     * complete for all four directions. */
    int is_left = (key == ARROW_LEFT);
    int is_right = (key == ARROW_RIGHT);
    /* CORRECTED 2026-07-21 per !.pal-standards.txt sec. 28.3: interact
     * mode is NOT a bound key (a former 'i'/'I' toggle lived here -
     * removed, direct user correction: "interact mode isn't an 'i'
     * selectable"). It's entered via a real, ordinary numbered method
     * row named "interact" on hero's own piece.pdl, dispatched by the
     * same is_digit||is_enter mechanism as every other method, below. */
    int is_emoji_toggle = (key == 'e' || key == 'E');
    /* '0' toggles the 3D GL view on/off (a no-op in ASCII terminal
     * rendering - see ops/compose_frame.c, which never reads
     * render_mode at all). While render_mode==1, '1'/'2'/'3' switch
     * camera POV (1st person/3rd person/free camera) INSTEAD of their
     * normal outer-action-bar meaning (pickup/drop) - a real, mode-
     * gated key reinterpretation, same shape as interact_mode's own
     * takeover of wasd/arrows. Matches mutaclsym's identical scheme. */
    int is_3d_toggle = (key == '0');
    int is_pov_key = (render_mode == 1 && (key == '1' || key == '2' || key == '3'));
    /* Universal escape, same real 1.TPMOS chtpm_parser.c convention
     * (ESC_KEY=27, confirmed via direct citation) - closes an open
     * panel without acting (same outcome as its own trailing Cancel
     * row, just reachable without digits), or exits interact_mode back
     * to movement if no panel is open. Already flows through both
     * keyboard_input.c and gl_mirror.c unmodified (confirmed by direct
     * code read - bare ESC returns raw byte 27 in both paths already). */
    int is_escape = (key == 27);
    char exec_handler[MAX_LINE] = "";
    int just_entered_active = 0; /* see the "interact" method branch below */
    char exec_arg[64] = "";

    int in_panel = (strcmp(active_panel, "craft") == 0 || strcmp(active_panel, "inventory") == 0);
    if (!in_panel && strcmp(active_panel, "none") != 0) {
        /* Unrecognized value (not "none" and not a real panel type) -
         * self-heal back to "none" rather than leaving it stuck. Hit
         * this for real once: a corrupted "active_panel=none<garbage>"
         * line (two fields glued together with a missing newline from
         * some earlier partial write) meant move_player.c's own
         * strcmp(active_panel,"none") check was never true again,
         * permanently blocking movement while turns kept ticking in
         * the background - see move_player.c's matching defensive fix
         * for the full writeup. This op is the only writer of
         * active_panel, so healing it here is the actual fix, not just
         * a symptom workaround. */
        snprintf(active_panel, sizeof(active_panel), "none");
    }

    if (is_3d_toggle) {
        /* Pure display-preference toggle, deliberately outside every
         * other branch (including in_panel) - touches nothing else, so
         * flipping the 3D view doesn't cancel a pending digit sequence
         * or panel state. Matches mutaclsym's identical "3D/emoji are
         * global preferences, not menu navigation" convention. */
        render_mode = !render_mode;
    } else if (is_pov_key) {
        /* Only reachable while render_mode==1 (see is_pov_key's own
         * definition above) - camera_mode is the only thing this
         * changes, same "pure preference, touches nothing else" shape
         * as is_3d_toggle. */
        camera_mode = key - '0';
    } else if (is_emoji_toggle) {
        /* Toggle ASCII<->emoji display mode, same as mutaclsym. Moved
         * up here (global preference, reachable even with a panel
         * open) alongside is_3d_toggle/is_pov_key - previously only
         * reachable outside a panel. Emoji glyphs are in field 6 of
         * registries; ASCII in field 3. */
        emoji_mode = !emoji_mode;
    } else if (in_panel) {
        /* Panel mode - "craft" (recipe picker, commits by exec'ing
         * craft.+x with the chosen recipe_id) or "inventory" (examine -
         * read-only browse, Enter never execs anything, just closes). */
        char panel_ids[MAX_PANEL_ITEMS][MAX_LINE];
        int panel_item_count = 0;
        if (strcmp(active_panel, "craft") == 0) panel_item_count = load_recipe_ids(panel_ids, MAX_PANEL_ITEMS);
        else if (strcmp(active_panel, "inventory") == 0) panel_item_count = load_inventory_ids(panel_ids, MAX_PANEL_ITEMS);
        int panel_total = panel_item_count + 1; /* + trailing Cancel row */

        if (is_up || is_down) {
            /* Wraparound arrow-cursor movement - real chtpm_parser.c
             * convention (confirmed via direct citation), always live
             * while a panel is open regardless of interact_mode's own
             * value (a panel IS already a menu-mode context, same as
             * move_player.c already independently suspends movement
             * here - no additional toggle needed to reach this). */
            if (panel_cursor < 1 || panel_cursor > panel_total) panel_cursor = is_down ? 0 : (panel_total + 1);
            if (is_up) { panel_cursor--; if (panel_cursor < 1) panel_cursor = panel_total; }
            else       { panel_cursor++; if (panel_cursor > panel_total) panel_cursor = 1; }
            panel_digit_accum = 0;
        } else if (is_escape) {
            /* Close the panel without acting - same outcome as
             * selecting the trailing Cancel row, just reachable
             * without digits (see this file's header comment on
             * is_escape). */
            snprintf(active_panel, sizeof(active_panel), "none");
            panel_cursor = 0;
            panel_digit_accum = 0;
        } else if (is_digit) {
            int d = key - '0';
            int new_val = panel_digit_accum * 10 + d;
            if (new_val >= 1 && new_val <= panel_total) {
                panel_digit_accum = new_val;
                panel_cursor = new_val;
            } else if (d >= 1 && d <= panel_total) {
                panel_digit_accum = d;
                panel_cursor = d;
            } else {
                panel_digit_accum = 0;
            }
        } else if (is_enter) {
            if (strcmp(active_panel, "craft") == 0 && panel_cursor >= 1 && panel_cursor <= panel_item_count) {
                snprintf(exec_handler, sizeof(exec_handler), "ops/+x/craft.+x");
                snprintf(exec_arg, sizeof(exec_arg), "%s", panel_ids[panel_cursor - 1]);
            }
            /* Inventory panel: never execs anything, whatever row is
             * selected - it's read-only browsing, not an action list.
             * Craft panel on Cancel/out-of-range: also nothing to exec.
             * Either way, closing is the only outcome. */
            snprintf(active_panel, sizeof(active_panel), "none");
            panel_cursor = 0;
            panel_digit_accum = 0;
        } else {
            /* Any other key is a pure no-op while a panel is open - see
             * this file's header comment for why closing on a stray key
             * would be bad UX. Only abandon a partial digit sequence. */
            panel_digit_accum = 0;
        }
    } else if (is_escape) {
        /* CORRECTED 2026-07-21, TWICE - see !.pal-standards.txt sec. 28
         * (chtpm_parser.c's own generic OUTER-layer ESC handler) AND
         * sec. 30 (fuzz-op_manager.c's own route_input(), the REAL
         * project-level INNER-layer input dispatcher, read in full only
         * after direct user insistence: "look at those fuzz op file[s]
         * too"). route_input() ~line 550 binds BOTH ESC and the literal
         * key '9' to an unconditional relinquish-only check, checked
         * BEFORE anything else in that function - "if not xlector,
         * become xlector, done" - it does NOT also drop out of
         * interact/ACTIVE mode in the same keypress. Only once ALREADY
         * unpossessed does ESC fall through to chtpm's own OUTER-layer
         * behavior (deactivate interact_mode -> NAV). This project has
         * no outer desktop host (civ-pal/angler-empires are standalone,
         * PAL-CRAFT-DESIGN.txt's own documented architecture
         * difference), so both layers collapse into this one op - ESC
         * here does step 1 (relinquish, if possessing) OR step 2
         * (deactivate interact_mode, if not), never both in one press,
         * matching the reference's own two-step "back out one level at
         * a time" feel. */
        if (strcmp(possessed_id, "none") != 0) {
            /* CORRECTED 2026-07-21 (direct user clarification: "during
             * possession xelector isnt visible on the map, till
             * relinquishment of possession where it will appear at the
             * new location of the possessed entity like before") -
             * compose_frame.c/compose_rgb_frame.c now hide xlector
             * entirely while possessed_id!="none" (see their own header
             * comments) - so xlector must be moved to the possessed
             * unit's OWN current position here, BEFORE clearing
             * possessed_id, or it would reappear at whatever stale spot
             * it was left at before possessing instead of where the
             * player actually ends up. */
            char player_path[PATH_BUF + 384];
            snprintf(player_path, sizeof(player_path), "%s/pieces/world_pal_craft_home/map_start/players/%s/state.txt", project_root, possessed_id);
            int upx = -1, upy = -1;
            FILE *uf = fopen(player_path, "r");
            if (uf) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), uf)) {
                    if (strncmp(line, "pos_x=", 6) == 0) upx = atoi(line + 6);
                    else if (strncmp(line, "pos_y=", 6) == 0) upy = atoi(line + 6);
                }
                fclose(uf);
            }
            if (upx >= 0 && upy >= 0) { xlector_x = upx; xlector_y = upy; }

            snprintf(possessed_id, sizeof(possessed_id), "none");
            char log_path[PATH_BUF];
            snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
            FILE *lf = fopen(log_path, "a");
            if (lf) { fprintf(lf, "Returned to Xlector.\n"); fclose(lf); }
            digit_accum = 0;
            action_cursor = 2; /* xlector's own first real method ("scan"), same
                                   "land focus on something real" rule as entering
                                   ACTIVE mode fresh (see just_entered_active below). */
        } else {
            /* real chtpm_parser.c's own ESC handler explicitly does
             * `focus_index = old_active` (chtpm_parser.c ~line 2868) -
             * landing the NAV cursor back ON THE ROW THAT WAS JUST
             * EXITED, not on no row at all (live-tested gap, direct
             * user report: "escape does not yet return '>' focus
             * selector to nav indexes" - fixed by looking up
             * "interact"'s own real index in hero's own method list,
             * never hardcoded, since method order can change). */
            interact_mode = 0;
            digit_accum = 0;
            action_cursor = -1;
            char hero_names[32][MAX_LINE];
            int hero_total = load_method_names("hero", hero_names, 32);
            for (int i = 2; i < hero_total; i++) {
                if (strcmp(hero_names[i], "interact") == 0) { action_cursor = i; break; }
            }
        }
    } else if (is_digit || is_enter) {
        /* CORRECTED 2026-07-21 per !.pal-standards.txt sec. 28: ONE
         * unified numbered-method dispatch, reused for all three real
         * states (sec. 28.1) by varying only WHICH piece_id's own
         * piece.pdl is consulted:
         *   interact_mode==0 (NAV)            -> "hero" (its own real
         *     "interact" method row is what ENTERS interact mode -
         *     sec. 28.3 - an ordinary numbered row, never a bound key).
         *   interact_mode==1, possessed_id=="none" (ACTIVE, free cursor)
         *     -> "xlector".
         *   interact_mode==1, possessed_id!="none" (ACTIVE, possessing)
         *     -> possessed_id itself (a spawned unit's own piece.pdl).
         * This is real fuzz-op's own [ACTIVE]: field made concrete -
         * xlector's own method list vs. a possessed piece's own -
         * same mechanism, not two copies of this dispatch block.
         *
         * CORRECTED 2026-07-21 per !.pal-standards.txt sec. 30 (direct
         * user insistence, twice, on actually reading fuzz-op_manager.c's
         * own route_input() - not done carefully enough the first time):
         * possessing was WRONGLY gated behind a numbered "scan" method
         * needing action_cursor==2 first - the real reference makes
         * Enter-while-unpossessed an UNCONDITIONAL possess-at-cursor
         * check (route_input() ~line 559), never routed through the
         * numbered dispatch at all. Handled as a special case BEFORE
         * the normal action_cursor-gated dispatch below, so it fires
         * regardless of any pending digit/action_cursor state. */
        const char *dispatch_id = (interact_mode == 0) ? "hero"
                                 : (strcmp(possessed_id, "none") != 0) ? possessed_id
                                 : "xlector";

        if (is_enter && strcmp(dispatch_id, "xlector") == 0) {
            /* Sec. 30.1: examine-and-possess at the cursor's current
             * position (moved by move_player.c, not this file) -
             * UNCONDITIONAL, matching route_input()'s own unconditional
             * Enter-while-xlector check, not gated behind any numbered
             * method selection. examine_at() already sets possessed_id
             * on a units/ match. */
            examine_at(map_id, xlector_x, xlector_y, possessed_id, sizeof(possessed_id));
            if (!possessed_id[0]) snprintf(possessed_id, sizeof(possessed_id), "none");
            digit_accum = 0;
            action_cursor = -1;
        } else {

        char names[32][MAX_LINE];
        int total = load_method_names(dispatch_id, names, 32);

        if (is_digit) {
            int d = key - '0';
            int new_val = digit_accum * 10 + d;
            if (new_val >= 2 && new_val < total) {
                digit_accum = new_val;
                action_cursor = new_val;
            } else if (d >= 2 && d < total) {
                digit_accum = d;
                action_cursor = d;
            } else {
                digit_accum = 0;
                action_cursor = -1;
            }
        } else { /* is_enter, dispatch_id is "hero" or a possessed unit -
                    xlector's own Enter was already handled above */
            if (action_cursor >= 2 && action_cursor < total) {
                if (strcmp(names[action_cursor], "craft") == 0) {
                    /* Craft doesn't execute directly - it opens the
                     * recipe-picker overlay panel instead. See this
                     * file's header comment. Hero-only method today
                     * (units/spawn_unit.c's own piece.pdl has no craft
                     * row) - kept as a named branch rather than removed
                     * so it still works correctly if a future unit type
                     * ever gains one. */
                    snprintf(active_panel, sizeof(active_panel), "craft");
                    panel_cursor = 1;
                    panel_digit_accum = 0;
                } else if (strcmp(names[action_cursor], "examine") == 0) {
                    /* Same pattern - opens the inventory browse panel
                     * instead of running examine.+x directly. */
                    snprintf(active_panel, sizeof(active_panel), "inventory");
                    panel_cursor = 1;
                    panel_digit_accum = 0;
                } else if (strcmp(dispatch_id, "hero") == 0 && strcmp(names[action_cursor], "interact") == 0) {
                    /* Sec. 28.3: the real, ordinary numbered method row
                     * that ENTERS interact mode - matches chtpm's own
                     * onClick="INTERACT" special case (a literal string
                     * match, never exec'd as an external command), not
                     * a bound key. Resets the free cursor to a sane
                     * starting position each entry (matches real
                     * xlector behavior) - but never touches possessed_id
                     * (sec. 28.2 - possession persists independently). */
                    interact_mode = 1;
                    xlector_x = hero_x; xlector_y = hero_y;
                    just_entered_active = 1;
                } else {
                    char cmd[PATH_BUF];
                    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' '%s' get_method '%s'", project_root, dispatch_id, names[action_cursor]);
                    FILE *pf = popen(cmd, "r");
                    if (pf) {
                        if (fgets(exec_handler, sizeof(exec_handler), pf)) exec_handler[strcspn(exec_handler, "\n")] = '\0';
                        pclose(pf);
                    }
                }
            }
            digit_accum = 0;
            action_cursor = just_entered_active ? 2 : -1;
            /* just_entered_active: same "land focus on the row you just
             * entered" fix as the ESC handler above (chtpm_parser.c's
             * own ACTIVATE case moves focus to the first navigable
             * CHILD, ~line 2826-2830) - without this, entering ACTIVE
             * mode would show no "[>]" marker on xlector's own list
             * either, the identical bug just found on the ESC side. */
        }
        }
    } else if (interact_mode == 0 && (is_up || is_down || is_left || is_right)) {
        /* Cycles the "[>]" NAV cursor among hero's own real method
         * rows, wrapping at both ends. up/down cycling matches real
         * chtpm_parser.c's own top-level NAV handling exactly
         * (active_index==-1, chtpm_parser.c ~line 2724-2733, confirmed
         * by direct read - see !.pal-standards.txt sec. 28). left/right
         * DOING THE SAME THING is a real, deliberate EXTENSION beyond
         * that reference (direct user request, 2026-07-21: "i just
         * expected left and right to move nav like up and down do...
         * can we add it?" - chtpm itself never gives LEFT/RIGHT any NAV
         * meaning at all, confirmed by grep across the whole file, so
         * this is new behavior, not a port) - left cycles backward,
         * right cycles forward, same as up/down. ONLY reachable here
         * while interact_mode==0 (NAV, "[>]") - while ACTIVE ("[^]"),
         * arrows are move_player.c's job entirely (moving the free
         * cursor or a possessed unit on the map) and must NEVER touch
         * this op's own action_cursor/digit_accum state at all, which
         * is exactly why this whole branch is gated on
         * interact_mode==0 - falling through to the old catch-all
         * (which reset action_cursor to -1 on ANY unrecognized key,
         * including arrows) was a real, separately-found bug: it
         * silently wiped the "[>]" marker on every arrow press with no
         * menu navigation ever actually happening. */
        char names[32][MAX_LINE];
        int total = load_method_names("hero", names, 32);
        if (total > 2) {
            int cur = (action_cursor >= 2 && action_cursor < total) ? action_cursor : 2;
            if (is_up || is_left) { cur--; if (cur < 2) cur = total - 1; }
            else                  { cur++; if (cur >= total) cur = 2; }
            action_cursor = cur;
        }
        digit_accum = 0;
    } else if (interact_mode == 1 && (is_up || is_down || is_left || is_right)) {
        /* Arrows while ACTIVE belong entirely to move_player.c (moving
         * the free cursor or a possessed unit) - this op must leave
         * its own action_cursor/digit_accum completely untouched here,
         * or the "[^]"/numbered-method marker on whatever's currently
         * possessed would flicker/vanish on every single movement
         * keypress, the same class of bug the interact_mode==0 branch
         * above just fixed for NAV mode. A genuine no-op, not even a
         * digit_accum reset. */
    } else {
        /* Any other key ('q', an unrecognized byte) abandons a pending
         * sequence rather than leaving it stale for several turns. */
        digit_accum = 0;
        action_cursor = -1;
    }

    f = fopen(hero_path, "w");
    if (!f) return 1;
    int ac_found = 0, da_found = 0, ap_found = 0, pc_found = 0, pda_found = 0, lk_found = 0, im_found = 0;
    int xx_found = 0, xy_found = 0, em_found = 0, rm_found = 0, cm_found = 0, si_found = 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "action_cursor") == 0) { fprintf(f, "action_cursor=%d\n", action_cursor); ac_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "digit_accum") == 0) { fprintf(f, "digit_accum=%d\n", digit_accum); da_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "active_panel") == 0) { fprintf(f, "active_panel=%s\n", active_panel); ap_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "possessed_id") == 0) { fprintf(f, "possessed_id=%s\n", possessed_id); si_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "panel_cursor") == 0) { fprintf(f, "panel_cursor=%d\n", panel_cursor); pc_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "panel_digit_accum") == 0) { fprintf(f, "panel_digit_accum=%d\n", panel_digit_accum); pda_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "interact_mode") == 0) { fprintf(f, "interact_mode=%d\n", interact_mode); im_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "emoji_mode") == 0) { fprintf(f, "emoji_mode=%d\n", emoji_mode); em_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "render_mode") == 0) { fprintf(f, "render_mode=%d\n", render_mode); rm_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "camera_mode") == 0) { fprintf(f, "camera_mode=%d\n", camera_mode); cm_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "xlector_pos_x") == 0) { fprintf(f, "xlector_pos_x=%d\n", xlector_x); xx_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "xlector_pos_y") == 0) { fprintf(f, "xlector_pos_y=%d\n", xlector_y); xy_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "last_key") == 0) { fprintf(f, "last_key=%d\n", key); lk_found = 1; *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!ac_found) fprintf(f, "action_cursor=%d\n", action_cursor);
    if (!da_found) fprintf(f, "digit_accum=%d\n", digit_accum);
    if (!ap_found) fprintf(f, "active_panel=%s\n", active_panel);
    if (!si_found) fprintf(f, "possessed_id=%s\n", possessed_id);
    if (!pc_found) fprintf(f, "panel_cursor=%d\n", panel_cursor);
    if (!pda_found) fprintf(f, "panel_digit_accum=%d\n", panel_digit_accum);
    if (!im_found) fprintf(f, "interact_mode=%d\n", interact_mode);
    if (!em_found) fprintf(f, "emoji_mode=%d\n", emoji_mode);
    if (!rm_found) fprintf(f, "render_mode=%d\n", render_mode);
    if (!cm_found) fprintf(f, "camera_mode=%d\n", camera_mode);
    if (!xx_found) fprintf(f, "xlector_pos_x=%d\n", xlector_x);
    if (!xy_found) fprintf(f, "xlector_pos_y=%d\n", xlector_y);
    /* last_key: the raw keycode this op was invoked with, on EVERY
     * tick regardless of what it turned out to mean - a debugging aid
     * so "why does the game look frozen" (e.g. repeatedly pressing a
     * movement key into a wall, which used to be a silent no-op - see
     * move_player.c's new "You can't go that way." message) has a
     * second, independent way to confirm input really is being read,
     * not just trust the message log. compose_frame.c displays this
     * in the footer. */
    if (!lk_found) fprintf(f, "last_key=%d\n", key);
    fclose(f);

    if (exec_handler[0]) {
        char exec_cmd[PATH_BUF];
        if (exec_arg[0]) snprintf(exec_cmd, sizeof(exec_cmd), "'%s/%s' '%s'", project_root, exec_handler, exec_arg);
        else snprintf(exec_cmd, sizeof(exec_cmd), "'%s/%s'", project_root, exec_handler);
        int rc = system(exec_cmd);
        (void)rc; /* the handler op's own exit status isn't consulted here, matching every other op-calling-op site in this project */
    }
    return 0;
}
