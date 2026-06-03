
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define MAX_LINE 256
#define MAX_PATH 256
#define MAX_BLOCKS 10000
#define FILE_CHECK_INTERVAL 100000 // 100ms in microseconds

// Global variables
float player_x = 0.0f, player_y = 0.0f, player_z = 0.0f;
float cursor_x = 0.0f, cursor_y = 0.0f, cursor_z = 0.0f;
float *block_x = NULL, *block_y = NULL, *block_z = NULL;
int num_blocks = 0;
int block_capacity = 0;
char state_path[MAX_PATH] = "state.txt";
char primitive_path[MAX_PATH] = "primitive.txt";
char log_path[MAX_PATH] = "log.txt";
time_t last_state_time = 0;
struct timespec last_check_time = {0, 0};

void log_to_file(const char *message) {
    FILE *fp = fopen(log_path, "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s\n", timestamp, message);
    fclose(fp);
}

void read_locations() {
    FILE *fp = fopen("locations.txt", "r");
    if (!fp) {
        log_to_file("Warning: Failed to open locations.txt, using default paths");
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_PATH], value[MAX_PATH];
        if (sscanf(line, "%s %s", key, value) != 2) continue;
        if (strcmp(key, "state") == 0) {
            strncpy(state_path, value, MAX_PATH - 1);
            state_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "primitive") == 0) {
            strncpy(primitive_path, value, MAX_PATH - 1);
            primitive_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "log") == 0) {
            strncpy(log_path, value, MAX_PATH - 1);
            log_path[MAX_PATH - 1] = '\0';
        }
    }
    fclose(fp);
    char msg[MAX_PATH * 3];
    snprintf(msg, sizeof(msg), "Loaded paths: state=%s, primitive=%s, log=%s", state_path, primitive_path, log_path);
    log_to_file(msg);
}

void resize_blocks() {
    int new_capacity = block_capacity == 0 ? 1000 : block_capacity * 2;
    if (new_capacity > MAX_BLOCKS) new_capacity = MAX_BLOCKS;
    float *new_x = realloc(block_x, new_capacity * sizeof(float));
    float *new_y = realloc(block_y, new_capacity * sizeof(float));
    float *new_z = realloc(block_z, new_capacity * sizeof(float));
    if (!new_x || !new_y || !new_z) {
        log_to_file("Error: Memory allocation failed in resize_blocks");
        exit(1);
    }
    block_x = new_x;
    block_y = new_y;
    block_z = new_z;
    block_capacity = new_capacity;
}

void read_state() {
    num_blocks = 0;
    FILE *fp = fopen(state_path, "r");
    if (!fp) {
        log_to_file("Warning: Failed to open state.txt");
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[16];
        float value;
        if (sscanf(line, "%15s %f", key, &value) == 2) {
            if (strcmp(key, "x") == 0) player_x = value;
            else if (strcmp(key, "y") == 0) player_y = value;
            else if (strcmp(key, "z") == 0) player_z = value;
            else if (strcmp(key, "cursor_x") == 0) cursor_x = value;
            else if (strcmp(key, "cursor_y") == 0) cursor_y = value;
            else if (strcmp(key, "cursor_z") == 0) cursor_z = value;
        }
    }
    fclose(fp);

    // Example: Add blocks (e.g., from map file or static maze)
    if (num_blocks >= block_capacity) {
        resize_blocks();
    }
    block_x[num_blocks] = 0.5f;
    block_y[num_blocks] = 0.5f;
    block_z[num_blocks] = 0.5f;
    num_blocks++;
    // Add more blocks as needed (e.g., parse map file)

    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded state: player=(%.1f, %.1f, %.1f), cursor=(%.1f, %.1f, %.1f), %d blocks",
             player_x, player_y, player_z, cursor_x, cursor_y, cursor_z, num_blocks);
    log_to_file(msg);
}

void write_primitive() {
    FILE *fp = fopen(primitive_path, "w");
    if (!fp) {
        log_to_file("Warning: Failed to open primitive.txt for writing");
        return;
    }
    fprintf(fp, "primitive sphere %.1f %.1f %.1f 0.0 0.0 1.0 1.0 type=player\n", player_x, player_y, player_z);
    fprintf(fp, "primitive cube %.1f %.1f %.1f 1.0 1.0 0.0 0.3 type=cursor\n", cursor_x, cursor_y, cursor_z);
    for (int i = 0; i < num_blocks; i++) {
        fprintf(fp, "primitive cube %.1f %.1f %.1f 0.5 0.5 0.5 0.3 type=block\n", block_x[i], block_y[i], block_z[i]);
    }
    fprintf(fp, "text Player: (%.1f, %.1f, %.1f)\n", player_x, player_y, player_z);
    fprintf(fp, "text Cursor: (%.1f, %.1f, %.1f)\n", cursor_x, cursor_y, cursor_z);
    fclose(fp);
    log_to_file("Wrote primitive.txt");
}

int check_file_changes() {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    long elapsed_ns = (current_time.tv_sec - last_check_time.tv_sec) * 1000000000L +
                      (current_time.tv_nsec - last_check_time.tv_nsec);
    if (elapsed_ns < FILE_CHECK_INTERVAL * 1000) {
        return 0;
    }
    last_check_time = current_time;

    struct stat stat_buf;
    if (stat(state_path, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_state_time) {
            last_state_time = stat_buf.st_mtime;
            log_to_file("State file changed, reloading");
            return 1;
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat state file %s", state_path);
        log_to_file(msg);
    }
    return 0;
}

int main() {
    read_locations();
    read_state();
    write_primitive();

    while (1) {
        if (check_file_changes()) {
            read_state();
            write_primitive();
        }
        usleep(10000); // 10ms sleep to avoid CPU overload
    }

    // Cleanup (unreachable)
    free(block_x);
    free(block_y);
    free(block_z);
    return 0;
}

