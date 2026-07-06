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
#define MAX_PROJECT_PICKS 128

typedef struct {
    char project_id[256];
    char title[128];
} ProjectPick;

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

/* Recursively scans project_root/projects/wraith-alpha/wraith-projects/
 * for any directory containing a project.pdl (to any nesting depth --
 * mirrors compile_all.sh's compile_wraith_project_tree recursion, same
 * "projects work nested or at main level" principle). Used to build the
 * Window Geometry picker: every discovered project can have its window
 * geometry edited, not just a hardcoded list. */
void discover_wraith_projects_recursive(const char *dir, ProjectPick *out, int *count, int max) {
    DIR *d;
    struct dirent *entry;
    char pdl_path[4096];

    if (*count >= max) return;

    snprintf(pdl_path, sizeof(pdl_path), "%s/project.pdl", dir);
    if (access(pdl_path, F_OK) == 0) {
        char project_id[256] = "";
        char title[128] = "";
        read_pdl_value(pdl_path, "project_id", project_id, sizeof(project_id));
        read_pdl_value(pdl_path, "title", title, sizeof(title));
        if (project_id[0] && *count < max) {
            strncpy(out[*count].project_id, project_id, sizeof(out[*count].project_id) - 1);
            out[*count].project_id[sizeof(out[*count].project_id) - 1] = '\0';
            strncpy(out[*count].title, title[0] ? title : project_id, sizeof(out[*count].title) - 1);
            out[*count].title[sizeof(out[*count].title) - 1] = '\0';
            (*count)++;
        }
    }

    d = opendir(dir);
    if (!d) return;
    while ((entry = readdir(d)) != NULL && *count < max) {
        char sub_dir[4096];
        struct stat st;

        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "manager") == 0 || strcmp(entry->d_name, "ops") == 0 ||
            strcmp(entry->d_name, "plugins") == 0 || strcmp(entry->d_name, "layouts") == 0 ||
            strcmp(entry->d_name, "session") == 0 || strcmp(entry->d_name, "+x") == 0) continue;

        snprintf(sub_dir, sizeof(sub_dir), "%s/%s", dir, entry->d_name);
        if (stat(sub_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        discover_wraith_projects_recursive(sub_dir, out, count, max);
    }
    closedir(d);
}

int discover_all_wraith_projects(ProjectPick *out, int max) {
    char wraith_projects_dir[4096];
    int count = 0;
    snprintf(wraith_projects_dir, sizeof(wraith_projects_dir),
             "%s/projects/wraith-alpha/wraith-projects", project_root);
    discover_wraith_projects_recursive(wraith_projects_dir, out, &count, max);
    return count;
}

