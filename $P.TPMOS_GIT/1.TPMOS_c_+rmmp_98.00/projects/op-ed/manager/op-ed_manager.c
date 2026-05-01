/*
 * op-ed_module.c - Advanced RMMP Editor (Unified Trait Edition)
 * CPU-SAFE: Signal handling + fork/exec/waitpid pattern
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define MAX_LINE 1024
#define MAX_ITEMS 50

/* CPU-SAFE: Global shutdown flag and signal handler */
static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

/* CPU-SAFE: Helper to run external commands with fork/exec/waitpid */
static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: redirect stdout/stderr to /dev/null */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        /* Parent: wait for child */
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";

/* Project Browser */
char projects[MAX_ITEMS][MAX_LINE];
int project_count = 0;
int project_idx = 0;

/* Map Palette */
char project_maps[MAX_ITEMS][MAX_LINE];
int map_count = 0;
int map_idx = 0;
int stage_map_idx = -1; /* Tracks which map is actually active for editing */

/* Glyph Palette */
const char *ascii_glyphs[] = {"#", ".", "R", "T", "@", "&", "Z", "X", "?", "!"};
const char *emoji_glyphs[] = {"🧱", "🟩", "🌲", "🏰", "🎯", "🐶", "🧟", "💰", "🏠", "🔥"};
int glyph_idx = 0;
int emoji_mode = 0;

/* State */
int gui_focus_index = 1;
int cursor_x = 5, cursor_y = 5, cursor_z = 0;
char active_target_id[64] = "xlector";
char last_key_str[32] = "None";
int last_sync_idx = 0;
char current_world_id[64] = "world_01";
char current_map_dir[64] = "map_01";

static void load_project_metadata(void);

char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void derive_map_dir_name(const char* map_file, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!map_file || !*map_file) { snprintf(out, out_sz, "map_01"); return; }
    snprintf(out, out_sz, "%s", map_file);
    char *dot = strstr(out, ".txt");
    if (dot) *dot = '\0';
    char *z = strstr(out, "_z");
    if (z) {
        char *p = z + 2;
        int digits = 0;
        while (*p && isdigit((unsigned char)*p)) { digits = 1; p++; }
        if (digits && *p == '\0') *z = '\0';
    }
}

int resolve_piece_dir(const char* piece_id, char* out_dir, size_t out_sz) {
    if (!piece_id || !out_dir || out_sz == 0) return 0;

    if (strlen(current_world_id) > 0 && strlen(current_map_dir) > 0) {
        snprintf(out_dir, out_sz, "%s/projects/%s/pieces/%s/%s/%s",
                 project_root, current_project, current_world_id, current_map_dir, piece_id);
        if (access(out_dir, F_OK) == 0) return 1;
    }

    snprintf(out_dir, out_sz, "%s/projects/%s/pieces/%s", project_root, current_project, piece_id);
    if (access(out_dir, F_OK) == 0) return 1;

    out_dir[0] = '\0';
    return 0;
}

void update_current_map_dir() {
    if (stage_map_idx >= 1 && stage_map_idx < map_count) {
        derive_map_dir_name(project_maps[stage_map_idx], current_map_dir, sizeof(current_map_dir));
    }
}

/* PDL Reader Utility - reads METHOD from piece.pdl files */
static int get_method(const char* piece_id, const char* event, 
               char* out_handler, size_t out_size,
               const char* project_root, const char* current_project) {
    char pdl_path[MAX_PATH];
    char piece_dir[MAX_PATH];
    if (!resolve_piece_dir(piece_id, piece_dir, sizeof(piece_dir))) return -1;
    snprintf(pdl_path, sizeof(pdl_path), "%.*s/piece.pdl", 4000, piece_dir);
    
    FILE *f = fopen(pdl_path, "r");
    if (!f) return -1;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) == 0) {
            char *pipe1 = strchr(line, '|');
            if (!pipe1) continue;
            
            char *pipe2 = strchr(pipe1 + 1, '|');
            if (!pipe2) continue;
            
            char parsed_event[64];
            size_t event_len = pipe2 - pipe1 - 1;
            if (event_len >= sizeof(parsed_event)) event_len = sizeof(parsed_event) - 1;
            strncpy(parsed_event, pipe1 + 1, event_len);
            parsed_event[event_len] = '\0';
            char* trimmed_event = trim_str(parsed_event);
            
            char parsed_handler[MAX_PATH];
            strncpy(parsed_handler, pipe2 + 1, sizeof(parsed_handler) - 1);
            parsed_handler[sizeof(parsed_handler) - 1] = '\0';
            char* trimmed_handler = trim_str(parsed_handler);
            
            if (strcmp(trimmed_event, event) == 0) {
                strncpy(out_handler, trimmed_handler, out_size - 1);
                out_handler[out_size - 1] = '\0';
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -1;
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
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

void scan_projects() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects", project_root) == -1) return;
    DIR *dir = opendir(path);
    if (!dir) { free(path); return; }
    project_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && project_count < MAX_ITEMS) {
        if (entry->d_name[0] == '.') continue;
        char *abs_p = NULL;
        if (asprintf(&abs_p, "%s/%s", path, entry->d_name) == -1) continue;
        struct stat st;
        if (stat(abs_p, &st) == 0 && S_ISDIR(st.st_mode)) strncpy(projects[project_count++], entry->d_name, MAX_LINE - 1);
        free(abs_p);
    }
    closedir(dir); free(path);
}

