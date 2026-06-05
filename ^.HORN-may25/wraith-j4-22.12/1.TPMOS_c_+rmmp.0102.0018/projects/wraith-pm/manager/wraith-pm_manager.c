#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE 4096

#define WINDOW_TERMINAL "terminal_window"
#define WINDOW_GAME_MAP "game_map_window"
#define WINDOW_BLANK_PROJ "blank_proj_window"
#define NAV_SCOPE_TERMINAL "terminal_menu"
#define NAV_SCOPE_GAME_MAP "game_map_window"
#define NAV_SCOPE_BLANK_PROJ "blank_proj_window"

static volatile sig_atomic_t g_shutdown = 0;
static char g_project_root[MAX_PATH_LEN] = ".";

static void handle_signal(int sig) { (void)sig; g_shutdown = 1; }

static char *trim_ws(char *s) {
    char *end = NULL; if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static int root_has_anchors(const char *root) {
    char p1[MAX_PATH_LEN], p2[MAX_PATH_LEN];
    snprintf(p1, sizeof(p1), "%s/pieces", root); snprintf(p2, sizeof(p2), "%s/projects", root);
    return access(p1, F_OK) == 0 && access(p2, F_OK) == 0;
}

static void resolve_root(char *project_root, size_t size) {
    if (getcwd(project_root, size) && root_has_anchors(project_root)) return;
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *value = trim_ws(line + 13);
                if (root_has_anchors(value)) { strncpy(project_root, value, size - 1); project_root[size-1] = '\0'; }
                break;
            }
        }
        fclose(kvp);
    }
}

typedef struct { char id[32]; char path[MAX_PATH_LEN]; } WraithProject;
static WraithProject g_projects[10];
static int g_project_count = 0;
static int g_selected_index = 1;
static int g_mouse_x = -1, g_mouse_y = -1;

static void update_session_kv(const char *root, const char *rel_path, const char *key, const char *value);

static const char *nav_scope_for_window(const char *window_id) {
    if (strcmp(window_id, WINDOW_GAME_MAP) == 0) return NAV_SCOPE_GAME_MAP;
    if (strcmp(window_id, WINDOW_BLANK_PROJ) == 0) return NAV_SCOPE_BLANK_PROJ;
    return NAV_SCOPE_TERMINAL;
}

static void set_focus_and_scope(const char *root, const char *window_id) {
    update_session_kv(root, "projects/wraith-pm/session/state.txt", "focused_window", window_id);
    update_session_kv(root, "projects/wraith-pm/session/nav_state.txt", "nav_scope", nav_scope_for_window(window_id));
}

static void update_session_kv(const char *root, const char *rel_path, const char *key, const char *value) {
    char path[MAX_PATH_LEN], lines[128][MAX_LINE];
    int count = 0, found = 0;
    snprintf(path, sizeof(path), "%s/%s", root, rel_path);

    FILE *f = fopen(path, "r");
    if (f) {
        while (count < 127 && fgets(lines[count], sizeof(lines[count]), f)) {
            char *eq = strchr(lines[count], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_ws(lines[count]), key) == 0) {
                    snprintf(lines[count], sizeof(lines[count]), "%s=%s\n", key, value);
                    found = 1;
                } else {
                    *eq = '=';
                }
            }
            count++;
        }
        fclose(f);
    }

    if (!found && count < 128) snprintf(lines[count++], sizeof(lines[0]), "%s=%s\n", key, value);

    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < count; i++) fputs(lines[i], f);
        fclose(f);
    }
}

static void read_session_value(const char *root, const char *rel_path, const char *key, char *out, size_t out_sz) {
    char path[MAX_PATH_LEN], line[MAX_LINE];
    snprintf(path, sizeof(path), "%s/%s", root, rel_path);
    out[0] = '\0';

    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(trim_ws(line), key) == 0) {
            strncpy(out, trim_ws(eq + 1), out_sz - 1);
            out[out_sz - 1] = '\0';
            break;
        }
    }
    fclose(f);
}

static void ensure_session_default(const char *root, const char *rel_path, const char *key, const char *value) {
    char current[MAX_LINE];
    read_session_value(root, rel_path, key, current, sizeof(current));
    if (!current[0]) update_session_kv(root, rel_path, key, value);
}

