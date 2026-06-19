#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdbool.h>
#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "craft_processed_hashes.txt"
#define LOG_FILE "craft_log.txt"
#define USERS_DIR "data"
#define ELEMENTS_FILE "data/elements.txt"
#define MAX_LINE 4096
#define MAX_HASHES 1000
#define MAX_CHEMS 1000
#define MAX_ELEMENTS 500

// Element data
char element_names[MAX_ELEMENTS][20];
int element_protons[MAX_ELEMENTS];
int element_neutrons[MAX_ELEMENTS];
int element_electrons[MAX_ELEMENTS];
int element_combo1[MAX_ELEMENTS];
int element_combo2[MAX_ELEMENTS];
int element_count = 0;

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

void load_elements() {
    FILE *file = fopen(ELEMENTS_FILE, "r");
    if (!file) {
        char err[256];
        snprintf(err, sizeof(err), "Error: Could not open %s", ELEMENTS_FILE);
        log_message(err);
        return;
    }
    char line[100];
    while (fgets(line, sizeof(line), file) && element_count < MAX_ELEMENTS) {
        int protons, neutrons, electrons, combo1, combo2;
        if (sscanf(line, "%19s %d %d %d %d %d", element_names[element_count],
                   &protons, &neutrons, &electrons, &combo1, &combo2) == 6) {
            element_protons[element_count] = protons;
            element_neutrons[element_count] = neutrons;
            element_electrons[element_count] = electrons;
            element_combo1[element_count] = combo1;
            element_combo2[element_count] = combo2;
            element_count++;
        }
    }
    fclose(file);
    char log[256];
    snprintf(log, sizeof(log), "Loaded %d elements from %s", element_count, ELEMENTS_FILE);
    log_message(log);
}

int get_element_index(const char* name) {
    char input_name[20];
    strncpy(input_name, name, sizeof(input_name) - 1);
    input_name[sizeof(input_name) - 1] = '\0';
    for (int i = 0; input_name[i]; i++) {
        input_name[i] = tolower(input_name[i]);
    }
    for (int i = 0; i < element_count; i++) {
        char temp_name[20];
        strcpy(temp_name, element_names[i]);
        for (int j = 0; temp_name[j]; j++) {
            temp_name[j] = tolower(temp_name[j]);
        }
        if (strcmp(temp_name, input_name) == 0) {
            return i;
        }
    }
    return -1;
}

int combine_elements(int index1, int index2) {
    for (int i = 0; i < element_count; i++) {
        if ((element_combo1[i] == index1 && element_combo2[i] == index2) ||
            (element_combo1[i] == index2 && element_combo2[i] == index1)) {
            return i;
        }
    }
    if (element_protons[index1] >= 0 && element_protons[index2] >= 0 &&
        element_neutrons[index1] >= 0 && element_neutrons[index2] >= 0 &&
        element_electrons[index1] >= 0 && element_electrons[index2] >= 0) {
        int total_p = element_protons[index1] + element_protons[index2];
        int total_n = element_neutrons[index1] + element_neutrons[index2];
        int total_e = element_electrons[index1] + element_electrons[index2];
        for (int i = 0; i < element_count; i++) {
            if (element_protons[i] == total_p && element_neutrons[i] == total_n && element_electrons[i] == total_e) {
                return i;
            }
        }
    }
    return -1;
}

