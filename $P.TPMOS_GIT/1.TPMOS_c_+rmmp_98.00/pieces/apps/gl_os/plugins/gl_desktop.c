/*
 * gl_desktop.c - GL-OS Desktop Environment
 * Opens OpenGL window with desktop, windows, context menu, and toolbar
 * TPM-Compliant: Separate session from CHTPM terminal
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#define MAX_LINE 1024
#define MAX_WIN_TITLE 64
#define MAX_WINDOWS 15
#define MAX_PATH 4096
#define MAX_TERM_OPTIONS 10
#define MAX_OPTION_LEN 80

/* Menu Contexts */
#define CTX_MAIN 0
#define CTX_DESKTOP_APPS 1
#define CTX_TERMINAL_APPS 2
#define CTX_GLTPM_APPS 3

#define MAX_GLTPM_TILES 256
#define MAX_GLTPM_SPRITES 32
#define MAX_GLTPM_BUTTONS 8
#define MAX_GLTPM_TEXTS 8

typedef struct {
    float x, y, z;
    float extrude;
    float color[3];
    char tile_id[64];
    char ascii[16];
    char unicode[16];
} GLTPMTile;

typedef struct {
    float x, y, z;
    float color[3];
    char sprite_id[64];
    char label[64];
    char ascii[16];
    char unicode[16];
    unsigned char artifact_mask[8][8];
    float face_colors[6][3]; /* Top, Bottom, Front, Back, Right, Left */
    int has_face_colors;
} GLTPMSprite;

typedef struct {
    char id[64];
    char label[128];
    char onClick[128];
} GLTPMButton;

typedef struct {
    char label[256];
} GLTPMText;

char project_root[MAX_PATH] = ".";

typedef struct {
    char id[64];
    char name[128];
} GLProject;

GLProject gl_projects[20];
int gl_project_count = 0;

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void load_gl_projects(void) {
    gl_project_count = 0;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/gl_os/gl_os_projects.kvp", project_root);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        /* Fallback if KVP missing */
        strncpy(gl_projects[0].id, "fuzz-op-gl", 63);
        strncpy(gl_projects[0].name, "Fuzz-Op-GL", 127);
        strncpy(gl_projects[1].id, "op-ed-gl", 63);
        strncpy(gl_projects[1].name, "Op-Ed-GL", 127);
        strncpy(gl_projects[2].id, "aow-2d", 63);
        strncpy(gl_projects[2].name, "AOW-2D", 127);
        strncpy(gl_projects[3].id, "piececraft-3d", 63);
        strncpy(gl_projects[3].name, "Piececraft-3D", 127);
        gl_project_count = 4;
        return;
    }
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && gl_project_count < 20) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *id = trim_str(line);
            char *name = trim_str(eq + 1);
            if (strlen(id) > 0 && strlen(name) > 0) {
                strncpy(gl_projects[gl_project_count].id, id, 63);
                strncpy(gl_projects[gl_project_count].name, name, 127);
                gl_project_count++;
            }
        }
    }
    fclose(f);
}

typedef struct {
    int loaded;
    int camera_mode;
    int is_map_control;
    char interact_src[MAX_PATH]; /* Path to history.txt for interact mode */
    float bg_color[3];
    char title[MAX_WIN_TITLE];
    char last_key[32];
    float cam_pos[3];
    float cam_rot[3];
    int xelector_pos[3];
    int tile_count;
    int sprite_count;
    int button_count;
    int text_count;
    GLTPMTile tiles[MAX_GLTPM_TILES];
    GLTPMSprite sprites[MAX_GLTPM_SPRITES];
    GLTPMButton buttons[MAX_GLTPM_BUTTONS];
    GLTPMText texts[MAX_GLTPM_TEXTS];
} GLTPMScene;

/* Window structure */
typedef struct {
    int id;
    int x, y, w, h;
    char title[MAX_WIN_TITLE];
    char type[32];  /* "terminal", "cube", "folder", "project_mirror" */
    char project_id[64]; /* Canonical ID for projects (e.g., "fuzz-op-gl") */
    int active;
    int minimized;

    /* App-specific state */
    char input_buffer[256];
    char output_history[12][80];
    int history_count;
    float rotation_x, rotation_y;

    /* Camera & Map Control (K3/K4) */
    int is_map_control;    /* 1 = WASD/Arrows control camera/map, 0 = Menu Nav */
    int camera_mode;       /* 1=1st, 2=2nd, 3=3rd, 4=Free */
    float cam_pos[3];      /* X, Y, Z */
    float cam_rot[3];      /* Pitch, Yaw, Roll */
    
    int xelector_pos[3];   /* Map grid coordinates (Int) */
    int follow_entity_id;  /* Entity to track in 3rd person mode */

    /* Terminal menu navigation state (joystick-first) */
    int selected_index;        /* Currently highlighted option (0-based) */
    int show_menu;             /* 1 = showing menu options, 0 = text input mode */
    int option_count;          /* Number of menu options */
    char menu_options[MAX_TERM_OPTIONS][MAX_OPTION_LEN];  /* Option labels */
    int menu_context;          /* CTX_MAIN, CTX_DESKTOP_APPS, etc. */
    
    /* Thumb scroll state (Linux terminal behavior) */
    int scroll_offset;         /* Lines scrolled UP from bottom (0 = at bottom) */
    int max_scroll;            /* Maximum scroll offset (history_count - visible_lines) */
    
    /* Text selection for copy/paste */
    int is_selecting;          /* 1 = user is selecting text */
    int select_start_x;        /* Selection start X (mouse) */
    int select_start_y;        /* Selection start Y (mouse) */
    int select_end_x;          /* Selection end X (mouse) */
    int select_end_y;          /* Selection end Y (mouse) */
    char selected_text[4096];  /* Selected text buffer */
    int selection_active;      /* 1 = selection is active (mouse up after drag) */

    /* GLTPM state */
    char gltpm_layout_path[MAX_PATH];
    char nav_buffer[32];   /* "Nav > n" command buffer */
    long gltpm_frame_marker_size;
    long gltpm_layout_marker_size;
    pid_t managed_pid;
    GLTPMScene gltpm_scene;
} DesktopWindow;

/* Global state */
int window_width = 800;
int window_height = 600;
int toolbar_height = 30;
float bg_color[3] = {0.0f, 0.0f, 0.27f};  /* #000044 */
int cursor_blink = 0;

DesktopWindow windows[MAX_WINDOWS];
int window_count = 0;
int next_window_id = 1;
int active_window_id = -1;
float global_rotation = 0.0f;

/* Joystick state (global for edge-triggering) */
int last_joy_x = 0;
int last_joy_y = 0;
unsigned int last_joy_buttons = 0;

/* Mouse interaction */
int mouse_x = 0, mouse_y = 0;
int is_dragging = 0;
int is_resizing = 0;
int drag_window_id = -1;
int drag_start_x = 0, drag_start_y = 0;
int win_drag_start_x = 0, win_drag_start_y = 0;
int win_drag_start_w = 0, win_drag_start_h = 0;

/* Scroll thumb dragging (Linux terminal behavior) */
int is_scroll_thumb_drag = 0;
int scroll_window_id = -1;
int scroll_track_y = 0;
int scroll_track_h = 0;

/* Idle backoff state */
time_t last_interaction_time = 0;

/* Context menu state */
int context_menu_visible = 0;
int context_menu_x = 0, context_menu_y = 0;
int context_menu_width = 150;
int context_menu_height = 100;
int context_menu_type = 0;  /* 0 = desktop, 1 = terminal */

/* Session paths */
char *session_state_path = NULL;
char *session_history_path = NULL;
char *session_view_path = NULL;
char *session_view_changed_path = NULL;
char *master_ledger_path = NULL;
char *input_focus_lock_path = NULL;  /* Input focus lock (TPM isolation) */
char *gl_os_frame_path = NULL;        /* GL-OS current frame file */
char *gl_os_frame_pulse = NULL;       /* GL-OS frame change marker */
char *gl_os_frame_history = NULL;     /* GL-OS frame history */

long last_history_pos = 0;

#include "gltpm_parser.c"

/* Forward declarations */
void create_terminal_window(void);
void create_cube_window(void);
void create_mirrored_window(const char* project_id, const char* title);
void create_gltpm_window(const char* project_id, const char* layout_path, const char* title);
void write_view(void);
void log_master(const char* event, const char* piece);
void free_paths(void);
void write_session_state(void);
void close_window(int win_id);
void init_terminal_menu(DesktopWindow* win);
void execute_option(DesktopWindow* win, int index);
void move_selection(DesktopWindow* win, int direction);
void special_keyboard(int key, int x, int y);
void draw_text_clipped(float x, float y, float max_width, const char* text);
void draw_scroll_thumb(float x, float y, float h, int scroll_offset, int max_scroll, int visible_lines);
void write_gl_os_frame(void);  /* Write frame to file (like ASCII-OS compose_frame) */
int is_active_layout(void);
void refresh_gltpm_window(DesktopWindow* win);
void dispatch_gltpm_button(DesktopWindow* win, int index);

static int root_has_anchors(const char* root) {
    char pieces_path[MAX_PATH];
    char projects_path[MAX_PATH];

    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

void resolve_root(void) {
    FILE *kvp = NULL;

    if (getcwd(project_root, sizeof(project_root)) && root_has_anchors(project_root)) {
        return;
    }

    kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) return;

    char line[2048];
    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *v = line + 13;
            v[strcspn(v, "\n\r")] = 0;
            if (strlen(v) > 0 && root_has_anchors(v)) {
                strncpy(project_root, v, MAX_PATH-1);
            }
            break;
        }
    }
    fclose(kvp);
}

/* Check if GL-OS is currently the active layout in CHTPM */
int is_active_layout(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) {
        return 1;  /* Default to active if allocation fails */
    }
    
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 1;  /* Default to active if file missing */ }
    
    char line[1024];
    int active = 0;
    if (fgets(line, sizeof(line), f)) {
        if (strstr(line, "gl_os") || strstr(line, "desktop.chtpm")) {
            active = 1;
        }
    }
    fclose(f);
    free(path);
    return active;
}

/* Clipboard buffer */
static char clipboard_buffer[4096] = "";

/* Copy to system clipboard */
void copy_to_clipboard(const char* text) {
    if (!text || strlen(text) == 0) {
        printf("[CLIPBOARD] Copy failed - empty text\n");
        return;
    }
    
    /* Store in internal buffer */
    strncpy(clipboard_buffer, text, sizeof(clipboard_buffer) - 1);
    printf("[CLIPBOARD] Internal buffer: '%s'\n", clipboard_buffer);
    
    /* Write to clipboard.txt for sanity/debugging */
    FILE *cb = fopen("pieces/apps/gl_os/session/clipboard.txt", "w");
    if (cb) {
        fprintf(cb, "%s\n", text);
        fclose(cb);
        printf("[CLIPBOARD] Written to clipboard.txt\n");
    }
    
    /* Try to copy to system clipboard */
    char cmd[4096];
    #ifdef __APPLE__
        /* macOS */
        snprintf(cmd, sizeof(cmd), "printf '%s' | pbcopy 2>/dev/null", text);
        printf("[CLIPBOARD] Running: %s\n", cmd);
    #else
        /* Linux - try xclip first, then xsel */
        snprintf(cmd, sizeof(cmd), "printf '%s' | xclip -selection clipboard 2>/dev/null || printf '%s' | xsel --clipboard 2>/dev/null", text, text);
        printf("[CLIPBOARD] Running: %s\n", cmd);
    #endif
    
    int result = system(cmd);
    printf("[CLIPBOARD] system() returned: %d\n", result);
}

/* Paste from system clipboard */
void paste_from_clipboard(char* dest, size_t dest_size) {
    if (!dest) return;
    
    /* Try to paste from system clipboard to temp file */
    char temp_file[] = "/tmp/gl_os_cb_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) return;
    close(fd);
    
    char cmd[4096];
    #ifdef __APPLE__
        snprintf(cmd, sizeof(cmd), "pbpaste > %s 2>/dev/null", temp_file);
    #else
        snprintf(cmd, sizeof(cmd), "xclip -selection clipboard -o > %s 2>/dev/null || xsel --clipboard > %s 2>/dev/null", temp_file, temp_file);
    #endif
    
    system(cmd);
    
    /* Read from temp file */
    FILE *f = fopen(temp_file, "r");
    if (f) {
        char line[4096];
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n\r")] = 0;
            strncat(dest, line, dest_size - strlen(dest) - 1);
        }
        fclose(f);
    }
    
    remove(temp_file);
}

void build_session_path(char* dst, size_t sz, const char* rel) {
    snprintf(dst, sz, "%s/pieces/apps/gl_os/session/%s", project_root, rel);
}

void log_master(const char* event, const char* piece) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    FILE *ledger = fopen(master_ledger_path, "a");
    if (ledger) {
        fprintf(ledger, "[%s] GL-OS: %s on %s\n", timestamp, event, piece);
        fclose(ledger);
    }
}

void write_view(void) {
    FILE *fp = fopen(session_view_path, "w");
    if (!fp) return;
    
    fprintf(fp, "DESKTOP_VIEW\n");
    fprintf(fp, "BG: #000044\n");
    fprintf(fp, "WINDOW_COUNT: %d\n", window_count);
    
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        fprintf(fp, "WINDOW | %d | %d | %d | %d | %d | %s | %s | %d\n", 
                windows[i].id, windows[i].x, windows[i].y, 
                windows[i].w, windows[i].h, 
                windows[i].title, windows[i].type, windows[i].minimized);
    }
    
    fclose(fp);
    
    /* Touch view_changed marker */
    FILE *marker = fopen(session_view_changed_path, "a");
    if (marker) { 
        fprintf(marker, "G\n"); 
        fclose(marker); 
    }
}

