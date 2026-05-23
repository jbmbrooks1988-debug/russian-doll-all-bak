/*
 * connect_peer.c - P2P-NET TCP Connection Op
 * Purpose: Establish TCP connection to specific peer
 * 
 * Args: peer_ip, peer_port, wallet_address, subnet
 * 
 * Network Protocol:
 *   - Creates TCP connection to peer
 *   - Sends handshake: "HELLO:<wallet>:<subnet>"
 *   - Waits for response: "WELCOME:<peer_wallet>:<subnet>"
 *   - Updates peers.txt with status=connected
 * 
 * GUI Integration:
 *   - Updates network/peers.txt
 *   - Updates network/peer_list.txt
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
#include <netdb.h>

#define MAX_PATH PATH_MAX
#define MAX_LINE 1024
#define MAX_PEERS 100
#define HANDSHAKE_TIMEOUT 5

/* Peer structure */
typedef struct {
    char wallet[64];
    char ip[64];
    int port;
    char subnet[32];
    long last_seen;
    char status[16];
} Peer;

/* Get current working directory */
static void get_base_path(char *base_path, size_t size) {
    if (getcwd(base_path, size) != NULL) {
        return;
    }
    strncpy(base_path, ".", size - 1);
    base_path[size - 1] = '\0';
}

/* Load existing peers */
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

/* Save peers */
static void save_peers(const char *peers_path, Peer *peers, int count) {
    FILE *f = fopen(peers_path, "w");
    if (!f) return;
    
    fprintf(f, "# P2P-NET Peer List\n");
    fprintf(f, "# Format: wallet_address|ip|port|subnet|last_seen|status\n");
    
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s|%s|%d|%s|%ld|%s\n", 
                peers[i].wallet, peers[i].ip, peers[i].port, 
                peers[i].subnet, peers[i].last_seen, peers[i].status);
    }
    fclose(f);
}

/* Add or update peer */
static int add_or_update_peer(Peer *peers, int count, const char *wallet, 
                              const char *ip, int port, const char *subnet,
                              const char *status) {
    /* Check if peer already exists */
    for (int i = 0; i < count; i++) {
        if (strcmp(peers[i].wallet, wallet) == 0 && 
            strcmp(peers[i].subnet, subnet) == 0) {
            strncpy(peers[i].ip, ip, sizeof(peers[i].ip) - 1);
            peers[i].port = port;
            peers[i].last_seen = time(NULL);
            strncpy(peers[i].status, status, sizeof(peers[i].status) - 1);
            return count;
        }
    }
    
    /* Add new peer */
    if (count < MAX_PEERS) {
        strncpy(peers[count].wallet, wallet, sizeof(peers[count].wallet) - 1);
        strncpy(peers[count].ip, ip, sizeof(peers[count].ip) - 1);
        peers[count].port = port;
        strncpy(peers[count].subnet, subnet, sizeof(peers[count].subnet) - 1);
        peers[count].last_seen = time(NULL);
        strncpy(peers[count].status, status, sizeof(peers[count].status) - 1);
        return count + 1;
    }
    
    return count;
}

int main(int argc, char *argv[]) {
    char base_path[MAX_PATH];
    Peer peers[MAX_PEERS];
    
    /* Parse arguments: connect_peer <ip> <port> <wallet> <subnet> */
    if (argc < 5) {
        fprintf(stderr, "Usage: connect_peer <peer_ip> <peer_port> <wallet> <subnet>\n");
        fprintf(stderr, "Example: connect_peer 127.0.0.1 8001 abc123 chat\n");
        return 1;
    }
    
    const char *peer_ip = argv[1];
    int peer_port = atoi(argv[2]);
    const char *wallet = argv[3];
    const char *subnet = argv[4];
    
    /* Get base path */
    get_base_path(base_path, sizeof(base_path));
    
    /* Load existing peers */
    char peers_path[MAX_PATH];
    snprintf(peers_path, sizeof(peers_path), 
             "%s/projects/p2p-net/pieces/network/peers.txt", base_path);
    
    int peer_count = load_peers(peers_path, peers, MAX_PEERS);
    
    /* Create TCP socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        return 1;
    }
    
    /* Set connection timeout */
    struct timeval timeout;
    timeout.tv_sec = HANDSHAKE_TIMEOUT;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    /* Connect to peer */
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip);
    peer_addr.sin_port = htons(peer_port);
    
    printf("Connecting to %s:%d...\n", peer_ip, peer_port);
    
    if (connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        
        /* Add peer with status=offline */
        peer_count = add_or_update_peer(peers, peer_count, wallet, 
                                       peer_ip, peer_port, subnet, "offline");
        save_peers(peers_path, peers, peer_count);
        return 1;
    }
    
    /* Send handshake */
    char handshake[MAX_LINE];
    snprintf(handshake, sizeof(handshake), "HELLO:%s:%s", wallet, subnet);
    send(sock, handshake, strlen(handshake), 0);
    
    printf("Handshake sent: %s\n", handshake);
    
    /* Wait for response */
    char response[MAX_LINE];
    int bytes = recv(sock, response, sizeof(response) - 1, 0);
    if (bytes > 0) {
        response[bytes] = '\0';
        printf("Response: %s\n", response);
        
        /* Parse response: WELCOME:<peer_wallet>:<subnet> */
        if (strncmp(response, "WELCOME:", 8) == 0) {
            char *peer_wallet = strtok(response + 8, ":");
            char *peer_subnet = strtok(NULL, ":");
            
            if (peer_wallet && peer_subnet) {
                printf("Connected to %s [%s]\n", peer_wallet, peer_subnet);
                
                /* Update peer status */
                peer_count = add_or_update_peer(peers, peer_count, peer_wallet, 
                                               peer_ip, peer_port, peer_subnet, "connected");
            }
        }
    } else {
        printf("No response received (peer may not be listening)\n");
        peer_count = add_or_update_peer(peers, peer_count, wallet, 
                                       peer_ip, peer_port, subnet, "pending");
    }
    
    close(sock);
    
    /* Save updated peer list */
    save_peers(peers_path, peers, peer_count);
    
    printf("\nConnection attempt complete!\n");
    printf("  Total peers: %d\n", peer_count);
    
    return 0;
}
