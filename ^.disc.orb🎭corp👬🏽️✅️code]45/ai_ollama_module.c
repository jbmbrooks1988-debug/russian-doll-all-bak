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
#define PROCESSED_HASHES_FILE "ai_ollama_hashes.txt"
#define LOG_FILE "ai_ollama_log.txt"
#define OLLAMA_RESPONSE_FILE "ollama_response.txt"
#define LOCK_FILE "ai_ollama_module.lock"
#define MAX_LINE 4096
#define MAX_HASHES 1000
#define MAX_RESPONSE_SIZE 2048
#define DISCORD_MAX_MESSAGE 2000
#define OLLAMA_TIMEOUT_SECONDS 30

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

// Simple JSON parser to extract the "response" field
int extract_response(const char* json, char* response, size_t max_len) {
    char log[256];
    snprintf(log, sizeof(log), "Raw JSON: %.100s...", json);
    log_message(log);

    const char* response_start = strstr(json, "\"response\":\"");
    if (!response_start) {
        log_message("No 'response' field found in JSON");
        return 0;
    }
    response_start += 12; // Skip "\"response\":\""
    const char* response_end = strstr(response_start, "\"");
    if (!response_end) {
        log_message("Invalid JSON: 'response' field not terminated");
        return 0;
    }
    size_t response_len = response_end - response_start;
    if (response_len >= max_len) {
        response_len = max_len - 1;
        log_message("Response truncated to fit max length");
    }
    strncpy(response, response_start, response_len);
    response[response_len] = '\0';

    // Unescape special characters and clean for Discord
    char temp[MAX_RESPONSE_SIZE];
    size_t j = 0;
    for (size_t i = 0; response[i] && j < max_len - 1; i++) {
        if (response[i] == '\\' && response[i + 1] != '\0') {
            if (response[i + 1] == 'n') {
                temp[j++] = ' '; // Replace \n with space
                i++;
            } else if (response[i + 1] == 't') {
                temp[j++] = ' '; // Replace \t with space
                i++;
            } else if (response[i + 1] == '"' || response[i + 1] == '\\' || response[i + 1] == 'u') {
                temp[j++] = response[i + 1] == 'u' ? ' ' : response[i + 1]; // Skip \u (e.g., \u003e)
                i++;
            } else {
                temp[j++] = response[i];
            }
        } else if (response[i] == '\n' || response[i] == '\r') {
            temp[j++] = ' '; // Replace raw newlines
        } else {
            temp[j++] = response[i];
        }
    }
    temp[j] = '\0';
    strncpy(response, temp, max_len);
    snprintf(log, sizeof(log), "Parsed response: %.100s...", response);
    log_message(log);
    return 1;
}