void write_session_state(void) {
    char win_list[MAX_LINE] = "";
    char line[256];
    
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        snprintf(line, sizeof(line), "%d:%s(%d,%d),", 
                 windows[i].id, windows[i].title, windows[i].x, windows[i].y);
        if (strlen(win_list) + strlen(line) < MAX_LINE - 50) {
            strcat(win_list, line);
        }
    }
    
    FILE *f = fopen(session_state_path, "w");
    if (f) {
        fprintf(f, "project_id=gl-os\n");
        fprintf(f, "session_status=active\n");
        fprintf(f, "window_count=%d\n", window_count);
        fprintf(f, "windows=%s\n", win_list);
        fprintf(f, "active_window=%d\n", active_window_id);
        fclose(f);
    }
    
    /* Write frame to file (like ASCII-OS) */
    write_gl_os_frame();
}

void add_history(DesktopWindow* win, const char* text) {
    if (win->history_count < 12) {
        strncpy(win->output_history[win->history_count++], text, 79);
    } else {
        for (int i = 0; i < 11; i++) strcpy(win->output_history[i], win->output_history[i+1]);
        strncpy(win->output_history[11], text, 79);
    }
}

static void cleanup_managed_window(DesktopWindow* win) {
    if (!win || win->managed_pid <= 0) return;

    kill(win->managed_pid, SIGTERM);
    waitpid(win->managed_pid, NULL, WNOHANG);
    win->managed_pid = -1;
}

static void sync_gltpm_menu_from_scene(DesktopWindow* win) {
    if (!win) return;

    win->show_menu = 1;
    win->option_count = 0;
    
    /* Responsive State Sync: Only clobber host state if it's currently inactive 
       or if the scene specifically mandates a mode change (Sovereign override) */
    if (!win->is_map_control || win->gltpm_scene.is_map_control) {
        win->is_map_control = win->gltpm_scene.is_map_control;
    }
    win->camera_mode = win->gltpm_scene.camera_mode;

    /* Sync dynamic camera/xelector coordinates from scene to window struct */
    for(int i=0; i<3; i++) {
        /* SOVEREIGNTY FIX: In Free Mode (4), the Host owns the camera position.
           Don't overwrite local fly coordinates with stale manager state. */
        if (win->camera_mode != 4) {
            win->cam_pos[i] = win->gltpm_scene.cam_pos[i];
            win->cam_rot[i] = win->gltpm_scene.cam_rot[i];
        }
        win->xelector_pos[i] = win->gltpm_scene.xelector_pos[i];
    }

    for (int i = 0; i < win->gltpm_scene.button_count && i < MAX_TERM_OPTIONS; i++) {
        strncpy(win->menu_options[i], win->gltpm_scene.buttons[i].label, MAX_OPTION_LEN - 1);
        win->menu_options[i][MAX_OPTION_LEN - 1] = '\0';
        win->option_count++;
    }

    if (win->selected_index >= win->option_count) win->selected_index = 0;
}

static void launch_gltpm_manager(DesktopWindow* win) {
    char *manager_path = NULL;
    if (!win || win->managed_pid > 0) return;

    if (asprintf(&manager_path, "%s/projects/%s/manager/+x/%s_manager.+x",
                 project_root, win->project_id, win->project_id) == -1) {
        return;
    }

    if (access(manager_path, F_OK) != 0) {
        free(manager_path);
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        chdir(project_root);
        execl(manager_path, manager_path, (char *)NULL);
        _exit(127);
    }

    if (pid > 0) {
        win->managed_pid = pid;
    }

    free(manager_path);
}

void refresh_gltpm_window(DesktopWindow* win) {
    char *frame_marker = NULL;
    char *layout_marker = NULL;
    struct stat st;
    int should_reload = 0;

    if (!win || strcmp(win->type, "gltpm_app") != 0) return;

    if (!win->gltpm_scene.loaded) {
        should_reload = 1;
    }

    if (asprintf(&frame_marker, "%s/projects/%s/session/frame_changed.txt",
                 project_root, win->project_id) != -1) {
        if (stat(frame_marker, &st) == 0) {
            if (st.st_size != win->gltpm_frame_marker_size) {
                win->gltpm_frame_marker_size = st.st_size;
                should_reload = 1;
            }
        }
    }

    if (asprintf(&layout_marker, "%s/projects/%s/session/layout_changed.txt",
                 project_root, win->project_id) != -1) {
        if (stat(layout_marker, &st) == 0) {
            if (st.st_size != win->gltpm_layout_marker_size) {
                win->gltpm_layout_marker_size = st.st_size;
                should_reload = 1;
            }
        }
    }

    if (should_reload &&
        gltpm_load_scene(&win->gltpm_scene, project_root, win->project_id, win->gltpm_layout_path)) {
        if (strlen(win->gltpm_scene.title) > 0) {
            strncpy(win->title, win->gltpm_scene.title, MAX_WIN_TITLE - 1);
            win->title[MAX_WIN_TITLE - 1] = '\0';
        }
        sync_gltpm_menu_from_scene(win);
    }

    if (frame_marker) free(frame_marker);
    if (layout_marker) free(layout_marker);
}

void dispatch_gltpm_button(DesktopWindow* win, int index) {
    char *history_path = NULL;
    char audit[200];

    if (!win || strcmp(win->type, "gltpm_app") != 0) return;
    if (index < 0 || index >= win->gltpm_scene.button_count) return;

    /* Responsive INTERACT handling (Concern #3) */
    if (strcmp(win->gltpm_scene.buttons[index].onClick, "INTERACT") == 0) {
        win->is_map_control = 1;
        add_history(win, "Map Control Activated (Host).");

        /* INJECT: Notify manager via project history */
        const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";
        if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
            FILE *hf = fopen(history_path, "a");
            if (hf) {
                time_t rawtime; struct tm *timeinfo; char timestamp[64];
                time(&rawtime); timeinfo = localtime(&rawtime);
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                fprintf(hf, "[%s] COMMAND: INTERACT\n", timestamp);
                fclose(hf);
            }
            free(history_path);
        }
        glutPostRedisplay();
        return;
    }

    if (asprintf(&history_path, "%s/projects/%s/session/history.txt",
                 project_root, win->project_id) == -1) {
        return;
    }

    FILE *hf = fopen(history_path, "a");
    if (hf) {
        time_t rawtime;
        struct tm *timeinfo;
        char timestamp[64];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        if (strcmp(win->gltpm_scene.buttons[index].onClick, "INTERACT") == 0) {
            fprintf(hf, "[%s] COMMAND: INTERACT\n", timestamp);
        } else {
            fprintf(hf, "[%s] COMMAND: %s\n", timestamp, win->gltpm_scene.buttons[index].onClick);
        }
        fclose(hf);
    }

    snprintf(audit, sizeof(audit), "[GLTPM] %s", win->gltpm_scene.buttons[index].label);
    add_history(win, audit);
    free(history_path);
    write_gl_os_frame();
}

/* Write state file for parser variable substitution */
void write_gl_os_state(void) {
    FILE *f = fopen("pieces/apps/gl_os/session/state.txt", "w");
    if (!f) return;

    fprintf(f, "window_count=%d\n", window_count);
    fprintf(f, "active_window_id=%d\n", active_window_id);
    /* Desktop state variables for layout substitution */
    fprintf(f, "bg_color=%s\n", "#000044");
    fprintf(f, "name=Desktop\n");
    fprintf(f, "type=desktop\n");
    fprintf(f, "pos_x=0\n");
    fprintf(f, "pos_y=0\n");
    fprintf(f, "width=800\n");
    fprintf(f, "height=600\n");
    fprintf(f, "session_status=active\n");

    /* Get selected index from active menu-driven window */
    int selected_idx = 0;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == active_window_id && windows[i].show_menu) {
            selected_idx = windows[i].selected_index;
            break;
        }
    }
    fprintf(f, "selected_index=%d\n", selected_idx);

    /* Debug info */
    char debug_info[256];
    snprintf(debug_info, sizeof(debug_info), "M(%d,%d) S=%d", mouse_x, mouse_y, 0);
    fprintf(f, "debug_info=%s\n", debug_info);

    /* Build window list with selection indicators */
    char window_list[512] = "";
    for (int i = 0; i < window_count; i++) {
        if (windows[i].active) {
            char line[128];
            const char *indicator = (windows[i].id == active_window_id) ? "[>]" : "[ ]";
            snprintf(line, sizeof(line), "  %s %d. %-20s\n",
                     indicator, i + 1, windows[i].title);
            if (strlen(window_list) + strlen(line) < sizeof(window_list) - 50) {
                strcat(window_list, line);
            }
        }
    }
    fprintf(f, "window_list=%s", window_list);

    /* Build menu options with selection indicator - ASCII-OS style */
    char menu_options_str[1024] = "";
    int opt_count = 0;
    char (*win_opts)[MAX_OPTION_LEN] = NULL;

    /* Get menu options from the active menu-driven window */
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == active_window_id && windows[i].show_menu) {
            opt_count = windows[i].option_count;
            win_opts = windows[i].menu_options;
            break;
        }
    }

    if (win_opts && opt_count > 0) {
        /* Build menu with [>] indicator on selected item (ASCII-OS style) */
        for (int i = 0; i < opt_count; i++) {
            char line[128];
            const char *indicator = (i == selected_idx) ? "[>]" : "[ ]";
            snprintf(line, sizeof(line), "║    %s %d. [%-20s]           ║\n", 
                     indicator, i + 1, win_opts[i]);
            strcat(menu_options_str, line);
        }
    } else {
        /* Fallback for no active terminal */
        strcpy(menu_options_str, "║    No Active Terminal                      ║\n");
    }
    fprintf(f, "menu_options=%s", menu_options_str);

    /* Read desktop view from view.txt for desktop_view variable */
    FILE *vf = fopen("pieces/apps/gl_os/session/view.txt", "r");
    if (vf) {
        char view_content[4096] = "";
        char vline[256];
        while (fgets(vline, sizeof(vline), vf)) {
            /* Replace newlines with | delimiter for parser */
            vline[strcspn(vline, "\n")] = '|';
            if (strlen(view_content) + strlen(vline) < sizeof(view_content) - 50) {
                strcat(view_content, vline);
            }
        }
        fprintf(f, "desktop_view=%s", view_content);
        fclose(vf);
    }

    /* Default response */
    fprintf(f, "gl_response=\n");

    fclose(f);
}

/* Simple parser - substitute ${var} from state file */
void parse_and_write_frame(void) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    /* Read state file */
    FILE *state = fopen("pieces/apps/gl_os/session/state.txt", "r");
    if (!state) return;
    
    char window_count_str[32] = "0";
    char active_window_str[32] = "0";
    char window_list[512] = "";
    char debug_info_str[256] = "";
    char menu_options_str[1024] = "";

    char line[MAX_LINE];
    char *current_val = NULL;
    int max_len = 0;
    while (fgets(line, sizeof(line), state)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *key = line;
            char *value = eq + 1;
            
            if (strcmp(key, "window_count") == 0) { strncpy(window_count_str, value, 31); window_count_str[strcspn(window_count_str, "\n\r")] = 0; current_val = NULL; }
            else if (strcmp(key, "active_window_id") == 0) { strncpy(active_window_str, value, 31); active_window_str[strcspn(active_window_str, "\n\r")] = 0; current_val = NULL; }
            else if (strcmp(key, "window_list") == 0) { strncpy(window_list, value, 511); current_val = window_list; max_len = 511; }
            else if (strcmp(key, "debug_info") == 0) { strncpy(debug_info_str, value, 255); debug_info_str[strcspn(debug_info_str, "\n\r")] = 0; current_val = debug_info_str; max_len = 255; }
            else if (strcmp(key, "menu_options") == 0) { strncpy(menu_options_str, value, 1023); current_val = menu_options_str; max_len = 1023; }
            else current_val = NULL;
        } else if (current_val) {
            /* Continuation line */
            strncat(current_val, line, max_len - strlen(current_val));
        }
    }
    fclose(state);
    
    /* Strip trailing newline from menu_options_str if it exists, but the substitution might need it? 
       Actually, the layout has ${menu_options} on its own line. */
    menu_options_str[strcspn(menu_options_str, "\r")] = 0;
    /* We keep the \n in menu_options_str for multi-line */
    
    /* Read layout file and substitute variables */
    FILE *layout = fopen("pieces/apps/gl_os/layouts/terminal.chtml", "r");
    if (!layout) return;
    
    /* Write frame to file */
    FILE *frame_file = fopen(gl_os_frame_path, "w");
    if (!frame_file) { fclose(layout); return; }
    
    /* DEBUG: Also print to stdout */
    printf("\n=== FRAME DEBUG OUTPUT ===\n");
    
    while (fgets(line, sizeof(line), layout)) {
        /* Skip empty lines */
        if (strlen(line) < 2) continue;
        
        /* Substitute ${var} */
        char output[MAX_LINE * 2];
        strcpy(output, line);
        
        /* Simple substitution */
        char *p;
        while ((p = strstr(output, "${window_count}")) != NULL) {
            memmove(p + strlen(window_count_str), p + 15, strlen(p) - 14);
            memcpy(p, window_count_str, strlen(window_count_str));
        }
        while ((p = strstr(output, "${active_window_id}")) != NULL) {
            memmove(p + strlen(active_window_str), p + 19, strlen(p) - 18);
            memcpy(p, active_window_str, strlen(active_window_str));
        }
        while ((p = strstr(output, "${debug_info}")) != NULL) {
            memmove(p + strlen(debug_info_str), p + 13, strlen(p) - 12);
            memcpy(p, debug_info_str, strlen(debug_info_str));
        }
        while ((p = strstr(output, "${window_list}")) != NULL) {
            /* Handle multi-line window_list */
            char rest[MAX_LINE];
            strcpy(rest, p + 14);
            strcpy(p, window_list);
            strcat(p, rest);
        }
        while ((p = strstr(output, "${menu_options}")) != NULL) {
            /* Handle multi-line menu_options */
            char rest[MAX_LINE];
            strcpy(rest, p + 15);
            strcpy(p, menu_options_str);
            strcat(p, rest);
        }

        fprintf(frame_file, "%s", output);
        printf("%s", output);  /* DEBUG: Print frame line */
    }
    
    fclose(layout);
    fclose(frame_file);
    
    printf("=== END FRAME DEBUG ===\n\n");
    fflush(stdout);
    
    /* Append to frame history */
    FILE *history = fopen(gl_os_frame_history, "a");
    if (history) {
        fprintf(history, "\n--- FRAME UPDATE at %s ---\n", timestamp);
        frame_file = fopen(gl_os_frame_path, "r");
        if (frame_file) {
            while (fgets(line, sizeof(line), frame_file)) {
                fprintf(history, "%s", line);
            }
            fclose(frame_file);
        }
        fclose(history);
    }
    
    /* Touch marker file */
    FILE *pulse = fopen(gl_os_frame_pulse, "a");
    if (pulse) {
        fprintf(pulse, "G\n");
        fclose(pulse);
    }
}

