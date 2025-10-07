// util.c
#include <stdio.h>

void save_high_score(int score) {
    FILE* f = fopen("highscore.txt", "w");
    if (f) {
        fprintf(f, "%d", score);
        fclose(f);
    }
}
