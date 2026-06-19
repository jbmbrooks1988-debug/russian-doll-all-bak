#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
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
#define MAX_CHEMS 10000
#define MAX_ELEMENTS 500
#define MAX_RESPONSE 512
#define MAX_LOG 256
#define MAX_INVENTORY 256
#define MAX_MISSING 256
#define LOCK_RETRIES 10
#define LOCK_TIMEOUT_US 500000

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
        if (strncmp(hashes[i], hash, 7) == 0) {
            return 1;
        }
    }
    return 0;
}

void save_hash(const char* hash, char hashes[][7], int* hash_count) {
    if (*hash_count < MAX_HASHES) {
        strncpy(hashes[*hash_count], hash, 7);
        hashes[*hash_count][6] = '\0';
        (*hash_count)++;
        FILE* fp = fopen(PROCESSED_HASHES_FILE, "a");
        if (fp) {
            fprintf(fp, "%s\n", hash);
            fflush(fp);
            fclose(fp);
        } else {
            char err[MAX_LOG];
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
                strncpy(hashes[*hash_count], line, 7);
                hashes[*hash_count][6] = '\0';
                (*hash_count)++;
            }
        }
        fclose(fp);
        char log[MAX_LOG];
        snprintf(log, sizeof(log), "Loaded %d hashes from %s", *hash_count, PROCESSED_HASHES_FILE);
        log_message(log);
    } else {
        char log[MAX_LOG];
        snprintf(log, sizeof(log), "Failed to open %s for reading: %s", PROCESSED_HASHES_FILE, strerror(errno));
        log_message(log);
    }
}

