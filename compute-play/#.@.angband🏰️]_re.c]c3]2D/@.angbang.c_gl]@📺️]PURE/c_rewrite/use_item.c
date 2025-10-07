#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INVENTORY 100

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <item_name>\n", argv[0]);
        return 1;
    }

    char *item_to_use = argv[1];
    char inventory[MAX_INVENTORY][50];
    int num_inventory_items = 0;
    int item_to_use_index = -1;

    // Read inventory
    FILE *inventory_file = fopen("c_rewrite/inventory.txt", "r");
    if (inventory_file == NULL) {
        return 0; // Empty inventory
    }
    while (fgets(inventory[num_inventory_items], sizeof(inventory[0]), inventory_file)) {
        inventory[num_inventory_items][strcspn(inventory[num_inventory_items], "\n")] = 0;
        if (strcmp(inventory[num_inventory_items], item_to_use) == 0) {
            item_to_use_index = num_inventory_items;
        }
        num_inventory_items++;
    }
    fclose(inventory_file);

    if (item_to_use_index != -1) {
        // Read player stats
        int player_hp, player_mp, player_gold;
        FILE *player_stats_file = fopen("c_rewrite/player_stats.txt", "r");
        if (player_stats_file == NULL) {
            perror("Error opening player stats file");
            return 1;
        }
        fscanf(player_stats_file, "HP:%d,MP:%d,Gold:%d", &player_hp, &player_mp, &player_gold);
        fclose(player_stats_file);

        // Use item
        if (strcmp(item_to_use, "healing_potion") == 0) {
            player_hp += 10;
        }

        // Write updated player stats
        player_stats_file = fopen("c_rewrite/player_stats.txt", "w");
        fprintf(player_stats_file, "HP:%d,MP:%d,Gold:%d\n", player_hp, player_mp, player_gold);
        fclose(player_stats_file);

        // Remove item from inventory
        inventory_file = fopen("c_rewrite/inventory.txt", "w");
        for (int i = 0; i < num_inventory_items; i++) {
            if (i != item_to_use_index) {
                fprintf(inventory_file, "%s\n", inventory[i]);
            }
        }
        fclose(inventory_file);
    }

    return 0;
}
