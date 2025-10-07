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

#define NUM_SPACES 40
#define GRID_SIZE 11
#define MAX_PLAYERS 5

// Space type enum
typedef enum {
    TYPE_GO, TYPE_PROPERTY, TYPE_UTILITY, TYPE_RAILROAD,
    TYPE_CHANCE, TYPE_CHEST, TYPE_TAX, TYPE_JAIL,
    TYPE_FREE_PARKING, TYPE_GO_TO_JAIL
} SpaceType;

// Player structure
typedef struct {
    char name[50];
    int type; // 0 human, 1 computer
    char emoji[5];
    int position;
    int money;
    int in_jail;
} Player;

// Space structure
typedef struct {
    char name[50];
    SpaceType type;
    int price;
    int mortgage;
    int house_cost;
    int rent[6];
    int color_group; // -1 if not property
    float group_color[3];
    int owner; // -1 none, else player index
    int houses; // 0-5
} Space;

// Game state
typedef struct {
    int current_turn;
    int state; // 0: normal, 1: buying
    char message[100];
    time_t message_expire;
    time_t last_action_time;
    int num_players;
    Player players[MAX_PLAYERS];
    Space board[NUM_SPACES];
    int game_over;
} GameState;

GameState game_state;

// FreeType globals
FT_Library ft;
FT_Face face; // For text
FT_Face emoji_face; // For emojis
float emoji_scale;
float font_size = 12.0f;
float font_color[3] = {0.0f, 0.0f, 0.0f}; // Black for better visibility
float background_color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // White background

// Window dimensions
int window_width = 1200;
int window_height = 800;
float cell_size = 60.0f;
float board_offset_x = 50.0f;
float board_offset_y = 50.0f;

// Function declarations
void initFreeType();
void render_text(const char* str, float x, float y);
void render_emoji(unsigned int codepoint, float x, float y);
float get_char_width(unsigned int codepoint);
float get_string_width(const char* str, int bytes_to_measure);
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void init_board(Space* board);
void display();
void keyboard(unsigned char key, int x, int y);
void reshape(int w, int h);
void idle();
void set_status_message(const char* msg, int duration_seconds);
void handle_roll(int is_human);

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
    emoji_scale = font_size / (float)loaded_emoji_size * 2.0f; // Larger emojis for tokens
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

// Render emoji
// Render emoji
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
    glColor3f(1.0f, 1.0f, 1.0f); // Set to white to preserve emoji colors

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
// Render text
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

// Draw rectangle
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

