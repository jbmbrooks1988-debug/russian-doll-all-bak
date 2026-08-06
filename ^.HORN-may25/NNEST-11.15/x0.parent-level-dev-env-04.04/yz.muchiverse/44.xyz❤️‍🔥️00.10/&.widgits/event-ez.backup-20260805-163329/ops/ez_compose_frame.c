/* ez_compose_frame - Event-EZ: "click nav buttons, fill in cli-io
 * blanks" event authoring, 4th event-editor variant (2026-08-05).
 *
 * CHTPM digit_accum: 1-4 behavior buttons, 5=ez_target cli_io,
 * 6=ez_speed cli_io, 7=Save. Continuous, never restart per section.
 *
 * Usage: ez_compose_frame.+x
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

/* REAL FIX 2026-08-05, direct user correction ("play is meaningless to
 * me as user/dev guide if i cant see the script in the event screen so
 * i dont know wht ur doing... if i asked for more visibility thats
 * always priority"): before this, the window only ever showed the
 * CURRENTLY TYPED field values (blank on a fresh reopen) plus a
 * transient "Saved: ..." message that vanished on the next render.
 * There was no way to SEE what's actually on disk - what Play would
 * really run - just by looking at the window. This reads page_1's own
 * real condition.pdl (pipe-delimited SECTION|KEY|VALUE format) and
 * event.ir.pdl (NODE rows) straight off disk and renders them verbatim,
 * every single frame, regardless of what's currently typed. */
static void read_pdl_value(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        char *v_end = v + strlen(v);
        while (v_end > v && v_end[-1] == ' ') v_end--;
        *v_end = '\0';
        snprintf(out, out_sz, "%s", v);
        break;
    }
    fclose(f);
}

/* REAL, 2026-08-05: Gallery<->Page navigation - see design doc's own
 * "Full nested flow, RPG-Maker-MV-accurate" section. Each page gets its
 * own real, generated .chtpm file (event_ez_page_N.chtpm); a screen
 * tells which page it is by reading chtpm_parser_pal.c's own real,
 * pre-existing pieces/display/current_layout.txt ("EXPORT CURRENT
 * LAYOUT FOR MODULE HEARTBEAT", written on every screen switch,
 * confirmed via source - not invented here). Returns 0 if we're on the
 * gallery (no page_N in the active layout's own filename). */
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

/* Real page-editor template, written fresh to event_ez_page_<n>.chtpm
 * every gallery compose (idempotent - same content every time, cheap to
 * overwrite) so its href target always exists before a gallery row can
 * ever be clicked. Kept in ONE place so every page's own screen is
 * byte-identical except its own filename (which is all that needs to
 * differ - current_page_number() above reads the rest back from disk). */
static void write_page_layout(const char *layouts_dir, int n) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/event_ez_page_%d.chtpm", layouts_dir, n);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
        "<panel time_reactive=\"true\">\n"
        "    <module>system/prisc+x pal/main_loop_chtpm.pal</module>\n"
        "    <interact src=\"pieces/apps/player_app/interact_relay.txt\" />\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"| PAGE %d  pkg=${pkg_name}                 |\" /><br/>\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"Pick a behavior:\" /><br/>\n"
        "    <button label=\"Chase\" onClick=\"KEY:1\" /><br/>\n"
        "    <button label=\"Flee\" onClick=\"KEY:2\" /><br/>\n"
        "    <button label=\"Wander\" onClick=\"KEY:3\" /><br/>\n"
        "    <button label=\"Idle\" onClick=\"KEY:4\" /><br/>\n"
        "    <text label=\"Fill in the blanks:\" /><br/>\n"
        "    <cli_io id=\"ez_trigger\" label=\"Trigger (on_spawn/on_click/parallel)\" target_id=\"ez_trigger\" /><br/>\n"
        "    <cli_io id=\"ez_target\" label=\"Target entity\" target_id=\"ez_target\" /><br/>\n"
        "    <cli_io id=\"ez_speed\" label=\"Speed\" target_id=\"ez_speed\" /><br/>\n"
        "    <cli_io id=\"ez_command\" label=\"Command (shell-exec action)\" target_id=\"ez_command\" /><br/>\n"
        "    <text label=\"RPG-Maker-style page overrides (optional):\" /><br/>\n"
        "    <cli_io id=\"ez_graphic\" label=\"Graphic override (blank=default sprite)\" target_id=\"ez_graphic\" /><br/>\n"
        "    <cli_io id=\"ez_move_type\" label=\"Move type (fixed/random/approach/custom)\" target_id=\"ez_move_type\" /><br/>\n"
        "    <cli_io id=\"ez_priority\" label=\"Priority (below/same/above)\" target_id=\"ez_priority\" /><br/>\n"
        "    <button label=\"Save\" onClick=\"KEY:5\" /><br/>\n"
        "    <button label=\"Back to Pages\" href=\"pieces/chtpm/layouts/event_ez.chtpm\" /><br/>\n"
        "    <text label=\"${last_message}\" /><br/>\n"
        "</panel>\n",
        n);
    fclose(f);
}

