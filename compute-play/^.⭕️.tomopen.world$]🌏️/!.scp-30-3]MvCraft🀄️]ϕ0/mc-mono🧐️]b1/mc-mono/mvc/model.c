#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// Constants
#define WORLD_SIZE 64
#define CHUNK_SIZE 16
#define BLOCK_TYPES_MAX 256

// Block types enum
typedef enum {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_WATER,
    BLOCK_SAND,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_SNOW,
    BLOCK_ICE
} BlockType;

// Block structure
typedef struct {
    BlockType type;
    uint8_t metadata;
    bool active;
} Block;

// Chunk structure (16x16x16)
typedef struct {
    Block blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    int x, y, z;  // Chunk position in world
    bool dirty;   // Whether this chunk needs to be re-rendered
} Chunk;

// Biome types
typedef enum {
    BIOME_PLAINS,
    BIOME_FOREST,
    BIOME_DESERT,
    BIOME_TUNDRA,
    BIOME_MOUNTAINS,
    BIOME_OCEAN
} BiomeType;

// Biome structure
typedef struct {
    BiomeType type;
    float temperature;
    float humidity;
    BlockType ground_block;
    BlockType subsurface_block;
    float elevation_factor;
} Biome;

// Player structure
typedef struct {
    float x, y, z;        // Position
    float rotation_x;     // Looking angle (horizontal)
    float rotation_y;     // Looking angle (vertical)
    float velocity_x, velocity_y, velocity_z;  // Movement velocity
    bool on_ground;
    bool jumping;
    int selected_block;   // Currently selected block type
} Player;

// World structure
typedef struct {
    Chunk*** chunks;      // 3D array of chunks (x, y, z)
    Biome* biomes;        // Biome data
    int width, height, depth;  // World dimensions
    Player player;
    int seed;             // World generation seed
} World;

// Function declarations
void init_world(World* world, int seed);
void generate_sphere_world(World* world);
void generate_biomes(World* world);
void set_block(World* world, int x, int y, int z, BlockType type);
Block get_block(World* world, int x, int y, int z);
Chunk* get_chunk(World* world, int chunk_x, int chunk_y, int chunk_z);
void destroy_world(World* world);
void update_player_physics(World* world);
void load_block_types(const char* filename);
BlockType get_block_type_by_name(const char* name);

// Global block types array
static BlockType block_types[BLOCK_TYPES_MAX];
static char block_names[BLOCK_TYPES_MAX][32];
static float block_hardness[BLOCK_TYPES_MAX];
static float block_colors[BLOCK_TYPES_MAX][3];

