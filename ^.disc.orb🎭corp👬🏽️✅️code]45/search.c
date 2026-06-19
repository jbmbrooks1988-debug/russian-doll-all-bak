#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "processed_hashes.txt"
#define LOG_FILE "search_log.txt"
#define SUMMARY_FILE "summary_result.txt"
#define MAX_LINE 4096
#define MAX_HASHES 1000
#define MAX_SUMMARY_SIZE 4096
#define SEARCH_TIMEOUT 10 // Timeout in seconds for search completion
#define POLL_INTERVAL 1 // Seconds between file checks

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) { 
        fprintf(f, "[%ld] %s\n", time(NULL), msg); 
        fflush(f);
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
            fflush(fp);
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
    *hash_count = 0;
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

int file_exists(const char* path) {
    FILE* fp = fopen(path, "r");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

int main() {
    log_message("Starting search.+x");
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

            if (strncmp(cmd, "!search ", 8) == 0) {
                char *keyword = cmd + 8;
                if (strlen(keyword) == 0) {
                    FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                    if (queue_fp) {
                        fprintf(queue_fp, "%s|Please provide a keyword to search.\n", channel_id);
                        fclose(queue_fp);
                        snprintf(log, sizeof(log), "Wrote to %s: %s|Please provide a keyword to search.", SEND_QUEUE_FILE, channel_id);
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
                    continue;
                }
                // Sanitize keyword to prevent command injection
                char safe_keyword[MAX_LINE];
                strncpy(safe_keyword, keyword, MAX_LINE - 1);
                safe_keyword[MAX_LINE - 1] = '\0';
                for (int i = 0; safe_keyword[i]; i++) {
                    if (safe_keyword[i] == ';' || safe_keyword[i] == '|' || safe_keyword[i] == '&' || safe_keyword[i] == '`') {
                        safe_keyword[i] = ' ';
                    }
                }
                // Clear any existing summary_result.txt to avoid stale data
                remove(SUMMARY_FILE);
                char search_cmd[512];
                snprintf(search_cmd, sizeof(search_cmd), "./+x/14.hybrid.pagerank.+x \"%s\" txt 1", safe_keyword);
                snprintf(log, sizeof(log), "Executing search: %s", search_cmd);
                log_message(log);
                system(search_cmd); // Run search in foreground to ensure completion
                // Wait for summary_result.txt to appear
                int elapsed = 0;
                while (!file_exists(SUMMARY_FILE) && elapsed < SEARCH_TIMEOUT) {
                    sleep(POLL_INTERVAL);
                    elapsed += POLL_INTERVAL;
                }
                // Read summary_result.txt
                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (!queue_fp) {
                    snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                    log_message(log);
                    continue;
                }
                if (elapsed >= SEARCH_TIMEOUT) {
                    fprintf(queue_fp, "%s|Search timed out for '%s' after %d seconds.\n", channel_id, safe_keyword, SEARCH_TIMEOUT);
                    snprintf(log, sizeof(log), "Search timed out for '%s' after %d seconds", safe_keyword, SEARCH_TIMEOUT);
                    log_message(log);
                } else {
                    FILE* summary_fp = fopen(SUMMARY_FILE, "r");
                    if (summary_fp) {
                        char summary_content[MAX_SUMMARY_SIZE] = {0};
                        size_t bytes_read = fread(summary_content, 1, MAX_SUMMARY_SIZE - 1, summary_fp);
                        summary_content[bytes_read] = '\0';
                        fclose(summary_fp);
                        // Replace newlines with spaces for Discord
                        for (size_t i = 0; i < bytes_read; i++) {
                            if (summary_content[i] == '\n') {
                                summary_content[i] = ' ';
                            }
                        }
                        fprintf(queue_fp, "%s|Search result for '%s': %s\n", channel_id, safe_keyword, summary_content);
                        snprintf(log, sizeof(log), "Wrote to %s: %s|Search result for '%s': %s", SEND_QUEUE_FILE, channel_id, safe_keyword, summary_content);
                        log_message(log);
                    } else {
                        fprintf(queue_fp, "%s|Failed to read search summary for '%s'.\n", channel_id, safe_keyword);
                        snprintf(log, sizeof(log), "Failed to open %s: %s", SUMMARY_FILE, strerror(errno));
                        log_message(log);
                    }
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
            }
        }
        if (feof(event_fp)) {
            clearerr(event_fp);
        }
        sleep(1);
    }
    fclose(event_fp);
    log_message("Exiting search.+x");
    return 0;
}
