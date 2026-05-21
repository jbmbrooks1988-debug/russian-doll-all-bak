/*
 * read_message.c - P2P-NET Email Message Read Op
 * Purpose: Mark message as read, update unread count
 * 
 * Args: wallet_address, msg_id
 * 
 * Email Protocol:
 *   - Updates msg_XXX.txt read=1
 *   - Decrements unread_count in status.txt
 *   - Returns message content for display
 * 
 * GUI Integration:
 *   - Outputs full message content for display
 *   - Updates status files
 *   - Writes to chat/inbox/<wallet>/current_message.txt (GUI reads this)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

#define MAX_PATH PATH_MAX
#define MAX_LINE 1024
#define MAX_BODY 4096

/* Get current working directory */
static void get_base_path(char *base_path, size_t size) {
    if (getcwd(base_path, size) != NULL) {
        return;
    }
    strncpy(base_path, ".", size - 1);
    base_path[size - 1] = '\0';
}

/* Read and display message */
static int read_and_display_message(const char *base_path, const char *wallet, const char *msg_id) {
    char msg_path[MAX_PATH];
    snprintf(msg_path, sizeof(msg_path), 
             "%s/projects/p2p-net/pieces/chat/inbox/%s/%s.txt", 
             base_path, wallet, msg_id);
    
    FILE *f = fopen(msg_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Message not found: %s\n", msg_path);
        return 1;
    }
    
    /* Parse message fields */
    char from[64] = "", to[64] = "", subnet[32] = "", timestamp[64] = "";
    char subject[256] = "", body[MAX_BODY] = "";
    int read_status = 0;
    
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
        
        if (strcmp(key, "from") == 0) strncpy(from, val, sizeof(from) - 1);
        else if (strcmp(key, "to") == 0) strncpy(to, val, sizeof(to) - 1);
        else if (strcmp(key, "subnet") == 0) strncpy(subnet, val, sizeof(subnet) - 1);
        else if (strcmp(key, "timestamp") == 0) strncpy(timestamp, val, sizeof(timestamp) - 1);
        else if (strcmp(key, "subject") == 0) strncpy(subject, val, sizeof(subject) - 1);
        else if (strcmp(key, "read") == 0) read_status = atoi(val);
        else if (strcmp(key, "body") == 0) strncpy(body, val, sizeof(body) - 1);
    }
    fclose(f);
    
    /* Display message */
    printf("═══════════════════════════════════════════════════════════\n");
    printf("MESSAGE: %s\n", subject);
    printf("═══════════════════════════════════════════════════════════\n");
    printf("From: %s\n", from);
    printf("To: %s\n", to);
    printf("Date: %s\n", timestamp);
    printf("Subnet: %s\n", subnet);
    printf("───────────────────────────────────────────────────────────\n");
    printf("%s\n", body);
    printf("═══════════════════════════════════════════════════════════\n");
    
    /* Write to current_message.txt for GUI */
    char gui_path[MAX_PATH];
    snprintf(gui_path, sizeof(gui_path), 
             "%s/projects/p2p-net/pieces/chat/inbox/%s/current_message.txt", 
             base_path, wallet);
    
    FILE *gui = fopen(gui_path, "w");
    if (gui) {
        fprintf(gui, "msg_id=%s\n", msg_id);
        fprintf(gui, "from=%s\n", from);
        fprintf(gui, "to=%s\n", to);
        fprintf(gui, "subject=%s\n", subject);
        fprintf(gui, "timestamp=%s\n", timestamp);
        fprintf(gui, "subnet=%s\n", subnet);
        fprintf(gui, "body=%s\n", body);
        fclose(gui);
    }
    
    return 0;
}

/* Mark message as read */
static int mark_as_read(const char *base_path, const char *wallet, const char *msg_id) {
    char msg_path[MAX_PATH];
    snprintf(msg_path, sizeof(msg_path), 
             "%s/projects/p2p-net/pieces/chat/inbox/%s/%s.txt", 
             base_path, wallet, msg_id);
    
    /* Read existing message */
    char lines[50][MAX_LINE];
    int line_count = 0;
    int found_read = 0;
    
    FILE *f = fopen(msg_path, "r");
    if (!f) return 1;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && line_count < 50) {
        if (strncmp(line, "read=", 5) == 0) {
            snprintf(lines[line_count++], MAX_LINE, "read=1\n");
            found_read = 1;
        } else {
            strncpy(lines[line_count++], line, MAX_LINE - 1);
        }
    }
    fclose(f);
    
    /* Write back */
    f = fopen(msg_path, "w");
    if (!f) return 1;
    
    for (int i = 0; i < line_count; i++) {
        fputs(lines[i], f);
    }
    fclose(f);
    
    return found_read ? 0 : 1;
}

/* Update unread count */
static void update_unread_count(const char *base_path, const char *wallet) {
    char status_path[MAX_PATH];
    snprintf(status_path, sizeof(status_path), 
             "%s/projects/p2p-net/pieces/chat/inbox/%s/status.txt", 
             base_path, wallet);
    
    FILE *f = fopen(status_path, "r");
    if (!f) return;
    
    int unread = 0;
    int total = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "unread_count=", 13) == 0) {
            unread = atoi(line + 13);
        } else if (strncmp(line, "total_messages=", 15) == 0) {
            total = atoi(line + 15);
        }
    }
    fclose(f);
    
    /* Decrement unread */
    if (unread > 0) unread--;
    
    /* Write back */
    f = fopen(status_path, "w");
    if (f) {
        fprintf(f, "unread_count=%d\n", unread);
        fprintf(f, "total_messages=%d\n", total);
        
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(f, "last_checked=%s\n", timestamp);
        
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    char base_path[MAX_PATH];
    
    /* Parse arguments: read_message <wallet> <msg_id> */
    if (argc < 3) {
        fprintf(stderr, "Usage: read_message <wallet_address> <msg_id>\n");
        fprintf(stderr, "Example: read_message abc123def456 msg_001\n");
        return 1;
    }
    
    const char *wallet = argv[1];
    const char *msg_id = argv[2];
    
    /* Get base path */
    get_base_path(base_path, sizeof(base_path));
    
    /* Read and display message */
    if (read_and_display_message(base_path, wallet, msg_id) != 0) {
        return 1;
    }
    
    /* Mark as read */
    if (mark_as_read(base_path, wallet, msg_id) != 0) {
        fprintf(stderr, "Warning: Could not mark message as read\n");
    }
    
    /* Update unread count */
    update_unread_count(base_path, wallet);
    
    printf("\n[Message marked as read]\n");
    
    return 0;
}
