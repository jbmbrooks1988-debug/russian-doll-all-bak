
#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

// For this prototype, we'll define a simple structure for UI elements
// and a hardcoded array to represent the parsed C-HTML file.

#define MAX_ELEMENTS 50
#define MAX_TEXT_LINES 1000
#define MAX_LINE_LENGTH 512

// Forward declaration for canvas render function
void canvas_render_sample(int x, int y, int width, int height);

#define CLIPBOARD_SIZE 10000

// Definition for the renderable shape object provided by the model
typedef struct {
    int id;
    char type[32];  // Increased size
    float x, y, z;
    float width, height, depth;  // Separate dimensions
    float color[4];
    char label[64];  // Added label
    int active;      // Added active flag
} Shape;

// Forward declaration for the function to get shapes from the model
Shape* model_get_shapes(int* count);
char (*model_get_dir_entries(int* count))[256];


typedef struct {
    char type[20];
    int x, y, width, height;
    char id[50]; // Unique identifier for UI elements
    char label[50]; // Used for button label, text element value, and checkbox label
    // Multi-line text support - replace simple text_content with array of lines
    char text_content[MAX_TEXT_LINES][MAX_LINE_LENGTH];
    int num_lines; // Number of lines in the text
    int cursor_x; // X position of cursor (column)
    int cursor_y; // Y position of cursor (line number)
    int is_active; // For textfield active state
    int is_checked; // For checkbox checked state
    int slider_value; // For slider current value
    int slider_min; // For slider minimum value
    int slider_max; // For slider maximum value
    int slider_step; // For slider step increment
    float color[3];
    int parent;
    // Canvas-specific properties
    int canvas_initialized; // Flag to track if canvas has been initialized
    void (*canvas_render_func)(int x, int y, int width, int height); // Render function for canvas
    char view_mode[10]; // View mode: "2d" or "3d"
    // Camera properties for 3D view
    float camera_pos[3]; // Camera position (x, y, z)
    float camera_target[3]; // Camera target (x, y, z)
    float camera_up[3]; // Camera up vector (x, y, z)
    float fov; // Field of view for 3D perspective
    // Event handling
    char onClick[50]; // Store the onClick handler function name
    // Menu-specific properties
    int menu_items_count; // Number of submenu items
    int is_open; // For menus - whether they are currently open
    // Text selection properties
    int selection_start_x; // X position of selection start
    int selection_start_y; // Y position of selection start
    int selection_end_x;   // X position of selection end
    int selection_end_y;   // Y position of selection end
    int has_selection;     // Whether there is an active selection
    // Clipboard for this element
    char clipboard[CLIPBOARD_SIZE];
    // Directory listing properties
    char dir_path[512]; // Directory path for directory listing elements
    char dir_entries[100][256]; // Store up to 100 directory entries
    int dir_entry_count; // Number of directory entries
    int dir_entry_selected; // Index of selected entry
} UIElement;



UIElement elements[MAX_ELEMENTS];
int num_elements = 0;

// Global variables to store window dimensions
int window_width = 800;
int window_height = 600;

// Emoji texture cache system
#define MAX_CACHED_EMOJIS 256
typedef struct {
    unsigned int codepoint;
    GLuint texture_id;
    int width;
    int height;
    int advance_x;  // For proper text flow
    int loaded;     // Whether this cache entry is populated
} EmojiCacheEntry;

EmojiCacheEntry emoji_cache[MAX_CACHED_EMOJIS];
int emoji_cache_size = 0;

// FreeType variables for emoji rendering
FT_Library ft;
FT_Face emoji_face;
float emoji_scale = 1.0f;

// Coordinate transformation helper functions
// Convert from real-world coordinates (y=0 at top) to OpenGL coordinates (y=0 at bottom)  
int convert_y_to_opengl(int y) {
    return window_height - y;
}

// Calculate absolute Y position with coordinate transformation (used in draw_element)
int calculate_absolute_y(int parent_y, int element_y, int element_height) {
    return window_height - (parent_y + element_y + element_height);
}

// FreeType initialization for emoji rendering
void init_freetype() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        // Don't exit since emoji support is optional
        return;
    }

    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Warning: Could not load emoji font at %s, emoji rendering will be disabled\n", emoji_font_path);
        FT_Done_FreeType(ft);
        return;
    }
    
    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, 32);  // Smaller size for UI elements
        if (err) {
            fprintf(stderr, "Warning: Could not set pixel size for emoji font\n");
            FT_Done_Face(emoji_face);
            FT_Done_FreeType(ft);
            return;
        }
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
        if (err) {
            fprintf(stderr, "Warning: Could not select size for emoji font\n");
            FT_Done_Face(emoji_face);
            FT_Done_FreeType(ft);
            return;
        }
    } else {
        fprintf(stderr, "Warning: No fixed sizes available in emoji font\n");
        FT_Done_Face(emoji_face);
        FT_Done_FreeType(ft);
        return;
    }
    
    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = 32.0f / (float)loaded_emoji_size;
    printf("Emoji font loaded, size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
}

// Initialize the emoji cache
void init_emoji_cache() {
    for (int i = 0; i < MAX_CACHED_EMOJIS; i++) {
        emoji_cache[i].loaded = 0;
        emoji_cache[i].texture_id = 0;
        emoji_cache[i].codepoint = 0;
        emoji_cache[i].width = 0;
        emoji_cache[i].height = 0;
        emoji_cache[i].advance_x = 0;
    }
    emoji_cache_size = 0;
}

// Find an emoji in the cache
int find_emoji_in_cache(unsigned int codepoint) {
    for (int i = 0; i < emoji_cache_size && i < MAX_CACHED_EMOJIS; i++) {
        if (emoji_cache[i].codepoint == codepoint && emoji_cache[i].loaded) {
            return i;
        }
    }
    return -1; // Not found
}

// Add an emoji to the cache - only creates the texture ID, doesn't upload data yet
int add_emoji_to_cache(unsigned int codepoint) {
    // Check if emoji is already in cache
    int existing_index = find_emoji_in_cache(codepoint);
    if (existing_index != -1) {
        return existing_index;
    }
    
    // If cache is full, we could implement a replacement strategy
    // For now, just return if cache is full
    if (emoji_cache_size >= MAX_CACHED_EMOJIS) {
        // Optional: Implement LRU or other cache replacement strategy
        return -1;
    }
    
    // Generate a texture ID for this emoji
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    
    // Add to cache
    emoji_cache[emoji_cache_size].codepoint = codepoint;
    emoji_cache[emoji_cache_size].texture_id = texture_id;
    emoji_cache[emoji_cache_size].width = 0;
    emoji_cache[emoji_cache_size].height = 0;
    emoji_cache[emoji_cache_size].advance_x = 0;
    emoji_cache[emoji_cache_size].loaded = 0; // Will be set to 1 when texture data is uploaded
    
    int index = emoji_cache_size;
    emoji_cache_size++;
    
    return index;
}

