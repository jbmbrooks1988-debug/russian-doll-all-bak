#include <GL/glut.h>
#include <stdbool.h>

int get_grid_rows(void);
int get_grid_cols(void);
int get_tile_size(void);
int get_window_height(void);
bool get_is_animating(void);
void set_is_animating(bool value);
bool is_adjacent(int r1, int c1, int r2, int c2);
void swap_candies(int r1, int c1, int r2, int c2);
bool find_matches(void);
void set_selected(int row, int col);
void set_status_message(const char* msg);
int get_window_width(void);
void remove_matches(void);
void drop_candies(void);

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && !get_is_animating()) {
        int col = (x - 10) / get_tile_size();
        float gl_y = get_window_height() - y;
        int row = (int)((get_window_height() - 50 - gl_y) / get_tile_size());
        if (col >= 0 && col < get_grid_cols() && row >= 0 && row < get_grid_rows()) {
            static int selected_row = -1;
            static int selected_col = -1;
            if (selected_row == -1) {
                selected_row = row;
                selected_col = col;
                set_selected(row, col);
            } else {
                if (is_adjacent(selected_row, selected_col, row, col)) {
                    swap_candies(selected_row, selected_col, row, col);
                    bool valid = find_matches();
                    if (!valid) {
                        swap_candies(selected_row, selected_col, row, col);
                        set_status_message("Invalid move!");
                    } else {
                        set_is_animating(true);
                    }
                    selected_row = -1;
                    selected_col = -1;
                    set_selected(-1, -1);
                }
            }
            glutPostRedisplay();
        }
    }
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle(void) {
    if (get_is_animating()) {
        remove_matches();
        drop_candies();
        if (!find_matches()) {
            set_is_animating(false);
        }
        glutPostRedisplay();
    }
}
