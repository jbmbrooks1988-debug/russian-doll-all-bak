#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

// --- Constants ---
#define GRID_ROWS 8
#define GRID_COLS 8
#define TILE_SIZE 64

// Emojis
const char* EMOJI_PET_BABY = "🐣";
const char* EMOJI_PET_CHILD = "🐥";
const char* EMOJI_PET_ADULT = "🐓";
const char* EMOJI_PET_SAD = "😢";
const char* EMOJI_PET_HAPPY = "😄";
const char* EMOJI_PET_TIRED = "🥱";
const char* EMOJI_PET_STRONG = "💪";
const char* EMOJI_PET_SMART = "🧠";
const char* EMOJI_PET_CUTE = "💖";
const char* EMOJI_FOOD = "🍗";
const char* EMOJI_REST = "🛏️";
const char* EMOJI_TRAIN = "🏋️";
const char* EMOJI_CLEAN = "🧼";
const char* EMOJI_SCOLD = "😠";
const char* EMOJI_STATS = "📊";
const char* EMOJI_MONEY = "💰";
const char* EMOJI_DIRT = "💩";

// Game state
int day = 1;
int money = 50;
int age = 0;
int items_food = 3;

// Pet Stats (0-100)
int health = 80;
int hunger = 70;
int energy = 60;
int happiness = 75;
int strength = 20;
int intelligence = 20;
int cuteness = 50;

// Training focus
int train_focus = 0;  // 0: none, 1: strength, 2: intelligence, 3: cuteness
bool pet_needs_cleaning = false;

// UI State
int cursor_row = 6, cursor_col = 3;
bool show_cursor = true;
bool debug_ui_enabled = true;
bool show_stats_popup = false;

// Window
int window_width = GRID_COLS * TILE_SIZE + 20;
int window_height = GRID_ROWS * TILE_SIZE + 120;

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.9f, 0.9f, 1.0f, 1.0f};

char status_message[256] = "Welcome! Your new pet has arrived! 💖";

// --- FreeType & Rendering ---
FT_Library ft;
FT_Face emoji_face;
Display *x_display = NULL;
Window x_window;

// --- Threading ---
pthread_t input_thread;
bool running = true;

// --- Function Prototypes ---
void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int xx, int yy);
void special(int key, int x, int y);
void init();
void idle();
void draw_rect(float x, float y, float w, float h, float color[3]);
void end_day();
int decode_utf8(const unsigned char* str, unsigned int* codepoint);
void set_status_message(const char* format, ...);
void feed_pet();
void rest_pet();
void train_pet();
void clean_pet();
void scold_pet();
const char* get_pet_emoji();
void update_pet();
void print_pet_status();
void* terminal_input_handler(void* arg);

#define MENU_HEIGHT 20
#define MENU_Y (window_height - MENU_HEIGHT)

// --- UTF-8 Decoder ---
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0 && (str[1] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    }
    if ((str[0] & 0xF0) == 0xE0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    }
    if ((str[0] & 0xF8) == 0xF0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }
    *codepoint = 0xFFFD;
    return 1;
}

// --- FreeType Init ---
void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        exit(1);
    }

    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error: Could not load emoji font at %s, error code: %d\n", emoji_font_path, err);
        exit(1);
    }

    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, TILE_SIZE - 10);
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
    } else {
        fprintf(stderr, "Error: No usable size in emoji font\n");
        exit(1);
    }

    if (err) {
        fprintf(stderr, "Error: Could not set emoji font size, error code: %d\n", err);
        exit(1);
    }

    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)(TILE_SIZE * 0.8f) / (float)loaded_emoji_size;
    fprintf(stderr, "Emoji font loaded. Size: %d, Scale: %.2f\n", loaded_emoji_size, emoji_scale);
}

