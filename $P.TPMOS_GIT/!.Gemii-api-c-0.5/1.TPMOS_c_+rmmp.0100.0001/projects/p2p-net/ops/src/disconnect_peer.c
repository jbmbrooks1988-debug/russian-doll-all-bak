/*
 * disconnect_peer.c - P2P-NET Disconnect from Peer Op
 * Purpose: Gracefully disconnect from peer
 * 
 * Args: peer_wallet, peer_ip, peer_port
 * 
 * Network Protocol:
 *   - Sends GOODBYE message
 *   - Closes TCP connection
 *   - Updates peers.txt with status=disconnected
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
    
    if (argc < 4) {
        fprintf(stderr, "Usage: disconnect_peer <wallet> <ip> <port>\n");
        return 1;
    }
    
    const char *wallet = argv[1];
    const char *ip = argv[2];
    int port = atoi(argv[3]);
    
    get_base_path(base_path, sizeof(base_path));
    
    char peers_path[MAX_PATH];
    snprintf(peers_path, sizeof(peers_path), 
             "%s/projects/p2p-net/pieces/network/peers.txt", base_path);
    
    int peer_count = load_peers(peers_path, peers, MAX_PEERS);
    
    /* Try to send GOODBYE message */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip);
        addr.sin_port = htons(port);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            char goodbye[MAX_LINE];
            snprintf(goodbye, sizeof(goodbye), "GOODBYE:%s", wallet);
            send(sock, goodbye, strlen(goodbye), 0);
            printf("Sent GOODBYE to %s:%d\n", ip, port);
        }
        close(sock);
    }
    
    /* Update peer status */
    for (int i = 0; i < peer_count; i++) {
        if (strcmp(peers[i].wallet, wallet) == 0 && 
            strcmp(peers[i].ip, ip) == 0 && 
            peers[i].port == port) {
            strncpy(peers[i].status, "disconnected", sizeof(peers[i].status) - 1);
            peers[i].last_seen = time(NULL);
            printf("Disconnected from %s\n", wallet);
            break;
        }
    }
    
    save_peers(peers_path, peers, peer_count);
    printf("Peer list updated\n");
    
    return 0;
}