void init_terminal_menu(DesktopWindow* win) {
    /* Initialize default menu options (joystick-first UX) */
    win->option_count = 3;
    win->selected_index = 0;
    win->show_menu = 1;
    win->menu_context = CTX_MAIN;
    strcpy(win->menu_options[0], "Desktop Apps");
    strcpy(win->menu_options[1], "GLTPM Apps");
    strcpy(win->menu_options[2], "Terminal Apps");

    /* Initialize scroll state (Linux terminal: thumb at bottom = newest) */
    win->scroll_offset = 0;      /* Start at bottom (newest content) */
    win->max_scroll = 0;         /* Will be calculated during render */

    /* Initialize text selection state */
    win->is_selecting = 0;
    win->select_start_x = 0;
    win->select_start_y = 0;
    win->select_end_x = 0;
    win->select_end_y = 0;
    win->selected_text[0] = '\0';
    win->selection_active = 0;

    /* Count frames in history file */
    FILE *hf = fopen(gl_os_frame_history, "r");
    if (hf) {
        char line[MAX_LINE];
        win->history_count = 0;
        while (fgets(line, sizeof(line), hf)) {
            if (strstr(line, "--- FRAME UPDATE at")) {
                win->history_count++;
            }
        }
        fclose(hf);
        printf("[DEBUG] Terminal %d: Found %d frames in history\n", win->id, win->history_count);
    }
}

void execute_option(DesktopWindow* win, int index) {
    if (index < 0 || index >= win->option_count) return;
    
    /* Add selection to history */
    char selection[200];
    snprintf(selection, sizeof(selection), "[EXECUTE] %s", win->menu_options[index]);
    add_history(win, selection);

    if (strcmp(win->type, "gltpm_app") == 0) {
        dispatch_gltpm_button(win, index);
        return;
    }
    
    /* Project Mirror Context Handling */
    if (strcmp(win->type, "project_mirror") == 0) {
        if (index == 0) {
            /* Control Map */
            win->is_map_control = 1;
            add_history(win, "Map Control Activated. WASD to move, ZX for vertical, ESC to exit.");
        } else if (index == 7 || index == 8) {
            /* Back to Selector / Exit to Menu */
            close_window(win->id);
            return;
        } else {
            add_history(win, "Action selected (logic placeholder)");
        }
    }
    /* Standard Terminal Contexts */
    else if (win->menu_context == CTX_MAIN) {
        if (index == 0) {
            /* Switch to Desktop Apps Context */
            win->menu_context = CTX_DESKTOP_APPS;
            win->selected_index = 0;
            win->option_count = gl_project_count + 3;
            for (int i = 0; i < gl_project_count; i++) {
                strcpy(win->menu_options[i], gl_projects[i].id);
            }
            strcpy(win->menu_options[gl_project_count], "GLTPM Apps ->");
            strcpy(win->menu_options[gl_project_count + 1], "3D Cube Demo");
            strcpy(win->menu_options[gl_project_count + 2], "back");
            add_history(win, "Entering Desktop Apps Category...");
        } else if (index == 1) {
            win->menu_context = CTX_GLTPM_APPS;
            win->selected_index = 0;
            win->option_count = 4;
            strcpy(win->menu_options[0], "demo_world.gltpm");
            strcpy(win->menu_options[1], "fuzz-op-gl.gltpm");
            strcpy(win->menu_options[2], "op-ed-gl.gltpm");
            strcpy(win->menu_options[3], "back");
            add_history(win, "Entering GLTPM Apps Category...");
        } else if (index == 2) {
            /* Terminal Apps */
            add_history(win, "Terminal Apps (WIP)");
        }
    } else if (win->menu_context == CTX_DESKTOP_APPS) {
        if (index < gl_project_count) {
            char history_msg[256];
            snprintf(history_msg, sizeof(history_msg), "Launching %s...", gl_projects[index].id);
            add_history(win, history_msg);
            create_mirrored_window(gl_projects[index].id, gl_projects[index].name);
        } else if (index == gl_project_count) {
            /* Shortcut to GLTPM Apps */
            win->menu_context = CTX_GLTPM_APPS;
            win->selected_index = 0;
            win->option_count = 4;
            strcpy(win->menu_options[0], "demo_world.gltpm");
            strcpy(win->menu_options[1], "fuzz-op-gl.gltpm");
            strcpy(win->menu_options[2], "op-ed-gl.gltpm");
            strcpy(win->menu_options[3], "back");
            add_history(win, "Switching to GLTPM Apps...");
        } else if (index == gl_project_count + 1) {
            add_history(win, "Launching 3D Cube...");
            create_cube_window();
        } else if (index == gl_project_count + 2) {
            /* Back to Main Menu */
            win->menu_context = CTX_MAIN;
            win->selected_index = 0;
            win->option_count = 3;
            strcpy(win->menu_options[0], "Desktop Apps");
            strcpy(win->menu_options[1], "GLTPM Apps");
            strcpy(win->menu_options[2], "Terminal Apps");
            add_history(win, "Returned to Main Menu.");
        }
    }
 else if (win->menu_context == CTX_GLTPM_APPS) {
        if (index == 0) {
            add_history(win, "Launching demo_world.gltpm...");
            create_gltpm_window("gltpm-demo",
                                "projects/gltpm-demo/layouts/demo_world.gltpm",
                                "Demo World GLTPM");
        } else if (index == 1) {
            add_history(win, "Launching fuzz-op-gl.gltpm...");
            create_gltpm_window("fuzz-op-gl",
                                "projects/fuzz-op-gl/layouts/main.gltpm",
                                "Fuzz-Op-GL (Sovereign)");
        } else if (index == 2) {
            add_history(win, "Launching op-ed-gl.gltpm...");
            create_gltpm_window("op-ed-gl",
                                "projects/op-ed-gl/layouts/main.gltpm",
                                "GL-Op-Ed (God Mode)");
        } else if (index == 3) {
            win->menu_context = CTX_MAIN;
            win->selected_index = 0;
            win->option_count = 3;
            strcpy(win->menu_options[0], "Desktop Apps");
            strcpy(win->menu_options[1], "GLTPM Apps");
            strcpy(win->menu_options[2], "Terminal Apps");
            add_history(win, "Returned to Main Menu.");
        }
    }
    
    /* Clear input buffer */
    win->input_buffer[0] = '\0';

    /* Refresh frame */
    write_gl_os_state();
    parse_and_write_frame();
}

void move_selection(DesktopWindow* win, int direction) {
    if (!win->show_menu) return;
    
    win->selected_index += direction;
    if (win->selected_index < 0) win->selected_index = 0;
    if (win->selected_index >= win->option_count) win->selected_index = win->option_count - 1;
}

void create_mirrored_window(const char* project_id, const char* title) {
    if (window_count >= MAX_WINDOWS) return;
    
    int idx = window_count++;
    windows[idx].id = next_window_id++;
    windows[idx].x = 80 + (window_count * 20);
    windows[idx].y = 80 + (window_count * 20);
    windows[idx].w = 400;
    windows[idx].h = 400;
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    strncpy(windows[idx].title, title, MAX_WIN_TITLE-1);
    strncpy(windows[idx].type, "project_mirror", sizeof(windows[idx].type)-1);
    strncpy(windows[idx].project_id, project_id, sizeof(windows[idx].project_id)-1);
    
    active_window_id = windows[idx].id;
    
    /* Initialize Camera & Map Control state */
    windows[idx].is_map_control = 0;
    windows[idx].camera_mode = 4; /* Default: Free Camera */
    windows[idx].cam_pos[0] = 0.0f; windows[idx].cam_pos[1] = 0.0f; windows[idx].cam_pos[2] = 0.0f;
    windows[idx].cam_rot[0] = 30.0f; windows[idx].cam_rot[1] = 0.0f; windows[idx].cam_rot[2] = 0.0f;

    /* Initialize Project Menu (Fuzz-Op Parity) */
    windows[idx].show_menu = 1;
    windows[idx].selected_index = 0;
    if (strcmp(project_id, "fuzz-op-gl") == 0) {
        windows[idx].option_count = 9;
        strcpy(windows[idx].menu_options[0], "Control Map");
        strcpy(windows[idx].menu_options[1], "scan");
        strcpy(windows[idx].menu_options[2], "collect");
        strcpy(windows[idx].menu_options[3], "place");
        strcpy(windows[idx].menu_options[4], "inventory");
        strcpy(windows[idx].menu_options[5], "toggle_emoji");
        strcpy(windows[idx].menu_options[6], "toggle_clock");
        strcpy(windows[idx].menu_options[7], "Back to Selector");
        strcpy(windows[idx].menu_options[8], "Exit to Menu");
    } else if (strcmp(project_id, "aow-2d") == 0) {
        windows[idx].option_count = 5;
        strcpy(windows[idx].menu_options[0], "Control Map");
        strcpy(windows[idx].menu_options[1], "tax");
        strcpy(windows[idx].menu_options[2], "produce");
        strcpy(windows[idx].menu_options[3], "Back to Selector");
        strcpy(windows[idx].menu_options[4], "Exit to Menu");
        
        /* Force 2D Top-Down View */
        windows[idx].cam_rot[0] = -90.0f; 
        windows[idx].cam_pos[1] = 10.0f; 
    }

    log_master("WINDOW_CREATE", project_id);
    write_view();
    write_session_state();
    
    printf("[GL-OS] Launched mirrored window for %s (%s)\n", project_id, title);
}

void create_gltpm_window(const char* project_id, const char* layout_path, const char* title) {
    if (window_count >= MAX_WINDOWS) return;

    int idx = window_count++;
    windows[idx].id = next_window_id++;
    windows[idx].x = 100 + (window_count * 20);
    windows[idx].y = 90 + (window_count * 20);
    windows[idx].w = 420;
    windows[idx].h = 360;
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    windows[idx].managed_pid = -1;
    windows[idx].selected_index = 0;
    windows[idx].show_menu = 1;
    windows[idx].gltpm_frame_marker_size = -1;
    windows[idx].gltpm_layout_marker_size = -1;
    memset(&windows[idx].gltpm_scene, 0, sizeof(windows[idx].gltpm_scene));

    strncpy(windows[idx].title, title, MAX_WIN_TITLE - 1);
    strncpy(windows[idx].type, "gltpm_app", sizeof(windows[idx].type) - 1);
    strncpy(windows[idx].project_id, project_id, sizeof(windows[idx].project_id) - 1);
    strncpy(windows[idx].gltpm_layout_path, layout_path, sizeof(windows[idx].gltpm_layout_path) - 1);

    active_window_id = windows[idx].id;
    launch_gltpm_manager(&windows[idx]);
    refresh_gltpm_window(&windows[idx]);

    log_master("WINDOW_CREATE", project_id);
    write_view();
    write_session_state();
}

void create_terminal_window(void) {
    if (window_count >= MAX_WINDOWS) return;

    static int term_count = 0;
    term_count++;

    int idx = window_count++;
    windows[idx].id = next_window_id++;
    windows[idx].x = 50 + (term_count * 20);
    windows[idx].y = 50 + (term_count * 20);
    windows[idx].w = 400;
    windows[idx].h = 300;
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    windows[idx].input_buffer[0] = '\0';
    windows[idx].history_count = 0;
    snprintf(windows[idx].title, MAX_WIN_TITLE, "Terminal %d", term_count);
    strncpy(windows[idx].type, "terminal", sizeof(windows[idx].type)-1);

    /* Initialize menu navigation state (joystick-first UX) */
    init_terminal_menu(&windows[idx]);

    /* Add multiple menu options for joystick navigation testing */
    add_history(&windows[idx], "========================================");
    add_history(&windows[idx], "  SELECTABLE MENU OPTIONS (Test Nav)");
    add_history(&windows[idx], "========================================");
    add_history(&windows[idx], "  1. Desktop Apps (legacy)");
    add_history(&windows[idx], "  2. GLTPM Apps");
    add_history(&windows[idx], "  3. Terminal Apps");
    add_history(&windows[idx], "========================================");
    add_history(&windows[idx], "Use joystick UP/DOWN to navigate");
    add_history(&windows[idx], "Press ENTER to execute selected option");

    active_window_id = windows[idx].id;

    log_master("WINDOW_CREATE", "terminal");
    write_view();
    write_session_state();
}

void create_cube_window(void) {
    if (window_count >= MAX_WINDOWS) return;
    
    int idx = window_count++;
    windows[idx].id = next_window_id++;
    windows[idx].x = 100 + (window_count * 20);
    windows[idx].y = 100 + (window_count * 20);
    windows[idx].w = 300;
    windows[idx].h = 300;
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    windows[idx].rotation_x = 0;
    windows[idx].rotation_y = 0;
    snprintf(windows[idx].title, MAX_WIN_TITLE, "3D Cube Test");
    strncpy(windows[idx].type, "cube", sizeof(windows[idx].type)-1);
    
    active_window_id = windows[idx].id;
    log_master("WINDOW_CREATE", "cube");
    write_view();
    write_session_state();
}

