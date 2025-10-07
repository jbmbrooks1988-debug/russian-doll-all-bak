#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        printf("Usage: %s <monster_id>\n", argv[0]);
        return 1;
    }

    int target_monster_id = atoi(argv[1]);
    Monster monsters[MAX_MONSTERS];
    int num_monsters = 0;
    int target_monster_index = -1;

    // Read monsters
    FILE *monsters_file = fopen("c_rewrite/monsters.txt", "r");
    if (monsters_file == NULL) {
        return 0; // No monsters
    }
    char line[256];
    while (fgets(line, sizeof(line), monsters_file)) {
        sscanf(line, "%d,%[^,],%c,%d,%d,%d,%d", &monsters[num_monsters].id, monsters[num_monsters].name, &monsters[num_monsters].symbol, &monsters[num_monsters].x, &monsters[num_monsters].y, &monsters[num_monsters].hp, &monsters[num_monsters].attack);
        if (monsters[num_monsters].id == target_monster_id) {
            target_monster_index = num_monsters;
        }
        num_monsters++;
    }
    fclose(monsters_file);

    if (target_monster_index == -1) {
        return 0; // Target monster not found
    }

    // Read player stats
    int player_hp, player_mp, player_gold;
    FILE *player_stats_file = fopen("c_rewrite/player_stats.txt", "r");
    if (player_stats_file == NULL) {
        perror("Error opening player stats file");
        return 1;
    }
    fscanf(player_stats_file, "HP:%d,MP:%d,Gold:%d", &player_hp, &player_mp, &player_gold);
    fclose(player_stats_file);

    // Combat calculation
    int player_attack = 1; // Let's assume player attack is 1 for now
    monsters[target_monster_index].hp -= player_attack;
    player_hp -= monsters[target_monster_index].attack;

    if (monsters[target_monster_index].hp <= 0) {
        player_gold += 10;
    }

    // Write updated monster data
    monsters_file = fopen("c_rewrite/monsters.txt", "w");
    for (int i = 0; i < num_monsters; i++) {
        if (i == target_monster_index) {
            if (monsters[i].hp > 0) {
                fprintf(monsters_file, "%d,%s,%c,%d,%d,%d,%d\n", monsters[i].id, monsters[i].name, monsters[i].symbol, monsters[i].x, monsters[i].y, monsters[i].hp, monsters[i].attack);
            }
        } else {
            fprintf(monsters_file, "%d,%s,%c,%d,%d,%d,%d\n", monsters[i].id, monsters[i].name, monsters[i].symbol, monsters[i].x, monsters[i].y, monsters[i].hp, monsters[i].attack);
        }
    }
    fclose(monsters_file);

    // Write updated player stats
    player_stats_file = fopen("c_rewrite/player_stats.txt", "w");
    fprintf(player_stats_file, "HP:%d,MP:%d,Gold:%d\n", player_hp, player_mp, player_gold);
    fclose(player_stats_file);

    return 0;
}