/*
 * broadcast_leave.c - P2P-NET Leave Subnet Gracefully
 * Purpose: Broadcast LEAVE message and disconnect from subnet
 * 
 * Args: wallet_address, subnet_name
 * 
 * Network Protocol:
 *   - Sends UDP broadcast: "LEAVE:<wallet>:<ip>:<port>:<subnet>"
 *   - Updates peers.txt with status=left
 *   - Removes self from active peer list
 * 
 * GUI Integration:
 *   - Updates network/peers.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PATH PATH_MAX
#define MAX_LINE 1024
#define MAX_PEERS 100

typedef struct {
    char wallet[64];
    char ip[64];
    int port;
    char subnet[32];
    long last_seen;
    char status[16];
} Peer;

static void get_base_path(char *base_path, size_t size) {
    if (getcwd(base_path, size) != NULL) return;
    strncpy(base_path, ".", size - 1);
    base_path[size - 1] = '\0';
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

static int get_subnet_port(const char *base_path, const char *subnet) {
    char config_path[MAX_PATH];
    snprintf(config_path, sizeof(config_path), 
             "%s/projects/p2p-net/pieces/network/subnets/%s.txt", base_path, subnet);
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

static int load_peers(const char *peers_path, Peer *peers, int max_peers) {
    FILE *f = fopen(peers_path, "r");
    if (!f) return 0;
    int count = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && count < max_peers) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *token = strtok(line, "|");
        if (!token) continue;
        strncpy(peers[count].wallet, token, sizeof(peers[count].wallet) - 1);
        token = strtok(NULL, "|");
        if (!token) continue;
        strncpy(peers[count].ip, token, sizeof(peers[count].ip) - 1);
        token = strtok(NULL, "|");
        if (!token) continue;
        peers[count].port = atoi(token);
        token = strtok(NULL, "|");
        if (!token) continue;
        strncpy(peers[count].subnet, token, sizeof(peers[count].subnet) - 1);
        token = strtok(NULL, "|");
        if (!token) continue;
        peers[count].last_seen = atol(token);
        token = strtok(NULL, "|\n");
        if (!token) continue;
        strncpy(peers[count].status, token, sizeof(peers[count].status) - 1);
        count++;
    }
    fclose(f);
    return count;
}

static void save_peers(const char *peers_path, Peer *peers, int count) {
    FILE *f = fopen(peers_path, "w");
    if (!f) return;
    fprintf(f, "# P2P-NET Peer List\n# Format: wallet_address|ip|port|subnet|last_seen|status\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s|%s|%d|%s|%ld|%s\n", 
                peers[i].wallet, peers[i].ip, peers[i].port, 
                peers[i].subnet, peers[i].last_seen, peers[i].status);
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    char base_path[MAX_PATH];
    Peer peers[MAX_PEERS];
    
    if (argc < 3) {
        fprintf(stderr, "Usage: broadcast_leave <wallet> <subnet>\n");
        return 1;
    }
    
    const char *wallet = argv[1];
    const char *subnet = argv[2];
    
    get_base_path(base_path, sizeof(base_path));
    
    int port = get_subnet_port(base_path, subnet);
    
    char local_ip[64];
    get_local_ip(local_ip, sizeof(local_ip));
    
    char peers_path[MAX_PATH];
    snprintf(peers_path, sizeof(peers_path), 
             "%s/projects/p2p-net/pieces/network/peers.txt", base_path);
    
    int peer_count = load_peers(peers_path, peers, MAX_PEERS);
    
    /* Create UDP broadcast socket */
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("Failed to create socket");
        return 1;
    }
    
    int broadcast_enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
    
    /* Send LEAVE broadcast */
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast_addr.sin_port = htons(port);
    
    char leave_msg[MAX_LINE];
    snprintf(leave_msg, sizeof(leave_msg), "LEAVE:%s:%s:%d:%s", 
             wallet, local_ip, port, subnet);
    
    sendto(sock, leave_msg, strlen(leave_msg), 0, 
           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    
    printf("Broadcasting LEAVE message...\n");
    printf("  Wallet: %s\n", wallet);
    printf("  Subnet: %s (port %d)\n", subnet, port);
    
    /* Update own peer status */
    for (int i = 0; i < peer_count; i++) {
        if (strcmp(peers[i].wallet, wallet) == 0 && 
            strcmp(peers[i].subnet, subnet) == 0) {
            strncpy(peers[i].status, "left", sizeof(peers[i].status) - 1);
            peers[i].last_seen = time(NULL);
            break;
        }
    }
    
    close(sock);
    
    save_peers(peers_path, peers, peer_count);
    
    printf("\nLeft subnet successfully!\n");
    
    return 0;
}
