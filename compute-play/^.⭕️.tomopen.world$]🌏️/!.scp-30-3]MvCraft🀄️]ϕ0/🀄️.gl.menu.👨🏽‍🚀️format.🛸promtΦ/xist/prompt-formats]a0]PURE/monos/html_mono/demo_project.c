/*
 * Demo Project: Simple Game Menu System
 * Using the HTML Canvas Framework for OpenGL/GLUT
 * 
 * This demo shows a complete game interface with:
 * - Menu system with multiple screens
 * - Interactive buttons with hover effects
 * - Game canvas for 2D/3D rendering
 * - Status indicators
 * - External module integration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <GL/glut.h>

#define MAX_ELEMENTS 256
#define MAX_CHILDREN 64
#define MAX_ID_LEN 64
#define MAX_TEXT_LEN 256

// UI Element structure
typedef struct element_t {
    char id[MAX_ID_LEN];
    char tag[32];
    float pos[4]; // x, y, width, height
    float bg_color[4]; // r, g, b, a
    float fg_color[4]; // r, g, b, a
    char text[MAX_TEXT_LEN];
    int (*onclick_handler)(struct element_t* element);
    int (*onhover_handler)(struct element_t* element);
    struct element_t *children[MAX_CHILDREN];
    int child_count;
    int visible;
    int enabled;
    int hovered;
    int state; // for custom states like toggled, etc.
} element_t;

// Global state
element_t elements[MAX_ELEMENTS];
int element_count = 0;
element_t *root_element = NULL;
int mouse_x = 0, mouse_y = 0;
int window_width = 1024, window_height = 768;
int current_screen = 0; // 0=main menu, 1=game, 2=settings
float game_time = 0.0f;
float player_x = 0.0f, player_y = 0.0f;

// Function declarations
element_t* create_element(const char* tag, const char* id);
void add_child(element_t* parent, element_t* child);
element_t* find_element_by_id(const char* id);
void init_demo_ui();
void render_element(element_t* element);
void render_all_elements();
void render_game_scene();
void handle_mouse(int button, int state, int x, int y);
void handle_motion(int x, int y);
void handle_keyboard(unsigned char key, int x, int y);
void display();
void reshape(int w, int h);
void idle();
void cleanup();

// Element creation and management
element_t* create_element(const char* tag, const char* id) {
    if (element_count >= MAX_ELEMENTS) {
        fprintf(stderr, "Maximum element count reached\n");
        return NULL;
    }
    
    element_t* elem = &elements[element_count++];
    strcpy(elem->tag, tag);
    strcpy(elem->id, id);
    
    // Default values
    elem->pos[0] = 0.0f; // x
    elem->pos[1] = 0.0f; // y
    elem->pos[2] = 100.0f; // width
    elem->pos[3] = 50.0f; // height
    elem->bg_color[0] = 0.5f; // r
    elem->bg_color[1] = 0.5f; // g
    elem->bg_color[2] = 0.5f; // b
    elem->bg_color[3] = 1.0f; // a
    elem->fg_color[0] = 1.0f; // r
    elem->fg_color[1] = 1.0f; // g
    elem->fg_color[2] = 1.0f; // b
    elem->fg_color[3] = 1.0f; // a
    strcpy(elem->text, "");
    elem->onclick_handler = NULL;
    elem->onhover_handler = NULL;
    elem->child_count = 0;
    elem->visible = 1;
    elem->enabled = 1;
    elem->hovered = 0;
    elem->state = 0;
    
    return elem;
}

void add_child(element_t* parent, element_t* child) {
    if (parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = child;
    }
}

element_t* find_element_by_id(const char* id) {
    for (int i = 0; i < element_count; i++) {
        if (strcmp(elements[i].id, id) == 0) {
            return &elements[i];
        }
    }
    return NULL;
}

// Event handlers
int start_game_handler(element_t* element) {
    printf("Starting game from button: %s\n", element->id);
    current_screen = 1; // Switch to game screen
    return 0;
}

int settings_handler(element_t* element) {
    printf("Opening settings from button: %s\n", element->id);
    current_screen = 2; // Switch to settings screen
    return 0;
}

int exit_handler(element_t* element) {
    printf("Exiting from button: %s\n", element->id);
    cleanup();
    exit(0);
    return 0;
}

int back_handler(element_t* element) {
    printf("Going back from button: %s\n", element->id);
    current_screen = 0; // Return to main menu
    return 0;
}

int button_hover_handler(element_t* element) {
    // Print message only if not already printed for this hover
    if (!element->hovered) {
        printf("Hovering over: %s\n", element->id);
        element->hovered = 1;
    }
    return 0;
}

// Initialize the demo UI
void init_demo_ui() {
    // Create root container
    root_element = create_element("div", "root_container");
    root_element->pos[0] = 0.0f;
    root_element->pos[1] = 0.0f;
    root_element->pos[2] = window_width;
    root_element->pos[3] = window_height;
    root_element->bg_color[0] = 0.1f;
    root_element->bg_color[1] = 0.1f;
    root_element->bg_color[2] = 0.1f;

    // Main Menu Screen (current_screen == 0)
    element_t* main_menu = create_element("div", "main_menu");
    main_menu->pos[0] = 0.0f;
    main_menu->pos[1] = 0.0f;
    main_menu->pos[2] = window_width;
    main_menu->pos[3] = window_height;
    main_menu->bg_color[0] = 0.15f;
    main_menu->bg_color[1] = 0.15f;
    main_menu->bg_color[2] = 0.2f;
    add_child(root_element, main_menu);

    // Title
    element_t* title = create_element("div", "title");
    title->pos[0] = window_width/2 - 150;
    title->pos[1] = 100;
    title->pos[2] = 300;
    title->pos[3] = 80;
    title->bg_color[0] = 0.0f;
    title->bg_color[1] = 0.0f;
    title->bg_color[2] = 0.0f;
    strcpy(title->text, "AWESOME GAME");
    title->fg_color[0] = 1.0f;
    title->fg_color[1] = 1.0f;
    title->fg_color[2] = 0.5f;
    add_child(main_menu, title);

    // Start button
    element_t* start_btn = create_element("button", "start_btn");
    start_btn->pos[0] = window_width/2 - 75;
    start_btn->pos[1] = 250;
    start_btn->pos[2] = 150;
    start_btn->pos[3] = 50;
    start_btn->bg_color[0] = 0.3f;
    start_btn->bg_color[1] = 0.6f;
    start_btn->bg_color[2] = 0.3f;
    strcpy(start_btn->text, "START GAME");
    start_btn->onclick_handler = start_game_handler;
    start_btn->onhover_handler = button_hover_handler;
    add_child(main_menu, start_btn);

    // Settings button
    element_t* settings_btn = create_element("button", "settings_btn");
    settings_btn->pos[0] = window_width/2 - 75;
    settings_btn->pos[1] = 320;
    settings_btn->pos[2] = 150;
    settings_btn->pos[3] = 50;
    settings_btn->bg_color[0] = 0.4f;
    settings_btn->bg_color[1] = 0.4f;
    settings_btn->bg_color[2] = 0.7f;
    strcpy(settings_btn->text, "SETTINGS");
    settings_btn->onclick_handler = settings_handler;
    settings_btn->onhover_handler = button_hover_handler;
    add_child(main_menu, settings_btn);

    // Exit button
    element_t* exit_btn = create_element("button", "exit_btn");
    exit_btn->pos[0] = window_width/2 - 75;
    exit_btn->pos[1] = 390;
    exit_btn->pos[2] = 150;
    exit_btn->pos[3] = 50;
    exit_btn->bg_color[0] = 0.7f;
    exit_btn->bg_color[1] = 0.3f;
    exit_btn->bg_color[2] = 0.3f;
    strcpy(exit_btn->text, "EXIT");
    exit_btn->onclick_handler = exit_handler;
    exit_btn->onhover_handler = button_hover_handler;
    add_child(main_menu, exit_btn);

    // Game Screen (current_screen == 1)
    element_t* game_screen = create_element("div", "game_screen");
    game_screen->pos[0] = 0.0f;
    game_screen->pos[1] = 0.0f;
    game_screen->pos[2] = window_width;
    game_screen->pos[3] = window_height;
    game_screen->bg_color[0] = 0.0f;
    game_screen->bg_color[1] = 0.0f;
    game_screen->bg_color[2] = 0.0f;
    game_screen->visible = 0; // Initially invisible
    add_child(root_element, game_screen);

    // Game canvas
    element_t* game_canvas = create_element("canvas", "game_canvas");
    game_canvas->pos[0] = 0.0f;
    game_canvas->pos[1] = 0.0f;
    game_canvas->pos[2] = window_width - 200;
    game_canvas->pos[3] = window_height;
    game_canvas->bg_color[0] = 0.05f;
    game_canvas->bg_color[1] = 0.05f;
    game_canvas->bg_color[2] = 0.1f;
    add_child(game_screen, game_canvas);

    // Game UI sidebar
    element_t* game_sidebar = create_element("div", "game_sidebar");
    game_sidebar->pos[0] = window_width - 200;
    game_sidebar->pos[1] = 0.0f;
    game_sidebar->pos[2] = 200;
    game_sidebar->pos[3] = window_height;
    game_sidebar->bg_color[0] = 0.15f;
    game_sidebar->bg_color[1] = 0.15f;
    game_sidebar->bg_color[2] = 0.2f;
    add_child(game_screen, game_sidebar);

    // Health bar
    element_t* health_bg = create_element("div", "health_bg");
    health_bg->pos[0] = window_width - 180;
    health_bg->pos[1] = 20;
    health_bg->pos[2] = 160;
    health_bg->pos[3] = 30;
    health_bg->bg_color[0] = 0.3f;
    health_bg->bg_color[1] = 0.0f;
    health_bg->bg_color[2] = 0.0f;
    add_child(game_sidebar, health_bg);

    element_t* health_fill = create_element("div", "health_fill");
    health_fill->pos[0] = window_width - 180;
    health_fill->pos[1] = 20;
    health_fill->pos[2] = 120; // 75% health
    health_fill->pos[3] = 30;
    health_fill->bg_color[0] = 0.8f;
    health_fill->bg_color[1] = 0.2f;
    health_fill->bg_color[2] = 0.2f;
    add_child(game_sidebar, health_fill);

    // Mana bar
    element_t* mana_bg = create_element("div", "mana_bg");
    mana_bg->pos[0] = window_width - 180;
    mana_bg->pos[1] = 70;
    mana_bg->pos[2] = 160;
    mana_bg->pos[3] = 30;
    mana_bg->bg_color[0] = 0.0f;
    mana_bg->bg_color[1] = 0.0f;
    mana_bg->bg_color[2] = 0.4f;
    add_child(game_sidebar, mana_bg);

    element_t* mana_fill = create_element("div", "mana_fill");
    mana_fill->pos[0] = window_width - 180;
    mana_fill->pos[1] = 70;
    mana_fill->pos[2] = 100; // 62.5% mana
    mana_fill->pos[3] = 30;
    mana_fill->bg_color[0] = 0.2f;
    mana_fill->bg_color[1] = 0.2f;
    mana_fill->bg_color[2] = 0.9f;
    add_child(game_sidebar, mana_fill);

    // Back to menu button (in game)
    element_t* game_back_btn = create_element("button", "game_back_btn");
    game_back_btn->pos[0] = window_width - 180;
    game_back_btn->pos[1] = window_height - 60;
    game_back_btn->pos[2] = 160;
    game_back_btn->pos[3] = 40;
    game_back_btn->bg_color[0] = 0.5f;
    game_back_btn->bg_color[1] = 0.3f;
    game_back_btn->bg_color[2] = 0.7f;
    strcpy(game_back_btn->text, "BACK TO MENU");
    game_back_btn->onclick_handler = back_handler;
    game_back_btn->onhover_handler = button_hover_handler;
    add_child(game_screen, game_back_btn);

    // Settings Screen (current_screen == 2)
    element_t* settings_screen = create_element("div", "settings_screen");
    settings_screen->pos[0] = 0.0f;
    settings_screen->pos[1] = 0.0f;
    settings_screen->pos[2] = window_width;
    settings_screen->pos[3] = window_height;
    settings_screen->bg_color[0] = 0.1f;
    settings_screen->bg_color[1] = 0.15f;
    settings_screen->bg_color[2] = 0.15f;
    settings_screen->visible = 0; // Initially invisible
    add_child(root_element, settings_screen);

    // Settings title
    element_t* settings_title = create_element("div", "settings_title");
    settings_title->pos[0] = window_width/2 - 100;
    settings_title->pos[1] = 80;
    settings_title->pos[2] = 200;
    settings_title->pos[3] = 50;
    settings_title->bg_color[0] = 0.0f;
    settings_title->bg_color[1] = 0.0f;
    settings_title->bg_color[2] = 0.0f;
    strcpy(settings_title->text, "SETTINGS");
    settings_title->fg_color[0] = 0.8f;
    settings_title->fg_color[1] = 0.8f;
    settings_title->fg_color[2] = 1.0f;
    add_child(settings_screen, settings_title);

    // Resolution setting
    element_t* res_label = create_element("div", "res_label");
    res_label->pos[0] = window_width/2 - 150;
    res_label->pos[1] = 180;
    res_label->pos[2] = 100;
    res_label->pos[3] = 30;
    res_label->bg_color[0] = 0.0f;
    res_label->bg_color[1] = 0.0f;
    res_label->bg_color[2] = 0.0f;
    strcpy(res_label->text, "Resolution:");
    res_label->fg_color[0] = 1.0f;
    res_label->fg_color[1] = 1.0f;
    res_label->fg_color[2] = 1.0f;
    add_child(settings_screen, res_label);

    element_t* res_value = create_element("div", "res_value");
    res_value->pos[0] = window_width/2 + 50;
    res_value->pos[1] = 180;
    res_value->pos[2] = 150;
    res_value->pos[3] = 30;
    res_value->bg_color[0] = 0.2f;
    res_value->bg_color[1] = 0.2f;
    res_value->bg_color[2] = 0.3f;
    strcpy(res_value->text, "1024x768");
    res_value->fg_color[0] = 0.9f;
    res_value->fg_color[1] = 0.9f;
    res_value->fg_color[2] = 0.9f;
    add_child(settings_screen, res_value);

    // Back button (in settings)
    element_t* settings_back_btn = create_element("button", "settings_back_btn");
    settings_back_btn->pos[0] = window_width/2 - 75;
    settings_back_btn->pos[1] = 400;
    settings_back_btn->pos[2] = 150;
    settings_back_btn->pos[3] = 50;
    settings_back_btn->bg_color[0] = 0.7f;
    settings_back_btn->bg_color[1] = 0.5f;
    settings_back_btn->bg_color[2] = 0.3f;
    strcpy(settings_back_btn->text, "BACK");
    settings_back_btn->onclick_handler = back_handler;
    settings_back_btn->onhover_handler = button_hover_handler;
    add_child(settings_screen, settings_back_btn);
}

// Rendering functions
void render_element(element_t* element) {
    if (!element->visible) return;
    
    // Convert element coordinates to OpenGL coordinates
    float x = element->pos[0];
    float y = window_height - element->pos[1] - element->pos[3]; // Flip Y coordinate
    float width = element->pos[2];
    float height = element->pos[3];
    
    // Draw background
    glColor4f(
        element->hovered ? element->bg_color[0] * 1.2f : element->bg_color[0],
        element->hovered ? element->bg_color[1] * 1.2f : element->bg_color[1], 
        element->hovered ? element->bg_color[2] * 1.2f : element->bg_color[2],
        element->bg_color[3]
    );
    
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
    
    // Draw border if it's a button
    if (strcmp(element->tag, "button") == 0) {
        glColor3f(1.0f, 1.0f, 1.0f); // White border
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + width, y);
        glVertex2f(x + width, y + height);
        glVertex2f(x, y + height);
        glEnd();
        glLineWidth(1.0f);
    }
    
    // Draw text if element has text
    if (strlen(element->text) > 0) {
        glColor3f(element->fg_color[0], element->fg_color[1], element->fg_color[2]);
        
        // Simple text rendering using bitmap fonts
        glRasterPos2f(x + (width - strlen(element->text) * 8) / 2, y + height/2 + 5);
        for (int i = 0; i < strlen(element->text); i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, element->text[i]);
        }
    }
    
    // Render children
    for (int i = 0; i < element->child_count; i++) {
        render_element(element->children[i]);
    }
}

void render_all_elements() {
    if (root_element) {
        // Show/hide screens based on current screen
        element_t* main_menu = find_element_by_id("main_menu");
        element_t* game_screen = find_element_by_id("game_screen");
        element_t* settings_screen = find_element_by_id("settings_screen");
        
        if (main_menu) main_menu->visible = (current_screen == 0);
        if (game_screen) game_screen->visible = (current_screen == 1);
        if (settings_screen) settings_screen->visible = (current_screen == 2);
        
        render_element(root_element);
    }
    
    // Render game-specific content when on game screen
    if (current_screen == 1) {
        render_game_scene();
    }
}

void render_game_scene() {
    element_t* canvas = find_element_by_id("game_canvas");
    if (!canvas) return;
    
    float x = canvas->pos[0];
    float y = window_height - canvas->pos[1] - canvas->pos[3];
    float width = canvas->pos[2];
    float height = canvas->pos[3];
    
    // Draw game elements within canvas
    glColor3f(0.0f, 0.5f, 0.0f); // Green ground
    glBegin(GL_QUADS);
    glVertex2f(x, y + height * 0.8f);
    glVertex2f(x + width, y + height * 0.8f);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
    
    // Draw a simple player avatar
    glColor3f(0.8f, 0.1f, 0.5f); // Pink player
    float player_screen_x = x + width/2 + sin(game_time) * 100;
    float player_screen_y = y + height * 0.7f;
    glBegin(GL_QUADS);
    glVertex2f(player_screen_x - 15, player_screen_y - 15);
    glVertex2f(player_screen_x + 15, player_screen_y - 15);
    glVertex2f(player_screen_x + 15, player_screen_y + 15);
    glVertex2f(player_screen_x - 15, player_screen_y + 15);
    glEnd();
    
    // Draw a moving enemy
    glColor3f(0.9f, 0.3f, 0.2f); // Red enemy
    float enemy_x = x + 100 + fmod(game_time * 50, width - 200);
    float enemy_y = y + height * 0.5f;
    glBegin(GL_TRIANGLES);
    glVertex2f(enemy_x, enemy_y + 15);
    glVertex2f(enemy_x - 15, enemy_y - 15);
    glVertex2f(enemy_x + 15, enemy_y - 15);
    glEnd();
    
    // Draw some static obstacles
    glColor3f(0.6f, 0.6f, 0.2f); // Brown obstacle
    glBegin(GL_QUADS);
    glVertex2f(x + 200, y + height * 0.75f - 30);
    glVertex2f(x + 250, y + height * 0.75f - 30);
    glVertex2f(x + 250, y + height * 0.75f);
    glVertex2f(x + 200, y + height * 0.75f);
    glEnd();
    
    glBegin(GL_QUADS);
    glVertex2f(x + 600, y + height * 0.6f - 20);
    glVertex2f(x + 700, y + height * 0.6f - 20);
    glVertex2f(x + 700, y + height * 0.6f);
    glVertex2f(x + 600, y + height * 0.6f);
    glEnd();
}

// Event handling
void handle_mouse(int button, int state, int x, int y) {
    mouse_x = x;
    mouse_y = y;
    
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Check if any element was clicked
        for (int i = 0; i < element_count; i++) {
            element_t* elem = &elements[i];
            if (!elem->visible || !elem->enabled) continue;
            
            float elem_x = elem->pos[0];
            float elem_y = elem->pos[1];
            float elem_w = elem->pos[2];
            float elem_h = elem->pos[3];
            
            // Check if click is within element bounds
            if (x >= elem_x && x <= elem_x + elem_w && 
                (window_height - y) >= elem_y && (window_height - y) <= elem_y + elem_h) {
                
                if (elem->onclick_handler) {
                    elem->onclick_handler(elem);
                }
            }
        }
    }
}

void handle_motion(int x, int y) {
    mouse_x = x;
    mouse_y = y;
    
    // Reset hover state for all elements
    for (int i = 0; i < element_count; i++) {
        elements[i].hovered = 0;
    }
    
    // Check if any element is being hovered over
    for (int i = 0; i < element_count; i++) {
        element_t* elem = &elements[i];
        if (!elem->visible || !elem->enabled) continue;
        
        float elem_x = elem->pos[0];
        float elem_y = elem->pos[1];
        float elem_w = elem->pos[2];
        float elem_h = elem->pos[3];
        
        // Check if mouse is within element bounds
        if (x >= elem_x && x <= elem_x + elem_w && 
            (window_height - y) >= elem_y && (window_height - y) <= elem_y + elem_h) {
            
            elem->hovered = 1;
            if (elem->onhover_handler) {
                elem->onhover_handler(elem);
            }
        }
    }
    
    glutPostRedisplay();
}

void handle_keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: // ESC key
            cleanup();
            exit(0);
            break;
        case ' ': // Space bar to switch screens in demo
            current_screen = (current_screen + 1) % 3;
            glutPostRedisplay();
            break;
        default:
            break;
    }
}

// GLUT callbacks
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    render_all_elements();
    
    glutSwapBuffers();
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void idle() {
    // Update game time for animations
    game_time += 0.02f;
    
    glutPostRedisplay();
}

void cleanup() {
    printf("Demo cleanup completed.\n");
}

// Main function
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("CHTML Framework Demo - Game Menu System");
    
    // Initialize OpenGL settings
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);  // Disable lighting for 2D UI rendering
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Initialize UI elements
    init_demo_ui();
    
    // Register GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(handle_mouse);
    glutMotionFunc(handle_motion);
    glutKeyboardFunc(handle_keyboard);
    glutIdleFunc(idle);
    
    printf("CHTML Framework Demo - Game Menu System\n");
    printf("Features:\n");
    printf("- Multi-screen interface (Main Menu, Game, Settings)\n");
    printf("- Interactive buttons with hover effects\n");
    printf("- Dynamic game rendering with animation\n");
    printf("- Health/Mana indicators\n");
    printf("- Sidebar UI with game controls\n");
    printf("- Screen switching functionality\n");
    printf("\nControls:\n");
    printf("- Click UI elements to interact\n");
    printf("- Press SPACE to cycle through screens (demo)\n");
    printf("- Press ESC to exit\n");
    
    glutMainLoop();
    return 0;
}