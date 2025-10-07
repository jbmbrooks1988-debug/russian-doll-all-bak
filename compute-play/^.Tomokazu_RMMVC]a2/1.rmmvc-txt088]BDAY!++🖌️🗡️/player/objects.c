/*
 * objects.c - Game objects
 * Based on RMMV's rpg_objects.js functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Game object structures
#define MAX_PARAMETERS 10

typedef struct {
    int id;
    char name[256];
    int level;
    int exp;
    int hp, mp;
    int param[MAX_PARAMETERS]; // str, dex, agi, int, luk, etc.
    int* skills;
    int skill_count;
    int* equip_slots;
    int equip_count;
} GameActor;

typedef struct {
    int id;
    char name[256];
    char description[1024];
    int price;
    int consumable;
    int* effects;
    int effect_count;
} GameItem;

typedef struct {
    int id;
    char name[256];
    char description[1024];
    int price;
    int atk, def, mat, mdf, agi, luk;
    int equip_type; // 0=weapon, 1=shield, 2=head, 3=body, 4=accessory
    int* traits;
    int trait_count;
} GameEquipment;

typedef struct {
    int id;
    char name[256];
    int hp, mp;
    int exp;
    int param[MAX_PARAMETERS];
    int* actions;
    int action_count;
    int* drop_items;
    int drop_count;
} GameEnemy;

// Objects function declarations
void init_objects(void);
void update_objects(void);
void render_objects(void);
void cleanup_objects(void);

// Getters
int get_actor_count(void);
GameActor* get_actor(int index);

int get_item_count(void);
GameItem* get_item(int index);

int get_weapon_count(void);
GameEquipment* get_weapon(int index);

int get_armor_count(void);
GameEquipment* get_armor(int index);

int get_enemy_count(void);
GameEnemy* get_enemy(int index);

// Game object collections
static GameActor* actors = NULL;
static int actor_count = 0;

static GameItem* items = NULL;
static int item_count = 0;

static GameEquipment* weapons = NULL;
static int weapon_count = 0;

static GameEquipment* armors = NULL;
static int armor_count = 0;

static GameEnemy* enemies = NULL;
static int enemy_count = 0;

static bool objects_initialized = false;

// Initialize a default actor
void init_default_actor(GameActor* actor, int id) {
    actor->id = id;
    snprintf(actor->name, sizeof(actor->name), "Actor %d", id);
    actor->level = 1;
    actor->exp = 0;
    actor->hp = 100;
    actor->mp = 50;
    
    // Initialize parameters
    for (int i = 0; i < MAX_PARAMETERS; i++) {
        actor->param[i] = 10;
    }
    
    // Initialize skills and equipment (empty for now)
    actor->skills = NULL;
    actor->skill_count = 0;
    actor->equip_slots = NULL;
    actor->equip_count = 0;
}

// Initialize a default item
void init_default_item(GameItem* item, int id) {
    item->id = id;
    snprintf(item->name, sizeof(item->name), "Item %d", id);
    snprintf(item->description, sizeof(item->description), "A generic item");
    item->price = 10;
    item->consumable = 1;
    item->effects = NULL;
    item->effect_count = 0;
}

// Initialize default equipment
void init_default_equipment(GameEquipment* equip, int id, int type) {
    equip->id = id;
    snprintf(equip->name, sizeof(equip->name), "%s %d", 
             type == 0 ? "Weapon" : (type == 1 ? "Shield" : (type == 2 ? "Head" : (type == 3 ? "Body" : "Accessory"))), id);
    snprintf(equip->description, sizeof(equip->description), "A generic %s", 
             type == 0 ? "weapon" : (type == 1 ? "shield" : (type == 2 ? "head gear" : (type == 3 ? "body armor" : "accessory"))));
    equip->price = 50;
    equip->atk = type == 0 ? 5 : 0;
    equip->def = type == 1 || type == 3 ? 3 : 0;
    equip->mat = 0;
    equip->mdf = type == 2 || type == 3 ? 2 : 0;
    equip->agi = 0;
    equip->luk = 0;
    equip->equip_type = type;
    equip->traits = NULL;
    equip->trait_count = 0;
}

// Initialize a default enemy
void init_default_enemy(GameEnemy* enemy, int id) {
    enemy->id = id;
    snprintf(enemy->name, sizeof(enemy->name), "Enemy %d", id);
    enemy->hp = 50;
    enemy->mp = 20;
    enemy->exp = 10;
    
    // Initialize parameters
    for (int i = 0; i < MAX_PARAMETERS; i++) {
        enemy->param[i] = 5;
    }
    
    enemy->actions = NULL;
    enemy->action_count = 0;
    enemy->drop_items = NULL;
    enemy->drop_count = 0;
}

// Objects initialization
void init_objects(void) {
    if (objects_initialized) return;
    
    printf("Initializing objects...\n");
    
    // Create some default actors
    actor_count = 4;
    actors = malloc(actor_count * sizeof(GameActor));
    if (actors) {
        for (int i = 0; i < actor_count; i++) {
            init_default_actor(&actors[i], i + 1);
        }
    }
    
    // Create some default items
    item_count = 5;
    items = malloc(item_count * sizeof(GameItem));
    if (items) {
        for (int i = 0; i < item_count; i++) {
            init_default_item(&items[i], i + 1);
        }
    }
    
    // Create some default weapons
    weapon_count = 3;
    weapons = malloc(weapon_count * sizeof(GameEquipment));
    if (weapons) {
        for (int i = 0; i < weapon_count; i++) {
            init_default_equipment(&weapons[i], i + 1, 0); // 0 = weapon
        }
    }
    
    // Create some default armors
    armor_count = 4;
    armors = malloc(armor_count * sizeof(GameEquipment));
    if (armors) {
        for (int i = 0; i < armor_count; i++) {
            init_default_equipment(&armors[i], i + 1, (i % 4) + 1); // 1-4 = shield, head, body, accessory
        }
    }
    
    // Create some default enemies
    enemy_count = 5;
    enemies = malloc(enemy_count * sizeof(GameEnemy));
    if (enemies) {
        for (int i = 0; i < enemy_count; i++) {
            init_default_enemy(&enemies[i], i + 1);
        }
    }
    
    objects_initialized = true;
    printf("Objects initialized.\n");
}

// Objects update
void update_objects(void) {
    // Objects update logic
}

// Objects rendering
void render_objects(void) {
    // Objects rendering logic
}

// Objects cleanup
void cleanup_objects(void) {
    if (!objects_initialized) return;
    
    printf("Cleaning up objects...\n");
    
    // Free actors
    if (actors) {
        for (int i = 0; i < actor_count; i++) {
            if (actors[i].skills) free(actors[i].skills);
            if (actors[i].equip_slots) free(actors[i].equip_slots);
        }
        free(actors);
        actors = NULL;
    }
    actor_count = 0;
    
    // Free items
    if (items) {
        for (int i = 0; i < item_count; i++) {
            if (items[i].effects) free(items[i].effects);
        }
        free(items);
        items = NULL;
    }
    item_count = 0;
    
    // Free weapons
    if (weapons) {
        for (int i = 0; i < weapon_count; i++) {
            if (weapons[i].traits) free(weapons[i].traits);
        }
        free(weapons);
        weapons = NULL;
    }
    weapon_count = 0;
    
    // Free armors
    if (armors) {
        for (int i = 0; i < armor_count; i++) {
            if (armors[i].traits) free(armors[i].traits);
        }
        free(armors);
        armors = NULL;
    }
    armor_count = 0;
    
    // Free enemies
    if (enemies) {
        for (int i = 0; i < enemy_count; i++) {
            if (enemies[i].actions) free(enemies[i].actions);
            if (enemies[i].drop_items) free(enemies[i].drop_items);
        }
        free(enemies);
        enemies = NULL;
    }
    enemy_count = 0;
    
    objects_initialized = false;
    printf("Objects cleaned up.\n");
}

// Getters
int get_actor_count(void) {
    return actor_count;
}

GameActor* get_actor(int index) {
    if (index >= 0 && index < actor_count) {
        return &actors[index];
    }
    return NULL;
}

int get_item_count(void) {
    return item_count;
}

GameItem* get_item(int index) {
    if (index >= 0 && index < item_count) {
        return &items[index];
    }
    return NULL;
}

int get_weapon_count(void) {
    return weapon_count;
}

GameEquipment* get_weapon(int index) {
    if (index >= 0 && index < weapon_count) {
        return &weapons[index];
    }
    return NULL;
}

int get_armor_count(void) {
    return armor_count;
}

GameEquipment* get_armor(int index) {
    if (index >= 0 && index < armor_count) {
        return &armors[index];
    }
    return NULL;
}

int get_enemy_count(void) {
    return enemy_count;
}

GameEnemy* get_enemy(int index) {
    if (index >= 0 && index < enemy_count) {
        return &enemies[index];
    }
    return NULL;
}