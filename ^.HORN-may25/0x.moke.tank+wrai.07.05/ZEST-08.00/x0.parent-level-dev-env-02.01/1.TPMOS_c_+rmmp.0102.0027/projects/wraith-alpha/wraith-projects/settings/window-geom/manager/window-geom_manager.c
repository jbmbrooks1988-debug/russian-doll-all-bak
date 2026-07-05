#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#define MAX_PATH 4096
#define MAX_LINE 2048

/*
 * window-geom_manager.c -- init-only manager for the "window-geom"
 * settings entry. See x0.short-term-vision/settings-hub-window-geom-design-j5.md.
 *
 * FIRST SLICE SCOPE (deliberately, not an oversight): this reads
 * whichever window is currently ACTIVE in the wraith-alpha desktop
 * (via projects/wraith-alpha/session/alpha_state.txt's
 * desktop_focused_window_project_id -- the one additive line just
 * added to write_projection() for exactly this) and displays its
 * project.pdl WINDOW section, READ-ONLY. A full "pick any open
 * window" picker needs a further additive change (exposing every open
 * window, not just the active one) -- explicitly deferred, not
 * bundled into this slice. The chrome-bar targeted-invocation path
 * (pre-setting a specific target, skipping this active-window read
 * entirely) is also explicitly deferred.
 *
 * Writes to its own manager/state.txt (NOT session/state.txt) for the
 * same reason settings_manager.c does -- that's the generic path
 * wraith_parser_alpha.c's load_vars() already reads for any project,
 * derived from its own layout's path. Zero shared-file changes needed
 * beyond the one additive line already made.
 */

char project_root[2048] = ".";
char project_dir[2048] = ".";
char state_path[4096] = "";
char debug_log_path[4096] = "";

char *trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int root_has_anchors(const char *root) {
    char pieces_path[4096], projects_path[4096];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

void resolve_paths(void) {
    if (!getcwd(project_root, sizeof(project_root))) {
        strncpy(project_root, ".", sizeof(project_root) - 1);
    }
    project_root[sizeof(project_root) - 1] = '\0';

    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0 && *v) {
                    if (root_has_anchors(v)) {
                        snprintf(project_root, sizeof(project_root), "%s", v);
                    }
                }
            }
        }
        fclose(kvp);
    }

    snprintf(project_dir, sizeof(project_dir),
             "%s/projects/wraith-alpha/wraith-projects/settings/window-geom", project_root);
    snprintf(state_path, sizeof(state_path), "%s/manager/state.txt", project_dir);
    snprintf(debug_log_path, sizeof(debug_log_path), "%s/manager/debug_log.txt", project_dir);
}

void log_debug(const char *fmt, ...) {
    FILE *f = fopen(debug_log_path, "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        fprintf(f, "[%ld] ", (long)time(NULL));
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

/* Same key=value scan convention already used across this codebase
 * (e.g. terminal_manager.c's resolve_paths()). */
void read_kvp_value(const char *path, const char *key, char *dst, size_t dst_sz) {
    FILE *f;
    char line[MAX_LINE];
    size_t key_len = strlen(key);

    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *val = trim_str(line + key_len + 1);
            char *nl = strchr(val, '\n');
            if (nl) *nl = '\0';
            strncpy(dst, val, dst_sz - 1);
            dst[dst_sz - 1] = '\0';
            break;
        }
    }
    fclose(f);
}

/* Same substring-key/pipe-split convention as
 * wraith-alpha_manager.c's read_pdl_value(). */
void read_pdl_value(const char *path, const char *key, char *dst, size_t dst_sz) {
    FILE *f;
    char line[MAX_LINE];

    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *pipe1, *pipe2, *val, *nl;
        if (!strstr(line, key)) continue;
        pipe1 = strchr(line, '|');
        if (!pipe1) continue;
        pipe2 = strchr(pipe1 + 1, '|');
        if (!pipe2) continue;
        val = trim_str(pipe2 + 1);
        nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        strncpy(dst, val, dst_sz - 1);
        dst[dst_sz - 1] = '\0';
        break;
    }
    fclose(f);
}

/* Reads session/wg_target.txt -- a single line, a project_id (e.g.
 * "wraith-alpha/wraith-projects/terminal"). This is the file-backed
 * handoff a future chrome-bar button would write before navigating
 * here (per settings-hub-window-geom-design-j5.md's "targeted"
 * invocation), and it's also how this can be tested by hand right now:
 * write a project_id into this file to simulate "opened from a
 * project"; leave it absent/empty to simulate "opened from settings"
 * (falls back to whatever window is currently active in the desktop). */