static void sync_taskbar_state(const char *root, const char *focused_window) {
    char open_game_map[16], open_blank_proj[16];
    char label[256] = "| ";
    read_session_value(root, "projects/wraith-pm/session/state.txt", "open_game_map_window", open_game_map, sizeof(open_game_map));
    read_session_value(root, "projects/wraith-pm/session/state.txt", "open_blank_proj_window", open_blank_proj, sizeof(open_blank_proj));

    strcat(label, strcmp(focused_window, "terminal_window") == 0 ? "[Wraith Term*]" : "[Wraith Term]");
    if (strcmp(open_blank_proj, "true") == 0) {
        strcat(label, " ");
        strcat(label, strcmp(focused_window, "blank_proj_window") == 0 ? "[Blank Proj*]" : "[Blank Proj]");
    }
    if (strcmp(open_game_map, "true") == 0) {
        strcat(label, " ");
        strcat(label, strcmp(focused_window, "game_map_window") == 0 ? "[Game Map*]" : "[Game Map]");
    }
    strcat(label, " |");
    update_session_kv(root, "projects/wraith-pm/session/state.txt", "taskbar_windows_label", label);
}

static void sync_clock_state(const char *root) {
    char clock_buf[64];
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    if (!tm_now) return;
    strftime(clock_buf, sizeof(clock_buf), "%H:%M %m-%d-%Y", tm_now);
    update_session_kv(root, "projects/wraith-pm/session/state.txt", "taskbar_clock", clock_buf);
}

static void sync_windows_registry(const char *root) {
    char focused_window[64];
    char nav_scope[64];
    char open_game_map[16], open_blank_proj[16];
    char path[MAX_PATH_LEN];
    int term_focused = 1, game_focused = 0, blank_focused = 0;
    int term_z = 40, game_z = 30, blank_z = 20;

    read_session_value(root, "projects/wraith-pm/session/state.txt", "focused_window", focused_window, sizeof(focused_window));
    read_session_value(root, "projects/wraith-pm/session/nav_state.txt", "nav_scope", nav_scope, sizeof(nav_scope));
    read_session_value(root, "projects/wraith-pm/session/state.txt", "open_game_map_window", open_game_map, sizeof(open_game_map));
    read_session_value(root, "projects/wraith-pm/session/state.txt", "open_blank_proj_window", open_blank_proj, sizeof(open_blank_proj));

    if (strcmp(focused_window, WINDOW_GAME_MAP) == 0 && strcmp(open_game_map, "true") == 0) {
        term_focused = 0;
        game_focused = 1;
        term_z = 30;
        game_z = 40;
    } else if (strcmp(focused_window, WINDOW_BLANK_PROJ) == 0 && strcmp(open_blank_proj, "true") == 0) {
        term_focused = 0;
        blank_focused = 1;
        term_z = 30;
        blank_z = 40;
    } else {
        strcpy(focused_window, WINDOW_TERMINAL);
        set_focus_and_scope(root, focused_window);
    }

    if (strcmp(nav_scope, nav_scope_for_window(focused_window)) != 0) {
        update_session_kv(root, "projects/wraith-pm/session/nav_state.txt", "nav_scope", nav_scope_for_window(focused_window));
    }

    sync_clock_state(root);
    sync_taskbar_state(root, focused_window);

    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/windows_state.pdl", root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "WINDOW | id=terminal_window | title=Wraith Terminal v1.0 | x=2 | y=2 | w=30 | h=8 | z=%d | focused=%s | minimized=false | role=panel | src=terminal_window.view.txt\n",
            term_z, term_focused ? "true" : "false");
    if (strcmp(open_blank_proj, "true") == 0) {
        fprintf(f, "WINDOW | id=blank_proj_window | title=blank-proj | x=16 | y=6 | w=30 | h=10 | z=%d | focused=%s | minimized=false | role=panel | src=blank_proj_window.view.txt\n",
                blank_z, blank_focused ? "true" : "false");
    }
    if (strcmp(open_game_map, "true") == 0) {
        fprintf(f, "WINDOW | id=game_map_window | title=GAME MAP: | x=18 | y=6 | w=26 | h=13 | z=%d | focused=%s | minimized=false | role=panel | src=game_map_window.view.txt\n",
                game_z, game_focused ? "true" : "false");
    }
    fclose(f);
}

static void ensure_default_state(const char *root) {
    ensure_session_default(root, "projects/wraith-pm/session/state.txt", "focused_window", WINDOW_TERMINAL);
    ensure_session_default(root, "projects/wraith-pm/session/state.txt", "open_game_map_window", "false");
    ensure_session_default(root, "projects/wraith-pm/session/state.txt", "open_blank_proj_window", "false");
    ensure_session_default(root, "projects/wraith-pm/session/nav_state.txt", "selected_index", "1");
    ensure_session_default(root, "projects/wraith-pm/session/nav_state.txt", "nav_scope", NAV_SCOPE_TERMINAL);
    ensure_session_default(root, "projects/wraith-pm/session/nav_state.txt", "drag_active", "false");
    ensure_session_default(root, "projects/wraith-pm/session/nav_state.txt", "drag_window_id", "none");
}

