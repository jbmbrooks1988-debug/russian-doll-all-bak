#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main() {
    char game_name[50];
    char folder_path[100];
    char static_path[150];
    char map[5][6] = {".P...", ".....", ".E...", ".....", "....."}; // 5x5 map, null-terminated rows
    char players[10][200] = {"Player1, base_health=100, base_strength=10, multiplier=1.2, xp_factor=100"};
    int player_count = 1;
    char enemies[10][200] = {"Goblin, level=2, base_health=50, base_strength=5, multiplier=1.1, xp_reward=50"};
    int enemy_count = 1;
    char items[10][200] = {"Potion, type=potion, effect=heal 20", "Sword, type=weapon, effect=strength +5"};
    int item_count = 2;
    char events[10][200] = {"2 2, type=battle, param=Battle1"};
    int event_count = 1;
    char battles[10][200] = {"Battle1, enemies=Goblin, rewards=Potion"};
    int battle_count = 1;

    printf("Enter game name: ");
    scanf("%s", game_name);
    sprintf(folder_path, "%s", game_name);
    sprintf(static_path, "%s/static.txt", folder_path);
    system("mkdir %s"); // Create folder (Unix-like, adjust for Windows if needed)

    int choice;
    do {
        printf("\nEditor Menu:\n");
        printf("1. Edit Map\n2. Manage Players\n3. Manage Enemies\n4. Manage Items\n");
        printf("5. Manage Events\n6. Manage Battles\n7. Save and Exit\n");
        scanf("%d", &choice);

        if (choice == 1) { // Edit Map
            int row, col;
            printf("Current Map:\n");
            for (int i = 0; i < 5; i++) printf("%s\n", map[i]);
            printf("Enter row (0-4) and col (0-4) to edit: ");
            scanf("%d %d", &row, &col);
            if (row >= 0 && row < 5 && col >= 0 && col < 5) {
                printf("Enter char (e.g., '.' empty, 'P' player, 'E' enemy): ");
                scanf(" %c", &map[row][col]);
            }
        } else if (choice == 2) { // Manage Players
            printf("Players:\n");
            for (int i = 0; i < player_count; i++) printf("%d. %s\n", i + 1, players[i]);
            printf("%d. Add new player\n", player_count + 1);
            scanf("%d", &choice);
            if (choice == player_count + 1) {
                printf("Enter name, base_health, base_strength, multiplier, xp_factor: ");
                scanf("%s %d %d %lf %d", players[player_count], 
                      &player_count, &player_count, &player_count, &player_count); // Temp use player_count as placeholder
                sprintf(players[player_count], "%s, base_health=%d, base_strength=%d, multiplier=%.1f, xp_factor=%d",
                        players[player_count], player_count, player_count, (double)player_count, player_count);
                player_count++;
            }
        } else if (choice == 3) { // Manage Enemies
            printf("Enemies:\n");
            for (int i = 0; i < enemy_count; i++) printf("%d. %s\n", i + 1, enemies[i]);
            printf("%d. Add new enemy\n", enemy_count + 1);
            scanf("%d", &choice);
            if (choice == enemy_count + 1) {
                printf("Enter name, level, base_health, base_strength, multiplier, xp_reward: ");
                scanf("%s %d %d %d %lf %d", enemies[enemy_count], 
                      &enemy_count, &enemy_count, &enemy_count, &enemy_count, &enemy_count);
                sprintf(enemies[enemy_count], "%s, level=%d, base_health=%d, base_strength=%d, multiplier=%.1f, xp_reward=%d",
                        enemies[enemy_count], enemy_count, enemy_count, enemy_count, (double)enemy_count, enemy_count);
                enemy_count++;
            }
        } else if (choice == 4) { // Manage Items
            printf("Items:\n");
            for (int i = 0; i < item_count; i++) printf("%d. %s\n", i + 1, items[i]);
            printf("%d. Add new item\n", item_count + 1);
            scanf("%d", &choice);
            if (choice == item_count + 1) {
                printf("Enter name, type (potion/weapon), effect (e.g., heal 20): ");
                scanf("%s %s %s %d", items[item_count], items[item_count], items[item_count], &item_count);
                sprintf(items[item_count], "%s, type=%s, effect=%s %d", 
                        items[item_count], items[item_count], items[item_count], item_count);
                item_count++;
            }
        } else if (choice == 5) { // Manage Events
            printf("Events:\n");
            for (int i = 0; i < event_count; i++) printf("%d. %s\n", i + 1, events[i]);
            printf("%d. Add new event\n", event_count + 1);
            scanf("%d", &choice);
            if (choice == event_count + 1) {
                printf("Enter x y, type (battle/teleport), param (e.g., Battle1 or 3 4): ");
                scanf("%d %d %s %s", &event_count, &event_count, events[event_count], events[event_count]);
                sprintf(events[event_count], "%d %d, type=%s, param=%s", 
                        event_count, event_count, events[event_count], events[event_count]);
                event_count++;
            }
        } else if (choice == 6) { // Manage Battles
            printf("Battles:\n");
            for (int i = 0; i < battle_count; i++) printf("%d. %s\n", i + 1, battles[i]);
            printf("%d. Add new battle\n", battle_count + 1);
            scanf("%d", &choice);
            if (choice == battle_count + 1) {
                printf("Enter name, enemies (comma-separated), rewards: ");
                scanf("%s %s %s", battles[battle_count], battles[battle_count], battles[battle_count]);
                sprintf(battles[battle_count], "%s, enemies=%s, rewards=%s", 
                        battles[battle_count], battles[battle_count], battles[battle_count]);
                battle_count++;
            }
        }
    } while (choice != 7);

    // Save to static.txt
    FILE *fp = fopen(static_path, "w");
    fprintf(fp, "[Map]\nSize: 5 5\n");
    for (int i = 0; i < 5; i++) fprintf(fp, "Row%d: %s\n", i + 1, map[i]);
    fprintf(fp, "[Players]\n");
    for (int i = 0; i < player_count; i++) fprintf(fp, "%s\n", players[i]);
    fprintf(fp, "[Enemies]\n");
    for (int i = 0; i < enemy_count; i++) fprintf(fp, "%s\n", enemies[i]);
    fprintf(fp, "[Items]\n");
    for (int i = 0; i < item_count; i++) fprintf(fp, "%s\n", items[i]);
    fprintf(fp, "[Events]\n");
    for (int i = 0; i < event_count; i++) fprintf(fp, "%s\n", events[i]);
    fprintf(fp, "[Battles]\n");
    for (int i = 0; i < battle_count; i++) fprintf(fp, "%s\n", battles[i]);
    fclose(fp);

    printf("Game saved to %s\n", static_path);
    return 0;
}
