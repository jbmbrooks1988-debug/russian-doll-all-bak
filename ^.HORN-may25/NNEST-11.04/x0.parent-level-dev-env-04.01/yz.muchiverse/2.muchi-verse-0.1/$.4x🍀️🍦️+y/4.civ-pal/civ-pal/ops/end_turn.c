/* end_turn - one verb, one binary, no shared headers.
 * Increments the turn counter on the hero's CURRENT map (read from
 * hero/state.txt's map_id field - turn counters are per-map, so a map
 * the hero isn't on doesn't advance while they're elsewhere), and ticks
 * the hero's own metabolism: hunger/thirst rise every turn, stamina
 * regenerates, and crossing the starvation/dehydration threshold costs
 * HP each turn until food/drink brings the level back down (see
 * ops/eat.c). Capped so they don't grow without bound while still
 * being clearly worse the longer they're ignored. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)

#define HUNGER_PER_TURN 1
#define THIRST_PER_TURN 2
#define STAMINA_REGEN_PER_TURN 5
#define METABOLISM_CAP 200
#define STARVING_THRESHOLD 100
#define DEHYDRATED_THRESHOLD 100
#define STARVING_HP_LOSS 1
#define DEHYDRATED_HP_LOSS 2

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_hero_interact_mode(const char *hero_path) {
    FILE *f = fopen(hero_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int im = 0;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, "interact_mode") == 0) { im = atoi(eq + 1); break; }
    }
    fclose(f);
    return im;
}

static void read_hero_map_id(const char *hero_path, char *out, size_t out_sz) {
    snprintf(out, out_sz, "map_start");
    FILE *f = fopen(hero_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, "map_id") == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

static void tick_map_turn(const char *map_id) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_muchi_civ_home/%s/state.txt", project_root, map_id);
    FILE *f = fopen(path, "r");
    if (!f) return;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int turn = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "turn") == 0) turn = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    turn++;

    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "turn") == 0) { fprintf(f, "turn=%d\n", turn); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void tick_hero_metabolism(const char *hero_path) {
    FILE *f = fopen(hero_path, "r");
    if (!f) return;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int hp = 100, hunger = 0, thirst = 0, stamina = 100;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "hp") == 0) hp = atoi(eq + 1);
            else if (strcmp(lines[nlines], "hunger") == 0) hunger = atoi(eq + 1);
            else if (strcmp(lines[nlines], "thirst") == 0) thirst = atoi(eq + 1);
            else if (strcmp(lines[nlines], "stamina") == 0) stamina = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    hunger = clamp(hunger + HUNGER_PER_TURN, 0, METABOLISM_CAP);
    thirst = clamp(thirst + THIRST_PER_TURN, 0, METABOLISM_CAP);
    stamina = clamp(stamina + STAMINA_REGEN_PER_TURN, 0, 100);
    if (hunger >= STARVING_THRESHOLD) hp = clamp(hp - STARVING_HP_LOSS, 0, 100);
    if (thirst >= DEHYDRATED_THRESHOLD) hp = clamp(hp - DEHYDRATED_HP_LOSS, 0, 100);

    f = fopen(hero_path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "hp") == 0) { fprintf(f, "hp=%d\n", hp); *eq = '='; continue; }
            if (strcmp(lines[i], "hunger") == 0) { fprintf(f, "hunger=%d\n", hunger); *eq = '='; continue; }
            if (strcmp(lines[i], "thirst") == 0) { fprintf(f, "thirst=%d\n", thirst); *eq = '='; continue; }
            if (strcmp(lines[i], "stamina") == 0) { fprintf(f, "stamina=%d\n", stamina); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_muchi_civ_home/map_start/hero/state.txt", project_root);

    /* CORRECTED 2026-07-21 per !.pal-standards.txt sec. 28: this op
     * runs UNCONDITIONALLY every keypress (pal/main_loop.pal's own
     * "move_player x2 / choice x2 / end_turn" sequence, no key
     * filtering at that level) - but a NAV-mode keypress (menu
     * navigation, interact_mode==0) never reaches the actual game at
     * all in the real engine this was ported from (chtpm_parser.c only
     * ever raw-injects keys into the game's own input once its
     * INTERACT/ACTIVE state is engaged - see sec. 28's own citation).
     * Direct user report, live-tested: hunger was rising on plain NAV-
     * mode arrow presses that move_player.c already correctly no-ops
     * for (sec. 28.4) - this was the other half of that same bug, a
     * turn/metabolism tick with no corresponding game action behind it.
     * Gate BOTH the map turn counter and hero metabolism on
     * interact_mode==1 - a turn only genuinely happens while the player
     * is actually controlling something (the free cursor or a
     * possessed unit), never while just navigating a menu. */
    if (!read_hero_interact_mode(hero_path)) return 0;

    char map_id[64];
    read_hero_map_id(hero_path, map_id, sizeof(map_id));

    tick_map_turn(map_id);
    tick_hero_metabolism(hero_path);
    return 0;
}
