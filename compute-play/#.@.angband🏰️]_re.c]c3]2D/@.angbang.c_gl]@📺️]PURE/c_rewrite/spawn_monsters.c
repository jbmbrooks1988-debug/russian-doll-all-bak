#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define MAX_MONSTERS 5

int main() {
    char map[MAP_HEIGHT][MAP_WIDTH + 2];
    char monster_templates[10][100];
    int num_monster_templates = 0;

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

    // Read monster data
    FILE *monster_data_file = fopen("c_rewrite/monster_data.txt", "r");
    if (monster_data_file == NULL) {
        perror("Error opening monster data file");
        return 1;
    }
    while (fgets(monster_templates[num_monster_templates], sizeof(monster_templates[0]), monster_data_file)) {
        num_monster_templates++;
    }
    fclose(monster_data_file);

    // Spawn monsters
    FILE *monsters_file = fopen("c_rewrite/monsters.txt", "w");
    if (monsters_file == NULL) {
        perror("Error opening monsters file");
        return 1;
    }

    srand(time(NULL));
    for (int i = 0; i < MAX_MONSTERS; i++) {
        int x, y;
        do {
            x = rand() % MAP_WIDTH;
            y = rand() % MAP_HEIGHT;
        } while (map[y][x] != '.');

        int monster_template_index = rand() % num_monster_templates;
        
        char name[20], symbol;
        int hp, attack;
        sscanf(monster_templates[monster_template_index], "%[^,],%c,%d,%d", name, &symbol, &hp, &attack);

        fprintf(monsters_file, "%d,%s,%c,%d,%d,%d,%d\n", i, name, symbol, x, y, hp, attack);
    }

    fclose(monsters_file);

    return 0;
}
