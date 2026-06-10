#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int r;
    int g;
    int b;
} RGB;

static void write_cell(FILE *out, int x, int y, char ch, RGB fg, RGB bg, int object_id) {
    fprintf(out,
            "CELL | x=%d,y=%d | ch=%02X fg=#%02X%02X%02X bg=#%02X%02X%02X object=%d style=video\n",
            x, y, (unsigned char)ch, fg.r, fg.g, fg.b, bg.r, bg.g, bg.b, object_id);
}

int main(int argc, char **argv) {
    const char *src = NULL;
    const char *out_path = NULL;
    int cols;
    int rows;
    int object_id;
    int frame_index;
    FILE *out = NULL;
    int y;
    int x;
    int source_exists = 0;

    if (argc < 7) {
        fprintf(stderr, "usage: %s <src> <out_pdl> <cols> <rows> <object_id> <frame_index>\n", argv[0]);
        return 2;
    }

    src = argv[1];
    out_path = argv[2];
    cols = atoi(argv[3]);
    rows = atoi(argv[4]);
    object_id = atoi(argv[5]);
    frame_index = atoi(argv[6]);
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (frame_index < 0) frame_index = 0;
    source_exists = access(src, F_OK) == 0;

    out = fopen(out_path, "w");
    if (!out) {
        perror("wraith_video_decode fopen");
        return 1;
    }

    fprintf(out, "SECTION | KEY | VALUE\n");
    fprintf(out, "MEDIA | kind | video\n");
    fprintf(out, "MEDIA | source | %s\n", src);
    fprintf(out, "MEDIA | cols | %d\n", cols);
    fprintf(out, "MEDIA | rows | %d\n", rows);
    fprintf(out, "MEDIA | object_id | %d\n", object_id);
    fprintf(out, "MEDIA | frame_index | %d\n", frame_index);
    if (!source_exists) fprintf(out, "MEDIA | warning | video_source_missing_using_synthetic_frame\n");

    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            int wave = (x + y + frame_index) % 6;
            RGB fg;
            RGB bg;
            char ch;
            fg.r = 100 + ((x * 80) / (cols > 1 ? cols - 1 : 1));
            fg.g = 180 + ((y * 60) / (rows > 1 ? rows - 1 : 1));
            fg.b = 220 - ((x * 70) / (cols > 1 ? cols - 1 : 1));
            bg.r = 45 + ((frame_index * 15) % 40);
            bg.g = 30 + ((x + y) % 30);
            bg.b = 70 + ((y * 70) / (rows > 1 ? rows - 1 : 1));
            ch = (wave == 0) ? '~' : (wave == 1) ? '=' : (wave == 2) ? '-' : (wave == 3) ? ':' : (wave == 4) ? '.' : '*';
            write_cell(out, x, y, ch, fg, bg, object_id);
        }
    }

    fclose(out);
    return 0;
}