void init_world(World* world, int seed) {
    world->seed = seed;
    srand(seed);
    
    // Initialize world dimensions (for a sphere world)
    world->width = WORLD_SIZE;
    world->height = WORLD_SIZE; 
    world->depth = WORLD_SIZE;
    
    // Allocate chunks (for now we'll just make a simple sphere)
    int chunk_count_x = (world->width + CHUNK_SIZE - 1) / CHUNK_SIZE;
    int chunk_count_y = (world->height + CHUNK_SIZE - 1) / CHUNK_SIZE;
    int chunk_count_z = (world->depth + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    world->chunks = malloc(chunk_count_x * sizeof(Chunk**));
    for (int i = 0; i < chunk_count_x; i++) {
        world->chunks[i] = malloc(chunk_count_y * sizeof(Chunk*));
        for (int j = 0; j < chunk_count_y; j++) {
            world->chunks[i][j] = malloc(chunk_count_z * sizeof(Chunk));
            
            for (int k = 0; k < chunk_count_z; k++) {
                world->chunks[i][j][k].x = i;
                world->chunks[i][j][k].y = j;
                world->chunks[i][j][k].z = k;
                world->chunks[i][j][k].dirty = false;
                
                // Initialize all blocks to air
                for (int x = 0; x < CHUNK_SIZE; x++) {
                    for (int y = 0; y < CHUNK_SIZE; y++) {
                        for (int z = 0; z < CHUNK_SIZE; z++) {
                            world->chunks[i][j][k].blocks[x][y][z].type = BLOCK_AIR;
                            world->chunks[i][j][k].blocks[x][y][z].metadata = 0;
                            world->chunks[i][j][k].blocks[x][y][z].active = false;
                        }
                    }
                }
            }
        }
    }
    
    // Generate the spherical world first
    generate_sphere_world(world);
    
    // Initialize player - find a surface block to start on
    world->player.x = WORLD_SIZE / 2.0f;
    world->player.z = WORLD_SIZE / 2.0f;
    
    // Find the highest solid block at the center position for the player to start on
    world->player.y = 1.0f; // Start at minimum height
    for (int y = world->height - 1; y >= 0; y--) {
        Block block = get_block(world, (int)world->player.x, y, (int)world->player.z);
        if (block.type != BLOCK_AIR) {
            // Place player just above the solid block
            world->player.y = y + 1.8f; // Player height above the block
            break;
        }
    }
    
    world->player.rotation_x = 0.0f;
    world->player.rotation_y = 0.0f;
    world->player.velocity_x = 0.0f;
    world->player.velocity_y = 0.0f;
    world->player.velocity_z = 0.0f;
    world->player.on_ground = true;  // Start on ground
    world->player.jumping = false;
    world->player.selected_block = BLOCK_GRASS;
    
    // Generate biomes
    generate_biomes(world);
    
    printf("World initialized with seed: %d\n", seed);
    printf("Player starting position: (%.1f, %.1f, %.1f)\n", 
           world->player.x, world->player.y, world->player.z);
}

void generate_sphere_world(World* world) {
    int center_x = world->width / 2;
    int center_y = world->height / 2;
    int center_z = world->depth / 2;
    float radius = WORLD_SIZE / 3.0f;
    
    // Create a spherical world
    for (int x = 0; x < world->width; x++) {
        for (int y = 0; y < world->height; y++) {
            for (int z = 0; z < world->depth; z++) {
                float distance = sqrtf(powf(x - center_x, 2) + 
                                      powf(y - center_y, 2) + 
                                      powf(z - center_z, 2));
                
                if (distance <= radius) {
                    // Simple height-based world generation
                    float height_factor = (float)(y - center_y + radius) / (2 * radius);
                    
                    if (height_factor > 0.7f) {
                        // Top layer - snow or grass
                        set_block(world, x, y, z, (height_factor > 0.85f) ? BLOCK_SNOW : BLOCK_GRASS);
                    } else if (height_factor > 0.5f) {
                        // Middle layer - dirt
                        set_block(world, x, y, z, BLOCK_DIRT);
                    } else {
                        // Bottom layer - stone
                        set_block(world, x, y, z, BLOCK_STONE);
                    }
                }
            }
        }
    }
    
    printf("Spherical world generated\n");
}

void generate_biomes(World* world) {
    // Allocate biome array
    int biome_count = (world->width / 16) * (world->depth / 16);
    world->biomes = malloc(biome_count * sizeof(Biome));
    
    // Simple biome generation
    for (int i = 0; i < biome_count; i++) {
        // Random biome assignment for now
        int biome_type = rand() % 6; // 6 biome types
        
        world->biomes[i].type = biome_type;
        world->biomes[i].temperature = (float)(rand() % 100) / 100.0f;
        world->biomes[i].humidity = (float)(rand() % 100) / 100.0f;
        
        switch (biome_type) {
            case BIOME_PLAINS:
                world->biomes[i].ground_block = BLOCK_GRASS;
                world->biomes[i].subsurface_block = BLOCK_DIRT;
                break;
            case BIOME_FOREST:
                world->biomes[i].ground_block = BLOCK_GRASS;
                world->biomes[i].subsurface_block = BLOCK_DIRT;
                break;
            case BIOME_DESERT:
                world->biomes[i].ground_block = BLOCK_SAND;
                world->biomes[i].subsurface_block = BLOCK_SAND;
                break;
            case BIOME_TUNDRA:
                world->biomes[i].ground_block = BLOCK_SNOW;
                world->biomes[i].subsurface_block = BLOCK_ICE;
                break;
            case BIOME_MOUNTAINS:
                world->biomes[i].ground_block = BLOCK_STONE;
                world->biomes[i].subsurface_block = BLOCK_STONE;
                break;
            case BIOME_OCEAN:
                world->biomes[i].ground_block = BLOCK_WATER;
                world->biomes[i].subsurface_block = BLOCK_SAND;
                break;
        }
        world->biomes[i].elevation_factor = 1.0f;
    }
    
    printf("Biomes generated\n");
}

void set_block(World* world, int x, int y, int z, BlockType type) {
    // Check bounds
    if (x < 0 || x >= world->width || 
        y < 0 || y >= world->height || 
        z < 0 || z >= world->depth) {
        return;
    }
    
    // Calculate chunk coordinates
    int chunk_x = x / CHUNK_SIZE;
    int chunk_y = y / CHUNK_SIZE;
    int chunk_z = z / CHUNK_SIZE;
    
    // Calculate local coordinates within chunk
    int local_x = x % CHUNK_SIZE;
    int local_y = y % CHUNK_SIZE;
    int local_z = z % CHUNK_SIZE;
    
    // Get chunk and set block
    Chunk* chunk = get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (chunk != NULL) {
        chunk->blocks[local_x][local_y][local_z].type = type;
        chunk->blocks[local_x][local_y][local_z].active = true;
        chunk->dirty = true;
    }
}

Block get_block(World* world, int x, int y, int z) {
    Block empty_block = {BLOCK_AIR, 0, false};
    
    // Check bounds
    if (x < 0 || x >= world->width || 
        y < 0 || y >= world->height || 
        z < 0 || z >= world->depth) {
        return empty_block;
    }
    
    // Calculate chunk coordinates
    int chunk_x = x / CHUNK_SIZE;
    int chunk_y = y / CHUNK_SIZE;
    int chunk_z = z / CHUNK_SIZE;
    
    // Calculate local coordinates within chunk
    int local_x = x % CHUNK_SIZE;
    int local_y = y % CHUNK_SIZE;
    int local_z = z % CHUNK_SIZE;
    
    // Get chunk and return block
    Chunk* chunk = get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (chunk != NULL) {
        return chunk->blocks[local_x][local_y][local_z];
    }
    
    return empty_block;
}

Chunk* get_chunk(World* world, int chunk_x, int chunk_y, int chunk_z) {
    // Check bounds
    int max_chunk_x = (world->width + CHUNK_SIZE - 1) / CHUNK_SIZE;
    int max_chunk_y = (world->height + CHUNK_SIZE - 1) / CHUNK_SIZE;
    int max_chunk_z = (world->depth + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    if (chunk_x < 0 || chunk_x >= max_chunk_x || 
        chunk_y < 0 || chunk_y >= max_chunk_y || 
        chunk_z < 0 || chunk_z >= max_chunk_z) {
        return NULL;
    }
    
    return &world->chunks[chunk_x][chunk_y][chunk_z];
}

void destroy_world(World* world) {
    if (world->chunks) {
        int chunk_count_x = (world->width + CHUNK_SIZE - 1) / CHUNK_SIZE;
        int chunk_count_y = (world->height + CHUNK_SIZE - 1) / CHUNK_SIZE;
        
        for (int i = 0; i < chunk_count_x; i++) {
            for (int j = 0; j < chunk_count_y; j++) {
                free(world->chunks[i][j]);
            }
            free(world->chunks[i]);
        }
        free(world->chunks);
        world->chunks = NULL;
    }
    
    if (world->biomes) {
        free(world->biomes);
        world->biomes = NULL;
    }
}

void update_player_physics(World* world) {
    // Apply gravity
    world->player.velocity_y -= 0.05f;  // Gravity
    
    // Store original position
    float original_x = world->player.x;
    float original_y = world->player.y;
    float original_z = world->player.z;
    
    // Update position based on velocity
    world->player.x += world->player.velocity_x;
    world->player.y += world->player.velocity_y;
    world->player.z += world->player.velocity_z;
    
    // Check for collisions with blocks and handle them
    int px = (int)world->player.x;
    int py = (int)world->player.y;
    int pz = (int)world->player.z;
    
    // Check if the player's new position collides with a solid block
    Block current_block = get_block(world, px, py, pz);
    
    // If we're inside a solid block, revert and stop velocity in that direction
    if (current_block.type != BLOCK_AIR) {
        world->player.x = original_x;
        world->player.y = original_y;
        world->player.z = original_z;
        
        // Stop all movement
        world->player.velocity_x = 0.0f;
        world->player.velocity_y = 0.0f;
        world->player.velocity_z = 0.0f;
        world->player.on_ground = false;
    }
    
    // Check if player is on ground (block directly below)
    Block below_block = get_block(world, px, py - 1, pz);
    world->player.on_ground = (below_block.type != BLOCK_AIR);
    
    // If on ground and falling (negative y velocity), stop the fall
    if (world->player.on_ground && world->player.velocity_y < 0) {
        world->player.velocity_y = 0.0f;
        // Position player exactly on top of the ground block
        world->player.y = (float)py;
    }
    
    // Check if player is hitting a ceiling (block directly above)
    Block above_block = get_block(world, px, py + 1, pz);
    if (above_block.type != BLOCK_AIR && world->player.velocity_y > 0) {
        world->player.velocity_y = 0.0f;
    }
    
    // Check horizontal collisions by testing the blocks in the movement direction
    if (world->player.velocity_x != 0) {
        Block x_collision = get_block(world, 
            (int)(original_x + world->player.velocity_x), 
            py, 
            pz);
        if (x_collision.type != BLOCK_AIR) {
            world->player.x = original_x;  // Revert x movement
            world->player.velocity_x = 0.0f;
        }
    }
    
    if (world->player.velocity_z != 0) {
        Block z_collision = get_block(world, 
            px, 
            py, 
            (int)(original_z + world->player.velocity_z));
        if (z_collision.type != BLOCK_AIR) {
            world->player.z = original_z;  // Revert z movement
            world->player.velocity_z = 0.0f;
        }
    }
    
    // Keep player in world bounds
    if (world->player.x < 0) world->player.x = 0;
    if (world->player.x >= world->width) world->player.x = world->width - 1;
    if (world->player.z < 0) world->player.z = 0;
    if (world->player.z >= world->depth) world->player.z = world->depth - 1;
    
    // If player falls below world, reset to starting position
    if (world->player.y < 0) {
        world->player.y = 1.8f; // Reset to surface level
        world->player.velocity_y = 0.0f;
    }
}

void load_block_types(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Could not open block_types.csv\n");
        return;
    }
    
    // Skip header line
    char header[256];
    fgets(header, sizeof(header), file);
    
    // Read block definitions
    int id;
    char name[32];
    float hardness;
    float r, g, b;
    
    while (fscanf(file, "%d,%31[^,],%f,%f,%f,%f\n", &id, name, &hardness, &r, &g, &b) == 6) {
        if (id >= 0 && id < BLOCK_TYPES_MAX) {
            block_types[id] = id;
            strcpy(block_names[id], name);
            block_hardness[id] = hardness;
            block_colors[id][0] = r;
            block_colors[id][1] = g;
            block_colors[id][2] = b;
        }
    }
    
    fclose(file);
    printf("Loaded %d block types from %s\n", BLOCK_TYPES_MAX, filename);
}

BlockType get_block_type_by_name(const char* name) {
    for (int i = 0; i < BLOCK_TYPES_MAX; i++) {
        if (strcmp(block_names[i], name) == 0) {
            return block_types[i];
        }
    }
    return BLOCK_AIR;  // Default to air if not found
}