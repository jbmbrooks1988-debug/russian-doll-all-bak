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
static char file_path_input_buffer[MAX_PATH] = "projects/agy-text-editor/pieces/document.txt";
static char search_query_buffer[MAX_LINE] = "";
static char current_dir[MAX_PATH] = "projects/agy-text-editor";
static int browser_mode = 0; // 0 = Load, 1 = Save As

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

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int run_command(const char* cmd) {
    debug_log("Executing Op: %s", cmd);
    pid_t pid = fork();
    if (pid == 0) {
        // Redirect stdout/stderr to a temporary file to avoid cluttering but allow debugging if needed
        // For now, let's just let it go to the parent's stderr which we can't easily see but won't crash
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        debug_log("Op exited with code: %d", exit_code);
        return exit_code;
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
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    struct stat st;
                    if (stat(v, &st) == 0 && S_ISDIR(st.st_mode)) {
                        snprintf(project_root, sizeof(project_root), "%s", v);
                        debug_log("Resolved project_root to: %s", project_root);
                    } else {
                        strcpy(project_root, ".");
                        debug_log("Invalid project_root in KVP, falling back to '.'");
                    }
                }
            }
        }
        fclose(kvp);
    } else {
        debug_log("Could not open location_kvp, using default project_root: %s", project_root);
    }
}

static int is_active_layout(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    int result = 0;
    if (fgets(line, sizeof(line), f)) {
        result = (strstr(line, "editor.chtpm") != NULL ||
                  strstr(line, "file_menu.chtpm") != NULL ||
                  strstr(line, "file_browser.chtpm") != NULL ||
                  strstr(line, "load_slot.chtpm") != NULL);
    }
    fclose(f);
    free(path);
    return result;
}

static void get_current_layout_name(char *buf, size_t sz) {
    buf[0] = '\0';
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        if (fgets(line, sizeof(line), f)) {
            char *trimmed = trim_str(line);
            char *last_slash = strrchr(trimmed, '/');
            if (last_slash) {
                strncpy(buf, last_slash + 1, sz - 1);
            } else {
                strncpy(buf, trimmed, sz - 1);
            }
            buf[sz - 1] = '\0';
        }
        fclose(f);
    }
    free(path);
}

static int get_active_gui_index(void) {
    char *path = NULL;
    int idx = 0;
    if (asprintf(&path, "%s/pieces/display/active_gui_index.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d", &idx) != 1) {
            idx = 0;
        }
        fclose(f);
    }
    free(path);
    return idx;
}


static void read_editor_line(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 'e') {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                strncpy(input_line_buffer, line + 1, sizeof(input_line_buffer) - 1);
            }
        }
        fclose(f);
    }
    free(path);
}

static void read_file_path_input(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 'f') {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                char *val = trim_str(line + 1);
                // If value is empty, only overwrite if we don't have a previous value
                // This helps persist the name when Enter clears the parser side buffer
                if (strlen(val) > 0 || strlen(file_path_input_buffer) == 0) {
                    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
                }
            }
        }
        fclose(f);
    }
    free(path);
}

static void set_file_path_input(const char *val) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "f%s\n", val);
            fclose(f);
        }
        free(path);
    }
    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
}

static void clear_editor_line(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "e\n");
            fclose(f);
        }
        free(path);
    }
    input_line_buffer[0] = '\0';
}

static void read_search_query_input(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 's') {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                strncpy(search_query_buffer, trim_str(line + 1), sizeof(search_query_buffer) - 1);
            }
        }
        fclose(f);
    }
    free(path);
}

static void clear_search_query(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "s\n");
            fclose(f);
        }
        free(path);
    }
    search_query_buffer[0] = '\0';
}

