#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#include <math.h>

#define BOARD_SIZE 8
#define MAX_INVENTORY 5
#define MAX_HP 100
#define MAX_LAYERS 10

// FreeType globals
FT_Library ft;
FT_Face face;
int freetype_initialized = 0; // Flag to track FreeType status

// Item type enum
typedef enum { ITEM_TYPE_POTION, ITEM_TYPE_WEAPON } ItemType;

// Item structure
typedef struct {
    char name[50];
    ItemType type;
    int value;
} Item;

// Piece state
typedef struct {
    int player;
    int hp;
    int attack;
    int defense;
    int x, y, z;
    char symbol[5]; // Emoji for the piece
} Piece;

// Game state
typedef struct {
    char* board[BOARD_SIZE][BOARD_SIZE][BOARD_SIZE]; // piece names
    int player_board[BOARD_SIZE][BOARD_SIZE][BOARD_SIZE]; // 1 or 2, or 0 if empty
    int current_turn; // 1 or 2
    int cursor_x, cursor_y, cursor_z; // cursor position
    int selected_x, selected_y, selected_z; // selected piece
    int in_menu; // showing action menu
    int menu_selection; // 0=Stats, 1=Move, 2=Items, 3=Attack
    int show_inventory; // showing inventory
    int inventory_count;
    Item inventory[MAX_INVENTORY];
    double player1_timer;
    double player2_timer;
    int game_over;
    int winner;
    char message[100]; // temporary message
    time_t message_expire; // when to clear message
} GameState;

GameState game_state;
int window_width = 800, window_height = 600;
float camera_angle_x = 45.0f, camera_angle_y = 30.0f;
float camera_distance = 15.0f;
float camera_height = 0.0f; // Added for Z/X vertical movement

// Function declarations
void read_board_state();
void init_freetype();
void render_emoji(unsigned int codepoint, float x, float y, float z, float size);
unsigned int get_emoji_codepoint(const char* emoji);
void draw_cube(float x, float y, float z, const char* symbol);
void draw_board();
void draw_cursor();
void draw_hud();
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void animate();
void init();
void cleanup();

// Simple hash function for color generation
unsigned int simple_hash(const char* str) {
    unsigned int hash = 0;
    while (*str) {
        hash = (hash * 31) + (*str++);
    }
    return hash;
}

// Log to file for debugging
void log_to_file(const char* message) {
    FILE* fp = fopen("game_log.txt", "a");
    if (fp) {
        time_t now = time(NULL);
        char* timestamp = ctime(&now);
        timestamp[strcspn(timestamp, "\n")] = '\0';
        fprintf(fp, "[%s] %s\n", timestamp, message);
        fclose(fp);
    }
}

// Initialize FreeType for emoji rendering
void init_freetype() {
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/noto/NotoColorEmoji.ttf",
        "/usr/local/share/fonts/NotoColorEmoji.ttf",
        NULL
    };
    int i = 0;
    FT_Error err;

    err = FT_Init_FreeType(&ft);
    if (err) {
        log_to_file("Error: Could not initialize FreeType");
        return;
    }

    // Try loading font from multiple paths
    while (font_paths[i]) {
        err = FT_New_Face(ft, font_paths[i], 0, &face);
        if (!err) break;
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Could not load font at %s, error code: %d", font_paths[i], err);
        log_to_file(msg);
        i++;
    }

    if (!font_paths[i]) {
        log_to_file("Error: Could not load any emoji font, disabling emoji rendering");
        FT_Done_FreeType(ft);
        return;
    }

    // Check if font is scalable or has fixed sizes
    if (FT_IS_SCALABLE(face)) {
        err = FT_Set_Pixel_Sizes(face, 0, 24);
        if (err) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Could not set pixel size to 24, error code: %d", err);
            log_to_file(msg);
            FT_Done_Face(face);
            FT_Done_FreeType(ft);
            return;
        }
    } else if (face->num_fixed_sizes > 0) {
        err = FT_Select_Size(face, 0); // Select first available fixed size
        if (err) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Could not select fixed size, error code: %d", err);
            log_to_file(msg);
            FT_Done_Face(face);
            FT_Done_FreeType(ft);
            return;
        }
    } else {
        log_to_file("Error: Font has no scalable or fixed sizes, disabling emoji rendering");
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return;
    }

    freetype_initialized = 1;
    char msg[256];
    snprintf(msg, sizeof(msg), "FreeType initialized with font: %s", font_paths[i]);
    log_to_file(msg);
}

