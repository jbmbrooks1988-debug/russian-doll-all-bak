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

char project_root[MAX_PATH] = ".";
char project_id[MAX_LINE] = "template";
char active_file_path[MAX_PATH] = "none";
char response_buffer[256] = "Ready.";
char current_dir[MAX_PATH] = "projects";
int browser_mode = 0; // 0 = Load, 1 = Save
char search_query_buffer[MAX_LINE] = "";
char file_path_input_buffer[MAX_PATH] = "";
char active_user_name[MAX_LINE] = "none";
char slot_name_buffer[MAX_LINE] = "default";
char return_layout[MAX_PATH] = "pieces/chtpm/layouts/os.chtpm";

static volatile sig_atomic_t g_shutdown = 0;
void handle_sigint(int sig) { (void)sig; g_shutdown = 1; }

static void debug_log(const char *fmt, ...) {
    FILE *f = fopen("agy_manager_debug.txt", "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fprintf(f, "\n");
        fclose(f);
    }
}

int run_command(const char* cmd) {
    debug_log("Executing Op: %s", cmd);
    pid_t pid = fork();
    if (pid == 0) {
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
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line), *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    struct stat st;
                    if (stat(v, &st) == 0 && S_ISDIR(st.st_mode)) {
                        strncpy(project_root, v, sizeof(project_root)-1);
                    } else {
                        strcpy(project_root, ".");
                    }
                }
            }
        }
        fclose(kvp);
    }
}

void resolve_active_user() {
    char session_dir[MAX_PATH];
    snprintf(session_dir, sizeof(session_dir), "%s/projects/user/pieces/session", project_root);
    DIR *dir = opendir(session_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".session") == 0) {
                strncpy(active_user_name, entry->d_name, dot - entry->d_name);
                active_user_name[dot - entry->d_name] = '\0';
                break;
            }
        }
        closedir(dir);
    }
}

void load_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line), *v = trim_str(eq + 1);
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
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/cli_buffers.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 's') strncpy(search_query_buffer, trim_str(line + 1), sizeof(search_query_buffer) - 1);
            else if (line[0] == 'f') strncpy(file_path_input_buffer, trim_str(line + 1), sizeof(file_path_input_buffer) - 1);
            else if (line[0] == 'n') strncpy(slot_name_buffer, trim_str(line + 1), sizeof(slot_name_buffer) - 1);
        }
        fclose(f);
    }
}

void clear_search_query() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "s\n"); fclose(f); }
        free(path);
    }
    search_query_buffer[0] = '\0';
}

void set_file_path_input(const char *val) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "f%s\n", val); fclose(f); }
        free(path);
    }
    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
}

