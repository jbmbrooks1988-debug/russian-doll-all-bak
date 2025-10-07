/*
 * managers.c - Game state and data management
 * Based on RMMV's rpg_managers.js functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

// Game data structures
#define MAX_MAPS 100
#define MAX_EVENTS 1000

typedef struct {
    int id;
    char name[256];
    char note[1024];
    int x, y, z;
} GameEvent;

typedef struct {
    int x, y, z;
    int emoji_idx;
    int fg_color_idx;
    int bg_color_idx;
} MapTile;

typedef struct {
    int id;
    char filename[256];
    char name[256];
    MapTile* tiles;
    int tile_count;
    GameEvent* events;
    int event_count;
} GameMap;

// Managers function declarations
void init_managers(void);
void update_managers(void);
void render_managers(void);
void cleanup_managers(void);
int load_event_data(const char* filename, GameMap* map);

// Getters
int get_map_count(void);
GameMap* get_map(int index);

// Game state
static GameMap maps[MAX_MAPS];
static int map_count = 0;
static bool managers_initialized = false;
static char data_path[512] = "data/";

// Utility function to check if file exists
bool file_exists(const char* filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// Load map data from file
int load_map_data(const char* filename, GameMap* map) {
    // Dynamically allocate buffer for full path
    size_t data_path_len = strlen(data_path);
    size_t filename_len = strlen(filename);
    char* full_path = malloc(data_path_len + filename_len + 2); // +2 for '/' and null terminator
    
    if (!full_path) {
        fprintf(stderr, "Error: Failed to allocate memory for full_path\n");
        return -1;
    }
    
    snprintf(full_path, data_path_len + filename_len + 2, "%s/%s", data_path, filename);
    
    FILE* fp = fopen(full_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for reading\n", full_path);
        return -1;
    }
    
    // Read header
    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    
    // Parse data
    map->tile_count = 0;
    map->tiles = NULL;
    
    while (fgets(line, sizeof(line), fp)) {
        int x, y, z, emoji_idx, fg_color_idx, bg_color_idx;
        if (sscanf(line, "%d,%d,%d,%d,%d,%d", &x, &y, &z, &emoji_idx, &fg_color_idx, &bg_color_idx) == 6) {
            // Reallocate memory for tiles
            MapTile* new_tiles = realloc(map->tiles, (map->tile_count + 1) * sizeof(MapTile));
            if (!new_tiles) {
                fclose(fp);
                free(map->tiles);
                return -1;
            }
            map->tiles = new_tiles;
            
            // Add tile
            map->tiles[map->tile_count].x = x;
            map->tiles[map->tile_count].y = y;
            map->tiles[map->tile_count].z = z;
            map->tiles[map->tile_count].emoji_idx = emoji_idx;
            map->tiles[map->tile_count].fg_color_idx = fg_color_idx;
            map->tiles[map->tile_count].bg_color_idx = bg_color_idx;
            map->tile_count++;
        }
    }
    
    fclose(fp);
    free(full_path);  // Free dynamically allocated memory
    return 0;
}

// Load event data from file
int load_event_data(const char* filename, GameMap* map) {
    // Dynamically allocate buffer for full path
    size_t data_path_len = strlen(data_path);
    size_t filename_len = strlen(filename);
    char* full_path = malloc(data_path_len + filename_len + 2); // +2 for '/' and null terminator
    
    if (!full_path) {
        fprintf(stderr, "Error: Failed to allocate memory for full_path\n");
        return -1;
    }
    
    snprintf(full_path, data_path_len + filename_len + 2, "%s/%s", data_path, filename);
    
    FILE* fp = fopen(full_path, "r");
    if (!fp) {
        // Event file might not exist, which is okay
        free(full_path);  // Free dynamically allocated memory
        return 0;
    }
    
    // Initialize events
    map->event_count = 0;
    map->events = NULL;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Simple parsing for now - in a real implementation, this would be more complex
        // For now, we'll just count events
        if (strstr(line, "events[")) {
            map->event_count++;
        }
    }
    
    // Reallocate memory for events
    if (map->event_count > 0) {
        map->events = malloc(map->event_count * sizeof(GameEvent));
        if (!map->events) {
            fclose(fp);
            free(full_path);  // Free dynamically allocated memory
            return -1;
        }
        
        // Reset file pointer and parse actual data
        fseek(fp, 0, SEEK_SET);
        int event_idx = 0;
        while (fgets(line, sizeof(line), fp) && event_idx < map->event_count) {
            if (strstr(line, "events[")) {
                // Parse event data
                sscanf(line, "events[%*d].id,%d", &map->events[event_idx].id);
            } else if (strstr(line, ".name,")) {
                char* comma = strchr(line, ',');
                if (comma) {
                    strncpy(map->events[event_idx].name, comma + 1, sizeof(map->events[event_idx].name) - 1);
                    map->events[event_idx].name[sizeof(map->events[event_idx].name) - 1] = '\0';
                    // Remove newline if present
                    char* newline = strchr(map->events[event_idx].name, '\n');
                    if (newline) *newline = '\0';
                }
                event_idx++;
            }
        }
    }
    
    fclose(fp);
    free(full_path);  // Free dynamically allocated memory
    return 0;
}

// Scan for map files in data directory
int scan_map_files(void) {
    printf("DEBUG: Scanning for map files in %s\n", data_path);
    
    DIR* dir = opendir(data_path);
    if (!dir) {
        fprintf(stderr, "Error: Could not open data directory %s\n", data_path);
        return -1;
    }
    
    struct dirent* entry;
    map_count = 0;
    
    while ((entry = readdir(dir)) != NULL && map_count < MAX_MAPS) {
        if (entry->d_type == DT_REG) {
            // Check if file matches pattern "map_*.txt" but not "map_events_*.txt"
            if (strncmp(entry->d_name, "map_", 4) == 0) {
                // Exclude map_events_*.txt files
                if (strncmp(entry->d_name, "map_events_", 11) != 0) {
                    char* ext = strrchr(entry->d_name, '.');
                    if (ext && strcmp(ext, ".txt") == 0) {
                        printf("DEBUG: Found map file: %s\n", entry->d_name);
                        
                        // Load map data
                        memset(&maps[map_count], 0, sizeof(GameMap));
                        maps[map_count].id = map_count;
                        strncpy(maps[map_count].filename, entry->d_name, sizeof(maps[map_count].filename) - 1);
                        maps[map_count].filename[sizeof(maps[map_count].filename) - 1] = '\0';
                        
                        // Try to extract map name from filename
                        char name[256];
                        strcpy(name, entry->d_name);
                        char* dot = strrchr(name, '.');
                        if (dot) *dot = '\0';
                        strncpy(maps[map_count].name, name, sizeof(maps[map_count].name) - 1);
                        maps[map_count].name[sizeof(maps[map_count].name) - 1] = '\0';
                        
                        // Load the map data
                        if (load_map_data(entry->d_name, &maps[map_count]) == 0) {
                            printf("DEBUG: Loaded map %d: %s with %d tiles\n", map_count, maps[map_count].name, maps[map_count].tile_count);
                            
                            // Try to load corresponding event data
                            char event_filename[256];
                            strcpy(event_filename, entry->d_name);
                            char* dot = strrchr(event_filename, '.');
                            if (dot) {
                                strcpy(dot, "_events.txt");
                                load_event_data(event_filename, &maps[map_count]);
                            }
                            
                            map_count++;
                        } else {
                            printf("DEBUG: Failed to load map: %s\n", entry->d_name);
                        }
                    }
                }
            }
        }
    }
    
    closedir(dir);
    printf("DEBUG: Finished scanning, found %d maps\n", map_count);
    return 0;
}

// Managers initialization
void init_managers(void) {
    if (managers_initialized) return;
    
    printf("Initializing managers...\n");
    
    // Set data path based on command line argument or environment variable
    char* env_data_path = getenv("PLAYER_DATA_PATH");
    if (env_data_path) {
        strncpy(data_path, env_data_path, sizeof(data_path) - 1);
        data_path[sizeof(data_path) - 1] = '\0';
        printf("Using data path from environment: %s\n", data_path);
    } else {
        // Default to the data directory in the player folder
        strncpy(data_path, "data/", sizeof(data_path) - 1);
        data_path[sizeof(data_path) - 1] = '\0';
        printf("Using default data path: %s\n", data_path);
    }
    
    // Scan for map files
    if (scan_map_files() < 0) {
        fprintf(stderr, "Warning: Failed to scan map files\n");
    }
    
    managers_initialized = true;
    printf("Managers initialized with %d maps.\n", map_count);
}

// Managers update
void update_managers(void) {
    // Managers update logic
}

// Managers rendering
void render_managers(void) {
    // Managers rendering logic
}

// Managers cleanup
void cleanup_managers(void) {
    if (!managers_initialized) return;
    
    printf("Cleaning up managers...\n");
    
    // Free map data
    for (int i = 0; i < map_count; i++) {
        if (maps[i].tiles) {
            free(maps[i].tiles);
            maps[i].tiles = NULL;
        }
        if (maps[i].events) {
            free(maps[i].events);
            maps[i].events = NULL;
        }
    }
    map_count = 0;
    
    managers_initialized = false;
    printf("Managers cleaned up.\n");
}

// Getters
int get_map_count(void) {
    return map_count;
}

GameMap* get_map(int index) {
    if (index >= 0 && index < map_count) {
        return &maps[index];
    }
    return NULL;
}