/*
 * scenes.c - Scene management
 * Based on RMMV's rpg_scenes.js functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <GL/glut.h>

// Forward declarations for camera functions
int init_camera_with_fallback(void);
int update_camera_frame(void);
unsigned char* get_camera_frame(void);
void get_camera_dimensions(int *width, int *height);
void cleanup_camera_resources(void);

// Forward declarations for manager functions
typedef struct GameMap GameMap;
GameMap* get_map(int index);
int get_map_count(void);

// Scene types
typedef enum {
    SCENE_TITLE,
    SCENE_MAP,
    SCENE_MENU,
    SCENE_BATTLE,
    SCENE_GAMEOVER,
    SCENE_NUM_TYPES
} SceneType;

// Base scene structure
typedef struct {
    SceneType type;
    bool active;
    int fade_sign;      // 0=none, 1=fadeIn, -1=fadeOut
    int fade_duration;
} Scene;

// Title scene
typedef struct {
    Scene base;
    int command_index;
} TitleScene;

// Map scene
typedef struct {
    Scene base;
    int map_id;
    int player_x, player_y, player_z;
    bool player_moving;
    int render_mode; // 0 = text, 1 = plat, 2 = 2d, 3 = 3d
} MapScene;

// Menu scene
typedef struct {
    Scene base;
    int menu_index;
} MenuScene;

// Battle scene
typedef struct {
    Scene base;
    int turn;
    bool player_turn;
} BattleScene;

// Game over scene
typedef struct {
    Scene base;
    int timer;
} GameOverScene;

// Scenes function declarations
void init_scenes(void);
void update_scenes(void);
void render_scenes(void);
void cleanup_scenes(void);

// Scene transition functions
void goto_scene(SceneType type);

// Getters
Scene* get_current_scene(void);

// Scene manager state
static Scene* current_scene = NULL;
static bool scenes_initialized = false;
static int window_width = 800;
static int window_height = 600;
static bool ar_mode_enabled = false; // AR mode flag

// Function to get window dimensions
int get_window_width(void) {
    return window_width;
}

int get_window_height(void) {
    return window_height;
}

// Forward declarations
void render_title_scene(TitleScene* scene);
void render_map_scene(MapScene* scene);
void render_menu_scene(MenuScene* scene);
void render_battle_scene(BattleScene* scene);
void render_gameover_scene(GameOverScene* scene);

// Player movement function
void move_player(int dx, int dy) {
    if (!current_scene || current_scene->type != SCENE_MAP) return;
    
    MapScene* map_scene = (MapScene*)current_scene;
    
    // Update player position
    map_scene->player_x += dx;
    
    // Handle Y movement differently for 2D and 3D modes
    if (map_scene->render_mode == 3) {
        // In 3D mode, don't invert Y axis (up is +dy, down is -dy)
        map_scene->player_y += dy;
    } else {
        // In 2D mode, invert Y axis (up is -dy, down is +dy)
        map_scene->player_y -= dy;
    }
    
    printf("DEBUG: Player moved to (%d, %d, %d) with delta (%d, %d)\n", 
           map_scene->player_x, map_scene->player_y, map_scene->player_z, dx, dy);
    
#if DEBUG_MODE
    // Update debug message to show player position on screen
    extern char debug_message[256];
    extern bool debug_mode;
    if (debug_mode) {
        // Append player position to existing message
        char temp_message[256];
        snprintf(temp_message, sizeof(temp_message), "%s | Player: (%d, %d, %d) Delta: (%d, %d)", 
                 debug_message, map_scene->player_x, map_scene->player_y, map_scene->player_z, dx, dy);
        strcpy(debug_message, temp_message);
    }
#endif
    
    // Trigger a redraw to show the updated player position
    glutPostRedisplay();
    
    // In a real implementation, we would:
    // 1. Check map boundaries
    // 2. Check for collisions with obstacles
    // 3. Check for events at the new position
}

// Create a new scene of the specified type
Scene* create_scene(SceneType type) {
    Scene* scene = NULL;
    
    switch (type) {
        case SCENE_TITLE: {
            TitleScene* title_scene = malloc(sizeof(TitleScene));
            if (title_scene) {
                memset(title_scene, 0, sizeof(TitleScene));
                title_scene->base.type = SCENE_TITLE;
                title_scene->base.active = true;
                title_scene->command_index = 0;
                scene = (Scene*)title_scene;
            }
            break;
        }
        case SCENE_MAP: {
            MapScene* map_scene = malloc(sizeof(MapScene));
            if (map_scene) {
                memset(map_scene, 0, sizeof(MapScene));
                map_scene->base.type = SCENE_MAP;
                map_scene->base.active = true;
                map_scene->map_id = 0;
                map_scene->player_x = 5;
                map_scene->player_y = 5;
                map_scene->player_z = 0;
                map_scene->player_moving = false;
                scene = (Scene*)map_scene;
            }
            break;
        }
        case SCENE_MENU: {
            MenuScene* menu_scene = malloc(sizeof(MenuScene));
            if (menu_scene) {
                memset(menu_scene, 0, sizeof(MenuScene));
                menu_scene->base.type = SCENE_MENU;
                menu_scene->base.active = true;
                menu_scene->menu_index = 0;
                scene = (Scene*)menu_scene;
            }
            break;
        }
        case SCENE_BATTLE: {
            BattleScene* battle_scene = malloc(sizeof(BattleScene));
            if (battle_scene) {
                memset(battle_scene, 0, sizeof(BattleScene));
                battle_scene->base.type = SCENE_BATTLE;
                battle_scene->base.active = true;
                battle_scene->turn = 1;
                battle_scene->player_turn = true;
                scene = (Scene*)battle_scene;
            }
            break;
        }
        case SCENE_GAMEOVER: {
            GameOverScene* gameover_scene = malloc(sizeof(GameOverScene));
            if (gameover_scene) {
                memset(gameover_scene, 0, sizeof(GameOverScene));
                gameover_scene->base.type = SCENE_GAMEOVER;
                gameover_scene->base.active = true;
                gameover_scene->timer = 0;
                scene = (Scene*)gameover_scene;
            }
            break;
        }
        default:
            break;
    }
    
    return scene;
}

// Destroy a scene
void destroy_scene(Scene* scene) {
    if (scene) {
        free(scene);
    }
}

// Scenes initialization
void init_scenes(void) {
    if (scenes_initialized) return;
    
    printf("Initializing scenes...\n");
    
    // Create initial map scene
    current_scene = create_scene(SCENE_MAP);
    
    // Set player starting position from file if available
    MapScene* map_scene = (MapScene*)current_scene;
    FILE* fp = fopen("data/player_start.txt", "r");
    if (fp) {
        char line[256];
        // Skip comment lines
        while (fgets(line, sizeof(line), fp)) {
            int text, plat, render_2d, render_3d, ar_mode;
            if (line[0] != '#' && sscanf(line, "%d,%d,%d,%d,%d,%d,%d,%d", 
                                         &map_scene->player_x, &map_scene->player_y, &map_scene->map_id,
                                         &text, &plat, &render_2d, &render_3d, &ar_mode) == 8) {
                printf("Loaded player starting position: (%d, %d) on map %d\n", 
                       map_scene->player_x, map_scene->player_y, map_scene->map_id);
                
                // Set render mode based on flags
                if (render_3d) {
                    map_scene->render_mode = 3; // 3D mode
                } else if (render_2d) {
                    map_scene->render_mode = 2; // 2D mode
                } else if (plat) {
                    map_scene->render_mode = 1; // Platform mode
                } else {
                    map_scene->render_mode = 0; // Text mode
                }
                
                // Check if AR mode is enabled
                ar_mode_enabled = (ar_mode == 1);
                
                printf("Render mode: %d (text:%d, plat:%d, 2d:%d, 3d:%d), AR mode: %d\n", 
                       map_scene->render_mode, text, plat, render_2d, render_3d, ar_mode_enabled);
                
                // Initialize camera if AR mode is enabled
                if (ar_mode_enabled) {
                    printf("Initializing camera for AR mode...\n");
                    if (!init_camera_with_fallback()) {
                        printf("Failed to initialize camera for AR mode\n");
                        ar_mode_enabled = false;
                    } else {
                        printf("Camera initialized successfully for AR mode\n");
                    }
                }
                break;
            }
        }
        fclose(fp);
    } else {
        // Default starting position
        map_scene->player_x = 5;
        map_scene->player_y = 5;
        map_scene->map_id = 0;
        map_scene->render_mode = 2; // Default to 2D mode
        ar_mode_enabled = false; // Disable AR mode by default
        printf("Using default player starting position: (%d, %d) on map %d\n", 
               map_scene->player_x, map_scene->player_y, map_scene->map_id);
    }
    
    scenes_initialized = true;
    printf("Scenes initialized.\n");
}

// Scenes update
void update_scenes(void) {
    if (!current_scene || !current_scene->active) return;
    
    // Update fade effects
    if (current_scene->fade_duration > 0) {
        current_scene->fade_duration--;
    }
    
    // Scene-specific updates
    switch (current_scene->type) {
        case SCENE_TITLE: {
            // Title scene specific updates
            break;
        }
        case SCENE_MAP: {
            MapScene* map_scene = (MapScene*)current_scene;
            // Map scene specific updates
            
            // Update camera frame if AR mode is enabled
            if (ar_mode_enabled) {
                update_camera_frame();
            }
            break;
        }
        case SCENE_MENU: {
            // Menu scene specific updates
            break;
        }
        case SCENE_BATTLE: {
            BattleScene* battle_scene = (BattleScene*)current_scene;
            // Battle scene specific updates
            break;
        }
        case SCENE_GAMEOVER: {
            GameOverScene* gameover_scene = (GameOverScene*)current_scene;
            gameover_scene->timer++;
            break;
        }
        default:
            break;
    }
}

// Scenes rendering
void render_scenes(void) {
    if (!current_scene) return;
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Scene-specific rendering
    switch (current_scene->type) {
        case SCENE_TITLE: {
            render_title_scene((TitleScene*)current_scene);
            break;
        }
        case SCENE_MAP: {
            render_map_scene((MapScene*)current_scene);
            break;
        }
        case SCENE_MENU: {
            render_menu_scene((MenuScene*)current_scene);
            break;
        }
        case SCENE_BATTLE: {
            render_battle_scene((BattleScene*)current_scene);
            break;
        }
        case SCENE_GAMEOVER: {
            render_gameover_scene((GameOverScene*)current_scene);
            break;
        }
        default:
            break;
    }
    
    // Render fade effects
    if (current_scene->fade_duration > 0) {
        float opacity = (float)current_scene->fade_duration / 30.0f; // Assuming 30 frames fade
        glColor4f(0.0f, 0.0f, 0.0f, opacity);
        glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(window_width, 0);
        glVertex2f(window_width, window_height);
        glVertex2f(0, window_height);
        glEnd();
    }
}

// Title scene rendering
void render_title_scene(TitleScene* scene) {
    // Render title background
    glClearColor(0.0f, 0.0f, 0.2f, 1.0f);
    
    // Render title text
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(window_width/2 - 50, window_height/2 + 50);
    char* title_text = "RMMV-Inspired Game";
    for (int i = 0; title_text[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, title_text[i]);
    }
    
    // Render menu options
    char* options[] = {"New Game", "Continue", "Options", "Exit"};
    int num_options = 4;
    
    for (int i = 0; i < num_options; i++) {
        glRasterPos2f(window_width/2 - 30, window_height/2 - i*30);
        if (i == scene->command_index) {
            glColor3f(1.0f, 1.0f, 0.0f); // Highlight selected option
        } else {
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        char* option_text = options[i];
        for (int j = 0; option_text[j] != '\0'; j++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, option_text[j]);
        }
    }
}

// Map scene rendering
void render_map_scene(MapScene* scene) {
    printf("DEBUG: Rendering map scene with player at (%d, %d), render mode: %d, AR mode: %d\n", 
           scene->player_x, scene->player_y, scene->render_mode, ar_mode_enabled);
    
    // Set up orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, window_width, 0, window_height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Render camera feed if AR mode is enabled (as background)
    if (ar_mode_enabled) {
        // Get camera frame data
        unsigned char* camera_frame = get_camera_frame();
        if (camera_frame) {
            // Get camera dimensions
            int cam_width, cam_height;
            get_camera_dimensions(&cam_width, &cam_height);
            
            // Disable depth testing for camera feed
            glDisable(GL_DEPTH_TEST);
            
            // Render camera frame as background
            glRasterPos2i(0, window_height);
            glPixelZoom((float)window_width / (float)cam_width, -(float)window_height / (float)cam_height);
            glDrawPixels(cam_width, cam_height, GL_RGBA, GL_UNSIGNED_BYTE, camera_frame);
            glPixelZoom(1.0, 1.0);
            
            // Re-enable depth testing for game objects
            glEnable(GL_DEPTH_TEST);
        }
    } else {
        // Render map background only if AR is not enabled
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
    // Enable depth testing for game objects
    glEnable(GL_DEPTH_TEST);
    
    // Render based on mode
    switch (scene->render_mode) {
        case 3: // 3D mode
            // Set up 3D perspective for 3D mode
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(45.0, (double)window_width / (double)window_height, 1.0, 100.0);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            
            // Camera follows player with a slight offset (behind and above)
            float cam_x = scene->player_x * 2.0f;
            float cam_y = 5.0f;  // Height above player
            float cam_z = scene->player_y * 2.0f + 10.0f;  // Behind player
            
            // Look at player position
            float look_x = scene->player_x * 2.0f;
            float look_y = 0.0f;
            float look_z = scene->player_y * 2.0f;
            
            gluLookAt(cam_x, cam_y, cam_z, look_x, look_y, look_z, 0, 1, 0); // Camera position and look direction
            
            // Render a simple 3D cube for the player
            glPushMatrix();
            glTranslatef(scene->player_x * 2.0f, 0.0f, scene->player_y * 2.0f); // Position the player
            glColor3f(0.0f, 1.0f, 0.0f); // Green color for the player
            glutSolidCube(1.0); // Draw a solid cube for the player
            glPopMatrix();
            
            // Render grid as 3D lines
            glColor3f(0.5f, 0.5f, 0.5f);
            glBegin(GL_LINES);
            // Vertical lines
            for (int x = -20; x < 20; x++) {
                glVertex3f(x * 2.0f, 0, -20 * 2.0f);
                glVertex3f(x * 2.0f, 0, 20 * 2.0f);
            }
            // Horizontal lines
            for (int z = -20; z < 20; z++) {
                glVertex3f(-20 * 2.0f, 0, z * 2.0f);
                glVertex3f(20 * 2.0f, 0, z * 2.0f);
            }
            glEnd();
            break;
            
        case 2: // 2D mode
        default:
            // For 2D mode, we already have the correct projection matrix set up above
            // Make sure depth testing is enabled
            glEnable(GL_DEPTH_TEST);
            
            // Push matrix for 2D rendering
            glPushMatrix();
            
            // Render grid (now always rendered, even in AR mode, as it's part of the game world)
            glColor3f(0.5f, 0.5f, 0.5f);
            glBegin(GL_LINES);
            // Vertical lines
            for (int x = 0; x < 20; x++) {
                glVertex2f(x * 32, 0);
                glVertex2f(x * 32, 20 * 32);
            }
            // Horizontal lines
            for (int y = 0; y < 20; y++) {
                glVertex2f(0, y * 32);
                glVertex2f(20 * 32, y * 32);
            }
            glEnd();
            
            // Render player as a colored square
            glColor3f(0.0f, 1.0f, 0.0f); // Green color for the player
            glBegin(GL_QUADS);
            glVertex2f(scene->player_x * 32, scene->player_y * 32);
            glVertex2f(scene->player_x * 32 + 32, scene->player_y * 32);
            glVertex2f(scene->player_x * 32 + 32, scene->player_y * 32 + 32);
            glVertex2f(scene->player_x * 32, scene->player_y * 32 + 32);
            glEnd();
            
            glPopMatrix(); // Restore matrix
            break;
    }


    
#if DEBUG_MODE
    // Render player position debug info
    if (debug_mode) {
        // Set up orthographic projection for UI rendering
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, window_width, 0, window_height);
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        // Render player position at the bottom of the screen
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(10, 40); // Position above the main debug message
        char player_pos[64];
        snprintf(player_pos, sizeof(player_pos), "Player: (%d, %d, %d) Mode: %d AR: %d", 
                 scene->player_x, scene->player_y, scene->player_z, scene->render_mode, ar_mode_enabled);
        for (int i = 0; player_pos[i] != '\0'; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, player_pos[i]);
        }
        
        // Restore previous matrices
        glPopMatrix(); // MODELVIEW
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }
#endif
}

// Menu scene rendering
void render_menu_scene(MenuScene* scene) {
    // Render menu background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Render menu title
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(window_width/2 - 30, window_height - 50);
    char* menu_text = "Menu";
    for (int i = 0; menu_text[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, menu_text[i]);
    }
    
    // Render menu options
    char* options[] = {"Items", "Skills", "Equipment", "Status", "Save", "Load", "Options", "Exit"};
    int num_options = 8;
    
    for (int i = 0; i < num_options; i++) {
        glRasterPos2f(50, window_height - 100 - i*30);
        if (i == scene->menu_index) {
            glColor3f(1.0f, 1.0f, 0.0f); // Highlight selected option
        } else {
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        char* option_text = options[i];
        for (int j = 0; option_text[j] != '\0'; j++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, option_text[j]);
        }
    }
}

// Battle scene rendering
void render_battle_scene(BattleScene* scene) {
    // Render battle background
    glClearColor(0.3f, 0.0f, 0.0f, 1.0f);
    
    // Render battle information
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(50, window_height - 50);
    char turn_text[64];
    snprintf(turn_text, sizeof(turn_text), "Turn: %d", scene->turn);
    for (int i = 0; turn_text[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, turn_text[i]);
    }
    
    // Render turn indicator
    glRasterPos2f(50, window_height - 70);
    char* turn_indicator = scene->player_turn ? "Player's Turn" : "Enemy's Turn";
    for (int i = 0; turn_indicator[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, turn_indicator[i]);
    }
    
    // Render player party
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    for (int i = 0; i < 0 && i < 4; i++) {
        glVertex2f(100 + i*100, 200);
        glVertex2f(150 + i*100, 200);
        glVertex2f(150 + i*100, 250);
        glVertex2f(100 + i*100, 250);
    }
    glEnd();
    
    // Render enemies
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    for (int i = 0; i < 0 && i < 4; i++) {
        glVertex2f(400 + i*100, 200);
        glVertex2f(450 + i*100, 200);
        glVertex2f(450 + i*100, 250);
        glVertex2f(400 + i*100, 250);
    }
    glEnd();
}

// Game over scene rendering
void render_gameover_scene(GameOverScene* scene) {
    // Render game over background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Render game over text
    glColor3f(1.0f, 0.0f, 0.0f);
    glRasterPos2f(window_width/2 - 50, window_height/2);
    char* gameover_text = "Game Over";
    for (int i = 0; gameover_text[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, gameover_text[i]);
    }
    
    // Render timer
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(window_width/2 - 30, window_height/2 - 30);
    char timer_text[32];
    snprintf(timer_text, sizeof(timer_text), "Time: %d", scene->timer);
    for (int i = 0; timer_text[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, timer_text[i]);
    }
}

// Scenes cleanup
void cleanup_scenes(void) {
    if (!scenes_initialized) return;
    
    printf("Cleaning up scenes...\n");
    
    // Destroy current scene
    if (current_scene) {
        destroy_scene(current_scene);
        current_scene = NULL;
    }
    
    // Clean up camera resources if AR mode was enabled
    if (ar_mode_enabled) {
        cleanup_camera_resources();
        ar_mode_enabled = false;
    }
    
    scenes_initialized = false;
    printf("Scenes cleaned up.\n");
}

// Getters
Scene* get_current_scene(void) {
    return current_scene;
}

// Scene transition functions
void goto_scene(SceneType type) {
    if (current_scene) {
        destroy_scene(current_scene);
    }
    current_scene = create_scene(type);
}