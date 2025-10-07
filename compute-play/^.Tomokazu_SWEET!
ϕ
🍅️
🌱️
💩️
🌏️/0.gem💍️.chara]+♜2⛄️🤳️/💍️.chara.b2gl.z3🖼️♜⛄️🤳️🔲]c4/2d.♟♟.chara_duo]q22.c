#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <locale.h>

#define BOARD_SIZE 8
#define MAX_INVENTORY 5
#define MAX_HP 100

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
} Piece;

// Game state
typedef struct {
    char* board[BOARD_SIZE][BOARD_SIZE];     // piece names
    int player_board[BOARD_SIZE][BOARD_SIZE]; // 1 or 2, or 0 if empty
    int current_turn;                         // 1 or 2
    int cursor_x, cursor_y;                   // cursor position
    int selected_x, selected_y;               // selected piece
    int in_menu;                              // showing action menu
    int menu_selection;                       // 0=Stats, 1=Move, 2=Items, 3=Attack
    int show_inventory;                       // showing inventory
    int inventory_count;
    Item inventory[MAX_INVENTORY];
    double player1_timer;
    double player2_timer;
    int game_over;
    int winner;
    char message[100];                        // temporary message
    time_t message_expire;                    // when to clear message
} GameState;

GameState game_state;

// FreeType globals
FT_Library ft;
FT_Face face; // For text
FT_Face emoji_face; // For emojis
float emoji_scale;
float font_size = 16.0f;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};

// Window dimensions
int window_width = 800;
int window_height = 600;
float cell_size = 60.0f; // Size of each board cell
float board_offset_x = 100.0f;
float board_offset_y = 50.0f;

// Terminal input
struct termios orig_termios;

// Function declarations
void initFreeType();
void render_text(const char* str, float x, float y);
void render_emoji(unsigned int codepoint, float x, float y);
float get_char_width(unsigned int codepoint);
float get_string_width(const char* str, int bytes_to_measure);
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void read_board_state();
char* get_emoji_for_piece(const char* piece_name);
void ensure_emoji_in_state(const char* piece_name);
int read_inventory(const char* piece_name, Item inventory[]);
void display();
void display_cli();
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void mouse(int button, int state, int x, int y);
void reshape(int w, int h);
void idle();
void handle_input(int key);
void set_status_message(const char* msg, int duration_seconds);
void init_terminal();
void reset_terminal();

// Initialize terminal for non-blocking input
void init_terminal() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

// Reset terminal settings
void reset_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// FreeType initialization
void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        exit(1);
    }

    // Load text face
    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
    FT_Error err = FT_New_Face(ft, font_path, 0, &face);
    if (err) {
        font_path = "/usr/share/fonts/liberation/LiberationMono-Regular.ttf";
        err = FT_New_Face(ft, font_path, 0, &face);
        if (err) {
            fprintf(stderr, "Could not open text font. Please check paths.\n");
            exit(1);
        }
    }
    if (FT_Set_Pixel_Sizes(face, 0, font_size)) {
        fprintf(stderr, "Could not set pixel size for text font\n");
        exit(1);
    }

    // Load emoji face
    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error: Could not load emoji font at %s, error code: %d\n", emoji_font_path, err);
        emoji_face = NULL;
        return;
    }
    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, 24);
        if (err) {
            fprintf(stderr, "Error: Could not set pixel size to 24 for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            return;
        }
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
        if (err) {
            fprintf(stderr, "Error: Could not select size 0 for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            return;
        }
    } else {
        fprintf(stderr, "Error: No fixed sizes available in emoji font\n");
        FT_Done_Face(emoji_face);
        emoji_face = NULL;
        return;
    }

    // Calculate emoji scale
    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = font_size / (float)loaded_emoji_size;
}

// UTF-8 decoding
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0) {
        if ((str[1] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            return 2;
        }
    }
    if ((str[0] & 0xF0) == 0xE0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            return 3;
        }
    }
    if ((str[0] & 0xF8) == 0xF0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            return 4;
        }
    }
    *codepoint = '?';
    return 1;
}

