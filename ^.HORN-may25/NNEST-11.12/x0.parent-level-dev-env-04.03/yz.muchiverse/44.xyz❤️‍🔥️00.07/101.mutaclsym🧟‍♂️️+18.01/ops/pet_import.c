/* pet_import - Import pet from exchange directory into mutaclsym.
 *
 * Adapted from zoo's pet_import.c for mutaclsym's directory structure.
 * Imports a pet from the shared exchange directory into mutaclsym's world.
 *
 * Reads trade_envelope.txt (or falls back to native state.txt) and
 * writes mutaclsym-native state.txt and piece.pdl for the imported pet.
 *
 * mutaclsym structure: pieces/world_01/map_start/<pet_id>/
 *   - state.txt: pet stats (hp, hunger, thirst, stamina, etc.)
 *   - piece.pdl: behavior definition with methods
 *
 * The pet is moved (not copied) from exchange to mutaclsym's world.
 *
 * Usage: pet_import [piece_id]
 *   With argument: import that specific pet from exchange/
 *   Without argument: import ALL pets from exchange/
 */
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

static void resolve_exchange_root(char *out, size_t out_sz) {
    const char *env = getenv("PRISC_EXCHANGE_ROOT");
    if (env && env[0]) { snprintf(out, out_sz, "%s", env); return; }
    char parent[MAX_PATH];
    snprintf(parent, sizeof(parent), "%s", project_root);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    snprintf(out, out_sz, "%s/exchange", parent);
}

/* Same pattern as move_entity.c's/xlector_input.c's own
 * resolve_piece_dir() - scan world_NN/map_NN/<piece_id>/ for the
 * piece's own directory, falling back to a flat pieces/<piece_id>/
 * layout. */
static void resolve_piece_dir(const char *piece_id, char *out, size_t out_sz) {
    char pieces_dir[PATH_BUF];
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/pieces", project_root);

    DIR *worlds = opendir(pieces_dir);
    if (worlds) {
        struct dirent *w;
        while ((w = readdir(worlds)) != NULL) {
            if (strncmp(w->d_name, "world_", 6) != 0) continue;
            char maps_dir[PATH_BUF + 256];
            snprintf(maps_dir, sizeof(maps_dir), "%s/%s", pieces_dir, w->d_name);
            DIR *maps = opendir(maps_dir);
            if (!maps) continue;
            struct dirent *m;
            while ((m = readdir(maps)) != NULL) {
                if (strncmp(m->d_name, "map_", 4) != 0) continue;
                char candidate[PATH_BUF + 512];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(candidate, sizeof(candidate), "%s/%s/%s", maps_dir, m->d_name, piece_id);
#pragma GCC diagnostic pop
                struct stat st;
                if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) {
                    closedir(maps);
                    closedir(worlds);
                    snprintf(out, out_sz, "%s", candidate);
                    return;
                }
            }
            closedir(maps);
        }
        closedir(worlds);
    }

    snprintf(out, out_sz, "%s/pieces/%s", project_root, piece_id);
}

/* Reads a pipe-delimited "key|value" line, the trade_envelope.txt
 * shape - distinct from every OTHER file format in this project family
 * (key=value), on purpose: envelopes are meant to be read by games
 * that share nothing else about the origin game's own conventions, so
 * their shape shouldn't accidentally collide with or be assumed
 * identical to any one game's own state.txt format. */