static void trigger_render(void) {
    char pulse[MAX_PATH];
    // 1. Standard CHTPM Frame pulse
    snprintf(pulse, sizeof(pulse), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(pulse, "a");
    if (f) { fprintf(f, "M\n"); fclose(f); }
    
    // 2. Player App State pulse (Fuzz-op style)
    snprintf(pulse, sizeof(pulse), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    f = fopen(pulse, "a");
    if (f) { fprintf(f, "S\n"); fclose(f); }
    
    debug_log("Render triggered (Markers hit)");
}

static void transition_to_layout(const char *layout_path) {
    char *lp = NULL;
    if (asprintf(&lp, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
        FILE *f = fopen(lp, "a");
        if (f) {
            fprintf(f, "%s\n", layout_path);
            fclose(f);
        }
        free(lp);
    }
}

static void save_project_metadata(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/agy-text-editor/project.pdl", project_root);
    
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | project_id         | agy-text-editor\n");
        fprintf(f, "META         | version            | 1.0\n");
        fprintf(f, "META         | entry_layout       | projects/agy-text-editor/layouts/editor.chtpm\n\n");
        
        fprintf(f, "STATE        | active_file        | %s\n", active_file_path);
        fprintf(f, "STATE        | cursor_x           | %d\n", cursor_x);
        fprintf(f, "STATE        | cursor_y           | %d\n", cursor_y);
        
        fprintf(f, "RESPONSE     | default            | %s\n", response_buffer);
        fclose(f);
        debug_log("Project metadata saved to %s", path);
    }
}

static void load_project_metadata(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/agy-text-editor/project.pdl", project_root);
    
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *pipe1 = strchr(line, '|');
            if (!pipe1) continue;
            char *pipe2 = strchr(pipe1 + 1, '|');
            if (!pipe2) continue;
            
            *pipe2 = '\0';
            char *key = trim_str(pipe1 + 1);
            char *val = trim_str(pipe2 + 1);
            
            if (strcmp(key, "active_file") == 0) {
                strncpy(active_file_path, val, sizeof(active_file_path) - 1);
            } else if (strcmp(key, "cursor_x") == 0) {
                cursor_x = atoi(val);
            } else if (strcmp(key, "cursor_y") == 0) {
                cursor_y = atoi(val);
            }
        }
        fclose(f);
        debug_log("Project metadata loaded from %s", path);
    }
}

static void sync_system_state(void) {
    char cmd[MAX_PATH * 2];
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    
    // Sync project_id
    snprintf(cmd, sizeof(cmd), "'%s/projects/agy-text-editor/manager/ops/sync_state_op.+x' '%s' 'project_id' 'agy-text-editor'", project_root, state_path);
    run_command(cmd);

    // Sync active_project
    snprintf(cmd, sizeof(cmd), "'%s/projects/agy-text-editor/manager/ops/sync_state_op.+x' '%s' 'active_project' '%s'", project_root, state_path, active_file_path);
    run_command(cmd);

    // Sync current_map (for Parser compatibility)
    snprintf(cmd, sizeof(cmd), "'%s/projects/agy-text-editor/manager/ops/sync_state_op.+x' '%s' 'current_map' '%s'", project_root, state_path, active_file_path);
    run_command(cmd);
}

static int save_to_path(const char *rel_path) {
    char full_path[MAX_PATH];
    if (rel_path[0] == '/' || strstr(rel_path, "projects/") == rel_path) {
        snprintf(full_path, sizeof(full_path), "%s/%s", project_root, rel_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s/%s", project_root, current_dir, rel_path);
    }
    
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "'%s/projects/agy-text-editor/manager/ops/save_file_op.+x' '%s' '%s'", project_root, project_root, full_path);
    int res = run_command(cmd);
    
    if (res == 0) {
        if (rel_path[0] == '/' || strstr(rel_path, "projects/") == rel_path) {
            strncpy(active_file_path, rel_path, sizeof(active_file_path) - 1);
        } else {
            snprintf(active_file_path, sizeof(active_file_path), "%s/%s", current_dir, rel_path);
        }
        snprintf(response_buffer, sizeof(response_buffer), "Saved to %s", active_file_path);
        save_project_metadata();
        return 0;
    } else {
        snprintf(response_buffer, sizeof(response_buffer), "Error: Save Op failed (%d)", res);
        return -1;
    }
}

static int load_from_path(const char *rel_path) {
    char full_path[MAX_PATH];
    if (rel_path[0] == '/' || strstr(rel_path, "projects/") == rel_path) {
        snprintf(full_path, sizeof(full_path), "%s/%s", project_root, rel_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s/%s", project_root, current_dir, rel_path);
    }
    
    char doc_path[MAX_PATH], pdl_path[MAX_PATH];
    snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/agy-text-editor/project.pdl", project_root);

    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "'%s/projects/agy-text-editor/manager/ops/load_file_op.+x' '%s' '%s' '%s'", project_root, full_path, doc_path, pdl_path);
    int res = run_command(cmd);
    
    if (res == 0) {
        if (rel_path[0] == '/' || strstr(rel_path, "projects/") == rel_path) {
            strncpy(active_file_path, rel_path, sizeof(active_file_path) - 1);
        } else {
            snprintf(active_file_path, sizeof(active_file_path), "%s/%s", current_dir, rel_path);
        }
        
        snprintf(response_buffer, sizeof(response_buffer), "Loaded %s", active_file_path);
        
        load_document_to_buffer();
        cursor_x = 0;
        cursor_y = 0;
        
        sync_system_state();
        transition_to_layout("projects/agy-text-editor/layouts/editor.chtpm");
        return 0;
    } else {
        snprintf(response_buffer, sizeof(response_buffer), "Error: Load Op failed (%d)", res);
        return -1;
    }
}

