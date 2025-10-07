#include <GL/glut.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <stdbool.h>

// Model function declarations
void set_canvas_rows(int value);
void set_canvas_cols(int value);
void set_tile_size(int value);
void set_sidebar_width(int value);
void set_window_width(int value);
void set_window_height(int value);
void set_status_message(const char* msg);
void set_selected_emoji(int value);
void set_selected_fg_color(int value);
void set_selected_bg_color(int value);
void set_selected_tool(int value);
void set_start_row(int value);
void set_start_col(int value);
void set_selector_row(int value);
void set_selector_col(int value);
void set_show_all_layers(bool value);
void set_canvas_tile(int layer, int r, int c, int field, int value);
void set_tab_bank(int idx, int field, int value);
void increment_tab_count(void);
void flood_fill(int layer, int r, int c, int old_emoji, int old_fg, int old_bg);
void draw_rectangle(int layer, int r1, int c1, int r2, int c2);
void save_canvas(void);
void load_canvas(void);
void print_ascii_grid(void);
void set_view_mode(int mode);
void set_current_z_level(int z);
void set_camera_x(float x);
void set_camera_y(float y);
void set_camera_z(float z);
void set_camera_yaw(float yaw);
void set_camera_pitch(float pitch);

// Model getter declarations
int get_canvas_rows(void);
int get_canvas_cols(void);
int get_tile_size(void);
int get_num_emojis(void);
int get_num_colors(void);
int get_max_layers(void);
int get_max_tabs(void);
int get_sidebar_width(void);
int get_file_tab_height(void);
int get_window_width(void);
int get_window_height(void);
int get_tab_count(void);
const char* get_emoji(int idx);
const char* get_color_name(int idx);
const char* get_status_message(void);
int get_selected_emoji(void);
int get_selected_fg_color(void);
int get_selected_bg_color(void);
int get_selected_tool(void);
int get_start_row(void);
int get_start_col(void);
int get_selector_row(void);
int get_selector_col(void);
bool get_show_all_layers(void);
int get_canvas_tile(int layer, int r, int c, int field);
int get_tab_bank(int idx, int field);
int get_view_mode(void);
int get_current_z_level(void);
float get_camera_x(void);
float get_camera_y(void);
float get_camera_z(void);
float get_camera_yaw(void);
float get_camera_pitch(void);

// View function declarations
void display(void);
void reshape(int w, int h);

// External function from main.c
extern void restore_terminal(void);

// State for quit confirmation
static bool pending_quit = false;

