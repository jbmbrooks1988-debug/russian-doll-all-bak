#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>
#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>

#define MAX_VARS 32
#define MAX_NAME 64
#define MAX_VALUE 256

void set_term_quiet_input() {
    struct termios tc;
    tcgetattr(0, &tc);
    tc.c_lflag &= ~(ICANON | ECHO);
    tc.c_cc[VMIN] = 0;
    tc.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tc);
}

int c;
int length_of_buffer = 0;
int length_of_file = 0;

char growing_list[1024];
char static_list[1];
char tmp = '.';

int sz;

// State variables
int x = 3;
int y = 2;
int z = 0;
int cam_x = 0;
int cam_y = 0;
int cam_z = 0;
int cam_tx = 0;
int extra_0 = 0;
int extra_1 = 0;
int cursor_x = 0;
int cursor_y = 0;
int cursor_z = 0;

char state_names[MAX_VARS][MAX_NAME];
char state_values[MAX_VARS][MAX_VALUE];
int var_count = 0;

void append(char* s, char c) {
    int len = strlen(s);
    s[len] = c;
    s[len+1] = '\0';
}

void replace(char* s, char c) {
    s[0] = c;
    s[1] = '\0';
}

void load_state_file() {
    FILE *file = fopen("state.txt", "r");
    if (!file) return;

    char line[512];
    var_count = 0;

    while (fgets(line, sizeof(line), file) && var_count < MAX_VARS) {
        char *name = strtok(line, " \n");
        char *value = strtok(NULL, "\n");
        if (name && value) {
            strncpy(state_names[var_count], name, MAX_NAME - 1);
            state_names[var_count][MAX_NAME - 1] = '\0';
            strncpy(state_values[var_count], value, MAX_VALUE - 1);
            state_values[var_count][MAX_VALUE - 1] = '\0';
            var_count++;
        }
    }
    fclose(file);

    // Update program variables from loaded state
    for (int i = 0; i < var_count; i++) {
        if (strcmp(state_names[i], "x") == 0) x = atoi(state_values[i]);
        else if (strcmp(state_names[i], "y") == 0) y = atoi(state_values[i]);
        else if (strcmp(state_names[i], "z") == 0) z = atoi(state_values[i]);
        else if (strcmp(state_names[i], "cam_x") == 0) cam_x = atoi(state_values[i]);
        else if (strcmp(state_names[i], "cam_y") == 0) cam_y = atoi(state_values[i]);
        else if (strcmp(state_names[i], "cam_z") == 0) cam_z = atoi(state_values[i]);
        else if (strcmp(state_names[i], "cam_tx") == 0) cam_tx = atoi(state_values[i]);
        else if (strcmp(state_names[i], "extra_0") == 0) extra_0 = atoi(state_values[i]);
        else if (strcmp(state_names[i], "extra_1") == 0) extra_1 = atoi(state_values[i]);
        else if (strcmp(state_names[i], "cursor_x") == 0) cursor_x = atoi(state_values[i]);
        else if (strcmp(state_names[i], "cursor_y") == 0) cursor_y = atoi(state_values[i]);
        else if (strcmp(state_names[i], "cursor_z") == 0) cursor_z = atoi(state_values[i]);
    }
}

void update_state_file() {
    // Update state_values with current program variables
    for (int i = 0; i < var_count; i++) {
        if (strcmp(state_names[i], "x") == 0) snprintf(state_values[i], MAX_VALUE, "%d", x);
        else if (strcmp(state_names[i], "y") == 0) snprintf(state_values[i], MAX_VALUE, "%d", y);
        else if (strcmp(state_names[i], "z") == 0) snprintf(state_values[i], MAX_VALUE, "%d", z);
        else if (strcmp(state_names[i], "cam_x") == 0) snprintf(state_values[i], MAX_VALUE, "%d", cam_x);
        else if (strcmp(state_names[i], "cam_y") == 0) snprintf(state_values[i], MAX_VALUE, "%d", cam_y);
        else if (strcmp(state_names[i], "cam_z") == 0) snprintf(state_values[i], MAX_VALUE, "%d", cam_z);
        else if (strcmp(state_names[i], "cam_tx") == 0) snprintf(state_values[i], MAX_VALUE, "%d", cam_tx);
        else if (strcmp(state_names[i], "extra_0") == 0) snprintf(state_values[i], MAX_VALUE, "%d", extra_0);
        else if (strcmp(state_names[i], "extra_1") == 0) snprintf(state_values[i], MAX_VALUE, "%d", extra_1);
        else if (strcmp(state_names[i], "cursor_x") == 0) snprintf(state_values[i], MAX_VALUE, "%d", cursor_x);
        else if (strcmp(state_names[i], "cursor_y") == 0) snprintf(state_values[i], MAX_VALUE, "%d", cursor_y);
        else if (strcmp(state_names[i], "cursor_z") == 0) snprintf(state_values[i], MAX_VALUE, "%d", cursor_z);
    }

    // Write back to file
    FILE *file = fopen("state.txt", "w");
    if (file) {
        for (int i = 0; i < var_count; i++) {
            fprintf(file, "%s %s\n", state_names[i], state_values[i]);
        }
        fclose(file);
    }
}

int main(int argc, char *argv[]) {
    int use_player = 0;
    if (argc > 1 && strcmp(argv[1], "player") == 0) {
        use_player = 1;
    }

    int fd = open("foo.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct pollfd pfd = { .fd = 0, .events = POLLIN };
    set_term_quiet_input();

    load_state_file(); // Load state at startup

    while (1) {
        if (poll(&pfd, 1, 0) > 0) {
            c = getchar();

            append(growing_list, c);
            replace(static_list, c);
            length_of_buffer = strlen(growing_list);

            // Handle arrow keys and z/x keys
            if (c == 27) { // Escape sequence for arrow keys
                c = getchar(); // Skip [
                if (c == '[') {
                    c = getchar();
                    // Always update player coordinates (x, y) for arrow keys
                    switch (c) {
                        case 'A': y--; break; // Up arrow: forward
                        case 'B': y++; break; // Down arrow: back
                        case 'C': x++; break; // Right arrow
                        case 'D': x--; break; // Left arrow
                    }
                    update_state_file();
                }
            } else {
                switch (c) {
                    case 'z':
                        if (use_player) z++;
                        else cursor_z++;
                        break;
                    case 'x':
                        if (use_player) z--;
                        else cursor_z++;
                        break;
                    case 'w': cam_y--; break; // W key: camera forward
                    case 's': cam_y++; break; // S key: camera back
                    case 'a': cam_x--; break; // A key: camera left
                    case 'd': cam_x++; break; // D key: camera right
                    case 'q': cam_z++; break; // Q key: camera up
                    case 'e': cam_z--; break; // E key: camera down
                    case 'Q': close(fd); return 0; // Quit
                }
                if (c == 'z' || c == 'x' || c == 'w' || c == 's' || c == 'a' || c == 'd' || c == 'q' || c == 'e') {
                    update_state_file();
                }
            }
        }

        while (length_of_file < length_of_buffer) {
            length_of_file++;
            write(fd, static_list, sizeof(static_list));
        }
    }
    close(fd);
}
