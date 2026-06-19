#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define EVENTS_FILE "events.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define PROCESSED_HASHES_FILE "processed_hashes.txt"
#define LOG_FILE "search_log.txt"
#define MAX_LINE 4096
#define MAX_HASHES 5000
#define MAX_KEYWORD 256
#define MAX_PATH 256
#define MAX_SUMMARY 2048
#define MAX_RESULTS 5

void log_message(const char *msg) {
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "[%ld] %s\n", time(NULL), msg);
        fflush(f);
        fclose(f);
    }
    printf("[%ld] %s\n", time(NULL), msg);
}

unsigned int simple_hash(const char *str) {
    unsigned int hash = 0;
    for (int i = 0; str[i]; i++) {
        hash = hash * 31 + str[i];
    }
    return hash % 1000000;
}

int is_hash_processed(const char *hash, char hashes[][7], int *hash_count) {
    for (int i = 0; i < *hash_count; i++) {
        if (strcmp(hashes[i], hash) == 0) {
            return 1;
        }
    }
    return 0;
}

void save_hash(const char *hash, char hashes[][7], int *hash_count) {
    if (*hash_count < MAX_HASHES) {
        strcpy(hashes[*hash_count], hash);
        (*hash_count)++;
        FILE *fp = fopen(PROCESSED_HASHES_FILE, "a");
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

void load_processed_hashes(char hashes[][7], int *hash_count) {
    *hash_count = 0;
    FILE *fp = fopen(PROCESSED_HASHES_FILE, "r");
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

// Sanitize keyword for file paths and command execution
void sanitize_keyword(const char *keyword, char *safe_keyword, size_t max_len) {
    if (!keyword || !safe_keyword) return;
    strncpy(safe_keyword, keyword, max_len - 1);
    safe_keyword[max_len - 1] = '\0';
    // Remove trailing "txt" if present
    char *txt_suffix = strstr(safe_keyword, " txt");
    if (txt_suffix) *txt_suffix = '\0';
    // Replace spaces with underscores for file paths
    for (int i = 0; safe_keyword[i]; i++) {
        if (safe_keyword[i] == ' ') {
            safe_keyword[i] = '_';
        }
    }
}

// Read top 5 results and summaries
int read_results(const char *keyword, char results[][MAX_LINE], char summaries[][MAX_SUMMARY], int *result_count) {
    char safe_keyword[MAX_KEYWORD];
    sanitize_keyword(keyword, safe_keyword, MAX_KEYWORD);
    char summary_path[MAX_PATH];
    snprintf(summary_path, MAX_PATH, "text-dl/%s_summary.txt", safe_keyword);
    char log[256];

    // Read dns_results.txt for results
    FILE *dns_fp = fopen("dns_results.txt", "r");
    if (!dns_fp) {
        snprintf(log, sizeof(log), "Failed to open dns_results.txt: %s", strerror(errno));
        log_message(log);
        return 0;
    }

    *result_count = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), dns_fp) && *result_count < MAX_RESULTS) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        strncpy(results[*result_count], line, MAX_LINE - 1);
        results[*result_count][MAX_LINE - 1] = '\0';
        summaries[*result_count][0] = '\0'; // Initialize summary
        (*result_count)++;
    }
    fclose(dns_fp);
    snprintf(log, sizeof(log), "Read %d results from dns_results.txt", *result_count);
    log_message(log);

    // Try to read summary file for context
    FILE *summary_fp = fopen(summary_path, "r");
    if (summary_fp) {
        char summary_content[MAX_SUMMARY] = {0};
        size_t bytes_read = fread(summary_content, 1, MAX_SUMMARY - 1, summary_fp);
        summary_content[bytes_read] = '\0';
        fclose(summary_fp);
        // Split summary content into lines and assign to corresponding results
        char *line_ptr = summary_content;
        int current_result = 0;
        char *next_line;
        while (current_result < *result_count && (next_line = strchr(line_ptr, '\n'))) {
            *next_line = '\0';
            if (strlen(line_ptr) > 0) {
                strncpy(summaries[current_result], line_ptr, MAX_SUMMARY - 1);
                summaries[current_result][MAX_SUMMARY - 1] = '\0';
                current_result++;
            }
            line_ptr = next_line + 1;
        }
        // If there's remaining content and we have room
        if (strlen(line_ptr) > 0 && current_result < *result_count) {
            strncpy(summaries[current_result], line_ptr, MAX_SUMMARY - 1);
            summaries[current_result][MAX_SUMMARY - 1] = '\0';
        }
        snprintf(log, sizeof(log), "Read summaries from %s", summary_path);
        log_message(log);
    } else {
        snprintf(log, sizeof(log), "No summary file found at %s", summary_path);
        log_message(log);
        // Fall back to extracting text from dns_results.txt
        for (int i = 0; i < *result_count; i++) {
            char *text_start = strstr(results[i], "Text: ");
            if (text_start) {
                text_start += 6;
                strncpy(summaries[i], text_start, MAX_SUMMARY - 1);
                summaries[i][MAX_SUMMARY - 1] = '\0';
                // Truncate at first newline or limit to 100 chars
                char *newline = strchr(summaries[i], '\n');
                if (newline) *newline = '\0';
                if (strlen(summaries[i]) > 100) {
                    summaries[i][100] = '\0';
                    strcat(summaries[i], "...");
                }
            }
        }
    }

    return *result_count;
}