// Character width calculation
float get_char_width(unsigned int codepoint) {
    int is_emoji = (codepoint >= 0x1F300 && codepoint <= 0x1FAD6);
    FT_Face current_face = is_emoji ? emoji_face : face;
    if (!current_face) return font_size;
    if (FT_Load_Char(current_face, codepoint, is_emoji ? (FT_LOAD_RENDER | FT_LOAD_COLOR) : FT_LOAD_RENDER)) {
        return font_size;
    }
    if (is_emoji) {
        return (emoji_face->glyph->advance.x >> 6) * emoji_scale;
    } else {
        return (face->glyph->advance.x >> 6);
    }
}

// String width calculation
float get_string_width(const char* str, int bytes_to_measure) {
    float total_width = 0;
    const unsigned char* p = (const unsigned char*)str;
    const unsigned char* end = p + bytes_to_measure;
    while (p < end) {
        unsigned int codepoint;
        int bytes = decode_utf8(p, &codepoint);
        if (p + bytes > end) break;
        total_width += get_char_width(codepoint);
        p += bytes;
    }
    return total_width;
}

// Render emoji for GLUT
void render_emoji(unsigned int codepoint, float x, float y) {
    if (!emoji_face) return;
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) return;

    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer || slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) return;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float scale_factor = emoji_scale;
    float w = slot->bitmap.width * scale_factor;
    float h = slot->bitmap.rows * scale_factor;
    float x2 = x + slot->bitmap_left * scale_factor;
    float y2 = y - (slot->bitmap.rows - slot->bitmap_top) * scale_factor;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex2f(x2, y2);
    glTexCoord2f(1.0, 1.0); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0, 0.0); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0, 0.0); glVertex2f(x2, y2 + h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &texture);
}

// Render text for GLUT
void render_text(const char* str, float x, float y) {
    const unsigned char* p = (const unsigned char*)str;
    while (*p) {
        unsigned int codepoint;
        int bytes = decode_utf8(p, &codepoint);

        if (codepoint >= 0x1F300 && codepoint <= 0x1FAD6 && emoji_face) {
            render_emoji(codepoint, x, y);
            x += get_char_width(codepoint);
        } else {
            if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
                p += bytes;
                continue;
            }

            FT_GlyphSlot slot = face->glyph;

            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, slot->bitmap.width, slot->bitmap.rows, 0, GL_ALPHA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);

            float x2 = x + slot->bitmap_left;
            float y2 = y - (slot->bitmap.rows - slot->bitmap_top);
            float w = slot->bitmap.width;
            float h = slot->bitmap.rows;

            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor3fv(font_color);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(x2, y2);
            glTexCoord2f(1, 1); glVertex2f(x2 + w, y2);
            glTexCoord2f(1, 0); glVertex2f(x2 + w, y2 + h);
            glTexCoord2f(0, 0); glVertex2f(x2, y2 + h);
            glEnd();
            glDisable(GL_TEXTURE_2D);
            glDeleteTextures(1, &texture);

            x += (slot->advance.x >> 6);
        }
        p += bytes;
    }
}

// Draw rectangle for GLUT
void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// Status message
void set_status_message(const char* msg, int duration_seconds) {
    strncpy(game_state.message, msg, sizeof(game_state.message) - 1);
    game_state.message[sizeof(game_state.message) - 1] = '\0';
    game_state.message_expire = time(NULL) + duration_seconds;
}

// Read emoji from state.txt
char* get_emoji_for_piece(const char* piece_name) {
    static char emoji[5] = "❓";
    char filename[100];
    sprintf(filename, "%s.dir/state.txt", piece_name);

    FILE* f = fopen(filename, "r");
    if (!f) return emoji;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "symbol :", 8) == 0) {
            char* em = line + 8;
            while (*em == ' ') em++;
            int len = 0;
            while (em[len] && em[len] != '\n' && em[len] != '\r') len++;

            if (len >= 1 && len <= 4) {
                strncpy(emoji, em, len);
                emoji[len] = '\0';
                fclose(f);
                return emoji;
            }
            if (len > 5 && (strstr(em, "0x") || strstr(em, "U000") || em[0] == '\\' || em[0] == 'U')) {
                fclose(f);
                return emoji;
            }
        }
    }
    fclose(f);
    return emoji;
}

