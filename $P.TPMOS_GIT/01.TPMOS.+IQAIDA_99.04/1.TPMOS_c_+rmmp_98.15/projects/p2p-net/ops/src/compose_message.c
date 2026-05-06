/*
 * compose_message.c - P2P-NET Email-Style Message Op
 * Purpose: Create and send message to another wallet on same subnet
 * 
 * Args: from_wallet, to_wallet, subnet, subject, body
 * 
 * Email Protocol:
 *   - Creates message file in recipient's inbox
 *   - Archives copy in sender's sent folder
 *   - Updates unread count in recipient's status
 * 
 * GUI Integration:
 *   - Writes to chat/inbox/<wallet>/msg_XXX.txt
 *   - Writes to chat/sent/<wallet>/msg_XXX.txt
 *   - Updates chat/inbox/<wallet>/status.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

#define MAX_PATH PATH_MAX
#define MAX_LINE 1024
#define MAX_MSG_BODY 4096

/* Get current working directory */
static void get_base_path(char *base_path, size_t size) {
    if (getcwd(base_path, size) != NULL) {
        return;
    }
    strncpy(base_path, ".", size - 1);
    base_path[size - 1] = '\0';
}

/* Ensure directory exists */
static void ensure_dir(const char *path) {
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    system(cmd);
}

/* Get next message ID for a wallet */
static int get_next_msg_id(const char *base_path, const char *wallet, const char *folder) {
    char status_path[MAX_PATH];
    snprintf(status_path, sizeof(status_path), 
             "%s/projects/p2p-net/pieces/chat/%s/%s/status.txt", 
             base_path, folder, wallet);
    
    FILE *f = fopen(status_path, "r");
    if (!f) return 1;
    
    int count = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "total_messages=", 15) == 0) {
            count = atoi(line + 15);
        }
    }
    fclose(f);
    
    return count + 1;
}

/* Update status file */
static void update_status(const char *base_path, const char *wallet, 
                          const char *folder, int msg_count, int unread_delta) {
    char status_path[MAX_PATH];
    snprintf(status_path, sizeof(status_path), 
             "%s/projects/p2p-net/pieces/chat/%s/%s/status.txt", 
             base_path, folder, wallet);
    
    /* Read existing status */
    int total = msg_count;
    int unread = 0;
    char last_checked[64] = "";
    
    FILE *f = fopen(status_path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "unread_count=", 13) == 0) {
                unread = atoi(line + 13);
            } else if (strncmp(line, "total_messages=", 15) == 0) {
                total = atoi(line + 15);
            } else if (strncmp(line, "last_checked=", 13) == 0) {
                char *nl = strchr(line + 13, '\n');
                if (nl) *nl = '\0';
                strncpy(last_checked, line + 13, sizeof(last_checked) - 1);
            }
        }
        fclose(f);
    }
    
    /* Update counts */
    total++;
    if (strcmp(folder, "inbox") == 0) {
        unread += unread_delta;
    }
    
    /* Write status */
    f = fopen(status_path, "w");
    if (f) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        
        fprintf(f, "unread_count=%d\n", unread);
        fprintf(f, "total_messages=%d\n", total);
        if (strlen(last_checked) > 0) {
            fprintf(f, "last_checked=%s\n", last_checked);
        } else {
            fprintf(f, "last_checked=%s\n", timestamp);
        }
        fclose(f);
    }
}

/* Write message file */
static int write_message(const char *base_path, const char *wallet, 
                         const char *folder, int msg_id,
                         const char *from, const char *to, 
                         const char *subnet, const char *subject, 
                         const char *body) {
    char msg_path[MAX_PATH];
    snprintf(msg_path, sizeof(msg_path), 
             "%s/projects/p2p-net/pieces/chat/%s/%s/msg_%03d.txt", 
             base_path, folder, wallet, msg_id);
    
    FILE *f = fopen(msg_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create message file: %s\n", msg_path);
        return 1;
    }
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(f, "# P2P Message\n");
    fprintf(f, "msg_id=msg_%03d\n", msg_id);
    fprintf(f, "from=%s\n", from);
    fprintf(f, "to=%s\n", to);
    fprintf(f, "subnet=%s\n", subnet);
    fprintf(f, "timestamp=%s\n", timestamp);
    fprintf(f, "subject=%s\n", subject);
    fprintf(f, "read=0\n");
    fprintf(f, "body=%s\n", body);
    
    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    char base_path[MAX_PATH];
    
    /* Parse arguments: compose_message <from> <to> <subnet> <subject> <body> */
    if (argc < 6) {
        fprintf(stderr, "Usage: compose_message <from_wallet> <to_wallet> <subnet> <subject> <body>\n");
        fprintf(stderr, "Example: compose_message abc123 def456 chat \"Hello\" \"Test message\"\n");
        return 1;
    }
    
    const char *from_wallet = argv[1];
    const char *to_wallet = argv[2];
    const char *subnet = argv[3];
    const char *subject = argv[4];
    const char *body = argv[5];
    
    /* Get base path */
    get_base_path(base_path, sizeof(base_path));
    
    /* Create directories */
    char inbox_path[MAX_PATH], sent_path[MAX_PATH];
    snprintf(inbox_path, sizeof(inbox_path), 
             "%s/projects/p2p-net/pieces/chat/inbox/%s", base_path, to_wallet);
    snprintf(sent_path, sizeof(sent_path), 
             "%s/projects/p2p-net/pieces/chat/sent/%s", base_path, from_wallet);
    
    ensure_dir(inbox_path);
    ensure_dir(sent_path);
    
    /* Get message IDs */
    int inbox_id = get_next_msg_id(base_path, to_wallet, "inbox");
    int sent_id = get_next_msg_id(base_path, from_wallet, "sent");
    
    /* Write message to recipient's inbox */
    if (write_message(base_path, to_wallet, "inbox", inbox_id, 
                      from_wallet, to_wallet, subnet, subject, body) != 0) {
        return 1;
    }
    
    /* Write copy to sender's sent folder */
    if (write_message(base_path, from_wallet, "sent", sent_id, 
                      from_wallet, to_wallet, subnet, subject, body) != 0) {
        return 1;
    }
    
    /* Update status files */
    update_status(base_path, to_wallet, "inbox", inbox_id, 1);  /* +1 unread */
    update_status(base_path, from_wallet, "sent", sent_id, 0);  /* +0 unread */
    
    /* Output confirmation */
    printf("Message sent successfully!\n");
    printf("  From: %s\n", from_wallet);
    printf("  To: %s\n", to_wallet);
    printf("  Subnet: %s\n", subnet);
    printf("  Subject: %s\n", subject);
    printf("  Message ID: msg_%03d\n", inbox_id);
    
    return 0;
}
