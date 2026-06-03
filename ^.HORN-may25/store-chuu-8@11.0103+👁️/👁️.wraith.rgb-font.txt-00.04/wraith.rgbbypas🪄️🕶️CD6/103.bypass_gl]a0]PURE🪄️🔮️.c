#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <regex.h>
#include <limits.h>

#define MAX_LINE 256
#define TRANSLUCENT_BLOCKS 1
#define INITIAL_MAP_CAPACITY 10
#define MAX_MAPS 100
#define MAX_MAP_SIZE 1000
#define MAX_PATH 256
#define MAX_SUPER_STATES 100
#define MAX_PIECES_PER_STATE 100
#define DEFAULT_PIECE_SIZE 0.25f
#define SHAPE_SPHERE 0
#define SHAPE_CUBE 1

// Global variables
int player_x = 0, player_y = 0, player_z = 0;
int cam_x = 0, cam_y = 0, cam_z = 0;
int cursor_pos[3] = {0, 0, 0};
char piece_symbol[16] = "P";
time_t last_map_load_time = 0;
time_t last_super_state_time = 0;
time_t last_state_time = 0;

// Dynamic file paths
char state_path[MAX_PATH] = "state.txt";
char maps_dir[MAX_PATH] = "./maps/";
char super_states_dir[MAX_PATH] = "";
char log_path[MAX_PATH] = "log.txt";
char primitive_path[MAX_PATH] = "./primitive.txt";

// Dynamic map storage
char ***maps = NULL;
int *map_coords = NULL;
int *map_sizes = NULL;
int num_maps = 0;
int map_capacity = 0;

// Super state storage (parallel arrays)
int *super_state_x = NULL;
int *super_state_y = NULL;
int *super_state_z = NULL;
char **super_state_symbol = NULL;
float *super_state_r = NULL;
float *super_state_g = NULL;
float *super_state_b = NULL;
int *super_state_has_color = NULL;
float *super_state_size = NULL;
int *super_state_shape = NULL;
int num_super_state_pieces = 0;
int super_state_piece_capacity = 0;

// Function declarations
void log_to_file(const char *message);
int is_directory_not_empty(const char *dir_path);
void read_locations();
void resize_maps();
void resize_super_state_pieces(int needed);
int parse_filename(const char *filename, int *map_x, int *map_y, int *map_z);
void parse_state_file(const char *filename, int is_state_file, int *x, int *y, int *z, int *cam_x, int *cam_y, int *cam_z, int *cursor_pos, char *piece_symbol);
void read_maps();
void read_state();
void read_super_state();
void write_primitive_file();
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

int is_directory_not_empty(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;
    struct dirent *entry;
    int file_count = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            file_count++;
            break;
        }
    }
    closedir(dir);
    return file_count > 0;
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
        } else if (strcmp(key, "maps") == 0) {
            strncpy(maps_dir, value, MAX_PATH - 1);
            maps_dir[MAX_PATH - 1] = '\0';
            if (maps_dir[strlen(maps_dir) - 1] != '/') {
                strncat(maps_dir, "/", MAX_PATH - strlen(maps_dir) - 1);
            }
        } else if (strcmp(key, "super_states") == 0) {
            strncpy(super_states_dir, value, MAX_PATH - 1);
            super_states_dir[MAX_PATH - 1] = '\0';
            if (super_states_dir[strlen(super_states_dir) - 1] != '/') {
                strncat(super_states_dir, "/", MAX_PATH - strlen(super_states_dir) - 1);
            }
        } else if (strcmp(key, "log") == 0) {
            strncpy(log_path, value, MAX_PATH - 1);
            log_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "primitive") == 0) {
            strncpy(primitive_path, value, MAX_PATH - 1);
            primitive_path[MAX_PATH - 1] = '\0';
        }
    }
    fclose(fp);
    char msg[MAX_PATH * 5];
    snprintf(msg, sizeof(msg), "Loaded paths: state=%s, maps=%s, super_states=%s, log=%s, primitive=%s",
             state_path, maps_dir, super_states_dir, log_path, primitive_path);
    log_to_file(msg);
}

