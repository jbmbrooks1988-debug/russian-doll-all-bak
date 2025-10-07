#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MAX_LINE 256
#define MAX_ACTIONS 100
#define OUTPUT_FILE "action_output.txt"
#define PID_FILE "background_pids.txt"
#define DEBUG_LOG "debug.log"
#define TIMEOUT_MS 100
#define MENU_FILE "menu_kvp.txt"

static char modules[MAX_ACTIONS][2][MAX_LINE];
static int module_count = 0;

void debug_log(const char *msg) {
    FILE *fp = fopen(DEBUG_LOG, "a");
    if (fp) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(fp, "[%s] %s (PID: %d)\n", timestamp, msg, getpid());
        fclose(fp);
    }
}

void debug_log_exec(const char *msg, const char *exec) {
    char full_msg[512];
    snprintf(full_msg, sizeof(full_msg), "%s: %s", msg, exec);
    debug_log(full_msg);
}

void debug_log_errno(const char *msg) {
    char full_msg[512];
    snprintf(full_msg, sizeof(full_msg), "%s: %s", msg, strerror(errno));
    debug_log(full_msg);
}

void log_pid_to_file(pid_t pid, const char *exec) {
    FILE *fp = fopen(PID_FILE, "a");
    if (fp) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(fp, "[%s] PID: %d, Exec: %s\n", timestamp, pid, exec);
        fclose(fp);
    }
}

void remove_pid_from_file(pid_t pid) {
    FILE *fp = fopen(PID_FILE, "r");
    if (!fp) return;
    char temp_file[] = "temp_pids.txt";
    FILE *temp_fp = fopen(temp_file, "w");
    if (!temp_fp) {
        fclose(fp);
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        int logged_pid;
        if (sscanf(line, "[%*[^]]] PID: %d", &logged_pid) == 1 && logged_pid != pid) {
            fputs(line, temp_fp);
        }
    }
    fclose(fp);
    fclose(temp_fp);
    rename(temp_file, PID_FILE);
}

void load_modules() {
    FILE *fp = fopen(MENU_FILE, "r");
    if (!fp) {
        debug_log("Failed to open menu_kvp.txt");
        return;
    }
    module_count = 0;
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp) && module_count < MAX_ACTIONS) {
        char *key = strtok(line, " \t");
        char *value = strtok(NULL, "\n");
        if (key && value) {
            snprintf(modules[module_count][0], MAX_LINE, "%d", module_count + 1);
            snprintf(modules[module_count][1], MAX_LINE, "%s", value);
            module_count++;
        }
    }
    fclose(fp);
    debug_log("Loaded modules from menu_kvp.txt");
}

void show_modules() {
    FILE *fp = fopen(OUTPUT_FILE, "w");
    if (fp) {
        for (int i = 0; i < module_count; i++) {
            fprintf(fp, "%s: %s\n", modules[i][0], modules[i][1]);
        }
        fclose(fp);
        debug_log("Wrote module list to output file");
    } else {
        debug_log("Failed to open output file for modules");
    }
}

int check_submenu(int pipe_fd, pid_t pid, const char *exec) {
    char msg[512];
    snprintf(msg, sizeof(msg), "Checking submenu for PID %d, exec: %s", pid, exec);
    debug_log(msg);

    if (waitpid(pid, NULL, WNOHANG) == pid) {
        snprintf(msg, sizeof(msg), "Child PID %d exited early", pid);
        debug_log(msg);
        return 0;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe_fd, &read_fds);
    struct timeval timeout = { .tv_sec = 0, .tv_usec = TIMEOUT_MS * 1000 };
    int ret = select(pipe_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret < 0) {
        debug_log_errno("Select error in check_submenu");
        return 0;
    }
    if (ret == 0) {
        debug_log("No output detected within timeout, assuming background process");
        return 0;
    }

    char buffer[MAX_LINE];
    ssize_t bytes_read = read(pipe_fd, buffer, MAX_LINE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        snprintf(msg, sizeof(msg), "Read %zd bytes: '%s'", bytes_read, buffer);
        debug_log(msg);
        debug_log("Detected output, assuming submenu");
        return 1;
    } else if (bytes_read == 0) {
        snprintf(msg, sizeof(msg), "Pipe closed by PID %d", pid);
        debug_log(msg);
        return 0;
    }
    debug_log_errno("Read error in check_submenu");
    return 0;
}

void launch_module(int index) {
    FILE *fp = fopen(OUTPUT_FILE, "w");
    if (!fp) {
        debug_log("Failed to open output file");
        return;
    }

    if (index < 1 || index > module_count) {
        fprintf(fp, "action_id: -1\n");
        fclose(fp);
        debug_log("Invalid action_id");
        return;
    }

    const char *exec = modules[index - 1][1];
    int pipefd_out[2];
    if (pipe(pipefd_out) == -1) {
        fprintf(fp, "action_id: %d\nis_submenu: false\n", index);
        fclose(fp);
        debug_log_errno("Failed to create output pipe");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd_out[0]);
        dup2(pipefd_out[1], STDOUT_FILENO);
        dup2(pipefd_out[1], STDERR_FILENO);
        close(pipefd_out[1]);
        setpgid(0, 0);
        debug_log_exec("Child about to exec", exec);
        execl("/bin/sh", "sh", "-c", exec, NULL);
        debug_log_errno("Failed to exec module");
        exit(1);
    } else if (pid > 0) {
        close(pipefd_out[1]);
        int is_submenu = check_submenu(pipefd_out[0], pid, exec);
        if (!is_submenu) {
            fcntl(pipefd_out[0], F_SETFL, O_NONBLOCK);
            int null_fd = open("/dev/null", O_RDWR);
            if (null_fd != -1) {
                dup2(null_fd, pipefd_out[0]);
                dup2(null_fd, STDOUT_FILENO);
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);
            }
        }
        log_pid_to_file(pid, exec);
        fprintf(fp, "action_id: %d\n", index);
        fprintf(fp, "pid: %d\n", pid);
        fprintf(fp, "is_submenu: %d\n", is_submenu);
        fprintf(fp, "exec: %s\n", exec);
        fclose(fp);
        char msg[256];
        snprintf(msg, sizeof(msg), "Launched module %d, is_submenu: %d, pid: %d, exec: %s", index, is_submenu, pid, exec);
        debug_log(msg);
        close(pipefd_out[0]);
    } else {
        debug_log_errno("Fork failed");
        close(pipefd_out[0]);
        close(pipefd_out[1]);
        fprintf(fp, "action_id: %d\nis_submenu: false\n", index);
        fclose(fp);
    }
}

int main(int argc, char *argv[]) {
    load_modules();
    if (argc < 2) {
        debug_log("No command provided");
        return 1;
    }
    if (strcmp(argv[1], "show") == 0) {
        show_modules();
    } else if (strcmp(argv[1], "launch") == 0 && argc == 3) {
        int index = atoi(argv[2]);
        launch_module(index);
    } else {
        debug_log("Invalid command");
        return 1;
    }
    return 0;
}