static int compare_names(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static int find_autocomplete_matches(const char *input, const char *current_browser_dir, char matches[][256], int max_matches) {
    char dir_to_scan[MAX_PATH];
    const char *prefix = "";
    
    const char *last_slash = strrchr(input, '/');
    if (last_slash) {
        size_t dir_len = last_slash - input;
        if (dir_len >= sizeof(dir_to_scan)) dir_len = sizeof(dir_to_scan) - 1;
        strncpy(dir_to_scan, input, dir_len);
        dir_to_scan[dir_len] = '\0';
        prefix = last_slash + 1;
        if (strlen(dir_to_scan) == 0) {
            strcpy(dir_to_scan, ".");
        }
    } else {
        prefix = input;
        strcpy(dir_to_scan, current_browser_dir);
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
            char entry_path[MAX_PATH];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", full_scan_path, entry->d_name);
            struct stat st;
            int is_dir = 0;
            if (stat(entry_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                is_dir = 1;
            }
            
            if (last_slash) {
                snprintf(matches[count], 256, "%.*s/%s%s", (int)(last_slash - input), input, entry->d_name, is_dir ? "/" : "");
            } else {
                snprintf(matches[count], 256, "%s%s", entry->d_name, is_dir ? "/" : "");
            }
            count++;
        }
    }
    closedir(d);
    return count;
}

static int get_digits(int num) {
    if (num >= 100) return 3;
    if (num >= 10) return 2;
    return 1;
}

static void append_aligned_button_attr(char *out, size_t max_sz, const char *label, const char *attr_name, const char *attr_val, int *p_display_num) {
    int num = *p_display_num;
    int digits = get_digits(num);
    int label_len = strlen(label);
    
    int visual_len = 8 + digits + label_len;
    int padding = 40 - visual_len;
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

static void handle_interact_key(int key) {
    debug_log("Interact key: %d, current pos: (%d, %d), total_lines: %d", key, cursor_x, cursor_y, total_lines);
    int modified = 0;
    if (key == 'w' || key == 'W' || key == 1002) {
        if (cursor_y > 0) cursor_y--;
        int len = strlen(document_lines[cursor_y]);
        if (cursor_x > len) cursor_x = len;
    } else if (key == 's' || key == 'S' || key == 1003) {
        if (cursor_y < 98) {
            if (cursor_y >= total_lines - 1) {
                strcpy(document_lines[total_lines], "");
                total_lines++;
                modified = 1;
            }
            cursor_y++;
            int len = strlen(document_lines[cursor_y]);
            if (cursor_x > len) cursor_x = len;
        }
    } else if (key == 'a' || key == 'A' || key == 1000) {
        if (cursor_x > 0) cursor_x--;
    } else if (key == 'd' || key == 'D' || key == 1001) {
        int len = strlen(document_lines[cursor_y]);
        if (cursor_x < len) {
            cursor_x++;
        } else if (len < MAX_LINE - 2) {
            // Pad with space to move right
            document_lines[cursor_y][len] = ' ';
            document_lines[cursor_y][len + 1] = '\0';
            cursor_x++;
            modified = 1;
        }
    } else if (key == 127 || key == 8) {
        // Backspace
        if (cursor_x > 0) {
            char *line = document_lines[cursor_y];
            memmove(line + cursor_x - 1, line + cursor_x, strlen(line + cursor_x) + 1);
            cursor_x--;
            modified = 1;
        } else if (cursor_y > 0) {
            // Merge with previous line
            int prev_len = strlen(document_lines[cursor_y-1]);
            if (prev_len + strlen(document_lines[cursor_y]) < MAX_LINE - 1) {
                strcat(document_lines[cursor_y-1], document_lines[cursor_y]);
                for (int i = cursor_y; i < total_lines - 1; i++) {
                    strcpy(document_lines[i], document_lines[i+1]);
                }
                total_lines--;
                cursor_y--;
                cursor_x = prev_len;
                modified = 1;
            }
        }
    } else if (key == 10 || key == 13) {
        // New line
        if (total_lines < 99) {
            for (int i = total_lines; i > cursor_y + 1; i--) {
                strcpy(document_lines[i], document_lines[i-1]);
            }
            strcpy(document_lines[cursor_y + 1], document_lines[cursor_y] + cursor_x);
            document_lines[cursor_y][cursor_x] = '\0';
            total_lines++;
            cursor_y++;
            cursor_x = 0;
            modified = 1;
        }
    } else if (key >= 32 && key <= 126) {
        // Printable character
        char *line = document_lines[cursor_y];
        int len = strlen(line);
        if (len < MAX_LINE - 2) {
            if (cursor_x > len) {
                // Pad with spaces if cursor was moved right past end
                for (int i = len; i < cursor_x; i++) line[i] = ' ';
                line[cursor_x] = '\0';
                len = cursor_x;
            }
            memmove(line + cursor_x + 1, line + cursor_x, len - cursor_x + 1);
            line[cursor_x] = (char)key;
            cursor_x++;
            modified = 1;
        }
    }
    debug_log("New pos: (%d, %d), total_lines: %d", cursor_x, cursor_y, total_lines);
    if (modified) save_buffer_to_document();
}

static int process_key(int key) {
    debug_log("Processing key: %d", key);
    int processed = 0;
    
    char layout[MAX_LINE];
    get_current_layout_name(layout, sizeof(layout));
    
    // Check for standard menu keys first (1, 2, 6, 7, 8, 9) regardless of layout
    // as long as it's one of our text editor layouts
    if (strcmp(layout, "editor.chtpm") == 0 || 
        strcmp(layout, "file_menu.chtpm") == 0 || 
        strcmp(layout, "file_browser.chtpm") == 0) {
        
        if (key == '1') {
            // Append Line
            read_editor_line();
            char doc_path[MAX_PATH];
            snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
            FILE *f = fopen(doc_path, "a");
            if (f) {
                fprintf(f, "%s\n", input_line_buffer);
                fclose(f);
                strcpy(response_buffer, "Line appended.");
                load_document_to_buffer(); // Reload into memory buffer
            } else {
                strcpy(response_buffer, "Error writing file.");
            }
            clear_editor_line();
            return 1;
        } else if (key == '2') {
            // Clear file
            char doc_path[MAX_PATH];
            snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
            FILE *f = fopen(doc_path, "w");
            if (f) {
                fclose(f);
                strcpy(response_buffer, "File cleared.");
                load_document_to_buffer(); // Reload into memory buffer
            } else {
                strcpy(response_buffer, "Error clearing file.");
            }
            return 1;
        } else if (key == '6') {
            // New File
            char doc_path[MAX_PATH];
            snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
            FILE *f = fopen(doc_path, "w");
            if (f) {
                fclose(f);
                strcpy(active_file_path, "none");
                strcpy(response_buffer, "New file created (buffer cleared).");
                load_document_to_buffer(); // Reload into memory buffer
                cursor_x = 0; cursor_y = 0;
                save_project_metadata();
            } else {
                strcpy(response_buffer, "Error creating new file.");
            }
            return 1;
        } else if (key == '7') {
            // Save File (to active path)
            if (strcmp(active_file_path, "none") == 0 || strlen(active_file_path) == 0) {
                browser_mode = 1;
                clear_search_query();
                set_file_path_input("projects/agy-text-editor/pieces/document.txt");
                transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm");
                strcpy(response_buffer, "Specify Save-As path.");
            } else {
                save_to_path(active_file_path);
            }
            return 1;
        } else if (key == '8') {
            // Save As...
            browser_mode = 1;
            clear_search_query();
            set_file_path_input(active_file_path);
            transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm");
            strcpy(response_buffer, "Enter Save-As path.");
            return 1;
        } else if (key == '9') {
            // Load File...
            browser_mode = 0;
            clear_search_query();
            set_file_path_input("");
            transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm");
            strcpy(response_buffer, "Select file to load.");
            return 1;
        }
    }

    if (strcmp(layout, "editor.chtpm") == 0) {
         handle_interact_key(key);
         return 1;
    }

    if (key == 127 || key == 8) {
        if (strcmp(layout, "file_browser.chtpm") == 0) {
            int active_idx = get_active_gui_index();
            if (active_idx == 1) { // search_query
                read_search_query_input();
                int len = strlen(search_query_buffer);
                if (len > 0) {
                    search_query_buffer[len - 1] = '\0';
                    char *path = NULL;
                    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
                        FILE *bf = fopen(path, "a");
                        if (bf) {
                            fprintf(bf, "s%s\n", search_query_buffer);
                            fclose(bf);
                        }
                        free(path);
                    }
                    processed = 1;
                }
            } else if (active_idx == 2) { // file_path_input
                read_file_path_input();
                int len = strlen(file_path_input_buffer);
                if (len > 0) {
                    file_path_input_buffer[len - 1] = '\0';
                    char *path = NULL;
                    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
                        FILE *bf = fopen(path, "a");
                        if (bf) {
                            fprintf(bf, "f%s\n", file_path_input_buffer);
                            fclose(bf);
                        }
                        free(path);
                    }
                    processed = 1;
                }
            }
        }
    } else if (key == 10 || key == 13) {
        // Enter or CR pressed
        if (strcmp(layout, "file_browser.chtpm") == 0) {
            int active_idx = get_active_gui_index();
            debug_log("Enter pressed in browser, active_idx: %d", active_idx);
            if (active_idx == 2) { // file_path_input
                read_file_path_input();
                if (strlen(file_path_input_buffer) > 0) {
                    debug_log("Auto-action on Enter for path: %s", file_path_input_buffer);
                    if (browser_mode == 0) handle_command("LOAD_ACTION");
                    else handle_command("SAVE_AS_ACTION");
                    processed = 1;
                }
            } else if (active_idx == 1) { // search_query
                read_search_query_input();
                debug_log("Search query submitted: %s", search_query_buffer);
                processed = 1;
            }
        }
    }
    return processed;
}

