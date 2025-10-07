#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

// Array to store emojis
char emojis[100][5];  // Assuming max 100 emojis, each up to 4 chars + null terminator
int emoji_count = 0;

int calculate_attribute(int base, double multiplier, int level) {
    return (int)(base * pow(multiplier, level - 1));
}

// Function to load emojis from emojis.txt
void load_emojis() {
    FILE *fp = fopen("emojis.txt", "r");
    if (!fp) {
        printf("Warning: Could not load emojis.txt, using default emojis\n");
        // Set default emojis
        strcpy(emojis[0], " ");   // Blank emoji
        strcpy(emojis[1], "🍒");
        strcpy(emojis[2], "🍋");
        strcpy(emojis[3], "🍊");
        strcpy(emojis[4], "💎");
        strcpy(emojis[5], "🔔");
        strcpy(emojis[6], "💩");
        strcpy(emojis[7], "🎨");
        strcpy(emojis[8], "🗡️");
        emoji_count = 9;
        return;
    }
    
    char line[10];
    emoji_count = 0;
    while (fgets(line, sizeof(line), fp) && emoji_count < 100) {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        strcpy(emojis[emoji_count], line);
        emoji_count++;
    }
    fclose(fp);
    printf("Loaded %d emojis from emojis.txt\n", emoji_count);
}

// Function to capture arrow key presses
int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

// Function to get arrow key input
int get_arrow_key() {
    int ch = getchar();
    if (ch == 27) { // ESC sequence
        ch = getchar();
        if (ch == 91) { // [ sequence
            switch (getchar()) {
                case 65: return 1; // Up arrow
                case 66: return 2; // Down arrow
                case 67: return 3; // Right arrow
                case 68: return 4; // Left arrow
            }
        }
    }
    return 0; // Not an arrow key
}

