/* end_turn - one verb, one binary, no shared headers.
 * Increments the turn counter in pieces/world_01/map_start/state.txt.
 * Phase 0: no monster AI tick yet, just the counter, so the loop shape
 * (move -> end_turn -> redraw) is proven before anything else hangs
 * off of it. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(void) {
    resolve_root();

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_start/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 1;

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
    if (!f) return 1;
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
    return 0;
}