void update_user_chems(const char* user_id, const char* chem_name, int idx1, int idx2, char chems[][20], int chem_count) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s/chems.txt", USERS_DIR, user_id);
    
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

    char new_chems[MAX_CHEMS][20] = {0};
    int new_chem_count = 0;
    for (int i = 0; i < chem_count && new_chem_count < MAX_CHEMS; i++) {
        if (i != idx1 && i != idx2) {
            strcpy(new_chems[new_chem_count], chems[i]);
            new_chem_count++;
        }
    }
    if (new_chem_count < MAX_CHEMS) {
        strcpy(new_chems[new_chem_count], chem_name);
        new_chem_count++;
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Max chems reached for user %s, cannot add %s", user_id, chem_name);
        log_message(err);
        return;
    }

    FILE* fp = fopen(filepath, "w");
    if (fp) {
        for (int i = 0; i < new_chem_count; i++) {
            fprintf(fp, "%s\n", new_chems[i]);
        }
        fflush(fp);
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Updated %s for user %s: removed idx %d, %d; added %s; total chems %d", 
                 filepath, user_id, idx1, idx2, chem_name, new_chem_count);
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
                int idx = get_element_index(line);
                if (idx >= 0) {
                    strcpy(chems[*chem_count], line);
                    (*chem_count)++;
                } else {
                    char log[256];
                    snprintf(log, sizeof(log), "Invalid element %s in %s, skipping", line, filepath);
                    log_message(log);
                }
            }
        }
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Read %d chems for user %s from %s", *chem_count, user_id, filepath);
        log_message(log);
    } else {
        for (int i = 0; i < element_count && *chem_count < MAX_CHEMS; i++) {
            if (element_combo1[i] == -1 && element_combo2[i] == -1) {
                strcpy(chems[*chem_count], element_names[i]);
                (*chem_count)++;
            }
        }
        if (*chem_count > 0) {
            FILE* fp = fopen(filepath, "w");
            if (fp) {
                for (int i = 0; i < *chem_count; i++) {
                    fprintf(fp, "%s\n", chems[i]);
                }
                fclose(fp);
                char log[256];
                snprintf(log, sizeof(log), "Initialized inventory for user %s with %d starting elements", user_id, *chem_count);
                log_message(log);
            } else {
                char err[256];
                snprintf(err, sizeof(err), "Failed to create chems file for user %s: %s", user_id, strerror(errno));
                log_message(err);
            }
        } else {
            char log[256];
            snprintf(log, sizeof(log), "No chems file for user %s, starting with empty inventory", user_id);
            log_message(log);
        }
    }
}

int get_min_elements_needed() {
    return 2;
}

int get_highest_element_index(char chems[][20], int chem_count) {
    int highest_idx = -1;
    for (int i = 0; i < chem_count; i++) {
        int idx = get_element_index(chems[i]);
        if (idx > highest_idx) {
            highest_idx = idx;
        }
    }
    return highest_idx;
}

void get_next_element(int current_idx, char chems[][20], int chem_count, int* next_idx, char required_elements[][20], int* required_count) {
    *next_idx = -1;
    *required_count = 0;
    int counts[MAX_ELEMENTS] = {0};
    for (int i = 0; i < chem_count; i++) {
        int idx = get_element_index(chems[i]);
        if (idx >= 0) {
            counts[idx]++;
        }
    }
    // Start from the index after current_idx to avoid suggesting the same or lower element
    for (int i = current_idx + 1; i < element_count; i++) {
        if (element_combo1[i] >= 0 && element_combo2[i] >= 0) {
            int c1 = element_combo1[i];
            int c2 = element_combo2[i];
            int count_c1 = counts[c1];
            int count_c2 = counts[c2];
            bool can_craft = (c1 == c2) ? (count_c1 >= 2) : (count_c1 > 0 && count_c2 > 0);
            if (can_craft) {
                *next_idx = i;
                strcpy(required_elements[0], element_names[c1]);
                strcpy(required_elements[1], element_names[c2]);
                *required_count = 2;
                break;
            }
        }
    }
    // If no craftable element found after current_idx, try any craftable element (excluding current_idx)
    if (*next_idx == -1) {
        for (int i = 0; i < element_count; i++) {
            if (i <= current_idx || element_combo1[i] < 0 || element_combo2[i] < 0) {
                continue;
            }
            int c1 = element_combo1[i];
            int c2 = element_combo2[i];
            int count_c1 = counts[c1];
            int count_c2 = counts[c2];
            bool can_craft = (c1 == c2) ? (count_c1 >= 2) : (count_c1 > 0 && count_c2 > 0);
            if (can_craft) {
                *next_idx = i;
                strcpy(required_elements[0], element_names[c1]);
                strcpy(required_elements[1], element_names[c2]);
                *required_count = 2;
                break;
            }
        }
    }
    // If no craftable element, suggest the next element's requirements
    if (*next_idx == -1) {
        for (int i = current_idx + 1; i < element_count; i++) {
            if (element_combo1[i] >= 0 && element_combo2[i] >= 0) {
                *next_idx = i;
                strcpy(required_elements[0], element_names[element_combo1[i]]);
                strcpy(required_elements[1], element_names[element_combo2[i]]);
                *required_count = 2;
                break;
            }
        }
    }
}