// Send a message to send_queue.txt and execute send.+x
int send_message(const char* channel_id, const char* message) {
    char log[256];
    // Truncate message to Discord limit
    char truncated_message[DISCORD_MAX_MESSAGE];
    strncpy(truncated_message, message, DISCORD_MAX_MESSAGE - 1);
    truncated_message[DISCORD_MAX_MESSAGE - 1] = '\0';

    FILE* queue_fp = fopen(SEND_QUEUE_FILE, "w");
    if (queue_fp) {
        flock(fileno(queue_fp), LOCK_EX);
        fprintf(queue_fp, "%s|%s\n", channel_id, truncated_message);
        flock(fileno(queue_fp), LOCK_UN);
        fclose(queue_fp);
        snprintf(log, sizeof(log), "Wrote to %s: %s|%.100s...", SEND_QUEUE_FILE, channel_id, truncated_message);
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
            // Clear send_queue.txt
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
    // Check for single instance using lock file
    int lock_fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0666);
    if (lock_fd < 0) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open lock file %s: %s", LOCK_FILE, strerror(errno));
        log_message(err);
        exit(1);
    }
    if (flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
        log_message("Another instance of ai_ollama_module.+x is already running");
        close(lock_fd);
        exit(1);
    }

    log_message("Starting ai_ollama_module.+x");
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
    long last_pos = 0;
    while (1) {
        flock(fileno(event_fp), LOCK_SH);
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
            if (strncmp(content, "!ai ", 4) == 0) {
                char* ai_message = content + 4; // Skip "!ai "
                if (strlen(ai_message) > 0) {
                    // Send confirmation message
                    log_message("Sending confirmation message for !ai command");
                    if (!send_message(channel_id, "Processing your !ai request, please wait...")) {
                        log_message("Failed to send confirmation message");
                        continue;
                    }

                    // Escape quotes in the message
                    char escaped_message[2048];
                    size_t j = 0;
                    for (size_t i = 0; ai_message[i] && j < sizeof(escaped_message) - 2; i++) {
                        if (ai_message[i] == '"' || ai_message[i] == '\\') {
                            escaped_message[j++] = '\\';
                        }
                        escaped_message[j++] = ai_message[i];
                    }
                    escaped_message[j] = '\0';

                    // Check message length to avoid truncation
                    if (strlen(escaped_message) > 490) {
                        log_message("AI message too long, truncating to 490 characters");
                        escaped_message[490] = '\0';
                    }

                    // Check if ai_ollama.+x exists and is executable
                    struct stat st;
                    if (stat("./+x/ai_ollama.+x", &st) != 0 || !(st.st_mode & S_IXUSR)) {
                        snprintf(log, sizeof(log), "ai_ollama.+x not found or not executable");
                        log_message(log);
                        send_message(channel_id, "Error: AI module not available.");
                        continue;
                    }

                    // Execute ai_ollama.+x
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "./+x/ai_ollama.+x \"%s\"", escaped_message);
                    log_message(cmd);
                    int ret = system(cmd);
                    if (ret != 0) {
                        snprintf(log, sizeof(log), "ai_ollama.+x failed with return code %d", ret);
                        log_message(log);
                        send_message(channel_id, "Error: Failed to process AI request.");
                        continue;
                    }

                    // Wait for ollama_response.txt to be written
                    time_t start_time = time(NULL);
                    int file_exists = 0;
                    while (time(NULL) - start_time < OLLAMA_TIMEOUT_SECONDS) {
                        if (access(OLLAMA_RESPONSE_FILE, F_OK) == 0) {
                            struct stat st;
                            stat(OLLAMA_RESPONSE_FILE, &st);
                            if (st.st_size > 0) {
                                file_exists = 1;
                                break;
                            }
                        }
                        sleep(1);
                    }

                    if (!file_exists) {
                        log_message("Timeout waiting for ollama_response.txt");
                        send_message(channel_id, "Error: AI response timed out.");
                        continue;
                    }

                    // Read response from ollama_response.txt
                    char ollama_response[MAX_RESPONSE_SIZE] = "No response from Ollama";
                    FILE* response_fp = fopen(OLLAMA_RESPONSE_FILE, "r");
                    if (response_fp) {
                        flock(fileno(response_fp), LOCK_SH);
                        size_t bytes_read = fread(ollama_response, 1, MAX_RESPONSE_SIZE - 1, response_fp);
                        ollama_response[bytes_read] = '\0';
                        flock(fileno(response_fp), LOCK_UN);
                        fclose(response_fp);
                        snprintf(log, sizeof(log), "Read %zu bytes from %s", bytes_read, OLLAMA_RESPONSE_FILE);
                        log_message(log);
                    } else {
                        snprintf(log, sizeof(log), "Failed to read %s: %s", OLLAMA_RESPONSE_FILE, strerror(errno));
                        log_message(log);
                        send_message(channel_id, "Error: Failed to read AI response.");
                        continue;
                    }

                    // Parse JSON to extract response field
                    char parsed_response[MAX_RESPONSE_SIZE] = "Failed to parse Ollama response";
                    if (extract_response(ollama_response, parsed_response, MAX_RESPONSE_SIZE)) {
                        // Check Discord message length limit
                        if (strlen(parsed_response) > DISCORD_MAX_MESSAGE) {
                            parsed_response[DISCORD_MAX_MESSAGE - 1] = '\0';
                            log_message("Response truncated to fit Discord message limit");
                        }
                    } else {
                        snprintf(parsed_response, MAX_RESPONSE_SIZE, "Error: Invalid response format from Ollama");
                    }

                    // Send the final response
                    send_message(channel_id, parsed_response);
                } else {
                    snprintf(log, sizeof(log), "No message provided after !ai command");
                    log_message(log);
                    send_message(channel_id, "Error: No message provided after !ai command.");
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
    log_message("Exiting ai_ollama_module.+x");
    return 0;
}
