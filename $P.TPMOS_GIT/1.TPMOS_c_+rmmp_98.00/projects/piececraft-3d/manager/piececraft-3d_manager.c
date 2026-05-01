#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
static volatile sig_atomic_t g_shutdown = 0;
char active_piece[64] = "adam";

void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) kvp = fopen("../../../pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    strncpy(project_root, v, MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }
}

void update_manager_state(const char* resp) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/1.TPMOS_c_+rmmp_96.09/projects/piececraft-3d/manager/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "active_piece=%s\n", active_piece);
        fprintf(f, "last_response=%s\n", resp);
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    setpgid(0, 0);
    resolve_paths();

    update_manager_state("Welcome to Piececraft-3D");

    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", project_root);
    long last_pos = 0;
    struct stat st;

    printf("Piececraft-3D Manager initialized.\n");

    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0 && st.st_size > last_pos) {
            FILE *f = fopen(hist_path, "r");
            if (f) {
                fseek(f, last_pos, SEEK_SET);
                int key;
                while (fscanf(f, "%d", &key) == 1) {
                    if (key == '9') { // Tab or 9 to switch
                        if (strcmp(active_piece, "adam") == 0) strcpy(active_piece, "eve");
                        else strcpy(active_piece, "adam");
                        update_manager_state("Switched piece.");
                    }
                }
                last_pos = ftell(f);
                fclose(f);
            }
        }
        usleep(16667);
    }
    return 0;
}
