#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For usleep
#include <termios.h> // For terminal control
#include <signal.h> // For signal handling
#include <time.h> // For random monster movement
#include <fcntl.h> // For O_NONBLOCK

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
    char name[50];
} Item;

Item items[MAX_ITEMS];
int num_items = 0;

// Signal handler for Ctrl+C
void handle_ctrl_c(int sig) {
    printf("\nExiting game...\n");
    // Restore terminal settings before exit
    struct termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    oldt.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    exit(0);
}

// Initialize a simple map
void init_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1) {
                map[y][x] = '#'; // Walls
            } else {
                map[y][x] = '.'; // Floor
            }
        }
    }
    // Add some walls for variety
    for (int x = 20; x < 30; x++) {
        map[10][x] = '#';
    }
}

// Initialize player
void init_player() {
    player_x = 5;
    player_y = 5;
    player_hp = 100;
    player_mp = 50;
    player_gold = 0;
}

// Initialize monsters
void init_monsters() {
    srand(time(NULL));
    num_monsters = 3;
    monsters[0] = (Monster){1, "Goblin", 'G', 10, 10, 20, 5};
    monsters[1] = (Monster){2, "Troll", 'T', 15, 15, 30, 8};
    monsters[2] = (Monster){3, "Orc", 'O', 20, 5, 25, 6};
}

// Initialize items
void init_items() {
    num_items = 2;
    items[0] = (Item){'*', 8, 8, "Potion"};
    items[1] = (Item){'$', 12, 12, "Gold"};
}

void load_map() {
    FILE *file = fopen("c_rewrite/map.txt", "r");
    if (file == NULL) {
        init_map(); // Fallback to default map
        return;
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
        init_player(); // Fallback to default position
        return;
    }
    fscanf(file, "%d,%d", &player_x, &player_y);
    fclose(file);
}

void load_player_stats() {
    FILE *file = fopen("c_rewrite/player_stats.txt", "r");
    if (file == NULL) {
        init_player(); // Fallback to default stats
        return;
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
        init_monsters(); // Fallback to default monsters
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
        init_items(); // Fallback to default items
        return;
    }
    num_items = 0;
    while (fscanf(file, "%c,%d,%d\n", &items[num_items].symbol, &items[num_items].x, &items[num_items].y) != EOF) {
        strcpy(items[num_items].name, items[num_items].symbol == '*' ? "Potion" : "Gold");
        num_items++;
    }
    fclose(file);
}

void move_player(const char *direction) {
    int new_x = player_x, new_y = player_y;
    if (strcmp(direction, "up") == 0) new_y--;
    else if (strcmp(direction, "down") == 0) new_y++;
    else if (strcmp(direction, "left") == 0) new_x--;
    else if (strcmp(direction, "right") == 0) new_x++;

    // Check boundaries and walls
    if (new_x > 0 && new_x < MAP_WIDTH - 1 && new_y > 0 && new_y < MAP_HEIGHT - 1 && map[new_y][new_x] != '#') {
        player_x = new_x;
        player_y = new_y;
    }
}

void move_monsters() {
    for (int i = 0; i < num_monsters; i++) {
        if (monsters[i].hp <= 0) continue; // Skip dead monsters
        int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}; // Up, down, left, right
        int dir = rand() % 4;
        int new_x = monsters[i].x + directions[dir][0];
        int new_y = monsters[i].y + directions[dir][1];
        // Check boundaries, walls, and player collision
        if (new_x > 0 && new_x < MAP_WIDTH - 1 && new_y > 0 && new_y < MAP_HEIGHT - 1 && map[new_y][new_x] != '#' && !(new_x == player_x && new_y == player_y)) {
            monsters[i].x = new_x;
            monsters[i].y = new_y;
        }
    }
}

void pickup_item(int x, int y) {
    for (int i = 0; i < num_items; i++) {
        if (items[i].x == x && items[i].y == y && num_inventory_items < MAX_INVENTORY) {
            strcpy(inventory[num_inventory_items], items[i].name);
            num_inventory_items++;
            if (strcmp(items[i].name, "Gold") == 0) {
                player_gold += 10;
            }
            // Remove item from map
            items[i] = items[num_items - 1];
            num_items--;
            printf("Picked up: %s\n", inventory[num_inventory_items - 1]);
            return;
        }
    }
    printf("No item to pick up here.\n");
}