static void normalize_boot_state(const char *root) {
    update_session_kv(root, "projects/wraith-pm/session/state.txt", "open_game_map_window", "false");
    update_session_kv(root, "projects/wraith-pm/session/state.txt", "open_blank_proj_window", "false");
    update_session_kv(root, "projects/wraith-pm/session/nav_state.txt", "selected_index", "1");
    set_focus_and_scope(root, WINDOW_TERMINAL);
}

static void load_nav_state(const char *root) {
    char selected[32];
    read_session_value(root, "projects/wraith-pm/session/nav_state.txt", "selected_index", selected, sizeof(selected));
    if (selected[0]) {
        g_selected_index = atoi(selected);
        if (g_selected_index < 1) g_selected_index = 1;
    }
}

static void clamp_selected_index(void) {
    if (g_project_count < 1) {
        g_selected_index = 1;
        return;
    }
    if (g_selected_index < 1) g_selected_index = 1;
    if (g_selected_index > g_project_count) g_selected_index = g_project_count;
}

static void sync_nav_state(const char *root) {
    char selected[32];
    snprintf(selected, sizeof(selected), "%d", g_selected_index);
    update_session_kv(root, "projects/wraith-pm/session/nav_state.txt", "selected_index", selected);
}

static void open_selected_project(const char *root) {
    const char *project_id = NULL;
    if (g_selected_index < 1 || g_selected_index > g_project_count) return;
    project_id = g_projects[g_selected_index - 1].id;

    if (strcmp(project_id, "game-map") == 0) {
        update_session_kv(root, "projects/wraith-pm/session/state.txt", "open_game_map_window", "true");
        set_focus_and_scope(root, WINDOW_GAME_MAP);
    } else if (strcmp(project_id, "blank-proj") == 0) {
        update_session_kv(root, "projects/wraith-pm/session/state.txt", "open_blank_proj_window", "true");
        set_focus_and_scope(root, WINDOW_BLANK_PROJ);
    }
}

static void set_piece_state(const char *root, const char *piece_id, const char *key, const char *val) {
    char path[MAX_PATH_LEN], lines[100][MAX_LINE];
    if (strcmp(piece_id, "mousehand") == 0) 
        snprintf(path, sizeof(path), "%s/projects/wraith-pm/pieces/mousehand/state.txt", root);
    else 
        snprintf(path, sizeof(path), "%s/projects/wraith-pm/pieces/world/map_01/%s/state.txt", root, piece_id);
    
    FILE *f = fopen(path, "r"); if (!f) return;
    int count = 0, found = 0;
    while (fgets(lines[count], MAX_LINE, f) && count < 99) {
        if (strncmp(lines[count], key, strlen(key)) == 0 && lines[count][strlen(key)] == '=') {
            snprintf(lines[count], MAX_LINE, "%s=%s\n", key, val);
            found = 1;
        }
        count++;
    }
    fclose(f);
    if (!found && count < 100) snprintf(lines[count++], MAX_LINE, "%s=%s\n", key, val);
    f = fopen(path, "w"); if (f) { for (int i = 0; i < count; i++) fputs(lines[i], f); fclose(f); }
}

static unsigned long long g_last_view_sum = 0;

static void trigger_render(const char *root) {
    char path[MAX_PATH_LEN], view_path[MAX_PATH_LEN]; pid_t pid;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/ops/+x/wraith_composer.+x", root);
    snprintf(view_path, sizeof(view_path), "%s/projects/wraith-pm/manager/view.txt", root);

    pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
        execl(path, "wraith_composer.+x", (char *)NULL); 
        _exit(127); 
    }
    if (pid > 0) { 
        waitpid(pid, NULL, 0); 
        
        /* CONTENT-CHANGE GUARD: Check if view.txt actually changed */
        unsigned long long current_sum = 0;
        FILE *vf = fopen(view_path, "r");
        if (vf) {
            int c; while ((c = fgetc(vf)) != EOF) current_sum += (unsigned char)c;
            fclose(vf);
        }
        
        if (current_sum == g_last_view_sum && g_last_view_sum != 0) return;
        g_last_view_sum = current_sum;

        snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", root); 
        FILE *f = fopen(path, "a"); if (f) { fprintf(f, "W\n"); fclose(f); } 
    }
}

static void load_projects(const char *root) {
    char path[MAX_PATH_LEN], line[MAX_LINE];
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/wraith-projects/projects.kvp", root);
    FILE *f = fopen(path, "r");
    g_project_count = 0;
    if (f) {
        while (fgets(line, sizeof(line), f) && g_project_count < 10) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                strncpy(g_projects[g_project_count].id, trim_ws(line), 31);
                strncpy(g_projects[g_project_count].path, trim_ws(eq + 1), MAX_PATH_LEN - 1);
                g_project_count++;
            }
        }
        fclose(f);
    }
}

