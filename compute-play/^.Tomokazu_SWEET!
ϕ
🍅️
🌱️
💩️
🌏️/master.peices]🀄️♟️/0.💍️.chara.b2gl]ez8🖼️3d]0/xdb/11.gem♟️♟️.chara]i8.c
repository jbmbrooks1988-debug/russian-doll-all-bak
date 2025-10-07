#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define BOARD_SIZE 8
#define TILE_SIZE 64
#define SCALE_FACTOR 2.0
#define WINDOW_WIDTH (BOARD_SIZE * TILE_SIZE * SCALE_FACTOR)
#define WINDOW_HEIGHT (BOARD_SIZE * TILE_SIZE * SCALE_FACTOR)
#define MAX_SPRITES 50
#define MOVE_RANGE 2
#define MAX_INVENTORY 5
#define MAX_HP 100

char* piece_names[MAX_SPRITES];
GLuint piece_textures[MAX_SPRITES];
int num_pieces = 0;
GLuint background_texture;
GLuint cursor_texture;

typedef enum { ITEM_TYPE_POTION, ITEM_TYPE_WEAPON } ItemType;

typedef struct {
    char name[50];
    ItemType type;
    int value;
} Item;

typedef struct {
    int player;
    int hp;
    int attack;
    int defense;
} Piece;

typedef struct {
    char* board[BOARD_SIZE][BOARD_SIZE];
    int player_board[BOARD_SIZE][BOARD_SIZE];
    int current_turn;
    int selected_x;
    int selected_y;
    int show_stats_x;
    int show_stats_y;
    int show_menu_x;
    int show_menu_y;
    int move_mode;
    int move_origin_x;
    int move_origin_y;
    float player1_timer;
    float player2_timer;
    int game_over;
    int winner;
    int show_inventory_x;
    int show_inventory_y;
    Item inventory[MAX_INVENTORY];
    int inventory_count;
    char temporary_message[100];
    float message_timer;
    int attack_mode;
    int menu_selection;
} GameState;

GameState game_state;
int last_time = 0;

void write_piece_stats(const char* piece_name, Piece piece);
Piece read_piece_stats(const char* piece_name);
void delete_piece_stats(const char* piece_name);
void handle_ai_turn();
void update(int value);
void write_inventory(const char* piece_name, Item inventory[], int count);
int read_inventory(const char* piece_name, Item inventory[]);
Item read_item_definition(const char* piece_name, const char* item_dir_name);
void draw_player_circles();

GLuint load_texture(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    if (data == NULL) {
        printf("Failed to load texture: %s\n", filename);
        return 0;
    }

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return texture_id;
}

void init_assets() {
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".dir") != NULL) {
                char full_dir_name[100];
                strcpy(full_dir_name, dir->d_name);

                char base_name[100];
                strncpy(base_name, full_dir_name, strlen(full_dir_name) - 4);
                base_name[strlen(full_dir_name) - 4] = '\0';

                if (strcmp(base_name, "sword") == 0) {
                    cursor_texture = load_texture("sword.dir/sword.png");
                } else if (strncmp(base_name, "ss", 2) == 0) {
                    char texture_path[100];
                    sprintf(texture_path, "%s.dir/%s.png", base_name, base_name);
                    background_texture = load_texture(texture_path);
                } else if (num_pieces < MAX_SPRITES) {
                    piece_names[num_pieces] = strdup(base_name);
                    char texture_path[100];
                    sprintf(texture_path, "%s.dir/%s.png", base_name, base_name);
                    piece_textures[num_pieces] = load_texture(texture_path);
                    num_pieces++;
                }
            }
        }
        closedir(d);
    }
}

void create_item_dir(const char* piece_name, const char* item_dir_name, Item item) {
    char item_path[100];
    sprintf(item_path, "%s.dir/%s.dir", piece_name, item_dir_name);
    mkdir(item_path, 0777);

    char stats_path[150];
    sprintf(stats_path, "%s.dir/%s.dir/stats.txt", piece_name, item_dir_name);
    FILE* f = fopen(stats_path, "w");
    if (f == NULL) return;
    fprintf(f, "type:%s\n", (item.type == ITEM_TYPE_POTION) ? "potion" : "weapon");
    fprintf(f, "value:%d\n", item.value);
    fclose(f);
}