void process_terminal_command(DesktopWindow* win) {
    char cmd[256];
    strncpy(cmd, win->input_buffer, 255);
    win->input_buffer[0] = '\0';
    
    char prompt[300];
    snprintf(prompt, sizeof(prompt), "$ %s", cmd);
    add_history(win, prompt);
    
    if (strcmp(cmd, "help") == 0) {
        add_history(win, "Available Categories:");
        add_history(win, "  []1.desktop  - Apps inside GL-OS");
        add_history(win, "  []2.terminal - CHTPM CLI Apps");
    } else if (strcmp(cmd, "1") == 0 || strcmp(cmd, "desktop") == 0) {
        add_history(win, "Desktop Apps:");
        add_history(win, "  - cube (Launch 3D Demo)");
        add_history(win, "  - folder (WIP)");
    } else if (strcmp(cmd, "2") == 0 || strcmp(cmd, "terminal") == 0) {
        add_history(win, "Terminal Apps:");
        add_history(win, "  - fuzzpet");
        add_history(win, "  - projects");
        add_history(win, "  - proc (Monitor)");
    } else if (strcmp(cmd, "cube") == 0) {
        add_history(win, "Launching 3D Cube App...");
        create_cube_window();
    } else if (strcmp(cmd, "clear") == 0) {
        win->history_count = 0;
    } else if (strlen(cmd) > 0) {
        add_history(win, "Unknown command. Type 'help' for options.");
    }
}

void close_window(int win_id) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == win_id) {
            cleanup_managed_window(&windows[i]);
            windows[i].active = 0;
            log_master("WINDOW_CLOSE", windows[i].title);
            
            if (active_window_id == win_id) {
                active_window_id = -1;
                /* Find next active window */
                for (int j = 0; j < window_count; j++) {
                    if (windows[j].active && !windows[j].minimized) {
                        active_window_id = windows[j].id;
                        break;
                    }
                }
            }
            
            write_view();
            write_session_state();
            return;
        }
    }
}

int find_window_at(int x, int y) {
    /* Search from top (last drawn) to bottom */
    for (int i = window_count - 1; i >= 0; i--) {
        if (!windows[i].active || windows[i].minimized) continue;
        /* y is GLUT (top-down), win->y is GLUT (top-down) */
        if (x >= windows[i].x && x <= windows[i].x + windows[i].w &&
            y >= windows[i].y && y <= windows[i].y + windows[i].h) {
            return i; /* Return index */
        }
    }
    return -1;
}

int is_in_titlebar(int x, int y, DesktopWindow* win) {
    int titlebar_h = 25;
    return (y >= win->y && y <= win->y + titlebar_h);
}

void draw_direction_cube(float size) {
    float s = size / 2.0f;
    glLineWidth(2.0f);
    glBegin(GL_QUADS);
    /* Front (Z+) - Blue */
    glColor3f(0.2f, 0.2f, 1.0f); glVertex3f(-s, -s,  s); glVertex3f( s, -s,  s); glVertex3f( s,  s,  s); glVertex3f(-s,  s,  s);
    /* Back (Z-) - Dark Blue */
    glColor3f(0, 0, 0.5f); glVertex3f(-s, -s, -s); glVertex3f(-s,  s, -s); glVertex3f( s,  s, -s); glVertex3f( s, -s, -s);
    /* Top (Y+) - Green */
    glColor3f(0.2f, 1.0f, 0.2f); glVertex3f(-s,  s, -s); glVertex3f(-s,  s,  s); glVertex3f( s,  s,  s); glVertex3f( s,  s, -s);
    /* Bottom (Y-) - Dark Green */
    glColor3f(0, 0.5f, 0); glVertex3f(-s, -s, -s); glVertex3f( s, -s, -s); glVertex3f( s, -s,  s); glVertex3f(-s, -s,  s);
    /* Right (X+) - Red */
    glColor3f(1.0f, 0.2f, 0.2f); glVertex3f( s, -s, -s); glVertex3f( s,  s, -s); glVertex3f( s,  s,  s); glVertex3f( s, -s,  s);
    /* Left (X-) - Dark Red */
    glColor3f(0.5f, 0, 0); glVertex3f(-s, -s, -s); glVertex3f(-s, -s,  s); glVertex3f(-s,  s,  s); glVertex3f(-s,  s, -s);
    glEnd();

    /* Draw RGB Axis Arrows */
    glBegin(GL_LINES);
    /* X - Red */
    glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(s*2, 0, 0);
    /* Y - Green */
    glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, s*2, 0);
    /* Z - Blue */
    glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, s*2);
    glEnd();
}

void draw_xelector(float size) {
    float s = size / 2.0f;
    glLineWidth(3.0f);
    
    /* Pulse effect based on time */
    float pulse = 0.7f + 0.3f * (float)sin(global_rotation * 0.2f);
    glColor3f(1.0f * pulse, 1.0f * pulse, 0); /* Yellow glow */
    
    glBegin(GL_LINES);
    /* Bottom */
    glVertex3f(-s, 0, -s); glVertex3f( s, 0, -s);
    glVertex3f( s, 0, -s); glVertex3f( s, 0,  s);
    glVertex3f( s, 0,  s); glVertex3f(-s, 0,  s);
    glVertex3f(-s, 0,  s); glVertex3f(-s, 0, -s);
    /* Top */
    glVertex3f(-s, s*2, -s); glVertex3f( s, s*2, -s);
    glVertex3f( s, s*2, -s); glVertex3f( s, s*2,  s);
    glVertex3f( s, s*2,  s); glVertex3f(-s, s*2,  s);
    glVertex3f(-s, s*2,  s); glVertex3f(-s, s*2, -s);
    /* Verticals */
    glVertex3f(-s, 0, -s); glVertex3f(-s, s*2, -s);
    glVertex3f( s, 0, -s); glVertex3f( s, s*2, -s);
    glVertex3f( s, 0,  s); glVertex3f( s, s*2,  s);
    glVertex3f(-s, 0,  s); glVertex3f(-s, s*2,  s);
    glEnd();
}

void draw_rect(float x, float y, float w, float h, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

/* Draw text clipped to max_width - adds "..." if too long */
void draw_text_clipped(float x, float y, float max_width, const char* text) {
    /* Measure text width (approximate: 9 pixels per char for 9x15 font) */
    int text_width = strlen(text) * 9;
    
    if (text_width <= max_width) {
        /* Text fits - draw normally */
        glRasterPos2f(x, y);
        for (const char* p = text; *p; p++)
            glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
    } else {
        /* Text too long - clip and add "..." */
        int max_chars = max_width / 9;
        if (max_chars > 3) {
            char clipped[256];
            strncpy(clipped, text, max_chars - 3);
            clipped[max_chars - 3] = '\0';
            strcat(clipped, "...");
            
            glRasterPos2f(x, y);
            for (const char* p = clipped; *p; p++)
                glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
        }
    }
}

/* Draw scroll thumb on right edge (Linux terminal behavior)
 * thumb at BOTTOM = viewing newest content (scroll_offset = 0)
 * thumb at TOP = viewing oldest content (scroll_offset = max_scroll)
 * y is the BOTTOM of the scroll track (OpenGL coords, y increases upward)
 * Stores track position for mouse click detection
 */
void draw_scroll_thumb(float x, float y, float h, int scroll_offset, int max_scroll, int visible_lines) {
    /* Store track position for mouse interaction */
    scroll_track_y = (int)y;
    scroll_track_h = (int)h;
    
    if (max_scroll <= 0) return;  /* No scroll needed */
    
    /* Thumb height proportional to visible/total (min 10%) */
    int total_lines = max_scroll + visible_lines;
    float thumb_h_ratio = (float)visible_lines / (float)total_lines;
    if (thumb_h_ratio < 0.1f) thumb_h_ratio = 0.1f;
    
    int thumb_h = (int)(h * thumb_h_ratio);
    if (thumb_h < 10) thumb_h = 10;
    
    /* Thumb position: 0.0 = bottom (newest), 1.0 = top (oldest) */
    float thumb_pos = (max_scroll > 0) ? ((float)scroll_offset / (float)max_scroll) : 0.0f;
    
    /* Y position: y is BOTTOM, so thumb at bottom when thumb_pos = 0 */
    int track_h = (int)h - thumb_h;
    int thumb_y = (int)(y + track_h * thumb_pos);  /* y + 0 = bottom, y + track_h = top */
    
    /* Draw thumb track (background) */
    glColor3f(0.3f, 0.3f, 0.3f);  /* Dark grey track */
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + 8, y);
    glVertex2f(x + 8, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    /* Draw thumb */
    if (scroll_offset == 0) {
        glColor3f(0.5f, 0.5f, 0.5f);  /* Grey - at bottom */
    } else {
        glColor3f(0.7f, 0.7f, 0.7f);  /* Lighter grey - scrolled up */
    }
    glBegin(GL_QUADS);
    glVertex2f(x + 1, thumb_y);
    glVertex2f(x + 7, thumb_y);
    glVertex2f(x + 7, thumb_y + thumb_h);
    glVertex2f(x + 1, thumb_y + thumb_h);
    glEnd();
}

void draw_cube(float size) {
    float s = size / 2.0f;
    glBegin(GL_QUADS);
    /* Front */
    glColor3f(1, 0, 0); glVertex3f(-s, -s,  s); glVertex3f( s, -s,  s); glVertex3f( s,  s,  s); glVertex3f(-s,  s,  s);
    /* Back */
    glColor3f(0, 1, 0); glVertex3f(-s, -s, -s); glVertex3f(-s,  s, -s); glVertex3f( s,  s, -s); glVertex3f( s, -s, -s);
    /* Top */
    glColor3f(0, 0, 1); glVertex3f(-s,  s, -s); glVertex3f(-s,  s,  s); glVertex3f( s,  s,  s); glVertex3f( s,  s, -s);
    /* Bottom */
    glColor3f(1, 1, 0); glVertex3f(-s, -s, -s); glVertex3f( s, -s, -s); glVertex3f( s, -s,  s); glVertex3f(-s, -s,  s);
    /* Right */
    glColor3f(1, 0, 1); glVertex3f( s, -s, -s); glVertex3f( s,  s, -s); glVertex3f( s,  s,  s); glVertex3f( s, -s,  s);
    /* Left */
    glColor3f(0, 1, 1); glVertex3f(-s, -s, -s); glVertex3f(-s, -s,  s); glVertex3f(-s,  s,  s); glVertex3f(-s,  s, -s);
    glEnd();
}

void draw_tile(float x, float y, float z, float size, float r, float g, float b) {
    float s = size / 2.0f;
    float h = size * (1.0f + z * 0.5f); /* Z-level extrusion */
    
    glBegin(GL_QUADS);
    /* Top - height depends on Z */
    glColor3f(r, g, b); 
    glVertex3f(-s,  h, -s); glVertex3f(-s,  h,  s); glVertex3f( s,  h,  s); glVertex3f( s,  h, -s);
    
    /* Sides - slightly darker for depth */
    glColor3f(r*0.8f, g*0.8f, b*0.8f);
    glVertex3f(-s, 0,  s); glVertex3f( s, 0,  s); glVertex3f( s,  h,  s); glVertex3f(-s,  h,  s);
    glVertex3f(-s, 0, -s); glVertex3f(-s,  h, -s); glVertex3f( s,  h, -s); glVertex3f( s, 0, -s);
    glVertex3f( s, 0, -s); glVertex3f( s,  h, -s); glVertex3f( s,  h,  s); glVertex3f( s, 0,  s);
    glVertex3f(-s, 0, -s); glVertex3f(-s, 0,  s); glVertex3f(-s,  h,  s); glVertex3f(-s,  h, -s);
    glEnd();
}

/* 
 * PIECE ARTIFACT: 8x8x8 Voxel Bitmasks 
 * Each array is [Z-level][Y-row] with 8 bits per row (X)
 */
unsigned char ARTIFACT_XEL[8][8] = {
    {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF}, // Bottom frame
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF}  // Top frame
};

void draw_artifact_piece(float x, float y, float z, float size, unsigned char mask[8][8], float r, float g, float b, int wireframe, float face_colors[6][3]) {
    float v_size = size / 8.0f;
    float offset = -size / 2.0f + v_size / 2.0f;
    float s = v_size / 2.0f;

    for (int iz = 0; iz < 8; iz++) {
        for (int iy = 0; iy < 8; iy++) {
            unsigned char row = mask[iz][iy];
            for (int ix = 0; ix < 8; ix++) {
                if (row & (1 << (7 - ix))) {
                    float vx = x + offset + (ix * v_size);
                    float vy = y + (iz * v_size);
                    float vz = z + offset + (iy * v_size);
                    
                    glPushMatrix();
                    glTranslatef(vx, vy, vz);
                    if (wireframe) {
                        glColor3f(r, g, b);
                        glutWireCube(v_size);
                    } else if (face_colors) {
                        glBegin(GL_QUADS);
                        /* Top (Y+) */ glColor3fv(face_colors[0]); glVertex3f(-s, s,-s); glVertex3f(-s, s, s); glVertex3f( s, s, s); glVertex3f( s, s,-s);
                        /* Bottom (Y-) */ glColor3fv(face_colors[1]); glVertex3f(-s,-s,-s); glVertex3f( s,-s,-s); glVertex3f( s,-s, s); glVertex3f(-s,-s, s);
                        /* Front (Z-) */ glColor3fv(face_colors[2]); glVertex3f(-s,-s,-s); glVertex3f(-s, s,-s); glVertex3f( s, s,-s); glVertex3f( s,-s,-s);
                        /* Back (Z+) */ glColor3fv(face_colors[3]); glVertex3f(-s,-s, s); glVertex3f( s,-s, s); glVertex3f( s, s, s); glVertex3f(-s, s, s);
                        /* Right (X+) */ glColor3fv(face_colors[4]); glVertex3f( s,-s,-s); glVertex3f( s, s,-s); glVertex3f( s, s, s); glVertex3f( s,-s, s);
                        /* Left (X-) */ glColor3fv(face_colors[5]); glVertex3f(-s,-s,-s); glVertex3f(-s,-s, s); glVertex3f(-s, s, s); glVertex3f(-s, s,-s);
                        glEnd();
                    } else {
                        draw_tile(0, 0, 0, v_size, r, g, b);
                    }
                    glPopMatrix();
                }
            }
        }
    }
}