// Cleanup FreeType resources
void cleanup() {
    if (freetype_initialized) {
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        freetype_initialized = 0;
    }
}

// Convert emoji string to Unicode codepoint
unsigned int get_emoji_codepoint(const char* emoji) {
    unsigned int codepoint = 0;
    unsigned char c = emoji[0];
    int bytes = 0;
    if (c < 0x80) {
        codepoint = c;
        bytes = 1;
    } else if ((c & 0xE0) == 0xC0) {
        codepoint = c & 0x1F;
        bytes = 2;
    } else if ((c & 0xF0) == 0xE0) {
        codepoint = c & 0x0F;
        bytes = 3;
    } else if ((c & 0xF8) == 0xF0) {
        codepoint = c & 0x07;
        bytes = 4;
    }
    for (int i = 1; i < bytes; i++) {
        if ((emoji[i] & 0xC0) != 0x80) return 0x1F300; // Fallback emoji
        codepoint = (codepoint << 6) | (emoji[i] & 0x3F);
    }
    return (codepoint >= 0x1F300 && codepoint <= 0x1FAD6) ? codepoint : 0x1F300;
}

// Render emoji on cube face
void render_emoji(unsigned int codepoint, float x, float y, float z, float size) {
    if (!freetype_initialized) return; // Skip if FreeType failed

    if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Could not load glyph for codepoint U+%04X", codepoint);
        log_to_file(msg);
        return;
    }
    FT_GlyphSlot slot = face->glyph;
    if (!slot->bitmap.buffer) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: No bitmap for glyph U+%04X", codepoint);
        log_to_file(msg);
        return;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float s = size * 0.5f;
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex3f(x - s, y - s, z + s + 0.01);
    glTexCoord2f(1.0, 1.0); glVertex3f(x + s, y - s, z + s + 0.01);
    glTexCoord2f(1.0, 0.0); glVertex3f(x + s, y + s, z + s + 0.01);
    glTexCoord2f(0.0, 0.0); glVertex3f(x - s, y + s, z + s + 0.01);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &texture);
}

// Draw cube with layered colors and emoji
void draw_cube(float x, float y, float z, const char* symbol) {
    unsigned int hash = simple_hash(symbol);
    srand(hash);
    float base_size = 0.5f;
    for (int i = 0; i < MAX_LAYERS; i++) {
        float r = (rand() % 256) / 255.0f;
        float g = (rand() % 256) / 255.0f;
        float b = (rand() % 256) / 255.0f;
        glColor3f(r, g, b);
        float size = base_size * (1.0f - 0.05f * i);
        glPushMatrix();
        glTranslatef(x, y, z);
        glutSolidCube(size);
        glPopMatrix();
    }
    if (freetype_initialized) {
        unsigned int codepoint = get_emoji_codepoint(symbol);
        render_emoji(codepoint, x, y, z, base_size);
    }
}

