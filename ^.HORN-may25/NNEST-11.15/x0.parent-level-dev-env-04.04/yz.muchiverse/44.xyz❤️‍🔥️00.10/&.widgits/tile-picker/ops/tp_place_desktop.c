/* tp_place_desktop - place current brush (or glyph) onto house desktop tray
 * Usage: tp_place_desktop.+x <widget_state_dir> <desktop_root> [glyph] [name]
 * If glyph omitted, reads brush.txt from widget_state_dir.
 * Writes #.desktop/tiles/<name>/ with glyph.txt + meta.pdl
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PATH_BUF 4352
#define MAX_LINE 2048

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "Usage: tp_place_desktop.+x <widget_state_dir> <desktop_root> [glyph] [name]\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *desk = argv[2];
    char glyph = 0;
    if (argc >= 4 && argv[3][0]) {
        glyph = argv[3][0];
    } else {
        char brush[PATH_BUF], line[MAX_LINE];
        snprintf(brush, sizeof(brush), "%s/brush.txt", wdir);
        FILE *bf = fopen(brush, "r");
        if (bf && fgets(line, sizeof(line), bf)) {
            glyph = line[0];
            fclose(bf);
        } else {
            if (bf) fclose(bf);
            fprintf(stderr, "tp_place_desktop: no brush\n");
            return 1;
        }
    }
    if (glyph < 32 || glyph > 126) return 1;

    char name[128];
    if (argc >= 5 && argv[4][0]) {
        snprintf(name, sizeof(name), "%s", argv[4]);
    } else {
        snprintf(name, sizeof(name), "tile_%c_%ld",
                 (glyph >= 'A' && glyph <= 'z') ? glyph : 'x',
                 (long)time(NULL));
    }

    char dir[PATH_BUF], cmd[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/tiles/%s", desk, name);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    if (system(cmd) != 0) return 1;

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", dir);
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "%c\n", glyph);
    fclose(f);

    snprintf(path, sizeof(path), "%s/meta.pdl", dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | piece_id           | %s\n", name);
        fprintf(f, "STATE        | kind                 | tile_stamp\n");
        fprintf(f, "STATE        | glyph                | %c\n", glyph);
        fprintf(f, "STATE        | created_at           | %ld\n", (long)time(NULL));
        fclose(f);
    }

    /* remember last place in widget state */
    char last[PATH_BUF];
    snprintf(last, sizeof(last), "%s/last_desktop_place.txt", wdir);
    f = fopen(last, "w");
    if (f) {
        fprintf(f, "path=%s\n", dir);
        fprintf(f, "glyph=%c\n", glyph);
        fclose(f);
    }

    printf("DESKTOP_TILE %s glyph=%c\n", dir, glyph);
    return 0;
}
