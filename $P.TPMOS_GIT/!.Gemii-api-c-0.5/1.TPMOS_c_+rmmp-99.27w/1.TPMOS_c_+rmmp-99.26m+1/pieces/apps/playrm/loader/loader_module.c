/*
 * loader_module.c - Project Loader Bridge
 * 
 * Responsibilities:
 * 1. Scan projects/ directory
 * 2. Update loader.pdl with dynamic METHOD entries for each project
 * 3. Handle KEY:n input to load selected project and switch layout
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
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char projects[50][MAX_LINE];
int project_count = 0;

char* trim_str(char *str) {
    char *end;
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

void scan_projects() {
    char *projects_dir_path = NULL;
    if (asprintf(&projects_dir_path, "%s/projects", project_root) == -1) return;
    DIR *dir = opendir(projects_dir_path);
    if (!dir) { free(projects_dir_path); return; }

    project_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && project_count < 50) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "trunk") == 0) continue;  
        
        char *abs_p = NULL;
        if (asprintf(&abs_p, "%s/%s", projects_dir_path, entry->d_name) == -1) continue;
        struct stat st;
        if (stat(abs_p, &st) == 0 && S_ISDIR(st.st_mode)) {
            char *pdl_path = NULL;
            if (asprintf(&pdl_path, "%s/project.pdl", abs_p) != -1) {
                if (access(pdl_path, F_OK) == 0) {
                    strncpy(projects[project_count++], entry->d_name, MAX_LINE - 1);
                }
                free(pdl_path);
            }
        }
        free(abs_p);
    }
    closedir(dir);
    free(projects_dir_path);

    for (int i = 0; i < project_count - 1; i++) {
        for (int j = i + 1; j < project_count; j++) {
            if (strcmp(projects[i], projects[j]) > 0) {
                char tmp[MAX_LINE];
                strncpy(tmp, projects[i], MAX_LINE - 1);
                strncpy(projects[i], projects[j], MAX_LINE - 1);
                strncpy(projects[j], tmp, MAX_LINE - 1);
            }
        }
    }
}

void update_loader_pdl() {
    char *pdl_path = NULL;
    if (asprintf(&pdl_path, "%s/pieces/apps/playrm/loader/loader.pdl", project_root) != -1) {
        FILE *f = fopen(pdl_path, "w");
        if (f) {
            fprintf(f, "SECTION      | KEY                | VALUE\n");
            fprintf(f, "----------------------------------------\n");
            fprintf(f, "META         | piece_id           | loader\n");
            fprintf(f, "META         | version            | 1.0\n");
            fprintf(f, "META         | determinism        | strict\n\n");
            fprintf(f, "STATE        | name                 | Project Loader\n");
            fprintf(f, "STATE        | status               | active\n\n");
            fprintf(f, "METHOD       | move                 | void\n");
            for (int i = 0; i < project_count; i++) {
                fprintf(f, "METHOD       | %s | LOAD_PROJECT:%s\n", projects[i], projects[i]);
            }
            fclose(f);
        }
        free(pdl_path);
    }
}

void write_gui_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/playrm/loader/gui_state.txt", project_root);
    
    FILE *gf = fopen(path, "w");
    if (!gf) return;

    fprintf(gf, "project_list=");
    for (int i = 0; i < project_count; i++) {
        int label_len = strlen(projects[i]);
        int padding = 45 - label_len;
        if (padding < 0) padding = 0;
        
        fprintf(gf, "<text label=\"║  \" /><button label=\"%s\" onClick=\"LOAD_PROJECT:%s\" />", projects[i], projects[i]);
        if (padding > 0) {
            fprintf(gf, "<text label=\"");
            for (int p = 0; p < padding; p++) fprintf(gf, " ");
            fprintf(gf, "\" />");
        }
        fprintf(gf, "<text label=\" ║\" /><br/>");
    }
    
    if (project_count == 0) {
        fprintf(gf, "<text label=\"║  [No Projects Found]                     ║\" /><br/>");
    }
    
    fprintf(gf, "\npiece_methods=\n");
    fclose(gf);

    char *sc = NULL;
    if (asprintf(&sc, "%s/pieces/apps/player_app/state_changed.txt", project_root) != -1) {
        FILE *f = fopen(sc, "a");
        if (f) { fprintf(f, "S\n"); fclose(f); }
        free(sc);
    }
}

int main() {
    resolve_paths();
    scan_projects();
    update_loader_pdl();
    write_gui_state();

    char *app_state = NULL;
    if (asprintf(&app_state, "%s/pieces/apps/player_app/state.txt", project_root) != -1) {
        FILE *f = fopen(app_state, "r+");
        if (f) {
            char line[MAX_LINE];
            int has_target = 0;
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "active_target_id=", 17) == 0) has_target = 1;
            }
            if (!has_target) {
                fseek(f, 0, SEEK_END);
                fprintf(f, "active_target_id=loader\n");
            }
            fclose(f);
        }
        free(app_state);
    }

    while (1) {
        usleep(1000000); 
    }

    return 0;
}
