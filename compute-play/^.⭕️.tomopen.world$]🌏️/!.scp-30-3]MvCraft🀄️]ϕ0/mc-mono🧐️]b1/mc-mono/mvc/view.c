#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <math.h>
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
    void* biomes;  // Using void* since we don't need biome details in view
    int width, height, depth;
    Player player;
    int seed;
} World;

typedef struct {
    float x, y, z;
    float look_x, look_y, look_z;
    float pitch, yaw, roll;
} Camera;

// Behavior tree node types (simplified for view.c) - we use void* to avoid complex definitions
typedef enum {
    BT_ACTION,
    BT_CONDITION,
    BT_SELECTOR,
    BT_SEQUENCE
} BehaviorNodeType;

// Behavior tree node structure (simplified for view.c)
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

// External reference to main game controller (defined in main.c)
extern GameController game_controller;

// Function declarations for model functions (defined in model.c) - needed for view.c
Chunk* get_chunk(World* world, int chunk_x, int chunk_y, int chunk_z);

// Function declarations for renderer
void setup_opengl();
void render_world(World* world, Camera* camera);
void update_projection_matrix(int width, int height);
void render_chunk(Chunk* chunk, int chunk_x, int chunk_y, int chunk_z);
void render_player(Player* player);
void render_crosshair();
void render_ui();
void display();
void reshape(int width, int height);
void idle();

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
                Chunk* chunk = get_chunk(world, cx, cy, cz); // Use the actual function
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

// Note: display, reshape, and idle functions are defined in main.c
// to avoid multiple definition errors during linking