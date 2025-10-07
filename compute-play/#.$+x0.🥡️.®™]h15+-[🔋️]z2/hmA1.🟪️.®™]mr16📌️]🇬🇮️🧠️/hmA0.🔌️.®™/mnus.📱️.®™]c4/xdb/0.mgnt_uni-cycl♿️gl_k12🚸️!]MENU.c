#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <sys/file.h>

#define MAX_LINE 256
#define MAX_ACTIONS 100
#define MAX_BACKGROUND 100
#define COMMAND_FILE "commands.txt"
#define OUTPUT_FILE "action_output.txt"
#define PID_FILE "background_pids.txt"
#define DEBUG_LOG "debug.log"
#define TIMEOUT_MS 100

static int initial_command_count = 0;
static int processed_command_count = 0;
static int ignore_ctrl_c = 0;
static int use_cli_input = 0;
static const char *command_file_path = NULL;
static struct termios orig_termios;
static pid_t background_pids[MAX_BACKGROUND];
static int background_input_fds[MAX_BACKGROUND];
static int num_background = 0;

// ADDED: Global to track initial file_processed_lines
static int initial_file_processed_lines = 0;

// Function prototypes
void debug_log(const char *msg);
void log_pid_to_file(pid_t pid, const char *exec);
void remove_pid_from_file(pid_t pid);
void cleanup_background_pids(void);
void sigint_handler(int sig);
void sigsegv_handler(int sig);
void setup_terminal(void);
void restore_terminal(void);
int count_lines(const char *path);
int read_command(int in_submenu, int at_main_menu, int prompt_row, char *result_buffer, size_t result_size);
int handle_interactive_submenu(const char *exec, int processed_count, int mode);

void debug_log(const char *msg) {
    FILE *fp = fopen(DEBUG_LOG, "a");
    if (fp) {
        flock(fileno(fp), LOCK_EX);
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(fp, "[%s] %s (PID: %d)\n", timestamp, msg, getpid());
        fflush(fp);
        fsync(fileno(fp));
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
    }
}

void log_pid_to_file(pid_t pid, const char *exec) {
    FILE *fp = fopen(PID_FILE, "a");
    if (fp) {
        flock(fileno(fp), LOCK_EX);
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(fp, "[%s] PID: %d, Exec: %s\n", timestamp, pid, exec);
        fflush(fp);
        fsync(fileno(fp));
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
    }
}

void remove_pid_from_file(pid_t pid) {
    FILE *fp = fopen(PID_FILE, "r");
    if (!fp) return;

    flock(fileno(fp), LOCK_EX);
    char temp_file[] = "temp_pids.txt";
    FILE *temp_fp = fopen(temp_file, "w");
    if (!temp_fp) {
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
        return;
    }
    flock(fileno(temp_fp), LOCK_EX);

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        int logged_pid;
        if (sscanf(line, "[%*[^]]] PID: %d", &logged_pid) == 1 && logged_pid != pid) {
            fputs(line, temp_fp);
        }
    }

    flock(fileno(temp_fp), LOCK_UN);
    flock(fileno(fp), LOCK_UN);
    fclose(fp);
    fclose(temp_fp);
    rename(temp_file, PID_FILE);
}

void restore_terminal() {
    if (tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios) == -1) {
        debug_log("Failed to restore terminal attributes");
    }
}

void cleanup_background_pids() {
    char msg[768];
    int i;
    for (i = 0; i < num_background; i++) {
        snprintf(msg, sizeof(msg), "Sending SIGHUP to background PID %d", background_pids[i]);
        debug_log(msg);
        kill(background_pids[i], SIGHUP);
        snprintf(msg, sizeof(msg), "Sending SIGTERM to background PID %d", background_pids[i]);
        debug_log(msg);
        kill(background_pids[i], SIGTERM);
        snprintf(msg, sizeof(msg), "Sending SIGHUP to process group -%d", background_pids[i]);
        debug_log(msg);
        kill(-background_pids[i], SIGHUP);
        snprintf(msg, sizeof(msg), "Sending SIGTERM to process group -%d", background_pids[i]);
        debug_log(msg);
        kill(-background_pids[i], SIGTERM);
        int status;
        int ret = waitpid(background_pids[i], &status, WNOHANG);
        if (ret == 0) {
            snprintf(msg, sizeof(msg), "Waitpid timed out for PID %d, sending SIGKILL", background_pids[i]);
            debug_log(msg);
            kill(background_pids[i], SIGKILL);
            kill(-background_pids[i], SIGKILL);
            waitpid(background_pids[i], &status, 0);
        }
        remove_pid_from_file(background_pids[i]);
        if (background_input_fds[i] != -1) {
            close(background_input_fds[i]);
            background_input_fds[i] = -1;
        }
    }
    num_background = 0;
}

