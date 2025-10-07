#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For usleep

#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define MAX_MONSTERS 100
#define MAX_ITEMS 100
#define MAX_INVENTORY 10

char map[MAP_HEIGHT][MAP_WIDTH];
int player_x, player_y;
int player_hp, player_mp, player_gold;
char inventory[MAX_INVENTORY][50];
int num_inventory_items = 0;

typedef struct {
    int id;
    char name[20];
    char symbol;
    int x;
    int y;
    int hp;
    int attack;
} Monster;

Monster monsters[MAX_MONSTERS];
int num_monsters = 0;

typedef struct {
    char symbol;
    int x;
    int y;
} Item;

Item items[MAX_ITEMS];
int num_items = 0;

void load_map() {
    FILE *file = fopen("c_rewrite/map.txt", "r");
    if (file == NULL) {
        perror("Error opening map file");
        exit(1);
    }
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int c = fgetc(file);
            if (c == EOF) break;
            if (c == '\n') {
                x--;
                continue;
            }
            map[y][x] = c;
        }
        int c = fgetc(file);
        if (c != '\n' && c != EOF) {
            ungetc(c, file);
        }
    }
    fclose(file);
}

void load_player_pos() {
    FILE *file = fopen("c_rewrite/player_pos.txt", "r");
    if (file == NULL) {
        perror("Error opening player position file");
        exit(1);
    }
    fscanf(file, "%d,%d", &player_x, &player_y);
    fclose(file);
}

void load_player_stats() {
    FILE *file = fopen("c_rewrite/player_stats.txt", "r");
    if (file == NULL) {
        perror("Error opening player stats file");
        exit(1);
    }
    fscanf(file, "HP:%d,MP:%d,Gold:%d", &player_hp, &player_mp, &player_gold);
    fclose(file);
}

void load_inventory() {
    FILE *file = fopen("c_rewrite/inventory.txt", "r");
    if (file == NULL) {
        num_inventory_items = 0;
        return;
    }
    num_inventory_items = 0;
    while (fgets(inventory[num_inventory_items], sizeof(inventory[0]), file)) {
        inventory[num_inventory_items][strcspn(inventory[num_inventory_items], "\n")] = 0;
        num_inventory_items++;
    }
    fclose(file);
}

void load_monsters() {
    FILE *file = fopen("c_rewrite/monsters.txt", "r");
    if (file == NULL) {
        num_monsters = 0;
        return;
    }
    num_monsters = 0;
    while (fscanf(file, "%d,%[^,],%c,%d,%d,%d,%d\n", &monsters[num_monsters].id, monsters[num_monsters].name, &monsters[num_monsters].symbol, &monsters[num_monsters].x, &monsters[num_monsters].y, &monsters[num_monsters].hp, &monsters[num_monsters].attack) != EOF) {
        num_monsters++;
    }
    fclose(file);
}

void load_items() {
    FILE *file = fopen("c_rewrite/item_pos.txt", "r");
    if (file == NULL) {
        num_items = 0;
        return;
    }
    num_items = 0;
    while (fscanf(file, "%c,%d,%d\n", &items[num_items].symbol, &items[num_items].x, &items[num_items].y) != EOF) {
        num_items++;
    }
    fclose(file);
}

void render_game_state_cli() {
    system("clear"); // Clear the console

    // Render map, player, monsters, items
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char char_to_print = map[y][x];
            // Check for player
            if (x == player_x && y == player_y) {
                char_to_print = '@';
            } else {
                // Check for monsters
                for (int i = 0; i < num_monsters; i++) {
                    if (x == monsters[i].x && y == monsters[i].y) {
                        char_to_print = monsters[i].symbol;
                        break;
                    }
                }
                // Check for items (only if no monster is there)s
                if (char_to_print == map[y][x]) { // If no monster was found
                    for (int i = 0; i < num_items; i++) {
                        if (x == items[i].x && y == items[i].y) {
                            char_to_print = items[i].symbol;
                            break;
                        }
                    }
                }
            }
            printf("%c", char_to_print);
        }
        printf("\n");
    }

    // Render HUD
    printf("HP: %d | MP: %d | Gold: %d\n", player_hp, player_mp, player_gold);
    printf("Inventory:\n");
    for (int i = 0; i < num_inventory_items; i++) {
        printf("- %s\n", inventory[i]);
    }
    printf("Enter command (up, down, left, right, pickup, use <item_name>, exit): ");
    fflush(stdout); // Ensure prompt is displayed immediately
}

void process_commands_from_file() {
    FILE *file = fopen("c_rewrite/commands.txt", "r");
    if (file != NULL) {
        char command[256];
        if (fgets(command, sizeof(command), file)) {
            command[strcspn(command, "\n")] = 0;

            if (strcmp(command, "up") == 0 || strcmp(command, "down") == 0 || strcmp(command, "left") == 0 || strcmp(command, "right") == 0) {
                char system_command[256];
                snprintf(system_command, sizeof(system_command), "./c_rewrite/player_control %s", command);
                system(system_command);
                system("./c_rewrite/monster_ai"); // Monsters move after player
            } else if (strcmp(command, "pickup") == 0) {
                char system_command[256];
                snprintf(system_command, sizeof(system_command), "./c_rewrite/pickup_item %d %d", player_x, player_y);
                system(system_command);
            } else if (strncmp(command, "use", 3) == 0) {
                char item_name[50];
                sscanf(command, "use %s", item_name);
                char system_command[256];
                snprintf(system_command, sizeof(system_command), "./c_rewrite/use_item %s", item_name);
                system(system_command);
            } else if (strcmp(command, "exit") == 0) {
                exit(0);
            }
        }
        fclose(file);
        remove("c_rewrite/commands.txt"); // Delete the command file after processing
    }
}

void get_cli_input() {
    char input_buffer[256];
    if (fgets(input_buffer, sizeof(input_buffer), stdin)) {
        input_buffer[strcspn(input_buffer, "\n")] = 0; // Remove newline

        FILE *file = fopen("c_rewrite/commands.txt", "w");
        if (file != NULL) {
            fprintf(file, "%s\n", input_buffer);
            fclose(file);
        }
    }
}

int main() {
    // Initial game setup
    system("./c_rewrite/generate_map");
    system("./c_rewrite/spawn_monsters");
    system("./c_rewrite/spawn_items");

    // Game loop
    while (1) {
        process_commands_from_file(); // Process commands from file (could be from GL or CLI)
        
        // Load all game data
        load_map();
        load_player_pos();
        load_player_stats();
        load_inventory();
        load_monsters();
        load_items();

        render_game_state_cli(); // Render the current state

        get_cli_input(); // Get input from CLI and write to commands.txt

        usleep(100000); // Small delay to prevent busy-waiting and allow GL window to update
    }

    return 0;
}