void draw_window(DesktopWindow* win) {
    if (win->minimized) return;

    int x = win->x;
    int y = window_height - win->y - win->h; /* OpenGL y (bottom-up) */
    int w = win->w;
    int h = win->h;

    /* Window shadow */
    draw_rect(x + 3, y - 3, w, h, 0.0f, 0.0f, 0.0f);

    /* Window background */
    if (win->id == active_window_id) {
        draw_rect(x, y, w, h, 0.15f, 0.15f, 0.35f);
    } else {
        draw_rect(x, y, w, h, 0.1f, 0.1f, 0.25f);
    }

    /* Titlebar */
    if (win->id == active_window_id) {
        draw_rect(x, y + h - 25, w, 25, 0.0f, 0.4f, 0.8f);
    } else {
        draw_rect(x, y + h - 25, w, 25, 0.3f, 0.3f, 0.5f);
    }

    /* Title text (clipped to titlebar width) */
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text_clipped(x + 10, y + h - 18, w - 100, win->title);

    /* Buttons: Close(X), Minimize(_) */
    /* Close */
    draw_rect(x + w - 22, y + h - 22, 18, 18, 0.8f, 0.2f, 0.2f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x + w - 17, y + h - 18);
    glutBitmapCharacter(GLUT_BITMAP_9_BY_15, 'X');

    /* Minimize */
    draw_rect(x + w - 44, y + h - 22, 18, 18, 0.6f, 0.6f, 0.2f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x + w - 39, y + h - 18);
    glutBitmapCharacter(GLUT_BITMAP_9_BY_15, '_');

    /* Resize handle (bottom-right) - Linux style */
    draw_rect(x + w - 15, y, 15, 15, 0.6f, 0.6f, 0.8f);  /* Lighter blue - more visible */
    /* Draw diagonal lines on resize handle */
    glColor3f(0.8f, 0.8f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(x + w - 13, y + 3); glVertex2f(x + w - 3, y + 3);
    glVertex2f(x + w - 13, y + 3); glVertex2f(x + w - 13, y + 13);
    glVertex2f(x + w - 10, y + 6); glVertex2f(x + w - 3, y + 6);
    glVertex2f(x + w - 10, y + 6); glVertex2f(x + w - 10, y + 13);
    glEnd();
    
    /* Window content area */
    if (strcmp(win->type, "terminal") == 0) {
        /* innerHTML Pattern: Read frame from file and display */
        int content_top = y + h - 30;  /* Below titlebar */
        int content_bottom = y + 10;   /* Above window floor */
        int line_height = 18;
        int x_start = x + 10;
        
        /* Calculate scroll state for frame history */
        int history_area_h = content_top - content_bottom;
        int visible_lines = history_area_h / line_height;
        win->max_scroll = (win->history_count > visible_lines) ?
                          (win->history_count - visible_lines) : 0;
        
        /* Clamp scroll offset */
        if (win->scroll_offset > win->max_scroll) win->scroll_offset = win->max_scroll;
        if (win->scroll_offset < 0) win->scroll_offset = 0;
        
        /* innerHTML: Read frame from history file - show MOST RECENT frame */
        FILE *frame_file = fopen(gl_os_frame_history, "r");
        if (frame_file) {
            /* Find the LAST frame in the file (most recent) */
            char line[MAX_LINE];
            char last_frame[MAX_LINE * 20];  /* Buffer for last frame */
            int in_last_frame = 0;
            int last_frame_lines = 0;
            
            last_frame[0] = '\0';
            
            while (fgets(line, sizeof(line), frame_file)) {
                if (strstr(line, "--- FRAME UPDATE at")) {
                    /* Start of new frame - reset buffer */
                    in_last_frame = 1;
                    last_frame_lines = 0;
                    last_frame[0] = '\0';
                }
                else if (in_last_frame && last_frame_lines < 20) {
                    /* Add line to last frame buffer */
                    strcat(last_frame, line);
                    last_frame_lines++;
                }
            }
            fclose(frame_file);
            
            /* Now display the last frame */
            if (last_frame[0] != '\0') {
                printf("[FRAME DEBUG] Displaying last frame (%d lines)\n", last_frame_lines);
                
                /* Parse and display each line */
                char *frame_line = strtok(last_frame, "\n");
                int display_line = 0;
                while (frame_line && display_line < visible_lines) {
                    int frame_y = content_top - (display_line * line_height);
                    if (frame_y > content_bottom) {
                        glColor3f(0.8f, 0.8f, 0.8f);
                        glRasterPos2f(x_start, frame_y);
                        for (char* p = frame_line; *p; p++)
                            glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
                        display_line++;
                    }
                    frame_line = strtok(NULL, "\n");
                }
                printf("[FRAME DEBUG] Displayed %d lines\n", display_line);
            } else {
                printf("[FRAME DEBUG] No frame found in history!\n");
            }
        } else {
            printf("[FRAME DEBUG] Could not open frame history file!\n");
        }

        /* Direct code: Draw text selection highlight */
        if (win->is_selecting || win->selection_active) {
            printf("[DEBUG] Drawing selection highlight for terminal\n");
            
            /* Convert GLUT coordinates (top-down) to OpenGL (bottom-up) */
            int gl_y1 = window_height - win->select_start_y;
            int gl_y2 = window_height - win->select_end_y;
            int sel_x1 = win->select_start_x;
            int sel_x2 = win->select_end_x;
            int sel_y1 = (gl_y1 < gl_y2) ? gl_y1 : gl_y2;
            int sel_y2 = (gl_y1 > gl_y2) ? gl_y1 : gl_y2;
            
            /* Normalize X */
            if (sel_x1 > sel_x2) { int t = sel_x1; sel_x1 = sel_x2; sel_x2 = t; }
            
            printf("[DEBUG] Selection rect: (%d,%d) to (%d,%d)\n", sel_x1, sel_y1, sel_x2, sel_y2);
            
            /* Draw semi-transparent highlight */
            glColor4f(0.5f, 0.5f, 1.0f, 0.3f);
            glBegin(GL_QUADS);
            glVertex2f(sel_x1, sel_y1);
            glVertex2f(sel_x2, sel_y1);
            glVertex2f(sel_x2, sel_y2);
            glVertex2f(sel_x1, sel_y2);
            glEnd();
            
            /* Extract text from frame based on selection */
            /* TODO: Map pixel coordinates to character positions in frame */
            /* For now, just copy a placeholder */
            if (win->selection_active && strlen(win->selected_text) == 0) {
                strncpy(win->selected_text, "[Selected text extraction - TODO]", sizeof(win->selected_text) - 1);
            }
        }

        /* Direct code: Draw scroll thumb on right edge */
        draw_scroll_thumb(x + w - 12, content_bottom, content_top - content_bottom,
                         win->scroll_offset, win->max_scroll, visible_lines);
    } else if (strcmp(win->type, "cube") == 0) {
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(45.0, (double)w/(double)(h-25), 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(0, 0, -5);
        glRotatef(global_rotation, 1, 1, 0);
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h - 25);
        draw_cube(2.0f);
        glDisable(GL_SCISSOR_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);
    } else if (strcmp(win->type, "gltpm_app") == 0) {
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(45.0, (double)w/(double)(h-25), 0.1, 1000.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        /* Dynamic Camera Logic (WASD + Modes) - Parity with POC */
        float target_x = win->cam_pos[0];
        float target_y = win->cam_pos[1];
        float target_z = win->cam_pos[2];

        if (win->camera_mode == 1 || win->camera_mode == 3) {
            /* Follow Xelector automatically in 1st/3rd person modes */
            target_x = win->xelector_pos[0] * 1.2f;
            target_z = win->xelector_pos[2] * 1.2f;
        }

        if (win->camera_mode == 1) { /* 1st Person - Close/Low */
            glTranslatef(0, -0.5f, -1.0f);
        } else if (win->camera_mode == 3) { /* 3rd Person - Behind/High */
            glTranslatef(0, -3.0f, -10.0f);
            glRotatef(20, 1, 0, 0);
        } else { /* Free/Isometric */
            /* Standard fallback if mode isn't follow-centric */
            glTranslatef(0, -2.5f, -15.0f);
            glRotatef(win->cam_rot[0] + 55.0f, 1, 0, 0); /* Pitch + ISO base */
        }
        
        /* Apply Targeted Position & User Rotation */
        glTranslatef(-target_x, -target_y, -target_z);
        glRotatef(win->cam_rot[1] - 45.0f, 0, 1, 0); /* Yaw + ISO base */

        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h - 25);

        for (int i = 0; i < win->gltpm_scene.tile_count; i++) {
            GLTPMTile *tile = &win->gltpm_scene.tiles[i];
            glPushMatrix();
            glTranslatef(tile->x * 1.2f, 0.0f, tile->y * 1.2f);
            draw_tile(0, 0, tile->z + (tile->extrude - 1.0f), 1.0f,
                      tile->color[0], tile->color[1], tile->color[2]);
            glPopMatrix();
        }

        for (int i = 0; i < win->gltpm_scene.sprite_count; i++) {
            GLTPMSprite *sprite = &win->gltpm_scene.sprites[i];
            draw_artifact_piece(sprite->x * 1.2f, 1.0f + sprite->z, sprite->y * 1.2f, 0.8f, sprite->artifact_mask, 
                               sprite->color[0], sprite->color[1], sprite->color[2], 0, 
                               sprite->has_face_colors ? sprite->face_colors : NULL);
        }

        /* Draw Xelector (Artifact-XEL) - Sovereign Position */
        float sel_gl_x = win->xelector_pos[0] * 1.2f;
        float sel_gl_z = win->xelector_pos[1] * 1.2f;
        float sel_gl_y = 1.1f + (win->xelector_pos[2] * 0.5f); 
        struct timeval tv; gettimeofday(&tv, NULL);
        if ((tv.tv_usec / 200000) % 2 == 0) {
            draw_artifact_piece(sel_gl_x, sel_gl_y, sel_gl_z, 1.2f, ARTIFACT_XEL, 1.0f, 1.0f, 0.0f, 1, NULL);
        }

        glDisable(GL_SCISSOR_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);

        /* Overlay UI Elements (after disabling 3D context) */
        glColor3f(1, 1, 1);
        glRasterPos2f(x + 15, y + h - 42);
        for (char *p = win->gltpm_scene.title; *p; p++) {
            glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
        }
        
        char key_feedback[64];
        snprintf(key_feedback, sizeof(key_feedback), "[KEY]: %s", win->gltpm_scene.last_key);
        glColor3f(1.0f, 1.0f, 0.0f);
        glRasterPos2f(x + w - 120, y + h - 42);
        for (char *p = key_feedback; *p; p++) {
            glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
        }

        /* 1. Coordinate HUD (Bottom) */
        char coord_hud[256];
        snprintf(coord_hud, sizeof(coord_hud), "CAM: (%.1f, %.1f, %.1f) | SEL: (%d, %d, %d)", 
                 win->cam_pos[0], win->cam_pos[1], win->cam_pos[2],
                 win->xelector_pos[0], win->xelector_pos[1], win->xelector_pos[2]);
        glColor3f(0.0f, 1.0f, 1.0f); /* Cyan HUD */
        glRasterPos2f(x + 15, y + 35);
        for (char *p = coord_hud; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);

        /* 2. 2D Minimap (Bottom-Right) */
        int mm_size = 80;
        int mm_x = x + w - mm_size - 15;
        int mm_y = y + 55;
        draw_rect(mm_x - 2, mm_y - 2, mm_size + 4, mm_size + 4, 0.2f, 0.2f, 0.2f); /* Border */
        draw_rect(mm_x, mm_y, mm_size, mm_size, 0.05f, 0.05f, 0.1f); /* BG */
        
        /* Draw static tiles on minimap */
        float cell_size = (float)mm_size / 20.0f; /* Assuming 20x20 grid center */
        for (int i = 0; i < win->gltpm_scene.tile_count; i++) {
            GLTPMTile *tile = &win->gltpm_scene.tiles[i];
            int tx = mm_x + (mm_size/2) + (int)(tile->x * cell_size);
            int ty = mm_y + (mm_size/2) + (int)(tile->y * cell_size);
            if (tx >= mm_x && tx < mm_x + mm_size && ty >= mm_y && ty < mm_y + mm_size) {
                draw_rect(tx, ty, (int)cell_size, (int)cell_size, tile->color[0], tile->color[1], tile->color[2]);
            }
        }

        /* Draw Xelector on minimap */
        int sel_mm_x = mm_x + (mm_size/2) + (int)(win->xelector_pos[0] * cell_size);
        int sel_mm_y = mm_y + (mm_size/2) + (int)(win->xelector_pos[2] * cell_size);
        draw_rect(sel_mm_x, sel_mm_y, (int)cell_size, (int)cell_size, 1.0f, 1.0f, 0.0f);

        /* Draw View Cone (Camera) on minimap */
        int cam_mm_x = mm_x + (mm_size/2) + (int)(win->cam_pos[0] / 1.2f * cell_size);
        int cam_mm_y = mm_y + (mm_size/2) + (int)(win->cam_pos[2] / 1.2f * cell_size);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
        /* Simple view cone lines based on Yaw */
        float yaw_rad = (win->cam_rot[1] - 45.0f) * 3.14159f / 180.0f;
        glVertex2f(cam_mm_x, cam_mm_y);
        glVertex2f(cam_mm_x + cos(yaw_rad - 0.5f) * 10, cam_mm_y + sin(yaw_rad - 0.5f) * 10);
        glVertex2f(cam_mm_x, cam_mm_y);
        glVertex2f(cam_mm_x + cos(yaw_rad + 0.5f) * 10, cam_mm_y + sin(yaw_rad + 0.5f) * 10);
        glEnd();

        for (int i = 0; i < win->gltpm_scene.text_count; i++) {
            int line_y = y + h - 62 - (i * 18);
            if (line_y < y + 45) break;
            glColor3f(0.85f, 0.85f, 0.9f);
            glRasterPos2f(x + 15, line_y);
            for (char *p = win->gltpm_scene.texts[i].label; *p; p++) {
                glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
            }
        }

        for (int i = 0; i < win->gltpm_scene.button_count; i++) {
            /* Draw from top to bottom. y is bottom-up, so start high and subtract. */
            int button_y = y + h - 110 - (i * 24);
            if (button_y < y + 42) break;

            const char *indicator = (i == win->selected_index) ? (win->is_map_control ? "^" : ">") : " ";
            char btn_label[256];
            /* Standard CHTPM Nav Index Pattern: [>] 1. [Label] */
            snprintf(btn_label, sizeof(btn_label), "[%s] %d. [%s]", indicator, i + 1, win->gltpm_scene.buttons[i].label);

            float r = (i == win->selected_index) ? 0.72f : 0.22f;
            float g = (i == win->selected_index) ? 0.58f : 0.24f;
            float b = (i == win->selected_index) ? 0.18f : 0.36f;
            
            draw_rect(x + 12, button_y, w - 24, 20, r, g, b);
            glColor3f(1, 1, 1);
            glRasterPos2f(x + 18, button_y + 6);
            for (char *p = btn_label; *p; p++) {
                glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
            }
        }

        /* Nav Command Prompt */
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "Nav > %s_", win->nav_buffer);
        glColor3f(0.8f, 1.0f, 0.8f);
        glRasterPos2f(x + 15, y + 15);
        for (char *p = prompt; *p; p++) {
            glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
        }
    } else if (strcmp(win->type, "project_mirror") == 0) {
        /* Project-Specific 3D Rendering + Menu Overlay */
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(45.0, (double)w/(double)(h-25), 0.1, 1000.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        /* Dynamic Camera Logic (WASD + Modes) */
        float target_x = win->cam_pos[0];
        float target_y = win->cam_pos[1];
        float target_z = win->cam_pos[2];

        if (win->camera_mode == 1 || win->camera_mode == 3) {
            /* Follow Xelector automatically in 1st/3rd person modes */
            target_x = win->xelector_pos[0] * 1.2f;
            target_z = win->xelector_pos[2] * 1.2f;
        }

        if (strcmp(win->project_id, "aow-2d") == 0) {
            /* Force Top-Down for aow-2d */
            glTranslatef(0, -2.5f, -15.0f);
            glRotatef(-90.0f, 1, 0, 0);
        } else if (win->camera_mode == 1) { /* 1st Person - Close/Low */
            glTranslatef(0, -0.5f, -1.0f);
        } else if (win->camera_mode == 3) { /* 3rd Person - Behind/High */
            glTranslatef(0, -3.0f, -10.0f);
            glRotatef(20, 1, 0, 0);
        } else { /* Free/Isometric */
            glTranslatef(0, -2.5f, -15.0f);
            glRotatef(win->cam_rot[0], 1, 0, 0); /* Pitch */
        }
        
        /* Apply Targeted Position & User Rotation */
        glTranslatef(-target_x, -target_y, -target_z);
        glRotatef(win->cam_rot[1], 0, 1, 0); /* Yaw (Q/E) */
        
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h - 25);
        
        /* Render 10x10 Mock Grid with Z-Extrusion (K4) */
        for (int row = -5; row <= 5; row++) {
            for (int col = -5; col <= 5; col++) {
                float dist = (float)(abs(row) + abs(col));
                float z_level = (float)((5 - (int)dist) % 4); /* Mock wave pattern */
                if (z_level < 0) z_level = 0;
                
                glPushMatrix();
                glTranslatef(col * 1.2f, 0, row * 1.2f);
                draw_tile(0, 0, z_level, 1.0f, 0.2f, 0.5f, 0.8f); /* Blueish tiles */
                glPopMatrix();
            }
        }

        /* Render Mock Entities (Fuzzball, Zombie, Tree) */
        /* Fuzzball (Player) */
        glPushMatrix();
        glTranslatef(0 * 1.2f, 1.0f, 0 * 1.2f);
        draw_tile(0, 0, 0, 0.8f, 1.0f, 1.0f, 1.0f); /* White cube */
        glPopMatrix();

        /* Zombie */
        glPushMatrix();
        glTranslatef(2 * 1.2f, 1.0f, -3 * 1.2f);
        draw_tile(0, 0, 0, 0.8f, 1.0f, 0.2f, 0.2f); /* Red cube */
        glPopMatrix();

        /* Tree */
        glPushMatrix();
        glTranslatef(-4 * 1.2f, 1.0f, 2 * 1.2f);
        draw_tile(0, 0, 1.0f, 0.8f, 0.2f, 0.8f, 0.2f); /* Green cube */
        glPopMatrix();

        /* Draw Xelector (Grid Cursor) */
        glPushMatrix();
        /* Map Grid Y to OpenGL Depth (Z) and Grid Z to OpenGL Elevation (Y) */
        float sel_gl_x = win->xelector_pos[0] * 1.2f;
        float sel_gl_y = win->xelector_pos[2] * 0.5f; /* Elevation */
        float sel_gl_z = win->xelector_pos[1] * 1.2f; /* Depth */
        glTranslatef(sel_gl_x, sel_gl_y, sel_gl_z);
        
        /* Add Blinking Logic (KISS) */
        if ((time(NULL) % 2) == 0) { /* Toggle visibility every second */
            draw_xelector(1.2f);
        }
        glPopMatrix();
        
        /* Draw Direction Cube in Corner */
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(2.5f, 1.8f, -6.0f); /* Corner of view */
        glRotatef(win->cam_rot[0], 1, 0, 0);
        glRotatef(win->cam_rot[1], 0, 1, 0);
        draw_direction_cube(0.5f);
        glPopMatrix();
        
        glDisable(GL_SCISSOR_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);
        
        /* Overlay Index-Based Menu (K3/Navigation Parity) */
        int content_top = y + h - 45;
        int x_start = x + 15;
        
        /* Status Bar (Map Control Hint) */
        if (win->is_map_control) {
            draw_rect(x, y + h - 45, w, 20, 0.8f, 0.1f, 0.1f);
            glColor3f(1, 1, 1);
            glRasterPos2f(x_start, y + h - 40);
            const char* hint = "[ MAP CONTROL ACTIVE ] - ESC TO EXIT";
            for (const char* p = hint; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
            content_top -= 25;
        }

        glColor3f(1, 1, 1);
        glRasterPos2f(x_start, content_top);
        const char* header = "=== PROJECT MENU ===";
        for (const char* p = header; *p; p++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
        
        /* Draw Menu Options */
        for (int i = 0; i < win->option_count; i++) {
            int line_y = content_top - 25 - (i * 18);
            if (line_y < y + 10) break;

            char line[128];
            if (i == win->selected_index) {
                glColor3f(1, 1, 0); /* Yellow for selection */
                glRasterPos2f(x_start, line_y);
                const char* indicator = "[>] ";
                for (const char* p = indicator; *p; p++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
                
                snprintf(line, sizeof(line), "%d. [%s]", i + 1, win->menu_options[i]);
                for (const char* p = line; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
            } else {
                glColor3f(0.7f, 0.7f, 0.7f);
                glRasterPos2f(x_start, line_y);
                const char* indicator = "[ ] ";
                for (const char* p = indicator; *p; p++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
                
                snprintf(line, sizeof(line), "%d. [%s]", i + 1, win->menu_options[i]);
                for (const char* p = line; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
            }
        }
    }
}

void draw_toolbar(void) {
    draw_rect(0, 0, window_width, toolbar_height, 0.1f, 0.1f, 0.2f);
    glColor3f(0.3f, 0.3f, 0.5f);
    glBegin(GL_LINES);
    glVertex2f(0, toolbar_height);
    glVertex2f(window_width, toolbar_height);
    glEnd();
    
    char info[128];
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char timestr[16];
    strftime(timestr, sizeof(timestr), "%H:%M:%S", timeinfo);
    snprintf(info, sizeof(info), "User: guest | Time: %s", timestr);
    
    glColor3f(0.8f, 0.8f, 1.0f);
    glRasterPos2f(10, 10);
    for (char* p = info; *p; p++) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
    }
    
    int x_off = 250;
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        if (windows[i].minimized) {
            draw_rect(x_off, 4, 120, 22, 0.2f, 0.3f, 0.5f);
        } else if (windows[i].id == active_window_id) {
            draw_rect(x_off, 4, 120, 22, 0.3f, 0.4f, 0.7f);
        } else {
            draw_rect(x_off, 4, 120, 22, 0.15f, 0.2f, 0.35f);
        }
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(x_off + 5, 10);
        char short_title[15];
        strncpy(short_title, windows[i].title, 12);
        short_title[12] = '\0';
        for (char* p = short_title; *p; p++) {
            glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
        }
        x_off += 130;
    }
}

void draw_context_menu(void) {
    if (!context_menu_visible) return;
    int x = context_menu_x;
    int y = window_height - context_menu_y - context_menu_height;
    int w = context_menu_width;
    int h = context_menu_height;
    draw_rect(x, y, w, h, 0.2f, 0.2f, 0.3f);
    glColor3f(0.5f, 0.5f, 0.7f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    
    /* Different menu items based on context */
    char* items[4];
    int item_count = 0;
    
    if (context_menu_type == 1) {
        /* Terminal context menu - Copy/Paste */
        items[0] = "[1] Copy";
        items[1] = "[2] Paste";
        items[2] = "[3] Clear";
        items[3] = "[ESC] Cancel";
        item_count = 4;
    } else {
        /* Desktop context menu */
        items[0] = "[1] New Terminal";
        items[1] = "[2] 3D Cube App";
        items[2] = "[3] New File";
        items[3] = "[ESC] Cancel";
        item_count = 4;
    }
    
    for (int i = 0; i < item_count; i++) {
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(x + 10, y + h - 25 - (i * 20));
        for (char* p = items[i]; *p; p++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
    }
}

void render_scene(void) {
    glClearColor(bg_color[0], bg_color[1], bg_color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    for (int i = 0; i < window_count; i++) {
        if (windows[i].active) draw_window(&windows[i]);
    }
    draw_toolbar();
    draw_context_menu();
    glutSwapBuffers();
}

void timer(int value) {
    (void)value;
    global_rotation += 2.0f;
    if (global_rotation > 360.0f) global_rotation -= 360.0f;

    cursor_blink++;
    if (cursor_blink >= 20) cursor_blink = 0;

    /* Force GLUT to poll joystick (required for continuous polling) */
    glutForceJoystickFunc();

    /* Focus and Idle-aware throttling */
    int next_timer = 30;  /* Default: 30ms (~33 FPS) */
    
    if (!is_active_layout()) {
        next_timer = 100; /* Throttled: 100ms (10 FPS) when not in focus */
    } else {
        /* Focused but potentially idle */
        time_t now = time(NULL);
        if (now - last_interaction_time > 5) {
            next_timer = 100; /* Idle backoff: 100ms after 5s of inactivity */
        }
    }

    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active || strcmp(windows[i].type, "gltpm_app") != 0) continue;

        if (windows[i].managed_pid > 0) {
            int status = 0;
            pid_t result = waitpid(windows[i].managed_pid, &status, WNOHANG);
            if (result == windows[i].managed_pid) {
                windows[i].managed_pid = -1;
            }
        }

        refresh_gltpm_window(&windows[i]);
    }

    glutPostRedisplay();
    glutTimerFunc(next_timer, timer, 0);
}

void joystick(unsigned int buttonMask, int x, int y, int z) {
    (void)z;
    last_interaction_time = time(NULL);

    /* Joystick navigation for menu (joystick-first UX) */
    if (active_window_id != -1) {
        int idx = -1;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == active_window_id) {
                idx = i;
                break;
            }
        }

        if (idx != -1) {
            DesktopWindow* win = &windows[idx];
            int is_navigable = (strcmp(win->type, "terminal") == 0) || 
                               (strcmp(win->type, "project_mirror") == 0 && !win->is_map_control) ||
                               (strcmp(win->type, "gltpm_app") == 0);

            if (is_navigable) {
                int selection_changed = 0;
                
                if (win->show_menu && win->option_count > 0) {
                    /* Menu navigation mode */
                    if (y < -500 && last_joy_y >= -500) {
                        move_selection(win, -1);  /* Up: up */
                        selection_changed = 1;
                    } else if (y > 500 && last_joy_y <= 500) {
                        move_selection(win, 1);   /* Down: down */
                        selection_changed = 1;
                    }

                    /* Button A: execute - edge-triggered */
                    if ((buttonMask & GLUT_JOYSTICK_BUTTON_A) &&
                        !(last_joy_buttons & GLUT_JOYSTICK_BUTTON_A)) {
                        execute_option(win, win->selected_index);
                    }
                } else if (strcmp(win->type, "terminal") == 0) {
                    /* Scroll mode (Terminal only) */
                    if (y < -500 && last_joy_y >= -500) {
                        win->scroll_offset++;
                        if (win->scroll_offset > win->max_scroll)
                            win->scroll_offset = win->max_scroll;
                        selection_changed = 1;
                    } else if (y > 500 && last_joy_y <= 500) {
                        win->scroll_offset--;
                        if (win->scroll_offset < 0)
                            win->scroll_offset = 0;
                        selection_changed = 1;
                    }
                }
                
                if (selection_changed) {
                    write_gl_os_frame();
                }
            }
        }
    }

    /* Update last state globally to ensure edge-triggering works correctly */
    last_joy_x = x;
    last_joy_y = y;
    last_joy_buttons = buttonMask;

    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    last_interaction_time = time(NULL);

    /* Check focused window type */
    int active_idx = -1;
    if (active_window_id != -1) {
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == active_window_id) {
                active_idx = i;
                break;
            }
        }
    }

    /* ESC behavior */
    if (key == 27) {
        if (context_menu_visible) {
            context_menu_visible = 0;
            glutPostRedisplay();
            return;
        } else if (active_idx != -1 && windows[active_idx].is_map_control) {
            windows[active_idx].is_map_control = 0;
            add_history(&windows[active_idx], "Returned to Menu Navigation.");
            
            /* Also inject ESC to project so manager knows */
            DesktopWindow* win = &windows[active_idx];
            const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";
            char *history_path = NULL;
            if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
                FILE *hf = fopen(history_path, "a");
                if (hf) {
                    time_t rawtime; struct tm *timeinfo; char timestamp[64];
                    time(&rawtime); timeinfo = localtime(&rawtime);
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                    fprintf(hf, "[%s] KEY_PRESSED: 27\n", timestamp);
                    fclose(hf);
                }
                free(history_path);
            }
            write_gl_os_frame();
            glutPostRedisplay();
            return;
        }
    }

    /* Priority Routing for gltpm_app (A23) */
    if (active_idx != -1 && strcmp(windows[active_idx].type, "gltpm_app") == 0) {
        DesktopWindow* win = &windows[active_idx];
        int in_map = win->is_map_control;

        if (in_map) {
            float speed = 0.5f;
            int moved = 0;
            int k = tolower(key);

            /* Local Camera Fly (Authority: Host for smooth movement) */
            if (k == '1') { win->camera_mode = 1; moved = 1; }
            else if (k == '2') { win->camera_mode = 2; moved = 1; }
            else if (k == '3') { win->camera_mode = 3; moved = 1; }
            else if (k == '4') { win->camera_mode = 4; moved = 1; }
            else if (k == 'w') { win->cam_pos[2] += speed; moved = 1; }
            else if (k == 's') { win->cam_pos[2] -= speed; moved = 1; }
            else if (k == 'a') { win->cam_pos[0] -= speed; moved = 1; }
            else if (k == 'd') { win->cam_pos[0] += speed; moved = 1; }
            else if (k == 'z') { win->cam_pos[1] += speed; moved = 1; }
            else if (k == 'x') { win->cam_pos[1] -= speed; moved = 1; }
            else if (k == 'q') { win->cam_rot[1] -= 5.0f; moved = 1; }
            else if (k == 'e') { win->cam_rot[1] += 5.0f; moved = 1; }

            char *history_path = NULL;
            const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";
            
            if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
                FILE *hf = fopen(history_path, "a");
                if (hf) {
                    time_t rawtime; struct tm *timeinfo; char timestamp[64];
                    time(&rawtime); timeinfo = localtime(&rawtime);
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                    
                    if (moved) {
                        /* SYNC: Inform manager of new camera state so it doesn't overwrite it */
                        fprintf(hf, "[%s] COMMAND: CAMERA_SET:%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", 
                                timestamp, win->cam_pos[0], win->cam_pos[1], win->cam_pos[2],
                                win->cam_rot[0], win->cam_rot[1], win->cam_rot[2]);
                        fprintf(hf, "[%s] COMMAND: CAMERA_MODE:%d\n", timestamp, win->camera_mode);
                    } else {
                        fprintf(hf, "[%s] KEY_PRESSED: %d\n", timestamp, (int)tolower(key));
                    }
                    fclose(hf);
                }
                free(history_path);
            }
            glutPostRedisplay();
            return; /* YIELD */
        } else {
            /* Menu Mode: Nav > behavior */
            if (key >= '0' && key <= '9') {
                int len = strlen(win->nav_buffer);
                if (len < sizeof(win->nav_buffer) - 1) {
                    win->nav_buffer[len] = key;
                    win->nav_buffer[len+1] = '\0';
                    int idx = atoi(win->nav_buffer) - 1;
                    if (idx >= 0 && idx < win->gltpm_scene.button_count) win->selected_index = idx;
                }
            } else if (key == 8 || key == 127) {
                int len = strlen(win->nav_buffer);
                if (len > 0) win->nav_buffer[len-1] = '\0';
            } else if (key == 13 || key == 10) {
                if (strlen(win->nav_buffer) > 0) {
                    int idx = atoi(win->nav_buffer) - 1;
                    if (idx >= 0 && idx < win->gltpm_scene.button_count) {
                        win->selected_index = idx;
                        dispatch_gltpm_button(win, idx);
                    }
                    win->nav_buffer[0] = '\0';
                } else {
                    dispatch_gltpm_button(win, win->selected_index);
                }
            }
            glutPostRedisplay();
            return;
        }
    }

    /* Project Mirror Controls (Hardcoded POC) */
    if (active_idx != -1 && strcmp(windows[active_idx].type, "project_mirror") == 0) {
        DesktopWindow* win = &windows[active_idx];
        if (win->is_map_control) {
            float speed = 0.2f;
            if (key == '1') win->camera_mode = 1;
            else if (key == '2') win->camera_mode = 2;
            else if (key == '3') win->camera_mode = 3;
            else if (key == '4') win->camera_mode = 4;
            else if (key == 'w' || key == 'W') win->cam_pos[2] += speed;
            else if (key == 's' || key == 'S') win->cam_pos[2] -= speed;
            else if (key == 'a' || key == 'A') win->cam_pos[0] -= speed;
            else if (key == 'd' || key == 'D') win->cam_pos[0] += speed;
            else if (key == 'z' || key == 'Z') win->cam_pos[1] += speed;
            else if (key == 'x' || key == 'X') win->cam_pos[1] -= speed;
            else if (key == 'q' || key == 'Q') win->cam_rot[1] -= 5.0f;
            else if (key == 'e' || key == 'E') win->cam_rot[1] += 5.0f;
            glutPostRedisplay();
            return;
        } else {
            if (key >= '1' && key <= '9') {
                int idx = key - '1';
                if (idx >= 0 && idx < win->option_count) win->selected_index = idx;
            } else if (key == 13) execute_option(win, win->selected_index);
            glutPostRedisplay();
            return;
        }
    }

    /* Terminal/Standard Controls */
    int terminal_focused = (active_idx != -1 && strcmp(windows[active_idx].type, "terminal") == 0);
    if (!terminal_focused) {
        if (key == '1') { create_terminal_window(); context_menu_visible = 0; }
        else if (key == '2') { create_cube_window(); context_menu_visible = 0; }
        else if (key == 'c' || key == 'C') { if (active_window_id != -1) close_window(active_window_id); }
    }

    if (terminal_focused) {
        DesktopWindow* win = &windows[active_idx];
        int len = strlen(win->input_buffer);
        if (win->show_menu && key >= '1' && key <= '9') {
            int idx = key - '1';
            if (idx >= 0 && idx < win->option_count) win->selected_index = idx;
        }
        if (key == 13) {
            if (win->show_menu && win->option_count > 0) execute_option(win, win->selected_index);
            else process_terminal_command(win);
        } else if (key == 8 || key == 127) {
            if (len > 0) win->input_buffer[len-1] = '\0';
        } else if (len < 250 && key >= 32 && key <= 126) {
            win->input_buffer[len] = key;
            win->input_buffer[len+1] = '\0';
        }
    }

    FILE *hf = fopen(session_history_path, "a");
    if (hf) {
        time_t rawtime; struct tm *timeinfo; char timestamp[100];
        time(&rawtime); timeinfo = localtime(&rawtime);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        fprintf(hf, "[%s] KEY_PRESSED: %d\n", timestamp, key); fclose(hf);
    }
    write_session_state();
    glutPostRedisplay();
}
void special_keyboard(int key, int x, int y) {
    (void)x; (void)y;
    last_interaction_time = time(NULL);
    
    if (active_window_id != -1) {
        int idx = -1;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == active_window_id) {
                idx = i;
                break;
            }
        }
        
        if (idx != -1) {
            DesktopWindow* win = &windows[idx];
            
            if (strcmp(win->type, "gltpm_app") == 0) {
                if (win->is_map_control) {
                    /* Map Mode: Route arrows to project history */
                    char *history_path = NULL;
                    const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";

                    if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
                        FILE *hf = fopen(history_path, "a");
                        if (hf) {
                            time_t rawtime; struct tm *timeinfo; char timestamp[64];
                            time(&rawtime); timeinfo = localtime(&rawtime);
                            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                            int tpm_key = 0;
                            if (key == GLUT_KEY_LEFT) tpm_key = 1000;
                            else if (key == GLUT_KEY_RIGHT) tpm_key = 1001;
                            else if (key == GLUT_KEY_UP) tpm_key = 1002;
                            else if (key == GLUT_KEY_DOWN) tpm_key = 1003;
                            if (tpm_key > 0) fprintf(hf, "[%s] KEY_PRESSED: %d\n", timestamp, tpm_key);
                            fclose(hf);
                        }
                        free(history_path);
                    }
                    glutPostRedisplay();
                    return; /* YIELD */
                } else {
                    /* Menu Mode: Standard Nav arrows */
                    if (key == GLUT_KEY_UP) move_selection(win, -1);
                    else if (key == GLUT_KEY_DOWN) move_selection(win, 1);
                    win->nav_buffer[0] = '\0'; /* Clear jump buffer on arrow move */
                    glutPostRedisplay();
                    return; /* YIELD: Don't fall through to generic nav */
                }
            }
            else if (strcmp(win->type, "project_mirror") == 0 && win->is_map_control) {
                /* Map Control: Arrow keys move Xelector only */
                if (key == GLUT_KEY_UP) { win->xelector_pos[2]--; }
                else if (key == GLUT_KEY_DOWN) { win->xelector_pos[2]++; }
                else if (key == GLUT_KEY_LEFT) { win->xelector_pos[0]--; }
                else if (key == GLUT_KEY_RIGHT) { win->xelector_pos[0]++; }
                glutPostRedisplay();
            } else {
                /* Menu Navigation */
                int is_navigable = (strcmp(win->type, "terminal") == 0) || 
                                   (strcmp(win->type, "project_mirror") == 0 && !win->is_map_control) ||
                                   (strcmp(win->type, "gltpm_app") == 0);
                
                if (is_navigable) {
                    if (key == GLUT_KEY_UP) {
                        move_selection(win, -1);
                        write_gl_os_frame();
                        glutPostRedisplay();
                    } else if (key == GLUT_KEY_DOWN) {
                        move_selection(win, 1);
                        write_gl_os_frame();
                        glutPostRedisplay();
                    }
                }
            }
        }
    }
}

