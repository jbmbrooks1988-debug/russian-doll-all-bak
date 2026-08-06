/* ez_menu_input - Event-EZ side effects
 * CHTPM owns [>] focus, digit_accum, and cli_io text editing (Enter
 * activates, ESC deactivates - see reference_cli_io_field_mechanic).
 * This op only handles KEY:n button clicks:
 *   1=Chase 2=Flee 3=Wander 4=Idle (sets "behavior" in ez_state.txt)
 *   5=Save (writes a real event_pkg/event.ir.pdl for pkg_name, reading
 *     the live-synced ez_target/ez_speed straight out of gui_state.txt)
 *   idle 0 = no-op
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *value) {
    char lines[64][MAX_LINE];
    int n = 0;
    int replaced = 0;
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[MAX_LINE];
        size_t klen = strlen(key);
        while (n < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                snprintf(lines[n], MAX_LINE, "%s=%s\n", key, value);
                replaced = 1;
            } else {
                snprintf(lines[n], MAX_LINE, "%s", line);
            }
            n++;
        }
        fclose(rf);
    }
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    for (int i = 0; i < n; i++) fputs(lines[i], wf);
    if (!replaced) fprintf(wf, "%s=%s\n", key, value);
    fclose(wf);
}

/* Same real mechanism as ez_compose_frame.c's own current_page_number()
 * - chtpm_parser_pal.c writes the active layout's own path into
 * pieces/display/current_layout.txt on every screen switch (confirmed
 * via source, not invented). Save needs to know which page it's
 * currently on the SAME way the renderer does - both must agree. */
static int current_page_number(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int n = 0;
    if (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, "page_");
        if (p) n = atoi(p + 5);
    }
    fclose(f);
    return n;
}