// Ensure emoji in state.txt
void ensure_emoji_in_state(const char* piece_name) {
    char filename[100];
    sprintf(filename, "%s.dir/state.txt", piece_name);

    FILE* f = fopen(filename, "r");
    if (!f) return;

    char temp[1000] = {0};
    char line[256];
    int has_symbol = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "symbol :", 8) == 0) {
            has_symbol = 1;
        } else {
            strcat(temp, line);
        }
    }
    fclose(f);

    if (!has_symbol) {
        strcat(temp, "symbol : 🎯\n");
        f = fopen(filename, "w");
        if (f) {
            fprintf(f, "%s", temp);
            fclose(f);
        }
    }
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

// Load board from board_state.txt
void read_board_state() {
    FILE* f = fopen("board_state.txt", "r");
    if (!f) {
        fprintf(stderr, "Error: board_state.txt not found!\n");
        exit(1);
    }

    char line[256];
    int row = 0;
    while (fgets(line, sizeof(line), f) && row < BOARD_SIZE) {
        char* token = strtok(line, " \n");
        int col = 0;
        while (token && col < BOARD_SIZE) {
            if (strcmp(token, "none") != 0) {
                game_state.board[row][col] = strdup(token);
                game_state.player_board[row][col] = (row < 2) ? 1 : 2;
                ensure_emoji_in_state(token);
            } else {
                game_state.board[row][col] = NULL;
                game_state.player_board[row][col] = 0;
            }
            token = strtok(NULL, " \n");
            col++;
        }
        row++;
    }
    fclose(f);
}

// CLI display function
void display_cli() {
    system("clear");

    // Top header: numbers
    printf(" ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("      %d", j);
    }
    printf("\n");

    // Board
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%c ", 'A' + i); // Row label

        // Top border
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("+-----");
        }
        printf("+\n");

        // Cell content
        printf("%c|", 'A' + i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            const char* emoji = "  ";
            if (game_state.board[i][j]) {
                emoji = get_emoji_for_piece(game_state.board[i][j]);
            }

            // Cursor: show [ ] around cell
            if (i == game_state.cursor_y && j == game_state.cursor_x) {
                printf("[ %s ]|", emoji);
            }
            // Selected piece
            else if (i == game_state.selected_y && j == game_state.selected_x) {
                printf("  %s*|", emoji);
            }
            // Normal cell
            else {
                printf("  %s  |", emoji);
            }
        }
        printf("\n");
    }

    // Bottom border
    printf(" ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("+-----");
    }
    printf("+\n");

    // Info panel
    printf("Turn: Player %d | P1: %.0fs | P2: %.0fs\n",
           game_state.current_turn,
           game_state.player1_timer,
           game_state.player2_timer);

    if (game_state.message[0]) {
        time_t now = time(NULL);
        if (now >= game_state.message_expire) {
            game_state.message[0] = '\0';
        } else {
            printf("Msg: %s\n", game_state.message);
        }
    }

    if (game_state.game_over) {
        printf("🏆 GAME OVER! Player %d wins! 🏆\n", game_state.winner);
    }

    // Menu
    if (game_state.in_menu) {
        const char* opts[] = {"Stats", "Move", "Items", "Attack"};
        printf("Action: ");
        for (int i = 0; i < 4; i++) {
            if (i == game_state.menu_selection) {
                printf("[%s] ", opts[i]);
            } else {
                printf(" %s  ", opts[i]);
            }
        }
        printf("\n");
    }

    // Inventory
    if (game_state.show_inventory) {
        printf("🎒 Inventory:\n");
        for (int i = 0; i < game_state.inventory_count; i++) {
            printf("  • %s (%s %+d)\n",
                   game_state.inventory[i].name,
                   game_state.inventory[i].type == ITEM_TYPE_POTION ? "heals" : "dmg",
                   game_state.inventory[i].value);
        }
    }

    printf("\n⌨️  Arrows: move | ENTER: select/place | ESC: cancel\n");
}

