/* layout_op - Interactive Terminal Menu (PoC)
 * CHTPM-inspired, stripped down for proof of concept
 * 
 * Usage: ./+x/layout_op.+x <menu_id>
 * menu_id: 1 = main menu, 2 = options menu
 * Returns: selected option number via stdout
 * 
 * Ctrl+C works normally (passed through from shell).
 * 
 * Layout files stored in: custom_ops/layout_op/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OPTIONS 16
#define LAYOUT_DIR "custom_ops/layout_op"

typedef struct {
    char title[64];
    char labels[MAX_OPTIONS][64];
    int count;
} Menu;

Menu get_menu(int menu_id) {
    Menu m = {0};
    
    if (menu_id == 1) {
        strncpy(m.title, "Main Menu", 63);
        strncpy(m.labels[0], "Start Game", 63);
        strncpy(m.labels[1], "Options", 63);
        strncpy(m.labels[2], "Exit", 63);
        m.count = 3;
    } else if (menu_id == 2) {
        strncpy(m.title, "Options Menu", 63);
        strncpy(m.labels[0], "Sound", 63);
        strncpy(m.labels[1], "Back to Main", 63);
        m.count = 2;
    }
    return m;
}

void render(Menu *m) {
    fprintf(stderr, "\n=== %s ===\n\n", m->title);
    for (int i = 0; i < m->count; i++) {
        fprintf(stderr, "    %d. %s\n", i + 1, m->labels[i]);
    }
    fprintf(stderr, "\nSelect (Ctrl+C to quit): ");
    fflush(stderr);
}

int get_input(void) {
    char buf[16];
    
    /* Try /dev/tty first for direct terminal access (blocking, Ctrl+C works) */
    FILE *tty = fopen("/dev/tty", "r");
    if (tty) {
        if (fgets(buf, sizeof(buf), tty)) {
            fclose(tty);
            return atoi(buf);
        }
        fclose(tty);
    }
    
    /* Fallback: read from input file with line tracking (for automated testing) */
    FILE *inf = fopen("/tmp/prisc_input.txt", "r");
    FILE *cntf = fopen("/tmp/prisc_input_count.txt", "r");
    int line_num = 0;
    if (cntf) {
        fscanf(cntf, "%d", &line_num);
        fclose(cntf);
    }
    
    if (inf) {
        int val = 0;
        int current = 0;
        while (fgets(buf, sizeof(buf), inf)) {
            if (current == line_num) {
                val = atoi(buf);
                break;
            }
            current++;
        }
        fclose(inf);
        
        /* Update line count */
        cntf = fopen("/tmp/prisc_input_count.txt", "w");
        if (cntf) {
            fprintf(cntf, "%d", line_num + 1);
            fclose(cntf);
        }
        
        if (val > 0) return val;
        /* No more input in file, fall through to blocking stdin */
    }
    
    /* Blocking read from stdin (for interactive piped use) */
    if (fgets(buf, sizeof(buf), stdin)) {
        return atoi(buf);
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    int menu_id = 1;
    if (argc > 1) {
        menu_id = atoi(argv[1]);
    }
    
    Menu m = get_menu(menu_id);
    
    if (m.count == 0) {
        fprintf(stderr, "Error: Unknown menu %d\n", menu_id);
        return 1;
    }
    
    render(&m);
    int sel = get_input();
    
    /* Return selection to emulator */
    printf("%d\n", sel);
    fflush(stdout);
    
    return 0;
}