void scan_maps() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/maps", project_root, current_project) == -1) return;
    DIR *dir = opendir(path);
    if (!dir) { 
        map_count = 1;
        strcpy(project_maps[0], "ADD");
        free(path); return; 
    }
    map_count = 0;
    strcpy(project_maps[map_count++], "ADD");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && map_count < MAX_ITEMS) {
        if (strstr(entry->d_name, ".txt")) strncpy(project_maps[map_count++], entry->d_name, MAX_LINE - 1);
    }
    closedir(dir); free(path);
}

void trigger_render() {
    char *cmd = NULL;
    if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/render_map.+x > /dev/null 2>&1", project_root) != -1) {
        run_command(cmd); free(cmd);
    }
}

void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "M\n"); fclose(f); }
        free(path);
    }
}

void set_response(const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/editor/response.txt", project_root) != -1) {
        FILE *f = fopen(path, "w"); if (f) { fprintf(f, "%-57s", msg); fclose(f); }
        free(path);
    }
}

void sync_current_map() {
    if (stage_map_idx < 1 || stage_map_idx >= map_count) return;
    update_current_map_dir();
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        char **lines = NULL;
        int line_count = 0, found = 0;
        char line_buf[MAX_LINE];
        if (f) {
            while (fgets(line_buf, sizeof(line_buf), f) && line_count < 49) {
                lines = realloc(lines, sizeof(char*) * (line_count + 1));
                char *eq = strchr(line_buf, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line_buf), "current_map") == 0) {
                        asprintf(&lines[line_count], "current_map=%s\n", project_maps[stage_map_idx]);
                        found = 1;
                    } else {
                        *eq = '=';
                        asprintf(&lines[line_count], "%s\n", line_buf);
                    }
                } else {
                    asprintf(&lines[line_count], "%s", line_buf);
                }
                line_count++;
            }
            fclose(f);
        }
        if (!found && line_count < 50) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            asprintf(&lines[line_count++], "current_map=%s\n", project_maps[stage_map_idx]);
        }
        f = fopen(path, "w");
        if (f) { for (int i = 0; i < line_count; i++) { fputs(lines[i], f); free(lines[i]); } fclose(f); }
        free(lines);
        free(path);
    }
}

void save_xlector_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "pos_x=%d\npos_y=%d\npos_z=%d\nactive=1\nicon=X\ntype=xlector\n", cursor_x, cursor_y, cursor_z);
            fclose(f);
        }
        free(path);
    }
}

void create_new_map() {
    int max_num = 0;
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/maps", project_root, current_project) == -1) return;
    DIR *dir = opendir(path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "map_", 4) == 0) {
                int num = atoi(entry->d_name + 4);
                if (num > max_num) max_num = num;
            }
        }
        closedir(dir);
    }
    char new_map[MAX_LINE]; snprintf(new_map, sizeof(new_map), "map_%04d.txt", max_num + 1);
    char *full_path = NULL;
    if (asprintf(&full_path, "%s/%s", path, new_map) != -1) {
        FILE *f = fopen(full_path, "w");
        if (f) {
            for (int y = 0; y < 10; y++) {
                for (int x = 0; x < 20; x++) fputc('.', f);
                fputc('\n', f);
            }
            fclose(f); set_response("New Map Created");
        }
        free(full_path);
    }
    free(path); scan_maps();
    for (int i = 1; i < map_count; i++) {
        if (strcmp(project_maps[i], new_map) == 0) { map_idx = i; break; }
    }
}

int get_state_int_fast(const char* piece_id, const char* key) {
    char piece_dir[MAX_PATH];
    if (!resolve_piece_dir(piece_id, piece_dir, sizeof(piece_dir))) return -1;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%.*s/state.txt", 4000, piece_dir);
    FILE *f = fopen(path, "r");
    char line[MAX_LINE]; int val = -1;
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) { *eq = '\0'; if (strcmp(trim_str(line), key) == 0) { val = atoi(trim_str(eq + 1)); break; } }
        }
        fclose(f);
    }
    return val;
}

