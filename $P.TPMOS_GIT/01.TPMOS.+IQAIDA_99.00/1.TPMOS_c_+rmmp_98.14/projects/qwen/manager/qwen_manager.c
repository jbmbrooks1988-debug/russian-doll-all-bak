#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define PROJECT_ID "qwen"
#define PIECE_PATH "projects/qwen/pieces/world_01/map_01/iqabel/"

static volatile sig_atomic_t should_exit = 0;
void handle_sig(int s) { (void)s; should_exit = 1; }

void trigger_render(const char* root) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", root) < 0) return;
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "M\n");
        fclose(f);
    }
    free(path);
}

void update_ui_vars(const char* root, const char* ai_state, const char* fsm_state, const char* resp_area, const char* sys_msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/qwen/manager/gui_state.txt", root) < 0) return;
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "ai_state=%s\n", ai_state);
        fprintf(f, "iqabel_fsm=%s\n", fsm_state);
        fprintf(f, "qwen_response_area=%s\n", resp_area);
        fprintf(f, "sys_msg=%s\n", sys_msg);
        fclose(f);
    }
    free(path);
}

char* get_cli_input(const char* root, const char* id) {
    char *path = NULL, *line = NULL;
    size_t len = 0;
    if (asprintf(&path, "%s/projects/qwen/manager/gui_state.txt", root) < 0) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return NULL; }
    while (getline(&line, &len, f) != -1) {
        if (strncmp(line, id, strlen(id)) == 0 && line[strlen(id)] == '=') {
            char *val = strdup(line + strlen(id) + 1);
            if (val[strlen(val)-1] == '\n') val[strlen(val)-1] = '\0';
            fclose(f); free(path); free(line);
            return val;
        }
    }
    fclose(f); free(path); free(line);
    return NULL;
}

int check_enter(const char* root, long *last_pos) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/keyboard/history.txt", root) < 0) return 0;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    fseek(f, *last_pos, SEEK_SET);
    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "KEY_PRESSED: 13")) found = 1;
    }
    *last_pos = ftell(f);
    fclose(f); free(path);
    return found;
}

int main(int argc, char *argv[]) {
    char *root = (argc > 1) ? argv[1] : ".";
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);
    setpgid(0, 0);

    long last_kb_pos = 0;
    // Seek to end of history initially to avoid triggering on old enters
    char *kb_path = NULL;
    asprintf(&kb_path, "%s/pieces/keyboard/history.txt", root);
    FILE *kf = fopen(kb_path, "r");
    if (kf) { fseek(kf, 0, SEEK_END); last_kb_pos = ftell(kf); fclose(kf); }
    free(kb_path);

    update_ui_vars(root, "IDLE", "IDLE", "║ Waiting for input...                                                       ║", "Ready.");
    trigger_render(root);

    while (!should_exit) {
        usleep(100000);
        if (check_enter(root, &last_kb_pos)) {
            char *input = get_cli_input(root, "input_text");
            if (input && strlen(input) > 0) {
                update_ui_vars(root, "THINKING", "THINKING", "║ Qwen is thinking...                                                        ║", "Querying AI...");
                trigger_render(root);

                char *cmd = NULL;
                asprintf(&cmd, "%s/projects/qwen/ops/+x/qwen_bridge.+x \"%s\"", root, input);
                FILE *pf = popen(cmd, "r");
                if (pf) {
                    char resp[4096] = {0};
                    char line[256];
                    while (fgets(line, sizeof(line), pf)) {
                        strncat(resp, line, sizeof(resp) - strlen(resp) - 1);
                    }
                    pclose(pf);
                    // Basic formatting for UI
                    char area[4096];
                    snprintf(area, sizeof(area), "║ %-74.74s ║", resp); // Very basic truncation
                    update_ui_vars(root, "IDLE", "IDLE", area, "Response received.");
                } else {
                    update_ui_vars(root, "ERROR", "IDLE", "║ Failed to call bridge.                                                     ║", "Bridge Error.");
                }
                trigger_render(root);
                free(cmd); free(input);
            }
        }
    }
    return 0;
}
