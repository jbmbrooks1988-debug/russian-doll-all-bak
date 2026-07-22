/* activity - one verb, one binary, no shared headers.
 * Finds activities for current room location, applies first one's meter
 * deltas (d_peer, d_ego, d_kink, d_masc) to hero/state.txt, logs outcome.
 * Self-contained: no shared headers. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

/* Resolve current room from AABB in locations.txt */
static void resolve_location_name(int px, int py, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", "");
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/locations/locations.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char fields[8][64];
        int nf = 0;
        char *save = NULL;
        char *tok = strtok_r(line, "|", &save);
        while (tok && nf < 8) { snprintf(fields[nf], sizeof(fields[0]), "%s", tok); nf++; tok = strtok_r(NULL, "|", &save); }
        if (nf < 7) continue;
        int x0 = atoi(fields[3]), y0 = atoi(fields[4]), x1 = atoi(fields[5]), y1 = atoi(fields[6]);
        if (px >= x0 && px <= x1 && py >= y0 && py <= y1) {
            snprintf(out, out_sz, "%s", fields[0]);
            break;
        }
    }
    fclose(f);
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_muchi_civ_home/map_start/hero/state.txt", project_root);
    int hero_x = read_kv_int(hero_path, "pos_x", 0);
    int hero_y = read_kv_int(hero_path, "pos_y", 0);
    int peer_bond = read_kv_int(hero_path, "peer_bond", 0);
    int ego_threat = read_kv_int(hero_path, "ego_threat", 0);
    int kink_heat = read_kv_int(hero_path, "kink_heat", 0);
    int masc_pressure = read_kv_int(hero_path, "masc_pressure", 0);

    char loc_id[64];
    resolve_location_name(hero_x, hero_y, loc_id, sizeof(loc_id));
    if (!loc_id[0]) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "Nothing to do here.\n"); fclose(lf); }
        return 0;
    }

    char act_path[PATH_BUF];
    snprintf(act_path, sizeof(act_path), "%s/pieces/registry/activities/activities.txt", project_root);
    FILE *af = fopen(act_path, "r");
    if (!af) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "No activities available.\n"); fclose(lf); }
        return 0;
    }

    /* Find first activity for this location */
    char act_name[64] = "", msg[160] = "";
    int d_peer = 0, d_ego = 0, d_kink = 0, d_masc = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), af)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        /* id|name|locations_csv|d_peer|d_ego|d_kink|d_masc|grief_nudge|message */
        char *p = line;
        int field = 0;
        char *bar, *next;
        char locations_str[256] = "", msg_str[160] = "";
        while ((bar = strchr(p, '|'))) {
            *bar = '\0';
            if (field == 0) { /* id */ }
            else if (field == 1) snprintf(act_name, sizeof(act_name), "%s", p);
            else if (field == 2) snprintf(locations_str, sizeof(locations_str), "%s", p);
            else if (field == 3) d_peer = atoi(p);
            else if (field == 4) d_ego = atoi(p);
            else if (field == 5) d_kink = atoi(p);
            else if (field == 6) d_masc = atoi(p);
            else if (field == 8) snprintf(msg_str, sizeof(msg_str), "%s", p);
            field++;
            p = bar + 1;
        }
        if (field == 8) { /* last field has no trailing pipe */
            snprintf(msg_str, sizeof(msg_str), "%s", p);
        }

        /* Check if loc_id is in locations_csv */
        if (strstr(locations_str, loc_id)) {
            snprintf(msg, sizeof(msg), "%s", msg_str[0] ? msg_str : "You did something.");
            break;
        }
        act_name[0] = '\0';
    }
    fclose(af);

    if (!act_name[0]) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "Nothing to do here.\n"); fclose(lf); }
        return 0;
    }

    /* Apply meter changes */
    peer_bond += d_peer;
    ego_threat += d_ego;
    kink_heat += d_kink;
    masc_pressure += d_masc;
    if (peer_bond < 0) peer_bond = 0;
    if (ego_threat < 0) ego_threat = 0;
    if (kink_heat < 0) kink_heat = 0;
    if (masc_pressure < 0) masc_pressure = 0;

    /* Write back hero state */
    FILE *hf = fopen(hero_path, "r");
    if (!hf) return 1;
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", hero_path);
    FILE *tmp = fopen(tmp_path, "w");
    if (!tmp) { fclose(hf); return 1; }
    while (fgets(line, sizeof(line), hf)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(line, "peer_bond") == 0) { fprintf(tmp, "peer_bond=%d\n", peer_bond); *eq = '='; continue; }
            if (strcmp(line, "ego_threat") == 0) { fprintf(tmp, "ego_threat=%d\n", ego_threat); *eq = '='; continue; }
            if (strcmp(line, "kink_heat") == 0) { fprintf(tmp, "kink_heat=%d\n", kink_heat); *eq = '='; continue; }
            if (strcmp(line, "masc_pressure") == 0) { fprintf(tmp, "masc_pressure=%d\n", masc_pressure); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(line, tmp);
    }
    fclose(hf);
    fclose(tmp);
    rename(tmp_path, hero_path);

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *lf = fopen(log_path, "a");
    if (lf) {
        fprintf(lf, "%s\n", msg);
        fclose(lf);
    }
    return 0;
}