void read_target_file(char *out_project_id, size_t out_sz) {
    char target_path[4096];
    FILE *f;
    char line[256];

    out_project_id[0] = '\0';
    snprintf(target_path, sizeof(target_path), "%s/session/wg_target.txt", project_dir);
    f = fopen(target_path, "r");
    if (!f) return;
    if (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_str(line);
        strncpy(out_project_id, trimmed, out_sz - 1);
        out_project_id[out_sz - 1] = '\0';
    }
    fclose(f);
}

void write_state(void) {
    char target_project_id[256] = "";
    char focused_id[128] = "";
    char focused_project_id[256] = "";
    char focused_title[128] = "";
    char display[2048] = "";
    char tmp_path[4096];
    const char *source_label;
    FILE *f;

    read_target_file(target_project_id, sizeof(target_project_id));

    if (target_project_id[0]) {
        /* Opened FROM A PROJECT (a chrome-bar-style shortcut, or a
           hand-written test file) -- the target is already known, no
           need to consult the desktop's currently-active window at all. */
        strncpy(focused_project_id, target_project_id, sizeof(focused_project_id) - 1);
        source_label = "targeted (session/wg_target.txt)";
    } else {
        /* Opened FROM SETTINGS (the generic path, no target pre-set) --
           fall back to whichever window is currently active in the
           desktop, via the one additive line in write_projection(). */
        char alpha_state_path[4096];
        snprintf(alpha_state_path, sizeof(alpha_state_path),
                 "%s/projects/wraith-alpha/session/alpha_state.txt", project_root);
        read_kvp_value(alpha_state_path, "desktop_focused_window_id", focused_id, sizeof(focused_id));
        read_kvp_value(alpha_state_path, "desktop_focused_window_project_id", focused_project_id, sizeof(focused_project_id));
        read_kvp_value(alpha_state_path, "desktop_focused_window_title", focused_title, sizeof(focused_title));
        source_label = "active window (opened via settings)";
    }

    if (!focused_project_id[0]) {
        snprintf(display, sizeof(display),
                 "No target set and no active window found (nothing open in the Wraith desktop right now).");
    } else {
        char pdl_path[4096];
        char win_x[32] = "", win_y[32] = "", win_w[32] = "", win_h[32] = "";
        char title_line[256] = "";

        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", project_root, focused_project_id);
        read_pdl_value(pdl_path, "window_x", win_x, sizeof(win_x));
        read_pdl_value(pdl_path, "window_y", win_y, sizeof(win_y));
        read_pdl_value(pdl_path, "window_width", win_w, sizeof(win_w));
        read_pdl_value(pdl_path, "window_height", win_h, sizeof(win_h));

        /* focused_title is only known in the "active window" path (read
           from alpha_state.txt) -- the "targeted" path has no instance
           title at all, only a project_id, so this line is omitted
           there rather than showing something misleading. */
        if (focused_title[0]) {
            snprintf(title_line, sizeof(title_line), "Window: %s (%s)\\n", focused_title, focused_id);
        }

        snprintf(display, sizeof(display),
                 "Source: %s\\n%sProject: %s\\nx=%s y=%s width=%s height=%s%s",
                 source_label,
                 title_line,
                 focused_project_id,
                 win_x[0] ? win_x : "0 (not set)",
                 win_y[0] ? win_y : "0 (not set)",
                 win_w[0] ? win_w : "0 (not set)",
                 win_h[0] ? win_h : "0 (not set)",
                 (win_x[0] || win_y[0] || win_w[0] || win_h[0]) ? "" : "\\n(no WINDOW section saved in project.pdl yet)");
    }

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", state_path);
    f = fopen(tmp_path, "w");
    if (!f) {
        log_debug("ERROR: failed to open %s for writing", tmp_path);
        return;
    }
    fprintf(f, "window_geom_display=%s\n", display);
    fclose(f);
    rename(tmp_path, state_path);

    log_debug("Wrote window_geom_display for target=%s", focused_project_id[0] ? focused_project_id : "(none)");
}

void trigger_render(void) {
    char frame_marker[4096];
    FILE *f;
    snprintf(frame_marker, sizeof(frame_marker), "%s/pieces/display/frame_changed.txt", project_root);
    f = fopen(frame_marker, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }
}

int main(void) {
    resolve_paths();
    log_debug("Window Geometry Manager Started");
    log_debug("Project root: %s", project_root);
    log_debug("Project dir: %s", project_dir);

    write_state();
    trigger_render();

    log_debug("Window Geometry Manager init complete");

    return 0;
}
