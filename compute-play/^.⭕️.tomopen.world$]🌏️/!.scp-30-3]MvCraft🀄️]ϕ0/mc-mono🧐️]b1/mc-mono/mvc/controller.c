#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Re-declare types and structures (duplicated for simplicity without headers)
#define WORLD_SIZE 64
#define CHUNK_SIZE 16
#define BLOCK_TYPES_MAX 256

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

typedef struct {
    BlockType type;
    uint8_t metadata;
    bool active;
} Block;

typedef struct {
    Block blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    int x, y, z;
    bool dirty;
} Chunk;

typedef struct {
    float x, y, z;
    float rotation_x;
    float rotation_y;
    float velocity_x, velocity_y, velocity_z;
    bool on_ground;
    bool jumping;
    int selected_block;
} Player;

typedef struct {
    Chunk*** chunks;
    void* biomes;
    int width, height, depth;
    Player player;
    int seed;
} World;

typedef struct {
    float x, y, z;
    float look_x, look_y, look_z;
    float pitch, yaw, roll;
} Camera;

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

// External references (to be defined in main.c)
extern GameController game_controller;

// Function declarations for model functions (defined in model.c)
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

// Function declarations for controller
void init_game();
void handle_keyboard_input(unsigned char key, int x, int y);
void handle_keyboard_release(unsigned char key, int x, int y);
void handle_special_keys(int key, int x, int y);
void handle_special_keys_release(int key, int x, int y);
void update_game_logic();
void execute_behavior_tree();
void dispatch_event(Event* event);
void process_events();

void init_game() {
    // Initialize the world with a random seed
    init_world(&game_controller.world, time(NULL));
    
    // Initialize camera to follow player
    game_controller.camera.x = game_controller.world.player.x;
    game_controller.camera.y = game_controller.world.player.y + 2.0f;  // Slightly above player
    game_controller.camera.z = game_controller.world.player.z;
    game_controller.camera.pitch = 0.0f;
    game_controller.camera.yaw = 0.0f;
    game_controller.camera.roll = 0.0f;
    
    // Initialize input states
    for (int i = 0; i < 256; i++) {
        game_controller.keys[i] = 0;
        game_controller.special_keys[i] = 0;
    }
    
    // Initialize behavior tree
    game_controller.behavior_tree = NULL;
    game_controller.last_update_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    
    // Load block types
    load_block_types("../block_types.csv");
    
    printf("Game initialized\n");
}

void handle_keyboard_input(unsigned char key, int x, int y) {
    game_controller.keys[key] = 1;
    
    // Handle specific keys
    switch (key) {
        case ' ':  // Space - jump
            if (game_controller.world.player.on_ground) {
                game_controller.world.player.velocity_y = 0.15f;  // Jump velocity
                game_controller.world.player.jumping = true;
                game_controller.world.player.on_ground = false;
            }
            break;
            
        case 'g':  // Reset camera
        case 'G':
            game_controller.camera.x = game_controller.world.player.x;
            game_controller.camera.y = game_controller.world.player.y + 2.0f;
            game_controller.camera.z = game_controller.world.player.z;
            game_controller.camera.pitch = 0.0f;
            game_controller.camera.yaw = 0.0f;
            break;
            
        case 'c':  // Move picker down
        case 'C':
            game_controller.world.player.y -= 1.0f;
            break;
            
        case 'v':  // Move picker up
        case 'V':
            game_controller.world.player.y += 1.0f;
            break;
    }
    
    // Dispatch event
    Event event;
    event.type = EVENT_KEY_PRESS;
    event.data.keyboard.key = key;
    event.data.keyboard.x = x;
    event.data.keyboard.y = y;
    dispatch_event(&event);
}

void handle_keyboard_release(unsigned char key, int x, int y) {
    game_controller.keys[key] = 0;
    
    if (key == ' ') {  // Space released
        game_controller.world.player.jumping = false;
    }
    
    // Dispatch event
    Event event;
    event.type = EVENT_KEY_RELEASE;
    event.data.keyboard.key = key;
    event.data.keyboard.x = x;
    event.data.keyboard.y = y;
    dispatch_event(&event);
}

void handle_special_keys(int key, int x, int y) {
    game_controller.special_keys[key] = 1;
    
    // Handle camera rotation
    switch (key) {
        case GLUT_KEY_LEFT:   // Rotate head left
            game_controller.camera.yaw -= 5.0f;
            break;
        case GLUT_KEY_RIGHT:  // Rotate head right
            game_controller.camera.yaw += 5.0f;
            break;
    }
    
    // Dispatch event
    Event event;
    event.type = EVENT_KEY_PRESS;
    event.data.keyboard.key = key + 256;  // Use extended range for special keys
    event.data.keyboard.x = x;
    event.data.keyboard.y = y;
    dispatch_event(&event);
}

