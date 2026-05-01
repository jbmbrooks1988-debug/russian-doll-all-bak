/*
 * p2p_manager.c - P2P-NET GUI Manager
 * Purpose: Handle GUI input, call P2P ops, update display
 * 
 * CPU-SAFE: Signal handling + fork/exec/waitpid pattern
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define MAX_LINE 1024

/* CPU-SAFE: Global shutdown flag */
static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

/* CPU-SAFE: Helper to run external commands */
static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

char project_root[MAX_PATH] = ".";
char current_wallet[64] = "abc123def456";
char current_subnet[32] = "chat";
char last_response[MAX_LINE] = "P2P-NET Ready. Press 1-7 for actions.";

/* Wallet Selector State */
char wallets[10][64];
int wallet_count = 0;
int wallet_idx = 0;

static void scan_wallets() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/p2p-net/pieces/chat/inbox", project_root);
    DIR *d = opendir(path);
    if (!d) return;
    
    struct dirent *dir;
    wallet_count = 0;
    while ((dir = readdir(d)) != NULL && wallet_count < 10) {
        if (dir->d_name[0] == '.') continue;
        
        /* Check if it's a directory */
        struct stat st;
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, dir->d_name);
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(wallets[wallet_count], dir->d_name, sizeof(wallets[wallet_count]) - 1);
            if (strcmp(wallets[wallet_count], current_wallet) == 0) {
                wallet_idx = wallet_count;
            }
            wallet_count++;
        }
    }
    closedir(d);
}

static void get_wallet_selector(char *buf, size_t size) {
    buf[0] = '\0';
    for (int i = 0; i < wallet_count; i++) {
        if (i == wallet_idx) strncat(buf, "[", size - strlen(buf) - 1);
        strncat(buf, wallets[i], size - strlen(buf) - 1);
        if (i == wallet_idx) strncat(buf, "]", size - strlen(buf) - 1);
        if (i < wallet_count - 1) strncat(buf, " ", size - strlen(buf) - 1);
    }
}

/* Helper to find location_kvp by walking up */
static int find_location_kvp(char *out_path, size_t size) {
    char current[MAX_PATH];
    if (!getcwd(current, sizeof(current))) return 0;
    
    while (strlen(current) > 1) {
        snprintf(out_path, size, "%s/pieces/locations/location_kvp", current);
        if (access(out_path, R_OK) == 0) return 1;
        
        /* Go up one level */
        char *last_slash = strrchr(current, '/');
        if (!last_slash) break;
        *last_slash = '\0';
    }
    return 0;
}

static void resolve_paths() {
    char kvp_path[MAX_PATH];
    if (find_location_kvp(kvp_path, sizeof(kvp_path))) {
        FILE *kvp = fopen(kvp_path, "r");
        if (kvp) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), kvp)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = line, *v = eq + 1;
                    char *nl = strchr(v, '\n');
                    if (nl) *nl = '\0';
                    if (strcmp(k, "project_root") == 0) {
                        strncpy(project_root, v, sizeof(project_root) - 1);
                    }
                }
            }
            fclose(kvp);
        }
    }
}

static void set_response(const char* msg) {
    strncpy(last_response, msg, sizeof(last_response) - 1);
    char *path = NULL;
    if (asprintf(&path, "%s/projects/p2p-net/manager/response.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "%s", msg); fclose(f); }
        free(path);
    }
}

static void write_state() {
    char selector[MAX_LINE];
    get_wallet_selector(selector, sizeof(selector));

    /* Populate GUI Variables (Placeholders/Initial State) */
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "wallet_address=%s\n", current_wallet);
            fprintf(f, "wallet_selector=%s\n", selector);
            fprintf(f, "current_subnet=%s\n", current_subnet);
            fprintf(f, "subnet_list=[chat] general auction\n");
            fprintf(f, "unread_count=0\n");
            fprintf(f, "total_count=0\n");
            fprintf(f, "inbox_list=No messages\n");
            fprintf(f, "message_preview=No message selected\n");
            fprintf(f, "peer_count=0\n");
            fprintf(f, "peer_list=No peers connected\n");
            fprintf(f, "p2p_response=%s\n", last_response);
            fprintf(f, "last_key=None\n");
            fclose(f);
        }
        free(path);
    }

    /* Also write to project-specific state for persistence/backup */
    if (asprintf(&path, "%s/projects/p2p-net/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "wallet_address=%s\n", current_wallet);
            fprintf(f, "current_subnet=%s\n", current_subnet);
            fprintf(f, "last_response=%s\n", last_response);
            fprintf(f, "wallet_selector=%s\n", selector);
            fclose(f);
        }
        free(path);
    }
}

