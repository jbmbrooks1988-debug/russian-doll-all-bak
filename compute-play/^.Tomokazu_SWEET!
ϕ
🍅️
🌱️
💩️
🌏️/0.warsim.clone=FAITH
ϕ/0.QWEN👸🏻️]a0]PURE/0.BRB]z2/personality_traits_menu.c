#include <stdio.h>
#include <stdlib.h>

void display_menu() {
    system("clear");
    printf("------------PERSONALITY TRAITS------------------------------------------------------------\n");
    printf("Your life coach unrolls a personality scroll, awaiting your new vibe\n");
    printf("1)  Gossip Magnet (pay 20 glitz per rumor you start)\n");
    printf("2)  Ban Stylists (go indie-only)\n");
    printf("3)  Glamour Tax (earn extra glitz from events, lose charm)\n");
    printf("4)  Goblin Chic Advocate (5 glitz reward for each goblin trend adopted)\n");
    printf("5)  Forceful Friendship (auto-add 1 friend per turn)\n");
    printf("6)  Subsidize Tea Parties (costs 1k glitz/turn, +charm)\n");
    printf("7)  Longer Photo Shoots\n");
    printf("8)  Ban Fast Fashion\n");
    printf("9)  Stylist Grant (allow larger image teams)\n");
    printf("10) Forgive Ex-Rivals (they may re-follow you)\n");
    printf("11) Cease Charm Training\n");
    printf("12) Sparkle Tax (extra income, lower popularity)\n");
    printf("13) Allow Only Mini-Crews\n");
    printf("14) Host the Goblin Celebration (1k per turn, +gossip power)\n");
    printf("______________________________________________________📄️\n");
    printf("Enter your choice: ");
}

int main() {
    display_menu();
    // Input handling logic would go here
    return 0;
}
