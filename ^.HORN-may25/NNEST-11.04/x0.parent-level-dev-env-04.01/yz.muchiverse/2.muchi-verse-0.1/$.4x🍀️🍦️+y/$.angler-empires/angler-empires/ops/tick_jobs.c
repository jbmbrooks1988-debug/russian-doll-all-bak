/* tick_jobs - one verb, one binary, no shared headers.
 * ANGLER-EXPANSES-DESIGN.txt sec. 4 - the idle-labor tick: assigns an
 * idle angler (role matches, job_id==none) to the nearest unassigned
 * pending job, moves every angler with an active job one step toward
 * its own target (greedy single-axis-per-tick pathing - the smallest
 * correct slice, not real pathfinding around obstacles, matching this
 * doc's own "dig only mutates terrain, prove one thing before
 * generalizing" scope), and completes a job once its angler has
 * arrived and turns_remaining reaches 0 - mutating map.txt directly
 * (same file-rewrite shape every other terrain-mutating op in this
 * family already uses, no new mutation mechanism).
 *
 * Tick-driven poll (cheap no-op if no jobs are pending/active),
 * matching irc_agent_poll.c's own shape - called from pal/main_loop.pal
 * every real gameplay tick, same interact_mode==1 gate end_turn.c
 * already uses (a NAV-mode keypress should not advance angler labor
 * for free, matching !.pal-standards.txt sec. 28.4/31.2's own end_turn
 * gating rule).
 *
 * Usage: tick_jobs.+x (no args - reads hero/state.txt's own
 * interact_mode itself, self-gating, so pal/main_loop.pal can call it
 * unconditionally every tick like end_turn already is). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

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

/* Rewrites one key=value line in a small state file, appending it if
 * absent - same "read all lines, rewrite, replace-or-append one field"
 * idiom every op in this family already uses. */
static void write_kv_str(const char *path, const char *key, const char *value) {
    char lines[32][MAX_LINE];
    int nlines = 0;
    FILE *f = fopen(path, "r");
    if (f) { while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++; fclose(f); }
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    size_t klen = strlen(key);
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
            continue;
        }
        fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv_str(path, key, buf);
}

/* One tile's own registry row lookup (glyph -> terrain id), reused for
 * writing a NEW glyph back into map.txt from a result_terrain_id - the
 * inverse direction of every other glyph_walkable()-style lookup in
 * this family, so it's a real, separate small function, not a copy. */
static char terrain_glyph_for_id(const char *terrain_id) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/terrain/terrain_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return '?';
    char line[MAX_LINE];
    char result = '?';
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char glyph = line[0];
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *id_start = p1 + 1;
        char *id_end = strchr(id_start, '|');
        if (!id_end) continue;
        if ((size_t)(id_end - id_start) == strlen(terrain_id) && strncmp(id_start, terrain_id, id_end - id_start) == 0) {
            result = glyph;
            break;
        }
    }
    fclose(f);
    return result;
}

/* Mutates ONE character in map.txt at (x,y) - same "rewrite the whole
 * grid, one cell changed" shape pickup.c/drop.c already use for
 * furniture.txt, ported here for terrain instead. */