void check_missing_elements(char chems[][20], int chem_count, char required_elements[][20], int required_count, char* missing_str, size_t max_len, const char* target_name) {
    int counts[MAX_ELEMENTS] = {0};
    for (int i = 0; i < chem_count; i++) {
        int idx = get_element_index(chems[i]);
        if (idx >= 0) {
            counts[idx]++;
        }
    }
    char* ptr = missing_str;
    size_t remaining = max_len;
    ptr += snprintf(ptr, remaining, "To craft the next element (%s), you need: ", target_name);
    remaining = max_len - (ptr - missing_str);
    for (int i = 0; i < required_count && remaining > 0; i++) {
        int len = snprintf(ptr, remaining, "%s%s", required_elements[i], 
                           (i < required_count - 1) ? ", " : "");
        ptr += len;
        remaining -= len;
    }
    int needed[2] = {0, 0};
    for (int i = 0; i < required_count; i++) {
        int idx = get_element_index(required_elements[i]);
        if (idx >= 0) {
            needed[i] = 1 - counts[idx];
            if (needed[i] < 0) needed[i] = 0;
        }
    }
    if (needed[0] > 0 || needed[1] > 0) {
        ptr += snprintf(ptr, remaining, " (");
        remaining = max_len - (ptr - missing_str);
        if (needed[0] > 0) {
            int len = snprintf(ptr, remaining, "%s (%d more)", required_elements[0], needed[0]);
            ptr += len;
            remaining -= len;
            if (needed[1] > 0) {
                ptr += snprintf(ptr, remaining, ", ");
                remaining = max_len - (ptr - missing_str);
            }
        }
        if (needed[1] > 0) {
            int len = snprintf(ptr, remaining, "%s (%d more)", required_elements[1], needed[1]);
            ptr += len;
            remaining -= len;
        }
        ptr += snprintf(ptr, remaining, ")");
    } else {
        ptr += snprintf(ptr, remaining, ". Try combining them!");
    }
}

int find_valid_combination(char chems[][20], int chem_count, int target_idx) {
    int counts[MAX_ELEMENTS] = {0};
    char log[256];
    char inventory[256] = {0};
    format_inventory(chems, chem_count, inventory, sizeof(inventory));
    snprintf(log, sizeof(log), "Inventory for crafting: %s", inventory);
    log_message(log);

    for (int i = 0; i < chem_count; i++) {
        int idx = get_element_index(chems[i]);
        if (idx >= 0) {
            counts[idx]++;
        }
    }

    if (target_idx >= 0 && element_combo1[target_idx] >= 0 && element_combo2[target_idx] >= 0) {
        int c1 = element_combo1[target_idx];
        int c2 = element_combo2[target_idx];
        bool can_craft = (c1 == c2) ? (counts[c1] >= 2) : (counts[c1] > 0 && counts[c2] > 0);
        if (can_craft) {
            snprintf(log, sizeof(log), "Crafting target %s with components %s and %s", 
                     element_names[target_idx], element_names[c1], element_names[c2]);
            log_message(log);
            return target_idx;
        }
        snprintf(log, sizeof(log), "Target %s not craftable with current inventory", element_names[target_idx]);
        log_message(log);
    }

    int selected_idx = -1;
    for (int i = element_count - 1; i >= 0; i--) {
        if (element_combo1[i] >= 0 && element_combo2[i] >= 0 && counts[i] == 0) {
            int c1 = element_combo1[i];
            int c2 = element_combo2[i];
            bool can_craft = (c1 == c2) ? (counts[c1] >= 2) : (counts[c1] > 0 && counts[c2] > 0);
            if (can_craft) {
                selected_idx = i;
                snprintf(log, sizeof(log), "Crafting highest craftable %s with components %s and %s", 
                         element_names[i], element_names[c1], element_names[c2]);
                log_message(log);
                break;
            }
        }
    }

    if (selected_idx == -1) {
        snprintf(log, sizeof(log), "No craftable elements found");
        log_message(log);
        return -1;
    }

    return selected_idx;
}