static void read_envelope_field(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *bar = strchr(line, '|');
        if (!bar) continue;
        if ((size_t)(bar - line) == strlen(key) && strncmp(line, key, bar - line) == 0) {
            snprintf(out, out_sz, "%s", bar + 1);
            break;
        }
    }
    fclose(f);
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

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void import_one(const char *name, const char *dest_map_id) {
    char exchange_root[PATH_BUF];
    resolve_exchange_root(exchange_root, sizeof(exchange_root));

    char src_dir[PATH_BUF + 64];
    snprintf(src_dir, sizeof(src_dir), "%s/%s", exchange_root, name);
    if (!path_exists(src_dir)) return;

    char envelope_path[PATH_BUF + 96];
    snprintf(envelope_path, sizeof(envelope_path), "%s/trade_envelope.txt", src_dir);
    char native_state_path[PATH_BUF + 96];
    snprintf(native_state_path, sizeof(native_state_path), "%s/state.txt", src_dir);

    char display_name[64], species[32], species_name[32];
    int hunger, energy, hp, hp_max, mp, mp_max, level, xp;
    int have_envelope = path_exists(envelope_path);

    if (have_envelope) {
        read_envelope_field(envelope_path, "display_name", display_name, sizeof(display_name), name);
        read_envelope_field(envelope_path, "species_id", species, sizeof(species), "unknown");
        read_envelope_field(envelope_path, "species_name", species_name, sizeof(species_name), "Unknown");
        char buf[16];
        read_envelope_field(envelope_path, "hunger", buf, sizeof(buf), "100"); hunger = atoi(buf);
        read_envelope_field(envelope_path, "energy", buf, sizeof(buf), "100"); energy = atoi(buf);
        read_envelope_field(envelope_path, "hp", buf, sizeof(buf), "25"); hp = atoi(buf);
        read_envelope_field(envelope_path, "hp_max", buf, sizeof(buf), "25"); hp_max = atoi(buf);
        read_envelope_field(envelope_path, "mp", buf, sizeof(buf), "13"); mp = atoi(buf);
        read_envelope_field(envelope_path, "mp_max", buf, sizeof(buf), "13"); mp_max = atoi(buf);
        read_envelope_field(envelope_path, "level", buf, sizeof(buf), "1"); level = atoi(buf);
        read_envelope_field(envelope_path, "xp", buf, sizeof(buf), "0"); xp = atoi(buf);
    } else if (path_exists(native_state_path)) {
        /* No envelope - fall back to reading the origin's own state.txt */
        char line[MAX_LINE];
        FILE *sf = fopen(native_state_path, "r");
        snprintf(display_name, sizeof(display_name), "%s", name);
        snprintf(species, sizeof(species), "unknown");
        snprintf(species_name, sizeof(species_name), "Unknown");
        if (sf) {
            while (fgets(line, sizeof(line), sf)) {
                line[strcspn(line, "\n")] = '\0';
                char *eq = strchr(line, '=');
                if (!eq) continue;
                *eq = '\0';
                if (strcmp(line, "name") == 0) snprintf(display_name, sizeof(display_name), "%s", eq + 1);
                else if (strcmp(line, "species") == 0) snprintf(species, sizeof(species), "%s", eq + 1);
                else if (strcmp(line, "species_id") == 0) snprintf(species, sizeof(species), "%s", eq + 1);
                else if (strcmp(line, "species_name") == 0) snprintf(species_name, sizeof(species_name), "%s", eq + 1);
            }
            fclose(sf);
        }
        hunger = read_kv_int(native_state_path, "hunger", 100);
        energy = read_kv_int(native_state_path, "energy", 100);
        hp = read_kv_int(native_state_path, "hp", 25);
        hp_max = read_kv_int(native_state_path, "hp_max", 25);
        mp = read_kv_int(native_state_path, "mp", 13);
        mp_max = read_kv_int(native_state_path, "mp_max", 13);
        level = read_kv_int(native_state_path, "level", 1);
        xp = read_kv_int(native_state_path, "xp", 0);
    } else {
        snprintf(display_name, sizeof(display_name), "%s", name);
        snprintf(species, sizeof(species), "unknown");
        snprintf(species_name, sizeof(species_name), "Unknown");
        hunger = 100; energy = 100;
        hp = 25; hp_max = 25; mp = 13; mp_max = 13;
        level = 1; xp = 0;
    }

    char dst_dir[PATH_BUF + 128];
    snprintf(dst_dir, sizeof(dst_dir), "%s/pieces/world_01/%s/%s", project_root, dest_map_id, name);
    if (rename(src_dir, dst_dir) != 0) return;

    /* ALWAYS (re)write a fresh, native state.txt/piece.pdl - whatever
     * game this pet came from, it becomes a real, fully-controllable
     * pet the instant import succeeds. A fixed, always-open import
     * point (2,2) - real placement/landing-spot logic (avoid overlap,
     * spawn near xlector, etc.) is a reasonable next step, not built
     * this pass. */
    char state_path[PATH_BUF + 160];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", dst_dir);
    FILE *sf = fopen(state_path, "w");
    if (sf) {
        fprintf(sf, "pos_x=2\n");
        fprintf(sf, "pos_y=2\n");
        fprintf(sf, "hp=%d\n", hp);
        fprintf(sf, "hp_max=%d\n", hp_max);
        fprintf(sf, "mp=%d\n", mp);
        fprintf(sf, "mp_max=%d\n", mp_max);
        fprintf(sf, "hunger=%d\n", hunger);
        fprintf(sf, "energy=%d\n", energy);
        fprintf(sf, "stamina=100\n");
        fprintf(sf, "map_id=%s\n", dest_map_id);
        fprintf(sf, "on_map=1\n");
        fprintf(sf, "species=%s\n", species);
        fprintf(sf, "species_name=%s\n", species_name);
        fprintf(sf, "level=%d\n", level);
        fprintf(sf, "xp=%d\n", xp);
        fprintf(sf, "skills=\n");
        fprintf(sf, "decision_mode=0\n");
        fprintf(sf, "digit_accum=0\n");
        fprintf(sf, "action_cursor=-1\n");
        fprintf(sf, "last_key=0\n");
        fclose(sf);
    }

    char pdl_path[PATH_BUF + 160];
    snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", dst_dir);
    FILE *pf = fopen(pdl_path, "w");
    if (pf) {
        fprintf(pf, "SECTION      | KEY                | VALUE\n");
        fprintf(pf, "----------------------------------------\n");
        fprintf(pf, "META         | piece_id           | %s\n", name);
        fprintf(pf, "META         | version            | 1.0\n\n");
        fprintf(pf, "STATE        | name                 | %s\n", display_name);
        fprintf(pf, "STATE        | species              | %s\n", species);
        fprintf(pf, "STATE        | species_name         | %s\n", species_name);
        fprintf(pf, "STATE        | map_id               | %s\n", dest_map_id);
        fprintf(pf, "STATE        | possessable          | 1\n\n");
        fprintf(pf, "METHOD       | move                 | ops/+x/move_entity.+x\n");
        fprintf(pf, "METHOD       | select               | ops/+x/xlector_input.+x\n");
        fprintf(pf, "METHOD       | feed                 | ops/+x/feed_pet.+x\n");
        fprintf(pf, "METHOD       | pet                  | ops/+x/pet_pet.+x\n");
        fprintf(pf, "METHOD       | play                 | ops/+x/play_pet.+x\n");
        fprintf(pf, "METHOD       | export               | ops/+x/pet_export.+x\n\n");
        fprintf(pf, "RESPONSE     | default              | *%s settles in*\n", display_name);
        fclose(pf);
    }

    /* Log import to console - mutaclsym doesn't have message_log.txt */
    printf("Imported pet '%s' (%s) from exchange to %s\n", display_name, name, dest_map_id);
}

int main(int argc, char **argv) {
    resolve_root();

    /* Destination map: mutaclsym uses map_start as the default map.
     * mutaclsym doesn't have xlector - it uses a different selection
     * mechanism. We import to map_start where the hero lives. */
    char dest_map_id[128];
    snprintf(dest_map_id, sizeof(dest_map_id), "map_start");

    if (argc >= 2) {
        import_one(argv[1], dest_map_id);
        return 0;
    }

    char exchange_root[PATH_BUF];
    resolve_exchange_root(exchange_root, sizeof(exchange_root));
    DIR *d = opendir(exchange_root);
    if (!d) return 0;
    struct dirent *entry;
    char names[128][256];
    int n = 0;
    while (n < 128 && (entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(names[n], sizeof(names[0]), "%s", entry->d_name);
        n++;
    }
    closedir(d);
    for (int i = 0; i < n; i++) import_one(names[i], dest_map_id);
    return 0;
}
