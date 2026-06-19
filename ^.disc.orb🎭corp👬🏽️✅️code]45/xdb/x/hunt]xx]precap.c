#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "hunt_processed_hashes.txt"
#define LOG_FILE "hunt_log.txt"
#define DATA_DIR "data"
#define ENEMY_LIST_FILE "data/enemy_list.txt"
#define MAX_LINE 4096
#define MAX_HASHES 1000
#define MAX_STATE_SIZE 4096
#define MAX_ENEMIES 10
#define MAX_ALLIES 10
#define DAY_SECONDS 86400

typedef struct {
    char name[32];
    int level;
    int xp;
    int hp;
    int max_hp;
    int mp;
    int attack;
    int defense;
    int magic_attack;
    int magic_defense;
    int agility;
    int luck;
} Fighter;

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
    if (is_hash_processed(hash, hashes, hash_count)) {
        char log[256];
        snprintf(log, sizeof(log), "Hash %s already exists, skipping save", hash);
        log_message(log);
        return;
    }
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
                if (!is_hash_processed(line, hashes, hash_count)) {
                    strcpy(hashes[*hash_count], line);
                    (*hash_count)++;
                }
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
    if (stat(DATA_DIR, &st) == -1) {
        if (mkdir(DATA_DIR, 0700) == -1) {
            char err[256];
            snprintf(err, sizeof(err), "Failed to create %s: %s", DATA_DIR, strerror(errno));
            log_message(err);
        } else {
            log_message("Created data directory");
        }
    }
}

void ensure_user_dir(const char* user_id) {
    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s/%s", DATA_DIR, user_id);
    struct stat st = {0};
    if (stat(user_dir, &st) == -1) {
        if (mkdir(user_dir, 0700) == -1) {
            char err[256];
            snprintf(err, sizeof(err), "Failed to create %s: %s", user_dir, strerror(errno));
            log_message(err);
        } else {
            char log[256];
            snprintf(log, sizeof(log), "Created user directory %s", user_dir);
            log_message(log);
        }
    }
}

void load_enemy_list(char enemy_names[][32], int* enemy_count) {
    *enemy_count = 0;
    FILE* fp = fopen(ENEMY_LIST_FILE, "r");
    if (fp) {
        char line[64];
        while (fgets(line, sizeof(line), fp) && *enemy_count < MAX_ENEMIES) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0) {
                strncpy(enemy_names[*enemy_count], line, 31);
                enemy_names[*enemy_count][31] = '\0';
                (*enemy_count)++;
            }
        }
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Loaded %d enemies from %s", *enemy_count, ENEMY_LIST_FILE);
        log_message(log);
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open %s: %s", ENEMY_LIST_FILE, strerror(errno));
        log_message(err);
    }
}

void generate_enemy(Fighter* enemy, int user_level, char enemy_names[][32], int enemy_count) {
    int r = rand() % 100;
    int level;
    if (r < 5) {
        level = user_level + (rand() % 5 + 5);
    } else {
        level = user_level + (rand() % 5 - 2);
        if (level < 1) level = 1;
    }
    int idx = rand() % enemy_count;
    strncpy(enemy->name, enemy_names[idx], 31);
    enemy->name[31] = '\0';
    enemy->level = level;
    int hp_val = level * 10 + (rand() % 20);
    enemy->max_hp = hp_val;
    enemy->hp = hp_val;
    enemy->mp = level * 5 + (rand() % 10);
    enemy->attack = level * 3 + (rand() % 5);
    enemy->defense = level * 2 + (rand() % 5);
    enemy->magic_attack = level * 2 + (rand() % 5);
    enemy->magic_defense = level * 2 + (rand() % 5);
    enemy->agility = level * 2 + (rand() % 5);
    enemy->luck = level + (rand() % 5);
}

