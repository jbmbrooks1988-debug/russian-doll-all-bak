#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 256
#define MAX_MENU_ITEMS 10
#define MAX_SUBMENU_ITEMS 5
#define MAX_COMMAND 512
#define MAX_INPUT 16
#define INPUT_FILE "commands_gl.txt"
#define OUTPUT_FILE "gl_cli_out.txt"

// Menu data (arrays instead of structs)
char main_menu[MAX_MENU_ITEMS][MAX_LINE] = {
    "System Commands",
    "Network Commands",
    "File Operations",
    "Exit"
};
int main_menu_size = 4;

char submenus[MAX_MENU_ITEMS][MAX_SUBMENU_ITEMS][MAX_LINE] = {
    { // Submenu for System Commands
        "Check Status",
        "Ping Localhost",
        "List Processes",
        "Back"
    },
    { // Submenu for Network Commands
        "Scan Network",
        "Check Ports",
        "Ping Remote",
        "Back"
    },
    { // Submenu for File Operations
        "List Files",
        "Create File",
        "Delete File",
        "Back"
    },
    { // No submenu for Exit
        ""
    }
};
int submenu_sizes[MAX_MENU_ITEMS] = {4, 4, 4, 0};

// Commands for each submenu item (executed via system())
char commands[MAX_MENU_ITEMS][MAX_SUBMENU_ITEMS][MAX_COMMAND] = {
    { // System Commands
        "echo \"hi @ 0.0\" >> gl_cli_out.txt",
        "echo \"hi @ 0.1\" >> gl_cli_out.txt",
        "echo \"hi @ 0.2\" >> gl_cli_out.txt",
        "echo \"Back to main menu\" >> gl_cli_out.txt"
    },
    { // Network Commands
        "echo \"hi @ 1.0\" >> gl_cli_out.txt",
        "echo \"hi @ 1.1\" >> gl_cli_out.txt",
        "echo \"hi @ 1.2\" >> gl_cli_out.txt",
        "echo \"Back to main menu\" >> gl_cli_out.txt"
    },
    { // File Operations
        "echo \"hi @ 2.0\" >> gl_cli_out.txt",
        "echo \"hi @ 2.1\" >> gl_cli_out.txt",
        "echo \"hi @ 2.2\" >> gl_cli_out.txt",
        "echo \"Back to main menu\" >> gl_cli_out.txt"
    },
    { // Exit
        ""
    }
};

void write_to_output(const char *message) {
    char command[MAX_COMMAND];
    snprintf(command, MAX_COMMAND, "echo \"%s\" >> %s", message, OUTPUT_FILE);
    system(command);
}

void clear_input_file() {
    FILE *fp = fopen(INPUT_FILE, "w");
    if (fp) {
        fclose(fp);
    } else {
        write_to_output("Error: Could not clear commands_gl.txt");
    }
}

int process_input(char *input_buffer, int *input_len, long *processed_lines) {
    FILE *fp = fopen(INPUT_FILE, "r");
    if (!fp) {
        write_to_output("Error: Could not open commands_gl.txt");
        return -1;
    }

    char line[MAX_LINE];
    long current_line = 0;
    int new_input = 0;
    int enter_pressed = 0;
    while (fgets(line, MAX_LINE, fp)) {
        current_line++;
        if (current_line <= *processed_lines) {
            continue; // Skip already processed lines
        }
        line[strcspn(line, "\n")] = '\0';
        // Parse lines like "[timestamp] Keyboard: key=<value>, x=..., y=..."
        if (strstr(line, "Keyboard: key=")) {
            char *key_start = strstr(line, "key=");
            if (key_start) {
                key_start += 4; // Skip "key="
                if (key_start[0] >= '0' && key_start[0] <= '9' && key_start[1] == ',') {
                    if (*input_len < MAX_INPUT - 1) {
                        input_buffer[*input_len] = key_start[0];
                        (*input_len)++;
                        input_buffer[*input_len] = '\0';
                        char msg[MAX_LINE];
                        snprintf(msg, MAX_LINE, "Keypress: %c, Current input = %s", key_start[0], input_buffer);
                        write_to_output(msg);
                        new_input = 1;
                    } else {
                        write_to_output("Error: Input buffer full");
                    }
                } else if (strncmp(key_start, "8,", 2) == 0) { // Backspace (GLUT key code 8)
                    if (*input_len > 0) {
                        (*input_len)--;
                        input_buffer[*input_len] = '\0';
                        char msg[MAX_LINE];
                        snprintf(msg, MAX_LINE, "Backspace: Current input = %s", input_buffer);
                        write_to_output(msg);
                        new_input = 1;
                    } else {
                        write_to_output("Backspace: No input to delete");
                        new_input = 1;
                    }
                } else if (strncmp(key_start, "13,", 3) == 0) { // Enter (GLUT key code 13)
                    char msg[MAX_LINE];
                    snprintf(msg, MAX_LINE, "Enter pressed: Attempting index = %s", input_buffer);
                    write_to_output(msg);
                    enter_pressed = 1;
                    new_input = 1;
                }
            }
        }
    }
    fclose(fp);
    *processed_lines = current_line; // Update processed lines count

    if (new_input && enter_pressed) {
        // Convert input buffer to index when Enter is pressed
        char *end;
        long index = strtol(input_buffer, &end, 10);
        if (end != input_buffer && *end == '\0') {
            return (int)index;
        }
        return -1; // Invalid number
    }
    return -2; // No Enter pressed or no relevant input
}

