
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <float.h> // Added for FLT_MAX

#define MAX_LINE 256
#define MAX_PATH 256
#define MAX_PRIMITIVES 10000
#define GRID_WIDTH 640
#define GRID_HEIGHT 480
#define PIXEL_SCALE 10.0f // World units per pixel
#define CUBE_PIXEL_SIZE 5 // Pixel square size for cubes

// Primitive structure
typedef struct {
    char shape[16]; // cube or sphere
    float x, y, z;  // World coordinates
    float r, g, b, a; // Color (0.0-1.0)
    char type[16];  // block, player, piece, cursor
} Primitive;

// Global variables
Primitive *primitives = NULL;
int num_primitives = 0;
int primitive_capacity = 0;
float player_x = 0.0f, player_y = 0.0f, player_z = 0.0f;
char primitive_path[MAX_PATH] = "./primitive.txt";
char log_path[MAX_PATH] = "log.txt";
char rgb_path[MAX_PATH] = "./rgb.txt";
time_t last_primitive_time = 0;
unsigned char pixel_r[GRID_HEIGHT][GRID_WIDTH];
unsigned char pixel_g[GRID_HEIGHT][GRID_WIDTH];
unsigned char pixel_b[GRID_HEIGHT][GRID_WIDTH];
float pixel_z[GRID_HEIGHT][GRID_WIDTH];

// Function declarations
void log_to_file(const char *message);
void read_locations();
void resize_primitives();
void read_primitive();
void project_to_grid();
void write_rgb_file();
int check_file_changes();

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
        } else if (strcmp(key, "log") == 0) {
            strncpy(log_path, value, MAX_PATH - 1);
            log_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "rgb") == 0) {
            strncpy(rgb_path, value, MAX_PATH - 1);
            rgb_path[MAX_PATH - 1] = '\0';
        }
    }
    fclose(fp);
    char msg[MAX_PATH * 3];
    snprintf(msg, sizeof(msg), "Loaded paths: primitive=%s, log=%s, rgb=%s",
             primitive_path, log_path, rgb_path);
    log_to_file(msg);
}

void resize_primitives() {
    if (num_primitives >= primitive_capacity) {
        int new_capacity = primitive_capacity == 0 ? 100 : primitive_capacity * 2;
        Primitive *new_primitives = realloc(primitives, new_capacity * sizeof(Primitive));
        if (!new_primitives) {
            log_to_file("Error: Memory allocation failed in resize_primitives");
            exit(1);
        }
        primitives = new_primitives;
        primitive_capacity = new_capacity;
    }
}

void read_primitive() {
    // Clear previous primitives
    free(primitives);
    primitives = NULL;
    num_primitives = 0;
    primitive_capacity = 0;

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
            char shape[16], type[16];
            float x, y, z, r, g, b, a;
            if (sscanf(line, "primitive %15s %f %f %f %f %f %f %f type=%15s",
                       shape, &x, &y, &z, &r, &g, &b, &a, type) == 9) {
                if (strcmp(type, "player") == 0) {
                    player_x = x;
                    player_y = y;
                    player_z = z;
                }
                if (num_primitives < MAX_PRIMITIVES) {
                    resize_primitives();
                    Primitive *p = &primitives[num_primitives];
                    strncpy(p->shape, shape, 15);
                    p->shape[15] = '\0';
                    p->x = x;
                    p->y = y;
                    p->z = z;
                    p->r = r;
                    p->g = g;
                    p->b = b;
                    p->a = a;
                    strncpy(p->type, type, 15);
                    p->type[15] = '\0';
                    num_primitives++;
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Loaded primitive: %s at (%.1f, %.1f, %.1f), color=(%.1f, %.1f, %.1f, %.1f), type=%s",
                             shape, x, y, z, r, g, b, a, type);
                    log_to_file(msg);
                }
            }
        }
    }
    fclose(fp);
    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded %d primitives from %s", num_primitives, primitive_path);
    log_to_file(msg);
}

