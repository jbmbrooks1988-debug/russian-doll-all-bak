#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "users_processed_hashes.txt"
#define LOG_FILE "users_log.txt"
#define USERS_DIR "users"
#define MAX_LINE 4096
#define MAX_HASHES 1000
#define MAX_STATE_SIZE 4096
#define MAX_ITEMS 5

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

void ensure_users_dir() {
    struct stat st = {0};
    if (stat(USERS_DIR, &st) == -1) {
        if (mkdir(USERS_DIR, 0700) == -1) {
            char err[256];
            snprintf(err, sizeof(err), "Failed to create %s: %s", USERS_DIR, strerror(errno));
            log_message(err);
        } else {
            log_message("Created users directory");
        }
    }
}

void update_user_state(const char* user_id, int user_cmd_count, int item_quantities[]) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s.state.txt", USERS_DIR, user_id);
    FILE* fp = fopen(filepath, "w");
    if (fp) {
        fprintf(fp, "user_command_count=%d\nlast_login=%ld\n", user_cmd_count, time(NULL));
        for (int i = 0; i < MAX_ITEMS; i++) {
            char* item_name = i == 0 ? "Sword" : i == 1 ? "Shield" : i == 2 ? "Potion" : i == 3 ? "Armor" : "Gem";
            fprintf(fp, "%s_count=%d\n", item_name, item_quantities[i]);
        }
        fflush(fp);
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Updated state for user %s: points=%d", user_id, user_cmd_count);
        log_message(log);
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Failed to write state for user %s: %s", user_id, strerror(errno));
        log_message(err);
    }
}

void read_user_state(const char* user_id, char* state_content, size_t max_size, int* user_cmd_count, int item_quantities[]) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s.state.txt", USERS_DIR, user_id);
    FILE* fp = fopen(filepath, "r");
    if (fp) {
        size_t bytes_read = fread(state_content, 1, max_size - 1, fp);
        state_content[bytes_read] = '\0';
        fclose(fp);
        char* cmd_count_str = strstr(state_content, "user_command_count=");
        if (cmd_count_str) {
            sscanf(cmd_count_str, "user_command_count=%d", user_cmd_count);
        }
        for (int i = 0; i < MAX_ITEMS; i++) {
            char item_key[32];
            snprintf(item_key, sizeof(item_key), "%s_count=", i == 0 ? "Sword" : i == 1 ? "Shield" : i == 2 ? "Potion" : i == 3 ? "Armor" : "Gem");
            char* item_str = strstr(state_content, item_key);
            if (item_str) {
                sscanf(item_str, "%*[^=]=%d", &item_quantities[i]);
            } else {
                item_quantities[i] = 0;
            }
        }
        char log[256];
        snprintf(log, sizeof(log), "Read state for user %s: points=%d", user_id, *user_cmd_count);
        log_message(log);
    } else {
        *user_cmd_count = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            item_quantities[i] = 0;
        }
        snprintf(state_content, max_size, "user_command_count=0\nlast_login=%ld\n", time(NULL));
        update_user_state(user_id, 0, item_quantities);
        char log[256];
        snprintf(log, sizeof(log), "Created new state for user %s", user_id);
        log_message(log);
    }
}

int main() {
    log_message("Starting users.+x");
    srand(time(NULL));
    ensure_users_dir();
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

            char user_id[32] = {0};
            char* user_id_start = strstr(content, "<@");
            if (user_id_start) {
                user_id_start += 2;
                char* user_id_end = strchr(user_id_start, '>');
                if (user_id_end) {
                    int user_id_len = user_id_end - user_id_start;
                    if (user_id_len < sizeof(user_id)) {
                        strncpy(user_id, user_id_start, user_id_len);
                        user_id[user_id_len] = '\0';
                    }
                }
            }

            if (strncmp(content, "!user", 5) == 0) {
                if (strlen(user_id) == 0) {
                    snprintf(log, sizeof(log), "No valid user ID found in content: %s", content);
                    log_message(log);
                    FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                    if (queue_fp) {
                        fprintf(queue_fp, "%s|Error: Invalid user ID format. Please use !user @username\n", channel_id);
                        fclose(queue_fp);
                        snprintf(log, sizeof(log), "Wrote to %s: Error response for invalid user ID", SEND_QUEUE_FILE);
                        log_message(log);
                    } else {
                        snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                        log_message(log);
                    }
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
                    continue;
                }
                char state_content[MAX_STATE_SIZE] = {0};
                int user_cmd_count = 0;
                int item_quantities[MAX_ITEMS] = {0};
                read_user_state(user_id, state_content, MAX_STATE_SIZE, &user_cmd_count, item_quantities);
                int r = rand() % 100;
                int delta;
                if (r < 70) {
                    delta = 10; // 70% chance for 10 points
                } else if (r < 85) {
                    delta = 30; // 15% chance for 30 points
                } else if (r < 90) {
                    delta = 50; // 5% chance for 50 points
                } else if (r < 91) {
                    delta = 1000; // 1% chance for 1000 points
                } else {
                    delta = -((rand() % 25) + 1); // 9% chance for -1 to -25 points
                }
                user_cmd_count += delta;
                update_user_state(user_id, user_cmd_count, item_quantities);
                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    char response[256] = {0};
                    snprintf(response, sizeof(response), "User %s: %d points\\nSword: %d\\nShield: %d\\nPotion: %d\\nArmor: %d\\nGem: %d", 
                             user_id, user_cmd_count, item_quantities[0], item_quantities[1], item_quantities[2], item_quantities[3], item_quantities[4]);
                    fprintf(queue_fp, "%s|%s", channel_id, response);
                    fclose(queue_fp);
                    snprintf(log, sizeof(log), "Wrote to %s: %s|%s", SEND_QUEUE_FILE, channel_id, response);
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
            }
        }
        if (feof(event_fp)) {
            clearerr(event_fp);
        }
        sleep(1);
    }
    fclose(event_fp);
    log_message("Exiting users.+x");
    return 0;
}