// --- Emoji Rendering ---
void render_emoji(unsigned int codepoint, float x, float y) {
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err || !emoji_face->glyph->bitmap.buffer) return;

    FT_GlyphSlot slot = emoji_face->glyph;
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) return;

    GLboolean was_blend, was_tex;
    glGetBooleanv(GL_BLEND, &was_blend);
    glGetBooleanv(GL_TEXTURE_2D, &was_tex);
    GLfloat color[4];
    glGetFloatv(GL_CURRENT_COLOR, color);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    float w = slot->bitmap.width * emoji_scale;
    float h = slot->bitmap.rows * emoji_scale;
    float x2 = x - w / 2;
    float y2 = y - h / 2;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x2, y2);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x2, y2 + h);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
    if (!was_blend) glDisable(GL_BLEND);
    if (!was_tex) glDisable(GL_TEXTURE_2D);
    glColor4fv(color);
}

// --- Text Rendering ---
void render_text(const char* str, float x, float y) {
    GLfloat current_color[4];
    glGetFloatv(GL_CURRENT_COLOR, current_color);
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
    glColor4fv(current_color);
}

// --- Get Pet Emoji Based on State ---
const char* get_pet_emoji() {
    if (health < 20) return EMOJI_PET_SAD;
    if (energy < 20) return EMOJI_PET_TIRED;
    if (hunger > 80) return EMOJI_PET_HAPPY;

    if (age < 5) return EMOJI_PET_BABY;
    else if (age < 10) return EMOJI_PET_CHILD;
    else return EMOJI_PET_ADULT;
}

// --- Update Pet (called every day) ---
void update_pet() {
    hunger = (hunger + 10 > 100) ? 100 : hunger + 10;
    energy = (energy - 15 < 0) ? 0 : energy - 15;
    happiness = (happiness - 5 < 0) ? 0 : happiness - 5;
    health = (hunger > 90 || energy < 10 || happiness < 10) ? (health - 10 > 0 ? health - 10 : 0) : (health + 5 > 100 ? 100 : health + 5);

    if (rand() % 100 < 30) pet_needs_cleaning = true;

    if (health <= 0) {
        set_status_message("💔 Your pet is very sick! You need to care better!");
    }
}

// --- End Day ---
void end_day() {
    day++;
    if (day % 5 == 0) age++;

    update_pet();
    set_status_message("🌞 Day %d. Age: %d. Health: %d", day, age, health);
    print_pet_status();
    glutPostRedisplay();
}

// --- Actions ---
void feed_pet() {
    if (items_food <= 0) {
        set_status_message("🍽️ No food left! Buy in shop.");
        return;
    }
    if (hunger < 30) {
        set_status_message("😊 Pet isn't hungry yet.");
        return;
    }
    items_food--;
    hunger = (hunger - 40 < 0) ? 0 : hunger - 40;
    happiness = (happiness + 10 > 100) ? 100 : happiness + 10;
    set_status_message("🍗 Fed your pet! Hunger: %d", hunger);
    print_pet_status();
}

void rest_pet() {
    if (energy > 70) {
        set_status_message("😴 Pet isn't tired.");
        return;
    }
    energy = (energy + 50 > 100) ? 100 : energy + 50;
    happiness = (happiness + 5 > 100) ? 100 : happiness + 5;
    set_status_message("🛌 Pet rested! Energy: %d", energy);
    print_pet_status();
}

void train_pet() {
    if (energy < 20) {
        set_status_message("💤 Too tired to train!");
        return;
    }
    energy -= 20;

    if (train_focus == 1) {
        strength = (strength + 15 > 100) ? 100 : strength + 15;
        set_status_message("💪 Training strength... +15! Strength: %d", strength);
    } else if (train_focus == 2) {
        intelligence = (intelligence + 15 > 100) ? 100 : intelligence + 15;
        money += 5;
        set_status_message("🧠 Training smarts... +15! +5 money. Intelligence: %d", intelligence);
    } else if (train_focus == 3) {
        cuteness = (cuteness + 15 > 100) ? 100 : cuteness + 15;
        happiness += 10;
        set_status_message("💖 Training cuteness... +15! Happiness +10. Cuteness: %d", cuteness);
    } else {
        set_status_message("🎯 Select a training focus first (T menu).");
        return;
    }
    print_pet_status();
    glutPostRedisplay();
}