int check_terminal_input() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    fd_set set;
    struct timeval timeout;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int ready = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);
    int ch = -1;
    if (ready > 0) {
        ch = getchar();
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        float gl_y = get_window_height() - y;
        float sidebar_x = 10;
        // Canvas click
        int col = (x - get_sidebar_width() - 10) / get_tile_size();
        int row = get_canvas_rows() - 1 - ((gl_y - get_file_tab_height() - 10) / get_tile_size());
        if (col >= 0 && col < get_canvas_cols() && row >= 0 && row < get_canvas_rows()) {
            set_selector_row(row);
            set_selector_col(col);
            if (get_selected_tool() == 0) { // Paint
                set_canvas_tile(get_current_z_level(), row, col, 0, get_selected_emoji());
                set_canvas_tile(get_current_z_level(), row, col, 1, get_selected_fg_color());
                set_canvas_tile(get_current_z_level(), row, col, 2, get_selected_bg_color());
            } else if (get_selected_tool() == 1) { // Fill
                flood_fill(get_current_z_level(), row, col,
                           get_canvas_tile(get_current_z_level(), row, col, 0),
                           get_canvas_tile(get_current_z_level(), row, col, 1),
                           get_canvas_tile(get_current_z_level(), row, col, 2));
            } else if (get_selected_tool() == 2) { // Rectangle
                if (get_start_row() == -1) {
                    set_start_row(row);
                    set_start_col(col);
                    set_status_message("Select second corner for rectangle");
                } else {
                    draw_rectangle(get_current_z_level(), get_start_row(), get_start_col(), row, col);
                    set_start_row(-1);
                    set_start_col(-1);
                    set_status_message("Rectangle drawn");
                }
            }
        }
        // File tab click
        else if (x >= sidebar_x && x < sidebar_x + 60 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
            save_canvas();
        } else if (x >= sidebar_x + 70 && x < sidebar_x + 130 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
            load_canvas();
        } else if (x >= sidebar_x + 140 && x < sidebar_x + 200 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
            set_status_message("Text button clicked (no functionality)");
        } else if (x >= sidebar_x + 210 && x < sidebar_x + 270 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
            set_status_message("Tile button clicked (no functionality)");
        } else if (x >= sidebar_x + 280 && x < sidebar_x + 340 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
            set_view_mode(0);
            set_status_message("Switched to 2D view");
        } else if (x >= sidebar_x + 350 && x < sidebar_x + 410 && gl_y >= get_window_height() - get_file_tab_height() - 10 && gl_y < get_window_height() - 10) {
            set_view_mode(1);
            // Reset camera for 2D-like view of current Z-level
            float center_x = (get_canvas_cols() * get_tile_size()) / 2.0f;
            float center_y = (get_canvas_rows() * get_tile_size()) / 2.0f;
            float z_level = get_current_z_level() * get_tile_size();
            set_camera_x(center_x);
            set_camera_y(center_y);
            set_camera_z(z_level + get_tile_size() * 5.0f); // Position camera above the Z-level
            set_camera_yaw(0.0f);
            set_camera_pitch(-90.0f); // Look straight down
            set_status_message("Switched to 3D view, centered on current Z-level");
        }
        // Sidebar click - Emojis
        float sidebar_y_offset = get_file_tab_height() + 50;
        float base_y = get_window_height() - sidebar_y_offset - 20;
        int emoji_cols = 8;
        int emoji_rows = (get_num_emojis() + emoji_cols - 1) / emoji_cols;
        float emoji_section_top = base_y + 40;
        float emoji_section_bottom = base_y - (emoji_rows - 1) * 40;
        if (x >= sidebar_x + 10 && x < sidebar_x + 10 + emoji_cols * 40 &&
            gl_y >= emoji_section_bottom && gl_y <= emoji_section_top) {
            float local_y_from_top = emoji_section_top - gl_y;
            int row = (int)(local_y_from_top / 40);
            int col = (int)((x - (sidebar_x + 10)) / 40);
            if (row >= 0 && row < emoji_rows && col >= 0 && col < emoji_cols) {
                int idx = row * emoji_cols + col;
                if (idx < get_num_emojis()) {
                    set_selected_emoji(idx);
                    set_status_message("Emoji selected");
                }
            }
        }
        // Colors
        float colors_y_start = base_y - (emoji_rows * 40) - 20;
        int colors_cols = 8;
        int colors_rows = 1;
        float colors_section_top = colors_y_start + 40;
        float colors_section_bottom = colors_y_start - (colors_rows - 1) * 40;
        if (x >= sidebar_x + 10 && x < sidebar_x + 10 + colors_cols * 40 &&
            gl_y >= colors_section_bottom && gl_y <= colors_section_top) {
            float local_y_from_top = colors_section_top - gl_y;
            int row = (int)(local_y_from_top / 40);
            int col = (int)((x - (sidebar_x + 10)) / 40);
            if (row >= 0 && row < colors_rows && col >= 0 && col < colors_cols) {
                int idx = row * colors_cols + col;
                if (idx < get_num_colors()) {
                    set_selected_fg_color(idx);
                    set_status_message("Color selected");
                }
            }
        }
        // Tabs
        float tabs_y_start = colors_y_start - (colors_rows * 40) - 20;
        float tabs_section_top = tabs_y_start + 40;
        float tabs_section_bottom = tabs_y_start - (get_tab_count() - 1) * 40;
        if (x >= sidebar_x + 10 && x < sidebar_x + 50 &&
            gl_y >= tabs_section_bottom && gl_y <= tabs_section_top) {
            float local_y_from_top = tabs_section_top - gl_y;
            int idx = (int)(local_y_from_top / 40);
            if (idx >= 0 && idx < get_tab_count()) {
                set_selected_emoji(get_tab_bank(idx, 0));
                set_selected_fg_color(get_tab_bank(idx, 1));
                set_selected_bg_color(get_tab_bank(idx, 2));
                set_status_message("Tab selected");
            }
        }
        // Tools
        float tabs_section_height = get_tab_count() * 40;
        float tools_y_start = tabs_y_start - tabs_section_height - 20;
        float tools_section_top = tools_y_start + 30;
        float tools_section_bottom = tools_y_start;
        if (x >= sidebar_x + 10 && x < sidebar_x + 10 + 3 * 70 &&
            gl_y >= tools_section_bottom && gl_y <= tools_section_top) {
            int idx = (int)((x - (sidebar_x + 10)) / 70);
            if (idx >= 0 && idx < 3) {
                set_selected_tool(idx);
                set_start_row(-1);
                set_start_col(-1);
                set_status_message("Tool selected");
            }
        }
        print_ascii_grid();
        glutPostRedisplay();
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (pending_quit) {
        if (key == 'y' || key == 'Y') {
            restore_terminal();
            exit(0);
        } else if (key == 'n' || key == 'N') {
            pending_quit = false;
            set_status_message("Quit cancelled");
        }
        glutPostRedisplay();
        return;
    }

    if (key == 'c' && (glutGetModifiers() & GLUT_ACTIVE_CTRL)) {
        pending_quit = true;
        set_status_message("Quit? Press Y/N");
        glutPostRedisplay();
        return;
    }

    if (key == ' ') {
        if (get_selected_tool() == 0) {
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 0, get_selected_emoji());
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 1, get_selected_fg_color());
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 2, get_selected_bg_color());
            set_status_message("Tile painted");
        } else if (get_selected_tool() == 1) {
            flood_fill(get_current_z_level(), get_selector_row(), get_selector_col(),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 0),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 1),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 2));
            set_status_message("Area filled");
        } else if (get_selected_tool() == 2) {
            if (get_start_row() == -1) {
                set_start_row(get_selector_row());
                set_start_col(get_selector_col());
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(get_current_z_level(), get_start_row(), get_start_col(), get_selector_row(), get_selector_col());
                set_start_row(-1);
                set_start_col(-1);
                set_status_message("Rectangle drawn");
            }
        }
    } else if (key == '\r') { // Enter key
        if (get_selected_tool() == 0) {
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 0, get_selected_emoji());
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 1, get_selected_fg_color());
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 2, get_selected_bg_color());
            set_status_message("Tile painted");
        } else if (get_selected_tool() == 1) {
            flood_fill(get_current_z_level(), get_selector_row(), get_selector_col(),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 0),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 1),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 2));
            set_status_message("Area filled");
        } else if (get_selected_tool() == 2) {
            if (get_start_row() == -1) {
                set_start_row(get_selector_row());
                set_start_col(get_selector_col());
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(get_current_z_level(), get_start_row(), get_start_col(), get_selector_row(), get_selector_col());
                set_start_row(-1);
                set_start_col(-1);
                set_status_message("Rectangle drawn");
            }
        }
    } else if (key == 'f') {
        set_selected_tool(1);
        set_start_row(-1);
        set_start_col(-1);
        set_status_message("Fill tool selected");
    } else if (key == 'r') {
        set_selected_tool(2);
        set_start_row(-1);
        set_start_col(-1);
        set_status_message("Rectangle tool selected");
    } else if (key == '1') {
        set_selected_emoji((get_selected_emoji() + 1) % get_num_emojis());
        set_status_message("Emoji selected");
    } else if (key == 'c') {
        set_selected_fg_color((get_selected_fg_color() + 1) % get_num_colors());
        set_status_message("Color selected");
    } else if (key == '2') {
        set_show_all_layers(!get_show_all_layers());
        set_status_message(get_show_all_layers() ? "Showing all layers" : "Showing top layer");
    } else if (key == 's') {
        save_canvas();
    } else if (key == 'l') {
        load_canvas();
    } else if (key == 't' && get_tab_count() < get_max_tabs()) {
        set_tab_bank(get_tab_count(), 0, get_selected_emoji());
        set_tab_bank(get_tab_count(), 1, get_selected_fg_color());
        set_tab_bank(get_tab_count(), 2, get_selected_bg_color());
        increment_tab_count();
        set_status_message("Tab created");
    } else if (key == 'w') {
        if (get_view_mode() == 1) {
            set_camera_pitch(get_camera_pitch() + 5.0f);
            set_status_message("Camera Pitch Up");
        }
    } else if (key == 's') {
        if (get_view_mode() == 1) {
            set_camera_pitch(get_camera_pitch() - 5.0f);
            set_status_message("Camera Pitch Down");
        }
    } else if (key == 'a') {
        if (get_view_mode() == 1) {
            set_camera_yaw(get_camera_yaw() - 5.0f);
            set_status_message("Camera Yaw Left");
        }
    } else if (key == 'd') {
        if (get_view_mode() == 1) {
            set_camera_yaw(get_camera_yaw() + 5.0f);
            set_status_message("Camera Yaw Right");
        }
    } else if (key == 'q') {
        if (get_view_mode() == 1) {
            set_camera_z(get_camera_z() - 10.0f);
            set_status_message("Camera Zoom In");
        }
    } else if (key == 'e') {
        if (get_view_mode() == 1) {
            set_camera_z(get_camera_z() + 10.0f);
            set_status_message("Camera Zoom Out");
        }
    } else if (key == 'z') {
        set_current_z_level(get_current_z_level() - 1);
        set_status_message("Z-level Decreased");
    } else if (key == 'x') {
        set_current_z_level(get_current_z_level() + 1);
        set_status_message("Z-level Increased");
    }
    print_ascii_grid();
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_UP:
            if (get_selector_row() > 0) set_selector_row(get_selector_row() - 1);
            break;
        case GLUT_KEY_DOWN:
            if (get_selector_row() < get_canvas_rows() - 1) set_selector_row(get_selector_row() + 1);
            break;
        case GLUT_KEY_LEFT:
            if (get_selector_col() > 0) set_selector_col(get_selector_col() - 1);
            break;
        case GLUT_KEY_RIGHT:
            if (get_selector_col() < get_canvas_cols() - 1) set_selector_col(get_selector_col() + 1);
            break;
    }
    set_status_message("Selector moved");
    print_ascii_grid();
    glutPostRedisplay();
}

