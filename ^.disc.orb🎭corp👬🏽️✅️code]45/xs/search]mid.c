#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define HASH_SIZE 1000
#define MAX_LINE 1024
#define DOMAIN_LIST "dns_domains.txt"
#define OUTPUT_FILE "send_queue.txt"
#define LAST_POS_FILE "buy_last_position.txt"

unsigned int hashes[HASH_SIZE];
int hash_count = 0;

void log_message(const char* msg) {
    FILE* f = fopen("main_log.txt", "a");
    if (f) {
        fprintf(f, "[%ld] %s\n", time(NULL), msg);
        fclose(f);
    }
    printf("[%ld] %s\n", time(NULL), msg);
}

int hash_exists(unsigned int hash) {
    for (int i = 0; i < hash_count; i++) {
        if (hashes[i] == hash) return 1;
    }
    return 0;
}

int save_hash(unsigned int hash) {
    if (hash_count >= HASH_SIZE) {
        log_message("Max hashes reached, cannot save new hash");
        return 0;
    }
    hashes[hash_count++] = hash;
    char log[256];
    snprintf(log, sizeof(log), "Saved hash: %u", hash);
    log_message(log);
    return 1;
}

void save_last_position(long pos) {
    FILE* f = fopen(LAST_POS_FILE, "w");
    if (f) {
        fprintf(f, "%ld\n", pos);
        fclose(f);
        char log[256];
        snprintf(log, sizeof(log), "Saved last position: %ld to %s", pos, LAST_POS_FILE);
        log_message(log);
    }
}

long read_last_position() {
    FILE* f = fopen(LAST_POS_FILE, "r");
    long pos = 0;
    if (f) {
        fscanf(f, "%ld", &pos);
        fclose(f);
    }
    return pos;
}

void execute_search(const char* channel, const char* keyword) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./+x/13.hybrid.+pagerank.+x %s txt", keyword);
    char log[256];
    snprintf(log, sizeof(log), "Executing search command: %s", cmd);
    log_message(log);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        log_message("Failed to execute search command");
        return;
    }

    char output[4096] = "";
    char line[1024];
    while (fgets(line, sizeof(line), pipe)) {
        strncat(output, line, sizeof(output) - strlen(output) - 1);
    }
    int status = pclose(pipe);
    if (status != 0) {
        snprintf(log, sizeof(log), "Search command failed with status %d", status);
        log_message(log);
        return;
    }

    if (strlen(output) == 0) {
        log_message("No output from search command");
        return;
    }

    FILE* f = fopen(OUTPUT_FILE, "a");
    if (f) {
        fprintf(f, "%s|Top 5 results for '%s':\n%s\n", channel, keyword, output);
        fclose(f);
        snprintf(log, sizeof(log), "Wrote search results to %s", OUTPUT_FILE);
        log_message(log);
    } else {
        log_message("Failed to write to send_queue.txt");
    }
}

int main() {
    FILE* f = fopen("events.txt", "r");
    if (!f) {
        log_message("Failed to open events.txt");
        return 1;
    }

    long last_pos = read_last_position();
    fseek(f, last_pos, SEEK_SET);

    char line[MAX_LINE];
    while (1) {
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = 0;
            char log[256];
            snprintf(log, sizeof(log), "Read line: %s", line);
            log_message(log);

            char channel[32], timestamp[32], hash[16], content[256];
            if (sscanf(line, "%31[^|]|%31[^|]|%15[^|]|%255[^\n]", channel, timestamp, hash, content) == 4) {
                snprintf(log, sizeof(log), "Parsed: channel=%s, timestamp=%s, hash=%s, content=%s", channel, timestamp, hash, content);
                log_message(log);

                unsigned int h = atoi(hash);
                if (!hash_exists(h)) {
                    save_hash(h);
                    if (strncmp(content, "!search ", 8) == 0) {
                        char* keyword = content + 8;
                        char* space = strchr(keyword, ' ');
                        if (space) *space = '\0'; // Remove any trailing args like "txt"
                        if (strlen(keyword) > 0) {
                            snprintf(log, sizeof(log), "No user ID prefix, using raw command: %s", content);
                            log_message(log);
                            execute_search(channel, keyword);
                        } else {
                            log_message("Empty search keyword");
                        }
                    }
                } else {
                    snprintf(log, sizeof(log), "Hash %s already processed", hash);
                    log_message(log);
                }
            }
            save_last_position(ftell(f));
        } else {
            sleep(1);
            clearerr(f);
        }
    }
    fclose(f);
    return 0;
}