void write_state(void) {
    char labels[MAX_ENTRIES][256];
    char layouts[MAX_ENTRIES][MAX_PATH];
    char entry_ids[MAX_ENTRIES][256];
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
            char entry_id[256];
            char layout_copy[MAX_PATH];
            char *layouts_pos, *slash;
            strncpy(layout_copy, layouts[i], sizeof(layout_copy) - 1);
            layout_copy[sizeof(layout_copy) - 1] = '\0';
            layouts_pos = strstr(layout_copy, "/layouts/");
            if (layouts_pos) {
                *layouts_pos = '\0';
                slash = strrchr(layout_copy, '/');
                if (slash) {
                    strncpy(entry_id, slash + 1, sizeof(entry_id) - 1);
                    entry_id[sizeof(entry_id) - 1] = '\0';
                } else {
                    strncpy(entry_id, "unknown", sizeof(entry_id) - 1);
                }
            } else {
                strncpy(entry_id, "unknown", sizeof(entry_id) - 1);
            }
            snprintf(one, sizeof(one), "<button label=\"%s\" onClick=\"SETTINGS_PAGE:%s\" />%s",
                     labels[i], entry_id, (i < count - 1) ? "\\n" : "");
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
 * as raw <button .../> lines makes BOTH paths (launcher-row embedded
 * Window, and direct href/current_layout) show options. For embedded mode,
 * buttons now use onClick="SETTINGS_PAGE:..." to stay within the window
 * instead of exiting to full-screen. The manager (route_command in
 * wraith-alpha_manager.c) handles SETTINGS_PAGE by writing to active_page.txt,
 * and this manager re-renders wraith_body.txt based on the active page. */
void write_wraith_body(void) {
    char labels[MAX_ENTRIES][256];
    char layouts[MAX_ENTRIES][MAX_PATH];
    int count;
    int i;
    char body_path[4096];
    char state_changed_path[4096];
    char active_page[256] = "settings";
    char tmp_path[4096];
    FILE *f;

    count = discover_entries(labels, layouts);

    /* Read which page should be displayed from the state_changed marker file (last line) */
    snprintf(state_changed_path, sizeof(state_changed_path), "%s/session/state_changed.txt", project_dir);
    FILE *sc = fopen(state_changed_path, "r");
    if (sc) {
        char line[256];
        while (fgets(line, sizeof(line), sc)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0]) strncpy(active_page, line, sizeof(active_page) - 1);
        }
        fclose(sc);
    }

    snprintf(body_path, sizeof(body_path), "%s/session/wraith_body.txt", project_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", body_path);
    f = fopen(tmp_path, "w");
    if (!f) {
        log_debug("ERROR: failed to open %s for writing", tmp_path);
        return;
    }

    if (strcmp(active_page, "settings") == 0) {
        fprintf(f, "SETTINGS -- choose an option:\n");
        fprintf(f, "\n");
        if (count == 0) {
            fprintf(f, "(no settings entries found)\n");
        } else {
            for (i = 0; i < count; i++) {
                char entry_id[256];
                char layout_copy[MAX_PATH];
                char *layouts_pos, *slash;
                strncpy(layout_copy, layouts[i], sizeof(layout_copy) - 1);
                layout_copy[sizeof(layout_copy) - 1] = '\0';
                layouts_pos = strstr(layout_copy, "/layouts/");
                if (layouts_pos) {
                    *layouts_pos = '\0';
                    slash = strrchr(layout_copy, '/');
                    if (slash) {
                        strncpy(entry_id, slash + 1, sizeof(entry_id) - 1);
                        entry_id[sizeof(entry_id) - 1] = '\0';
                    } else {
                        strncpy(entry_id, "unknown", sizeof(entry_id) - 1);
                    }
                } else {
                    strncpy(entry_id, "unknown", sizeof(entry_id) - 1);
                }
                int label_len = (int)strlen(labels[i]);
                int rendered_estimate = label_len + 8;
                int pad = 83 - rendered_estimate;
                if (pad < 1) pad = 1;
                fprintf(f, "<text label=\"| |  \" /><button label=\"%s\" onClick=\"SETTINGS_PAGE:%s\" /><text label=\"%*s|\" /><br/>\n",
                        labels[i], entry_id, pad, "");
            }
        }
    } else if (strcmp(active_page, "window-geom") == 0) {
        /* Picker: every discovered wraith project (nested or not) can have
         * its window geometry edited -- not a hardcoded list. */
        ProjectPick picks[MAX_PROJECT_PICKS];
        int pick_count = discover_all_wraith_projects(picks, MAX_PROJECT_PICKS);
        int i;

        fprintf(f, "WINDOW GEOMETRY -- choose a project to edit:\n");
        fprintf(f, "\n");
        if (pick_count == 0) {
            fprintf(f, "(no wraith projects found)\n");
        } else {
            for (i = 0; i < pick_count; i++) {
                fprintf(f, "<button label=\"%s\" onClick=\"SETTINGS_PAGE:window-geom:%s\" /><br/>\n",
                        picks[i].title, picks[i].project_id);
            }
        }
        fprintf(f, "\n");
        fprintf(f, "<button label=\"Back\" onClick=\"SETTINGS_PAGE:settings\" /><br/>\n");
    } else if (strncmp(active_page, "window-geom:", 12) == 0) {
        /* Editor: active_page is "window-geom:<project_id>". Working edit
         * values live in session/edit_state.txt, keyed to whichever
         * target_project_id they were last initialized for -- so switching
         * targets resets to that target's real current geometry, but
         * repeated re-renders of the SAME target (after a +/- or cli_io
         * edit) don't stomp the user's in-progress edits. */
        const char *target_project_id = active_page + 12;
        char pdl_path[4096];
        char win_x[32] = "", win_y[32] = "", win_w[32] = "", win_h[32] = "";
        char edit_state_path[4096];
        char stored_target[256] = "";
        char edit_x[32] = "", edit_y[32] = "", edit_w[32] = "", edit_h[32] = "";
        char status[256] = "Ready to edit";
        FILE *ef;

        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", project_root, target_project_id);
        read_pdl_value(pdl_path, "window_x", win_x, sizeof(win_x));
        read_pdl_value(pdl_path, "window_y", win_y, sizeof(win_y));
        read_pdl_value(pdl_path, "window_width", win_w, sizeof(win_w));
        read_pdl_value(pdl_path, "window_height", win_h, sizeof(win_h));

        snprintf(edit_state_path, sizeof(edit_state_path), "%s/session/edit_state.txt", project_dir);
        ef = fopen(edit_state_path, "r");
        if (ef) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), ef)) {
                char *eq = strchr(line, '=');
                char *key, *val, *nl;
                if (!eq) continue;
                *eq = '\0';
                key = trim_str(line);
                val = trim_str(eq + 1);
                nl = strchr(val, '\n');
                if (nl) *nl = '\0';
                if (strcmp(key, "target_project_id") == 0) strncpy(stored_target, val, sizeof(stored_target) - 1);
                else if (strcmp(key, "edit_x") == 0) strncpy(edit_x, val, sizeof(edit_x) - 1);
                else if (strcmp(key, "edit_y") == 0) strncpy(edit_y, val, sizeof(edit_y) - 1);
                else if (strcmp(key, "edit_width") == 0) strncpy(edit_w, val, sizeof(edit_w) - 1);
                else if (strcmp(key, "edit_height") == 0) strncpy(edit_h, val, sizeof(edit_h) - 1);
                else if (strcmp(key, "status") == 0) strncpy(status, val, sizeof(status) - 1);
            }
            fclose(ef);
        }

        if (strcmp(stored_target, target_project_id) != 0) {
            char tmp_es_path[4096];
            FILE *wf;

            strncpy(edit_x, win_x[0] ? win_x : "0", sizeof(edit_x) - 1);
            strncpy(edit_y, win_y[0] ? win_y : "0", sizeof(edit_y) - 1);
            strncpy(edit_w, win_w[0] ? win_w : "0", sizeof(edit_w) - 1);
            strncpy(edit_h, win_h[0] ? win_h : "0", sizeof(edit_h) - 1);
            snprintf(status, sizeof(status), "Editing %s", target_project_id);

            snprintf(tmp_es_path, sizeof(tmp_es_path), "%s.tmp", edit_state_path);
            wf = fopen(tmp_es_path, "w");
            if (wf) {
                fprintf(wf, "target_project_id=%s\n", target_project_id);
                fprintf(wf, "edit_x=%s\n", edit_x);
                fprintf(wf, "edit_y=%s\n", edit_y);
                fprintf(wf, "edit_width=%s\n", edit_w);
                fprintf(wf, "edit_height=%s\n", edit_h);
                fprintf(wf, "status=%s\n", status);
                fclose(wf);
                rename(tmp_es_path, edit_state_path);
            }
        }

        fprintf(f, "WINDOW GEOMETRY EDITOR\n");
        fprintf(f, "\n");
        fprintf(f, "Project: %s\n", target_project_id);
        fprintf(f, "Current: x=%s y=%s width=%s height=%s\n",
                win_x[0] ? win_x : "0 (not set)", win_y[0] ? win_y : "0 (not set)",
                win_w[0] ? win_w : "0 (not set)", win_h[0] ? win_h : "0 (not set)");
        fprintf(f, "\n");
        fprintf(f, "Edit via CLI Input:\n");
        fprintf(f, "<cli_io id=\"edit_x\" label=\"  X position\" target_id=\"edit_x\" /><br/>\n");
        fprintf(f, "<cli_io id=\"edit_y\" label=\"  Y position\" target_id=\"edit_y\" /><br/>\n");
        fprintf(f, "<cli_io id=\"edit_width\" label=\"  Width\" target_id=\"edit_width\" /><br/>\n");
        fprintf(f, "<cli_io id=\"edit_height\" label=\"  Height\" target_id=\"edit_height\" /><br/>\n");
        fprintf(f, "\n");
        fprintf(f, "Or use buttons (working values: x=%s y=%s w=%s h=%s):\n", edit_x, edit_y, edit_w, edit_h);
        fprintf(f, "<button label=\"[-] X\" onClick=\"KEY:5\" /><button label=\"[+] X\" onClick=\"KEY:6\" /><br/>\n");
        fprintf(f, "<button label=\"[-] Y\" onClick=\"KEY:7\" /><button label=\"[+] Y\" onClick=\"KEY:8\" /><br/>\n");
        fprintf(f, "<button label=\"[-] W\" onClick=\"KEY:9\" /><button label=\"[+] W\" onClick=\"KEY:10\" /><br/>\n");
        fprintf(f, "<button label=\"[-] H\" onClick=\"KEY:11\" /><button label=\"[+] H\" onClick=\"KEY:12\" /><br/>\n");
        fprintf(f, "\n");
        fprintf(f, "<button label=\"Apply Changes\" onClick=\"KEY:13\" /><br/>\n");
        fprintf(f, "<button label=\"Back to Project List\" onClick=\"SETTINGS_PAGE:window-geom\" /><br/>\n");
        fprintf(f, "\n");
        fprintf(f, "Status: %s\n", status);
    } else {
        fprintf(f, "[Embedded page: %s]\n", active_page);
        fprintf(f, "[For full editing, open from chrome button]\n");
        fprintf(f, "\n");
        fprintf(f, "<button label=\"Back\" onClick=\"SETTINGS_PAGE:settings\" /><br/>\n");
    }

    fclose(f);
    rename(tmp_path, body_path);
}