/* Sanitize a dynamically-injected button label: no double-quotes (would
 * break the injected attribute string), no embedded newlines. */
static void sanitize(const char *in, char *out, size_t n) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < n; i++) {
        char c = in[i];
        if (c == '"') c = '\'';
        if (c == '\n' || c == '\r') c = ' ';
        out[j++] = c;
    }
    out[j] = '\0';
}

static void ping(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
    snprintf(p, sizeof(p), "%s/pieces/display/ez_screen_changed.txt", project_root);
    f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

/* Real, on-disk page count: how many pages/page_N/ dirs actually exist. */
static int count_real_pages(const char *pkg_dir) {
    if (!pkg_dir[0]) return 0;
    int n = 1;
    char path[PATH_BUF];
    while (1) {
        snprintf(path, sizeof(path), "%s/pages/page_%d", pkg_dir, n);
        struct stat st;
        if (stat(path, &st) != 0) break;
        n++;
    }
    return n - 1;
}

static void compose_gallery(const char *state, const char *view, const char *gui,
                             const char *pkg, const char *pkg_dir, const char *msg,
                             char *rows_out, size_t rows_out_sz) {
    /* Real page-editor files must exist BEFORE the gallery can href to
     * them - generate every real page's own file plus one trailing
     * blank slot ("always one more empty page," RPG Maker's own
     * convention) fresh every compose. Cheap, idempotent. */
    char layouts_dir[PATH_BUF];
    snprintf(layouts_dir, sizeof(layouts_dir), "%s/pieces/chtpm/layouts", project_root);
    int n_real = count_real_pages(pkg_dir);
    for (int i = 1; i <= n_real + 1; i++) write_page_layout(layouts_dir, i);

    /* Real dynamic button injection, SAME proven mechanism the real
     * CHTPM editor's own ${event_content_rows} already uses - a bare
     * ${var} placeholder (not inside a <text> tag) gets substituted
     * with raw markup BEFORE tokenization (parse_chtm()'s own
     * substitute_vars_naked(), confirmed via source), so injected
     * <button href="..."> tags parse as REAL new elements. Using href
     * (a plain literal path, no prefix-matching) rather than onClick
     * here - onClick strings other than a few recognized prefixes are
     * silently rejected by send_command(), a real latent bug already
     * found this session; href has no such restriction. */
    rows_out[0] = '\0';
    size_t used = 0;
    for (int i = 1; i <= n_real + 1; i++) {
        char label[80];
        /* REAL FIX 2026-08-05, root-caused via live k3 testing + a real
         * instrumented chtpm_parser_pal trace (see
         * visual-event-compiler-pal.md): this used to bake its OWN
         * "N. [...]" numbering into the label text - but
         * chtpm_parser_pal's own render_element() ALREADY prepends a
         * real "[ ]"/"[>]" cursor + real auto-numbered "N." for every
         * navigable button automatically. The result was double,
         * nested numbering ("[ ] 1. [1. [on-click]]") - confirmed via
         * live trace this was purely cosmetic (do_jump()/href commit
         * both traced CORRECTLY against the real element every time),
         * not an actual navigation-targeting bug. Labels now carry only
         * the real content - chtpm's own real nav chrome supplies the
         * number and bracket. */
        if (i <= n_real) {
            char cond_path[PATH_BUF], trig[64] = "";
            snprintf(cond_path, sizeof(cond_path), "%s/pages/page_%d/condition.pdl", pkg_dir, i);
            read_pdl_value(cond_path, "trigger", trig, sizeof(trig));
            snprintf(label, sizeof(label), "%s", trig[0] ? trig : "?");
        } else {
            snprintf(label, sizeof(label), "empty");
        }
        char clean[80]; sanitize(label, clean, sizeof(clean));
        int wrote = snprintf(rows_out + used, rows_out_sz - used,
                              "<button label=\"%s\" href=\"pieces/chtpm/layouts/event_ez_page_%d.chtpm\" /><br/>",
                              clean, i);
        if (wrote < 0 || (size_t)wrote >= rows_out_sz - used) break;
        used += (size_t)wrote;
    }

    FILE *o = fopen(view, "w");
    if (!o) return;
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| EVENT-EZ  pkg=%-27.27s|\n", pkg);
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| -- PAGES --                                |\n");
    for (int i = 1; i <= n_real; i++) {
        char cond_path[PATH_BUF], ir_path[PATH_BUF], trig[64] = "";
        snprintf(cond_path, sizeof(cond_path), "%s/pages/page_%d/condition.pdl", pkg_dir, i);
        snprintf(ir_path, sizeof(ir_path), "%s/pages/page_%d/event.ir.pdl", pkg_dir, i);
        read_pdl_value(cond_path, "trigger", trig, sizeof(trig));
        char summary[64] = "(empty)";
        FILE *irf = fopen(ir_path, "r");
        if (irf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), irf)) {
                if (strncmp(line, "NODE", 4) != 0) continue;
                char *txp = strstr(line, "text=");
                if (!txp) continue;
                char *v = txp + 5;
                v[strcspn(v, "\r\n")] = '\0';
                snprintf(summary, sizeof(summary), "%s", v);
                break;
            }
            fclose(irf);
        }
        char row[96];
        snprintf(row, sizeof(row), "%d. [%s] %.30s", i, trig[0] ? trig : "?", summary);
        fprintf(o, "| %-42.42s|\n", row);
    }
    {
        char row[64];
        snprintf(row, sizeof(row), "%d. [ ] empty  <- click to add", n_real + 1);
        fprintf(o, "| %-42.42s|\n", row);
    }
    fprintf(o, "+==========================================+\n");
    if (msg[0]) fprintf(o, "| %-42.42s|\n", msg);
    else fprintf(o, "| Click a page number to open/create it    |\n");
    fprintf(o, "+==========================================+\n");
    fclose(o);
    (void)state; (void)gui;
}

