#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE 4096

static volatile sig_atomic_t g_shutdown = 0;

static void handle_signal(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static char *trim_ws(char *s) {
    char *end = NULL;
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static int root_has_anchors(const char *root) {
    char pieces_path[MAX_PATH_LEN];
    char projects_path[MAX_PATH_LEN];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

static void resolve_root(char *project_root, size_t size) {
    FILE *kvp = NULL;
    if (!getcwd(project_root, size)) {
        strncpy(project_root, ".", size - 1);
        project_root[size - 1] = '\0';
    }

    if (root_has_anchors(project_root)) return;

    kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *value = trim_ws(line + 13);
                if (root_has_anchors(value)) {
                    strncpy(project_root, value, size - 1);
                    project_root[size - 1] = '\0';
                }
                break;
            }
        }
        fclose(kvp);
    }
}

static void ensure_file(const char *path) {
    FILE *f = fopen(path, "a");
    if (f) fclose(f);
}

static int run_parser(const char *project_root) {
    char parser_path[MAX_PATH_LEN];
    pid_t pid;
    int status = 0;

    snprintf(parser_path, sizeof(parser_path), "%s/projects/wraith-pm/ops/+x/wraith_parser.+x", project_root);
    pid = fork();
    if (pid == 0) {
        execl(parser_path, "wraith_parser.+x", "projects/wraith-pm/layouts/desktop.chtmgl", "wraith-pm", (char *)NULL);
        perror("wraith-pm_manager: execl wraith_parser");
        _exit(127);
    }
    if (pid < 0) return 1;
    if (waitpid(pid, &status, 0) < 0) return 1;
    return status == 0 ? 0 : 1;
}

static int should_render_command(const char *cmd) {
    if (!cmd) return 0;
    return strstr(cmd, "wraith-pm::render") ||
           strstr(cmd, "wraith-pm::inspect") ||
           strstr(cmd, "wraith-pm::hitmap") ||
           strstr(cmd, "RENDER") ||
           strstr(cmd, "FRAME");
}

int main(void) {
    char project_root[MAX_PATH_LEN] = ".";
    char history_path[MAX_PATH_LEN];
    long last_history_pos = 0;

    setpgid(0, 0);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    resolve_root(project_root, sizeof(project_root));
    snprintf(history_path, sizeof(history_path), "%s/projects/wraith-pm/session/history.txt", project_root);
    ensure_file(history_path);

    run_parser(project_root);

    FILE *history = fopen(history_path, "r");
    if (history) {
        fseek(history, 0, SEEK_END);
        last_history_pos = ftell(history);
        fclose(history);
    }

    while (!g_shutdown) {
        int did_work = 0;
        history = fopen(history_path, "r");
        if (history) {
            char line[MAX_LINE];
            fseek(history, last_history_pos, SEEK_SET);
            while (fgets(line, sizeof(line), history)) {
                char *cmd = strstr(line, "COMMAND:");
                if (cmd && should_render_command(trim_ws(cmd + 8))) {
                    run_parser(project_root);
                    did_work = 1;
                }
            }
            last_history_pos = ftell(history);
            fclose(history);
        }
        usleep(did_work ? 16667 : 100000);
    }

    return 0;
}