void ensure_users_dir() {
    struct stat st = {0};
    if (stat(USERS_DIR, &st) == -1) {
        if (mkdir(USERS_DIR, 0700) == -1) {
            char err[MAX_LOG];
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
        char err[MAX_LOG];
        snprintf(err, sizeof(err), "Error: Could not open %s", ELEMENTS_FILE);
        log_message(err);
        return;
    }
    char line[100];
    while (fgets(line, sizeof(line), file) && element_count < MAX_ELEMENTS) {
        int protons, neutrons, electrons, combo1, combo2;
        if (sscanf(line, "%19s %d %d %d %d %d", element_names[element_count],
                   &protons, &neutrons, &electrons, &combo1, &combo2) == 6) {
            if ((combo1 == -1 && combo2 == -1) || (combo1 >= 0 && combo1 < MAX_ELEMENTS && combo2 >= 0 && combo2 < MAX_ELEMENTS)) {
                element_protons[element_count] = protons;
                element_neutrons[element_count] = neutrons;
                element_electrons[element_count] = electrons;
                element_combo1[element_count] = combo1;
                element_combo2[element_count] = combo2;
                element_count++;
            } else {
                char log[MAX_LOG];
                snprintf(log, sizeof(log), "Invalid combo indices for %s: combo1=%d, combo2=%d", element_names[element_count], combo1, combo2);
                log_message(log);
            }
        } else {
            char log[MAX_LOG];
            snprintf(log, sizeof(log), "Invalid line format in %s: %s", ELEMENTS_FILE, line);
            log_message(log);
        }
    }
    fclose(file);
    char log[MAX_LOG];
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
        strncpy(temp_name, element_names[i], sizeof(temp_name));
        temp_name[sizeof(temp_name) - 1] = '\0';
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
    return -1; // Disable proton/neutron/electron sum crafting to avoid unexpected results
}

void update_user_chems(const char* user_id, const char* chem_name, int idx1, int idx2, char chems[][20], int chem_count) {
    char filepath[256];
    char temp_filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s/chems.txt", USERS_DIR, user_id);
    snprintf(temp_filepath, sizeof(temp_filepath), "%s/%s/chems.txt.tmp", USERS_DIR, user_id);
    
    struct stat st = {0};
    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s/%s", USERS_DIR, user_id);
    if (stat(user_dir, &st) == -1) {
        if (mkdir(user_dir, 0700) == -1) {
            char err[MAX_LOG];
            snprintf(err, sizeof(err), "Failed to create %s: %s", user_dir, strerror(errno));
            log_message(err);
            return;
        }
    }

    char inventory[MAX_INVENTORY] = {0};
    format_inventory(chems, chem_count, inventory, sizeof(inventory));
    char log[MAX_LOG];
    snprintf(log, sizeof(log), "Before update for user %s: %s", user_id, inventory);
    log_message(log);

    char new_chems[MAX_CHEMS][20] = {0};
    int new_chem_count = 0;
    for (int i = 0; i < chem_count && new_chem_count < MAX_CHEMS; i++) {
        if (i != idx1 && i != idx2) {
            strncpy(new_chems[new_chem_count], chems[i], 20);
            new_chems[new_chem_count][19] = '\0';
            new_chem_count++;
        }
    }
    if (new_chem_count < MAX_CHEMS) {
        strncpy(new_chems[new_chem_count], chem_name, 20);
        new_chems[new_chem_count][19] = '\0';
        new_chem_count++;
    } else {
        char err[MAX_LOG];
        snprintf(err, sizeof(err), "Max chems reached for user %s, cannot add %s", user_id, chem_name);
        log_message(err);
        return;
    }

    FILE* fp = fopen(temp_filepath, "w");
    if (fp) {
        int retries = LOCK_RETRIES;
        while (retries > 0 && flock(fileno(fp), LOCK_EX | LOCK_NB) == -1) {
            retries--;
            usleep(LOCK_TIMEOUT_US);
            char log[MAX_LOG];
            snprintf(log, sizeof(log), "Retrying lock on %s (%d retries left)", temp_filepath, retries);
            log_message(log);
        }
        if (retries == 0) {
            char err[MAX_LOG];
            snprintf(err, sizeof(err), "Failed to lock %s after %d retries: %s", temp_filepath, LOCK_RETRIES, strerror(errno));
            log_message(err);
            fclose(fp);
            return;
        }
        for (int i = 0; i < new_chem_count; i++) {
            fprintf(fp, "%s\n", new_chems[i]);
        }
        fflush(fp);
        if (flock(fileno(fp), LOCK_UN) == -1) {
            char err[MAX_LOG];
            snprintf(err, sizeof(err), "Failed to unlock %s: %s", temp_filepath, strerror(errno));
            log_message(err);
        }
        fclose(fp);
        if (rename(temp_filepath, filepath) == -1) {
            char err[MAX_LOG];
            snprintf(err, sizeof(err), "Failed to rename %s to %s: %s", temp_filepath, filepath, strerror(errno));
            log_message(err);
        } else {
            // Verify write by re-reading
            char verify_chems[MAX_CHEMS][20] = {0};
            int verify_count = 0;
            FILE* verify_fp = fopen(filepath, "r");
            if (verify_fp) {
                char line[20];
                while (fgets(line, sizeof(line), verify_fp) && verify_count < MAX_CHEMS) {
                    line[strcspn(line, "\n")] = 0;
                    if (strlen(line) > 0 && get_element_index(line) >= 0) {
                        strncpy(verify_chems[verify_count], line, 20);
                        verify_chems[verify_count][19] = '\0';
                        verify_count++;
                    }
                }
                fclose(verify_fp);
                char verify_inventory[MAX_INVENTORY] = {0};
                format_inventory(verify_chems, verify_count, verify_inventory, sizeof(verify_inventory));
                snprintf(log, sizeof(log), "Verified %s for user %s: %s", filepath, user_id, verify_inventory);
                log_message(log);
                if (verify_count != new_chem_count) {
                    snprintf(log, sizeof(log), "Warning: Verification mismatch for user %s: wrote %d, read %d", user_id, new_chem_count, verify_count);
                    log_message(log);
                }
            } else {
                snprintf(log, sizeof(log), "Failed to verify %s for user %s: %s", filepath, user_id, strerror(errno));
                log_message(log);
            }
        }
    } else {
        char err[MAX_LOG];
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
        int retries = LOCK_RETRIES;
        while (retries > 0 && flock(fileno(fp), LOCK_SH | LOCK_NB) == -1) {
            retries--;
            usleep(LOCK_TIMEOUT_US);
            char log[MAX_LOG];
            snprintf(log, sizeof(log), "Retrying lock on %s (%d retries left)", filepath, retries);
            log_message(log);
        }
        if (retries == 0) {
            char err[MAX_LOG];
            snprintf(err, sizeof(err), "Failed to lock %s after %d retries: %s", filepath, LOCK_RETRIES, strerror(errno));
            log_message(err);
            fclose(fp);
            return;
        }
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char file_content[1024] = {0};
        char* ptr = file_content;
        size_t remaining = sizeof(file_content);
        char line[20];
        while (fgets(line, sizeof(line), fp) && *chem_count < MAX_CHEMS) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0) {
                int idx = get_element_index(line);
                if (idx >= 0) {
                    strncpy(chems[*chem_count], line, 20);
                    chems[*chem_count][19] = '\0';
                    (*chem_count)++;
                    int len = snprintf(ptr, remaining, "%s%s", line, (*chem_count < MAX_CHEMS - 1) ? "," : "");
                    ptr += len;
                    remaining -= len;
                } else {
                    char log[MAX_LOG];
                    snprintf(log, sizeof(log), "Invalid element %s in %s, skipping", line, filepath);
                    log_message(log);
                }
            }
        }
        if (flock(fileno(fp), LOCK_UN) == -1) {
            char err[MAX_LOG];
            snprintf(err, sizeof(err), "Failed to unlock %s: %s", filepath, strerror(errno));
            log_message(err);
        }
        fclose(fp);
        char log[MAX_LOG];
        snprintf(log, sizeof(log), "Read %d chems for user %s from %s: %s", *chem_count, user_id, filepath, file_content);
        log_message(log);
        if (*chem_count == 0 && file_size > 0) {
            snprintf(log, sizeof(log), "Warning: No valid elements read from non-empty %s for user %s", filepath, user_id);
            log_message(log);
        }
    } else {
        for (int i = 0; i < element_count && *chem_count < MAX_CHEMS; i++) {
            if (element_combo1[i] == -1 && element_combo2[i] == -1) {
                strncpy(chems[*chem_count], element_names[i], 20);
                chems[*chem_count][19] = '\0';
                (*chem_count)++;
            }
        }
        if (*chem_count > 0) {
            FILE* fp = fopen(filepath, "w");
            if (fp) {
                int retries = LOCK_RETRIES;
                while (retries > 0 && flock(fileno(fp), LOCK_EX | LOCK_NB) == -1) {
                    retries--;
                    usleep(LOCK_TIMEOUT_US);
                    char log[MAX_LOG];
                    snprintf(log, sizeof(log), "Retrying lock on %s (%d retries left)", filepath, retries);
                    log_message(log);
                }
                if (retries == 0) {
                    char err[MAX_LOG];
                    snprintf(err, sizeof(err), "Failed to lock %s after %d retries: %s", filepath, LOCK_RETRIES, strerror(errno));
                    log_message(err);
                    fclose(fp);
                    return;
                }
                for (int i = 0; i < *chem_count; i++) {
                    fprintf(fp, "%s\n", chems[i]);
                }
                if (flock(fileno(fp), LOCK_UN) == -1) {
                    char err[MAX_LOG];
                    snprintf(err, sizeof(err), "Failed to unlock %s: %s", filepath, strerror(errno));
                    log_message(err);
                }
                fclose(fp);
                char inventory[MAX_INVENTORY] = {0};
                format_inventory(chems, *chem_count, inventory, sizeof(inventory));
                char log[MAX_LOG];
                snprintf(log, sizeof(log), "Initialized inventory for user %s with %d starting elements: %s", user_id, *chem_count, inventory);
                log_message(log);
            } else {
                char err[MAX_LOG];
                snprintf(err, sizeof(err), "Failed to create chems file for user %s: %s", user_id, strerror(errno));
                log_message(err);
            }
        } else {
            char log[MAX_LOG];
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

void get_prerequisite_elements(int target_idx, char chems[][20], int chem_count, int* prereq_idx, char required_elements[][20], int* required_count) {
    *prereq_idx = -1;
    *required_count = 0;
    int counts[MAX_ELEMENTS] = {0};
    for (int i = 0; i < chem_count; i++) {
        int idx = get_element_index(chems[i]);
        if (idx >= 0) {
            counts[idx]++;
        }
    }
    if (target_idx >= 0 && element_combo1[target_idx] >= 0 && element_combo2[target_idx] >= 0) {
        int c1 = element_combo1[target_idx];
        int c2 = element_combo2[target_idx];
        bool has_c1 = counts[c1] > 0;
        bool has_c2 = counts[c2] > 0 || (c1 == c2 && counts[c1] >= 2);
        if (!has_c1 && element_combo1[c1] >= 0 && element_combo2[c1] >= 0) {
            int c1_c1 = element_combo1[c1];
            int c1_c2 = element_combo2[c1];
            if ((c1_c1 == c1_c2 && counts[c1_c1] >= 2) || (counts[c1_c1] > 0 && counts[c1_c2] > 0)) {
                *prereq_idx = c1;
                strncpy(required_elements[0], element_names[c1_c1], 20);
                required_elements[0][19] = '\0';
                strncpy(required_elements[1], element_names[c1_c2], 20);
                required_elements[1][19] = '\0';
                *required_count = 2;
                return;
            }
            // Recursive check for c1's prerequisites
            int sub_prereq_idx;
            char sub_prereq_elements[2][20] = {0};
            int sub_prereq_count = 0;
            get_prerequisite_elements(c1, chems, chem_count, &sub_prereq_idx, sub_prereq_elements, &sub_prereq_count);
            if (sub_prereq_idx >= 0) {
                *prereq_idx = sub_prereq_idx;
                strncpy(required_elements[0], sub_prereq_elements[0], 20);
                required_elements[0][19] = '\0';
                strncpy(required_elements[1], sub_prereq_elements[1], 20);
                required_elements[1][19] = '\0';
                *required_count = sub_prereq_count;
                return;
            }
        }
        if (!has_c2 && element_combo1[c2] >= 0 && element_combo2[c2] >= 0) {
            int c2_c1 = element_combo1[c2];
            int c2_c2 = element_combo2[c2];
            if ((c2_c1 == c2_c2 && counts[c2_c1] >= 2) || (counts[c2_c1] > 0 && counts[c2_c2] > 0)) {
                *prereq_idx = c2;
                strncpy(required_elements[0], element_names[c2_c1], 20);
                required_elements[0][19] = '\0';
                strncpy(required_elements[1], element_names[c2_c2], 20);
                required_elements[1][19] = '\0';
                *required_count = 2;
                return;
            }
            // Recursive check for c2's prerequisites
            int sub_prereq_idx;
            char sub_prereq_elements[2][20] = {0};
            int sub_prereq_count = 0;
            get_prerequisite_elements(c2, chems, chem_count, &sub_prereq_idx, sub_prereq_elements, &sub_prereq_count);
            if (sub_prereq_idx >= 0) {
                *prereq_idx = sub_prereq_idx;
                strncpy(required_elements[0], sub_prereq_elements[0], 20);
                required_elements[0][19] = '\0';
                strncpy(required_elements[1], sub_prereq_elements[1], 20);
                required_elements[1][19] = '\0';
                *required_count = sub_prereq_count;
                return;
            }
        }
    }
}

void get_next_element(int current_idx, char chems[][20], int chem_count, int* next_idx, char required_elements[][20], int* required_count) {
    *next_idx = -1;
    *required_count = 0;
    // Find the next element with valid combo indices
    for (int i = current_idx + 1; i < element_count; i++) {
        if (element_combo1[i] >= 0 && element_combo2[i] >= 0) {
            *next_idx = i;
            strncpy(required_elements[0], element_names[element_combo1[i]], 20);
            required_elements[0][19] = '\0';
            strncpy(required_elements[1], element_names[element_combo2[i]], 20);
            required_elements[1][19] = '\0';
            *required_count = 2;
            break;
        }
    }
    if (*next_idx >= 0) {
        char log[MAX_LOG];
        snprintf(log, sizeof(log), "Next element suggested: %s (idx %d) requiring %s, %s", 
                 element_names[*next_idx], *next_idx, required_elements[0], required_elements[1]);
        log_message(log);
    } else {
        char log[MAX_LOG];
        snprintf(log, sizeof(log), "No further elements with valid combos after idx %d", current_idx);
        log_message(log);
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
    int len = snprintf(ptr, remaining, "To craft %s, you need: ", target_name);
    if (len >= remaining) return;
    ptr += len;
    remaining -= len;
    for (int i = 0; i < required_count && remaining > 1; i++) {
        len = snprintf(ptr, remaining, "%s%s", required_elements[i], 
                       (i < required_count - 1) ? ", " : "");
        if (len >= remaining) break;
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
        len = snprintf(ptr, remaining, " (");
        if (len >= remaining) return;
        ptr += len;
        remaining -= len;
        if (needed[0] > 0) {
            len = snprintf(ptr, remaining, "%s (%d more)", required_elements[0], needed[0]);
            if (len >= remaining) return;
            ptr += len;
            remaining -= len;
            if (needed[1] > 0) {
                len = snprintf(ptr, remaining, ", ");
                if (len >= remaining) return;
                ptr += len;
                remaining -= len;
            }
        }
        if (needed[1] > 0) {
            len = snprintf(ptr, remaining, "%s (%d more)", required_elements[1], needed[1]);
            if (len >= remaining) return;
            ptr += len;
            remaining -= len;
        }
        snprintf(ptr, remaining, ")");
    } else {
        snprintf(ptr, remaining, ". Try combining them!");
    }
}

int find_valid_combination(char chems[][20], int chem_count, int target_idx, char required_elements[][20], int* required_count) {
    int counts[MAX_ELEMENTS] = {0};
    char log[MAX_LOG];
    char inventory[MAX_INVENTORY] = {0};
    format_inventory(chems, chem_count, inventory, sizeof(inventory));
    snprintf(log, sizeof(log), "Inventory for crafting: %s", inventory);
    log_message(log);

    for (int i = 0; i < chem_count; i++) {
        int idx = get_element_index(chems[i]);
        if (idx >= 0) {
            counts[idx]++;
        }
    }

    *required_count = 0;
    if (target_idx >= 0 && element_combo1[target_idx] >= 0 && element_combo2[target_idx] >= 0) {
        int c1 = element_combo1[target_idx];
        int c2 = element_combo2[target_idx];
        strncpy(required_elements[0], element_names[c1], 20);
        required_elements[0][19] = '\0';
        strncpy(required_elements[1], element_names[c2], 20);
        required_elements[1][19] = '\0';
        *required_count = 2;
        bool can_craft = (c1 == c2) ? (counts[c1] >= 2) : (counts[c1] > 0 && counts[c2] > 0);
        if (can_craft) {
            snprintf(log, sizeof(log), "Crafting target %s with components %s and %s", 
                     element_names[target_idx], element_names[c1], element_names[c2]);
            log_message(log);
            return target_idx;
        } else {
            snprintf(log, sizeof(log), "Target %s not craftable with current inventory", element_names[target_idx]);
            log_message(log);
            return -1; // Only return target_idx if craftable, else -1 to trigger prerequisite check
        }
    }

    // Fallback to highest craftable element if no target
    int selected_idx = -1;
    for (int i = 0; i < element_count; i++) {
        if (element_combo1[i] >= 0 && element_combo2[i] >= 0 && counts[i] == 0) {
            int c1 = element_combo1[i];
            int c2 = element_combo2[i];
            bool can_craft = (c1 == c2) ? (counts[c1] >= 2) : (counts[c1] > 0 && counts[c2] > 0);
            if (can_craft && i > selected_idx) {
                selected_idx = i;
                strncpy(required_elements[0], element_names[c1], 20);
                required_elements[0][19] = '\0';
                strncpy(required_elements[1], element_names[c2], 20);
                required_elements[1][19] = '\0';
                *required_count = 2;
                snprintf(log, sizeof(log), "Found craftable %s with components %s and %s", 
                         element_names[i], element_names[c1], element_names[c2]);
                log_message(log);
            }
        }
    }

    return selected_idx;
}

void format_inventory(char chems[][20], int chem_count, char* inventory_str, size_t max_len) {
    if (chem_count == 0) {
        snprintf(inventory_str, max_len, "Your inventory is empty.");
        return;
    }
    char* ptr = inventory_str;
    size_t remaining = max_len;
    int len = snprintf(ptr, remaining, "Your inventory: ");
    if (len >= remaining) return;
    ptr += len;
    remaining -= len;
    for (int i = 0; i < chem_count && remaining > 1; i++) {
        len = snprintf(ptr, remaining, "%s%s", chems[i], (i < chem_count - 1) ? ", " : "");
        if (len >= remaining) break;
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
        char err[MAX_LOG];
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
            char log[MAX_LOG];
            snprintf(log, sizeof(log), "Read line: %s", line);
            log_message(log);
            char channel_id[32] = {0};
            char timestamp[32] = {0};
            char hash[7] = {0};
            char content[MAX_LINE] = {0};
            if (sscanf(line, "%31[^|]|%31[^|]|%6[^|]|%[^\n]", channel_id, timestamp, hash, content) != 4) {
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
            strncpy(command, content, sizeof(command));
            command[sizeof(command) - 1] = '\0';
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
                        if (uid_len >= sizeof(user_id)) uid_len = sizeof(user_id) - 1;
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
                char response[MAX_RESPONSE] = {0};
                char inventory[MAX_INVENTORY] = {0};
                char missing[MAX_MISSING] = {0};
                format_inventory(chems, chem_count, inventory, sizeof(inventory));

                int target_idx = get_element_index(target_name);
                if (target_idx >= 0) {
                    snprintf(log, sizeof(log), "Processing craft for %s (idx %d) for user %s", target_name, target_idx, user_id);
                    log_message(log);
                } else if (strlen(target_name) > 0) {
                    snprintf(response, sizeof(response), "Invalid element name: %s. %s", target_name, inventory);
                } else {
                    snprintf(response, sizeof(response), "No target element specified. Please specify an element to craft. %s", inventory);
                }

                int result = -1;
                char required_elements[2][20] = {0};
                int required_count = 0;
                if (chem_count < get_min_elements_needed()) {
                    snprintf(response, sizeof(response), "Craft failed: you need at least %d elements to craft. %s", 
                             get_min_elements_needed(), inventory);
                } else if (strlen(target_name) > 0 && target_idx >= 0) {
                    result = find_valid_combination(chems, chem_count, target_idx, required_elements, &required_count);
                    if (result >= 0) {
                        int idx1 = -1, idx2 = -1;
                        int c1 = element_combo1[result];
                        int c2 = element_combo2[result];
                        for (int i = 0; i < chem_count && (idx1 == -1 || idx2 == -1); i++) {
                            int idx = get_element_index(chems[i]);
                            if (idx == c1 && idx1 == -1) idx1 = i;
                            else if (idx == c2 && idx2 == -1) idx2 = i;
                        }
                        if (idx1 >= 0 && idx2 >= 0) {
                            update_user_chems(user_id, element_names[result], idx1, idx2, chems, chem_count);
                            // Reload inventory to reflect changes
                            chem_count = 0;
                            read_user_chems(user_id, chems, &chem_count);
                            format_inventory(chems, chem_count, inventory, sizeof(inventory));
                        }
                    }
                } else {
                    result = find_valid_combination(chems, chem_count, -1, required_elements, &required_count);
                    if (result >= 0) {
                        int idx1 = -1, idx2 = -1;
                        int c1 = element_combo1[result];
                        int c2 = element_combo2[result];
                        for (int i = 0; i < chem_count && (idx1 == -1 || idx2 == -1); i++) {
                            int idx = get_element_index(chems[i]);
                            if (idx == c1 && idx1 == -1) idx1 = i;
                            else if (idx == c2 && idx2 == -1) idx2 = i;
                        }
                        if (idx1 >= 0 && idx2 >= 0) {
                            update_user_chems(user_id, element_names[result], idx1, idx2, chems, chem_count);
                            // Reload inventory to reflect changes
                            chem_count = 0;
                            read_user_chems(user_id, chems, &chem_count);
                            format_inventory(chems, chem_count, inventory, sizeof(inventory));
                        }
                    }
                }

                char next_missing[MAX_MISSING] = {0};
                if (result >= 0) {
                    int next_idx;
                    char next_required[2][20] = {0};
                    int next_required_count = 0;
                    get_next_element(result, chems, chem_count, &next_idx, next_required, &next_required_count);
                    if (next_idx >= 0) {
                        check_missing_elements(chems, chem_count, next_required, next_required_count, next_missing, sizeof(next_missing), element_names[next_idx]);
                    } else {
                        snprintf(next_missing, sizeof(next_missing), "No further elements defined in elements.txt.");
                    }
                    snprintf(response, sizeof(response), "Success! Crafted: %s. %s %s", element_names[result], inventory, next_missing);
                } else if (target_idx >= 0 && required_count > 0) {
                    check_missing_elements(chems, chem_count, required_elements, required_count, missing, sizeof(missing), element_names[target_idx]);
                    int prereq_idx;
                    char prereq_elements[2][20] = {0};
                    int prereq_count = 0;
                    get_prerequisite_elements(target_idx, chems, chem_count, &prereq_idx, prereq_elements, &prereq_count);
                    if (prereq_idx >= 0) {
                        char prereq_missing[MAX_MISSING] = {0};
                        check_missing_elements(chems, chem_count, prereq_elements, prereq_count, prereq_missing, sizeof(prereq_missing), element_names[prereq_idx]);
                        snprintf(response, sizeof(response), "Craft failed: cannot craft %s. %s %s Try crafting %s first: %s", 
                                 element_names[target_idx], inventory, missing, element_names[prereq_idx], prereq_missing);
                    } else {
                        snprintf(response, sizeof(response), "Craft failed: cannot craft %s. %s %s", 
                                 element_names[target_idx], inventory, missing);
                    }
                } else {
                    int next_idx;
                    char next_required[2][20] = {0};
                    int next_required_count = 0;
                    get_next_element(get_highest_element_index(chems, chem_count), chems, chem_count, &next_idx, next_required, &next_required_count);
                    if (next_idx >= 0) {
                        check_missing_elements(chems, chem_count, next_required, next_required_count, missing, sizeof(missing), element_names[next_idx]);
                        snprintf(response, sizeof(response), "Craft failed: no valid combination found. %s %s", inventory, missing);
                    } else {
                        snprintf(response, sizeof(response), "Craft failed: no valid combination found. %s No further elements defined in elements.txt.", inventory);
                    }
                }

                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    int retries = LOCK_RETRIES;
                    while (retries > 0 && flock(fileno(queue_fp), LOCK_EX | LOCK_NB) == -1) {
                        retries--;
                        usleep(LOCK_TIMEOUT_US);
                        char log[MAX_LOG];
                        snprintf(log, sizeof(log), "Retrying lock on %s (%d retries left)", SEND_QUEUE_FILE, retries);
                        log_message(log);
                    }
                    if (retries == 0) {
                        char err[MAX_LOG];
                        snprintf(err, sizeof(err), "Failed to lock %s after %d retries: %s", SEND_QUEUE_FILE, LOCK_RETRIES, strerror(errno));
                        log_message(err);
                        fclose(queue_fp);
                        continue;
                    }
                    fprintf(queue_fp, "%s|%s", channel_id, response);
                    if (flock(fileno(queue_fp), LOCK_UN) == -1) {
                        char err[MAX_LOG];
                        snprintf(err, sizeof(err), "Failed to unlock %s: %s", SEND_QUEUE_FILE, strerror(errno));
                        log_message(err);
                    }
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