void sync_emoji_mode() {
    /* TPM Direct Mirror Access: Write emoji_mode to project's sovereign state */
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/emoji_mode.txt", project_root, current_project) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "emoji_mode=%d\n", emoji_mode);
            fclose(f);
        }
        free(path);
    }
    
    /* Also write to manager state for global visibility */
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        /* Read existing state */
        FILE *f = fopen(path, "r");
        char **lines = NULL;
        int line_count = 0, found = 0;
        char line_buf[MAX_LINE];
        if (f) {
            while (fgets(line_buf, sizeof(line_buf), f) && line_count < 99) {
                lines = realloc(lines, sizeof(char*) * (line_count + 1));
                char *eq = strchr(line_buf, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line_buf), "emoji_mode") == 0) {
                        asprintf(&lines[line_count], "emoji_mode=%d\n", emoji_mode);
                        found = 1;
                    } else {
                        *eq = '=';
                        asprintf(&lines[line_count], "%s\n", line_buf);
                    }
                } else {
                    asprintf(&lines[line_count], "%s", line_buf);
                }
                line_count++;
            }
            fclose(f);
        }
        if (!found && line_count < 100) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            asprintf(&lines[line_count++], "emoji_mode=%d\n", emoji_mode);
        }
        f = fopen(path, "w");
        if (f) {
            for (int i = 0; i < line_count; i++) { fputs(lines[i], f); free(lines[i]); }
            fclose(f);
        }
        free(lines);
        free(path);
    }
}

void write_editor_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        /* Read existing content to preserve fields like current_map */
        FILE *f = fopen(path, "r");
        char **lines = NULL;
        int line_count = 0, found_proj = 0, found_target = 0, found_z = 0, found_key = 0;
        char line_buf[MAX_LINE];
        if (f) {
            while (fgets(line_buf, sizeof(line_buf), f) && line_count < 49) {
                lines = realloc(lines, sizeof(char*) * (line_count + 1));
                char *eq = strchr(line_buf, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line_buf);
                    if (strcmp(k, "project_id") == 0) { asprintf(&lines[line_count], "project_id=%s\n", current_project); found_proj = 1; }
                    else if (strcmp(k, "active_target_id") == 0) { asprintf(&lines[line_count], "active_target_id=%s\n", active_target_id); found_target = 1; }
                    else if (strcmp(k, "current_z") == 0) { asprintf(&lines[line_count], "current_z=%d\n", cursor_z); found_z = 1; }
                    else if (strcmp(k, "last_key") == 0) { asprintf(&lines[line_count], "last_key=%s\n", last_key_str); found_key = 1; }
                    else { *eq = '='; asprintf(&lines[line_count], "%s\n", line_buf); }
                } else {
                    asprintf(&lines[line_count], "%s", line_buf);
                }
                line_count++;
            }
            fclose(f);
        }
        if (!found_proj && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "project_id=%s\n", current_project); }
        if (!found_target && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "active_target_id=%s\n", active_target_id); }
        if (!found_z && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "current_z=%d\n", cursor_z); }
        if (!found_key && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "last_key=%s\n", last_key_str); }
        f = fopen(path, "w");
        if (f) { for (int i = 0; i < line_count; i++) { fputs(lines[i], f); free(lines[i]); } fclose(f); }
        free(lines);
        free(path);
    }
    /* Write xlector state for rendering */
    char *sel_path = NULL;
    if (asprintf(&sel_path, "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project) != -1) {
        FILE *sf = fopen(sel_path, "w");
        if (sf) {
            fprintf(sf, "name=Xlector\ntype=xlector\npos_x=%d\npos_y=%d\npos_z=%d\non_map=1\n", cursor_x, cursor_y, cursor_z);
            fclose(sf);
        }
        free(sel_path);
    }
    if (asprintf(&path, "%s/pieces/apps/player_app/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w"); if (!f) { free(path); return; }
        fprintf(f, "project_id=%s\ngui_focus=%d\nlast_key=%s\nxlector_pos=(%d,%d,%d)\n", current_project, gui_focus_index, last_key_str, cursor_x, cursor_y, cursor_z);
        char raw_resp[MAX_LINE] = ""; char *resp_path = NULL;
        if (asprintf(&resp_path, "%s/pieces/apps/editor/response.txt", project_root) != -1) {
            FILE *rf = fopen(resp_path, "r"); if (rf) { if (fgets(raw_resp, sizeof(raw_resp), rf)) trim_str(raw_resp); fclose(rf); }
            free(resp_path);
        }
        fprintf(f, "editor_response=[RESP]: %-49s\n", raw_resp);
        char stats[MAX_LINE]; snprintf(stats, sizeof(stats), "[POS]: (%d,%d,%d) | [H-BEAT]: %d | [KEY]: %-10s", cursor_x, cursor_y, cursor_z, last_sync_idx, last_key_str);
        fprintf(f, "editor_status_2=%-57s\n", stats);
        char maps_str[MAX_LINE] = "";
        for (int i = 0; i < map_count && strlen(maps_str) < 30; i++) { if (i == map_idx) strcat(maps_str, "["); strcat(maps_str, project_maps[i]); if (i == map_idx) strcat(maps_str, "]"); strcat(maps_str, " "); }
        fprintf(f, "map_palette_view=%s\n", maps_str);
        char gly_str[MAX_LINE] = ""; const char **active_set = emoji_mode ? emoji_glyphs : ascii_glyphs;
        for (int i = 0; i < 10; i++) { if (i == glyph_idx) strcat(gly_str, "["); strcat(gly_str, active_set[i]); if (i == glyph_idx) strcat(gly_str, "]"); strcat(gly_str, " "); }
        fprintf(f, "glyph_palette_view=%s\narmed_glyph=%s\n", gly_str, active_set[glyph_idx]);
        char proj_str[MAX_LINE] = "";
        for (int i = 0; i < project_count && strlen(proj_str) < 100; i++) {
            if (i == project_idx) strcat(proj_str, "[");
            strcat(proj_str, projects[i]);
            if (i == project_idx) strcat(proj_str, "]");
            strcat(proj_str, " ");
        }
        fprintf(f, "projects_list_view=%s\n", proj_str);
        fclose(f); free(path);
    }
}