// Initialize board
void init_board(Space* board) {
    int i;

    for (i = 0; i < NUM_SPACES; i++) {
        board[i].owner = -1;
        board[i].houses = 0;
        board[i].color_group = -1;
        board[i].group_color[0] = 1.0f;
        board[i].group_color[1] = 1.0f;
        board[i].group_color[2] = 1.0f;
    }

    // 0: Go
    strcpy(board[0].name, "GO");
    board[0].type = TYPE_GO;

    // 1: Mediterranean Avenue
    strcpy(board[1].name, "Mediterranean Ave");
    board[1].type = TYPE_PROPERTY;
    board[1].price = 60;
    board[1].mortgage = 30;
    board[1].house_cost = 50;
    board[1].rent[0] = 2;
    board[1].rent[1] = 10;
    board[1].rent[2] = 30;
    board[1].rent[3] = 90;
    board[1].rent[4] = 160;
    board[1].rent[5] = 250;
    board[1].color_group = 0;
    board[1].group_color[0] = 0.5f; board[1].group_color[1] = 0.0f; board[1].group_color[2] = 0.5f;

    // 2: Community Chest
    strcpy(board[2].name, "Community Chest");
    board[2].type = TYPE_CHEST;

    // 3: Baltic Avenue
    strcpy(board[3].name, "Baltic Ave");
    board[3].type = TYPE_PROPERTY;
    board[3].price = 60;
    board[3].mortgage = 30;
    board[3].house_cost = 50;
    board[3].rent[0] = 4;
    board[3].rent[1] = 20;
    board[3].rent[2] = 60;
    board[3].rent[3] = 180;
    board[3].rent[4] = 320;
    board[3].rent[5] = 450;
    board[3].color_group = 0;
    board[3].group_color[0] = 0.5f; board[3].group_color[1] = 0.0f; board[3].group_color[2] = 0.5f;

    // 4: Income Tax
    strcpy(board[4].name, "Income Tax");
    board[4].type = TYPE_TAX;
    board[4].price = 200;

    // 5: Reading Railroad
    strcpy(board[5].name, "Reading Railroad");
    board[5].type = TYPE_RAILROAD;
    board[5].price = 200;
    board[5].mortgage = 100;
    board[5].rent[0] = 25;
    board[5].rent[1] = 50;
    board[5].rent[2] = 100;
    board[5].rent[3] = 200;

    // 6: Oriental Avenue
    strcpy(board[6].name, "Oriental Ave");
    board[6].type = TYPE_PROPERTY;
    board[6].price = 100;
    board[6].mortgage = 50;
    board[6].house_cost = 50;
    board[6].rent[0] = 6;
    board[6].rent[1] = 30;
    board[6].rent[2] = 90;
    board[6].rent[3] = 270;
    board[6].rent[4] = 400;
    board[6].rent[5] = 550;
    board[6].color_group = 1;
    board[6].group_color[0] = 0.6f; board[6].group_color[1] = 0.8f; board[6].group_color[2] = 1.0f;

    // 7: Chance
    strcpy(board[7].name, "Chance");
    board[7].type = TYPE_CHANCE;

    // 8: Vermont Avenue
    strcpy(board[8].name, "Vermont Ave");
    board[8].type = TYPE_PROPERTY;
    board[8].price = 100;
    board[8].mortgage = 50;
    board[8].house_cost = 50;
    board[8].rent[0] = 6;
    board[8].rent[1] = 30;
    board[8].rent[2] = 90;
    board[8].rent[3] = 270;
    board[8].rent[4] = 400;
    board[8].rent[5] = 550;
    board[8].color_group = 1;
    board[8].group_color[0] = 0.6f; board[8].group_color[1] = 0.8f; board[8].group_color[2] = 1.0f;

    // 9: Connecticut Avenue
    strcpy(board[9].name, "Connecticut Ave");
    board[9].type = TYPE_PROPERTY;
    board[9].price = 120;
    board[9].mortgage = 60;
    board[9].house_cost = 50;
    board[9].rent[0] = 8;
    board[9].rent[1] = 40;
    board[9].rent[2] = 100;
    board[9].rent[3] = 300;
    board[9].rent[4] = 450;
    board[9].rent[5] = 600;
    board[9].color_group = 1;
    board[9].group_color[0] = 0.6f; board[9].group_color[1] = 0.8f; board[9].group_color[2] = 1.0f;

    // 10: Jail
    strcpy(board[10].name, "Jail");
    board[10].type = TYPE_JAIL;

    // 11: St. Charles Place
    strcpy(board[11].name, "St. Charles Pl");
    board[11].type = TYPE_PROPERTY;
    board[11].price = 140;
    board[11].mortgage = 70;
    board[11].house_cost = 100;
    board[11].rent[0] = 10;
    board[11].rent[1] = 50;
    board[11].rent[2] = 150;
    board[11].rent[3] = 450;
    board[11].rent[4] = 625;
    board[11].rent[5] = 750;
    board[11].color_group = 2;
    board[11].group_color[0] = 1.0f; board[11].group_color[1] = 0.75f; board[11].group_color[2] = 0.8f;

    // 12: Electric Company
    strcpy(board[12].name, "Electric Co");
    board[12].type = TYPE_UTILITY;
    board[12].price = 150;
    board[12].mortgage = 75;

    // 13: States Avenue
    strcpy(board[13].name, "States Ave");
    board[13].type = TYPE_PROPERTY;
    board[13].price = 140;
    board[13].mortgage = 70;
    board[13].house_cost = 100;
    board[13].rent[0] = 10;
    board[13].rent[1] = 50;
    board[13].rent[2] = 150;
    board[13].rent[3] = 450;
    board[13].rent[4] = 625;
    board[13].rent[5] = 750;
    board[13].color_group = 2;
    board[13].group_color[0] = 1.0f; board[13].group_color[1] = 0.75f; board[13].group_color[2] = 0.8f;

    // 14: Virginia Avenue
    strcpy(board[14].name, "Virginia Ave");
    board[14].type = TYPE_PROPERTY;
    board[14].price = 160;
    board[14].mortgage = 80;
    board[14].house_cost = 100;
    board[14].rent[0] = 12;
    board[14].rent[1] = 60;
    board[14].rent[2] = 180;
    board[14].rent[3] = 500;
    board[14].rent[4] = 700;
    board[14].rent[5] = 900;
    board[14].color_group = 2;
    board[14].group_color[0] = 1.0f; board[14].group_color[1] = 0.75f; board[14].group_color[2] = 0.8f;

    // 15: Pennsylvania Railroad
    strcpy(board[15].name, "Pennsylvania RR");
    board[15].type = TYPE_RAILROAD;
    board[15].price = 200;
    board[15].mortgage = 100;
    board[15].rent[0] = 25;
    board[15].rent[1] = 50;
    board[15].rent[2] = 100;
    board[15].rent[3] = 200;

    // 16: St. James Place
    strcpy(board[16].name, "St. James Pl");
    board[16].type = TYPE_PROPERTY;
    board[16].price = 180;
    board[16].mortgage = 90;
    board[16].house_cost = 100;
    board[16].rent[0] = 14;
    board[16].rent[1] = 70;
    board[16].rent[2] = 200;
    board[16].rent[3] = 550;
    board[16].rent[4] = 750;
    board[16].rent[5] = 950;
    board[16].color_group = 3;
    board[16].group_color[0] = 1.0f; board[16].group_color[1] = 0.5f; board[16].group_color[2] = 0.0f;

    // 17: Community Chest
    strcpy(board[17].name, "Community Chest");
    board[17].type = TYPE_CHEST;

    // 18: Tennessee Avenue
    strcpy(board[18].name, "Tennessee Ave");
    board[18].type = TYPE_PROPERTY;
    board[18].price = 180;
    board[18].mortgage = 90;
    board[18].house_cost = 100;
    board[18].rent[0] = 14;
    board[18].rent[1] = 70;
    board[18].rent[2] = 200;
    board[18].rent[3] = 550;
    board[18].rent[4] = 750;
    board[18].rent[5] = 950;
    board[18].color_group = 3;
    board[18].group_color[0] = 1.0f; board[18].group_color[1] = 0.5f; board[18].group_color[2] = 0.0f;

    // 19: New York Avenue
    strcpy(board[19].name, "New York Ave");
    board[19].type = TYPE_PROPERTY;
    board[19].price = 200;
    board[19].mortgage = 100;
    board[19].house_cost = 100;
    board[19].rent[0] = 16;
    board[19].rent[1] = 80;
    board[19].rent[2] = 220;
    board[19].rent[3] = 600;
    board[19].rent[4] = 800;
    board[19].rent[5] = 1000;
    board[19].color_group = 3;
    board[19].group_color[0] = 1.0f; board[19].group_color[1] = 0.5f; board[19].group_color[2] = 0.0f;

    // 20: Free Parking
    strcpy(board[20].name, "Free Parking");
    board[20].type = TYPE_FREE_PARKING;

    // 21: Kentucky Avenue
    strcpy(board[21].name, "Kentucky Ave");
    board[21].type = TYPE_PROPERTY;
    board[21].price = 220;
    board[21].mortgage = 110;
    board[21].house_cost = 150;
    board[21].rent[0] = 18;
    board[21].rent[1] = 90;
    board[21].rent[2] = 250;
    board[21].rent[3] = 700;
    board[21].rent[4] = 875;
    board[21].rent[5] = 1050;
    board[21].color_group = 4;
    board[21].group_color[0] = 1.0f; board[21].group_color[1] = 0.0f; board[21].group_color[2] = 0.0f;

    // 22: Chance
    strcpy(board[22].name, "Chance");
    board[22].type = TYPE_CHANCE;

    // 23: Indiana Avenue
    strcpy(board[23].name, "Indiana Ave");
    board[23].type = TYPE_PROPERTY;
    board[23].price = 220;
    board[23].mortgage = 110;
    board[23].house_cost = 150;
    board[23].rent[0] = 18;
    board[23].rent[1] = 90;
    board[23].rent[2] = 250;
    board[23].rent[3] = 700;
    board[23].rent[4] = 875;
    board[23].rent[5] = 1050;
    board[23].color_group = 4;
    board[23].group_color[0] = 1.0f; board[23].group_color[1] = 0.0f; board[23].group_color[2] = 0.0f;

    // 24: Illinois Avenue
    strcpy(board[24].name, "Illinois Ave");
    board[24].type = TYPE_PROPERTY;
    board[24].price = 240;
    board[24].mortgage = 120;
    board[24].house_cost = 150;
    board[24].rent[0] = 20;
    board[24].rent[1] = 100;
    board[24].rent[2] = 300;
    board[24].rent[3] = 750;
    board[24].rent[4] = 925;
    board[24].rent[5] = 1100;
    board[24].color_group = 4;
    board[24].group_color[0] = 1.0f; board[24].group_color[1] = 0.0f; board[24].group_color[2] = 0.0f;

    // 25: B & O Railroad
    strcpy(board[25].name, "B & O RR");
    board[25].type = TYPE_RAILROAD;
    board[25].price = 200;
    board[25].mortgage = 100;
    board[25].rent[0] = 25;
    board[25].rent[1] = 50;
    board[25].rent[2] = 100;
    board[25].rent[3] = 200;

    // 26: Atlantic Avenue
    strcpy(board[26].name, "Atlantic Ave");
    board[26].type = TYPE_PROPERTY;
    board[26].price = 260;
    board[26].mortgage = 130;
    board[26].house_cost = 150;
    board[26].rent[0] = 22;
    board[26].rent[1] = 110;
    board[26].rent[2] = 330;
    board[26].rent[3] = 800;
    board[26].rent[4] = 975;
    board[26].rent[5] = 1150;
    board[26].color_group = 5;
    board[26].group_color[0] = 1.0f; board[26].group_color[1] = 1.0f; board[26].group_color[2] = 0.0f;

    // 27: Ventnor Avenue
    strcpy(board[27].name, "Ventnor Ave");
    board[27].type = TYPE_PROPERTY;
    board[27].price = 260;
    board[27].mortgage = 130;
    board[27].house_cost = 150;
    board[27].rent[0] = 22;
    board[27].rent[1] = 110;
    board[27].rent[2] = 330;
    board[27].rent[3] = 800;
    board[27].rent[4] = 975;
    board[27].rent[5] = 1150;
    board[27].color_group = 5;
    board[27].group_color[0] = 1.0f; board[27].group_color[1] = 1.0f; board[27].group_color[2] = 0.0f;

    // 28: Water Works
    strcpy(board[28].name, "Water Works");
    board[28].type = TYPE_UTILITY;
    board[28].price = 150;
    board[28].mortgage = 75;

    // 29: Marvin Gardens
    strcpy(board[29].name, "Marvin Gardens");
    board[29].type = TYPE_PROPERTY;
    board[29].price = 280;
    board[29].mortgage = 140;
    board[29].house_cost = 150;
    board[29].rent[0] = 24;
    board[29].rent[1] = 120;
    board[29].rent[2] = 360;
    board[29].rent[3] = 850;
    board[29].rent[4] = 1025;
    board[29].rent[5] = 1200;
    board[29].color_group = 5;
    board[29].group_color[0] = 1.0f; board[29].group_color[1] = 1.0f; board[29].group_color[2] = 0.0f;

    // 30: Go to Jail
    strcpy(board[30].name, "Go to Jail");
    board[30].type = TYPE_GO_TO_JAIL;

    // 31: Pacific Avenue
    strcpy(board[31].name, "Pacific Ave");
    board[31].type = TYPE_PROPERTY;
    board[31].price = 300;
    board[31].mortgage = 150;
    board[31].house_cost = 200;
    board[31].rent[0] = 26;
    board[31].rent[1] = 130;
    board[31].rent[2] = 390;
    board[31].rent[3] = 900;
    board[31].rent[4] = 1100;
    board[31].rent[5] = 1275;
    board[31].color_group = 6;
    board[31].group_color[0] = 0.0f; board[31].group_color[1] = 0.5f; board[31].group_color[2] = 0.0f;

    // 32: North Carolina Avenue
    strcpy(board[32].name, "N Carolina Ave");
    board[32].type = TYPE_PROPERTY;
    board[32].price = 300;
    board[32].mortgage = 150;
    board[32].house_cost = 200;
    board[32].rent[0] = 26;
    board[32].rent[1] = 130;
    board[32].rent[2] = 390;
    board[32].rent[3] = 900;
    board[32].rent[4] = 1100;
    board[32].rent[5] = 1275;
    board[32].color_group = 6;
    board[32].group_color[0] = 0.0f; board[32].group_color[1] = 0.5f; board[32].group_color[2] = 0.0f;

    // 33: Community Chest
    strcpy(board[33].name, "Community Chest");
    board[33].type = TYPE_CHEST;

    // 34: Pennsylvania Avenue
    strcpy(board[34].name, "Pennsylvania Ave");
    board[34].type = TYPE_PROPERTY;
    board[34].price = 320;
    board[34].mortgage = 160;
    board[34].house_cost = 200;
    board[34].rent[0] = 28;
    board[34].rent[1] = 150;
    board[34].rent[2] = 450;
    board[34].rent[3] = 1000;
    board[34].rent[4] = 1200;
    board[34].rent[5] = 1400;
    board[34].color_group = 6;
    board[34].group_color[0] = 0.0f; board[34].group_color[1] = 0.5f; board[34].group_color[2] = 0.0f;

    // 35: Short Line
    strcpy(board[35].name, "Short Line");
    board[35].type = TYPE_RAILROAD;
    board[35].price = 200;
    board[35].mortgage = 100;
    board[35].rent[0] = 25;
    board[35].rent[1] = 50;
    board[35].rent[2] = 100;
    board[35].rent[3] = 200;

    // 36: Chance
    strcpy(board[36].name, "Chance");
    board[36].type = TYPE_CHANCE;

    // 37: Park Place
    strcpy(board[37].name, "Park Place");
    board[37].type = TYPE_PROPERTY;
    board[37].price = 350;
    board[37].mortgage = 175;
    board[37].house_cost = 200;
    board[37].rent[0] = 35;
    board[37].rent[1] = 175;
    board[37].rent[2] = 500;
    board[37].rent[3] = 1100;
    board[37].rent[4] = 1300;
    board[37].rent[5] = 1500;
    board[37].color_group = 7;
    board[37].group_color[0] = 0.0f; board[37].group_color[1] = 0.0f; board[37].group_color[2] = 0.5f;

    // 38: Luxury Tax
    strcpy(board[38].name, "Luxury Tax");
    board[38].type = TYPE_TAX;
    board[38].price = 100;

    // 39: Boardwalk
    strcpy(board[39].name, "Boardwalk");
    board[39].type = TYPE_PROPERTY;
    board[39].price = 400;
    board[39].mortgage = 200;
    board[39].house_cost = 200;
    board[39].rent[0] = 50;
    board[39].rent[1] = 200;
    board[39].rent[2] = 600;
    board[39].rent[3] = 1400;
    board[39].rent[4] = 1700;
    board[39].rent[5] = 2000;
    board[39].color_group = 7;
    board[39].group_color[0] = 0.0f; board[39].group_color[1] = 0.0f; board[39].group_color[2] = 0.5f;
}

