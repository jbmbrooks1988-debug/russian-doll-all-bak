/* spawn_entity - one verb, one binary, no shared headers.
 * Creates ONE living unit instance from a unit_bank.txt row - the
 * "spawn" half of MUCHI-CIV-DESIGN.txt sec. 3's bank-vs-instance-dir
 * split (zest-er-summary sec. 4.4), same shape as angler-empires'
 * sibling ops/spawn_angler.c - see that file's own header comment for
 * the full split reasoning, not repeated here.
 *
 * Difference from spawn_angler.c: unit_bank.txt rows carry NO civ
 * ownership (any civ can build a Settler) - civ_id is supplied HERE,
 * at spawn time, as an instance-only field, not read from the bank.
 * Control model: same free-roaming cursor as angler-empires (direct
 * user note: "this is how civilization's own control system works
 * too") - no unit is ever "the player's hero," civ_id only gates who
 * may currently SELECT/command it via the cursor.
 *
 * Usage: spawn_entity.+x <bank_id> <instance_id> <world_dir> <civ_id> <spawn_x> <spawn_y>
 * Exit 0 success, 1 bad args, 2 bank_id not found, 3 instance exists /
 * write failure (never silently overwrite a living instance). */
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
    return mkdir(tmp, 0755);
}

/* unit_bank.txt row: id|name|glyph|color|category|move_range|attack|
 * defense|required_tech|build_cost - see that file's own header. */
