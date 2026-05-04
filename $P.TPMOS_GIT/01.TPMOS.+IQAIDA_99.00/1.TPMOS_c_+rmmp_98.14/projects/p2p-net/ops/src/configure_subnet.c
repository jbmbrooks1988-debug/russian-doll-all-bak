/*
 * configure_subnet.c - P2P-NET Op 1
 * Purpose: Configure subnet settings (creates/updates subnet config file)
 * 
 * Args: subnet_name, network_mode, connection_mode, port
 * 
 * Multi-Network Support: Each subnet has independent config file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#define MAX_PATH PATH_MAX
#define MAX_LINE 512

/* Get base path - use current working directory */
static void get_base_path(char *base_path, size_t size) {
    if (getcwd(base_path, size) != NULL) {
        return;
    }
    /* Fallback to current directory */
    strncpy(base_path, ".", size - 1);
    base_path[size - 1] = '\0';
}

/* Ensure directory exists */
static void ensure_dir(const char *path) {
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    system(cmd);
}

/* Write subnet config file */
static int write_subnet_config(const char *base_path, const char *subnet_name,
                                const char *network_mode, const char *connection_mode,
                                int port) {
    char config_path[MAX_PATH];
    char subnet_dir[MAX_PATH];
    
    /* Create subnets directory */
    snprintf(subnet_dir, sizeof(subnet_dir), "%s/projects/p2p-net/pieces/network/subnets", base_path);
    ensure_dir(subnet_dir);
    
    /* Write config file */
    snprintf(config_path, sizeof(config_path), "%s/%s.txt", subnet_dir, subnet_name);
    
    FILE *f = fopen(config_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create config file: %s\n", config_path);
        return 1;
    }
    
    fprintf(f, "subnet_name=%s\n", subnet_name);
    fprintf(f, "network_mode=%s\n", network_mode);
    fprintf(f, "connection_mode=%s\n", connection_mode);
    fprintf(f, "port=%d\n", port);
    fprintf(f, "max_peers=50\n");
    fprintf(f, "ping_interval=30\n");
    fprintf(f, "timeout=90\n");
    fprintf(f, "active=1\n");
    
    fclose(f);
    
    printf("Configured subnet '%s':\n", subnet_name);
    printf("  Mode: %s / %s\n", network_mode, connection_mode);
    printf("  Port: %d\n", port);
    printf("  Config: %s\n", config_path);
    
    return 0;
}

int main(int argc, char *argv[]) {
    char base_path[MAX_PATH];
    
    /* Parse arguments: configure_subnet <subnet_name> <network_mode> <connection_mode> <port> */
    if (argc < 5) {
        fprintf(stderr, "Usage: configure_subnet <subnet_name> <network_mode> <connection_mode> <port>\n");
        fprintf(stderr, "Example: configure_subnet chat broadcast continuous 8001\n");
        return 1;
    }
    
    const char *subnet_name = argv[1];
    const char *network_mode = argv[2];
    const char *connection_mode = argv[3];
    int port = atoi(argv[4]);
    
    /* Validate network_mode */
    if (strcmp(network_mode, "broadcast") != 0 &&
        strcmp(network_mode, "ring") != 0 &&
        strcmp(network_mode, "hybrid") != 0) {
        fprintf(stderr, "Error: network_mode must be 'broadcast', 'ring', or 'hybrid'\n");
        return 1;
    }
    
    /* Validate connection_mode */
    if (strcmp(connection_mode, "continuous") != 0 &&
        strcmp(connection_mode, "on_demand") != 0) {
        fprintf(stderr, "Error: connection_mode must be 'continuous' or 'on_demand'\n");
        return 1;
    }
    
    /* Validate port */
    if (port < 8000 || port > 8010) {
        fprintf(stderr, "Error: port must be between 8000 and 8010\n");
        return 1;
    }
    
    /* Get base path */
    get_base_path(base_path, sizeof(base_path));
    
    /* Write config */
    return write_subnet_config(base_path, subnet_name, network_mode, connection_mode, port);
}
