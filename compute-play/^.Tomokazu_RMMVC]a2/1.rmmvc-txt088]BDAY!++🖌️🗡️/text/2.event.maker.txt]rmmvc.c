#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>

// Event menu item structure - refactored to use arrays for beginner readability
#define EVENT_MENU_ITEM_LABEL_SIZE 64
#define EVENT_MENU_ITEM_FIELDS 4

// Text input dialog structure
typedef struct {
    int x, y;           // Position of the dialog
    int width, height;  // Dimensions of the dialog
    bool visible;       // Whether the dialog is visible
    char input_buffer[512];  // Buffer for text input
    int input_cursor;   // Current cursor position
    int input_length;   // Length of the input text
    bool confirmed;     // Whether the user confirmed the input
} TextInputDialog;

// External declaration for current_project_path
extern char current_project_path[256];

// External declaration for current_map_file
extern char current_map_file[256];

// Function declarations for map file functions
int scan_map_files(const char* project_path);
void free_map_files_list(void);

// Event menu item fields:
// [0] = label (char array)
// [1] = id (int)
// [2] = is_category (bool) 
// [3] = page (int)

// Helper functions to access event menu item fields
char* get_event_menu_item_label(void* item) {
    return (char*)item;
}

int get_event_menu_item_id(void* item) {
    return *((int*)((char*)item + EVENT_MENU_ITEM_LABEL_SIZE));
}

bool get_event_menu_item_is_category(void* item) {
    return *((bool*)((char*)item + EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int)));
}

int get_event_menu_item_page(void* item) {
    return *((int*)((char*)item + EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool)));
}

void set_event_menu_item_label(void* item, const char* label) {
    strncpy((char*)item, label, EVENT_MENU_ITEM_LABEL_SIZE - 1);
    ((char*)item)[EVENT_MENU_ITEM_LABEL_SIZE - 1] = '\0';
}

void set_event_menu_item_id(void* item, int id) {
    *((int*)((char*)item + EVENT_MENU_ITEM_LABEL_SIZE)) = id;
}

void set_event_menu_item_is_category(void* item, bool is_category) {
    *((bool*)((char*)item + EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int))) = is_category;
}

void set_event_menu_item_page(void* item, int page) {
    *((int*)((char*)item + EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool))) = page;
}

// Event menu structure - keeping as struct since it's more complex
typedef struct {
    int x, y;           // Position of the menu
    int width, height;  // Dimensions of the menu
    int item_count;
    void* items;        // Pointer to array of menu items (using byte array instead of struct)
    int selected_item;
    bool visible;
    int parent_x, parent_y;  // Position of parent menu for positioning
    int type;  // 0 = context menu, 1 = event editor, 2 = event commands, 3 = contents context menu
    int current_page; // For tabbed menus
} EventMenu;

// Maximum items per menu
#define MAX_MENU_ITEMS 200

// External declarations for drawing functions that are already defined in view_gl]3d]a0.c
extern void draw_rect(float x, float y, float w, float h, float color[3]);
extern void draw_border(float x, float y, float w, float h, float color[3]);
extern void render_text(const char* str, float x, float y);

// External declarations for event menu variables
extern EventMenu* main_event_menu;
extern EventMenu* event_area_menu;
extern EventMenu* event_commands_menu;
extern EventMenu* contents_context_menu;

// Global variable for text input dialog
TextInputDialog* text_input_dialog = NULL;

// Function declarations
EventMenu* create_main_event_menu(int parent_x, int parent_y);
EventMenu* create_event_area_menu(int parent_x, int parent_y);
EventMenu* create_event_commands_menu(int parent_x, int parent_y);
EventMenu* create_contents_context_menu(int parent_x, int parent_y);
void destroy_event_menu(EventMenu* menu);
void render_event_menu(EventMenu* menu);
void handle_event_menu_click(EventMenu* menu, int x, int y);
void show_event_menu(EventMenu* menu);
void hide_event_menu(EventMenu* menu);

// Text input dialog function declarations
TextInputDialog* create_text_input_dialog(int x, int y, int width, int height);
void destroy_text_input_dialog(TextInputDialog* dialog);
void render_text_input_dialog(TextInputDialog* dialog);
void handle_text_input_dialog_key(TextInputDialog* dialog, unsigned char key);
void handle_text_input_dialog_special(TextInputDialog* dialog, int key);
void show_text_input_dialog(TextInputDialog* dialog);
void hide_text_input_dialog(TextInputDialog* dialog);
void save_event_text_to_file(const char* text);

// Function to read event commands from file
int read_event_commands_file(const char* filename, void* items, int max_items, int page) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return 0;
    }
    
    char line[256];
    int item_count = 0;
    int in_category = 0;
    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
    
    // Skip the first line (header)
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    
    // Skip the second line (page indicators)
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    
    // Read the rest of the file
    while (fgets(line, sizeof(line), file) && item_count < max_items) {
        // Remove trailing newline and carriage return
        line[strcspn(line, "\n\r")] = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }
        
        // Check if this is a category header (ends with colon)
        if (line[strlen(line) - 1] == ':') {
            // Remove the colon
            line[strlen(line) - 1] = '\0';
            
            // Add category item
            void* item = (char*)items + item_count * item_size;
            set_event_menu_item_label(item, line);
            set_event_menu_item_id(item, item_count);
            set_event_menu_item_is_category(item, 1);
            set_event_menu_item_page(item, page);
            item_count++;
            in_category = 1;
        } else {
            // Add command item
            void* item = (char*)items + item_count * item_size;
            set_event_menu_item_label(item, line);
            set_event_menu_item_id(item, item_count);
            set_event_menu_item_is_category(item, 0);
            set_event_menu_item_page(item, page);
            item_count++;
        }
    }
    
    fclose(file);
    return item_count;
}

