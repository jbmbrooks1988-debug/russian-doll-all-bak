/* move_player - one verb, one binary, no shared headers.
 * argv[1] = raw keycode (decimal). Moves the hero (always at
 * pieces/world_01/map_start/hero/ - its own map_id FIELD tracks which
 * map it's logically on, per the "legacy map_id field" resolution rule
 * in !.world_architecture+1=rusindol.txt, not its directory location) if
 * the target cell is walkable on the hero's CURRENT map, teleports it to
 * another map if the target cell is a transition tile (see
 * transitions.txt), or - if a monster piece physically sits at the
 * target cell - attacks it instead of moving (matching
 * tick_monsters.c's own move-or-attack logic in reverse). Self-contained:
 * resolves its own root, defines its own constants, reads its own copies
 * of the map/registry. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#define MAX_LINE 512
#define MAX_PATH 4096
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)
/* MAX_MAP_W/MAX_MAP_H are generous compile-time buffer-size caps, NOT
 * the real per-map dimensions any more - every map's actual width/
 * height now comes from its own state.txt at runtime (read_map_dims()
 * below), matching dox/01-cdda-architecture.md §5a exactly: this is
 * the blocking prerequisite for any map bigger than the old fixed
 * 40x16, done before authoring one. */
#define MAX_MAP_W 256
#define MAX_MAP_H 256
#define HERO_ATTACK_DAMAGE 5

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

/* Shared by terrain and furniture lookups - both registries use the same
 * glyph|id|name|walkable format. Local reuse within one file is fine per
 * doctrine; this is not a shared header across files. */
static int glyph_walkable(const char *registry_rel_path, char glyph) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
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

/* Shared by map.txt (terrain) and furniture.txt (furniture) reads - same
 * fixed-width-row-file layout, different out-of-bounds/missing default
 * per caller ('#' for terrain = blocked, ' ' for furniture = nothing
 * there, not blocked). map_w/map_h are THIS map's own real dimensions
 * (read from its state.txt by the caller), not a fixed constant. */
static char file_glyph_at(const char *abs_path, int x, int y, char default_glyph, int map_w, int map_h) {
    if (x < 0 || y < 0 || x >= map_w || y >= map_h) return default_glyph;
    FILE *f = fopen(abs_path, "r");
    if (!f) return default_glyph;
    char line[MAX_MAP_W + 4];
    char glyph = default_glyph;
    for (int row = 0; row <= y; row++) {
        if (!fgets(line, sizeof(line), f)) { glyph = default_glyph; break; }
        if (row == y) glyph = (x < (int)strlen(line)) ? line[x] : default_glyph;
    }
    fclose(f);
    return glyph;
}

static int furniture_walkable(char glyph) {
    if (glyph == ' ') return 1; /* no furniture here - terrain alone decides */
    return glyph_walkable("pieces/registry/furniture/furniture_types.txt", glyph);
}

/* transitions.txt format: x|y|dest_map_id|dest_x|dest_y (one per line).
 * Returns 1 and fills the destination fields if (x,y) on the given map
 * has a transition; 0 otherwise. */
static int find_transition(const char *transitions_path, int x, int y,
                            char *dest_map, size_t dest_map_sz, int *dest_x, int *dest_y) {
    FILE *f = fopen(transitions_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int tx, ty, dx_, dy_;
        char map_buf[64];
        if (sscanf(line, "%d|%d|%63[^|]|%d|%d", &tx, &ty, map_buf, &dx_, &dy_) == 5) {
            if (tx == x && ty == y) {
                snprintf(dest_map, dest_map_sz, "%s", map_buf);
                *dest_x = dx_;
                *dest_y = dy_;
                found = 1;
                break;
            }
        }
    }
    fclose(f);
    return found;
}

static int read_int_field(const char *path, const char *key, int def) {
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

static void read_str_field(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
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

static void monster_name(const char *monster_type, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", monster_type);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *end = strchr(name, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", name);
        break;
    }
    fclose(f);
}

/* Finds a monster piece at (x,y) under monsters_dir. Returns 1 and fills
 * state_path/hp if found, 0 otherwise. */
