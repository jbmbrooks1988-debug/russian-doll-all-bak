#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main() {
    char game_name[50];
    char static_path[150];
    char map[5][6] = {".P...", ".....", ".E...", ".....", "....."};
    char players[10][200] = {"Player1, base_health=100, base_strength=10, multiplier=1.2, xp_factor=100"};
    int player_count = 1;
    char enemies[10][200] = {"Goblin, level=2, base_health=50, base_strength=5, multiplier=1.1, xp_reward=50"};
    int enemy_count = 1;
    char items[10][200] = {"Potion, type=potion, effect=heal 20", "Sword, type=weapon, effect=strength +5"};
    int item_count = 2;
    char events[10][200];
    int event_count = 1;
    char battles[10][200] = {"Battle1, enemies=Goblin, rewards=Potion"};
    int battle_count = 1;

    strcpy(events[0], "2 2, type=battle, param=Battle1");

    printf("Enter game name: ");
    scanf("%s", game_name);
    sprintf(static_path, "%s_static.txt", game_name); // e.g., "icemen_static.txt"

    int choice;
    do {
        printf("\nEditor Menu:\n");
        printf("1. Edit Map (using map_editor module)\n2. Manage Players\n3. Manage Enemies\n4. Manage Items\n");
        printf("5. Manage Events\n6. Manage Battles\n7. Save and Exit\n");
        scanf("%d", &choice);

        if (choice == 1) { // Edit Map - now calls the map_editor module
            char command[200];
            // Call the map editor module as a separate executable
            sprintf(command, "./+x/map_editor.+x %s", game_name);
            printf("Launching map editor...\n");
            system(command);
            
            // Reload the map from the static file after editing
            FILE *fp = fopen(static_path, "r");
            if (fp) {
                char line[200];
                int row_num = 0;
                while (fgets(line, sizeof(line), fp)) {
                    if (strstr(line, "Row")) {
                        int row_index;
                        char temp[6];
                        if (sscanf(line, "Row%d: %5s", &row_index, temp) == 2 && row_index >= 1 && row_index <= 5) {
                            strncpy(map[row_index - 1], temp, 5);
                            map[row_index - 1][5] = '\0';
                            row_num++;
                        }
                    }
                }
                fclose(fp);
                printf("Reloaded map from %s\n", static_path);
            }
        } else if (choice == 2) { // Manage Players
            printf("Players:\n");
            for (int i = 0; i < player_count; i++) printf("%d. %s\n", i + 1, players[i]);
            printf("%d. Add new player\n", player_count + 1);
            scanf("%d", &choice);
            if (choice >= 1 && choice <= player_count) { // Edit existing player
                int index = choice - 1;
                char name[50];
                int bh, bs, xf;
                double m;
                printf("Editing %s\n", players[index]);
                printf("Enter new name base_health base_strength multiplier xp_factor (e.g., Hero 150 15 1.3 200): ");
                if (scanf("%s %d %d %lf %d", name, &bh, &bs, &m, &xf) != 5) {
                    printf("Invalid input. All 5 values required.\n");
                    while (getchar() != '\n');
                    continue;
                }
                sprintf(players[index], "%s, base_health=%d, base_strength=%d, multiplier=%.1f, xp_factor=%d",
                        name, bh, bs, m, xf);
                printf("Player updated.\n");
            } else if (choice == player_count + 1) { // Add new player
                char name[50];
                int bh, bs, xf;
                double m;
                printf("Enter name base_health base_strength multiplier xp_factor (e.g., Hero 100 10 1.2 100): ");
                if (scanf("%s %d %d %lf %d", name, &bh, &bs, &m, &xf) != 5) {
                    printf("Invalid input. All 5 values required.\n");
                    while (getchar() != '\n');
                    continue;
                }
                sprintf(players[player_count], "%s, base_health=%d, base_strength=%d, multiplier=%.1f, xp_factor=%d",
                        name, bh, bs, m, xf);
                player_count++;
            }
        } else if (choice == 3) { // Manage Enemies
            printf("Enemies:\n");
            for (int i = 0; i < enemy_count; i++) printf("%d. %s\n", i + 1, enemies[i]);
            printf("%d. Add new enemy\n", enemy_count + 1);
            scanf("%d", &choice);
            if (choice == enemy_count + 1) {
                char name[50];
                int l, bh, bs, xr;
                double m;
                printf("Enter name level base_health base_strength multiplier xp_reward (e.g., Orc 3 60 8 1.1 75): ");
                if (scanf("%s %d %d %d %lf %d", name, &l, &bh, &bs, &m, &xr) != 6) {
                    printf("Invalid input. All 6 values required.\n");
                    while (getchar() != '\n');
                    continue;
                }
                sprintf(enemies[enemy_count], "%s, level=%d, base_health=%d, base_strength=%d, multiplier=%.1f, xp_reward=%d",
                        name, l, bh, bs, m, xr);
                enemy_count++;
            }
        } else if (choice == 4) { // Manage Items
            printf("Items:\n");
            for (int i = 0; i < item_count; i++) printf("%d. %s\n", i + 1, items[i]);
            printf("%d. Add new item\n", item_count + 1);
            scanf("%d", &choice);
            if (choice == item_count + 1) {
                char input[200], name[50], type[20], effect[20];
                int val = 0;
                printf("Enter name type effect value (e.g., Crown potion heal 20, or Sword weapon equip 0): ");
                scanf(" %199[^\n]s", input);
                int fields = sscanf(input, "%s %s %s %d", name, type, effect, &val);
                if (fields < 3 || (strcmp(type, "potion") != 0 && strcmp(type, "weapon") != 0)) {
                    printf("Invalid input. Need name, type (potion/weapon), effect, and optional value (number).\n");
                    continue;
                }
                if (fields == 3) val = 0;
                sprintf(items[item_count], "%s, type=%s, effect=%s %d", name, type, effect, val);
                item_count++;
            }
        } else if (choice == 5) { // Manage Events
            printf("Events:\n");
            for (int i = 0; i < event_count; i++) printf("%d. %s\n", i + 1, events[i]);
            printf("%d. Add new event\n", event_count + 1);
            scanf("%d", &choice);
            if (choice == event_count + 1) {
                int x, y;
                char type[20];
                printf("Enter x y (e.g., 2 3): ");
                if (scanf("%d %d", &x, &y) != 2 || x < 0 || x >= 5 || y < 0 || y >= 5) {
                    printf("Invalid coordinates. Must be 0-4.\n");
                    while (getchar() != '\n');
                    continue;
                }
                printf("Event type (battle/teleport/text/choice): ");
                scanf("%s", type);
                if (strcmp(type, "battle") == 0) {
                    char param[50];
                    printf("Enter battle param (e.g., Battle1): ");
                    scanf("%s", param);
                    sprintf(events[event_count], "%d %d, type=battle, param=%s", x, y, param);
                    event_count++;
                } else if (strcmp(type, "teleport") == 0) {
                    int tx, ty;
                    printf("Enter target x y (e.g., 1 1): ");
                    if (scanf("%d %d", &tx, &ty) != 2 || tx < 0 || tx >= 5 || ty < 0 || ty >= 5) {
                        printf("Invalid target coordinates. Must be 0-4.\n");
                        while (getchar() != '\n');
                        continue;
                    }
                    sprintf(events[event_count], "%d %d, type=teleport, param=%d %d", x, y, tx, ty);
                    event_count++;
                } else if (strcmp(type, "text") == 0) {
                    char text[100];
                    printf("Enter message (use _ for spaces, e.g., Hi_there): ");
                    scanf(" %99[^\n]s", text);
                    sprintf(events[event_count], "%d %d, type=text, param=%s", x, y, text);
                    event_count++;
                } else if (strcmp(type, "choice") == 0) {
                    char opts[100];
                    printf("Enter 4 options (comma-separated, use _ for spaces, e.g., Yes,No,Maybe,Later): ");
                    scanf(" %99[^\n]s", opts);
                    if (sscanf(opts, "%*[^,],%*[^,],%*[^,],%*s") != 4) {
                        printf("Invalid input. Please provide exactly 4 options separated by commas.\n");
                        continue;
                    }
                    sprintf(events[event_count], "%d %d, type=choice, param=%s", x, y, opts);
                    event_count++;
                } else {
                    printf("Invalid event type. Use battle, teleport, text, or choice.\n");
                }
            }
        } else if (choice == 6) { // Manage Battles
            printf("Battles:\n");
            for (int i = 0; i < battle_count; i++) printf("%d. %s\n", i + 1, battles[i]);
            printf("%d. Add new battle\n", battle_count + 1);
            scanf("%d", &choice);
            if (choice == battle_count + 1) {
                char name[50], enemies_list[50], rewards[50];
                printf("Enter name enemies rewards (e.g., Fight1 Goblin Potion): ");
                scanf("%s %s %s", name, enemies_list, rewards);
                sprintf(battles[battle_count], "%s, enemies=%s, rewards=%s", name, enemies_list, rewards);
                battle_count++;
            }
        }
    } while (choice != 7);

    FILE *fp = fopen(static_path, "w");
    if (!fp) {
        printf("Error: Could not write to %s. Check permissions in current directory.\n", static_path);
        return 1;
    }
    printf("Saving to %s\n", static_path);
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