// Function to process player movement
void process_movement(int direction, char map[16][17], int *pos_x, int *pos_y, 
                      char events[10][200], int event_count, 
                      char enemies[10][200], int enemy_count,
                      int *current_health, int *level, int *xp) {
    int new_x = *pos_x, new_y = *pos_y;
    
    switch (direction) {
        case 1: new_x--; break; // Up/North
        case 2: new_x++; break; // Down/South
        case 3: new_y++; break; // Right/East
        case 4: new_y--; break; // Left/West
    }
    
    // Check bounds (16x16 map)
    if (new_x >= 0 && new_x < 16 && new_y >= 0 && new_y < 16) {
        *pos_x = new_x;
        *pos_y = new_y;
        
        // Process events at new position
        for (int i = 0; i < event_count; i++) {
            int x, y;
            char type[20], param[100];
            sscanf(events[i], "%d %d, type=%s param=%[^\n]s", &x, &y, type, param);
            if (x == *pos_x && y == *pos_y) {
                if (strcmp(type, "battle") == 0) {
                    printf("\nBattle started: %s\n", param);
                    int e_health, e_strength, e_level;
                    double e_multiplier;
                    for (int j = 0; j < enemy_count; j++) {
                        if (strstr(enemies[j], "Goblin")) {
                            sscanf(enemies[j], "%*[^,], level=%d, base_health=%d, base_strength=%d, multiplier=%lf",
                                   &e_level, &e_health, &e_strength, &e_multiplier);
                            e_health = calculate_attribute(e_health, e_multiplier, e_level);
                            e_strength = calculate_attribute(e_strength, e_multiplier, e_level);
                            while (e_health > 0 && *current_health > 0) {
                                e_health -= calculate_attribute(2, 1.1, *level); // Use player's base_strength
                                if (e_health > 0) *current_health -= e_strength;
                            }
                            if (*current_health > 0) {
                                int xp_reward;
                                sscanf(enemies[j], "%*[^,], %*[^,], %*[^,], %*[^,], %*[^,], xp_reward=%d", &xp_reward);
                                *xp += xp_reward;
                                int next_xp = 33 * (*level * *level); // Use player's xp_factor
                                if (*xp >= next_xp) {
                                    (*level)++;
                                    *current_health = calculate_attribute(22, 1.1, *level);
                                    printf("Level up! Now level %d\n", *level);
                                }
                            } else {
                                printf("You died.\n");
                                exit(1);
                            }
                        }
                    }
                } else if (strcmp(type, "teleport") == 0) {
                    sscanf(param, "%d %d", pos_x, pos_y);
                    printf("\nTeleported to %d,%d\n", *pos_x, *pos_y);
                } else if (strcmp(type, "text") == 0) {
                    char display[100];
                    strcpy(display, param);
                    for (int j = 0; display[j]; j++) if (display[j] == '_') display[j] = ' ';
                    printf("\nMessage: %s\n", display);
                } else if (strcmp(type, "choice") == 0) {
                    char opts[4][20];
                    sscanf(param, "%[^,],%[^,],%[^,],%s", opts[0], opts[1], opts[2], opts[3]);
                    printf("\nChoose:\n");
                    for (int j = 0; j < 4; j++) {
                        for (int k = 0; opts[j][k]; k++) if (opts[j][k] == '_') opts[j][k] = ' ';
                        printf("%d. %s\n", j + 1, opts[j]);
                    }
                    int choice;
                    if (scanf("%d", &choice) != 1 || choice < 1 || choice > 4) {
                        printf("Invalid choice. Pick 1-4.\n");
                    } else {
                        printf("You chose: %s\n", opts[choice - 1]);
                    }
                }
            }
        }
    } else {
        printf("\nOut of bounds! Stay within 0-4.\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <game_file> (e.g., %s icers_static.txt)\n", argv[0], argv[0]);
        return 1;
    }

    // Load emojis first
    load_emojis();

    char static_path[150];
    char dynamic_path[150];
    // Updated to 16x16 map
    char map[16][17] = {{0}};  // Initialize to all zeros
    char player[200] = {0};
    char enemies[10][200] = {{0}};
    int enemy_count = 0;
    char items[10][200] = {{0}};
    int item_count = 0;
    char events[10][200] = {{0}};
    int event_count = 0;
    char battles[10][200] = {{0}};
    int battle_count = 0;
    char inventory[10][50] = {{0}};
    int inv_count = 0;
    int pos_x = 0, pos_y = 1;
    int level = 1, xp = 0, current_health = 0;

    strncpy(static_path, argv[1], sizeof(static_path) - 1);
    static_path[sizeof(static_path) - 1] = '\0';
    snprintf(dynamic_path, sizeof(dynamic_path), "%s_dynamic.txt", argv[1]);

    printf("Loading static file: %s\n", static_path);
    FILE *fp = fopen(static_path, "r");
    if (!fp) {
        printf("Cannot load game from %s. Did you save it with the editor?\n", static_path);
        return 1;
    }

    char line[200];
    // First try to load from debug.csv (new format)
    FILE *csv_fp = fopen("debug.csv", "r");
    if (csv_fp) {
        printf("Loading map from debug.csv (new format)\n");
        
        // Initialize map with empty spaces (index 0 for blank emoji)
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 16; j++) {
                map[i][j] = 0;  // 0 is the blank emoji
            }
            map[i][16] = '\0';
        }
        
        // Skip header line
        if (fgets(line, sizeof(line), csv_fp)) {
            printf("Skipped header: %s", line);
        }
        
        // Skip scale info line
        if (fgets(line, sizeof(line), csv_fp)) {
            printf("Skipped scale info: %s", line);
        }
        
        // Load tile data
        while (fgets(line, sizeof(line), csv_fp)) {
            int x, y, z, emoji_idx, fg_color_idx, bg_color_idx;
            float scale, rel_x, rel_y, rel_z;
            
            // Parse the line with the new format (10 columns)
            int parsed = sscanf(line, "%d,%d,%d,%d,%d,%d,%f,%f,%f,%f", 
                               &x, &y, &z, &emoji_idx, &fg_color_idx, &bg_color_idx,
                               &scale, &rel_x, &rel_y, &rel_z);
            
            if (parsed == 10) {
                // Place player at initial position if this is a player tile
                // (We'll use 'P' character to represent player for now in the map array)
                if (emoji_idx == 5 && pos_x == 0 && pos_y == 1) {  // 5 is the poop emoji, used for player
                    pos_x = x;
                    pos_y = y;
                }
                
                // Place tile on map if within bounds
                if (x >= 0 && x < 16 && y >= 0 && y < 16) {
                    map[y][x] = emoji_idx;  // Store the emoji index directly
                }
            } else {
                printf("Warning: Failed to parse line: %s", line);
            }
        }
        fclose(csv_fp);
    } else {
        // Fall back to old format loading
        while (fgets(line, sizeof(line), fp)) {
            printf("Read line: %s", line);
            if (strstr(line, "Row")) {
                int row_num;
                char temp[6];
                if (sscanf(line, "Row%d: %5s", &row_num, temp) == 2 && row_num >= 1 && row_num <= 5) {
                    strncpy(map[row_num - 1], temp, 5);
                    map[row_num - 1][5] = '\0';
                    printf("Loaded map row %d: %s\n", row_num, map[row_num - 1]);
                } else {
                    printf("Failed to parse map row: %s", line);
                }
            }
            else if (strstr(line, "[Players]")) {
                if (fgets(player, sizeof(player), fp)) {
                    printf("Loaded player: %s", player);
                }
            }
            else if (strstr(line, "[Enemies]")) {
                while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                    strcpy(enemies[enemy_count++], strtok(line, "\n"));
                    printf("Loaded enemy %d: %s\n", enemy_count, enemies[enemy_count - 1]);
                }
            } else if (strstr(line, "[Items]")) {
                while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                    strcpy(items[item_count++], strtok(line, "\n"));
                    printf("Loaded item %d: %s\n", item_count, items[item_count - 1]);
                }
            } else if (strstr(line, "[Events]")) {
                while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                    strcpy(events[event_count++], strtok(line, "\n"));
                    printf("Loaded event %d: %s\n", event_count, events[event_count - 1]);
                }
            } else if (strstr(line, "[Battles]")) {
                while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                    strcpy(battles[battle_count++], strtok(line, "\n"));
                    printf("Loaded battle %d: %s\n", battle_count, battles[battle_count - 1]);
                }
            }
        }
    }
    fclose(fp);
    printf("Static file loaded successfully.\n");

    printf("Checking dynamic file: %s\n", dynamic_path);
    fp = fopen(dynamic_path, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            printf("Dynamic line: %s", line);
            if (strstr(line, "pos_x")) sscanf(line, "pos_x=%d", &pos_x);
            if (strstr(line, "pos_y")) sscanf(line, "pos_y=%d", &pos_y);
            if (strstr(line, "level")) sscanf(line, "level=%d", &level);
            if (strstr(line, "xp")) sscanf(line, "xp=%d", &xp);
            if (strstr(line, "health")) sscanf(line, "health=%d", &current_health);
            if (strstr(line, "inventory")) {
                while (fgets(line, sizeof(line), fp) && line[0] != '[') {
                    strcpy(inventory[inv_count++], strtok(line, "\n"));
                    printf("Loaded inventory %d: %s\n", inv_count, inventory[inv_count - 1]);
                }
            }
        }
        fclose(fp);
    } else {
        int base_health;
        double multiplier;
        if (sscanf(player, "%*[^,], base_health=%d, %*[^,], multiplier=%lf", &base_health, &multiplier) == 2) {
            current_health = calculate_attribute(base_health, multiplier, level);
            printf("Initialized health: %d\n", current_health);
        } else {
            printf("Failed to parse player stats from: %s", player);
            current_health = 100;
        }
    }

    // Main game loop with arrow key controls
    printf("\nUse arrow keys to move. Press 'q' to quit.\n");
    int quit = 0;
    while (!quit) {
        // Clear screen and move cursor to top-left (works on most terminals)
        printf("\033[2J\033[1;1H");
        
        // Display map with player position
        printf("Map:\n");
        // Top border
        printf("┌");
        for (int j = 0; j < 16; j++) {
            printf("──");
        }
        printf("┐\n");
        
        for (int i = 0; i < 16; i++) {
            // Left border
            printf("│");
            // Display row
            for (int j = 0; j < 16; j++) {
                // Show player position
                if (i == pos_x && j == pos_y) {
                    printf("P ");  // Show player with 'P'
                } else {
                    // Display the emoji based on the stored index
                    int emoji_idx = map[i][j];
                    if (emoji_idx > 0 && emoji_idx < emoji_count) {
                        printf("%s ", emojis[emoji_idx]);
                    } else {
                        printf("  ");  // Show blank if index is 0 or invalid
                    }
                }
            }
            // Right border
            printf("│\n");
        }
        // Bottom border
        printf("└");
        for (int j = 0; j < 16; j++) {
            printf("──");
        }
        printf("┘\n");
        printf("Health: %d, Level: %d, XP: %d\n", current_health, level, xp);
        printf("\nUse arrow keys to move. Press 'q' to quit.\n");
        
        // Check for key press
        if (kbhit()) {
            int key = getchar();
            if (key == 'q' || key == 'Q') {
                quit = 1;
            } else {
                ungetc(key, stdin);
                int direction = get_arrow_key();
                if (direction > 0) {
                    process_movement(direction, map, &pos_x, &pos_y, events, event_count, 
                                   enemies, enemy_count, &current_health, &level, &xp);
                }
            }
        } else {
            // Small delay to prevent excessive CPU usage when no input
            usleep(100000); // 100ms
        }
    }

    // Save game on exit
    fp = fopen(dynamic_path, "w");
    if (fp) {
        fprintf(fp, "[Player]\npos_x=%d\npos_y=%d\nlevel=%d\nxp=%d\nhealth=%d\n[inventory]\n", 
                pos_x, pos_y, level, xp, current_health);
        for (int i = 0; i < inv_count; i++) fprintf(fp, "%s\n", inventory[i]);
        fclose(fp);
        printf("Game saved to %s\n", dynamic_path);
    }

    printf("Exiting game.\n");
    return 0;
}