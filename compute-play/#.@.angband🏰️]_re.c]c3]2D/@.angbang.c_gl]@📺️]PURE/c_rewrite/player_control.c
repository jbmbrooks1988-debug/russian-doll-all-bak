#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <direction>\n", argv[0]);
        return 1;
    }

    char *direction = argv[1];
    int player_x, player_y;
    char map[MAP_HEIGHT][MAP_WIDTH + 2];
    Monster monsters[MAX_MONSTERS];
    int num_monsters = 0;

    // Read player position
    FILE *pos_file = fopen("c_rewrite/player_pos.txt", "r");
    if (pos_file == NULL) {
        perror("Error opening player position file");
        return 1;
    }
    fscanf(pos_file, "%d,%d", &player_x, &player_y);
    fclose(pos_file);

    // Read map data
    FILE *map_file = fopen("c_rewrite/map.txt", "r");
    if (map_file == NULL) {
        perror("Error opening map file");
        return 1;
    }
    for (int i = 0; i < MAP_HEIGHT; i++) {
        fgets(map[i], sizeof(map[i]), map_file);
    }
    fclose(map_file);

    // Read monsters
    FILE *monsters_file = fopen("c_rewrite/monsters.txt", "r");
    if (monsters_file != NULL) {
        while (fscanf(monsters_file, "%d,%[^,],%c,%d,%d,%d,%d\n", &monsters[num_monsters].id, monsters[num_monsters].name, &monsters[num_monsters].symbol, &monsters[num_monsters].x, &monsters[num_monsters].y, &monsters[num_monsters].hp, &monsters[num_monsters].attack) != EOF) {
            num_monsters++;
        }
        fclose(monsters_file);
    }

    // Calculate new position
    int new_x = player_x;
    int new_y = player_y;

    if (strcmp(direction, "up") == 0) new_y--;
    else if (strcmp(direction, "down") == 0) new_y++;
    else if (strcmp(direction, "left") == 0) new_x--;
    else if (strcmp(direction, "right") == 0) new_x++;

    // Check for monster collision
    for (int i = 0; i < num_monsters; i++) {
        if (monsters[i].x == new_x && monsters[i].y == new_y) {
            char command[256];
            snprintf(command, sizeof(command), "./c_rewrite/combat_resolver %d", monsters[i].id);
            system(command);
            return 0; // Don't move, just attack
        }
    }

    // Check for wall collision
    if (new_x >= 0 && new_x < MAP_WIDTH && new_y >= 0 && new_y < MAP_HEIGHT && map[new_y][new_x] != '#') {
        // Update player position
        pos_file = fopen("c_rewrite/player_pos.txt", "w");
        if (pos_file == NULL) {
            perror("Error opening player position file for writing");
            return 1;
        }
        fprintf(pos_file, "%d,%d\n", new_x, new_y);
        fclose(pos_file);
    }

    return 0;
}