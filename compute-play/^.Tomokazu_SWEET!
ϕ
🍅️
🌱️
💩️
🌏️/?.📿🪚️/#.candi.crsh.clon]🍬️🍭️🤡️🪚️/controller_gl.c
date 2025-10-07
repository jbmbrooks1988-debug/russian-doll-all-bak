#include <GL/glut.h>
#include "model_gl.h"

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && !is_animating) {
        int col = (x - 10) / TILE_SIZE;
        // Convert GLUT y (top-left origin) to OpenGL y (bottom-left origin)
        float gl_y = window_height - y;
        // Calculate row from the top of the grid (window_height - 50)
        int row = (int)((window_height - 50 - gl_y) / TILE_SIZE);
        if (col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS) {
            if (selected_row == -1) {
                selected_row = row;
                selected_col = col;
            } else {
                if (is_adjacent(selected_row, selected_col, row, col)) {
                    swap_candies(selected_row, selected_col, row, col);
                    bool valid = find_matches();
                    if (!valid) {
                        // Invalid swap, revert
                        swap_candies(selected_row, selected_col, row, col);
                        set_status_message("Invalid move!");
                    } else {
                        is_animating = true;  // Trigger processing in idle
                    }
                }
                selected_row = -1;
                selected_col = -1;
            }
            glutPostRedisplay();
        }
    }
}

void idle() {
    if (is_animating) {
        remove_matches();
        drop_candies();
        if (!find_matches()) {
            is_animating = false;
        }
        glutPostRedisplay();
    }
}
