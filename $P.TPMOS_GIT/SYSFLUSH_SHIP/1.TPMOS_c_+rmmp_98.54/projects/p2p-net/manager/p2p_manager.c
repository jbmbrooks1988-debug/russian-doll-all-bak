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
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define MAX_LINE 1024
#define MAX_VAR_VALUE 65536

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
int gui_focus_index = 1;

static char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void load_kv_value(const char* filename, const char* key, char* out_val, size_t out_sz) {
    out_val[0] = '\0';
    FILE *f = fopen(filename, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(trim_str(line), key) == 0) {
                strncpy(out_val, trim_str(eq + 1), out_sz - 1);
                out_val[out_sz - 1] = '\0';
                break;
            }
        }
    }
    fclose(f);
}

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
                    char *k = trim_str(line), *v = trim_str(eq + 1);
                    if (strcmp(k, "project_root") == 0) {
                        strncpy(project_root, v, sizeof(project_root) - 1);
                    }
                }
            }
            fclose(kvp);
        }
    }

    /* Safety check: fallback to current working directory if path is invalid */
    if (access(project_root, F_OK) != 0) {
        if (!getcwd(project_root, sizeof(project_root))) {
            strcpy(project_root, ".");
        }
    }
}

static void get_local_ip(char *ip, size_t size) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        strncpy(ip, "127.0.0.1", size - 1);
        return;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    addr.sin_port = htons(80);
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    getsockname(sock, (struct sockaddr*)&local, &len);
    strncpy(ip, inet_ntoa(local.sin_addr), size - 1);
    close(sock);
}

static int get_subnet_port(const char *subnet) {
    char config_path[MAX_PATH];
    snprintf(config_path, sizeof(config_path), 
             "%s/projects/p2p-net/pieces/network/subnets/%s.txt", project_root, subnet);
    FILE *f = fopen(config_path, "r");
    if (!f) return 8000;
    int port = 8000;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "port=", 5) == 0) {
            port = atoi(line + 5);
        }
    }
    fclose(f);
    return port;
}

static void sync_focus() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_index.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        int active_idx = 0;
        if (fscanf(f, "%d", &active_idx) == 1) {
            if (active_idx > 0) gui_focus_index = active_idx;
        }
        fclose(f);
    }
}

static int is_active_layout() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        fclose(f);
        return (strstr(line, "p2p-net.chtpm") != NULL);
    }
    fclose(f);
    return 0;
}

static void set_response(const char* msg) {
    strncpy(last_response, msg, sizeof(last_response) - 1);
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/p2p-net/manager/response.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%s", msg); fclose(f); }
}