// GLUT display function
void display() {
    // Render CLI
    display_cli();

    // Render GLUT
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw board
    float checker_color1[3] = {0.8f, 0.8f, 0.8f};
    float checker_color2[3] = {0.4f, 0.4f, 0.4f};
    float cursor_color[3] = {0.0f, 1.0f, 0.0f};
    float selected_color[3] = {1.0f, 1.0f, 0.0f};

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            float x = board_offset_x + j * cell_size;
            float y = board_offset_y + (BOARD_SIZE - 1 - i) * cell_size;
            float color[3] = {(i + j) % 2 ? checker_color1[0] : checker_color2[0],
                             (i + j) % 2 ? checker_color1[1] : checker_color2[1],
                             (i + j) % 2 ? checker_color1[2] : checker_color2[2]};

            // Highlight cursor
            if (i == game_state.cursor_y && j == game_state.cursor_x) {
                draw_rect(x, y, cell_size, cell_size, cursor_color);
            }
            // Highlight selected
            else if (i == game_state.selected_y && j == game_state.selected_x) {
                draw_rect(x, y, cell_size, cell_size, selected_color);
            }
            else {
                draw_rect(x, y, cell_size, cell_size, color);
            }

            // Draw piece
            if (game_state.board[i][j]) {
                const char* emoji = get_emoji_for_piece(game_state.board[i][j]);
                float emoji_x = x + (cell_size - get_string_width(emoji, strlen(emoji))) / 2;
                float emoji_y = y + (cell_size - font_size) / 2 + font_size;
                render_text(emoji, emoji_x, emoji_y);
            }
        }
    }

    // Draw row labels (A-H)
    for (int i = 0; i < BOARD_SIZE; i++) {
        char label[2] = {'A' + i, '\0'};
        render_text(label, board_offset_x - 20, board_offset_y + (BOARD_SIZE - 1 - i) * cell_size + cell_size / 2 + font_size / 2);
    }

    // Draw column labels (0-7)
    for (int j = 0; j < BOARD_SIZE; j++) {
        char label[2];
        sprintf(label, "%d", j);
        render_text(label, board_offset_x + j * cell_size + cell_size / 2 - font_size / 2, board_offset_y - 20);
    }

    // Draw info panel
    char info[256];
    sprintf(info, "Turn: Player %d | P1: %.0fs | P2: %.0fs", game_state.current_turn,
            game_state.player1_timer, game_state.player2_timer);
    render_text(info, 10, window_height - 20);

    // Draw message
    if (game_state.message[0] && time(NULL) < game_state.message_expire) {
        render_text(game_state.message, 10, window_height - 40);
    }

    // Draw game over
    if (game_state.game_over) {
        char win_msg[100];
        sprintf(win_msg, "🏆 GAME OVER! Player %d wins! 🏆", game_state.winner);
        render_text(win_msg, 10, window_height - 60);
    }

    // Draw menu
    if (game_state.in_menu) {
        const char* opts[] = {"Stats", "Move", "Items", "Attack"};
        float menu_x = board_offset_x;
        float menu_y = board_offset_y - 40;
        float menu_color[3] = {0.2f, 0.2f, 0.2f};
        draw_rect(menu_x, menu_y - 20, 200, 25, menu_color);

        float x_offset = menu_x + 10;
        for (int i = 0; i < 4; i++) {
            if (i == game_state.menu_selection) {
                float sel_color[3] = {0.0f, 1.0f, 0.0f};
                draw_rect(x_offset - 5, menu_y - 15, get_string_width(opts[i], strlen(opts[i])) + 10, 15, sel_color);
            }
            render_text(opts[i], x_offset, menu_y);
            x_offset += get_string_width(opts[i], strlen(opts[i])) + 20;
        }
    }

    // Draw inventory
    if (game_state.show_inventory) {
        float inv_x = board_offset_x;
        float inv_y = board_offset_y - 60;
        float inv_color[3] = {0.2f, 0.2f, 0.2f};
        draw_rect(inv_x, inv_y - 20, 300, 20 + game_state.inventory_count * 20, inv_color);
        render_text("🎒 Inventory:", inv_x + 10, inv_y);

        for (int i = 0; i < game_state.inventory_count; i++) {
            char inv_item[100];
            sprintf(inv_item, "• %s (%s %+d)", game_state.inventory[i].name,
                    game_state.inventory[i].type == ITEM_TYPE_POTION ? "heals" : "dmg",
                    game_state.inventory[i].value);
            render_text(inv_item, inv_x + 20, inv_y - 20 * (i + 1));
        }
    }

    // Draw instructions
    render_text("Arrows: move | ENTER: select/place | ESC: cancel", 10, 10);

    glutSwapBuffers();
}

