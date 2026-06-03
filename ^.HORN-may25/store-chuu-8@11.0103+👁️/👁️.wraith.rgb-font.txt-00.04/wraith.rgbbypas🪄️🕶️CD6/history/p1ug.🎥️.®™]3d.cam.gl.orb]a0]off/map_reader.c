#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_LINE 256
#define INITIAL_MAP_CAPACITY 10
#define MAX_MAPS 100
#define MAX_MAP_SIZE 1000

int maps_data[1024*1024]; // Array for maps, coords, sizes (oversized for safety)
int num_maps = 0;
int map_capacity = 0;
time_t last_map_load_time = 0;

void log_to_file(const char *message) {
    FILE *fp = fopen("log.txt", "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s\n", timestamp, message);
    fclose(fp);
}

void resize_maps() {
    if (num_maps >= map_capacity) {
        int new_capacity = map_capacity == 0 ? INITIAL_MAP_CAPACITY : map_capacity * 2;
        if (new_capacity > MAX_MAPS) new_capacity = MAX_MAPS;
        map_capacity = new_capacity;
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

void read_maps() {
    struct stat dir_stat;
    if (stat("./maps", &dir_stat) != 0 || dir_stat.st_mtime <= last_map_load_time) {
        return;
    }
    last_map_load_time = dir_stat.st_mtime;

    num_maps = 0;
    DIR *dir = opendir("./maps");
    if (!dir) {
        log_to_file("Error: Failed to open maps directory");
        return;
    }

    struct dirent *entry;
    int map_offset = 0;
    while ((entry = readdir(dir)) && num_maps < MAX_MAPS) {
        if (entry->d_type != DT_REG) continue;
        int map_x, map_y, map_z;
        if (!parse_filename(entry->d_name, &map_x, &map_y, &map_z)) continue;

        char filename[MAX_LINE];
        snprintf(filename, MAX_LINE, "./maps/%s", entry->d_name);
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            char msg[256];
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
        maps_data[num_maps*4 + 0] = map_x;
        maps_data[num_maps*4 + 1] = map_y;
        maps_data[num_maps*4 + 2] = map_z;
        maps_data[num_maps*4 + 3] = map_offset;

        int row = 0;
        while (row < height && fgets(line, MAX_LINE, fp)) {
            line[strcspn(line, "\n")] = '\0';
            int len = strlen(line);
            if (len > width) len = width;
            for (int x = 0; x < width; x++) {
                maps_data[map_offset + row*MAX_MAP_SIZE + x] = line[x] ? line[x] : '.';
            }
            row++;
        }
        fclose(fp);
        maps_data[num_maps*4 + 3 + 1] = width;
        maps_data[num_maps*4 + 3 + 2] = height;
        map_offset += width * height;
        num_maps++;
    }
    closedir(dir);

    FILE *fp = fopen("maps_data.txt", "w");
    if (!fp) {
        log_to_file("Error: Failed to write maps_data.txt");
        return;
    }
    fprintf(fp, "%d\n", num_maps);
    for (int i = 0; i < num_maps; i++) {
        int map_x = maps_data[i*4 + 0];
        int map_y = maps_data[i*4 + 1];
        int map_z = maps_data[i*4 + 2];
        int offset = maps_data[i*4 + 3];
        int width = maps_data[i*4 + 3 + 1];
        int height = maps_data[i*4 + 3 + 2];
        fprintf(fp, "%d %d %d %d %d\n", map_x, map_y, map_z, width, height);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                fprintf(fp, "%c", maps_data[offset + y*MAX_MAP_SIZE + x]);
            }
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
    log_to_file("Maps written to maps_data.txt");
}

int main(int argc, char *argv[]) {
    read_maps();
    return 0;
}