int main() {
    log_message("Starting search.+x");
    char hashes[MAX_HASHES][7] = {0};
    int hash_count = 0;
    load_processed_hashes(hashes, &hash_count);
    FILE *event_fp = fopen(EVENTS_FILE, "r");
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

            // Check for !search command
            char *cmd_start = strstr(content, "> ");
            char *cmd = cmd_start ? cmd_start + 2 : content;
            char user_id[32] = {0};
            if (cmd_start && strncmp(content, "<@", 2) == 0) {
                char *user_id_start = content + 2;
                char *user_id_end = strchr(user_id_start, '>');
                if (user_id_end) {
                    int user_id_len = user_id_end - user_id_start;
                    if (user_id_len < sizeof(user_id)) {
                        strncpy(user_id, user_id_start, user_id_len);
                        user_id[user_id_len] = '\0';
                    }
                }
                snprintf(log, sizeof(log), "Extracted user_id: %s, command: %s", user_id[0] ? user_id : "none", cmd);
                log_message(log);
            } else {
                snprintf(log, sizeof(log), "No user ID prefix, using raw command: %s", cmd);
                log_message(log);
            }

            if (strncmp(cmd, "!search ", 8) == 0) {
                char keyword[MAX_KEYWORD];
                strncpy(keyword, cmd + 8, MAX_KEYWORD - 1);
                keyword[MAX_KEYWORD - 1] = '\0';
                // Trim trailing whitespace and "txt" suffix
                int len = strlen(keyword);
                while (len > 0 && (keyword[len - 1] == ' ' || keyword[len - 1] == '\n')) {
                    keyword[--len] = '\0';
                }
                char *txt_suffix = strstr(keyword, " txt");
                if (txt_suffix) *txt_suffix = '\0';
                if (len == 0 || strlen(keyword) == 0) {
                    snprintf(log, sizeof(log), "Empty keyword in !search command");
                    log_message(log);
                    continue;
                }
                snprintf(log, sizeof(log), "Executing search for keyword: %s", keyword);
                log_message(log);

                // Execute 13.hybrid.+pagerank.+x
                char exec_cmd[512];
                snprintf(exec_cmd, sizeof(exec_cmd), "./+x/13.hybrid.+pagerank.+x \"%s\" txt", keyword);
                snprintf(log, sizeof(log), "Executing: %s", exec_cmd);
                log_message(log);
                int ret = system(exec_cmd);
                if (ret != 0) {
                    snprintf(log, sizeof(log), "13.hybrid.+pagerank.+x failed with return code %d: %s", ret, strerror(errno));
                    log_message(log);
                    FILE *queue_fp = fopen(SEND_QUEUE_FILE, "w");
                    if (queue_fp) {
                        fprintf(queue_fp, "%s|Search failed for '%s', please try again.\n", channel_id, keyword);
                        fclose(queue_fp);
                        snprintf(log, sizeof(log), "Wrote error to %s: %s|Search failed", SEND_QUEUE_FILE, channel_id);
                        log_message(log);
                        snprintf(exec_cmd, sizeof(exec_cmd), "./+x/send.+x");
                        snprintf(log, sizeof(log), "Executing: %s", exec_cmd);
                        log_message(log);
                        ret = system(exec_cmd);
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
                    continue;
                }
                snprintf(log, sizeof(log), "13.hybrid.+pagerank.+x executed successfully");
                log_message(log);

                // Read results and summaries
                char results[MAX_RESULTS][MAX_LINE] = {0};
                char summaries[MAX_RESULTS][MAX_SUMMARY] = {0};
                int result_count = 0;
                read_results(keyword, results, summaries, &result_count);

                // Write to send_queue.txt
                FILE *queue_fp = fopen(SEND_QUEUE_FILE, "w");
                if (queue_fp) {
                    if (result_count == 0) {
                        fprintf(queue_fp, "%s|No results found for '%s'.\n", channel_id, keyword);
                        snprintf(log, sizeof(log), "No results found for '%s'", keyword);
                        log_message(log);
                    } else {
                        fprintf(queue_fp, "%s|Top %d results for '%s':\n", channel_id, result_count, keyword);
                        for (int i = 0; i < result_count; i++) {
                            char *identifier = results[i];
                            char *source_start = strstr(results[i], "Source: ");
                            if (source_start) {
                                *source_start = '\0'; // Truncate to get identifier
                            }
                            fprintf(queue_fp, "[%d] %s\n", i + 1, identifier);
                            if (summaries[i][0]) {
                                fprintf(queue_fp, "%s\n\n", summaries[i]);
                            } else {
                                fprintf(queue_fp, "No summary available.\n\n");
                            }
                        }
                        snprintf(log, sizeof(log), "Wrote %d results to %s", result_count, SEND_QUEUE_FILE);
                        log_message(log);
                    }
                    fclose(queue_fp);
                    snprintf(exec_cmd, sizeof(exec_cmd), "./+x/send.+x");
                    snprintf(log, sizeof(log), "Executing: %s", exec_cmd);
                    log_message(log);
                    ret = system(exec_cmd);
                    if (ret != 0) {
                        snprintf(log, sizeof(log), "send.+x failed with return code %d", ret);
                        log_message(log);
                    } else {
                        snprintf(log, sizeof(log), "send.+x executed successfully");
                        log_message(log);
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
    log_message("Exiting search.+x");
    return 0;
}
