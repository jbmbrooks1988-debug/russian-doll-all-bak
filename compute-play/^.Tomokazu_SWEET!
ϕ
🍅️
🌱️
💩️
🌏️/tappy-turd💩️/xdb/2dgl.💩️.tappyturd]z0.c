#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <math.h>

// --- Configuration ---
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PIPE_WIDTH 80
#define PIPE_GAP_START 200
#define PIPE_SPEED 3.0f
#define GRAVITY 0.8f
#define JUMP_FORCE -15.0f
#define MIN_GAP 150
#define PIPE_SPAWN_X (WINDOW_WIDTH + PIPE_WIDTH)

// --- Globals ---
FT_Library ft;
FT_Face emoji_face;
Display *x_display = NULL;
Window x_window;

// Player (the poop)
float player_x = 200.0f;
float player_y = WINDOW_HEIGHT / 2.0f;
float velocity = 0.0f;

// Pipes (pairs of top + bottom)
typedef struct {
    float x;
    float gap_top;
    float gap_size;
} Pipe;

Pipe pipes[10];
int num_pipes = 3;
int pipe_index = 0; // For circular reuse

// Game state
int score = 0;
int high_score = 0;
int game_running = 1;
int level = 1;

// Emoji rendering scale
float emoji_scale = 1.0f;

// Font & text color
float font_color[3] = {1.0f, 1.0f, 1.0f};

// --- Function Prototypes ---
void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y, float scale);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void timer(int value);
void init_pipes();
void update_game();
void draw_rect(float x, float y, float w, float h, float color[3]);
void show_game_over();
void reset_game();

// --- UTF-8 Decoder ---
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

// --- Render Emoji using FreeType ---
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
}

// --- Render Text ---
void render_text(const char* str, float x, float y) {
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
    glColor3f(1.0f, 1.0f, 1.0f);
}

// --- Initialize FreeType ---
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

    err = FT_Set_Pixel_Sizes(emoji_face, 0, 64);
    if (err) {
        fprintf(stderr, "❌ Could not set pixel size\n");
        exit(1);
    }

    emoji_scale = 1.8f; // Adjust based on visual fit
    printf("✅ Emoji font loaded!\n");
}

// --- Draw Colored Rectangle ---
void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// --- Initialize Pipes ---
void init_pipes() {
    for (int i = 0; i < num_pipes; i++) {
        pipes[i].x = PIPE_SPAWN_X + i * (WINDOW_WIDTH / 2);
        pipes[i].gap_top = rand() % (WINDOW_HEIGHT - 300) + 50;
        pipes[i].gap_size = 300 - (level * 20);
        if (pipes[i].gap_size < MIN_GAP) pipes[i].gap_size = MIN_GAP;
    }
}

// --- Update Game Logic ---
void update_game() {
    if (!game_running) return;

    // Physics
    velocity += GRAVITY;
    player_y += velocity;

    // Clamp player
    if (player_y < 0 || player_y > WINDOW_HEIGHT) {
        show_game_over();
        return;
    }

    // Move pipes
    int passed = 0;
    for (int i = 0; i < num_pipes; i++) {
        pipes[i].x -= PIPE_SPEED;

        // Check pass
        if (pipes[i].x + PIPE_WIDTH < player_x && pipes[i].x + PIPE_WIDTH > player_x - PIPE_SPEED) {
            score++;
            passed = 1;
        }

        // Respawn off-screen pipe
        if (pipes[i].x < -PIPE_WIDTH) {
            pipes[i].x = PIPE_SPAWN_X;
            pipes[i].gap_top = rand() % (WINDOW_HEIGHT - 300) + 50;
            pipes[i].gap_size = 300 - (level * 20);
            if (pipes[i].gap_size < MIN_GAP) pipes[i].gap_size = MIN_GAP;
        }

        // Collision
        if (player_x + 40 > pipes[i].x && player_x < pipes[i].x + PIPE_WIDTH) {
            if (player_y < pipes[i].gap_top || player_y > pipes[i].gap_top + pipes[i].gap_size) {
                show_game_over();
                return;
            }
        }
    }

    if (passed) {
        if (score % 5 == 0) {
            level++;
            PIPE_SPEED += 0.5f;
            printf("🚀 Level up! Now %d\n", level);
        }
    }

    glutPostRedisplay();
}