// Function to decode UTF-8 character to Unicode codepoint
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0) {
        if ((str[1] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            return 2;
        }
    }
    if ((str[0] & 0xF0) == 0xE0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            return 3;
        }
    }
    if ((str[0] & 0xF8) == 0xF0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            return 4;
        }
    }
    *codepoint = '?';
    return 1;
}

// Function to render a single emoji character at given position using cache
// Returns the advance width for proper text flow
int render_emoji(unsigned int codepoint, float x, float y) {
    if (!emoji_face) return 0; // Skip if emoji font not loaded, return 0 for advance width
    
    // Check if emoji is already in cache
    int cache_index = find_emoji_in_cache(codepoint);
    GLuint texture_id = 0;
    int width = 0, height = 0;
    int advance_x = 0;

    if (cache_index != -1) {
        // Emoji is in cache, use it
        texture_id = emoji_cache[cache_index].texture_id;
        width = emoji_cache[cache_index].width;
        height = emoji_cache[cache_index].height;
        advance_x = emoji_cache[cache_index].advance_x;
    } else {
        // Emoji not in cache, need to load and add to cache
        FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
        if (err) {
            fprintf(stderr, "Warning: Could not load glyph for codepoint U+%04X\n", codepoint);
            return 0; // Return 0 for advance width on error
        }

        FT_GlyphSlot slot = emoji_face->glyph;
        if (!slot->bitmap.buffer) {
            fprintf(stderr, "Warning: No bitmap for glyph U+%04X\n", codepoint);
            return 0; // Return 0 for advance width on error
        }

        // Handle different pixel modes
        GLint format = GL_RGBA;
        if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
            format = GL_BGRA;
        } else if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
            format = GL_LUMINANCE_ALPHA;
        }

        // Add emoji to cache to get a texture ID
        cache_index = add_emoji_to_cache(codepoint);
        if (cache_index == -1) {
            // Cache is full, just load and render without caching
            GLuint temp_texture;
            glGenTextures(1, &temp_texture);
            glBindTexture(GL_TEXTURE_2D, temp_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, 
                         format, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float scale_factor = emoji_scale;
            float w = slot->bitmap.width * scale_factor;
            float h = slot->bitmap.rows * scale_factor;
            
            // Calculate position so that emoji aligns with text baseline
            float x2 = x;
            float y2 = y;  // Position the emoji at the same baseline as regular text

            glBegin(GL_QUADS);
            glTexCoord2f(0.0, 1.0); glVertex2f(x2, y2);
            glTexCoord2f(1.0, 1.0); glVertex2f(x2 + w, y2);
            glTexCoord2f(1.0, 0.0); glVertex2f(x2 + w, y2 + h);
            glTexCoord2f(0.0, 0.0); glVertex2f(x2, y2 + h);
            glEnd();

            glDisable(GL_TEXTURE_2D);
            glDisable(GL_BLEND);
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &temp_texture);
            glColor3f(1.0f, 1.0f, 1.0f);
            // Calculate rough advance width for uncached emoji
            if (FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR) == 0) {
                return emoji_face->glyph->advance.x >> 6; // advance is in 1/64th pixels
            } else {
                return 0; // fallback if loading fails
            }
        }

        // Get the texture ID from the cache
        texture_id = emoji_cache[cache_index].texture_id;
        width = slot->bitmap.width;
        height = slot->bitmap.rows;
        advance_x = slot->advance.x >> 6; // advance is in 1/64th pixels

        // Upload texture data to the cached texture
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                     format, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Update cache entry with dimensions and advance
        emoji_cache[cache_index].width = width;
        emoji_cache[cache_index].height = height;
        emoji_cache[cache_index].advance_x = slot->advance.x >> 6; // advance is in 1/64th pixels
        emoji_cache[cache_index].loaded = 1; // Mark as fully loaded
    }

    // Render using the texture
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, texture_id);

    float scale_factor = emoji_scale;
    float w = width * scale_factor;
    float h = height * scale_factor;
    
    // Calculate position so that emoji aligns with text baseline
    float x2 = x;
    float y2 = y;  // Position the emoji at the same baseline as regular text

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex2f(x2, y2);
    glTexCoord2f(1.0, 1.0); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0, 0.0); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0, 0.0); glVertex2f(x2, y2 + h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glColor3f(1.0f, 1.0f, 1.0f);
    return advance_x;
}

// Function to check if a string contains emoji characters
int contains_emoji(const char* str) {
    const unsigned char* s = (const unsigned char*)str;
    while (*s) {
        // Check for high Unicode values that are likely emojis
        unsigned int codepoint;
        int bytes = decode_utf8(s, &codepoint);
        if (codepoint >= 0x1F000) {  // Emoji range typically starts around U+1F000
            return 1;
        }
        // Also check for other emoji ranges
        if ((codepoint >= 0x2600 && codepoint <= 0x26FF) ||  // Misc symbols
            (codepoint >= 0x2700 && codepoint <= 0x27BF) ||  // Dingbats
            (codepoint >= 0x1F100 && codepoint <= 0x1F64F) ||  // Emoticons, etc.
            (codepoint >= 0x1F680 && codepoint <= 0x1F6FF) ||  // Transport & map symbols
            (codepoint >= 0x1F700 && codepoint <= 0x1F77F)) {  // Alchemical symbols
            return 1;
        }
        s += bytes;
    }
    return 0;
}