static void handle_command(const char *cmd) {
    debug_log("Handling command: [%s]", cmd);
    if (strncmp(cmd, "SET_DIR:", 8) == 0) {
        strncpy(current_dir, cmd + 8, sizeof(current_dir) - 1);
        if (strlen(current_dir) == 0) strcpy(current_dir, ".");
        clear_search_query();
    } else if (strncmp(cmd, "SET_INPUT_PATH:", 15) == 0) {
        set_file_path_input(cmd + 15);
    } else if (strncmp(cmd, "AUTOCOMPLETE:", 13) == 0) {
        set_file_path_input(cmd + 13);
    } else if (strncmp(cmd, "LOAD_FILE:", 10) == 0) {
        debug_log("LOAD_FILE triggered for: %s", cmd + 10);
        set_file_path_input(cmd + 10);
        load_from_path(cmd + 10);
    } else if (strcmp(cmd, "LOAD_ACTION") == 0) {
        read_file_path_input();
        debug_log("LOAD_ACTION triggered, path buffer: %s", file_path_input_buffer);
        load_from_path(file_path_input_buffer);
    } else if (strcmp(cmd, "SAVE_AS_ACTION") == 0) {
        read_file_path_input();
        debug_log("SAVE_AS_ACTION triggered, path buffer: %s", file_path_input_buffer);
        save_to_path(file_path_input_buffer);
    } else if (strcmp(cmd, "OP:FILE_MENU") == 0) {
        sync_system_state();
        char state_path[MAX_PATH];
        snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
        char sc[MAX_PATH * 2];
        snprintf(sc, sizeof(sc), "'%s/pieces/ops/file-op/+x/sync_op.+x' '%s' 'browser_current_dir' 'projects/agy-text-editor'", project_root, state_path);
        run_command(sc);
        snprintf(sc, sizeof(sc), "'%s/pieces/ops/file-op/+x/sync_op.+x' '%s' 'return_layout' 'projects/agy-text-editor/layouts/editor.chtpm'", project_root, state_path);
        run_command(sc);
        transition_to_layout("pieces/ops/file-op/layouts/file_menu.chtpm");
    }
}

