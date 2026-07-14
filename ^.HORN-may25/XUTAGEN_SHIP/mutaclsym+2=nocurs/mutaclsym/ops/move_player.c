/* move_player - one verb, one binary, no shared headers.
 * argv[1] = raw keycode (decimal). Moves pieces/world_01/map_start/hero
 * if the target cell is walkable per the terrain registry, rewriting
 * hero/state.txt in place. Self-contained: resolves its own root,
 * defines its own constants, reads its own copies of the map/registry. */
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

/* Must match the sentinel values keyboard_input.c writes to history.txt
 * for arrow keys - no ncurses anywhere in this project, so these are
 * plain agreed-upon integers, not KEY_* macros from a curses header. */
#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int terrain_walkable(char glyph) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/terrain/terrain_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return glyph == '.'; /* sane fallback if registry is missing */
    char line[MAX_LINE];
    int result = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (line[0] != glyph) continue;
        /* glyph|id|name|walkable */
        char *p = strchr(line, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        result = atoi(p + 1);
        break;
    }
    fclose(f);
    return result;
}

static char map_glyph_at(int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return '#';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_start/map.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return '#';
    char line[MAP_W + 4];
    char glyph = '#';
    for (int row = 0; row <= y; row++) {
        if (!fgets(line, sizeof(line), f)) { glyph = '#'; break; }
        if (row == y) glyph = (x < (int)strlen(line)) ? line[x] : '#';
    }
    fclose(f);
    return glyph;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    int key = atoi(argv[1]);
    int dx = 0, dy = 0;
    switch (key) {
        case 'w': case ARROW_UP:    dy = -1; break;
        case 's': case ARROW_DOWN:  dy =  1; break;
        case 'a': case ARROW_LEFT:  dx = -1; break;
        case 'd': case ARROW_RIGHT: dx =  1; break;
        default: return 0; /* not a movement key: no-op */
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 1;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int px = 0, py = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "pos_x") == 0) px = atoi(eq + 1);
            if (strcmp(lines[nlines], "pos_y") == 0) py = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    int nx = px + dx, ny = py + dy;
    if (terrain_walkable(map_glyph_at(nx, ny))) {
        px = nx;
        py = ny;
    }

    f = fopen(path, "w");
    if (!f) return 1;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "pos_x") == 0) { fprintf(f, "pos_x=%d\n", px); *eq = '='; continue; }
            if (strcmp(lines[i], "pos_y") == 0) { fprintf(f, "pos_y=%d\n", py); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);
    return 0;
}