void format_inventory(char chems[][20], int chem_count, char* inventory_str, size_t max_len) {
    if (chem_count == 0) {
        snprintf(inventory_str, max_len, "Your inventory is empty.");
        return;
    }
    char* ptr = inventory_str;
    ptr += snprintf(ptr, max_len, "Your inventory: ");
    size_t remaining = max_len - (ptr - inventory_str);
    for (int i = 0; i < chem_count && remaining > 0; i++) {
        int len = snprintf(ptr, remaining, "%s%s", chems[i], (i < chem_count - 1) ? ", " : "");
        ptr += len;
        remaining -= len;
    }
}

int main() {
    log_message("Starting craft.+x");
    srand(time(NULL));
    ensure_users_dir();
    load_elements();
    if (element_count == 0) {
        log_message("No elements loaded. Exiting.");
        exit(1);
    }
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

            if (strncmp(content, "!craft", 6) != 0) {
                continue;
            }

            char command[MAX_LINE];
            strcpy(command, content);
            char* token = strtok(command, " ");
            if (token && strcmp(token, "!craft") == 0) {
                token = strtok(NULL, " ");
                char user_id[32] = {0};
                char target_name[20] = {0};
                if (token && strncmp(token, "<@", 2) == 0) {
                    char* uid_start = token + 2;
                    char* uid_end = strchr(uid_start, '>');
                    if (uid_end) {
                        int uid_len = uid_end - uid_start;
                        strncpy(user_id, uid_start, uid_len);
                        user_id[uid_len] = '\0';
                        token = strtok(NULL, " ");
                        if (token) {
                            strncpy(target_name, token, sizeof(target_name) - 1);
                            target_name[sizeof(target_name) - 1] = '\0';
                        }
                    } else {
                        snprintf(log, sizeof(log), "Invalid user format in content: %s", content);
                        log_message(log);
                        continue;
                    }
                } else {
                    snprintf(log, sizeof(log), "No valid user ID found in content: %s", content);
                    log_message(log);
                    continue;
                }

                if (strlen(user_id) == 0) {
                    snprintf(log, sizeof(log), "No valid user ID parsed");
                    log_message(log);
                    continue;
                }

                char chems[MAX_CHEMS][20] = {0};
                int chem_count = 0;
                read_user_chems(user_id, chems, &chem_count);
                char response[512] = {0};
                char inventory[256] = {0};
                char missing[256] = {0};
                format_inventory(chems, chem_count, inventory, sizeof(inventory));

                int target_idx = get_element_index(target_name);
                if (target_idx >= 0) {
                    snprintf(log, sizeof(log), "Processing craft for %s (idx %d) for user %s", target_name, target_idx, user_id);
                    log_message(log);
                } else if (strlen(target_name) > 0) {
                    snprintf(response, sizeof(response), "Invalid element name: %s. %s", target_name, inventory);
                }

                int result = -1;
                if (chem_count < get_min_elements_needed()) {
                    snprintf(response, sizeof(response), "Craft failed: you need at least %d elements to craft. %s", 
                             get_min_elements_needed(), inventory);
                } else {
                    result = find_valid_combination(chems, chem_count, target_idx);
                }

                char next_missing[256] = {0};
                if (result >= 0) {
                    format_inventory(chems, chem_count, inventory, sizeof(inventory));
                    int next_idx;
                    char next_required[2][20] = {0};
                    int next_required_count = 0;
                    // Use the crafted element's index for progression
                    get_next_element(result, chems, chem_count, &next_idx, next_required, &next_required_count);
                    if (next_idx >= 0) {
                        check_missing_elements(chems, chem_count, next_required, next_required_count, next_missing, sizeof(next_missing), element_names[next_idx]);
                    } else {
                        snprintf(next_missing, sizeof(next_missing), "No further elements can be crafted with your current inventory.");
                    }
                    snprintf(response, sizeof(response), "Success! Crafted: %s. %s %s", element_names[result], inventory, next_missing);
                } else {
                    int next_idx;
                    char required_elements[2][20] = {0};
                    int required_count = 0;
                    get_next_element(get_highest_element_index(chems, chem_count), chems, chem_count, &next_idx, required_elements, &required_count);
                    check_missing_elements(chems, chem_count, required_elements, required_count, missing, sizeof(missing), next_idx >= 0 ? element_names[next_idx] : "Pair_up_quarks");
                    snprintf(response, sizeof(response), "Craft failed: no valid combination found. %s %s", inventory, missing);
                }

                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
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
    log_message("Exiting craft.+x");
    return 0;
}