void update_user_state(const char* user_id, Fighter* user, int next_hunt_time, Fighter allies[], int ally_count) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s.hunt.txt", DATA_DIR, user_id);
    FILE* fp = fopen(filepath, "w");
    if (fp) {
        fprintf(fp, "user:%s\n", user->name);
        fprintf(fp, "LVL:%d\n", user->level);
        fprintf(fp, "XP:%d\n", user->xp);
        fprintf(fp, "HP:%d\n", user->hp);
        fprintf(fp, "MaxHP:%d\n", user->max_hp);
        fprintf(fp, "MP:%d\n", user->mp);
        fprintf(fp, "Attack:%d\n", user->attack);
        fprintf(fp, "Defense:%d\n", user->defense);
        fprintf(fp, "Magic Attack:%d\n", user->magic_attack);
        fprintf(fp, "Magic Defense:%d\n", user->magic_defense);
        fprintf(fp, "Agility:%d\n", user->agility);
        fprintf(fp, "Luck:%d\n", user->luck);
        fprintf(fp, "NextHuntTime:%d\n", next_hunt_time);
        for (int i = 0; i < ally_count; i++) {
            fprintf(fp, "Ally:%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                    allies[i].name, allies[i].level, allies[i].hp, allies[i].max_hp, allies[i].mp,
                    allies[i].attack, allies[i].defense, allies[i].magic_attack, allies[i].magic_defense, allies[i].agility, allies[i].luck);
        }
        fflush(fp);
        fclose(fp);
        char log[256];
        snprintf(log, sizeof(log), "Updated state for user %s: level=%d, xp=%d, hp=%d, allies=%d", user_id, user->level, user->xp, user->hp, ally_count);
        log_message(log);
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Failed to write state for user %s: %s", user_id, strerror(errno));
        log_message(err);
    }
}

void read_user_state(const char* user_id, Fighter* user, int* next_hunt_time, Fighter allies[], int* ally_count) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s.hunt.txt", DATA_DIR, user_id);
    FILE* fp = fopen(filepath, "r");
    *ally_count = 0;
    user->xp = 0;
    *next_hunt_time = 0;
    user->max_hp = 0;
    user->hp = 0;
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = 0;
            if (strncmp(line, "user:", 5) == 0) {
                strncpy(user->name, line + 6, 31);
                user->name[31] = '\0';
            } else if (strncmp(line, "LVL:", 4) == 0) {
                sscanf(line, "LVL:%d", &user->level);
            } else if (strncmp(line, "XP:", 3) == 0) {
                sscanf(line, "XP:%d", &user->xp);
            } else if (strncmp(line, "HP:", 3) == 0) {
                sscanf(line, "HP:%d", &user->hp);
            } else if (strncmp(line, "MaxHP:", 6) == 0) {
                sscanf(line, "MaxHP:%d", &user->max_hp);
            } else if (strncmp(line, "MP:", 3) == 0) {
                sscanf(line, "MP:%d", &user->mp);
            } else if (strncmp(line, "Attack:", 7) == 0) {
                sscanf(line, "Attack:%d", &user->attack);
            } else if (strncmp(line, "Defense:", 8) == 0) {
                sscanf(line, "Defense:%d", &user->defense);
            } else if (strncmp(line, "Magic Attack:", 12) == 0) {
                sscanf(line, "Magic Attack:%d", &user->magic_attack);
            } else if (strncmp(line, "Magic Defense:", 13) == 0) {
                sscanf(line, "Magic Defense:%d", &user->magic_defense);
            } else if (strncmp(line, "Agility:", 8) == 0) {
                sscanf(line, "Agility:%d", &user->agility);
            } else if (strncmp(line, "Luck:", 5) == 0) {
                sscanf(line, "Luck:%d", &user->luck);
            } else if (strncmp(line, "NextHuntTime:", 13) == 0) {
                sscanf(line, "NextHuntTime:%d", next_hunt_time);
            } else if (strncmp(line, "Ally:", 5) == 0 && *ally_count < MAX_ALLIES) {
                Fighter* ally = &allies[*ally_count];
                sscanf(line, "Ally:%31[^,],%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", ally->name, &ally->level, &ally->hp, &ally->max_hp, &ally->mp, &ally->attack, &ally->defense, &ally->magic_attack, &ally->magic_defense, &ally->agility, &ally->luck);
                (*ally_count)++;
            }
        }
        fclose(fp);
        if (user->max_hp == 0) {
            user->max_hp = user->level * 10 + 20;
            user->hp = user->max_hp;
        }
        time_t now = time(NULL);
        if (*next_hunt_time <= now && user->hp < user->max_hp) {
            int hours_elapsed = (now - *next_hunt_time) / 3600;
            if (hours_elapsed > 0) {
                int recovery = (int)(user->max_hp * 0.1 * hours_elapsed);
                user->hp += recovery;
                if (user->hp > user->max_hp) user->hp = user->max_hp;
                char log[256];
                snprintf(log, sizeof(log), "Recovered %d HP for user %s (now HP:%d/%d)", recovery, user_id, user->hp, user->max_hp);
                log_message(log);
                for (int i = 0; i < *ally_count; i++) {
                    if (allies[i].hp < allies[i].max_hp) {
                        int ally_recovery = (int)(allies[i].max_hp * 0.1 * hours_elapsed);
                        allies[i].hp += ally_recovery;
                        if (allies[i].hp > allies[i].max_hp) allies[i].hp = allies[i].max_hp;
                        snprintf(log, sizeof(log), "Recovered %d HP for ally %s (now HP:%d/%d)", ally_recovery, allies[i].name, allies[i].hp, allies[i].max_hp);
                        log_message(log);
                    }
                }
                *next_hunt_time = now;
                update_user_state(user_id, user, *next_hunt_time, allies, *ally_count);
            }
        }
        char log[256];
        snprintf(log, sizeof(log), "Read state for user %s: level=%d, xp=%d, hp=%d", user_id, user->level, user->xp, user->hp);
        log_message(log);
    } else {
        user->level = (rand() % 10) + 1;
        int hp_val = user->level * 10 + 20;
        user->max_hp = hp_val;
        user->hp = hp_val;
        user->mp = user->level * 5 + 10;
        user->attack = user->level * 3 + 5;
        user->defense = user->level * 2 + 5;
        user->magic_attack = user->level * 2 + 5;
        user->magic_defense = user->level * 2 + 5;
        user->agility = user->level * 2 + 5;
        user->luck = user->level + 5;
        *next_hunt_time = time(NULL);
        strncpy(user->name, user_id, 31);
        user->name[31] = '\0';
        update_user_state(user_id, user, *next_hunt_time, allies, *ally_count);
        char log[256];
        snprintf(log, sizeof(log), "Created new state for user %s", user_id);
        log_message(log);
    }
}

