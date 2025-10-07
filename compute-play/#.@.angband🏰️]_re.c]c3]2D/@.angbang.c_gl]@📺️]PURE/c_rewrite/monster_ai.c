#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define MAX_MONSTERS 100

typedef struct {
    int id;
    char name[20];
    char symbol;
    int x;
    int y;
    int hp;
    int attack;
} Monster;

int main() {
    char map[MAP_HEIGHT][MAP_WIDTH + 2];
    Monster monsters[MAX_MONSTERS];
    int num_monsters = 0;

    // Read map
    FILE *map_file = fopen("c_rewrite/map.txt", "r");
    if (map_file == NULL) {
        perror("Error opening map file");
        return 1;
    }
    for (int i = 0; i < MAP_HEIGHT; i++) {
        fgets(map[i], sizeof(map[i]), map_file);
    }
    fclose(map_file);

    // Read monster positions
    FILE *monsters_file = fopen("c_rewrite/monsters.txt", "r");
    if (monsters_file == NULL) {
        return 0; // No monsters to move
    }
    while (fscanf(monsters_file, "%d,%[^,],%c,%d,%d,%d,%d\n", &monsters[num_monsters].id, monsters[num_monsters].name, &monsters[num_monsters].symbol, &monsters[num_monsters].x, &monsters[num_monsters].y, &monsters[num_monsters].hp, &monsters[num_monsters].attack) != EOF) {
        num_monsters++;
    }
    fclose(monsters_file);

    // AI for each monster
    srand(time(NULL));
    for (int i = 0; i < num_monsters; i++) {
        int move = rand() % 5; // 0: stay, 1: up, 2: down, 3: left, 4: right
        int new_x = monsters[i].x;
        int new_y = monsters[i].y;

        if (move == 1) new_y--;
        else if (move == 2) new_y++;
        else if (move == 3) new_x--;
        else if (move == 4) new_x++;

        if (new_x >= 0 && new_x < MAP_WIDTH && new_y >= 0 && new_y < MAP_HEIGHT && map[new_y][new_x] != '#') {
            monsters[i].x = new_x;
            monsters[i].y = new_y;
        }
    }

    // Write new monster positions
    monsters_file = fopen("c_rewrite/monsters.txt", "w");
    if (monsters_file == NULL) {
        perror("Error opening monsters file for writing");
        return 1;
    }
    for (int i = 0; i < num_monsters; i++) {
        fprintf(monsters_file, "%d,%s,%c,%d,%d,%d,%d\n", monsters[i].id, monsters[i].name, monsters[i].symbol, monsters[i].x, monsters[i].y, monsters[i].hp, monsters[i].attack);
    }
    fclose(monsters_file);

    return 0;
}