#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <GL/glut.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// Camera structure
typedef struct {
    float x, y, z;        // Position
    float look_x, look_y, look_z;  // Look direction
    float pitch, yaw, roll;  // Rotation angles
} Camera;

// World structure
typedef struct {
    Chunk*** chunks;      // 3D array of chunks (x, y, z)
    Biome* biomes;        // Biome data
    int width, height, depth;  // World dimensions
    Player player;
    int seed;             // World generation seed
} World;

// Event types for the event system
typedef enum {
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_MOUSE_MOVE,
    EVENT_TICK
} EventType;

// Event structure
typedef struct {
    EventType type;
    union {
        struct {
            unsigned char key;
            int x, y;
        } keyboard;
        struct {
            int x, y, z;
        } mouse;
    } data;
} Event;

// Behavior tree node types
typedef enum {
    BT_ACTION,
    BT_CONDITION,
    BT_SELECTOR,
    BT_SEQUENCE
} BehaviorNodeType;

// Behavior tree node structure
typedef struct BehaviorNode {
    BehaviorNodeType type;
    int (*condition_func)(void* context);
    void (*action_func)(void* context);
    struct BehaviorNode** children;
    int num_children;
} BehaviorNode;

// Main game controller structure
typedef struct {
    World world;
    Camera camera;
    BehaviorNode* behavior_tree;
    float last_update_time;
    
    // Input state
    int keys[256];
    int special_keys[256];
} GameController;

// Function declarations for world
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

// Function declarations for renderer
void setup_opengl();
void render_world(World* world, Camera* camera);
void update_projection_matrix(int width, int height);
void render_chunk(Chunk* chunk, int chunk_x, int chunk_y, int chunk_z);
void render_player(Player* player);
void render_crosshair();
void render_ui();

// Function declarations for controller
void init_game(GameController* controller);
void handle_keyboard_input(unsigned char key, int x, int y, GameController* controller);
void handle_keyboard_release(unsigned char key, int x, int y, GameController* controller);
void handle_special_keys(int key, int x, int y, GameController* controller);
void handle_special_keys_release(int key, int x, int y, GameController* controller);
void update_game_logic(GameController* controller);
void execute_behavior_tree(GameController* controller);
void dispatch_event(Event* event, GameController* controller);
void process_events(GameController* controller);

// Global game state
GameController game_controller;

// Global block types array
static BlockType block_types[BLOCK_TYPES_MAX];
static char block_names[BLOCK_TYPES_MAX][32];
static float block_hardness[BLOCK_TYPES_MAX];
static float block_colors[BLOCK_TYPES_MAX][3];

// WORLD IMPLEMENTATION
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

// RENDERER IMPLEMENTATION
void setup_opengl() {
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // Set up lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    GLfloat light_position[] = {10.0f, 10.0f, 10.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    
    // Set clear color to sky blue
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);  // Sky blue
    
    // Enable color material
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    
    // Enable smooth shading
    glShadeModel(GL_SMOOTH);
    
    printf("OpenGL initialized\n");
}