int calculate_xp_for_level(int level) {
    return (int)(100 * pow(1.5, level - 1));
}

void battle_round(Fighter* attacker, Fighter* defender, char* battle_log, int* log_offset, int round) {
    int damage = 0;
    int hit_chance = 50 + (attacker->agility - defender->agility) + (attacker->luck - defender->luck);
    if (rand() % 100 < hit_chance) {
        if (rand() % 100 < 50) {
            damage = attacker->attack - (defender->defense / 2);
            if (damage < 0) damage = 0;
            *log_offset += snprintf(battle_log + *log_offset, MAX_STATE_SIZE - *log_offset,
                                   "Round %d: %s hits %s for %d damage (Physical)\n",
                                   round, attacker->name, defender->name, damage);
        } else {
            damage = attacker->magic_attack - (defender->magic_defense / 2);
            if (damage < 0) damage = 0;
            *log_offset += snprintf(battle_log + *log_offset, MAX_STATE_SIZE - *log_offset,
                                   "Round %d: %s hits %s for %d damage (Magic)\n",
                                   round, attacker->name, defender->name, damage);
        }
        defender->hp -= damage;
        if (defender->hp < 0) defender->hp = 0;
    } else {
        *log_offset += snprintf(battle_log + *log_offset, MAX_STATE_SIZE - *log_offset,
                               "Round %d: %s misses %s\n",
                               round, attacker->name, defender->name);
    }
}