// Create main event menu (context menu)
EventMenu* create_main_event_menu(int parent_x, int parent_y) {
    EventMenu* menu = (EventMenu*)malloc(sizeof(EventMenu));
    if (!menu) return NULL;
    
    // Make the menu nearly full screen
    menu->x = 50;  // Leave some margin on the left
    menu->y = 50;  // Leave some margin at the top
    menu->width = 800;  // Nearly full width
    menu->height = 600; // Nearly full height
    menu->item_count = 0;
    menu->selected_item = -1;
    menu->visible = 0;
    menu->parent_x = parent_x;
    menu->parent_y = parent_y;
    menu->type = 0; // Context menu
    menu->current_page = 0;
    
    // Allocate items - using byte array instead of struct
    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
    menu->items = (void*)malloc(MAX_MENU_ITEMS * item_size);
    if (!menu->items) {
        free(menu);
        return NULL;
    }
    
    // Add context menu items
    void* item0 = (char*)menu->items + 0 * item_size;
    set_event_menu_item_label(item0, "New");
    set_event_menu_item_id(item0, 0);
    set_event_menu_item_is_category(item0, 0);
    set_event_menu_item_page(item0, 0);
    
    void* item1 = (char*)menu->items + 1 * item_size;
    set_event_menu_item_label(item1, "Edit");
    set_event_menu_item_id(item1, 1);
    set_event_menu_item_is_category(item1, 0);
    set_event_menu_item_page(item1, 0);
    
    void* item2 = (char*)menu->items + 2 * item_size;
    set_event_menu_item_label(item2, "Cut");
    set_event_menu_item_id(item2, 2);
    set_event_menu_item_is_category(item2, 0);
    set_event_menu_item_page(item2, 0);
    
    void* item3 = (char*)menu->items + 3 * item_size;
    set_event_menu_item_label(item3, "Copy");
    set_event_menu_item_id(item3, 3);
    set_event_menu_item_is_category(item3, 0);
    set_event_menu_item_page(item3, 0);
    
    void* item4 = (char*)menu->items + 4 * item_size;
    set_event_menu_item_label(item4, "Paste");
    set_event_menu_item_id(item4, 4);
    set_event_menu_item_is_category(item4, 0);
    set_event_menu_item_page(item4, 0);
    
    void* item5 = (char*)menu->items + 5 * item_size;
    set_event_menu_item_label(item5, "Delete");
    set_event_menu_item_id(item5, 5);
    set_event_menu_item_is_category(item5, 0);
    set_event_menu_item_page(item5, 0);
    
    void* item6 = (char*)menu->items + 6 * item_size;
    set_event_menu_item_label(item6, "Find");
    set_event_menu_item_id(item6, 6);
    set_event_menu_item_is_category(item6, 0);
    set_event_menu_item_page(item6, 0);
    
    void* item7 = (char*)menu->items + 7 * item_size;
    set_event_menu_item_label(item7, "Quick Event");
    set_event_menu_item_id(item7, 7);
    set_event_menu_item_is_category(item7, 0);
    set_event_menu_item_page(item7, 0);
    
    void* item8 = (char*)menu->items + 8 * item_size;
    set_event_menu_item_label(item8, "Set Starting Position");
    set_event_menu_item_id(item8, 8);
    set_event_menu_item_is_category(item8, 0);
    set_event_menu_item_page(item8, 0);
    
    menu->item_count = 9;
    
    return menu;
}

// Create event area menu (event editor)
EventMenu* create_event_area_menu(int parent_x, int parent_y) {
    EventMenu* menu = (EventMenu*)malloc(sizeof(EventMenu));
    if (!menu) return NULL;
    
    // Make the menu nearly full screen with space for system buttons
    menu->x = 50;   // Leave some margin on the left
    menu->y = 50;   // Leave some margin at the top
    menu->width = 800;  // Nearly full width
    menu->height = 600; // Nearly full height
    menu->item_count = 0;
    menu->selected_item = -1;
    menu->visible = 0;
    menu->parent_x = parent_x;
    menu->parent_y = parent_y;
    menu->type = 1; // Event editor
    menu->current_page = 0;
    
    // Allocate items - using byte array instead of struct
    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
    menu->items = (void*)malloc(MAX_MENU_ITEMS * item_size);
    if (!menu->items) {
        free(menu);
        return NULL;
    }
    
    // This will be populated dynamically when events are added
    menu->item_count = 0;
    
    return menu;
}

