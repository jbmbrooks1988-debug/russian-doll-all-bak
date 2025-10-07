#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>

FT_Library ft;
FT_Face emoji_face;
float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};
Display *x_display = NULL;
Window x_window;

int get_window_width(void);
int get_window_height(void);
int get_grid_rows(void);
int get_grid_cols(void);
int get_tile_size(void);
int get_grid_value(int r, int c);
int get_selected_row(void);
int get_selected_col(void);
int get_score(void);
const char* get_candy_emoji(int type);
const char* get_status_message(void);

void initFreeType(void) {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        exit(1);
    }
    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error: Could not load emoji font at %s, error code: %d\n", emoji_font_path, err);
        emoji_face = NULL;
        exit(1);
    }
    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, get_tile_size() - 10);
        if (err) {
            fprintf(stderr, "Error: Could not set pixel size for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            exit(1);
        }
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
        if (err) {
            fprintf(stderr, "Error: Could not select size for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            exit(1);
        }
    } else {
        fprintf(stderr, "Error: No fixed sizes available in emoji font\n");
        FT_Done_Face(emoji_face);
        emoji_face = NULL;
        exit(1);
    }
    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)(get_tile_size() * 0.8f) / (float)loaded_emoji_size;
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
}

void render_emoji(unsigned int codepoint, float x, float y) {
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        fprintf(stderr, "Error: Could not load glyph for codepoint U+%04X, error code: %d\n", codepoint, err);
        return;
    }
    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer) {
        fprintf(stderr, "Error: No bitmap for glyph U+%04X\n", codepoint);
        return;
    }
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        fprintf(stderr, "Error: Incorrect pixel mode for glyph U+%04X\n", codepoint);
        return;
    }
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float scale_factor = emoji_scale;
    float w = slot->bitmap.width * scale_factor;
    float h = slot->bitmap.rows * scale_factor;
    float x2 = x - w / 2;
    float y2 = y - h / 2;
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex2f(x2, y2);
    glTexCoord2f(1.0, 1.0); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0, 0.0); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0, 0.0); glVertex2f(x2, y2 + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void render_text(const char* str, float x, float y) {
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
    glColor3f(1.0f, 1.0f, 1.0f);
}

void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
}

int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0) {
        if ((str[1] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            return 2;
        }
    }
    if ((str[0] & 0xF0) == 0xE0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            return 3;
        }
    }
    if ((str[0] & 0xF8) == 0xF0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            return 4;
        }
    }
    *codepoint = '?';
    return 1;
}

void display(void) {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, get_window_width(), 0, get_window_height(), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float grid_color[3] = {0.4f, 0.4f, 0.4f};
    for (int r = 0; r < get_grid_rows(); r++) {
        for (int c = 0; c < get_grid_cols(); c++) {
            float x = 10 + c * get_tile_size();
            float y = get_window_height() - 50 - (r + 1) * get_tile_size();
            draw_rect(x, y, get_tile_size(), get_tile_size(), grid_color);
        }
    }
    for (int r = 0; r < get_grid_rows(); r++) {
        for (int c = 0; c < get_grid_cols(); c++) {
            int type = get_grid_value(r, c);
            if (type == -1) continue;
            float x = 10 + c * get_tile_size() + get_tile_size() / 2;
            float y = get_window_height() - 50 - (r + 1) * get_tile_size() + get_tile_size() / 2;
            const char *emoji = get_candy_emoji(type);
            unsigned int codepoint;
            decode_utf8((const unsigned char*)emoji, &codepoint);
            render_emoji(codepoint, x, y);
        }
    }
    if (get_selected_row() != -1) {
        float sel_color[3] = {1.0f, 1.0f, 0.0f};
        float x = 10 + get_selected_col() * get_tile_size();
        float y = get_window_height() - 50 - (get_selected_row() + 1) * get_tile_size();
        glLineWidth(3.0f);
        glColor3fv(sel_color);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + get_tile_size(), y);
        glVertex2f(x + get_tile_size(), y + get_tile_size());
        glVertex2f(x, y + get_tile_size());
        glEnd();
        glColor3f(1.0f, 1.0f, 1.0f);
    }
    char score_str[50];
    snprintf(score_str, 50, "Score: %d", get_score());
    render_text(score_str, 10, get_window_height() - 30);
    render_text(get_status_message(), 10, 10);
    glutSwapBuffers();
}