// Display function
void display() {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw board spaces
    for (int p = 0; p < NUM_SPACES; p++) {
        int gx, gy;
        if (p <= 10) {
            gx = 10 - p;
            gy = 0;
        } else if (p <= 20) {
            gx = 0;
            gy = p - 10;
        } else if (p <= 30) {
            gx = p - 20;
            gy = 10;
        } else {
            gx = 10;
            gy = 40 - p;
        }

        float rx = board_offset_x + gx * cell_size;
        float ry = board_offset_y + gy * cell_size;
        float color[3] = {0.95f, 0.95f, 0.95f}; // Light gray for spaces
        draw_rect(rx, ry, cell_size, cell_size, color);

        // Draw color group bar for properties
        if (game_state.board[p].type == TYPE_PROPERTY) {
            draw_rect(rx, ry + cell_size - 10, cell_size, 10, game_state.board[p].group_color);
        }

        // Draw name (simple, no wrap for now - abbreviated names used)
        render_text(game_state.board[p].name, rx + 5, ry + cell_size - 25);

        // Draw price if applicable
        if (game_state.board[p].type == TYPE_PROPERTY || game_state.board[p].type == TYPE_RAILROAD || game_state.board[p].type == TYPE_UTILITY || game_state.board[p].type == TYPE_TAX) {
            char pr[20];
            sprintf(pr, "$%d", game_state.board[p].price);
            render_text(pr, rx + 5, ry + 5);
        }

        // Draw player tokens
        int player_count = 0;
        for (int pl = 0; pl < game_state.num_players; pl++) {
            if (game_state.players[pl].position == p) {
                float px = rx + player_count * (font_size * 1.5) + 5;
                float py = ry + cell_size / 2;
                render_text(game_state.players[pl].emoji, px, py);
                player_count++;
            }
        }
    }

    // Draw player info panel
    float panel_x = board_offset_x + GRID_SIZE * cell_size + 50;
    for (int i = 0; i < game_state.num_players; i++) {
        float py = window_height - 50 - i * 60;
        char info[100];
        sprintf(info, "%s %s $%d", game_state.players[i].name, game_state.players[i].emoji, game_state.players[i].money);
        render_text(info, panel_x, py);
        if (game_state.current_turn == i) {
            float sel_color[3] = {0.0f, 1.0f, 0.0f};
            draw_rect(panel_x - 5, py - 5, get_string_width(info, strlen(info)) + 10, font_size + 10, sel_color);
        }
    }

    // Draw message
    if (game_state.message[0]) {
        render_text(game_state.message, 10, window_height - 40);
    }

    // Draw instructions
    render_text("Press 'r' to roll dice (humans only)", 10, 10);

    glutSwapBuffers();
}