static int find_monster_at(const char *monsters_dir, int x, int y, char *state_path, size_t state_path_sz, int *hp) {
    DIR *d = opendir(monsters_dir);
    if (!d) return 0;
    struct dirent *entry;
    int found = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char candidate[PATH_BUF + 384];
        snprintf(candidate, sizeof(candidate), "%s/%s/state.txt", monsters_dir, entry->d_name);
        int mx = read_int_field(candidate, "pos_x", -1);
        int my = read_int_field(candidate, "pos_y", -1);
        if (mx == x && my == y) {
            snprintf(state_path, state_path_sz, "%s", candidate);
            *hp = read_int_field(candidate, "hp", 1);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
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
    char map_id[64] = "map_start";
    char active_panel[32] = "none";
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "pos_x") == 0) px = atoi(eq + 1);
            else if (strcmp(lines[nlines], "pos_y") == 0) py = atoi(eq + 1);
            else if (strcmp(lines[nlines], "map_id") == 0) {
                char *v = eq + 1;
                v[strcspn(v, "\n")] = '\0';
                snprintf(map_id, sizeof(map_id), "%s", v);
            } else if (strcmp(lines[nlines], "active_panel") == 0) {
                char *v = eq + 1;
                v[strcspn(v, "\n")] = '\0';
                snprintf(active_panel, sizeof(active_panel), "%s", v);
            }
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    /* An open overlay panel (e.g. the crafting list - see ops/choice.c's
     * panel-mode handling) captures input the same way real CDDA's own
     * menus do: movement is suspended while it's open, matching
     * gl_desktop.c's is_map_control mode switch (confirmed via research
     * into wraith's overlay rendering) rather than letting the hero
     * silently walk around behind an open menu. */
    if (strcmp(active_panel, "none") != 0) return 0;

    int nx = px + dx, ny = py + dy;

    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_01/%s", project_root, map_id);
    char map_path[PATH_BUF + 32], furniture_path[PATH_BUF + 32], transitions_path[PATH_BUF + 32];
    char monsters_dir[PATH_BUF + 32], map_state_path[PATH_BUF + 32];
    snprintf(map_path, sizeof(map_path), "%s/map.txt", map_dir);
    snprintf(furniture_path, sizeof(furniture_path), "%s/furniture.txt", map_dir);
    snprintf(transitions_path, sizeof(transitions_path), "%s/transitions.txt", map_dir);
    snprintf(monsters_dir, sizeof(monsters_dir), "%s/monsters", map_dir);
    snprintf(map_state_path, sizeof(map_state_path), "%s/state.txt", map_dir);
    int map_w = read_int_field(map_state_path, "width", 40);
    int map_h = read_int_field(map_state_path, "height", 16);

    char msg[128] = "";
    char monster_state_path[PATH_BUF + 384];
    int monster_hp;
    if (find_monster_at(monsters_dir, nx, ny, monster_state_path, sizeof(monster_state_path), &monster_hp)) {
        /* Attack, don't move - matches tick_monsters.c's own
         * move-or-attack logic in reverse. */
        char monster_type[64], name[64];
        read_str_field(monster_state_path, "monster_type", monster_type, sizeof(monster_type), "zombie");
        monster_name(monster_type, name, sizeof(name));
        monster_hp -= HERO_ATTACK_DAMAGE;
        if (monster_hp <= 0) {
            /* Killed - delete the piece outright, same as eat.c consuming
             * a food item, not moved anywhere. */
            char *dir_end = strrchr(monster_state_path, '/');
            char monster_dir[PATH_BUF + 384];
            if (dir_end) { size_t len = dir_end - monster_state_path; snprintf(monster_dir, sizeof(monster_dir), "%.*s", (int)len, monster_state_path); }
            else snprintf(monster_dir, sizeof(monster_dir), "%s", monster_state_path);
            remove(monster_state_path);
            rmdir(monster_dir);
            snprintf(msg, sizeof(msg), "You kill the %s!", name);
        } else {
            FILE *mf = fopen(monster_state_path, "r");
            char mlines[16][MAX_LINE];
            int mnlines = 0;
            if (mf) { while (mnlines < 16 && fgets(mlines[mnlines], MAX_LINE, mf)) mnlines++; fclose(mf); }
            mf = fopen(monster_state_path, "w");
            if (mf) {
                for (int i = 0; i < mnlines; i++) {
                    if (strncmp(mlines[i], "hp", 2) == 0 && mlines[i][2] == '=') { fprintf(mf, "hp=%d\n", monster_hp); continue; }
                    fputs(mlines[i], mf);
                }
                fclose(mf);
            }
            snprintf(msg, sizeof(msg), "You hit the %s for %d.", name, HERO_ATTACK_DAMAGE);
        }
    } else {
        char dest_map[64];
        int dest_x = 0, dest_y = 0;
        if (find_transition(transitions_path, nx, ny, dest_map, sizeof(dest_map), &dest_x, &dest_y)) {
            /* Map change - land at the destination map's entry point.
             * Does NOT also run the normal walkability check against the
             * old map's tile at (nx,ny); the transition tile is always
             * walkable-in-spirit (it's how you leave). */
            snprintf(map_id, sizeof(map_id), "%s", dest_map);
            px = dest_x;
            py = dest_y;
        } else {
            char terrain_glyph = file_glyph_at(map_path, nx, ny, '#', map_w, map_h);
            char furniture_glyph = file_glyph_at(furniture_path, nx, ny, ' ', map_w, map_h);
            if (glyph_walkable("pieces/registry/terrain/terrain_types.txt", terrain_glyph) &&
                furniture_walkable(furniture_glyph)) {
                px = nx;
                py = ny;
            }
        }
    }

    f = fopen(path, "w");
    if (!f) return 1;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "pos_x") == 0) { fprintf(f, "pos_x=%d\n", px); *eq = '='; continue; }
            if (strcmp(lines[i], "pos_y") == 0) { fprintf(f, "pos_y=%d\n", py); *eq = '='; continue; }
            if (strcmp(lines[i], "map_id") == 0) { fprintf(f, "map_id=%s\n", map_id); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);

    if (msg[0]) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "%s\n", msg); fclose(lf); }
    }
    return 0;
}
