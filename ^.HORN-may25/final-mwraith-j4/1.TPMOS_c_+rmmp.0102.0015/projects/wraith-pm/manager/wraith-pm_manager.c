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

#define MAX_PATH_LEN 4096
#define MAX_LINE 4096

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

typedef struct {
    char id[64], title[256], role[48], src[MAX_PATH_LEN];
    int x, y, w, h, z, focused, minimized;
} Window;

typedef struct { char id[32]; char path[MAX_PATH_LEN]; } WraithProject;

typedef struct {
    char focused_window[64], nav_scope[64], drag_window_id[64];
    int selected_index, mouse_x, mouse_y, drag_active;
    int drag_start_x, drag_start_y, drag_window_start_x, drag_window_start_y;
    WraithProject projects[10]; int project_count;
    Window windows[20]; int window_count;
} NavState;

static char* extract_val(const char* line, const char* key, char* dst, size_t sz) {
    char needle[64]; snprintf(needle, 64, "%s=", key);
    char *p = strstr(line, needle); if (!p) return NULL;
    p += strlen(needle);
    size_t i = 0; while (*p && *p != '|' && i < sz - 1) { dst[i++] = *p++; }
    dst[i] = '\0'; return trim_ws(dst);
}

static void load_nav_state(const char *root, NavState *state) {
    char path[MAX_PATH_LEN], line[MAX_LINE]; FILE *f;
    memset(state, 0, sizeof(*state));
    
    // Defaults
    strncpy(state->focused_window, "terminal_window", 63);
    strncpy(state->nav_scope, "terminal_menu", 63);
    state->selected_index = 1;

    // Projects
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/wraith-projects/projects.kvp", root);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f) && state->project_count < 10) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0'; strncpy(state->projects[state->project_count].id, trim_ws(line), 31);
                strncpy(state->projects[state->project_count].path, trim_ws(eq + 1), MAX_PATH_LEN - 1);
                state->project_count++;
            }
        }
        fclose(f);
    }

    // Windows
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/windows_state.pdl", root);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f) && state->window_count < 20) {
            if (strncmp(line, "WINDOW |", 8) == 0) {
                Window *w = &state->windows[state->window_count++];
                extract_val(line, "id", w->id, 64); extract_val(line, "title", w->title, 256);
                char tmp[256];
                if (extract_val(line, "x", tmp, 256)) w->x = atoi(tmp);
                if (extract_val(line, "y", tmp, 256)) w->y = atoi(tmp);
                if (extract_val(line, "w", tmp, 256)) w->w = atoi(tmp);
                if (extract_val(line, "h", tmp, 256)) w->h = atoi(tmp);
                if (extract_val(line, "z", tmp, 256)) w->z = atoi(tmp);
                if (extract_val(line, "focused", tmp, 256)) w->focused = (strcmp(tmp, "true") == 0);
                if (extract_val(line, "minimized", tmp, 256)) w->minimized = (strcmp(tmp, "true") == 0);
                extract_val(line, "role", w->role, 48); extract_val(line, "src", w->src, MAX_PATH_LEN);
            }
        }
        fclose(f);
    }

    // Transient State
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/state.txt", root);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '='); if (!eq) continue; *eq = '\0';
            char *k = trim_ws(line), *v = trim_ws(eq + 1);
            if (strcmp(k, "mouse_x") == 0) state->mouse_x = atoi(v);
            else if (strcmp(k, "mouse_y") == 0) state->mouse_y = atoi(v);
            else if (strcmp(k, "drag_active") == 0) state->drag_active = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
            else if (strcmp(k, "drag_window_id") == 0) strncpy(state->drag_window_id, v, 63);
            else if (strcmp(k, "focused_window") == 0) strncpy(state->focused_window, v, 63);
        }
        fclose(f);
    }
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/nav_state.txt", root);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '='); if (!eq) continue; *eq = '\0';
            char *k = trim_ws(line), *v = trim_ws(eq + 1);
            if (strcmp(k, "selected_index") == 0) state->selected_index = atoi(v);
            else if (strcmp(k, "nav_scope") == 0) strncpy(state->nav_scope, v, 63);
        }
        fclose(f);
    }
    if (state->selected_index < 1) state->selected_index = 1;
}