// Draw the 3D board
void draw_board() {
    glEnable(GL_DEPTH_TEST);
    glPushMatrix();
    for (int z = 0; z < BOARD_SIZE; z++) {
        for (int y = 0; y < BOARD_SIZE; y++) {
            for (int x = 0; x < BOARD_SIZE; x++) {
                if (game_state.board[z][y][x]) {
                    float pos_x = x + 0.5f;
                    float pos_y = y + 0.5f;
                    float pos_z = z + 0.5f;
                    draw_cube(pos_x, pos_y, pos_z, game_state.board[z][y][x]);
                }
            }
        }
    }
    // Draw grid
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINES);
    for (int i = 0; i <= BOARD_SIZE; i++) {
        for (int z = 0; z <= BOARD_SIZE; z++) {
            glVertex3f(i, 0, z); glVertex3f(i, BOARD_SIZE, z);
            glVertex3f(0, i, z); glVertex3f(BOARD_SIZE, i, z);
        }
        for (int y = 0; y <= BOARD_SIZE; y++) {
            glVertex3f(0, y, i); glVertex3f(BOARD_SIZE, y, i);
        }
    }
    glEnd();
    glPopMatrix();
}

// Draw cursor
void draw_cursor() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 0.0f, 0.5f);
    glPushMatrix();
    glTranslatef(game_state.cursor_x + 0.5f, game_state.cursor_y + 0.5f, game_state.cursor_z + 0.5f);
    glutWireCube(0.6f);
    glPopMatrix();
    glDisable(GL_BLEND);
}

