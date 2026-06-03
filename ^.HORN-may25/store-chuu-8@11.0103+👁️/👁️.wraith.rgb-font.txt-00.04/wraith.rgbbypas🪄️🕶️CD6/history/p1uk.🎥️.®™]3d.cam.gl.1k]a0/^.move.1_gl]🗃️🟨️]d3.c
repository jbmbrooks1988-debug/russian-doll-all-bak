#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <stdlib.h>

#define MAX_LINE 256
#define MAX_VARS 32
#define MAX_NAME 64
#define MAX_VALUE 256

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

char growing_list[1024];
char static_list[1];
int length_of_buffer = 0;
int length_of_file = 0;

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
    if (!file) {
        // Initialize with cursor variables if file doesn't exist
        var_count = 0;
        snprintf(state_names[var_count], MAX_NAME, "cursor_x");
        snprintf(state_values[var_count++], MAX_VALUE, "%d", cursor_x);
        snprintf(state_names[var_count], MAX_NAME, "cursor_y");
        snprintf(state_values[var_count++], MAX_VALUE, "%d", cursor_y);
        snprintf(state_names[var_count], MAX_NAME, "cursor_z");
        snprintf(state_values[var_count++], MAX_VALUE, "%d", cursor_z);
        return;
    }

    char line[512];
    var_count = 0;
    int has_cursor_x = 0, has_cursor_y = 0, has_cursor_z = 0;

    while (fgets(line, sizeof(line), file) && var_count < MAX_VARS) {
        char *name = strtok(line, " \n");
        char *value = strtok(NULL, "\n");
        if (name && value) {
            strncpy(state_names[var_count], name, MAX_NAME - 1);
            state_names[var_count][MAX_NAME - 1] = '\0';
            strncpy(state_values[var_count], value, MAX_VALUE - 1);
            state_values[var_count][MAX_VALUE - 1] = '\0';
            if (strcmp(name, "cursor_x") == 0) has_cursor_x = 1;
            else if (strcmp(name, "cursor_y") == 0) has_cursor_y = 1;
            else if (strcmp(name, "cursor_z") == 0) has_cursor_z = 1;
            var_count++;
        }
    }
    fclose(file);

    // Add cursor variables if not found in state.txt
    if (!has_cursor_x && var_count < MAX_VARS) {
        snprintf(state_names[var_count], MAX_NAME, "cursor_x");
        snprintf(state_values[var_count++], MAX_VALUE, "%d", cursor_x);
    }
    if (!has_cursor_y && var_count < MAX_VARS) {
        snprintf(state_names[var_count], MAX_NAME, "cursor_y");
        snprintf(state_values[var_count++], MAX_VALUE, "%d", cursor_y);
    }
    if (!has_cursor_z && var_count < MAX_VARS) {
        snprintf(state_names[var_count], MAX_NAME, "cursor_z");
        snprintf(state_values[var_count++], MAX_VALUE, "%d", cursor_z);
    }

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

    int fd = open("foo_gl.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int input_fd = open("commands_gl.txt", O_RDONLY);
    if (input_fd < 0) {
        printf("Error opening commands_gl.txt\n");
        close(fd);
        return 1;
    }
    flags = fcntl(input_fd, F_GETFL, 0);
    fcntl(input_fd, F_SETFL, flags | O_NONBLOCK);

    // Seek to the end of commands_gl.txt to process only new commands
    lseek(input_fd, 0, SEEK_END);

    struct pollfd pfd = { .fd = input_fd, .events = POLLIN };
    char line[MAX_LINE];
    char buffer[1024];
    int buffer_pos = 0;

    load_state_file(); // Load state at startup

    while (1) {
        if (poll(&pfd, 1, 0) > 0) {
            int bytes_read = read(input_fd, buffer + buffer_pos, 1024 - buffer_pos);
            if (bytes_read > 0) {
                buffer_pos += bytes_read;
                buffer[buffer_pos] = '\0';

                // Process complete lines
                char *line_start = buffer;
                char *line_end;
                while ((line_end = strchr(line_start, '\n')) != NULL) {
                    *line_end = '\0';
                    strncpy(line, line_start, MAX_LINE - 1);
                    line[MAX_LINE - 1] = '\0';

                    // Parse line: [timestamp] Type: details
                    char input_type[32], details[128], key[32];
                    int pos_x, pos_y;
                    if (sscanf(line, "[%*[^]]] %31[^:]: %127[^\n]", input_type, details) == 2) {
                        if (strcmp(input_type, "Keyboard") == 0) {
                            if (sscanf(details, "key=%c, x=%d, y=%d", &key[0], &pos_x, &pos_y) == 3) {
                                append(growing_list, key[0]);
                                replace(static_list, key[0]);
                                length_of_buffer = strlen(growing_list);

                                switch (key[0]) {
                                    case 'z':
                                        if (use_player) z++;
                                        else cursor_z++;
                                        break;
                                    case 'x':
                                        if (use_player) z--;
                                        else cursor_z--;
                                        break;
                                    case 'w': cam_y--; break; // W key: camera forward
                                    case 's': cam_y++; break; // S key: camera back
                                    case 'a': cam_x--; break; // A key: camera left
                                    case 'd': cam_x++; break; // D key: camera right
                                    case 'q': cam_z++; break; // Q key: camera up
                                    case 'e': cam_z--; break; // E key: camera down
                                    case 'Q': close(fd); close(input_fd); return 0; // Quit
                                }
                                if (key[0] == 'z' || key[0] == 'x' || key[0] == 'w' || key[0] == 's' || 
                                    key[0] == 'a' || key[0] == 'd' || key[0] == 'q' || key[0] == 'e') {
                                    update_state_file();
                                }
                            }
                        } else if (strcmp(input_type, "SpecialKeyboard") == 0) {
                            if (sscanf(details, "key=%31[^,], x=%d, y=%d", key, &pos_x, &pos_y) == 3) {
                                append(growing_list, key[0]);
                                replace(static_list, key[0]);
                                length_of_buffer = strlen(growing_list);

                                if (use_player) {
                                    if (strcmp(key, "UpArrow") == 0) y--;
                                    else if (strcmp(key, "DownArrow") == 0) y++;
                                    else if (strcmp(key, "RightArrow") == 0) x++;
                                    else if (strcmp(key, "LeftArrow") == 0) x--;
                                } else {
                                    if (strcmp(key, "UpArrow") == 0) cursor_y--;
                                    else if (strcmp(key, "DownArrow") == 0) cursor_y++;
                                    else if (strcmp(key, "RightArrow") == 0) cursor_x++;
                                    else if (strcmp(key, "LeftArrow") == 0) cursor_x--;
                                }
                                update_state_file();
                            }
                        }
                    }

                    line_start = line_end + 1;
                }

                // Shift remaining buffer content
                if (line_start != buffer) {
                    memmove(buffer, line_start, buffer_pos - (line_start - buffer));
                    buffer_pos -= (line_start - buffer);
                }
            }
        }

        while (length_of_file < length_of_buffer) {
            length_of_file++;
            write(fd, static_list, sizeof(static_list));
        }
    }

    close(fd);
    close(input_fd);
    return 0;
}
