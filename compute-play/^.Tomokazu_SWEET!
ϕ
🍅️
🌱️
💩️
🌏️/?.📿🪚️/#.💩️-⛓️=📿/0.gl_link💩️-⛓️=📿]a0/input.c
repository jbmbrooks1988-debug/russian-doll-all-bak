// input.c
#include <GL/glut.h>
#include <stdlib.h>  // For exit()

extern float player_y;
extern float velocity;
extern int game_running;
extern void init_pipes();
extern float pipe_speed;
extern int level;
extern void play_sfx(const char* freq);

void game_keyboard(unsigned char key, int x, int y) {
    if (key == ' ' && game_running) {
        velocity = -15.0f;
        play_sfx("E6");
    } else if (key == ' ' && !game_running) {
        player_y = 300.0f;
        velocity = 0.0f;
        pipe_speed = 3.0f;
        level = 1;
        game_running = 1;
        init_pipes();
        play_sfx("G5");
    } else if (key == 'q') {
        exit(0);
    }
}