// Function to render text with emoji support
void render_text_with_emojis(const char* str, float x, float y, float color[3]) {
    glColor3fv(color);
    
    const unsigned char* s = (const unsigned char*)str;
    float current_x = x;
    float current_y = y;
    
    // Process the string character by character
    while (*s) {
        unsigned int codepoint;
        int bytes = decode_utf8(s, &codepoint);
        
        // Check if this character is an emoji
        int is_emoji = 0;
        if (codepoint >= 0x1F000 || 
            (codepoint >= 0x2600 && codepoint <= 0x26FF) ||  
            (codepoint >= 0x2700 && codepoint <= 0x27BF) ||  
            (codepoint >= 0x1F100 && codepoint <= 0x1F64F) ||  
            (codepoint >= 0x1F680 && codepoint <= 0x1F6FF) ||  
            (codepoint >= 0x1F700 && codepoint <= 0x1F77F)) {
            is_emoji = 1;
        }
        
        if (is_emoji) {
            // Render emoji using FreeType
            // Adjust the y position to align with text baseline - emojis might need vertical adjustment
            int advance_width = render_emoji(codepoint, current_x, current_y);  // Using same y as text baseline
            
            // Move position forward based on emoji advance width
            current_x += advance_width;
        } else {
            // For regular ASCII characters, we need to render with GLUT but properly spaced
            // Find the next emoji or end of string
            const unsigned char* next_ptr = s + bytes;
            while (*next_ptr) {
                unsigned int next_codepoint;
                int next_bytes = decode_utf8(next_ptr, &next_codepoint);
                
                if (next_codepoint >= 0x1F000 || 
                    (next_codepoint >= 0x2600 && next_codepoint <= 0x26FF) ||  
                    (next_codepoint >= 0x2700 && next_codepoint <= 0x27BF) ||  
                    (next_codepoint >= 0x1F100 && next_codepoint <= 0x1F64F) ||  
                    (next_codepoint >= 0x1F680 && next_codepoint <= 0x1F6FF) ||  
                    (next_codepoint >= 0x1F700 && next_codepoint <= 0x1F77F)) {
                    break; // Found next emoji
                }
                next_ptr += next_bytes;
            }
            
            // Create substring from s to next_ptr
            int segment_len = next_ptr - s;
            char* segment = malloc(segment_len + 1);
            strncpy(segment, (const char*)s, segment_len);
            segment[segment_len] = '\0';
            
            // Render this ASCII segment with GLUT at current position
            glRasterPos2f(current_x, current_y);
            for (char* c = segment; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
            }
            
            // Calculate the width of the rendered segment
            float segment_width = 0;
            for (char* c = segment; *c != '\0'; c++) {
                segment_width += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, *c);
            }
            current_x += segment_width;
            
            free(segment);
            
            // Update s to continue after the ASCII segment
            s = next_ptr;
            continue; // Continue the main loop without incrementing s again
        }
        
        s += bytes;
    }
    
    glColor3f(1.0f, 1.0f, 1.0f);
}

