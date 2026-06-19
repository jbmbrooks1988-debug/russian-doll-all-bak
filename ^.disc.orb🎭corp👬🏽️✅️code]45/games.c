#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "processed_hashes.txt"
#define LOG_FILE "games_log.txt"
#define HELP_FILE "sos.txt"
#define MAX_LINE 4096
#define MAX_HASHES 1000
#define MAX_HELP_SIZE 2000

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) { 
        fprintf(f, "[%ld] %s\n", time(NULL), msg); 
        fflush(f); // Ensure immediate write
        fclose(f); 
    }
    printf("[%ld] %s\n", time(NULL), msg);
}

unsigned int simple_hash(const char* str) {
    unsigned int hash = 0;
    for (int i = 0; str[i]; i++) {
        hash = hash * 31 + str[i];
    }
    return hash % 1000000;
}

int is_hash_processed(const char* hash, char hashes[][7], int* hash_count) {
    for (int i = 0; i < *hash_count; i++) {
        if (strcmp(hashes[i], hash) == 0) {
            return 1;
        }
    }
    return 0;
}

void save_hash(const char* hash, char hashes[][7], int* hash_count) {
    if (*hash_count < MAX_HASHES) {
        strcpy(hashes[*hash_count], hash);
        (*hash_count)++;
        FILE* fp = fopen(PROCESSED_HASHES_FILE, "a");
        if (fp) {
            fprintf(fp, "%s\n", hash);
            fflush(fp); // Ensure immediate write
            fclose(fp);
        } else {
            char err[256];
            snprintf(err, sizeof(err), "Failed to open %s: %s", PROCESSED_HASHES_FILE, strerror(errno));
            log_message(err);
        }
    } else {
        log_message("Max hashes reached, cannot save new hash");
    }
}

void load_processed_hashes(char hashes[][7], int* hash_count) {
    *hash_count = 0; // Reset to avoid duplicates
    FILE* fp = fopen(PROCESSED_HASHES_FILE, "r");
    if (fp) {
        char line[8];
        while (fgets(line, sizeof(line), fp) && *hash_count < MAX_HASHES) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0) {
                strcpy(hashes[*hash_count], line);
                (*hash_count)++;
            }
        }
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Loaded %d hashes from %s", *hash_count, PROCESSED_HASHES_FILE);
        log_message(log);
    } else {
        char log[256];
        snprintf(log, sizeof(log), "Failed to open %s for reading: %s", PROCESSED_HASHES_FILE, strerror(errno));
        log_message(log);
    }
}