void trigger_render(void) {
    char frame_marker[4096];
    char fs_watch_marker[4096];
    FILE *f;

    snprintf(frame_marker, sizeof(frame_marker), "%s/pieces/display/frame_changed.txt", project_root);
    f = fopen(frame_marker, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }

    /* Bumping frame_changed.txt alone only tells the renderer "redraw now" --
       it draws whatever wraith-alpha_manager.c last embedded via
       update_state(), which is NOT re-run just because this marker grew.
       process_active_project_marker() in wraith-alpha_manager.c already
       polls session/fs_watch.marker for the currently-focused project at
       ~60Hz and calls update_state()+trigger_render() on growth -- bumping
       that same marker here (settings is the focused window whenever this
       runs) plugs into that existing loop instead of relying on some
       unrelated later event to happen to refresh the embedded body. */
    snprintf(fs_watch_marker, sizeof(fs_watch_marker), "%s/session/fs_watch.marker", project_dir);
    f = fopen(fs_watch_marker, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }
}

int main(void) {
    struct stat st;
    off_t last_state_changed_size = 0;
    char state_changed_path[4096];

    resolve_paths();
    log_debug("Settings Hub Manager Started");
    log_debug("Project root: %s", project_root);
    log_debug("Project dir: %s", project_dir);

    write_state();
    write_wraith_body();
    trigger_render();

    snprintf(state_changed_path, sizeof(state_changed_path), "%s/session/state_changed.txt", project_dir);
    if (stat(state_changed_path, &st) == 0) {
        last_state_changed_size = st.st_size;
    }

    log_debug("Settings Hub Manager: init complete, entering marker-file-watch loop");

    while (1) {
        /* Watch state_changed.txt marker file for growth (when user clicks a page button via SETTINGS_PAGE action) */
        if (stat(state_changed_path, &st) == 0 && st.st_size > last_state_changed_size) {
            last_state_changed_size = st.st_size;
            log_debug("Page change detected, re-rendering wraith_body.txt");
            write_wraith_body();
            trigger_render();
        }

        usleep(50000); /* 50ms poll interval */
    }

    return 0;
}