void mouse(int button, int state, int x, int y) {
    mouse_x = x; mouse_y = y;
    last_interaction_time = time(NULL);
    
    printf("[MOUSE DEBUG] button=%d state=%d pos=(%d,%d)\n", button, state, x, y);
    
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        if (y > window_height - toolbar_height) return;
        
        /* Check if right-clicking on a terminal window */
        context_menu_type = 0;  /* Default: desktop menu */
        int idx = find_window_at(x, y);
        if (idx != -1 && strcmp(windows[idx].type, "terminal") == 0) {
            context_menu_type = 1;  /* Terminal menu (Copy/Paste) */
        }
        
        context_menu_visible = 1; context_menu_x = x; context_menu_y = y;
        if (context_menu_x + context_menu_width > window_width) context_menu_x = window_width - context_menu_width;
        if (context_menu_y + context_menu_height > window_height) context_menu_y = window_height - context_menu_height;
        log_master("CONTEXT_MENU", (context_menu_type == 1) ? "terminal" : "desktop");
        glutPostRedisplay();
        return;
    }
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (y > window_height - toolbar_height) {
            int x_off = 250;
            for (int i = 0; i < window_count; i++) {
                if (!windows[i].active) continue;
                if (x >= x_off && x <= x_off + 120) {
                    windows[i].minimized = !windows[i].minimized;
                    if (!windows[i].minimized) {
                        active_window_id = windows[i].id;
                        DesktopWindow win = windows[i];
                        for (int j = i; j < window_count - 1; j++) windows[j] = windows[j+1];
                        windows[window_count-1] = win;
                    } else active_window_id = -1;
                    write_session_state(); glutPostRedisplay(); return;
                }
                x_off += 130;
            }
            return;
        }
        if (context_menu_visible) {
            if (x >= context_menu_x && x <= context_menu_x + context_menu_width && y >= context_menu_y && y <= context_menu_y + context_menu_height) {
                int item = (y - context_menu_y) / 25;
                
                if (context_menu_type == 1) {
                    /* Terminal context menu - Copy/Paste/Clear */
                    if (item == 0) {
                        /* Copy - copy ENTIRE current frame to clipboard.txt */
                        printf("[COPY] Copy menu selected - copying entire frame\n");
                        
                        /* Read current frame and write to clipboard.txt */
                        FILE *frame_file = fopen("pieces/apps/gl_os/session/current_frame.txt", "r");
                        FILE *cb = fopen("pieces/apps/gl_os/session/clipboard.txt", "w");
                        
                        if (frame_file && cb) {
                            char line[MAX_LINE];
                            printf("[COPY] Copying frame to clipboard.txt:\n");
                            while (fgets(line, sizeof(line), frame_file)) {
                                fprintf(cb, "%s", line);
                                printf("%s", line);  /* Also print to console for debug */
                            }
                            fclose(cb);
                            printf("[COPY] ✓ Frame copied to clipboard.txt\n");
                        } else {
                            printf("[COPY] ERROR: Could not open files\n");
                            if (frame_file) fclose(frame_file);
                            if (cb) fclose(cb);
                        }
                    }
                    else if (item == 1) {
                        /* Paste - paste from clipboard to input buffer */
                        if (active_window_id != -1) {
                            for (int i = 0; i < window_count; i++) {
                                if (windows[i].id == active_window_id && strcmp(windows[i].type, "terminal") == 0) {
                                    paste_from_clipboard(windows[i].input_buffer, sizeof(windows[i].input_buffer));
                                    break;
                                }
                            }
                        }
                    }
                    else if (item == 2) {
                        /* Clear - clear terminal history */
                        if (active_window_id != -1) {
                            for (int i = 0; i < window_count; i++) {
                                if (windows[i].id == active_window_id && strcmp(windows[i].type, "terminal") == 0) {
                                    windows[i].history_count = 0;
                                    windows[i].input_buffer[0] = '\0';
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    /* Desktop context menu */
                    if (item == 0) create_terminal_window();
                    else if (item == 1) create_cube_window();
                }
                context_menu_visible = 0; glutPostRedisplay(); return;
            }
            context_menu_visible = 0;
        }
        int idx = find_window_at(x, y);
        printf("[MOUSE DEBUG] find_window_at returned idx=%d\n", idx);
        
        if (idx != -1) {
            printf("[MOUSE DEBUG] Window %d type='%s' pos=(%d,%d) size=(%d,%d)\n",
                   windows[idx].id, windows[idx].type,
                   windows[idx].x, windows[idx].y,
                   windows[idx].w, windows[idx].h);
            
            int win_id = windows[idx].id;
            if (idx < window_count - 1) {
                DesktopWindow clicked_win = windows[idx];
                for (int i = idx; i < window_count - 1; i++) windows[i] = windows[i + 1];
                windows[window_count - 1] = clicked_win;
                idx = window_count - 1;
            }
            active_window_id = win_id;

            /* Check if clicking in terminal content area (for text selection) */
            if (strcmp(windows[idx].type, "terminal") == 0) {
                printf("[SELECT DEBUG] Clicked on terminal window\n");

                /* Check if NOT in titlebar */
                int in_titlebar = is_in_titlebar(x, y, &windows[idx]);
                printf("[SELECT DEBUG] in_titlebar=%d\n", in_titlebar);

                if (!in_titlebar) {
                    int res_x = windows[idx].x + windows[idx].w - 15;
                    int res_y = windows[idx].y + windows[idx].h - 15;
                    int thumb_x = windows[idx].x + windows[idx].w - 12;
                    int titlebar_y = windows[idx].y + windows[idx].h - 25;

                    printf("[SELECT DEBUG] Click=(%d,%d) thumb_x=%d res_y=%d titlebar_y=%d\n",
                           x, y, thumb_x, res_y, titlebar_y);
                    printf("[SELECT DEBUG] Conditions: x<thumb_x=%d y>res_y=%d y<titlebar_y=%d\n",
                           (x < thumb_x), (y > res_y), (y < titlebar_y));

                    /* Content area: left of thumb, above resize handle, below titlebar */
                    /* Be more lenient - just check it's in the main content area */
                    if (x < thumb_x && y > res_y && y < titlebar_y) {
                        /* In content area - start text selection */
                        printf("[SELECT] STARTING text selection at (%d,%d) in terminal %d\n", x, y, windows[idx].id);
                        windows[idx].is_selecting = 1;
                        windows[idx].select_start_x = x;
                        windows[idx].select_start_y = y;
                        windows[idx].select_end_x = x;
                        windows[idx].select_end_y = y;
                        windows[idx].selection_active = 0;
                        windows[idx].selected_text[0] = '\0';
                    } else {
                        printf("[SELECT DEBUG] NOT in content area - conditions failed\n");
                        /* Still allow selection if clicking in general content area */
                        if (x < thumb_x && y > res_y) {
                            printf("[SELECT] Starting selection (relaxed conditions)\n");
                            windows[idx].is_selecting = 1;
                            windows[idx].select_start_x = x;
                            windows[idx].select_start_y = y;
                            windows[idx].select_end_x = x;
                            windows[idx].select_end_y = y;
                            windows[idx].selection_active = 0;
                            windows[idx].selected_text[0] = '\0';
                        }
                    }
                }
            }

            /* Check if clicking on scroll thumb (right edge of terminal window) */
            if (strcmp(windows[idx].type, "terminal") == 0) {
                int thumb_x = windows[idx].x + windows[idx].w - 12;
                int thumb_right = windows[idx].x + windows[idx].w - 4;
                /* Check if click is in thumb track area */
                if (x >= thumb_x && x <= thumb_right && y >= scroll_track_y && y <= scroll_track_y + scroll_track_h) {
                    is_scroll_thumb_drag = 1;
                    scroll_window_id = win_id;
                    glutPostRedisplay();
                    return;
                }
            }
            
            if (is_in_titlebar(x, y, &windows[idx])) {
                int close_x = windows[idx].x + windows[idx].w - 22;
                int min_x = windows[idx].x + windows[idx].w - 44;
                int btn_y = windows[idx].y + 4;
                if (x >= close_x && x <= close_x + 18 && y >= btn_y && y <= btn_y + 18) close_window(win_id);
                else if (x >= min_x && x <= min_x + 18 && y >= btn_y && y <= btn_y + 18) { windows[idx].minimized = 1; active_window_id = -1; }
                else { is_dragging = 1; drag_window_id = win_id; drag_start_x = x; drag_start_y = y; win_drag_start_x = windows[idx].x; win_drag_start_y = windows[idx].y; }
            } else {
                int res_x = windows[idx].x + windows[idx].w - 15;  /* Match resize handle size */
                int res_y = windows[idx].y + windows[idx].h - 15;
                if (x >= res_x && x <= res_x + 15 && y >= res_y && y <= res_y + 15) { is_resizing = 1; drag_window_id = win_id; drag_start_x = x; drag_start_y = y; win_drag_start_w = windows[idx].w; win_drag_start_h = windows[idx].h; }
            }
            write_session_state();
        } else active_window_id = -1;
        glutPostRedisplay();
    }
    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        is_dragging = 0; is_resizing = 0; drag_window_id = -1;
        is_scroll_thumb_drag = 0; scroll_window_id = -1;
        
        /* Finalize text selection */
        if (active_window_id != -1) {
            for (int i = 0; i < window_count; i++) {
                if (windows[i].id == active_window_id && strcmp(windows[i].type, "terminal") == 0) {
                    if (windows[i].is_selecting) {
                        windows[i].is_selecting = 0;
                        windows[i].selection_active = 1;

                        /* Calculate selection dimensions */
                        int dx = windows[i].select_end_x - windows[i].select_start_x;
                        int dy = windows[i].select_end_y - windows[i].select_start_y;

                        printf("[SELECT] FINALIZED selection at (%d,%d) to (%d,%d) delta=(%d,%d)\n",
                               windows[i].select_start_x, windows[i].select_start_y,
                               windows[i].select_end_x, windows[i].select_end_y,
                               dx, dy);

                        /* TODO: Extract text from frame based on selection coordinates */
                        /* For now, just mark selection as active */
                        if (strlen(windows[i].selected_text) == 0) {
                            strncpy(windows[i].selected_text, "[Selected text]", sizeof(windows[i].selected_text) - 1);
                        }
                        
                        /* Write to clipboard.txt immediately */
                        FILE *cb = fopen("pieces/apps/gl_os/session/clipboard.txt", "w");
                        if (cb) {
                            fprintf(cb, "%s\n", windows[i].selected_text);
                            fclose(cb);
                            printf("[SELECT] Written to clipboard.txt\n");
                        }
                    } else {
                        printf("[SELECT DEBUG] Mouse up but is_selecting=0\n");
                    }
                    break;
                }
            }
        }
    }
}

void motion(int x, int y) {
    mouse_x = x; mouse_y = y;
    last_interaction_time = time(NULL);

    /* Handle text selection in terminal */
    if (active_window_id != -1) {
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == active_window_id && strcmp(windows[i].type, "terminal") == 0) {
                if (windows[i].is_selecting) {
                    windows[i].select_end_x = x;
                    windows[i].select_end_y = y;
                    glutPostRedisplay();
                    return;
                }
            }
        }
    }
    
    /* Handle scroll thumb dragging */
    if (is_scroll_thumb_drag && scroll_window_id != -1) {
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == scroll_window_id && strcmp(windows[i].type, "terminal") == 0) {
                /* Calculate scroll based on mouse Y position in thumb track
                 * Linux terminal behavior: drag thumb UP → see OLDER content (scroll_offset increases)
                 * drag thumb DOWN → see NEWER content (scroll_offset decreases to 0)
                 * GLUT y is top-down, so smaller y = higher on screen
                 */
                int track_y = scroll_track_y;
                int track_h = scroll_track_h;
                
                /* Thumb height (same as in draw_scroll_thumb) */
                int visible_lines = (track_h - 20) / 18;
                int total_lines = windows[i].max_scroll + visible_lines;
                float thumb_h_ratio = (float)visible_lines / (float)total_lines;
                if (thumb_h_ratio < 0.1f) thumb_h_ratio = 0.1f;
                int thumb_h = (int)(track_h * thumb_h_ratio);
                if (thumb_h < 10) thumb_h = 10;
                
                /* Calculate new scroll position from mouse Y
                 * Mouse y is GLUT (top-down), track_y is bottom of track
                 * Drag UP (smaller y) → increase scroll_offset (see older)
                 * Drag DOWN (larger y) → decrease scroll_offset (see newer)
                 */
                int track_h_adj = track_h - thumb_h;
                /* Invert: mouse at top (small y) = max_scroll, mouse at bottom (large y) = 0 */
                float mouse_ratio = 1.0f - ((float)(y - track_y - thumb_h/2) / (float)track_h_adj);
                if (mouse_ratio < 0.0f) mouse_ratio = 0.0f;
                if (mouse_ratio > 1.0f) mouse_ratio = 1.0f;
                
                windows[i].scroll_offset = (int)(mouse_ratio * windows[i].max_scroll);
                glutPostRedisplay();
                return;
            }
        }
    }
    
    if (drag_window_id != -1) {
        int dx = x - drag_start_x; int dy = y - drag_start_y;
        if (is_dragging) {
            windows[window_count - 1].x = win_drag_start_x + dx;
            windows[window_count - 1].y = win_drag_start_y + dy;
            if (windows[window_count - 1].y < 0) windows[window_count - 1].y = 0;
            if (windows[window_count - 1].y > window_height - toolbar_height - 25) windows[window_count - 1].y = window_height - toolbar_height - 25;
        } else if (is_resizing) {
            windows[window_count - 1].w = win_drag_start_w + dx;
            windows[window_count - 1].h = win_drag_start_h + dy;
            if (windows[window_count - 1].w < 100) windows[window_count - 1].w = 100;
            if (windows[window_count - 1].h < 50) windows[window_count - 1].h = 50;
        }
        write_session_state(); glutPostRedisplay();
    }
}