// Draw HUD (text overlay)
void draw_hud() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    char text[256];
    snprintf(text, sizeof(text), "Turn: Player %d | P1: %.0fs | P2: %.0fs", 
             game_state.current_turn, game_state.player1_timer, game_state.player2_timer);
    glRasterPos2f(10, window_height - 20);
    for (const char* c = text; *c; c++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);

    if (game_state.message[0]) {
        time_t now = time(NULL);
        if (now < game_state.message_expire) {
            glRasterPos2f(10, window_height - 40);
            for (const char* c = game_state.message; *c; c++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
        } else {
            game_state.message[0] = '\0';
        }
    }

    if (game_state.game_over) {
        snprintf(text, sizeof(text), "GAME OVER! Player %d wins!", game_state.winner);
        glRasterPos2f(10, window_height - 60);
        for (const char* c = text; *c; c++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
    }

    if (game_state.in_menu) {
        const char* opts[] = {"Stats", "Move", "Items", "Attack"};
        glRasterPos2f(10, window_height - 80);
        for (int i = 0; i < 4; i++) {
            snprintf(text, sizeof(text), "%s%s%s ", i == game_state.menu_selection ? "[" : " ", opts[i], i == game_state.menu_selection ? "]" : "");
            for (const char* c = text; *c; c++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
        }
    }

    if (game_state.show_inventory) {
        glRasterPos2f(10, window_height - 100);
        const char* inv = "Inventory:";
        for (const char* c = inv; *c; c++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
        for (int i = 0; i < game_state.inventory_count; i++) {
            snprintf(text, sizeof(text), "  * %s (%s %+d)", 
                     game_state.inventory[i].name,
                     game_state.inventory[i].type == ITEM_TYPE_POTION ? "heals" : "dmg",
                     game_state.inventory[i].value);
            glRasterPos2f(10, window_height - 120 - i * 20);
            for (const char* c = text; *c; c++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
        }
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}

// Read board state from board_state.txt
void read_board_state() {
    FILE* f = fopen("board_state.txt", "r");
    if (!f) {
        log_to_file("Error: board_state.txt not found!");
        exit(1);
    }

    char line[256];
    int z = 0;
    while (fgets(line, sizeof(line), f) && z < BOARD_SIZE) {
        char* token = strtok(line, " \n");
        int y = 0;
        while (token && y < BOARD_SIZE) {
            char* subtoken = strtok(token, ",");
            int x = 0;
            while (subtoken && x < BOARD_SIZE) {
                if (strcmp(subtoken, "none") != 0) {
                    game_state.board[z][y][x] = strdup(subtoken);
                    game_state.player_board[z][y][x] = (z < 2) ? 1 : 2;
                    char filename[100];
                    sprintf(filename, "%s.dir/state.txt", subtoken);
                    FILE* sf = fopen(filename, "r");
                    if (sf) {
                        Piece piece;
                        fscanf(sf, "player:%d\nhp:%d\nattack:%d\ndefense:%d\nx:%d\ny:%d\nz:%d\nsymbol:%s\n",
                               &piece.player, &piece.hp, &piece.attack, &piece.defense, 
                               &piece.x, &piece.y, &piece.z, piece.symbol);
                        fclose(sf);
                        if (piece.symbol[0]) {
                            free(game_state.board[z][y][x]);
                            game_state.board[z][y][x] = strdup(piece.symbol);
                        }
                    }
                } else {
                    game_state.board[z][y][x] = NULL;
                    game_state.player_board[z][y][x] = 0;
                }
                subtoken = strtok(NULL, ",");
                x++;
            }
            token = strtok(NULL, " \n");
            y++;
        }
        z++;
    }
    fclose(f);
}

// Read inventory
int read_inventory(const char* piece_name, Item inventory[]) {
    char piece_path[100];
    sprintf(piece_path, "%s.dir", piece_name);
    DIR* d = opendir(piece_path);
    if (!d) return 0;

    struct dirent* dir;
    int count = 0;
    while ((dir = readdir(d)) != NULL && count < MAX_INVENTORY) {
        if (strstr(dir->d_name, ".dir") != NULL) {
            char* item_dir_name = strdup(dir->d_name);
            char* base_name = strtok(item_dir_name, ".");
            if (base_name) {
                char item_path[150];
                sprintf(item_path, "%s.dir/%s.dir/stats.txt", piece_name, base_name);
                FILE* f = fopen(item_path, "r");
                if (f) {
                    char type_str[20];
                    int value;
                    fscanf(f, "type:%s\n", type_str);
                    fscanf(f, "value:%d\n", &value);
                    strcpy(inventory[count].name, base_name);
                    inventory[count].type = (strcmp(type_str, "potion") == 0) ? ITEM_TYPE_POTION : ITEM_TYPE_WEAPON;
                    inventory[count].value = value;
                    count++;
                    fclose(f);
                }
            }
            free(item_dir_name);
        }
    }
    closedir(d);
    return count;
}

// Display function
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float rad_x = camera_angle_x * 3.14159f / 180.0f;
    float rad_y = camera_angle_y * 3.14159f / 180.0f;
    float cam_x = camera_distance * cos(rad_y) * cos(rad_x);
    float cam_y = camera_distance * cos(rad_y) * sin(rad_x);
    float cam_z = camera_distance * sin(rad_y) + camera_height;
    gluLookAt(cam_x + BOARD_SIZE/2.0f, cam_y + BOARD_SIZE/2.0f, cam_z + BOARD_SIZE/2.0f,
              BOARD_SIZE/2.0f, BOARD_SIZE/2.0f, BOARD_SIZE/2.0f,
              0, 0, 1);

    draw_board();
    draw_cursor();
    draw_hud();

    glutSwapBuffers();
}

// Reshape function
void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / h, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

// Keyboard input
void keyboard(unsigned char key, int x, int y) {
    if (game_state.game_over) return;

    // Camera controls
    switch (key) {
        case 'w': case 'W':
            camera_distance -= 0.5f;
            if (camera_distance < 5.0f) camera_distance = 5.0f; // Prevent too close
            break;
        case 's': case 'S':
            camera_distance += 0.5f;
            if (camera_distance > 30.0f) camera_distance = 30.0f; // Prevent too far
            break;
        case 'a': case 'A':
            camera_angle_x -= 5.0f;
            if (camera_angle_x < 0) camera_angle_x += 360.0f;
            break;
        case 'd': case 'D':
            camera_angle_x += 5.0f;
            if (camera_angle_x >= 360.0f) camera_angle_x -= 360.0f;
            break;
        case 'q': case 'Q':
            camera_angle_y += 5.0f;
            if (camera_angle_y > 80.0f) camera_angle_y = 80.0f; // Prevent flipping
            break;
        case 'e': case 'E':
            camera_angle_y -= 5.0f;
            if (camera_angle_y < -80.0f) camera_angle_y = -80.0f;
            break;
        case 'z': case 'Z':
            camera_height += 0.5f;
            if (camera_height > 10.0f) camera_height = 10.0f;
            break;
        case 'x': case 'X':
            camera_height -= 0.5f;
            if (camera_height < -10.0f) camera_height = -10.0f;
            break;
        // Cursor z-movement
        case 'c': case 'C':
            if (!game_state.in_menu && game_state.cursor_z < BOARD_SIZE - 1) {
                game_state.cursor_z++;
            }
            break;
        case 'v': case 'V':
            if (!game_state.in_menu && game_state.cursor_z > 0) {
                game_state.cursor_z--;
            }
            break;
        case 27: // ESC
            if (game_state.selected_x != -1 || game_state.selected_y != -1 || game_state.selected_z != -1) {
                game_state.selected_x = game_state.selected_y = game_state.selected_z = -1;
                sprintf(game_state.message, "Selection cancelled");
                game_state.message_expire = time(NULL) + 1;
            } else if (game_state.in_menu) {
                game_state.in_menu = 0;
            } else if (game_state.show_inventory) {
                game_state.show_inventory = 0;
            }
            break;
        case '\n': // Enter
            if (game_state.in_menu) {
                if (game_state.menu_selection == 2) { // Items
                    game_state.show_inventory = !game_state.show_inventory;
                    if (game_state.show_inventory) {
                        game_state.inventory_count = read_inventory(
                            game_state.board[game_state.selected_z][game_state.selected_y][game_state.selected_x],
                            game_state.inventory);
                    }
                } else if (game_state.menu_selection == 1) { // Move
                    if (game_state.board[game_state.cursor_z][game_state.cursor_y][game_state.cursor_x] == NULL) {
                        int from_x = game_state.selected_x;
                        int from_y = game_state.selected_y;
                        int from_z = game_state.selected_z;
                        int to_x = game_state.cursor_x;
                        int to_y = game_state.cursor_y;
                        int to_z = game_state.cursor_z;

                        const char* piece_name = game_state.board[from_z][from_y][from_x];
                        game_state.board[to_z][to_y][to_x] = game_state.board[from_z][from_y][from_x];
                        game_state.player_board[to_z][to_y][to_x] = game_state.player_board[from_z][from_y][from_x];
                        game_state.board[from_z][from_y][from_x] = NULL;
                        game_state.player_board[from_z][from_y][from_x] = 0;

                        char filename[100];
                        sprintf(filename, "%s.dir/state.txt", piece_name);
                        FILE* f = fopen(filename, "r");
                        Piece piece = {0};
                        if (f) {
                            fscanf(f, "player:%d\nhp:%d\nattack:%d\ndefense:%d\nx:%d\ny:%d\nz:%d\nsymbol:%s\n",
                                   &piece.player, &piece.hp, &piece.attack, &piece.defense, 
                                   &piece.x, &piece.y, &piece.z, piece.symbol);
                            fclose(f);
                        }
                        piece.x = to_x;
                        piece.y = to_y;
                        piece.z = to_z;

                        f = fopen(filename, "w");
                        if (f) {
                            fprintf(f, "player:%d\nhp:%d\nattack:%d\ndefense:%d\nx:%d\ny:%d\nz:%d\nsymbol:%s\n",
                                    piece.player, piece.hp, piece.attack, piece.defense, 
                                    piece.x, piece.y, piece.z, piece.symbol);
                            fclose(f);
                        }

                        sprintf(game_state.message, "Moved %s to %c%d%d", piece_name, 'A' + to_y, to_x, to_z);
                        game_state.message_expire = time(NULL) + 2;
                        game_state.current_turn = (game_state.current_turn == 1) ? 2 : 1;
                        game_state.selected_x = game_state.selected_y = game_state.selected_z = -1;
                        game_state.in_menu = 0;
                    } else {
                        sprintf(game_state.message, "Target occupied!");
                        game_state.message_expire = time(NULL) + 2;
                    }
                }
            } else if (game_state.selected_x == -1 && game_state.selected_y == -1 && game_state.selected_z == -1) {
                if (game_state.board[game_state.cursor_z][game_state.cursor_y][game_state.cursor_x] != NULL &&
                    game_state.player_board[game_state.cursor_z][game_state.cursor_y][game_state.cursor_x] == game_state.current_turn) {
                    game_state.selected_x = game_state.cursor_x;
                    game_state.selected_y = game_state.cursor_y;
                    game_state.selected_z = game_state.cursor_z;
                    game_state.in_menu = 1;
                    game_state.menu_selection = 0;
                    sprintf(game_state.message, "Selected %s", game_state.board[game_state.cursor_z][game_state.cursor_y][game_state.cursor_x]);
                    game_state.message_expire = time(NULL) + 2;
                } else {
                    sprintf(game_state.message, "Can't select that piece");
                    game_state.message_expire = time(NULL) + 2;
                }
            }
            break;
    }
    glutPostRedisplay();
}

// Special keys for cursor movement
void special(int key, int x, int y) {
    if (game_state.game_over) return;
    if (game_state.in_menu) {
        if (key == GLUT_KEY_LEFT && game_state.menu_selection > 0) {
            game_state.menu_selection--;
        } else if (key == GLUT_KEY_RIGHT && game_state.menu_selection < 3) {
            game_state.menu_selection++;
        }
    } else {
        if (key == GLUT_KEY_LEFT && game_state.cursor_x > 0) {
            game_state.cursor_x--;
        } else if (key == GLUT_KEY_RIGHT && game_state.cursor_x < BOARD_SIZE - 1) {
            game_state.cursor_x++;
        } else if (key == GLUT_KEY_UP && game_state.cursor_y > 0) {
            game_state.cursor_y--;
        } else if (key == GLUT_KEY_DOWN && game_state.cursor_y < BOARD_SIZE - 1) {
            game_state.cursor_y++;
        }
    }
    glutPostRedisplay();
}

// Animation loop
void animate() {
    static struct timeval last_time = {0};
    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    double elapsed = (curr_time.tv_sec - last_time.tv_sec) * 1000.0 +
                     (curr_time.tv_usec - last_time.tv_usec) / 1000.0;
    if (elapsed >= 1000.0 / 60.0) {
        if (game_state.current_turn == 1) {
            game_state.player1_timer -= 0.0167;
            if (game_state.player1_timer <= 0) {
                game_state.game_over = 1;
                game_state.winner = 2;
            }
        } else {
            game_state.player2_timer -= 0.0167;
            if (game_state.player2_timer <= 0) {
                game_state.game_over = 1;
                game_state.winner = 1;
            }
        }
        glutPostRedisplay();
        last_time = curr_time;
    }
}

// Initialize OpenGL
void init() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    init_freetype();
}

// Main function
int main(int argc, char* argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("3D Chess Game");
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(animate);
    init();

    memset(&game_state, 0, sizeof(game_state));
    game_state.current_turn = 1;
    game_state.cursor_x = game_state.cursor_y = game_state.cursor_z = 0;
    game_state.selected_x = game_state.selected_y = game_state.selected_z = -1;
    game_state.player1_timer = 300;
    game_state.player2_timer = 300;
    game_state.message[0] = '\0';
    read_board_state();

    glutMainLoop();

    cleanup();
    for (int z = 0; z < BOARD_SIZE; z++) {
        for (int y = 0; y < BOARD_SIZE; y++) {
            for (int x = 0; x < BOARD_SIZE; x++) {
                if (game_state.board[z][y][x]) free(game_state.board[z][y][x]);
            }
        }
    }
    return 0;
}