// Simple parser for our C-HTML format
void parse_chtml(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening CHTML file");
        return;
    }

    char line[512]; // Increased buffer size for longer lines
    num_elements = 0;
    int parent_stack[MAX_ELEMENTS];
    int stack_top = -1;

    while (fgets(line, sizeof(line), file)) {
        // Trim leading/trailing whitespace
        char* trimmed_line = line;
        while (*trimmed_line == ' ' || *trimmed_line == '\t') trimmed_line++;
        size_t len = strlen(trimmed_line);
        while (len > 0 && (trimmed_line[len-1] == '\n' || trimmed_line[len-1] == '\r' || trimmed_line[len-1] == ' ' || trimmed_line[len-1] == '\t')) {
            trimmed_line[--len] = '\0';
        }

        if (len == 0) continue; // Skip empty lines

        // Check for closing tag
        if (trimmed_line[0] == '<' && trimmed_line[1] == '/') {
            if (stack_top > -1) {
                stack_top--;
            }
            continue;
        }

        // Check for opening tag
        if (trimmed_line[0] != '<') continue;

        // Extract tag name
        char* tag_start = trimmed_line + 1;
        char* tag_end = strchr(tag_start, ' ');
        if (!tag_end) tag_end = strchr(tag_start, '>');
        if (!tag_end) continue; // Malformed tag

        char tag_name[50];
        strncpy(tag_name, tag_start, tag_end - tag_start);
        tag_name[tag_end - tag_start] = '\0';

        if (num_elements >= MAX_ELEMENTS) {
            fprintf(stderr, "Error: Max elements reached!\n");
            break;
        }

        elements[num_elements].parent = (stack_top > -1) ? parent_stack[stack_top] : -1;
        strcpy(elements[num_elements].type, tag_name);
        elements[num_elements].id[0] = '\0'; // Initialize ID to empty string
        elements[num_elements].is_active = 0;
        elements[num_elements].cursor_x = 0;
        elements[num_elements].cursor_y = 0;
        elements[num_elements].num_lines = 1;
        // Initialize all lines to empty
        for (int line_idx = 0; line_idx < MAX_TEXT_LINES; line_idx++) {
            elements[num_elements].text_content[line_idx][0] = '\0';
        }
        // For textfield and textarea, set first line to the value attribute
        if (strcmp(tag_name, "textfield") == 0 || strcmp(tag_name, "textarea") == 0) {
            // Find the value attribute and set the first line
            char* attr_start = tag_end;
            while (attr_start && (attr_start = strstr(attr_start, " ")) != NULL) {
                attr_start++; // Move past space
                char* eq_sign = strchr(attr_start, '=');
                if (!eq_sign) break; // No more attributes

                char attr_name[50];
                strncpy(attr_name, attr_start, eq_sign - attr_start);
                attr_name[eq_sign - attr_start] = '\0';

                char* value_start = eq_sign + 1;
                if (*value_start != '"') break; // Value not quoted
                value_start++; // Move past opening quote

                char* value_end = strchr(value_start, '"');
                if (!value_end) break; // No closing quote

                char attr_value[256];
                strncpy(attr_value, value_start, value_end - value_start);
                attr_value[value_end - value_start] = '\0';

                if (strcmp(attr_name, "value") == 0) {
                    // Handle multi-line value by splitting on &#10; (HTML newline entity)
                    int line_idx = 0;
                    char* token = strtok(attr_value, "&#10;");
                    while (token != NULL && line_idx < MAX_TEXT_LINES) {
                        strncpy(elements[num_elements].text_content[line_idx], token, MAX_LINE_LENGTH - 1);
                        elements[num_elements].text_content[line_idx][MAX_LINE_LENGTH - 1] = '\0';
                        token = strtok(NULL, "&#10;");
                        line_idx++;
                    }
                    elements[num_elements].num_lines = line_idx > 0 ? line_idx : 1;
                    break;
                }
                attr_start = value_end + 1;
            }
        }
        elements[num_elements].selection_start_x = 0;
        elements[num_elements].selection_start_y = 0;
        elements[num_elements].selection_end_x = 0;
        elements[num_elements].selection_end_y = 0;
        elements[num_elements].has_selection = 0;
        elements[num_elements].clipboard[0] = '\0'; // Initialize clipboard to empty
        elements[num_elements].is_checked = 0;
        elements[num_elements].slider_value = 0;
        elements[num_elements].slider_min = 0;
        elements[num_elements].slider_max = 100;
        elements[num_elements].slider_step = 1;
        elements[num_elements].canvas_initialized = 0;
        elements[num_elements].canvas_render_func = NULL;
        strcpy(elements[num_elements].view_mode, "2d"); // Default to 2D view
        // Initialize camera properties for 3D
        elements[num_elements].camera_pos[0] = 0.0f;
        elements[num_elements].camera_pos[1] = 0.0f;
        elements[num_elements].camera_pos[2] = 5.0f; // Position camera in front of scene
        elements[num_elements].camera_target[0] = 0.0f;
        elements[num_elements].camera_target[1] = 0.0f;
        elements[num_elements].camera_target[2] = 0.0f; // Look at origin
        elements[num_elements].camera_up[0] = 0.0f;
        elements[num_elements].camera_up[1] = 1.0f;
        elements[num_elements].camera_up[2] = 0.0f; // Y is up
        elements[num_elements].fov = 45.0f; // Default field of view
        elements[num_elements].onClick[0] = '\0'; // Initialize onClick to empty string
        elements[num_elements].menu_items_count = 0;
        elements[num_elements].is_open = 0;
        strcpy(elements[num_elements].dir_path, ".");
        elements[num_elements].dir_entry_count = 0;
        elements[num_elements].dir_entry_selected = -1;

        // Parse attributes
        char* attr_start = tag_end;
        while (attr_start && (attr_start = strstr(attr_start, " ")) != NULL) {
            attr_start++; // Move past space
            char* eq_sign = strchr(attr_start, '=');
            if (!eq_sign) break; // No more attributes

            char attr_name[50];
            strncpy(attr_name, attr_start, eq_sign - attr_start);
            attr_name[eq_sign - attr_start] = '\0';

            char* value_start = eq_sign + 1;
            if (*value_start != '\"') break; // Value not quoted
            value_start++; // Move past opening quote

            char* value_end = strchr(value_start, '\"');
            if (!value_end) break; // No closing quote

            char attr_value[256];
            strncpy(attr_value, value_start, value_end - value_start);
            attr_value[value_end - value_start] = '\0';

            if (strcmp(attr_name, "x") == 0) elements[num_elements].x = atoi(attr_value);
            else if (strcmp(attr_name, "y") == 0) elements[num_elements].y = atoi(attr_value);
            else if (strcmp(attr_name, "width") == 0) elements[num_elements].width = atoi(attr_value);
            else if (strcmp(attr_name, "height") == 0) elements[num_elements].height = atoi(attr_value);
            else if (strcmp(attr_name, "id") == 0) strncpy(elements[num_elements].id, attr_value, 49);
            else if (strcmp(attr_name, "label") == 0) strncpy(elements[num_elements].label, attr_value, 49);
            else if (strcmp(attr_name, "value") == 0) {
                if (strcmp(elements[num_elements].type, "textfield") == 0 || strcmp(elements[num_elements].type, "textarea") == 0) {
                    // For textfield and textarea, set first line to the value attribute
                    // Handle multi-line value by splitting on &#10; (HTML newline entity)
                    int line_idx = 0;
                    char* temp_value = malloc(strlen(attr_value) + 1);
                    strcpy(temp_value, attr_value);
                    char* token = strtok(temp_value, "&#10;");
                    while (token != NULL && line_idx < MAX_TEXT_LINES) {
                        strncpy(elements[num_elements].text_content[line_idx], token, MAX_LINE_LENGTH - 1);
                        elements[num_elements].text_content[line_idx][MAX_LINE_LENGTH - 1] = '\0';
                        token = strtok(NULL, "&#10;");
                        line_idx++;
                    }
                    elements[num_elements].num_lines = line_idx > 0 ? line_idx : 1;
                    // Set cursor to end of first line
                    elements[num_elements].cursor_x = (line_idx > 0) ? strlen(elements[num_elements].text_content[0]) : 0;
                    elements[num_elements].cursor_y = 0;
                    free(temp_value);
                } else if (strcmp(elements[num_elements].type, "slider") == 0) {
                    elements[num_elements].slider_value = atoi(attr_value);
                } else if (strcmp(elements[num_elements].type, "text") == 0) {
                    // For text elements, store the value in the label field
                    strncpy(elements[num_elements].label, attr_value, 49);
                }
            }
            else if (strcmp(attr_name, "checked") == 0) {
                if (strcmp(attr_value, "true") == 0) {
                    elements[num_elements].is_checked = 1;
                }
            }
            else if (strcmp(attr_name, "min") == 0) elements[num_elements].slider_min = atoi(attr_value);
            else if (strcmp(attr_name, "max") == 0) elements[num_elements].slider_max = atoi(attr_value);
            else if (strcmp(attr_name, "step") == 0) elements[num_elements].slider_step = atoi(attr_value);
            else if (strcmp(attr_name, "color") == 0) {
                long color = strtol(attr_value + 1, NULL, 16);
                elements[num_elements].color[0] = ((color >> 16) & 0xFF) / 255.0f;
                elements[num_elements].color[1] = ((color >> 8) & 0xFF) / 255.0f;
                elements[num_elements].color[2] = (color & 0xFF) / 255.0f;
            }
            else if (strcmp(attr_name, "onClick") == 0) {
                strncpy(elements[num_elements].onClick, attr_value, 49);
                elements[num_elements].onClick[49] = '\0'; // Ensure null termination
            }
            else if (strcmp(attr_name, "view_mode") == 0 || strcmp(attr_name, "viewMode") == 0) {
                // Support both view_mode and viewMode for consistency
                strncpy(elements[num_elements].view_mode, attr_value, 9);
                elements[num_elements].view_mode[9] = '\0'; // Ensure null termination
            }
            else if (strcmp(attr_name, "path") == 0) {
                // For dirlist elements, set the directory path
                if (strcmp(elements[num_elements].type, "dirlist") == 0) {
                    strncpy(elements[num_elements].dir_path, attr_value, 511);
                    elements[num_elements].dir_path[511] = '\0'; // Ensure null termination
                }
            }
            attr_start = value_end + 1;
        }

        // Ensure slider_value is within min/max bounds after parsing all attributes
        if (strcmp(elements[num_elements].type, "slider") == 0) {
            if (elements[num_elements].slider_value < elements[num_elements].slider_min) {
                elements[num_elements].slider_value = elements[num_elements].slider_min;
            }
            if (elements[num_elements].slider_value > elements[num_elements].slider_max) {
                elements[num_elements].slider_value = elements[num_elements].slider_max;
            }
        }

        if (strcmp(tag_name, "panel") == 0) {
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "canvas") == 0) {
            // Assign a default render function to canvas elements
            elements[num_elements].canvas_render_func = canvas_render_sample;
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "menu") == 0) {
            elements[num_elements].menu_items_count = 0; // Initialize to 0, will be updated during parsing
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "menuitem") == 0) {
            // menuitem elements don't change the stack (they don't contain other elements)
            // but need to increment the parent's menu_items_count
            if (stack_top >= 0) {
                int parent_index = parent_stack[stack_top];
                if (strcmp(elements[parent_index].type, "menu") == 0) {
                    elements[parent_index].menu_items_count++;
                }
            }
        }

        num_elements++;
    }

    fclose(file);
}