char my_layout_path[MAX_PATH] = "pieces/apps/op-ed/layouts/op-ed.chtpm";

void resolve_my_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/op-ed/op-ed.pdl", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "layout_path")) {
                char *pipe = strrchr(line, '|');
                if (pipe) strncpy(my_layout_path, trim_str(pipe + 1), MAX_PATH - 1);
            }
        }
        fclose(f);
    }
    free(path);
}

int is_active_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r"); if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) { 
        fclose(f); 
        char *cur = trim_str(line);
        int res = 0;

        /* Exact layout match (legacy behavior). */
        if (cur && strstr(cur, "op-ed.chtpm") != NULL) {
            res = 1;
        } else {
            /* Suite match: keep op-ed active for file/load menu layouts too. */
            /* BUT: If it's the PAL Editor, op-ed MUST sleep so editor gets the keys! */
            if (cur && strstr(cur, "pal_editor.chtpm") != NULL) {
                res = 0;
            }
            else if (cur && (strstr(cur, "pieces/apps/op-ed/layouts/") != NULL ||
                             strstr(cur, "projects/op-ed/layouts/") != NULL)) {
                res = 1;
            }
        }
        free(path); 
        return res; 
    }
    fclose(f); free(path); return 0;
}

void sync_focus() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/active_gui_index.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        if (f) { int active_idx = 0; if (fscanf(f, "%d", &active_idx) == 1) { last_sync_idx = active_idx; if (active_idx > 0) gui_focus_index = active_idx; } fclose(f); }
        free(path);
    }
}

void save_project() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/project.pdl", project_root, current_project) == -1) return;
    
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | project_id         | %s\n", current_project);
        fprintf(f, "META         | version            | 1.0\n");
        fprintf(f, "META         | entry_layout       | pieces/apps/op-ed/layouts/op-ed.chtpm\n\n");
        
        fprintf(f, "STATE        | starting_map       | %s\n", current_map_dir);
        fprintf(f, "STATE        | player_piece       | %s\n", active_target_id);
        fprintf(f, "STATE        | current_world      | %s\n", current_world_id);
        fprintf(f, "STATE        | current_map        | %s\n\n", current_map_dir);
        
        fprintf(f, "RESPONSE     | default            | Project %s Saved.\n", current_project);
        fclose(f);
        set_response("Project Saved Successfully");
    }
    free(path);
}

void trigger_event(const char* piece_id, const char* event) {
    char handler[MAX_PATH];
    if (get_method(piece_id, event, handler, sizeof(handler), project_root, current_project) == 0) {
        char *cmd = NULL;
        if (strstr(handler, ".asm")) {
            asprintf(&cmd, "PRISC_PROJECT_ID=%s PRISC_PROJECT_ROOT=%s %s/pieces/system/prisc/prisc+x %s/%s > /dev/null 2>&1",
                     current_project, project_root, project_root, project_root, handler);
        } else {
            asprintf(&cmd, "%s/%s > /dev/null 2>&1", project_root, handler);
        }
        if (cmd) { 
            run_command(cmd); 
            char msg[MAX_LINE]; snprintf(msg, sizeof(msg), "Triggered %s on %s", event, piece_id);
            set_response(msg);
            free(cmd); 
        }
    } else {
        char msg[MAX_LINE]; snprintf(msg, sizeof(msg), "No handler for %s on %s", event, piece_id);
        set_response(msg);
    }
}

void bind_event(const char* piece_id, const char* event, const char* handler) {
    char *cmd = NULL;
    if (asprintf(&cmd, "%s/pieces/master_ledger/plugins/+x/piece_manager.+x %s add-method %s %s > /dev/null 2>&1",
                 project_root, piece_id, event, handler) != -1) {
        run_command(cmd); free(cmd);
        char msg[MAX_LINE]; snprintf(msg, sizeof(msg), "Bound %s to %s", event, piece_id);
        set_response(msg);
    }
}