void project_to_grid() {
    // Initialize grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            pixel_r[y][x] = 0;
            pixel_g[y][x] = 0;
            pixel_b[y][x] = 0;
            pixel_z[y][x] = -FLT_MAX; // Ensure first primitive wins
        }
    }

    // Orthographic projection centered on player
    float window_width = GRID_WIDTH / PIXEL_SCALE;  // World units
    float window_height = GRID_HEIGHT / PIXEL_SCALE;
    float center_x = player_x;
    float center_y = player_y;

    // Render order: blocks, pieces, player, cursor
    for (int render_pass = 0; render_pass < 4; render_pass++) {
        const char *target_type;
        switch (render_pass) {
            case 0: target_type = "block"; break;
            case 1: target_type = "piece"; break;
            case 2: target_type = "player"; break;
            case 3: target_type = "cursor"; break;
            default: continue;
        }

        for (int i = 0; i < num_primitives; i++) {
            Primitive *p = &primitives[i];
            if (strcmp(p->type, target_type) != 0) continue;

            // Project world coords to pixel grid
            float rel_x = p->x - center_x;
            float rel_y = p->y - center_y;
            int base_pixel_x = (int)((rel_x + window_width / 2.0f) * PIXEL_SCALE);
            int base_pixel_y = (int)((window_height / 2.0f - rel_y) * PIXEL_SCALE);

            // Render cubes as squares, spheres as single pixels
            int pixel_size = strcmp(p->shape, "cube") == 0 ? CUBE_PIXEL_SIZE : 1;
            for (int dy = -pixel_size / 2; dy <= pixel_size / 2; dy++) {
                for (int dx = -pixel_size / 2; dx <= pixel_size / 2; dx++) {
                    int pixel_x = base_pixel_x + dx;
                    int pixel_y = base_pixel_y + dy;

                    // Check bounds
                    if (pixel_x >= 0 && pixel_x < GRID_WIDTH && pixel_y >= 0 && pixel_y < GRID_HEIGHT) {
                        // Update if z is closer (or equal for same pass)
                        if (p->z >= pixel_z[pixel_y][pixel_x]) {
                            pixel_r[pixel_y][pixel_x] = (unsigned char)(p->r * 255.0f);
                            pixel_g[pixel_y][pixel_x] = (unsigned char)(p->g * 255.0f);
                            pixel_b[pixel_y][pixel_x] = (unsigned char)(p->b * 255.0f);
                            pixel_z[pixel_y][pixel_x] = p->z;
                        }
                    }
                }
            }
        }
    }
}

void write_rgb_file() {
    FILE *fp = fopen(rgb_path, "w");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Error: Failed to open rgb file %s for writing", rgb_path);
        log_to_file(msg);
        return;
    }

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (pixel_r[y][x] != 0 || pixel_g[y][x] != 0 || pixel_b[y][x] != 0) {
                fprintf(fp, "pixel %d %d %d %d %d\n",
                        x, y, pixel_r[y][x], pixel_g[y][x], pixel_b[y][x]);
            }
        }
    }

    fclose(fp);
    char msg[MAX_PATH + 50];
    snprintf(msg, sizeof(msg), "Wrote pixel data to %s", rgb_path);
    log_to_file(msg);
}

int check_file_changes() {
    struct stat stat_buf;
    if (stat(primitive_path, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_primitive_time) {
            last_primitive_time = stat_buf.st_mtime;
            read_primitive();
            project_to_grid();
            write_rgb_file();
            log_to_file("Primitive file changed, reprocessed");
            return 1;
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat primitive file %s", primitive_path);
        log_to_file(msg);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        log_to_file("Error: No arguments allowed");
        return 1;
    }

    read_locations();
    read_primitive();
    project_to_grid();
    write_rgb_file();

    // Main loop for change detection
    while (1) {
        check_file_changes();
        usleep(1000000); // Sleep for 1 second
    }

    // Cleanup (unreachable in infinite loop)
    free(primitives);
    return 0;
}

