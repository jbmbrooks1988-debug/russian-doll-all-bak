#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//🛡️@🪆️🥡️

#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define TILE_SIZE 10
#define MAX_MONSTERS 100
#define MAX_ITEMS 100
#define MAX_INVENTORY 10

char map[MAP_HEIGHT][MAP_WIDTH];
int player_x, player_y;
int player_hp, player_mp, player_gold;
char inventory[MAX_INVENTORY][50];
int num_inventory_items = 0;
int show_help = 0;

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
} Item;

Item items[MAX_ITEMS];
int num_items = 0;

void load_map() {
    FILE *file = fopen("c_rewrite/map.txt", "r");
    if (file == NULL) {
        perror("Error opening map file");
        exit(1);
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
        perror("Error opening player position file");
        exit(1);
    }
    fscanf(file, "%d,%d", &player_x, &player_y);
    fclose(file);
}

void load_player_stats() {
    FILE *file = fopen("c_rewrite/player_stats.txt", "r");
    if (file == NULL) {
        perror("Error opening player stats file");
        exit(1);
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
        num_monsters = 0;
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
        num_items = 0;
        return;
    }
    num_items = 0;
    while (fscanf(file, "%c,%d,%d\n", &items[num_items].symbol, &items[num_items].x, &items[num_items].y) != EOF) {
        num_items++;
    }
    fclose(file);
}

void render_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map[y][x] == '#') {
                glColor3f(0.5f, 0.5f, 0.5f);
            } else {
                glColor3f(0.2f, 0.2f, 0.2f);
            }
            glBegin(GL_QUADS);
            glVertex2i(x * TILE_SIZE, y * TILE_SIZE);
            glVertex2i((x + 1) * TILE_SIZE, y * TILE_SIZE);
            glVertex2i((x + 1) * TILE_SIZE, (y + 1) * TILE_SIZE);
            glVertex2i(x * TILE_SIZE, (y + 1) * TILE_SIZE);
            glEnd();
        }
    }
}

void render_player() {
    glColor3f(1.0f, 1.0f, 0.0f);
    glRasterPos2i(player_x * TILE_SIZE + TILE_SIZE / 4, player_y * TILE_SIZE + TILE_SIZE - TILE_SIZE / 4);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, '@');
}

void render_monsters() {
    glColor3f(1.0f, 0.0f, 0.0f); // Red for monsters
    for (int i = 0; i < num_monsters; i++) {
        glRasterPos2i(monsters[i].x * TILE_SIZE + TILE_SIZE / 4, monsters[i].y * TILE_SIZE + TILE_SIZE - TILE_SIZE / 4);
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, monsters[i].symbol);
    }
}

void render_items() {
    glColor3f(0.0f, 1.0f, 1.0f); // Cyan for items
    for (int i = 0; i < num_items; i++) {
        glRasterPos2i(items[i].x * TILE_SIZE + TILE_SIZE / 4, items[i].y * TILE_SIZE + TILE_SIZE - TILE_SIZE / 4);
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, items[i].symbol);
    }
}

void render_hud() {
    char hud_text[100];
    
    sprintf(hud_text, "HP: %d | MP: %d | Gold: %d", player_hp, player_mp, player_gold);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2i(10, 20);
    for (char *c = hud_text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }

    char inventory_title[] = "Inventory:";
    glRasterPos2i(10, 40);
    for (char *c = inventory_title; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    for (int i = 0; i < num_inventory_items; i++) {
        glRasterPos2i(10, 60 + i * 20);
        for (char *c = inventory[i]; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
    }
}

void render_help() {
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    char *help_lines[] = {
        "--- HELP ---",
        "Arrow Keys: Move",
        ", (comma): Pick up item",
        "h: Toggle this help screen",
        "u: Use item (uses first item in inventory)",
        "ESC: Exit game",
        NULL
    };

    for (int i = 0; help_lines[i] != NULL; i++) {
        glRasterPos2i(10, 20 + i * 20);
        for (char *c = help_lines[i]; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (show_help) {
        render_help();
    } else {
        render_map();
        render_items();
        render_monsters();
        render_player();
        render_hud();
    }
    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, MAP_WIDTH * TILE_SIZE, MAP_HEIGHT * TILE_SIZE, 0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 27) {
        exit(0);
    } else if (key == ',') {
        char command[256];
        snprintf(command, sizeof(command), "./c_rewrite/pickup_item %d %d", player_x, player_y);
        system(command);
        load_items();
        load_inventory();
        glutPostRedisplay();
    } else if (key == 'h') {
        show_help = !show_help;
        glutPostRedisplay();
    } else if (key == 'u') {
        if (num_inventory_items > 0) {
            char command[256];
            snprintf(command, sizeof(command), "./c_rewrite/use_item %s", inventory[0]);
            system(command);
            load_inventory();
            load_player_stats();
            glutPostRedisplay();
        }
    }
}

void special_keyboard(int key, int x, int y) {
    char command[256];
    char *direction = NULL;

    if (key == GLUT_KEY_UP) direction = "up";
    else if (key == GLUT_KEY_DOWN) direction = "down";
    else if (key == GLUT_KEY_LEFT) direction = "left";
    else if (key == GLUT_KEY_RIGHT) direction = "right";

    if (direction) {
        snprintf(command, sizeof(command), "./c_rewrite/player_control %s", direction);
        system(command);
        load_player_pos();
        load_player_stats();

        system("./c_rewrite/monster_ai");
        load_monsters();

        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {

 if (chdir("../") != 0) {
        perror("chdir failed");
        return 1;
    }

    system("./c_rewrite/generate_map");
    system("./c_rewrite/spawn_monsters");
    system("./c_rewrite/spawn_items");

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(MAP_WIDTH * TILE_SIZE, MAP_HEIGHT * TILE_SIZE);
    glutCreateWindow("Angband Remake");

    glClearColor(0.0, 0.0, 0.0, 1.0);

    load_map();
    load_player_pos();
    load_player_stats();
    load_inventory();
    load_monsters();
    load_items();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keyboard);

    glutMainLoop();

    return 0;
}
