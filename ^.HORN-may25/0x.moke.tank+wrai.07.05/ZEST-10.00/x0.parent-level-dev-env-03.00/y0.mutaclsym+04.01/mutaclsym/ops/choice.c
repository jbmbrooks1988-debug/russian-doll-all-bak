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
 *     convention already used in egg-pals. While a panel is open, ONLY
 *     digits and Enter do anything; any other key is a pure no-op
 *     (does not close the panel - closing requires an explicit Cancel
 *     selection, unlike the outer bar's "any other key abandons the
 *     sequence" rule, since an accidental keypress silently dropping
 *     an open menu would be bad UX). move_player.c independently
 *     suspends movement while active_panel != "none", so wasd
 *     genuinely does nothing while a panel is open, not just "doesn't
 *     reach here."
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

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Reads hero's own piece.pdl method NAMES into a small in-memory list
 * (not just a single lookup) since this op needs both the total count
 * (for bounds-checking the accumulator) and, on commit, the name at a
 * specific index - one popen covers both instead of two like an
 * earlier version needed. */
static int load_method_names(char names[][MAX_LINE], int max_names) {
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' hero list_methods", project_root);
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
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/world_01/map_start/hero/inventory", project_root);
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

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);
    FILE *f = fopen(hero_path, "r");
    if (!f) return 1;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int action_cursor = -1, digit_accum = 0, panel_cursor = 0, panel_digit_accum = 0;
    char active_panel[32] = "none";
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "action_cursor") == 0) action_cursor = atoi(eq + 1);
            else if (strcmp(lines[nlines], "digit_accum") == 0) digit_accum = atoi(eq + 1);
            else if (strcmp(lines[nlines], "panel_cursor") == 0) panel_cursor = atoi(eq + 1);
            else if (strcmp(lines[nlines], "panel_digit_accum") == 0) panel_digit_accum = atoi(eq + 1);
            else if (strcmp(lines[nlines], "active_panel") == 0) {
                char *v = eq + 1;
                v[strcspn(v, "\n")] = '\0';
                snprintf(active_panel, sizeof(active_panel), "%s", v);
            }
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    int is_digit = (key >= '0' && key <= '9');
    int is_enter = (key == 10 || key == 13);
    char exec_handler[MAX_LINE] = "";
    char exec_arg[64] = "";

    if (strcmp(active_panel, "none") != 0) {
        /* Panel mode - "craft" (recipe picker, commits by exec'ing
         * craft.+x with the chosen recipe_id) or "inventory" (examine -
         * read-only browse, Enter never execs anything, just closes). */
        char panel_ids[MAX_PANEL_ITEMS][MAX_LINE];
        int panel_item_count = 0;
        if (strcmp(active_panel, "craft") == 0) panel_item_count = load_recipe_ids(panel_ids, MAX_PANEL_ITEMS);
        else if (strcmp(active_panel, "inventory") == 0) panel_item_count = load_inventory_ids(panel_ids, MAX_PANEL_ITEMS);
        int panel_total = panel_item_count + 1; /* + trailing Cancel row */

        if (is_digit) {
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
    } else if (is_digit || is_enter) {
        char names[32][MAX_LINE];
        int total = load_method_names(names, 32);

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
        } else { /* is_enter */
            if (action_cursor >= 2 && action_cursor < total) {
                if (strcmp(names[action_cursor], "craft") == 0) {
                    /* Craft doesn't execute directly - it opens the
                     * recipe-picker overlay panel instead. See this
                     * file's header comment. */
                    snprintf(active_panel, sizeof(active_panel), "craft");
                    panel_cursor = 1;
                    panel_digit_accum = 0;
                } else if (strcmp(names[action_cursor], "examine") == 0) {
                    /* Same pattern - opens the inventory browse panel
                     * instead of running examine.+x directly. */
                    snprintf(active_panel, sizeof(active_panel), "inventory");
                    panel_cursor = 1;
                    panel_digit_accum = 0;
                } else {
                    char cmd[PATH_BUF];
                    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' hero get_method '%s'", project_root, names[action_cursor]);
                    FILE *pf = popen(cmd, "r");
                    if (pf) {
                        if (fgets(exec_handler, sizeof(exec_handler), pf)) exec_handler[strcspn(exec_handler, "\n")] = '\0';
                        pclose(pf);
                    }
                }
            }
            digit_accum = 0;
            action_cursor = -1;
        }
    } else {
        /* Any other key (movement, 'q', an unrecognized byte) abandons
         * a pending sequence rather than leaving it stale for several
         * turns. */
        digit_accum = 0;
        action_cursor = -1;
    }

    f = fopen(hero_path, "w");
    if (!f) return 1;
    int ac_found = 0, da_found = 0, ap_found = 0, pc_found = 0, pda_found = 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "action_cursor") == 0) { fprintf(f, "action_cursor=%d\n", action_cursor); ac_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "digit_accum") == 0) { fprintf(f, "digit_accum=%d\n", digit_accum); da_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "active_panel") == 0) { fprintf(f, "active_panel=%s\n", active_panel); ap_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "panel_cursor") == 0) { fprintf(f, "panel_cursor=%d\n", panel_cursor); pc_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "panel_digit_accum") == 0) { fprintf(f, "panel_digit_accum=%d\n", panel_digit_accum); pda_found = 1; *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!ac_found) fprintf(f, "action_cursor=%d\n", action_cursor);
    if (!da_found) fprintf(f, "digit_accum=%d\n", digit_accum);
    if (!ap_found) fprintf(f, "active_panel=%s\n", active_panel);
    if (!pc_found) fprintf(f, "panel_cursor=%d\n", panel_cursor);
    if (!pda_found) fprintf(f, "panel_digit_accum=%d\n", panel_digit_accum);
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