// Create event commands menu (grandchild)
EventMenu* create_event_commands_menu(int parent_x, int parent_y) {
    printf("Creating event commands menu: parent_x=%d, parent_y=%d\n", parent_x, parent_y);
    EventMenu* menu = (EventMenu*)malloc(sizeof(EventMenu));
    if (!menu) {
        printf("Failed to allocate menu\n");
        return NULL;
    }
    printf("Menu allocated: %p\n", menu);
    
    // Make the menu nearly full screen
    menu->x = 50;   // Leave some margin on the left
    menu->y = 50;   // Leave some margin at the top
    menu->width = 800;  // Nearly full width
    menu->height = 600; // Nearly full height
    menu->item_count = 0;
    menu->selected_item = -1;
    menu->visible = 0;
    menu->parent_x = parent_x;
    menu->parent_y = parent_y;
    menu->type = 2; // Commands menu
    menu->current_page = 1; // Default to page 1
    
    // Allocate items - using byte array instead of struct
    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
    menu->items = (void*)malloc(MAX_MENU_ITEMS * item_size);
    if (!menu->items) {
        printf("Failed to allocate items\n");
        free(menu);
        return NULL;
    }
    printf("Items allocated: %p\n", menu->items);
    
    // Read commands from files with proper bounds checking
    int page1_count = read_event_commands_file("menu/event.commands.1.txt", menu->items, MAX_MENU_ITEMS/3, 1);
    printf("Page 1 count: %d\n", page1_count);
    
    // Check if we have space for more items
    if (page1_count < MAX_MENU_ITEMS/3) {
        int page2_count = read_event_commands_file("menu/event.commands.2.txt", 
                                                  (char*)menu->items + page1_count * item_size, 
                                                  MAX_MENU_ITEMS/3, 2);
        printf("Page 2 count: %d\n", page2_count);
        
        // Check if we have space for more items
        if (page1_count + page2_count < 2 * (MAX_MENU_ITEMS/3)) {
            int remaining_space = MAX_MENU_ITEMS - page1_count - page2_count;
            int page3_count = read_event_commands_file("menu/event.commands.3.txt", 
                                                      (char*)menu->items + (page1_count + page2_count) * item_size,
                                                      remaining_space, 3);
            printf("Page 3 count: %d\n", page3_count);
            menu->item_count = page1_count + page2_count + page3_count;
        } else {
            menu->item_count = page1_count + page2_count;
        }
    } else {
        menu->item_count = page1_count;
    }
    
    printf("Total item count: %d\n", menu->item_count);
    return menu;
}

// Create contents context menu
EventMenu* create_contents_context_menu(int parent_x, int parent_y) {
    EventMenu* menu = (EventMenu*)malloc(sizeof(EventMenu));
    if (!menu) return NULL;
    
    // Make the menu nearly full screen
    menu->x = 50;  // Leave some margin on the left
    menu->y = 50;  // Leave some margin at the top
    menu->width = 800;  // Nearly full width
    menu->height = 600; // Nearly full height
    menu->item_count = 0;
    menu->selected_item = -1;
    menu->visible = 0;
    menu->parent_x = parent_x;
    menu->parent_y = parent_y;
    menu->type = 3; // Contents context menu
    menu->current_page = 0;
    
    // Allocate items - using byte array instead of struct
    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
    menu->items = (void*)malloc(MAX_MENU_ITEMS * item_size);
    if (!menu->items) {
        free(menu);
        return NULL;
    }
    
    // Add contents context menu items
    void* item0 = (char*)menu->items + 0 * item_size;
    set_event_menu_item_label(item0, "New");
    set_event_menu_item_id(item0, 0);
    set_event_menu_item_is_category(item0, 0);
    set_event_menu_item_page(item0, 0);
    
    void* item1 = (char*)menu->items + 1 * item_size;
    set_event_menu_item_label(item1, "Edit");
    set_event_menu_item_id(item1, 1);
    set_event_menu_item_is_category(item1, 0);
    set_event_menu_item_page(item1, 0);
    
    void* item2 = (char*)menu->items + 2 * item_size;
    set_event_menu_item_label(item2, "Copy");
    set_event_menu_item_id(item2, 2);
    set_event_menu_item_is_category(item2, 0);
    set_event_menu_item_page(item2, 0);
    
    void* item3 = (char*)menu->items + 3 * item_size;
    set_event_menu_item_label(item3, "Paste");
    set_event_menu_item_id(item3, 3);
    set_event_menu_item_is_category(item3, 0);
    set_event_menu_item_page(item3, 0);
    
    void* item4 = (char*)menu->items + 4 * item_size;
    set_event_menu_item_label(item4, "Delete");
    set_event_menu_item_id(item4, 4);
    set_event_menu_item_is_category(item4, 0);
    set_event_menu_item_page(item4, 0);
    
    void* item5 = (char*)menu->items + 5 * item_size;
    set_event_menu_item_label(item5, "Select All");
    set_event_menu_item_id(item5, 5);
    set_event_menu_item_is_category(item5, 0);
    set_event_menu_item_page(item5, 0);
    
    void* item6 = (char*)menu->items + 6 * item_size;
    set_event_menu_item_label(item6, "Test");
    set_event_menu_item_id(item6, 6);
    set_event_menu_item_is_category(item6, 0);
    set_event_menu_item_page(item6, 0);
    
    menu->item_count = 7;
    
    return menu;
}

