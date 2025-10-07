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
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// --- Configuration ---
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PIPE_WIDTH 80
#define PIPE_GAP_START 200
#define PIPE_SPEED_BASE 3.0f
#define GRAVITY 0.8f
#define JUMP_FORCE -15.0f
#define MIN_GAP 150
#define PIPE_SPAWN_X (WINDOW_WIDTH + PIPE_WIDTH)
#define HIGHSCORE_FILE "highscore.txt"

// --- Globals ---
FT_Library ft;
FT_Face emoji_face;
Display *x_display = NULL;
Window x_window;

float player_x = 200.0f;
float player_y = WINDOW_HEIGHT / 2.0f;
float velocity = 0.0f;
float pipe_speed = PIPE_SPEED_BASE;

int score = 0;
int high_score = 0;
int game_running = 1;
int level = 1;

float emoji_scale = 1.8f;
float font_color[3] = {1.0f, 1.0f, 1.0f};

pid_t music_pid = 0;

typedef struct {
    float x;
    float gap_top;
    float gap_size;
} Pipe;

Pipe pipes[10];
int num_pipes = 3;

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
void display_game_over();  // <-- Now a real function
void show_game_over();
void reset_game();
void draw_procedural_bg(float time);
void save_high_score(int score);
int load_high_score();
void start_music();
void stop_music();
void play_sfx(const char* freq);
int decode_utf8(const unsigned char* str, unsigned int* codepoint);

// --- UTF-8 Decoder ---
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

// --- Render Emoji ---
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

// --- Procedural Background ---
void draw_procedural_bg(float time) {
    for (int x = 0; x < WINDOW_WIDTH; x++) {
        float hue = fmodf(x * 0.01f + time * 0.05f, 1.0f);
        float saturation = 0.7f;
        float value = 0.4f + 0.3f * sinf(x * 0.02f + time * 0.1f);

        int i = (int)(hue * 6.0f);
        float f = hue * 6.0f - i;
        float p = value * (1 - saturation);
        float q = value * (1 - saturation * f);
        float t = value * (1 - saturation * (1 - f));

        float r, g, b;
        switch (i % 6) {
            case 0: r = value; g = t; b = p; break;
            case 1: r = q; g = value; b = p; break;
            case 2: r = p; g = value; b = t; break;
            case 3: r = p; g = q; b = value; break;
            case 4: r = t; g = p; b = value; break;
            case 5: r = value; g = p; b = q; break;
        }

        glBegin(GL_LINES);
        glColor3f(r, g, b);
        glVertex2f(x, 0);
        glVertex2f(x, WINDOW_HEIGHT);
        glEnd();
    }
    glColor3f(1.0f, 1.0f, 1.0f);
}

// --- Draw Rectangle ---
void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// --- Load/Save High Score ---
int load_high_score() {
    FILE* f = fopen(HIGHSCORE_FILE, "r");
    int score = 0;
    if (f) {
        fscanf(f, "%d", &score);
        fclose(f);
    }
    return score;
}

void save_high_score(int score) {
    FILE* f = fopen(HIGHSCORE_FILE, "w");
    if (f) {
        fprintf(f, "%d", score);
        fclose(f);
    }
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

// --- Play SFX ---
void play_sfx(const char* freq) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("sox", "sox", "-n", "-d", "synth", "0.1", "sine", freq, "gain", "-3", NULL);
        exit(0);
    }
}

// --- Start Music ---
void start_music() {
    music_pid = fork();
    if (music_pid == 0) {
        execlp("sox", "sox", "-n", "-d", "synth", "0.2", "sine", "C4", "delay", "0.2", "0.4", "remix", "v1,v2", "gain", "-10", "loop", "1000", NULL);
        exit(0);
    }
}

// --- Stop Music ---
void stop_music() {
    if (music_pid > 0) {
        kill(music_pid, SIGTERM);
        waitpid(music_pid, NULL, 0);
        music_pid = 0;
    }
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

    // NotoColorEmoji is bitmap-only — use FT_Select_Size
    if (emoji_face->num_fixed_sizes == 0) {
        fprintf(stderr, "❌ NotoColorEmoji has no bitmap sizes!\n");
        exit(1);
    }

    // Find best match for desired size (e.g. ~64px)
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

    // Update emoji_scale based on actual size
    int actual_size = emoji_face->available_sizes[best_strike].height;
    emoji_scale = 64.0f / actual_size;  // Scale to target rendering size
    printf("✅ Emoji font loaded with size %dpx. Scale = %.2f\n",
           actual_size, emoji_scale);
}

