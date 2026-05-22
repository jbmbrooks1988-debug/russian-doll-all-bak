/*
 * list_peers.c - P2P-NET Op 2
 * Purpose: List connected peers (reads from peers.txt, outputs formatted list)
 * 
 * Args: subnet_name (or "all")
 * 
 * GUI Integration:
 *   - Writes to pieces/network/peers.txt (GUI reads this)
 *   - Outputs formatted list for response display
 *   - Returns peer count for status display
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define MAX_PATH PATH_MAX
#define MAX_LINE 512
#define MAX_PEERS 100

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

/* Parse peers.txt file */
static int parse_peers(const char *peers_path, Peer *peers, int max_peers) {
    FILE *f = fopen(peers_path, "r");
    if (!f) {
        return 0; /* No peers yet */
    }
    
    int count = 0;
    char line[MAX_LINE];
    
    while (fgets(line, sizeof(line), f) && count < max_peers) {
        /* Skip header/comments */
        if (line[0] == '#' || line[0] == '\n') continue;
        
        /* Parse: wallet|ip|port|subnet|last_seen|status */
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

/* Format and print peer list */
static void print_peer_list(Peer *peers, int count, const char *subnet_filter) {
    int filtered_count = 0;
    
    printf("Connected Peers");
    if (strcmp(subnet_filter, "all") != 0) {
        printf(" (subnet: %s)", subnet_filter);
    }
    printf("\n");
    printf("─────────────────────────────────────────────────────────\n");
    
    for (int i = 0; i < count; i++) {
        /* Filter by subnet if specified */
        if (strcmp(subnet_filter, "all") != 0 && 
            strcmp(peers[i].subnet, subnet_filter) != 0) {
            continue;
        }
        
        printf("[ ] %d. %-15s (%s:%d) - %s\n", 
               i + 1,
               peers[i].wallet,
               peers[i].ip,
               peers[i].port,
               peers[i].subnet);
        filtered_count++;
    }
    
    printf("─────────────────────────────────────────────────────────\n");
    printf("Total: %d peer(s)\n", filtered_count);
}

/* Write peer list to state file for GUI */
static void write_peer_state(const char *base_path, Peer *peers, int count, const char *subnet_filter) {
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), 
             "%s/projects/p2p-net/pieces/network/peer_list.txt", base_path);
    
    FILE *f = fopen(state_path, "w");
    if (!f) return;
    
    fprintf(f, "peer_count=%d\n", count);
    fprintf(f, "subnet_filter=%s\n", subnet_filter);
    
    /* Write first 5 peers for quick display */
    int written = 0;
    for (int i = 0; i < count && written < 5; i++) {
        if (strcmp(subnet_filter, "all") != 0 && 
            strcmp(peers[i].subnet, subnet_filter) != 0) {
            continue;
        }
        
        fprintf(f, "peer_%d_wallet=%s\n", written, peers[i].wallet);
        fprintf(f, "peer_%d_ip=%s\n", written, peers[i].ip);
        fprintf(f, "peer_%d_port=%d\n", written, peers[i].port);
        fprintf(f, "peer_%d_subnet=%s\n", written, peers[i].subnet);
        written++;
    }
    
    fclose(f);
}

int main(int argc, char *argv[]) {
    char base_path[MAX_PATH];
    char peers_path[MAX_PATH];
    Peer peers[MAX_PEERS];
    
    /* Parse arguments: list_peers <subnet_name> */
    const char *subnet_name = "all";
    if (argc >= 2) {
        subnet_name = argv[1];
    }
    
    /* Get base path */
    get_base_path(base_path, sizeof(base_path));
    
    /* Build peers.txt path */
    snprintf(peers_path, sizeof(peers_path), 
             "%s/projects/p2p-net/pieces/network/peers.txt", base_path);
    
    /* Parse peers */
    int count = parse_peers(peers_path, peers, MAX_PEERS);
    
    /* Print formatted list */
    print_peer_list(peers, count, subnet_name);
    
    /* Write state for GUI */
    write_peer_state(base_path, peers, count, subnet_name);
    
    return 0;
}