void display_menu(int current_menu, int submenu_index, const char *input_buffer) {
    char buffer[MAX_LINE];
    if (current_menu == -1) { // Main menu
        snprintf(buffer, MAX_LINE, "Main Menu:");
        write_to_output(buffer);
        for (int i = 0; i < main_menu_size; i++) {
            snprintf(buffer, MAX_LINE, "%d: %s", i, main_menu[i]);
            write_to_output(buffer);
        }
    } else { // Submenu
        snprintf(buffer, MAX_LINE, "Submenu: %s", main_menu[current_menu]);
        write_to_output(buffer);
        for (int i = 0; i < submenu_sizes[current_menu]; i++) {
            snprintf(buffer, MAX_LINE, "%d: %s", i, submenus[current_menu][i]);
            write_to_output(buffer);
        }
    }
    snprintf(buffer, MAX_LINE, "Enter index (0-%d): %s", submenu_index >= 0 ? submenu_sizes[current_menu] - 1 : main_menu_size - 1, input_buffer);
    write_to_output(buffer);
}

int main() {
    int current_menu = -1; // -1 for main menu, 0+ for submenus
    char input_buffer[MAX_INPUT] = "";
    int input_len = 0;
    long processed_lines = 0; // Track processed lines in commands_gl.txt
    time_t last_input_time = 0;

    // Initial menu display
    display_menu(current_menu, current_menu, input_buffer);

    while (1) {
        // Check for new input
        struct stat stat_buf;
        if (stat(INPUT_FILE, &stat_buf) == 0) {
            if (stat_buf.st_mtime > last_input_time) {
                last_input_time = stat_buf.st_mtime;
                int index = process_input(input_buffer, &input_len, &processed_lines);
                if (index != -2) { // Only redisplay for relevant inputs or Enter
                    display_menu(current_menu, current_menu, input_buffer);
                }
                if (index >= -1) { // Valid or invalid index (Enter pressed)
                    if (current_menu == -1) { // In main menu
                        if (index >= 0 && index < main_menu_size) {
                            if (index == main_menu_size - 1) { // Exit
                                write_to_output("Exiting menu");
                                clear_input_file();
                                break;
                            }
                            current_menu = index; // Enter submenu
                            char buffer[MAX_LINE];
                            snprintf(buffer, MAX_LINE, "Selected: %s", main_menu[current_menu]);
                            write_to_output(buffer);
                            // Clear input after selection
                            input_buffer[0] = '\0';
                            input_len = 0;
                            clear_input_file(); // Clear file only after selection
                            display_menu(current_menu, current_menu, input_buffer);
                        } else {
                            write_to_output("Invalid main menu index");
                            // Clear input for invalid selection
                            input_buffer[0] = '\0';
                            input_len = 0;
                            clear_input_file(); // Clear file only after selection
                            display_menu(current_menu, current_menu, input_buffer);
                        }
                    } else { // In submenu
                        if (index >= 0 && index < submenu_sizes[current_menu]) {
                            if (index == submenu_sizes[current_menu] - 1) { // Back
                                current_menu = -1; // Return to main menu
                                write_to_output("Returned to main menu");
                            } else {
                                // Execute the selected command
                                system(commands[current_menu][index]);
                                char buffer[MAX_LINE];
                                snprintf(buffer, MAX_LINE, "Executed: %s", submenus[current_menu][index]);
                                write_to_output(buffer);
                            }
                            // Clear input after selection
                            input_buffer[0] = '\0';
                            input_len = 0;
                            clear_input_file(); // Clear file only after selection
                            display_menu(current_menu, current_menu, input_buffer);
                        } else {
                            write_to_output("Invalid submenu index");
                            // Clear input for invalid selection
                            input_buffer[0] = '\0';
                            input_len = 0;
                            clear_input_file(); // Clear file only after selection
                            display_menu(current_menu, current_menu, input_buffer);
                        }
                    }
                }
            }
        } else {
            write_to_output("Error: Could not stat commands_gl.txt");
            display_menu(current_menu, current_menu, input_buffer);
        }
        sleep(1); // Poll every second to avoid excessive CPU usage
    }
    return 0;
}
