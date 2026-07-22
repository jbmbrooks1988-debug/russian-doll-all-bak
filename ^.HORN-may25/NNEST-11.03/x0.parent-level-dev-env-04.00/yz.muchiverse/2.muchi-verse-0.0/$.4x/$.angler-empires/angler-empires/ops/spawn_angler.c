/* spawn_angler - one verb, one binary, no shared headers.
 * Creates ONE living angler instance from an angler_bank.txt row - the
 * "spawn" half of ANGLER-EXPANSES-DESIGN.txt sec. 3's bank-vs-instance-
 * dir split (zest-er-summary sec. 4.4). angler_bank.txt itself is never
 * mutated - this op reads one row from it once, then writes a real,
 * separately-mutable pieces/<world_dir>/map_start/anglers/<instance_id>/
 * {piece.pdl,state.txt} pair, same two-file shape hero/ already uses
 * (piece.pdl = static METHOD table + a couple of mirrored STATE fields
 * for tooling that expects piece.pdl to be the source of truth;
 * state.txt = the real, live, per-tick-mutated fields - matches hero's
 * own existing split exactly, not a new convention).
 *
 * Idle-labor AI (move_player.c-equivalent for anglers), job assignment,
 * and tick-driven advancement are NOT this op's job - this op only
 * ever runs ONCE per living angler, at spawn time. Everything after
 * that reads/writes the instance's own state.txt directly, never
 * angler_bank.txt again (design doc sec. 3's own "the bank is read-only
 * template data once the game is running" rule, restated in
 * HANDOFF-4X-FAMILY.txt sec. 3.1 step 3).
 *
 * Usage: spawn_angler.+x <bank_id> <instance_id> <world_dir> <spawn_x> <spawn_y>
 *   bank_id      - a row id in pieces/registry/anglers/angler_bank.txt
 *   instance_id  - this living angler's own unique id (caller's choice,
 *                  e.g. "angler_01" - not derived from bank_id, since a
 *                  bank_id like ang_miner could in principle be spawned
 *                  more than once per tribe later - not true today with
 *                  only 7 fixed rows, but the op doesn't hardcode that
 *                  assumption).
 *   world_dir    - which world's own map_start this instance belongs to,
 *                  e.g. "world_angler_home" (matches every other op's
 *                  own <world_dir>/map_start/... convention).
 *   spawn_x/y    - starting position (caller's choice, not read from
 *                  the bank - the bank has no position column, position
 *                  is instance state from the very first tick).
 *
 * Exit 0 on success, 1 on bad args, 2 if bank_id not found in
 * angler_bank.txt, 3 if an instance dir with that instance_id already
 * exists (never silently overwrite a living instance's own state). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int mkdir_p(const char *path) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755); /* final component; EEXIST is fine */
}

/* angler_bank.txt row: id|name|glyph|color|is_player|role|
 * default_outfit|tribe_id - see that file's own header comment. */
typedef struct {
    char id[64];
    char name[64];
    char glyph[8];
    char color[32];
    int is_player;
    char role[32];
    char default_outfit[64];
    char tribe_id[64];
} BankRow;

