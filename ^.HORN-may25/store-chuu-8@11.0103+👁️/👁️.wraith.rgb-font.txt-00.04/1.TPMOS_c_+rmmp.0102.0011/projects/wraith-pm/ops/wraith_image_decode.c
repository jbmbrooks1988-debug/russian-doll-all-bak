#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../../libraries/stb_image.h"

#define ASCII_RAMP " .`'-;+=x#&$M"

typedef struct {
    int r;
    int g;
    int b;
} RGB;

static int clamp_int(int value, int min_v, int max_v) {
    if (value < min_v) return min_v;
    if (value > max_v) return max_v;
    return value;
}

static char glyph_for_rgb(int r, int g, int b) {
    double avg = sqrt(((double)r * r + (double)g * g + (double)b * b) / 3.0);
    int ramp_len = (int)strlen(ASCII_RAMP);
    int idx = (int)round((avg / 255.0) * (double)(ramp_len - 1));
    idx = clamp_int(idx, 0, ramp_len - 1);
    return ASCII_RAMP[idx];
}

static void write_cell(FILE *out, int x, int y, char ch, RGB rgb, int object_id, const char *style) {
    fprintf(out,
            "CELL | x=%d,y=%d | ch=%02X fg=#%02X%02X%02X bg=#%02X%02X%02X object=%d style=%s\n",
            x, y, (unsigned char)ch, rgb.r, rgb.g, rgb.b, rgb.r, rgb.g, rgb.b, object_id, style);
}

static void write_fallback(FILE *out, const char *src, int cols, int rows, int object_id) {
    int y;
    int x;
    fprintf(out, "MEDIA | warning | image_decode_fallback\n");
    fprintf(out, "MEDIA | source | %s\n", src ? src : "");
    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            RGB rgb;
            rgb.r = 60 + (x * 120) / (cols > 1 ? cols - 1 : 1);
            rgb.g = 80 + (y * 120) / (rows > 1 ? rows - 1 : 1);
            rgb.b = 130 + ((x + y) * 80) / (cols + rows > 2 ? cols + rows - 2 : 1);
            write_cell(out, x, y, ((x + y) % 3 == 0) ? '#' : ((x + y) % 3 == 1) ? ':' : '.', rgb, object_id, "image_fallback");
        }
    }
}

static unsigned char *load_ppm_p3(const char *src, int *width, int *height) {
    FILE *f = fopen(src, "r");
    char magic[8];
    int max_value = 0;
    int count;
    unsigned char *pixels = NULL;
    int i;

    if (!f) return NULL;
    if (fscanf(f, "%7s", magic) != 1 || strcmp(magic, "P3") != 0) {
        fclose(f);
        return NULL;
    }
    if (fscanf(f, "%d %d", width, height) != 2 || *width <= 0 || *height <= 0) {
        fclose(f);
        return NULL;
    }
    if (fscanf(f, "%d", &max_value) != 1 || max_value <= 0) {
        fclose(f);
        return NULL;
    }

    count = (*width) * (*height) * 3;
    pixels = (unsigned char *)malloc((size_t)count);
    if (!pixels) {
        fclose(f);
        return NULL;
    }

    for (i = 0; i < count; i++) {
        int value = 0;
        if (fscanf(f, "%d", &value) != 1) {
            free(pixels);
            fclose(f);
            return NULL;
        }
        value = clamp_int(value, 0, max_value);
        pixels[i] = (unsigned char)((value * 255) / max_value);
    }

    fclose(f);
    return pixels;
}

int main(int argc, char **argv) {
    const char *src = NULL;
    const char *out_path = NULL;
    int cols;
    int rows;
    int object_id;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *pixels = NULL;
    int pixels_from_stbi = 0;
    FILE *out = NULL;
    int y;
    int x;

    if (argc < 6) {
        fprintf(stderr, "usage: %s <src> <out_pdl> <cols> <rows> <object_id>\n", argv[0]);
        return 2;
    }

    src = argv[1];
    out_path = argv[2];
    cols = atoi(argv[3]);
    rows = atoi(argv[4]);
    object_id = atoi(argv[5]);
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    out = fopen(out_path, "w");
    if (!out) {
        perror("wraith_image_decode fopen");
        return 1;
    }

    fprintf(out, "SECTION | KEY | VALUE\n");
    fprintf(out, "MEDIA | kind | image\n");
    fprintf(out, "MEDIA | source | %s\n", src);
    fprintf(out, "MEDIA | cols | %d\n", cols);
    fprintf(out, "MEDIA | rows | %d\n", rows);
    fprintf(out, "MEDIA | object_id | %d\n", object_id);

    pixels = stbi_load(src, &width, &height, &channels, 3);
    if (pixels) pixels_from_stbi = 1;
    if (!pixels) pixels = load_ppm_p3(src, &width, &height);
    if (!pixels || width <= 0 || height <= 0) {
        write_fallback(out, src, cols, rows, object_id);
        if (pixels) {
            if (pixels_from_stbi) stbi_image_free(pixels);
            else free(pixels);
        }
        fclose(out);
        return 0;
    }

    fprintf(out, "MEDIA | decoded_width | %d\n", width);
    fprintf(out, "MEDIA | decoded_height | %d\n", height);

    for (y = 0; y < rows; y++) {
        int src_y = (y * height) / rows;
        if (src_y >= height) src_y = height - 1;
        for (x = 0; x < cols; x++) {
            int src_x = (x * width) / cols;
            int idx;
            RGB rgb;
            char ch;
            if (src_x >= width) src_x = width - 1;
            idx = (src_y * width + src_x) * 3;
            rgb.r = pixels[idx];
            rgb.g = pixels[idx + 1];
            rgb.b = pixels[idx + 2];
            ch = glyph_for_rgb(rgb.r, rgb.g, rgb.b);
            write_cell(out, x, y, ch, rgb, object_id, "image");
        }
    }

    if (pixels_from_stbi) stbi_image_free(pixels);
    else free(pixels);
    fclose(out);
    return 0;
}
