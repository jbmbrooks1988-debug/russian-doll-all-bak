#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "users_processed_hashes.txt"
#define LOG_FILE "buy_log.txt"
#define USERS_DIR "users"
#define PRICES_FILE "data/prices.txt"
#define POSITION_FILE "buy_last_position.txt"
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

long load_last_position() {
    long last_pos = 0;
    FILE* fp = fopen(POSITION_FILE, "r");
    if (fp) {
        if (fscanf(fp, "%ld", &last_pos) != 1) {
            last_pos = 0;
        }
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Loaded last position: %ld from %s", last_pos, POSITION_FILE);
        log_message(log);
    } else {
        log_message("No last position file found, starting from beginning");
    }
    return last_pos;
}

void save_last_position(long last_pos) {
    FILE* fp = fopen(POSITION_FILE, "w");
    if (fp) {
        fprintf(fp, "%ld", last_pos);
        fflush(fp);
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Saved last position: %ld to %s", last_pos, POSITION_FILE);
        log_message(log);
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Failed to save %s: %s", POSITION_FILE, strerror(errno));
        log_message(err);
    }
}

int is_hash_processed(const char* hash, char hashes[][7], int hash_count) {
    for (int i = 0; i < hash_count; i++) {
        if (strcmp(hashes[i], hash) == 0) {
            return 1;
        }
    }
    return 0;
}

void save_hash(const char* hash, char hashes[][7], int* hash_count) {
    if (*hash_count >= MAX_HASHES) {
        char log[256];
        snprintf(log, sizeof(log), "Max hashes reached (%d), cannot save hash: %s", *hash_count, hash);
        log_message(log);
        return;
    }
    int fd = open(PROCESSED_HASHES_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open %s: %s", PROCESSED_HASHES_FILE, strerror(errno));
        log_message(err);
        return;
    }
    if (flock(fd, LOCK_EX) == -1) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to lock %s: %s", PROCESSED_HASHES_FILE, strerror(errno));
        log_message(err);
        close(fd);
        return;
    }
    strcpy(hashes[*hash_count], hash);
    (*hash_count)++;
    dprintf(fd, "%s\n", hash);
    flock(fd, LOCK_UN);
    close(fd);
    char log[256];
    snprintf(log, sizeof(log), "Saved hash: %s (hash_count=%d)", hash, *hash_count);
    log_message(log);
}

void load_processed_hashes(char hashes[][7], int* hash_count) {
    *hash_count = 0;
    int fd = open(PROCESSED_HASHES_FILE, O_RDONLY);
    if (fd == -1) {
        char log[256];
        snprintf(log, sizeof(log), "Failed to open %s for reading: %s", PROCESSED_HASHES_FILE, strerror(errno));
        log_message(log);
        return;
    }
    if (flock(fd, LOCK_SH) == -1) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to lock %s for reading: %s", PROCESSED_HASHES_FILE, strerror(errno));
        log_message(err);
        close(fd);
        return;
    }
    FILE* fp = fdopen(fd, "r");
    if (!fp) {
        char log[256];
        snprintf(log, sizeof(log), "Failed to fdopen %s: %s", PROCESSED_HASHES_FILE, strerror(errno));
        log_message(log);
        flock(fd, LOCK_UN);
        close(fd);
        return;
    }
    char line[8];
    while (fgets(line, sizeof(line), fp) && *hash_count < MAX_HASHES) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            strcpy(hashes[*hash_count], line);
            (*hash_count)++;
        }
    }
    flock(fd, LOCK_UN);
    fclose(fp);
    char log[256];
    snprintf(log, sizeof(log), "Loaded %d hashes from %s", *hash_count, PROCESSED_HASHES_FILE);
    log_message(log);
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
            char name[32];
            float price;
            if (sscanf(line, "%[^=]=%f", name, &price) == 2) {
                for (int j = 0; j < MAX_ITEMS; j++) {
                    if (strcmp(name, item_names[j]) == 0) {
                        prices[j] = price;
                        i++;
                        break;
                    }
                }
            }
        }
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Loaded %d item prices from %s", i, PRICES_FILE);
        log_message(log);
        for (int j = 0; j < MAX_ITEMS; j++) {
            if (prices[j] == 0.0) {
                prices[j] = 100.0 + (j * 50.0);
            }
        }
        if (i < MAX_ITEMS) {
            log_message("Incomplete prices file, initialized remaining with defaults");
        }
    } else {
        for (int i = 0; i < MAX_ITEMS; i++) {
            prices[i] = 100.0 + (i * 50.0);
        }
        log_message("No prices file found, initialized default prices");
        save_item_prices(prices, item_names);
    }
}

void save_item_prices(float prices[], char item_names[][32]) {
    ensure_data_dir();
    FILE* fp = fopen(PRICES_FILE, "w");
    if (fp) {
        for (int i = 0; i < MAX_ITEMS; i++) {
            fprintf(fp, "%s=%f\n", item_names[i], prices[i]);
        }
        fflush(fp);
        fclose(fp);
        log_message("Saved item prices to data/prices.txt");
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Failed to save %s: %s", PRICES_FILE, strerror(errno));
        log_message(err);
    }
}

void fluctuate_prices(float prices[]) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        int r = rand() % 100;
        float change = (r - 50) / 100.0 * prices[i] * 0.1;
        prices[i] += change;
        if (prices[i] < 10.0) prices[i] = 10.0;
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
        for (int i = 0; i < MAX_ITEMS; i) {
            item_quantities[i] = 0;
        }
        snprintf(state_content, max_size, "user_command_count=0\nlast_login=%ld\n", time(NULL));
        update_user_state(user_id, 0, item_quantities);
        char log[256];
        snprintf(log, sizeof(log), "Created new state for user %s", user_id);
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
        char log[256];
        snprintf(log, sizeof(log), "Failed to write state for user %s: %s", user_id, strerror(errno));
        log_message(log);
    }
}