void use_item(const char *item_name) {
    for (int i = 0; i < num_inventory_items; i++) {
        if (strcmp(inventory[i], item_name) == 0) {
            if (strcmp(item_name, "Potion") == 0) {
                player_hp += 20;
                if (player_hp > 100) player_hp = 100;
                printf("Used %s: HP restored to %d\n", item_name, player_hp);
            }
            // Remove item from inventory
            inventory[i][0] = '\0';
            for (int j = i; j < num_inventory_items - 1; j++) {
                strcpy(inventory[j], inventory[j + 1]);
            }
            num_inventory_items--;
            return;
        }
    }
    printf("No %s in inventory.\n", item_name);
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
                    if (x == monsters[i].x && y == monsters[i].y && monsters[i].hp > 0) {
                        char_to_print = monsters[i].symbol;
                        break;
                    }
                }
                // Check for items (only if no monster is there)
                if (char_to_print == map[y][x]) {
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
    printf("Controls: Arrow keys or WASD (move), p (pickup), u (use potion), Ctrl+C (exit)\n");
    fflush(stdout); // Ensure prompt is displayed immediately
}

void get_cli_input() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // Save current terminal settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    newt.c_cc[VMIN] = 0; // Non-blocking read
    newt.c_cc[VTIME] = 0; // No timeout
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); // Apply new settings

    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    char buffer[3] = {0}; // Buffer for escape sequences
    int buf_pos = 0;

    // Poll for input
    while (buf_pos < 3) {
        char c;
        ssize_t nread = read(STDIN_FILENO, &c, 1);
        if (nread == -1) {
            usleep(10000); // Brief delay to avoid CPU hogging
            continue;
        }
        if (nread == 0) continue;

        printf("DEBUG: Got char %d (%c)\n", c, (c >= 32 && c <= 126) ? c : '.'); // Debug: print ASCII value

        if (buf_pos == 0 && c != '\x1b') {
            // Single-character inputs
            if (c == 3) { // Ctrl+C
                handle_ctrl_c(SIGINT);
            } else if (c == 'p') {
                pickup_item(player_x, player_y);
            } else if (c == 'u') {
                use_item("Potion");
            } else if (c == 'w') {
                move_player("up");
                move_monsters();
            } else if (c == 's') {
                move_player("down");
                move_monsters();
            } else if (c == 'a') {
                move_player("left");
                move_monsters();
            } else if (c == 'd') {
                move_player("right");
                move_monsters();
            }
            break; // Process single character and exit
        }

        // Handle escape sequences
        buffer[buf_pos++] = c;
        if (buf_pos == 1 && c != '\x1b') {
            buf_pos = 0; // Reset if not an escape sequence
        } else if (buf_pos == 3) {
            // Complete escape sequence (e.g., \x1b[A)
            printf("DEBUG: Sequence %c%c%c\n", buffer[0], buffer[1], buffer[2]); // Debug: print sequence
            if (buffer[0] == '\x1b' && buffer[1] == '[') {
                switch (buffer[2]) {
                    case 'A':
                        printf("DEBUG: Detected up arrow\n");
                        move_player("up");
                        move_monsters();
                        break;
                    case 'B':
                        printf("DEBUG: Detected down arrow\n");
                        move_player("down");
                        move_monsters();
                        break;
                    case 'C':
                        printf("DEBUG: Detected right arrow\n");
                        move_player("right");
                        move_monsters();
                        break;
                    case 'D':
                        printf("DEBUG: Detected left arrow\n");
                        move_player("left");
                        move_monsters();
                        break;
                }
            }
            buf_pos = 0; // Reset buffer after processing
            break;
        }
    }

    // Restore stdin to blocking
    fcntl(STDIN_FILENO, F_SETFL, flags);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore terminal settings
}

int main() {
    // Set up Ctrl+C handler
    signal(SIGINT, handle_ctrl_c);

    // Initial game setup
    load_map();
    load_player_pos();
    load_player_stats();
    load_inventory();
    load_monsters();
    load_items();

    // Game loop
    while (1) {
        render_game_state_cli(); // Render the current state
        get_cli_input(); // Get and process input directly
    }

    return 0;
}