void simulate_battle(Fighter* user, Fighter allies[], int ally_count, Fighter enemies[], int enemy_count_in_battle, char* battle_log, int* outcome, int* xp_gained, Fighter* captured, int* captured_flag, int rounds) {
    int log_offset = 0;
    Fighter* user_team[MAX_ALLIES + 1];
    int user_team_size = 1 + ally_count;
    user_team[0] = user;
    for (int i = 0; i < ally_count; i++) {
        user_team[i + 1] = &allies[i];
    }
    *outcome = 0;
    *xp_gained = 0;
    *captured_flag = 0;

    log_offset += snprintf(battle_log + log_offset, MAX_STATE_SIZE - log_offset,
                          "Battle begins! %s's team (size %d) vs %d enemies\n",
                          user->name, user_team_size, enemy_count_in_battle);

    for (int round = 1; round <= rounds && *outcome == 0; round++) {
        for (int i = 0; i < user_team_size && *outcome == 0; i++) {
            if (user_team[i]->hp <= 0) continue;
            int target = rand() % enemy_count_in_battle;
            int loop_count = 0;
            while (enemies[target].hp <= 0 && loop_count < enemy_count_in_battle) {
                target = (target + 1) % enemy_count_in_battle;
                loop_count++;
            }
            if (loop_count >= enemy_count_in_battle) {
                *outcome = 1;
                break;
            }
            battle_round(user_team[i], &enemies[target], battle_log, &log_offset, round);
        }
        for (int i = 0; i < enemy_count_in_battle && *outcome == 0; i++) {
            if (enemies[i].hp <= 0) continue;
            int target = rand() % user_team_size;
            int loop_count = 0;
            while (user_team[target]->hp <= 0 && loop_count < user_team_size) {
                target = (target + 1) % user_team_size;
                loop_count++;
            }
            if (loop_count >= user_team_size) {
                *outcome = -1;
                break;
            }
            battle_round(&enemies[i], user_team[target], battle_log, &log_offset, round);
        }
    }

    if (*outcome == 0) {
        int user_team_alive = 0, enemy_team_alive = 0;
        for (int i = 0; i < user_team_size; i++) {
            if (user_team[i]->hp > 0) user_team_alive = 1;
        }
        for (int i = 0; i < enemy_count_in_battle; i++) {
            if (enemies[i].hp > 0) enemy_team_alive = 1;
        }
        if (!user_team_alive) *outcome = -1;
        else if (!enemy_team_alive) *outcome = 1;
    }

    if (*outcome == 1) {
        *xp_gained = 0;
        for (int i = 0; i < enemy_count_in_battle; i++) {
            *xp_gained += enemies[i].level * 10 + (rand() % 10);
        }
        log_offset += snprintf(battle_log + log_offset, MAX_STATE_SIZE - log_offset,
                              "%s's team wins! Gained %d XP\n", user->name, *xp_gained);
        for (int i = 0; i < enemy_count_in_battle; i++) {
            if (enemies[i].hp > 0 && enemies[i].hp < 0.1 * enemies[i].max_hp && (rand() % 100) < 10) {
                *captured_flag = 1;
                *captured = enemies[i];
                log_offset += snprintf(battle_log + log_offset, MAX_STATE_SIZE - log_offset,
                                      "Captured %s (Level %d)!\n", enemies[i].name, enemies[i].level);
                break;
            }
        }
    } else if (*outcome == -1) {
        log_offset += snprintf(battle_log + log_offset, MAX_STATE_SIZE - log_offset,
                              "%s's team defeated!\n", user->name);
    } else {
        log_offset += snprintf(battle_log + log_offset, MAX_STATE_SIZE - log_offset,
                              "No winner, battle ends in a draw.\n");
    }
}