void write_weights_and_biases_to_file(const char* piece_name) {
    char weights_filename[100];
    sprintf(weights_filename, "%s.dir/weights.txt", piece_name);
    FILE* f = fopen(weights_filename, "w");
    if (f == NULL) return;
    fprintf(f, "attack_enemy:1.0\n");
    fprintf(f, "move_to_center:0.5\n");
    fprintf(f, "retreat_when_low_hp:0.8\n");
    fclose(f);

    char biases_filename[100];
    sprintf(biases_filename, "%s.dir/biases.txt", piece_name);
    f = fopen(biases_filename, "w");
    if (f == NULL) return;
    fprintf(f, "aggressive_bias:0.1\n");
    fprintf(f, "defensive_bias:-0.1\n");
    fclose(f);
}

void init_game() {
    game_state.current_turn = 1;
    game_state.selected_x = -1;
    game_state.selected_y = -1;
    game_state.show_stats_x = -1;
    game_state.show_stats_y = -1;
    game_state.show_menu_x = -1;
    game_state.show_menu_y = -1;
    game_state.move_mode = 0;
    game_state.move_origin_x = -1;
    game_state.move_origin_y = -1;
    game_state.attack_mode = 0;
    game_state.player1_timer = 300;
    game_state.player2_timer = 300;
    game_state.game_over = 0;
    game_state.winner = 0;
    game_state.show_inventory_x = -1;
    game_state.show_inventory_y = -1;
    game_state.inventory_count = 0;
    game_state.menu_selection = -1;
    strcpy(game_state.temporary_message, "");
    game_state.message_timer = 0;
    last_time = glutGet(GLUT_ELAPSED_TIME);

    // Initialize board to empty
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            game_state.board[i][j] = NULL;
            game_state.player_board[i][j] = 0;
        }
    }

    // Read board_state.txt
    FILE* f = fopen("board_state.txt", "r");
    if (f == NULL) {
        printf("Failed to open board_state.txt\n");
        return;
    }

    char line[256];
    int row = 0;
    while (fgets(line, sizeof(line), f) && row < BOARD_SIZE) {
        char* token = strtok(line, " \n");
        int col = 0;
        while (token != NULL && col < BOARD_SIZE) {
            if (strcmp(token, "none") != 0) {
                game_state.board[row][col] = strdup(token);
                // Read player ownership from <piece_name>.dir/state.txt
                Piece piece = read_piece_stats(token);
                if (piece.player == 1 || piece.player == 2) {
                    game_state.player_board[row][col] = piece.player;
                } else {
                    // Fallback: assume player based on row if state.txt is missing or invalid
                    game_state.player_board[row][col] = (row < 2) ? 1 : 2;
                    piece.player = game_state.player_board[row][col];
                    piece.hp = 100;
                    piece.attack = 10;
                    piece.defense = 0;
                    write_piece_stats(token, piece);
                }
                write_weights_and_biases_to_file(token);
                // Add default items
                if (game_state.player_board[row][col] == 1) {
                    Item potion = {"potion", ITEM_TYPE_POTION, 50};
                    create_item_dir(token, "potion", potion);
                } else {
                    Item power_glove = {"power_glove", ITEM_TYPE_WEAPON, 20};
                    create_item_dir(token, "power_glove", power_glove);
                }
            }
            token = strtok(NULL, " \n");
            col++;
        }
        row++;
    }
    fclose(f);
}

void write_piece_stats(const char* piece_name, Piece piece) {
    char filename[100];
    sprintf(filename, "%s.dir/state.txt", piece_name);
    FILE* f = fopen(filename, "w");
    if (f == NULL) return;
    fprintf(f, "player:%d\nhp:%d\nattack:%d\ndefense:%d\n", piece.player, piece.hp, piece.attack, piece.defense);
    fclose(f);
}

Piece read_piece_stats(const char* piece_name) {
    char filename[100];
    sprintf(filename, "%s.dir/state.txt", piece_name);
    FILE* f = fopen(filename, "r");
    Piece piece = {0};
    if (f != NULL) {
        fscanf(f, "player:%d\nhp:%d\nattack:%d\ndefense:%d\n", &piece.player, &piece.hp, &piece.attack, &piece.defense);
        fclose(f);
    }
    return piece;
}

void delete_piece_stats(const char* piece_name) {
    char filename[100];
    sprintf(filename, "%s.dir/state.txt", piece_name);
    remove(filename);
}