void render_world(World* world, Camera* camera) {
    // Clear the screen and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Set up camera
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Apply camera transformation
    gluLookAt(
        camera->x, camera->y, camera->z,  // Camera position
        camera->x + camera->look_x, 
        camera->y + camera->look_y, 
        camera->z + camera->look_z,      // Look at point
        0.0f, 1.0f, 0.0f                 // Up vector
    );
    
    // Render all chunks
    int chunk_count_x = (world->width + CHUNK_SIZE - 1) / CHUNK_SIZE;
    int chunk_count_y = (world->height + CHUNK_SIZE - 1) / CHUNK_SIZE;
    int chunk_count_z = (world->depth + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    for (int cx = 0; cx < chunk_count_x; cx++) {
        for (int cy = 0; cy < chunk_count_y; cy++) {
            for (int cz = 0; cz < chunk_count_z; cz++) {
                Chunk* chunk = get_chunk(world, cx, cy, cz);
                if (chunk != NULL && chunk->dirty) {
                    render_chunk(chunk, cx, cy, cz);
                }
            }
        }
    }
    
    // Render the player
    render_player(&world->player);
    
    // Render UI elements
    render_ui();
    
    // Render crosshair
    render_crosshair();
    
    // Swap buffers
    glutSwapBuffers();
}

void render_chunk(Chunk* chunk, int chunk_x, int chunk_y, int chunk_z) {
    // Render each block in the chunk
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                Block* block = &chunk->blocks[x][y][z];
                
                if (block->type != BLOCK_AIR) {
                    // Calculate world position
                    float wx = chunk_x * CHUNK_SIZE + x;
                    float wy = chunk_y * CHUNK_SIZE + y;
                    float wz = chunk_z * CHUNK_SIZE + z;
                    
                    // Set color based on block type
                    switch (block->type) {
                        case BLOCK_GRASS:
                            glColor3f(0.3f, 0.8f, 0.3f);  // Green
                            break;
                        case BLOCK_DIRT:
                            glColor3f(0.6f, 0.4f, 0.2f);  // Brown
                            break;
                        case BLOCK_STONE:
                            glColor3f(0.5f, 0.5f, 0.5f);  // Gray
                            break;
                        case BLOCK_WATER:
                            glColor3f(0.2f, 0.4f, 0.9f);  // Blue
                            break;
                        case BLOCK_SAND:
                            glColor3f(0.9f, 0.8f, 0.4f);  // Yellow
                            break;
                        case BLOCK_WOOD:
                            glColor3f(0.6f, 0.4f, 0.1f);  // Brown
                            break;
                        case BLOCK_LEAVES:
                            glColor3f(0.2f, 0.7f, 0.2f);  // Green
                            break;
                        case BLOCK_SNOW:
                            glColor3f(0.95f, 0.95f, 0.95f);  // White
                            break;
                        case BLOCK_ICE:
                            glColor3f(0.7f, 0.8f, 0.95f);  // Light blue
                            break;
                        default:
                            glColor3f(0.8f, 0.8f, 0.8f);  // Default gray
                    }
                    
                    // Draw cube at position (wx, wy, wz)
                    glPushMatrix();
                    glTranslatef(wx, wy, wz);
                    
                    glBegin(GL_QUADS);
                        // Front face
                        glVertex3f(0.0f, 0.0f, 1.0f);
                        glVertex3f(1.0f, 0.0f, 1.0f);
                        glVertex3f(1.0f, 1.0f, 1.0f);
                        glVertex3f(0.0f, 1.0f, 1.0f);
                        
                        // Back face
                        glVertex3f(1.0f, 0.0f, 0.0f);
                        glVertex3f(0.0f, 0.0f, 0.0f);
                        glVertex3f(0.0f, 1.0f, 0.0f);
                        glVertex3f(1.0f, 1.0f, 0.0f);
                        
                        // Top face
                        glVertex3f(0.0f, 1.0f, 0.0f);
                        glVertex3f(1.0f, 1.0f, 0.0f);
                        glVertex3f(1.0f, 1.0f, 1.0f);
                        glVertex3f(0.0f, 1.0f, 1.0f);
                        
                        // Bottom face
                        glVertex3f(0.0f, 0.0f, 0.0f);
                        glVertex3f(0.0f, 0.0f, 1.0f);
                        glVertex3f(1.0f, 0.0f, 1.0f);
                        glVertex3f(1.0f, 0.0f, 0.0f);
                        
                        // Right face
                        glVertex3f(1.0f, 0.0f, 0.0f);
                        glVertex3f(1.0f, 0.0f, 1.0f);
                        glVertex3f(1.0f, 1.0f, 1.0f);
                        glVertex3f(1.0f, 1.0f, 0.0f);
                        
                        // Left face
                        glVertex3f(0.0f, 0.0f, 0.0f);
                        glVertex3f(0.0f, 1.0f, 0.0f);
                        glVertex3f(0.0f, 1.0f, 1.0f);
                        glVertex3f(0.0f, 0.0f, 1.0f);
                    glEnd();
                    
                    glPopMatrix();
                }
            }
        }
    }
}

void render_player(Player* player) {
    // Render a simple representation of the player
    glPushMatrix();
    glTranslatef(player->x, player->y, player->z);
    
    // Player body
    glColor3f(0.8f, 0.5f, 0.2f);  // Orange
    glutSolidCube(0.8);
    
    glPopMatrix();
}