static void save_nav_state(const char *root, const NavState *state) {
    char path[MAX_PATH_LEN], taskbar_label[256], clock_str[64]; FILE *f;
    
    // Windows PDL
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/windows_state.pdl", root);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION | KEY | VALUE\n");
        for (int i = 0; i < state->window_count; i++) {
            const Window *w = &state->windows[i];
            fprintf(f, "WINDOW | id=%s | title=%s | x=%d | y=%d | w=%d | h=%d | z=%d | focused=%s | minimized=%s | role=%s | src=%s\n",
                    w->id, w->title, w->x, w->y, w->w, w->h, w->z, w->focused ? "true" : "false", w->minimized ? "true" : "false", w->role, w->src);
        }
        fclose(f);
    }

    // Nav State
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/nav_state.txt", root);
    f = fopen(path, "w"); if (f) { fprintf(f, "nav_scope=%s\nselected_index=%d\ndrag_window_id=%s\n", state->nav_scope, state->selected_index, state->drag_window_id[0] ? state->drag_window_id : "none"); fclose(f); }
    
    // Taskbar
    snprintf(taskbar_label, 256, "| [Wraith Term%s]", strcmp(state->focused_window, "terminal_window") == 0 ? "*" : "");
    for (int i = 0; i < state->window_count; i++) {
        if (strcmp(state->windows[i].id, "terminal_window") == 0) continue;
        char win_entry[128]; snprintf(win_entry, 128, " [%s%s]", state->windows[i].title, strcmp(state->focused_window, state->windows[i].id) == 0 ? "*" : "");
        strcat(taskbar_label, win_entry);
    }
    strcat(taskbar_label, " |");

    time_t now = time(NULL); struct tm *t = localtime(&now); strftime(clock_str, 64, "%H:%M %m-%d-%Y", t);
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/state.txt", root);
    f = fopen(path, "w"); if (f) { fprintf(f, "mouse_x=%d\nmouse_y=%d\ndrag_active=%s\ndrag_window_id=%s\nfocused_window=%s\ntaskbar_windows_label=%s\ntaskbar_clock=%s\n", 
                                        state->mouse_x, state->mouse_y, state->drag_active ? "true" : "false", state->drag_window_id, state->focused_window, taskbar_label, clock_str); fclose(f); }
    
    // Terminal View
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/terminal_window.view.txt", root);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < state->project_count; i++) {
            const char *marker = (state->selected_index == (i + 1)) ? "[>] " : "[ ] ";
            fprintf(f, "%s%d. %s\n", marker, i + 1, state->projects[i].id);
        }
        fclose(f);
    }
}

static int hit_test(const char *root, int x, int y, char *win_id, char *role) {
    char path[MAX_PATH_LEN], line[MAX_LINE], target[64];
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/current_frame.hitmap.pdl", root);
    FILE *f = fopen(path, "r"); if (!f) return 0;
    snprintf(target, sizeof(target), "x=%d,y=%d", x / 10, y / 18);
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, target)) {
            extract_val(line, "id", win_id, 64);
            extract_val(line, "role", role, 48);
            fclose(f); return 1;
        }
    }
    fclose(f); return 0;
}

static void trigger_render(const char *root) {
    char path[MAX_PATH_LEN]; pid_t pid;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/ops/+x/wraith_composer.+x", root);
    pid = fork();
    if (pid == 0) { execl(path, "wraith_composer.+x", (char *)NULL); _exit(127); }
    if (pid > 0) {
        waitpid(pid, NULL, 0);
        snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", root);
        FILE *f = fopen(path, "a"); if (f) { fprintf(f, "W\n"); fclose(f); }
    }
}

static void spawn_window(NavState *state, const char *id, const char *title, int x, int y, int w, int h, const char *src) {
    if (state->window_count >= 20) return;
    Window *win = &state->windows[state->window_count++];
    strncpy(win->id, id, 63); strncpy(win->title, title, 255); strncpy(win->role, "panel", 47); strncpy(win->src, src, MAX_PATH_LEN-1);
    win->x = x; win->y = y; win->w = w; win->h = h; win->z = 50; win->focused = 1; win->minimized = 0;
    strncpy(state->focused_window, id, 63);
}

