#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "chem_processed_hashes.txt"
#define LOG_FILE "chem_log.txt"
#define USERS_DIR "data"
#define MAX_LINE 4096
#define MAX_HASHES 1000
#define MAX_CHEMS 1000
#define MAX_LOW_LEVEL_CHEMS 3 // Up_quark, Down_quark, Electron

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
            log_message("Created data directory");
        }
    }
}

void update_user_chems(const char* user_id, const char* chem_name) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s/chems.txt", USERS_DIR, user_id);
    
    // Ensure user directory exists
    struct stat st = {0};
    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s/%s", USERS_DIR, user_id);
    if (stat(user_dir, &st) == -1) {
        if (mkdir(user_dir, 0700) == -1) {
            char err[256];
            snprintf(err, sizeof(err), "Failed to create %s: %s", user_dir, strerror(errno));
            log_message(err);
            return;
        }
    }

    FILE* fp = fopen(filepath, "a");
    if (fp) {
        fprintf(fp, "%s\n", chem_name);
        fflush(fp);
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Added %s to %s for user %s", chem_name, filepath, user_id);
        log_message(log);
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Failed to write chems for user %s: %s", user_id, strerror(errno));
        log_message(err);
    }
}

void read_user_chems(const char* user_id, char chems[][20], int* chem_count) {
    *chem_count = 0;
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s/chems.txt", USERS_DIR, user_id);
    FILE* fp = fopen(filepath, "r");
    if (fp) {
        char line[20];
        while (fgets(line, sizeof(line), fp) && *chem_count < MAX_CHEMS) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0) {
                strcpy(chems[*chem_count], line);
                (*chem_count)++;
            }
        }
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Read %d chems for user %s from %s", *chem_count, user_id, filepath);
        log_message(log);
    } else {
        char log[256];
        snprintf(log, sizeof(log), "No chems file for user %s, starting with empty inventory", user_id);
        log_message(log);
    }
}

int main() {
    log_message("Starting chem.+x");
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
    char* low_level_chems[] = {"Up_quark", "Down_quark", "Electron"};
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

            if (strncmp(content, "!chem", 5) == 0) {
                if (strlen(user_id) == 0) {
                    snprintf(log, sizeof(log), "No valid user ID found in content: %s", content);
                    log_message(log);
                    FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                    if (queue_fp) {
                        fprintf(queue_fp, "%s|Error: Invalid user ID format. Please use !chem @username\n", channel_id);
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

                // Generate random low-level compound
                int r = rand() % MAX_LOW_LEVEL_CHEMS;
                char* selected_chem = low_level_chems[r];
                update_user_chems(user_id, selected_chem);

                // Read current chems
                char chems[MAX_CHEMS][20] = {0};
                int chem_count = 0;
                read_user_chems(user_id, chems, &chem_count);

                // Prepare response
                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    char response[512] = {0};
                    snprintf(response, sizeof(response), "User %s received: %s\nCurrent inventory:", user_id, selected_chem);
                    for (int i = 0; i < chem_count; i++) {
                        char chem_line[32];
                        snprintf(chem_line, sizeof(chem_line), "\n- %s", chems[i]);
                        strncat(response, chem_line, sizeof(response) - strlen(response) - 1);
                    }
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
    log_message("Exiting chem.+x");
    return 0;
}