void resize_maps() {
    if (num_maps >= map_capacity) {
        int new_capacity = map_capacity == 0 ? INITIAL_MAP_CAPACITY : map_capacity * 2;
        char ***new_maps = realloc(maps, new_capacity * sizeof(char **));
        int *new_coords = realloc(map_coords, new_capacity * 3 * sizeof(int));
        int *new_sizes = realloc(map_sizes, new_capacity * 2 * sizeof(int));
        if (!new_maps || !new_coords || !new_sizes) {
            log_to_file("Error: Memory allocation failed in resize_maps");
            exit(1);
        }
        maps = new_maps;
        map_coords = new_coords;
        map_sizes = new_sizes;
        map_capacity = new_capacity;
    }
}

void resize_super_state_pieces(int needed) {
    if (num_super_state_pieces + needed >= super_state_piece_capacity) {
        int new_capacity = super_state_piece_capacity == 0 ? 10 : super_state_piece_capacity * 2;
        while (new_capacity < num_super_state_pieces + needed) new_capacity *= 2;
        int *new_x = realloc(super_state_x, new_capacity * sizeof(int));
        int *new_y = realloc(super_state_y, new_capacity * sizeof(int));
        int *new_z = realloc(super_state_z, new_capacity * sizeof(int));
        char **new_symbol = realloc(super_state_symbol, new_capacity * sizeof(char *));
        float *new_r = realloc(super_state_r, new_capacity * sizeof(float));
        float *new_g = realloc(super_state_g, new_capacity * sizeof(float));
        float *new_b = realloc(super_state_b, new_capacity * sizeof(float));
        int *new_has_color = realloc(super_state_has_color, new_capacity * sizeof(int));
        float *new_size = realloc(super_state_size, new_capacity * sizeof(float));
        int *new_shape = realloc(super_state_shape, new_capacity * sizeof(int));
        if (!new_x || !new_y || !new_z || !new_symbol || !new_r || !new_g || !new_b || !new_has_color || !new_size || !new_shape) {
            log_to_file("Error: Memory allocation failed in resize_super_state_pieces");
            exit(1);
        }
        if (new_symbol) {
            for (int i = super_state_piece_capacity; i < new_capacity; i++) {
                new_symbol[i] = malloc(16 * sizeof(char));
                if (!new_symbol[i]) {
                    log_to_file("Error: Memory allocation failed for symbol array");
                    exit(1);
                }
                new_symbol[i][0] = '\0';
            }
        }
        super_state_x = new_x;
        super_state_y = new_y;
        super_state_z = new_z;
        super_state_symbol = new_symbol;
        super_state_r = new_r;
        super_state_g = new_g;
        super_state_b = new_b;
        super_state_has_color = new_has_color;
        super_state_size = new_size;
        super_state_shape = new_shape;
        super_state_piece_capacity = new_capacity;
    }
}

int parse_filename(const char *filename, int *map_x, int *map_y, int *map_z) {
    if (strncmp(filename, "x", 1) != 0) return 0;
    char *end;
    *map_x = strtol(filename + 1, &end, 10);
    if (*end != 'y') return 0;
    *map_y = strtol(end + 1, &end, 10);
    if (*end != 'z') return 0;
    *map_z = strtol(end + 1, &end, 10);
    if (strncmp(end, "_layer1.txt", 11) != 0) return 0;
    return 1;
}

