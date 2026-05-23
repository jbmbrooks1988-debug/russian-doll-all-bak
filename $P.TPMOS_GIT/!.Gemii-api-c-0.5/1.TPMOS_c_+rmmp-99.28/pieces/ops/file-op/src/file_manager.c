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

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_ITEMS 100

static char project_root[MAX_PATH] = ".";
static char project_id[MAX_LINE] = "template";
static char active_file_path[MAX_PATH] = "none";
static char response_buffer[256] = "Ready.";
static char current_dir[MAX_PATH] = "projects";
static int browser_mode = 0; // 0 = Load, 1 = Save
static char search_query_buffer[MAX_LINE] = "";
static char file_path_input_buffer[MAX_PATH] = "";
static char return_layout[MAX_PATH] = "pieces/chtpm/layouts/os.chtpm";

static int is_active_layout(void);
static void update_gui_state(void);
static void handle_command(const char *cmd);
static void clear_search_query(void);
static void set_file_path_input(const char *val);

static volatile sig_atomic_t g_shutdown = 0;
void handle_sigint(int sig) { (void)sig; g_shutdown = 1; }

int run_command(const char* cmd) {
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

char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
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

void load_state() {
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/pieces/os/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0'; char *k = trim_str(line), *v = trim_str(eq + 1);
                if (strcmp(k, "project_id") == 0) strncpy(project_id, v, sizeof(project_id)-1);
                else if (strcmp(k, "active_project") == 0) strncpy(active_file_path, v, sizeof(active_file_path)-1);
                else if (strcmp(k, "return_layout") == 0) strncpy(return_layout, v, sizeof(return_layout)-1);
                else if (strcmp(k, "browser_current_dir") == 0) strncpy(current_dir, v, sizeof(current_dir)-1);
                else if (strcmp(k, "browser_mode") == 0) browser_mode = atoi(v);
            }
        }
        fclose(f);
    }
}

void read_cli_buffers() {
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/pieces/os/cli_buffers.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) { char line[MAX_LINE]; while (fgets(line, sizeof(line), f)) if (line[0] == 's') strncpy(search_query_buffer, trim_str(line + 1), sizeof(search_query_buffer) - 1); else if (line[0] == 'f') strncpy(file_path_input_buffer, trim_str(line + 1), sizeof(file_path_input_buffer) - 1); fclose(f); }
}

static void set_file_path_input(const char *val) {
    char *path = NULL; if (asprintf(&path, "%s/pieces/os/cli_buffers.txt", project_root) != -1) { FILE *f = fopen(path, "a"); if (f) { fprintf(f, "f%s\n", val); fclose(f); } free(path); }
    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
}

static void clear_search_query() {
    char *path = NULL; if (asprintf(&path, "%s/pieces/os/cli_buffers.txt", project_root) != -1) { FILE *f = fopen(path, "a"); if (f) { fprintf(f, "s\n"); fclose(f); } free(path); }
    search_query_buffer[0] = '\0';
}

