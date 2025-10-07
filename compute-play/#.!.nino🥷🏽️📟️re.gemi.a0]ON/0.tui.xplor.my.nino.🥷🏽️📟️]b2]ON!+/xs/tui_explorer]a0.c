/*
 * TUI Explorer: A minimal C program for a multi-tab UI with a toggleable file explorer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>

// =========================== Defines ===========================

#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

// =========================== Data Structures ===========================

// Append buffer for writing to screen
typedef struct {
    char *b;
    int len;
} abuf;

// Represents a single file or directory in the explorer
typedef struct {
    char name[256];
    int is_dir;
} ExplorerEntry;

// Represents an open file in a tab
typedef struct {
    char path[1024];
    char content[8192];
    int content_len;
} Tab;

// Global application state
struct {
    int screen_rows;
    int screen_cols;
    struct termios orig_termios;

    // Explorer State
    int explorer_visible;
    int explorer_width;
    int explorer_scroll_offset;
    int explorer_selected_item;
    ExplorerEntry* explorer_entries;
    int explorer_entry_count;

    // Tab State
    Tab tabs[32];
    int tab_count;
    int active_tab;

} AppState;

// =========================== Terminal Handling ===========================

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &AppState.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &AppState.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = AppState.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

// =========================== Append Buffer ===========================

void abAppend(abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(abuf *ab) {
    free(ab->b);
}

// =========================== File I/O ===========================

void open_file_in_new_tab(const char* path) {
    if (AppState.tab_count >= 32) return; // Max tabs

    FILE *fp = fopen(path, "r");
    if (!fp) return;

    Tab *tab = &AppState.tabs[AppState.tab_count];
    strncpy(tab->path, path, sizeof(tab->path) - 1);
    tab->path[sizeof(tab->path) - 1] = '\0';

    tab->content_len = fread(tab->content, 1, sizeof(tab->content) - 1, fp);
    tab->content[tab->content_len] = '\0';

    fclose(fp);

    AppState.tab_count++;
    AppState.active_tab = AppState.tab_count - 1;
}

// =========================== Explorer ===========================

int compareExplorerEntries(const struct dirent **a, const struct dirent **b) {
    return strcmp((*a)->d_name, (*b)->d_name);
}

void explorer_load_dir(const char *path) {
    struct dirent **namelist;
    int n = scandir(path, &namelist, NULL, compareExplorerEntries);

    if (n == -1) {
        die("scandir");
    }

    if (AppState.explorer_entries) {
        free(AppState.explorer_entries);
    }
    AppState.explorer_entries = malloc(sizeof(ExplorerEntry) * n);
    AppState.explorer_entry_count = n;

    for (int i = 0; i < n; i++) {
        strncpy(AppState.explorer_entries[i].name, namelist[i]->d_name, 255);
        AppState.explorer_entries[i].name[255] = '\0';
        AppState.explorer_entries[i].is_dir = (namelist[i]->d_type == DT_DIR);
        free(namelist[i]);
    }
    free(namelist);
}

void draw_explorer(abuf *ab) {
    if (!AppState.explorer_visible) return;

    for (int y = 0; y < AppState.screen_rows; y++) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 1, 1);
        abAppend(ab, buf, len);

        int explorer_index = AppState.explorer_scroll_offset + y;
        if (explorer_index < AppState.explorer_entry_count) {
            if (explorer_index == AppState.explorer_selected_item) {
                abAppend(ab, "\x1b[7m", 4); // Invert colors
            }

            char entry_text[256];
            int entry_len = snprintf(entry_text, sizeof(entry_text), "%s%s",
                AppState.explorer_entries[explorer_index].name,
                AppState.explorer_entries[explorer_index].is_dir ? "/" : "");
            
            if (entry_len > AppState.explorer_width) entry_len = AppState.explorer_width;
            abAppend(ab, entry_text, entry_len);
            
            if (explorer_index == AppState.explorer_selected_item) {
                abAppend(ab, "\x1b[m", 3); // Reset colors
            }
        }
        abAppend(ab, "\x1b[K", 3); // Clear rest of line
    }
}

// =========================== UI Drawing ===========================