// Handle roll and space logic
void handle_roll(int is_human) {
    int roll = (rand() % 6 + 1) + (rand() % 6 + 1);
    Player *pl = &game_state.players[game_state.current_turn];
    int old_pos = pl->position;
    pl->position = (pl->position + roll) % NUM_SPACES;
    if (pl->position < old_pos) {
        pl->money += 200;
    }

    char msg[100];
    sprintf(msg, "%s rolled %d", pl->name, roll);
    set_status_message(msg, 2);

    Space *s = &game_state.board[pl->position];

    if (s->type == TYPE_GO_TO_JAIL) {
        pl->position = 10;
        pl->in_jail = 1;
        set_status_message("Go to Jail!", 2);
    } else if (s->type == TYPE_PROPERTY && s->owner == -1) {
        if (is_human) {
            sprintf(msg, "Press 'b' to buy %s for $%d, 's' to skip", s->name, s->price);
            set_status_message(msg, 5);
            game_state.state = 1;
            return; // Wait for input
        } else if (pl->money >= s->price) {
            pl->money -= s->price;
            s->owner = game_state.current_turn;
            sprintf(msg, "%s bought %s", pl->name, s->name);
            set_status_message(msg, 2);
        }
    } else if ((s->type == TYPE_PROPERTY || s->type == TYPE_RAILROAD || s->type == TYPE_UTILITY) && s->owner != -1 && s->owner != game_state.current_turn) {
        int rent = s->rent[0]; // Basic rent, no houses/special yet
        pl->money -= rent;
        game_state.players[s->owner].money += rent;
        sprintf(msg, "Paid $%d rent to %s", rent, game_state.players[s->owner].name);
        set_status_message(msg, 2);
    } else if (s->type == TYPE_TAX) {
        pl->money -= s->price;
        sprintf(msg, "Paid $%d tax", s->price);
        set_status_message(msg, 2);
    } else if (s->type == TYPE_CHANCE || s->type == TYPE_CHEST) {
        set_status_message("Draw card (not implemented)", 2);
    }

    // Next turn
    game_state.current_turn = (game_state.current_turn + 1) % game_state.num_players;
    game_state.state = 0;
}

