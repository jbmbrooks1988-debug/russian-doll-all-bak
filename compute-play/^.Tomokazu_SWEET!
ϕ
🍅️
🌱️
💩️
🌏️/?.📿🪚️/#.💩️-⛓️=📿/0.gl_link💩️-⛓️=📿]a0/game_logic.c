// game_logic.c
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
extern void save_high_score(int score);
// Define the Pipe struct here (and elsewhere that needs it)
typedef struct {
    float x;
    float gap_top;
    float gap_size;
} Pipe;

// Now declare the array
Pipe pipes[10];  // This is the real array

// Game state
float player_x = 200.0f;
float player_y = 600 / 2;
float velocity = 0.0f;
float pipe_speed = 3.0f;

int score = 0;
int high_score = 0;
int game_running = 1;
int level = 1;

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PIPE_WIDTH 80
#define MIN_GAP 150

// Function to init pipes
void init_pipes() {
    for (int i = 0; i < 3; i++) {
        pipes[i].x = 800 + i * 400;
        pipes[i].gap_top = rand() % (600 - 300) + 50;
        pipes[i].gap_size = 300 - (level * 20);
        if (pipes[i].gap_size < MIN_GAP) pipes[i].gap_size = MIN_GAP;
    }
}

// Update player physics
void update_player() {
    velocity += 0.8f;
    player_y += velocity;

    if (player_y < 0 || player_y > WINDOW_HEIGHT) {
        game_running = 0;
        if (score > high_score) {
            high_score = score;
            // Assume save_high_score is defined in util.c
            save_high_score(high_score);
        }
    }
}

// Update pipes and check collisions
void update_pipes() {
    int passed = 0;
    for (int i = 0; i < 3; i++) {
        pipes[i].x -= pipe_speed;

        if (pipes[i].x + PIPE_WIDTH < player_x && pipes[i].x + PIPE_WIDTH > player_x - pipe_speed) {
            score++;
            passed = 1;
        }

        if (pipes[i].x < -PIPE_WIDTH) {
            pipes[i].x = 800;
            pipes[i].gap_top = rand() % (600 - 300) + 50;
            pipes[i].gap_size = 300 - (level * 20);
            if (pipes[i].gap_size < MIN_GAP) pipes[i].gap_size = MIN_GAP;
        }

        float bottom_y = pipes[i].gap_top + pipes[i].gap_size;
        if (player_x + 40 > pipes[i].x && player_x < pipes[i].x + PIPE_WIDTH) {
            if (player_y < pipes[i].gap_top || player_y > bottom_y) {
                game_running = 0;
                if (score > high_score) {
                    high_score = score;
                    save_high_score(high_score);
                }
            }
        }
    }

    if (passed && score % 5 == 0) {
        level++;
        pipe_speed += 0.5f;
    }
}

// Init game
void init_game() {
    srand(12345);
    FILE* f = fopen("highscore.txt", "r");
    if (f) {
        fscanf(f, "%d", &high_score);
        fclose(f);
    }
    init_pipes();
}