void parse_state_file(const char *filename, int is_state_file, int *x, int *y, int *z, int *cam_x, int *cam_y, int *cam_z, int *cursor_pos, char *piece_symbol) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open state file %s", filename);
        log_to_file(msg);
        return;
    }

    // Temporary storage for pieces
    int temp_x[MAX_PIECES_PER_STATE];
    int temp_y[MAX_PIECES_PER_STATE];
    int temp_z[MAX_PIECES_PER_STATE];
    char temp_symbol[MAX_PIECES_PER_STATE][16];
    int temp_piece_count = 0;

    // First pass: collect pieces and state data
    char line[MAX_LINE];
    char current_symbol[16] = "P";
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_LINE];
        if (sscanf(line, "%s", key) != 1) continue;
        if (strcmp(key, "symbol") == 0) {
            char symbol[16];
            if (sscanf(line, "%*s %15[^\n]", symbol) == 1) {
                strncpy(current_symbol, symbol, 15);
                current_symbol[15] = '\0';
                if (is_state_file) {
                    strncpy(piece_symbol, symbol, 15);
                    piece_symbol[15] = '\0';
                }
            }
        } else if (strcmp(key, "piece") == 0 && temp_piece_count < MAX_PIECES_PER_STATE) {
            int px, py, pz;
            if (sscanf(line, "%*s %d %d %d", &px, &py, &pz) == 3) {
                temp_x[temp_piece_count] = px;
                temp_y[temp_piece_count] = py;
                temp_z[temp_piece_count] = pz;
                strncpy(temp_symbol[temp_piece_count], current_symbol, 15);
                temp_symbol[temp_piece_count][15] = '\0';
                temp_piece_count++;
            }
        } else if (is_state_file) {
            if (strcmp(key, "x") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *x = value;
            } else if (strcmp(key, "y") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *y = value;
            } else if (strcmp(key, "z") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *z = value;
            } else if (strcmp(key, "cam_x") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *cam_x = value;
            } else if (strcmp(key, "cam_y") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *cam_y = value;
            } else if (strcmp(key, "cam_z") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *cam_z = value;
            } else if (strcmp(key, "cursor_x") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) cursor_pos[0] = value;
            } else if (strcmp(key, "cursor_y") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) cursor_pos[1] = value;
            } else if (strcmp(key, "cursor_z") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) cursor_pos[2] = value;
            }
        }
    }

    // Second pass: apply colors, sizes, and shapes
    rewind(fp);
    float current_r = 1.0f, current_g = 0.8f, current_b = 0.8f;
    int current_has_color = 0;
    float current_size = DEFAULT_PIECE_SIZE;
    int current_shape = SHAPE_SPHERE;
     current_symbol[16] = "P";
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_LINE];
        if (sscanf(line, "%s", key) != 1) continue;
        if (strcmp(key, "symbol") == 0) {
            char symbol[16];
            if (sscanf(line, "%*s %15[^\n]", symbol) == 1) {
                strncpy(current_symbol, symbol, 15);
                current_symbol[15] = '\0';
            }
        } else if (strcmp(key, "color") == 0) {
            float r, g, b;
            if (sscanf(line, "%*s %f %f %f", &r, &g, &b) == 3) {
                current_r = r;
                current_g = g;
                current_b = b;
                current_has_color = 1;
            }
        } else if (strcmp(key, "size") == 0) {
            float size;
            if (sscanf(line, "%*s %f", &size) == 1) {
                current_size = size;
            }
        } else if (strcmp(key, "shape") == 0) {
            char shape[16];
            if (sscanf(line, "%*s %15s", shape) == 1) {
                if (strcmp(shape, "cube") == 0) {
                    current_shape = SHAPE_CUBE;
                } else if (strcmp(shape, "sphere") == 0) {
                    current_shape = SHAPE_SPHERE;
                }
            }
        }
    }

    // Store pieces in super_state arrays
    for (int i = 0; i < temp_piece_count; i++) {
        resize_super_state_pieces(1);
        super_state_x[num_super_state_pieces] = temp_x[i];
        super_state_y[num_super_state_pieces] = temp_y[i];
        super_state_z[num_super_state_pieces] = temp_z[i];
        strncpy(super_state_symbol[num_super_state_pieces], temp_symbol[i], 15);
        super_state_symbol[num_super_state_pieces][15] = '\0';
        super_state_r[num_super_state_pieces] = current_has_color && strcmp(temp_symbol[i], current_symbol) == 0 ? current_r : 1.0f;
        super_state_g[num_super_state_pieces] = current_has_color && strcmp(temp_symbol[i], current_symbol) == 0 ? current_g : 0.8f;
        super_state_b[num_super_state_pieces] = current_has_color && strcmp(temp_symbol[i], current_symbol) == 0 ? current_b : 0.8f;
        super_state_has_color[num_super_state_pieces] = current_has_color && strcmp(temp_symbol[i], current_symbol) == 0 ? 1 : 0;
        super_state_size[num_super_state_pieces] = strcmp(temp_symbol[i], current_symbol) == 0 ? current_size : DEFAULT_PIECE_SIZE;
        super_state_shape[num_super_state_pieces] = strcmp(temp_symbol[i], current_symbol) == 0 ? current_shape : SHAPE_SPHERE;
        if (is_state_file) {
            for (int j = 0; j < num_maps; j++) {
                int mx = map_coords[j*3 + 0];
                int my = map_coords[j*3 + 1];
                int mz = map_coords[j*3 + 2];
                int width = map_sizes[j*2 + 0];
                int height = map_sizes[j*2 + 1];
                if (temp_z[i] == mz && temp_x[i] >= mx && temp_x[i] < mx + width && temp_y[i] >= my && temp_y[i] < my + height) {
                    maps[j][temp_y[i] - my][temp_x[i] - mx] = temp_symbol[i][0];
                }
            }
        }
        char debug_msg[256];
        snprintf(debug_msg, sizeof(debug_msg), "Piece %d: symbol=%s, x=%d, y=%d, z=%d, color=(%.1f, %.1f, %.1f), has_color=%d, size=%.2f, shape=%s",
                 num_super_state_pieces, super_state_symbol[num_super_state_pieces],
                 super_state_x[num_super_state_pieces], super_state_y[num_super_state_pieces],
                 super_state_z[num_super_state_pieces], super_state_r[num_super_state_pieces],
                 super_state_g[num_super_state_pieces], super_state_b[num_super_state_pieces],
                 super_state_has_color[num_super_state_pieces], super_state_size[num_super_state_pieces],
                 super_state_shape[num_super_state_pieces] == SHAPE_CUBE ? "cube" : "sphere");
        log_to_file(debug_msg);
        num_super_state_pieces++;
    }

    fclose(fp);
    char msg[MAX_PATH + 50];
    snprintf(msg, sizeof(msg), "Loaded %d pieces from %s", temp_piece_count, filename);
    log_to_file(msg);
}

