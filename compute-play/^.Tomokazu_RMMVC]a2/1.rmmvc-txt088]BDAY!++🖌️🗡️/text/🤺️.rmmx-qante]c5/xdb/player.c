#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int calculate_attribute(int base, double multiplier, int level) {
    return (int)(base * pow(multiplier, level - 1));
}

int main() {
    char game_name[50];
    char folder_path[100];
    char static_path[150];
    char dynamic_path[150];
    char map[5][6];
    char player[200];
    char enemies[10][200];
    int enemy_count = 0;
    char items[10][200];
    int item_count = 0;
    char events[10][200];
    int event_count = 0;
    char battles[10][200];
    int battle_count = 0;
    char inventory[10][50];
    int inv_count = 0;
    int pos_x = 0, pos_y = 1; // Starting at 'P' in map
    int level = 1, xp = 0, current_health;
    int choice; // Declared here to be available throughout main

    printf("Enter game name: ");
    scanf("%s", game_name);
    sprintf(folder_path, "%s", game_name);
    sprintf(static_path, "%s/static.txt", folder_path);
    sprintf(dynamic_path, "%s/dynamic.txt", folder_path);

    // Load static.txt
    FILE *fp = fopen(static_path, "r");
    if (!fp) {
        printf("Cannot load game.\n");
        return 1;
    }
    char line[200];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Row")) sscanf(line, "Row%d: %s", &enemy_count, map[enemy_count - 1]); // Reuse enemy_count as temp
        else if (strstr(line, "[Players]")) fgets(player, sizeof(player), fp);
        else if (strstr(line, "[Enemies]")) {
            while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                strcpy(enemies[enemy_count++], strtok(line, "\n"));
            }
        } else if (strstr(line, "[Items]")) {
            while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                strcpy(items[item_count++], strtok(line, "\n"));
            }
        } else if (strstr(line, "[Events]")) {
            while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                strcpy(events[event_count++], strtok(line, "\n"));
            }
        } else if (strstr(line, "[Battles]")) {
            while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                strcpy(battles[battle_count++], strtok(line, "\n"));
            }
        }
    }
    fclose(fp);

    // Load or initialize dynamic.txt
    fp = fopen(dynamic_path, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "pos_x")) sscanf(line, "pos_x=%d", &pos_x);
            if (strstr(line, "pos_y")) sscanf(line, "pos_y=%d", &pos_y);
            if (strstr(line, "level")) sscanf(line, "level=%d", &level);
            if (strstr(line, "xp")) sscanf(line, "xp=%d", &xp);
            if (strstr(line, "health")) sscanf(line, "health=%d", &current_health);
            if (strstr(line, "inventory")) {
                while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                    strcpy(inventory[inv_count++], strtok(line, "\n"));
                }
            }
        }
        fclose(fp);
    } else {
        int base_health;
        double multiplier;
        sscanf(player, "%*[^,], base_health=%d, %*[^,], multiplier=%lf", &base_health, &multiplier);
        current_health = calculate_attribute(base_health, multiplier, level);
    }

    char command[50];
    do {
        printf("\nMap:\n");
        for (int i = 0; i < 5; i++) printf("%s\n", map[i]);
        printf("Health: %d, Level: %d, XP: %d\n", current_health, level, xp);
        printf("Command (look, move, use, attack, save, exit): ");
        scanf("%s", command);

        if (strcmp(command, "look") == 0) {
            continue; // Map already displayed
        } else if (strcmp(command, "move") == 0) {
            char dir[10];
            scanf("%s", dir);
            int new_x = pos_x, new_y = pos_y;
            if (strcmp(dir, "north") == 0) new_x--;
            else if (strcmp(dir, "south") == 0) new_x++;
            else if (strcmp(dir, "east") == 0) new_y++;
            else if (strcmp(dir, "west") == 0) new_y--;
            if (new_x >= 0 && new_x < 5 && new_y >= 0 && new_y < 5) {
                pos_x = new_x;
                pos_y = new_y;
                for (int i = 0; i < event_count; i++) {
                    int x, y;
                    char type[20], param[50];
                    sscanf(events[i], "%d %d, type=%s param=%s", &x, &y, type, param);
                    if (x == pos_x && y == pos_y && strcmp(type, "battle") == 0) {
                        printf("Battle started: %s\n", param);
                        // Simple battle simulation
                        int e_health, e_strength, e_level;
                        double e_multiplier;
                        for (int j = 0; j < enemy_count; j++) {
                            if (strstr(enemies[j], "Goblin")) {
                                sscanf(enemies[j], "%*[^,], level=%d, base_health=%d, base_strength=%d, multiplier=%lf",
                                       &e_level, &e_health, &e_strength, &e_multiplier);
                                e_health = calculate_attribute(e_health, e_multiplier, e_level);
                                e_strength = calculate_attribute(e_strength, e_multiplier, e_level);
                                while (e_health > 0 && current_health > 0) {
                                    e_health -= calculate_attribute(10, 1.2, level); // Player attack
                                    if (e_health > 0) current_health -= e_strength;
                                }
                                if (current_health > 0) {
                                    int xp_reward;
                                    sscanf(enemies[j], "%*[^,], %*[^,], %*[^,], %*[^,], %*[^,], xp_reward=%d", &xp_reward);
                                    xp += xp_reward;
                                    int next_xp = 100 * (level * level); // xp_factor=100
                                    if (xp >= next_xp) {
                                        level++;
                                        current_health = calculate_attribute(100, 1.2, level);
                                        printf("Level up! Now level %d\n", level);
                                    }
                                } else {
                                    printf("You died.\n");
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        } else if (strcmp(command, "use") == 0) {
            if (inv_count == 0) {
                printf("No items.\n");
                continue;
            }
            printf("Inventory:\n");
            for (int i = 0; i < inv_count; i++) printf("%d. %s\n", i + 1, inventory[i]);
            scanf("%d", &choice); // Now choice is declared
            if (choice > 0 && choice <= inv_count) {
                for (int i = 0; i < item_count; i++) {
                    if (strstr(items[i], inventory[choice - 1]) && strstr(items[i], "heal")) {
                        int heal;
                        sscanf(items[i], "%*[^,], %*[^,], effect=heal %d", &heal);
                        current_health += heal;
                        printf("Healed %d\n", heal);
                        for (int j = choice - 1; j < inv_count - 1; j++) strcpy(inventory[j], inventory[j + 1]);
                        inv_count--;
                        break;
                    }
                }
            }
        } else if (strcmp(command, "attack") == 0) {
            if (map[pos_x][pos_y] == 'E') {
                printf("Attacking enemy at %d,%d\n", pos_x, pos_y);
                // Similar battle logic as above
            }
        } else if (strcmp(command, "save") == 0) {
            fp = fopen(dynamic_path, "w");
            fprintf(fp, "[Player]\npos_x=%d\npos_y=%d\nlevel=%d\nxp=%d\nhealth=%d\n[inventory]\n", 
                    pos_x, pos_y, level, xp, current_health);
            for (int i = 0; i < inv_count; i++) fprintf(fp, "%s\n", inventory[i]);
            fclose(fp);
            printf("Game saved.\n");
        }
    } while (strcmp(command, "exit") != 0);

    return 0;
}