void trigger_render() {
    char pulse[MAX_PATH];
    snprintf(pulse, sizeof(pulse), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(pulse, "a");
    if (f) { fprintf(f, "M\n"); fclose(f); }
}

int compare_names(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int get_digits(int num) {
    if (num >= 100) return 3;
    if (num >= 10) return 2;
    return 1;
}

static void append_aligned_button_attr(char *out, size_t max_sz, const char *label, const char *attr_name, const char *attr_val, int *p_display_num) {
    int num = *p_display_num;
    int digits = get_digits(num);
    int label_len = strlen(label);
    int visual_len = 9 + digits + label_len;
    int padding = 45 - visual_len;
    if (padding < 0) padding = 0;
    char btn_markup[1024];
    snprintf(btn_markup, sizeof(btn_markup), "<text label=\"║  \" /><button label=\"%s\" %s=\"%s\" />", label, attr_name, attr_val);
    strncat(out, btn_markup, max_sz - strlen(out) - 1);
    if (padding > 0) {
        char pad_str[128];
        snprintf(pad_str, sizeof(pad_str), "<text label=\"%.*s\" />", padding, "                                                                                ");
        strncat(out, pad_str, max_sz - strlen(out) - 1);
    }
    strncat(out, "<text label=\" ║\" /><br/>", max_sz - strlen(out) - 1);
    (*p_display_num)++;
}

void build_slot_markup(char* out, size_t sz, int *p_next_num) {
    out[0] = '\0';
    resolve_active_user();
    char saves_dir[MAX_PATH];
    snprintf(saves_dir, sizeof(saves_dir), "%s/projects/user/pieces/profiles/%s/saves/%s", project_root, active_user_name, project_id);
    DIR *dir = opendir(saves_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && strlen(out) < sz - 1000) {
            if (entry->d_name[0] == '.') continue;
            struct stat st;
            char entry_path[MAX_PATH];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", saves_dir, entry->d_name);
            if (stat(entry_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                append_aligned_button_attr(out, sz, entry->d_name, "onClick", entry->d_name, p_next_num);
            }
        }
        closedir(dir);
    } else {
        strcat(out, "<text label=\"║  [No slots found]                       ║\" /><br/>");
    }
}

int find_autocomplete_matches(const char *input, const char *current_browser_dir, char matches[][256], int max_matches) {
    char dir_to_scan[MAX_PATH];
    const char *prefix = "";
    const char *last_slash = strrchr(input, '/');
    if (last_slash) {
        size_t dir_len = last_slash - input;
        strncpy(dir_to_scan, input, dir_len); dir_to_scan[dir_len] = '\0';
        prefix = last_slash + 1;
        if (strlen(dir_to_scan) == 0) strcpy(dir_to_scan, ".");
    } else {
        prefix = input; strcpy(dir_to_scan, current_browser_dir);
    }
    char full_scan_path[MAX_PATH];
    snprintf(full_scan_path, sizeof(full_scan_path), "%s/%s", project_root, dir_to_scan);
    DIR *d = opendir(full_scan_path);
    if (!d) return 0;
    struct dirent *entry;
    int count = 0;
    size_t prefix_len = strlen(prefix);
    while ((entry = readdir(d)) != NULL && count < max_matches) {
        if (entry->d_name[0] == '.') continue;
        if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
            if (last_slash) snprintf(matches[count], 256, "%.*s/%s", (int)(last_slash - input), input, entry->d_name);
            else snprintf(matches[count], 256, "%s", entry->d_name);
            count++;
        }
    }
    closedir(d);
    return count;
}

void build_browser_markup(char* out, size_t sz, int *p_next_num) {
    out[0] = '\0';
    if (strcmp(current_dir, ".") != 0 && strlen(current_dir) > 0) {
        char parent_dir[MAX_PATH];
        char *last_slash = strrchr(current_dir, '/');
        if (last_slash) { strncpy(parent_dir, current_dir, last_slash - current_dir); parent_dir[last_slash - current_dir] = '\0'; }
        else strcpy(parent_dir, ".");
        char action[MAX_PATH + 10]; snprintf(action, sizeof(action), "SET_DIR:%s", parent_dir);
        append_aligned_button_attr(out, sz, "<- BACK", "onClick", action, p_next_num);
    }
    char subdirs[MAX_ITEMS][256], files[MAX_ITEMS][256];
    int subdir_count = 0, file_count = 0;
    char full_current_dir[MAX_PATH];
    snprintf(full_current_dir, sizeof(full_current_dir), "%s/%s", project_root, current_dir);
    DIR *d = opendir(full_current_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strlen(search_query_buffer) > 0 && strcasestr(entry->d_name, search_query_buffer) == NULL) continue;
            char entry_path[MAX_PATH]; snprintf(entry_path, sizeof(entry_path), "%s/%s", full_current_dir, entry->d_name);
            struct stat st;
            if (stat(entry_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) { if (subdir_count < MAX_ITEMS) strncpy(subdirs[subdir_count++], entry->d_name, 255); }
                else { if (file_count < MAX_ITEMS) strncpy(files[file_count++], entry->d_name, 255); }
            }
        }
        closedir(d);
    }
    qsort(subdirs, subdir_count, 256, compare_names);
    qsort(files, file_count, 256, compare_names);
    int items_displayed = 0, limit = 12;
    for (int i = 0; i < subdir_count && items_displayed < limit; i++) {
        char next_dir[MAX_PATH];
        if (strcmp(current_dir, ".") == 0) snprintf(next_dir, sizeof(next_dir), "%s", subdirs[i]);
        else snprintf(next_dir, sizeof(next_dir), "%s/%s", current_dir, subdirs[i]);
        char label[300]; snprintf(label, sizeof(label), "[DIR] %s/", subdirs[i]);
        char action[MAX_PATH + 10]; snprintf(action, sizeof(action), "SET_DIR:%s", next_dir);
        append_aligned_button_attr(out, sz, label, "onClick", action, p_next_num);
        items_displayed++;
    }
    for (int i = 0; i < file_count && items_displayed < limit; i++) {
        char target_file[MAX_PATH];
        if (strcmp(current_dir, ".") == 0) snprintf(target_file, sizeof(target_file), "%s", files[i]);
        else snprintf(target_file, sizeof(target_file), "%s/%s", current_dir, files[i]);
        char full_target[MAX_PATH]; snprintf(full_target, sizeof(full_target), "%s/%s", project_root, target_file);
        struct stat st; long size = 0; if (stat(full_target, &st) == 0) size = st.st_size;
        char label[300]; snprintf(label, sizeof(label), "[FIL] %s (%ldB)", files[i], size);
        char action[MAX_PATH + 20];
        if (browser_mode == 0) snprintf(action, sizeof(action), "LOAD_FILE:%s", target_file);
        else snprintf(action, sizeof(action), "SET_INPUT_PATH:%s", target_file);
        append_aligned_button_attr(out, sz, label, "onClick", action, p_next_num);
        items_displayed++;
    }
    if (items_displayed == 0) strcat(out, "<text label=\"║  [No matching items found]                  ║\" /><br/>");
}