void sigint_handler(int sig) {
    debug_log("SIGINT received");
    // No longer setting ctrl_c_received
}

void sigsegv_handler(int sig) {
    debug_log("SIGSEGV received, cleaning up");
    cleanup_background_pids();
    restore_terminal();
    fprintf(stderr, "Segmentation fault, exiting...\n");
    exit(1);
}

void setup_terminal() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        debug_log("Failed to get terminal attributes");
        return;
    }
    struct termios new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO | ISIG);
    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termios) == -1) {
        debug_log("Failed to set terminal attributes");
        return;
    }
    if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1) {
        debug_log("Failed to set non-blocking stdin");
    }
}

int count_lines(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    flock(fileno(fp), LOCK_SH);
    int count = 0;
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        count++;
    }
    flock(fileno(fp), LOCK_UN);
    fclose(fp);
    return count;
}

int read_command(int in_submenu, int at_main_menu, int prompt_row, char *result_buffer, size_t result_size) {
    static char input_buffer[MAX_LINE] = "";
    static int buffer_pos = 0;
    static char escape_sequence[3] = {0};
    static int escape_pos = 0;
    static int file_processed_lines = 0;
    char msg[768];

    // Initialize file_processed_lines on first call
    if (file_processed_lines == 0 && initial_file_processed_lines > 0) {
        file_processed_lines = initial_file_processed_lines;
        snprintf(msg, sizeof(msg), "Initialized file_processed_lines to %d on first call", file_processed_lines);
        debug_log(msg);
    }

    // Handle CLI input by writing to commands.txt
    if (use_cli_input) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        struct timeval timeout = { .tv_sec = 0, .tv_usec = TIMEOUT_MS * 1000 };
        int select_result = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
            char c;
            ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
            if (bytes_read > 0) {
                int key = (unsigned char)c;

                if (key == 3) {
                    debug_log("Ctrl+C received in CLI mode");
                    FILE *fp = fopen(command_file_path, "a");
                    if (fp) {
                        flock(fileno(fp), LOCK_EX);
                        fputc('\x03', fp); // Write Ctrl+C character
                        fputc('\n', fp);
                        fflush(fp);
                        fsync(fileno(fp));
                        flock(fileno(fp), LOCK_UN);
                        fclose(fp);
                    }
                    return -1;
                }

                if (key == 27) {
                    escape_sequence[escape_pos++] = c;
                    if (escape_pos == 1) return -1;
                    if (escape_pos == 2 && escape_sequence[1] != '[') {
                        escape_pos = 0;
                        return -1;
                    }
                    if (escape_pos == 3) {
                        if (escape_sequence[1] == '[' && (c == 'A' || c == 'B' || c == 'C' || c == 'D')) {
                            FILE *fp = fopen(command_file_path, "a");
                            if (fp) {
                                flock(fileno(fp), LOCK_EX);
                                fprintf(fp, "%c%c%c", escape_sequence[0], escape_sequence[1], c);
                                fflush(fp);
                                fsync(fileno(fp));
                                flock(fileno(fp), LOCK_UN);
                                fclose(fp);
                            }
                        }
                        escape_pos = 0;
                        return -1;
                    }
                    return -1;
                } else if (escape_pos > 0) {
                    escape_sequence[escape_pos++] = c;
                    if (escape_pos == 2 && c != '[') {
                        escape_pos = 0;
                        return -1;
                    }
                    if (escape_pos == 3) {
                        if (escape_sequence[1] == '[' && (c == 'A' || c == 'B' || c == 'C' || c == 'D')) {
                            FILE *fp = fopen(command_file_path, "a");
                            if (fp) {
                                flock(fileno(fp), LOCK_EX);
                                fprintf(fp, "%c%c%c", escape_sequence[0], escape_sequence[1], c);
                                fflush(fp);
                                fsync(fileno(fp));
                                flock(fileno(fp), LOCK_UN);
                                fclose(fp);
                            }
                        }
                        escape_pos = 0;
                        return -1;
                    }
                    return -1;
                }

                FILE *fp = fopen(command_file_path, "a");
                if (fp) {
                    flock(fileno(fp), LOCK_EX);
                    if (key == 10 || key == 13) {
                        write(STDOUT_FILENO, "\n", 1);
                        if (buffer_pos > 0) {
                            input_buffer[buffer_pos] = '\0';
                            fprintf(fp, "%s\n", input_buffer);
                            buffer_pos = 0;
                            input_buffer[0] = '\0';
                            debug_log("Wrote CLI input to commands.txt");
                        }
                        fprintf(fp, "%d\n", key);
                    } else if (key == 127) {
                        if (buffer_pos > 0) {
                            buffer_pos--;
                            input_buffer[buffer_pos] = '\0';
                            write(STDOUT_FILENO, "\b \b", 3);
                            fprintf(fp, "%d\n", key);
                        }
                    } else if (key == 'q' || key == 'Q') {
                        write(STDOUT_FILENO, &c, 1);
                        fprintf(fp, "q\n");
                        debug_log("Wrote 'q' to commands.txt");
                    } else if (key >= 32 && key <= 126) {
                        write(STDOUT_FILENO, &c, 1);
                        if (buffer_pos < MAX_LINE - 1) {
                            input_buffer[buffer_pos++] = c;
                        }
                    }
                    fflush(fp);
                    fsync(fileno(fp));
                    flock(fileno(fp), LOCK_UN);
                    fclose(fp);
                } else {
                    debug_log("Failed to open commands.txt for writing CLI input");
                }
                return -1;
            }
        }
    }

    // Process commands.txt
    int current_line_count = count_lines(command_file_path);
    if (current_line_count < file_processed_lines) {
        snprintf(msg, sizeof(msg), "commands.txt truncated, resetting file_processed_lines from %d to 0", file_processed_lines);
        debug_log(msg);
        file_processed_lines = 0;
    }
    if (current_line_count > file_processed_lines) {
        FILE *fp = fopen(command_file_path, "r");
        if (!fp) {
            debug_log("Failed to open command file");
            return -1;
        }
        flock(fileno(fp), LOCK_SH);

        char line[MAX_LINE];
        int current_line = 0;
        int result = -1;
        while (fgets(line, MAX_LINE, fp)) {
            current_line++;
            if (current_line <= file_processed_lines) continue;

            line[strcspn(line, "\n")] = '\0';
            if (strlen(line) == 0) {
                snprintf(msg, sizeof(msg), "Ignored empty line %d in command file", current_line);
                debug_log(msg);
                continue;
            }
            if (strcmp(line, "q") == 0 || strcmp(line, "Q") == 0) {
                if (in_submenu) {
                    write(STDOUT_FILENO, "q\n", 2);
                    debug_log("Read 'q' from command file in submenu");
                    fclose(fp);
                    flock(fileno(fp), LOCK_UN);
                    file_processed_lines = current_line_count;
                    return -3;
                }
            } else if (strcmp(line, "\x03") == 0) {
                debug_log("Ctrl+C detected in file mode");
                printf("\033[%d;1H\033[K%s\n", prompt_row, at_main_menu ? "Quitting..." : "Returning to module selection...");
                fflush(stdout);
                fclose(fp);
                flock(fileno(fp), LOCK_UN);
                file_processed_lines = current_line_count;
                return in_submenu ? -2 : (at_main_menu ? -4 : -2);
            } else if (strcmp(line, "10") == 0 || strcmp(line, "13") == 0) {
                if (buffer_pos > 0) {
                    input_buffer[buffer_pos] = '\0';
                    strncpy(result_buffer, input_buffer, result_size - 1);
                    result_buffer[result_size - 1] = '\0';
                    buffer_pos = 0;
                    input_buffer[0] = '\0';
                    write(STDOUT_FILENO, "\n", 1);
                    debug_log("Processed Enter from command file");
                    fclose(fp);
                    flock(fileno(fp), LOCK_UN);
                    file_processed_lines = current_line_count;
                    return 0;
                }
            } else if (strcmp(line, "127") == 0) {
                if (buffer_pos > 0) {
                    buffer_pos--;
                    input_buffer[buffer_pos] = '\0';
                    write(STDOUT_FILENO, "\b \b", 3);
                    debug_log("Processed Backspace from command file");
                }
            } else if (line[0] == 27 && strlen(line) == 3 && line[1] == '[' && (line[2] == 'A' || line[2] == 'B' || line[2] == 'C' || line[2] == 'D')) {
                if (in_submenu) {
                    for (int i = 0; i < num_background; i++) {
                        if (background_input_fds[i] != -1) {
                            write(background_input_fds[i], line, strlen(line));
                        }
                    }
                }
            } else {
                int valid = 1;
                for (size_t i = 0; i < strlen(line); i++) {
                    if (!(isprint(line[i]) || line[i] == '\n')) {
                        valid = 0;
                        break;
                    }
                }
                if (valid) {
                    write(STDOUT_FILENO, line, strlen(line));
                    if (buffer_pos < MAX_LINE - 1) {
                        int len = strlen(line);
                        if (buffer_pos + len < MAX_LINE) {
                            strncpy(input_buffer + buffer_pos, line, len);
                            buffer_pos += len;
                            snprintf(msg, sizeof(msg), "Processed input line %d: %s", current_line, line);
                            debug_log(msg);
                        }
                    }
                } else {
                    snprintf(msg, sizeof(msg), "Ignored invalid input line %d: %s", current_line, line);
                    debug_log(msg);
                }
            }
        }
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
        file_processed_lines = current_line_count;
        return result;
    }

    return -1;
}