void idle(void) {
    int ch = check_terminal_input();
    if (pending_quit) {
        if (ch == 'y' || ch == 'Y') {
            restore_terminal();
            exit(0);
        } else if (ch == 'n' || ch == 'N') {
            pending_quit = false;
            set_status_message("Quit cancelled");
        }
        if (ch != -1) glutPostRedisplay();
        return;
    }

    if (ch == ' ') {
        if (get_selected_tool() == 0) {
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 0, get_selected_emoji());
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 1, get_selected_fg_color());
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 2, get_selected_bg_color());
            set_status_message("Tile painted");
        } else if (get_selected_tool() == 1) {
            flood_fill(get_current_z_level(), get_selector_row(), get_selector_col(),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 0),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 1),
                       get_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 2));
            set_status_message("Area filled");
        } else if (get_selected_tool() == 2) {
            if (get_start_row() == -1) {
                set_start_row(get_selector_row());
                set_start_col(get_selector_col());
                set_status_message("Select second corner for rectangle");
            } else {
                draw_rectangle(get_current_z_level(), get_start_row(), get_start_col(), get_selector_row(), get_selector_col());
                set_start_row(-1);
                set_start_col(-1);
                set_status_message("Rectangle drawn");
            }
        }
    } else if (ch == '\r') { // Enter key
        if (get_selected_tool() == 0) {
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 0, get_selected_emoji());
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 1, get_selected_fg_color());
            set_canvas_tile(get_current_z_level(), get_selector_row(), get_selector_col(), 2, get_selected_bg_color());
            set_status_message("Tile painted");
        } else if (ch == 'f') {
            set_selected_tool(1);
            set_start_row(-1);
            set_start_col(-1);
            set_status_message("Fill tool selected");
        } else if (ch == 'r') {
            set_selected_tool(2);
            set_start_row(-1);
            set_start_col(-1);
            set_status_message("Rectangle tool selected");
        } else if (ch == '1') {
            set_selected_emoji((get_selected_emoji() + 1) % get_num_emojis());
            set_status_message("Emoji selected");
        } else if (ch == 'c') {
            set_selected_fg_color((get_selected_fg_color() + 1) % get_num_colors());
            set_status_message("Color selected");
        } else if (ch == '2') {
            set_show_all_layers(!get_show_all_layers());
            set_status_message(get_show_all_layers() ? "Showing all layers" : "Showing top layer");
        } else if (ch == 's') {
            save_canvas();
        } else if (ch == 'l') {
            load_canvas();
        } else if (ch == 't' && get_tab_count() < get_max_tabs()) {
            set_tab_bank(get_tab_count(), 0, get_selected_emoji());
            set_tab_bank(get_tab_count(), 1, get_selected_fg_color());
            set_tab_bank(get_tab_count(), 2, get_selected_bg_color());
            increment_tab_count();
            set_status_message("Tab created");
        } else if (ch == 'q') {
            if (get_view_mode() == 1) {
                set_camera_z(get_camera_z() - 10.0f);
                set_status_message("Camera Zoom In");
            }
        } else if (ch == 'e') {
            if (get_view_mode() == 1) {
                set_camera_z(get_camera_z() + 10.0f);
                set_status_message("Camera Zoom Out");
            }
        }
    }
    if (ch != -1) {
        print_ascii_grid();
        glutPostRedisplay();
    }
}

void reshape(int w, int h) {
    set_window_width(w);
    set_window_height(h);
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}
