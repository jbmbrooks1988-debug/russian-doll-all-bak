#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE_LEN 2048

/*
 * Real hot-path logic for the "settings" Wraith project. Invoked fresh on
 * every KEY: press by wraith-alpha_manager.c's run_active_project_input_op()
 * (see terminal/ops/src/wraith_project_input.c for the reference pattern
 * this follows).
 *
 * Only does anything while settings' embedded page (session/state_changed.txt's
 * last line, written by SETTINGS_PAGE:...) is "window-geom:<project_id>" --
 * i.e. the geometry editor for a specific picked project is on screen.
 * Handles KEY:5-12 (+/- nudges for x/y/width/height) and KEY:13 (Apply:
 * persist the working values into that project's own project.pdl WINDOW
 * section). Everything else (KEY: presses on the settings menu or the
 * project picker) is a no-op here -- those pages don't need a hot-path op.
 */

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    if (a[0] && a[strlen(a) - 1] == '/') snprintf(out, out_sz, "%s%s", a, b);
    else snprintf(out, out_sz, "%s/%s", a, b);
}

static char *trim_str(char *s) {
    char *end;
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static int read_last_key_pressed(const char *root) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;
    int last_key = -1;

    path_join(path, sizeof(path), root, "session/history.txt");
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "KEY_PRESSED:");
        if (p) {
            p += strlen("KEY_PRESSED:");
            while (*p && isspace((unsigned char)*p)) p++;
            last_key = atoi(p);
        }
    }
    fclose(f);
    return last_key;
}

static void read_active_page(const char *root, char *out, size_t out_sz) {
    char path[MAX_PATH_LEN];
    char line[256];
    FILE *f;

    snprintf(out, out_sz, "settings");
    path_join(path, sizeof(path), root, "session/state_changed.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0]) snprintf(out, out_sz, "%s", line);
    }
    fclose(f);
}

static void append_active_page(const char *root, const char *page) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/state_changed.txt");
    f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", page);
        fclose(f);
    }
}

/* Same substring-key/pipe-split convention as settings_manager.c's /
 * wraith-alpha_manager.c's read_pdl_value(). */
static void read_pdl_value(const char *path, const char *key, char *dst, size_t dst_sz) {
    FILE *f;
    char line[MAX_LINE_LEN];

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

/* Same atomic-write, pipe-delimited (SECTION | KEY | VALUE) update as
 * window-geom_manager.c's write_pdl_value() -- reimplemented locally
 * since this is a separate binary. */
static void write_pdl_value(const char *path, const char *section, const char *key, const char *value) {
    FILE *src, *tmp;
    char line[MAX_LINE_LEN];
    char tmp_path[MAX_PATH_LEN];
    int found = 0;

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    src = fopen(path, "r");
    tmp = fopen(tmp_path, "w");
    if (!tmp) {
        if (src) fclose(src);
        return;
    }

    if (src) {
        while (fgets(line, sizeof(line), src)) {
            char *pipe1 = strchr(line, '|');
            char line_section[64] = "", line_key[64] = "";

            if (pipe1) {
                char *pipe2 = strchr(pipe1 + 1, '|');
                if (pipe2) {
                    size_t sec_len = (size_t)(pipe1 - line);
                    size_t key_len = (size_t)(pipe2 - pipe1 - 1);
                    if (sec_len > 0 && sec_len < sizeof(line_section)) {
                        memcpy(line_section, line, sec_len);
                        line_section[sec_len] = '\0';
                        line_section[strcspn(line_section, " \t")] = '\0';
                    }
                    if (key_len > 0 && key_len < sizeof(line_key)) {
                        char *ks, *ke;
                        memcpy(line_key, pipe1 + 1, key_len);
                        line_key[key_len] = '\0';
                        ks = line_key;
                        while (*ks && isspace((unsigned char)*ks)) ks++;
                        ke = ks + strlen(ks) - 1;
                        while (ke > ks && isspace((unsigned char)*ke)) ke--;
                        ke[1] = '\0';
                        memmove(line_key, ks, strlen(ks) + 1);
                    }
                    if (strcmp(line_section, section) == 0 && strcmp(line_key, key) == 0) {
                        fprintf(tmp, "%s | %s | %s\n", section, key, value);
                        found = 1;
                        continue;
                    }
                }
            }
            fputs(line, tmp);
        }
        fclose(src);
    }

    if (!found) {
        fprintf(tmp, "%s | %s | %s\n", section, key, value);
    }

    fclose(tmp);
    rename(tmp_path, path);
}

static void derive_repo_root(const char *project_dir, char *repo_root, size_t repo_root_sz) {
    const char *needle = "/projects/wraith-alpha/wraith-projects/";
    const char *p = strstr(project_dir, needle);
    if (p) {
        size_t len = (size_t)(p - project_dir);
        if (len >= repo_root_sz) len = repo_root_sz - 1;
        memcpy(repo_root, project_dir, len);
        repo_root[len] = '\0';
        return;
    }
    snprintf(repo_root, repo_root_sz, "%s", project_dir);
}

static void trigger_render(const char *repo_root) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), repo_root, "pieces/display/frame_changed.txt");
    f = fopen(path, "a");
    if (f) {
        fprintf(f, "X\n");
        fclose(f);
    }
}

