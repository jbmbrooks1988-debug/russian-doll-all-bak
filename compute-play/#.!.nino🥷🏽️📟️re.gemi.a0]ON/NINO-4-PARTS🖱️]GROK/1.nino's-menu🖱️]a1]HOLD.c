#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
char pos[10];
// Terminal control
struct termios original_termios;
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &original_termios);
    struct termios raw = original_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    write(STDOUT_FILENO, "\033[?1000h", 8); // Enable mouse reporting
}

void disable_raw_mode() {
    write(STDOUT_FILENO, "\033[?1000l", 8); // Disable mouse reporting
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

// Buffer struct for editor tabs
struct Buffer {
    char* name;
    char* content; // Simplified: single string for content
    int cursor_row, cursor_col;
    int scroll_row;
    int sel_start, sel_end; // Selection for copy-paste
    int selecting; // Flag for mouse drag
};

// Global state
struct Buffer* buffers[10];
int num_buffers = 0, current_buffer_index = 0;
int rows, cols;
char* files[100]; // File explorer entries
int num_files = 0, file_selection = 0, file_scroll = 0;
int file_explorer_width = 20;
int show_file_explorer = 0; // Toggle for file explorer visibility
char* clipboard = NULL; // Simple clipboard for copy-paste

// Screen rendering
void clear_screen() {
    write(STDOUT_FILENO, "\033[2J", 4);
    write(STDOUT_FILENO, "\033[H", 3); // Move cursor to top-left
}

void get_terminal_size() {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    rows = ws.ws_row;
    cols = ws.ws_col;
}

void draw_file_explorer() {
    if (!show_file_explorer) return;
    for (int i = 0; i < rows; i++) {
        write(STDOUT_FILENO, "\033[", 2);
        char pos[10];
        sprintf(pos, "%d;1H", i + 1);
        write(STDOUT_FILENO, pos, strlen(pos));
        if (i + file_scroll < num_files) {
            if (i + file_scroll == file_selection) {
                write(STDOUT_FILENO, "\033[7m", 4); // Highlight selection
            }
            char* name = files[i + file_scroll];
            int len = strlen(name) > file_explorer_width - 1 ? file_explorer_width - 1 : strlen(name);
            write(STDOUT_FILENO, name, len);
            for (int j = len; j < file_explorer_width; j++) write(STDOUT_FILENO, " ", 1);
            if (i + file_scroll == file_selection) {
                write(STDOUT_FILENO, "\033[0m", 4); // Reset color
            }
        } else {
            for (int j = 0; j < file_explorer_width; j++) write(STDOUT_FILENO, " ", 1);
        }
    }
}

void draw_tab_bar() {
    write(STDOUT_FILENO, "\033[1;1H", 6); // Start at top-left
    write(STDOUT_FILENO, show_file_explorer ? "[Files] " : "[Files]", 7);
    int tab_x = show_file_explorer ? file_explorer_width + 1 : 8;
    for (int i = 0; i < num_buffers; i++) {
        write(STDOUT_FILENO, "\033[1;", 6);
        char pos[10];
        sprintf(pos, "%dH", tab_x);
        write(STDOUT_FILENO, pos, strlen(pos));
        if (i == current_buffer_index) write(STDOUT_FILENO, "\033[7m", 4);
        write(STDOUT_FILENO, buffers[i]->name, strlen(buffers[i]->name));
        write(STDOUT_FILENO, " ", 1);
        if (i == current_buffer_index) write(STDOUT_FILENO, "\033[0m", 4);
        tab_x += strlen(buffers[i]->name) + 1;
    }
}

void draw_buffer() {
    struct Buffer* buf = buffers[current_buffer_index];
    int start_col = show_file_explorer ? file_explorer_width + 1 : 1;
    for (int i = 0; i < rows - 1; i++) {
        write(STDOUT_FILENO, "\033[", 2);
        char pos[10];
        sprintf(pos, "%d;%dH", i + 2, start_col);
        write(STDOUT_FILENO, pos, strlen(pos));
        for (int j = start_col; j < cols; j++) write(STDOUT_FILENO, " ", 1); // Clear line
        if (i + buf->scroll_row < (buf->content ? strlen(buf->content) : 0)) {
            write(STDOUT_FILENO, "\033[", 2);
            write(STDOUT_FILENO, pos, strlen(pos));
            if (buf->sel_start != -1 && i + buf->scroll_row >= buf->sel_start && i + buf->scroll_row <= buf->sel_end) {
                write(STDOUT_FILENO, "\033[7m", 4); // Highlight selection
            }
            write(STDOUT_FILENO, &buf->content[i + buf->scroll_row], 1);
            write(STDOUT_FILENO, "\033[0m", 4); // Reset color
        }
    }
    // Position cursor
    sprintf(pos, "%d;%dH", buf->cursor_row - buf->scroll_row + 2, buf->cursor_col + start_col);
    write(STDOUT_FILENO, "\033[", 2);
    write(STDOUT_FILENO, pos, strlen(pos));
}

void refresh_screen() {
    clear_screen();
    draw_tab_bar();
    draw_file_explorer();
    draw_buffer();
    fflush(stdout);
}

// File explorer
void load_files(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) { perror("opendir"); return; }
    struct dirent* entry;
    num_files = 0;
    while ((entry = readdir(dir)) && num_files < 100) {
        files[num_files] = strdup(entry->d_name);
        num_files++;
    }
    closedir(dir);
}

