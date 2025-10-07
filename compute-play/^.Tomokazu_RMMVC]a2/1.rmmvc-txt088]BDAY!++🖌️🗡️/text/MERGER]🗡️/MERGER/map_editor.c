#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Check if game name is provided
    if (argc != 2) {
        printf("Usage: %s <game_name> (e.g., %s icers)\n", argv[0], argv[0]);
        return 1;
    }
    
    char game_name[50];
    char static_path[150];
    char map[5][6] = {".P...", ".....", ".E...", ".....", "....."};
    
    // Copy game name from argument
    strcpy(game_name, argv[1]);
    sprintf(static_path, "%s_static.txt", game_name); // e.g., "icemen_static.txt"
    
    // Try to load existing map from static file if it exists
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
                }
            }
        }
        fclose(fp);
        printf("Loaded existing map from %s\n", static_path);
    }

    int choice;
    do {
        printf("\nMap Editor Menu:\n");
        printf("1. Display Map\n");
        printf("2. Edit Map Cell\n");
        printf("3. Save Map and Exit\n");
        printf("4. Exit Without Saving\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        if (choice == 1) { // Display Map
            printf("\nCurrent Map:\n");
            for (int i = 0; i < 5; i++) printf("%s\n", map[i]);
        } else if (choice == 2) { // Edit Map Cell
            int row, col;
            printf("\nCurrent Map:\n");
            for (int i = 0; i < 5; i++) printf("%s\n", map[i]);
            printf("Enter row (0-4) and col (0-4) to edit: ");
            scanf("%d %d", &row, &col);
            if (row >= 0 && row < 5 && col >= 0 && col < 5) {
                printf("Enter char (e.g., '.' empty, 'P' player, 'E' enemy): ");
                scanf(" %c", &map[row][col]);
                printf("Cell updated.\n");
            } else {
                printf("Invalid coordinates. Use 0-4.\n");
            }
        } else if (choice == 3) { // Save Map and Exit
            // Create a temporary file with just the map data
            FILE *fp = fopen(static_path, "w");
            if (!fp) {
                printf("Error: Could not write to %s. Check permissions in current directory.\n", static_path);
                return 1;
            }
            printf("Saving map to %s\n", static_path);
            fprintf(fp, "[Map]\nSize: 5 5\n");
            for (int i = 0; i < 5; i++) fprintf(fp, "Row%d: %s\n", i + 1, map[i]);
            // Add empty sections for other data that will be filled by other modules
            fprintf(fp, "[Players]\n[Enemies]\n[Items]\n[Events]\n[Battles]\n");
            fclose(fp);
            printf("Map saved to %s\n", static_path);
            break;
        } else if (choice == 4) { // Exit Without Saving
            printf("Exiting without saving.\n");
            break;
        } else {
            printf("Invalid choice. Please enter 1-4.\n");
        }
    } while (1);

    return 0;
}