EventMenu* create_maps_context_menu(int parent_x, int parent_y) {
    EventMenu* menu = (EventMenu*)malloc(sizeof(EventMenu));
    if (!menu) return NULL;
    
    // Make the menu nearly full screen
    menu->x = 50;  // Leave some margin on the left
    menu->y = 50;  // Leave some margin at the top
    menu->width = 800;  // Nearly full width
    menu->height = 600; // Nearly full height
    menu->item_count = 0;
    menu->selected_item = -1;
    menu->visible = 0;
    menu->parent_x = parent_x;
    menu->parent_y = parent_y;
    menu->type = 4; // Maps context menu
    menu->current_page = 0;
    
    // Allocate items - using byte array instead of struct
    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
    menu->items = (void*)malloc(MAX_MENU_ITEMS * item_size);
    if (!menu->items) {
        free(menu);
        return NULL;
    }
    
    // Add maps context menu items
    void* item0 = (char*)menu->items + 0 * item_size;
    set_event_menu_item_label(item0, "New");
    set_event_menu_item_id(item0, 0);
    set_event_menu_item_is_category(item0, 0);
    set_event_menu_item_page(item0, 0);
    
    void* item1 = (char*)menu->items + 1 * item_size;
    set_event_menu_item_label(item1, "Copy");
    set_event_menu_item_id(item1, 1);
    set_event_menu_item_is_category(item1, 0);
    set_event_menu_item_page(item1, 0);
    
    void* item2 = (char*)menu->items + 2 * item_size;
    set_event_menu_item_label(item2, "Paste");
    set_event_menu_item_id(item2, 2);
    set_event_menu_item_is_category(item2, 0);
    set_event_menu_item_page(item2, 0);
    
    void* item3 = (char*)menu->items + 3 * item_size;
    set_event_menu_item_label(item3, "Edit");
    set_event_menu_item_id(item3, 3);
    set_event_menu_item_is_category(item3, 0);
    set_event_menu_item_page(item3, 0);
    
    menu->item_count = 4;
    
    return menu;
}

// Destroy event menu
void destroy_event_menu(EventMenu* menu) {
    if (menu) {
        if (menu->items) {
            free(menu->items);
        }
        free(menu);
    }
}

// Show event menu
void show_event_menu(EventMenu* menu) {
    if (menu) {
        menu->visible = 1;
    }
}

// Hide event menu
void hide_event_menu(EventMenu* menu) {
    if (menu) {
        menu->visible = 0;
    }
}

// Render event menu
void render_event_menu(EventMenu* menu) {
    if (!menu || !menu->visible) return;
    
    // Background color
    float bg_color[3] = {0.2f, 0.2f, 0.2f};
    float border_color[3] = {0.5f, 0.5f, 0.5f};
    float item_color[3] = {0.3f, 0.3f, 0.3f};
    float selected_color[3] = {0.4f, 0.4f, 0.6f};
    float text_color[3] = {1.0f, 1.0f, 1.0f};
    
    // Draw menu background
    draw_rect(menu->x, menu->y, menu->width, menu->height, bg_color);
    draw_border(menu->x, menu->y, menu->width, menu->height, border_color);
    
    // Calculate layout dimensions
    int left_empty_width = menu->width / 3;  // Empty left third
    int right_content_width = menu->width - left_empty_width;  // Right two-thirds for content
    int content_x = menu->x + left_empty_width;
    
    // Draw a subtle separator line between empty left side and content area
    float separator_color[3] = {0.4f, 0.4f, 0.4f};
    draw_rect(content_x - 2, menu->y, 2, menu->height, separator_color);
    
    // Draw menu items in the right content area
    int item_height = 25;
    int title_height = 30;
    int content_y_start = menu->y + menu->height - title_height;
    
    // Draw menu title based on type
    glColor3fv(text_color);
    if (menu->type == 0) {
        render_text("Context Menu", content_x + 10, content_y_start + 5);
    } else if (menu->type == 1) {
        render_text("Event Editor", content_x + 10, content_y_start + 5);
    } else if (menu->type == 2) {
        render_text("Event Commands", content_x + 10, content_y_start + 5);
    } else if (menu->type == 3) {
        render_text("Contents Menu", content_x + 10, content_y_start + 5);
    } else if (menu->type == 4) {
        render_text("Maps Menu", content_x + 10, content_y_start + 5);
    }
    glColor3f(1.0f, 1.0f, 1.0f);

    // If this is the commands menu, draw page tabs
    if (menu->type == 2) {
        float tab_width = right_content_width / 3;
        float tab_y = content_y_start - 30; // Below title
        float tab_color[3] = {0.4f, 0.4f, 0.4f};
        float selected_tab_color[3] = {0.6f, 0.6f, 0.8f};

        for (int i = 1; i <= 3; i++) {
            float tab_x = content_x + (i - 1) * tab_width;
            draw_rect(tab_x, tab_y, tab_width, 30, (i == menu->current_page) ? selected_tab_color : tab_color);
            char tab_label[10];
            snprintf(tab_label, sizeof(tab_label), "Page %d", i);
            render_text(tab_label, tab_x + 10, tab_y + 10);
        }
        content_y_start -= 40; // Adjust content start position
    }
    
    // Draw menu items
    int visible_items = (content_y_start - menu->y) / item_height;
    int items_drawn = 0;
    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
    
    for (int i = 0; i < menu->item_count && items_drawn < visible_items - 1; i++) {
        // Bounds check
        if (i >= MAX_MENU_ITEMS) break;
        
        void* item = (char*)menu->items + i * item_size;
        if (menu->type == 2 && get_event_menu_item_page(item) != menu->current_page) {
            continue;
        }

        float item_y = content_y_start - (items_drawn + 1) * item_height;
        
        // Draw item background
        float* item_bg_color = (i == menu->selected_item) ? selected_color : item_color;
        draw_rect(content_x, item_y, right_content_width, item_height, item_bg_color);
        
        // Draw item text
        glColor3fv(text_color);
        if (get_event_menu_item_is_category(item)) {
            // Draw category with different styling
            render_text(get_event_menu_item_label(item), content_x + 15, item_y + 5);
        } else {
            render_text(get_event_menu_item_label(item), content_x + 25, item_y + 5);
        }
        glColor3f(1.0f, 1.0f, 1.0f);
        items_drawn++;
    }
}