static void load_document_to_buffer(void) {
    char doc_path[MAX_PATH];
    snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
    FILE *f = fopen(doc_path, "r");
    total_lines = 0;
    if (f) {
        while (fgets(document_lines[total_lines], MAX_LINE, f) && total_lines < 99) {
            char *nl = strchr(document_lines[total_lines], '\n');
            if (nl) *nl = '\0';
            nl = strchr(document_lines[total_lines], '\r');
            if (nl) *nl = '\0';
            total_lines++;
        }
        fclose(f);
    }
    if (total_lines == 0) {
        strcpy(document_lines[0], "");
        total_lines = 1;
    }
}

static void save_buffer_to_document(void) {
    char doc_path[MAX_PATH];
    snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
    FILE *f = fopen(doc_path, "w");
    if (f) {
        for (int i = 0; i < total_lines; i++) {
            fprintf(f, "%s\n", document_lines[i]);
        }
        fclose(f);
    }
}

static void build_editor_map(char *out, size_t max_sz) {
    out[0] = '\0';
    
    // Vertical Camera
    int view_height = 8;
    int view_start_y = cursor_y - (view_height / 2);
    if (view_start_y < 0) view_start_y = 0;
    if (view_start_y + view_height > total_lines && total_lines > view_height) 
        view_start_y = total_lines - view_height;
    
    // Horizontal Camera
    int view_width = 40;
    int view_start_x = cursor_x - (view_width / 2);
    if (view_start_x < 0) view_start_x = 0;

    for (int i = view_start_y; i < view_start_y + view_height; i++) {
        char line_buf[MAX_LINE];
        if (i < total_lines) {
            strncpy(line_buf, document_lines[i], MAX_LINE - 1);
        } else {
            strcpy(line_buf, "");
        }
        
        char formatted[MAX_LINE + 512];
        char final_line[MAX_LINE + 64];
        
        int len = strlen(line_buf);
        if (i == cursor_y) {
            if (cursor_x > len) cursor_x = len;
            
            // Render line with [X] cursor
            char before[MAX_LINE], after[MAX_LINE];
            strncpy(before, line_buf, cursor_x);
            before[cursor_x] = '\0';
            strcpy(after, line_buf + cursor_x);
            
            snprintf(final_line, sizeof(final_line), "%s[X]%s", before, after);
        } else {
            strcpy(final_line, line_buf);
        }
        
        // Apply horizontal scrolling
        char scrolled_line[view_width + 1];
        int final_len = strlen(final_line);
        if (view_start_x < final_len) {
            strncpy(scrolled_line, final_line + view_start_x, view_width);
            scrolled_line[view_width] = '\0';
        } else {
            strcpy(scrolled_line, "");
        }
        
        snprintf(formatted, sizeof(formatted), "<text label=\"║  %-40.40s ║\" /><br/>", scrolled_line);
        strncat(out, formatted, max_sz - strlen(out) - 1);
    }
}