int main() {
    log_message("Starting games.+x");
    char hashes[MAX_HASHES][7] = {0};
    int hash_count = 0;
    load_processed_hashes(hashes, &hash_count);
    FILE* event_fp = fopen(EVENTS_FILE, "r");
    if (!event_fp) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open %s: %s", EVENTS_FILE, strerror(errno));
        log_message(err);
        exit(1);
    }
    log_message("Opened events.txt successfully");
    long last_pos = 0;
    while (1) {
        fseek(event_fp, last_pos, SEEK_SET);
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), event_fp)) {
            last_pos = ftell(event_fp);
            line[strcspn(line, "\n")] = 0;
            char log[256];
            snprintf(log, sizeof(log), "Read line: %s", line);
            log_message(log);
            char channel_id[32] = {0};
            char timestamp[32] = {0};
            char hash[7] = {0};
            char content[MAX_LINE] = {0};
            if (sscanf(line, "%[^|]|%[^|]|%[^|]|%[^\n]", channel_id, timestamp, hash, content) != 4) {
                snprintf(log, sizeof(log), "Invalid line format: %s", line);
                log_message(log);
                continue;
            }
            snprintf(log, sizeof(log), "Parsed: channel=%s, timestamp=%s, hash=%s, content=%s", channel_id, timestamp, hash, content);
            log_message(log);
            if (is_hash_processed(hash, hashes, &hash_count)) {
                snprintf(log, sizeof(log), "Hash %s already processed, skipping", hash);
                log_message(log);
                continue;
            }
            save_hash(hash, hashes, &hash_count);
            snprintf(log, sizeof(log), "Saved hash: %s", hash);
            log_message(log);

            // Check for user ID prefix and extract command
            char* cmd_start = strstr(content, "> ");
            char* cmd = cmd_start ? cmd_start + 2 : content;
            char user_id[32] = {0};
            if (cmd_start && strncmp(content, "<@", 2) == 0) {
                char* user_id_start = content + 2;
                char* user_id_end = strchr(user_id_start, '>');
                if (user_id_end) {
                    int user_id_len = user_id_end - user_id_start;
                    if (user_id_len < sizeof(user_id)) {
                        strncpy(user_id, user_id_start, user_id_len);
                        user_id[user_id_len] = '\0';
                    }
                }
                snprintf(log, sizeof(log), "Extracted user_id: %s, command: %s", user_id[0] ? user_id : "none", cmd);
                log_message(log);
            } else {
                snprintf(log, sizeof(log), "No user ID prefix, using raw command: %s", cmd);
                log_message(log);
            }

            if (strcmp(cmd, "!hi") == 0) {
                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    fprintf(queue_fp, "%s|Hello from the game module, %s!\n", channel_id, user_id[0] ? user_id : "user");
                    fclose(queue_fp);
                    snprintf(log, sizeof(log), "Wrote to %s: %s|Hello from the game module, %s!", SEND_QUEUE_FILE, channel_id, user_id[0] ? user_id : "user");
                    log_message(log);
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "./+x/send.+x");
                    log_message("Executing: ./+x/send.+x");
                    int ret = system(cmd);
                    if (ret != 0) {
                        snprintf(log, sizeof(log), "send.+x failed with return code %d", ret);
                        log_message(log);
                    } else {
                        log_message("send.+x executed successfully");
                    }
                } else {
                    snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                    log_message(log);
                }
            } else if (strcmp(cmd, "!test") == 0) {
                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    fprintf(queue_fp, "%s|passed\n", channel_id);
                    fclose(queue_fp);
                    snprintf(log, sizeof(log), "Wrote to %s: %s|passed", SEND_QUEUE_FILE, channel_id);
                    log_message(log);
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "./+x/send.+x");
                    log_message("Executing: ./+x/send.+x");
                    int ret = system(cmd);
                    if (ret != 0) {
                        snprintf(log, sizeof(log), "send.+x failed with return code %d", ret);
                        log_message(log);
                    } else {
                        log_message("send.+x executed successfully");
                    }
                } else {
                    snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                    log_message(log);
                }
            } else if (strcmp(cmd, "!sos") == 0) {
                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    FILE* help_fp = fopen(HELP_FILE, "r");
                    if (help_fp) {
                        char help_content[MAX_HELP_SIZE] = {0};
                        size_t bytes_read = fread(help_content, 1, MAX_HELP_SIZE - 1, help_fp);
                        help_content[bytes_read] = '\0';
                        fclose(help_fp);
                        // Replace newlines with spaces
                        for (size_t i = 0; i < bytes_read; i++) {
                            if (help_content[i] == '\n') {
                                help_content[i] = ' ';
                            }
                        }
                        fprintf(queue_fp, "%s|%s\n", channel_id, help_content);
                        snprintf(log, sizeof(log), "Wrote to %s: %s|%s", SEND_QUEUE_FILE, channel_id, help_content);
                        log_message(log);
                    } else {
                        fprintf(queue_fp, "%s|Help file not found.\n", channel_id);
                        snprintf(log, sizeof(log), "Failed to open %s: %s", HELP_FILE, strerror(errno));
                        log_message(log);
                    }
                    fclose(queue_fp);
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "./+x/send.+x");
                    log_message("Executing: ./+x/send.+x");
                    int ret = system(cmd);
                    if (ret != 0) {
                        snprintf(log, sizeof(log), "send.+x failed with return code %d", ret);
                        log_message(log);
                    } else {
                        log_message("send.+x executed successfully");
                    }
                } else {
                    snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                    log_message(log);
                }
            }
        }
        if (feof(event_fp)) {
            clearerr(event_fp);
        }
        sleep(1);
    }
    fclose(event_fp);
    log_message("Exiting games.+x");
    return 0;
}