static void compose_page(const char *state, const char *view, const char *gui,
                          const char *pkg, const char *pkg_dir, const char *msg, int page_n) {
    char behavior[32], target[128], speed[32];
    read_kv(state, "behavior", behavior, sizeof(behavior));
    if (!behavior[0]) snprintf(behavior, sizeof(behavior), "(unset)");
    read_kv(gui, "ez_target", target, sizeof(target));
    read_kv(gui, "ez_speed", speed, sizeof(speed));

    char saved_trigger[64] = "";
    char cond_path[PATH_BUF], ir_path[PATH_BUF];
    int have_saved_page = 0;
    if (pkg_dir[0]) {
        snprintf(cond_path, sizeof(cond_path), "%s/pages/page_%d/condition.pdl", pkg_dir, page_n);
        snprintf(ir_path, sizeof(ir_path), "%s/pages/page_%d/event.ir.pdl", pkg_dir, page_n);
        struct stat st;
        if (stat(ir_path, &st) == 0) {
            have_saved_page = 1;
            read_pdl_value(cond_path, "trigger", saved_trigger, sizeof(saved_trigger));
        }
    }

    FILE *o = fopen(view, "w");
    if (!o) return;
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| EVENT-EZ  pkg=%-13.13s page=%-8d|\n", pkg, page_n);
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| Behavior: %-32.32s|\n", behavior);
    fprintf(o, "| Target:   %-32.32s|\n", target[0] ? target : "(blank)");
    fprintf(o, "| Speed:    %-32.32s|\n", speed[0] ? speed : "(blank)");
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| -- SAVED ON DISK (what Play runs) --       |\n");
    if (have_saved_page) {
        fprintf(o, "| trigger=%-33.33s|\n", saved_trigger[0] ? saved_trigger : "(unset)");
        FILE *irf = fopen(ir_path, "r");
        if (irf) {
            char line[MAX_LINE];
            int shown = 0;
            while (shown < 4 && fgets(line, sizeof(line), irf)) {
                if (strncmp(line, "NODE", 4) != 0) continue;
                char *tp = strstr(line, "type=");
                char *txp = strstr(line, "text=");
                if (!tp) continue;
                char type_buf[48] = "";
                char *t = tp + 5, *sp = strchr(t, ' ');
                char *pipe = strchr(t, '|');
                size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
                if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
                memcpy(type_buf, t, len);
                type_buf[len] = '\0';
                char *text_val = txp ? txp + 5 : "";
                while (*text_val == ' ') text_val++;
                text_val[strcspn(text_val, "\r\n")] = '\0';
                char row[96];
                snprintf(row, sizeof(row), "%s: %s", type_buf, text_val);
                fprintf(o, "| %-42.42s|\n", row);
                shown++;
            }
            fclose(irf);
        }
    } else {
        fprintf(o, "| (nothing saved yet - fill in fields, Save)|\n");
    }
    fprintf(o, "+==========================================+\n");
    if (msg[0]) fprintf(o, "| %-42.42s|\n", msg);
    else fprintf(o, "| 1-4 pick behavior, fill blanks, 5=Save    |\n");
    fprintf(o, "+==========================================+\n");
    fclose(o);
}