void handle_special_keys_release(int key, int x, int y) {
    game_controller.special_keys[key] = 0;
    
    // Dispatch event
    Event event;
    event.type = EVENT_KEY_RELEASE;
    event.data.keyboard.key = key + 256;  // Use extended range for special keys
    event.data.keyboard.x = x;
    event.data.keyboard.y = y;
    dispatch_event(&event);
}

void update_game_logic() {
    float current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float delta_time = current_time - game_controller.last_update_time;
    game_controller.last_update_time = current_time;
    
    // Handle player movement based on keys
    float move_speed = 0.1f;
    
    // Selector movement (arrow keys)
    if (game_controller.keys['w'] || game_controller.special_keys[GLUT_KEY_UP]) {
        game_controller.world.player.x += move_speed * cosf(game_controller.world.player.rotation_x * M_PI / 180.0f);
        game_controller.world.player.z += move_speed * sinf(game_controller.world.player.rotation_x * M_PI / 180.0f);
    }
    if (game_controller.keys['s'] || game_controller.special_keys[GLUT_KEY_DOWN]) {
        game_controller.world.player.x -= move_speed * cosf(game_controller.world.player.rotation_x * M_PI / 180.0f);
        game_controller.world.player.z -= move_speed * sinf(game_controller.world.player.rotation_x * M_PI / 180.0f);
    }
    if (game_controller.keys['a'] || game_controller.special_keys[GLUT_KEY_LEFT]) {
        game_controller.world.player.x += move_speed * cosf((game_controller.world.player.rotation_x - 90) * M_PI / 180.0f);
        game_controller.world.player.z += move_speed * sinf((game_controller.world.player.rotation_x - 90) * M_PI / 180.0f);
    }
    if (game_controller.keys['d'] || game_controller.special_keys[GLUT_KEY_RIGHT]) {
        game_controller.world.player.x += move_speed * cosf((game_controller.world.player.rotation_x + 90) * M_PI / 180.0f);
        game_controller.world.player.z += move_speed * sinf((game_controller.world.player.rotation_x + 90) * M_PI / 180.0f);
    }
    
    // Camera movement (WASD)
    if (game_controller.keys['W'] || game_controller.keys['w']) {
        game_controller.camera.x += 0.1f * cosf(game_controller.camera.yaw * M_PI / 180.0f);
        game_controller.camera.z += 0.1f * sinf(game_controller.camera.yaw * M_PI / 180.0f);
    }
    if (game_controller.keys['S'] || game_controller.keys['s']) {
        game_controller.camera.x -= 0.1f * cosf(game_controller.camera.yaw * M_PI / 180.0f);
        game_controller.camera.z -= 0.1f * sinf(game_controller.camera.yaw * M_PI / 180.0f);
    }
    if (game_controller.keys['A'] || game_controller.keys['a']) {
        game_controller.camera.x += 0.1f * cosf((game_controller.camera.yaw - 90) * M_PI / 180.0f);
        game_controller.camera.z += 0.1f * sinf((game_controller.camera.yaw - 90) * M_PI / 180.0f);
    }
    if (game_controller.keys['D'] || game_controller.keys['d']) {
        game_controller.camera.x += 0.1f * cosf((game_controller.camera.yaw + 90) * M_PI / 180.0f);
        game_controller.camera.z += 0.1f * sinf((game_controller.camera.yaw + 90) * M_PI / 180.0f);
    }
    
    // Q/E for camera rotation
    if (game_controller.keys['q'] || game_controller.keys['Q']) {
        game_controller.camera.yaw -= 1.0f;
    }
    if (game_controller.keys['e'] || game_controller.keys['E']) {
        game_controller.camera.yaw += 1.0f;
    }
    
    // Update player physics
    update_player_physics(&game_controller.world);
    
    // Update camera to follow player
    game_controller.camera.x = game_controller.world.player.x;
    game_controller.camera.y = game_controller.world.player.y + 1.6f;  // Eye level
    game_controller.camera.z = game_controller.world.player.z;
    
    // Calculate look direction based on player rotation
    game_controller.camera.look_x = cosf(game_controller.world.player.rotation_x * M_PI / 180.0f) * cosf(game_controller.world.player.rotation_y * M_PI / 180.0f);
    game_controller.camera.look_y = sinf(game_controller.world.player.rotation_y * M_PI / 180.0f);
    game_controller.camera.look_z = sinf(game_controller.world.player.rotation_x * M_PI / 180.0f) * cosf(game_controller.world.player.rotation_y * M_PI / 180.0f);
    
    // Process any pending events
    process_events();
    
    // Execute behavior tree if it exists
    if (game_controller.behavior_tree) {
        execute_behavior_tree();
    }
    
    // Dispatch tick event
    Event tick_event;
    tick_event.type = EVENT_TICK;
    dispatch_event(&tick_event);
}

void execute_behavior_tree() {
    // For now, this is a placeholder
    // Actual behavior tree execution would go here
}

void dispatch_event(Event* event) {
    // In a full implementation, events would be added to a queue and processed
    // For now, we'll just handle them directly
    process_events();
}

void process_events() {
    // Process all pending events
    // This would typically involve a queue of events to process
}