Item read_item_definition(const char* piece_name, const char* item_dir_name) {
    char filename[100];
    sprintf(filename, "%s.dir/%s.dir/stats.txt", piece_name, item_dir_name);
    FILE* f = fopen(filename, "r");
    Item item = {0};
    if (f != NULL) {
        char type_str[20];
        int value;
        fscanf(f, "type:%s\n", type_str);
        fscanf(f, "value:%d\n", &value);
        if (strcmp(type_str, "potion") == 0) {
            item.type = ITEM_TYPE_POTION;
        } else if (strcmp(type_str, "weapon") == 0) {
            item.type = ITEM_TYPE_WEAPON;
        }
        item.value = value;
        strcpy(item.name, item_dir_name);
        fclose(f);
    }
    return item;
}

int read_inventory(const char* piece_name, Item inventory[]) {
    char piece_path[100];
    sprintf(piece_path, "%s.dir", piece_name);
    DIR *d;
    struct dirent *dir;
    d = opendir(piece_path);
    int count = 0;
    if (d) {
        while ((dir = readdir(d)) != NULL && count < MAX_INVENTORY) {
            if (strstr(dir->d_name, ".dir") != NULL && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                char* item_dir_name = strdup(dir->d_name);
                char* base_item_name = strtok(item_dir_name, ".");
                inventory[count] = read_item_definition(piece_name, base_item_name);
                count++;
                free(item_dir_name);
            }
        }
        closedir(d);
    }
    return count;
}

int get_piece_texture_index(const char* piece_name) {
    for (int i = 0; i < num_pieces; i++) {
        if (strcmp(piece_names[i], piece_name) == 0) {
            return i;
        }
    }
    return -1;
}

void write_board_state() {
    FILE* f = fopen("board_state.txt", "w");
    if (f == NULL) return;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (game_state.board[i][j] != NULL) {
                fprintf(f, "%s ", game_state.board[i][j]);
            } else {
                fprintf(f, "none ");
            }
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

void read_ai_move() {
    FILE* f = fopen("ai_move.txt", "r");
    if (f == NULL) return;
    int from_x, from_y, to_x, to_y;
    fscanf(f, "%d %d %d %d", &from_x, &from_y, &to_x, &to_y);
    fclose(f);

    if (from_x != -1) {
        const char* piece_name = game_state.board[from_y][from_x];
        Piece piece = read_piece_stats(piece_name);
        write_piece_stats(piece_name, piece); 

        game_state.board[to_y][to_x] = game_state.board[from_y][from_x];
        game_state.player_board[to_y][to_x] = game_state.player_board[from_y][from_x];
        game_state.board[from_y][from_x] = NULL;
        game_state.player_board[from_y][from_x] = 0;

        game_state.current_turn = 1;
    }
    glutPostRedisplay();
}

void handle_ai_turn() {
    write_board_state();
    system("./ai_module");
    read_ai_move();
}

void draw_text(float x, float y, char *string) {
    glRasterPos2f(x, y);
    for (char *c = string; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

void draw_background() {
    glBindTexture(GL_TEXTURE_2D, background_texture);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(WINDOW_WIDTH, 0);
    glTexCoord2f(1, 1); glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glTexCoord2f(0, 1); glVertex2f(0, WINDOW_HEIGHT);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void draw_pieces() {
    float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (game_state.board[i][j] != NULL) {
                int texture_index = get_piece_texture_index(game_state.board[i][j]);
                if (texture_index != -1) {
                    glBindTexture(GL_TEXTURE_2D, piece_textures[texture_index]);
                    glBegin(GL_QUADS);
                    glTexCoord2f(0, 0); glVertex2f(j * scaled_tile_size, i * scaled_tile_size);
                    glTexCoord2f(1, 0); glVertex2f((j + 1) * scaled_tile_size, i * scaled_tile_size);
                    glTexCoord2f(1, 1); glVertex2f((j + 1) * scaled_tile_size, (i + 1) * scaled_tile_size);
                    glTexCoord2f(0, 1); glVertex2f(j * scaled_tile_size, (i + 1) * scaled_tile_size);
                    glEnd();
                }
            }
        }
    }
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void draw_player_circles() {
    float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0);
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (game_state.board[i][j] != NULL && (game_state.player_board[i][j] == 1 || game_state.player_board[i][j] == 2)) {
                float center_x = j * scaled_tile_size + scaled_tile_size / 2;
                float center_y = i * scaled_tile_size + scaled_tile_size / 2;
                float radius = scaled_tile_size / 2 * 1.1; // Slightly larger than piece
                if (game_state.player_board[i][j] == 1) {
                    glColor4f(0.0, 1.0, 0.8, 0.8); // Neon blue-green for Player 1
                } else {
                    glColor4f(1.0, 0.5, 0.0, 0.8); // Neon red-orange-yellow for Player 2
                }
                glBegin(GL_LINE_LOOP);
                for (int k = 0; k < 360; k += 10) {
                    float angle = k * 3.14159 / 180.0;
                    glVertex2f(center_x + radius * cos(angle), center_y + radius * sin(angle));
                }
                glEnd();
            }
        }
    }
    glDisable(GL_BLEND);
    glLineWidth(1.0);
}