int handle_interactive_submenu(const char *exec, int processed_count, int mode) {
    debug_log("Entering handle_interactive_submenu");

    struct sigaction sa = {.sa_handler = sigint_handler, .sa_flags = SA_RESTART};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int pipefd_in[2], pipefd_out[2];
    if (pipe(pipefd_in) == -1 || pipe(pipefd_out) == -1) {
        debug_log("Failed to create pipes");
        printf("\033[2J\033[HError: Failed to create pipes\n");
        fflush(stdout);
        return processed_count;
    }

    pid_t pid = fork();
    if (pid == -1) {
        debug_log("Failed to fork");
        close(pipefd_in[0]); close(pipefd_in[1]);
        close(pipefd_out[0]); close(pipefd_out[1]);
        return processed_count;
    }

    if (pid == 0) {
        close(pipefd_in[1]);
        close(pipefd_out[0]);
        setpgid(0, 0);
        dup2(pipefd_in[0], STDIN_FILENO);
        dup2(pipefd_out[1], STDOUT_FILENO);
        dup2(pipefd_out[1], STDERR_FILENO);
        close(pipefd_in[0]);
        close(pipefd_out[1]);
        execl("/bin/sh", "sh", "-c", exec, NULL);
        debug_log("Failed to exec");
        exit(1);
    }

    log_pid_to_file(pid, exec);
    close(pipefd_in[0]);
    close(pipefd_out[1]);
    fcntl(pipefd_out[0], F_SETFL, O_NONBLOCK);

    char msg[768];
    snprintf(msg, sizeof(msg), "Started submenu process with PID %d", pid);
    debug_log(msg);

    printf("\033[2J\033[H");
    debug_log("Displayed submenu");
    printf("\n");
    tcdrain(STDOUT_FILENO);
    fflush(stdout);

    int child_running = 1;
    char output_buffer[MAX_LINE];
    int output_pos = 0;
    int status;

    while (child_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(pipefd_out[0], &read_fds);
        int max_fd = pipefd_out[0];

        if (use_cli_input) {
            FD_SET(STDIN_FILENO, &read_fds);
            if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        }

        struct timeval timeout = { .tv_sec = 0, .tv_usec = TIMEOUT_MS * 1000 };
        int select_result = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (select_result > 0) {
            if (FD_ISSET(pipefd_out[0], &read_fds)) {
                ssize_t bytes_read = read(pipefd_out[0], output_buffer + output_pos, MAX_LINE - output_pos - 1);
                if (bytes_read > 0) {
                    output_pos += bytes_read;
                    output_buffer[output_pos] = '\0';
                    printf("%s", output_buffer);
                    fflush(stdout);
                    output_pos = 0;
                } else if (bytes_read == -1 && errno != EAGAIN) {
                    debug_log("Error reading child output");
                    child_running = 0;
                }
            }
        }

        char input_buffer[MAX_LINE];
        int result = read_command(1, 0, 1, input_buffer, sizeof(input_buffer));
        if (result == -3 || result == -2) { // Handle 'q' and Ctrl+C
            snprintf(msg, sizeof(msg), "Received %s in submenu, killing PID %d", 
                     result == -3 ? "'q'" : "Ctrl+C", pid);
            debug_log(msg);
            printf("\033[2J\033[HTerminating submenu...\n");
            fflush(stdout);
            kill(pid, SIGINT); // Send SIGINT to submenu process
            kill(-pid, SIGINT); // Send SIGINT to process group
            waitpid(pid, &status, 0);
            remove_pid_from_file(pid);
            child_running = 0;
            processed_count = count_lines(command_file_path);
            break;
        } else if (result == 0 && input_buffer[0] != '\0') {
            write(pipefd_in[1], input_buffer, strlen(input_buffer));
            write(pipefd_in[1], "\n", 1);
            debug_log("Sent input to submenu child process");
            processed_count = count_lines(command_file_path);
        }

        if (waitpid(pid, &status, WNOHANG) == pid) {
            snprintf(msg, sizeof(msg), "Submenu process %d exited", pid);
            debug_log(msg);
            remove_pid_from_file(pid);
            child_running = 0;
            processed_count = count_lines(command_file_path);
            break;
        }

        usleep(10000);
    }

    close(pipefd_in[1]);
    close(pipefd_out[0]);
    tcflush(STDIN_FILENO, TCIFLUSH);
    snprintf(msg, sizeof(msg), "Exiting submenu");
    debug_log(msg);
    return processed_count;
}