void trigger_render() {
    char pulse[MAX_PATH]; snprintf(pulse, sizeof(pulse), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(pulse, "a"); if (f) { fprintf(f, "M\n"); fclose(f); }
}

void trigger_state_pulse() {
    char pulse[MAX_PATH]; snprintf(pulse, sizeof(pulse), "%s/projects/%s/manager/state_changed.txt", project_root, project_id);
    FILE *f = fopen(pulse, "a"); if (f) { fprintf(f, "S\n"); fclose(f); }
}

static void transition_to_layout(const char *layout_path) {
    char *lp = NULL; if (asprintf(&lp, "%s/pieces/display/layout_changed.txt", project_root) != -1) { FILE *f = fopen(lp, "a"); if (f) { fprintf(f, "%s\n", layout_path); fclose(f); } free(lp); }
}

int get_digits(int num) { if (num >= 100) return 3; if (num >= 10) return 2; return 1; }

static void append_aligned_button_attr(char *out, size_t max_sz, const char *label, const char *attr_name, const char *attr_val, int *p_display_num) {
    int num = *p_display_num, digits = get_digits(num), label_len = strlen(label);
    int visual_len = 9 + digits + label_len, padding = 45 - visual_len;
    if (padding < 0) padding = 0;
    char btn_markup[1024]; snprintf(btn_markup, sizeof(btn_markup), "<text label=\"║  \" /><button label=\"%s\" %s=\"%s\" />", label, attr_name, attr_val); strncat(out, btn_markup, max_sz - strlen(out) - 1);
    if (padding > 0) { char pad_str[128]; snprintf(pad_str, sizeof(pad_str), "<text label=\"%.*s\" />", padding, "                                                                                "); strncat(out, pad_str, max_sz - strlen(out) - 1); }
    strncat(out, "<text label=\" ║\" /><br/>", max_sz - strlen(out) - 1); (*p_display_num)++;
}

int find_autocomplete_matches(const char *input, const char *current_browser_dir, char matches[][256], int max_matches) {
    char dir_to_scan[MAX_PATH]; const char *prefix = ""; const char *last_slash = strrchr(input, '/');
    if (last_slash) { size_t dir_len = last_slash - input; strncpy(dir_to_scan, input, dir_len); dir_to_scan[dir_len] = '\0'; prefix = last_slash + 1; if (strlen(dir_to_scan) == 0) strcpy(dir_to_scan, "/"); }
    else { prefix = input; strcpy(dir_to_scan, current_browser_dir); }
    char full_scan_path[MAX_PATH]; if (dir_to_scan[0] == '/') strncpy(full_scan_path, dir_to_scan, MAX_PATH - 1); else snprintf(full_scan_path, sizeof(full_scan_path), "%s/%s", project_root, dir_to_scan);
    char cmd[MAX_PATH + 100]; snprintf(cmd, sizeof(cmd), "'%s/pieces/ops/file-op/+x/list_dir_op.+x' '%s'", project_root, full_scan_path);
    FILE *fp = popen(cmd, "r"); if (!fp) return 0;
    char line[MAX_LINE]; int count = 0; size_t prefix_len = strlen(prefix);
    while (fgets(line, sizeof(line), fp) && count < max_matches) {
        int is_dir = (strncmp(line, "[DIR] ", 6) == 0); char *name = line + 6, *space = strchr(name, ' '), *nl = strchr(name, '\n'), *slash = strchr(name, '/');
        if (space) *space = '\0'; if (nl) *nl = '\0'; if (slash) *slash = '\0';
        if (strncmp(name, prefix, prefix_len) == 0) { if (last_slash) snprintf(matches[count], 256, "%.*s/%s%s", (int)(last_slash - input), input, name, is_dir ? "/" : ""); else snprintf(matches[count], 256, "%s%s", name, is_dir ? "/" : ""); count++; }
    }
    pclose(fp); return count;
}

void build_browser_markup(char* out, size_t sz, int *p_next_num) {
    out[0] = '\0';
    if (strcmp(current_dir, ".") != 0 && strlen(current_dir) > 0) { char parent_dir[MAX_PATH], *last_slash = strrchr(current_dir, '/'); if (last_slash) { strncpy(parent_dir, current_dir, last_slash - current_dir); parent_dir[last_slash - current_dir] = '\0'; } else strcpy(parent_dir, "."); char action[MAX_PATH + 10]; snprintf(action, sizeof(action), "SET_DIR:%s", parent_dir); append_aligned_button_attr(out, sz, "<- BACK", "onClick", action, p_next_num); }
    char full_current_dir[MAX_PATH]; if (current_dir[0] == '/') strncpy(full_current_dir, current_dir, MAX_PATH - 1); else snprintf(full_current_dir, sizeof(full_current_dir), "%s/%s", project_root, current_dir);
    char cmd[MAX_PATH + 100]; snprintf(cmd, sizeof(cmd), "'%s/pieces/ops/file-op/+x/list_dir_op.+x' '%s'", project_root, full_current_dir);
    FILE *lp = popen(cmd, "r");
    if (lp) {
        char line[512]; int items_displayed = 0, limit = 12;
        while (fgets(line, sizeof(line), lp) && items_displayed < limit) {
            int is_dir = (strncmp(line, "[DIR] ", 6) == 0); char *name = line + 6, *nl = strchr(name, '\n'); if (nl) *nl = '\0';
            if (strlen(search_query_buffer) > 0 && strcasestr(name, search_query_buffer) == NULL) continue;
            char *name_only = strdup(name), *space = strchr(name_only, ' '), *slash = strchr(name_only, '/'); if (space) *space = '\0'; if (slash) *slash = '\0';
            char target_path[MAX_PATH]; if (current_dir[0] == '/') snprintf(target_path, sizeof(target_path), "%s/%s", current_dir, name_only); else if (strcmp(current_dir, ".") == 0) snprintf(target_path, sizeof(target_path), "%s", name_only); else snprintf(target_path, sizeof(target_path), "%s/%s", current_dir, name_only);
            char action[MAX_PATH + 20]; if (is_dir) snprintf(action, sizeof(action), "SET_DIR:%s", target_path); else if (browser_mode == 0) snprintf(action, sizeof(action), "LOAD_FILE:%s", target_path); else snprintf(action, sizeof(action), "SET_INPUT_PATH:%s", target_path);
            append_aligned_button_attr(out, sz, name, "onClick", action, p_next_num); items_displayed++; free(name_only);
        }
        if (items_displayed == 0) strcat(out, "<text label=\"║  [Empty or Not Found]                      ║\" /><br/>");
        pclose(lp);
    }
}

void update_gui_state() {
    if (!is_active_layout()) return;
    char state_path[MAX_PATH]; snprintf(state_path, sizeof(state_path), "%s/projects/%s/manager/gui_state.txt", project_root, project_id);
    FILE *f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "project_id=%s\nactive_project=%s\nactive_file=%s\nreturn_layout=%s\nbrowser_mode=%d\nbrowser_current_dir=%s\n", project_id, active_file_path, active_file_path, return_layout, browser_mode, current_dir);
        fprintf(f, "active_file_info_line=<text label=\"║  FILE: %-34.34s ║\" /><br/>\n", active_file_path);
        fprintf(f, "editor_response_line=║  %-45.45s ║\n", response_buffer);
        fprintf(f, "browser_mode_header=<text label=\"║  MODE: %-34.34s ║\" /><br/>\n", browser_mode == 0 ? "LOAD FILE" : "SAVE FILE AS");
        fprintf(f, "browser_current_dir_line=<text label=\"║  DIR: %-35.35s ║\" /><br/>\n", current_dir);
        fprintf(f, "search_query_val=%s\nfile_path_input_val=%s\n", search_query_buffer, file_path_input_buffer);
        int next_num = 3;
        char suggestions_markup[8192] = ""; char matches[4][256]; int num_matches = find_autocomplete_matches(file_path_input_buffer, current_dir, matches, 4);
        if (num_matches > 0) { strcat(suggestions_markup, "<text label=\"║  SUGGESTIONS:                               ║\" /><br/>"); for (int i = 0; i < num_matches; i++) { char action[512]; snprintf(action, sizeof(action), "SET_INPUT_PATH:%s", matches[i]); append_aligned_button_attr(suggestions_markup, sizeof(suggestions_markup), matches[i], "onClick", action, &next_num); } }
        else strcat(suggestions_markup, "<text label=\"║  (No suggestions)                          ║\" /><br/>");
        fprintf(f, "autocomplete_suggestions_markup=%s\n", suggestions_markup);
        char browser_markup[32768] = ""; build_browser_markup(browser_markup, sizeof(browser_markup), &next_num);
        fprintf(f, "directory_browser_markup=%s\n", browser_markup);
        char actions_markup[8192] = ""; append_aligned_button_attr(actions_markup, sizeof(actions_markup), browser_mode == 0 ? "LOAD FILE" : "SAVE FILE", "onClick", "OK_ACTION", &next_num);
        append_aligned_button_attr(actions_markup, sizeof(actions_markup), "CANCEL", "href", return_layout, &next_num);
        fprintf(f, "browser_action_buttons_markup=%s\n", actions_markup);
        fclose(f);
    }
}