void init_view(const char* filename) {
    init_freetype();  // Initialize FreeType for emoji rendering
    init_emoji_cache();  // Initialize emoji texture cache
    parse_chtml(filename);
}

void draw_element(UIElement* el) {
    int parent_x = 0;
    int parent_y = 0;

    if (el->parent != -1) {
        parent_x = elements[el->parent].x;
        parent_y = elements[el->parent].y;
    }

    int abs_x = parent_x + el->x;
    int abs_y = calculate_absolute_y(parent_y, el->y, el->height); // Convert from real-world to OpenGL coordinates (y=0 at top becomes y=window_height at bottom)

    if (strcmp(el->type, "canvas") == 0) {
        // Draw a border for the canvas
        glColor3f(0.5f, 0.5f, 0.5f); // Gray border
        glBegin(GL_LINE_LOOP);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();
        
        // Draw canvas content without changing global OpenGL state
        if (el->canvas_render_func != NULL) {
            // Save the current modelview matrix
            glPushMatrix();
            
            // Check view mode and set up appropriate projection
            if (strcmp(el->view_mode, "3d") == 0) {
                // Switch to projection matrix to set up 3D view
                glMatrixMode(GL_PROJECTION);
                glPushMatrix(); // Save current projection matrix
                glLoadIdentity();
                
                // Set up perspective projection for 3D
                gluPerspective(el->fov, (double)el->width/(double)el->height, 0.1, 100.0);
                
                // Switch back to modelview matrix for drawing
                glMatrixMode(GL_MODELVIEW);
                
                // Set up camera view
                gluLookAt(
                    el->camera_pos[0], el->camera_pos[1], el->camera_pos[2],  // Camera position
                    el->camera_target[0], el->camera_target[1], el->camera_target[2],  // Look at point
                    el->camera_up[0], el->camera_up[1], el->camera_up[2]  // Up vector
                );
            }
            
            // Apply transformation to move to canvas position (only in 2D mode)
            if (strcmp(el->view_mode, "3d") == 0) {
                // In 3D mode, we've already set up the camera, so no translation needed
                // The canvas position will be handled in the render function if needed
            } else {
                // In 2D mode, apply translation to canvas position
                glTranslatef(abs_x, abs_y, 0.0f);
            }
            
            // Call the canvas render function, adjusting the coordinates it receives
            // The function will draw in the local coordinate system
            el->canvas_render_func(0, 0, el->width, el->height);
            
            // Restore projection matrix if we were in 3D mode
            if (strcmp(el->view_mode, "3d") == 0) {
                glMatrixMode(GL_PROJECTION);
                glPopMatrix(); // Restore previous projection matrix
                glMatrixMode(GL_MODELVIEW);
            }
            
            // Restore the modelview matrix
            glPopMatrix();
        }
    } else if (strcmp(el->type, "menu") == 0) {
        // For context menus (like "Generic Context Menu"), only draw the menu bar when open
        // For permanent menu bars, always draw the menu bar
        int should_draw_menu_bar = 1; // By default, draw for permanent menu bars
        
        // Check if this is likely a context menu (by label or other heuristics)
        if (strcmp(el->label, "Generic Context Menu") == 0) {
            // This is a context menu - only draw if it's open
            should_draw_menu_bar = el->is_open;
        }
        
        if (should_draw_menu_bar) {
            // Draw menu bar background
            glColor3fv(el->color);
            glBegin(GL_QUADS);
            glVertex2i(abs_x, abs_y);
            glVertex2i(abs_x + el->width, abs_y);
            glVertex2i(abs_x + el->width, abs_y + el->height);
            glVertex2i(abs_x, abs_y + el->height);
            glEnd();
            
            // Draw menu text - center it vertically in the menu bar
            glColor3f(1.0f, 1.0f, 1.0f);
            int text_offset_x = 5;
            // Center the text vertically: start from the bottom of the menu and add centered offset
            int text_offset_y = (el->height / 2) - 6;  // Adjusted to position text properly in menu
            glRasterPos2i(abs_x + text_offset_x, abs_y + text_offset_y);
            for (char* c = el->label; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
            }
        }
        

    } else if (strcmp(el->type, "menuitem") == 0) {
        // Draw menu item - only if it's not part of an open menu (since those are drawn in the menu section)
        int parent_menu_index = el->parent;
        
        if (!(parent_menu_index != -1 && 
              strcmp(elements[parent_menu_index].type, "menu") == 0 && 
              elements[parent_menu_index].is_open)) {
            // Draw menu item in its original position only if its parent menu is not open
            // Draw menu item
            glColor3fv(el->color);
            glBegin(GL_QUADS);
            glVertex2i(abs_x, abs_y);
            glVertex2i(abs_x + el->width, abs_y);
            glVertex2i(abs_x + el->width, abs_y + el->height);
            glVertex2i(abs_x, abs_y + el->height);
            glEnd();
            
            // Draw menu item text
            glColor3f(0.0f, 0.0f, 0.0f); // Black text for menu items
            glRasterPos2i(abs_x + 5, abs_y + 15);
            for (char* c = el->label; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
            }
        }
        // If the parent menu IS open, menuitems are drawn in the menu section instead
    } else if (strcmp(el->type, "dirlist") == 0) {
        // Draw directory listing background
        glColor3f(0.2f, 0.2f, 0.2f); // Dark gray background
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();

        // Get directory contents from the model
        int entry_count = 0;
        char (*entries)[256] = model_get_dir_entries(&entry_count);

        // Draw "dir element list:" header
        glColor3f(1.0f, 1.0f, 0.0f); // Yellow text for header
        glRasterPos2i(abs_x + 5, abs_y + 15);
        const char* header_text = "dir element list:";
        for (const char* c = header_text; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }

        // Draw directory entries
        glColor3f(1.0f, 1.0f, 1.0f); // White text for entries
        int line_height = 20;
        for (int i = 0; i < entry_count; i++) {
            int text_y = abs_y + 40 + (i * line_height); // Start below header
            if (text_y + line_height < abs_y + el->height) { // Make sure we don't draw outside bounds
                // TODO: Add back selection highlighting if needed
                glRasterPos2i(abs_x + 10, text_y);
                for (const char* c = entries[i]; *c != '\0'; c++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
                }
            }
        }
    } else if (strcmp(el->type, "textfield") == 0 || strcmp(el->type, "textarea") == 0) {
        // Background
        glColor3f(0.1f, 0.1f, 0.1f);
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();
        // Border
        glColor3f(0.8f, 0.8f, 0.8f); // Light grey border
        glBegin(GL_LINE_LOOP);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();
    } else if (strcmp(el->type, "header") == 0 || strcmp(el->type, "button") == 0 || strcmp(el->type, "panel") == 0 || strcmp(el->type, "checkbox") == 0 || strcmp(el->type, "slider") == 0) {
        if (strcmp(el->type, "checkbox") == 0) {
            glColor3f(0.8f, 0.8f, 0.8f); // Light background for checkbox square
        } else if (strcmp(el->type, "slider") == 0) {
            glColor3f(0.3f, 0.3f, 0.3f); // Dark background for slider track
        } else {
            glColor3fv(el->color);
        }
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->width, abs_y);
        glVertex2i(abs_x + el->width, abs_y + el->height);
        glVertex2i(abs_x, abs_y + el->height);
        glEnd();

        if (strcmp(el->type, "checkbox") == 0 && el->is_checked) {
            // Draw a simple checkmark (upright V shape, adjusted for inverted Y)
            glColor3f(0.0f, 0.0f, 0.0f); // Black checkmark
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            // First segment: bottom-left to top-middle
            glVertex2i(abs_x + el->width * 0.2, abs_y + el->height * 0.8); // Visually lower
            glVertex2i(abs_x + el->width * 0.4, abs_y + el->height * 0.2); // Visually higher
            // Second segment: top-middle to bottom-right
            glVertex2i(abs_x + el->width * 0.4, abs_y + el->height * 0.2); // Visually higher
            glVertex2i(abs_x + el->width * 0.8, abs_y + el->height * 0.8); // Visually lower
            glEnd();
            glLineWidth(1.0f);
        } else if (strcmp(el->type, "slider") == 0) {
            // Check if this is a vertical slider by looking at the aspect ratio
            int is_vertical = (el->height > el->width);  // If height is greater than width, consider it vertical
            
            if (is_vertical) {
                // Draw vertical slider track (background)
                glColor3f(0.3f, 0.3f, 0.3f); // Dark background for slider track
                glBegin(GL_QUADS);
                glVertex2i(abs_x + el->width/2 - 3, abs_y); // Centered track
                glVertex2i(abs_x + el->width/2 + 3, abs_y);
                glVertex2i(abs_x + el->width/2 + 3, abs_y + el->height);
                glVertex2i(abs_x + el->width/2 - 3, abs_y + el->height);
                glEnd();
            
                // Draw vertical slider thumb
                float normalized_pos = (float)(el->slider_value - el->slider_min) / (el->slider_max - el->slider_min);
                float thumb_pos_y = abs_y + normalized_pos * (el->height - 10); // Position based on normalized value
                glColor3f(0.8f, 0.8f, 0.8f); // Light grey for thumb
                glBegin(GL_QUADS);
                glVertex2i(abs_x, thumb_pos_y); // Thumb wider than track
                glVertex2i(abs_x + el->width, thumb_pos_y);
                glVertex2i(abs_x + el->width, thumb_pos_y + 10); // Thumb height of 10
                glVertex2i(abs_x, thumb_pos_y + 10);
                glEnd();

                // Draw slider value as text
                char value_str[10];
                sprintf(value_str, "%d", el->slider_value);
                glColor3f(1.0f, 1.0f, 1.0f);
                glRasterPos2i(abs_x + el->width + 5, abs_y + el->height / 2 - 5);
                for (char* c = value_str; *c != '\0'; c++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
                }
            } else {
                // Original horizontal slider implementation
                float thumb_pos_x = abs_x + (float)(el->slider_value - el->slider_min) / (el->slider_max - el->slider_min) * (el->width - 10); // 10 is thumb width
                glColor3f(0.8f, 0.8f, 0.8f); // Light grey for thumb
                glBegin(GL_QUADS);
                glVertex2i(thumb_pos_x, abs_y);
                glVertex2i(thumb_pos_x + 10, abs_y);
                glVertex2i(thumb_pos_x + 10, abs_y + el->height);
                glVertex2i(thumb_pos_x, abs_y + el->height);
                glEnd();

                // Draw slider value as text
                char value_str[10];
                sprintf(value_str, "%d", el->slider_value);
                glColor3f(1.0f, 1.0f, 1.0f);
                glRasterPos2i(abs_x + el->width + 5, abs_y + el->height / 2 - 5);
                for (char* c = value_str; *c != '\0'; c++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
                }
            }
        }
    }

    if (strcmp(el->type, "text") == 0 || strcmp(el->type, "button") == 0 || strcmp(el->type, "checkbox") == 0) {
        // Check if the text contains emojis
        if (contains_emoji(el->label)) {
            // Use emoji-aware text rendering
            int text_offset_x = 5;
            int text_offset_y = 15;
            if (strcmp(el->type, "checkbox") == 0) {
                text_offset_x = el->width + 5; // Label next to checkbox
                text_offset_y = el->height / 2 - 5; // Center label vertically
            }
            
            float text_color[3] = {1.0f, 1.0f, 1.0f}; // Default white
            float text_pos_x = abs_x + text_offset_x;
            float text_pos_y = abs_y + text_offset_y + 15; // Use slightly adjusted Y to align with text baseline
            
            render_text_with_emojis(el->label, text_pos_x, text_pos_y, text_color); // Use original approach
        } else {
            // Use original GLUT rendering for non-emoji text
            glColor3f(1.0f, 1.0f, 1.0f);
            int text_offset_x = 5;
            int text_offset_y = 15;
            if (strcmp(el->type, "checkbox") == 0) {
                text_offset_x = el->width + 5; // Label next to checkbox
                text_offset_y = el->height / 2 - 5; // Center label vertically
            }
            glRasterPos2i(abs_x + text_offset_x, abs_y + text_offset_y);
            for (char* c = el->label; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
            }
        }
    } else if (strcmp(el->type, "textfield") == 0 || strcmp(el->type, "textarea") == 0) {
        // Draw multi-line text, with emoji support when needed
        int line_height = 20; // Approximate height for each line
        int start_y = abs_y + 15; // Starting y position for first line
        
        for (int line = 0; line < el->num_lines; line++) {
            if (start_y + (line * line_height) >= abs_y + el->height) break; // Don't draw outside bounds
            
            if (contains_emoji(el->text_content[line])) {
                // Use emoji-aware text rendering
                float text_color[3] = {1.0f, 1.0f, 1.0f}; // White color for textfield
                float text_pos_x = abs_x + 5;
                float text_pos_y = start_y + (line * line_height) + 15; // Use slightly adjusted Y to align with text baseline
                
                render_text_with_emojis(el->text_content[line], text_pos_x, text_pos_y, text_color); // Use original approach
            } else {
                // Use original GLUT rendering for non-emoji text
                glRasterPos2i(abs_x + 5, start_y + (line * line_height));
                for (char* c = el->text_content[line]; *c != '\0'; c++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
                }
            }
        }

        // Draw cursor if active
        if (el->is_active) {
            // Simple blinking cursor (toggle every 500ms)
            if ((glutGet(GLUT_ELAPSED_TIME) / 500) % 2) {
                int text_width = 0;
                for (int i = 0; i < el->cursor_x && i < (int)strlen(el->text_content[el->cursor_y]); i++) {
                    text_width += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, el->text_content[el->cursor_y][i]);
                }
                glColor3f(1.0f, 1.0f, 1.0f);
                glBegin(GL_QUADS);
                glVertex2i(abs_x + 5 + text_width, start_y + (el->cursor_y * line_height) - 15);
                glVertex2i(abs_x + 5 + text_width + 2, start_y + (el->cursor_y * line_height) - 15);
                glVertex2i(abs_x + 5 + text_width + 2, start_y + (el->cursor_y * line_height) + 5);
                glVertex2i(abs_x + 5 + text_width, start_y + (el->cursor_y * line_height) + 5);
                glEnd();
            }
        }
    }
}