void read_maps() {
    for (int i = 0; i < num_maps; i++) {
        for (int y = 0; y < map_sizes[i*2 + 1]; y++) {
            free(maps[i][y]);
        }
        free(maps[i]);
    }
    free(map_coords);
    free(map_sizes);
    maps = NULL;
    map_coords = NULL;
    map_sizes = NULL;
    num_maps = 0;
    map_capacity = 0;

    DIR *dir = opendir(maps_dir);
    if (!dir) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Error: Failed to open maps directory %s", maps_dir);
        log_to_file(msg);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;
        int map_x, map_y, map_z;
        if (!parse_filename(entry->d_name, &map_x, &map_y, &map_z)) continue;

        if (num_maps >= MAX_MAPS) {
            log_to_file("Error: Maximum number of maps reached");
            closedir(dir);
            return;
        }

        char filename[MAX_PATH];
        snprintf(filename, MAX_PATH, "%s%s", maps_dir, entry->d_name);
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            char msg[MAX_PATH + 50];
            snprintf(msg, sizeof(msg), "Warning: Failed to open map file %s", filename);
            log_to_file(msg);
            continue;
        }

        int width = 0, height = 0;
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, fp)) {
            line[strcspn(line, "\n")] = '\0';
            int len = strlen(line);
            if (len > width) width = len;
            height++;
        }
        if (width > MAX_MAP_SIZE || height > MAX_MAP_SIZE) {
            log_to_file("Error: Map dimensions exceed maximum allowed size");
            fclose(fp);
            continue;
        }
        rewind(fp);

        resize_maps();
        char **map = malloc(height * sizeof(char *));
        if (!map) {
            log_to_file("Error: Memory allocation failed for map");
            fclose(fp);
            exit(1);
        }
        for (int y = 0; y < height; y++) {
            map[y] = malloc((width + 1) * sizeof(char));
            if (!map[y]) {
                log_to_file("Error: Memory allocation failed for map row");
                for (int j = 0; j < y; j++) free(map[j]);
                free(map);
                fclose(fp);
                exit(1);
            }
            for (int x = 0; x < width; x++) {
                map[y][x] = '.';
            }
            map[y][width] = '\0';
        }

        int row = 0;
        while (row < height && fgets(line, MAX_LINE, fp)) {
            line[strcspn(line, "\n")] = '\0';
            int len = strlen(line);
            if (len > width) len = width;
            for (int x = 0; x < len; x++) {
                map[row][x] = line[x];
            }
            row++;
        }
        fclose(fp);

        maps[num_maps] = map;
        map_coords[num_maps*3 + 0] = map_x;
        map_coords[num_maps*3 + 1] = map_y;
        map_coords[num_maps*3 + 2] = map_z;
        map_sizes[num_maps*2 + 0] = width;
        map_sizes[num_maps*2 + 1] = height;
        num_maps++;
    }
    closedir(dir);
}