static void handle_command(const char *cmd) {
    if (strncmp(cmd, "SET_DIR:", 8) == 0) { strncpy(current_dir, cmd + 8, sizeof(current_dir) - 1); if (strlen(current_dir) == 0) strcpy(current_dir, "."); clear_search_query(); }
    else if (strncmp(cmd, "SET_INPUT_PATH:", 15) == 0) set_file_path_input(cmd + 15);
    else if (strcmp(cmd, "OK_ACTION") == 0) {
        int success = 0;
        if (browser_mode == 0) {
            char run_cmd[MAX_PATH*2]; snprintf(run_cmd, sizeof(run_cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' load '%s/%s' '%s/projects/%s/pieces/document.txt'", project_root, project_root, file_path_input_buffer, project_root, project_id);
            if (run_command(run_cmd) == 0) success = 1;
        } else {
            char *dir_part = strdup(file_path_input_buffer), *last_s = dir_part ? strrchr(dir_part, '/') : NULL;
            if (last_s) { *last_s = '\0'; char mk_cmd[MAX_PATH*2]; snprintf(mk_cmd, sizeof(mk_cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' mkdir '%s/%s'", project_root, project_root, dir_part); run_command(mk_cmd); }
            if (dir_part) free(dir_part);
            char run_cmd[MAX_PATH*2]; snprintf(run_cmd, sizeof(run_cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' save '%s/projects/%s/pieces/document.txt' '%s/%s'", project_root, project_root, project_id, project_root, file_path_input_buffer);
            if (run_command(run_cmd) == 0) success = 1;
        }
        if (success) {
            strncpy(active_file_path, file_path_input_buffer, MAX_PATH - 1);
            char handshake[MAX_PATH*2], pdl_path[MAX_PATH];
            snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", project_root, project_id);
            snprintf(handshake, sizeof(handshake), "'%s/pieces/ops/file-op/+x/sync_pdl_op.+x' '%s' 'STATE' 'active_file' '%s'", project_root, pdl_path, active_file_path);
            run_command(handshake); trigger_state_pulse(); transition_to_layout(return_layout);
        }
    }
}

static int is_active_layout(void) {
    char *path = NULL; if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r"); if (!f) { free(path); return 0; }
    char line[MAX_LINE]; int result = 0; if (fgets(line, sizeof(line), f)) result = (strstr(line, "pieces/ops/file-op/layouts/file_menu.chtpm") != NULL || strstr(line, "pieces/ops/file-op/layouts/file_browser.chtpm") != NULL);
    fclose(f); free(path); return result;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint); signal(SIGTERM, handle_sigint); resolve_paths();
    if (argc > 3) {
        if (strcmp(argv[1], "--test-save") == 0) { strncpy(project_id, argv[2], MAX_LINE-1); strncpy(file_path_input_buffer, argv[3], MAX_PATH-1); browser_mode = 1; handle_command("OK_ACTION"); printf("Test Save Done.\n"); return 0; }
        if (strcmp(argv[1], "--test-load") == 0) { strncpy(project_id, argv[2], MAX_LINE-1); strncpy(file_path_input_buffer, argv[3], MAX_PATH-1); browser_mode = 0; handle_command("OK_ACTION"); printf("Test Load Done.\n"); return 0; }
    }
    load_state(); read_cli_buffers();
    char *hist_path = NULL; asprintf(&hist_path, "%s/pieces/keyboard/history.txt", project_root);
    long last_pos = 0; struct stat st; if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    while (!g_shutdown) {
        if (!is_active_layout()) { usleep(100000); continue; }
        read_cli_buffers(); if (stat(hist_path, &st) == 0 && st.st_size > last_pos) {
            FILE *hf = fopen(hist_path, "r"); if (hf) { fseek(hf, last_pos, SEEK_SET); char line[MAX_LINE]; int processed = 0;
                while (fgets(line, sizeof(line), hf)) { char *cmd = strstr(line, "COMMAND: "); if (cmd) { char *c = trim_str(cmd + 9);
                        if (strncmp(c, "SET_DIR:", 8) == 0) { handle_command(c); processed = 1; }
                        else if (strncmp(c, "SET_INPUT_PATH:", 15) == 0) { handle_command(c); processed = 1; }
                        else if (strncmp(c, "LOAD_FILE:", 10) == 0) { set_file_path_input(c + 10); handle_command("OK_ACTION"); processed = 1; }
                        else if (strcmp(c, "OK_ACTION") == 0) { handle_command(c); processed = 1; }
                    }
                }
                if (processed) { update_gui_state(); trigger_render(); }
                last_pos = ftell(hf); fclose(hf);
            }
        } else if (st.st_size < last_pos) last_pos = 0;
        usleep(16667);
    }
    free(hist_path); return 0;
}
