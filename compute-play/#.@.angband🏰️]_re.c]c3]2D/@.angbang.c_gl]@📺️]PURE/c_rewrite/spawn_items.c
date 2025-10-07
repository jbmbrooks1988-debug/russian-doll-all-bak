#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define MAX_ITEMS 3

int main() {
    char map[MAP_HEIGHT][MAP_WIDTH + 2];
    char item_templates[10][100];
    int num_item_templates = 0;

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

    // Read item data
    FILE *item_data_file = fopen("c_rewrite/items.txt", "r");
    if (item_data_file == NULL) {
        perror("Error opening item data file");
        return 1;
    }
    while (fgets(item_templates[num_item_templates], sizeof(item_templates[0]), item_data_file)) {
        num_item_templates++;
    }
    fclose(item_data_file);

    // Spawn items
    FILE *item_pos_file = fopen("c_rewrite/item_pos.txt", "w");
    if (item_pos_file == NULL) {
        perror("Error opening item position file");
        return 1;
    }

    srand(time(NULL));
    for (int i = 0; i < MAX_ITEMS; i++) {
        int x, y;
        do {
            x = rand() % MAP_WIDTH;
            y = rand() % MAP_HEIGHT;
        } while (map[y][x] != '.');

        int item_template_index = rand() % num_item_templates;
        char symbol;
        sscanf(item_templates[item_template_index], "%*[^,],%c,", &symbol);
        
        fprintf(item_pos_file, "%c,%d,%d\n", symbol, x, y);
    }

    fclose(item_pos_file);

    return 0;
}
