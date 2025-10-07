#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>

// --- Globals ---

FT_Library ft;
FT_Face emoji_face;

Display *x_display = NULL;
Window x_window;

#define GRID_ROWS 3
#define GRID_COLS 3
#define NUM_SYMBOLS 5
#define TILE_SIZE 100
#define SPIN_DURATION 2.0

const char *symbols[NUM_SYMBOLS] = {"🍒", "🍋", "🍊", "💎", "🔔"};
int grid[GRID_ROWS][GRID_COLS];
bool is_spinning = false;
float spin_time = 0.0f;
int score = 0;
int bet = 10;

int window_width = GRID_COLS * TILE_SIZE + 20;
int window_height = GRID_ROWS * TILE_SIZE + 100;

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};
char status_message[256] = "Press SPACE to spin!";

// --- Function Prototypes ---

void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void init();
void idle();
void draw_rect(float x, float y, float w, float h, float color[3]);
void spin_reels();
bool check_win();
void set_status_message(const char* msg);

// --- UTF8 & Font Helpers ---

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

void initFreeType() {
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
        err = FT_Set_Pixel_Sizes(emoji_face, 0, TILE_SIZE - 10);
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
    emoji_scale = (float)(TILE_SIZE * 0.8f) / (float)loaded_emoji_size;
    fprintf(stderr, "Emoji font loaded, loaded size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
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

void set_status_message(const char* msg) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

// --- Game Logic ---

void spin_reels() {
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            grid[r][c] = rand() % NUM_SYMBOLS;
        }
    }
}

bool check_win() {
    if (grid[1][0] == grid[1][1] && grid[1][1] == grid[1][2]) {
        score += bet * 10;
        set_status_message("Jackpot! +100 points!");
        return true;
    }
    if (grid[0][0] == grid[1][1] && grid[1][1] == grid[2][2]) {
        score += bet * 5;
        set_status_message("Diagonal Win! +50 points!");
        return true;
    }
    if (grid[0][2] == grid[1][1] && grid[1][1] == grid[2][0]) {
        score += bet * 5;
        set_status_message("Diagonal Win! +50 points!");
        return true;
    }
    set_status_message("No win. Try again!");
    return false;
}

// --- Display ---

void display() {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float grid_color[3] = {0.4f, 0.4f, 0.4f};
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = 10 + c * TILE_SIZE;
            float y = window_height - 50 - (r + 1) * TILE_SIZE;
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, grid_color);
        }
    }

    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = 10 + c * TILE_SIZE + TILE_SIZE / 2;
            float y = window_height - 50 - (r + 1) * TILE_SIZE + TILE_SIZE / 2;
            const char *emoji = symbols[grid[r][c]];
            unsigned int codepoint;
            decode_utf8((const unsigned char*)emoji, &codepoint);
            render_emoji(codepoint, x, y);
        }
    }

    char score_str[50];
    snprintf(score_str, 50, "Score: %d  Bet: %d", score, bet);
    render_text(score_str, 10, window_height - 30);
    render_text("SPACE: Spin  Q: Quit", 10, window_height - 60);
    render_text(status_message, 10, 10);

    glutSwapBuffers();
}

// --- GLUT Callbacks ---

void keyboard(unsigned char key, int x, int y) {
    if (key == ' ' && !is_spinning) {
        is_spinning = true;
        spin_time = 0.0f;
        set_status_message("Spinning...");
        glutPostRedisplay();
    } else if (key == 'q' || key == 'Q') {
        if (emoji_face) FT_Done_Face(emoji_face);
        FT_Done_FreeType(ft);
        exit(0);
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle() {
    if (is_spinning) {
        spin_time += 0.016f;
        if (spin_time < SPIN_DURATION) {
            spin_reels();
            glutPostRedisplay();
        } else {
            is_spinning = false;
            check_win();
            glutPostRedisplay();
        }
    }
}

// --- Main ---

void init() {
    initFreeType();
    srand(time(NULL));
    spin_reels();
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Slot Machine");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(idle);

    init();

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);
    return 0;
}