void clean_pet() {
    if (!pet_needs_cleaning) {
        set_status_message("🧼 No mess to clean.");
        return;
    }
    pet_needs_cleaning = false;
    happiness += 15;
    set_status_message("🧽 Cleaned up! Happiness +15 💖");
    print_pet_status();
}

void scold_pet() {
    if (happiness < 20) {
        set_status_message("😭 Pet is already too sad...");
        return;
    }
    happiness -= 20;
    set_status_message("😠 You scolded your pet... Happiness -20");
    print_pet_status();
}

// --- Print Pet Status to Terminal ---
void print_pet_status() {
    printf("\n--- 🐣 PET STATUS ---\n");
    printf("Age:       %d days\n", age);
    printf("Health:    %d/100\n", health);
    printf("Hunger:    %d/100\n", hunger);
    printf("Energy:    %d/100\n", energy);
    printf("Happiness: %d/100\n", happiness);
    printf("Strength:  %d/100\n", strength);
    printf("Intel:     %d/100\n", intelligence);
    printf("Cuteness:  %d/100\n", cuteness);
    printf("Money:     $%d\n", money);
    printf("Food:      %d\n", items_food);
    printf("Clean?:    %s\n", pet_needs_cleaning ? "NO! 💩" : "Yes");
    printf("Focus:     %s\n", 
        train_focus == 1 ? "Strength" :
        train_focus == 2 ? "Intelligence" :
        train_focus == 3 ? "Cuteness" : "None");
    printf("------------------------\n");
}

// --- Terminal Input Thread ---
void* terminal_input_handler(void* arg) {
    char input[64];

    printf("\n🎮 Terminal Control Ready! Commands:\n");
    printf("  feed, rest, train, clean, scold, info, buy, sleep, exit\n\n");

    while (running) {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) != NULL) {
            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "feed") == 0) {
                feed_pet();
            }
            else if (strcmp(input, "rest") == 0) {
                rest_pet();
            }
            else if (strcmp(input, "train") == 0) {
                train_pet();
            }
            else if (strcmp(input, "clean") == 0) {
                clean_pet();
            }
            else if (strcmp(input, "scold") == 0) {
                scold_pet();
            }
            else if (strcmp(input, "info") == 0 || strcmp(input, "status") == 0) {
                print_pet_status();
            }
            else if (strcmp(input, "buy") == 0) {
                if (money >= 20) {
                    money -= 20;
                    items_food += 5;
                    set_status_message("🛒 Bought 5 food! Money: %d", money);
                    print_pet_status();
                } else {
                    set_status_message("💸 Not enough money!");
                }
            }
            else if (strcmp(input, "sleep") == 0) {
                end_day();
            }
            else if (strcmp(input, "exit") == 0) {
                running = false;
                printf("👋 Goodbye! Your pet will miss you.\n");
                exit(0);
            }
            else if (strlen(input) > 0) {
                set_status_message("❓ Unknown command: %s", input);
            }
        }
    }
    return NULL;
}

// --- Input: Mouse ---
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        int gl_y = window_height - y;  // Convert to OpenGL Y

        // Menu clicks (top)
        if (gl_y >= MENU_Y && gl_y < window_height) {
            if (x >= 10 && x <= 60) {
                set_status_message("File > Save (not implemented)");
            } else if (x >= 80 && x <= 140) {
                set_status_message("T: Set training focus");
            } else if (x >= 160 && x <= 220) {
                train_focus = 1;
                set_status_message("🎯 Training: Strength");
            } else if (x >= 240 && x <= 300) {
                train_focus = 2;
                set_status_message("🎯 Training: Intelligence");
            } else if (x >= 320 && x <= 380) {
                train_focus = 3;
                set_status_message("🎯 Training: Cuteness");
            } else if (x >= 400 && x <= 460) {
                show_stats_popup = !show_stats_popup;
                set_status_message("📊 Stats: %s", show_stats_popup ? "Visible" : "Hidden");
            }
            glutPostRedisplay();
            return;
        }

        // Action buttons (bottom)
        if (gl_y < 80) {
            float btn_w = window_width / 4;
            int ax = x / btn_w;
            switch (ax) {
                case 0: feed_pet(); break;
                case 1: rest_pet(); break;
                case 2: train_pet(); break;
                case 3: 
                    if (pet_needs_cleaning) clean_pet();
                    else scold_pet();
                    break;
            }
            glutPostRedisplay();
            return;
        }
    }
}