static void bump(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/ez_screen_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static void set_msg(const char *state, const char *msg) {
    write_kv(state, "last_message", msg);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char state[PATH_BUF], gui[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/system/ez_state.txt", project_root);
    snprintf(gui, sizeof(gui), "%s/projects/event-ez/manager/gui_state.txt", project_root);

    if (key == 0) return 0;

    if (key == '1') { write_kv(state, "behavior", "Chase"); set_msg(state, "Behavior: Chase"); bump(); return 0; }
    if (key == '2') { write_kv(state, "behavior", "Flee"); set_msg(state, "Behavior: Flee"); bump(); return 0; }
    if (key == '3') { write_kv(state, "behavior", "Wander"); set_msg(state, "Behavior: Wander"); bump(); return 0; }
    if (key == '4') { write_kv(state, "behavior", "Idle"); set_msg(state, "Behavior: Idle"); bump(); return 0; }

    if (key == '5') {
        /* REAL PAGES DATA MODEL, 2026-08-05 (design doc: "Design: event
         * PAGES, RPG-Maker-modeled" + "Full nested flow" sections in
         * EVENT_SCRIPTING_PROGRESS_AND_GOALS.md). A page holds EITHER a
         * pet-AI behavior (existing Chase/Flee/Wander/Idle buttons +
         * target/speed) OR a raw shell-exec action (the Command blank).
         * Page number now comes from current_page_number() (real
         * chtpm_parser_pal.c current_layout.txt mechanism, same as
         * ez_compose_frame.c's own) - NOT a typed cli_io field anymore,
         * since Gallery<->Page navigation now uses real, distinct
         * event_ez_page_N.chtpm files (one real file per page) reached
         * via real href, matching the "which page am I" answer the
         * renderer already computes the same way. */
        char pkg[128], behavior[32], target[128], speed[32];
        char trigger[64], command[MAX_LINE];
        char graphic[128], move_type[32], priority[16];
        int page_n = current_page_number();
        if (page_n <= 0) { set_msg(state, "Save failed: not on a page screen"); bump(); return 0; }
        read_kv(state, "pkg_name", pkg, sizeof(pkg));
        read_kv(state, "behavior", behavior, sizeof(behavior));
        read_kv(gui, "ez_target", target, sizeof(target));
        read_kv(gui, "ez_speed", speed, sizeof(speed));
        read_kv(gui, "ez_trigger", trigger, sizeof(trigger));
        read_kv(gui, "ez_command", command, sizeof(command));
        /* RPG-Maker-style page overrides, 2026-08-05 direct instruction
         * ("the automovement and other stuff absolutely applies here,
         * u should put them in editor so we can sort it out") - real
         * fields now exist in the page editor UI (ez_compose_frame.c's
         * own write_page_layout()); persisted here alongside trigger,
         * not yet consumed by any real movement/sprite logic (that's
         * tp_desktop_window.c's own separate subsystem - this just
         * records the override so it CAN be wired up later without a
         * schema change). */
        read_kv(gui, "ez_graphic", graphic, sizeof(graphic));
        read_kv(gui, "ez_move_type", move_type, sizeof(move_type));
        read_kv(gui, "ez_priority", priority, sizeof(priority));
        if (!pkg[0]) { set_msg(state, "Save failed: no pkg_name set"); bump(); return 0; }
        if (!behavior[0] && !command[0]) {
            set_msg(state, "Save failed: pick a behavior (1-4) or type a Command");
            bump();
            return 0;
        }
        char page[16];
        snprintf(page, sizeof(page), "%d", page_n);
        if (!trigger[0]) snprintf(trigger, sizeof(trigger), "on_click");

        char pkg_dir[PATH_BUF];
        read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
        if (!pkg_dir[0]) { set_msg(state, "Save failed: no pkg_dir set"); bump(); return 0; }

        char page_dir[PATH_BUF];
        snprintf(page_dir, sizeof(page_dir), "%s/pages/page_%s", pkg_dir, page);
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", page_dir);
        if (system(cmd) != 0) { /* best-effort */ }

        /* condition.pdl - real RPG-Maker-modeled trigger, see design doc */
        char cond_path[PATH_BUF];
        snprintf(cond_path, sizeof(cond_path), "%s/condition.pdl", page_dir);
        FILE *cf = fopen(cond_path, "w");
        if (cf) {
            fprintf(cf, "SECTION      | KEY                | VALUE\n");
            fprintf(cf, "----------------------------------------\n");
            fprintf(cf, "META         | piece_id           | %s\n", pkg);
            fprintf(cf, "COND         | trigger              | %s\n", trigger);
            if (graphic[0]) fprintf(cf, "GFX          | graphic              | %s\n", graphic);
            if (move_type[0]) fprintf(cf, "MOVE         | move_type            | %s\n", move_type);
            if (priority[0]) fprintf(cf, "OPT          | priority             | %s\n", priority);
            fclose(cf);
        }

        /* event.ir.pdl - description, same NODE-row format the real
         * CHTPM editor already reads back via EE_PKG_DIR. */
        char ir_path[PATH_BUF];
        snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", page_dir);
        FILE *f = fopen(ir_path, "w");
        if (!f) { set_msg(state, "Save failed: could not write event.ir.pdl"); bump(); return 0; }
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | piece_id           | %s\n", pkg);
        fprintf(f, "STATE        | source               | event-ez\n");
        fprintf(f, "NODE         | id=1 type=trigger      | text=%s\n", trigger);
        if (behavior[0]) {
            fprintf(f, "NODE         | id=2 type=behavior     | text=%s\n", behavior);
            fprintf(f, "NODE         | id=3 type=target       | text=%s\n", target[0] ? target : "(none)");
            fprintf(f, "NODE         | id=4 type=speed        | text=%s\n", speed[0] ? speed : "(none)");
        }
        if (command[0]) {
            fprintf(f, "NODE         | id=5 type=command      | text=%s\n", command);
        }
        fprintf(f, "NODE         | id=9 type=ret          | text=\n");
        fclose(f);

        /* event.pal - real, executable prisc+x opcodes. Only the
         * Command case is genuinely runnable right now (a real `exec`
         * opcode, confirmed via direct prisc+x.c read - real syntax
         * `exec <cmd> [arg1] [arg2]`, plain whitespace tokens, no quote
         * parsing). A behavior page (Chase/Flee/Wander/Idle) still only
         * gets a descriptive stub - real chase/wander AI logic is
         * explicitly future scope, not part of this pass. */
        char pal_path[PATH_BUF];
        snprintf(pal_path, sizeof(pal_path), "%s/event.pal", page_dir);
        FILE *pf = fopen(pal_path, "w");
        if (pf) {
            fprintf(pf, "# event.pal - real prisc+x opcodes, written by event-ez\n");
            fprintf(pf, "# pkg=%s page=%s trigger=%s\n", pkg, page, trigger);
            if (command[0]) {
                fprintf(pf, "exec %s\n", command);
            } else {
                fprintf(pf, "show_text \"%s: %s (target=%s speed=%s)\"\n",
                        pkg, behavior, target[0] ? target : "-", speed[0] ? speed : "-");
            }
            fprintf(pf, "ret\n");
            fclose(pf);
        }

        char msg[MAX_LINE];
        if (command[0]) {
            snprintf(msg, sizeof(msg), "Saved page %s (%s): exec %s", page, trigger, command);
        } else {
            snprintf(msg, sizeof(msg), "Saved page %s (%s): %s target=%s speed=%s",
                     page, trigger, behavior, target[0] ? target : "-", speed[0] ? speed : "-");
        }
        set_msg(state, msg);
        bump();
        return 0;
    }

    if (key == 10 || key == 13) {
        set_msg(state, "Enter: CHTPM commits focused button/field (onClick)");
        bump();
        return 0;
    }

    return 0;
}