int main(void) {
    resolve_root();

    char state[PATH_BUF], view[PATH_BUF], gui[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/system/ez_state.txt", project_root);
    snprintf(view, sizeof(view), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(gui, sizeof(gui), "%s/projects/event-ez/manager/gui_state.txt", project_root);

    char pkg[128], msg[MAX_LINE], pkg_dir[PATH_BUF];
    read_kv(state, "pkg_name", pkg, sizeof(pkg));
    read_kv(state, "last_message", msg, sizeof(msg));
    read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
    if (!pkg[0]) snprintf(pkg, sizeof(pkg), "(none)");

    char page_gallery_rows[4096] = "";
    int page_n = current_page_number();
    if (page_n > 0) compose_page(state, view, gui, pkg, pkg_dir, msg, page_n);
    else compose_gallery(state, view, gui, pkg, pkg_dir, msg, page_gallery_rows, sizeof(page_gallery_rows));

    {
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s/projects/event-ez/manager'", project_root);
        if (system(cmd) != 0) { /* best-effort, dir likely already exists */ }
    }

    /* REAL FIX: a plain "a" (append) open here would grow gui_state.txt
     * unbounded (this op runs every ~30ms in the PAL loop). chtpm_parser_
     * pal.c owns writing ez_target=/ez_speed= itself on every keystroke
     * (save_cli_io_gui_state(), confirmed via source read) - read the
     * WHOLE file first, keep every line except our own pkg_name=/
     * last_message= (which we're about to rewrite fresh), then write it
     * all back plus our fresh values. Preserves the live cli_io keys
     * without ever growing the file. */
    char keep[64][MAX_LINE];
    int n_keep = 0;
    FILE *rf = fopen(gui, "r");
    if (rf) {
        char line[MAX_LINE];
        while (n_keep < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, "pkg_name=", 9) == 0) continue;
            if (strncmp(line, "last_message=", 13) == 0) continue;
            if (strncmp(line, "page_gallery_rows=", 18) == 0) continue;
            snprintf(keep[n_keep], MAX_LINE, "%s", line);
            n_keep++;
        }
        fclose(rf);
    }
    /* REAL FIX 2026-08-05, root-caused via live k3 reproduction + a
     * real instrumented chtpm_parser_pal trace (see
     * visual-event-compiler-pal.md / EVENT_SCRIPTING_PROGRESS_AND_GOALS.md
     * "KNOWN BUG"): this used to fopen(gui, "w") directly - a plain
     * truncate-in-place write. chtpm_parser_pal's own load_vars() reads
     * this EXACT file from a SEPARATE, concurrently-running process on
     * every parse_chtm() (this PAL loop re-runs ez_compose_frame every
     * tick that processed a key - main_loop_chtpm.pal's own
     * render_always path). A torn read (parser catching this file mid-
     * truncate, after the old content was wiped but before the new
     * page_gallery_rows= line was written) is EXACTLY what produced the
     * live-observed symptom: page rows sometimes duplicated, sometimes
     * missing entirely, no crash - a classic non-atomic-write race, not
     * a parsing/counting bug in do_jump() itself (which traced correctly
     * every single time once fed a consistent file). Write to a real
     * temp file, then atomically rename() over the real path - same
     * fix shape this house's own livedesk registry files already use. */
    char gui_tmp[PATH_BUF];
    snprintf(gui_tmp, sizeof(gui_tmp), "%s.tmp", gui);
    FILE *g = fopen(gui_tmp, "w");
    if (!g) return 1;
    for (int i = 0; i < n_keep; i++) fputs(keep[i], g);
    fprintf(g, "pkg_name=%s\n", pkg);
    fprintf(g, "last_message=%s\n", msg[0] ? msg : "");
    fprintf(g, "page_gallery_rows=%s\n", page_gallery_rows);
    fclose(g);
    rename(gui_tmp, gui);

    ping();
    return 0;
}
