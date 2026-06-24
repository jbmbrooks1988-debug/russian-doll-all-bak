#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define GLYPH_W 8
#define GLYPH_H 16
#define COLS 128
#define ROWS 40
#define WIDTH (COLS * GLYPH_W)
#define HEIGHT (ROWS * GLYPH_H)

unsigned char glyphs[128][GLYPH_W * GLYPH_H];

void load_glyphs() {
    printf("[RGB-DAEMON] Loading ASCII set from assets...\n");
    for (int i = 32; i < 127; i++) {
        char path[1024];
        snprintf(path, sizeof(path), "projects/wraith-alpha/assets/fonts/ascii/%d/glyph.txt", i);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char line[64];
        int y = 0;
        while (fgets(line, sizeof(line), f) && y < GLYPH_H) {
            for (int x = 0; x < GLYPH_W && line[x] != '\0' && line[x] != '\n'; x++) {
                glyphs[i][y * GLYPH_W + x] = (line[x] == '#') ? 255 : 0;
            }
            y++;
        }
        fclose(f);
    }
}

void blit_char(unsigned char *buffer, int col, int row, unsigned char c, unsigned char r, unsigned char g, unsigned char b) {
    if (c > 127) return;
    int start_x = col * GLYPH_W;
    int start_y = row * GLYPH_H;

    for (int y = 0; y < GLYPH_H; y++) {
        for (int x = 0; x < GLYPH_W; x++) {
            int dx = start_x + x;
            int dy = start_y + y;
            if (dx >= WIDTH || dy >= HEIGHT) continue;

            if (glyphs[c][y * GLYPH_W + x]) {
                int idx = (dy * WIDTH + dx) * 4;
                buffer[idx] = r;
                buffer[idx + 1] = g;
                buffer[idx + 2] = b;
                buffer[idx + 3] = 255;
            }
        }
    }
}

void render_frame(const char *frame_path, unsigned char *buffer) {
    // Clear buffer (Dark Blue background per GL-OS look)
    for (int i = 0; i < WIDTH * HEIGHT * 4; i += 4) {
        buffer[i] = 0;
        buffer[i + 1] = 0;
        buffer[i + 2] = 68; // #000044
        buffer[i + 3] = 255;
    }

    FILE *f = fopen(frame_path, "r");
    if (!f) return;

    char line[1024];
    int row = 0;
    while (fgets(line, sizeof(line), f) && row < ROWS) {
        for (int col = 0; col < (int)strlen(line) && col < COLS; col++) {
            unsigned char c = line[col];
            if (c == '\n' || c == '\r') break;
            
            // Simple logic: Highlight focused items [>] in cyan, others in light gray
            // This is a POC approximation until we parse objects.pdl
            unsigned char r = 200, g = 200, b = 200;
            if (strstr(line, "[>]")) {
                r = 0; g = 255; b = 255;
            }

            blit_char(buffer, col, row, c, r, g, b);
        }
        row++;
    }
    fclose(f);
}

void pulse_rgb() {
    FILE *f = fopen("projects/wraith-alpha/session/rgb/rgb_frame_changed.txt", "a");
    if (f) {
        fprintf(f, "P\n");
        fclose(f);
    }
}

int main() {
    printf("[RGB-DAEMON] Starting Middle Fork Rasterizer...\n");
    
    load_glyphs();

    unsigned char *buffer = malloc(WIDTH * HEIGHT * 4);
    if (!buffer) return 1;

    struct stat st;
    off_t last_size = 0;
    const char *trigger = "pieces/display/frame_changed.txt";
    const char *frame_src = "pieces/display/current_frame.txt";
    const char *output = "projects/wraith-alpha/session/rgb/current_frame.rgba32";

    if (stat(trigger, &st) == 0) last_size = st.st_size;

    while (1) {
        int dirty = 0;
        if (stat(trigger, &st) == 0) {
            if (st.st_size != last_size) {
                last_size = st.st_size;
                dirty = 1;
            }
        }

        if (dirty) {
            render_frame(frame_src, buffer);
            FILE *f = fopen(output, "wb");
            if (f) {
                fwrite(buffer, 1, WIDTH * HEIGHT * 4, f);
                fclose(f);
                pulse_rgb();
            }
        }
        usleep(16667); // 60 FPS scan
    }
    
    free(buffer);
    return 0;
}