void draw_attack_range() {
    float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;

    if (game_state.attack_mode) {
        glColor4f(1.0, 0.0, 0.0, 0.3);
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;
                int x = game_state.selected_x + j;
                int y = game_state.selected_y + i;
                if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) {
                    if (game_state.board[y][x] != NULL && game_state.player_board[y][x] != game_state.current_turn) {
                        glRectf(x * scaled_tile_size, y * scaled_tile_size, (x + 1) * scaled_tile_size, (y + 1) * scaled_tile_size);
                    }
                }
            }
        }
    }
}

void draw_cursor() {
    float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;

    if (game_state.selected_x != -1 && game_state.selected_y != -1) {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, cursor_texture);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(game_state.selected_x * scaled_tile_size, game_state.selected_y * scaled_tile_size);
        glTexCoord2f(1, 0); glVertex2f((game_state.selected_x + 1) * scaled_tile_size, game_state.selected_y * scaled_tile_size);
        glTexCoord2f(1, 1); glVertex2f((game_state.selected_x + 1) * scaled_tile_size, (game_state.selected_y + 1) * scaled_tile_size);
        glTexCoord2f(0, 1); glVertex2f(game_state.selected_x * scaled_tile_size, (game_state.selected_y + 1) * scaled_tile_size);
        glEnd();
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
    }
}

void draw_stats() {
    float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;

    if (game_state.show_stats_x != -1 && game_state.show_stats_y != -1) {
        const char* piece_name = game_state.board[game_state.show_stats_y][game_state.show_stats_x];
        if (piece_name != NULL) {
            Piece piece = read_piece_stats(piece_name);
            char stats_text[50];
            sprintf(stats_text, "HP: %d ATK: %d", piece.hp, piece.attack);

            glColor4f(0.0, 0.0, 0.0, 0.7);
            glRectf(game_state.show_stats_x * scaled_tile_size, game_state.show_stats_y * scaled_tile_size - 20 * SCALE_FACTOR,
                    (game_state.show_stats_x + 1) * scaled_tile_size, game_state.show_stats_y * scaled_tile_size);

            glColor3f(1.0, 1.0, 1.0);
            draw_text(game_state.show_stats_x * scaled_tile_size + 5 * SCALE_FACTOR, game_state.show_stats_y * scaled_tile_size - 5 * SCALE_FACTOR, stats_text);
        }
    }
}

void draw_menu() {
    float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;

    if (game_state.show_menu_x != -1 && game_state.show_menu_y != -1) {
        glColor4f(0.2, 0.2, 0.2, 0.8);
        glRectf(game_state.show_menu_x * scaled_tile_size + scaled_tile_size, game_state.show_menu_y * scaled_tile_size,
                game_state.show_menu_x * scaled_tile_size + scaled_tile_size + 100 * SCALE_FACTOR, game_state.show_menu_y * scaled_tile_size + 120 * SCALE_FACTOR);

        if (game_state.menu_selection != -1) {
            glColor4f(0.0, 1.0, 0.0, 0.3);
            glRectf(game_state.show_menu_x * scaled_tile_size + scaled_tile_size, game_state.show_menu_y * scaled_tile_size + game_state.menu_selection * 30 * SCALE_FACTOR,
                    game_state.show_menu_x * scaled_tile_size + scaled_tile_size + 100 * SCALE_FACTOR, game_state.show_menu_y * scaled_tile_size + (game_state.menu_selection + 1) * 30 * SCALE_FACTOR);
        }

        glColor3f(1.0, 1.0, 1.0);
        draw_text(game_state.show_menu_x * scaled_tile_size + scaled_tile_size + 5 * SCALE_FACTOR, game_state.show_menu_y * scaled_tile_size + 20 * SCALE_FACTOR, "Show Stats");
        draw_text(game_state.show_menu_x * scaled_tile_size + scaled_tile_size + 5 * SCALE_FACTOR, game_state.show_menu_y * scaled_tile_size + 50 * SCALE_FACTOR, "Move");
        draw_text(game_state.show_menu_x * scaled_tile_size + scaled_tile_size + 5 * SCALE_FACTOR, game_state.show_menu_y * scaled_tile_size + 80 * SCALE_FACTOR, "Items");
        draw_text(game_state.show_menu_x * scaled_tile_size + scaled_tile_size + 5 * SCALE_FACTOR, game_state.show_menu_y * scaled_tile_size + 110 * SCALE_FACTOR, "Attack");
    }
}