void save_as_clone() {
    char new_project[MAX_LINE];
    char suffix[32];
    size_t max_len = sizeof(new_project) - 1;
    size_t base_len;
    size_t suffix_len;
    size_t prefix_cap;

    snprintf(suffix, sizeof(suffix), "_clone_%04ld", time(NULL) % 10000);
    suffix_len = strnlen(suffix, sizeof(suffix));
    if (suffix_len > max_len) suffix_len = max_len;

    prefix_cap = max_len - suffix_len;
    base_len = strnlen(current_project, sizeof(current_project));
    if (base_len > prefix_cap) base_len = prefix_cap;

    memcpy(new_project, current_project, base_len);
    memcpy(new_project + base_len, suffix, suffix_len);
    new_project[base_len + suffix_len] = '\0';

    char *cmd = NULL;
    if (asprintf(&cmd, "cp -r %s/projects/%s %s/projects/%s > /dev/null 2>&1", 
                 project_root, current_project, project_root, new_project) != -1) {
        run_command(cmd); free(cmd);
        strncpy(current_project, new_project, sizeof(current_project) - 1);
        save_project();
        char msg[MAX_LINE];
        const char *prefix = "Cloned to: ";
        size_t prefix_len = strlen(prefix);
        size_t room = (sizeof(msg) - 1 > prefix_len) ? (sizeof(msg) - 1 - prefix_len) : 0;
        size_t project_len = strnlen(new_project, sizeof(new_project));
        if (project_len > room) project_len = room;
        memcpy(msg, prefix, prefix_len);
        memcpy(msg + prefix_len, new_project, project_len);
        msg[prefix_len + project_len] = '\0';
        set_response(msg);
        scan_projects();
    }
}

void load_selected_project() {
    if (project_idx >= 0 && project_idx < project_count) {
        strncpy(current_project, projects[project_idx], sizeof(current_project) - 1);
        load_project_metadata();
        scan_maps();
        if (map_count > 1) { map_idx = 1; stage_map_idx = 1; }
        else { map_idx = 0; stage_map_idx = 0; }
        sync_current_map();
        update_current_map_dir();
        set_response("Project Loaded");
    }
}

