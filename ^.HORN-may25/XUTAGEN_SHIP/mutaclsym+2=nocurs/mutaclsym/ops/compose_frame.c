/* compose_frame - one verb, one binary, no shared headers.
 * Renders current game state (map + hero position + turn) into a plain
 * text frame at pieces/display/current_frame.txt. Does NOT touch a
 * terminal itself - rendering is the renderer process's job, reading
 * the file this op writes. Self-contained: own root resolution, own
 * constants, own copies of the map/state reading logic (same pattern
 * already used by move_player.c / end_turn.c). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)
#define MAP_W 40
#define MAP_H 16

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

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF], map_path[PATH_BUF], turn_path[PATH_BUF], out_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);
    snprintf(map_path, sizeof(map_path), "%s/pieces/world_01/map_start/map.txt", project_root);
    snprintf(turn_path, sizeof(turn_path), "%s/pieces/world_01/map_start/state.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/current_frame.txt", project_root);

    int px = read_kv_int(hero_path, "pos_x", 0);
    int py = read_kv_int(hero_path, "pos_y", 0);
    int turn = read_kv_int(turn_path, "turn", 0);

    FILE *out = fopen(out_path, "w");
    if (!out) return 1;

    fprintf(out, "MUTACLSYM   turn: %d\n", turn);

    FILE *mf = fopen(map_path, "r");
    if (mf) {
        char line[MAP_W + 4];
        int row = 0;
        while (fgets(line, sizeof(line), mf) && row < MAP_H) {
            line[strcspn(line, "\n")] = '\0';
            if (row == py) {
                /* overlay '@' at px, padding the line with spaces if needed */
                char rendered[MAP_W + 4];
                size_t len = strlen(line);
                snprintf(rendered, sizeof(rendered), "%s", line);
                while ((int)strlen(rendered) <= px && strlen(rendered) < sizeof(rendered) - 1) {
                    strcat(rendered, " ");
                }
                if (px >= 0 && px < (int)sizeof(rendered) - 1) rendered[px] = '@';
                fprintf(out, "%s\n", rendered);
                (void)len;
            } else {
                fprintf(out, "%s\n", line);
            }
            row++;
        }
        fclose(mf);
    }

    fprintf(out, "[wasd/arrows] move  [q] quit\n");
    fclose(out);
    return 0;
}
