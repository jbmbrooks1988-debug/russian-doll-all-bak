#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>

// External variables
extern FT_Library ft;
extern FT_Face emoji_face;
extern float emoji_scale;
extern float font_color[3];
extern float background_color[4];
extern Display *x_display;
extern Window x_window;

// Function declarations
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void render_emoji(unsigned int codepoint, float x, float y, float fg[3], float bg[3]);
void render_emoji_3d_quad(unsigned int codepoint, float x, float y, float z, float fg[3], float bg[3]);
void render_text(const char* str, float x, float y);
void draw_rect(float x, float y, float w, float h, float color[3]);
void draw_border(float x, float y, float w, float h, float color[3]);

// Model function declarations
int get_canvas_rows(void);
int get_canvas_cols(void);
int get_tile_size(void);
int get_num_emojis(void);
int get_num_colors(void);
int get_max_layers(void);
int get_sidebar_width(void);
int get_file_tab_height(void);
int get_window_width(void);
int get_window_height(void);
int get_tab_count(void);
const char* get_emoji(int idx);
const char* get_color(int idx, int component);
const char* get_color_name(int idx);
const char* get_status_message(void);
int get_selected_emoji(void);
int get_selected_fg_color(void);
int get_selected_bg_color(void);
int get_selected_tool(void);
int get_start_row(void);
int get_start_col(void);
int get_selector_row(void);
int get_selector_col(void);
int get_canvas_tile(int layer, int r, int c, int field);
int get_tab_bank(int idx, int field);
int get_view_mode(void);
int get_current_z_level(void);
float get_camera_x(void);
float get_camera_y(void);
float get_camera_z(void);
float get_camera_yaw(void);
float get_camera_pitch(void);