static void update_terminal_view(const char *root) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/terminal_window.view.txt", root);
    FILE *f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < g_project_count; i++) {
            const char *marker = (g_selected_index == (i + 1)) ? "[>] " : "[ ] ";
            fprintf(f, "%s%d. %s\n", marker, i + 1, g_projects[i].id);
        }
        fclose(f);
    }
}

static void update_gui_state(const char *root) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/manager/gui_state.txt", root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "app_title=Wraith PM\n");
        fprintf(f, "mouse_x=%d\n", g_mouse_x);
        fprintf(f, "mouse_y=%d\n", g_mouse_y);
        fprintf(f, "project_id=wraith-pm\n");
        fprintf(f, "selected_index=%d\n", g_selected_index);
        fclose(f);
    }
}

static int route_input(const char *root, const char *line) {
    int key;
    int changed = 0;

    if (strstr(line, "COMMAND: OP wraith-pm::focus terminal")) {
        set_focus_and_scope(root, WINDOW_TERMINAL);
        changed = 1;
    } else if (strstr(line, "COMMAND: OP wraith-pm::focus map")) {
        char open_game_map[16];
        read_session_value(root, "projects/wraith-pm/session/state.txt", "open_game_map_window", open_game_map, sizeof(open_game_map));
        if (strcmp(open_game_map, "true") == 0) {
            set_focus_and_scope(root, WINDOW_GAME_MAP);
            changed = 1;
        }
    } else if (strstr(line, "COMMAND: OP wraith-pm::render")) {
        changed = 1;
    } else if (strstr(line, "MOUSE_MOVE")) {
        /* MOUSE IGNORED FOR NAV TESTING */
        return 0;
    } else if (isdigit(line[0]) || strstr(line, "KEY_PRESSED:")) {
        const char *p = line;
        if (strstr(line, "KEY_PRESSED:")) p = strstr(line, "KEY_PRESSED:") + 12;
        key = atoi(p);
        if (key == 1002 || key == 'w') { // UP
            g_selected_index--; if (g_selected_index < 1) g_selected_index = 1;
            changed = 1;
        } else if (key == 1003 || key == 's') { // DOWN
            g_selected_index++; if (g_selected_index > g_project_count) g_selected_index = g_project_count;
            changed = 1;
        } else if (key == 10 || key == 13) {
            open_selected_project(root);
            changed = 1;
        } else if (key >= '1' && key <= '9') {
            int val = key - '0';
            if (val <= g_project_count) { g_selected_index = val; changed = 1; }
        }
    }
    return changed;
}

void* history_thread(void* arg) {
    char path[MAX_PATH_LEN], *root = (char*)arg; long last_pos = 0; struct stat st;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/history.txt", root);
    
    if (stat(path, &st) == 0) last_pos = st.st_size;

    while (!g_shutdown) {
        if (stat(path, &st) == 0 && st.st_size > last_pos) {
            FILE *f = fopen(path, "r");
            if (f) {
                fseek(f, last_pos, SEEK_SET);
                char line[MAX_LINE];
                int batch_changed = 0;
                while (fgets(line, sizeof(line), f)) {
                    if (route_input(root, line)) batch_changed = 1;
                }
                last_pos = ftell(f);
                fclose(f);
                
                if (batch_changed) {
                    sync_nav_state(root);
                    update_terminal_view(root);
                    update_gui_state(root);
                    sync_windows_registry(root);
                    trigger_render(root);
                }
            }
        }
        usleep(16667);
    }
    return NULL;
}

static void init_mouse_from_piece(const char *root) {
    char path[MAX_PATH_LEN], line[MAX_LINE];
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/pieces/mousehand/state.txt", root);
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_ws(line), *v = trim_ws(eq + 1);
                if (strcmp(k, "pos_x") == 0) g_mouse_x = atoi(v);
                else if (strcmp(k, "pos_y") == 0) g_mouse_y = atoi(v);
            }
        }
        fclose(f);
    }
}

int main(void) {
    pthread_t thread; setpgid(0, 0); 
    signal(SIGINT, handle_signal); signal(SIGTERM, handle_signal);
    resolve_root(g_project_root, sizeof(g_project_root));
    
    init_mouse_from_piece(g_project_root);
    ensure_default_state(g_project_root);
    load_projects(g_project_root);
    normalize_boot_state(g_project_root);
    load_nav_state(g_project_root);
    clamp_selected_index();
    update_terminal_view(g_project_root);
    update_gui_state(g_project_root);
    sync_nav_state(g_project_root);
    sync_windows_registry(g_project_root);
    trigger_render(g_project_root);

    pthread_create(&thread, NULL, history_thread, (void*)g_project_root);
    while (!g_shutdown) usleep(1000000);
    pthread_join(thread, NULL); 
    return 0;
}
