/*
 * check_inbox.c - P2P-NET Email Inbox Check Op
 * Purpose: List all messages in wallet's inbox
 * 
 * Args: wallet_address, subnet (optional, "all" for all subnets)
 * 
 * GUI Integration:
 *   - Writes to chat/inbox/<wallet>/inbox_list.txt (GUI reads this)
 *   - Outputs formatted list for response display
 *   - Returns unread count and total count
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>

#define MAX_PATH PATH_MAX
#define MAX_LINE 1024
#define MAX_MESSAGES 100

/* Message info structure */
typedef struct {
    char msg_id[32];
    char from[64];
    char to[64];
    char subnet[32];
    char timestamp[64];
    char subject[256];
    int read;
} Message;

/* Get current working directory */
static void get_base_path(char *base_path, size_t size) {
    if (getcwd(base_path, size) != NULL) {
        return;
    }
    strncpy(base_path, ".", size - 1);
    base_path[size - 1] = '\0';
}

/* Parse a message file */
static int parse_message_file(const char *filepath, Message *msg) {
    FILE *f = fopen(filepath, "r");
    if (!f) return 0;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        
        /* Remove newline */
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        
        if (strcmp(key, "msg_id") == 0) strncpy(msg->msg_id, val, sizeof(msg->msg_id) - 1);
        else if (strcmp(key, "from") == 0) strncpy(msg->from, val, sizeof(msg->from) - 1);
        else if (strcmp(key, "to") == 0) strncpy(msg->to, val, sizeof(msg->to) - 1);
        else if (strcmp(key, "subnet") == 0) strncpy(msg->subnet, val, sizeof(msg->subnet) - 1);
        else if (strcmp(key, "timestamp") == 0) strncpy(msg->timestamp, val, sizeof(msg->timestamp) - 1);
        else if (strcmp(key, "subject") == 0) strncpy(msg->subject, val, sizeof(msg->subject) - 1);
        else if (strcmp(key, "read") == 0) msg->read = atoi(val);
    }
    
    fclose(f);
    return 1;
}

/* Format and print inbox */
static void print_inbox(Message *messages, int count, int unread_count, const char *wallet) {
    printf("INBOX for %s\n", wallet);
    printf("─────────────────────────────────────────────────────────\n");
    printf("Unread: %d / Total: %d\n", unread_count, count);
    printf("─────────────────────────────────────────────────────────\n");
    
    if (count == 0) {
        printf("(No messages)\n");
    } else {
        for (int i = 0; i < count; i++) {
            char status = messages[i].read ? ' ' : 'U';
            printf("[%c] %d. From: %-15s Subject: %-20s [%s]\n", 
                   status,
                   i + 1,
                   messages[i].from,
                   messages[i].subject,
                   messages[i].subnet);
        }
    }
    
    printf("─────────────────────────────────────────────────────────\n");
}

/* Write inbox list to state file for GUI */
static void write_inbox_state(const char *base_path, const char *wallet,
                              Message *messages, int count, int unread_count) {
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), 
             "%s/projects/p2p-net/pieces/chat/inbox/%s/inbox_list.txt", base_path, wallet);
    
    FILE *f = fopen(state_path, "w");
    if (!f) return;
    
    fprintf(f, "wallet=%s\n", wallet);
    fprintf(f, "unread_count=%d\n", unread_count);
    fprintf(f, "total_count=%d\n", count);
    
    /* Write first 10 messages for GUI display */
    int max_display = (count < 10) ? count : 10;
    for (int i = 0; i < max_display; i++) {
        fprintf(f, "msg_%d_id=%s\n", i, messages[i].msg_id);
        fprintf(f, "msg_%d_from=%s\n", i, messages[i].from);
        fprintf(f, "msg_%d_subject=%s\n", i, messages[i].subject);
        fprintf(f, "msg_%d_subnet=%s\n", i, messages[i].subnet);
        fprintf(f, "msg_%d_timestamp=%s\n", i, messages[i].timestamp);
        fprintf(f, "msg_%d_read=%d\n", i, messages[i].read);
    }
    
    fclose(f);
}

int main(int argc, char *argv[]) {
    char base_path[MAX_PATH];
    Message messages[MAX_MESSAGES];
    
    /* Parse arguments: check_inbox <wallet> [subnet] */
    if (argc < 2) {
        fprintf(stderr, "Usage: check_inbox <wallet_address> [subnet]\n");
        fprintf(stderr, "Example: check_inbox abc123def456 chat\n");
        return 1;
    }
    
    const char *wallet = argv[1];
    const char *subnet_filter = (argc >= 3) ? argv[2] : "all";
    
    /* Get base path */
    get_base_path(base_path, sizeof(base_path));
    
    /* Build inbox directory path */
    char inbox_dir[MAX_PATH];
    snprintf(inbox_dir, sizeof(inbox_dir), 
             "%s/projects/p2p-net/pieces/chat/inbox/%s", base_path, wallet);
    
    /* Read message files */
    int count = 0;
    int unread_count = 0;
    
    DIR *dir = opendir(inbox_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && count < MAX_MESSAGES) {
            /* Only process msg_XXX.txt files */
            if (strncmp(entry->d_name, "msg_", 4) != 0) continue;
            if (strstr(entry->d_name, ".txt") == NULL) continue;
            
            char filepath[MAX_PATH];
            snprintf(filepath, sizeof(filepath), "%s/%s", inbox_dir, entry->d_name);
            
            Message msg;
            memset(&msg, 0, sizeof(msg));
            
            if (parse_message_file(filepath, &msg)) {
                /* Filter by subnet if specified */
                if (strcmp(subnet_filter, "all") != 0 && 
                    strcmp(msg.subnet, subnet_filter) != 0) {
                    continue;
                }
                
                messages[count] = msg;
                if (!msg.read) unread_count++;
                count++;
            }
        }
        closedir(dir);
    }
    
    /* Print formatted inbox */
    print_inbox(messages, count, unread_count, wallet);
    
    /* Write state for GUI */
    write_inbox_state(base_path, wallet, messages, count, unread_count);
    
    return 0;
}