// --- Input: Keyboard ---
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'f': case 'F': feed_pet(); break;
        case 'r': case 'R': rest_pet(); break;
        case 't': case 'T':
            if (train_focus == 1) train_focus = 2;
            else if (train_focus == 2) train_focus = 3;
            else if (train_focus == 3) train_focus = 0;
            else train_focus = 1;
            const char* focus_name[] = {"None", "Strength", "Intelligence", "Cuteness"};
            set_status_message("🎯 Training focus: %s", focus_name[train_focus]);
            break;
        case 'c': case 'C': clean_pet(); break;
        case 's': case 'S': scold_pet(); break;
        case 'b': case 'B':
            if (money >= 20) {
                money -= 20;
                items_food += 5;
                set_status_message("🛒 Bought 5 food! Money: %d", money);
                print_pet_status();
            } else {
                set_status_message("💸 Not enough money to buy food!");
            }
            break;
        case 'i': case 'I':
            show_stats_popup = !show_stats_popup;
            set_status_message("📊 Stats: %s", show_stats_popup ? "ON" : "OFF");
            break;
        case 13:  // Enter = sleep
            end_day();
            break;
        case 'g':
            debug_ui_enabled = !debug_ui_enabled;
            set_status_message("UI Debug: %s", debug_ui_enabled ? "ON" : "OFF");
            break;
    }
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_LEFT:  if (cursor_col > 0) cursor_col--; break;
        case GLUT_KEY_RIGHT: if (cursor_col < GRID_COLS - 1) cursor_col++; break;
    }
    glutPostRedisplay();
}

