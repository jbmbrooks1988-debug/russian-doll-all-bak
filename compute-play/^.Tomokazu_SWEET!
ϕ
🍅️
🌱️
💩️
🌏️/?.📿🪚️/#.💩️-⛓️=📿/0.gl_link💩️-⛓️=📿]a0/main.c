// main.c
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <X11/Xlib.h>
#include <math.h>

// ——— Function & Variable Declarations ———
extern void init_game();
extern void game_display();
extern void game_keyboard(unsigned char key, int x, int y);
extern void start_music();
extern void play_sfx(const char* freq);
extern int game_running;

// ——— Global State ———
int window_width = 800;
int window_height = 600;
FT_Library ft;
FT_Face emoji_face;
Display *x_display = NULL;
Window x_window;
float emoji_scale = 1.8f;

// ——— UTF-8 Decoder ———
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0 && (str[1] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    }
    if ((str[0] & 0xF0) == 0xE0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    }
    if ((str[0] & 0xF8) == 0xF0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }
    *codepoint = '?';
    return 1;
}

// ——— FreeType Initialization ———
void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "❌ Could not init FreeType\n");
        exit(1);
    }

    const char *font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "❌ Could not load %s\n", font_path);
        exit(1);
    }

    if (emoji_face->num_fixed_sizes == 0) {
        fprintf(stderr, "❌ NotoColorEmoji has no bitmap sizes!\n");
        exit(1);
    }

    int target = 64;
    int best_strike = 0;
    for (int i = 1; i < emoji_face->num_fixed_sizes; i++) {
        if (abs(emoji_face->available_sizes[i].height - target) <
            abs(emoji_face->available_sizes[best_strike].height - target)) {
            best_strike = i;
        }
    }

    err = FT_Select_Size(emoji_face, best_strike);
    if (err) {
        fprintf(stderr, "❌ FT_Select_Size failed: %d\n", err);
        exit(1);
    }

    int actual_size = emoji_face->available_sizes[best_strike].height;
    emoji_scale = 64.0f / actual_size;
    printf("✅ Emoji font loaded with size %dpx. Scale = %.2f\n", actual_size, emoji_scale);
}

// ——— Render Emoji ———
void render_emoji(unsigned int codepoint, float x, float y, float scale) {
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) return;

    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer || slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) return;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = slot->bitmap.width * scale;
    float h = slot->bitmap.rows * scale;
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

// ——— Game Timer ———
extern float player_y, velocity;
extern int game_running;
void update_player();
void update_pipes();

void game_timer(int value) {
    if (game_running) {
        update_player();
        update_pipes();
    }
    game_display();
    glutTimerFunc(16, game_timer, 0); // ~60 FPS
}

// ——— Main ———
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Tappy Turd: Flappy Poop Adventure 🚽💩");

    // Initialize systems
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
    initFreeType();

    // Set callbacks
    glutDisplayFunc(game_display);
    glutKeyboardFunc(game_keyboard);
    glutTimerFunc(0, game_timer, 0);

    // Start audio and game
    start_music();
    init_game();

    printf("🎮 Tappy Turd launched! Press SPACE to jump.\n");
    glutMainLoop();

    // Cleanup
    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);
    return 0;
}