// Buffer management
void add_buffer(const char* name, const char* content) {
    struct Buffer* buf = malloc(sizeof(struct Buffer));
    buf->name = strdup(name);
    buf->content = content ? strdup(content) : strdup("");
    buf->cursor_row = 0;
    buf->cursor_col = 0;
    buf->scroll_row = 0;
    buf->sel_start = -1; // No selection initially
    buf->sel_end = -1;
    buf->selecting = 0;
    buffers[num_buffers++] = buf;
    current_buffer_index = num_buffers - 1;
}

// Copy-paste functions
void copy_selection(struct Buffer* buf) {
    if (buf->sel_start == -1 || buf->sel_end == -1) return;
    int len = buf->sel_end - buf->sel_start + 1;
    if (clipboard) free(clipboard);
    clipboard = malloc(len + 1);
    strncpy(clipboard, buf->content + buf->sel_start, len);
    clipboard[len] = '\0';
}

void paste(struct Buffer* buf) {
    if (!clipboard) return;
    int len = strlen(clipboard);
    char* new_content = malloc(strlen(buf->content) + len + 1);
    strncpy(new_content, buf->content, buf->cursor_row);
    new_content[buf->cursor_row] = '\0';
    strcat(new_content, clipboard);
    strcat(new_content, buf->content + buf->cursor_row);
    free(buf->content);
    buf->content = new_content;
    buf->cursor_row += len;
}

// Input handling
int read_key() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    return c;
}

void handle_mouse_event(int b, int x, int y) {
    struct Buffer* buf = buffers[current_buffer_index];
    int start_col = show_file_explorer ? file_explorer_width + 1 : 1;

    if (y == 1 && x <= 7) { // Click on "[Files]"
        show_file_explorer = !show_file_explorer;
    } else if (show_file_explorer && x <= file_explorer_width) { // File explorer click
        file_selection = y - 1 + file_scroll;
        if (file_selection >= num_files) file_selection = num_files - 1;
        if (file_selection < 0) file_selection = 0;
    } else if (y == 1) { // Tab bar click
        int tab_x = start_col;
        for (int i = 0; i < num_buffers; i++) {
            int len = strlen(buffers[i]->name) + 1;
            if (x >= tab_x && x < tab_x + len) {
                current_buffer_index = i;
                break;
            }
            tab_x += len;
        }
    } else if (x >= start_col) { // Buffer click
        int pos = y - 2 + buf->scroll_row;
        if (b == 0) { // Left press
            buf->sel_start = pos;
            buf->sel_end = pos;
            buf->selecting = 1;
            buf->cursor_row = pos;
            buf->cursor_col = x - start_col;
        } else if (b == 3 && buf->selecting) { // Release
            buf->sel_end = pos;
            buf->selecting = 0;
            if (buf->sel_end < buf->sel_start) {
                int temp = buf->sel_start;
                buf->sel_start = buf->sel_end;
                buf->sel_end = temp;
            }
        }
    }
}

void process_input() {
    int c = read_key();
    if (c == -1) return;

    struct Buffer* buf = buffers[current_buffer_index];
    if (c == '\033') { // Escape sequence
        c = read_key();
        if (c == '[') {
            c = read_key();
            if (c == 'M') { // Mouse event
                char b = read_key();
                char x = read_key();
                char y = read_key();
                handle_mouse_event(b - 32, x - 32, y - 32);
            } else if (c == 'A' && show_file_explorer && file_selection > 0) file_selection--; // Up arrow
            else if (c == 'B' && show_file_explorer && file_selection < num_files - 1) file_selection++; // Down arrow
        }
    } else if (c == '\n' && show_file_explorer && num_files > 0) { // Enter to open file
        char path[256];
        snprintf(path, sizeof(path), "./%s", files[file_selection]);
        if (is_directory(path)) {
            // Directory navigation not implemented
        } else {
            FILE* file = fopen(path, "r");
            if (file) {
                fseek(file, 0, SEEK_END);
                long size = ftell(file);
                fseek(file, 0, SEEK_SET);
                char* content = malloc(size + 1);
                fread(content, 1, size, file);
                content[size] = '\0';
                fclose(file);
                add_buffer(files[file_selection], content);
                free(content);
            }
        }
    } else if (c == 'c' && buf->sel_start != -1) { // Copy
        copy_selection(buf);
    } else if (c == 'v') { // Paste
        paste(buf);
    } else if (c == 'q') { // Quit
        exit(0);
    }
}

// Main function
int main() {
    enable_raw_mode();
    atexit(disable_raw_mode);
    get_terminal_size();
    load_files("."); // Load current directory
    add_buffer("empty.txt", NULL); // Initial empty buffer

    while (1) {
        refresh_screen();
        process_input();
    }
    return 0;
}

// Helper function for directory check
int is_directory(const char* path) {
    struct stat statbuf;
    if (stat(path, &statbuf) == 0) return S_ISDIR(statbuf.st_mode);
    return 0;
}