static void write_state() {
    char selector[MAX_LINE];
    get_wallet_selector(selector, sizeof(selector));

    char local_ip[64];
    get_local_ip(local_ip, sizeof(local_ip));
    int local_port = get_subnet_port(current_subnet);

    char peer_list[MAX_VAR_VALUE] = "";
    
    /* Prepend local user (Self) */
    snprintf(peer_list, sizeof(peer_list), "Self@%s:%d[%s]", local_ip, local_port, current_subnet);

    /* Parse raw peers.txt */
    char peers_path[MAX_PATH];
    snprintf(peers_path, sizeof(peers_path), "%s/projects/p2p-net/pieces/network/peers.txt", project_root);
    FILE *pf = fopen(peers_path, "r");
    int neighbor_count = 0;
    if (pf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), pf)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char *line_ptr = strdup(line);
            char *wallet = strtok(line_ptr, "|");
            char *ip = strtok(NULL, "|");
            char *port = strtok(NULL, "|");
            char *sub = strtok(NULL, "|");
            if (wallet && ip && port && sub) {
                if (neighbor_count < 12) {
                    char entry[256];
                    snprintf(entry, sizeof(entry), "%s@%s:%s[%s]", wallet, ip, port, sub);
                    if (peer_list[0] != '\0') strncat(peer_list, " ", sizeof(peer_list) - strlen(peer_list) - 1);
                    strncat(peer_list, entry, sizeof(peer_list) - strlen(peer_list) - 1);
                }
                neighbor_count++;
            }
            free(line_ptr);
        }
        fclose(pf);
    }

    char subnet_list[MAX_LINE] = "";
    char subnets_path[MAX_PATH];
    snprintf(subnets_path, sizeof(subnets_path), "%s/projects/p2p-net/pieces/network/subnets", project_root);
    DIR *d = opendir(subnets_path);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char *dot = strrchr(ent->d_name, '.');
            if (!dot || strcmp(dot, ".txt") != 0) continue;
            char name[64]; snprintf(name, sizeof(name), "%.*s", (int)(dot - ent->d_name), ent->d_name);
            if (subnet_list[0] != '\0') strncat(subnet_list, " ", sizeof(subnet_list) - strlen(subnet_list) - 1);
            if (strcmp(name, current_subnet) == 0) {
                strncat(subnet_list, "[", sizeof(subnet_list) - strlen(subnet_list) - 1);
                strncat(subnet_list, name, sizeof(subnet_list) - strlen(subnet_list) - 1);
                strncat(subnet_list, "]", sizeof(subnet_list) - strlen(subnet_list) - 1);
            } else {
                strncat(subnet_list, name, sizeof(subnet_list) - strlen(subnet_list) - 1);
            }
        }
        closedir(d);
    }
    if (subnet_list[0] == '\0') strcpy(subnet_list, "[chat]");

    char inbox_list[MAX_LINE] = "No messages";
    char message_preview[MAX_LINE] = "No message selected";
    char inbox_path[MAX_PATH];
    snprintf(inbox_path, sizeof(inbox_path), "%s/projects/p2p-net/pieces/chat/inbox/%s/inbox_list.txt", project_root, current_wallet);
    char msg_id[64], msg_from[64], msg_subj[128];
    load_kv_value(inbox_path, "msg_0_id", msg_id, sizeof(msg_id));
    if (msg_id[0] != '\0') {
        load_kv_value(inbox_path, "msg_0_from", msg_from, sizeof(msg_from));
        load_kv_value(inbox_path, "msg_0_subject", msg_subj, sizeof(msg_subj));
        snprintf(inbox_list, sizeof(inbox_list), "%s from %s: %s", msg_id, msg_from, msg_subj);
    }

    char cur_msg_path[MAX_PATH];
    snprintf(cur_msg_path, sizeof(cur_msg_path), "%s/projects/p2p-net/pieces/chat/inbox/%s/current_message.txt", project_root, current_wallet);
    char body[512];
    load_kv_value(cur_msg_path, "subject", msg_subj, sizeof(msg_subj));
    load_kv_value(cur_msg_path, "body", body, sizeof(body));
    if (msg_subj[0] != '\0' || body[0] != '\0') {
        snprintf(message_preview, sizeof(message_preview), "%s: %s", msg_subj, body);
    }

    char status_path[MAX_PATH];
    snprintf(status_path, sizeof(status_path), "%s/projects/p2p-net/pieces/chat/inbox/%s/status.txt", project_root, current_wallet);
    char unread_str[32] = "0", total_str[32] = "0";
    load_kv_value(status_path, "unread_count", unread_str, sizeof(unread_str));
    load_kv_value(status_path, "total_messages", total_str, sizeof(total_str));

    /* Write to gui_state.txt */
    char gui_path[MAX_PATH];
    snprintf(gui_path, sizeof(gui_path), "%s/projects/p2p-net/manager/gui_state.txt", project_root);
    FILE *f = fopen(gui_path, "w");
    if (f) {
        fprintf(f, "project_id=p2p-net\n");
        fprintf(f, "active_gui_index=%d\n", gui_focus_index);
        fprintf(f, "wallet_address=%s\n", current_wallet);
        fprintf(f, "wallet_selector=%s\n", selector);
        fprintf(f, "current_subnet=%s\n", current_subnet);
        fprintf(f, "subnet_list=%s\n", subnet_list);
        fprintf(f, "unread_count=%s\n", unread_str);
        fprintf(f, "total_count=%s\n", total_str);
        fprintf(f, "inbox_list=%s\n", inbox_list);
        fprintf(f, "message_preview=%s\n", message_preview);
        fprintf(f, "peer_count=%d\n", neighbor_count);
        fprintf(f, "peer_list=%s\n", peer_list);
        fprintf(f, "p2p_response=%s\n", last_response);
        fclose(f);
    }

    /* Also write to legacy path for compatibility if needed */
    snprintf(gui_path, sizeof(gui_path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    f = fopen(gui_path, "w");
    if (f) {
        fprintf(f, "project_id=p2p-net\n");
        fprintf(f, "wallet_address=%s\n", current_wallet);
        fprintf(f, "p2p_response=%s\n", last_response);
        fclose(f);
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
    else if (key == '8') {
        /* Debug-Append: Create real new peer */
        snprintf(cmd, sizeof(cmd), 
                 "%s/projects/p2p-net/ops/+x/debug_peer_agent.+x %s %s %s &",
                 project_root, current_wallet, current_subnet, project_root);
        system(cmd);
        set_response("Forked debug peer agent...");
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
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/projects/p2p-net/history.txt", project_root);
    
    if (stat(hist_path, &st) == 0) {
        last_pos = st.st_size;
    }
    else {
        FILE *f = fopen(hist_path, "w");
        if (f) fclose(f);
        last_pos = 0;
    }
    
    while (!g_shutdown) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }

        sync_focus();

        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    int processed = 0;
                    while (fgets(line, sizeof(line), hf)) {
                        int key = atoi(line);
                        if (key > 0) {
                            process_key(key);
                            processed = 1;
                        }
                    }
                    if (processed) {
                        /* Trigger render pulse */
                        char pulse_path[MAX_PATH];
                        snprintf(pulse_path, sizeof(pulse_path), "%s/pieces/display/frame_changed.txt", project_root);
                        FILE *pf = fopen(pulse_path, "a");
                        if (pf) { fprintf(pf, "M\n"); fclose(pf); }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
            }
        }
        
        usleep(100000); // 100ms
    }
    
    return 0;
}