int main(int argc, char **argv) {
    const char *root;
    char repo_root[MAX_PATH_LEN];
    char active_page[256];
    char edit_state_path[MAX_PATH_LEN];
    char target_project_id[256] = "";
    char edit_x[32] = "0", edit_y[32] = "0", edit_w[32] = "0", edit_h[32] = "0";
    char status[256] = "";
    int key;
    int x, y, w, h;
    FILE *ef;

    if (argc < 2) return 2;
    root = argv[1];

    derive_repo_root(root, repo_root, sizeof(repo_root));

    read_active_page(root, active_page, sizeof(active_page));
    if (strncmp(active_page, "window-geom:", 12) != 0) {
        return 0;
    }

    key = read_last_key_pressed(root);
    if (key < 5 || key > 13) return 0;

    path_join(edit_state_path, sizeof(edit_state_path), root, "session/edit_state.txt");
    ef = fopen(edit_state_path, "r");
    if (ef) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), ef)) {
            char *eq = strchr(line, '=');
            char *k, *v, *nl;
            if (!eq) continue;
            *eq = '\0';
            k = trim_str(line);
            v = trim_str(eq + 1);
            nl = strchr(v, '\n');
            if (nl) *nl = '\0';
            if (strcmp(k, "target_project_id") == 0) strncpy(target_project_id, v, sizeof(target_project_id) - 1);
            else if (strcmp(k, "edit_x") == 0) strncpy(edit_x, v, sizeof(edit_x) - 1);
            else if (strcmp(k, "edit_y") == 0) strncpy(edit_y, v, sizeof(edit_y) - 1);
            else if (strcmp(k, "edit_width") == 0) strncpy(edit_w, v, sizeof(edit_w) - 1);
            else if (strcmp(k, "edit_height") == 0) strncpy(edit_h, v, sizeof(edit_h) - 1);
        }
        fclose(ef);
    }
    if (!target_project_id[0]) return 0;

    /* Typed cli_io values (if any) override the working values before
     * applying a nudge or Apply -- lets the user type an exact number
     * instead of only nudging with +/- buttons. Each cli_io field's
     * target_id (edit_x/edit_y/edit_width/edit_height) is its own
     * gui_state.txt variable -- see the wraith_parser_alpha.c fix that
     * made target_id actually distinguish fields instead of every cli_io
     * colliding on one shared "input_text" slot. */
    {
        char gui_state_path[MAX_PATH_LEN];
        FILE *gf;
        path_join(gui_state_path, sizeof(gui_state_path), root, "manager/gui_state.txt");
        gf = fopen(gui_state_path, "r");
        if (gf) {
            char line[MAX_LINE_LEN];
            while (fgets(line, sizeof(line), gf)) {
                char *eq = strchr(line, '=');
                char *k, *v, *nl;
                if (!eq) continue;
                *eq = '\0';
                k = trim_str(line);
                v = trim_str(eq + 1);
                nl = strchr(v, '\n');
                if (nl) *nl = '\0';
                if (v[0] && strcmp(k, "edit_x") == 0) strncpy(edit_x, v, sizeof(edit_x) - 1);
                else if (v[0] && strcmp(k, "edit_y") == 0) strncpy(edit_y, v, sizeof(edit_y) - 1);
                else if (v[0] && strcmp(k, "edit_width") == 0) strncpy(edit_w, v, sizeof(edit_w) - 1);
                else if (v[0] && strcmp(k, "edit_height") == 0) strncpy(edit_h, v, sizeof(edit_h) - 1);
            }
            fclose(gf);
        }
    }

    x = atoi(edit_x);
    y = atoi(edit_y);
    w = atoi(edit_w);
    h = atoi(edit_h);

    switch (key) {
        case 5: x -= 1; break;
        case 6: x += 1; break;
        case 7: y -= 1; break;
        case 8: y += 1; break;
        case 9: w -= 1; break;
        case 10: w += 1; break;
        case 11: h -= 1; break;
        case 12: h += 1; break;
        case 13: {
            char pdl_path[MAX_PATH_LEN];
            char val[32];

            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (w < 0) w = 0;
            if (h < 0) h = 0;

            snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", repo_root, target_project_id);
            snprintf(val, sizeof(val), "%d", x); write_pdl_value(pdl_path, "WINDOW", "window_x", val);
            snprintf(val, sizeof(val), "%d", y); write_pdl_value(pdl_path, "WINDOW", "window_y", val);
            snprintf(val, sizeof(val), "%d", w); write_pdl_value(pdl_path, "WINDOW", "window_width", val);
            snprintf(val, sizeof(val), "%d", h); write_pdl_value(pdl_path, "WINDOW", "window_height", val);

            snprintf(status, sizeof(status), "Saved to %s", target_project_id);
            break;
        }
        default: break;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (w < 0) w = 0;
    if (h < 0) h = 0;

    {
        char tmp_path[MAX_PATH_LEN];
        FILE *wf;
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", edit_state_path);
        wf = fopen(tmp_path, "w");
        if (wf) {
            fprintf(wf, "target_project_id=%s\n", target_project_id);
            fprintf(wf, "edit_x=%d\n", x);
            fprintf(wf, "edit_y=%d\n", y);
            fprintf(wf, "edit_width=%d\n", w);
            fprintf(wf, "edit_height=%d\n", h);
            fprintf(wf, "status=%s\n", status[0] ? status : "Ready to edit");
            fclose(wf);
            rename(tmp_path, edit_state_path);
        }
    }

    /* Force settings_manager's long-running loop to detect growth on
     * state_changed.txt and re-render wraith_body.txt with the new
     * working values -- reuses the exact size-growth watch SETTINGS_PAGE
     * navigation already relies on, just re-appending the CURRENT page
     * instead of a new one. */
    append_active_page(root, active_page);

    trigger_render(repo_root);

    return 0;
}
