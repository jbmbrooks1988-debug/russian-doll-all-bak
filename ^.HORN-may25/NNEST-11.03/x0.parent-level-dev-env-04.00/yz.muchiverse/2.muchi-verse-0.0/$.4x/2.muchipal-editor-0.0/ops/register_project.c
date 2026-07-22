/* register_project - one verb, one binary, no shared headers.
 * Closes a real, named gap from MUCHIPAL-EDITOR-DESIGN.txt sec. 2/3.1
 * and 0.HANDOFF-2026-07-16.md before it: the only way to add a project
 * to pieces/registry/known_projects.txt used to be hand-editing the
 * file, which is exactly how it went stale in the first place (both
 * rows this session found pointed at dead, pre-reorganization paths -
 * see that file's own 2026-07-21 header comment). This op is a
 * one-shot CLI tool, not wired into the interactive UI (a real,
 * deliberate scope cut - see design doc sec. 3.1 for why a future
 * "Register Project" screen is a separate, later step):
 *
 *   ops/+x/register_project.+x <root_path> [display_name]
 *
 * Auto-detects, from real files under <root_path> (never guessed from
 * convention alone - "if it's not in a file, it's a lie", per this
 * whole family's own doctrine):
 *   - map_rel_path: the first pieces/world_NAME/map_start/map.txt found,
 *     preferring a non-template one (a path WITHOUT "_template" in it)
 *     over a template one, matching how all 4 current $.4x game
 *     projects are actually laid out (each keeps both a *_home and a
 *     *_home_template world dir; the template is not the live map).
 *   - registry_format/registry_rel_path: "pipe" if pieces/registry/
 *     terrain/terrain_types.txt exists (glyph|id|name|walkable|
 *     rgb_top - all 4 $.4x game projects plus mutaclsym); else "equals"
 *     if pieces/registry/tiles/registry.txt exists (piececraft-3d-
 *     pal's own glyph=id shape). Neither found -> refuses to register
 *     rather than writing a broken row.
 *   - id: <root_path>'s own basename, lowercased. display_name: the
 *     optional second argument, or the id itself if omitted.
 *
 * Appends one row; does not de-duplicate or update an existing row for
 * the same id (a real, named limitation - re-running this after a
 * project moves again will produce a second row rather than replacing
 * the first; whoever hits that should hand-edit known_projects.txt to
 * remove the stale row first, same as before this op existed).
 *
 * Schema (extended 2026-07-21 for instance placement, sec. 3.2):
 *   id|display_name|path|map_rel_path|registry_format|registry_rel_path|
 *   bank_rel_path|instance_dir|spawn_op_rel_path|has_civ_id
 * The last 4 fields may be empty/0 if no *_bank.txt was found under the
 * project (mutaclsym, which predates the bank/instance convention) -
 * instance placement is simply unavailable for such a project. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Runs find <root>/pieces -path pattern matching world_NAME/map_start/
 * map.txt and
 * returns the first non-template hit if any exist, else the first hit
 * of any kind, else empty. A real subprocess call rather than manual
 * directory-tree walking in C - matches project_browser.c's own
 * load_pieces() precedent (popen+find) elsewhere in this same file
 * family. */
static int find_map_rel_path(const char *root, char *out, size_t out_sz) {
    char cmd[PATH_BUF + 128];
    snprintf(cmd, sizeof(cmd),
             "find '%s/pieces' -path '*/world_*/map_start/map.txt' 2>/dev/null", root);
    FILE *pf = popen(cmd, "r");
    if (!pf) return 0;
    char line[PATH_BUF];
    char best_any[PATH_BUF] = "";
    char best_nontemplate[PATH_BUF] = "";
    while (fgets(line, sizeof(line), pf)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        if (!best_any[0]) snprintf(best_any, sizeof(best_any), "%s", line);
        if (!best_nontemplate[0] && !strstr(line, "_template")) {
            snprintf(best_nontemplate, sizeof(best_nontemplate), "%s", line);
        }
    }
    pclose(pf);
    const char *chosen = best_nontemplate[0] ? best_nontemplate : best_any;
    if (!chosen[0]) return 0;
    size_t root_len = strlen(root);
    const char *rel = (strncmp(chosen, root, root_len) == 0) ? chosen + root_len : chosen;
    while (*rel == '/') rel++;
    snprintf(out, out_sz, "%s", rel);
    return 1;
}

