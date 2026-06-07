#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#define MAX_LINE 4096
#define MAX_PATH 1024

static volatile int g_shutdown = 0;
static int g_active_gui_index = 1;
static int g_max_index = 5;
static int g_desktop_window_count = 0;
static int g_desktop_window_count_override = -1;
static char g_project_root[MAX_PATH] = ".";

void handle_signal(int sig) { g_shutdown = 1; }

char* trim_ws(char *str) {
    if (!str) return NULL;
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_root() {
    if (getcwd(g_project_root, sizeof(g_project_root)) == NULL) strcpy(g_project_root, ".");
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = trim_ws(line + 13);
                if (strlen(v) > 0) strncpy(g_project_root, v, MAX_PATH - 1);
                break;
            }
        }
        fclose(kvp);
    }
}

void log_alpha(const char *fmt, ...) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/wraith-alpha/manager/alpha_manager.log", g_project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        time_t t = time(NULL);
        char *ts = ctime(&t);
        ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s] ", ts);
        va_list args; va_start(args, fmt); vfprintf(f, fmt, args); va_end(args);
        fprintf(f, "\n");
        fclose(f);
    }
}

void trigger_render() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", g_project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "A\n");
        fclose(f);
    }
}

static void sync_desktop_window_count(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/wraith-alpha/session/desktop_ui_state.txt", g_project_root);
    FILE *f = fopen(path, "r");
    if (!f) {
        g_desktop_window_count = 0;
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "desktop_window_count=", 21) == 0) {
            char *v = trim_ws(line + 21);
            g_desktop_window_count = atoi(v);
            if (g_desktop_window_count < 0) g_desktop_window_count = 0;
            fclose(f);
            return;
        }
        if (strncmp(line, "desktop_window_1_open=", 22) == 0) {
            char *v = trim_ws(line + 22);
            g_desktop_window_count = (strcmp(v, "true") == 0) ? 1 : 0;
        }
    }
    fclose(f);
}

void format_key_label(int key, char *out, size_t sz) {
    if (key >= 32 && key <= 126) snprintf(out, sz, "%c", key);
    else if (key == 1002) snprintf(out, sz, "UP");
    else if (key == 1003) snprintf(out, sz, "DOWN");
    else if (key == 1000) snprintf(out, sz, "LEFT");
    else if (key == 1001) snprintf(out, sz, "RIGHT");
    else if (key == 10 || key == 13) snprintf(out, sz, "ENTER");
    else snprintf(out, sz, "%d", key);
}

