#include <stdio.h>
#include <stdlib.h>

#define WIDTH 80
#define HEIGHT 24

int main() {
    char map[HEIGHT][WIDTH];
    FILE *file = fopen("c_rewrite/map.txt", "w");

    if (file == NULL) {
        printf("Error opening file!\n");
        return 1;
    }

    // Create a simple room
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1) {
                map[y][x] = '#'; // Wall
            } else {
                map[y][x] = '.'; // Floor
            }
        }
    }

    // Write map to file
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            fprintf(file, "%c", map[y][x]);
        }
        fprintf(file, "\n");
    }

    fclose(file);
    return 0;
}