// --- Update Game Logic ---
void update_game() {
    if (!game_running) return;

    velocity += GRAVITY;
    player_y += velocity;

    if (player_y < 0 || player_y > WINDOW_HEIGHT) {
        stop_music();
        play_sfx("C2");
        show_game_over();
        return;
    }

    int passed = 0;
    for (int i = 0; i < num_pipes; i++) {
        pipes[i].x -= pipe_speed;

        if (pipes[i].x + PIPE_WIDTH < player_x && pipes[i].x + PIPE_WIDTH > player_x - pipe_speed) {
            score++;
            passed = 1;
        }

        if (pipes[i].x < -PIPE_WIDTH) {
            pipes[i].x = PIPE_SPAWN_X;
            pipes[i].gap_top = rand() % (WINDOW_HEIGHT - 300) + 50;
            pipes[i].gap_size = 300 - (level * 20);
            if (pipes[i].gap_size < MIN_GAP) pipes[i].gap_size = MIN_GAP;
        }

        if (player_x + 40 > pipes[i].x && player_x < pipes[i].x + PIPE_WIDTH) {
            if (player_y < pipes[i].gap_top || player_y > pipes[i].gap_top + pipes[i].gap_size) {
                stop_music();
                play_sfx("C2");
                show_game_over();
                return;
            }
        }
    }

    if (passed && score % 5 == 0) {
        level++;
        pipe_speed += 0.5f;
        play_sfx("G5");
    }

    glutPostRedisplay();
}

// --- Display Game Over Screen ---
void display_game_over() {
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    render_text("💥 GAME OVER!", 300, 400);
    char buf[100];
    snprintf(buf, sizeof(buf), "Score: %d", score);
    render_text(buf, 300, 350);
    snprintf(buf, sizeof(buf), "High Score: %d", high_score);
    render_text(buf, 300, 320);
    render_text("Press SPACE to play again, 'q' to quit", 200, 200);

    unsigned int code;
    decode_utf8((const unsigned char*)"💩", &code);
    render_emoji(code, 200, 400, emoji_scale);
    decode_utf8((const unsigned char*)"🚽", &code);
    render_emoji(code, 580, 400, emoji_scale * 1.5f);

    glutSwapBuffers();
}

// --- Show Game Over ---
void show_game_over() {
    game_running = 0;
    if (score > high_score) {
        high_score = score;
        save_high_score(high_score);
    }
    glutDisplayFunc(display_game_over);
    glutPostRedisplay();
}

// --- Reset Game ---
void reset_game() {
    player_y = WINDOW_HEIGHT / 2;
    velocity = 0;
    score = 0;
    level = 1;
    pipe_speed = PIPE_SPEED_BASE;
    game_running = 1;
    init_pipes();
    start_music();
    glutDisplayFunc(display);
    glutPostRedisplay();
}

// --- Main Display ---
void display() {
    static float bg_time = 0.0f;
    bg_time += 0.1f;

    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    draw_procedural_bg(bg_time);

    float pipe_color[3] = {0.6f, 0.3f, 0.0f};
    for (int i = 0; i < num_pipes; i++) {
        draw_rect(pipes[i].x, 0, PIPE_WIDTH, pipes[i].gap_top, pipe_color);
        float bottom_y = pipes[i].gap_top + pipes[i].gap_size;
        draw_rect(pipes[i].x, bottom_y, PIPE_WIDTH, WINDOW_HEIGHT - bottom_y, pipe_color);
    }

    unsigned int code;
    decode_utf8((const unsigned char*)"💩", &code);
    render_emoji(code, player_x + 20, player_y, emoji_scale);

    decode_utf8((const unsigned char*)"🚽", &code);
    render_emoji(code, WINDOW_WIDTH - 100, WINDOW_HEIGHT / 2, emoji_scale * 1.5f);

    char buf[50];
    snprintf(buf, sizeof(buf), "Score: %d | Level: %d", score, level);
    render_text(buf, 10, WINDOW_HEIGHT - 30);

    glutSwapBuffers();
}

// --- Keyboard ---
void keyboard(unsigned char key, int x, int y) {
    if (key == ' ' && game_running) {
        velocity = JUMP_FORCE;
        play_sfx("E6");
    } else if (key == ' ' && !game_running) {
        reset_game();
    } else if (key == 'q') {
        stop_music();
        exit(0);
    }
}

// --- Timer ---
void timer(int value) {
    update_game();
    glutTimerFunc(16, timer, 0);
}

// --- Reshape ---
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

// --- Init ---
void init() {
    srand(time(NULL));
    high_score = load_high_score();
    initFreeType();
    init_pipes();
    start_music();
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

    glutMainLoop();

    stop_music();
    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);
    return 0;
}