int main(int argc, char **argv) {
    resolve_root();

    if (argc < 2) {
        fprintf(stderr, "usage: register_project <root_path> [display_name]\n");
        return 1;
    }
    const char *root = argv[1];
    if (!file_exists(root)) {
        fprintf(stderr, "error: root path does not exist: %s\n", root);
        return 1;
    }

    char id[64];
    {
        const char *base = strrchr(root, '/');
        base = base ? base + 1 : root;
        int i = 0;
        for (const char *p = base; *p && i < (int)sizeof(id) - 1; p++) {
            id[i++] = (char)tolower((unsigned char)*p);
        }
        id[i] = '\0';
        if (!id[0]) { fprintf(stderr, "error: could not derive an id from root path\n"); return 1; }
    }
    char display_name[128];
    snprintf(display_name, sizeof(display_name), "%s", (argc >= 3) ? argv[2] : id);

    char map_rel_path[512];
    if (!find_map_rel_path(root, map_rel_path, sizeof(map_rel_path))) {
        fprintf(stderr, "error: no pieces/world_*/map_start/map.txt found under %s - refusing to register a project with no real map\n", root);
        return 1;
    }

    char pipe_check[PATH_BUF], equals_check[PATH_BUF];
    snprintf(pipe_check, sizeof(pipe_check), "%s/pieces/registry/terrain/terrain_types.txt", root);
    snprintf(equals_check, sizeof(equals_check), "%s/pieces/registry/tiles/registry.txt", root);
    const char *registry_format;
    char registry_rel_path[128];
    if (file_exists(pipe_check)) {
        registry_format = "pipe";
        snprintf(registry_rel_path, sizeof(registry_rel_path), "pieces/registry/terrain/terrain_types.txt");
    } else if (file_exists(equals_check)) {
        registry_format = "equals";
        snprintf(registry_rel_path, sizeof(registry_rel_path), "pieces/registry/tiles/registry.txt");
    } else {
        fprintf(stderr, "error: neither pieces/registry/terrain/terrain_types.txt (pipe) nor pieces/registry/tiles/registry.txt (equals) found under %s - refusing to register a project with no known tile registry shape\n", root);
        return 1;
    }

    /* Bank/instance detection (added for instance placement, sec. 3.2
     * of MUCHIPAL-EDITOR-DESIGN.txt): finds the FIRST pieces/registry/
     * <kind>/<kind_sing>_bank.txt under root (all 4 $.4x game projects
     * follow this exact naming - unit_bank.txt, angler_bank.txt,
     * player_bank.txt, entity_bank.txt), and derives:
     *   instance_dir  - the bank's own singular name + 's' (unit_bank
     *                    -> "units", matching each project's real
     *                    world_NAME/<instance_dir>/<id>/state.txt layout -
     *                    confirmed against all 4 projects' real
     *                    directories, not assumed from the name alone).
     *   spawn_op_rel  - "ops/+x/spawn_<kind_sing>.+x" (spawn_unit,
     *                    spawn_angler, spawn_player, spawn_entity - same
     *                    naming convention, confirmed to exist for all
     *                    4 real projects).
     *   has_civ_id    - grepped from that spawn op's OWN C source
     *                    Usage: line (civ-pal/pal-craft/muchimon-pal's
     *                    spawn ops take a civ_id argument; angler-
     *                    empires' spawn_angler.c does not - anglers have
     *                    no faction concept) - read from the real file,
     *                    never assumed.
     * mutaclsym (pre-dates this whole bank/instance convention) simply
     * has no *_bank.txt - all four fields stay empty, and instance
     * placement is unavailable for it (terrain editing is unaffected). */
    char bank_rel_path[256] = "", instance_dir[64] = "", spawn_op_rel[128] = "";
    int has_civ_id = 0;
    {
        char cmd[PATH_BUF + 128];
        snprintf(cmd, sizeof(cmd), "find '%s/pieces/registry' -maxdepth 2 -name '*_bank.txt' 2>/dev/null | head -1", root);
        FILE *pf = popen(cmd, "r");
        if (pf) {
            char line[PATH_BUF];
            if (fgets(line, sizeof(line), pf)) {
                line[strcspn(line, "\r\n")] = '\0';
                if (line[0]) {
                    size_t root_len = strlen(root);
                    const char *rel = (strncmp(line, root, root_len) == 0) ? line + root_len : line;
                    while (*rel == '/') rel++;
                    /* rel/base alias line[PATH_BUF] - gcc can't prove
                     * from the buffer's own declared size that a real
                     * pieces/registry/.../foo_bank.txt path is this
                     * short, but every real one this session has
                     * touched is well under 256/64 bytes; truncation
                     * would only ever occur for a pathologically deep
                     * project layout, same class of narrow suppression
                     * used elsewhere in this project family. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                    snprintf(bank_rel_path, sizeof(bank_rel_path), "%s", rel);

                    const char *base = strrchr(line, '/');
                    base = base ? base + 1 : line;
                    char singular[64];
                    snprintf(singular, sizeof(singular), "%s", base);
#pragma GCC diagnostic pop
                    char *suffix = strstr(singular, "_bank.txt");
                    if (suffix) *suffix = '\0';

                    /* Naive English pluralization ("unit"->"units",
                     * "angler"->"anglers", "player"->"players" - plain
                     * +s covers all of those) needs one real-world
                     * exception: "entity"->"entities" (muchimon-pal's
                     * own real instance dir), the standard consonant+y
                     * -> "ies" rule - checked against all 4 $.4x
                     * projects' actual on-disk instance directory
                     * names, not assumed from the bank filename alone. */
                    size_t slen = strlen(singular);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                    if (slen > 1 && singular[slen - 1] == 'y' &&
                        strchr("aeiouAEIOU", singular[slen - 2]) == NULL) {
                        char stem[64];
                        snprintf(stem, sizeof(stem), "%.*s", (int)(slen - 1), singular);
                        snprintf(instance_dir, sizeof(instance_dir), "%sies", stem);
                    } else {
                        snprintf(instance_dir, sizeof(instance_dir), "%ss", singular);
                    }
#pragma GCC diagnostic pop
                    snprintf(spawn_op_rel, sizeof(spawn_op_rel), "ops/+x/spawn_%s.+x", singular);

                    char spawn_src[PATH_BUF];
                    snprintf(spawn_src, sizeof(spawn_src), "%s/ops/spawn_%s.c", root, singular);
                    FILE *sf = fopen(spawn_src, "r");
                    if (sf) {
                        char sline[MAX_PATH];
                        while (fgets(sline, sizeof(sline), sf)) {
                            if (strstr(sline, "Usage:") && strstr(sline, "civ_id")) { has_civ_id = 1; break; }
                        }
                        fclose(sf);
                    }
                }
            }
            pclose(pf);
        }
    }

    char reg_path[PATH_BUF];
    snprintf(reg_path, sizeof(reg_path), "%s/pieces/registry/known_projects.txt", project_root);
    FILE *rf = fopen(reg_path, "a");
    if (!rf) {
        fprintf(stderr, "error: could not open %s for append\n", reg_path);
        return 1;
    }
    fprintf(rf, "%s|%s|%s|%s|%s|%s|%s|%s|%s|%d\n", id, display_name, root, map_rel_path,
            registry_format, registry_rel_path, bank_rel_path, instance_dir, spawn_op_rel, has_civ_id);
    fclose(rf);

    printf("registered: %s|%s|%s|%s|%s|%s|%s|%s|%s|%d\n", id, display_name, root, map_rel_path,
           registry_format, registry_rel_path, bank_rel_path, instance_dir, spawn_op_rel, has_civ_id);
    return 0;
}