// Handle input
void handle_input(int key) {
    if (game_state.game_over) return;

    // AI Turn
    if (game_state.current_turn == 2) {
        FILE* f = fopen("board_state.txt", "w");
        if (!f) return;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                fprintf(f, "%s ", game_state.board[i][j] ? game_state.board[i][j] : "none");
            }
            fprintf(f, "\n");
        }
        fclose(f);

        system("./ai_module");

        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (game_state.board[i][j]) free(game_state.board[i][j]);
                game_state.board[i][j] = NULL;
            }
        }
        read_board_state();
        game_state.current_turn = 1;
        glutPostRedisplay();
        return;
    }

    // Handle cursor movement
    if (key == GLUT_KEY_UP) {
        if (game_state.cursor_y > 0) game_state.cursor_y--;
    }
    else if (key == GLUT_KEY_DOWN) {
        if (game_state.cursor_y < BOARD_SIZE - 1) game_state.cursor_y++;
    }
    else if (key == GLUT_KEY_LEFT) {
        if (game_state.cursor_x > 0) game_state.cursor_x--;
    }
    else if (key == GLUT_KEY_RIGHT) {
        if (game_state.cursor_x < BOARD_SIZE - 1) game_state.cursor_x++;
    }
    else if (key == '\n') {
        if (game_state.selected_x == -1 && game_state.selected_y == -1) {
            if (game_state.board[game_state.cursor_y][game_state.cursor_x] != NULL &&
                game_state.player_board[game_state.cursor_y][game_state.cursor_x] == game_state.current_turn) {
                game_state.selected_x = game_state.cursor_x;
                game_state.selected_y = game_state.cursor_y;
                sprintf(game_state.message, "Selected %s", game_state.board[game_state.cursor_y][game_state.cursor_x]);
                game_state.message_expire = time(NULL) + 2;
            } else {
                set_status_message("Can't select that piece", 2);
            }
        }
        else {
            int from_x = game_state.selected_x;
            int from_y = game_state.selected_y;
            int to_x = game_state.cursor_x;
            int to_y = game_state.cursor_y;

            if (game_state.board[to_y][to_x] == NULL) {
                const char* piece_name = game_state.board[from_y][from_x];

                game_state.board[to_y][to_x] = game_state.board[from_y][from_x];
                game_state.player_board[to_y][to_x] = game_state.player_board[from_y][from_x];
                game_state.board[from_y][from_x] = NULL;
                game_state.player_board[from_y][from_x] = 0;

                char filename[100];
                sprintf(filename, "%s.dir/state.txt", piece_name);
                FILE* f = fopen(filename, "r");
                Piece piece = {0};
                if (f) {
                    fscanf(f, "player:%d\nhp:%d\nattack:%d\ndefense:%d\nx:%d\ny:%d\nz:%d\n",
                           &piece.player, &piece.hp, &piece.attack, &piece.defense, &piece.x, &piece.y, &piece.z);
                    fclose(f);
                }
                piece.x = to_x;
                piece.y = to_y;

                f = fopen(filename, "w");
                if (f) {
                    fprintf(f, "player:%d\nhp:%d\nattack:%d\ndefense:%d\nx:%d\ny:%d\nz:%d\nsymbol : %s\n",
                            piece.player, piece.hp, piece.attack, piece.defense, piece.x, piece.y, piece.z,
                            get_emoji_for_piece(piece_name));
                    fclose(f);
                }

                sprintf(game_state.message, "Moved %s to %c%d", piece_name, 'A' + to_y, to_x);
                game_state.message_expire = time(NULL) + 2;

                game_state.current_turn = 2;
                game_state.selected_x = -1;
                game_state.selected_y = -1;
            } else {
                set_status_message("Target occupied!", 2);
            }
        }
    }
    else if (key == 27) { // ESC
        if (game_state.selected_x != -1 || game_state.selected_y != -1) {
            game_state.selected_x = -1;
            game_state.selected_y = -1;
            set_status_message("Selection cancelled", 1);
        }
    }
    glutPostRedisplay();
}

