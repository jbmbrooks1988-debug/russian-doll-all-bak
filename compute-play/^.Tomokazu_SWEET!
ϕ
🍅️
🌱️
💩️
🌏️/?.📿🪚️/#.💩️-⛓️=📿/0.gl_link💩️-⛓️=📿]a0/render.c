// render.c
#include <GL/glut.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

// Redefine struct (no .h, so repeat it)
typedef struct {
    float x;
    float gap_top;
    float gap_size;
} Pipe;

// Declare extern variables
extern float player_x, player_y, velocity, pipe_speed;
extern int score, high_score, game_running, level;
extern Pipe pipes[10];  // This refers to the real array in game_logic.c

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PIPE_WIDTH 80

// Forward declare emoji functions (defined in main.c or elsewhere)
void render_emoji(unsigned int codepoint, float x, float y, float scale);
int decode_utf8(const unsigned char* str, unsigned int* codepoint);

// Draw colored rectangle
void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// Procedural background
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
            default:
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

// Main display function
void game_display() {
    static float bg_time = 0.0f;
    if (game_running) bg_time += 0.1f;

    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (game_running) {
        draw_procedural_bg(bg_time);

        float pipe_color[3] = {0.6f, 0.3f, 0.0f};
        for (int i = 0; i < 3; i++) {
            float top_h = pipes[i].gap_top;
            float bot_y = pipes[i].gap_top + pipes[i].gap_size;
            draw_rect(pipes[i].x, WINDOW_HEIGHT - top_h, PIPE_WIDTH, top_h, pipe_color);
            draw_rect(pipes[i].x, 0, PIPE_WIDTH, WINDOW_HEIGHT - bot_y, pipe_color);
        }

        unsigned int code;
        decode_utf8((unsigned char*)"💩", &code);
        render_emoji(code, player_x + 20, WINDOW_HEIGHT - player_y, 1.8f);

        decode_utf8((unsigned char*)"🚽", &code);
        render_emoji(code, WINDOW_WIDTH - 100, WINDOW_HEIGHT - 300, 2.7f);

        char buf[50];
        sprintf(buf, "Score: %d | Level: %d", score, level);
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(10, WINDOW_HEIGHT - 30);
        for (char* c = buf; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    } else {
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(300, 400);
        const char* msg = "💥 GAME OVER!";
        for (const char* c = msg; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

        char buf[100];
        sprintf(buf, "Score: %d | High: %d", score, high_score);
        glRasterPos2f(300, 350);
        for (char* c = buf; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }

    glutSwapBuffers();
}
