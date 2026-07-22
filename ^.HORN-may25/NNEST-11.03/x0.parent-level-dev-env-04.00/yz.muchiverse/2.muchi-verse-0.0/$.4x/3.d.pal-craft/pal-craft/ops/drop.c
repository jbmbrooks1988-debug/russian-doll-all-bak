/* drop - one verb, one binary, no shared headers.
 * Drops first inventory item at hero position using rename().
 * Self-contained. */
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

static void write_kv(FILE *out, const char *key, const char *value) {
    fprintf(out, "%s=%s\n", key, value);
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
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_pal_craft_home/map_start/hero/state.txt", project_root);
    int hero_x = read_kv_int(hero_path, "pos_x", 0);
    int hero_y = read_kv_int(hero_path, "pos_y", 0);
    char map_id[64];
    read_kv_str(hero_path, "map_id", map_id, sizeof(map_id), "map_start");

    char inv_dir[PATH_BUF];
    snprintf(inv_dir, sizeof(inv_dir), "%s/pieces/world_pal_craft_home/map_start/hero/inventory", project_root);
    DIR *d = opendir(inv_dir);
    if (!d) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "You have nothing to drop.\n"); fclose(lf); }
        return 0;
    }

    char dropped_dir[256] = "", item_id[64] = "", item_name_str[64] = "";
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(dropped_dir, sizeof(dropped_dir), "%s", entry->d_name);
        char state_path[PATH_BUF + 320];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inv_dir, entry->d_name);
        read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");
        item_name(item_id, item_name_str, sizeof(item_name_str));
        break;
    }
    closedir(d);

    if (!dropped_dir[0]) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "You have nothing to drop.\n"); fclose(lf); }
        return 0;
    }

    /* Update item's position to hero position */
    char inv_path[PATH_BUF + 64], state_path[PATH_BUF + 128];
    snprintf(inv_path, sizeof(inv_path), "%s/%s", inv_dir, dropped_dir);
    snprintf(state_path, sizeof(state_path), "%s/state.txt", inv_path);
    FILE *sf = fopen(state_path, "r");
    FILE *tmp = fopen("/tmp/drop_tmp.txt", "w");
    if (sf && tmp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), sf)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "pos_x") == 0) { fprintf(tmp, "pos_x=%d\n", hero_x); *eq = '='; continue; }
                if (strcmp(line, "pos_y") == 0) { fprintf(tmp, "pos_y=%d\n", hero_y); *eq = '='; continue; }
                *eq = '=';
            }
            fputs(line, tmp);
        }
        fclose(sf);
        fclose(tmp);
        rename("/tmp/drop_tmp.txt", state_path);
    }

    char items_dir[PATH_BUF + 32], to[PATH_BUF + 64];
    snprintf(items_dir, sizeof(items_dir), "%s/pieces/world_pal_craft_home/%s/items", project_root, map_id);
    snprintf(to, sizeof(to), "%s/%s", items_dir, dropped_dir);
    if (rename(inv_path, to) == 0) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "You dropped a %s.\n", item_name_str); fclose(lf); }
    } else {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "Failed to drop item.\n"); fclose(lf); }
    }
    return 0;
}
