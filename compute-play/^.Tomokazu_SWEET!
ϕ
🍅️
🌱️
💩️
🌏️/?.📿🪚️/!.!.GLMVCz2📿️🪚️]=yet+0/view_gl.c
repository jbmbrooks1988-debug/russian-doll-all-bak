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

void initFreeType(void);
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);
void draw_rect(float x, float y, float w, float h, float color[3]);
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void get_viewport_size(int *width, int *height);
void get_camera_position(float *x, float *y, float *z);
void get_camera_orientation(float *pitch, float *yaw, float *roll);
int get_num_entities(void);
void get_entity_position(int index, float *x, float *y, float *z);
int get_entity_type(int index);
const char* get_entity_emoji(int type);
int get_world_value(float x, float y, float z);
const char* get_status_message(void);
int get_score(void);

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
    int tile_size = 40;
    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, tile_size - 10);
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
    emoji_scale = (float)(tile_size * 0.8f) / (float)loaded_emoji_size;
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
    printf("FreeType initialized: font loaded, emoji_scale = %f\n", emoji_scale);
}

void render_emoji(unsigned int codepoint, float x, float y) {
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        fprintf(stderr, "Error: Could not load glyph for codepoint U+%04X, error code: %d\n", codepoint, err);
        return;
    }
    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer || slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        fprintf(stderr, "Error: Invalid bitmap for glyph U+%04X\n", codepoint);
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
    float w = slot->bitmap.width * emoji_scale;
    float h = slot->bitmap.rows * emoji_scale;
    float x2 = x - w / 2;
    float y2 = y - h / 2;
    printf("Rendering emoji U+%04X at screen (%.1f, %.1f), size (%.1f, %.1f)\n", codepoint, x2, y2, w, h);
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST); // Disable depth test for 2D rendering

    int width, height;
    get_viewport_size(&width, &height);
    float cam_x, cam_y, cam_z, pitch, yaw, roll;
    get_camera_position(&cam_x, &cam_y, &cam_z);
    get_camera_orientation(&pitch, &yaw, &roll);

    // Set 2D orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Draw background rectangle
    float field_color[3] = {0.2f, 0.2f, 0.2f};
    draw_rect(10, 50, width - 20, height - 100, field_color);

    // Center view on hero (entity 0)
    float hero_x, hero_y, hero_z;
    get_entity_position(0, &hero_x, &hero_y, &hero_z);
    int hero_row = (get_num_entities() > 0) ? (int)(hero_y / 40) : 0;
    int hero_col = (get_num_entities() > 0) ? (int)(hero_x / 40) : 0;
    int view_rows = (height - 100) / 40; // 12
    int view_cols = (width - 20) / 40;  // 12
    int tile_size = 40;

    // Draw world tiles centered on hero
    for (int r = 0; r < view_rows; r++) {
        for (int c = 0; c < view_cols; c++) {
            int world_row = hero_row - (view_rows / 2) + r;
            int world_col = hero_col - (view_cols / 2) + c;
            float x = 10 + c * tile_size + tile_size / 2;
            float y = height - 50 - (r + 1) * tile_size + tile_size / 2;
            int type = get_world_value(world_col * tile_size, world_row * tile_size, 0);
            if (type >= 0) {
                unsigned int codepoint;
                decode_utf8((const unsigned char*)get_entity_emoji(type), &codepoint);
                printf("Rendering tile type %d at screen (%.1f, %.1f), world (%d, %d)\n", type, x, y, world_col, world_row);
                render_emoji(codepoint, x, y);
            }
        }
    }

    // Draw entities
    for (int i = 0; i < get_num_entities(); i++) {
        float x, y, z;
        get_entity_position(i, &x, &y, &z);
        if (z != 0) continue; // Skip 3D entities
        int entity_row = (int)(y / tile_size);
        int entity_col = (int)(x / tile_size);
        if (abs(entity_row - hero_row) <= view_rows / 2 && abs(entity_col - hero_col) <= view_cols / 2) {
            x = 10 + (entity_col - (hero_col - view_cols / 2)) * tile_size + tile_size / 2;
            y = height - 50 - (view_rows - (entity_row - (hero_row - view_rows / 2))) * tile_size + tile_size / 2;
            unsigned int codepoint;
            decode_utf8((const unsigned char*)get_entity_emoji(get_entity_type(i)), &codepoint);
            printf("Rendering entity %d (type %d) at screen (%.1f, %.1f), world (%d, %d)\n", 
                   i, get_entity_type(i), x, y, entity_col, entity_row);
            render_emoji(codepoint, x, y);
        }
    }

    // Draw UI
    float panel_color[3] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, width, 80, panel_color);
    char score_str[50];
    snprintf(score_str, 50, "Score: %d", get_score());
    render_text(score_str, 10, height - 30);
    render_text(get_status_message(), 10, 10);

    glEnable(GL_DEPTH_TEST); // Re-enable depth test for potential 3D use
    glutSwapBuffers();
}