void render_crosshair() {
    // Switch to orthographic projection for UI
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT));
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth testing for UI
    glDisable(GL_DEPTH_TEST);
    
    // Draw crosshair
    int width = glutGet(GLUT_WINDOW_WIDTH);
    int height = glutGet(GLUT_WINDOW_HEIGHT);
    int center_x = width / 2;
    int center_y = height / 2;
    int length = 10;
    
    glColor3f(1.0f, 1.0f, 1.0f);  // White
    glBegin(GL_LINES);
        // Horizontal line
        glVertex2i(center_x - length, center_y);
        glVertex2i(center_x + length, center_y);
        
        // Vertical line
        glVertex2i(center_x, center_y - length);
        glVertex2i(center_x, center_y + length);
    glEnd();
    
    // Re-enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void render_ui() {
    // Switch to orthographic projection for UI
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT));
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth testing for UI
    glDisable(GL_DEPTH_TEST);
    
    // Render minimap in top-right corner
    int map_size = 200;
    int margin = 20;
    int map_x = glutGet(GLUT_WINDOW_WIDTH) - map_size - margin;
    int map_y = glutGet(GLUT_WINDOW_HEIGHT) - map_size - margin;
    
    // Draw minimap background
    glColor3f(0.2f, 0.2f, 0.2f);  // Dark gray
    glBegin(GL_QUADS);
        glVertex2i(map_x, map_y);
        glVertex2i(map_x + map_size, map_y);
        glVertex2i(map_x + map_size, map_y + map_size);
        glVertex2i(map_x, map_y + map_size);
    glEnd();
    
    // Draw minimap border
    glColor3f(1.0f, 1.0f, 1.0f);  // White
    glBegin(GL_LINE_LOOP);
        glVertex2i(map_x, map_y);
        glVertex2i(map_x + map_size, map_y);
        glVertex2i(map_x + map_size, map_y + map_size);
        glVertex2i(map_x, map_y + map_size);
    glEnd();
    
    // Draw player position on minimap (as red dot)
    float player_x = game_controller.world.player.x;
    float player_z = game_controller.world.player.z;
    
    // Scale from world coordinates to minimap coordinates
    float world_half_size = WORLD_SIZE / 2.0f;
    float scale_factor = (float)map_size / (float)WORLD_SIZE;
    
    int player_minimap_x = map_x + (int)((player_x / WORLD_SIZE) * map_size);
    int player_minimap_y = map_y + (int)((player_z / WORLD_SIZE) * map_size);
    
    glColor3f(1.0f, 0.0f, 0.0f);  // Red
    glPointSize(5.0f);
    glBegin(GL_POINTS);
        glVertex2i(player_minimap_x, player_minimap_y);
    glEnd();
    
    // Draw coordinate text
    glColor3f(1.0f, 1.0f, 1.0f);  // White
    
    // We'll use GLUT's bitmap font to draw text
    char coord_text[100];
    sprintf(coord_text, "X: %.1f, Y: %.1f, Z: %.1f", 
            game_controller.world.player.x, 
            game_controller.world.player.y, 
            game_controller.world.player.z);
    
    glRasterPos2i(map_x, map_y - 20);  // Position text below minimap
    for (char* c = coord_text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Re-enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void update_projection_matrix(int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Set up perspective projection
    gluPerspective(45.0, (double)width / (double)height, 0.1, 1000.0);
    
    glMatrixMode(GL_MODELVIEW);
}

// CONTROLLER IMPLEMENTATION
void init_game(GameController* controller) {
    // Initialize the world with a random seed
    init_world(&controller->world, time(NULL));
    
    // Initialize camera to follow player
    controller->camera.x = controller->world.player.x;
    controller->camera.y = controller->world.player.y + 2.0f;  // Slightly above player
    controller->camera.z = controller->world.player.z;
    controller->camera.pitch = 0.0f;
    controller->camera.yaw = 0.0f;
    controller->camera.roll = 0.0f;
    
    // Initialize input states
    for (int i = 0; i < 256; i++) {
        controller->keys[i] = 0;
        controller->special_keys[i] = 0;
    }
    
    // Initialize behavior tree
    controller->behavior_tree = NULL;
    controller->last_update_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    
    // Load block types
    load_block_types("block_types.csv");
    
    printf("Game initialized\n");
}

void handle_keyboard_input(unsigned char key, int x, int y, GameController* controller) {
    controller->keys[key] = 1;
    
    // Handle specific keys
    switch (key) {
        case ' ':  // Space - jump
            if (controller->world.player.on_ground) {
                controller->world.player.velocity_y = 0.15f;  // Jump velocity
                controller->world.player.jumping = true;
                controller->world.player.on_ground = false;
            }
            break;
            
        case 'g':  // Reset camera
        case 'G':
            controller->camera.x = controller->world.player.x;
            controller->camera.y = controller->world.player.y + 2.0f;
            controller->camera.z = controller->world.player.z;
            controller->camera.pitch = 0.0f;
            controller->camera.yaw = 0.0f;
            break;
            
        case 'c':  // Move picker down
        case 'C':
            controller->world.player.y -= 1.0f;
            break;
            
        case 'v':  // Move picker up
        case 'V':
            controller->world.player.y += 1.0f;
            break;
    }
    
    // Dispatch event
    Event event;
    event.type = EVENT_KEY_PRESS;
    event.data.keyboard.key = key;
    event.data.keyboard.x = x;
    event.data.keyboard.y = y;
    dispatch_event(&event, controller);
}

void handle_keyboard_release(unsigned char key, int x, int y, GameController* controller) {
    controller->keys[key] = 0;
    
    if (key == ' ') {  // Space released
        controller->world.player.jumping = false;
    }
    
    // Dispatch event
    Event event;
    event.type = EVENT_KEY_RELEASE;
    event.data.keyboard.key = key;
    event.data.keyboard.x = x;
    event.data.keyboard.y = y;
    dispatch_event(&event, controller);
}

void handle_special_keys(int key, int x, int y, GameController* controller) {
    controller->special_keys[key] = 1;
    
    // Handle camera rotation
    switch (key) {
        case GLUT_KEY_LEFT:   // Rotate head left
            controller->camera.yaw -= 5.0f;
            break;
        case GLUT_KEY_RIGHT:  // Rotate head right
            controller->camera.yaw += 5.0f;
            break;
    }
    
    // Dispatch event
    Event event;
    event.type = EVENT_KEY_PRESS;
    event.data.keyboard.key = key + 256;  // Use extended range for special keys
    event.data.keyboard.x = x;
    event.data.keyboard.y = y;
    dispatch_event(&event, controller);
}

void handle_special_keys_release(int key, int x, int y, GameController* controller) {
    controller->special_keys[key] = 0;
    
    // Dispatch event
    Event event;
    event.type = EVENT_KEY_RELEASE;
    event.data.keyboard.key = key + 256;  // Use extended range for special keys
    event.data.keyboard.x = x;
    event.data.keyboard.y = y;
    dispatch_event(&event, controller);
}

void update_game_logic(GameController* controller) {
    float current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float delta_time = current_time - controller->last_update_time;
    controller->last_update_time = current_time;
    
    // Handle player movement based on keys
    float move_speed = 0.1f;
    
    // Selector movement (arrow keys)
    if (controller->keys['w'] || controller->special_keys[GLUT_KEY_UP]) {
        controller->world.player.x += move_speed * cosf(controller->world.player.rotation_x * M_PI / 180.0f);
        controller->world.player.z += move_speed * sinf(controller->world.player.rotation_x * M_PI / 180.0f);
    }
    if (controller->keys['s'] || controller->special_keys[GLUT_KEY_DOWN]) {
        controller->world.player.x -= move_speed * cosf(controller->world.player.rotation_x * M_PI / 180.0f);
        controller->world.player.z -= move_speed * sinf(controller->world.player.rotation_x * M_PI / 180.0f);
    }
    if (controller->keys['a'] || controller->special_keys[GLUT_KEY_LEFT]) {
        controller->world.player.x += move_speed * cosf((controller->world.player.rotation_x - 90) * M_PI / 180.0f);
        controller->world.player.z += move_speed * sinf((controller->world.player.rotation_x - 90) * M_PI / 180.0f);
    }
    if (controller->keys['d'] || controller->special_keys[GLUT_KEY_RIGHT]) {
        controller->world.player.x += move_speed * cosf((controller->world.player.rotation_x + 90) * M_PI / 180.0f);
        controller->world.player.z += move_speed * sinf((controller->world.player.rotation_x + 90) * M_PI / 180.0f);
    }
    
    // Camera movement (WASD)
    if (controller->keys['W'] || controller->keys['w']) {
        controller->camera.x += 0.1f * cosf(controller->camera.yaw * M_PI / 180.0f);
        controller->camera.z += 0.1f * sinf(controller->camera.yaw * M_PI / 180.0f);
    }
    if (controller->keys['S'] || controller->keys['s']) {
        controller->camera.x -= 0.1f * cosf(controller->camera.yaw * M_PI / 180.0f);
        controller->camera.z -= 0.1f * sinf(controller->camera.yaw * M_PI / 180.0f);
    }
    if (controller->keys['A'] || controller->keys['a']) {
        controller->camera.x += 0.1f * cosf((controller->camera.yaw - 90) * M_PI / 180.0f);
        controller->camera.z += 0.1f * sinf((controller->camera.yaw - 90) * M_PI / 180.0f);
    }
    if (controller->keys['D'] || controller->keys['d']) {
        controller->camera.x += 0.1f * cosf((controller->camera.yaw + 90) * M_PI / 180.0f);
        controller->camera.z += 0.1f * sinf((controller->camera.yaw + 90) * M_PI / 180.0f);
    }
    
    // Q/E for camera rotation
    if (controller->keys['q'] || controller->keys['Q']) {
        controller->camera.yaw -= 1.0f;
    }
    if (controller->keys['e'] || controller->keys['E']) {
        controller->camera.yaw += 1.0f;
    }
    
    // Update player physics
    update_player_physics(&controller->world);
    
    // Update camera to follow player
    controller->camera.x = controller->world.player.x;
    controller->camera.y = controller->world.player.y + 1.6f;  // Eye level
    controller->camera.z = controller->world.player.z;
    
    // Calculate look direction based on player rotation
    controller->camera.look_x = cosf(controller->world.player.rotation_x * M_PI / 180.0f) * cosf(controller->world.player.rotation_y * M_PI / 180.0f);
    controller->camera.look_y = sinf(controller->world.player.rotation_y * M_PI / 180.0f);
    controller->camera.look_z = sinf(controller->world.player.rotation_x * M_PI / 180.0f) * cosf(controller->world.player.rotation_y * M_PI / 180.0f);
    
    // Process any pending events
    process_events(controller);
    
    // Execute behavior tree if it exists
    if (controller->behavior_tree) {
        execute_behavior_tree(controller);
    }
    
    // Dispatch tick event
    Event tick_event;
    tick_event.type = EVENT_TICK;
    dispatch_event(&tick_event, controller);
}

void execute_behavior_tree(GameController* controller) {
    // For now, this is a placeholder
    // Actual behavior tree execution would go here
}

void dispatch_event(Event* event, GameController* controller) {
    // In a full implementation, events would be added to a queue and processed
    // For now, we'll just handle them directly
    process_events(controller);
}

void process_events(GameController* controller) {
    // Process all pending events
    // This would typically involve a queue of events to process
}

// MAIN IMPLEMENTATION
void display() {
    render_world(&game_controller.world, &game_controller.camera);
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    update_projection_matrix(width, height);
}

void keyboard(unsigned char key, int x, int y) {
    handle_keyboard_input(key, x, y, &game_controller);
}

void keyboard_up(unsigned char key, int x, int y) {
    handle_keyboard_release(key, x, y, &game_controller);
}

void special_keys(int key, int x, int y) {
    handle_special_keys(key, x, y, &game_controller);
}

void special_keys_up(int key, int x, int y) {
    handle_special_keys_release(key, x, y, &game_controller);
}

void idle() {
    update_game_logic(&game_controller);
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Mojicraft - Minecraft Clone");
    
    // Initialize game
    init_game(&game_controller);
    
    // Set GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboard_up);
    glutSpecialFunc(special_keys);
    glutSpecialUpFunc(special_keys_up);
    glutIdleFunc(idle);
    
    // Set up OpenGL
    setup_opengl();
    
    printf("Mojicraft initialized!\n");
    printf("Controls:\n");
    printf("- Arrow keys: Move selector\n");
    printf("- C: Move picker down, V: Move picker up\n");
    printf("- WASD: Move camera\n");
    printf("- Q/E: Rotate head left/right\n");
    printf("- G: Reset camera\n");
    printf("- Space: Jump\n");
    
    // Start the game loop
    glutMainLoop();
    
    return 0;
}