// Handle event menu click
void handle_event_menu_click(EventMenu* menu, int x, int y) {
    if (!menu || !menu->visible) return;
    
    // Check if click is within menu bounds
    if (x >= menu->x && x <= menu->x + menu->width &&
        y >= menu->y && y <= menu->y + menu->height) {
        
        // Calculate layout dimensions
        int left_empty_width = menu->width / 3;  // Empty left third
        int content_x = menu->x + left_empty_width;
        int right_content_width = menu->width - left_empty_width;
        
        // Check if click is in the empty left side - if so, ignore it
        if (x < content_x) {
            return;  // Clicked in empty left area, do nothing
        }
        
        // Click is in the content area, process it
        int item_height = 25;
        int title_height = 30;
        int content_y_start = menu->y + menu->height - title_height;

        // Check for tab clicks in the commands menu
        if (menu->type == 2) {
            float tab_width = right_content_width / 3;
            float tab_y = content_y_start - 30;
            if (y >= tab_y && y <= tab_y + 30) {
                int clicked_tab = (x - content_x) / tab_width + 1;
                if (clicked_tab >= 1 && clicked_tab <= 3) {
                    menu->current_page = clicked_tab;
                    return; // Tab was clicked, no further processing needed
                }
            }
            content_y_start -= 40; // Adjust for item calculation
        }
        
        // Calculate which item was clicked
        if (y <= content_y_start && y >= menu->y) {  // Within menu bounds
            int adjusted_y = content_y_start - y;
            int item_index_visible = adjusted_y / item_height;

            int visible_item_count = 0;
            int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
            
            for (int i = 0; i < menu->item_count; i++) {
                // Bounds check
                if (i >= MAX_MENU_ITEMS) break;
                
                void* item = (char*)menu->items + i * item_size;
                if (menu->type == 2 && get_event_menu_item_page(item) != menu->current_page) {
                    continue;
                }

                if (visible_item_count == item_index_visible) {
                    menu->selected_item = i;
                    
                    // Handle item selection based on menu type
                    if (menu->type == 0) {
                        // Context menu
                        switch (i) {
                            case 0: // New
                                printf("New selected\n");
                                // Create and show the event editor menu when "New" is selected
                                // Hide other menus first
                                if (main_event_menu) hide_event_menu(main_event_menu);
                                
                                if (!event_area_menu) {
                                    event_area_menu = create_event_area_menu(menu->parent_x, menu->parent_y);
                                }
                                if (event_area_menu) {
                                    event_area_menu->x = menu->x + menu->width;
                                    event_area_menu->y = menu->y;
                                    // Make sure the menu is within the window bounds
                                    if (event_area_menu->x + event_area_menu->width > 1024) {
                                        event_area_menu->x = 1024 - event_area_menu->width;
                                    }
                                    if (event_area_menu->y + event_area_menu->height > 768) {
                                        event_area_menu->y = 768 - event_area_menu->height;
                                    }
                                    show_event_menu(event_area_menu);
                                    // Hide the context menu
                                    hide_event_menu(menu);
                                }
                                break;
                            case 1: // Edit
                                printf("Edit selected\n");
                                // Create and show the event editor menu when "Edit" is selected
                                // Hide other menus first
                                if (main_event_menu) hide_event_menu(main_event_menu);
                                
                                if (!event_area_menu) {
                                    event_area_menu = create_event_area_menu(menu->parent_x, menu->parent_y);
                                }
                                if (event_area_menu) {
                                    event_area_menu->x = menu->x + menu->width;
                                    event_area_menu->y = menu->y;
                                    // Make sure the menu is within the window bounds
                                    if (event_area_menu->x + event_area_menu->width > 1024) {
                                        event_area_menu->x = 1024 - event_area_menu->width;
                                    }
                                    if (event_area_menu->y + event_area_menu->height > 768) {
                                        event_area_menu->y = 768 - event_area_menu->height;
                                    }
                                    show_event_menu(event_area_menu);
                                    // Hide the context menu
                                    hide_event_menu(menu);
                                }
                                break;
                            case 2: // Cut
                                printf("Cut selected\n");
                                break;
                            case 3: // Copy
                                printf("Copy selected\n");
                                break;
                            case 4: // Paste
                                printf("Paste selected\n");
                                break;
                            case 5: // Delete
                                printf("Delete selected\n");
                                break;
                            case 6: // Find
                                printf("Find selected\n");
                                break;
                            case 7: // Quick Event
                                printf("Quick Event selected\n");
                                break;
                            case 8: // Set Starting Position
                                printf("Set Starting Position selected\n");
                                break;
                        }
                    } else if (menu->type == 1) {
                        // Event area - check if click is in the contents area (right side)
                        // For now, we'll just print a message
                        // In a real implementation, this would handle event selection
                        printf("Event %d selected\n", i);
                    } else if (menu->type == 2) {
                        // Commands menu
                        void* item = (char*)menu->items + i * item_size;
                        printf("Command '%s' selected (page %d)\n", get_event_menu_item_label(item), get_event_menu_item_page(item));
                        
                        // Check if this is the "Show Text..." command
                        if (strcmp(get_event_menu_item_label(item), "Show Text...") == 0) {
                            printf("Show Text command selected\n");
                            // Create and show text input dialog
                            if (!text_input_dialog) {
                                text_input_dialog = create_text_input_dialog(200, 200, 400, 200);
                            }
                            if (text_input_dialog) {
                                show_text_input_dialog(text_input_dialog);
                            }
                        }
                        
                        // Hide all menus when a command is selected (for now)
                        // In a real implementation, this would add the command to the event
                        if (main_event_menu) hide_event_menu(main_event_menu);
                        if (event_area_menu) hide_event_menu(event_area_menu);
                        if (event_commands_menu) hide_event_menu(event_commands_menu);
                    } else if (menu->type == 3) {
                        // Contents context menu
                        switch (i) {
                            case 0: // New
                                printf("Contents New selected\n");
                                // Create and show the event commands menu when "New" is selected
                                // Hide other menus first
                                if (main_event_menu) hide_event_menu(main_event_menu);
                                if (event_area_menu) hide_event_menu(event_area_menu);
                                if (contents_context_menu) hide_event_menu(contents_context_menu);
                                
                                if (!event_commands_menu) {
                                    printf("Creating event commands menu\n");
                                    event_commands_menu = create_event_commands_menu(menu->parent_x, menu->parent_y);
                                    printf("Event commands menu created: %p\n", event_commands_menu);
                                }
                                if (event_commands_menu) {
                                    printf("Positioning event commands menu\n");
                                    event_commands_menu->x = menu->x + menu->width;
                                    event_commands_menu->y = menu->y;
                                    // Make sure the menu is within the window bounds
                                    if (event_commands_menu->x + event_commands_menu->width > 1024) {
                                        event_commands_menu->x = 1024 - event_commands_menu->width;
                                    }
                                    if (event_commands_menu->y + event_commands_menu->height > 768) {
                                        event_commands_menu->y = 768 - event_commands_menu->height;
                                    }
                                    show_event_menu(event_commands_menu);
                                    // Hide the contents context menu
                                    hide_event_menu(menu);
                                }
                                break;
                            case 1: // Edit
                                printf("Contents Edit selected\n");
                                // Create and show the event commands menu when "Edit" is selected
                                // Hide other menus first
                                if (main_event_menu) hide_event_menu(main_event_menu);
                                if (event_area_menu) hide_event_menu(event_area_menu);
                                if (contents_context_menu) hide_event_menu(contents_context_menu);
                                
                                if (!event_commands_menu) {
                                    event_commands_menu = create_event_commands_menu(menu->parent_x, menu->parent_y);
                                }
                                if (event_commands_menu) {
                                    event_commands_menu->x = menu->x + menu->width;
                                    event_commands_menu->y = menu->y;
                                    // Make sure the menu is within the window bounds
                                    if (event_commands_menu->x + event_commands_menu->width > 1024) {
                                        event_commands_menu->x = 1024 - event_commands_menu->width;
                                    }
                                    if (event_commands_menu->y + event_commands_menu->height > 768) {
                                        event_commands_menu->y = 768 - event_commands_menu->height;
                                    }
                                    show_event_menu(event_commands_menu);
                                    // Hide the contents context menu
                                    hide_event_menu(menu);
                                }
                                break;
                            case 2: // Copy
                                printf("Contents Copy selected\n");
                                break;
                            case 3: // Paste
                                printf("Contents Paste selected\n");
                                break;
                            case 4: // Delete
                                printf("Contents Delete selected\n");
                                break;
                            case 5: // Select All
                                printf("Contents Select All selected\n");
                                break;
                            case 6: // Test
                                printf("Contents Test selected\n");
                                break;
                        }
                    } else if (menu->type == 4) {
                        // Maps context menu
                        switch (i) {
                            case 0: // New
                                printf("Maps New selected\n");
                                // Create a new map file
                                // For now, just print a message
                                // In a real implementation, this would create a new map file
                                hide_event_menu(menu);
                                // Create a new map file with a unique name in the current project directory
                                char* new_map_filename = NULL;
                                int map_count = 1;
                                bool file_exists = true;
                                while (file_exists) {
                                    size_t len = strlen(current_project_path) + strlen("/map_.txt") + 12; // 12 for map_count digits
                                    new_map_filename = malloc(len);
                                    if (!new_map_filename) {
                                        printf("Error: Could not allocate memory for new map filename\n");
                                        break;
                                    }
                                    snprintf(new_map_filename, len, "%s/map_%d.txt", current_project_path, map_count);
                                    // Check if file exists
                                    FILE* test_file = fopen(new_map_filename, "r");
                                    if (test_file) {
                                        fclose(test_file);
                                        free(new_map_filename);
                                        new_map_filename = NULL;
                                        map_count++;
                                    } else {
                                        file_exists = false;
                                    }
                                }
                                // Create the new map file
                                FILE* new_map_file = fopen(new_map_filename, "w");
                                if (new_map_file) {
                                    // Write header
                                    fprintf(new_map_file, "x,y,z,emoji_idx,fg_color_idx,bg_color_idx\n");
                                    fclose(new_map_file);
                                    printf("Created new map file: %s\n", new_map_filename);
                                    // Refresh the map files list
                                    free_map_files_list();
                                    scan_map_files(current_project_path);
                                } else {
                                    printf("Failed to create new map file: %s\n", new_map_filename);
                                }
                                break;
                            case 1: // Copy
                                printf("Maps Copy selected\n");
                                // For now, just print a message
                                // In a real implementation, this would copy the selected map
                                hide_event_menu(menu);
                                break;
                            case 2: // Paste
                                printf("Maps Paste selected\n");
                                // For now, just print a message
                                // In a real implementation, this would paste the copied map
                                hide_event_menu(menu);
                                break;
                            case 3: // Edit
                                printf("Maps Edit selected\n");
                                // For now, just print a message
                                // In a real implementation, this would edit the selected map
                                hide_event_menu(menu);
                                // Create a simple edit menu (blank for now)
                                EventMenu* edit_menu = create_main_event_menu(menu->parent_x, menu->parent_y);
                                if (edit_menu) {
                                    // Set up the edit menu with some basic options
                                    edit_menu->item_count = 3;
                                    
                                    int item_size = EVENT_MENU_ITEM_LABEL_SIZE + sizeof(int) + sizeof(bool) + sizeof(int);
                                    void* item0 = (char*)edit_menu->items + 0 * item_size;
                                    set_event_menu_item_label(item0, "Rename");
                                    set_event_menu_item_id(item0, 0);
                                    set_event_menu_item_is_category(item0, 0);
                                    set_event_menu_item_page(item0, 0);
                                    
                                    void* item1 = (char*)edit_menu->items + 1 * item_size;
                                    set_event_menu_item_label(item1, "Duplicate");
                                    set_event_menu_item_id(item1, 1);
                                    set_event_menu_item_is_category(item1, 0);
                                    set_event_menu_item_page(item1, 0);
                                    
                                    void* item2 = (char*)edit_menu->items + 2 * item_size;
                                    set_event_menu_item_label(item2, "Delete");
                                    set_event_menu_item_id(item2, 2);
                                    set_event_menu_item_is_category(item2, 0);
                                    set_event_menu_item_page(item2, 0);
                                    
                                    // Position the edit menu
                                    edit_menu->x = menu->x + menu->width;
                                    edit_menu->y = menu->y;
                                    // Make sure the menu is within the window bounds
                                    if (edit_menu->x + edit_menu->width > 1024) {
                                        edit_menu->x = 1024 - edit_menu->width;
                                    }
                                    if (edit_menu->y + edit_menu->height > 768) {
                                        edit_menu->y = 768 - edit_menu->height;
                                    }
                                    show_event_menu(edit_menu);
                                    
                                    // Store the edit menu in main_event_menu for handling
                                    if (main_event_menu) {
                                        destroy_event_menu(main_event_menu);
                                    }
                                    main_event_menu = edit_menu;
                                }
                                break;
                        }
                    }
                    return; // Item found and processed
                }
                visible_item_count++;
            }
        }
    }
}


