/* pickup - one verb, one binary, no shared headers.
 * Grabs the first ground item at hero position and moves it into
 * hero/inventory/. Uses rename() per mutaclsym doctrine. Self-contained. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

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

static void item_name(const char *item_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", item_id);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/items/items.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(item_id) || strncmp(line, item_id, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *end = strchr(name, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", name);
        break;
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

    char items_dir[PATH_BUF + 32];
    snprintf(items_dir, sizeof(items_dir), "%s/pieces/world_tempt_island/%s/items", project_root, map_id);
    DIR *d = opendir(items_dir);
    if (!d) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "Nothing to pick up here.\n"); fclose(lf); }
        return 0;
    }

    char picked_id[64] = "", picked_name[64] = "";
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 320];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", items_dir, entry->d_name);
        int ix = read_kv_int(state_path, "pos_x", -1);
        int iy = read_kv_int(state_path, "pos_y", -1);
        if (ix == hero_x && iy == hero_y) {
            read_kv_str(state_path, "item_id", picked_id, sizeof(picked_id), "?");
            item_name(picked_id, picked_name, sizeof(picked_name));
            snprintf(picked_id, sizeof(picked_id), "%s", entry->d_name);
            break;
        }
    }
    closedir(d);

    if (!picked_id[0]) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "Nothing to pick up here.\n"); fclose(lf); }
        return 0;
    }

    char from[PATH_BUF + 64], to[PATH_BUF + 64];
    snprintf(from, sizeof(from), "%s/%s", items_dir, picked_id);
    snprintf(to, sizeof(to), "%s/pieces/world_tempt_island/map_start/hero/inventory/%s", project_root, picked_id);
    if (rename(from, to) == 0) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "You picked up a %s.\n", picked_name); fclose(lf); }
    } else {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "Failed to pick up item.\n"); fclose(lf); }
    }
    return 0;
}