int main() {
    log_message("Starting buy.+x");
    srand(time(NULL));
    ensure_users_dir();
    char hashes[MAX_HASHES][7] = {0};
    int hash_count = 0;
    load_processed_hashes(hashes, &hash_count);
    
    char item_names[MAX_ITEMS][32];
    float item_prices[MAX_ITEMS] = {0};
    initialize_items(item_prices, item_names);
    fluctuate_prices(item_prices);
    save_item_prices(item_prices, item_names);
    
    FILE* event_fp = fopen(EVENTS_FILE, "r");
    if (!event_fp) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open %s: %s", EVENTS_FILE, strerror(errno));
        log_message(err);
        exit(1);
    }
    log_message("Opened events.txt successfully");
    long last_pos = load_last_position();
    fseek(event_fp, last_pos, SEEK_SET);
    while (1) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), event_fp)) {
            last_pos = ftell(event_fp);
            save_last_position(last_pos);
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
            if (is_hash_processed(hash, hashes, hash_count)) {
                snprintf(log, sizeof(log), "Hash %s already processed, skipping", hash);
                log_message(log);
                continue;
            }
            save_hash(hash, hashes, &hash_count);
            snprintf(log, sizeof(log), "Saved hash: %s", hash);
            log_message(log);

            if (strstr(content, "!buy")) {
                char user_id[32] = {0};
                char item_name[32] = {0};
                char quantity_str[16] = {0};
                int quantity = 1;
                char* user_id_start = strstr(content, "<@");
                if (user_id_start) {
                    user_id_start += 2;
                    char* user_id_end = strchr(user_id_start, '>');
                    if (user_id_end && user_id_end - user_id_start < sizeof(user_id)) {
                        int user_id_len = user_id_end - user_id_start;
                        strncpy(user_id, user_id_start, user_id_len);
                        user_id[user_id_len] = '\0';
                        char* item_start = user_id_end + 1;
                        while (*item_start == ' ') item_start++;
                        char* quantity_start = item_start;
                        while (*quantity_start && *quantity_start != ' ') quantity_start++;
                        if (*quantity_start == ' ') {
                            *quantity_start = '\0';
                            quantity_start++;
                            while (*quantity_start == ' ') quantity_start++;
                            strncpy(quantity_str, quantity_start, sizeof(quantity_str) - 1);
                            quantity_str[sizeof(quantity_str) - 1] = '\0';
                            if (strlen(quantity_str) > 0) {
                                quantity = atoi(quantity_str);
                                if (quantity <= 0) quantity = 1;
                            }
                        }
                        strncpy(item_name, item_start, sizeof(item_name) - 1);
                        item_name[sizeof(item_name) - 1] = '\0';
                    }
                }
                
                int fd = open(SEND_QUEUE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                    log_message(log);
                    continue;
                }
                FILE* queue_fp = fdopen(fd, "w");
                if (!queue_fp) {
                    snprintf(log, sizeof(log), "Failed to fdopen %s: %s", SEND_QUEUE_FILE, strerror(errno));
                    log_message(log);
                    close(fd);
                    continue;
                }
                
                if (strlen(user_id) == 0 || strlen(item_name) == 0) {
                    char item_list[256] = "Available items: ";
                    for (int i = 0; i < MAX_ITEMS; i++) {
                        char item_entry[64];
                        snprintf(item_entry, sizeof(item_entry), "%s: %.2f points\\n", item_names[i], item_prices[i]);
                        strncat(item_list, item_entry, sizeof(item_list) - strlen(item_list) - 1);
                    }
                    fprintf(queue_fp, "%s|%s", channel_id, item_list);
                    fclose(queue_fp);
                    snprintf(log, sizeof(log), "Wrote item list to %s: %s", SEND_QUEUE_FILE, item_list);
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
                        
                        float total_cost = item_prices[item_index] * quantity;
                        int margin_points = user_points * 2;
                        if (user_points + margin_points < total_cost) {
                            fprintf(queue_fp, "%s|Error: User %s has %d points (with %d margin), needs %.2f for %d %s", channel_id, user_id, user_points, margin_points, total_cost, quantity, item_name);
                            fclose(queue_fp);
                            snprintf(log, sizeof(log), "Wrote insufficient points error for user %s to %s", user_id, SEND_QUEUE_FILE);
                            log_message(log);
                        } else {
                            user_points -= (int)total_cost;
                            item_quantities[item_index] += quantity;
                            update_user_state(user_id, user_points, item_quantities);
                            fprintf(queue_fp, "%s|User %s purchased %d %s for %.2f points. Remaining points: %d", channel_id, user_id, quantity, item_name, total_cost, user_points);
                            fclose(queue_fp);
                            snprintf(log, sizeof(log), "Wrote purchase confirmation for user %s, %d %s to %s", user_id, quantity, item_name, SEND_QUEUE_FILE);
                            log_message(log);
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
                
                fluctuate_prices(item_prices);
                save_item_prices(item_prices, item_names);
            }
        }
        if (feof(event_fp)) {
            clearerr(event_fp);
        }
        sleep(1);
    }
    
    fclose(event_fp);
    log_message("Exiting buy.+x");
    return 0;
}