// Text input dialog functions

TextInputDialog* create_text_input_dialog(int x, int y, int width, int height) {
    TextInputDialog* dialog = (TextInputDialog*)malloc(sizeof(TextInputDialog));
    if (!dialog) return NULL;
    
    dialog->x = x;
    dialog->y = y;
    dialog->width = width;
    dialog->height = height;
    dialog->visible = false;
    dialog->input_buffer[0] = '\0';
    dialog->input_cursor = 0;
    dialog->input_length = 0;
    dialog->confirmed = false;
    
    return dialog;
}

void destroy_text_input_dialog(TextInputDialog* dialog) {
    if (dialog) {
        free(dialog);
    }
}

void render_text_input_dialog(TextInputDialog* dialog) {
    if (!dialog || !dialog->visible) return;
    
    // Background color
    float bg_color[3] = {0.2f, 0.2f, 0.2f};
    float border_color[3] = {0.5f, 0.5f, 0.5f};
    float text_color[3] = {1.0f, 1.0f, 1.0f};
    float button_color[3] = {0.3f, 0.3f, 0.3f};
    float button_hover_color[3] = {0.4f, 0.4f, 0.6f};
    
    // Draw dialog background
    draw_rect(dialog->x, dialog->y, dialog->width, dialog->height, bg_color);
    draw_border(dialog->x, dialog->y, dialog->width, dialog->height, border_color);
    
    // Draw title
    glColor3fv(text_color);
    render_text("Enter Text", dialog->x + 10, dialog->y + dialog->height - 25);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Draw input box
    float input_x = dialog->x + 20;
    float input_y = dialog->y + dialog->height - 70;
    float input_width = dialog->width - 40;
    float input_height = 30;
    draw_rect(input_x, input_y, input_width, input_height, button_color);
    draw_border(input_x, input_y, input_width, input_height, border_color);
    
    // Draw input text
    glColor3fv(text_color);
    // Create a temporary string with cursor
    char display_text[514];
    if (dialog->input_length > 0) {
        strncpy(display_text, dialog->input_buffer, dialog->input_length);
        display_text[dialog->input_length] = '\0';
    } else {
        display_text[0] = '\0';
    }
    render_text(display_text, input_x + 5, input_y + 10);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Draw OK button
    float ok_x = dialog->x + dialog->width - 160;
    float ok_y = dialog->y + 20;
    float ok_width = 60;
    float ok_height = 30;
    draw_rect(ok_x, ok_y, ok_width, ok_height, button_color);
    draw_border(ok_x, ok_y, ok_width, ok_height, border_color);
    glColor3fv(text_color);
    render_text("OK", ok_x + 20, ok_y + 10);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Draw Cancel button
    float cancel_x = dialog->x + dialog->width - 80;
    float cancel_y = dialog->y + 20;
    float cancel_width = 60;
    float cancel_height = 30;
    draw_rect(cancel_x, cancel_y, cancel_width, cancel_height, button_color);
    draw_border(cancel_x, cancel_y, cancel_width, cancel_height, border_color);
    glColor3fv(text_color);
    render_text("Cancel", cancel_x + 10, cancel_y + 10);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void handle_text_input_dialog_key(TextInputDialog* dialog, unsigned char key) {
    if (!dialog || !dialog->visible) return;
    
    if (key == '\r' || key == '\n') {  // Enter key
        dialog->confirmed = true;
        dialog->visible = false;
        save_event_text_to_file(dialog->input_buffer);
    } else if (key == '\b' || key == 127) {  // Backspace or Delete
        // Handle backspace
        if (dialog->input_cursor > 0) {
            dialog->input_cursor--;
            dialog->input_length--;
            dialog->input_buffer[dialog->input_length] = '\0';
        }
    } else if (key >= 32 && key <= 126) {  // Printable characters
        // Add character to input buffer
        if (dialog->input_length < sizeof(dialog->input_buffer) - 1) {
            dialog->input_buffer[dialog->input_length] = key;
            dialog->input_length++;
            dialog->input_cursor++;
            dialog->input_buffer[dialog->input_length] = '\0';
        }
    }
}

void handle_text_input_dialog_special(TextInputDialog* dialog, int key) {
    if (!dialog || !dialog->visible) return;
    
    // Handle special keys if needed
}

void show_text_input_dialog(TextInputDialog* dialog) {
    if (dialog) {
        dialog->visible = true;
        dialog->input_buffer[0] = '\0';
        dialog->input_cursor = 0;
        dialog->input_length = 0;
        dialog->confirmed = false;
    }
}

void hide_text_input_dialog(TextInputDialog* dialog) {
    if (dialog) {
        dialog->visible = false;
    }
}

// Function to save event text to file
void save_event_text_to_file(const char* text) {
    printf("Saving event text: %s\n", text);
    
    // Create the events file path using the current project path and current map file
    char* events_file_path = NULL;
    // Make sure we have a proper project path
    if (strlen(current_project_path) == 0) {
        printf("Error: Current project path is empty\n");
        return;
    }
    
    // Extract the base name of the map file (without extension) to create the events file name
    char* map_base_name = NULL;
    if (strlen(current_map_file) > 0) {
        // Copy the map file name
        map_base_name = strdup(current_map_file);
        if (!map_base_name) {
            printf("Error: Memory allocation failed\n");
            return;
        }
        
        // Remove the .txt extension if present
        char* dot = strrchr(map_base_name, '.');
        if (dot && strcmp(dot, ".txt") == 0) {
            *dot = '\0';
        }
    } else {
        // Default to map_0 if no map is loaded
        map_base_name = strdup("map_0");
        if (!map_base_name) {
            printf("Error: Memory allocation failed\n");
            return;
        }
    }
    
    // Create the events file name by appending "_events" to the map base name
    size_t events_file_name_len = strlen(map_base_name) + strlen("_events.txt") + 1;
    char* events_file_name = malloc(events_file_name_len);
    if (!events_file_name) {
        printf("Error: Could not allocate memory for events file name\n");
        free(map_base_name);
        return;
    }
    snprintf(events_file_name, events_file_name_len, "%s_events.txt", map_base_name);
    
    size_t events_file_path_len = strlen(current_project_path) + 1 + events_file_name_len + 1;
    events_file_path = malloc(events_file_path_len);
    if (!events_file_path) {
        printf("Error: Could not allocate memory for events file path\n");
        free(map_base_name);
        free(events_file_name);
        return;
    }
    snprintf(events_file_path, events_file_path_len, "%s/%s", current_project_path, events_file_name);
    
    printf("Events file path: %s\n", events_file_path);
    
    // Check if the events file exists to determine the next event index
    int event_index = 1;
    FILE* check_file = fopen(events_file_path, "r");
    if (check_file) {
        // File exists, find the next available event index
        char line[512];
        while (fgets(line, sizeof(line), check_file)) {
            // Look for lines with event IDs
            if (strstr(line, "events[") && strstr(line, ".id,")) {
                int id;
                if (sscanf(line, "events[%*d].id,%d", &id) == 1) {
                    if (id >= event_index) {
                        event_index = id + 1;
                    }
                }
            }
        }
        fclose(check_file);
    }
    
    printf("Next event index: %d\n", event_index);
    
    // Open the file for appending
    FILE* file = fopen(events_file_path, "a");
    if (!file) {
        // If file doesn't exist, create it with header
        file = fopen(events_file_path, "w");
        if (!file) {
            printf("Error: Could not create events file %s\n", events_file_path);
            free(events_file_path);
            return;
        }
        fprintf(file, "events[0],null\n");
    }
    
    // Write the event data
    fprintf(file, "events[%d].id,%d\n", event_index, event_index);
    fprintf(file, "events[%d].name,test-event-%d\n", event_index, event_index);
    fprintf(file, "events[%d].note,\n", event_index);
    fprintf(file, "events[%d].pages[0].list[1].parameters[0],%s\n", event_index, text);
    fprintf(file, "events[%d].x,4\n", event_index);
    fprintf(file, "events[%d].y,3\n", event_index);
    fprintf(file, "events[%d].z,9\n", event_index);
    
    fclose(file);
    printf("Event text saved to %s\n", events_file_path);
    
    // Free dynamically allocated memory
    free(events_file_path);
    free(events_file_name);
    free(map_base_name);
}