// Keyboard input
void keyboard(unsigned char key, int x, int y) {
    if (game_state.game_over) return;

    if (game_state.state == 0 && key == 'r' && game_state.players[game_state.current_turn].type == 0) {
        handle_roll(1);
    } else if (game_state.state == 1 && game_state.players[game_state.current_turn].type == 0) {
        Player *pl = &game_state.players[game_state.current_turn];
        Space *s = &game_state.board[pl->position];
        if (key == 'b' && pl->money >= s->price) {
            pl->money -= s->price;
            s->owner = game_state.current_turn;
            set_status_message("Bought property", 2);
            game_state.current_turn = (game_state.current_turn + 1) % game_state.num_players;
            game_state.state = 0;
        } else if (key == 's') {
            set_status_message("Skipped buy", 2);
            game_state.current_turn = (game_state.current_turn + 1) % game_state.num_players;
            game_state.state = 0;
        }
    }
    glutPostRedisplay();
}

// Reshape
void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
}

// Idle
void idle() {
    time_t now = time(NULL);
    if (now >= game_state.message_expire && game_state.message[0] != '\0') {
        game_state.message[0] = '\0';
        glutPostRedisplay();
    }
    if (game_state.players[game_state.current_turn].type == 1 && game_state.state == 0 && now > game_state.last_action_time) {
        handle_roll(0);
        game_state.last_action_time = now + 2;
        glutPostRedisplay();
    }
    usleep(100000);
}

