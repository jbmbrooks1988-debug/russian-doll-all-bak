#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "llama2_hashes.txt"
#define LOG_FILE "llama2_module_log.txt"
#define LOCK_FILE "llama2_module.lock"
#define MAX_LINE 4096
#define MAX_HASHES 1000

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) {
        flock(fileno(f), LOCK_EX);
        fprintf(f, "[%ld] %s\n", time(NULL), msg);
        flock(fileno(f), LOCK_UN);
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
            flock(fileno(fp), LOCK_EX);
            fprintf(fp, "%s\n", hash);
            flock(fileno(fp), LOCK_UN);
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
        flock(fileno(fp), LOCK_SH);
        char line[8];
        while (fgets(line, sizeof(line), fp) && *hash_count < MAX_HASHES) {
            line[strcspn(line, "\n")] = 0;
            strcpy(hashes[*hash_count], line);
            (*hash_count)++;
        }
        flock(fileno(fp), LOCK_UN);
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

int send_message(const char* channel_id, const char* message) {
    char log[256];
    FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
    if (queue_fp) {
        flock(fileno(queue_fp), LOCK_EX);
        fprintf(queue_fp, "%s|%s\n", channel_id, message);
        flock(fileno(queue_fp), LOCK_UN);
        fclose(queue_fp);
        snprintf(log, sizeof(log), "Wrote to %s: %s|%.100s...", SEND_QUEUE_FILE, channel_id, message);
        log_message(log);
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "./+x/send.+x");
        log_message(cmd);
        int ret = system(cmd);
        if (ret != 0) {
            snprintf(log, sizeof(log), "send.+x failed with return code %d", ret);
            log_message(log);
            return 0;
        } else {
            log_message("send.+x executed successfully");
            queue_fp = fopen(SEND_QUEUE_FILE, "w");
            if (queue_fp) {
                flock(fileno(queue_fp), LOCK_EX);
                fclose(queue_fp);
                flock(fileno(queue_fp), LOCK_UN);
            }
            return 1;
        }
    } else {
        snprintf(log, sizeof(log), "Failed to open %s: %s", SEND_QUEUE_FILE, strerror(errno));
        log_message(log);
        return 0;
    }
}

int main() {
    int lock_fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0666);
    if (lock_fd < 0) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open lock file %s: %s", LOCK_FILE, strerror(errno));
        log_message(err);
        exit(1);
    }
    if (flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
        log_message("Another instance of llama2_module.+x is already running");
        close(lock_fd);
        exit(1);
    }

    log_message("Starting llama2_module.+x");
    char hashes[MAX_HASHES][7] = {0};
    int hash_count = 0;
    load_processed_hashes(hashes, &hash_count);
    FILE* event_fp = fopen(EVENTS_FILE, "r");
    if (!event_fp) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open %s: %s", EVENTS_FILE, strerror(errno));
        log_message(err);
        close(lock_fd);
        exit(1);
    }
    log_message("Opened events.txt successfully");
    fseek(event_fp, 0, SEEK_END);
    long last_pos = ftell(event_fp);

    while (1) {
        flock(fileno(event_fp), LOCK_SH);
        fseek(event_fp, last_pos, SEEK_SET);
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), event_fp)) {
            last_pos = ftell(event_fp);
            line[strcspn(line, "\n")] = 0;
            char log_buf[MAX_LINE + 64];
            snprintf(log_buf, sizeof(log_buf), "Read line: %s", line);
            log_message(log_buf);

            char* channel_id = strtok(line, "|");
            char* timestamp = strtok(NULL, "|");
            char* hash = strtok(NULL, "|");
            char* content = strtok(NULL, "");

            if (channel_id == NULL || timestamp == NULL || hash == NULL || content == NULL) {
                snprintf(log_buf, sizeof(log_buf), "Invalid line format: %s", line);
                log_message(log_buf);
                continue;
            }

            snprintf(log_buf, sizeof(log_buf), "Parsed: channel=%s, timestamp=%s, hash=%s, content=%.100s", channel_id, timestamp, hash, content);
            log_message(log_buf);

            if (is_hash_processed(hash, hashes, &hash_count)) {
                snprintf(log_buf, sizeof(log_buf), "Hash %s already processed, skipping", hash);
                log_message(log_buf);
                continue;
            }
            save_hash(hash, hashes, &hash_count);

            if (strncmp(content, "!llama2 ", 8) == 0) {
                char* llama2_message = content + 8;
                if (strlen(llama2_message) > 0) {
                    send_message(channel_id, "Processing your !llama2 request, please wait...");
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg), "Processing !llama2 command with message: %s", llama2_message);
                    log_message(log_msg);

                    char escaped_message[2048];
                    size_t j = 0;
                    for (size_t i = 0; llama2_message[i] && j < sizeof(escaped_message) - 2; i++) {
                        if (llama2_message[i] == '"' || llama2_message[i] == '\\') {
                            escaped_message[j++] = '\\';
                        }
                        escaped_message[j++] = llama2_message[i];
                    }
                    escaped_message[j] = '\0';

                    char cmd[4096];
                    snprintf(cmd, sizeof(cmd), "./+x/llama2.+x %s \"%s\"", channel_id, escaped_message);
                    log_message(cmd);
                    int ret = system(cmd);
                    if (ret != 0) {
                        snprintf(log_msg, sizeof(log_msg), "llama2.+x failed with return code %d", ret);
                        log_message(log_msg);
                        send_message(channel_id, "Error: Failed to process Llama2 request.");
                    } else {
                        log_message("llama2.+x executed successfully");
                    }
                } else {
                    send_message(channel_id, "Error: No message provided after !llama2 command.");
                }
            }
        }
        flock(fileno(event_fp), LOCK_UN);
        if (feof(event_fp)) {
            clearerr(event_fp);
        }
        sleep(1);
    }
    fclose(event_fp);
    close(lock_fd);
    log_message("Exiting llama2_module.+x");
    return 0;
}