// --- Game Over Screen ---
void show_game_over() {
    game_running = 0;
    if (score > high_score) {
        high_score = score;
        printf("🏆 NEW HIGH SCORE: %d!\n", high_score);
    }

    glutDisplayFunc([](){
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float red[3] = {1.0f, 0.0f, 0.0f};
        float green[3] = {0.0f, 1.0f, 0.0f};

        render_text("💥 GAME OVER!", 300, 400);
        char buf[100];
        snprintf(buf, 100, "Score: %d", score);
        render_text(buf, 300, 350);
        snprintf(buf, 100, "High Score: %d", high_score);
        render_text(buf, 300, 320);
        render_text("Press SPACE to play again, 'q' to quit", 200, 200);

        // Render 💩 and 🚽
        unsigned int code;
        decode_utf8((unsigned char*)"💩", &code);
        render_emoji(code, 200, 400, emoji_scale);
        decode_utf8((unsigned char*)"🚽", &code);
        render_emoji(code, 580, 400, emoji_scale);

        glutSwapBuffers();
    });
    glutPostRedisplay();
}

// --- Reset Game ---
void reset_game() {
    player_y = WINDOW_HEIGHT / 2;
    velocity = 0;
    score = 0;
    level = 1;
    PIPE_SPEED = 3.0f;
    game_running = 1;
    init_pipes();
    glutDisplayFunc(display);
    glutPostRedisplay();
}

// --- Display Callback ---
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Background
    float bg[3] = {0.0f, 0.1f, 0.2f};
    draw_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, bg);

    // Pipes
    float pipe_color[3] = {0.6f, 0.3f, 0.0f};
    for (int i = 0; i < num_pipes; i++) {
        // Top pipe
        draw_rect(pipes[i].x, 0, PIPE_WIDTH, pipes[i].gap_top, pipe_color);
        // Bottom pipe
        float bottom_y = pipes[i].gap_top + pipes[i].gap_size;
        draw_rect(pipes[i].x, bottom_y, PIPE_WIDTH, WINDOW_HEIGHT - bottom_y, pipe_color);
    }

    // Render 💩
    unsigned int code;
    decode_utf8((unsigned char*)"💩", &code);
    render_emoji(code, player_x + 20, player_y, emoji_scale);

    // Render 🚽 at end
    decode_utf8((unsigned char*)"🚽", &code);
    render_emoji(code, WINDOW_WIDTH - 100, WINDOW_HEIGHT / 2, emoji_scale * 1.5f);

    // HUD
    char buf[50];
    snprintf(buf, 50, "Score: %d | Level: %d", score, level);
    render_text(buf, 10, WINDOW_HEIGHT - 30);

    glutSwapBuffers();
}

// --- Keyboard ---
void keyboard(unsigned char key, int x, int y) {
    if (key == ' ' && game_running) {
        velocity = JUMP_FORCE;
        printf("\a"); // Beep
    } else if (key == ' ' && !game_running) {
        reset_game();
    } else if (key == 'q') {
        exit(0);
    }
}

// --- Timer for smooth update ---
void timer(int value) {
    update_game();
    glutTimerFunc(16, timer, 0); // ~60 FPS
}

// --- Reshape ---
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

// --- Init ---
void init() {
    srand(time(NULL));
    initFreeType();
    init_pipes();
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
}

// --- Main ---
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("Tappy Turd: Flappy Poop Adventure 🚽💩");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);

    init();
    printf("🎮 Press SPACE to jump! Avoid the pipes and reach the toilet!\n");

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);
    return 0;
}