void update_gui_state() {
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f = fopen(state_path, "w"); 
    if (f) {
        fprintf(f, "module_path=pieces/ops/file-op/+x/file_manager.+x\n");
        fprintf(f, "app_title=GLOBAL FILE MANAGER\n");
        fprintf(f, "project_id=%s\n", project_id);
        fprintf(f, "active_project=%s\n", active_file_path);
        fprintf(f, "active_file=%s\n", active_file_path);
        fprintf(f, "return_layout=%s\n", return_layout);
        fprintf(f, "browser_mode=%d\n", browser_mode);
        fprintf(f, "browser_current_dir=%s\n", current_dir);
        fprintf(f, "active_file_info_line=<text label=\"║  FILE: %-34.34s ║\" /><br/>\n", active_file_path);
        fprintf(f, "editor_response_line=║  %-40.40s ║\n", response_buffer);
        fprintf(f, "editor_response=[RESP]: %-49s\n", response_buffer);
        fprintf(f, "browser_mode_header=<text label=\"║  MODE: %-34.34s ║\" /><br/>\n", browser_mode == 0 ? "LOAD FILE" : "SAVE FILE AS");
        fprintf(f, "browser_current_dir_line=<text label=\"║  DIR: %-35.35s ║\" /><br/>\n", current_dir);
        fprintf(f, "search_query_val=%s\n", search_query_buffer);
        fprintf(f, "file_path_input_val=%s\n", file_path_input_buffer);
        int next_num = 1;
        char slot_markup[16384] = ""; build_slot_markup(slot_markup, sizeof(slot_markup), &next_num);
        fprintf(f, "slot_selection_markup=%s\n", slot_markup);
        next_num = 3;
        char suggestions_markup[8192] = "";
        char matches[4][256];
        int num_matches = find_autocomplete_matches(file_path_input_buffer, current_dir, matches, 4);
        if (num_matches > 0) {
            strcat(suggestions_markup, "<text label=\"║  SUGGESTIONS:                               ║\" /><br/>");
            for (int i = 0; i < num_matches; i++) {
                char action[512]; snprintf(action, sizeof(action), "SET_INPUT_PATH:%s", matches[i]);
                append_aligned_button_attr(suggestions_markup, sizeof(suggestions_markup), matches[i], "onClick", action, &next_num);
            }
        } else strcat(suggestions_markup, "<text label=\"║  (No suggestions)                          ║\" /><br/>");
        fprintf(f, "autocomplete_suggestions_markup=%s\n", suggestions_markup);
        char browser_markup[32768] = ""; build_browser_markup(browser_markup, sizeof(browser_markup), &next_num);
        fprintf(f, "directory_browser_markup=%s\n", browser_markup);
        char actions_markup[8192] = "";
        append_aligned_button_attr(actions_markup, sizeof(actions_markup), browser_mode == 0 ? "LOAD FILE" : "SAVE FILE", "onClick", "OK_ACTION", &next_num);
        append_aligned_button_attr(actions_markup, sizeof(actions_markup), "CANCEL", "href", return_layout, &next_num);
        fprintf(f, "browser_action_buttons_markup=%s\n", actions_markup);
        fclose(f);
    }
}

