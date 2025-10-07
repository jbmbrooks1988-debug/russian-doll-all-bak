#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>

#define WIDTH 60
#define HEIGHT 20
#define PIPE_CHAR "||"
#define PLAYER_EMOJI "💩"
#define TOILET_EMOJI "🚽"
#define GAP_SIZE 6
#define GRAVITY 1
#define JUMP_FORCE -2
#define MIN_GAP 5

// For high score
int high_score = 0;

// Terminal input helper
int kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

int getch() {
    int ch;
    ch = getchar();
    return ch;
}

void init_term() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void reset_term() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    fcntl(STDIN_FILENO, F_SETFL, 0);
}

// Clear screen
void clear_screen() {
    printf("\x1b[2J\x1b[H");
}

// Draw game state
void draw(int player_y, int pipes[][2], int num_pipes, int score) {
    char screen[HEIGHT][WIDTH];
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            screen[i][j] = ' ';
        }
    }

    // Draw player
    if (player_y >= 0 && player_y < HEIGHT) {
        // Emoji takes ~2 spaces
        if (WIDTH - 5 >= 0) {
            int row = (int)player_y;
            if (row < HEIGHT) {
                sprintf(&screen[row][5], "%s", PLAYER_EMOJI);
            }
        }
    }

    // Draw pipes
    for (int i = 0; i < num_pipes; i++) {
        int x = pipes[i][0];
        int gap_top = pipes[i][1];
        int gap_bottom = gap_top + GAP_SIZE;

        for (int y = 0; y < gap_top; y++) {
            if (x >= 5 && x < WIDTH-2) {
                if (y < HEIGHT) {
                    sprintf(&screen[y][x], "%s", PIPE_CHAR);
                }
            }
        }
        for (int y = gap_bottom; y < HEIGHT; y++) {
            if (x >= 5 && x < WIDTH-2) {
                if (y < HEIGHT) {
                    sprintf(&screen[y][x], "%s", PIPE_CHAR);
                }
            }
        }
    }

    // Draw toilet at end
    int toilet_x = WIDTH - 10;
    int toilet_y = HEIGHT / 2;
    if (toilet_y < HEIGHT) {
        sprintf(&screen[toilet_y][toilet_x], "%s", TOILET_EMOJI);
    }

    // Print screen
    clear_screen();
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            printf("%c", screen[i][j]);
        }
        printf("\n");
    }
    printf("Score: %d | High Score: %d\n", score, high_score);
}

// Main game loop
void play_level(int level) {
    int player_y = HEIGHT / 2;
    int velocity = 0;
    int score = 0;
    int level_score = 0;
    int game_speed = 100000 - (level * 10000);  // Faster each level
    if (game_speed < 30000) game_speed = 30000;

    int pipes[10][2];  // x, gap_top
    int num_pipes = 3 + level;
    int pipe_gap = GAP_SIZE - level;
    if (pipe_gap < MIN_GAP) pipe_gap = MIN_GAP;

    // Initialize pipes
    for (int i = 0; i < num_pipes; i++) {
        pipes[i][0] = WIDTH + i * (WIDTH / 2);
        pipes[i][1] = rand() % (HEIGHT - pipe_gap - 2) + 1;
    }

    while (1) {
        // Input
        if (kbhit()) {
            int ch = getch();
            if (ch == 32) {  // Spacebar
                velocity = JUMP_FORCE;
                printf("\a");  // Beep
            }
        }

        // Physics
        velocity += GRAVITY;
        player_y += velocity;

        // Move pipes
        for (int i = 0; i < num_pipes; i++) {
            pipes[i][0]--;

            // Reset pipe when off screen
            if (pipes[i][0] < -2) {
                pipes[i][0] = WIDTH;
                pipes[i][1] = rand() % (HEIGHT - pipe_gap - 2) + 1;
                level_score++;
            }
        }

        // Collision with floor/ceiling
        if (player_y < 0 || player_y >= HEIGHT) {
            break;
        }

        // Collision with pipes
        int hit = 0;
        for (int i = 0; i < num_pipes; i++) {
            int x = pipes[i][0];
            int gap_top = pipes[i][1];
            int gap_bottom = gap_top + pipe_gap;

            if (5 + 2 >= x && 5 <= x + 2) {  // Player x overlaps pipe
                if (player_y < gap_top || player_y >= gap_bottom) {
                    hit = 1;
                }
            }
        }
        if (hit) break;

        // Reached toilet?
        if (5 > WIDTH - 10 && 5 < WIDTH - 10 + 2) {
            score = level_score;
            printf("\n🎉 Level %d complete! 🎉\n", level);
            sleep(2);
            play_level(level + 1);  // Next level
            return;
        }

        draw((int)player_y, pipes, num_pipes, level_score);
        usleep(game_speed);
    }

    // Game over
    score = level_score;
    if (score > high_score) {
        high_score = score;
        printf("\n🏆 NEW HIGH SCORE: %d!\n", high_score);
    } else {
        printf("\n💥 Game Over! Score: %d\n", score);
    }
    printf("🪫 High Score: %d\n", high_score);
    printf("🔁 Press any key to play again, 'q' to quit.\n");

    char ch = getchar();
    if (ch != 'q' && ch != 'Q') {
        play_level(1);
    }
}

int main() {
    srand(time(NULL));
    clear_screen();
    printf("\n\n");
    printf("  ██████╗  █████╗ ████████╗████████╗██╗     ███████╗███████╗\n");
    printf("  ██╔══██╗██╔══██╗╚══██╔══╝╚══██╔══╝██║     ██╔════╝██╔════╝\n");
    printf("  ██████╔╝███████║   ██║      ██║   ██║     █████╗  ███████╗\n");
    printf("  ██╔══██╗██╔══██║   ██║      ██║   ██║     ██╔══╝  ╚════██║\n");
    printf("  ██████╔╝██║  ██║   ██║      ██║   ███████╗███████╗███████║\n");
    printf("  ╚═════╝ ╚═╝  ╚═╝   ╚═╝      ╚═╝   ╚══════╝╚══════╝╚══════╝\n\n");
    printf("            💩 Tappy Turd: Flappy Poop Edition 🚽\n");
    printf("                   Press SPACE to jump!\n");
    printf("                   Press any key to start...\n");

    getchar();
    init_term();

    printf("\a");
    play_level(1);

    reset_term();
    return 0;
}