void initFreeType(void) {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        exit(1);
    }

    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err || emoji_face == NULL) {
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
    fprintf(stderr, "Emoji font loaded, loaded size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
}

void render_emoji(unsigned int codepoint, float x, float y, float fg[3], float bg[3]) {
    if (emoji_face == NULL) {
        fprintf(stderr, "Error: Emoji font not loaded\n");
        return;
    }
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw background first
    draw_rect(x - get_tile_size()/2, y - get_tile_size()/2, get_tile_size(), get_tile_size(), bg);

    // Then draw emoji
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glColor3fv(fg);

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

void render_emoji_3d_quad(unsigned int codepoint, float x, float y, float z, float fg[3], float bg[3]) {
    if (emoji_face == NULL) {
        fprintf(stderr, "Error: Emoji font not loaded\n");
        return;
    }
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw background first (as a 3D quad)
    glColor3fv(bg);
    glBegin(GL_QUADS);
    glVertex3f(x - get_tile_size()/2, y - get_tile_size()/2, z);
    glVertex3f(x + get_tile_size()/2, y - get_tile_size()/2, z);
    glVertex3f(x + get_tile_size()/2, y + get_tile_size()/2, z);
    glVertex3f(x - get_tile_size()/2, y + get_tile_size()/2, z);
    glEnd();

    // Then draw emoji texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glColor3fv(fg);

    float scale_factor = emoji_scale;
    float w = slot->bitmap.width * scale_factor;
    float h = slot->bitmap.rows * scale_factor;
    float x2 = x - w / 2;
    float y2 = y - h / 2;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex3f(x2, y2, z + 0.01f);
    glTexCoord2f(1.0, 1.0); glVertex3f(x2 + w, y2, z + 0.01f);
    glTexCoord2f(1.0, 0.0); glVertex3f(x2 + w, y2 + h, z + 0.01f);
    glTexCoord2f(0.0, 0.0); glVertex3f(x2, y2 + h, z + 0.01f);
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

void draw_border(float x, float y, float w, float h, float color[3]) {
    glLineWidth(2.0f);
    glColor3fv(color);
    glBegin(GL_LINE_LOOP);
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

void render_sidebar_ui(void) {
    float sidebar_x = 10;
    float tab_color[3] = {0.5f, 0.5f, 0.5f};
    float sel_color[3] = {1.0f, 1.0f, 0.0f};

    // Draw file tab with additional buttons
    draw_rect(sidebar_x, get_window_height() - get_file_tab_height() - 10, 60, get_file_tab_height(), tab_color);
    draw_rect(sidebar_x + 70, get_window_height() - get_file_tab_height() - 10, 60, get_file_tab_height(), tab_color);
    draw_rect(sidebar_x + 140, get_window_height() - get_file_tab_height() - 10, 60, get_file_tab_height(), tab_color);
    draw_rect(sidebar_x + 210, get_window_height() - get_file_tab_height() - 10, 60, get_file_tab_height(), tab_color);
    draw_rect(sidebar_x + 280, get_window_height() - get_file_tab_height() - 10, 60, get_file_tab_height(), tab_color);
    draw_rect(sidebar_x + 350, get_window_height() - get_file_tab_height() - 10, 60, get_file_tab_height(), tab_color);
    render_text("Save", sidebar_x + 10, get_window_height() - get_file_tab_height() + 5);
    render_text("Load", sidebar_x + 80, get_window_height() - get_file_tab_height() + 5);
    render_text("Text", sidebar_x + 150, get_window_height() - get_file_tab_height() + 5);
    render_text("Tile", sidebar_x + 220, get_window_height() - get_file_tab_height() + 5);
    render_text("2D", sidebar_x + 290, get_window_height() - get_file_tab_height() + 5);
    render_text("3D", sidebar_x + 360, get_window_height() - get_file_tab_height() + 5);

    // Draw sidebar (left side, below file tab)
    float sidebar_y_offset = get_file_tab_height() + 50;
    float base_y = get_window_height() - sidebar_y_offset - 20;

    // Emoji grid (8x8)
    int emoji_cols = 8;
    int emoji_rows = (get_num_emojis() + emoji_cols - 1) / emoji_cols;
    for (int i = 0; i < get_num_emojis(); i++) {
        int row = i / emoji_cols;
        int col = i % emoji_cols;
        float x = sidebar_x + 10 + col * 40;
        float y = base_y - row * 40;
        unsigned int codepoint;
        decode_utf8((const unsigned char*)get_emoji(i), &codepoint);
        float fg[3] = {1.0f, 1.0f, 1.0f};
        float bg[3] = {0.0f, 0.0f, 0.0f};
        render_emoji(codepoint, x + 20, y + 20, fg, bg);
        if (i == get_selected_emoji()) {
            draw_border(x, y, 40, 40, sel_color);
        }
    }

    // Colors (horizontal row below emojis)
    float colors_y_start = base_y - (emoji_rows * 40) - 20;
    int colors_cols = 8;
    int colors_rows = 1;
    for (int i = 0; i < get_num_colors(); i++) {
        int row = i / colors_cols;
        int col = i % colors_cols;
        float x = sidebar_x + 10 + col * 40;
        float y = colors_y_start - row * 40;
        float color[3] = {
            atof(get_color(i, 0)) / 255.0f,
            atof(get_color(i, 1)) / 255.0f,
            atof(get_color(i, 2)) / 255.0f
        };
        draw_rect(x, y, 40, 40, color);
        if (i == get_selected_fg_color()) {
            draw_border(x, y, 40, 40, sel_color);
        }
    }

    // Tab bank (vertical below colors)
    float tabs_y_start = colors_y_start - (colors_rows * 40) - 20;
    for (int i = 0; i < get_tab_count(); i++) {
        float x = sidebar_x + 10;
        float y = tabs_y_start - i * 40;
        unsigned int codepoint;
        decode_utf8((const unsigned char*)get_emoji(get_tab_bank(i, 0)), &codepoint);
        float fg[3] = {
            atof(get_color(get_tab_bank(i, 1), 0)) / 255.0f,
            atof(get_color(get_tab_bank(i, 1), 1)) / 255.0f,
            atof(get_color(get_tab_bank(i, 1), 2)) / 255.0f
        };
        float bg[3] = {
            atof(get_color(get_tab_bank(i, 2), 0)) / 255.0f,
            atof(get_color(get_tab_bank(i, 2), 1)) / 255.0f,
            atof(get_color(get_tab_bank(i, 2), 2)) / 255.0f
        };
        render_emoji(codepoint, x + 20, y + 20, fg, bg);
    }

    // Tools (horizontal below tabs)
    float tabs_section_height = get_tab_count() * 40;
    float tools_y_start = tabs_y_start - tabs_section_height - 20;
    const char *tools[] = {"Paint", "Fill", "Rect"};
    for (int i = 0; i < 3; i++) {
        float x = sidebar_x + 10 + i * 70;
        float y = tools_y_start;
        draw_rect(x, y, 60, 30, tab_color);
        render_text(tools[i], x + 10, y + 10);
        if (i == get_selected_tool()) {
            draw_border(x, y, 60, 30, sel_color);
        }
    }

    // Display current Z-level
    char z_level_str[50];
    sprintf(z_level_str, "Z-Level: %d", get_current_z_level());
    render_text(z_level_str, get_sidebar_width() + 10, 30);

    render_text(get_status_message(), get_sidebar_width() + 10, 10);
}

void display(void) {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float sidebar_x = 10;

    if (get_view_mode() == 0) { // 2D View
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, get_window_width(), 0, get_window_height(), -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Draw canvas grid
        float grid_color[3] = {0.6f, 0.6f, 0.6f};
        for (int r = 0; r < get_canvas_rows(); r++) {
            for (int c = 0; c < get_canvas_cols(); c++) {
                float x = get_sidebar_width() + 10 + c * get_tile_size();
                float y = get_window_height() - get_file_tab_height() - 10 - (r + 1) * get_tile_size();
                draw_border(x, y, get_tile_size(), get_tile_size(), grid_color);
            }
        }

        // Render only the current Z-level in 2D view
        int current_layer = get_current_z_level();
        for (int r = 0; r < get_canvas_rows(); r++) {
            for (int c = 0; c < get_canvas_cols(); c++) {
                if (get_canvas_tile(current_layer, r, c, 0) == -1) continue;
                float x = get_sidebar_width() + 10 + c * get_tile_size() + get_tile_size() / 2;
                float y = get_window_height() - get_file_tab_height() - 10 - (r + 1) * get_tile_size() + get_tile_size() / 2;
                const char *emoji = get_emoji(get_canvas_tile(current_layer, r, c, 0));
                unsigned int codepoint;
                decode_utf8((const unsigned char*)emoji, &codepoint);
                float fg[3] = {
                    atof(get_color(get_canvas_tile(current_layer, r, c, 1), 0)) / 255.0f,
                    atof(get_color(get_canvas_tile(current_layer, r, c, 1), 1)) / 255.0f,
                    atof(get_color(get_canvas_tile(current_layer, r, c, 1), 2)) / 255.0f
                };
                float bg[3] = {
                    atof(get_color(get_canvas_tile(current_layer, r, c, 2), 0)) / 255.0f,
                    atof(get_color(get_canvas_tile(current_layer, r, c, 2), 1)) / 255.0f,
                    atof(get_color(get_canvas_tile(current_layer, r, c, 2), 2)) / 255.0f
                };
                render_emoji(codepoint, x, y, fg, bg);
            }
        }

        // Draw tile selector
        float sel_color[3] = {1.0f, 1.0f, 0.0f};
        if (get_selected_tool() == 2 && get_start_row() != -1) {
            int r_min = get_start_row() < get_selector_row() ? get_start_row() : get_selector_row();
            int r_max = get_start_row() > get_selector_row() ? get_start_row() : get_selector_row();
            int c_min = get_start_col() < get_selector_col() ? get_start_col() : get_selector_col();
            int c_max = get_start_col() > get_selector_col() ? get_start_col() : get_selector_col();
            float x = get_sidebar_width() + 10 + c_min * get_tile_size();
            float y = get_window_height() - get_file_tab_height() - 10 - (r_max + 1) * get_tile_size();
            float w = (c_max - c_min + 1) * get_tile_size();
            float h = (r_max - r_min + 1) * get_tile_size();
            draw_border(x, y, w, h, sel_color);
        } else {
            float x = get_sidebar_width() + 10 + get_selector_col() * get_tile_size();
            float y = get_window_height() - get_file_tab_height() - 10 - (get_selector_row() + 1) * get_tile_size();
            draw_border(x, y, get_tile_size(), get_tile_size(), sel_color);
        }

    } else { // 3D View
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, (double)get_window_width() / get_window_height(), 1.0, 1000.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Log camera parameters for debugging
        printf("Camera: x=%.2f, y=%.2f, z=%.2f, yaw=%.2f, pitch=%.2f\n",
               get_camera_x(), get_camera_y(), get_camera_z(), get_camera_yaw(), get_camera_pitch());

        // Apply camera transformations
        glTranslatef(-get_camera_x(), -get_camera_y(), -get_camera_z());
        glRotatef(get_camera_pitch(), 1.0f, 0.0f, 0.0f);
        glRotatef(get_camera_yaw(), 0.0f, 1.0f, 0.0f);

        // Log canvas bounds for debugging
        float center_x = (get_canvas_cols() * get_tile_size()) / 2.0f;
        float center_y = (get_canvas_rows() * get_tile_size()) / 2.0f;
        float center_z = get_current_z_level() * get_tile_size();
        printf("Canvas bounds: x=[0,%.2f], y=[0,%.2f], z=%.2f\n",
               get_canvas_cols() * get_tile_size(), get_canvas_rows() * get_tile_size(), center_z);

        // Draw 3D cubes with emojis
        for (int layer = 0; layer < get_max_layers(); layer++) {
            for (int r = 0; r < get_canvas_rows(); r++) {
                for (int c = 0; c < get_canvas_cols(); c++) {
                    if (get_canvas_tile(layer, r, c, 0) == -1) continue;

                    printf("Rendering cube at layer=%d, row=%d, col=%d, emoji_idx=%d\n",
                           layer, r, c, get_canvas_tile(layer, r, c, 0));

                    float x = c * get_tile_size();
                    float y = r * get_tile_size();
                    float z = layer * get_tile_size();
                    float half_size = get_tile_size() / 2.0f;

                    glPushMatrix();
                    glTranslatef(x + half_size, y + half_size, z + half_size);

                    float cube_color[3] = {0.2f, 0.2f, 0.2f};
                    float border_color[3] = {0.4f, 0.4f, 0.4f};

                    // Draw all six faces of the cube
                    glBegin(GL_QUADS);
                    glColor3fv(cube_color);
                    // Front face (z = +half_size)
                    glVertex3f(-half_size, -half_size, half_size);
                    glVertex3f(half_size, -half_size, half_size);
                    glVertex3f(half_size, half_size, half_size);
                    glVertex3f(-half_size, half_size, half_size);
                    // Back face (z = -half_size)
                    glVertex3f(-half_size, -half_size, -half_size);
                    glVertex3f(-half_size, half_size, -half_size);
                    glVertex3f(half_size, half_size, -half_size);
                    glVertex3f(half_size, -half_size, -half_size);
                    // Top face (y = +half_size)
                    glVertex3f(-half_size, half_size, -half_size);
                    glVertex3f(-half_size, half_size, half_size);
                    glVertex3f(half_size, half_size, half_size);
                    glVertex3f(half_size, half_size, -half_size);
                    // Bottom face (y = -half_size)
                    glVertex3f(-half_size, -half_size, -half_size);
                    glVertex3f(half_size, -half_size, -half_size);
                    glVertex3f(half_size, -half_size, half_size);
                    glVertex3f(-half_size, -half_size, half_size);
                    // Left face (x = -half_size)
                    glVertex3f(-half_size, -half_size, -half_size);
                    glVertex3f(-half_size, -half_size, half_size);
                    glVertex3f(-half_size, half_size, half_size);
                    glVertex3f(-half_size, half_size, -half_size);
                    // Right face (x = +half_size)
                    glVertex3f(half_size, -half_size, -half_size);
                    glVertex3f(half_size, half_size, -half_size);
                    glVertex3f(half_size, half_size, half_size);
                    glVertex3f(half_size, -half_size, half_size);
                    glEnd();

                    // Draw emoji on front face
                    const char *emoji = get_emoji(get_canvas_tile(layer, r, c, 0));
                    unsigned int codepoint;
                    decode_utf8((const unsigned char*)emoji, &codepoint);
                    float fg[3] = {
                        atof(get_color(get_canvas_tile(layer, r, c, 1), 0)) / 255.0f,
                        atof(get_color(get_canvas_tile(layer, r, c, 1), 1)) / 255.0f,
                        atof(get_color(get_canvas_tile(layer, r, c, 1), 2)) / 255.0f
                    };
                    float bg[3] = {
                        atof(get_color(get_canvas_tile(layer, r, c, 2), 0)) / 255.0f,
                        atof(get_color(get_canvas_tile(layer, r, c, 2), 1)) / 255.0f,
                        atof(get_color(get_canvas_tile(layer, r, c, 2), 2)) / 255.0f
                    };
                    render_emoji_3d_quad(codepoint, 0, 0, half_size + 0.01f, fg, bg);

                    glPopMatrix();
                }
            }
        }

        // Draw 3D grid lines
        glLineWidth(1.0f);
        float grid_color[3] = {0.6f, 0.6f, 0.6f};
        glColor3fv(grid_color);
        glBegin(GL_LINES);
        // X-Y grid lines for each layer
        for (int layer = 0; layer < get_max_layers(); layer++) {
            float z = layer * get_tile_size();
            for (int r = 0; r <= get_canvas_rows(); r++) {
                float y = r * get_tile_size();
                glVertex3f(0, y, z);
                glVertex3f(get_canvas_cols() * get_tile_size(), y, z);
            }
            for (int c = 0; c <= get_canvas_cols(); c++) {
                float x = c * get_tile_size();
                glVertex3f(x, 0, z);
                glVertex3f(x, get_canvas_rows() * get_tile_size(), z);
            }
        }
        // Z-axis lines to connect layers
        for (int r = 0; r <= get_canvas_rows(); r++) {
            for (int c = 0; c <= get_canvas_cols(); c++) {
                float x = c * get_tile_size();
                float y = r * get_tile_size();
                glVertex3f(x, y, 0);
                glVertex3f(x, y, get_max_layers() * get_tile_size());
            }
        }
        glEnd();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
    }

    // Render UI overlay
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, get_window_width(), 0, get_window_height(), -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    render_sidebar_ui();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}