void reshape(int w, int h) { window_width = w; window_height = h; glViewport(0, 0, w, h); }

void free_paths(void) {
    if (session_state_path) free(session_state_path); if (session_history_path) free(session_history_path);
    if (session_view_path) free(session_view_path); if (session_view_changed_path) free(session_view_changed_path);
    if (master_ledger_path) free(master_ledger_path);
    if (input_focus_lock_path) free(input_focus_lock_path);
    if (gl_os_frame_path) free(gl_os_frame_path);
    if (gl_os_frame_pulse) free(gl_os_frame_pulse);
    if (gl_os_frame_history) free(gl_os_frame_history);
}

/* Write GL-OS frame to file (like ASCII-OS compose_frame) */
void write_gl_os_frame(void) {
    /* Write state first (for parser) */
    write_gl_os_state();
    
    /* Parse layout and write frame */
    parse_and_write_frame();
}

int main(int argc, char** argv) {
    setpgid(0, 0); // CPU Safety: Become group leader
    glutInit(&argc, argv); glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height); glutCreateWindow("GL-OS Desktop");
    
    /* Zero-initialize all window state */
    memset(windows, 0, sizeof(windows));
    
    resolve_root();
    load_gl_projects();
    asprintf(&session_state_path, "%s/pieces/apps/gl_os/session/state.txt", project_root);
    asprintf(&session_history_path, "%s/pieces/apps/gl_os/session/history.txt", project_root);
    asprintf(&session_view_path, "%s/pieces/apps/gl_os/session/view.txt", project_root);
    asprintf(&session_view_changed_path, "%s/pieces/apps/gl_os/session/view_changed.txt", project_root);
    asprintf(&master_ledger_path, "%s/pieces/apps/gl_os/ledger/master_ledger.txt", project_root);
    asprintf(&input_focus_lock_path, "%s/pieces/apps/gl_os/session/input_focus.lock", project_root);
    asprintf(&gl_os_frame_path, "%s/pieces/apps/gl_os/session/current_frame.txt", project_root);
    asprintf(&gl_os_frame_pulse, "%s/pieces/apps/gl_os/session/gl_os_pulse.txt", project_root);
    asprintf(&gl_os_frame_history, "%s/pieces/apps/gl_os/session/frame_history.txt", project_root);

    last_interaction_time = time(NULL);

    /* Create input focus lock - tells CHTPM daemons to stop writing input */
    FILE *lock = fopen(input_focus_lock_path, "w");
    if (lock) {
        fprintf(lock, "# GL-OS Input Focus Lock\n");
        fprintf(lock, "# CHTPM daemons: DO NOT write to input files while this file exists\n");
        fprintf(lock, "owner=gl_desktop\n");
        fprintf(lock, "session=gl-os\n");
        fclose(lock);
    }

    /* Initialize session files (clear history on each launch) */
    FILE *f; f = fopen(session_state_path, "w"); if (f) fclose(f);  /* Clear state */
    f = fopen(session_history_path, "w"); if (f) fclose(f);        /* Clear history */
    f = fopen(session_view_path, "w"); if (f) fclose(f);           /* Clear view */
    f = fopen(session_view_changed_path, "w"); if (f) fclose(f);   /* Clear marker */
    f = fopen(master_ledger_path, "a"); if (f) fclose(f);          /* Keep ledger (append) */
    f = fopen(gl_os_frame_path, "w"); if (f) fclose(f);            /* Clear frame */
    f = fopen(gl_os_frame_history, "w"); if (f) fclose(f);         /* Clear frame history */

    create_terminal_window();
    printf("[GL-OS] Desktop Environment Starting... (GLTPM Support ENABLED)\n");
    log_master("DESKTOP_START", "gl-os");
    write_gl_os_frame();  /* Write initial frame to file */
    glutDisplayFunc(render_scene); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard); 
    glutSpecialFunc(special_keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion); glutTimerFunc(30, timer, 0);
    glutJoystickFunc(joystick, 50);  /* Joystick callback (50ms poll rate) */
    glutMainLoop();
    
    /* Remove input focus lock on exit - CHTPM can resume input */
    if (input_focus_lock_path) {
        remove(input_focus_lock_path);
    }
    
    free_paths(); return 0;
}
