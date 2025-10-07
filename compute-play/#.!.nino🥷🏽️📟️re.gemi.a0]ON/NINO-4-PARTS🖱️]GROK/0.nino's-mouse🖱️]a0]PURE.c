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
};

// Global state
struct Buffer* buffers[10];
int num_buffers = 0, current_buffer_index = 0;
int rows, cols;
char* files[100]; // File explorer entries
int num_files = 0, file_selection = 0, file_scroll = 0;
int file_explorer_width = 20;

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
    for (int i = 0; i < rows; i++) {
        write(STDOUT_FILENO, "\033[", 2);
        
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
    write(STDOUT_FILENO, "\033[1;", 6);
    char pos[10];
    sprintf(pos, "%dH", file_explorer_width + 1);
    write(STDOUT_FILENO, pos, strlen(pos));
    for (int i = 0; i < num_buffers; i++) {
        if (i == current_buffer_index) write(STDOUT_FILENO, "\033[7m", 4);
        write(STDOUT_FILENO, buffers[i]->name, strlen(buffers[i]->name));
        write(STDOUT_FILENO, " ", 1);
        if (i == current_buffer_index) write(STDOUT_FILENO, "\033[0m", 4);
    }
}

void draw_buffer() {
    struct Buffer* buf = buffers[current_buffer_index];
    for (int i = 0; i < rows - 2; i++) {
        write(STDOUT_FILENO, "\033[", 2);
        char pos[10];
        sprintf(pos, "%d;%dH", i + 2, file_explorer_width + 1);
        write(STDOUT_FILENO, pos, strlen(pos));
        for (int j = file_explorer_width + 1; j < cols; j++) write(STDOUT_FILENO, " ", 1); // Clear line
        if (i + buf->scroll_row < buf->content ? strlen(buf->content) : 0) {
            write(STDOUT_FILENO, "\033[", 2);
            write(STDOUT_FILENO, pos, strlen(pos));
            write(STDOUT_FILENO, buf->content, strlen(buf->content));
        }
    }
    // Position cursor
    sprintf(pos, "%d;%dH", buf->cursor_row - buf->scroll_row + 2, buf->cursor_col + file_explorer_width + 1);
    write(STDOUT_FILENO, "\033[", 2);
    write(STDOUT_FILENO, pos, strlen(pos));
}

void refresh_screen() {
    clear_screen();
    draw_file_explorer();
    draw_tab_bar();
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
    buffers[num_buffers++] = buf;
    current_buffer_index = num_buffers - 1;
}

// Input handling
int read_key() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    return c;
}

void handle_mouse_event(int b, int x, int y) {
    if (x <= file_explorer_width) { // File explorer click
        file_selection = y - 1 + file_scroll;
        if (file_selection >= num_files) file_selection = num_files - 1;
        if (file_selection < 0) file_selection = 0;
    } else if (y == 1) { // Tab bar click
        int tab_x = file_explorer_width + 1;
        for (int i = 0; i < num_buffers; i++) {
            int len = strlen(buffers[i]->name) + 1;
            if (x >= tab_x && x < tab_x + len) {
                current_buffer_index = i;
                break;
            }
            tab_x += len;
        }
    } else { // Buffer click
        struct Buffer* buf = buffers[current_buffer_index];
        buf->cursor_row = y - 2 + buf->scroll_row;
        buf->cursor_col = x - file_explorer_width - 1;
        if (buf->cursor_row >= (buf->content ? strlen(buf->content) : 0)) buf->cursor_row = 0;
        if (buf->cursor_col >= cols - file_explorer_width - 1) buf->cursor_col = cols - file_explorer_width - 2;
    }
}

void process_input() {
    int c = read_key();
    if (c == -1) return;

    if (c == '\033') { // Escape sequence
        c = read_key();
        if (c == '[') {
            c = read_key();
            if (c == 'M') { // Mouse event
                char b = read_key();
                char x = read_key();
                char y = read_key();
                handle_mouse_event(b - 32, x - 32, y - 32);
            } else if (c == 'A' && file_selection > 0) file_selection--; // Up arrow
            else if (c == 'B' && file_selection < num_files - 1) file_selection++; // Down arrow
        }
    } else if (c == '\n' && num_files > 0) { // Enter to open file
        char path[256];
        snprintf(path, sizeof(path), "./%s", files[file_selection]);
        if (is_directory(path)) {
            // Directory navigation not implemented for simplicity
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
