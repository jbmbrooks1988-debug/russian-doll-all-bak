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
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_LINE 2048
#define MAX_ENTRIES 32

/*
 * settings_manager.c -- init-only manager for the "settings" hub project.
 *
 * See x0.short-term-vision/settings-hub-window-geom-design-j5.md for the
 * full design. Short version: settings is a real, standalone project,
 * reached via <href> (not the Window/g_windows[] desktop-shell system),
 * that discovers sub-entries living in its own directory (each marked
 * by a small settings_entry.pdl) and renders them as href links. No
 * hot-path ops needed -- href clicks are handled generically by
 * wraith_parser_alpha.c itself, and the entry list only needs to be
 * (re)computed once, at manager startup, same as any other one-time
 * discovery.
 *
 * Writes to manager/state.txt (NOT session/state.txt) deliberately --
 * that is the generic, already-existing path
 * wraith_parser_alpha.c's load_vars()/load_state_file() reads for any
 * project reached via its own layout, derived from project_id. This
 * project needs zero changes to any shared file to get ${settings_menu_markup}
 * substituted into settings.chtpm.
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
             "%s/projects/wraith-alpha/wraith-projects/settings", project_root);
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

/* Same substring-key/pipe-split convention as
 * wraith-alpha_manager.c's read_pdl_value(), reimplemented locally --
 * this is a separate binary, it can't call that file's static function. */
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

/* Scans settings/ for immediate subdirectories that declare themselves
 * a settings entry via settings_entry.pdl (label= / entry_layout=).
 * Discovery, not hardcoding -- adding a new settings sub-project later
 * means adding one marker file, no changes here. */
int discover_entries(char labels[MAX_ENTRIES][256], char layouts[MAX_ENTRIES][MAX_PATH]) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    dir = opendir(project_dir);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL && count < MAX_ENTRIES) {
        char sub_dir[4096];
        char marker_path[4096];
        struct stat st;

        if (entry->d_name[0] == '.') continue;

        snprintf(sub_dir, sizeof(sub_dir), "%s/%s", project_dir, entry->d_name);
        if (stat(sub_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        snprintf(marker_path, sizeof(marker_path), "%s/settings_entry.pdl", sub_dir);
        if (access(marker_path, F_OK) != 0) continue;

        read_pdl_value(marker_path, "label", labels[count], sizeof(labels[0]));
        read_pdl_value(marker_path, "entry_layout", layouts[count], sizeof(layouts[0]));
        if (labels[count][0] && layouts[count][0]) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

void write_state(void) {
    char labels[MAX_ENTRIES][256];
    char layouts[MAX_ENTRIES][MAX_PATH];
    int count;
    int i;
    char menu_markup[4096] = "";
    char tmp_path[4096];
    FILE *f;

    count = discover_entries(labels, layouts);

    if (count == 0) {
        snprintf(menu_markup, sizeof(menu_markup), "(no settings entries found)");
    } else {
        for (i = 0; i < count; i++) {
            char one[1024];
            snprintf(one, sizeof(one), "<button label=\"%s\" href=\"%s\" />%s",
                     labels[i], layouts[i], (i < count - 1) ? "\\n" : "");
            strncat(menu_markup, one, sizeof(menu_markup) - strlen(menu_markup) - 1);
        }
    }

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", state_path);
    f = fopen(tmp_path, "w");
    if (!f) {
        log_debug("ERROR: failed to open %s for writing", tmp_path);
        return;
    }
    fprintf(f, "settings_menu_markup=%s\n", menu_markup);
    fprintf(f, "settings_entry_count=%d\n", count);
    fclose(f);
    rename(tmp_path, state_path);

    log_debug("Discovered %d settings entr%s", count, count == 1 ? "y" : "ies");
}

/* 2fix.txt, 2026-07-05: settings is also auto-discoverable via the
 * existing discover_launcher_projects() launcher-row list (it's a real
 * project with a project.pdl -- that already-generic scanner picks it
 * up for free). Opening it THAT way launches it as a desktop-embedded
 * Window, which reads session/wraith_body.txt via
 * append_project_probe_body() -- a completely different file from the
 * manager/state.txt the href/standalone path uses. Without this,
 * opening settings via the launcher row showed the generic "Missing
 * project body file" fallback instead of the real menu.
 *
 * append_project_probe_body() already passes through, verbatim, any
 * line starting with '<' -- so writing the same discovered entries here
 * as raw <button href="..." /><br/> lines makes BOTH paths (launcher-row
 * embedded Window, and direct href/current_layout) render the identical
 * real menu. No changes needed in wraith-alpha_manager.c for this. */
void write_wraith_body(void) {
    char labels[MAX_ENTRIES][256];
    char layouts[MAX_ENTRIES][MAX_PATH];
    int count;
    int i;
    char body_path[4096];
    char tmp_path[4096];
    FILE *f;

    count = discover_entries(labels, layouts);

    snprintf(body_path, sizeof(body_path), "%s/session/wraith_body.txt", project_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", body_path);
    f = fopen(tmp_path, "w");
    if (!f) {
        log_debug("ERROR: failed to open %s for writing", tmp_path);
        return;
    }

    fprintf(f, "SETTINGS -- choose an option:\n");
    fprintf(f, "\n");
    if (count == 0) {
        fprintf(f, "(no settings entries found)\n");
    } else {
        for (i = 0; i < count; i++) {
            /* Wrapping the button in its own raw <text> border pieces,
             * all on one line starting with '<' -- append_project_probe_body()
             * passes the whole line through verbatim either way, so this
             * still renders as a REAL, nav-indexed, clickable button (not
             * literal text), while matching the "| |  ...  |" box border
             * every other body line has. Plain <button .../> alone (the
             * first version of this fix) rendered correctly but broke out
             * of the box visually -- this keeps both. */
            int label_len = (int)strlen(labels[i]);
            int rendered_estimate = label_len + 8; /* "[ ] N. [" + label + "]" roughly */
            int pad = 83 - rendered_estimate;
            if (pad < 1) pad = 1;
            fprintf(f, "<text label=\"| |  \" /><button label=\"%s\" href=\"%s\" /><text label=\"%*s|\" /><br/>\n",
                    labels[i], layouts[i], pad, "");
        }
    }
    fclose(f);
    rename(tmp_path, body_path);
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
    log_debug("Settings Hub Manager Started");
    log_debug("Project root: %s", project_root);
    log_debug("Project dir: %s", project_dir);

    write_state();
    write_wraith_body();
    trigger_render();

    log_debug("Settings Hub Manager init complete (discovery is one-time; href clicks are handled generically, no hot path needed)");

    return 0;
}
