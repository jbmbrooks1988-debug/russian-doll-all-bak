/* designate - one verb, one binary, no shared headers.
 * ANGLER-EXPANSES-DESIGN.txt sec. 4 - the player's own "queue a job"
 * action, dispatched from ops/choice.c the same way any other real
 * xlector method is (a "designate" METHOD row on xlector's own
 * piece.pdl, requires digit-selecting it first, unlike "scan"'s own
 * unconditional-on-Enter special case - designating is a deliberate
 * choice, not an automatic default).
 *
 * Reads xlector's own current cursor position from hero/state.txt,
 * looks up the given job_id in pieces/registry/jobs/job_types.txt,
 * writes a real pending-job piece at pieces/world_angler_home/
 * map_start/jobs/<job_instance_id>/state.txt (job_id, target_x,
 * target_y, turns_remaining, assigned_angler_id=none) - same "each
 * pending job is its own stateful piece" pattern as any other instance
 * in this family (angler_bank.txt's own spawn_angler.c precedent).
 *
 * Usage: designate.+x <job_id>
 * Exit 0 success, 1 bad args/job_id not found, 2 job instance dir
 * already exists at that position (one pending job per tile at a time -
 * don't silently queue a second dig on the same tile). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

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

typedef struct {
    char id[32];
    char name[64];
    char glyph[4];
    int turns_to_complete;
    char requires_role[32];
    char result_terrain_id[32];
} JobRow;

static int find_job_row(const char *path, const char *job_id, JobRow *out) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[6] = {0};
        int nf = 0;
        char *tok = strtok(line, "|");
        while (tok && nf < 6) { fields[nf++] = tok; tok = strtok(NULL, "|"); }
        if (nf < 6 || strcmp(fields[0], job_id) != 0) continue;
        snprintf(out->id, sizeof(out->id), "%s", fields[0]);
        snprintf(out->name, sizeof(out->name), "%s", fields[1]);
        snprintf(out->glyph, sizeof(out->glyph), "%s", fields[2]);
        out->turns_to_complete = atoi(fields[3]);
        snprintf(out->requires_role, sizeof(out->requires_role), "%s", fields[4]);
        snprintf(out->result_terrain_id, sizeof(out->result_terrain_id), "%s", fields[5]);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

int main(int argc, char **argv) {
    /* CORRECTED: argv[1] is deliberately NOT used, even though a
     * <job_id> parameter reads naturally here - ops/choice.c's own
     * generic method dispatch wraps a piece.pdl METHOD row's whole
     * handler string in one pair of shell quotes (see its own
     * `exec_cmd` construction), so a handler value with an embedded
     * argument like "ops/+x/designate.+x dig" would be treated as ONE
     * literal (broken) path, not a command plus an argument. Smallest
     * correct fix matching this doc's own "one job type, prove it end-
     * to-end first" scope: hardcode "dig" here, one op per job type
     * later (designate_<job>.c) once more than one job exists, or fix
     * choice.c's own arg-passing generically at that point - don't
     * over-build this now for a second job type that doesn't exist
     * yet. */
    (void)argc; (void)argv;
    const char *job_id = "dig";
    resolve_root();

    char job_bank_path[PATH_BUF];
    snprintf(job_bank_path, sizeof(job_bank_path), "%s/pieces/registry/jobs/job_types.txt", project_root);
    JobRow row;
    if (!find_job_row(job_bank_path, job_id, &row)) {
        fprintf(stderr, "designate: job_id '%s' not found in %s\n", job_id, job_bank_path);
        return 1;
    }

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_angler_home/map_start/hero/state.txt", project_root);
    int tx = read_kv_int(hero_path, "xlector_pos_x", -1);
    int ty = read_kv_int(hero_path, "xlector_pos_y", -1);
    if (tx < 0 || ty < 0) { fprintf(stderr, "designate: xlector position unknown\n"); return 1; }

    char jobs_dir[PATH_BUF];
    snprintf(jobs_dir, sizeof(jobs_dir), "%s/pieces/world_angler_home/map_start/jobs", project_root);
    mkdir(jobs_dir, 0755);

    char instance_id[64];
    snprintf(instance_id, sizeof(instance_id), "job_%s_%d_%d", job_id, tx, ty);
    char instance_dir[PATH_BUF];
    snprintf(instance_dir, sizeof(instance_dir), "%s/%s", jobs_dir, instance_id);

    struct stat st;
    if (stat(instance_dir, &st) == 0) {
        fprintf(stderr, "designate: a job already exists at (%d,%d)\n", tx, ty);
        return 2;
    }
    if (mkdir(instance_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "designate: could not create %s\n", instance_dir);
        return 2;
    }

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", instance_dir);
    FILE *f = fopen(state_path, "w");
    if (!f) { fprintf(stderr, "designate: could not write %s\n", state_path); return 2; }
    fprintf(f, "job_id=%s\n", row.id);
    fprintf(f, "target_x=%d\n", tx);
    fprintf(f, "target_y=%d\n", ty);
    fprintf(f, "turns_remaining=%d\n", row.turns_to_complete);
    fprintf(f, "requires_role=%s\n", row.requires_role);
    fprintf(f, "result_terrain_id=%s\n", row.result_terrain_id);
    fprintf(f, "assigned_angler_id=none\n");
    fclose(f);

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *lf = fopen(log_path, "a");
    if (lf) { fprintf(lf, "Designated %s at (%d,%d).\n", row.name, tx, ty); fclose(lf); }

    printf("designated %s at (%d,%d) as %s\n", row.name, tx, ty, instance_id);
    return 0;
}
