/* talk - one verb, one binary, no shared headers.
 * Finds adjacent NPCs and logs a canned greeting. Self-contained:
 * no shared headers. Standalone copy of the registry/path-resolution
 * logic. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_tempt_island/map_start/hero/state.txt", project_root);
    int hero_x = read_kv_int(hero_path, "pos_x", 0);
    int hero_y = read_kv_int(hero_path, "pos_y", 0);
    char map_id[64];
    read_kv_str(hero_path, "map_id", map_id, sizeof(map_id), "map_start");

    char npcs_dir[PATH_BUF + 32];
    snprintf(npcs_dir, sizeof(npcs_dir), "%s/pieces/world_tempt_island/%s/npcs", project_root, map_id);
    DIR *d = opendir(npcs_dir);
    if (!d) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "No one nearby.\n"); fclose(lf); }
        return 0;
    }

    char found_name[64] = "";
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 320];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", npcs_dir, entry->d_name);
        int nx = read_kv_int(state_path, "pos_x", -999);
        int ny = read_kv_int(state_path, "pos_y", -999);
        /* Adjacent = Manhattan distance <= 1 (orthogonal or self, but
         * not self in this case since hero isn't in npcs/ dir) */
        if (abs(nx - hero_x) + abs(ny - hero_y) <= 1) {
            read_kv_str(state_path, "name", found_name, sizeof(found_name), "?");
            break;
        }
    }
    closedir(d);

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *lf = fopen(log_path, "a");
    if (lf) {
        if (found_name[0]) {
            fprintf(lf, "%s: Hey! How are you doing?\n", found_name);
        } else {
            fprintf(lf, "No one nearby.\n");
        }
        fclose(lf);
    }
    return 0;
}
