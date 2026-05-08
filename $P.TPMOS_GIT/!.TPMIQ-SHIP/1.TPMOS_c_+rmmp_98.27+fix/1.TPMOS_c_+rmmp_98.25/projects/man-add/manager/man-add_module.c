/*
 * man-add_module.c - Man Add Project Module (Clean Fix)
 * Pattern: Input -> Trigger Render -> Overwrite State (to win race)
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char last_key_str[64] = "None";
int last_raw_key = 0;
int selector_x = 10, selector_y = 5;
int heartbeat = 0;
int state_updates = 0;

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
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

void load_selector_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/man-add/pieces/selector/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line);
                    char *v = trim_str(eq + 1);
                    if (strcmp(k, "pos_x") == 0) selector_x = atoi(v);
                    else if (strcmp(k, "pos_y") == 0) selector_y = atoi(v);
                }
            }
            fclose(f);
        }
        free(path);
    }
}

void save_selector_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/man-add/pieces/selector/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "name=Selector\ntype=selector\npos_x=%d\npos_y=%d\npos_z=0\non_map=1\n", selector_x, selector_y);
            fclose(f);
        }
        free(path);
    }
}

void write_app_state() {
    char *path = NULL;
    // 1. Update Manager State
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "project_id=man-add\nactive_target_id=selector\ncurrent_z=0\nlast_key=%s\n", last_key_str);
            fclose(f);
        }
        free(path);
    }

    // 2. Update App State (Parser Source)
    if (asprintf(&path, "%s/pieces/apps/player_app/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "project_id=man-add\n");
            fprintf(f, "last_key=%s\n", last_key_str);
            fprintf(f, "last_raw_key=%d\n", last_raw_key);
            fprintf(f, "selector_x=%d\n", selector_x);
            fprintf(f, "selector_y=%d\n", selector_y);
            fprintf(f, "module_heartbeat=%d\n", heartbeat);
            fprintf(f, "state_updates=%d\n", state_updates);
            fclose(f);
            state_updates++;
        }
        free(path);
    }
}

void trigger_render() {
    char *cmd = NULL;
    if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/render_map.+x > /dev/null 2>&1", project_root) != -1) {
        system(cmd);
        free(cmd);
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

int main() {
    resolve_paths();
    load_selector_state();
    
    // Initial State Push
    trigger_render();
    write_app_state();
    hit_frame_marker();

    long last_pos = 0;
    struct stat st;
    char history_path[MAX_PATH];
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);

    while (1) {
        heartbeat++;
        if (heartbeat % 100 == 0) write_app_state();

        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    int key;
                    while (fscanf(hf, "%d", &key) == 1) {
                        if (key <= 0) continue;
                        last_raw_key = key;

                        if (key >= 32 && key <= 126) snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
                        else if (key == 1000) strcpy(last_key_str, "LEFT");
                        else if (key == 1001) strcpy(last_key_str, "RIGHT");
                        else if (key == 1002) strcpy(last_key_str, "UP");
                        else if (key == 1003) strcpy(last_key_str, "DOWN");
                        else snprintf(last_key_str, sizeof(last_key_str), "CODE %d", key);

                        if (key == 'w' || key == 'W' || key == 1002) selector_y--;
                        else if (key == 's' || key == 'S' || key == 1003) selector_y++;
                        else if (key == 'a' || key == 'A' || key == 1000) selector_x--;
                        else if (key == 'd' || key == 'D' || key == 1001) selector_x++;

                        save_selector_state();
                        trigger_render(); // render_map overwrites state.txt
                        write_app_state(); // we overwrite IT back immediately
                        hit_frame_marker();
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
    return 0;
}
