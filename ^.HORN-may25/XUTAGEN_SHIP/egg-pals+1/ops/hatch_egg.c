/* hatch_egg - one verb, one binary, no shared headers.
 * Flips an egg piece into a pet: type=egg -> type=pet, hatched=0 -> 1,
 * rolls starting stats from the species' rarity, and runs the emoji
 * pipeline for real (emoji_gen_atlas -> emoji_xtract) to produce the
 * pet's own sprite.csv, which egg_window reads as a GL texture.
 *
 * Usage: hatch_egg.+x <pet_piece_id>
 * Prints a one-line result message to stdout. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define SPRITE_RES 32 /* NxN sprite.csv resolution egg_window expects */

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pet_piece_id>\n", argv[0]);
        return 1;
    }
    const char *pet_id = argv[1];
    resolve_root();

    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/pieces/world_01/map_lobby/%s", project_root, pet_id);
    char state_path[PATH_BUF + 32];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", dir);

    FILE *f = fopen(state_path, "r");
    if (!f) { printf("Hatch failed: unknown piece.\n"); return 1; }

    char lines[32][MAX_LINE];
    int nlines = 0;
    int hatched = 0, rarity = 1;
    char emoji[32] = "";
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "hatched") == 0) hatched = atoi(eq + 1);
            else if (strcmp(lines[nlines], "rarity") == 0) rarity = atoi(eq + 1);
            else if (strcmp(lines[nlines], "species_emoji") == 0) snprintf(emoji, sizeof(emoji), "%s", eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    if (hatched) { printf("%s has already hatched.\n", pet_id); return 0; }
    if (!emoji[0]) { printf("Hatch failed: no species_emoji on %s.\n", pet_id); return 1; }

    int hp = 10 + rarity * 10;
    int mp = 5 + rarity * 5;

    /* Run the emoji pipeline for real: glyph -> PNG -> NxN pixel CSV. */
    char png_path[PATH_BUF + 32], csv_path[PATH_BUF + 32], cmd[PATH_BUF * 4];
    snprintf(png_path, sizeof(png_path), "%s/atlas.png", dir);
    snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", dir);

    snprintf(cmd, sizeof(cmd), "'%s/system/emoji_gen_atlas' '%s' '%s'", project_root, emoji, png_path);
    int rc1 = system(cmd);
    snprintf(cmd, sizeof(cmd), "'%s/system/emoji_xtract' '%s' 0 %d '%s'", project_root, png_path, SPRITE_RES, csv_path);
    int rc2 = system(cmd);
    if (rc1 != 0 || rc2 != 0) {
        printf("Hatch warning: sprite generation failed, hatching without sprite.\n");
    }

    f = fopen(state_path, "w");
    if (!f) { printf("Hatch failed: could not write state.\n"); return 1; }
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "type") == 0) { fprintf(f, "type=pet\n"); *eq = '='; continue; }
            if (strcmp(lines[i], "hatched") == 0) { fprintf(f, "hatched=1\n"); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fprintf(f, "hp=%d\n", hp);
    fprintf(f, "hp_max=%d\n", hp);
    fprintf(f, "mp=%d\n", mp);
    fprintf(f, "mp_max=%d\n", mp);
    fprintf(f, "skills=peck\n");
    fclose(f);

    printf("%s hatched! HP:%d MP:%d\n", pet_id, hp, mp);
    return 0;
}