typedef struct {
    char id[64];
    char name[64];
    char glyph[8];
    char color[32];
    char category[32];
    int move_range;
    int attack;
    int defense;
    char required_tech[64];
    int build_cost;
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

        char *fields[10] = {0};
        int nf = 0;
        char *tok = strtok(buf, "|");
        while (tok && nf < 10) { fields[nf++] = tok; tok = strtok(NULL, "|"); }
        if (nf < 10) continue;
        if (strcmp(fields[0], bank_id) != 0) continue;

        snprintf(out->id, sizeof(out->id), "%s", fields[0]);
        snprintf(out->name, sizeof(out->name), "%s", fields[1]);
        snprintf(out->glyph, sizeof(out->glyph), "%s", fields[2]);
        snprintf(out->color, sizeof(out->color), "%s", fields[3]);
        snprintf(out->category, sizeof(out->category), "%s", fields[4]);
        out->move_range = atoi(fields[5]);
        out->attack = atoi(fields[6]);
        out->defense = atoi(fields[7]);
        snprintf(out->required_tech, sizeof(out->required_tech), "%s", fields[8]);
        out->build_cost = atoi(fields[9]);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "usage: spawn_entity.+x <bank_id> <instance_id> <world_dir> <civ_id> <spawn_x> <spawn_y>\n");
        return 1;
    }
    const char *bank_id = argv[1];
    const char *instance_id = argv[2];
    const char *world_dir = argv[3];
    const char *civ_id = argv[4];
    int spawn_x = atoi(argv[5]);
    int spawn_y = atoi(argv[6]);

    resolve_root();

    char bank_path[PATH_BUF];
    snprintf(bank_path, sizeof(bank_path), "%s/pieces/registry/entities/entity_bank.txt", project_root);

    BankRow row;
    if (!find_bank_row(bank_path, bank_id, &row)) {
        fprintf(stderr, "spawn_entity: bank_id '%s' not found in %s\n", bank_id, bank_path);
        return 2;
    }

    char instance_dir[PATH_BUF];
    snprintf(instance_dir, sizeof(instance_dir), "%s/pieces/%s/map_start/entities/%s",
             project_root, world_dir, instance_id);

    struct stat st;
    if (stat(instance_dir, &st) == 0) {
        fprintf(stderr, "spawn_entity: instance '%s' already exists at %s, refusing to overwrite\n",
                instance_id, instance_dir);
        return 3;
    }
    if (mkdir_p(instance_dir) != 0 && errno != EEXIST) {
        fprintf(stderr, "spawn_entity: could not create %s\n", instance_dir);
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
        /* No "release" method here - CORRECTED 2026-07-21 per
         * !.pal-standards.txt sec. 30.4: relinquish is a fixed ESC
         * check in ops/choice.c (matching fuzz-op_manager.c's own
         * route_input()), not a numbered method dispatched off a
         * possessed piece's own piece.pdl - a "release" row here would
         * be dead code. */
        if (strcmp(row.category, "muchimon") == 0 || strcmp(row.category, "trainer") == 0) {
            /* MUCHIMON-DESIGN.txt sec. 8 ("trainers also have personal
             * health, skills... and can also be captured" - direct
             * design input, $.4x/!.tpmuchi-pal.txt) - a real numbered
             * method, dispatched via digit+Enter, special-cased by NAME
             * in ops/choice.c (same pattern as "interact"/"scan"
             * elsewhere in this family). Both muchimon AND trainer rows
             * get it now - NOT player (capturing yourself makes no
             * sense). Real capture is meant to be gated by the
             * capturing item's own "flexbox" power tier vs the target's
             * own strength (direct user note: "you will likely not have
             * a powerful enough flexbox starting out") - NOT built yet
             * (muchibox's own real design is explicitly deferred,
             * MUCHIMON-DESIGN.txt sec. 8.3/8.4), ops/choice.c's own
             * "capture" special case bypasses that gate unconditionally
             * for now, real for TESTING only, not the intended final
             * balance - flagged there too, not just here. */
            fprintf(pf, "METHOD       | capture              | ops/+x/choice.+x\n");
        }
        if (strcmp(row.category, "player") == 0 || strcmp(row.category, "trainer") == 0) {
            /* MUCHIMON-DESIGN.txt sec. 8 - "fight the wild muchimon to
             * wear it down" - a real numbered method, only meaningful
             * on a directly-controllable combatant (player/trainer),
             * same digit+Enter+NAME-special-case pattern as "capture"
             * above. Trainers get both attack AND capture (a trainer
             * can fight back before being caught, matching real design
             * intent - not mutually exclusive like the old muchimon-
             * only/player-trainer-only split this replaced). */
            fprintf(pf, "METHOD       | attack               | ops/+x/choice.+x\n");
        }
        fprintf(pf, "\n");
        fprintf(pf, "RESPONSE     | default              | *awaiting orders*\n");
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
        fprintf(sf, "category=%s\n", row.category);
        fprintf(sf, "civ_id=%s\n", civ_id);
        fprintf(sf, "move_range=%d\n", row.move_range);
        fprintf(sf, "attack=%d\n", row.attack);
        fprintf(sf, "defense=%d\n", row.defense);
        fprintf(sf, "pos_x=%d\n", spawn_x);
        fprintf(sf, "pos_y=%d\n", spawn_y);
        fprintf(sf, "pos_z=0\n");
        fprintf(sf, "map_id=map_start\n");
        /* MUCHIMON-DESIGN.txt sec. 8 (real combat/capture slice, direct
         * user request: "fight the wild muchimon to wear it down and
         * use a [muchi]box to catch it"): muchimon-category rows spawn
         * with a lower max_hp than player/trainer - they're meant to be
         * weakened, not defeated outright, before capture succeeds
         * (ops/choice.c's own "capture" special case checks hp against
         * max_hp/2). max_hp is a NEW field (previous spawns only had a
         * bare hp=100, no ceiling) - written alongside hp so "wear it
         * down" has something real to measure against. */
        {
            int starting_hp = (strcmp(row.category, "muchimon") == 0) ? 20 : 100;
            fprintf(sf, "hp=%d\n", starting_hp);
            fprintf(sf, "max_hp=%d\n", starting_hp);
        }
        fprintf(sf, "orders=none\n");
        fprintf(sf, "decision_mode=0\n");
        fclose(sf);
    } else {
        fprintf(stderr, "spawn_entity: could not write %s\n", state_path);
        return 3;
    }

    printf("spawned %s (%s, civ=%s) as instance '%s' in %s at (%d,%d)\n",
           row.name, row.id, civ_id, instance_id, world_dir, spawn_x, spawn_y);
    return 0;
}