// --- Drawing ---
void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void display() {
    // Day-night cycle
    if (day % 2 == 0) {
        background_color[0] = 0.9f; background_color[1] = 0.9f; background_color[2] = 1.0f;
    } else {
        background_color[0] = 0.1f; background_color[1] = 0.1f; background_color[2] = 0.2f;
    }
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float tile_area_top = MENU_Y - 80;

    // === MENU BAR ===
    float menu_bg[3] = {0.2f, 0.2f, 0.4f};
    draw_rect(0, MENU_Y, window_width, MENU_HEIGHT, menu_bg);

    render_text(" File ", 10, MENU_Y + 4);
    render_text(" Train: ", 80, MENU_Y + 4);

    if (train_focus == 1) render_text("💪 Str", 160, MENU_Y + 4);
    else if (train_focus == 2) render_text("🧠 Int", 160, MENU_Y + 4);
    else if (train_focus == 3) render_text("💖 Cut", 160, MENU_Y + 4);
    else render_text("❌ None", 160, MENU_Y + 4);

    render_text(" Info ", 400, MENU_Y + 4);

    // === PET DISPLAY ===
    float pet_x = window_width / 2;
    float pet_y = tile_area_top - (GRID_ROWS / 2) * TILE_SIZE;

    unsigned int code;
    decode_utf8((const unsigned char*)get_pet_emoji(), &code);
    render_emoji(code, pet_x, pet_y);

    if (pet_needs_cleaning) {
        decode_utf8((const unsigned char*)EMOJI_DIRT, &code);
        render_emoji(code, pet_x - 30, pet_y - 30);
    }

    // === ACTION BUTTONS (bottom) ===
    float btn_w = window_width / 4;
    float btn_h = 40;
    float btn_y = 20;

    float btn_colors[4][3] = {
        {0.2f, 0.6f, 0.2f},
        {0.2f, 0.2f, 0.8f},
        {0.8f, 0.4f, 0.0f},
        {0.6f, 0.2f, 0.6f}
    };

    const char* labels[] = {"F: Feed", "R: Rest", "T: Train", pet_needs_cleaning ? "C: Clean" : "S: Scold"};

    for (int i = 0; i < 4; i++) {
        draw_rect(i * btn_w, btn_y, btn_w, btn_h, btn_colors[i]);
        render_text(labels[i], i * btn_w + 10, btn_y + 12);
    }

    // === STATS POPUP ===
    if (show_stats_popup) {
        float overlay[4] = {0.0f, 0.0f, 0.0f, 0.7f};
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4fv(overlay);
        glBegin(GL_QUADS);
        glVertex2f(50, 50);
        glVertex2f(window_width - 50, 50);
        glVertex2f(window_width - 50, window_height - MENU_HEIGHT - 50);
        glVertex2f(50, window_height - MENU_HEIGHT - 50);
        glEnd();
        glDisable(GL_BLEND);

        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(50, 50);
        glVertex2f(window_width - 50, 50);
        glVertex2f(window_width - 50, window_height - MENU_HEIGHT - 50);
        glVertex2f(50, window_height - MENU_HEIGHT - 50);
        glEnd();

        float tx = 60, ty = window_height - MENU_HEIGHT - 70;
        char buf[100];

        snprintf(buf, sizeof(buf), "🐣 Pet Stats");
        render_text(buf, tx, ty); ty -= 25;

        #define STAT(var) do { \
            snprintf(buf, sizeof(buf), #var ": %d", var); \
            render_text(buf, tx, ty); \
            ty -= 20; \
        } while(0)

        STAT(age);
        STAT(health);
        STAT(hunger);
        STAT(energy);
        STAT(happiness);
        STAT(strength);
        STAT(intelligence);
        STAT(cuteness);

        snprintf(buf, sizeof(buf), "Money: $%d", money);
        render_text(buf, tx, ty); ty -= 20;

        snprintf(buf, sizeof(buf), "Food: %d", items_food);
        render_text(buf, tx, ty); ty -= 20;

        if (pet_needs_cleaning) {
            snprintf(buf, sizeof(buf), "💩 Pet needs cleaning!");
            render_text(buf, tx, ty); ty -= 20;
        }

        snprintf(buf, sizeof(buf), "Training: %s", 
            train_focus == 1 ? "💪 Strength" :
            train_focus == 2 ? "🧠 Intelligence" :
            train_focus == 3 ? "💖 Cuteness" : "❌ None");
        render_text(buf, tx, ty); ty -= 20;

        #undef STAT
    }

    // === BOTTOM PANEL ===
    float panel[3] = {0.1f, 0.1f, 0.2f};
    draw_rect(0, 0, window_width, 20, panel);
    char stats[256];
    snprintf(stats, sizeof(stats), "Age: %d | $%d | Food: %d", age, money, items_food);
    render_text(stats, 10, 2);

    render_text(status_message, 10, 60);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle() {}

void set_status_message(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    strncpy(status_message, buffer, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

void init() {
    initFreeType();
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
    srand(time(NULL));
    set_status_message("🐣 Your pet has hatched! 💖 Care for it daily.");
    print_pet_status();

    // Start terminal input thread
    pthread_create(&input_thread, NULL, terminal_input_handler, NULL);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Tamagotchi: Emoji Edition 💖");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();

    printf("🎮 Controls:\n");
    printf("  GUI: Click buttons or press F/R/T/C/S/B/I/Enter\n");
    printf("  Terminal: Type 'feed', 'sleep', 'info', etc.\n");
    printf("  Type 'exit' in terminal to quit.\n");

    glutMainLoop();

    running = false;
    pthread_join(input_thread, NULL);

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    return 0;
}