static void update_gui_state(void) {
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/projects/agy-text-editor/manager/gui_state.txt", project_root);
    
    char editor_map[8192] = "";
    build_editor_map(editor_map, sizeof(editor_map));
    
    read_editor_line();
    read_file_path_input();
    read_search_query_input();
    
    // Build browser mode header
    char browser_mode_header_val[256];
    if (browser_mode == 0) {
        strcpy(browser_mode_header_val, "<text label=\"║  MODE: LOAD FILE                            ║\" /><br/>");
    } else {
        strcpy(browser_mode_header_val, "<text label=\"║  MODE: SAVE FILE AS                         ║\" /><br/>");
    }
    
    // Build current directory line
    char browser_current_dir_line_val[512];
    snprintf(browser_current_dir_line_val, sizeof(browser_current_dir_line_val), 
             "<text label=\"║  DIR: %-35.35s ║\" /><br/>", current_dir);
             
    // Build autocomplete suggestions
    char autocomplete_markup[8192] = "";
    char suggestions[4][256];
    int num_suggestions = find_autocomplete_matches(file_path_input_buffer, current_dir, suggestions, 4);
    
    int next_display_num = 3; // search_query is 1, file_path_input is 2
    
    if (num_suggestions > 0) {
        strcat(autocomplete_markup, "<text label=\"║  SUGGESTIONS:                               ║\" /><br/>");
        for (int i = 0; i < num_suggestions; i++) {
            char display_label[256];
            if (strlen(suggestions[i]) > 28) {
                snprintf(display_label, sizeof(display_label), "...%s", suggestions[i] + strlen(suggestions[i]) - 25);
            } else {
                strcpy(display_label, suggestions[i]);
            }
            char action[512];
            snprintf(action, sizeof(action), "AUTOCOMPLETE:%s", suggestions[i]);
            append_aligned_button_attr(autocomplete_markup, sizeof(autocomplete_markup), display_label, "onClick", action, &next_display_num);
        }
    } else {
        strcat(autocomplete_markup, "<text label=\"║  (Type to see autocompletions)              ║\" /><br/>");
    }
    
    // Build directory browser tree
    char browser_markup[8192] = "";
    
    if (strcmp(current_dir, ".") != 0 && strlen(current_dir) > 0) {
        char parent_dir[MAX_PATH];
        char *last_slash = strrchr(current_dir, '/');
        if (last_slash) {
            strncpy(parent_dir, current_dir, last_slash - current_dir);
            parent_dir[last_slash - current_dir] = '\0';
        } else {
            strcpy(parent_dir, ".");
        }
        char action[MAX_PATH + 10];
        snprintf(action, sizeof(action), "SET_DIR:%s", parent_dir);
        append_aligned_button_attr(browser_markup, sizeof(browser_markup), "<- BACK", "onClick", action, &next_display_num);
    }
    
    char subdirs[256][256];
    char files[256][256];
    int subdir_count = 0;
    int file_count = 0;
    
    char full_current_dir[MAX_PATH];
    snprintf(full_current_dir, sizeof(full_current_dir), "%s/%s", project_root, current_dir);
    
    DIR *d = opendir(full_current_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            
            // Replicate groq-ollama directory search filter
            if (strlen(search_query_buffer) > 0 && strcasestr(entry->d_name, search_query_buffer) == NULL) {
                continue;
            }
            
            char entry_path[MAX_PATH];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", full_current_dir, entry->d_name);
            struct stat st;
            if (stat(entry_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    if (subdir_count < 256) {
                        strncpy(subdirs[subdir_count++], entry->d_name, 255);
                    }
                } else {
                    if (file_count < 256) {
                        strncpy(files[file_count++], entry->d_name, 255);
                    }
                }
            }
        }
        closedir(d);
    }
    
    qsort(subdirs, subdir_count, 256, compare_names);
    qsort(files, file_count, 256, compare_names);
    
    int items_displayed = 0;
    int limit = 8;
    
    for (int i = 0; i < subdir_count && items_displayed < limit; i++) {
        char next_dir_path[MAX_PATH];
        if (strcmp(current_dir, ".") == 0) {
            snprintf(next_dir_path, sizeof(next_dir_path), "%s", subdirs[i]);
        } else {
            snprintf(next_dir_path, sizeof(next_dir_path), "%s/%s", current_dir, subdirs[i]);
        }
        
        char btn_label[300];
        snprintf(btn_label, sizeof(btn_label), "[DIR] %s/", subdirs[i]);
        
        char display_label[256];
        if (strlen(btn_label) > 28) {
            snprintf(display_label, sizeof(display_label), "...%s", btn_label + strlen(btn_label) - 25);
        } else {
            strcpy(display_label, btn_label);
        }
        
        char action[MAX_PATH + 10];
        snprintf(action, sizeof(action), "SET_DIR:%s", next_dir_path);
        
        append_aligned_button_attr(browser_markup, sizeof(browser_markup), display_label, "onClick", action, &next_display_num);
        items_displayed++;
    }
    
    for (int i = 0; i < file_count && items_displayed < limit; i++) {
        char target_file_path[MAX_PATH];
        if (strcmp(current_dir, ".") == 0) {
            snprintf(target_file_path, sizeof(target_file_path), "%s", files[i]);
        } else {
            snprintf(target_file_path, sizeof(target_file_path), "%s/%s", current_dir, files[i]);
        }
        
        char full_target_path[MAX_PATH];
        snprintf(full_target_path, sizeof(full_target_path), "%s/%s", project_root, target_file_path);
        struct stat st;
        long size = 0;
        if (stat(full_target_path, &st) == 0) size = st.st_size;

        char size_str[32];
        if (size < 1024) snprintf(size_str, sizeof(size_str), "%ldB", size);
        else if (size < 1024 * 1024) snprintf(size_str, sizeof(size_str), "%ldKB", size / 1024);
        else snprintf(size_str, sizeof(size_str), "%.1fMB", (float)size / (1024 * 1024));

        char btn_label[300];
        snprintf(btn_label, sizeof(btn_label), "[FIL] %s (%s)", files[i], size_str);
        
        char display_label[256];
        if (strlen(btn_label) > 28) {
            snprintf(display_label, sizeof(display_label), "...%s", btn_label + strlen(btn_label) - 25);
        } else {
            strcpy(display_label, btn_label);
        }
        
        char action[MAX_PATH + 20];
        if (browser_mode == 0) {
            snprintf(action, sizeof(action), "LOAD_FILE:%s", target_file_path);
        } else {
            snprintf(action, sizeof(action), "SET_INPUT_PATH:%s", target_file_path);
        }
        
        append_aligned_button_attr(browser_markup, sizeof(browser_markup), display_label, "onClick", action, &next_display_num);
        items_displayed++;
    }
    
    if (items_displayed == 0) {
        strcat(browser_markup, "<text label=\"║  [Empty Directory]                         ║\" /><br/>");
    } else {
        int total_remaining = (subdir_count + file_count) - items_displayed;
        if (total_remaining > 0) {
            char temp_msg[128];
            snprintf(temp_msg, sizeof(temp_msg), "... and %d more items ...", total_remaining);
            char remaining_line[256];
            snprintf(remaining_line, sizeof(remaining_line), "<text label=\"║  %-40.40s ║\" /><br/>", temp_msg);
            strcat(browser_markup, remaining_line);
        }
    }
    
    // Build browser action buttons markup
    char action_markup[8192] = "";
    if (browser_mode == 0) {
        append_aligned_button_attr(action_markup, sizeof(action_markup), "LOAD FILE", "onClick", "LOAD_ACTION", &next_display_num);
    } else {
        append_aligned_button_attr(action_markup, sizeof(action_markup), "SAVE FILE", "onClick", "SAVE_AS_ACTION", &next_display_num);
    }
    append_aligned_button_attr(action_markup, sizeof(action_markup), "CANCEL", "href", "projects/agy-text-editor/layouts/file_menu.chtpm", &next_display_num);
    
    // Build file info line
    char active_file_info_line_val[512];
    snprintf(active_file_info_line_val, sizeof(active_file_info_line_val), 
             "<text label=\"║  FILE: %-34.34s ║\" /><br/>", active_file_path);
             
    FILE *f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "module_path=projects/agy-text-editor/manager/+x/agy-text-editor_manager.+x\n");
        fprintf(f, "active_layout_id=editor.chtpm\n");
        fprintf(f, "project_id=agy-text-editor\n");
        fprintf(f, "active_project=%s\n", active_file_path);
        fprintf(f, "editor_map=%s\n", editor_map);
        fprintf(f, "input_line=%s\n", input_line_buffer);
        
        fprintf(f, "active_file_info_line=%s\n", active_file_info_line_val);
        fprintf(f, "editor_response_line=║  %-40.40s ║\n", response_buffer);
        
        debug_log("gui_state.txt updated. Cursor pos: (%d, %d)", cursor_x, cursor_y);
        // Browser fields
        fprintf(f, "browser_mode_header=%s\n", browser_mode_header_val);
        fprintf(f, "browser_current_dir_line=%s\n", browser_current_dir_line_val);
        fprintf(f, "search_query_val=%s\n", search_query_buffer);
        fprintf(f, "file_path_input_val=%s\n", file_path_input_buffer);
        fprintf(f, "autocomplete_suggestions_markup=%s\n", autocomplete_markup);
        fprintf(f, "directory_browser_markup=%s\n", browser_markup);
        fprintf(f, "browser_action_buttons_markup=%s\n", action_markup);
        
        // Legacy slot support just in case
        fprintf(f, "slot_name_input=default\n");
        fprintf(f, "slot_info_line=║  CURRENT SLOT: default                    ║\n");
        fprintf(f, "slot_selection_markup=<text label=\"║  [Slots deprecated]                    ║\" /><br/>\n");
        fclose(f);
    }
}