void process_key(int key) {
    if (key >= 32 && key <= 126) {
        if (key == ' ') strcpy(last_key_str, "SPACE");
        else snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
    }
    else if (key == 10 || key == 13) strcpy(last_key_str, "ENTER");
    else if (key == 27) strcpy(last_key_str, "ESC");
    else if (key == 1000) strcpy(last_key_str, "LEFT");
    else if (key == 1001) strcpy(last_key_str, "RIGHT");
    else if (key == 1002) strcpy(last_key_str, "UP");
    else if (key == 1003) strcpy(last_key_str, "DOWN");

    /* Submenu Handlers */
    if (key == 's' || key == 'S') { save_as_clone(); return; }
    if (key == 'p' || key == 'P') {
        /* PLAY PROJECT: Switch to game.chtpm */
        char *lp = NULL;
        if (asprintf(&lp, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
            FILE *f = fopen(lp, "a");
            if (f) { fprintf(f, "pieces/apps/playrm/layouts/game.chtpm\n"); fclose(f); }
            free(lp);
        }
        set_response("Switching to Play Mode...");
        return;
    }
    if (key == 'e' && gui_focus_index == 1) {
        /* PAL EDITOR: Open PAL script editor (works from map view, any selection) */
        /* If xlector is selected, use a default event name (like RPG Maker common events) */
        const char *target = (strcmp(active_target_id, "xlector") == 0) ? "common_event" : active_target_id;
        
        /* Set up PAL editor state */
        char *mgr_path = NULL;
        if (asprintf(&mgr_path, "%s/projects/op-ed/manager/state.txt", project_root) != -1) {
            FILE *f = fopen(mgr_path, "w");
            if (f) {
                /* Preserve existing state, add PAL editor fields */
                fprintf(f, "project_id=%s\n", current_project);
                fprintf(f, "active_target_id=%s\n", active_target_id);
                fprintf(f, "pal_editor_piece=%s\n", target);
                fprintf(f, "pal_editor_event=on_interact\n");
                fprintf(f, "current_z=%d\n", cursor_z);
                fprintf(f, "last_key=None\n");
                fprintf(f, "current_map=%s\n", current_map_dir);
                fclose(f);
            }
            free(mgr_path);
        }
        /* Write to layout_changed.txt (triggers parser reload) */
        char *lp = NULL;
        if (asprintf(&lp, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
            FILE *f = fopen(lp, "a");
            if (f) { fprintf(f, "projects/op-ed/layouts/pal_editor.chtpm\n"); fclose(f); }
            free(lp);
        }
        /* Hit frame marker to trigger parser reload */
        char *pulse = NULL;
        if (asprintf(&pulse, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
            FILE *f = fopen(pulse, "a");
            if (f) { fprintf(f, "L\n"); fclose(f); }
            free(pulse);
        }
        /* Truncate view files to prevent ghost frame */
        char *v1 = NULL;
        if (asprintf(&v1, "%s/pieces/apps/player_app/view.txt", project_root) != -1) {
            FILE *f = fopen(v1, "w"); if (f) fclose(f);
            free(v1);
        }
        
        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "PAL Editor: %s", target);
        set_response(msg);
        return;
    }

    if (key == '9' || key == 27) { strcpy(active_target_id, "xlector"); set_response("Returned to Xlector"); }
    if (key >= '1' && key <= '9') {
        if (key == '4') { 
            emoji_mode = !emoji_mode; 
            char msg[64]; 
            snprintf(msg, sizeof(msg), "Emoji Mode: %s", emoji_mode ? "ON" : "OFF"); 
            set_response(msg);
            sync_emoji_mode();  /* TPM Mirror Sync */
        }
        else if (key == '7') { save_project(); }
        else if (key == '8') { 
            if (strcmp(active_target_id, "xlector") != 0) {
                bind_event(active_target_id, "interact", "projects/man-pal/scripts/move.asm");
            } else {
                set_response("Select a piece first!");
            }
        }
        else gui_focus_index = key - '0';
    }

    if (key == 122) { /* Ctrl+Z */
        char *cmd = NULL; if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/undo_action.+x > /dev/null 2>&1", project_root) != -1) { run_command(cmd); free(cmd); set_response("Undo performed"); }
    }
    
    /* Load Menu Selection Logic */
    char *cl = NULL;
    if (asprintf(&cl, "%s/pieces/display/current_layout.txt", project_root) != -1) {
        FILE *clf = fopen(cl, "r");
        if (clf) {
            char line[MAX_LINE];
            if (fgets(line, sizeof(line), clf)) {
                if (strstr(line, "load_menu.chtpm")) {
                    if (key == 1000 || key == 'a') { project_idx--; if (project_idx < 0) project_idx = project_count - 1; }
                    else if (key == 1001 || key == 'd') { project_idx++; if (project_idx >= project_count) project_idx = 0; }
                    else if (key == 10 || key == 13) { load_selected_project(); }
                    fclose(clf); free(cl); return;
                }
            }
            fclose(clf);
        }
        free(cl);
    }

    if (key == 127 || key == 8) { /* Backspace */
        if (gui_focus_index == 1 && stage_map_idx > 0 && stage_map_idx < map_count) {
            char *cmd = NULL; if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/place_tile.+x %s %d %d . > /dev/null 2>&1", project_root, project_maps[stage_map_idx], cursor_x, cursor_y) != -1) { run_command(cmd); free(cmd); set_response("Tile cleared"); }
        }
    }

    if (gui_focus_index == 1) {
        if (key == 'w' || key == 'W' || key == 1002 || key == 's' || key == 'S' || key == 1003 || key == 'a' || key == 'A' || key == 1000 || key == 'd' || key == 'D' || key == 1001) {
            const char *dir = (key == 'w' || key == 'W' || key == 1002) ? "w" : (key == 's' || key == 'S' || key == 1003) ? "s" : (key == 'a' || key == 'A' || key == 1000) ? "a" : "d";
            if (strcmp(active_target_id, "xlector") == 0) {
                /* Move xlector cursor directly */
                if (strcmp(dir, "w") == 0) cursor_y--; else if (strcmp(dir, "s") == 0) cursor_y++; else if (strcmp(dir, "a") == 0) cursor_x--; else if (strcmp(dir, "d") == 0) cursor_x++;
                if (cursor_x < 0) cursor_x = 0; if (cursor_x >= 100) cursor_x = 99; if (cursor_y < 0) cursor_y = 0; if (cursor_y >= 100) cursor_y = 99;
                save_xlector_state();
            } else {
                /* Move entity */
                char piece_dir[MAX_PATH];
                if (!resolve_piece_dir(active_target_id, piece_dir, sizeof(piece_dir))) {
                    strcpy(active_target_id, "xlector");
                    set_response("Entity not found, returned to xlector");
                } else {
                    char *cmd = NULL; 
                    if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/move_entity.+x %s %s %s > /dev/null 2>&1", 
                                 project_root, active_target_id, dir, current_project) != -1) { 
                        run_command(cmd); free(cmd); 
                    }
                    int px = get_state_int_fast(active_target_id, "pos_x"), py = get_state_int_fast(active_target_id, "pos_y");
                    if (px != -1 && py != -1) { cursor_x = px; cursor_y = py; save_xlector_state(); }
                }
            }
        }
        if (key == ' ') {
            if (strcmp(active_target_id, "xlector") == 0) {
                int found = 0;
                char map_path[MAX_PATH];
                snprintf(map_path, sizeof(map_path), "%.*s/projects/%.*s/pieces/%.*s/%.*s",
                         1000, project_root, 1000, current_project, 1000, current_world_id, 1000, current_map_dir);
                DIR *dir = opendir(map_path);
                if (dir) {
                    struct dirent *entry;
                    while ((entry = readdir(dir)) != NULL) {
                        if (entry->d_name[0] == '.' || strcmp(entry->d_name, "xlector") == 0) continue;
                        int tx = get_state_int_fast(entry->d_name, "pos_x");
                        int ty = get_state_int_fast(entry->d_name, "pos_y");
                        if (tx == cursor_x && ty == cursor_y) {
                            strncpy(active_target_id, entry->d_name, 63);
                            char *m = NULL; if (asprintf(&m, "Selected %s", entry->d_name) != -1) { set_response(m); free(m); }
                            found = 1;
                            break;
                        }
                    }
                    closedir(dir);
                }
                if (!found) {
                    char *pieces_p = NULL;
                    if (asprintf(&pieces_p, "%s/projects/%s/pieces", project_root, current_project) != -1) {
                        dir = opendir(pieces_p);
                        if (dir) {
                            struct dirent *entry;
                            while ((entry = readdir(dir)) != NULL) {
                                if (entry->d_name[0] == '.' || strcmp(entry->d_name, "xlector") == 0 || strncmp(entry->d_name, "world_", 6) == 0) continue;
                                int tx = get_state_int_fast(entry->d_name, "pos_x");
                                int ty = get_state_int_fast(entry->d_name, "pos_y");
                                if (tx == cursor_x && ty == cursor_y) {
                                    strncpy(active_target_id, entry->d_name, 63);
                                    char *m = NULL; if (asprintf(&m, "Selected %s", entry->d_name) != -1) { set_response(m); free(m); }
                                    found = 1;
                                    break;
                                }
                            }
                            closedir(dir);
                        }
                        free(pieces_p);
                    }
                }
                if (!found) set_response("No piece at cursor");
            } else {
                /* If piece is already selected, trigger its 'interact' method */
                trigger_event(active_target_id, "interact");
            }
        }
        char *s_path = NULL; if (asprintf(&s_path, "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project) != -1) {
            FILE *ssf = fopen(s_path, "r"); if (ssf) {
                char line[MAX_LINE]; while (fgets(line, sizeof(line), ssf)) {
                    char *eq = strchr(line, '='); if (eq) { *eq = '\0'; char *k = trim_str(line), *v = trim_str(eq + 1); if (strcmp(k, "pos_x") == 0) cursor_x = atoi(v); else if (strcmp(k, "pos_y") == 0) cursor_y = atoi(v); else if (strcmp(k, "pos_z") == 0) cursor_z = atoi(v); }
                }
                fclose(ssf);
            }
            free(s_path);
        }
        if (strcmp(active_target_id, "xlector") != 0) {
            int px = get_state_int_fast(active_target_id, "pos_x"), py = get_state_int_fast(active_target_id, "pos_y");
            if (px != -1 && py != -1) {
                char *c = NULL; if (asprintf(&c, "%s/pieces/master_ledger/plugins/+x/piece_manager.+x xlector set-state pos_x %d > /dev/null 2>&1", project_root, px) != -1) { run_command(c); free(c); }
                if (asprintf(&c, "%s/pieces/master_ledger/plugins/+x/piece_manager.+x xlector set-state pos_y %d > /dev/null 2>&1", project_root, py) != -1) { run_command(c); free(c); }
                cursor_x = px; cursor_y = py;
            }
        }
    }
    else if (gui_focus_index == 2) { 
        if (key == 1000 || key == 'a') { map_idx--; if (map_idx < 0) map_idx = map_count - 1; } 
        else if (key == 1001 || key == 'd') { map_idx++; if (map_idx >= map_count) map_idx = 0; } 
        else if (key == 10 || key == 13) {
            if (map_idx == 0) { 
                create_new_map(); 
                stage_map_idx = 1;
                sync_current_map();
                set_response("New Map Created"); 
            } else { 
                stage_map_idx = map_idx;
                sync_current_map();
                set_response("Map Switched"); 
            }
        }
    }
    else if (gui_focus_index == 3) { 
        if (key == 1000 || key == 'a') { glyph_idx--; if (glyph_idx < 0) glyph_idx = 9; } 
        else if (key == 1001 || key == 'd') { glyph_idx++; if (glyph_idx >= 10) glyph_idx = 0; } 
        else if (key == 10 || key == 13) set_response("Glyph Armed");
    }

    if ((key == 10 || key == 13) && gui_focus_index == 1) {
        if (stage_map_idx < 1 || stage_map_idx >= map_count) {
            set_response("Select a map first!");
            return;
        }
        const char **active_set = emoji_mode ? emoji_glyphs : ascii_glyphs; const char *glyph = active_set[glyph_idx];
        int is_entity = (strcmp(glyph, "@") == 0 || strcmp(glyph, "&") == 0 || strcmp(glyph, "Z") == 0 || strcmp(glyph, "T") == 0);
        char *cmd = NULL;
                if (is_entity) {
                    const char *type = NULL;
            if (strcmp(glyph, "@") == 0) type = "player";
            else if (strcmp(glyph, "&") == 0) type = "npc";
            else if (strcmp(glyph, "Z") == 0) type = "zombie";
            else if (strcmp(glyph, "T") == 0) type = "chest";
            
                update_current_map_dir();
                /* Try to get custom handler from PDL first */
                char handler[MAX_PATH];
                char piece_id[64];
                snprintf(piece_id, sizeof(piece_id), "%s_%d_%d", type, cursor_x, cursor_y);
            
                if (get_method(piece_id, "on_create", handler, sizeof(handler),
                           project_root, current_project) == 0) {
                /* Use custom handler from PDL */
                asprintf(&cmd, "%s > /dev/null 2>&1", handler);
                } else {
                    /* Fallback to default create_piece op */
                    asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/create_piece.+x '%s' %d %d %s --world %s --map %s > /dev/null 2>&1",
                         project_root, type, cursor_x, cursor_y, current_project, current_world_id, current_map_dir);
                }
        } else {
            /* Try to get custom handler for tile placement */
            char handler[MAX_PATH];
            char tile_id[64];
            snprintf(tile_id, sizeof(tile_id), "tile_%s", glyph);
            
            if (get_method(tile_id, "on_place", handler, sizeof(handler),
                           project_root, current_project) == 0) {
                /* Use custom handler from PDL */
                asprintf(&cmd, "%s > /dev/null 2>&1", handler);
            } else {
                /* Fallback to default place_tile op */
                asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/place_tile.+x %s %d %d '%s' > /dev/null 2>&1",
                         project_root, project_maps[stage_map_idx], cursor_x, cursor_y, glyph);
            }
        }
        if (cmd) { run_command(cmd); free(cmd); if (is_entity) set_response("Piece created"); else set_response("Tile placed"); }
    }
}