static void mutate_map_glyph(const char *map_path, int x, int y, char new_glyph) {
    FILE *f = fopen(map_path, "r");
    if (!f) return;
    char lines[64][256];
    int n = 0;
    while (n < 64 && fgets(lines[n], sizeof(lines[0]), f)) n++;
    fclose(f);

    if (y < 0 || y >= n) return;
    int len = (int)strlen(lines[y]);
    if (x < 0 || x >= len) return;
    if (lines[y][x] == '\n' || lines[y][x] == '\0') return;
    lines[y][x] = new_glyph;

    f = fopen(map_path, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) fputs(lines[i], f);
    fclose(f);
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_angler_home/map_start/hero/state.txt", project_root);
    if (!read_kv_int(hero_path, "interact_mode", 0)) return 0; /* NAV-mode tick, no labor progresses - sec. 28.4/31.2 */

    char jobs_dir[PATH_BUF];
    snprintf(jobs_dir, sizeof(jobs_dir), "%s/pieces/world_angler_home/map_start/jobs", project_root);
    char anglers_dir[PATH_BUF];
    snprintf(anglers_dir, sizeof(anglers_dir), "%s/pieces/world_angler_home/map_start/anglers", project_root);
    char map_path[PATH_BUF];
    snprintf(map_path, sizeof(map_path), "%s/pieces/world_angler_home/map_start/map.txt", project_root);

    DIR *jd = opendir(jobs_dir);
    if (!jd) return 0; /* no jobs directory yet = no pending jobs, cheap no-op */
    struct dirent *jentry;
    while ((jentry = readdir(jd)) != NULL) {
        if (jentry->d_name[0] == '.') continue;
        char job_state[PATH_BUF];
        snprintf(job_state, sizeof(job_state), "%s/%s/state.txt", jobs_dir, jentry->d_name);

        char assigned[64];
        read_kv_str(job_state, "assigned_angler_id", assigned, sizeof(assigned), "none");
        int tx = read_kv_int(job_state, "target_x", -1);
        int ty = read_kv_int(job_state, "target_y", -1);
        if (tx < 0 || ty < 0) continue;

        /* Step 1: assignment - find the nearest idle angler whose own
         * role matches this job's requires_role, if none assigned yet. */
        if (strcmp(assigned, "none") == 0) {
            char requires_role[32];
            read_kv_str(job_state, "requires_role", requires_role, sizeof(requires_role), "");

            DIR *ad = opendir(anglers_dir);
            char best_id[64] = "";
            int best_dist = -1;
            if (ad) {
                struct dirent *aentry;
                while ((aentry = readdir(ad)) != NULL) {
                    if (aentry->d_name[0] == '.') continue;
                    char angler_state[PATH_BUF];
                    snprintf(angler_state, sizeof(angler_state), "%s/%s/state.txt", anglers_dir, aentry->d_name);
                    char role[32], job_id_field[64];
                    read_kv_str(angler_state, "role", role, sizeof(role), "");
                    read_kv_str(angler_state, "job_id", job_id_field, sizeof(job_id_field), "none");
                    if (strcmp(role, requires_role) != 0) continue;
                    if (strcmp(job_id_field, "none") != 0) continue; /* already working a job */
                    int ax = read_kv_int(angler_state, "pos_x", -1);
                    int ay = read_kv_int(angler_state, "pos_y", -1);
                    if (ax < 0 || ay < 0) continue;
                    int dist = abs(ax - tx) + abs(ay - ty);
                    if (best_dist < 0 || dist < best_dist) {
                        best_dist = dist;
                        snprintf(best_id, sizeof(best_id), "%s", aentry->d_name);
                    }
                }
                closedir(ad);
            }
            if (best_id[0]) {
                write_kv_str(job_state, "assigned_angler_id", best_id);
                char angler_state[PATH_BUF];
                snprintf(angler_state, sizeof(angler_state), "%s/%s/state.txt", anglers_dir, best_id);
                write_kv_str(angler_state, "job_id", jentry->d_name);
                write_kv_int(angler_state, "job_target_x", tx);
                write_kv_int(angler_state, "job_target_y", ty);
                snprintf(assigned, sizeof(assigned), "%s", best_id);
            }
        }

        if (strcmp(assigned, "none") == 0) continue; /* still nobody free to do it */

        /* Step 2: move the assigned angler one step toward the job's
         * own target (greedy, single axis per tick - no obstacle
         * pathfinding, matches this doc's own smallest-slice scope). */
        char angler_state[PATH_BUF];
        snprintf(angler_state, sizeof(angler_state), "%s/%s/state.txt", anglers_dir, assigned);
        int ax = read_kv_int(angler_state, "pos_x", -1);
        int ay = read_kv_int(angler_state, "pos_y", -1);
        if (ax < 0 || ay < 0) continue;

        int adjacent = (abs(ax - tx) + abs(ay - ty)) <= 1;
        if (!adjacent) {
            int nx = ax, ny = ay;
            if (ax != tx) nx += (tx > ax) ? 1 : -1;
            else if (ay != ty) ny += (ty > ay) ? 1 : -1;
            write_kv_int(angler_state, "pos_x", nx);
            write_kv_int(angler_state, "pos_y", ny);
            continue; /* one move per tick - work on the job starts once adjacent */
        }

        /* Step 3: adjacent (or on) the target - work the job. */
        int remaining = read_kv_int(job_state, "turns_remaining", 0) - 1;
        if (remaining > 0) {
            write_kv_int(job_state, "turns_remaining", remaining);
            continue;
        }

        /* Step 4: complete - mutate the map, clear both the job and the
         * angler's own job assignment. */
        char result_terrain_id[32];
        read_kv_str(job_state, "result_terrain_id", result_terrain_id, sizeof(result_terrain_id), "");
        char new_glyph = terrain_glyph_for_id(result_terrain_id);
        if (new_glyph != '?') mutate_map_glyph(map_path, tx, ty, new_glyph);

        write_kv_str(angler_state, "job_id", "none");
        write_kv_int(angler_state, "job_target_x", -1);
        write_kv_int(angler_state, "job_target_y", -1);

        char job_state_dir[PATH_BUF];
        snprintf(job_state_dir, sizeof(job_state_dir), "%s/%s", jobs_dir, jentry->d_name);
        char rm_state[PATH_BUF];
        snprintf(rm_state, sizeof(rm_state), "%s/state.txt", job_state_dir);
        remove(rm_state);
        rmdir(job_state_dir);

        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "Job complete at (%d,%d).\n", tx, ty); fclose(lf); }
    }
    closedir(jd);
    return 0;
}