void read_state() {
    parse_state_file(state_path, 1, &player_x, &player_y, &player_z, &cam_x, &cam_y, &cam_z, cursor_pos, piece_symbol);
}

void read_super_state() {
    if (super_states_dir[0] == '\0') {
        log_to_file("super_states directory not specified in locations.txt, skipping");
        return;
    }

    for (int i = 0; i < num_super_state_pieces; i++) {
        free(super_state_symbol[i]);
    }
    free(super_state_x);
    free(super_state_y);
    free(super_state_z);
    free(super_state_symbol);
    free(super_state_r);
    free(super_state_g);
    free(super_state_b);
    free(super_state_has_color);
    free(super_state_size);
    free(super_state_shape);
    super_state_x = NULL;
    super_state_y = NULL;
    super_state_z = NULL;
    super_state_symbol = NULL;
    super_state_r = NULL;
    super_state_g = NULL;
    super_state_b = NULL;
    super_state_has_color = NULL;
    super_state_size = NULL;
    super_state_shape = NULL;
    num_super_state_pieces = 0;
    super_state_piece_capacity = 0;

    DIR *dir = opendir(super_states_dir);
    if (!dir) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open super_states directory %s", super_states_dir);
        log_to_file(msg);
        return;
    }

    struct dirent *entry;
    int file_count = 0;
    regex_t regex;
    if (regcomp(&regex, "^state_.*\\.txt$", REG_EXTENDED) != 0) {
        log_to_file("Error: Failed to compile regex for super_states files");
        closedir(dir);
        return;
    }

    while ((entry = readdir(dir)) && file_count < MAX_SUPER_STATES) {
        if (entry->d_type != DT_REG) continue;
        if (regexec(&regex, entry->d_name, 0, NULL, 0) != 0) continue;

        char filename[MAX_PATH];
        snprintf(filename, MAX_PATH, "%s%s", super_states_dir, entry->d_name);
        parse_state_file(filename, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        file_count++;
    }
    regfree(&regex);
    closedir(dir);

    char msg[256];
    snprintf(msg, sizeof(msg), "Total super_states pieces loaded: %d", num_super_state_pieces);
    log_to_file(msg);
}