static void process_key(int key) {
    char cmd[MAX_CMD];
    
    if (key == '1') {
        /* Compose Message */
        snprintf(cmd, sizeof(cmd), 
                 "%s/projects/p2p-net/ops/+x/compose_message.+x %s def456ghi789 %s \"Test\" \"GUI test message\"",
                 project_root, current_wallet, current_subnet);
        run_command(cmd);
        set_response("Message sent to def456ghi789");
    }
    else if (key == '2') {
        /* Check Inbox */
        snprintf(cmd, sizeof(cmd), 
                 "%s/projects/p2p-net/ops/+x/check_inbox.+x %s %s",
                 project_root, current_wallet, current_subnet);
        run_command(cmd);
        set_response("Inbox checked");
    }
    else if (key == '3') {
        /* Reply Message */
        snprintf(cmd, sizeof(cmd), 
                 "%s/projects/p2p-net/ops/+x/compose_message.+x %s def456ghi789 %s \"Re: Test\" \"GUI reply\"",
                 project_root, current_wallet, current_subnet);
        run_command(cmd);
        set_response("Reply sent");
    }
    else if (key == '4') {
        /* Read Message */
        snprintf(cmd, sizeof(cmd), 
                 "%s/projects/p2p-net/ops/+x/read_message.+x %s msg_001",
                 project_root, current_wallet);
        run_command(cmd);
        set_response("Message marked as read");
    }
    else if (key == '5') {
        /* Join Subnet */
        snprintf(cmd, sizeof(cmd), 
                 "%s/projects/p2p-net/ops/+x/broadcast_join.+x %s %s",
                 project_root, current_wallet, current_subnet);
        run_command(cmd);
        set_response("Joined subnet");
    }
    else if (key == '6') {
        /* Leave Subnet */
        snprintf(cmd, sizeof(cmd), 
                 "%s/projects/p2p-net/ops/+x/broadcast_leave.+x %s %s",
                 project_root, current_wallet, current_subnet);
        run_command(cmd);
        set_response("Left subnet");
    }
    else if (key == '7') {
        /* Switch Account */
        if (wallet_count > 0) {
            wallet_idx = (wallet_idx + 1) % wallet_count;
            strncpy(current_wallet, wallets[wallet_idx], sizeof(current_wallet) - 1);
            char resp[MAX_LINE];
            snprintf(resp, sizeof(resp), "Switched to account: %s", current_wallet);
            set_response(resp);
        } else {
            set_response("No other wallets found");
        }
    }
    
    write_state();
}

int main() {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    
    resolve_paths();
    scan_wallets();
    write_state();
    
    long last_pos = 0;
    struct stat st;
    char *hist_path = NULL;
    if (asprintf(&hist_path, "%s/projects/p2p-net/history.txt", project_root) == -1) return 1;
    
    if (stat(hist_path, &st) == 0) {
        last_pos = st.st_size;
    }
    else {
        /* Create if not exists */
        FILE *f = fopen(hist_path, "w");
        if (f) fclose(f);
        last_pos = 0;
    }
    
    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), hf)) {
                        int key = atoi(line);
                        if (key > 0) {
                            process_key(key);
                        }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
            }
        }
        
        usleep(16667);
    }
    
    free(hist_path);
    return 0;
}