static void load_project_metadata(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/project.pdl", project_root, current_project) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *pipe1 = strchr(line, '|');
            if (!pipe1) continue;
            char *pipe2 = strchr(pipe1 + 1, '|');
            if (!pipe2) continue;
            
            char key[MAX_LINE];
            int k_len = pipe2 - pipe1 - 1;
            if (k_len >= MAX_LINE) k_len = MAX_LINE - 1;
            strncpy(key, trim_str(pipe1 + 1), k_len);
            key[k_len] = '\0';
            
            char *val = trim_str(pipe2 + 1);
            if (strcmp(trim_str(key), "current_world") == 0) strncpy(current_world_id, val, 63);
            else if (strcmp(trim_str(key), "current_map") == 0) strncpy(current_map_dir, val, 63);
            else if (strcmp(trim_str(key), "player_piece") == 0) strncpy(active_target_id, val, 63);
        }
        fclose(f);
    }
    free(path);
}

int main() {
    /* CPU-SAFE: Register signal handlers */
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    /* CPU-SAFE: Set process group */
    setpgid(0, 0);
    
    resolve_paths();
    resolve_my_layout();
    
    char path[MAX_CMD];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f_init = fopen(path, "r");
    if (f_init) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f_init)) {
            char *eq = strchr(line, '=');
            if (eq) { *eq = '\0'; char *k = trim_str(line), *v = trim_str(eq + 1); if (strcmp(k, "project_id") == 0) strncpy(current_project, v, sizeof(current_project)-1); }
        }
        fclose(f_init);
    }
    
    load_project_metadata();
    scan_projects(); scan_maps();
    
    /* Try to match map_idx to current_map_dir from metadata */
    if (strlen(current_map_dir) > 0) {
        for (int i = 1; i < map_count; i++) {
            char dir_name[64];
            derive_map_dir_name(project_maps[i], dir_name, sizeof(dir_name));
            if (strcmp(dir_name, current_map_dir) == 0) {
                map_idx = i;
                stage_map_idx = i;
                break;
            }
        }
    }

    if (stage_map_idx == -1) {
        if (map_count > 1) { map_idx = 1; stage_map_idx = 1; } 
        else { map_idx = 0; stage_map_idx = 0; }
    }
    
    sync_current_map();
    update_current_map_dir();
    write_editor_state(); trigger_render(); hit_frame_marker();

    long last_pos = 0; struct stat st; char *hist_path = NULL;
    if (asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) return 1;
    // Skip existing keys (only read NEW keys after op-ed starts)
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    while (!g_shutdown) {
        if (!is_active_layout()) { usleep(100000); continue; }
        sync_focus();
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r"); if (hf) {
                    fseek(hf, last_pos, SEEK_SET); int key, processed = 0;
                    while (fscanf(hf, "%d", &key) == 1) { process_key(key); processed = 1; }
                    if (processed) { write_editor_state(); trigger_render(); hit_frame_marker(); }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    free(hist_path); return 0;
}
