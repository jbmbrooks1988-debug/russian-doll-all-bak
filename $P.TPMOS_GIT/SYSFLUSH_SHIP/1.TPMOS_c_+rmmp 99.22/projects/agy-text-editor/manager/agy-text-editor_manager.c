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

#define MODULE_NAME "agy-text-editor"
#define MAX_PATH 4096
#define MAX_LINE 1024

static char project_root[MAX_PATH] = ".";
static char active_file_path[MAX_PATH] = "projects/agy-text-editor/pieces/document.txt";
static char file_path_input_buffer[MAX_PATH] = "projects/agy-text-editor/pieces/document.txt";
static char search_query_buffer[MAX_LINE] = "";
static char current_dir[MAX_PATH] = "projects/agy-text-editor";
static int browser_mode = 0; // 0 = Load, 1 = Save As

static char response_buffer[256] = "Ready.";
static char input_line_buffer[MAX_LINE] = "";

static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
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
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
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
                strncpy(file_path_input_buffer, trim_str(line + 1), sizeof(file_path_input_buffer) - 1);
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
    snprintf(pulse, sizeof(pulse), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(pulse, "a");
    if (f) { fprintf(f, "F\n"); fclose(f); }
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

static int save_to_path(const char *rel_path) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", project_root, rel_path);
    
    char doc_path[MAX_PATH];
    snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
    
    FILE *src = fopen(doc_path, "r");
    if (!src) {
        snprintf(response_buffer, sizeof(response_buffer), "Error: cannot read working buffer");
        return -1;
    }
    
    FILE *dst = fopen(full_path, "w");
    if (!dst) {
        fclose(src);
        snprintf(response_buffer, sizeof(response_buffer), "Error: cannot write target file");
        return -1;
    }
    
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    
    snprintf(response_buffer, sizeof(response_buffer), "Saved to %s", rel_path);
    strncpy(active_file_path, rel_path, sizeof(active_file_path) - 1);
    return 0;
}

static int load_from_path(const char *rel_path) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", project_root, rel_path);
    
    FILE *src = fopen(full_path, "r");
    if (!src) {
        snprintf(response_buffer, sizeof(response_buffer), "Error: file not found");
        return -1;
    }
    
    char doc_path[MAX_PATH];
    snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
    
    FILE *dst = fopen(doc_path, "w");
    if (!dst) {
        fclose(src);
        snprintf(response_buffer, sizeof(response_buffer), "Error: cannot write working buffer");
        return -1;
    }
    
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    
    snprintf(response_buffer, sizeof(response_buffer), "Loaded %s", rel_path);
    strncpy(active_file_path, rel_path, sizeof(active_file_path) - 1);
    return 0;
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

static void process_key(int key) {
    if (key == 127 || key == 8) {
        char layout[MAX_LINE];
        get_current_layout_name(layout, sizeof(layout));
        if (strcmp(layout, "editor.chtpm") == 0) {
            read_editor_line();
            int len = strlen(input_line_buffer);
            if (len > 0) {
                input_line_buffer[len - 1] = '\0';
                char *path = NULL;
                if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
                    FILE *bf = fopen(path, "a");
                    if (bf) {
                        fprintf(bf, "e%s\n", input_line_buffer);
                        fclose(bf);
                    }
                    free(path);
                }
            }
        } else if (strcmp(layout, "file_browser.chtpm") == 0) {
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
                }
            }
        }
    } else if (key == '1') {
        // Append Line
        read_editor_line();
        char doc_path[MAX_PATH];
        snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
        FILE *f = fopen(doc_path, "a");
        if (f) {
            fprintf(f, "%s\n", input_line_buffer);
            fclose(f);
            strcpy(response_buffer, "Line appended.");
        } else {
            strcpy(response_buffer, "Error writing file.");
        }
        clear_editor_line();
    } else if (key == '2') {
        // Clear file
        char doc_path[MAX_PATH];
        snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
        FILE *f = fopen(doc_path, "w");
        if (f) {
            fclose(f);
            strcpy(response_buffer, "File cleared.");
        } else {
            strcpy(response_buffer, "Error clearing file.");
        }
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
    } else if (key == '8') {
        // Save As...
        browser_mode = 1;
        clear_search_query();
        set_file_path_input(active_file_path);
        transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm");
        strcpy(response_buffer, "Enter Save-As path.");
    } else if (key == '9') {
        // Load File...
        browser_mode = 0;
        clear_search_query();
        set_file_path_input("");
        transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm");
        strcpy(response_buffer, "Select file to load.");
    }
}