int main(void) {
    // SINGLETON CHECK (Robust)
    char pid_path[MAX_PATH];
    snprintf(pid_path, sizeof(pid_path), "/tmp/agy-text-editor.pid");
    FILE *pf = fopen(pid_path, "r");
    if (pf) {
        int other_pid;
        if (fscanf(pf, "%d", &other_pid) == 1) {
            if (kill(other_pid, 0) == 0) {
                // Another process is running. But is it US?
                // We'll check /proc if we really wanted to be sure, 
                // but kill(pid, 0) is standard enough.
                debug_log("Manager (PID %d) already running. Exiting.", other_pid);
                fclose(pf);
                return 0;
            }
        }
        fclose(pf);
    }
    pf = fopen(pid_path, "w");
    if (pf) {
        fprintf(pf, "%d", getpid());
        fclose(pf);
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    resolve_paths();
    load_project_metadata();
    load_document_to_buffer();
    
    update_gui_state();
    
    char *hist_path = NULL;
    if (asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) return 1;
    
    long last_pos = 0;
    struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    
    while (!g_shutdown) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }
        
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    int processed = 0;
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd = strstr(line, "COMMAND: ");
                        char *kpress = strstr(line, "KEY_PRESSED: ");
                        int processed_this_line = 0;
                        if (cmd) {
                            handle_command(trim_str(cmd + 9));
                            processed_this_line = 1;
                        } else if (kpress) {
                            int key = atoi(kpress + 13);
                            if (process_key(key)) processed_this_line = 1;
                        } else {
                            int key = atoi(line);
                            if (key != 0 || line[0] == '0') {
                                if (process_key(key)) processed_this_line = 1;
                            }
                        }
                        
                        if (processed_this_line) {
                            update_gui_state();
                            trigger_render();
                            processed = 1;
                        }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
            }
        }
        usleep(16667);
    }
    
    free(hist_path);
    return 0;
}
