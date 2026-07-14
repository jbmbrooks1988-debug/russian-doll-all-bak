/* buy_egg - one verb, one binary, no shared headers.
 * The store's "buy" verb: checks/deducts EGG_COST tokens from an owner
 * piece, then shells out to the existing generate_egg.+x to actually
 * mint the egg - reuses that op's species-pick/serial/piece-creation
 * logic rather than duplicating it, matching the project's own
 * piece_manager-style "call the other op" convention.
 *
 * Usage: buy_egg.+x <owner_piece_id>
 * Prints a one-line result message to stdout. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define EGG_COST 20

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_write_tokens(const char *owner_id, int delta, int *out_balance) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, owner_id);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int tokens = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "tokens") == 0) tokens = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    if (tokens + delta < 0) { *out_balance = tokens; return 0; }
    tokens += delta;

    f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "tokens") == 0) { fprintf(f, "tokens=%d\n", tokens); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);

    *out_balance = tokens;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <owner_piece_id>\n", argv[0]);
        return 1;
    }
    const char *owner_id = argv[1];
    resolve_root();

    int balance = 0;
    if (!read_write_tokens(owner_id, -EGG_COST, &balance)) {
        printf("Buy failed: need %d tokens, have %d.\n", EGG_COST, balance);
        return 1;
    }

    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/generate_egg.+x' '%s'", project_root, owner_id);
    FILE *pf = popen(cmd, "r");
    if (!pf) {
        printf("Buy failed: could not mint egg.\n");
        /* Refund - the deduction above already happened. */
        read_write_tokens(owner_id, EGG_COST, &balance);
        return 1;
    }
    char egg_id[64] = "";
    if (!fgets(egg_id, sizeof(egg_id), pf)) egg_id[0] = '\0';
    pclose(pf);
    egg_id[strcspn(egg_id, "\n")] = '\0';

    if (egg_id[0] == '\0') {
        printf("Buy failed: mint returned nothing.\n");
        read_write_tokens(owner_id, EGG_COST, &balance);
        return 1;
    }

    printf("Bought %s! Balance: %d\n", egg_id, balance);
    return 0;
}
