#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define MAX_LINE 256
#define MAX_PATH 256
#define MAX_PRIMITIVES 10000
#define MAX_TEXTS 100
#define FILE_CHECK_INTERVAL 100000 // 100ms in microseconds

// Global variables
char (*shape)[16] = NULL;
float *x = NULL, *y = NULL, *z = NULL;
float *r = NULL, *g = NULL, *b = NULL, *a = NULL;
char (*type)[16] = NULL;
int num_primitives = 0;
int primitive_capacity = 0;
char (*text_lines)[256] = NULL;
int num_texts = 0;
int text_capacity = 0;
char primitive_path[MAX_PATH] = "./primitive.txt";
char output_path[MAX_PATH] = "rgb.txt";
char log_path[MAX_PATH] = "log.txt";
time_t last_primitive_time = 0;
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
        if (strcmp(key, "primitive") == 0) {
            strncpy(primitive_path, value, MAX_PATH - 1);
            primitive_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "rgb") == 0) {
            strncpy(output_path, value, MAX_PATH - 1);
            output_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "log") == 0) {
            strncpy(log_path, value, MAX_PATH - 1);
            log_path[MAX_PATH - 1] = '\0';
        }
    }
    fclose(fp);
    char msg[MAX_PATH * 3];
    snprintf(msg, sizeof(msg), "Loaded paths: primitive=%s, rgb=%s, log=%s", primitive_path, output_path, log_path);
    log_to_file(msg);
}

void resize_arrays() {
    int new_capacity = primitive_capacity == 0 ? 1000 : primitive_capacity * 2;
    if (new_capacity > MAX_PRIMITIVES) new_capacity = MAX_PRIMITIVES;
    char (*new_shape)[16] = realloc(shape, new_capacity * sizeof(char[16]));
    float *new_x = realloc(x, new_capacity * sizeof(float));
    float *new_y = realloc(y, new_capacity * sizeof(float));
    float *new_z = realloc(z, new_capacity * sizeof(float));
    float *new_r = realloc(r, new_capacity * sizeof(float));
    float *new_g = realloc(g, new_capacity * sizeof(float));
    float *new_b = realloc(b, new_capacity * sizeof(float));
    float *new_a = realloc(a, new_capacity * sizeof(float));
    char (*new_type)[16] = realloc(type, new_capacity * sizeof(char[16]));

    if (!new_shape || !new_x || !new_y || !new_z || !new_r || !new_g || !new_b || !new_a || !new_type) {
        log_to_file("Error: Memory allocation failed in resize_arrays");
        exit(1);
    }

    shape = new_shape;
    x = new_x;
    y = new_y;
    z = new_z;
    r = new_r;
    g = new_g;
    b = new_b;
    a = new_a;
    type = new_type;
    primitive_capacity = new_capacity;
}

void resize_text_arrays() {
    int new_capacity = text_capacity == 0 ? 100 : text_capacity * 2;
    if (new_capacity > MAX_TEXTS) new_capacity = MAX_TEXTS;
    char (*new_texts)[256] = realloc(text_lines, new_capacity * sizeof(char[256]));
    if (!new_texts) {
        log_to_file("Error: Memory allocation failed in resize_text_arrays");
        exit(1);
    }
    text_lines = new_texts;
    text_capacity = new_capacity;
}

void read_primitive() {
    num_primitives = 0;
    num_texts = 0;

    FILE *fp = fopen(primitive_path, "r");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open primitive file %s", primitive_path);
        log_to_file(msg);
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_LINE];
        if (sscanf(line, "%s", key) != 1) continue;
        if (strcmp(key, "primitive") == 0) {
            char temp_shape[16], temp_type[16];
            float temp_x, temp_y, temp_z, temp_r, temp_g, temp_b, temp_a;
            if (sscanf(line, "primitive %15s %f %f %f %f %f %f %f type=%15s",
                       temp_shape, &temp_x, &temp_y, &temp_z, &temp_r, &temp_g, &temp_b, &temp_a, temp_type) == 9) {
                if (num_primitives < MAX_PRIMITIVES) {
                    if (num_primitives >= primitive_capacity) {
                        resize_arrays();
                    }
                    strncpy(shape[num_primitives], temp_shape, 15);
                    shape[num_primitives][15] = '\0';
                    x[num_primitives] = temp_x;
                    y[num_primitives] = temp_y;
                    z[num_primitives] = temp_z;
                    r[num_primitives] = temp_r;
                    g[num_primitives] = temp_g;
                    b[num_primitives] = temp_b;
                    a[num_primitives] = temp_a;
                    strncpy(type[num_primitives], temp_type, 15);
                    type[num_primitives][15] = '\0';
                    num_primitives++;
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Loaded primitive: %s at (%.1f, %.1f, %.1f), color=(%.1f, %.1f, %.1f, %.1f), type=%s",
                             temp_shape, temp_x, temp_y, temp_z, temp_r, temp_g, temp_b, temp_a, temp_type);
                    log_to_file(msg);
                } else {
                    log_to_file("Warning: MAX_PRIMITIVES reached, ignoring primitive");
                }
            }
        } else if (strcmp(key, "text") == 0) {
            char text_content[256];
            if (sscanf(line, "text %255[^\n]", text_content) == 1) {
                if (num_texts < MAX_TEXTS) {
                    if (num_texts >= text_capacity) {
                        resize_text_arrays();
                    }
                    strncpy(text_lines[num_texts], text_content, 255);
                    text_lines[num_texts][255] = '\0';
                    num_texts++;
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Loaded text: %s", text_content);
                    log_to_file(msg);
                } else {
                    log_to_file("Warning: MAX_TEXTS reached, ignoring text");
                }
            }
        }
    }
    fclose(fp);

    // Write to rgb.txt
    FILE *out_fp = fopen(output_path, "w");
    if (!out_fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open output file %s", output_path);
        log_to_file(msg);
        return;
    }
    for (int i = 0; i < num_primitives; i++) {
        fprintf(out_fp, "primitive %s %.1f %.1f %.1f %.1f %.1f %.1f %.1f type=%s\n",
                shape[i], x[i], y[i], z[i], r[i], g[i], b[i], a[i], type[i]);
    }
    for (int i = 0; i < num_texts; i++) {
        fprintf(out_fp, "text %s\n", text_lines[i]);
    }
    fclose(out_fp);

    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded %d primitives and %d texts from %s, wrote to %s",
             num_primitives, num_texts, primitive_path, output_path);
    log_to_file(msg);
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
    if (stat(primitive_path, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_primitive_time) {
            last_primitive_time = stat_buf.st_mtime;
            log_to_file("Primitive file changed, reloading");
            return 1;
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat primitive file %s", primitive_path);
        log_to_file(msg);
    }
    return 0;
}

int main() {
    read_locations();
    resize_arrays();
    resize_text_arrays();
    read_primitive();

    while (1) {
        if (check_file_changes()) {
            read_primitive();
        }
        usleep(10000); // 10ms sleep
    }

    // Cleanup (unreachable)
    free(shape);
    free(x);
    free(y);
    free(z);
    free(r);
    free(g);
    free(b);
    free(a);
    free(type);
    free(text_lines);
    return 0;
}