static int handle_mouse(const char *root, const char *cmd, NavState *state) {
    int mx, my, btn;
    if (sscanf(cmd, "MOUSE_MOVE %d %d %d", &btn, &mx, &my) == 3) {
        state->mouse_x = mx * 10; state->mouse_y = my * 18;
        if (state->drag_active && strcmp(state->drag_window_id, "none") != 0) {
            for (int i=0; i<state->window_count; i++) if (strcmp(state->windows[i].id, state->drag_window_id) == 0) {
                state->windows[i].x = state->drag_window_start_x + (state->mouse_x - state->drag_start_x);
                state->windows[i].y = state->drag_window_start_y + (state->mouse_y - state->drag_start_y);
                break;
            }
        }
        return 1;
    } else if (strstr(cmd, "wraith-pm::down")) {
        char win_id[64], role[48];
        if (hit_test(root, state->mouse_x, state->mouse_y, win_id, role)) {
            if (strcmp(win_id, "none") != 0) {
                strncpy(state->focused_window, win_id, 63);
                Window tmp; int found = -1;
                for(int i=0; i<state->window_count; i++) if(strcmp(state->windows[i].id, win_id)==0) { found = i; break; }
                if(found != -1) {
                    tmp = state->windows[found];
                    for(int i=found; i<state->window_count-1; i++) state->windows[i] = state->windows[i+1];
                    state->windows[state->window_count-1] = tmp;
                    for(int i=0; i<state->window_count; i++) state->windows[i].focused = (i == state->window_count-1);
                }
                if (strcmp(role, "titlebar") == 0) {
                    state->drag_active = 1; strncpy(state->drag_window_id, win_id, 63);
                    state->drag_start_x = state->mouse_x; state->drag_start_y = state->mouse_y;
                    state->drag_window_start_x = state->windows[state->window_count-1].x;
                    state->drag_window_start_y = state->windows[state->window_count-1].y;
                } else if (strcmp(role, "close_btn") == 0) {
                    for(int i=found; i<state->window_count-1; i++) state->windows[i] = state->windows[i+1];
                    state->window_count--; strncpy(state->focused_window, "terminal_window", 63);
                } else if (strcmp(role, "min_btn") == 0) {
                    state->windows[state->window_count-1].minimized = 1;
                }
                if (strcmp(win_id, "terminal_window") == 0) strncpy(state->nav_scope, "terminal_menu", 63);
                else strncpy(state->nav_scope, "window_body", 63);
            }
        }
        return 1;
    } else if (strstr(cmd, "wraith-pm::up")) { state->drag_active = 0; strncpy(state->drag_window_id, "none", 63); return 1; }
    return 0;
}

static int handle_key(const char *root, int key, NavState *state) {
    if (key == 1002) { state->selected_index--; if (state->selected_index < 1) state->selected_index = 1; return 1; }
    if (key == 1003) { state->selected_index++; int max = (strcmp(state->nav_scope, "terminal_menu") == 0) ? state->project_count : 1; if (state->selected_index > max) state->selected_index = max; return 1; }
    if (key >= '1' && key <= '9') { int val = key - '0'; if (val <= state->project_count) state->selected_index = val; return 1; }
    if (key == 10 || key == 13) {
        if (strcmp(state->nav_scope, "terminal_menu") == 0 && state->selected_index <= state->project_count) {
            WraithProject *p = &state->projects[state->selected_index-1];
            if (strcmp(p->id, "game-map") == 0) spawn_window(state, "game_map_window", "GAME MAP:", 340, 150, 420, 270, "game_map_window.view.txt");
            else spawn_window(state, p->id, p->id, 100, 100, 400, 200, "none");
            return 1;
        }
    }
    return 0;
}

void* history_thread(void* arg) {
    char path[MAX_PATH_LEN], *root = (char*)arg; long last_pos = 0; struct stat st;
    snprintf(path, sizeof(path), "%s/projects/wraith-pm/session/history.txt", root);
    if (stat(path, &st) == 0) last_pos = st.st_size;
    while (!g_shutdown) {
        if (stat(path, &st) == 0 && st.st_size > last_pos) {
            FILE *f = fopen(path, "r");
            if (f) {
                NavState state; load_nav_state(root, &state); int changed = 0; char line[MAX_LINE]; fseek(f, last_pos, SEEK_SET);
                while (fgets(line, sizeof(line), f)) {
                    char *cmd = strstr(line, "COMMAND:"), *kp = strstr(line, "KEY_PRESSED:");
                    if (cmd) { if (handle_mouse(root, trim_ws(cmd + 8), &state)) changed = 1; }
                    else if (kp) { if (handle_key(root, atoi(trim_ws(kp + 12)), &state)) changed = 1; }
                }
                last_pos = ftell(f); fclose(f);
                if (changed) { save_nav_state(root, &state); trigger_render(root); }
            }
        }
        usleep(16667);
    }
    return NULL;
}

int main(void) {
    pthread_t thread; setpgid(0, 0); signal(SIGINT, handle_signal); signal(SIGTERM, handle_signal);
    resolve_root(g_project_root, sizeof(g_project_root));
    trigger_render(g_project_root);
    pthread_create(&thread, NULL, history_thread, (void*)g_project_root);
    while (!g_shutdown) usleep(1000000);
    pthread_join(thread, NULL); return 0;
}
