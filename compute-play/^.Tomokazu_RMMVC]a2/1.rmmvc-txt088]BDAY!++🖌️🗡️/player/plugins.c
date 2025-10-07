/*
 * plugins.c - Plugin system
 * Based on RMMV's plugins.js functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <dlfcn.h>

// Plugin structure
typedef struct Plugin {
    char name[256];
    char description[1024];
    bool enabled;
    void* handle;  // Dynamic library handle
    int (*init_func)(void);
    void (*update_func)(void);
    void (*render_func)(void);
    void (*cleanup_func)(void);
    struct Plugin* next;
} Plugin;

// Plugins function declarations
void init_plugins(void);
void update_plugins(void);
void render_plugins(void);
void cleanup_plugins(void);

// Plugin management functions
int register_plugin(const char* name, const char* description,
                   int (*init_func)(void),
                   void (*update_func)(void),
                   void (*render_func)(void),
                   void (*cleanup_func)(void));

// Getters
Plugin* get_first_plugin(void);
Plugin* get_plugin_by_name(const char* name);

// Plugin manager state
static Plugin* first_plugin = NULL;
static bool plugins_initialized = false;
static char plugins_path[512] = "plugins/";

// Create a new plugin
Plugin* create_plugin(const char* name) {
    Plugin* plugin = malloc(sizeof(Plugin));
    if (plugin) {
        memset(plugin, 0, sizeof(Plugin));
        strncpy(plugin->name, name, sizeof(plugin->name) - 1);
        plugin->name[sizeof(plugin->name) - 1] = '\0';
        plugin->enabled = false;
        plugin->handle = NULL;
        plugin->init_func = NULL;
        plugin->update_func = NULL;
        plugin->render_func = NULL;
        plugin->cleanup_func = NULL;
    }
    return plugin;
}

// Destroy a plugin
void destroy_plugin(Plugin* plugin) {
    if (!plugin) return;
    
    // Cleanup plugin if enabled
    if (plugin->enabled && plugin->cleanup_func) {
        plugin->cleanup_func();
    }
    
    // Close dynamic library
    if (plugin->handle) {
        dlclose(plugin->handle);
    }
    
    free(plugin);
}

// Load a plugin from a shared library
int load_plugin_library(Plugin* plugin) {
    if (!plugin) return -1;
    
    // Construct library path
    // Dynamically allocate buffer for library path
    size_t plugins_path_len = strlen(plugins_path);
    size_t plugin_name_len = strlen(plugin->name);
    char* lib_path = malloc(plugins_path_len + plugin_name_len + 5); // +5 for '/', ".so", and null terminator
    
    if (!lib_path) {
        fprintf(stderr, "Error: Failed to allocate memory for lib_path\n");
        return -1;
    }
    
    snprintf(lib_path, plugins_path_len + plugin_name_len + 5, "%s%s.so", plugins_path, plugin->name);
    
    printf("Loading plugin library: %s\n", lib_path);
    // Load the library
    plugin->handle = dlopen(lib_path, RTLD_LAZY);
    free(lib_path);  // Free dynamically allocated memory
    if (!plugin->handle) {
        fprintf(stderr, "Cannot load plugin library %s: %s\n", lib_path, dlerror());
        return -1;
    }
    
    // Clear any existing error
    dlerror();
    
    // Load function pointers
    plugin->init_func = (int (*)(void))dlsym(plugin->handle, "plugin_init");
    plugin->update_func = (void (*)(void))dlsym(plugin->handle, "plugin_update");
    plugin->render_func = (void (*)(void))dlsym(plugin->handle, "plugin_render");
    plugin->cleanup_func = (void (*)(void))dlsym(plugin->handle, "plugin_cleanup");
    
    // Check for errors
    char* error = dlerror();
    if (error) {
        fprintf(stderr, "Cannot load plugin functions from %s: %s\n", lib_path, error);
        dlclose(plugin->handle);
        plugin->handle = NULL;
        return -1;
    }
    
    return 0;
}

// Enable a plugin
int enable_plugin(Plugin* plugin) {
    if (!plugin || plugin->enabled) return -1;
    
    // Load plugin library if not already loaded
    if (!plugin->handle) {
        if (load_plugin_library(plugin) < 0) {
            return -1;
        }
    }
    
    // Call init function
    if (plugin->init_func) {
        if (plugin->init_func() < 0) {
            fprintf(stderr, "Plugin %s init function failed\n", plugin->name);
            return -1;
        }
    }
    
    plugin->enabled = true;
    return 0;
}

// Disable a plugin
void disable_plugin(Plugin* plugin) {
    if (!plugin || !plugin->enabled) return;
    
    // Call cleanup function
    if (plugin->cleanup_func) {
        plugin->cleanup_func();
    }
    
    plugin->enabled = false;
}

// Scan for plugin files in plugins directory
int scan_plugin_files(void) {
    DIR* dir = opendir(plugins_path);
    if (!dir) {
        // Plugins directory might not exist, which is okay
        return 0;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Check if file matches pattern "*.so"
        char* ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".so") == 0) {
            // Create plugin entry (without .so extension)
            char plugin_name[256];
            strncpy(plugin_name, entry->d_name, sizeof(plugin_name) - 1);
            plugin_name[sizeof(plugin_name) - 1] = '\0';
            ext = strrchr(plugin_name, '.');
            if (ext) *ext = '\0';
            
            // Create plugin
            Plugin* plugin = create_plugin(plugin_name);
            if (plugin) {
                // Add plugin to list
                plugin->next = first_plugin;
                first_plugin = plugin;
                
                // Set default description
                snprintf(plugin->description, sizeof(plugin->description), 
                         "Plugin %s loaded from %s", plugin->name, entry->d_name);
                
                printf("Found plugin: %s\n", plugin->name);
            }
        }
    }
    
    closedir(dir);
    return 0;
}

// Plugins initialization
void init_plugins(void) {
    if (plugins_initialized) return;
    
    printf("Initializing plugins...\n");
    
    // Set plugins path
    char* env_plugins_path = getenv("PLAYER_PLUGINS_PATH");
    if (env_plugins_path) {
        strncpy(plugins_path, env_plugins_path, sizeof(plugins_path) - 1);
        plugins_path[sizeof(plugins_path) - 1] = '\0';
    }
    
    // Scan for plugin files
    if (scan_plugin_files() < 0) {
        fprintf(stderr, "Warning: Failed to scan plugin files\n");
    }
    
    // Enable all plugins by default (in a real implementation, this would be configurable)
    Plugin* plugin = first_plugin;
    while (plugin) {
        if (enable_plugin(plugin) < 0) {
            fprintf(stderr, "Warning: Failed to enable plugin %s\n", plugin->name);
        }
        plugin = plugin->next;
    }
    
    plugins_initialized = true;
    printf("Plugins initialized.\n");
}

// Plugins update
void update_plugins(void) {
    if (!plugins_initialized) return;
    
    // Update all enabled plugins
    Plugin* plugin = first_plugin;
    while (plugin) {
        if (plugin->enabled && plugin->update_func) {
            plugin->update_func();
        }
        plugin = plugin->next;
    }
}

// Plugins rendering
void render_plugins(void) {
    if (!plugins_initialized) return;
    
    // Render all enabled plugins
    Plugin* plugin = first_plugin;
    while (plugin) {
        if (plugin->enabled && plugin->render_func) {
            plugin->render_func();
        }
        plugin = plugin->next;
    }
}

// Plugins cleanup
void cleanup_plugins(void) {
    if (!plugins_initialized) return;
    
    printf("Cleaning up plugins...\n");
    
    // Destroy all plugins
    while (first_plugin) {
        Plugin* next = first_plugin->next;
        disable_plugin(first_plugin);
        destroy_plugin(first_plugin);
        first_plugin = next;
    }
    
    plugins_initialized = false;
    printf("Plugins cleaned up.\n");
}

// Plugin management functions
int register_plugin(const char* name, const char* description,
                   int (*init_func)(void),
                   void (*update_func)(void),
                   void (*render_func)(void),
                   void (*cleanup_func)(void)) {
    if (!plugins_initialized) return -1;
    
    // Create plugin
    Plugin* plugin = create_plugin(name);
    if (!plugin) return -1;
    
    // Set plugin properties
    strncpy(plugin->description, description, sizeof(plugin->description) - 1);
    plugin->description[sizeof(plugin->description) - 1] = '\0';
    plugin->init_func = init_func;
    plugin->update_func = update_func;
    plugin->render_func = render_func;
    plugin->cleanup_func = cleanup_func;
    
    // Add to plugin list
    plugin->next = first_plugin;
    first_plugin = plugin;
    
    // Enable by default
    if (enable_plugin(plugin) < 0) {
        fprintf(stderr, "Warning: Failed to enable registered plugin %s\n", name);
    }
    
    return 0;
}

// Getters
Plugin* get_first_plugin(void) {
    return first_plugin;
}

Plugin* get_plugin_by_name(const char* name) {
    Plugin* plugin = first_plugin;
    while (plugin) {
        if (strcmp(plugin->name, name) == 0) {
            return plugin;
        }
        plugin = plugin->next;
    }
    return NULL;
}