int main(int argc, char *argv[]) {
    debug_log("Program started");
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        debug_log("Failed to set SIGINT handler");
    }

    sa.sa_handler = sigsegv_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        debug_log("Failed to set SIGSEGV handler");
    }

    if (argc > 1) {
        command_file_path = argv[1];
        ignore_ctrl_c = 0;
        use_cli_input = 0;
        debug_log("Running in command file mode");
    } else {
        command_file_path = COMMAND_FILE;
        ignore_ctrl_c = 0;
        use_cli_input = 1;
        setup_terminal();
        debug_log("Running in CLI and file mode");
    }

    initial_file_processed_lines = count_lines(command_file_path);
    char msg[768];
    snprintf(msg, sizeof(msg), "Initialized initial_file_processed_lines to %d", initial_file_processed_lines);
    debug_log(msg);
    initial_command_count = initial_file_processed_lines;
    processed_command_count = initial_command_count;

    for (int i = 0; i < MAX_BACKGROUND; i++) {
        background_input_fds[i] = -1;
    }

    int mode = 2;

    while (1) {
        printf("\033[2J\033[H");
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "./+x/menu_kvp]d3]BEST.+x show");
        debug_log("Executing system command");
        int ret = system(cmd);
        snprintf(msg, sizeof(msg), "System command '%s' returned %d", cmd, ret);
        debug_log(msg);

        printf("\nAvailable Modules:\n");
        debug_log("Opening action_output.txt");
        FILE *fp = fopen(OUTPUT_FILE, "r");
        int line_count = 2;
        char line[MAX_LINE];
        if (fp) {
            flock(fileno(fp), LOCK_SH);
            while (fgets(line, MAX_LINE, fp)) {
                printf("%s", line);
                line_count++;
            }
            flock(fileno(fp), LOCK_UN);
            fclose(fp);
            debug_log("Closed action_output.txt");
        } else {
            debug_log("Failed to open output file");
            printf("\n");
            line_count++;
        }

        printf("Enter module index: ");
        fflush(stdout);

        debug_log("Reading index command");
        tcflush(STDIN_FILENO, TCIFLUSH);
        char index_input[MAX_LINE];
        int index = -1;
        int result = read_command(0, 1, line_count + 1, index_input, sizeof(index_input));
        while (result == -1) {
            usleep(100000);
            result = read_command(0, 1, line_count + 1, index_input, sizeof(index_input));
        }
        if (result == -4) {
            printf("\033[%d;1H\033[KQuitting...\n", line_count + 1);
            fflush(stdout);
            cleanup_background_pids();
            restore_terminal();
            debug_log("Exiting via Ctrl+C");
            exit(0);
        }
        if (result == -2) {
            printf("\033[%d;1H\033[KReturning to module selection...\n", line_count + 1);
            fflush(stdout);
            continue; // Return to main menu
        }
        if (result == 0) {
            index = atoi(index_input);
        }
        if (index < 1 || index > MAX_ACTIONS) {
            printf("\033[%d;1H\033[KInvalid index (use 1-%d)\n", line_count + 1, MAX_ACTIONS);
            fflush(stdout);
            usleep(20000);
            continue;
        }

        tcflush(STDIN_FILENO, TCIFLUSH);
        snprintf(msg, sizeof(msg), "Cleared STDIN before launching module %d", index);
        debug_log(msg);

        snprintf(cmd, sizeof(cmd), "./+x/menu_kvp]d3]BEST.+x launch %d", index);
        debug_log("Executing launch system command");
        ret = system(cmd);
        snprintf(msg, sizeof(msg), "Launch command '%s' returned %d", cmd, ret);
        debug_log(msg);

        int action_id = -1, is_submenu = 0, pid = 0;
        char exec[MAX_LINE] = "";
        debug_log("Opening action_output.txt for module details");
        fp = fopen(OUTPUT_FILE, "r");
        if (fp) {
            flock(fileno(fp), LOCK_SH);
            while (fgets(line, MAX_LINE, fp)) {
                if (sscanf(line, "action_id: %d", &action_id) == 1) continue;
                if (sscanf(line, "is_submenu: %d", &is_submenu) == 1) continue;
                if (sscanf(line, "pid: %d", &pid) == 1) continue;
                if (sscanf(line, "exec: %[^\n]", exec) == 1) continue;
            }
            flock(fileno(fp), LOCK_UN);
            fclose(fp);
            debug_log("Closed action_output.txt after module details");
        } else {
            debug_log("Failed to open output file for module details");
        }

        if (is_submenu && exec[0] != '\0') {
            snprintf(msg, sizeof(msg), "Entering submenu for module %d, PID %d, exec %.100s", index, pid, exec);
            debug_log(msg);
            processed_command_count = handle_interactive_submenu(exec, processed_command_count, mode);
        } else if (pid > 0 && exec[0] != '\0') {
            if (num_background < MAX_BACKGROUND) {
                int pipefd_in[2];
                if (pipe(pipefd_in) == -1) {
                    snprintf(msg, sizeof(msg), "Failed to create pipe for PID %d", pid);
                    debug_log(msg);
                    kill(pid, SIGTERM);
                    waitpid(pid, NULL, 0);
                    remove_pid_from_file(pid);
                    continue;
                }
                pid_t new_pid = fork();
                if (new_pid == -1) {
                    snprintf(msg, sizeof(msg), "Failed to fork for PID %d", pid);
                    debug_log(msg);
                    close(pipefd_in[0]);
                    close(pipefd_in[1]);
                    kill(pid, SIGTERM);
                    waitpid(pid, NULL, 0);
                    remove_pid_from_file(pid);
                    continue;
                }
                if (new_pid == 0) {
                    close(pipefd_in[1]);
                    setpgid(0, 0);
                    dup2(pipefd_in[0], STDIN_FILENO);
                    close(pipefd_in[0]);
                    execl("/bin/sh", "sh", "-c", exec, NULL);
                    debug_log("Failed to exec background process");
                    exit(1);
                }
                close(pipefd_in[0]);
                background_pids[num_background] = new_pid;
                background_input_fds[num_background] = pipefd_in[1];
                num_background++;
                snprintf(msg, sizeof(msg), "Started background process %d for module %d, exec %.100s", new_pid, index, exec);
                debug_log(msg);
                log_pid_to_file(new_pid, exec);
            } else {
                snprintf(msg, sizeof(msg), "Background process limit reached for PID %d", pid);
                debug_log(msg);
                kill(pid, SIGTERM);
                waitpid(pid, NULL, 0);
                remove_pid_from_file(pid);
            }
        }
    }
    cleanup_background_pids();
    restore_terminal();
    debug_log("Program exiting");
    return 0;
}
