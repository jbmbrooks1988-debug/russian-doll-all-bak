/*
 * sprites.c - Sprite rendering system
 * Based on RMMV's rpg_sprites.js functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <GL/glut.h>

// Sprite structure
typedef struct Sprite {
    float x, y, z;
    float width, height;
    float scale_x, scale_y;
    float rotation;
    float opacity;
    int blend_mode;
    bool visible;
    int priority;
    struct Sprite* parent;
    struct Sprite* children;
    struct Sprite* next;
    struct Sprite* prev;
} Sprite;

// Bitmap structure (simplified)
typedef struct {
    int width, height;
    unsigned char* pixels;
    bool loaded;
} Bitmap;

// Tilemap sprite
typedef struct {
    Sprite base;
    int map_id;
    int* tile_data;
    int tile_count;
} TilemapSprite;

// Character sprite
typedef struct {
    Sprite base;
    int character_index;
    int direction;
    int pattern;
    bool is_player;
} CharacterSprite;

// Blend modes
#define BLEND_NORMAL 0
#define BLEND_ADD 1
#define BLEND_MULTIPLY 2
#define BLEND_SCREEN 3

// Sprite manager state
static Sprite* root_sprite = NULL;
static bool sprites_initialized = false;
static int window_width = 800;
static int window_height = 600;

// Create a new sprite
Sprite* create_sprite(void) {
    Sprite* sprite = malloc(sizeof(Sprite));
    if (sprite) {
        memset(sprite, 0, sizeof(Sprite));
        sprite->scale_x = 1.0f;
        sprite->scale_y = 1.0f;
        sprite->opacity = 1.0f;
        sprite->visible = true;
        sprite->priority = 0;
    }
    return sprite;
}

// Destroy a sprite
void destroy_sprite(Sprite* sprite) {
    if (!sprite) return;
    
    // Remove from parent
    if (sprite->parent) {
        if (sprite->parent->children == sprite) {
            sprite->parent->children = sprite->next;
        }
    }
    
    // Remove from siblings
    if (sprite->prev) {
        sprite->prev->next = sprite->next;
    }
    if (sprite->next) {
        sprite->next->prev = sprite->prev;
    }
    
    // Destroy children
    Sprite* child = sprite->children;
    while (child) {
        Sprite* next = child->next;
        destroy_sprite(child);
        child = next;
    }
    
    free(sprite);
}

// Add a child sprite
void add_child(Sprite* parent, Sprite* child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    child->prev = NULL;
    child->next = parent->children;
    
    if (parent->children) {
        parent->children->prev = child;
    }
    
    parent->children = child;
}

// Set sprite position
void set_sprite_position(Sprite* sprite, float x, float y) {
    if (sprite) {
        sprite->x = x;
        sprite->y = y;
    }
}

// Set sprite scale
void set_sprite_scale(Sprite* sprite, float scale_x, float scale_y) {
    if (sprite) {
        sprite->scale_x = scale_x;
        sprite->scale_y = scale_y;
    }
}

// Set sprite rotation
void set_sprite_rotation(Sprite* sprite, float rotation) {
    if (sprite) {
        sprite->rotation = rotation;
    }
}

// Set sprite opacity
void set_sprite_opacity(Sprite* sprite, float opacity) {
    if (sprite) {
        sprite->opacity = opacity;
    }
}

// Set sprite visibility
void set_sprite_visible(Sprite* sprite, bool visible) {
    if (sprite) {
        sprite->visible = visible;
    }
}

// Set sprite blend mode
void set_sprite_blend_mode(Sprite* sprite, int blend_mode) {
    if (sprite) {
        sprite->blend_mode = blend_mode;
    }
}

// Create a tilemap sprite
TilemapSprite* create_tilemap_sprite(int map_id) {
    TilemapSprite* tilemap = malloc(sizeof(TilemapSprite));
    if (tilemap) {
        memset(tilemap, 0, sizeof(TilemapSprite));
        tilemap->base.scale_x = 1.0f;
        tilemap->base.scale_y = 1.0f;
        tilemap->base.opacity = 1.0f;
        tilemap->base.visible = true;
        tilemap->base.priority = 0;
        tilemap->map_id = map_id;
        
        // Load map data
        // GameMap* map = get_map(map_id);
        // if (map && map->tiles) {
        //     tilemap->tile_count = map->tile_count;
        //     tilemap->tile_data = malloc(tilemap->tile_count * 3 * sizeof(int)); // x, y, emoji_idx
        //     if (tilemap->tile_data) {
        //         for (int i = 0; i < map->tile_count; i++) {
        //             tilemap->tile_data[i*3] = map->tiles[i].x;
        //             tilemap->tile_data[i*3+1] = map->tiles[i].y;
        //             tilemap->tile_data[i*3+2] = map->tiles[i].emoji_idx;
        //         }
        //     }
        // }
    }
    return tilemap;
}

// Create a character sprite
CharacterSprite* create_character_sprite(int character_index, bool is_player) {
    CharacterSprite* character = malloc(sizeof(CharacterSprite));
    if (character) {
        memset(character, 0, sizeof(CharacterSprite));
        character->base.scale_x = 1.0f;
        character->base.scale_y = 1.0f;
        character->base.opacity = 1.0f;
        character->base.visible = true;
        character->base.priority = 10;
        character->character_index = character_index;
        character->is_player = is_player;
    }
    return character;
}

// Render a sprite
void render_sprite(Sprite* sprite) {
    if (!sprite || !sprite->visible) return;
    
    // Save current matrix
    glPushMatrix();
    
    // Apply transformations
    glTranslatef(sprite->x, sprite->y, sprite->z);
    glRotatef(sprite->rotation, 0, 0, 1);
    glScalef(sprite->scale_x, sprite->scale_y, 1.0f);
    
    // Apply opacity
    glColor4f(1.0f, 1.0f, 1.0f, sprite->opacity);
    
    // Set blend mode
    switch (sprite->blend_mode) {
        case BLEND_ADD:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        case BLEND_MULTIPLY:
            glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BLEND_SCREEN:
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
            break;
        case BLEND_NORMAL:
        default:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
    }
    
    // Render sprite-specific content
    // This would normally render a bitmap, but we'll use a simple quad for now
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(sprite->width, 0);
    glVertex2f(sprite->width, sprite->height);
    glVertex2f(0, sprite->height);
    glEnd();
    
    // Render children
    Sprite* child = sprite->children;
    while (child) {
        render_sprite(child);
        child = child->next;
    }
    
    // Restore matrix
    glPopMatrix();
}

// Render a tilemap sprite
void render_tilemap_sprite(TilemapSprite* tilemap) {
    if (!tilemap || !tilemap->base.visible) return;
    
    // Save current matrix
    glPushMatrix();
    
    // Apply transformations
    glTranslatef(tilemap->base.x, tilemap->base.y, tilemap->base.z);
    glScalef(tilemap->base.scale_x, tilemap->base.scale_y, 1.0f);
    
    // Apply opacity
    glColor4f(1.0f, 1.0f, 1.0f, tilemap->base.opacity);
    
    // Render tiles
    for (int i = 0; i < tilemap->tile_count; i++) {
        int x = tilemap->tile_data[i*3];
        int y = tilemap->tile_data[i*3+1];
        int emoji_idx = tilemap->tile_data[i*3+2];
        
        // Simple tile rendering
        glBegin(GL_QUADS);
        glVertex2f(x * 32, y * 32);
        glVertex2f(x * 32 + 32, y * 32);
        glVertex2f(x * 32 + 32, y * 32 + 32);
        glVertex2f(x * 32, y * 32 + 32);
        glEnd();
    }
    
    // Restore matrix
    glPopMatrix();
}

// Render a character sprite
void render_character_sprite(CharacterSprite* character) {
    if (!character || !character->base.visible) return;
    
    // Save current matrix
    glPushMatrix();
    
    // Apply transformations
    glTranslatef(character->base.x, character->base.y, character->base.z);
    glScalef(character->base.scale_x, character->base.scale_y, 1.0f);
    
    // Apply opacity
    glColor4f(0.0f, 1.0f, 0.0f, character->base.opacity); // Green for player, red for others
    
    if (character->is_player) {
        glColor4f(0.0f, 1.0f, 0.0f, character->base.opacity); // Green for player
    } else {
        glColor4f(1.0f, 0.0f, 0.0f, character->base.opacity); // Red for others
    }
    
    // Render character as a simple quad
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(32, 0);
    glVertex2f(32, 32);
    glVertex2f(0, 32);
    glEnd();
    
    // Restore matrix
    glPopMatrix();
}

// Sprites initialization
void init_sprites(void) {
    if (sprites_initialized) return;
    
    printf("Initializing sprites...\n");
    
    // Create root sprite
    root_sprite = create_sprite();
    if (root_sprite) {
        root_sprite->width = window_width;
        root_sprite->height = window_height;
    }
    
    sprites_initialized = true;
    printf("Sprites initialized.\n");
}

// Sprites update
void update_sprites(void) {
    // Sprites update logic
    // In a real implementation, this would update animations, positions, etc.
}

// Sprites rendering
void render_sprites(void) {
    if (!sprites_initialized || !root_sprite) return;
    
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render root sprite and all children
    render_sprite(root_sprite);
    
    // Disable blending
    glDisable(GL_BLEND);
}

// Sprites cleanup
void cleanup_sprites(void) {
    if (!sprites_initialized) return;
    
    printf("Cleaning up sprites...\n");
    
    // Destroy root sprite and all children
    if (root_sprite) {
        destroy_sprite(root_sprite);
        root_sprite = NULL;
    }
    
    sprites_initialized = false;
    printf("Sprites cleaned up.\n");
}

// Getters
Sprite* get_root_sprite(void) {
    return root_sprite;
}