static void handle_command(const char *cmd) {
    if (strncmp(cmd, "SET_DIR:", 8) == 0) {
        strncpy(current_dir, cmd + 8, sizeof(current_dir) - 1);
        if (strlen(current_dir) == 0) strcpy(current_dir, ".");
        clear_search_query();
    } else if (strncmp(cmd, "SET_INPUT_PATH:", 15) == 0) {
        set_file_path_input(cmd + 15);
    } else if (strncmp(cmd, "AUTOCOMPLETE:", 13) == 0) {
        set_file_path_input(cmd + 13);
    } else if (strncmp(cmd, "LOAD_FILE:", 10) == 0) {
        if (load_from_path(cmd + 10) == 0) {
            transition_to_layout("projects/agy-text-editor/layouts/editor.chtpm");
        }
    } else if (strcmp(cmd, "LOAD_ACTION") == 0) {
        read_file_path_input();
        if (load_from_path(file_path_input_buffer) == 0) {
            transition_to_layout("projects/agy-text-editor/layouts/editor.chtpm");
        }
    } else if (strcmp(cmd, "SAVE_AS_ACTION") == 0) {
        read_file_path_input();
        if (save_to_path(file_path_input_buffer) == 0) {
            transition_to_layout("projects/agy-text-editor/layouts/editor.chtpm");
        }
    }
}

static void build_editor_content(char *out, size_t max_sz) {
    out[0] = '\0';
    char doc_path[MAX_PATH];
    snprintf(doc_path, sizeof(doc_path), "%s/projects/agy-text-editor/pieces/document.txt", project_root);
    FILE *f = fopen(doc_path, "r");
    int lines_written = 0;
    
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f) && lines_written < 8) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            nl = strchr(line, '\r');
            if (nl) *nl = '\0';
            
            char formatted[512];
            snprintf(formatted, sizeof(formatted), "<text label=\"║  %-40.40s ║\" /><br/>", line);
            strncat(out, formatted, max_sz - strlen(out) - 1);
            lines_written++;
        }
        fclose(f);
    }
    
    if (lines_written == 0) {
        strncat(out, "<text label=\"║  [Empty File]                            ║\" /><br/>", max_sz - strlen(out) - 1);
        lines_written++;
    }
    
    while (lines_written < 6) {
        strncat(out, "<text label=\"║                                             ║\" /><br/>", max_sz - strlen(out) - 1);
        lines_written++;
    }
}

static void update_gui_state(void) {
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/projects/agy-text-editor/manager/gui_state.txt", project_root);
    
    char editor_content[8192] = "";
    build_editor_content(editor_content, sizeof(editor_content));
    
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
        append_aligned_button_attr(browser_markup, sizeof(browser_markup), "[..] Up", "onClick", action, &next_display_num);
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
        snprintf(btn_label, sizeof(btn_label), "%s/", subdirs[i]);
        
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
        
        char display_label[256];
        if (strlen(files[i]) > 28) {
            snprintf(display_label, sizeof(display_label), "...%s", files[i] + strlen(files[i]) - 25);
        } else {
            strcpy(display_label, files[i]);
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
        fprintf(f, "editor_content=%s\n", editor_content);
        fprintf(f, "input_line=%s\n", input_line_buffer);
        
        fprintf(f, "active_file_info_line=%s\n", active_file_info_line_val);
        fprintf(f, "editor_response_line=║  %-40.40s ║\n", response_buffer);
        
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
    
    // Sync active project to player_app
    char main_state_path[MAX_PATH];
    snprintf(main_state_path, sizeof(main_state_path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *rf = fopen(main_state_path, "r");
    char lines[200][MAX_LINE];
    int lc = 0;
    int found_proj = 0;
    if (rf) {
        while (fgets(lines[lc], MAX_LINE, rf) && lc < 190) {
            if (strncmp(lines[lc], "project_id=", 11) == 0) {
                snprintf(lines[lc], MAX_LINE, "project_id=agy-text-editor\n");
                found_proj = 1;
            }
            lc++;
        }
        fclose(rf);
    }
    if (!found_proj && lc < 190) {
        snprintf(lines[lc++], MAX_LINE, "project_id=agy-text-editor\n");
    }
    FILE *wf = fopen(main_state_path, "w");
    if (wf) {
        for (int i = 0; i < lc; i++) fputs(lines[i], wf);
        fclose(wf);
    }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    resolve_paths();
    
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
                        if (cmd) {
                            handle_command(trim_str(cmd + 9));
                            processed = 1;
                        } else if (kpress) {
                            int key = atoi(kpress + 13);
                            process_key(key);
                            processed = 1;
                        } else {
                            int key = atoi(line);
                            if (key != 0 || line[0] == '0') {
                                process_key(key);
                                processed = 1;
                            }
                        }
                    }
                    if (processed) {
                        update_gui_state();
                        trigger_render();
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
