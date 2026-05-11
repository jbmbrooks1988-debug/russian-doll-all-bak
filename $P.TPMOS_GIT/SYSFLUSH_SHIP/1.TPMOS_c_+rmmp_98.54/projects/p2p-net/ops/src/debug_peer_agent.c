/*
 * debug_peer_agent.c - P2P-NET Debug Peer Simulator
 * Purpose: Act as a virtual peer that joins the network and sends messages
 * 
 * Usage: debug_peer_agent <target_wallet> <subnet> <project_root>
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

static volatile sig_atomic_t g_running = 1;
char my_wallet[64];
char target_wallet[64];
char subnet[32];
char project_root[MAX_PATH];
int my_port = 0;

void handle_sig(int sig) {
    (void)sig;
    g_running = 0;
}

int find_available_port() {
    for (int port = 8005; port < 8020; port++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(sock);
            return port;
        }
        close(sock);
    }
    return 0;
}

void register_peer() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/p2p-net/pieces/network/peers.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s|127.0.0.1|%d|%s|%ld|active\n", my_wallet, my_port, subnet, time(NULL));
        fclose(f);
    }
}

void unregister_peer() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/p2p-net/pieces/network/peers.txt", project_root);
    char tmp_path[MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    
    FILE *f = fopen(path, "r");
    FILE *t = fopen(tmp_path, "w");
    if (f && t) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, my_wallet) == NULL) {
                fputs(line, t);
            }
        }
    }
    if (f) fclose(f);
    if (t) fclose(t);
    rename(tmp_path, path);
}

void send_debug_message(const char* subject, const char* body) {
    char cmd[MAX_LINE * 4];
    snprintf(cmd, sizeof(cmd), 
             "%s/projects/p2p-net/ops/+x/compose_message.+x %s %s %s \"%s\" \"%s\" > /dev/null 2>&1",
             project_root, my_wallet, target_wallet, subnet, subject, body);
    system(cmd);
}

int main(int argc, char *argv[]) {
    if (argc < 4) return 1;
    
    strncpy(target_wallet, argv[1], 63);
    strncpy(subnet, argv[2], 31);
    strncpy(project_root, argv[3], MAX_PATH - 1);
    
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);
    
    srand(time(NULL) ^ getpid());
    snprintf(my_wallet, sizeof(my_wallet), "dbg_%04x", rand() % 10000);
    
    my_port = find_available_port();
    if (my_port == 0) return 1;
    
    register_peer();
    
    char body[MAX_LINE];
    snprintf(body, sizeof(body), "Debug peer %s connected on port %d.", my_wallet, my_port);
    send_debug_message("Peer Connected", body);
    
    int counter = 0;
    while (g_running) {
        sleep(10);
        if (!g_running) break;
        
        counter++;
        time_t now = time(NULL);
        char *ts = ctime(&now);
        if (ts) ts[strlen(ts)-1] = '\0';
        
        snprintf(body, sizeof(body), "Heartbeat #%d from %s at %s", counter, my_wallet, ts ? ts : "unknown time");
        send_debug_message("Peer Heartbeat", body);
    }
    
    unregister_peer();
    return 0;
}