void update_state(int last_key) {
    char path[MAX_PATH], tmp[MAX_PATH], label[64];
    snprintf(path, sizeof(path), "%s/projects/wraith-alpha/session/alpha_state.txt", g_project_root);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    format_key_label(last_key, label, sizeof(label));
    if (g_desktop_window_count_override >= 0) {
        g_desktop_window_count = g_desktop_window_count_override;
    } else {
        sync_desktop_window_count();
    }
    char focused_title[64];
    if (g_desktop_window_count > 0) snprintf(focused_title, sizeof(focused_title), "Terminal #%d", g_desktop_window_count);
    else snprintf(focused_title, sizeof(focused_title), "Desktop");

    FILE *f = fopen(tmp, "w");
    if (f) {
        fprintf(f, "project_id=wraith-alpha\n");
        fprintf(f, "desktop_mode=desktop\n");
        fprintf(f, "desktop_window_count=%d\n", g_desktop_window_count);
        fprintf(f, "desktop_default_window_id=terminal\n");
        fprintf(f, "desktop_default_window_title=Terminal\n");
        fprintf(f, "desktop_focused_window_id=terminal\n");
        fprintf(f, "desktop_focused_window_title=%s\n", focused_title);
        fprintf(f, "desktop_launcher_count=1\n");
        fprintf(f, "desktop_launcher_1_id=terminal\n");
        fprintf(f, "desktop_launcher_1_title=Terminal\n");
        fprintf(f, "desktop_launcher_1_kind=launcher\n");
        fprintf(f, "desktop_window_1_id=terminal\n");
        fprintf(f, "desktop_window_1_title=%s\n", g_desktop_window_count > 0 ? focused_title : "Terminal");
        fprintf(f, "desktop_window_1_kind=terminal\n");
        fprintf(f, "desktop_window_1_open=%s\n", g_desktop_window_count > 0 ? "true" : "false");
        fprintf(f, "desktop_window_1_collapsed=false\n");
        fprintf(f, "desktop_window_1_body_visible=%s\n", g_desktop_window_count > 0 ? "true" : "false");
        fprintf(f, "desktop_window_1_hidden=%s\n", g_desktop_window_count > 0 ? "false" : "true");
        fprintf(f, "active_gui_index=%d\n", g_active_gui_index);
        fprintf(f, "current_key_label=%s\n", label);
        fprintf(f, "current_key_raw=%d\n", last_key);
        fclose(f);
        rename(tmp, path);

        /* Keep the desktop UI projection in sync with canonical manager state.
         * The parser loads desktop_ui_state.txt last, so this must reflect launch
         * state as well as chrome state or the desktop will snap back closed. */
        char ui_path[MAX_PATH], ui_tmp[MAX_PATH];
        snprintf(ui_path, sizeof(ui_path), "%s/projects/wraith-alpha/session/desktop_ui_state.txt", g_project_root);
        snprintf(ui_tmp, sizeof(ui_tmp), "%s.tmp", ui_path);
        FILE *ui = fopen(ui_tmp, "w");
        if (ui) {
            fprintf(ui, "desktop_window_count=%d\n", g_desktop_window_count);
            fprintf(ui, "desktop_window_1_open=%s\n", g_desktop_window_count > 0 ? "true" : "false");
            fprintf(ui, "desktop_window_1_collapsed=false\n");
            fprintf(ui, "desktop_window_1_body_visible=%s\n", g_desktop_window_count > 0 ? "true" : "false");
            fprintf(ui, "desktop_window_1_collapse_glyph=-\n");
            fprintf(ui, "desktop_window_1_fullscreen=false\n");
            fprintf(ui, "desktop_window_1_mode=half\n");
            fprintf(ui, "desktop_window_1_halfscreen_visible=true\n");
            fprintf(ui, "desktop_window_1_hidden=%s\n", g_desktop_window_count > 0 ? "false" : "true");
            fprintf(ui, "desktop_window_1_title=%s\n", g_desktop_window_count > 0 ? focused_title : "Terminal");
            fprintf(ui, "desktop_focused_window_title=%s\n", g_desktop_window_count > 0 ? focused_title : "Desktop");
            fclose(ui);
            rename(ui_tmp, ui_path);
        } else {
            log_alpha("ERROR: Failed to open desktop UI state file for writing: %s", ui_tmp);
        }
        g_desktop_window_count_override = -1;
        log_alpha("State updated: idx=%d, key=%s", g_active_gui_index, label);
    } else {
        log_alpha("ERROR: Failed to open state file for writing: %s", tmp);
    }
}
void archive_input(int key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/wraith-alpha/session/history_archive.txt", g_project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        time_t t = time(NULL);
        char *ts = ctime(&t);
        ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s] KEY: %d\n", ts, key);
        fclose(f);
    }
}

void route_input(int key) {
    int changed = 0;
    log_alpha("Input received: %d", key);

    /* ARCHIVE IMMEDIATELY */
    archive_input(key);

    if (key >= '1' && key <= '9') {
        int digit = key - '0';
        if (digit <= g_max_index) {
            g_active_gui_index = digit;
        }
        changed = 1;
    } else if (key == 1002) { // UP
        g_active_gui_index--;
        if (g_active_gui_index < 1) g_active_gui_index = 1;
        changed = 1;
    } else if (key == 1003) { // DOWN
        g_active_gui_index++;
        if (g_active_gui_index > g_max_index) g_active_gui_index = g_max_index;
        changed = 1;
    } else if (key == 10 || key == 13) {
        if (g_active_gui_index == 1 && g_desktop_window_count == 0) {
            g_desktop_window_count = 1;
            g_desktop_window_count_override = 1;
            log_alpha("Launch requested: opening Terminal #1 from dock");
        }
        changed = 1;
    } else {
        changed = 1; 
    }

    if (changed) {
        update_state(key);
        trigger_render();
    }
}
int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    resolve_root();

    g_max_index = 5;

    log_alpha("Wraith-Alpha Manager starting in %s", g_project_root);

    long last_pos = 0;
    struct stat st;
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", g_project_root);

    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    g_desktop_window_count = 0;
    g_desktop_window_count_override = 0;
    update_state(0);
    g_desktop_window_count_override = -1;
    trigger_render();

    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *f = fopen(hist_path, "r");
                if (f) {
                    fseek(f, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), f)) {
                        char *kp = strstr(line, "KEY_PRESSED: ");
                        if (kp) {
                            int key = atoi(kp + 12);
                            if (key > 0) route_input(key);
                        }
                    }
                    last_pos = ftell(f);
                    fclose(f);
                }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
                log_alpha("History truncation detected.");
            }
        }
        usleep(16667);
    }

    log_alpha("Wraith-Alpha Manager shutting down.");
    return 0;
}