// Function to cleanup FreeType resources


void cleanup_freetype() {
    // Clean up cached emoji textures
    for (int i = 0; i < emoji_cache_size && i < MAX_CACHED_EMOJIS; i++) {
        if (emoji_cache[i].loaded) {
            glDeleteTextures(1, &emoji_cache[i].texture_id);
        }
    }
    
    if (emoji_face) {
        FT_Done_Face(emoji_face);
        emoji_face = NULL;
    }
    if (ft) {
        FT_Done_FreeType(ft);
        ft = 0;
    }
}

void display() {
    // Find window element and set background color
    float bg_r = 0.0f, bg_g = 0.0f, bg_b = 0.0f; // Default to black
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "window") == 0) {
            bg_r = elements[i].color[0];
            bg_g = elements[i].color[1];
            bg_b = elements[i].color[2];
            break;
        }
    }
    glClearColor(bg_r, bg_g, bg_b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw all regular elements first
    for (int i = 0; i < num_elements; i++) {
        // Draw the element but don't draw submenu items here
        // (they'll be drawn separately to ensure they appear on top)
        if (strcmp(elements[i].type, "menuitem") == 0 && 
            elements[i].parent != -1 && 
            strcmp(elements[elements[i].parent].type, "menu") == 0 &&
            elements[elements[i].parent].is_open) {
            // Skip drawing menuitems that are part of an open menu here
            // They will be drawn with the menu in the next step
        } else {
            draw_element(&elements[i]);
        }
    }

    // Draw submenu items separately to ensure they appear on top of everything else
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "menu") == 0 && elements[i].is_open) {
            // Draw this open menu's submenu items
            // Calculate absolute position of menu using same transformation as draw_element
            int parent_x = 0;
            int parent_y = 0;
            int current_parent = elements[i].parent;
            while (current_parent != -1) {
                parent_x += elements[current_parent].x;
                parent_y += elements[current_parent].y;
                current_parent = elements[current_parent].parent;
            }

            int abs_x = parent_x + elements[i].x;
            int abs_y = calculate_absolute_y(parent_y, elements[i].y, elements[i].height); // Same transformation as draw_element
            
            // Calculate submenu position - need to check if it would go outside window bounds
            int submenu_start_y = abs_y + elements[i].height;
            int submenu_end_y = abs_y + elements[i].height + 20 * elements[i].menu_items_count;
            
            // If submenu would go outside window bounds, draw it above the menu instead
            if (submenu_end_y > window_height) {
                submenu_start_y = abs_y - 20 * elements[i].menu_items_count;
                if (submenu_start_y < 0) {
                    submenu_start_y = 0; // Ensure it doesn't go above the window
                }
            }
            
            // Draw submenu background
            glColor3f(1.0f, 1.0f, 0.0f); // Bright yellow for submenu background to make it visible
            glBegin(GL_QUADS);
            glVertex2i(abs_x, submenu_start_y); // Position submenu appropriately
            glVertex2i(abs_x + elements[i].width, submenu_start_y);
            glVertex2i(abs_x + elements[i].width, submenu_start_y + 20 * elements[i].menu_items_count); // Each item is ~20px high
            glVertex2i(abs_x, submenu_start_y + 20 * elements[i].menu_items_count);
            glEnd();
            
            // Draw submenu items
            for (int j = 0; j < num_elements; j++) {
                if (elements[j].parent == i && strcmp(elements[j].type, "menuitem") == 0) {
                    // Calculate position of this item in the submenu
                    int item_position = 0;
                    for (int k = 0; k < num_elements; k++) {
                        if (elements[k].parent == i && 
                            strcmp(elements[k].type, "menuitem") == 0) {
                            if (k == j) { // If this is our element
                                break;
                            }
                            item_position++;
                        }
                    }
                    
                    // Calculate the y position for this specific menu item
                    int item_y = submenu_start_y + item_position * 20;
                    
                    // Draw the submenu item
                    glColor3f(0.7f, 0.7f, 0.9f); // Light blue for menu items
                    glBegin(GL_QUADS);
                    glVertex2i(abs_x, item_y);
                    glVertex2i(abs_x + elements[j].width, item_y);
                    glVertex2i(abs_x + elements[j].width, item_y + 20);
                    glVertex2i(abs_x, item_y + 20);
                    glEnd();
                    
                    // Draw menu item text
                    glColor3f(0.0f, 0.0f, 0.0f); // Black text for menu items
                    glRasterPos2i(abs_x + 5, item_y + 15);
                    for (char* c = elements[j].label; *c != '\0'; c++) {
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
                    }
                }
            }
        }
    }

    glutSwapBuffers();
}

