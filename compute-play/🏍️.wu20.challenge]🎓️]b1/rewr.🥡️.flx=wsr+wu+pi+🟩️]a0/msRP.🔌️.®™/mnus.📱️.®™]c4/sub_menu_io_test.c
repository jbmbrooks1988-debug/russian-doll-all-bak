#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE 32

int main(int argc, char *argv[]) {
    if (argc > 1) {
        printf("Error: No arguments allowed\n");
        return 1;
    }

    printf("Submenu Options:\n");
    printf("  1: Do X\n");
    printf("  2: Do Y\n");
    printf("Enter choice (1-2): ");
    fflush(stdout); // Ensure prompt is sent

    char input[MAX_LINE];
    if (!fgets(input, sizeof(input), stdin)) {
        printf("Error: Failed to read input\n");
        return 1;
    }
    input[strcspn(input, "\n")] = '\0';
    int choice = atoi(input);
    if (choice < 1 || choice > 2) {
        printf("Error: Invalid choice '%s'. Choose 1-2.\n", input);
        return 1;
    }

    FILE *fp = fopen("test_submenu_output.txt", "a");
    if (!fp) {
        printf("Error: Cannot write to test_submenu_output.txt\n");
        return 1;
    }
    fprintf(fp, "Chose %d: %s\n", choice, choice == 1 ? "Do X" : "Do Y");
    fclose(fp);

    printf("You chose: %s\n", choice == 1 ? "Do X" : "Do Y");
    return 0;
}