void write_primitive_file() {
    FILE *fp = fopen(primitive_path, "w");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Error: Failed to open primitive file %s for writing", primitive_path);
        log_to_file(msg);
        return;
    }

    // Write blocks from maps
    for (int i = 0; i < num_maps; i++) {
        int mx = map_coords[i*3 + 0];
        int my = map_coords[i*3 + 1];
        int mz = map_coords[i*3 + 2];
        int width = map_sizes[i*2 + 0];
        int height = map_sizes[i*2 + 1];
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (maps[i][y][x] == '#') {
                    fprintf(fp, "primitive cube %.1f %.1f %.1f 0.5 0.5 0.5 %.1f type=block\n",
                            (float)mx + x + 0.5f, (float)my + y + 0.5f, (float)mz + 0.5f,
                            TRANSLUCENT_BLOCKS ? 0.3f : 1.0f);
                }
            }
        }
    }

    // Write player
    fprintf(fp, "primitive sphere %.1f %.1f %.1f 0.0 0.0 1.0 1.0 type=player\n",
            (float)player_x + 0.5f, (float)player_y + 0.5f, (float)player_z + 0.5f);

    // Write super state pieces
    for (int i = 0; i < num_super_state_pieces; i++) {
        const char *shape = super_state_shape[i] == SHAPE_CUBE ? "cube" : "sphere";
        float r = super_state_has_color[i] ? super_state_r[i] : 0.2f;
        float g = super_state_has_color[i] ? super_state_g[i] : 0.8f;
        float b = super_state_has_color[i] ? super_state_b[i] : 0.2f;
        fprintf(fp, "primitive %s %.1f %.1f %.1f %.1f %.1f %.1f 1.0 type=piece\n",
                shape, (float)super_state_x[i] + 0.5f, (float)super_state_y[i] + 0.5f,
                (float)super_state_z[i] + 0.5f, r, g, b);
    }

    // Write cursor
    if (cursor_pos[0] >= -1000 && cursor_pos[0] < 1000 &&
        cursor_pos[1] >= -1000 && cursor_pos[1] < 1000 &&
        cursor_pos[2] >= -1000 && cursor_pos[2] < 1000) {
        fprintf(fp, "primitive cube %.1f %.1f %.1f 1.0 1.0 0.0 0.3 type=cursor\n",
                (float)cursor_pos[0] + 0.5f, (float)cursor_pos[1] + 0.5f, (float)cursor_pos[2] + 0.5f);
    }

    // Write text entries
    char coord_text[256];
    snprintf(coord_text, sizeof(coord_text), "Player: (%d, %d, %d)", player_x, player_y, player_z);
    fprintf(fp, "text %s\n", coord_text);
    snprintf(coord_text, sizeof(coord_text), "Camera: (%d, %d, %d)", cam_x, cam_y, cam_z);
    fprintf(fp, "text %s\n", coord_text);
    snprintf(coord_text, sizeof(coord_text), "Cursor: (%d, %d, %d)", cursor_pos[0], cursor_pos[1], cursor_pos[2]);
    fprintf(fp, "text %s\n", coord_text);

    fclose(fp);
    char msg[MAX_PATH + 50];
    snprintf(msg, sizeof(msg), "Wrote scene to %s", primitive_path);
    log_to_file(msg);
}

int check_file_changes() {
    int changed = 0;
    struct stat stat_buf;

    // Check state.txt
    if (stat(state_path, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_state_time) {
            last_state_time = stat_buf.st_mtime;
            read_state();
            changed = 1;
            log_to_file("State file changed, reloaded");
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat state file %s", state_path);
        log_to_file(msg);
    }

    // Check maps directory
    if (stat(maps_dir, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_map_load_time) {
            last_map_load_time = stat_buf.st_mtime;
            read_maps();
            changed = 1;
            log_to_file("Maps directory changed, reloaded");
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat maps directory %s", maps_dir);
        log_to_file(msg);
    }

    // Check super_states directory
    if (super_states_dir[0] != '\0') {
        if (stat(super_states_dir, &stat_buf) == 0) {
            if (stat_buf.st_mtime > last_super_state_time) {
                last_super_state_time = stat_buf.st_mtime;
                read_super_state();
                changed = 1;
                log_to_file("Super states directory changed, reloaded");
            }
        } else {
            char msg[MAX_PATH + 50];
            snprintf(msg, sizeof(msg), "Warning: Failed to stat super_states directory %s", super_states_dir);
            log_to_file(msg);
        }
    }

    return changed;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        log_to_file("Error: No arguments allowed");
        return 1;
    }

    read_locations();

    if (super_states_dir[0] != '\0' && is_directory_not_empty(super_states_dir)) {
        log_to_file("Using super_states directory for piece data");
    } else {
        log_to_file("super_states directory empty or not specified");
    }

    // Initial read
    read_state();
    read_maps();
    read_super_state();
    write_primitive_file();

    // Main loop for change detection
    while (1) {
        if (check_file_changes()) {
            write_primitive_file();
        }
        usleep(1000000); // Sleep for 1 second
    }

    // Cleanup (unreachable in infinite loop, but included for completeness)
    for (int i = 0; i < num_maps; i++) {
        for (int y = 0; y < map_sizes[i*2 + 1]; y++) {
            free(maps[i][y]);
        }
        free(maps[i]);
    }
    free(maps);
    free(map_coords);
    free(map_sizes);
    for (int i = 0; i < num_super_state_pieces; i++) {
        free(super_state_symbol[i]);
    }
    free(super_state_x);
    free(super_state_y);
    free(super_state_z);
    free(super_state_symbol);
    free(super_state_r);
    free(super_state_g);
    free(super_state_b);
    free(super_state_has_color);
    free(super_state_size);
    free(super_state_shape);
    return 0;
}