static int find_bank_row(const char *bank_path, const char *bank_id, BankRow *out) {
    FILE *f = fopen(bank_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char buf[MAX_LINE];
        snprintf(buf, sizeof(buf), "%s", line);
        buf[strcspn(buf, "\r\n")] = '\0';

        char *fields[8] = {0};
        int nf = 0;
        char *tok = strtok(buf, "|");
        while (tok && nf < 8) { fields[nf++] = tok; tok = strtok(NULL, "|"); }
        if (nf < 8) continue;
        if (strcmp(fields[0], bank_id) != 0) continue;

        snprintf(out->id, sizeof(out->id), "%s", fields[0]);
        snprintf(out->name, sizeof(out->name), "%s", fields[1]);
        snprintf(out->glyph, sizeof(out->glyph), "%s", fields[2]);
        snprintf(out->color, sizeof(out->color), "%s", fields[3]);
        out->is_player = atoi(fields[4]);
        snprintf(out->role, sizeof(out->role), "%s", fields[5]);
        snprintf(out->default_outfit, sizeof(out->default_outfit), "%s", fields[6]);
        snprintf(out->tribe_id, sizeof(out->tribe_id), "%s", fields[7]);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: spawn_angler.+x <bank_id> <instance_id> <world_dir> <spawn_x> <spawn_y>\n");
        return 1;
    }
    const char *bank_id = argv[1];
    const char *instance_id = argv[2];
    const char *world_dir = argv[3];
    int spawn_x = atoi(argv[4]);
    int spawn_y = atoi(argv[5]);

    resolve_root();

    char bank_path[PATH_BUF];
    snprintf(bank_path, sizeof(bank_path), "%s/pieces/registry/anglers/angler_bank.txt", project_root);

    BankRow row;
    if (!find_bank_row(bank_path, bank_id, &row)) {
        fprintf(stderr, "spawn_angler: bank_id '%s' not found in %s\n", bank_id, bank_path);
        return 2;
    }

    char instance_dir[PATH_BUF];
    snprintf(instance_dir, sizeof(instance_dir), "%s/pieces/%s/map_start/anglers/%s",
             project_root, world_dir, instance_id);

    struct stat st;
    if (stat(instance_dir, &st) == 0) {
        fprintf(stderr, "spawn_angler: instance '%s' already exists at %s, refusing to overwrite\n",
                instance_id, instance_dir);
        return 3;
    }
    if (mkdir_p(instance_dir) != 0 && errno != EEXIST) {
        fprintf(stderr, "spawn_angler: could not create %s\n", instance_dir);
        return 3;
    }

    char piece_pdl_path[PATH_BUF];
    snprintf(piece_pdl_path, sizeof(piece_pdl_path), "%s/piece.pdl", instance_dir);
    FILE *pf = fopen(piece_pdl_path, "w");
    if (pf) {
        fprintf(pf, "SECTION      | KEY                | VALUE\n");
        fprintf(pf, "----------------------------------------\n");
        fprintf(pf, "META         | piece_id           | %s\n", instance_id);
        fprintf(pf, "META         | version            | 1.0\n\n");
        fprintf(pf, "STATE        | name                 | %s\n", row.name);
        fprintf(pf, "STATE        | pos_x                | %d\n", spawn_x);
        fprintf(pf, "STATE        | pos_y                | %d\n", spawn_y);
        fprintf(pf, "STATE        | map_id               | map_start\n\n");
        fprintf(pf, "METHOD       | move                 | ops/+x/move_player.+x\n");
        fprintf(pf, "METHOD       | end_turn             | ops/+x/end_turn.+x\n");
        fprintf(pf, "METHOD       | activity             | ops/+x/activity.+x\n");
        fprintf(pf, "METHOD       | talk                 | ops/+x/talk.+x\n\n");
        fprintf(pf, "RESPONSE     | default              | *waiting...*\n");
        fclose(pf);
    }

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", instance_dir);
    FILE *sf = fopen(state_path, "w");
    if (sf) {
        fprintf(sf, "bank_id=%s\n", row.id);
        fprintf(sf, "name=%s\n", row.name);
        fprintf(sf, "glyph=%s\n", row.glyph);
        fprintf(sf, "color=%s\n", row.color);
        fprintf(sf, "is_player=%d\n", row.is_player);
        fprintf(sf, "role=%s\n", row.role);
        fprintf(sf, "outfit_id=%s\n", row.default_outfit);
        fprintf(sf, "tribe_id=%s\n", row.tribe_id);
        fprintf(sf, "pos_x=%d\n", spawn_x);
        fprintf(sf, "pos_y=%d\n", spawn_y);
        fprintf(sf, "pos_z=0\n");
        fprintf(sf, "map_id=map_start\n");
        fprintf(sf, "hp=100\n");
        fprintf(sf, "job_id=none\n");
        fprintf(sf, "job_target_x=-1\n");
        fprintf(sf, "job_target_y=-1\n");
        fprintf(sf, "decision_mode=0\n");
        fclose(sf);
    } else {
        fprintf(stderr, "spawn_angler: could not write %s\n", state_path);
        return 3;
    }

    printf("spawned %s (%s, role=%s) as instance '%s' in %s at (%d,%d)\n",
           row.name, row.id, row.role, instance_id, world_dir, spawn_x, spawn_y);
    return 0;
}
