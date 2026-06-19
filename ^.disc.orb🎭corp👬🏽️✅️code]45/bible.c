#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "bible_processed_hashes.txt"
#define LOG_FILE "bible_log.txt"
#define BIBLE_FILE "../!.bible.DeathNote.c]+]a1+b3/bible.txt" //../
#define MAX_LINE 4096
#define MAX_HASHES 1000

// From 0.bible.rnd.conv]GRAIL]📔️🏆️]a1.c
#define BIBLE_BEGIN 3000
#define BIBLE_END 100109
#define LINE_SIZE 256

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "[%ld] %s\n", time(NULL), msg);
        fclose(f);
    }
    printf("[%ld] %s\n", time(NULL), msg);
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
    FILE* fp = fopen(PROCESSED_HASHES_FILE, "r");
    if (fp) {
        char line[8];
        while (fgets(line, sizeof(line), fp) && *hash_count < MAX_HASHES) {
            line[strcspn(line, "\n")] = 0;
            strcpy(hashes[*hash_count], line);
            (*hash_count)++;
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

void get_random_verse(char* verse, size_t len) {
    srand(time(NULL));
    int rand_line = (rand() % (BIBLE_END + 1 - BIBLE_BEGIN)) + BIBLE_BEGIN;

    FILE *bible = fopen(BIBLE_FILE, "r");
    if (!bible) {
        snprintf(verse, len, "Error opening bible file.");
        log_message("Error opening bible file.");
        return;
    }

    if (fseek(bible, 0, SEEK_SET) != 0) {
        snprintf(verse, len, "Error seeking in bible file.");
        log_message("Error seeking in bible file.");
        fclose(bible);
        return;
    }

    for (int i = 0; i < rand_line - 1; ++i) {
        char buffer[LINE_SIZE];
        if (!fgets(buffer, LINE_SIZE, bible)) {
            snprintf(verse, len, "Error reading bible file to get to verse.");
            log_message("Error reading bible file to get to verse.");
            fclose(bible);
            return;
        }
    }

    char line[LINE_SIZE];
    if (fgets(line, LINE_SIZE, bible)) {
        // Remove newline character from the verse
        line[strcspn(line, "\n")] = 0;

        // Find the first space to remove the leading number
        char* verse_start = strchr(line, ' ');
        if (verse_start) {
            // Move the verse to the beginning of the string
            memmove(line, verse_start + 1, strlen(verse_start));
        }

        strncpy(verse, line, len);
    } else {
        snprintf(verse, len, "Could not read the chosen verse.");
        log_message("Could not read the chosen verse.");
    }

    fclose(bible);
}


int main() {
    log_message("Starting bible.+x");
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
    fseek(event_fp, 0, SEEK_END);
    long last_pos = ftell(event_fp);

    while (1) {
        fseek(event_fp, last_pos, SEEK_SET);
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), event_fp)) {
            last_pos = ftell(event_fp);
            line[strcspn(line, "\n")] = 0;
            char log[MAX_LINE + 64];
            snprintf(log, sizeof(log), "Read line: %s", line);
            log_message(log);

            char* channel_id = strtok(line, "|");
            char* timestamp = strtok(NULL, "|");
            char* hash = strtok(NULL, "|");
            char* content = strtok(NULL, "");

            if (channel_id == NULL || timestamp == NULL || hash == NULL || content == NULL) {
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

            if (strcmp(content, "!bible") == 0) {
                char verse[LINE_SIZE];
                get_random_verse(verse, sizeof(verse));

                FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    fprintf(queue_fp, "%s|%s\n", channel_id, verse);
                    fclose(queue_fp);
                    snprintf(log, sizeof(log), "Wrote to %s: %s|%s", SEND_QUEUE_FILE, channel_id, verse);
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
    log_message("Exiting bible.+x");
    return 0;
}
