#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";

void resolve_root() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                snprintf(project_root, sizeof(project_root), "%s", v);
                break;
            }
        }
        fclose(kvp);
    }
}

char* get_active_user() {
    char session_dir[MAX_PATH];
    snprintf(session_dir, sizeof(session_dir), "%s/projects/user/pieces/session", project_root);
    DIR *dir = opendir(session_dir);
    if (!dir) return NULL;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char *dot = strrchr(entry->d_name, '.');
        if (dot && strcmp(dot, ".session") == 0) {
            char *user = strdup(entry->d_name);
            user[dot - entry->d_name] = '\0';
            closedir(dir);
            return user;
        }
    }
    closedir(dir);
    return NULL;
}

void ensure_dir(const char *path) {
    char tmp[MAX_PATH];
    char *p = NULL;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            *p = '/';
        }
    }
    mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <cmd:save|load|new> <project_id> [slot_name]\n", argv[0]);
        return 1;
    }
    resolve_root();
    const char *cmd = argv[1];
    const char *project_id = argv[2];
    char slot_name[256] = "default";
    if (argc >= 4) strncpy(slot_name, argv[3], 255);

    char *user = get_active_user();
    if (!user) {
        printf("Error: No active user session found.\n");
        return 1;
    }

    char save_base[MAX_PATH];
    snprintf(save_base, sizeof(save_base), "%s/projects/user/pieces/profiles/%s/saves/%s/%s", project_root, user, project_id, slot_name);
    
    char project_pieces[MAX_PATH];
    snprintf(project_pieces, sizeof(project_pieces), "%s/projects/%s/pieces", project_root, project_id);

    char shell_cmd[MAX_PATH * 3];
    if (strcmp(cmd, "save") == 0) {
        ensure_dir(save_base);
        snprintf(shell_cmd, sizeof(shell_cmd), "cp -r '%s'/* '%s'/", project_pieces, save_base);
        system(shell_cmd);
        printf("Project '%s' saved to slot '%s' for user '%s'.\n", project_id, slot_name, user);
    } else if (strcmp(cmd, "load") == 0) {
        if (access(save_base, F_OK) != 0) {
            printf("Error: Save slot '%s' not found.\n", slot_name);
            free(user);
            return 1;
        }
        // Backup current before wipe? Maybe later.
        snprintf(shell_cmd, sizeof(shell_cmd), "rm -rf '%s'/* && cp -r '%s'/* '%s'/", project_pieces, save_base, project_pieces);
        system(shell_cmd);
        printf("Project '%s' loaded from slot '%s' for user '%s'.\n", project_id, slot_name, user);
    } else if (strcmp(cmd, "new") == 0) {
        char template_pieces[MAX_PATH];
        snprintf(template_pieces, sizeof(template_pieces), "%s/projects/template/pieces", project_root);
        if (access(template_pieces, F_OK) != 0) {
            // Fallback: just clear current pieces if no template
            snprintf(shell_cmd, sizeof(shell_cmd), "rm -rf '%s'/*", project_pieces);
        } else {
            snprintf(shell_cmd, sizeof(shell_cmd), "rm -rf '%s'/* && cp -r '%s'/* '%s'/", project_pieces, template_pieces, project_pieces);
        }
        system(shell_cmd);
        printf("Project '%s' reset to new state.\n", project_id);
    } else if (strcmp(cmd, "delete") == 0) {
        if (access(save_base, F_OK) == 0) {
            snprintf(shell_cmd, sizeof(shell_cmd), "rm -rf '%s'", save_base);
            system(shell_cmd);
            printf("Save slot '%s' deleted for user '%s'.\n", slot_name, user);
        } else {
            printf("Error: Save slot '%s' not found.\n", slot_name);
            free(user);
            return 1;
        }
    }

    // Update last_response for the project
    char resp_path[MAX_PATH];
    snprintf(resp_path, sizeof(resp_path), "%s/projects/%s/pieces/fuzzpet/last_response.txt", project_root, project_id);
    FILE *f = fopen(resp_path, "w");
    if (f) {
        fprintf(f, "File: %s %s", cmd, slot_name);
        fclose(f);
    }

    // Pulse frame
    char pulse[MAX_PATH];
    snprintf(pulse, sizeof(pulse), "%s/pieces/display/frame_changed.txt", project_root);
    f = fopen(pulse, "a");
    if (f) { fprintf(f, "F\n"); fclose(f); }

    free(user);
    return 0;
}
