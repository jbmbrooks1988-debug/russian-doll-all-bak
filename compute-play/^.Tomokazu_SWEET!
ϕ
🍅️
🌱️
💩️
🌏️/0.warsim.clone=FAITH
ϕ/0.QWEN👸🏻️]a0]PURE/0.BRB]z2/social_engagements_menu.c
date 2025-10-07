#include <stdio.h>
#include <stdlib.h>

void display_menu() {
    system("clear");
    printf("         _______________");
    printf("    ()==(              (@==()\n");
    printf("         '______________'|");
    printf("           |             |");
    printf("           |             |");
    printf("         __)_____________|");
    printf("    ()==(               (@==()\n");
    printf("         '--------------'");
    printf("------------SOCIAL ENGAGEMENTS----------------------------------------------\n");
    printf("Your social manager appears with a planner, ready to schedule your next big move\n");
    printf("1) Drama Club\n");
    printf("2) Trendsetters\n");
    printf("3) K'rut Fashion Circle\n");
    printf("4) E'rak Art Collective\n");
    printf("5) Independent Influencers\n");
    printf("6) Minor Goblin Fashionistas\n");
    printf("7) Underground Gossip Rings\n");
    printf("8) Image Teams\n");
    printf("9) Deserted Ex-Admirers\n");
    printf("0) Exit\n");
    printf("______________________________________________________📄️\n");
    printf("Enter your choice: ");
}

int main() {
    display_menu();
    // Input handling logic would go here
    return 0;
}
