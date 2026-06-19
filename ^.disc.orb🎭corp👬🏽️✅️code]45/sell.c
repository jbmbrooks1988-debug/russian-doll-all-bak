#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define LOG_FILE "sell_log.txt"
#define USERS_DIR "users"
#define PRICES_FILE "data/prices.txt"
#define PROCESSED_HASHES_FILE "sell_processed_hashes.txt"
#define MAX_LINE 4096
#define MAX_ITEMS 5
#define MAX_STATE_SIZE 4096
#define MAX_HASHES 1000

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

void ensure_data_dir() {
    struct stat st = {0};
    if (stat("data", &st) == -1) {
        if (mkdir("data", 0700) == -1) {
            char err[256];
            snprintf(err, sizeof(err), "Failed to create data directory: %s", strerror(errno));
            log_message(err);
        } else {
            log_message("Created data directory");
        }
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

void initialize_items(float prices[], char item_names[][32]) {
    strcpy(item_names[0], "Sword");
    strcpy(item_names[1], "Shield");
    strcpy(item_names[2], "Potion");
    strcpy(item_names[3], "Armor");
    strcpy(item_names[4], "Gem");
    
    FILE* fp = fopen(PRICES_FILE, "r");
    if (fp) {
        char line[256];
        int i = 0;
        while (fgets(line, sizeof(line), fp) && i < MAX_ITEMS) {
            line[strcspn(line, "\n")] = 0;
            if (sscanf(line, "%*[^=]=%f", &prices[i]) == 1) {
                i++;
            }
        }
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Loaded %d item prices from %s", i, PRICES_FILE);
        log_message(log);
    } else {
        for (int i = 0; i < MAX_ITEMS; i++) {
            prices[i] = 100.0 + (i * 50.0); // Default prices: 100, 150, 200, 250, 300
        }
        log_message("No prices file found, initialized default prices");
    }
}

void read_user_state(const char* user_id, char* state_content, size_t max_size, int* user_points, int item_quantities[]) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s.state.txt", USERS_DIR, user_id);
    FILE* fp = fopen(filepath, "r");
    if (fp) {
        size_t bytes_read = fread(state_content, 1, max_size - 1, fp);
        state_content[bytes_read] = '\0';
        fclose(fp);
        char* points_str = strstr(state_content, "user_command_count=");
        if (points_str) {
            sscanf(points_str, "user_command_count=%d", user_points);
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
        snprintf(log, sizeof(log), "Read state for user %s: points=%d", user_id, *user_points);
        log_message(log);
    } else {
        *user_points = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            item_quantities[i] = 0;
        }
        snprintf(state_content, max_size, "user_command_count=0\nlast_login=%ld\n", time(NULL));
        char log[256];
        snprintf(log, sizeof(log), "No state found for user %s, initialized default", user_id);
        log_message(log);
    }
}

void update_user_state(const char* user_id, int user_points, int item_quantities[]) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s.state.txt", USERS_DIR, user_id);
    FILE* fp = fopen(filepath, "w");
    if (fp) {
        fprintf(fp, "user_command_count=%d\nlast_login=%ld\n", user_points, time(NULL));
        for (int i = 0; i < MAX_ITEMS; i++) {
            char* item_name = i == 0 ? "Sword" : i == 1 ? "Shield" : i == 2 ? "Potion" : i == 3 ? "Armor" : "Gem";
            fprintf(fp, "%s_count=%d\n", item_name, item_quantities[i]);
        }
        fflush(fp);
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Updated state for user %s: points=%d", user_id, user_points);
        log_message(log);
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Failed to write state for user %s: %s", user_id, strerror(errno));
        log_message(err);
    }
}

