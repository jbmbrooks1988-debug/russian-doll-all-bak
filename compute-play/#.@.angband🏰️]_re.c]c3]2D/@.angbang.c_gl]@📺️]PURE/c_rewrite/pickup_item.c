#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 100

typedef struct {
    char symbol;
    int x;
    int y;
} ItemPos;

int main(int argc, char *argv[]) {
    FILE *log_file = fopen("c_rewrite/pickup_item.log", "w");

    if (argc != 3) {
        fprintf(log_file, "Usage: %s <player_x> <player_y>\n", argv[0]);
        fclose(log_file);
        return 1;
    }

    int player_x = atoi(argv[1]);
    int player_y = atoi(argv[2]);
    fprintf(log_file, "Player at: %d, %d\n", player_x, player_y);

    ItemPos items[MAX_ITEMS];
    int num_items = 0;
    int item_to_pickup_index = -1;

    FILE *item_pos_file = fopen("c_rewrite/item_pos.txt", "r");
    if (item_pos_file == NULL) {
        fprintf(log_file, "item_pos.txt not found.\n");
        fclose(log_file);
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), item_pos_file)) {
        sscanf(line, "%c,%d,%d", &items[num_items].symbol, &items[num_items].x, &items[num_items].y);
        fprintf(log_file, "Item found in item_pos.txt: %c at %d,%d\n", items[num_items].symbol, items[num_items].x, items[num_items].y);
        if (items[num_items].x == player_x && items[num_items].y == player_y) {
            item_to_pickup_index = num_items;
            fprintf(log_file, "Item to pick up found at index %d\n", item_to_pickup_index);
        }
        num_items++;
    }
    fclose(item_pos_file);

    if (item_to_pickup_index != -1) {
        fprintf(log_file, "Item to pick up is at index %d\n", item_to_pickup_index);
        // Find item name from items.txt
        char item_templates[10][100];
        int num_item_templates = 0;
        FILE *item_data_file = fopen("c_rewrite/items.txt", "r");
        if (item_data_file == NULL) {
            perror("Error opening item data file");
            return 1;
        }
        while (fgets(item_templates[num_item_templates], sizeof(item_templates[0]), item_data_file)) {
            char symbol;
            sscanf(item_templates[num_item_templates], "%*[^,],%c,", &symbol);
            if (symbol == items[item_to_pickup_index].symbol) {
                char *item_name = strtok(item_templates[num_item_templates], ",");
                // Add to inventory
                FILE *inventory_file = fopen("c_rewrite/inventory.txt", "a");
                if (inventory_file != NULL) {
                    fprintf(inventory_file, "%s\n", item_name);
                    fclose(inventory_file);
                }
                break;
            }
            num_item_templates++;
        }
        fclose(item_data_file);

        // Remove item from item_pos.txt
        item_pos_file = fopen("c_rewrite/item_pos.txt", "w");
        for (int i = 0; i < num_items; i++) {
            if (i != item_to_pickup_index) {
                fprintf(item_pos_file, "%c,%d,%d\n", items[i].symbol, items[i].x, items[i].y);
            }
        }
        fclose(item_pos_file);
    } else {
        fprintf(log_file, "No item to pick up at player's location.\n");
    }

    fclose(log_file);
    return 0;
}
