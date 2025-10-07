#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void read_glitz(int *glitz) {
    FILE *fp = fopen("data/player_stats.txt", "r");
    if (fp == NULL) { *glitz = 0; return; }
    char line[256];
    *glitz = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "glitz:%d", glitz) == 1) break;
    }
    fclose(fp);
}

void display_menu(int glitz) {
    system("clear");
    printf("       .-.\n");
    printf("     __|=|__\n");
    printf("    (_/`-`_\)
");
    printf("    //\\___//\\
");
    printf("    <>/   <>\n");
    printf("     \|_._|/\n");
    printf("      <_I_>\n");
    printf("       |||\n");
    printf("      /_|_\\ 
");
    printf("------OUTFIT DESIGN MENU-----------you have [%d Glitz]-----------------------\n", glitz);
    printf("1) Sew a Casual Look (20 glitz)\n");
    printf("2) Craft a Party Dress (50 glitz)\n");
    printf("3) Design a Ball Gown (500 glitz)\n");
    printf("x) No stolen designs currently available to rework\n");
    printf("5) Stitch a Goblin-Chic Accessory (5 glitz)\n");
    printf("0) exit\n");
    printf("______________________________________________________📄️\n");
    printf("Enter your choice: ");
}

int main() {
    int glitz;
    read_glitz(&glitz);
    display_menu(glitz);
    // Input handling logic would go here
    return 0;
}
