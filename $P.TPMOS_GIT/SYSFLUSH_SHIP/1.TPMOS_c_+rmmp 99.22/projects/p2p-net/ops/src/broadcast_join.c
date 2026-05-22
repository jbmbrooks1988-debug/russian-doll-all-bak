/*
 * broadcast_join.c - P2P-NET Join Subnet via UDP Broadcast
 * Purpose: Join subnet and discover peers via UDP broadcast
 * 
 * Args: wallet_address, subnet_name
 * 
 * Network Protocol:
 *   - Sends UDP broadcast to 255.255.255.255:<port>
 *   - Message: "JOIN:<wallet>:<ip>:<port>:<subnet>"
 *   - Listens for responses from other peers
 *   - Adds discovered peers to peers.txt
 * 
 * GUI Integration:
 *   - Updates network/peers.txt with discovered peers
 *   - Updates network/peer_list.txt for GUI display
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
#include <errno.h>
#include <sys/select.h>

#define MAX_PATH PATH_MAX
#define MAX_LINE 1024
#define MAX_PEERS 100
#define BROADCAST_PORT 8000
#define JOIN_TIMEOUT 3  /* seconds */

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

/* Get local IP address */
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

/* Get port for subnet */
static int get_subnet_port(const char *base_path, const char *subnet) {
    char config_path[MAX_PATH];
    snprintf(config_path, sizeof(config_path), 
             "%s/projects/p2p-net/pieces/network/subnets/%s.txt", 
             base_path, subnet);
    
    FILE *f = fopen(config_path, "r");
    if (!f) return BROADCAST_PORT;
    
    int port = BROADCAST_PORT;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "port=", 5) == 0) {
            port = atoi(line + 5);
        }
    }
    fclose(f);
    
    return port;
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
                              const char *ip, int port, const char *subnet) {
    /* Check if peer already exists */
    for (int i = 0; i < count; i++) {
        if (strcmp(peers[i].wallet, wallet) == 0 && 
            strcmp(peers[i].subnet, subnet) == 0) {
            /* Update existing peer */
            strncpy(peers[i].ip, ip, sizeof(peers[i].ip) - 1);
            peers[i].port = port;
            peers[i].last_seen = time(NULL);
            strncpy(peers[i].status, "active", sizeof(peers[i].status) - 1);
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
        strncpy(peers[count].status, "active", sizeof(peers[count].status) - 1);
        return count + 1;
    }
    
    return count;
}

int main(int argc, char *argv[]) {
    char base_path[MAX_PATH];
    Peer peers[MAX_PEERS];
    
    /* Parse arguments: broadcast_join <wallet> <subnet> */
    if (argc < 3) {
        fprintf(stderr, "Usage: broadcast_join <wallet_address> <subnet_name>\n");
        fprintf(stderr, "Example: broadcast_join abc123def456 chat\n");
        return 1;
    }
    
    const char *wallet = argv[1];
    const char *subnet = argv[2];
    
    /* Get base path */
    get_base_path(base_path, sizeof(base_path));
    
    /* Get subnet port */
    int port = get_subnet_port(base_path, subnet);
    
    /* Get local IP */
    char local_ip[64];
    get_local_ip(local_ip, sizeof(local_ip));
    
    /* Load existing peers */
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
    
    /* Enable broadcast */
    int broadcast_enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
    
    /* Set receive timeout */
    struct timeval timeout;
    timeout.tv_sec = JOIN_TIMEOUT;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    /* Send JOIN broadcast */
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast_addr.sin_port = htons(port);
    
    char join_msg[MAX_LINE];
    snprintf(join_msg, sizeof(join_msg), "JOIN:%s:%s:%d:%s", 
             wallet, local_ip, port, subnet);
    
    sendto(sock, join_msg, strlen(join_msg), 0, 
           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    
    printf("Broadcasting JOIN message...\n");
    printf("  Wallet: %s\n", wallet);
    printf("  Subnet: %s (port %d)\n", subnet, port);
    printf("  IP: %s\n", local_ip);
    
    /* Listen for responses */
    int new_peers = 0;
    char buffer[MAX_LINE];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    
    while (1) {
        int bytes = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, 
                            (struct sockaddr*)&sender_addr, &sender_len);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  /* Timeout */
            }
            continue;
        }
        
        buffer[bytes] = '\0';
        
        /* Parse response: PEER:<wallet>:<ip>:<port>:<subnet> */
        if (strncmp(buffer, "PEER:", 5) == 0) {
            char *token = strtok(buffer + 5, ":");
            if (!token) continue;
            char *peer_wallet = token;
            
            token = strtok(NULL, ":");
            if (!token) continue;
            char *peer_ip = token;
            
            token = strtok(NULL, ":");
            if (!token) continue;
            int peer_port = atoi(token);
            
            token = strtok(NULL, ":");
            if (!token) continue;
            char *peer_subnet = token;
            
            /* Add peer */
            int old_count = peer_count;
            peer_count = add_or_update_peer(peers, peer_count, 
                                           peer_wallet, peer_ip, peer_port, peer_subnet);
            if (peer_count > old_count) {
                new_peers++;
                printf("  Discovered: %s (%s:%d) [%s]\n", 
                       peer_wallet, peer_ip, peer_port, peer_subnet);
            }
        }
    }
    
    close(sock);
    
    /* Save updated peer list */
    save_peers(peers_path, peers, peer_count);
    
    printf("\nJoin complete!\n");
    printf("  Total peers: %d\n", peer_count);
    printf("  New peers discovered: %d\n", new_peers);
    
    return 0;
}