int main() {
    log_message("Starting sell.+x");
    srand(time(NULL));
    ensure_users_dir();
    ensure_data_dir();
    
    char hashes[MAX_HASHES][7] = {0};
    int hash_count = 0;
    load_processed_hashes(hashes, &hash_count);
    
    char item_names[MAX_ITEMS][32];
    float item_prices[MAX_ITEMS];
    initialize_items(item_prices, item_names);
    
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
            
            if (is_hash_processed(hash, hashes, &hash_count)) {
                snprintf(log, sizeof(log), "Hash %s already processed, skipping", hash);
                log_message(log);
                continue;
            }
            save_hash(hash, hashes, &hash_count);
            snprintf(log, sizeof(log), "Saved hash: %s", hash);
            log_message(log);
            
            if (strstr(content, "!sell")) {
                char user_id[32] = {0};
                char item_name[32] = {0};
                char number_str[32] = {0};
                char* token = strtok(content, " ");
                int arg_count = 0;
                while (token) {
                    if (arg_count == 1 && strstr(token, "<@")) {
                        char* user_id_start = token + 2;
                        char* user_id_end = strchr(user_id_start, '>');
                        if (user_id_end) {
                            int user_id_len = user_id_end - user_id_start;
                            if (user_id_len < sizeof(user_id)) {
                                strncpy(user_id, user_id_start, user_id_len);
                                user_id[user_id_len] = '\0';
                            }
                        }
                    } else if (arg_count == 2) {
                        strncpy(item_name, token, sizeof(item_name) - 1);
                        item_name[sizeof(item_name) - 1] = '\0';
                    } else if (arg_count == 3) {
                        strncpy(number_str, token, sizeof(number_str) - 1);
                        number_str[sizeof(number_str) - 1] = '\0';
                    }
                    token = strtok(NULL, " ");
                    arg_count++;
                }
                
                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (!queue_fp) {
                    snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                    log_message(log);
                    continue;
                }
                
                if (arg_count < 4 || strlen(user_id) == 0 || strlen(item_name) == 0 || strlen(number_str) == 0) {
                    fprintf(queue_fp, "%s|Error: Invalid !sell command. Usage: !sell @user item number", channel_id);
                    fclose(queue_fp);
                    snprintf(log, sizeof(log), "Wrote error for insufficient arguments to %s", SEND_QUEUE_FILE);
                    log_message(log);
                } else {
                    int number = atoi(number_str);
                    if (number <= 0) {
                        fprintf(queue_fp, "%s|Error: Number must be a positive integer", channel_id);
                        fclose(queue_fp);
                        snprintf(log, sizeof(log), "Wrote error for invalid number %s to %s", number_str, SEND_QUEUE_FILE);
                        log_message(log);
                    } else {
                        int item_index = -1;
                        for (int i = 0; i < MAX_ITEMS; i++) {
                            if (strcasecmp(item_name, item_names[i]) == 0) {
                                item_index = i;
                                break;
                            }
                        }
                        
                        if (item_index == -1) {
                            char item_list[256] = "Error: Item not found. Available items: ";
                            for (int i = 0; i < MAX_ITEMS; i++) {
                                char item_entry[64];
                                snprintf(item_entry, sizeof(item_entry), "%s: %.2f points\\n", item_names[i], item_prices[i]);
                                strncat(item_list, item_entry, sizeof(item_list) - strlen(item_list) - 1);
                            }
                            fprintf(queue_fp, "%s|%s", channel_id, item_list);
                            fclose(queue_fp);
                            snprintf(log, sizeof(log), "Wrote error for invalid item %s to %s: %s", item_name, SEND_QUEUE_FILE, item_list);
                            log_message(log);
                        } else {
                            char state_content[MAX_STATE_SIZE] = {0};
                            int user_points = 0;
                            int item_quantities[MAX_ITEMS] = {0};
                            read_user_state(user_id, state_content, MAX_STATE_SIZE, &user_points, item_quantities);
                            
                            int points_gained = (int)(item_prices[item_index] * number);
                            int new_points = user_points + points_gained;
                            int new_quantity = item_quantities[item_index] - number;
                            
                            if (new_points < 0) {
                                fprintf(queue_fp, "%s|Error: User %s cannot sell %d %s, would result in negative points (%d)", channel_id, user_id, number, item_name, new_points);
                                fclose(queue_fp);
                                snprintf(log, sizeof(log), "Wrote insufficient points error for user %s to %s", user_id, SEND_QUEUE_FILE);
                                log_message(log);
                            } else {
                                item_quantities[item_index] = new_quantity;
                                user_points = new_points;
                                update_user_state(user_id, user_points, item_quantities);
                                fprintf(queue_fp, "%s|User %s sold %d %s for %d points. Remaining points: %d, %s count: %d", channel_id, user_id, number, item_name, points_gained, user_points, item_name, new_quantity);
                                fclose(queue_fp);
                                snprintf(log, sizeof(log), "Wrote sell confirmation for user %s, %d %s to %s", user_id, number, item_name, SEND_QUEUE_FILE);
                                log_message(log);
                            }
                        }
                    }
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
            }
        }
        if (feof(event_fp)) {
            clearerr(event_fp);
        }
        sleep(1);
    }
    
    fclose(event_fp);
    log_message("Exiting sell.+x");
    return 0;
}
