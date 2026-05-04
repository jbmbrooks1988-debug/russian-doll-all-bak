/*
 * man-ops_module.c - Minimal OPS-ONLY Module (Track 1 Validation)
 * Purpose: Prove the C-Ops pipeline works using Shared Traits
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
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_ITEMS 50

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "man-ops";

/* Palette State */
char project_maps[MAX_ITEMS][MAX_LINE];
int map_count = 0;
int map_idx = 0;
const char *ascii_glyphs[] = {"#", ".", "R", "T", "@", "&", "Z", "X", "?", "!"};
int glyph_idx = 0;

/* State */
int gui_focus_index = 1;
int cursor_x = 5, cursor_y = 5, cursor_z = 0;
char last_key_str[32] = "None";

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

void scan_maps() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/maps", project_root, current_project) == -1) return;
    DIR *dir = opendir(path);
    if (!dir) { map_count = 0; free(path); return; }
    map_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && map_count < MAX_ITEMS) {
        if (strstr(entry->d_name, ".txt")) strncpy(project_maps[map_count++], entry->d_name, MAX_LINE - 1);
    }
    closedir(dir); free(path);
}

void trigger_render() {
    char *cmd = NULL;
    /* render_map.+x hits the frame marker itself */
    if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/render_map.+x > /dev/null 2>&1", project_root) != -1) {
        system(cmd); free(cmd);
    }
}

void write_ops_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "project_id=%s\nactive_target_id=selector\ncurrent_z=%d\nlast_key=%s\n", current_project, cursor_z, last_key_str); fclose(f); }
        free(path);
    }
    if (asprintf(&path, "%s/pieces/apps/player_app/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w"); if (!f) { free(path); return; }
        fprintf(f, "project_id=%s\ngui_focus=%d\nlast_key=%s\nselector_pos=(%d,%d,%d)\n", current_project, gui_focus_index, last_key_str, cursor_x, cursor_y, cursor_z);
        fprintf(f, "editor_response=[RESP]: MAN-OPS PIPELINE ACTIVE\n");
        char stats[MAX_LINE]; snprintf(stats, sizeof(stats), "[POS]: (%d,%d,%d) | [KEY]: %-10s", cursor_x, cursor_y, cursor_z, last_key_str);
        fprintf(f, "editor_status_2=%-57s\n", stats);
        char maps_str[MAX_LINE] = "";
        for (int i = 0; i < map_count; i++) { if (i == map_idx) strcat(maps_str, "["); strcat(maps_str, project_maps[i]); if (i == map_idx) strcat(maps_str, "]"); strcat(maps_str, " "); }
        fprintf(f, "map_palette_view=%s\nglyph_palette_view=", maps_str);
        for (int i = 0; i < 10; i++) { if (i == glyph_idx) fprintf(f, "[%s] ", ascii_glyphs[i]); else fprintf(f, "%s ", ascii_glyphs[i]); }
        fprintf(f, "\narmed_glyph=%s\n", ascii_glyphs[glyph_idx]);
        fclose(f); free(path);
    }
}

int is_active_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r"); if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) { fclose(f); int res = (strstr(line, "man-ops.chtpm") != NULL || strstr(line, "editor.chtpm") != NULL); free(path); return res; }
    fclose(f); free(path); return 0;
}

void sync_focus() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/active_gui_index.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        if (f) {
            int active_idx = 0;
            if (fscanf(f, "%d", &active_idx) == 1) { if (active_idx > 0) gui_focus_index = active_idx; }
            fclose(f);
        }
        free(path);
    }
}

void process_key(int key) {
    if (key >= 32 && key <= 126) snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
    else if (key == 10 || key == 13) strcpy(last_key_str, "ENTER");
    else if (key == 27) strcpy(last_key_str, "ESC");
    else if (key == 1000) strcpy(last_key_str, "LEFT");
    else if (key == 1001) strcpy(last_key_str, "RIGHT");
    else if (key == 1002) strcpy(last_key_str, "UP");
    else if (key == 1003) strcpy(last_key_str, "DOWN");

    if (key >= '1' && key <= '9') gui_focus_index = key - '0';

    if (gui_focus_index == 1) {
        if (key == 'w' || key == 'W' || key == 1002 || key == 's' || key == 'S' || key == 1003 ||
            key == 'a' || key == 'A' || key == 1000 || key == 'd' || key == 'D' || key == 1001) {
            const char *dir = (key == 'w' || key == 'W' || key == 1002) ? "w" : (key == 's' || key == 'S' || key == 1003) ? "s" : (key == 'a' || key == 'A' || key == 1000) ? "a" : "d";
            char *cmd = NULL;
            if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/move_entity.+x selector %s %s > /dev/null 2>&1", project_root, dir, current_project) != -1) { system(cmd); free(cmd); }
        }
        // Sync cursor
        char *s_path = NULL;
        if (asprintf(&s_path, "%s/projects/%s/pieces/selector/state.txt", project_root, current_project) != -1) {
            FILE *ssf = fopen(s_path, "r");
            if (ssf) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), ssf)) {
                    char *eq = strchr(line, '=');
                    if (eq) { *eq = '\0'; char *k = trim_str(line), *v = trim_str(eq + 1); if (strcmp(k, "pos_x") == 0) cursor_x = atoi(v); else if (strcmp(k, "pos_y") == 0) cursor_y = atoi(v); }
                }
                fclose(ssf);
            }
            free(s_path);
        }
    }
    else if (gui_focus_index == 2) {
        if (key == 1000 || key == 'a') { map_idx--; if (map_idx < 0) map_idx = map_count - 1; }
        else if (key == 1001 || key == 'd') { map_idx++; if (map_idx >= map_count) map_idx = 0; }
    }
    else if (gui_focus_index == 3) {
        if (key == 1000 || key == 'a') { glyph_idx--; if (glyph_idx < 0) glyph_idx = 9; }
        else if (key == 1001 || key == 'd') { glyph_idx++; if (glyph_idx >= 10) glyph_idx = 0; }
    }
}

int main() {
    resolve_paths(); scan_maps();
    write_ops_state(); trigger_render();
    long last_pos = 0; struct stat st;
    char *hist_path = NULL; asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root);
    while (1) {
        if (!is_active_layout()) { usleep(100000); continue; }
        sync_focus();
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) { fseek(hf, last_pos, SEEK_SET); int key, processed = 0; while (fscanf(hf, "%d", &key) == 1) { process_key(key); processed = 1; } if (processed) { trigger_render(); write_ops_state(); } last_pos = ftell(hf); fclose(hf); }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    return 0;
}