void draw_inventory() {
    float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;

    if (game_state.show_inventory_x != -1 && game_state.show_inventory_y != -1) {
        glColor4f(0.2, 0.2, 0.2, 0.8);
        glRectf(game_state.show_inventory_x * scaled_tile_size + scaled_tile_size, game_state.show_inventory_y * scaled_tile_size,
                game_state.show_inventory_x * scaled_tile_size + scaled_tile_size + 150 * SCALE_FACTOR, game_state.show_inventory_y * scaled_tile_size + (game_state.inventory_count * 30 * SCALE_FACTOR) + 10 * SCALE_FACTOR);

        glColor3f(1.0, 1.0, 1.0);
        for (int i = 0; i < game_state.inventory_count; i++) {
            draw_text(game_state.show_inventory_x * scaled_tile_size + scaled_tile_size + 5 * SCALE_FACTOR, game_state.show_inventory_y * scaled_tile_size + (i * 30 * SCALE_FACTOR) + 20 * SCALE_FACTOR, game_state.inventory[i].name);
        }
    }
}

void draw_move_range() {
    float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;

    if (game_state.move_mode) {
        glColor4f(0.0, 1.0, 0.0, 1.0);
        glLineWidth(2.0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int i = -MOVE_RANGE; i <= MOVE_RANGE; i++) {
            for (int j = -MOVE_RANGE; j <= MOVE_RANGE; j++) {
                int x = game_state.move_origin_x + j;
                int y = game_state.move_origin_y + i;
                if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) {
                    if (game_state.player_board[y][x] == 0) {
                        glBegin(GL_LINE_LOOP);
                        glVertex2f(x * scaled_tile_size, y * scaled_tile_size);
                        glVertex2f((x + 1) * scaled_tile_size, y * scaled_tile_size);
                        glVertex2f((x + 1) * scaled_tile_size, (y + 1) * scaled_tile_size);
                        glVertex2f(x * scaled_tile_size, (y + 1) * scaled_tile_size);
                        glEnd();
                    }
                }
            }
        }
        glDisable(GL_BLEND);
        glLineWidth(1.0);
    }
}

void draw_timers() {
    char timer_text[50];
    sprintf(timer_text, "P1: %02d:%02d", (int)game_state.player1_timer / 60, (int)game_state.player1_timer % 60);
    glColor3f(1.0, 1.0, 1.0);
    draw_text(10 * SCALE_FACTOR, 20 * SCALE_FACTOR, timer_text);
    sprintf(timer_text, "P2: %02d:%02d", (int)game_state.player2_timer / 60, (int)game_state.player2_timer % 60);
    draw_text((WINDOW_WIDTH - 100 * SCALE_FACTOR), 20 * SCALE_FACTOR, timer_text);
}

void draw_game_over() {
    if (game_state.game_over) {
        char game_over_text[50];
        sprintf(game_over_text, "Game Over! Player %d wins!", game_state.winner);
        glColor4f(0.0, 0.0, 0.0, 0.7);
        glRectf((WINDOW_WIDTH / 2 - 100 * SCALE_FACTOR), (WINDOW_HEIGHT / 2 - 20 * SCALE_FACTOR), (WINDOW_WIDTH / 2 + 100 * SCALE_FACTOR), (WINDOW_HEIGHT / 2 + 20 * SCALE_FACTOR));
        glColor3f(1.0, 1.0, 1.0);
        draw_text((WINDOW_WIDTH / 2 - 80 * SCALE_FACTOR), (WINDOW_HEIGHT / 2 + 5 * SCALE_FACTOR), game_over_text);
    }
}

