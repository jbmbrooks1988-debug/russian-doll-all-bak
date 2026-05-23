#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>

#define MODULE_NAME "agy-text-editor"
#define MAX_PATH 4096
#define MAX_LINE 1024

static char project_root[MAX_PATH] = ".";
static char active_file_path[MAX_PATH] = "projects/agy-text-editor/pieces/document.txt";
static char file_path_input_buffer[MAX_PATH] = "";
static char search_query_buffer[MAX_LINE] = "";
static char current_dir[MAX_PATH] = "projects/agy-text-editor";
static int browser_mode = 0; // Added back

static int cursor_x = 0, cursor_y = 0;
static char document_lines[100][MAX_LINE];
static int total_lines = 0;

static char response_buffer[256] = "Ready.";
static char input_line_buffer[MAX_LINE] = "";

static volatile sig_atomic_t g_shutdown = 0;

static void handle_command(const char *cmd);
static void load_document_to_buffer(void);
static void save_buffer_to_document(void);
static void handle_interact_key(int key);
static int process_key(int key);
static void trigger_render(void);
static void update_gui_state(void);
static void build_editor_map(char *out, size_t max_sz);
static void load_project_metadata(void);

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        setenv("PROJECT_ROOT", project_root, 1);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

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

static void resolve_paths(void) {
    char *abs_cwd = realpath(".", NULL);
    if (abs_cwd) { strncpy(project_root, abs_cwd, MAX_PATH - 1); free(abs_cwd); }
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0'; char *k = trim_str(line), *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    char *abs_v = realpath(v, NULL);
                    if (abs_v) { strncpy(project_root, abs_v, MAX_PATH - 1); free(abs_v); }
                }
            }
        }
        fclose(kvp);
    }
}

static void load_state_truth(void) {
    load_project_metadata();
    load_document_to_buffer();
    update_gui_state();
    trigger_render();
}

static int is_active_layout(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    char line[MAX_LINE]; int result = 0;
    if (fgets(line, sizeof(line), f)) {
        char *cur = trim_str(line);
        result = (strstr(cur, "projects/agy-text-editor/layouts/") != NULL);
    }
    fclose(f); free(path); return result;
}

static void get_current_layout_name(char *buf, size_t sz) {
    buf[0] = '\0'; char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        if (fgets(line, sizeof(line), f)) {
            char *trimmed = trim_str(line), *last_slash = strrchr(trimmed, '/');
            if (last_slash) strncpy(buf, last_slash + 1, sz - 1);
            else strncpy(buf, trimmed, sz - 1);
            buf[sz - 1] = '\0';
        }
        fclose(f);
    }
    free(path);
}

static void read_editor_line(void) {
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/pieces/os/cli_buffers.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) { char line[MAX_LINE]; while (fgets(line, sizeof(line), f)) if (line[0] == 'e') { char *nl = strchr(line, '\n'); if (nl) *nl = '\0'; strncpy(input_line_buffer, line + 1, sizeof(input_line_buffer) - 1); } fclose(f); }
}

static void read_file_path_input(void) {
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/pieces/os/cli_buffers.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) { char line[MAX_LINE]; while (fgets(line, sizeof(line), f)) if (line[0] == 'f') { char *nl = strchr(line, '\n'); if (nl) *nl = '\0'; char *val = trim_str(line + 1); if (strlen(val) > 0 || strlen(file_path_input_buffer) == 0) strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1); } fclose(f); }
}

static void set_file_path_input(const char *val) {
    char *path = NULL; if (asprintf(&path, "%s/pieces/os/cli_buffers.txt", project_root) != -1) { FILE *f = fopen(path, "a"); if (f) { fprintf(f, "f%s\n", val); fclose(f); } free(path); }
    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
}

static void clear_editor_line(void) {
    char *path = NULL; if (asprintf(&path, "%s/pieces/os/cli_buffers.txt", project_root) != -1) { FILE *f = fopen(path, "a"); if (f) { fprintf(f, "e\n"); fclose(f); } free(path); }
    input_line_buffer[0] = '\0';
}

static void read_search_query_input(void) {
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/pieces/os/cli_buffers.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) { char line[MAX_LINE]; while (fgets(line, sizeof(line), f)) if (line[0] == 's') { char *nl = strchr(line, '\n'); if (nl) *nl = '\0'; strncpy(search_query_buffer, trim_str(line + 1), sizeof(search_query_buffer) - 1); } fclose(f); }
}

