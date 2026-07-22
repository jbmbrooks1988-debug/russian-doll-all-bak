/* dispatch_event - one verb, one binary, no shared headers. Runtime half
 * of pal-op's own event editor (see $.4x/2.muchipal-editor-0.0/ops/
 * map_edit_input.c's own event-editor header comment for the authoring
 * side, and $.4x/pal-op-handoff.txt sec. 3.1 for the full design this
 * file implements verbatim).
 *
 * ARCHITECTURE, stated once, precisely (direct user correction that
 * shaped this file's own shape: "do u understand that rpgmaker events
 * are meant to be built 'ON PAL' (that pal will be invisible to the
 * average player)?"): this is a STANDALONE OP, called as its own line in
 * pal/main_loop.pal right after move_player - NOT a function threaded
 * into move_player.c's own C source. Same shape as angler-empires' own
 * tick_jobs.c (called right after end_turn, every turn, self-filtering,
 * invisible to the player - they see a job get done, never "an op ran").
 * This is what "built on pal, invisible to the player" concretely means
 * in this engine: the player experiences the effect (teleported, saw a
 * message, a wild muchimon appeared), never the mechanism.
 *
 * Runs every tick (cheap: one fopen of hero/state.txt, one fopen of
 * events.txt, no-ops if neither the mover's position nor the events file
 * itself changed anything). No args needed - purely reads state, same as
 * tick_jobs.c/check_response.c's own "no-op unless actually relevant"
 * convention.
 *
 * MOVER POSITION RESOLUTION - reuses ops/choice.c's own established
 * 3-way dispatch_id logic verbatim (see that file and !.pal-standards.txt
 * sec. 28/30 for the full citation of where this pattern comes from):
 *   interact_mode==0                       -> hero's own pos_x/pos_y
 *   interact_mode==1 && possessed_id!="none" -> that instance's OWN
 *                                              pos_x/pos_y, read fresh
 *                                              from entities/<id>/
 *                                              state.txt
 *   else (interact_mode==1, nothing possessed) -> xlector_pos_x/y
 * Reusing this exact resolution (not inventing a new one) is what lets
 * an event fire whether the player is free-roaming the xlector cursor or
 * currently possessing a captured muchimon/trainer - both can trigger a
 * floor event, matching how a real RPG Maker player triggers events
 * regardless of which party member visually leads the group.
 *
 * Usage: dispatch_event.+x (no args) */
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
        line[strcspn(line, "\r\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

/* Rewrites ONE key's value in-place in a plain key=value file, leaving
 * every other line untouched - same "read all lines, rewrite the one
 * that matches, write back" shape ops/choice.c's own state-file rewrites
 * already use elsewhere in this project (see its own emoji_mode rewrite
 * for the precedent). */
static void write_kv_int(const char *path, const char *key, int value) {
    char lines[256][MAX_LINE];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (n < 256 && fgets(lines[n], sizeof(lines[n]), f)) n++;
        fclose(f);
    }
    FILE *out = fopen(path, "w");
    if (!out) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        char tmp[MAX_LINE];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(tmp, sizeof(tmp), "%s", lines[i]);
#pragma GCC diagnostic pop
        tmp[strcspn(tmp, "\r\n")] = '\0';
        char *eq = strchr(tmp, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(tmp, key) == 0) {
                fprintf(out, "%s=%d\n", key, value);
                found = 1;
                continue;
            }
        }
        fprintf(out, "%s", lines[i]);
    }
    if (!found) fprintf(out, "%s=%d\n", key, value);
    fclose(out);
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_muchimon_home/map_start/hero/state.txt", project_root);

    int interact_mode = read_kv_int(hero_path, "interact_mode", 0);
    char possessed_id[64];
    read_kv_str(hero_path, "possessed_id", possessed_id, sizeof(possessed_id), "none");

    int mover_x, mover_y;
    char mover_state_path[PATH_BUF];
    int mover_is_hero = 0;

    if (interact_mode == 0) {
        mover_is_hero = 1;
        snprintf(mover_state_path, sizeof(mover_state_path), "%s", hero_path);
        mover_x = read_kv_int(hero_path, "pos_x", 0);
        mover_y = read_kv_int(hero_path, "pos_y", 0);
    } else if (strcmp(possessed_id, "none") != 0) {
        snprintf(mover_state_path, sizeof(mover_state_path),
                 "%s/pieces/world_muchimon_home/map_start/entities/%s/state.txt", project_root, possessed_id);
        mover_x = read_kv_int(mover_state_path, "pos_x", 0);
        mover_y = read_kv_int(mover_state_path, "pos_y", 0);
    } else {
        mover_is_hero = 1;
        snprintf(mover_state_path, sizeof(mover_state_path), "%s", hero_path);
        mover_x = read_kv_int(hero_path, "xlector_pos_x", 0);
        mover_y = read_kv_int(hero_path, "xlector_pos_y", 0);
    }

    char events_path[PATH_BUF];
    snprintf(events_path, sizeof(events_path), "%s/pieces/world_muchimon_home/map_start/events.txt", project_root);
    FILE *ef = fopen(events_path, "r");
    if (!ef) return 0; /* no events authored yet - cheap no-op, matching every other tick-driven op */

    char op_id[32] = "";
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), ef)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#') continue;
        int ex = 0, ey = 0;
        char id[32];
        if (sscanf(line, "%d|%d|%31[^|\n]", &ex, &ey, id) == 3 && ex == mover_x && ey == mover_y) {
            snprintf(op_id, sizeof(op_id), "%s", id);
            break;
        }
    }
    fclose(ef);

    if (!op_id[0]) return 0; /* no event at the mover's current tile */

    if (strcmp(op_id, "message") == 0) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "%s\n", "Welcome!"); fclose(lf); }
    } else if (strcmp(op_id, "teleport") == 0) {
        write_kv_int(mover_state_path, "pos_x", 0);
        write_kv_int(mover_state_path, "pos_y", 0);
        if (mover_is_hero) {
            /* Keep the xlector cursor in sync too when the hero/xlector
             * itself was the mover (not a possessed piece) - otherwise
             * the camera-follow logic (compose_frame.c/compose_rgb_
             * frame.c's own cam_anchor resolution) would still show the
             * OLD position for one frame, since it reads xlector_pos_x/y
             * as its own anchor whenever nothing is possessed. */
            write_kv_int(hero_path, "xlector_pos_x", 0);
            write_kv_int(hero_path, "xlector_pos_y", 0);
        }
    } else if (strcmp(op_id, "spawn") == 0) {
        char entities_dir[PATH_BUF];
        snprintf(entities_dir, sizeof(entities_dir), "%s/pieces/world_muchimon_home/map_start/entities", project_root);
        char instance_id[64] = "";
        for (int i = 1; i < 1000; i++) {
            char candidate[64], probe_path[PATH_BUF + 128];
            snprintf(candidate, sizeof(candidate), "entity_%02d", i);
            snprintf(probe_path, sizeof(probe_path), "%s/%s", entities_dir, candidate);
            FILE *pf = fopen(probe_path, "r");
            if (pf) { fclose(pf); continue; }
            snprintf(instance_id, sizeof(instance_id), "%s", candidate);
            break;
        }
        if (instance_id[0]) {
            char cmd[PATH_BUF * 2 + 256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(cmd, sizeof(cmd), "'%s/ops/+x/spawn_entity.+x' sparkit '%s' world_muchimon_home wild %d %d",
                     project_root, instance_id, mover_x, mover_y);
#pragma GCC diagnostic pop
            if (system(cmd) != 0) { /* spawn_entity.+x's own stderr already reports failure reasons */ }
        }
    }

    return 0;
}