void draw_temporary_message() {
    if (game_state.message_timer > 0) {
        glColor3f(1.0, 1.0, 1.0);
        draw_text((WINDOW_WIDTH / 2 - 100 * SCALE_FACTOR), (WINDOW_HEIGHT - 30 * SCALE_FACTOR), game_state.temporary_message);
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    draw_background();
    draw_pieces();
    draw_player_circles();
    draw_cursor();
    draw_stats();
    draw_menu();
    draw_inventory();
    draw_move_range();
    draw_attack_range();
    draw_timers();
    draw_game_over();
    draw_temporary_message();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && !game_state.game_over && game_state.current_turn == 1) {
        float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;
        int tile_x = (int)(x / scaled_tile_size);
        int tile_y = (int)(y / scaled_tile_size);

        if (game_state.show_inventory_x != -1) {
            int item_index = ((int)(y - game_state.show_inventory_y * scaled_tile_size) / (30 * SCALE_FACTOR));
            if (item_index >= 0 && item_index < game_state.inventory_count) {
                Item item = game_state.inventory[item_index];
                if (item.type == ITEM_TYPE_POTION) {
                    const char* piece_name = game_state.board[game_state.show_inventory_y][game_state.show_inventory_x];
                    Piece piece = read_piece_stats(piece_name);
                    piece.hp += item.value;
                    if (piece.hp > MAX_HP) piece.hp = MAX_HP;
                    write_piece_stats(piece_name, piece);

                    char item_path[100];
                    sprintf(item_path, "%s.dir/%s.dir", piece_name, item.name);
                    char rm_command[150];
                    sprintf(rm_command, "rm -r %s", item_path);
                    system(rm_command);

                    sprintf(game_state.temporary_message, "Used %s! +%d HP", item.name, item.value);
                    game_state.message_timer = 2.0;
                }
            }
            game_state.show_inventory_x = -1;
            game_state.show_inventory_y = -1;
            game_state.menu_selection = -1;
            glutPostRedisplay();
            return;
        }

        if (game_state.attack_mode) {
            int dx = abs(tile_x - game_state.selected_x);
            int dy = abs(tile_y - game_state.selected_y);
            if (dx <= 1 && dy <= 1 && (dx != 0 || dy != 0)) {
                if (game_state.board[tile_y][tile_x] != NULL && game_state.player_board[tile_y][tile_x] != game_state.current_turn) {
                    const char* attacker_name = game_state.board[game_state.selected_y][game_state.selected_x];
                    Piece attacker_piece = read_piece_stats(attacker_name);

                    const char* defender_name = game_state.board[tile_y][tile_x];
                    Piece defender_piece = read_piece_stats(defender_name);

                    int damage = attacker_piece.attack - defender_piece.defense;
                    if (damage < 0) damage = 0;

                    defender_piece.hp -= damage;

                    if (defender_piece.hp <= 0) {
                        delete_piece_stats(defender_name);
                        game_state.board[tile_y][tile_x] = NULL;
                        game_state.player_board[tile_y][tile_x] = 0;
                        sprintf(game_state.temporary_message, "%s defeated %s!", attacker_name, defender_name);
                    } else {
                        write_piece_stats(defender_name, defender_piece);
                        sprintf(game_state.temporary_message, "%s attacked %s for %d damage!", attacker_name, defender_name, damage);
                    }
                    game_state.message_timer = 2.0;

                    game_state.attack_mode = 0;
                    game_state.selected_x = -1;
                    game_state.selected_y = -1;
                    game_state.current_turn = 2;
                    glutPostRedisplay();
                    return;
                }
            }
        }

        if (game_state.move_mode) {
            int dx = abs(tile_x - game_state.move_origin_x);
            int dy = abs(tile_y - game_state.move_origin_y);
            if (dx <= MOVE_RANGE && dy <= MOVE_RANGE && game_state.board[tile_y][tile_x] == NULL) {
                const char* piece_name = game_state.board[game_state.move_origin_y][game_state.move_origin_x];
                Piece piece = read_piece_stats(piece_name);
                write_piece_stats(piece_name, piece);

                game_state.board[tile_y][tile_x] = game_state.board[game_state.move_origin_y][game_state.move_origin_x];
                game_state.player_board[tile_y][tile_x] = game_state.player_board[game_state.move_origin_y][game_state.move_origin_x];
                game_state.board[game_state.move_origin_y][game_state.move_origin_x] = NULL;
                game_state.player_board[game_state.move_origin_y][game_state.move_origin_x] = 0;

                game_state.current_turn = 2;
                game_state.selected_x = -1;
                game_state.selected_y = -1;
                game_state.move_mode = 0;
                game_state.move_origin_x = -1;
                game_state.move_origin_y = -1;
                game_state.menu_selection = -1;
            }
            glutPostRedisplay();
            return;
        }

        if (game_state.show_menu_x != -1 && game_state.show_menu_y != -1) {
            float scaled_tile_size = TILE_SIZE * SCALE_FACTOR;
            if (x > game_state.show_menu_x * scaled_tile_size + scaled_tile_size && x < game_state.show_menu_x * scaled_tile_size + scaled_tile_size + 100 * SCALE_FACTOR) {
                if (y > game_state.show_menu_y * scaled_tile_size && y < game_state.show_menu_y * scaled_tile_size + 30 * SCALE_FACTOR) {
                    game_state.show_stats_x = game_state.show_menu_x;
                    game_state.show_stats_y = game_state.show_menu_y;
                } else if (y > game_state.show_menu_y * scaled_tile_size + 30 * SCALE_FACTOR && y < game_state.show_menu_y * scaled_tile_size + 60 * SCALE_FACTOR) {
                    game_state.move_mode = 1;
                    game_state.move_origin_x = game_state.show_menu_x;
                    game_state.move_origin_y = game_state.show_menu_y;
                    game_state.show_menu_x = -1;
                    game_state.show_menu_y = -1;
                    game_state.menu_selection = -1;
                } else if (y > game_state.show_menu_y * scaled_tile_size + 60 * SCALE_FACTOR && y < game_state.show_menu_y * scaled_tile_size + 90 * SCALE_FACTOR) {
                    game_state.show_inventory_x = game_state.show_menu_x;
                    game_state.show_inventory_y = game_state.show_menu_y;
                    const char* piece_name = game_state.board[game_state.show_menu_y][game_state.show_menu_x];
                    game_state.inventory_count = read_inventory(piece_name, game_state.inventory);
                } else if (y > game_state.show_menu_y * scaled_tile_size + 90 * SCALE_FACTOR && y < game_state.show_menu_y * scaled_tile_size + 120 * SCALE_FACTOR) {
                    game_state.attack_mode = 1;
                }
            }
            game_state.show_menu_x = -1;
            game_state.show_menu_y = -1;
            game_state.menu_selection = -1;
            glutPostRedisplay();
            return;
        }

        if (game_state.board[tile_y][tile_x] != NULL && game_state.player_board[tile_y][tile_x] == game_state.current_turn) {
            game_state.selected_x = tile_x;
            game_state.selected_y = tile_y;
            game_state.show_menu_x = tile_x;
            game_state.show_menu_y = tile_y;
            game_state.menu_selection = -1;
        } else {
            game_state.selected_x = -1;
            game_state.selected_y = -1;
            game_state.menu_selection = -1;
        }
        game_state.show_stats_x = -1;
        game_state.show_stats_y = -1;
        glutPostRedisplay();
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (!game_state.game_over && game_state.current_turn == 1) {
        switch (key) {
            case 27: // ESC key to deselect
                game_state.selected_x = -1;
                game_state.selected_y = -1;
                game_state.show_menu_x = -1;
                game_state.show_menu_y = -1;
                game_state.show_stats_x = -1;
                game_state.show_stats_y = -1;
                game_state.show_inventory_x = -1;
                game_state.show_inventory_y = -1;
                game_state.move_mode = 0;
                game_state.move_origin_x = -1;
                game_state.move_origin_y = -1;
                game_state.attack_mode = 0;
                game_state.menu_selection = -1;
                break;
            case 13: // Enter key
                if (game_state.show_menu_x != -1 && game_state.show_menu_y != -1 && game_state.menu_selection != -1) {
                    if (game_state.menu_selection == 0) {
                        game_state.show_stats_x = game_state.show_menu_x;
                        game_state.show_stats_y = game_state.show_menu_y;
                    } else if (game_state.menu_selection == 1) {
                        game_state.move_mode = 1;
                        game_state.move_origin_x = game_state.show_menu_x;
                        game_state.move_origin_y = game_state.show_menu_y;
                        game_state.show_menu_x = -1;
                        game_state.show_menu_y = -1;
                        game_state.menu_selection = -1;
                    } else if (game_state.menu_selection == 2) {
                        game_state.show_inventory_x = game_state.show_menu_x;
                        game_state.show_inventory_y = game_state.show_menu_y;
                        const char* piece_name = game_state.board[game_state.show_menu_y][game_state.show_menu_x];
                        game_state.inventory_count = read_inventory(piece_name, game_state.inventory);
                    } else if (game_state.menu_selection == 3) {
                        game_state.attack_mode = 1;
                    }
                    game_state.show_menu_x = -1;
                    game_state.show_menu_y = -1;
                    game_state.menu_selection = -1;
                } else if (game_state.move_mode) {
                    int tile_x = game_state.selected_x;
                    int tile_y = game_state.selected_y;
                    int dx = abs(tile_x - game_state.move_origin_x);
                    int dy = abs(tile_y - game_state.move_origin_y);
                    if (dx <= MOVE_RANGE && dy <= MOVE_RANGE && game_state.board[tile_y][tile_x] == NULL) {
                        const char* piece_name = game_state.board[game_state.move_origin_y][game_state.move_origin_x];
                        Piece piece = read_piece_stats(piece_name);
                        write_piece_stats(piece_name, piece);

                        game_state.board[tile_y][tile_x] = game_state.board[game_state.move_origin_y][game_state.move_origin_x];
                        game_state.player_board[tile_y][tile_x] = game_state.player_board[game_state.move_origin_y][game_state.move_origin_x];
                        game_state.board[game_state.move_origin_y][game_state.move_origin_x] = NULL;
                        game_state.player_board[game_state.move_origin_y][game_state.move_origin_x] = 0;

                        game_state.current_turn = 2;
                        game_state.selected_x = -1;
                        game_state.selected_y = -1;
                        game_state.move_mode = 0;
                        game_state.move_origin_x = -1;
                        game_state.move_origin_y = -1;
                        game_state.menu_selection = -1;
                    }
                } else if (game_state.selected_x != -1 && game_state.selected_y != -1 &&
                           game_state.board[game_state.selected_y][game_state.selected_x] != NULL &&
                           game_state.player_board[game_state.selected_y][game_state.selected_x] == game_state.current_turn) {
                    game_state.show_menu_x = game_state.selected_x;
                    game_state.show_menu_y = game_state.selected_y;
                    game_state.menu_selection = 0;
                }
                break;
        }
        glutPostRedisplay();
    }
}

void special(int key, int x, int y) {
    if (!game_state.game_over && game_state.current_turn == 1) {
        if (game_state.show_menu_x != -1 && game_state.show_menu_y != -1) {
            switch (key) {
                case GLUT_KEY_UP:
                    if (game_state.menu_selection > 0) {
                        game_state.menu_selection--;
                    }
                    break;
                case GLUT_KEY_DOWN:
                    if (game_state.menu_selection < 3) {
                        game_state.menu_selection++;
                    }
                    break;
            }
        } else {
            int new_x = game_state.selected_x;
            int new_y = game_state.selected_y;

            switch (key) {
                case GLUT_KEY_UP:
                    if (new_y > 0) new_y--;
                    break;
                case GLUT_KEY_DOWN:
                    if (new_y < BOARD_SIZE - 1) new_y++;
                    break;
                case GLUT_KEY_LEFT:
                    if (new_x > 0) new_x--;
                    break;
                case GLUT_KEY_RIGHT:
                    if (new_x < BOARD_SIZE - 1) new_x++;
                    break;
            }

            if (new_x != game_state.selected_x || new_y != game_state.selected_y) {
                game_state.selected_x = new_x;
                game_state.selected_y = new_y;
                game_state.menu_selection = -1;
            }
        }
        glutPostRedisplay();
    }
}

void update(int value) {
    if (!game_state.game_over) {
        int current_time = glutGet(GLUT_ELAPSED_TIME);
        float delta_time = (float)(current_time - last_time) / 1000.0f;
        last_time = current_time;

        if (game_state.message_timer > 0) {
            game_state.message_timer -= delta_time;
        }

        if (game_state.current_turn == 1) {
            game_state.player1_timer -= delta_time;
        } else {
            game_state.player2_timer -= delta_time;
        }

        if (game_state.player1_timer <= 0) {
            game_state.game_over = 1;
            game_state.winner = 2;
        }
        if (game_state.player2_timer <= 0) {
            game_state.game_over = 1;
            game_state.winner = 1;
        }

        if (game_state.current_turn == 2) {
            handle_ai_turn();
        }
    }
    glutTimerFunc(100, update, 0);
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("OpenGL Tactical Game");

    init_assets();
    init_game();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutTimerFunc(100, update, 0);

    glClearColor(0.0, 0.0, 0.0, 1.0);

    glutMainLoop();

    return 0;
}