int main() {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    resolve_paths();
    load_state();
    read_cli_buffers();
    update_gui_state();
    trigger_render();
    char *hist_path = NULL; asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root);
    long last_pos = 0; struct stat st; if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    int processed = 0;
                    while (fgets(line, sizeof(line), hf)) {
                        read_cli_buffers();
                        char *cmd = strstr(line, "COMMAND: ");
                        if (cmd) {
                            char *c = trim_str(cmd + 9);
                            if (strncmp(c, "SET_DIR:", 8) == 0) { strncpy(current_dir, c + 8, MAX_PATH - 1); clear_search_query(); processed = 1; }
                            else if (strncmp(c, "SET_INPUT_PATH:", 15) == 0) { set_file_path_input(c + 15); processed = 1; }
                            else if (strncmp(c, "LOAD_FILE:", 10) == 0) {
                                set_file_path_input(c + 10);
                                if (strcmp(project_id, "op-ed") == 0) {
                                     snprintf(response_buffer, sizeof(response_buffer), "Loading Map: %s", c + 10);
                                     char *sc = NULL;
                                     asprintf(&sc, "'%s/pieces/ops/file-op/+x/sync_op.+x' '%s/pieces/apps/player_app/manager/state.txt' 'current_map' '%s'", project_root, project_root, c + 10);
                                     run_command(sc); free(sc);
                                } else {
                                     snprintf(response_buffer, sizeof(response_buffer), "Loading File: %s", c + 10);
                                     char run_cmd[MAX_PATH];
                                     snprintf(run_cmd, sizeof(run_cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' load '%s/%s' '%s/projects/%s/pieces/document.txt'", project_root, project_root, c + 10, project_root, project_id);
                                     run_command(run_cmd);
                                }
                                strncpy(active_file_path, c + 10, MAX_PATH - 1);
                                processed = 1;
                            } else if (strcmp(c, "OK_ACTION") == 0) {
                                if (browser_mode == 0) {
                                    if (strcmp(project_id, "op-ed") == 0) {
                                        snprintf(response_buffer, sizeof(response_buffer), "Loaded Map: %s", file_path_input_buffer);
                                        char *sc = NULL;
                                        asprintf(&sc, "'%s/pieces/ops/file-op/+x/sync_op.+x' '%s/pieces/apps/player_app/manager/state.txt' 'current_map' '%s'", project_root, project_root, file_path_input_buffer);
                                        run_command(sc); free(sc);
                                    } else {
                                        snprintf(response_buffer, sizeof(response_buffer), "Loaded %s", file_path_input_buffer);
                                        char run_cmd[MAX_PATH];
                                        snprintf(run_cmd, sizeof(run_cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' load '%s/%s' '%s/projects/%s/pieces/document.txt'", project_root, project_root, file_path_input_buffer, project_root, project_id);
                                        run_command(run_cmd);
                                    }
                                } else {
                                    if (strcmp(project_id, "op-ed") == 0) {
                                        snprintf(response_buffer, sizeof(response_buffer), "Saved Map: %s", file_path_input_buffer);
                                        // TODO: Actual map save op if needed
                                    } else {
                                        snprintf(response_buffer, sizeof(response_buffer), "Saved to %s", file_path_input_buffer);
                                        char run_cmd[MAX_PATH];
                                        snprintf(run_cmd, sizeof(run_cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' save '%s/projects/%s/pieces/document.txt' '%s/%s'", project_root, project_root, project_id, project_root, file_path_input_buffer);
                                        run_command(run_cmd);
                                    }
                                }
                                strncpy(active_file_path, file_path_input_buffer, MAX_PATH - 1);
                                processed = 1;
                            } else if (strncmp(c, "SET_SLOT:", 9) == 0) {
                                strncpy(slot_name_buffer, c + 9, sizeof(slot_name_buffer)-1);
                                snprintf(response_buffer, sizeof(response_buffer), "Loading %s...", slot_name_buffer);
                                char run_cmd[MAX_PATH];
                                snprintf(run_cmd, sizeof(run_cmd), "'%s/pieces/ops/file-op/+x/project_file_op.+x' load %s %s", project_root, project_id, slot_name_buffer);
                                run_command(run_cmd);
                                processed = 1;
                            }
                        }
                        if (strstr(line, "KEY_PRESSED: ")) {
                            int key = atoi(strstr(line, "KEY_PRESSED: ") + 13);
                            if (key == '7') { // Save
                                if (strcmp(project_id, "agy-text-editor") == 0) {
                                    snprintf(response_buffer, sizeof(response_buffer), "Saved File: %s", active_file_path);
                                    char run_cmd[MAX_PATH];
                                    snprintf(run_cmd, sizeof(run_cmd), "'%s/pieces/ops/file-op/+x/file_op.+x' save '%s/projects/%s/pieces/document.txt' '%s/%s'", project_root, project_root, project_id, project_root, active_file_path);
                                    run_command(run_cmd);
                                } else {
                                    snprintf(response_buffer, sizeof(response_buffer), "Saved Project: %s", project_id);
                                    char run_cmd[MAX_PATH];
                                    snprintf(run_cmd, sizeof(run_cmd), "'%s/pieces/ops/file-op/+x/project_file_op.+x' save %s default", project_root, project_id);
                                    run_command(run_cmd);
                                }
                                processed = 1;
                            }
                        }
                    }
                    if (processed) { update_gui_state(); trigger_render(); }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    free(hist_path); return 0;
}
