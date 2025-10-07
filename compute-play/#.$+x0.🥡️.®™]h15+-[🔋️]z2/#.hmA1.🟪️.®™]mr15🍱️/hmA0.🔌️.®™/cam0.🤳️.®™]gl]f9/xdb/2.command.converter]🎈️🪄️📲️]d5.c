#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_LINE 256
#define INPUT_FILE "commands_gl.txt"
#define OUTPUT_FILE "commands.txt"
#define STATE_FILE "processed_lines.txt"
#define DEBUG_LOG "convert_debug.log"

void debug_log(const char *msg) {
    FILE *fp = fopen(DEBUG_LOG, "a");
    if (fp) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(fp, "[%s] %s\n", timestamp, msg);
        fflush(fp);
        fclose(fp);
    }
}

int main() {
    FILE *input_fp, *output_fp, *state_fp;
    char line[MAX_LINE];
    int processed_lines = 0;
    int current_lines;
    char timestamp[64];
    char debug_msg[512];

    debug_log("Program started");

    // Load previously processed line count
    state_fp = fopen(STATE_FILE, "r");
    if (state_fp) {
        fscanf(state_fp, "%d", &processed_lines);
        snprintf(debug_msg, sizeof(debug_msg), "Read %d processed lines from %s", processed_lines, STATE_FILE);
        debug_log(debug_msg);
        fclose(state_fp);
    } else {
        snprintf(debug_msg, sizeof(debug_msg), "Failed to open %s for reading", STATE_FILE);
        debug_log(debug_msg);
    }

    while (1) {
        // Count current lines in input file
        current_lines = 0;
        input_fp = fopen(INPUT_FILE, "r");
        if (!input_fp) {
            snprintf(debug_msg, sizeof(debug_msg), "Failed to open %s for reading", INPUT_FILE);
            debug_log(debug_msg);
            sleep(1);
            continue;
        }
        while (fgets(line, MAX_LINE, input_fp)) {
            current_lines++;
        }
        fclose(input_fp);
        snprintf(debug_msg, sizeof(debug_msg), "Counted %d lines in %s", current_lines, INPUT_FILE);
        debug_log(debug_msg);

        // Process new lines
        if (current_lines > processed_lines) {
            input_fp = fopen(INPUT_FILE, "r");
            if (!input_fp) {
                snprintf(debug_msg, sizeof(debug_msg), "Failed to open %s for processing", INPUT_FILE);
                debug_log(debug_msg);
                sleep(1);
                continue;
            }

            output_fp = fopen(OUTPUT_FILE, "a");
            if (!output_fp) {
                snprintf(debug_msg, sizeof(debug_msg), "Failed to open %s for appending", OUTPUT_FILE);
                debug_log(debug_msg);
                fclose(input_fp);
                sleep(1);
                continue;
            }

            // Skip already processed lines
            int current_line = 0;
            while (fgets(line, MAX_LINE, input_fp)) {
                current_line++;
                if (current_line <= processed_lines) continue;

                // Remove newline
                line[strcspn(line, "\n")] = '\0';
                snprintf(debug_msg, sizeof(debug_msg), "Processing line %d: %s", current_line, line);
                debug_log(debug_msg);

                // Parse input line
                char device[32], button[32], state[32];
                int x, y, key;
                if (sscanf(line, "[%[^]]] %31[^:]: button=%31[^,], state=%31[^,], x=%d, y=%d", timestamp, device, button, state, &x, &y) == 6) {
                    if (strcmp(device, "Mouse") == 0) {
                        if (strcmp(state, "Up") == 0) {
                            fprintf(output_fp, "13\n");
                            snprintf(debug_msg, sizeof(debug_msg), "Wrote '13' to %s for Mouse Up event", OUTPUT_FILE);
                            debug_log(debug_msg);
                        }
                    }
                } else if (sscanf(line, "[%[^]]] %31[^:]: key=%d, x=%d, y=%d", timestamp, device, &key, &x, &y) == 5) {
                    if (strcmp(device, "Keyboard") == 0) {
                        fprintf(output_fp, "%d\n", key);
                        snprintf(debug_msg, sizeof(debug_msg), "Wrote key '%d' to %s for Keyboard event", key, OUTPUT_FILE);
                        debug_log(debug_msg);
                    }
                } else if (sscanf(line, "[%[^]]] %31[^:]: key=%31[^,], x=%d, y=%d", timestamp, device, button, &x, &y) == 5) {
                    if (strcmp(device, "SpecialKeyboard") == 0 && strcmp(button, "UnknownSpecial") == 0) {
                        fprintf(output_fp, "3\n");
                        snprintf(debug_msg, sizeof(debug_msg), "Wrote '3' to %s for SpecialKeyboard event", OUTPUT_FILE);
                        debug_log(debug_msg);
                    }
                } else {
                    snprintf(debug_msg, sizeof(debug_msg), "Failed to parse line %d: %s", current_line, line);
                    debug_log(debug_msg);
                }
            }
            fclose(input_fp);
            fclose(output_fp);
            processed_lines = current_lines;

            // Save processed line count
            state_fp = fopen(STATE_FILE, "w");
            if (state_fp) {
                fprintf(state_fp, "%d", processed_lines);
                snprintf(debug_msg, sizeof(debug_msg), "Wrote %d processed lines to %s", processed_lines, STATE_FILE);
                debug_log(debug_msg);
                fclose(state_fp);
            } else {
                snprintf(debug_msg, sizeof(debug_msg), "Failed to open %s for writing", STATE_FILE);
                debug_log(debug_msg);
            }
        } else {
            snprintf(debug_msg, sizeof(debug_msg), "No new lines to process (current: %d, processed: %d)", current_lines, processed_lines);
            debug_log(debug_msg);
        }
        sleep(1); // Wait before checking for new input
    }
    debug_log("Program exiting");
    return 0;
}