void draw_tabs(abuf *ab) {
    int start_col = AppState.explorer_visible ? AppState.explorer_width + 1 : 1;
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 1, start_col);
    abAppend(ab, buf, len);

    for (int i = 0; i < AppState.tab_count; i++) {
        if (i == AppState.active_tab) {
            abAppend(ab, "\x1b[7m", 4);
        }
        char tab_text[32];
        int tab_len = snprintf(tab_text, sizeof(tab_text), " %s ", AppState.tabs[i].path);
        abAppend(ab, tab_text, tab_len);
        if (i == AppState.active_tab) {
            abAppend(ab, "\x1b[m", 3);
        }
    }
}

void draw_content(abuf *ab) {
    int start_col = AppState.explorer_visible ? AppState.explorer_width + 1 : 1;
    int start_row = 2; // Below tab bar

    if (AppState.tab_count == 0 || AppState.active_tab < 0) return;

    Tab *tab = &AppState.tabs[AppState.active_tab];
    char *line = tab->content;
    for (int y = 0; y < AppState.screen_rows - start_row; y++) {
        if (line >= tab->content + tab->content_len) break;
        char *next_line = strchr(line, '\n');
        int line_len = next_line ? (next_line - line) : strlen(line);

        char buf[32];
        int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + start_row, start_col);
        abAppend(ab, buf, len);
        abAppend(ab, line, line_len);

        if (!next_line) break;
        line = next_line + 1;
    }
}

// =========================== Main Loop ===========================

void refreshScreen() {
    abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); // Hide cursor
    abAppend(&ab, "\x1b[2J", 4);   // Clear screen
    abAppend(&ab, "\x1b[H", 3);    // Go home

    draw_explorer(&ab);
    draw_tabs(&ab);
    draw_content(&ab);

    abAppend(&ab, "\x1b[H", 3); // Go home
    abAppend(&ab, "\x1b[?25h", 6); // Show cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void process_keypress() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return;

    if (c == CTRL_KEY('q')) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
    } else if (c == CTRL_KEY('b')) {
        AppState.explorer_visible = !AppState.explorer_visible;
    } else if (c == '\r') { // Enter
        if (AppState.explorer_visible) {
            ExplorerEntry *entry = &AppState.explorer_entries[AppState.explorer_selected_item];
            if (!entry->is_dir) {
                open_file_in_new_tab(entry->name);
            }
        }
    } else if (c == CTRL_KEY('w')) { // Close tab
        if (AppState.tab_count > 0) {
            for (int i = AppState.active_tab; i < AppState.tab_count - 1; i++) {
                AppState.tabs[i] = AppState.tabs[i+1];
            }
            AppState.tab_count--;
            if (AppState.active_tab >= AppState.tab_count) {
                AppState.active_tab = AppState.tab_count - 1;
            }
        }
    } else if (c == CTRL_KEY('[')) {
        if (AppState.active_tab > 0) {
            AppState.active_tab--;
        }
    } else if (c == CTRL_KEY(']')) {
        if (AppState.active_tab < AppState.tab_count - 1) {
            AppState.active_tab++;
        }
    } else if (c == '\x1b') { // Arrow keys
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                 if (read(STDIN_FILENO, &seq[2], 1) != 1) return;
                 if (seq[2] == '~') {
                     // Page up, page down, etc.
                 }
            } else {
                switch (seq[1]) {
                    case 'A': // Up
                        if (AppState.explorer_selected_item > 0) {
                            AppState.explorer_selected_item--;
                        }
                        break;
                    case 'B': // Down
                        if (AppState.explorer_selected_item < AppState.explorer_entry_count - 1) {
                            AppState.explorer_selected_item++;
                        }
                        break;
                }
            }
        } 
    }
}

int main() {
    enableRawMode();

    if (getWindowSize(&AppState.screen_rows, &AppState.screen_cols) == -1) die("getWindowSize");

    // Initial state
    AppState.explorer_visible = 1;
    AppState.explorer_width = 30;
    AppState.explorer_selected_item = 0;
    AppState.explorer_scroll_offset = 0;
    AppState.tab_count = 0;
    AppState.active_tab = -1;
    explorer_load_dir(".");

    while (1) {
        refreshScreen();
        process_keypress();
    }

    return 0;
}