// GLUT callbacks
void keyboard(unsigned char key, int x, int y) {
    int mapped_key = key;
    if (key == 27) mapped_key = 27; // ESC
    else if (key == 13) mapped_key = '\n'; // Enter
    handle_input(mapped_key);
}

void special(int key, int x, int y) {
    handle_input(key); // Directly pass GLUT special keys
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        float gl_x = x;
        float gl_y = window_height - y;
        int board_x = (int)((gl_x - board_offset_x) / cell_size);
        int board_y = BOARD_SIZE - 1 - (int)((gl_y - board_offset_y) / cell_size);
        if (board_x >= 0 && board_x < BOARD_SIZE && board_y >= 0 && board_y < BOARD_SIZE) {
            game_state.cursor_x = board_x;
            game_state.cursor_y = board_y;
            handle_input('\n');
        }
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
}

void idle() {
    // Check for CLI input
    char buf[3];
    int n = read(STDIN_FILENO, buf, 3);
    if (n > 0) {
        if (n == 1) {
            int key = buf[0];
            if (key == '\n' || key == 27) {
                handle_input(key);
            }
        } else if (n == 3 && buf[0] == 27 && buf[1] == '[') {
            // Arrow key escape sequences
            switch (buf[2]) {
                case 'A': handle_input(GLUT_KEY_UP); break;    // Up
                case 'B': handle_input(GLUT_KEY_DOWN); break;  // Down
                case 'C': handle_input(GLUT_KEY_RIGHT); break; // Right
                case 'D': handle_input(GLUT_KEY_LEFT); break;  // Left
            }
        }
    }

    // Update timers
    if (game_state.current_turn == 1) {
        game_state.player1_timer -= 0.1;
        if (game_state.player1_timer <= 0) {
            game_state.game_over = 1;
            game_state.winner = 2;
        }
    } else {
        game_state.player2_timer -= 0.1;
        if (game_state.player2_timer <= 0) {
            game_state.game_over = 1;
            game_state.winner = 1;
        }
    }

    // Handle AI turn
    if (game_state.current_turn == 2) {
        handle_input(0);
    }

    usleep(100000);
    glutPostRedisplay();
}

// Main
int main(int argc, char** argv) {
    // Enable UTF-8 for terminal
    setlocale(LC_ALL, "");

    // Initialize terminal for non-blocking input
    init_terminal();

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Chess-like Game (GLUT)");

    // Initialize game state
    memset(&game_state, 0, sizeof(game_state));
    game_state.current_turn = 1;
    game_state.cursor_x = 0;
    game_state.cursor_y = 0;
    game_state.selected_x = -1;
    game_state.selected_y = -1;
    game_state.player1_timer = 300;
    game_state.player2_timer = 300;
    game_state.message[0] = '\0';

    // Load board
    read_board_state();

    // Initialize FreeType
    initFreeType();

    // Set GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutIdleFunc(idle);

    glutMainLoop();

    // Cleanup
    reset_terminal();
    FT_Done_Face(face);
    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (game_state.board[i][j]) free(game_state.board[i][j]);
        }
    }

    return 0;
}