int main() {
    log_message("Starting hunt.+x");
    srand(time(NULL));
    ensure_data_dir();
    char hashes[MAX_HASHES][7] = {0};
    int hash_count = 0;
    load_processed_hashes(hashes, &hash_count);
    char enemy_names[MAX_ENEMIES][32] = {0};
    int enemy_count = 0;
    load_enemy_list(enemy_names, &enemy_count);
    if (enemy_count == 0) {
        log_message("No enemies loaded, exiting");
        exit(1);
    }
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

            int rounds = 10 + (rand() % 91); // Default rounds
            if (strncmp(content, "!hunt", 5) == 0) {
                char* args = content + 5;
                while (*args == ' ') args++; // Skip spaces after !hunt
                if (*args != '\0' && strncmp(args, "<@", 2) == 0) {
                    char* rounds_str = strchr(args, ' ');
                    if (rounds_str) {
                        while (*rounds_str == ' ') rounds_str++; // Skip spaces before rounds
                        if (*rounds_str != '\0') {
                            int user_rounds;
                            if (sscanf(rounds_str, "%d", &user_rounds) == 1) {
                                if (user_rounds >= 10 && user_rounds <= 100) {
                                    rounds = user_rounds;
                                    snprintf(log, sizeof(log), "User specified %d rounds for hunt", rounds);
                                    log_message(log);
                                } else {
                                    snprintf(log, sizeof(log), "Invalid rounds %d, using default", user_rounds);
                                    log_message(log);
                                }
                            }
                        }
                    }
                }
                if (strlen(user_id) == 0) {
                    snprintf(log, sizeof(log), "No valid user ID found in content: %s", content);
                    log_message(log);
                    FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                    if (queue_fp) {
                        fprintf(queue_fp, "%s|Error: Invalid user ID format. Please use !hunt @username [rounds]", channel_id);
                        fflush(queue_fp);
                        fclose(queue_fp);
                        snprintf(log, sizeof(log), "Wrote to %s: Error response for invalid user ID", SEND_QUEUE_FILE);
                        log_message(log);
                    } else {
                        snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                        log_message(log);
                    }
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "./+x/send.+x > send_output.log 2>&1");
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
                ensure_user_dir(user_id);
                Fighter user = {0};
                int next_hunt_time = 0;
                Fighter allies[MAX_ALLIES] = {0};
                int ally_count = 0;
                read_user_state(user_id, &user, &next_hunt_time, allies, &ally_count);
                time_t now = time(NULL);
                if (next_hunt_time > now) {
                    int hours_left = (next_hunt_time - now + 3600 - 1) / 3600;
                    char response[256] = {0};
                    snprintf(response, sizeof(response), "User %s cannot hunt yet. Try again in %d hour(s). HP:%d/%d", user_id, hours_left, user.hp, user.max_hp);
                    FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                    if (queue_fp) {
                        fprintf(queue_fp, "%s|%s", channel_id, response);
                        fflush(queue_fp);
                        fclose(queue_fp);
                        snprintf(log, sizeof(log), "Wrote to %s: %s|%s", SEND_QUEUE_FILE, channel_id, response);
                        log_message(log);
                    } else {
                        snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                        log_message(log);
                    }
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "./+x/send.+x > send_output.log 2>&1");
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
                if (user.hp <= 0) {
                    char response[256] = {0};
                    snprintf(response, sizeof(response), "User %s is defeated (HP:0/%d). Recover HP to hunt again.", user_id, user.max_hp);
                    FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                    if (queue_fp) {
                        fprintf(queue_fp, "%s|%s", channel_id, response);
                        fflush(queue_fp);
                        fclose(queue_fp);
                        snprintf(log, sizeof(log), "Wrote to %s: %s|%s", SEND_QUEUE_FILE, channel_id, response);
                        log_message(log);
                    } else {
                        snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                        log_message(log);
                    }
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "./+x/send.+x > send_output.log 2>&1");
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
                int enemy_count_in_battle = 1 + (rand() % (ally_count + 1));
                Fighter enemies[MAX_ENEMIES] = {0};
                for (int i = 0; i < enemy_count_in_battle; i++) {
                    generate_enemy(&enemies[i], user.level, enemy_names, enemy_count);
                }
                char battle_log[MAX_STATE_SIZE] = {0};
                int outcome = 0;
                int xp_gained = 0;
                Fighter captured = {0};
                int captured_flag = 0;
                simulate_battle(&user, allies, ally_count, enemies, enemy_count_in_battle, battle_log, &outcome, &xp_gained, &captured, &captured_flag, rounds);
                int battle_end_hp = user.hp;
                int ally_end_hp[MAX_ALLIES] = {0};
                for (int i = 0; i < ally_count; i++) {
                    ally_end_hp[i] = allies[i].hp;
                }
                char response[256] = {0};
                int leveled_up = 0;
                if (outcome == 1) {
                    user.xp += xp_gained;
                    int next_level_xp = calculate_xp_for_level(user.level + 1);
                    while (user.xp >= next_level_xp) {
                        user.level++;
                        int hp_val = user.level * 10 + 20;
                        user.max_hp = hp_val;
                        user.hp = hp_val;
                        user.mp = user.level * 5 + 10;
                        user.attack = user.level * 3 + 5;
                        user.defense = user.level * 2 + 5;
                        user.magic_attack = user.level * 2 + 5;
                        user.magic_defense = user.level * 2 + 5;
                        user.agility = user.level * 2 + 5;
                        user.luck = user.level + 5;
                        next_level_xp = calculate_xp_for_level(user.level + 1);
                        leveled_up = 1;
                        for (int i = 0; i < ally_count; i++) {
                            allies[i].max_hp = allies[i].level * 10 + 20;
                            allies[i].hp = allies[i].max_hp;
                        }
                    }
                    snprintf(response, sizeof(response), "Won XP:%d%s LVL:%d Enemies:%d XP:%d/%d", 
                             xp_gained, leveled_up ? " LeveledUp" : "", user.level, enemy_count_in_battle, user.xp, next_level_xp);
                    if (captured_flag && ally_count < MAX_ALLIES) {
                        allies[ally_count] = captured;
                        ally_count++;
                        snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                                 " Captured:%s-LVL:%d", captured.name, captured.level);
                    }
                    if (ally_count > 0) {
                        snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                                 " Allies:%d", ally_count);
                    }
                    snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                             " Vs:");
                    for (int i = 0; i < enemy_count_in_battle && strlen(response) < 230; i++) {
                        snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                                 "%s%s-LVL:%d", i > 0 ? "," : "", enemies[i].name, enemies[i].level);
                    }
                    snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                             " HP:%d/%d", battle_end_hp, user.max_hp);
                    next_hunt_time = now;
                } else if (outcome == -1) {
                    user.hp = 0;
                    for (int i = 0; i < ally_count; i++) {
                        allies[i].hp = 0;
                    }
                    int total_enemy_hp = 0;
                    int living_enemies = 0;
                    for (int i = 0; i < enemy_count_in_battle; i++) {
                        if (enemies[i].hp > 0) {
                            total_enemy_hp += enemies[i].hp;
                            living_enemies++;
                        }
                    }
                    int avg_enemy_hp = living_enemies > 0 ? total_enemy_hp / living_enemies : 0;
                    snprintf(response, sizeof(response), "Lost LVL:%d Enemies:%d XP:%d/%d Cooldown:%d AvgEnemyHP:%d", 
                             user.level, enemy_count_in_battle, user.xp, calculate_xp_for_level(user.level + 1), 
                             (next_hunt_time - now + 3600 - 1) / 3600, avg_enemy_hp);
                    if (ally_count > 0) {
                        snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                                 " Allies:%d", ally_count);
                    }
                    snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                             " Vs:");
                    for (int i = 0; i < enemy_count_in_battle && strlen(response) < 230; i++) {
                        snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                                 "%s%s-LVL:%d", i > 0 ? "," : "", enemies[i].name, enemies[i].level);
                    }
                    snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                             " HP:%d/%d", user.hp, user.max_hp);
                    next_hunt_time = now + DAY_SECONDS;
                    user.hp = user.max_hp;
                    for (int i = 0; i < ally_count; i++) {
                        allies[i].hp = allies[i].max_hp;
                    }
                } else {
                    snprintf(response, sizeof(response), "Draw LVL:%d Enemies:%d XP:%d/%d", 
                             user.level, enemy_count_in_battle, user.xp, calculate_xp_for_level(user.level + 1));
                    if (ally_count > 0) {
                        snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                                 " Allies:%d", ally_count);
                    }
                    snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                             " Vs:");
                    for (int i = 0; i < enemy_count_in_battle && strlen(response) < 230; i++) {
                        snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                                 "%s%s-LVL:%d", i > 0 ? "," : "", enemies[i].name, enemies[i].level);
                    }
                    snprintf(response + strlen(response), sizeof(response) - strlen(response), 
                             " HP:%d/%d", battle_end_hp, user.max_hp);
                    next_hunt_time = now;
                }
                update_user_state(user_id, &user, next_hunt_time, allies, ally_count);
                char battle_filepath[256];
                snprintf(battle_filepath, sizeof(battle_filepath), "%s/%s/battle_%ld.txt", DATA_DIR, user_id, now);
                FILE* battle_fp = fopen(battle_filepath, "w");
                if (battle_fp) {
                    fprintf(battle_fp, "%s", battle_log);
                    fflush(battle_fp);
                    fclose(battle_fp);
                    char log[256];
                    snprintf(log, sizeof(log), "Wrote full battle log to %s", battle_filepath);
                    log_message(log);
                } else {
                    char err[256];
                    snprintf(err, sizeof(err), "Failed to write battle log: %s", strerror(errno));
                    log_message(err);
                }
                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    fprintf(queue_fp, "%s|%s", channel_id, response);
                    fflush(queue_fp);
                    fclose(queue_fp);
                    snprintf(log, sizeof(log), "Wrote to %s: %s|%s", SEND_QUEUE_FILE, channel_id, response);
                    log_message(log);
                } else {
                    snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
                    log_message(log);
                }
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "./+x/send.+x > send_output.log 2>&1");
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
    log_message("Exiting hunt.+x");
    return 0;
}