// Main
int main(int argc, char** argv) {
    srand(time(NULL));

    // Game setup
    printf("Enter number of players (1-5): ");
    scanf("%d", &game_state.num_players);
    if (game_state.num_players < 1 || game_state.num_players > 5) {
        fprintf(stderr, "Invalid number of players\n");
        exit(1);
    }

    const char* emoji_options[5] = {"🚗", "🐶", "🎩", "🚢", "🐱"};
    for (int i = 0; i < game_state.num_players; i++) {
        printf("--- Player %d ---\n", i + 1);
        printf("Enter name: ");
        scanf("%s", game_state.players[i].name);
        printf("Enter player type (0 for Human, 1 for Computer): ");
        scanf("%d", &game_state.players[i].type);
        printf("Choose emoji:\n");
        for (int j = 0; j < 5; j++) {
            printf("%d: %s\n", j, emoji_options[j]);
        }
        int choice;
        scanf("%d", &choice);
        if (choice < 0 || choice >= 5) choice = 0;
        strcpy(game_state.players[i].emoji, emoji_options[choice]);
        game_state.players[i].position = 0;
        game_state.players[i].money = 1500;
        game_state.players[i].in_jail = 0;
    }

    game_state.current_turn = 0;
    game_state.state = 0;
    game_state.game_over = 0;
    game_state.message[0] = '\0';
    game_state.last_action_time = time(NULL);

    init_board(game_state.board);

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Monopoly Game");

    // Initialize FreeType
    initFreeType();

    // Set GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(idle);

    glutMainLoop();

    // Cleanup
    FT_Done_Face(face);
    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    return 0;
}
