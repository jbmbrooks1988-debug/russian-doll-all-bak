/*
 * HTML Canvas Framework for OpenGL/GLUT Applications - Main Prototype
 * 
 * Implements a basic HTML-like UI framework that renders with OpenGL/GLUT
 * Supports UI elements, events, and canvas rendering for 2D/3D games
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
} element_t;

// Global state
element_t elements[MAX_ELEMENTS];
int element_count = 0;
element_t *root_element = NULL;
int mouse_x = 0, mouse_y = 0;
int window_width = 800, window_height = 600;

// Function declarations
void init_elements();
element_t* create_element(const char* tag, const char* id);
void add_child(element_t* parent, element_t* child);
element_t* find_element_by_id(const char* id);
int parse_markup_file(const char* filename);
void render_element(element_t* element);
void render_all_elements();
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

// Basic event handlers
int button_click_handler(element_t* element) {
    printf("Button clicked: %s\n", element->id);
    return 0;
}

int button_hover_handler(element_t* element) {
    printf("Button hovered: %s\n", element->id);
    return 0;
}

// Parse simple markup format (simplified version)
int parse_markup_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Cannot open markup file: %s\n", filename);
        return -1;
    }
    
    // Create a simple test UI structure
    root_element = create_element("div", "root");
    
    // Create a header
    element_t* header = create_element("div", "header");
    header->pos[0] = 0.0f;
    header->pos[1] = 0.0f;
    header->pos[2] = window_width;
    header->pos[3] = 50.0f;
    header->bg_color[0] = 0.2f;
    header->bg_color[1] = 0.2f;
    header->bg_color[2] = 0.2f;
    add_child(root_element, header);
    
    // Create a menu button
    element_t* menu_button = create_element("button", "menu_btn");
    menu_button->pos[0] = 10.0f;
    menu_button->pos[1] = 10.0f;
    menu_button->pos[2] = 100.0f;
    menu_button->pos[3] = 30.0f;
    menu_button->bg_color[0] = 0.4f;
    menu_button->bg_color[1] = 0.4f;
    menu_button->bg_color[2] = 0.8f;
    strcpy(menu_button->text, "Menu");
    menu_button->onclick_handler = button_click_handler;
    menu_button->onhover_handler = button_hover_handler;
    add_child(header, menu_button);
    
    // Create canvas area
    element_t* canvas = create_element("canvas", "game_canvas");
    canvas->pos[0] = 0.0f;
    canvas->pos[1] = 50.0f;
    canvas->pos[2] = window_width;
    canvas->pos[3] = window_height - 50.0f;
    canvas->bg_color[0] = 0.1f;
    canvas->bg_color[1] = 0.1f;
    canvas->bg_color[2] = 0.1f;
    strcpy(canvas->text, ""); // No text for canvas
    add_child(root_element, canvas);
    
    fclose(file);
    return 0;
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
        glRasterPos2f(x + 10, y + height/2 + 5);
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
        render_element(root_element);
    }
    
    // Render a simple 2D/3D game scene in the canvas area
    element_t* canvas = find_element_by_id("game_canvas");
    if (canvas) {
        // Example 2D rendering in canvas area
        float x = canvas->pos[0];
        float y = window_height - canvas->pos[1] - canvas->pos[3];
        float width = canvas->pos[2];
        float height = canvas->pos[3];
        
        // Draw a simple 2D game element
        glColor3f(0.0f, 1.0f, 0.0f); // Green square
        glBegin(GL_QUADS);
        glVertex2f(x + width/2 - 25, y + height/2 - 25);
        glVertex2f(x + width/2 + 25, y + height/2 - 25);
        glVertex2f(x + width/2 + 25, y + height/2 + 25);
        glVertex2f(x + width/2 - 25, y + height/2 + 25);
        glEnd();
        
        // Draw a simple rotating triangle to demo 3D capability
        glPushMatrix();
        glTranslatef(x + width - 100, y + height - 100, 0);
        static float rotation = 0.0f;
        rotation += 1.0f;
        glRotatef(rotation, 0.0f, 0.0f, 1.0f);
        
        glColor3f(1.0f, 0.0f, 0.0f); // Red triangle
        glBegin(GL_TRIANGLES);
        glVertex2f(0.0f, 30.0f);
        glVertex2f(-26.0f, -15.0f);
        glVertex2f(26.0f, -15.0f);
        glEnd();
        glPopMatrix();
    }
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
    glutPostRedisplay();
}

void cleanup() {
    printf("Cleaning up...\n");
}

// External module execution (simplified)
int call_external_module(const char* module_name, const char* input_data, char* output_buffer) {
    char command[512];
    snprintf(command, sizeof(command), "./%s", module_name);
    
    FILE* pipe = popen(command, "w");
    if (!pipe) return -1;
    
    fprintf(pipe, "%s", input_data);
    pclose(pipe);
    
    return 0;
}

// Main function
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("HTML Canvas Framework - Prototype 0");
    
    // Initialize OpenGL settings
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);  // Disable lighting for 2D UI rendering
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Initialize elements
    init_elements();
    
    // Parse markup file if provided, otherwise use default
    if (argc > 1) {
        if (parse_markup_file(argv[1]) != 0) {
            printf("Using default UI structure\n");
        }
    } else {
        printf("No markup file provided, using default UI structure\n");
        parse_markup_file(NULL); // Use default structure
    }
    
    // Register GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(handle_mouse);
    glutMotionFunc(handle_motion);
    glutKeyboardFunc(handle_keyboard);
    glutIdleFunc(idle);
    
    printf("HTML Canvas Framework - Prototype 0\n");
    printf("Features:\n");
    printf("- HTML-like UI elements (div, button, canvas)\n");
    printf("- Mouse interaction (click, hover)\n");
    printf("- Basic 2D rendering in canvas\n");
    printf("- Simple 3D rendering example\n");
    printf("- Relative positioning system\n");
    printf("- External module integration\n");
    printf("\nControls:\n");
    printf("- Click buttons to see events in console\n");
    printf("- Hover over elements to see highlighting\n");
    printf("- Press ESC to exit\n");
    
    glutMainLoop();
    return 0;
}

// Initialize elements
void init_elements() {
    element_count = 0;
    root_element = NULL;
}