static void clear_search_query(void) {
    char *path = NULL; if (asprintf(&path, "%s/pieces/os/cli_buffers.txt", project_root) != -1) { FILE *f = fopen(path, "a"); if (f) { fprintf(f, "s\n"); fclose(f); } free(path); }
    search_query_buffer[0] = '\0';
}

static void trigger_render(void) {
    char pulse[MAX_PATH];
    snprintf(pulse, sizeof(pulse), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(pulse, "a"); if (f) { fprintf(f, "M\n"); fclose(f); }
}

static void transition_to_layout(const char *layout_path) {
    char *lp = NULL; if (asprintf(&lp, "%s/pieces/display/layout_changed.txt", project_root) != -1) { FILE *f = fopen(lp, "a"); if (f) { fprintf(f, "%s\n", layout_path); fclose(f); } free(lp); }
    update_gui_state(); trigger_render();
}

static void save_project_metadata(void) {
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/projects/agy-text-editor/project.pdl", project_root);
    FILE *f = fopen(path, "w"); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | project_id         | agy-text-editor\nMETA         | version            | 1.0\nMETA         | entry_layout       | projects/agy-text-editor/layouts/editor.chtpm\n\nSTATE        | active_file        | %s\nSTATE        | cursor_x           | %d\nSTATE        | cursor_y           | %d\n\nRESPONSE     | default            | %s\n", active_file_path, cursor_x, cursor_y, response_buffer); fclose(f); }
}

static void load_project_metadata(void) {
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/projects/agy-text-editor/project.pdl", project_root);
    FILE *f = fopen(path, "r"); if (f) { char line[MAX_LINE]; while (fgets(line, sizeof(line), f)) { char *p1 = strchr(line, '|'), *p2 = p1 ? strchr(p1+1, '|') : NULL; if (!p2) continue; *p2 = '\0'; char *k = trim_str(p1+1), *v = trim_str(p2+1); if (strcmp(k, "active_file") == 0) strncpy(active_file_path, v, sizeof(active_file_path)-1); else if (strcmp(k, "cursor_x") == 0) cursor_x = atoi(v); else if (strcmp(k, "cursor_y") == 0) cursor_y = atoi(v); } fclose(f); }
}

static void sync_system_state(void) {
    char state_path[MAX_PATH], cmd[MAX_PATH * 2];
    snprintf(state_path, sizeof(state_path), "%s/pieces/os/state.txt", project_root);
    snprintf(cmd, sizeof(cmd), "'%s/pieces/ops/file-op/+x/sync_op.+x' '%s' 'project_id' 'agy-text-editor'", project_root, state_path); run_command(cmd);
    snprintf(cmd, sizeof(cmd), "'%s/pieces/ops/file-op/+x/sync_op.+x' '%s' 'active_project' '%s'", project_root, state_path, active_file_path); run_command(cmd);
}

static int save_to_path(const char *rel_path) {
    char full_path[MAX_PATH]; if (rel_path[0] == '/') strncpy(full_path, rel_path, MAX_PATH - 1); else if (strstr(rel_path, "projects/") == rel_path) snprintf(full_path, sizeof(full_path), "%s/%s", project_root, rel_path); else snprintf(full_path, sizeof(full_path), "%s/%s/%s", project_root, current_dir, rel_path);
    char *dir_part = strdup(full_path), *last_s = dir_part ? strrchr(dir_part, '/') : NULL; if (last_s) { *last_s = '\0'; char mk_cmd[MAX_PATH*2]; snprintf(mk_cmd, sizeof(mk_cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' mkdir '%s'", project_root, dir_part); run_command(mk_cmd); } if (dir_part) free(dir_part);
    char doc_path[MAX_PATH], cmd[MAX_PATH*2]; snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root); snprintf(cmd, sizeof(cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' save '%s' '%s'", project_root, doc_path, full_path);
    if (run_command(cmd) == 0) { strncpy(active_file_path, full_path, sizeof(active_file_path)-1); snprintf(response_buffer, sizeof(response_buffer), "Saved to %s", active_file_path); save_project_metadata(); return 0; }
    snprintf(response_buffer, sizeof(response_buffer), "Error: Save Op failed"); return -1;
}

static int load_from_path(const char *rel_path) {
    char full_path[MAX_PATH]; if (rel_path[0] == '/') strncpy(full_path, rel_path, MAX_PATH - 1); else if (strstr(rel_path, "projects/") == rel_path) snprintf(full_path, sizeof(full_path), "%s/%s", project_root, rel_path); else snprintf(full_path, sizeof(full_path), "%s/%s/%s", project_root, current_dir, rel_path);
    char doc_path[MAX_PATH], pdl_path[MAX_PATH], cmd[MAX_PATH*2]; snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root); snprintf(pdl_path, sizeof(pdl_path), "%s/projects/agy-text-editor/project.pdl", project_root); snprintf(cmd, sizeof(cmd), "'%s/pieces/ops/file-op/+x/load_file_op.+x' '%s' '%s' '%s' 'agy-text-editor' 'projects/agy-text-editor/layouts/editor.chtpm'", project_root, full_path, doc_path, pdl_path);
    if (run_command(cmd) == 0) { char m_path[MAX_PATH]; snprintf(m_path, sizeof(m_path), "%s/projects/agy-text-editor/manager/load_complete.txt", project_root); struct stat st; long i_size = (stat(m_path, &st) == 0) ? st.st_size : 0; int t = 0; while (t < 50) { usleep(100000); if (stat(m_path, &st) == 0 && st.st_size > i_size) break; t++; } strncpy(active_file_path, full_path, sizeof(active_file_path)-1); snprintf(response_buffer, sizeof(response_buffer), "Loaded %s", active_file_path); load_document_to_buffer(); cursor_x = 0; cursor_y = 0; sync_system_state(); transition_to_layout("projects/agy-text-editor/layouts/editor.chtpm"); return 0; }
    snprintf(response_buffer, sizeof(response_buffer), "Error: Load Op failed"); return -1;
}

static void load_document_to_buffer(void) {
    char doc_path[MAX_PATH]; snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
    FILE *f = fopen(doc_path, "r"); total_lines = 0;
    if (f) { while (fgets(document_lines[total_lines], MAX_LINE, f) && total_lines < 99) { char *nl = strchr(document_lines[total_lines], '\n'); if (nl) *nl = '\0'; nl = strchr(document_lines[total_lines], '\r'); if (nl) *nl = '\0'; total_lines++; } fclose(f); }
    if (total_lines == 0) { strcpy(document_lines[0], ""); total_lines = 1; }
}

static void save_buffer_to_document(void) {
    char doc_path[MAX_PATH]; snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
    FILE *f = fopen(doc_path, "w"); if (f) { for (int i = 0; i < total_lines; i++) fprintf(f, "%s\n", document_lines[i]); fclose(f); }
}

static void build_editor_map(char *out, size_t max_sz) {
    out[0] = '\0'; int v_h = 8, v_s_y = cursor_y - (v_h / 2); if (v_s_y < 0) v_s_y = 0; if (v_s_y + v_h > total_lines && total_lines > v_h) v_s_y = total_lines - v_h;
    int v_w = 40, v_s_x = cursor_x - (v_w / 2); if (v_s_x < 0) v_s_x = 0;
    for (int i = v_s_y; i < v_s_y + v_h; i++) {
        char line_buf[MAX_LINE]; if (i < total_lines) strncpy(line_buf, document_lines[i], MAX_LINE - 1); else strcpy(line_buf, "");
        char formatted[MAX_LINE + 512], final_line[MAX_LINE + 64]; int len = strlen(line_buf);
        if (i == cursor_y) { if (cursor_x > len) cursor_x = len; char before[MAX_LINE], after[MAX_LINE]; strncpy(before, line_buf, cursor_x); before[cursor_x] = '\0'; strcpy(after, line_buf + cursor_x); snprintf(final_line, sizeof(final_line), "%s[X]%s", before, after); }
        else strcpy(final_line, line_buf);
        char scrolled_line[v_w + 1]; int final_len = strlen(final_line); if (v_s_x < final_len) { strncpy(scrolled_line, final_line + v_s_x, v_w); scrolled_line[v_w] = '\0'; } else strcpy(scrolled_line, "");
        snprintf(formatted, sizeof(formatted), "<text label=\"║  %-40.40s ║\" /><br/>", scrolled_line); strncat(out, formatted, max_sz - strlen(out) - 1);
    }
}

static void handle_interact_key(int key) {
    int modified = 0;
    if (key == 'w' || key == 'W' || key == 1002) { if (cursor_y > 0) cursor_y--; int len = strlen(document_lines[cursor_y]); if (cursor_x > len) cursor_x = len; }
    else if (key == 's' || key == 'S' || key == 1003) { if (cursor_y < 98) { if (cursor_y >= total_lines - 1) { strcpy(document_lines[total_lines], ""); total_lines++; modified = 1; } cursor_y++; int len = strlen(document_lines[cursor_y]); if (cursor_x > len) cursor_x = len; } }
    else if (key == 'a' || key == 'A' || key == 1000) { if (cursor_x > 0) cursor_x--; }
    else if (key == 'd' || key == 'D' || key == 1001) { int len = strlen(document_lines[cursor_y]); if (cursor_x < len) cursor_x++; else if (len < MAX_LINE - 2) { document_lines[cursor_y][len] = ' '; document_lines[cursor_y][len + 1] = '\0'; cursor_x++; modified = 1; } }
    else if (key == 127 || key == 8) { if (cursor_x > 0) { char *line = document_lines[cursor_y]; memmove(line + cursor_x - 1, line + cursor_x, strlen(line + cursor_x) + 1); cursor_x--; modified = 1; } else if (cursor_y > 0) { int prev_len = strlen(document_lines[cursor_y-1]); if (prev_len + strlen(document_lines[cursor_y]) < MAX_LINE - 1) { strcat(document_lines[cursor_y-1], document_lines[cursor_y]); for (int i = cursor_y; i < total_lines - 1; i++) strcpy(document_lines[i], document_lines[i+1]); total_lines--; cursor_y--; cursor_x = prev_len; modified = 1; } } }
    else if (key == 10 || key == 13) { if (total_lines < 99) { for (int i = total_lines; i > cursor_y + 1; i--) strcpy(document_lines[i], document_lines[i-1]); strcpy(document_lines[cursor_y + 1], document_lines[cursor_y] + cursor_x); document_lines[cursor_y][cursor_x] = '\0'; total_lines++; cursor_y++; cursor_x = 0; modified = 1; } }
    else if (key >= 32 && key <= 126) { char *line = document_lines[cursor_y]; int len = strlen(line); if (len < MAX_LINE - 2) { if (cursor_x > len) { for (int i = len; i < cursor_x; i++) line[i] = ' '; line[cursor_x] = '\0'; len = cursor_x; } memmove(line + cursor_x + 1, line + cursor_x, len - cursor_x + 1); line[cursor_x] = (char)key; cursor_x++; modified = 1; } }
    if (modified) save_buffer_to_document();
}

static int process_key(int key) {
    int processed = 0; char layout[MAX_LINE]; get_current_layout_name(layout, sizeof(layout));
    if (key == 27) { if (strcmp(layout, "editing.chtpm") == 0) transition_to_layout("projects/agy-text-editor/layouts/editor.chtpm"); return 1; }
    if (strcmp(layout, "editor.chtpm") == 0 || strcmp(layout, "file_menu.chtpm") == 0 || strcmp(layout, "file_browser.chtpm") == 0 || strcmp(layout, "editing.chtpm") == 0) {
        if (key == '1') { read_editor_line(); char doc_path[MAX_PATH]; snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root); FILE *f = fopen(doc_path, "a"); if (f) { fprintf(f, "%s\n", input_line_buffer); fclose(f); strcpy(response_buffer, "Line appended."); load_document_to_buffer(); } else strcpy(response_buffer, "Error writing file."); clear_editor_line(); return 1; }
        else if (key == '2') { char doc_path[MAX_PATH]; snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root); FILE *f = fopen(doc_path, "w"); if (f) { fclose(f); strcpy(response_buffer, "File cleared."); load_document_to_buffer(); } else strcpy(response_buffer, "Error clearing file."); return 1; }
        else if (key == '6') { char doc_path[MAX_PATH]; snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root); FILE *f = fopen(doc_path, "w"); if (f) { fclose(f); strcpy(active_file_path, "none"); strcpy(response_buffer, "New file created."); load_document_to_buffer(); cursor_x = 0; cursor_y = 0; save_project_metadata(); } else strcpy(response_buffer, "Error creating new file."); return 1; }
        else if (key == '7') { if (strcmp(active_file_path, "none") == 0 || strlen(active_file_path) == 0) { browser_mode = 1; clear_search_query(); set_file_path_input("projects/agy-text-editor/pieces/document.txt"); transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm"); strcpy(response_buffer, "Specify Save-As path."); } else save_to_path(active_file_path); return 1; }
        else if (key == '8') { browser_mode = 1; clear_search_query(); set_file_path_input(active_file_path); transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm"); strcpy(response_buffer, "Enter Save-As path."); return 1; }
        else if (key == '9') { browser_mode = 0; clear_search_query(); set_file_path_input(""); transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm"); strcpy(response_buffer, "Select file to load."); return 1; }
    }
    if (strcmp(layout, "editing.chtpm") == 0) { handle_interact_key(key); return 1; }
    return processed;
}

static void handle_command(const char *cmd) {
    if (strcmp(cmd, "SET_MODE:EDITING") == 0) transition_to_layout("projects/agy-text-editor/layouts/editing.chtpm");
    else if (strcmp(cmd, "SET_MODE:MENU") == 0) transition_to_layout("projects/agy-text-editor/layouts/editor.chtpm");
    else if (strcmp(cmd, "SET_MODE:BROWSER") == 0) { sync_system_state(); transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm"); }
    else if (strncmp(cmd, "SET_DIR:", 8) == 0) { strncpy(current_dir, cmd + 8, sizeof(current_dir) - 1); clear_search_query(); }
    else if (strncmp(cmd, "SET_INPUT_PATH:", 15) == 0) set_file_path_input(cmd + 15);
    else if (strncmp(cmd, "LOAD_FILE:", 10) == 0) { set_file_path_input(cmd + 10); load_from_path(cmd + 10); }
    else if (strcmp(cmd, "LOAD_ACTION") == 0) { read_file_path_input(); load_from_path(file_path_input_buffer); }
    else if (strcmp(cmd, "SAVE_AS_ACTION") == 0) { read_file_path_input(); save_to_path(file_path_input_buffer); }
}

void update_gui_state() {
    if (!is_active_layout()) return;
    char s_path[MAX_PATH]; snprintf(s_path, sizeof(s_path), "%s/projects/agy-text-editor/manager/gui_state.txt", project_root);
    char editor_map[8192] = ""; build_editor_map(editor_map, sizeof(editor_map));
    read_editor_line(); read_file_path_input(); read_search_query_input();
    
    char header_title[256];
    char *fname = strrchr(active_file_path, '/');
    if (fname) fname++; else fname = active_file_path;
    snprintf(header_title, sizeof(header_title), "AGY EDITOR - %s", fname);
    
    char active_file_line[512]; snprintf(active_file_line, sizeof(active_file_line), "<text label=\"║  FILE: %-34.34s ║\" /><br/>", active_file_path);
    FILE *f = fopen(s_path, "w");
    if (f) {
        fprintf(f, "module_path=projects/agy-text-editor/manager/+x/agy-text-editor_manager.+x\n");
        fprintf(f, "active_layout_id=editor.chtpm\n");
        fprintf(f, "app_title=%s\n", header_title);
        fprintf(f, "project_id=agy-text-editor\n");
        fprintf(f, "active_project=%s\n", active_file_path);
        fprintf(f, "editor_map=%s\n", editor_map);
        fprintf(f, "input_line=%s\n", input_line_buffer);
        fprintf(f, "active_file_info_line=%s\n", active_file_line);
        fprintf(f, "editor_response_line=║  %-45.45s ║\n", response_buffer);
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint); signal(SIGTERM, handle_sigint); resolve_paths();
    if (argc > 1 && strcmp(argv[1], "--test-load-pdl") == 0) { load_project_metadata(); printf("Current Active File from PDL: %s\n", active_file_path); return 0; }
    load_project_metadata(); load_document_to_buffer(); update_gui_state(); trigger_render();
    char *h_path = NULL; asprintf(&h_path, "%s/pieces/keyboard/history.txt", project_root);
    char *p_path = NULL; asprintf(&p_path, "%s/projects/agy-text-editor/manager/state_changed.txt", project_root);
    long last_pos = 0, last_pulse_size = 0; struct stat st; if (stat(h_path, &st) == 0) last_pos = st.st_size; if (stat(p_path, &st) == 0) last_pulse_size = st.st_size;
    while (!g_shutdown) {
        if (stat(p_path, &st) == 0 && st.st_size > last_pulse_size) { last_pulse_size = st.st_size; load_state_truth(); }
        if (!is_active_layout()) { usleep(100000); continue; }
        if (stat(h_path, &st) == 0 && st.st_size > last_pos) {
            FILE *hf = fopen(h_path, "r"); if (hf) {
                fseek(hf, last_pos, SEEK_SET); char line[MAX_LINE]; while (fgets(line, sizeof(line), hf)) {
                    char *cmd = strstr(line, "COMMAND: "), *kpress = strstr(line, "KEY_PRESSED: "); int processed_l = 0;
                    if (cmd) { handle_command(trim_str(cmd + 9)); processed_l = 1; }
                    else if (kpress) { if (process_key(atoi(kpress + 13))) processed_l = 1; }
                    else { int k = atoi(line); if (k != 0 || line[0] == '0') if (process_key(k)) processed_l = 1; }
                    if (processed_l) { update_gui_state(); trigger_render(); }
                }
                last_pos = ftell(hf); fclose(hf);
            }
        } else if (st.st_size < last_pos) last_pos = 0;
        usleep(16667);
    }
    free(h_path); free(p_path); return 0;
}