void canvas_render_sample(int x, int y, int width, int height) {
    // Find the canvas UIElement to check its view_mode
    UIElement* current_canvas = NULL;
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "canvas") == 0 && elements[i].width == width && elements[i].height == height) {
            current_canvas = &elements[i];
            break;
        }
    }

    int is_3d = (current_canvas && strcmp(current_canvas->view_mode, "3d") == 0);

    // Get the shapes from the model
    int shape_count = 0;
    Shape* shapes = model_get_shapes(&shape_count);

    if (is_3d) {
        // --- 3D Rendering Mode ---
        glEnable(GL_DEPTH_TEST);
        // Temporarily disable lighting for consistent color appearance with 2D mode
        // glEnable(GL_LIGHTING);
        // glEnable(GL_LIGHT0);

        // Setup projection matrix for 3D
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(current_canvas->fov, (double)width / (double)height, 0.1, 100.0);

        // Setup modelview matrix for camera
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        gluLookAt(current_canvas->camera_pos[0], current_canvas->camera_pos[1], current_canvas->camera_pos[2],
                  current_canvas->camera_target[0], current_canvas->camera_target[1], current_canvas->camera_target[2],
                  current_canvas->camera_up[0], current_canvas->camera_up[1], current_canvas->camera_up[2]);

        // Render shapes in 3D
        for (int i = 0; i < shape_count; i++) {
            Shape* s = &shapes[i];
            if (strcmp(s->type, "SQUARE") == 0 || strcmp(s->type, "RECT") == 0) {
                glPushMatrix();
                // Map the 2D coordinates from the module to the XZ plane in 3D
                float x_3d = (s->x / (float)width) * 10.0f - 5.0f;
                float z_3d = (s->y / (float)height) * 10.0f - 5.0f;
                glTranslatef(x_3d, 0.5f, z_3d);
                glColor4fv(s->color);
                // Use width and height for 3D scaling
                float size_x = s->width / 10.0f;
                float size_y = (s->height) / 10.0f;  // Height for the cube
                float size_z = s->depth / 10.0f;
                
                // Draw a rectangular prism
                glBegin(GL_QUADS);
                // Front face
                glVertex3f(-size_x/2, -size_y/2, size_z/2);
                glVertex3f(size_x/2, -size_y/2, size_z/2);
                glVertex3f(size_x/2, size_y/2, size_z/2);
                glVertex3f(-size_x/2, size_y/2, size_z/2);
                
                // Back face
                glVertex3f(-size_x/2, -size_y/2, -size_z/2);
                glVertex3f(-size_x/2, size_y/2, -size_z/2);
                glVertex3f(size_x/2, size_y/2, -size_z/2);
                glVertex3f(size_x/2, -size_y/2, -size_z/2);
                
                // Top face
                glVertex3f(-size_x/2, size_y/2, -size_z/2);
                glVertex3f(-size_x/2, size_y/2, size_z/2);
                glVertex3f(size_x/2, size_y/2, size_z/2);
                glVertex3f(size_x/2, size_y/2, -size_z/2);
                
                // Bottom face
                glVertex3f(-size_x/2, -size_y/2, -size_z/2);
                glVertex3f(size_x/2, -size_y/2, -size_z/2);
                glVertex3f(size_x/2, -size_y/2, size_z/2);
                glVertex3f(-size_x/2, -size_y/2, size_z/2);
                
                // Left face
                glVertex3f(-size_x/2, -size_y/2, -size_z/2);
                glVertex3f(-size_x/2, -size_y/2, size_z/2);
                glVertex3f(-size_x/2, size_y/2, size_z/2);
                glVertex3f(-size_x/2, size_y/2, -size_z/2);
                
                // Right face
                glVertex3f(size_x/2, -size_y/2, -size_z/2);
                glVertex3f(size_x/2, size_y/2, -size_z/2);
                glVertex3f(size_x/2, size_y/2, size_z/2);
                glVertex3f(size_x/2, -size_y/2, size_z/2);
                glEnd();
                glPopMatrix();
            } else if (strcmp(s->type, "TRIANGLE") == 0) {
                glPushMatrix();
                // Map the 2D coordinates from the module to the XZ plane in 3D
                float x_3d = (s->x / (float)width) * 10.0f - 5.0f;
                float z_3d = (s->y / (float)height) * 10.0f - 5.0f;
                glTranslatef(x_3d, 0.5f, z_3d);
                glColor4fv(s->color);
                // Use width and height for 3D scaling
                float avg_size = (s->width + s->height) / 2.0f;
                
                // Draw a triangle pyramid
                glBegin(GL_TRIANGLES);
                // Front face
                glVertex3f(0.0f, avg_size/10.0f, 0.0f);  // Top
                glVertex3f(-avg_size/20.0f, -avg_size/20.0f, avg_size/20.0f);  // Bottom left
                glVertex3f(avg_size/20.0f, -avg_size/20.0f, avg_size/20.0f);   // Bottom right
                
                // Right face
                glVertex3f(0.0f, avg_size/10.0f, 0.0f);  // Top
                glVertex3f(avg_size/20.0f, -avg_size/20.0f, avg_size/20.0f);   // Bottom right
                glVertex3f(0.0f, -avg_size/20.0f, -avg_size/20.0f);  // Bottom back
                
                // Left face
                glVertex3f(0.0f, avg_size/10.0f, 0.0f);  // Top
                glVertex3f(0.0f, -avg_size/20.0f, -avg_size/20.0f);  // Bottom back
                glVertex3f(-avg_size/20.0f, -avg_size/20.0f, avg_size/20.0f);  // Bottom left
                glEnd();
                glPopMatrix();
            }
        }

        // Restore matrices and state
        glPopMatrix(); // Pop modelview
        glMatrixMode(GL_PROJECTION);
        glPopMatrix(); // Pop projection
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);
        // glDisable(GL_LIGHTING);  // Not needed since we didn't enable it

    } else {
        // --- 2D Rendering Mode ---
        // Draw a simple background for the canvas
        glColor3f(0.8f, 0.9f, 1.0f); // Light blue background
        glBegin(GL_QUADS);
        glVertex2i(0, 0);  // The drawing is translated to the canvas's origin
        glVertex2i(width, 0);
        glVertex2i(width, height);
        glVertex2i(0, height);
        glEnd();

        // Render each shape in 2D
        for (int i = 0; i < shape_count; i++) {
            Shape* s = &shapes[i];
            glColor4fv(s->color);

            if (strcmp(s->type, "SQUARE") == 0 || strcmp(s->type, "RECT") == 0) {
                glBegin(GL_QUADS);
                glVertex2f(s->x - s->width / 2, s->y - s->height / 2);
                glVertex2f(s->x + s->width / 2, s->y - s->height / 2);
                glVertex2f(s->x + s->width / 2, s->y + s->height / 2);
                glVertex2f(s->x - s->width / 2, s->y + s->height / 2);
                glEnd();
            } else if (strcmp(s->type, "CIRCLE") == 0) {
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(s->x, s->y);
                for (int j = 0; j <= 20; j++) {
                    float angle = j * (2.0f * 3.14159f / 20);
                    float dx = s->width / 2 * cos(angle);
                    float dy = s->height / 2 * sin(angle);
                    glVertex2f(s->x + dx, s->y + dy);
                }
                glEnd();
            } else if (strcmp(s->type, "TRIANGLE") == 0) {
                glBegin(GL_TRIANGLES);
                // Calculate the three vertices of the triangle
                // For an equilateral triangle centered at (x, y)
                float half_width = s->width / 2.0f;
                float half_height = s->height / 2.0f;
                
                // Vertex 1: Top vertex
                glVertex2f(s->x, s->y + half_height);
                // Vertex 2: Bottom left
                glVertex2f(s->x - half_width, s->y - half_height);
                // Vertex 3: Bottom right
                glVertex2f(s->x + half_width, s->y - half_height);
                glEnd();
            }
        }
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
